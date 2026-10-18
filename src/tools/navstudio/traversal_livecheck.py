"""Observe a fixed route on an existing, naturally spawned bot.

Uses the one configured visible test installation. Never installs a graph,
changes population, teleports an actor, or locks the game window. A successful
run is evidence for this route only, not for the complete map.
"""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tce_nav_livecheck import (INSTANCE, ROOT, Rcon, LogWindow, positions,
    process_identity, provenance, serial_lock, fresh_screenshot, save)


def capture_frame(rcon, directory, index, row, event):
    """Capture the current followed actor immediately, without camera delays."""
    stem = 'navtrav_' + uuid.uuid4().hex
    epoch = time.time()
    reply = rcon.command('screenshotJPEG ' + stem)
    source = INSTANCE / 'tce2/screenshots' / (stem + '.jpg')
    raw = source.read_bytes() if source.is_file() else b''
    if 'Wrote screenshots/' not in reply or len(raw) < 1000 or raw[:2] != b'\xff\xd8' or raw[-2:] != b'\xff\xd9':
        raise RuntimeError('Fresh action screenshot not acknowledged.')
    target = directory / ('screenshot-%03d.jpg' % index)
    with target.open('xb') as stream:
        stream.write(raw)
    value = dict(image=str(target.resolve()), engine_file=str(source), requested_epoch=epoch,
        sha256=hashlib.sha256(raw).hexdigest(), follow_requested=row, action_event=event,
        fresh_complete_engine_jpeg=True, visually_reviewed=False, bot_positions_modified=False)
    save(target.with_suffix('.json'), value)
    return value


def run(args):
    route = json.loads(args.route.read_text(encoding='utf8'))
    if not isinstance(route, list) or not route or any(
        not isinstance(p, list) or len(p) != 3 or any(
            type(v) not in (int, float) or not math.isfinite(v) for v in p)
        for p in route):
        raise ValueError('Route must contain finite world player origins.')
    target = args.out.resolve()
    if not target.is_relative_to((ROOT / 'build').resolve()) or target == INSTANCE.resolve():
        raise ValueError('Use a new evidence directory under build.')
    target.mkdir(parents=True, exist_ok=False)
    result = dict(map=args.map, slot=args.slot, route=route, complete=False,
        bot_teleports=False, population_changed=False, window_locked=False,
        samples=[], screenshots=[], claim='Only the selected fixed route is tested.')
    rcon = log = identity = None
    marker = None
    started = None
    try:
        with serial_lock():
            identity = process_identity(args.port)
            result['process'] = identity
            result['provenance_before'] = provenance(args)
            rcon = Rcon(args.port, args.password, target)
            for key, value in [('mapname', args.map), ('fs_game', 'tce2'), ('dedicated', '0')]:
                if rcon.cvar(key) != value:
                    raise RuntimeError('Wrong active ' + key)
            rows = positions(rcon.command('navprobe'))
            row = next((p for p in rows if p['slot'] == args.slot and p['health'] > 0 and p['team'] in (1, 2)), None)
            if row is None:
                raise RuntimeError('Selected existing bot is not alive on a playing team.')
            result['initial_bot'] = row
            marker = 'NAVTRAV_' + uuid.uuid4().hex[:12]
            # Console argument parsing strips literal GM string quotes.
            # Stage exact script text rather than reassembling quoted code.
            script = INSTANCE / 'navgen' / (marker + '.gm')
            script.parent.mkdir(exist_ok=True)
            script.write_text('foreach(b in BotTable){if(b.GetGameId()==%d){print("%s",b.Name);}}' % (args.slot, marker), encoding='ascii')
            registry = rcon.command('bot script_runfile homepath/navgen/' + script.name)
            if marker not in registry:
                raise RuntimeError('Selected player is not a registered bot.')
            result['registry_reply'] = registry
            rcon.command('follow %d' % args.slot)
            rcon.command('bot waypoint_view on')
            rcon.command('cg_omnibotdrawing 1')
            rcon.command('cg_omnibot_render_distance 800')
            log = LogWindow(INSTANCE / 'tce2/etconsole.log', target / 'console-window.log')
            program = ('global NavTravTokens = {}; '
                'global NavTravObserve = function(b,token){while(NavTravTokens[token]){'
                'print("NAVTRAVTICK",token,GetTime(),b.GetPosition(),b.GetVelocity(),'
                'b.HasEntityFlag(ENTFLAG.ON_LADDER),b.HasEntityFlag(ENTFLAG.ON_GROUND),'
                'b.GetStat("tce_free_climb_stamina"),b.GetStat("tce_free_climb_ready"),'
                'b.GetStat("tce_free_climb_blockers"),b.GetStat("tce_free_climb_pmflags"),'
                'b.GetStat("tce_free_climb_tcflags"),b.GetStat("tce_free_climb_wflags"),'
                'b.GetStat("tce_free_climb_wstate"),b.GetStat("tce_free_climb_ground"));sleep(.05);}}; '
                'foreach(b in BotTable){if(b.GetGameId()==%d){r={%s};'
                'if(b.Name=="NavProbe"){global NavProbeRoute=r;}else{NavProbeRoutes[b.Name]=r;}'
                'NavTravTokens["%s"]=1;thread(NavTravObserve,b,"%s");print("%s DISPATCH");}}' %
                (args.slot, ','.join('Vector3(%g,%g,%g)' % tuple(p) for p in route), marker, marker, marker))
            script.write_text(program, encoding='ascii')
            result['dispatch_reply'] = rcon.command('bot script_runfile homepath/navgen/' + script.name)
            if marker + ' DISPATCH' not in result['dispatch_reply']:
                raise RuntimeError('Route dispatch not acknowledged.')
            started = time.monotonic()
            next_shot = 0
            captured_events = 0
            while time.monotonic() - started < args.seconds:
                elapsed = time.monotonic() - started
                rows = positions(rcon.command('navprobe'))
                row = next((p for p in rows if p['slot'] == args.slot), None)
                if row is None or row['name'] != result['initial_bot']['name'] or row['team'] != result['initial_bot']['team']:
                    raise RuntimeError('Selected bot changed or disappeared.')
                result['samples'].append(dict(time=round(elapsed, 3), bot=row))
                log.poll()
                text = (target / 'console-window.log').read_text(encoding='latin1')
                events = [line for line in text.splitlines()
                    if 'TCE2FREECLIMB LAUNCH ' in line or 'TCE2WALKDROP LAUNCH ' in line or
                       'TCE2STANDJUMP LAUNCH ' in line or 'TCE2STANDJUMP LAND ' in line or
                       'TCE2STANDJUMP FAIL ' in line]
                new_event = len(events) > captured_events
                if (elapsed >= next_shot or new_event) and row['health'] > 0:
                    result['screenshots'].append(capture_frame(rcon, target, len(result['screenshots']) + 1, row,
                        events[-1] if new_event else None))
                    next_shot = elapsed + 12
                    captured_events = len(events)
                if re.search(r'NAVPATH (?:DONE|FAILED|TIMEOUT)\b', text) or row['health'] <= 0:
                    break
                time.sleep(.1)
            log.poll()
            text = (target / 'console-window.log').read_text(encoding='latin1')
            result['successes'] = re.findall(r'NAVPATH SUCCESS[^\r\n]*', text)
            result['failures'] = re.findall(r'NAVPATH (?:FAILED|TIMEOUT)[^\r\n]*', text)
            result['action_events'] = [line for line in text.splitlines()
                if 'TCE2FREECLIMB ' in line or 'TCE2WALKDROP ' in line or 'TCE2STANDJUMP ' in line]
            result['motion_ticks'] = [line for line in text.splitlines() if 'NAVTRAVTICK ' in line]
            result['minimum_health'] = min((p['bot']['health'] for p in result['samples']), default=0)
            result['provenance_after'] = provenance(args)
            result['files_unchanged'] = result['provenance_before']['files'] == result['provenance_after']['files']
            result['complete'] = (len(result['successes']) == len(route) and not result['failures']
                and 'NAVPATH DONE' in text and result['minimum_health'] == result['initial_bot']['health']
                and result['files_unchanged'] and not log.events)
    except Exception as exc:
        result['error'] = str(exc)
    finally:
        result['elapsed_seconds'] = round(time.monotonic() - started, 3) if started else 0
        if log:
            log.poll(); log.close()
        if rcon:
            try:
                after = process_identity(args.port)
                if identity and marker and all(after[k] == identity[k] for k in ('pid', 'path', 'start_ticks')):
                    cleanup = INSTANCE / 'navgen' / (marker + '-cleanup.gm')
                    cleanup.write_text('if(NavTravTokens!=null){NavTravTokens["%s"]=null;}foreach(b in BotTable){if(b.GetGameId()==%d){if(b.Name=="NavProbe"){global NavProbeRoute=null;}else{NavProbeRoutes[b.Name]=null;}}}' % (marker, args.slot), encoding='ascii')
                    result['cleanup_reply'] = rcon.command('bot script_runfile homepath/navgen/' + cleanup.name)
                else:
                    result['cleanup_error'] = 'Owned process changed; no command sent.'
                    result['complete'] = False
            finally:
                rcon.stream.close()
        result['visually_reviewed'] = False
        save(target / 'result.json', result)
    print(json.dumps({k: len(v) if isinstance(v, list) else v for k, v in result.items()
        if k not in ('process', 'registry_reply', 'provenance_before', 'provenance_after', 'samples', 'route')}, indent=2))
    return 0 if result['complete'] else 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--map', required=True)
    parser.add_argument('--slot', type=int, required=True)
    parser.add_argument('--route', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=120)
    parser.add_argument('--port', type=int, default=27986)
    parser.add_argument('--password', default='navgen_local')
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--way', type=Path, required=True)
    parser.add_argument('--proof', type=Path, required=True)
    parser.add_argument('--report-sha256')
    parser.add_argument('--way-sha256')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]+', args.map) or not 1 <= args.slot < 64 or not 10 <= args.seconds <= 300:
        parser.error('Invalid map, existing bot slot, or duration.')
    raise SystemExit(run(args))
