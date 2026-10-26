"""Visible original/port A/B in the single existing ET installation.

The observer only reads process memory. Actions use engine console commands;
no input injection, live code patching, new engine installations or servers.
Original references are never changed. SHA identities and observed state are
recorded independently of any conclusion about equivalence.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import math
import re
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import time
import zipfile

ROOT = Path(__file__).resolve().parents[2]
INSTANCE = ROOT / 'build/navgen-overkill'
MOD = INSTANCE / 'tce2'
OUT = ROOT / 'build/runtime-parity-live'
MODULES = ('cgame_mp_x86.dll', 'qagame_mp_x86.dll', 'ui_mp_x86.dll')
K = C.WinDLL('kernel32', use_last_error=True)
P = C.WinDLL('psapi', use_last_error=True)
K.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
K.OpenProcess.restype = W.HANDLE
K.CloseHandle.argtypes = [W.HANDLE]
K.WaitForSingleObject.argtypes = [W.HANDLE, W.DWORD]
K.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
P.EnumProcessModulesEx.argtypes = [W.HANDLE, C.POINTER(C.c_void_p), W.DWORD, C.POINTER(W.DWORD), W.DWORD]
P.GetModuleFileNameExW.argtypes = [W.HANDLE, C.c_void_p, W.LPWSTR, W.DWORD]
LAST_RCON = 0.0


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rcon(command, timeout=.25):
    global LAST_RCON
    time.sleep(max(0, .65 - (time.monotonic() - LAST_RCON)))
    LAST_RCON = time.monotonic()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.sendto(b'\xff' * 4 + ('rcon navgen_local ' + command + '\n').encode('ascii'), ('127.0.0.1', 27986))
        chunks = []
        while True:
            try:
                chunks.append(sock.recv(65535).replace(b'\xff' * 4 + b'print\n', b'').decode('latin1'))
            except (TimeoutError, ConnectionResetError):
                return ''.join(chunks)


def alive(pid):
    handle = K.OpenProcess(0x100000, False, pid)
    if not handle:
        return False
    try:
        return K.WaitForSingleObject(handle, 0) == 258
    finally:
        K.CloseHandle(handle)


def stop():
    pid_file = INSTANCE / 'client.pid'
    if not pid_file.is_file():
        return
    pid = int(pid_file.read_text().strip())
    if not alive(pid):
        return
    rcon('-forward'); rcon('-moveright'); rcon('-speed'); rcon('-sprint'); rcon('-attack')
    rcon('quit')
    deadline = time.monotonic() + 20
    while alive(pid):
        if time.monotonic() > deadline:
            raise RuntimeError('Existing test game did not exit; refusing another start')
        time.sleep(.2)


class Observer:
    def __init__(self, variant, expected):
        self.pid = int((INSTANCE / 'client.pid').read_text().strip())
        self.handle = K.OpenProcess(0x410, False, self.pid)
        if not self.handle:
            raise C.WinError(C.get_last_error())
        entries = (C.c_void_p * 2048)(); needed = W.DWORD()
        if not P.EnumProcessModulesEx(self.handle, entries, C.sizeof(entries), C.byref(needed), 3):
            raise C.WinError(C.get_last_error())
        self.modules = {}
        for entry in entries[:needed.value // C.sizeof(C.c_void_p)]:
            buffer = C.create_unicode_buffer(32768)
            P.GetModuleFileNameExW(self.handle, entry, buffer, len(buffer))
            path = Path(buffer.value)
            if path.name.lower() in MODULES:
                name = path.name.lower(); actual = digest(path)
                if actual != expected[name]:
                    raise RuntimeError('Loaded DLL identity differs from selected reference: ' + str(path))
                self.modules[name] = {'path': str(path), 'sha256': actual, 'base': entry}
        if set(self.modules) != set(MODULES):
            raise RuntimeError('Not all three selected DLLs are loaded yet')
        self.variant = variant
        self.base = self.modules['cgame_mp_x86.dll']['base']
        self.layout = json.loads((ROOT / 'build/parity-live-observer/offsets.json').read_text())
        if variant == 'original':
            self.cg = self.base + 0x40352a0
            self.cg_fields = dict(self.layout['cg'], snap=36, time=0x407dea4 - 0x40352a0,
                                  frametime=0x407dea0 - 0x40352a0,
                                  predictedPlayerState=0x407dec0 - 0x40352a0,
                                  predictedPlayerEntity=0x407e46c - 0x40352a0,
                                  refdef_current=0x40863ac - 0x40352a0)
        else:
            text = (ROOT / 'build/sdk-win32/cgame-symbols-Release.map').read_text()
            preferred = int(re.search(r'Preferred load address is\s+([0-9a-fA-F]+)', text).group(1), 16)
            symbol = re.search(r'^\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+_cg\s+([0-9a-fA-F]+)', text, re.M)
            if not symbol:
                raise RuntimeError('Current cgame symbol map has no cg global')
            if digest(ROOT / 'build/sdk-win32/bin/Release/cgame_mp_x86.dll') != expected['cgame_mp_x86.dll']:
                raise RuntimeError('Port module and symbol-map build are not the same artifact')
            self.cg = self.base + int(symbol.group(1), 16) - preferred
            self.cg_fields = self.layout['cg']

    def read(self, address, fmt='i'):
        size = struct.calcsize('<' + fmt); data = C.create_string_buffer(size); got = C.c_size_t()
        if not K.ReadProcessMemory(self.handle, address, data, size, C.byref(got)) or got.value != size:
            raise C.WinError(C.get_last_error())
        value = struct.unpack('<' + fmt, data.raw)
        return value[0] if len(value) == 1 else list(value)

    def capture(self):
        fields = self.cg_fields
        snap = self.read(self.cg + fields['snap'], 'I')
        if not snap:
            raise RuntimeError('No client snapshot yet')
        result = {'monotonic': time.monotonic(), 'cg_time': self.read(self.cg + fields['time']),
                  'frame_ms': self.read(self.cg + fields['frametime']),
                  'frame_count': self.read(self.cg), 'server_time': self.read(snap + 8)}
        for label, address in (('server', snap + 44), ('predicted', self.cg + fields['predictedPlayerState'])):
            state = {}
            for name, offset in self.layout['ps'].items():
                if name == 'size':
                    continue
                fmt = '3f' if name in ('origin', 'velocity', 'viewangles') else 'f' if name.endswith('SpeedScale') else '16i' if name in ('stats', 'persistant') else 'i'
                state[name] = self.read(address + offset, fmt)
            result[label] = state
        refdef = self.read(self.cg + fields['refdef_current'], 'I')
        if refdef:
            result['camera'] = self.read(refdef + 24, '3f')
        entity = self.cg + fields['predictedPlayerEntity']
        offsets = self.layout.get('centity', {})
        if self.variant == 'original':
            offsets = dict(offsets, **{'pe.legs': 0x268, 'pe.torso': 0x2bc, 'pe.weap': 0x364})
        for part in ('pe.legs', 'pe.torso', 'pe.weap'):
            if part not in offsets:
                continue
            result[part] = {name: self.read(entity + offsets[part] + offset, 'f' if name == 'backlerp' else 'i')
                            for name, offset in self.layout.get('lerpFrame', {}).items() if name != 'size'}
        return result

    def close(self):
        if self.handle:
            K.CloseHandle(self.handle); self.handle = None


def launch(variant, mapname, gametype=7):
    stop()
    OUT.mkdir(parents=True, exist_ok=True)
    backup = OUT / 'installed-before'
    backup.mkdir(exist_ok=True)
    for name in (*MODULES, 'tce2-1.0.1.pk3', 'zz_tce2_navstudio_editor.pk3'):
        path = MOD / name
        if path.is_file():
            if not (backup / name).exists():
                shutil.copy2(path, backup / name)
            # Single named file, never a recursive move/deletion.
            if name == 'zz_tce2_navstudio_editor.pk3':
                retired = OUT / ('disabled-editor-' + str(time.time_ns()) + '.pk3')
                path.rename(retired)
    source = ROOT / ('tcetest' if variant == 'original' else 'dist/TCE2-Runtime-Parity-Windows-x86/Game/tce2')
    expected = {name: digest(source / name) for name in MODULES}
    for name in MODULES:
        shutil.copy2(source / name, MOD / name)
    if variant == 'port':
        shutil.copy2(source / 'tce2-1.0.1.pk3', MOD / 'tce2-1.0.1.pk3')
    else:
        with zipfile.ZipFile(MOD / 'tce2-1.0.1.pk3', 'w', zipfile.ZIP_DEFLATED) as archive:
            for name in MODULES:
                archive.write(source / name, name)
    cfg = '\n'.join(['set sv_pure 0', 'set sv_floodProtect 0', 'set rconPassword navgen_local',
         'set sv_master1 ""', 'set sv_master2 ""', 'set omnibot_enable 0', 'set bot_enable 0',
         'set lua_modules ""', 'set navstudio_waypoint_view 0', 'set navstudio_editor 0',
         'set g_mapScriptDirectory ""', 'set g_doWarmup 0', 'set match_minplayers 0',
         'set match_readypercent 0', 'set timelimit 999', 'set g_timelimit 999',
         'set g_redlimbotime 1000', 'set g_bluelimbotime 1000', 'set logfile 2',
         'set g_log runtime-parity.log', 'set name RuntimeParityObserver', 'set cg_drawFPS 1',
         'set r_swapInterval 0', 'set r_primitives 2', 'set pmove_fixed 0', 'set pmove_msec 8',
         'bind n noclip', 'bind k kill', 'bind ESCAPE togglemenu', ''])
    (MOD / 'runtime-parity.cfg').write_text(cfg, encoding='ascii')
    args = [str(INSTANCE / 'ET.exe'), '+set', 'fs_basepath', str(INSTANCE), '+set', 'fs_homepath', str(INSTANCE),
         '+set', 'fs_game', 'tce2', '+set', 'com_hunkMegs', '512', '+set', 'com_zoneMegs', '64',
         '+set', 'com_soundMegs', '64', '+set', 'com_introPlayed', '1', '+set', 'com_recommendedSet', '1',
         '+set', 'net_ip', '127.0.0.1', '+set', 'net_port', '27986', '+set', 'dedicated', '0',
         '+set', 'r_fullscreen', '0', '+set', 'r_mode', '-1', '+set', 'r_customwidth', '1280',
         '+set', 'r_customheight', '720', '+set', 'com_maxfps', '76', '+set', 'sv_official', '0',
         '+set', 'g_gametype', str(gametype), '+exec', 'runtime-parity.cfg', '+devmap', mapname]
    process = subprocess.Popen(args, cwd=INSTANCE)
    (INSTANCE / 'client.pid').write_text(str(process.pid))
    run = {'variant': variant, 'map': mapname, 'gametype': gametype, 'pid': process.pid, 'expected': expected,
           'args': args, 'started': time.time()}
    (OUT / 'current.json').write_text(json.dumps(run, indent=2))
    return run


def attach(run, timeout=45):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not alive(run['pid']):
            raise RuntimeError('The selected game exited; inspect its console log')
        observer = None
        try:
            observer = Observer(run['variant'], run['expected'])
            state = observer.capture()
            if state['server_time'] > 0 and state['cg_time'] > 0:
                return observer
        except (OSError, RuntimeError):
            if observer:
                observer.close()
        time.sleep(.2)
    raise TimeoutError('Client snapshot/loaded DLL identity did not become ready')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=['original', 'port'])
    parser.add_argument('--action', choices=['launch', 'state', 'commands', 'fps', 'stop'], default='state')
    parser.add_argument('--map', default='obj_overkill')
    parser.add_argument('--gametype', type=int, choices=[2, 5, 7], default=7)
    parser.add_argument('--command', action='append', default=[])
    parser.add_argument('--fps', default='20,30,60,76,125,250')
    parser.add_argument('--seconds', type=float, default=.8)
    parser.add_argument('--label', default='probe')
    args = parser.parse_args()
    if args.action == 'stop':
        stop(); return
    if args.action == 'launch':
        if not args.variant or not re.fullmatch(r'[a-zA-Z0-9_]+', args.map):
            parser.error('A variant and a plain map name are required')
        run = launch(args.variant, args.map, args.gametype)
    else:
        run = json.loads((OUT / 'current.json').read_text())
    observer = attach(run)
    report = {'run': run, 'loaded_modules': observer.modules, 'observations': []}
    try:
        for command in args.command:
            report.setdefault('commands', []).append({'command': command, 'response': rcon(command)})
        if args.action == 'fps':
            for fps in [int(value) for value in args.fps.split(',')]:
                rcon('com_maxfps ' + str(fps)); rcon('-forward'); rcon('-sprint'); rcon('-speed')
                rcon('cmd setviewpos -150 1000 32 270')
                time.sleep(.5)
                samples = [observer.capture()]
                rcon('+forward', timeout=.02)
                deadline = time.monotonic() + args.seconds
                try:
                    while time.monotonic() < deadline:
                        samples.append(observer.capture()); time.sleep(.02)
                finally:
                    rcon('-forward', timeout=.02)
                start, end = samples[0], samples[-1]
                dt = (end['server_time'] - start['server_time']) / 1000
                distance = math.dist(start['server']['origin'][:2], end['server']['origin'][:2])
                wall_seconds = end['monotonic'] - start['monotonic']
                velocities = [math.hypot(*s['server']['velocity'][:2]) for s in samples]
                record = {'fps_limit': fps, 'snapshot_seconds': dt, 'distance': distance,
                          'observed_fps': (end['frame_count'] - start['frame_count']) / wall_seconds if wall_seconds else None,
                          'maximum_horizontal_velocity': max(velocities),
                          'units_per_second': distance / dt if dt else None, 'samples': samples}
                report['observations'].append(record)
                print(json.dumps({key: value for key, value in record.items() if key != 'samples'}), flush=True)
        else:
            report['observations'].append(observer.capture())
            print(json.dumps(report['observations'][-1]), flush=True)
        for name in ('g_realism', 'g_speed', 'pmove_fixed', 'pmove_msec', 'g_gametype', 'com_maxfps', 'gamestate', 'r_primitives', 'r_swapInterval'):
            report.setdefault('cvars', {})[name] = rcon(name)
        path = OUT / (run['variant'] + '-' + args.label + '.json')
        path.write_text(json.dumps(report, indent=2) + '\n')
    finally:
        observer.close()


if __name__ == '__main__':
    main()
