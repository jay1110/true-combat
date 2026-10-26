"""Alternate classic engine primitive paths in one stationary live view.

This records measured frames, not FPS cap labels or a whole-mod benchmark.
The requested r_primitives=2 is always restored, including on failure.
"""
import json
import statistics
import time
from live_compare import OUT, attach, rcon


def main():
    run = json.loads((OUT / 'current.json').read_text())
    observer = attach(run)
    report = {'run': run, 'loaded_modules': observer.modules, 'trials': []}
    try:
        for command in ('-forward', '-attack', '-moveup', 'cg_thirdPerson 0',
                        'com_maxfps 333', 'cmd setviewpos -150 1000 32 270'):
            rcon(command)
        for primitive in (0, 2, 0, 2):
            rcon('r_primitives ' + str(primitive))
            time.sleep(.25)
            start = observer.capture()
            samples = []
            until = time.monotonic() + 3
            while time.monotonic() < until:
                samples.append(observer.capture())
                time.sleep(.02)
            end = samples[-1]
            elapsed = end['monotonic'] - start['monotonic']
            trial = {'r_primitives': primitive, 'seconds': elapsed,
                     'frames': end['frame_count'] - start['frame_count'],
                     'fps': (end['frame_count'] - start['frame_count']) / elapsed,
                     'origin_before': start['server']['origin'], 'origin_after': end['server']['origin'],
                     'median_frame_ms': statistics.median(x['frame_ms'] for x in samples)}
            report['trials'].append(trial)
            print(json.dumps(trial), flush=True)
        (OUT / (run['variant'] + '-primitive-fps.json')).write_text(json.dumps(report, indent=2) + '\n')
    finally:
        rcon('r_primitives 2')
        rcon('com_maxfps 76')
        observer.close()


if __name__ == '__main__':
    main()
