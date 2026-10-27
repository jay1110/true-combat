#!/usr/bin/env python3
"""Focused current-source proof obligations for live ET_PLAYER time/time2.

This checks concrete event constructor routes and consumer dispatch, not all C
pointer aliases. It intentionally does not wire or certify lifecycle clears
that do not exist yet. A new dynamic event route requires explicit review.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]
TARGETS = {'EV_SMOKE', 'EV_OILPARTICLES', 'EV_SNOWFLURRY'}


def clean(text):
    token = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*')
    return token.sub(lambda m: re.sub(r'[^\n]', ' ', m[0]) if m[0].startswith(('/', '/*')) else m[0], text)


def calls(text, names):
    for match in re.finditer(r'\b(' + '|'.join(names) + r')\s*\(', text):
        start = match.end(); depth = 1; quoted = None; escape = False; at = start; fields = []; part = start
        while at < len(text) and depth:
            c = text[at]
            if quoted:
                if escape:
                    escape = False
                elif c == '\\':
                    escape = True
                elif c == quoted:
                    quoted = None
            elif c in ('"', "'"):
                quoted = c
            elif c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
            elif c == ',' and depth == 1:
                fields.append(text[part:at].strip()); part = at + 1
            at += 1
        fields.append(text[part:at - 1].strip())
        yield match[1], fields, match.start()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(); args.out.mkdir(parents=True, exist_ok=True)
    checks = 0; errors = []

    def require(ok, detail):
        nonlocal checks
        checks += 1
        if not ok:
            errors.append(detail)

    routes = []; dynamic = []; hashes = {}
    signatures = {'G_TempEntity': 1, 'G_AddEvent': 1, 'G_AddPredictableEvent': 1,
                  'BG_AddPredictableEventToPlayerstate': 0, 'PM_AddEvent': 0, 'PM_AddEventExt': 0}
    allowed_dynamic = {('src/game/bg_pmove.c', 'fireEvent'), ('src/game/bg_pmove.c', 'event'),
                       ('src/game/bg_pmove.c', 'newEvent'), ('src/game/g_utils.c', 'event'),
                       ('src/game/g_items.c', 'makenoise')}
    for path in (ROOT / 'src/game').rglob('*'):
        if path.suffix not in ('.c', '.cpp', '.inc'):
            continue
        relative = path.relative_to(ROOT).as_posix()
        raw = path.read_text(encoding='utf-8', errors='replace'); text = clean(raw)
        relevant = False
        for name, fields, at in calls(text, signatures):
            if len(fields) <= signatures[name] or any(re.match(r'(?:const\s+)?(?:int|gentity_t|playerState_t)\b', f) for f in fields):
                continue  # Declaration/definition, not a call site.
            event = re.sub(r'\s+', ' ', fields[signatures[name]])
            event_names = set(re.findall(r'\bEV_\w+\b', event))
            item = dict(file=relative, line=text.count('\n', 0, at) + 1, constructor=name, event=event)
            if event_names & TARGETS:
                routes.append(item); relevant = True
                require(name == 'G_TempEntity', 'Duration event entered a non-temp route: ' + repr(item))
            if name != 'G_TempEntity' and not event_names:
                dynamic.append(item); relevant = True
                require((relative, event) in allowed_dynamic, 'New dynamic player-event expression needs review: ' + repr(item))
        if relevant:
            hashes[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
    require({e for r in routes for e in re.findall(r'\bEV_\w+\b', r['event'])} == TARGETS,
            'Expected three duration-reading event constructors were not all found')
    # Reviewed local dynamic assignments: neither item pickup nor fall/fire
    # selections can produce duration-reading particle event IDs.
    for rel, name, expected in [('src/game/g_items.c', 'makenoise', {'EV_ITEM_PICKUP', 'EV_ITEM_PICKUP_QUIET'}),
                                 ('src/game/bg_pmove.c', 'fireEvent', {'EV_FIRE_WEAPON_LASTSHOT', 'EV_FIRE_WEAPON'})]:
        text = clean((ROOT / rel).read_text())
        assigned = re.findall(r'\b' + name + r'\s*=([^;]+);', text)
        actual = {e for a in assigned for e in re.findall(r'\bEV_\w+\b', a)}
        require(actual == expected, 'Dynamic event assignment domain changed: ' + name)
    pmove = clean((ROOT / 'src/game/bg_pmove.c').read_text())
    landing = re.findall(r'\bevent\s*=([^;]+);', pmove)
    require(not any(set(re.findall(r'\bEV_\w+\b', a)) & TARGETS for a in landing),
            'Landing event selection acquired a duration event')

    event_source = clean(function((ROOT / 'src/cgame/cg_event.c').read_text(), 'CG_EntityEvent'))
    for label, consumer in [('EV_SMOKE', 'CG_ParticleSmoke'), ('EV_OILPARTICLES', 'CG_Particle_OilParticle'), ('EV_SNOWFLURRY', 'CG_ParticleSnowFlurry')]:
        at = event_source.index('case ' + label + ':')
        body = event_source[at:event_source.index('break;', at)]
        require(consumer + '(' in body, 'Duration event consumer changed: ' + label)
    # Non-event consumers are dispatched by nonplayer eType, not by CG_Player.
    ents = clean(function((ROOT / 'src/cgame/cg_ents.c').read_text(), 'CG_ProcessEntity'))
    player = ents[ents.index('case ET_CORPSE:'):ents.index('case ET_ITEM:')]
    require('CG_Player(cent)' in player and not any(c in player for c in ['CG_General(', 'CG_Smoker(', 'TCE_CG_Missile(', 'CG_RecipientRadarPositions(']), 'Player type dispatch changed')
    cgp = clean(function((ROOT / 'src/cgame/cg_players.c').read_text(), 'CG_Player'))
    require(not re.search(r'currentState\.time\b', cgp), 'New live player time reader')
    require(cgp.count('currentState.time2') == 2 and cgp.count('cent->currentState.eType == ET_CORPSE && cent->currentState.time2 == 1') == 2, 'Player time2 guard changed')
    for rel in ['src/game/bg_misc.c', 'src/game/g_client.c', 'src/cgame/cg_event.c', 'src/cgame/cg_players.c', 'src/cgame/cg_ents.c', 'src/cgame/cg_particles.c', 'src/game/g_utils.c']:
        hashes[rel] = hashlib.sha256((ROOT / rel).read_bytes()).hexdigest()
    report = dict(checks=checks, failures=errors, time_reading_event_routes=routes, reviewed_dynamic_routes=dynamic, source_hashes=hashes,
                  result='Current duration-reading event producers are temp-only; normal ET_PLAYER presentation does not consume time/time2. Reservation remains conditional on explicit lifecycle clears and same-snapshot association.',
                  limits=['No field is written or network protocol enabled.', 'Actual converter native fixture separately proves current stale-word preservation through ET_INVISIBLE.', 'Checks are a focused source graph regression, not a general C alias/reachability proof.', 'Accepted new call sites or consumers require explicit classification; do not add a blanket ignore.'])
    (args.out / 'carrier-audit.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps(dict(checks=checks, failures=errors, temp_routes=len(routes), dynamic_routes=len(dynamic)), indent=2))
    return int(bool(errors))


if __name__ == '__main__':
    raise SystemExit(main())
