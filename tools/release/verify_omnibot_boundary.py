"""Check compiled Omni-bot gameplay/editor boundaries without running a game."""
import argparse
import hashlib
import json
import re
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COMMON = ROOT / 'vendor/omni-bot/source/Omnibot/Common'
NATIVE_PUBLIC_COMMANDS = ('help', 'addbot', 'kickbot', 'kickall', 'dontshoot',
                          'reload_weapons', 'version', 'revision')
OPTIONAL_PUBLIC_COMMANDS = ('update_nav', 'update_all_nav')
OPTIONAL_AUTHORING_COMMANDS = ('stopprocess', 'showprocesses', 'navsystem')
REGISTRY_FILES = ('CommandReciever.cpp', 'IGame.cpp', 'IGameManager.cpp',
                  'ScriptManager.cpp', 'TriggerManager.cpp', 'GoalManager.cpp',
                  'PathPlannerBase.cpp', 'PathPlannerWaypointCmds.cpp')
ALTERNATIVE_REGISTRY_FILES = ('PathPlannerNavMeshCmds.cpp',
                              'PathPlannerFloodFillCmds.cpp',
                              'PathPlannerRecastCmds.cpp')


def native_registry():
    """Inventory every literal native command registration, not a sample list.

    Alternate planner command files require ENABLE_PATH_PLANNERS, which is off
    in these Game/Editor targets. The downloader is also an optional feature.
    They remain classified explicitly so additions cannot silently escape.
    """
    files = {}
    entries = []
    candidates = list(COMMON.glob('*.cpp')) + list((COMMON.parent / 'ET').glob('*.cpp'))
    for path in sorted(candidates):
        source = path.read_text()
        # Strip comments while preserving quoted strings and source order.
        source = re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/',
                        lambda m: m[0] if m[0].startswith('"') else ' ', source)
        found = re.findall(r'\bSet(?:Ex)?\s*\(\s*"([^"\n]+)"[\s\S]*?'
                           r'&\w+::(\w+)', source)
        if not found:
            continue
        if path.name not in REGISTRY_FILES + ALTERNATIVE_REGISTRY_FILES:
            raise ValueError('Unclassified native command registry: ' + str(path))
        files[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        for name, method in found:
            if path.name in ALTERNATIVE_REGISTRY_FILES:
                category = 'optional_alternative_planner'
            elif name in NATIVE_PUBLIC_COMMANDS:
                category = 'public'
            elif name in OPTIONAL_PUBLIC_COMMANDS:
                category = 'optional_public_downloader'
            elif name in OPTIONAL_AUTHORING_COMMANDS:
                category = 'optional_editor_planner'
            else:
                category = 'editor'
            entries.append({'name': name, 'method': method,
                            'source': path.name, 'category': category})
    return {'files': files, 'entries': entries}


REGISTRY = native_registry()
NATIVE_AUTHORING_COMMANDS = tuple(sorted({e['name'] for e in REGISTRY['entries']
                                        if e['category'] == 'editor'}))
AUTHORING_COMMANDS = NATIVE_AUTHORING_COMMANDS + ('ExportClientFunctionsFromDLL',)
AUTHORING_METHODS = tuple(sorted({e['method'] for e in REGISTRY['entries']
                                 if e['category'] == 'editor'}))
VISUAL_METHODS = ('gmfEnableDebugWindow', 'gmfDumpDebugConsoleToFile',
                  'gmfDrawDebugLine', 'gmfDrawDebugArrow', 'gmfDrawDebugAABB',
                  'gmfDrawDebugRadius', 'gmfDrawDebugText3d',
                  'gmfTransformAndDrawLineList', 'gmfDrawTrajectory',
                  'gmfDrawEntityAABB', 'gmfDrawEntityOBB', 'gmfShowPaths',
                  'gmfSetDebugFlag', 'gmfDumpBotTable')
RUNTIME_METHODS = ('cmdHelp', 'cmdAddbot', 'cmdKickbot', 'cmdKickAll',
                   'cmdBotDontShoot', 'cmdReloadWeaponDatabase', 'cmdVersion',
                   'PlanPathToGoal', 'LoadFromFile')
RUNTIME_BINDINGS = ('GetClosestWaypoint', 'SetWaypointFlag', 'SetAvailableMapGoals')


def method_matches(symbol, method):
    if symbol.startswith('?'):
        return symbol.startswith('?' + method + '@')
    if not symbol.startswith(('_Z', '__Z')):
        return False
    # Itanium names carry the exact identifier length. This rejects a longer
    # similarly named function, not just arbitrary strings containing a name.
    return bool(re.search(r'(?<!\d)' + str(len(method)) + re.escape(method) +
                          r'(?=[A-Z]|$)', symbol))


def nm_tools(data, explicit=None):
    paths = [explicit or os.environ.get('TCE_NM')]
    if data.startswith(b'\0asm') and os.environ.get('EMSDK'):
        paths += [str(Path(os.environ['EMSDK']) / 'upstream/bin/llvm-nm')]
    for key in ('ANDROID_NDK_HOME', 'ANDROID_NDK_ROOT'):
        if os.environ.get(key):
            paths += [str(p) for p in (Path(os.environ[key]) / 'toolchains/llvm/prebuilt').glob('*/bin/llvm-nm*')
                      if p.is_file() and p.suffix in ('', '.exe')]
    names = ['llvm-nm'] + ['llvm-nm-' + str(v) for v in range(24, 13, -1)]
    if data.startswith(b'\x7fELF'):
        endian = '<' if data[5] == 1 else '>'
        machine = struct.unpack_from(endian + 'H', data, 18)[0]
        names += {40: ['arm-linux-gnueabihf-nm'], 183: ['aarch64-linux-gnu-nm']}.get(machine, [])
    names += ['nm']
    paths += [shutil.which(name) for name in names]
    return list(dict.fromkeys(str(p) for p in paths if p))


def macho_architectures(data):
    if data[:4] not in (b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
                        b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'):
        return [None]
    endian = '>' if data[0] == 0xca else '<'
    count = struct.unpack_from(endian + 'I', data, 4)[0]
    stride = 32 if data[:4] in (b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca') else 20
    result = []
    for index in range(count):
        cpu = struct.unpack_from(endian + 'I', data, 8 + index * stride)[0]
        if cpu not in (0x01000007, 0x0100000c):
            raise ValueError('Unsupported Mach-O core slice: ' + hex(cpu))
        result.append({0x01000007: 'x86_64', 0x0100000c: 'arm64'}[cpu])
    return result


def defined_symbols(core, data, symbol_map=None, nm=None):
    if symbol_map is not None and data.startswith(b'MZ'):
        output = Path(symbol_map).read_bytes()
        symbols = [{'symbol': m[0], 'address': m[1], 'type': 'T'} for m in re.findall(
            r'^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]+)\s+f\b',
            output.decode(errors='replace'), re.M)]
        if not symbols:
            raise ValueError('No defined functions in MSVC link map')
        return {'default': {'kind': 'msvc-link-map', 'tool': 'MSVC linker /MAP',
                            'sha256': hashlib.sha256(output).hexdigest(), 'symbols': symbols}}
    if data.startswith(b'\0asm'):
        if symbol_map is None:
            raise ValueError('Final WASM requires its linker map: exports omit internal visual callbacks')
        output = Path(symbol_map).read_bytes()
        symbols = []
        in_code = False
        for line in output.decode(errors='replace').splitlines():
            fields = line.split(maxsplit=3)
            if len(fields) == 4 and fields[3] in ('TYPE', 'IMPORT', 'FUNCTION', 'GLOBAL',
                                                 'EXPORT', 'START', 'ELEM', 'CODE', 'DATA'):
                in_code = fields[3] == 'CODE'
            if in_code:
                match = re.match(r'^\s*-\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+.+:\(([^()\s]+)\)\s*$', line)
                if match:
                    symbols.append({'symbol': match[2], 'address': match[1], 'type': 'T'})
        if not symbols:
            raise ValueError('No defined CODE functions in WASM linker map')
        return {'default': {'kind': 'wasm-lld-link-map', 'tool': 'wasm-ld -Map',
                            'sha256': hashlib.sha256(output).hexdigest(), 'symbols': symbols}}
    errors = []
    for tool in nm_tools(data, nm):
        results = {}
        for arch in macho_architectures(data):
            arguments = [tool, '-P']
            if arch:
                arguments += (['-arch', arch] if sys.platform == 'darwin' and Path(tool).name == 'nm'
                              else ['--arch=' + arch])
            arguments.append(str(core))
            try:
                process = subprocess.run(arguments, capture_output=True, text=True, timeout=60)
            except (OSError, subprocess.TimeoutExpired) as error:
                errors.append(type(error).__name__ + ': ' + str(tool)); break
            # A value is mandatory: undefined/weak imports are not definitions.
            symbols = [{'symbol': m[0], 'type': m[1], 'address': m[2]} for m in re.findall(
                r'^(\S+)\s+([TtWw])\s+([0-9a-fA-F]+)(?:\s+[0-9a-fA-F]+)?\s*$',
                process.stdout, re.M)]
            if process.returncode or not symbols:
                errors.append(str(tool) + ': ' + process.stderr[-300:]); break
            results[arch or 'default'] = {'kind': 'native-nm', 'tool': str(tool),
                'command': arguments, 'sha256': hashlib.sha256(process.stdout.encode()).hexdigest(),
                'symbols': symbols}
        else:
            return results
    raise ValueError('Defined function evidence requires a matching nm/llvm-nm or MSVC map: ' +
                     '; '.join(errors))


def verify_core(core, variant, symbol_map=None, nm=None):
    core = Path(core)
    data = core.read_bytes()
    editor = variant == 'navstudio'
    result = {'library': core.name, 'variant': variant,
              'sha256': hashlib.sha256(data).hexdigest(),
              'authoring_command_literals': {
                  s: s.encode() + (b'' if editor else b'\0') in data
                  for s in AUTHORING_COMMANDS},
              'public_command_literals': {s: s.encode() in data for s in NATIVE_PUBLIC_COMMANDS},
              'native_registry': REGISTRY,
              'runtime_bindings': {s: s.encode() + b'\0' in data for s in RUNTIME_BINDINGS}}
    diagnostic_formats = set()
    for name in ('BotBaseStates.cpp', 'PathPlannerWaypoint.cpp', 'Client.cpp'):
        diagnostic_formats.update(re.findall(r'"(TCE2[^"\n]+)"', (COMMON / name).read_text()))
    result['navigation_diagnostic_formats'] = {
        s: s.encode() + b'\0' in data for s in sorted(diagnostic_formats)}
    if not data.startswith((b'MZ', b'\x7fELF', b'\0asm',
                            b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe',
                            b'\xfe\xed\xfa\xcf', b'\xcf\xfa\xed\xfe',
                            b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
                            b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca')):
        raise ValueError('Bot runtime is not a native/wasm module: ' + str(core))
    if b'ExportBotFunctionsFromDLL' not in data:
        raise ValueError('Missing Omni-bot ABI entry: ' + str(core))
    if not editor and any(result['authoring_command_literals'].values()):
        raise ValueError('Editor command literals in public core: ' + json.dumps(result))
    if not editor and any(result['navigation_diagnostic_formats'].values()):
        raise ValueError('Navigation authoring diagnostics in public core: ' + json.dumps(result))
    if not all(result['runtime_bindings'].values()):
        raise ValueError('Missing gameplay navigation/goal bindings: ' + json.dumps(result))
    inventory = defined_symbols(core, data, symbol_map, nm)
    result['symbol_evidence'] = {'core_sha256': result['sha256'], 'architectures': {}}
    for arch, evidence in inventory.items():
        symbols = evidence.pop('symbols')
        methods = {m: [s for s in symbols if method_matches(s['symbol'], m)]
                   for m in set(AUTHORING_METHODS + RUNTIME_METHODS + VISUAL_METHODS)}
        evidence['defined_function_count'] = len(symbols)
        evidence['matched_methods'] = methods
        result['symbol_evidence']['architectures'][arch] = evidence
        for key, names, expected in (('authoring_methods', AUTHORING_METHODS, editor),
                                     ('runtime_methods', RUNTIME_METHODS, True),
                                     ('visual_methods', VISUAL_METHODS, editor)):
            actual = {name: bool(methods[name]) for name in names}
            if any(found != expected for found in actual.values()):
                raise ValueError('Wrong compiled ' + key + ' for ' + arch + ': ' + json.dumps(actual))
            result[key] = actual
    registered = {e['name']: e['method'] for e in REGISTRY['entries']
                  if e['category'] in ('public', 'editor')}
    result['public_commands'] = {name: result['runtime_methods'][registered[name]]
                                 for name in NATIVE_PUBLIC_COMMANDS}
    result['authoring_commands'] = {name: result['authoring_methods'][registered[name]]
                                    for name in NATIVE_AUTHORING_COMMANDS}
    result['authoring_commands']['ExportClientFunctionsFromDLL'] = (
        b'ExportClientFunctionsFromDLL\0' in data)
    if result['authoring_commands']['ExportClientFunctionsFromDLL'] != editor:
        raise ValueError('Wrong client drawing export lookup in bot core')
    if symbol_map is not None:
        result['link_map_sha256'] = hashlib.sha256(Path(symbol_map).read_bytes()).hexdigest()
    validate_symbol_evidence(result, data)
    return result


def validate_symbol_evidence(report, data):
    """Revalidate archived symbol records against this core and source policy.

    Do not accept a collection of previously asserted booleans. Each architecture
    needs the complete, exact method inventory and matching defined-function
    records, bound to the inspected module's bytes and the registry source.
    """
    digest = hashlib.sha256(data).hexdigest()
    evidence = report.get('symbol_evidence', {})
    if report.get('sha256') != digest or evidence.get('core_sha256') != digest:
        raise ValueError('Symbol evidence is bound to different core bytes')
    if report.get('native_registry') != REGISTRY:
        raise ValueError('Symbol evidence uses a different native command registry')
    if report.get('variant') not in ('game', 'navstudio'):
        raise ValueError('Unknown symbol evidence variant')
    editor = report['variant'] == 'navstudio'
    architectures = evidence.get('architectures', {})
    expected_architectures = {arch or 'default' for arch in macho_architectures(data)}
    if set(architectures) != expected_architectures:
        raise ValueError('Incomplete per-architecture symbol evidence')
    required = set(AUTHORING_METHODS + RUNTIME_METHODS + VISUAL_METHODS)
    required_kind = ('msvc-link-map' if data.startswith(b'MZ') else
                     'wasm-lld-link-map' if data.startswith(b'\0asm') else 'native-nm')
    for arch, entry in architectures.items():
        if entry.get('kind') != required_kind:
            raise ValueError('Unrecognized defined-function evidence source')
        if not re.fullmatch(r'[0-9a-f]{64}', entry.get('sha256', '')):
            raise ValueError('Missing symbol inventory digest')
        if not isinstance(entry.get('defined_function_count'), int) or entry['defined_function_count'] < len(RUNTIME_METHODS):
            raise ValueError('Incomplete defined-function inventory')
        methods = entry.get('matched_methods', {})
        if set(methods) != required:
            raise ValueError('Incomplete method evidence for ' + arch)
        for method, records in methods.items():
            if not isinstance(records, list):
                raise ValueError('Method evidence must contain actual symbol records')
            for record in records:
                if (record.get('type') not in ('T', 't', 'W', 'w') or
                        not re.fullmatch(r'[0-9a-fA-F]+', record.get('address', '')) or
                        not method_matches(record.get('symbol', ''), method)):
                    raise ValueError('Invalid defined-function record for ' + method)
                if entry['kind'] == 'native-nm' and record['symbol'].encode() + b'\0' not in data:
                    raise ValueError('Defined symbol is absent from the core symbol table: ' + method)
        for key, names, expected in (('authoring_methods', AUTHORING_METHODS, editor),
                                     ('runtime_methods', RUNTIME_METHODS, True),
                                     ('visual_methods', VISUAL_METHODS, editor)):
            actual = {name: bool(methods[name]) for name in names}
            if report.get(key) != actual or any(value != expected for value in actual.values()):
                raise ValueError('Wrong compiled method inventory: ' + key + '/' + arch)
    registered = {entry['name']: entry['method'] for entry in REGISTRY['entries']
                  if entry['category'] in ('public', 'editor')}
    public = {name: report['runtime_methods'][registered[name]] for name in NATIVE_PUBLIC_COMMANDS}
    authoring = {name: report['authoring_methods'][registered[name]] for name in NATIVE_AUTHORING_COMMANDS}
    authoring['ExportClientFunctionsFromDLL'] = b'ExportClientFunctionsFromDLL\0' in data
    if report.get('public_commands') != public or report.get('authoring_commands') != authoring:
        raise ValueError('Command registry disagrees with compiled handler evidence')
    if authoring['ExportClientFunctionsFromDLL'] != editor:
        raise ValueError('Wrong compiled client drawing export lookup')
    if not editor and any(name.encode() + b'\0' in data for name in AUTHORING_COMMANDS):
        raise ValueError('Editor command literal in public core')
    formats = {text for name in ('BotBaseStates.cpp', 'PathPlannerWaypoint.cpp', 'Client.cpp')
               for text in re.findall(r'"(TCE2[^"\n]+)"', (COMMON / name).read_text())}
    actual_formats = {text: text.encode() + b'\0' in data for text in formats}
    if report.get('navigation_diagnostic_formats') != actual_formats or (not editor and any(actual_formats.values())):
        raise ValueError('Incomplete or incorrect navigation diagnostic evidence')
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--variant', choices=('game', 'navstudio'), required=True)
    parser.add_argument('--map', type=Path)
    parser.add_argument('--nm', type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    result = verify_core(args.core, args.variant, args.map, args.nm)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
