"""Record real jump/fire/reload input in the one already-running test game.

Uses console input and the read-only observer. It makes no synthetic equality
claim: command dispatch, snapshots and screenshot timing are asynchronous.
"""
import json
from pathlib import Path
import re
import time

from live_compare import OUT, attach, rcon


def main():
    run = json.loads((OUT / 'current.json').read_text())
    observer = attach(run)
    report = {'run': run, 'loaded_modules': observer.modules, 'phases': []}
    deadline = time.monotonic() + 70
    while True:
        game_state = rcon('gamestate')
        actor = observer.capture()['server']
        if re.search(r'is:\"0\^7\"', game_state) and actor['pm_type'] == 0 and actor['stats'][0] > 0:
            report['ready'] = {'gamestate': game_state, 'actor': actor}
            break
        if time.monotonic() > deadline:
            raise RuntimeError('Real match/healthy actor not ready; input sequence is not a valid comparison')
        time.sleep(.2)

    def phase(name, commands, seconds):
        responses = [{'command': command, 'response': rcon(command, timeout=.02)} for command in commands]
        samples = []
        start = time.monotonic()
        shot = False
        while time.monotonic() - start < seconds:
            samples.append(observer.capture())
            if not shot and time.monotonic() - start > .2:
                rcon('screenshotJPEG parity-' + run['variant'] + '-' + name, timeout=.02)
                shot = True
            time.sleep(.01)
        report['phases'].append({'name': name, 'commands': responses, 'samples': samples})
        print(name, 'samples', len(samples), 'weapon states', sorted(set(s['server']['weaponstate'] for s in samples)),
              'weapon animations', sorted(set(s['server']['weapAnim'] for s in samples)),
              'height range', [min(s['server']['origin'][2] for s in samples), max(s['server']['origin'][2] for s in samples)], flush=True)

    try:
        for command in ('-forward', '-moveup', '-attack', '-reload', 'com_maxfps 76',
                        'cg_thirdPerson 0', 'cmd setviewpos -150 1000 32 270'):
            rcon(command)
        phase('reload-prepare', ['+reload'], 3.5)
        rcon('-reload')
        phase('fire', ['+attack'], 1.0)
        rcon('-attack')
        phase('reload', ['+reload'], 3.5)
        rcon('-reload')
        phase('jump', ['+moveup'], 1.0)
        rcon('-moveup')
        phase('thirdperson-jump', ['cg_thirdPerson 1', '+moveup'], 1.0)
        rcon('-moveup')
        rcon('cg_thirdPerson 0')
        (OUT / (run['variant'] + '-engine-sequence.json')).write_text(json.dumps(report, indent=2) + '\n')
    finally:
        for command in ('-attack', '-reload', '-moveup', '-forward', 'cg_thirdPerson 0'):
            rcon(command)
        observer.close()


if __name__ == '__main__':
    main()
