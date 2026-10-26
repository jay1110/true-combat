"""Read-only/mod-local smoke capture of an already running test installation.

Does not start another engine, change maps, generate navigation, or install
packages. The named local observer may be moved by its existing Lua command.
"""

import argparse
import hashlib
import json
from pathlib import Path
import socket
import time
import re


def query(command, port, password):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(1.1)
        sock.sendto(b'\xff' * 4 + f'rcon {password} {command}\n'.encode('ascii'), ('127.0.0.1', port))
        chunks = []
        while True:
            try:
                chunks.append(sock.recv(65535).replace(b'\xff' * 4 + b'print\n', b'').decode('latin1'))
            except (TimeoutError, ConnectionResetError):
                return ''.join(chunks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--instance', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--port', type=int, default=27986)
    parser.add_argument('--password', required=True)
    parser.add_argument('--samples', type=int, default=3)
    parser.add_argument('--interval', type=int, default=15)
    parser.add_argument('--screenshot', action='store_true')
    parser.add_argument('--screenshot-name', default='modernization-smoke')
    parser.add_argument('--ready-timeout', type=int, default=30)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]+', args.screenshot_name):
        parser.error('Screenshot name must contain only letters, digits, underscores or hyphens')
    report = {'samples': [], 'binaries': {}, 'limits': 'Startup/combat smoke only; not a complete gameplay/parity test.'}
    ready_deadline = time.monotonic() + args.ready_timeout
    while 'map:' not in query('status', args.port, args.password):
        if time.monotonic() >= ready_deadline:
            parser.error('The existing local game did not become ready; no second instance was started')
        time.sleep(0.5)
    for path in [*(args.instance / 'tce2').glob('*_mp_x86.dll'), args.instance / 'omni-bot/omnibot_et.dll']:
        if path.is_file():
            report['binaries'][str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    for i in range(args.samples):
        if i:
            time.sleep(args.interval)
        sample = {'time': time.time()}
        for command in ('status', 'g_gametype', 'navprobe'):
            sample[command] = query(command, args.port, args.password)
        report['samples'].append(sample)
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({'sample': i + 1, 'responding': 'map:' in sample['status']}), flush=True)
    if args.screenshot:
        query('navcamera -503.5 413 190.375 10 270', args.port, args.password)
        query('screenshotJPEG ' + args.screenshot_name, args.port, args.password)
        report['screenshot'] = 'tce2/screenshots/' + args.screenshot_name + '.jpg'
        args.out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    raise SystemExit(not all('map:' in sample['status'] for sample in report['samples']))


if __name__ == '__main__':
    main()
