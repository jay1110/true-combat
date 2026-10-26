"""Check compiled Omni-bot gameplay/editor boundaries without running a game."""
import argparse
import hashlib
import json
import re
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


def verify_core(core, variant, symbol_map=None):
    core = Path(core)
    data = core.read_bytes()
    editor = variant == 'navstudio'
    result = {'library': core.name, 'variant': variant,
              'sha256': hashlib.sha256(data).hexdigest(),
              # MSVC x64 can emit a short std::string as a mov-immediate and
              # store its terminator separately (e.g. revision/debugbot).
              # The complete method-map and source registry checks below are
              # essential; a literal-only absence check cannot prove a gate.
              'authoring_commands': {
                  s: s.encode() + (b'' if editor else b'\0') in data
                  for s in AUTHORING_COMMANDS},
              'public_commands': {s: s.encode() in data for s in NATIVE_PUBLIC_COMMANDS},
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
    if any(found != editor for found in result['authoring_commands'].values()):
        raise ValueError('Wrong compiled authoring command set: ' + json.dumps(result))
    if not all(result['public_commands'].values()):
        raise ValueError('Missing public bot administration commands: ' + json.dumps(result))
    if not editor and any(result['navigation_diagnostic_formats'].values()):
        raise ValueError('Navigation authoring diagnostics in public core: ' + json.dumps(result))
    if not all(result['runtime_bindings'].values()):
        raise ValueError('Missing gameplay navigation/goal bindings: ' + json.dumps(result))
    if symbol_map is not None:
        symbols = Path(symbol_map).read_text(errors='replace')
        result['authoring_methods'] = {s: s in symbols for s in AUTHORING_METHODS}
        result['runtime_methods'] = {s: s in symbols for s in RUNTIME_METHODS}
        result['visual_methods'] = {s: s in symbols for s in VISUAL_METHODS}
        if any(found != editor for found in result['authoring_methods'].values()):
            raise ValueError('Wrong compiled authoring methods in link map: ' + json.dumps(result))
        if not all(result['runtime_methods'].values()):
            raise ValueError('Missing gameplay methods in link map: ' + json.dumps(result))
        if any(found != editor for found in result['visual_methods'].values()):
            raise ValueError('Wrong compiled visualization methods in link map: ' + json.dumps(result))
        result['link_map_sha256'] = hashlib.sha256(Path(symbol_map).read_bytes()).hexdigest()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--variant', choices=('game', 'navstudio'), required=True)
    parser.add_argument('--map', type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    result = verify_core(args.core, args.variant, args.map)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
