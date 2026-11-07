"""Validate final canonical Editor menu bytes and stock-metadata closure.

No native-catalog import, DMS merge, namespace rewrite or resource generation
is performed here. Edit navstudio/editor-pak/ directly.
"""
from dataclasses import dataclass
import json
from pathlib import Path
import re

REGISTRIES = ('menus.txt', 'menus_gt2.txt', 'menus_gt5.txt', 'menus_gt7.txt')
VOICE_FILES = ('wm_quickmessage.menu', 'wm_quickmessageAlt.menu',
               'wm_ftquickmessage.menu', 'wm_ftquickmessageAlt.menu')
NATIVE_ROOT = 'tce_omnibot_dms'
# Unchanged imports inherited from the original registries. The first two are
# supplied by ET pak0; the last two are unused historical loader entries (no
# open action targets them). Never exempt native Editor files from closure.
LEGACY_EXTERNAL = {'ui/ingame_messagemode.menu', 'ui/ingame_tapout.menu',
                   'ui/options_sub.menu', 'ui/ingame_main_sub.menu'}


@dataclass
class Token:
    kind: str
    value: str
    start: int
    end: int


def tokens(data):
    text = data.decode('latin1'); result = []; at = 0
    while at < len(text):
        if text[at].isspace():
            at += 1; continue
        if text.startswith('//', at):
            end = text.find('\n', at); at = len(text) if end < 0 else end + 1; continue
        if text.startswith('/*', at):
            end = text.find('*/', at + 2)
            if end < 0: raise ValueError('Unterminated editor-menu comment')
            at = end + 2; continue
        start = at
        if text[at] == '"':
            at += 1
            while at < len(text) and text[at] != '"':
                at += 2 if text[at] == '\\' else 1
            if at >= len(text): raise ValueError('Unterminated editor-menu string')
            at += 1; result.append(Token('string', text[start+1:at-1], start, at)); continue
        match = re.match(r'[A-Za-z_][A-Za-z0-9_./-]*', text[at:])
        if match:
            at += len(match[0]); result.append(Token('word', match[0], start, at)); continue
        at += 1; result.append(Token('punct', text[start:at], start, at))
    return result


def close_brace(lexed, start):
    depth = 0
    for index in range(start, len(lexed)):
        if lexed[index].kind == 'punct' and lexed[index].value == '{': depth += 1
        elif lexed[index].kind == 'punct' and lexed[index].value == '}':
            depth -= 1
            if depth == 0: return index
    raise ValueError('Unbalanced editor-menu braces')


def loaders(data):
    lexed = tokens(data); result = []
    for index, token in enumerate(lexed[:-3]):
        if token.kind == 'word' and token.value == 'loadMenu':
            if lexed[index+1].value != '{' or lexed[index+2].kind != 'string' or lexed[index+3].value != '}':
                raise ValueError('Invalid editor loadMenu declaration')
            result.append(lexed[index+2].value)
    if len(result) != len({name.casefold() for name in result}):
        raise ValueError('Duplicate editor registry loader')
    return result


def definitions(data):
    """Literal menuDefs plus literal QM_MENU_START invocations, not macro templates."""
    lexed = tokens(data); result = []
    for index, token in enumerate(lexed):
        if token.kind != 'word': continue
        if token.value == 'menuDef' and index+3 < len(lexed) and lexed[index+1].value == '{':
            end = close_brace(lexed, index+1)
            if lexed[index+2].value == 'name' and lexed[index+3].kind in ('string', 'word'):
                name = lexed[index+3]
                if name.kind == 'string' or not name.value.isupper():
                    result.append((name.value, lexed[index+1].start, lexed[end].start, lexed[index+4:end]))
        elif (token.value == 'QM_MENU_START' and index+3 < len(lexed) and
              lexed[index+1].value == '(' and lexed[index+2].kind == 'string'):
            end = next((n for n in range(index+3, len(lexed)) if lexed[n].value == 'QM_MENU_END'), len(lexed))
            result.append((lexed[index+2].value, token.start, lexed[end-1].end, lexed[index+3:end]))
    return result


def opens(lexed):
    return {lexed[index+1].value for index, token in enumerate(lexed[:-1])
            if token.kind == 'word' and token.value == 'open' and
            lexed[index+1].kind in ('word', 'string') and lexed[index+1].value.startswith('tce_')}


def base_ui_reference(release_version=None, reference=None):
    """Use metadata only; installed TCE supplies the unchanged base menu bytes."""
    repo = Path(__file__).resolve().parents[2]
    if release_version is None:
        match = re.search(r'^#define TCE_VERSION "([^"]+)"', (repo/'src/game/tce_version.h').read_text(), re.M)
        release_version = match[1] if match else None
    if reference is None:
        path = repo/'tools/assets/base-ui-reference.json'
        if not path.is_file(): return {}
        reference = json.loads(path.read_bytes())
    if reference.get('schema') != 1 or reference.get('release_version') != release_version:
        raise ValueError('Wrong version-bound base UI reference')
    result = {}
    for name, record in reference.get('files', {}).items():
        if not re.fullmatch(r'[0-9a-f]{64}', record.get('sha256', '')) or not isinstance(record.get('bytes'), int):
            raise ValueError('Invalid base UI reference fingerprint: '+name)
        for menu in record.get('menus', []):
            value = menu.get('name', '')
            if value in ('tce_omnibot', NATIVE_ROOT) or value.startswith('tce_ob_'):
                raise ValueError('Native Editor cannot fall back to base UI metadata')
            if not isinstance(menu.get('opens'), list) or not all(isinstance(target, str) for target in menu['opens']):
                raise ValueError('Invalid base UI reference edges: '+name)
        if name.casefold() in result: raise ValueError('Case-colliding base UI metadata')
        result[name.casefold()] = record
    return result


def validate_editor_ui(files, release_version=None, reference=None):
    """Run against final installable bytes, including after manual pak edits."""
    required_goals = {'tce_ob_goalname', 'tce_ob_groupname', 'tce_ob_waypointname', 'tce_ob_value'}
    reports = {}; base = base_ui_reference(release_version, reference)
    for registry in REGISTRIES:
        key = 'ui/'+registry
        if key not in files: raise ValueError('Missing Editor registry: '+key)
        loaded = loaders(files[key]); menus = {}; edges = {}; external = []; referenced = []
        for path in loaded:
            if path not in files and path.casefold() in base:
                entries = [(menu['name'], 0, 0, None) for menu in base[path.casefold()].get('menus', [])]
                referenced.append(path)
            else:
                entries = None
            if path not in files and path in LEGACY_EXTERNAL:
                if entries is None: external.append(path); continue
            if path not in files and entries is None: raise ValueError('Missing registered Editor menu: '+path)
            for name, _start, _end, body in definitions(files[path]) if entries is None else entries:
                folded = name.casefold()
                if folded in menus: raise ValueError('Duplicate Editor menu name: '+name+' in '+path+' and '+menus[folded])
                menus[folded] = path
                edges[folded] = ({target.casefold() for target in opens(body)} if entries is None else
                                 {target.casefold() for menu in base[path.casefold()]['menus'] if menu['name'] == name for target in menu['opens']})
        needed = required_goals | {'tce_omnibot', NATIVE_ROOT}
        if not needed <= menus.keys(): raise ValueError('Missing Editor goal/root feature: '+','.join(sorted(needed-menus.keys())))
        missing = {target for targets in edges.values() for target in targets if target not in menus}
        if missing: raise ValueError('Editor action opens an unloaded menu: '+','.join(sorted(missing)))
        reached = set(); todo = ['tce_omnibot']
        while todo:
            name = todo.pop()
            if name in reached: continue
            reached.add(name); todo.extend(edges.get(name, ()))
        native = {name for name in menus if name.startswith('tce_ob_') or name == NATIVE_ROOT}
        if not native <= reached: raise ValueError('Unreachable Editor goal/catalog menus: '+','.join(sorted(native-reached)))
        for name in VOICE_FILES:
            path = 'ui/'+name
            if path not in loaded or not any(token.kind == 'string' and token.value == 'omnibot_menu' for token in tokens(files[path])):
                raise ValueError('Missing native Editor voice entry: '+path)
        reports[registry] = {'loaded_files':len(loaded)-len(external), 'menu_names':len(menus),
                             'reachable_catalog_menus':len(native), 'legacy_external_loaders':external,
                             'stock_metadata_loaders':referenced}
    return {'passed':True, 'registries':reports, 'goal_features':sorted(required_goals), 'native_root':NATIVE_ROOT}
