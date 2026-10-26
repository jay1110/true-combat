"""Bounded, visible combat check of an already installed generated map network.

Only uses build/navgen-overkill: it never installs/starts a game, switches maps,
teleports players, or injects desktop input. Run serially with every other RCON
job stopped. The output directory must be new and is never reused.

Example (hashes are those approved by the caller, not values to invent):
  python navstudio/workers/tce_nav_livecheck.py --map goldrush --seconds 180 \
    --out build/navgen-overkill/standard-tests/goldrush/run-001 \
    --way-sha256 SHA256 --report-sha256 SHA256

Alternatively --proof accepts a NavStudio native-proof JSON containing matching
"way" and "native_report" SHA-256 fields. Screenshots still require human visual
inspection: neither fresh pixels nor error-free logs certify a good mesh.

TCE standard maps are supported without aliases: obj_delta, obj_northport,
obj_railhouse, obj_snow, obj_stadtrand, obj_village. --map must exactly match
the loaded map and native report; ET map names remain supported as well.
"""
from __future__ import annotations

import argparse
import contextlib
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from layout import REPO_ROOT, install_import_paths
install_import_paths()
import re
import socket
import subprocess
import sys
import time
import uuid
from navstudio.objective_evidence import summarize as summarize_objectives

ROOT = REPO_ROOT
INSTANCE = ROOT / "build/navgen-overkill"
NAME = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")
SHA = re.compile(r"[a-fA-F0-9]{64}\Z")
POS = re.compile(r"NAVPOS (\d+) ([\d.e+-]+) ([\d.e+-]+) ([\d.e+-]+) (-?\d+) (\d+) ([^\r\n]+)")


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def save(path: Path, value) -> None:
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False)
        stream.write("\n")


def clean(text: str) -> str:
    # Q_IsColorString accepts every non-caret character, including the
    # extended letter colours used by Omni-bot names.
    return re.sub(r"\^[^\^\r\n]", "", text)


def registered_players(rows: list[dict], registered: dict[int, str]) -> list[dict]:
    """Match the native BotTable slot and the same uncoloured player name."""
    current = []
    for row in rows:
        if row['name'] == 'NavStudioObserver' and row['team'] == 3:
            continue
        if row['slot'] not in registered or clean(row['name']) != clean(registered[row['slot']]).strip():
            raise RuntimeError('Unregistered player in the owned observation')
        if row['team'] in (1, 2):
            current.append(row)
    return current


def cvar_value(name: str, text: str) -> str:
    match = re.search(r'"?' + re.escape(name) + r'"?\s+is:\s*"([^"\r\n]*)"', clean(text), re.I)
    if not match:
        raise RuntimeError(f"Cannot confirm {name}: {text[:300]!r}")
    return match[1]


def positions(reply: str) -> list[dict]:
    result = []
    for m in POS.finditer(clean(reply)):
        xyz = [float(m[i]) for i in (2, 3, 4)]
        if not all(math.isfinite(v) for v in xyz):
            raise RuntimeError("Nonfinite live position")
        result.append(dict(slot=int(m[1]), position=xyz, health=int(m[5]), team=int(m[6]), name=m[7].strip()))
    if "NAVSTATE " not in reply or not result:
        raise RuntimeError("Existing navprobe.lua telemetry unavailable")
    return result


class Rcon:
    def __init__(self, port: int, password: str, output: Path):
        self.port, self.password = port, password
        self.stream = (output / "commands.jsonl").open("x", encoding="utf-8")

    def command(self, text: str) -> str:
        started = time.time()
        chunks = []
        error = None
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                s.connect(("127.0.0.1", self.port))
                s.settimeout(.65)
                s.send(b"\xff\xff\xff\xffrcon " + self.password.encode("ascii") + b" " + text.encode("ascii") + b"\n")
                deadline = time.monotonic() + 4
                while time.monotonic() < deadline:
                    try:
                        chunks.append(s.recv(65535).decode("latin1").removeprefix("\xff\xff\xff\xffprint\n"))
                    except socket.timeout:
                        break
        except Exception as exc:
            error = str(exc)
            raise
        finally:
            self.stream.write(json.dumps(dict(epoch=started, command=text, reply="".join(chunks), error=error)) + "\n")
            self.stream.flush()
        answer = "".join(chunks)
        if "bad rconpassword" in answer.lower():
            raise RuntimeError("RCON password rejected")
        return answer

    def cvar(self, name: str) -> str:
        return cvar_value(name, self.command(name))


@contextlib.contextmanager
def serial_lock():
    # Kernel lock releases on process death; other tools must still be coordinated.
    import msvcrt
    path = INSTANCE / "nav-livecheck.lock"
    with path.open("a+b") as stream:
        if stream.tell() == 0:
            stream.write(b"0")
            stream.flush()
        stream.seek(0)
        try:
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as exc:
            raise RuntimeError("Another map livecheck owns this instance") from exc
        try:
            yield
        finally:
            stream.seek(0)
            msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)


def process_identity(port: int) -> dict:
    """Read-only exact executable/PID/UDP ownership; no process/UI changes."""
    pid = int((INSTANCE / "client.pid").read_text().strip())
    if pid <= 0:
        raise RuntimeError("Invalid existing client.pid")
    script = f"""
$ErrorActionPreference='Stop'
$navProc=Get-Process -Id {pid}
$navCim=Get-CimInstance Win32_Process -Filter 'ProcessId = {pid}'
$navUdp=@(Get-NetUDPEndpoint -LocalPort {port} -ErrorAction Stop | Select-Object -ExpandProperty OwningProcess -Unique)
[pscustomobject]@{{pid=$navProc.Id; path=$navProc.Path; start_ticks=$navProc.StartTime.ToUniversalTime().Ticks; hwnd=$navProc.MainWindowHandle.ToInt64(); udp_owners=$navUdp; command_line=$navCim.CommandLine}} | ConvertTo-Json -Compress
"""
    run = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
                         capture_output=True, text=True, timeout=15,
                         creationflags=subprocess.CREATE_NO_WINDOW)
    if run.returncode:
        raise RuntimeError("Cannot inspect existing test process: " + run.stderr[-800:])
    value = json.loads(run.stdout)
    if Path(value["path"]).resolve() != (INSTANCE / "ET.exe").resolve():
        raise RuntimeError("client.pid is not the existing isolated test executable")
    if set(value.get("udp_owners", [])) != {pid}:
        raise RuntimeError("RCON UDP port is not owned exclusively by this test process")
    hwnd = value["hwnd"]
    visible = bool(hwnd and ctypes.windll.user32.IsWindowVisible(ctypes.c_void_p(hwnd)))
    minimized = bool(hwnd and ctypes.windll.user32.IsIconic(ctypes.c_void_p(hwnd)))
    value.update(visible=visible, minimized=minimized)
    if not visible or minimized:
        raise RuntimeError("The existing test game must be visible and not minimized")
    return value


def verify_runtime(rcon: Rcon, map_name: str) -> dict:
    expected = {"mapname": map_name, "dedicated": "0", "fs_game": "tce2", "net_port": str(rcon.port)}
    values = {key: rcon.cvar(key) for key in expected}
    for key, value in expected.items():
        if values[key].lower() != value.lower():
            raise RuntimeError(f"Wrong running {key}: {values[key]!r}, expected {value!r}")
    for key in ("fs_basepath", "fs_homepath"):
        values[key] = rcon.cvar(key)
        if Path(values[key]).resolve() != INSTANCE.resolve():
            raise RuntimeError(f"Wrong {key}; refusing to control another installation")
    rows = positions(rcon.command("navprobe"))
    observers = [r for r in rows if r["name"] == "NavStudioObserver"]
    if len(observers) != 1 or observers[0]["team"] != 3:
        raise RuntimeError("Named local NavStudioObserver spectator missing")
    return dict(cvars=values, players=rows)


def provenance(args) -> dict:
    report_path = args.report or INSTANCE / "tce2/navgen" / (args.map + "_navmesh.json")
    way_path = args.way or report_path.with_suffix(".way")
    installed = INSTANCE / "omni-bot/et/nav" / (args.map + ".way")
    expected = {}
    if args.proof:
        expected = json.loads(args.proof.read_text(encoding="utf-8-sig"))
    report_hash = args.report_sha256 or expected.get("native_report", "")
    way_hash = args.way_sha256 or expected.get("way", "")
    if not SHA.fullmatch(report_hash) or not SHA.fullmatch(way_hash):
        raise RuntimeError("Supply --proof or both reviewed --way-sha256 and --report-sha256")
    report = json.loads(report_path.read_text(encoding="utf-8-sig"))
    if report.get("map") != args.map or report.get("generation_complete") is not True:
        raise RuntimeError("Native report is incomplete or belongs to another map")
    capacity = report.get("native_input_coverage", {}).get("capacity", {})
    if capacity.get("input_truncated") or capacity.get("input_validation_complete") is False:
        raise RuntimeError("Native input validation is truncated/incomplete")
    hashes = {str(report_path.resolve()): digest(report_path), str(way_path.resolve()): digest(way_path), str(installed.resolve()): digest(installed)}
    if hashes[str(report_path.resolve())] != report_hash.lower():
        raise RuntimeError("Native report no longer matches reviewed SHA-256")
    if hashes[str(way_path.resolve())] != way_hash.lower() or digest(installed) != way_hash.lower():
        raise RuntimeError("Installed WAY differs from reviewed generated WAY")
    for relative in ("ET.exe", "tce2/qagame_mp_x86.dll", "tce2/cgame_mp_x86.dll", "tce2/ui_mp_x86.dll", "omni-bot/omnibot_et.dll", "tce2/navprobe.lua", "tce2/navstudio-display.lua"):
        path = INSTANCE / relative
        if path.is_file():
            hashes[str(path)] = digest(path)
    if getattr(args,'tactics',False):
        for suffix in ('.gm','_goals.gm','_gt2.gm','_gt2_goals.gm','_gt5.gm','_gt5_goals.gm'):
            path=INSTANCE/'omni-bot/et/nav'/(args.map+suffix)
            if path.is_file():hashes[str(path)]=digest(path)
        for name in ('goal_camp.gm','goal_flag.gm','goal_flagreturn.gm','goal_pickupitems.gm',
                     'goal_tceobjective.gm','goal_tce2navprobe.gm','goal_checkstuck.gm'):
            path=INSTANCE/'omni-bot/et/scripts/goals'/name
            if not path.is_file():raise RuntimeError('Required tactical runtime goal script missing: '+str(path))
            hashes[str(path)]=digest(path)
    return dict(files=hashes, map=args.map, native_source=report.get("source"), full_map_verified=False,
                proof=str(args.proof) if args.proof else None)


class LogWindow:
    def __init__(self, source: Path, target: Path):
        if not source.is_file():
            raise RuntimeError("Required live log missing: " + str(source))
        self.source, self.target = source, target
        stat = source.stat()
        self.offset, self.inode = stat.st_size, stat.st_ino
        self.initial_offset = self.offset
        with source.open("rb") as stream:
            stream.seek(max(0, self.offset - 64))
            self.anchor = stream.read(min(64, self.offset))
        self.events = []
        self.stream = target.open("xb")

    def poll(self):
        stat = self.source.stat()
        with self.source.open("rb") as stream:
            stream.seek(max(0, self.offset - len(self.anchor)))
            anchor_matches = stream.read(len(self.anchor)) == self.anchor
        if stat.st_ino != self.inode or stat.st_size < self.offset or not anchor_matches:
            self.events.append(dict(epoch=time.time(), reason="log_replaced_or_truncated", previous_offset=self.offset))
            self.offset, self.inode = 0, stat.st_ino
        with self.source.open("rb") as stream:
            stream.seek(self.offset)
            data = stream.read()
            self.offset = stream.tell()
            stream.seek(max(0, self.offset - 64))
            self.anchor = stream.read(min(64, self.offset))
        self.stream.write(data)
        self.stream.flush()

    def close(self):
        self.stream.close()


def screenshot_motion(previous: dict, actor: dict, before: dict, after: dict) -> dict:
    """Bound ordinary TCE motion by measured telemetry windows, not file latency.

    Native origins are sampled during each RCON request. Use the earliest prior
    request time and latest current response time, including that uncertainty.
    G_Active:320*1.25; CmdScale:sprint .744, optional doubled input, light load1.05.
    Vertical envelope includes jump337.5, gravity1000 and one18-unit step. This
    deliberately selects ordinary movement, rather than certifying arbitrary
    launchers, developer speeds or an already fast fall for visual evidence.
    """
    elapsed = after['finished_monotonic'] - before['started_monotonic']
    if not math.isfinite(elapsed) or elapsed <= 0:
        raise RuntimeError('Invalid screenshot telemetry time interval')
    horizontal = math.dist(actor['position'][:2], previous['position'][:2])
    vertical = abs(actor['position'][2] - previous['position'][2])
    max_speed = 320 * 1.25 * .744 * 2 * 1.05
    horizontal_limit = max_speed * elapsed + 2
    vertical_limit = 337.5 * elapsed + .5 * 1000 * elapsed * elapsed + 18
    if horizontal > horizontal_limit or vertical > vertical_limit:
        raise RuntimeError('Follow actor exceeded physically possible ordinary movement over '
                           f'{elapsed:.3f}s; respawn/teleport cannot be excluded')
    return dict(elapsed_upper_seconds=elapsed, horizontal_units=horizontal,
                horizontal_limit_units=horizontal_limit, maximum_horizontal_speed=max_speed,
                vertical_units=vertical, vertical_limit_units=vertical_limit)


def fresh_screenshot(rcon: Rcon, output: Path, index: int, row: dict) -> dict:
    folder = INSTANCE / "tce2/screenshots"
    attempts, tried = [], set()

    def snapshot():
        window = dict(started_monotonic=time.monotonic(), started_epoch=time.time())
        reply = rcon.command("navprobe")
        window.update(finished_monotonic=time.monotonic(), finished_epoch=time.time())
        state = re.search(r"NAVSTATE (\d+) GT (\d+)", reply)
        if not state:
            raise RuntimeError("Screenshot telemetry has no game/GT state")
        return positions(reply), list(state.groups()), window

    def confirm(previous, current, expected_state, current_state, before_window, after_window):
        matching = [p for p in current if p["slot"] == previous["slot"]]
        if len(matching) != 1:
            raise RuntimeError("Follow actor disappeared or its slot is ambiguous")
        actor = matching[0]
        if (clean(actor["name"]) != clean(previous["name"]) or actor["team"] != previous["team"] or
                actor["team"] not in (1, 2)):
            raise RuntimeError("Follow actor name/team changed")
        if actor["health"] <= 0:
            raise RuntimeError("Follow actor died before screenshot confirmation")
        if actor["health"] > previous["health"]:
            raise RuntimeError("Follow actor health increased; respawn/restart cannot be excluded")
        if current_state != expected_state:
            raise RuntimeError("Game/GT state changed during screenshot")
        motion = screenshot_motion(previous, actor, before_window, after_window)
        observer = [p for p in current if p['name'] == 'NavStudioObserver' and p['team'] == 3]
        if len(observer) != 1 or math.dist(observer[0]['position'], actor['position']) > 1:
            raise RuntimeError('Observer does not confirm the requested follow actor')
        return actor, motion

    for number in range(1, 6):
        attempt = dict(attempt=number, started_epoch=time.time(), status="pending")
        attempts.append(attempt)
        try:
            # Refresh after a possible round restart on every bounded retry.
            # Follow changes only the observer, never a bot's position.
            rcon.command("cg_omnibotdrawing 1")
            rcon.command("bot waypoint_view on")
            rcon.command("cg_omnibot_render_distance 800")
            current, state, initial_window = snapshot()
            alive = [p for p in current if p["health"] > 0 and p["team"] in (1, 2)]
            if not alive:
                raise RuntimeError("No currently alive team actor for screenshot")
            untried = [p for p in alive if (p["slot"], clean(p["name"])) not in tried]
            preferred = next((p for p in alive if number == 1 and p["slot"] == row["slot"] and
                              clean(p["name"]) == clean(row["name"])), None)
            actor = preferred or max(untried or alive, key=lambda p: p["health"])
            tried.add((actor["slot"], clean(actor["name"])))
            attempt.update(follow_requested=actor, game_state=state, stage="follow",
                           initial_snapshot_window=initial_window)
            reply = rcon.command(f"follow {actor['slot']}")
            attempt["follow_reply"] = reply
            time.sleep(.5)
            current, after_state, follow_window = snapshot()
            attempt["post_follow_snapshot"] = dict(players=current, game_state=after_state, window=follow_window)
            after_wait, follow_motion = confirm(actor, current, state, after_state, initial_window, follow_window)
            attempt['follow_motion'] = follow_motion
            attempt.update(after_follow_wait=after_wait, stage="capture")
            before = {p.name: (p.stat().st_mtime_ns, p.stat().st_size) for p in folder.glob("*.jpg")}
            epoch = time.time()
            attempt["requested_epoch"] = epoch
            rcon.command("screenshotJPEG")
            deadline = time.monotonic() + 12
            captured = None
            while time.monotonic() < deadline and captured is None:
                fresh = [p for p in folder.glob("*.jpg") if (p.stat().st_mtime_ns, p.stat().st_size) != before.get(p.name)]
                for source in sorted(fresh, key=lambda p: p.stat().st_mtime_ns, reverse=True):
                    raw = source.read_bytes()
                    if len(raw) > 1000 and raw[:2] == b"\xff\xd8" and raw[-2:] == b"\xff\xd9":
                        captured = source, raw
                        break
                if captured is None:
                    time.sleep(.2)
            if captured is None:
                raise RuntimeError("No fresh complete engine JPEG")
            source, raw = captured
            attempt.update(engine_file=str(source), sha256=hashlib.sha256(raw).hexdigest(), stage="confirm_capture")
            current, capture_state, capture_window = snapshot()
            attempt["post_capture_snapshot"] = dict(players=current, game_state=capture_state, window=capture_window)
            after_capture, capture_motion = confirm(after_wait, current, state, capture_state, follow_window, capture_window)
            _, total_motion = confirm(actor, current, state, capture_state, initial_window, capture_window)
            attempt.update(capture_motion=capture_motion, total_motion=total_motion)
            attempt.update(after_capture=after_capture, status="accepted", stage="complete",
                           finished_epoch=time.time())
        except RuntimeError as exc:
            attempt.update(status="rejected", reason=str(exc), finished_epoch=time.time())
            continue
        # Publish only after both telemetry checks. Rejected engine JPEGs stay
        # in their original folder, never under a valid screenshot evidence name.
        target = output / f"screenshot-{index:03d}.jpg"
        with target.open("xb") as stream:
            stream.write(raw)
        value = dict(image=str(target.resolve()), engine_file=str(source), requested_epoch=epoch,
                     sha256=hashlib.sha256(raw).hexdigest(), follow_requested=actor, follow_reply=reply,
                     follow_after_wait=after_wait, follow_after_capture=after_capture, attempts=attempts,
                     telemetry_actor_stable=True, fresh_complete_engine_jpeg=True,
                     visually_reviewed=False, bot_positions_modified=False)
        save(target.with_suffix(".json"), value)
        return value
    failure = output / f"screenshot-{index:03d}-attempts.json"
    save(failure, dict(attempts=attempts, visually_reviewed=False, bot_positions_modified=False))
    raise RuntimeError("No stable alive actor/fresh engine JPEG after 5 attempts; visual test cannot be claimed. " + str(failure))


# Literal GameMonkey VM/compile diagnostics from gmThread.cpp and
# gmCodeTree.cpp. Anchor the new messages after ET color removal: ordinary
# traffic failures, player chat and printed source lines are not VM errors.
GM_VM_ERROR = re.compile(
    r"^(?:attempt to call non function type|stack (?:over|under)flow|"
    r"(?:getdot|setdot|setind|setthis) failed\.|"
    r"(?:getdot|setdot) failed on '.+'|getthis failed\. this is null|"
    r"foreach expression (?:has no iterator function|is not table type)|"
    r"unary operator .+ (?:for type .+ error|undefined for type .+)|"
    r"operator .+ (?:bad operands .+ and .+|bad operand .+ for .+|"
    r"undefined for type .+ and .+|failed)|"
    r"User break\. Execution halted\.|error \(\d+\) .+|Could not parse file .+)\s*$",
    re.I)


def script_error_lines(lines):
    return [s for s in lines if
            re.search(r"exception|script error|unknown member|unknown function|error:",s,re.I) or
            GM_VM_ERROR.fullmatch(clean(s).strip())]


def classify(console: str, game: str, objective_samples=None) -> dict:
    lines = console.splitlines()
    kills = [s for s in game.splitlines() if re.search(r"\bKill:\s", s)]
    objectives = summarize_objectives(objective_samples)
    return dict(kills=kills, suicides=[s for s in kills if "MOD_SUICIDE" in s],
                arrivals=[s for s in lines if "TCE2EXPLORE ARRIVE" in s],
                failures=[s for s in lines if "FAILURE_DETAIL" in s or
                          re.search(r"TCE2PATH (?:BLOCKED_ORDINARY|BLOCKED_DESTINATION)\b|TCE2DOOR BLOCKED\b", s)],
                timeouts=[s for s in lines if "TCE2EXPLORE TIMEOUT" in s or "NAVPATH TIMEOUT" in s],
                recoveries=[s for s in lines if "TCE2STUCK RECOVERY" in s],
                terminal_stuck=[s for s in lines if "TCE2STUCK TERMINAL" in s],
                traffic_blocked=[s for s in lines if "TCE2EXPLORE BLOCKED_TRAFFIC" in s],
                traffic_yields=sum("TCE2TRAFFIC YIELD" in s for s in lines),
                errors=script_error_lines(lines),
                objective_completion_messages=[s for s in lines if re.search(r"TCEOBJECTIVE (?:CAPTURED|PLANTED|DEFUSED)\b",s)],
                objective_completions=objectives['completions'],
                objective_native_evidence=objectives,
                objective_failures=[s for s in lines if re.search(r"TCEOBJECTIVE (?:NO_PATH|USE_TIMEOUT|NATIVE_OBSERVER_MISSING)\b",s)],
                round_or_map_initializations=[s for s in game.splitlines() if "InitGame:" in s])


def dumps() -> dict:
    return {str(p): (p.stat().st_mtime_ns, p.stat().st_size) for p in INSTANCE.rglob("*.dmp")}


def qualify_combat(result: dict, output_directory: Path) -> dict:
    """Pure, current-file postflight; never run commands or rewrite evidence.

    A finished observation may contain real navigation/VM failures. Keep that
    fact separate from this quality gate. Human image review and completion of
    missions remain separate; a kill must join two observed opposing actors
    before this reader calls the window observed combat.
    """
    reasons, raw_files, raw_text = [], {}, {}
    def reject(reason):
        if reason not in reasons:reasons.append(reason)
    def number(value):
        return type(value) in (int,float) and math.isfinite(value)
    directory=Path(output_directory).resolve()
    if not isinstance(result,dict):
        return dict(quality_contract='combat_window_quality_v1',quality_pass=False,
                    qualification_reasons=['invalid_result'],combat_observed=False,
                    combat_observation_evidence=dict(reason='Invalid result.'),raw_evidence={})
    observed=result.get('observation_complete',result.get('complete'))
    if observed is not True:reject('observation_incomplete')
    if result.get('complete') is not True:reject('operational_completion_incomplete')
    requested=result.get('requested_bots');seconds=result.get('requested_seconds')
    if type(requested)is not int or requested<2:reject('invalid_requested_bot_count')
    duration=result.get('combat_observation_seconds')
    if not number(seconds)or seconds<=0 or not number(duration)or duration<seconds:reject('combat_window_short_or_unmeasured')
    if result.get('aborted_reason')or result.get('error'):reject('observation_aborted')
    if result.get('log_discontinuities'):reject('log_discontinuity')
    if result.get('new_or_changed_dumps')!=[]:reject('new_dump_or_missing_dump_inventory')
    if result.get('bot_teleport_commands_used')is not False:reject('bot_teleport_not_excluded')
    if result.get('population_mode')!='existing_native_bots_no_population_mutation':
        if result.get('population_restoration',{}).get('confirmed')is not True:reject('population_restoration_unconfirmed')
        if result.get('setup_round_hold_released_before_observation')is not True:reject('setup_round_hold_not_released')
    before=result.get('process_before',{});after=result.get('process_after',{})
    if (not isinstance(before,dict)or not isinstance(after,dict)or type(before.get('pid'))is not int or before.get('pid',0)<=0 or
            any(not before.get(k)or before.get(k)!=after.get(k)for k in('pid','path','start_ticks'))):
        reject('process_identity_changed_or_missing')
    for when in('runtime_before','runtime_after'):
        loaded=result.get(when,{}).get('cvars',{}).get('mapname');expected=result.get('map')
        if not isinstance(loaded,str)or not isinstance(expected,str)or loaded.lower()!=expected.lower():reject('runtime_map_changed_or_missing')
    pbefore=result.get('provenance_before',{}).get('files',{});pafter=result.get('provenance_after',{}).get('files',{})
    if not isinstance(pbefore,dict)or not pbefore or pbefore!=pafter or result.get('files_unchanged')is not True:
        reject('provenance_changed_or_missing')
    else:
        for path,expected in pbefore.items():
            try:
                if not isinstance(expected,str)or not SHA.fullmatch(expected)or digest(Path(path))!=expected.lower():
                    reject('provenance_current_file_mismatch')
            except (OSError,TypeError,ValueError):reject('provenance_current_file_unavailable')
    samples=result.get('samples',[]);counts=[];actors={};sample_times=[]
    if not isinstance(samples,list)or not samples:reject('population_samples_missing');samples=[]
    for sample in samples:
        rows=sample.get('players')if isinstance(sample,dict)else None
        tick=sample.get('time')if isinstance(sample,dict)else None
        if not isinstance(rows,list)or not number(tick)or tick<0:
            reject('population_sample_malformed');continue
        counts.append(len(rows));sample_times.append(tick);slots=set();names=set();teams=set()
        for row in rows:
            if (not isinstance(row,dict)or type(row.get('slot'))is not int or
                    not isinstance(row.get('name'),str)or not row['name']or row.get('team')not in(1,2)or
                    type(row.get('health'))is not int or not isinstance(row.get('position'),list)or
                    len(row['position'])!=3 or not all(number(v)for v in row['position'])or
                    row['slot']in slots or row['name']in names):
                reject('population_actor_invalid_or_duplicate');continue
            slots.add(row['slot']);names.add(row['name']);teams.add(row['team'])
            actors.setdefault(row['slot'],set()).add((clean(row['name']),row['team']))
        if teams!={1,2}:reject('both_bot_teams_not_sampled')
    minimum=min(counts,default=0)
    if type(requested)is not int or minimum<requested:reject('population_below_requested')
    if result.get('minimum_sampled_bot_count')!=minimum:reject('population_summary_mismatch')
    if sample_times!=sorted(sample_times):reject('population_sample_times_not_ordered')
    # Require the actual source windows, not merely old summary arrays. RCON
    # VM replies are included because redirected script errors can be absent
    # from the client console window.
    commands=[]
    for filename in('console-window.log','game-window.log','commands.jsonl'):
        try:
            raw=(directory/filename).read_bytes()
            raw_files[filename]=dict(sha256=hashlib.sha256(raw).hexdigest(),bytes=len(raw))
            raw_text[filename]=raw.decode('latin1')
            if filename=='commands.jsonl':
                commands=[json.loads(line)for line in raw_text[filename].splitlines()if line.strip()]
                if not commands or any(not isinstance(row,dict)or not isinstance(row.get('reply'),str)for row in commands):
                    raise ValueError('Missing command replies')
        except (OSError,ValueError,TypeError):reject('raw_window_missing_or_invalid:'+filename)
    replies='\n'.join(row['reply']for row in commands if isinstance(row,dict)and isinstance(row.get('reply'),str))
    if result.get('population_mode')=='existing_native_bots_no_population_mutation':
        registry={(int(slot),clean(name).strip())for slot,name in re.findall(r'\bNAVLIVEBOT\s+(\d+)\s+([^\r\n]+)',clean(replies))}
        initial=result.get('initial_registered_bots',{})
        if not isinstance(initial,dict)or type(requested)is not int or len(initial)<requested:
            reject('native_bot_registry_missing_or_short')
        if any((slot,name)not in registry for slot,identities in actors.items()for name,team in identities):
            reject('sampled_actor_not_native_registered_bot')
    elif type(requested)is int:
        controlled={f'NavCheck{i+1:02d}'for i in range(requested)}
        if any(name not in controlled for identities in actors.values()for name,team in identities):
            reject('sampled_actor_not_controlled_bot')
    console=raw_text.get('console-window.log','');game=raw_text.get('game-window.log','')
    events=classify(clean(console+'\n'+game+'\n'+replies),clean(game),result.get('objective_samples'))
    event_fields=('errors','failures','timeouts','suicides','terminal_stuck','objective_failures')
    for key in event_fields:
        if events[key]or result.get(key):reject('raw_or_recorded_'+key)
    if any(row.get('error')for row in commands if isinstance(row,dict)):reject('rcon_command_error')
    gametypes={int(value)for value in re.findall(r'\bNAVSTATE\s+\d+\s+GT\s+(\d+)\b',replies)}
    for state in result.get('states',[]):
        if isinstance(state,dict):gametypes.update(int(value)for value in re.findall(r'\bGT\s+(\d+)\b',str(state.get('state',''))))
    if len(gametypes)!=1:reject('observed_gametype_missing_or_changed')
    screenshots=result.get('screenshots',[]);valid_images=[]
    if not isinstance(screenshots,list)or not screenshots:reject('fresh_engine_screenshots_missing');screenshots=[]
    for shot in screenshots:
        try:
            image=Path(shot['image']).resolve();raw=image.read_bytes()
            if (not image.is_relative_to(directory)or len(raw)<=1000 or not raw.startswith(b'\xff\xd8')or not raw.endswith(b'\xff\xd9')or
                    hashlib.sha256(raw).hexdigest()!=shot.get('sha256')or shot.get('fresh_complete_engine_jpeg')is not True or
                    shot.get('telemetry_actor_stable')is not True or shot.get('bot_positions_modified')is not False):
                raise ValueError('Image or telemetry binding changed')
            sidecar=json.loads(image.with_suffix('.json').read_bytes())
            fields=('image','sha256','engine_file','requested_epoch','follow_requested','follow_after_wait',
                    'follow_after_capture','attempts','telemetry_actor_stable','fresh_complete_engine_jpeg','bot_positions_modified')
            if any(shot.get(key)!=sidecar.get(key)for key in fields):raise ValueError('Screenshot sidecar differs')
            epoch=shot.get('requested_epoch');start=result.get('started_epoch');end=result.get('combat_observation_finished_epoch')
            if not all(number(value)for value in(epoch,start,end))or not start<=epoch<=end:raise ValueError('Screenshot is outside observation')
            valid_images.append(dict(path=str(image),sha256=shot['sha256']))
        except (OSError,ValueError,KeyError,TypeError):reject('screenshot_or_telemetry_binding_invalid')
    # Kill logs identify client slots. Join only slots whose sampled name and
    # team stayed unambiguous across this window; world/suicide and friendly
    # kills are not evidence of an enemy firefight. No attack/damage telemetry
    # is invented when the engine does not log it.
    enemy=[]
    for line in events['kills']:
        match=re.search(r'\bKill:\s+(-?\d+)\s+(-?\d+)\s+(-?\d+):',line)
        if not match:continue
        killer,victim=int(match[1]),int(match[2]);left=actors.get(killer,set());right=actors.get(victim,set())
        names=re.search(r':\s*(.*?) killed (.*?) by \S+(?:[ \t]+\([^\r\n)]*\))?[ \t]*$',line[match.end()-1:])
        if (killer!=victim and len(left)==len(right)==1 and next(iter(left))[1]!=next(iter(right))[1] and
                names and names[1]==next(iter(left))[0] and names[2]==next(iter(right))[0] and 'MOD_SUICIDE'not in line):
            enemy.append(dict(killer=killer,victim=victim,line=line))
    combat=bool(enemy)
    if not combat:reject('enemy_combat_not_observed')
    return dict(quality_contract='combat_window_quality_v1',quality_pass=not reasons,qualification_reasons=reasons,
                combat_observed=combat,combat_observation_evidence=dict(enemy_kill_count=len(enemy),enemy_kills=enemy,
                    attack_events=None,damage_events=None,reason=None if combat else
                    'No logged enemy kill between uniquely observed opposing actors; attack/damage telemetry unavailable.'),
                raw_evidence=dict(files=raw_files,event_counts={key:len(events[key])for key in event_fields},
                    event_lines={key:events[key]for key in event_fields},minimum_sampled_bot_count=minimum,
                    observed_gametypes=sorted(gametypes),valid_engine_screenshots=valid_images,
                    scope='Console/game combat windows and setup/cleanup command replies, plus current file hashes; human image review, mission completion and full-map coverage remain separate.'))


def run(args) -> int:
    if os.name != "nt":
        raise RuntimeError("This live harness requires the existing Windows test instance")
    if args.out.resolve() == INSTANCE.resolve() or not args.out.resolve().is_relative_to((ROOT / "build").resolve()):
        raise RuntimeError("Use a new evidence subdirectory under build")
    args.out.mkdir(parents=True, exist_ok=False)
    result = dict(schema=1, map=args.map, requested_seconds=args.seconds, requested_bots=args.bots,
                  output=str(args.out.resolve()), claim="Bounded combat and visual evidence only; no whole-map or indefinite stability claim.",
                  bot_teleport_commands_used=False, screenshots=[], samples=[], complete=False,observation_complete=False)
    logs = []
    rcon = None
    started = None
    before_dumps = dumps()
    pause_token = None
    setup_hold = False
    try:
        with serial_lock():
            before = provenance(args)
            result["provenance_before"] = before
            initial_identity = process_identity(args.port)
            result["process_before"] = initial_identity
            rcon = Rcon(args.port, args.password, args.out)
            result["runtime_before"] = verify_runtime(rcon, args.map)
            # Cvars guarantee renderer limits are recorded, not silently changed.
            result["renderer"] = {key: rcon.cvar(key) for key in ("r_maxpolyverts", "r_maxpolys", "cg_omnibot_render_distance")}
            log_name = rcon.cvar("g_log")
            if not log_name or Path(log_name).is_absolute() or ".." in Path(log_name).parts:
                raise RuntimeError("Expected a relative test game log")
            game_log = INSTANCE / "tce2" / log_name
            # TC restarts the map once at the end of its initial countdown,
            # even when g_doWarmup is zero. Wait before saving a VM table.
            deadline=time.monotonic()+60
            while rcon.cvar('gamestate')!='0':
                if time.monotonic()>=deadline:
                    raise RuntimeError('Initial warmup did not settle before the live run')
                if rcon.cvar('mapname')!=args.map:
                    raise RuntimeError('Map changed during initial warmup')
                time.sleep(.25)
            if args.existing_bots:
                # Observe the normal GT5 round population. TC intentionally
                # prevents late joiners from spawning midway through a round.
                # No kick, respawn, teleport or population override is used.
                registry_path=INSTANCE/'navgen'/('navstudio-live-'+uuid.uuid4().hex[:16]+'-bots.gm')
                registry_path.parent.mkdir(exist_ok=True)
                registry_path.write_text('foreach(b in BotTable){print("NAVLIVEBOT",b.GetGameId(),b.Name);}',encoding='utf8')
                def bot_registry():
                    reply=rcon.command('bot script_runfile homepath/navgen/'+registry_path.name)
                    values={int(slot):name.strip() for slot,name in re.findall(r'NAVLIVEBOT\s+(\d+)\s+([^\r\n]+)',reply)}
                    if len(values)<args.bots:raise RuntimeError('Fewer native registered bots than requested: '+reply)
                    return values
                registered=bot_registry()
                names=set(registered.values())
                rows=positions(rcon.command('navprobe'))
                registered_players(rows, registered)
                if not all(any(r['slot'] in registered and r['team']==team and r['health']>0 for r in rows) for team in (1,2)):
                    raise RuntimeError('Normal round has no alive bots on both teams')
                result['population_mode']='existing_native_bots_no_population_mutation'
                result['initial_registered_bots']=registered
            else:
                # Removing every player otherwise drives TC into waiting/new-round
                # transitions while the eight controlled bots are still joining.
                # Freeze those transitions only during setup, never during combat.
                held=rcon.command('tce_navgen_authoring on')
                if 'NAVSTUDIO_AUTHORING_ON' not in held:
                    raise RuntimeError('Cannot hold round transitions during bot setup: '+held)
                setup_hold=True
                rcon.command("bot waypoint_load")
                # This is a bot command, so it preserves the spectator. Path, process,
                # UDP owner, map and listen mode have all been checked before mutation.
                sys.path.insert(0,str(ROOT/'navstudio'))
                import quiet
                pause_token=uuid.uuid4().hex[:16]
                quiet_path=INSTANCE/'navgen'/('navstudio-live-'+pause_token+'-begin.gm')
                quiet_path.parent.mkdir(exist_ok=True)
                quiet_path.write_text(quiet.file_source(quiet.begin_script(pause_token)),encoding='utf8')
                paused=rcon.command('bot script_runfile homepath/navgen/'+quiet_path.name)
                if 'NAVSTUDIO_QUIET_OK' not in paused:
                    raise RuntimeError('Cannot suspend automatic bot refill for a controlled live run: '+paused)
                rcon.command("bot kickall")
                # MaxBots is a fill target, not merely a ceiling: setting it to 8
                # would create random bots between the eight named add commands.
                rcon.command('bot script_run Server.MaxBots = -1;')
                for index in range(args.bots):
                    team, cls = 1 + index % 2, (1, 3, 5)[index % 3]
                    rcon.command(f"bot addbot {team} {cls} NavCheck{index + 1:02d}")
                names = {f"NavCheck{i + 1:02d}" for i in range(args.bots)}
                deadline = time.monotonic() + 45
                while time.monotonic() < deadline:
                    rows = positions(rcon.command("navprobe"))
                    active = [r for r in rows if r["name"] in names and r["team"] in (1, 2)]
                    other = [r for r in rows if r["name"] not in names and r["name"] != "NavStudioObserver"]
                    if other:
                        raise RuntimeError("Unexpected players after test bot setup; refusing a mixed observation")
                    if len(active) == args.bots and all(r['health']>0 for r in active):
                        break
                    time.sleep(.3)
                else:
                    raise RuntimeError("Requested named bots did not join both teams")
                released=rcon.command('tce_navgen_authoring off')
                if 'NAVSTUDIO_AUTHORING_OFF' not in released:
                    raise RuntimeError('Round transition hold release was not confirmed: '+released)
                setup_hold=False
                result['setup_round_hold_released_before_observation']=True
            rcon.command("bot waypoint_view on")
            rcon.command("cg_omnibotdrawing 1")
            rcon.command("cg_omnibot_render_distance 800")
            rcon.command("set omnibot_tce_objective_debug 1")
            logs.append(LogWindow(INSTANCE / "tce2/etconsole.log", args.out / "console-window.log"))
            logs.append(LogWindow(game_log, args.out / "game-window.log"))
            result["log_start_offsets"] = {str(log.source): log.initial_offset for log in logs}
            started = time.monotonic()
            result["started_epoch"] = time.time()
            next_shot, next_guard, next_objectives = 0, 0, 0
            result['objective_samples']=[]
            objective_probe=INSTANCE/'navgen'/('navstudio-live-'+uuid.uuid4().hex[:16]+'-objectives.gm')
            observer='if(Commands["tceobjectives"]){Commands["tceobjectives"].Func({});}else{print("NAVLIVE_OBJECTIVE_OBSERVER_UNAVAILABLE");}'
            if getattr(args,'tactics',False):
                observer+='''
foreach(bot in BotTable) {
 print("NAVLIVE_TACTIC",bot.GetGameId(),"team",bot.GetTeam(),"high",bot.GetHighLevelGoalName(),"goal",bot.GetMapGoalName(),"carrier",bot.IsCarryingFlag(),"bomb",bot.GetStat("tce_bomb_carrier"));
}
'''
            objective_probe.write_text(observer,encoding='utf8')
            shot_index = 0
            states = []
            while time.monotonic() - started < args.seconds:
                tick = time.monotonic() - started
                if tick >= next_guard:
                    identity = process_identity(args.port)
                    if any(identity[k] != initial_identity[k] for k in ("pid", "path", "start_ticks")):
                        raise RuntimeError("Test process changed during observation")
                    if rcon.cvar("mapname") != args.map:
                        raise RuntimeError("Map changed during observation")
                    next_guard = tick + 20
                reply = rcon.command("navprobe")
                rows = positions(reply)
                state = re.search(r"NAVSTATE (\d+) GT (\d+)", reply)
                if state and (not states or states[-1]["state"] != state[0]):
                    states.append(dict(time=tick, state=state[0]))
                if args.existing_bots and tick >= next_objectives:
                    registered=bot_registry();names=set(registered.values())
                current = (registered_players(rows, registered) if args.existing_bots else
                           [r for r in rows if r["name"] in names and r["team"] in (1, 2)])
                if tick >= next_objectives:
                    # The command-table dispatcher schedules callbacks after
                    # the RCON redirect ends. Invoke the same read-only function
                    # directly so this sample receives its actual native data.
                    result['objective_samples'].append(dict(time=round(tick,3),reply=rcon.command(
                        'bot script_runfile homepath/navgen/'+objective_probe.name)))
                    next_objectives=tick+5
                if not args.existing_bots and any(r["name"] not in names and r["name"] != "NavStudioObserver" for r in rows):
                    raise RuntimeError("Unexpected player joined the observation")
                result["samples"].append(dict(time=round(tick, 3), players=current))
                for log in logs:
                    log.poll()
                alive = [r for r in current if r["health"] > 0]
                if alive and tick >= next_shot:
                    target = alive[shot_index % len(alive)]
                    shot_index += 1
                    result["screenshots"].append(fresh_screenshot(rcon, args.out, shot_index, target))
                    next_shot = tick + args.screenshot_interval
                time.sleep(.25)
            result["states"] = states
            result["combat_observation_finished_epoch"] = time.time()
            result["combat_observation_seconds"] = round(time.monotonic() - started, 3)
            result['observation_complete']=True
            result["process_after"] = process_identity(args.port)
            result["runtime_after"] = verify_runtime(rcon, args.map)
            result["provenance_after"] = provenance(args)
            result["files_unchanged"] = result["provenance_after"]["files"] == before["files"]
            if not result["files_unchanged"]:
                raise RuntimeError("Evidence files changed during run")
            if not result["screenshots"]:
                raise RuntimeError("No alive bot screenshot obtained")
            result["complete"] = True
    except (Exception, KeyboardInterrupt) as exc:
        result["aborted_reason"] = type(exc).__name__ + ": " + str(exc)
    finally:
        result["finished_epoch"] = time.time()
        result["elapsed_seconds"] = round(time.monotonic() - started, 3) if started else 0
        for log in logs:
            try:
                log.poll()
            except Exception as exc:
                log.events.append(dict(epoch=time.time(), reason=str(exc)))
            log.close()
        result["log_discontinuities"] = {str(log.source): log.events for log in logs if log.events}
        if len(logs) == 2:
            result.update(classify(logs[0].target.read_text(encoding="latin1"),
                logs[1].target.read_text(encoding="latin1"), result.get('objective_samples')))
        if result["log_discontinuities"]:
            result["complete"] = False
            result["log_count_limitation"] = "Log discontinuity prevents an uninterrupted event-count claim."
        result["new_or_changed_dumps"] = [p for p, stat in dumps().items() if before_dumps.get(p) != stat]
        all_rows = [r for sample in result["samples"] for r in sample["players"]]
        result["observed_bot_names"] = sorted({r["name"] for r in all_rows})
        result["observed_alive_bot_names"] = sorted({r["name"] for r in all_rows if r["health"] > 0})
        result["minimum_sampled_bot_count"] = min((len(s["players"]) for s in result["samples"]), default=0)
        result["visually_reviewed"] = False
        if pause_token and rcon:
            try:
                # Restore only the same owned renderer. A replacement process
                # must not receive cleanup commands for this observation.
                identity=process_identity(args.port)
                if any(identity[k]!=initial_identity[k] for k in ('pid','path','start_ticks')):
                    raise RuntimeError('Test process changed; bot restoration refused')
                import quiet
                held=rcon.command('tce_navgen_authoring on')
                if 'NAVSTUDIO_AUTHORING_ON' not in held:
                    raise RuntimeError('Cannot hold round transitions during bot restoration: '+held)
                setup_hold=True
                restore=INSTANCE/'navgen'/('navstudio-live-'+pause_token+'-restore.gm')
                restore.write_text(quiet.file_source(quiet.restore_script(pause_token)),encoding='utf8')
                rcon.command('bot kickall')
                reply=rcon.command('bot script_runfile homepath/navgen/'+restore.name)
                restored='NAVSTUDIO_QUIET_RESTORED' in reply
                result['population_restoration']=dict(confirmed=restored,reply=reply)
                if not restored:result['complete']=False
                released=rcon.command('tce_navgen_authoring off')
                if 'NAVSTUDIO_AUTHORING_OFF' not in released:
                    raise RuntimeError('Round hold release after restoration was not confirmed')
                setup_hold=False
            except Exception as exc:
                result['population_restoration']=dict(confirmed=False,error=str(exc))
                result['complete']=False
        if setup_hold and rcon:
            try:
                identity=process_identity(args.port)
                if any(identity[k]!=initial_identity[k] for k in ('pid','path','start_ticks')):
                    raise RuntimeError('Test process changed; setup hold release refused')
                result['setup_hold_cleanup_reply']=rcon.command('tce_navgen_authoring off')
            except Exception as exc:
                result['setup_hold_cleanup_error']=str(exc)
            result['complete']=False
        if rcon:
            rcon.stream.close()
        result.update(qualify_combat(result,args.out))
        save(args.out / "result.json", result)
        summary = {k: len(v) if isinstance(v, list) else v for k, v in result.items() if k not in ("samples", "runtime_before", "runtime_after", "process_before", "process_after", "provenance_before", "provenance_after")}
        summary['qualification_reasons']=result['qualification_reasons']
        print(json.dumps(summary, indent=2))
    return 0 if result["complete"] and result['quality_pass'] else 1


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--map", required=True)
    p.add_argument("--seconds", type=int, default=180)
    p.add_argument("--out", type=Path, required=True, help="New immutable evidence directory under build")
    p.add_argument("--bots", type=int, default=8)
    p.add_argument("--existing-bots", action="store_true", help="Observe native round bots; never replace the GT5 population.")
    p.add_argument("--tactics", action="store_true", help="Read active goal names for spatial/role distribution evidence.")
    p.add_argument("--port", type=int, default=27986)
    p.add_argument("--password", default=os.environ.get("TCE_NAV_RCON_PASSWORD", "navgen_local"))
    p.add_argument("--report", type=Path)
    p.add_argument("--way", type=Path)
    p.add_argument("--proof", type=Path)
    p.add_argument("--report-sha256")
    p.add_argument("--way-sha256")
    p.add_argument("--screenshot-interval", type=int, default=30)
    args = p.parse_args(argv)
    if not NAME.fullmatch(args.map):
        p.error("Invalid map name")
    if not 10 <= args.seconds <= 3600 or not 2 <= args.bots <= 16 or args.bots % 2:
        p.error("Use 10..3600 seconds and an even bot count of 2..16")
    if not 15 <= args.screenshot_interval <= 300 or not 1024 <= args.port <= 65535:
        p.error("Screenshot interval must be 15..300 seconds; invalid port")
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,80}", args.password):
        p.error("Invalid RCON credential characters")
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
