"""Conservative, byte-preserving semantic projection of ET UI menu sources.

This is a source comparison, not an engine preprocessor or runtime interpreter.
Unknown tokens stay significant. Original commands, list values and labels are exact.
Numeric paint operands, checked list-column geometry, one validated private widget
header and four exact Back-derived settings links are explicit layout exceptions.
"""
from dataclasses import dataclass
import ast
import hashlib
import json
import math
from pathlib import Path
import re

DIRECTORIES = ('ui', 'ui_obj', 'ui_robj', 'ui_tdm')
SEMANTIC_SUFFIXES = {'.menu', '.h', '.txt'}
PAINT = {
    'rect': 4, 'origin': 2, 'style': 1, 'border': 1, 'bordersize': 1,
    'backcolor': 4, 'forecolor': 4, 'bordercolor': 4, 'outlinecolor': 4,
    'focuscolor': 4, 'disablecolor': 4, 'shadowcolor': 4,
    'background': 1, 'textfont': 1, 'textscale': 1, 'textalign': 1,
    'textalignx': 1, 'textaligny': 1, 'textstyle': 1,
    'elementwidth': 1, 'elementheight': 1, 'gradientbar': 1, 'cursor': 1,
    'fadeclamp': 1, 'fadecycle': 1, 'fadeamount': 1,
}
CONTAINERS = {'menudef', 'itemdef', 'assetglobaldef'}
SCRIPTS = {'action', 'onopen', 'onesc', 'onclose', 'onenter', 'onexit',
           'onfocus', 'leavefocus', 'mouseenter', 'mouseexit', 'mouseentertext',
           'mouseexittext', 'doubleclick', 'ontimeout', 'execkey', 'execkeyint'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def decode(data):
    try:
        return data.decode('utf-8-sig'), 'utf-8'
    except UnicodeDecodeError:
        return data.decode('latin-1'), 'latin-1 byte preservation'


def strip_comments(text):
    result = []
    i = 0
    while i < len(text):
        if text[i] == '"':
            start = i; i += 1
            while i < len(text):
                if text[i] == '\\':
                    i += 2
                elif text[i] == '"':
                    i += 1; break
                else:
                    i += 1
            result.append(text[start:i])
        elif text.startswith(('//', '/*'), i):
            start = i
            if text.startswith('//', i):
                end = text.find('\n', i)
                i = len(text) if end < 0 else end
            else:
                end = text.find('*/', i + 2)
                if end < 0:
                    raise ValueError('Unterminated block comment')
                i = end + 2
            result.append(''.join('\n' if c == '\n' else ' ' for c in text[start:i]))
        else:
            result.append(text[i]); i += 1
    return ''.join(result)


def tokens(text):
    """Strip comments without corrupting quoted paths/actions or legacy bytes."""
    result = []
    i = 0
    while i < len(text):
        if text[i].isspace() or text[i] == '\\':
            i += 1
        elif text.startswith('//', i):
            end = text.find('\n', i)
            i = len(text) if end < 0 else end + 1
        elif text.startswith('/*', i):
            end = text.find('*/', i + 2)
            if end < 0:
                raise ValueError('Unterminated block comment')
            i = end + 2
        elif text[i] == '"':
            start = i
            i += 1
            while i < len(text):
                if text[i] == '\\':
                    i += 2
                elif text[i] == '"':
                    i += 1
                    break
                else:
                    i += 1
            else:
                raise ValueError('Unterminated quoted string')
            result.append(text[start:i])
        elif text[i] in '{}(),;':
            result.append(text[i]); i += 1
        else:
            start = i
            while (i < len(text) and not text[i].isspace() and text[i] not in '{}(),;"\\'
                   and not text.startswith(('//', '/*'), i)):
                i += 1
            result.append(text[start:i])
    return result


def group(ts, start, opening='{', closing='}'):
    if start >= len(ts) or ts[start] != opening:
        raise ValueError('Expected ' + opening)
    depth = 1
    end = start + 1
    while end < len(ts):
        depth += (ts[end] == opening) - (ts[end] == closing)
        if not depth:
            return end + 1
        end += 1
    raise ValueError('Unbalanced ' + opening + closing)


def atom(ts, start):
    if start >= len(ts) or ts[start] in '{};,':
        raise ValueError('Missing/unsafe paint value')
    if ts[start] == '(':
        return group(ts, start, '(', ')')
    if start + 1 < len(ts) and ts[start + 1] == '(':
        return group(ts, start + 1, '(', ')')
    return start + 1


def arguments(ts, start):
    end = group(ts, start, '(', ')')
    result, current, stack = [], [], []
    for t in ts[start + 1:end - 1]:
        if t in ('(', '{'):
            stack.append(t)
        elif t in (')', '}'):
            if not stack or (stack.pop(), t) not in (('(', ')'), ('{', '}')):
                raise ValueError('Mismatched macro argument delimiters')
        if t == ',' and not stack:
            result.append(current); current = []
        else:
            current.append(t)
    if stack:
        raise ValueError('Unbalanced macro arguments')
    result.append(current)
    return result, end


@dataclass
class Define:
    name: str
    params: tuple | None
    body: list


@dataclass
class Unit:
    path: str
    body: list
    defines: list
    raw: list


class MacroContracts(dict):
    paint_names = frozenset()


def paint_argument_extension(arg, contracts):
    """Legacy LABEL alignment args can append cvarTest/hideCvar data.

    Drop only known scalar/expression tokens. Any unknown field, string or
    structural token starts a significant extension, rather than vanishing.
    """
    names = getattr(contracts, 'paint_names', frozenset())
    for i, token in enumerate(arg):
        identifiers = re.findall(r'(?<![A-Za-z0-9_])[$A-Za-z_]\w*', token)
        if (token.startswith('"') or token in ('{', '}', ';', '#') or
                any(name not in names for name in identifiers)):
            return arg[i:]
    return []


def parse(path, data):
    # Engine menu text is byte-oriented. Transcoding a legacy label to UTF-8
    # must not masquerade as preserving the original rendered glyph sequence.
    text = (data[3:] if data.startswith(b'\xef\xbb\xbf') else data).decode('latin-1')
    text = strip_comments(text)
    text = re.sub(r'\\[ \t]*\r?\n', ' ', text)
    body, definitions = [], []
    # Directives retain their exact position relative to conditional branches.
    for line in text.splitlines(keepends=True):
        match = re.match(r'^\s*#\s*define\s+([A-Za-z_]\w*)(\([^\n]*?\))?(?:\s+|$)(.*)', line)
        if match:
            name, params, value = match.groups()
            params = tuple(p.strip() for p in params[1:-1].split(',')) if params else None
            definitions.append(Define(name, params, tokens(value)))
            body.append('@define:' + str(len(definitions) - 1))
        else:
            body.extend(tokens(line))
    return Unit(path, body, definitions, tokens(text))


def script_projection(ts):
    result = []
    i = 0
    while i < len(ts):
        t = ts[i]
        targets = {'setitemcolor': 1, 'setmenuitemcolor': 2}.get(t.lower())
        field = i + 1
        for _ in range(targets or 0):
            field += 1
            while field < len(ts) and ts[field].startswith('##'):
                field += 2 if ts[field] == '##' else 1
        prefix = field - i + 1
        if targets and i + prefix + 4 <= len(ts) and ts[field].lower() in (
                'forecolor', 'backcolor', 'bordercolor', 'outlinecolor'):
            values = ts[i + prefix:i + prefix + 4]
            numeric = all(re.fullmatch(r'[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?', v) for v in values)
            if numeric:
                if not all(math.isfinite(float(v)) for v in values):
                    raise ValueError('Nonfinite script paint color')
                result += ts[i:i + prefix] + ['@paint-rgba']
                i += prefix + 4
                continue
        if t.startswith('"') and any(command in t.lower() for command in ('setitemcolor', 'setmenuitemcolor')):
            # ConditionalScript stores a script in a quoted argument. Do not
            # relax arbitrary label/list strings, only this script context.
            inside = tokens(t[1:-1])
            normalized = script_projection(inside)
            result.append('@script-string:' + json.dumps(normalized, ensure_ascii=True))
        else:
            result.append(t)
        i += 1
    return result


def decorative(ts, known_names):
    if 'decoration' not in (t.lower() for t in ts):
        return False
    if any(t.lower()=='columns' for t in ts):return False
    rest = projection(ts, {})
    i = 0; name = None
    while i < len(rest):
        key = rest[i].lower()
        if key == 'decoration':
            i += 1
        elif key in ('name', 'group', 'visible') and i + 1 < len(rest):
            if key == 'name':
                name = rest[i + 1]
            if key == 'visible' and rest[i + 1] not in ('0', '1'):
                return False
            i += 2
        else:
            return False
    return name is not None and name not in known_names


def arithmetic_names(ts, allowed):
    """Only numeric column geometry; no strings, scripts or function calls."""
    expression=' '.join(ts).replace('$evalfloat','evalfloat').replace('$evalint','evalint')
    try:tree=ast.parse(expression,mode='eval')
    except SyntaxError as error:raise ValueError('Invalid column geometry expression') from error
    names=set();functions={id(node.func) for node in ast.walk(tree) if isinstance(node,ast.Call)}
    for node in ast.walk(tree):
        if isinstance(node,ast.Name):
            if node.id in ('evalfloat','evalint'):
                if id(node) not in functions:raise ValueError('Uncalled column geometry function')
            else:
                if node.id not in allowed:raise ValueError('Unknown column geometry constant: '+node.id)
                names.add(node.id)
        elif isinstance(node,ast.Constant):
            if type(node.value) not in (int,float) or not math.isfinite(node.value):
                raise ValueError('Nonnumeric column geometry')
        elif isinstance(node,ast.Call):
            if (not isinstance(node.func,ast.Name) or node.func.id not in ('evalfloat','evalint') or
                    len(node.args)!=1 or node.keywords):raise ValueError('Unsafe column geometry call')
        elif not isinstance(node,(ast.Expression,ast.Load,ast.BinOp,ast.UnaryOp,ast.Add,ast.Sub,ast.Mult,ast.Div,ast.UAdd,ast.USub)):
            raise ValueError('Unsafe column geometry operator')
    return names


def projection(ts, contracts, known_names=None, partial=False):
    result = []
    i = 0
    while i < len(ts):
        t = ts[i]
        key = t.lower()
        if key in CONTAINERS and i + 1 < len(ts) and ts[i + 1] == '{':
            closed = True
            try:
                end = group(ts, i + 1)
            except ValueError:
                if not partial:
                    raise
                end = len(ts); closed = False
            inner = ts[i + 2:end - 1 if closed else end]
            if not (key == 'itemdef' and known_names is not None and decorative(inner, known_names)):
                result += [t, '{'] + projection(inner, contracts, known_names, partial)
                if closed:
                    result.append('}')
            i = end
        elif key in SCRIPTS:
            result.append(t); i += 1
            while i < len(ts) and ts[i] != '{':
                if ts[i] in ('}', ';'):
                    raise ValueError('Missing script block: ' + t)
                result.append(ts[i]); i += 1
            end = group(ts, i)
            result += ['{'] + script_projection(ts[i + 1:end - 1]) + ['}']
            i = end
        elif t == '{':
            # All script/list/conditional blocks are opaque semantic data.
            end = group(ts, i)
            result += ts[i:end]
            i = end
        elif key in PAINT:
            end = i + 1
            for _ in range(PAINT[key]):
                end = atom(ts, end)
            if any(v in ('{', '}', ';', '#') for v in ts[i + 1:end]):
                raise ValueError('Structural tokens in paint property ' + t)
            i = end
        elif key == 'font':
            # ET assetGlobalDef: preserve slot identity; face/point size are paint.
            first = atom(ts, i + 1)
            end = atom(ts, atom(ts, first))
            result += [t] + ts[i + 1:first]
            i = end
        elif key == 'columns':
            # Native list tuple = visual position, visual width, maxChars.
            # Count/order/maxChars remain semantic; geometry is checked math.
            if i+1>=len(ts) or not re.fullmatch(r'(?:[1-9]|1[0-6])',ts[i+1]):
                raise ValueError('Invalid native list column count')
            count=ts[i+1];result += [t,count];i+=2
            for index in range(int(count)):
                for _ in range(2):
                    end=atom(ts,i)
                    refs=arithmetic_names(ts[i:end],getattr(contracts,'geometry_names',frozenset()))
                    contracts.column_refs.update(refs)
                    i=end
                if i>=len(ts) or not re.fullmatch(r'\d+',ts[i]):raise ValueError('Invalid native list maxChars')
                result += ['@column:'+str(index),ts[i]];i+=1
        elif t in contracts and i + 1 < len(ts) and ts[i + 1] == '(':
            args, end = arguments(ts, i + 1)
            count, retain = contracts[t]
            if len(args) != count:
                raise ValueError('Macro arity changed: ' + t)
            result += [t, '(']
            for index, arg in enumerate(args):
                if index in retain:
                    result += ['@arg:' + str(index)] + arg
                else:
                    extension = paint_argument_extension(arg, contracts)
                    if extension:
                        result += ['@paint-extension:' + str(index)] + extension
            result.append(')')
            i = end
        else:
            result.append(t); i += 1
    return result


def contracts(units):
    functions, aliases = {}, {}
    for unit in units:
        for d in unit.defines:
            if d.params is not None:
                functions.setdefault(d.name, []).append(d)
            elif len(d.body) == 1:
                aliases.setdefault(d.name, set()).add(d.body[0])
    result = MacroContracts()
    result.paint_names = frozenset(
        [d.name for u in units for d in u.defines] +
        [p for u in units for d in u.defines for p in (d.params or ())] + ['$evalfloat', '$evalint'])
    for name, defs in functions.items():
        arities = {len(d.params) for d in defs}
        if len(arities) != 1:
            raise ValueError('Conflicting macro arities: ' + name)
        count = arities.pop()
        result[name] = (count, set(range(count)))
    for _ in range(1024):
        old = {k: (v[0], set(v[1])) for k, v in result.items()}
        for name, defs in functions.items():
            used = set()
            for d in defs:
                semantic = projection(d.body, result, partial=True)
                used.update(i for i, p in enumerate(d.params) if p in semantic)
            result[name] = (len(defs[0].params), used)
        for name, targets in aliases.items():
            actual = [result[t] for t in targets if t in result]
            if actual and len({v[0] for v in actual}) == 1:
                result[name] = (actual[0][0], set().union(*(v[1] for v in actual)))
        if result == old:
            return result
    raise ValueError('Macro semantic analysis did not converge')


def signatures(units, macro_contracts=None, known_names=None):
    baseline = macro_contracts is None
    macro_contracts = contracts(units) if baseline else macro_contracts
    # Candidate-only geometry definitions are permitted only if reached from
    # checked list positions/widths and never referenced by semantic data.
    macro_contracts.geometry_names=frozenset(d.name for u in units for d in u.defines if d.params is None)
    macro_contracts.column_refs=set()
    projected = {u.path: projection(u.body, macro_contracts, known_names) for u in units}
    semantic_refs = set(t for value in projected.values() for t in value)
    raw_refs = set(t for u in units for t in u.body)
    objects = {}
    for unit in units:
        for d in unit.defines:
            raw_refs.update(d.body)
            if d.params is not None:
                semantic_refs.update(projection(d.body, macro_contracts, partial=True))
            else:
                objects.setdefault(d.name, []).append(d.body)
    # Object aliases used by a script/label/feeder remain semantic transitively.
    for _ in range(len(objects) + 1):
        prior = set(semantic_refs)
        for name in semantic_refs & objects.keys():
            for body in objects[name]:
                semantic_refs.update(body)
        if semantic_refs == prior:
            break
    column_objects=set()
    def column_object(name,visiting):
        if name in visiting:raise ValueError('Cyclic column geometry constant: '+name)
        if name in column_objects:return
        if name not in objects:raise ValueError('Unbound column geometry constant: '+name)
        for body in objects[name]:
            refs=arithmetic_names(body,objects.keys())
            for ref in refs:column_object(ref,visiting|{name})
        column_objects.add(name)
    for name in macro_contracts.column_refs:column_object(name,set())
    if baseline:
        macro_contracts.paint_objects = {name for name in objects if name not in semantic_refs and name in raw_refs and
                                        all(not any(t in ('{', '}', ';', '#') for t in body) for body in objects[name])}
    result = {}
    for unit in units:
        value = []
        for t in projected[unit.path]:
            if not t.startswith('@define:'):
                value.append(t); continue
            d = unit.defines[int(t.split(':')[1])]
            if d.params is not None:
                value += ['#define', d.name, '('] + list(d.params) + [')'] + projection(d.body, macro_contracts, partial=True)
            elif d.name.startswith('UI_FONT_'):
                if len(d.body) != 1 or not re.fullmatch(r'[0-5]', d.body[0]):
                    raise ValueError('UI font slot outside native0..5: ' + d.name)
                value += ['#define', d.name, '@font-slot']
            elif (unit.path.lower().endswith('menudef.h') or
                  (d.name not in macro_contracts.paint_objects and not (d.name in column_objects and d.name not in semantic_refs))):
                value += ['#define', d.name] + d.body
            # A referenced paint-only scalar/expression definition may restyle.
        result[unit.path] = value
    return result, macro_contracts


PRIVATE_WIDGETS='ui/modern_widgets.h'
ORIGINAL_WIDGETS='ui/menumacros.h'
SETTINGS_NAV={
    'ui/options_customise_game.menu':'game',
    'ui/options_customise_hud.menu':'view',
    'ui/options_controls.menu':'controls',
    'ui/options_system.menu':'system',
}
NAV_TARGETS={'game':'options_customise_game','view':'options_customise_hud',
             'controls':'options_controls','system':'options_system'}
APPEARANCE='ui/modern_appearance.menu'
APPEARANCE_LISTS=tuple('ui/'+name for name in ('menus.txt','menus_gt2.txt','menus_gt5.txt','menus_gt7.txt'))
# Reviewed against the requested palette, opacity, scale and reset contract.
# Presentation geometry/colors are projected out; script/list/cvar data is not.
APPEARANCE_SEMANTIC_SHA256='a95661d9dfd4aa686029da17a8845a7075e623300385d7bd38a9fe4488b77ec3'


def appearance_signature(data,reference):
    units=[parse(p,d) for p,d in reference.items() if Path(p).suffix in SEMANTIC_SUFFIXES and p!=APPEARANCE]
    _,macro_contracts=signatures(units)
    unit=parse(APPEARANCE,data)
    value,_=signatures([unit],macro_contracts)
    return digest(json.dumps(value[APPEARANCE],ensure_ascii=True).encode())


def _appearance_load(ts):
    pattern=['loadMenu','{','"'+APPEARANCE+'"','}'];result=[];count=0;i=0
    while i<len(ts):
        if ts[i:i+4]==pattern:count+=1;i+=4
        else:result.append(ts[i]);i+=1
    return result,count


def layout_reference(reference,candidate):
    """One explicit private paint header, bound to the original macro contract."""
    result=dict(reference)
    if PRIVATE_WIDGETS in candidate:
        if ORIGINAL_WIDGETS not in reference:raise ValueError('Private widgets lack original macro reference')
        result[PRIVATE_WIDGETS]=reference[ORIGINAL_WIDGETS]
    if APPEARANCE in candidate:
        if appearance_signature(candidate[APPEARANCE],reference)!=APPEARANCE_SEMANTIC_SHA256:
            raise ValueError('Appearance page differs from reviewed cvar/list/script contract')
        result[APPEARANCE]=candidate[APPEARANCE].replace(('"'+PRIVATE_WIDGETS+'"').encode(),('"'+ORIGINAL_WIDGETS+'"').encode())
        for path in APPEARANCE_LISTS:
            if path in reference and _appearance_load(parse(path,candidate.get(path,b'')).body)[1]!=1:
                raise ValueError('Appearance page must be loaded exactly once: '+path)
        if 'ui/options.menu' in candidate:
            names=parse('ui/options.menu',candidate['ui/options.menu']).body
            if names.count('"modern_appearance_link"')!=1:
                raise ValueError('Appearance link must be present exactly once in Options')
    return result


def _back_action(unit):
    matches=[];ts=unit.body;i=0
    while i<len(ts):
        if ts[i]=='BUTTON' and i+1<len(ts) and ts[i+1]=='(':
            args,end=arguments(ts,i+1)
            if len(args)==8 and len(args[4])==1 and args[4][0].strip('"').lower()=='back':matches.append(args[7])
            i=end
        else:i+=1
    if len(matches)!=1:raise ValueError('Navigation must bind one exact original Back action: '+unit.path)
    result=list(matches[0])
    while result and result[-1]==';':result.pop()
    return result


def _navigation_fields(inner,contracts):
    ts=projection(inner,contracts);result={};i=0
    while i<len(ts):
        key=ts[i].lower();i+=1
        if key in result:raise ValueError('Duplicate navigation field: '+key)
        if key=='decoration':result[key]=True
        elif key in ('name','text','group','type','visible') and i<len(ts):
            result[key]=ts[i];i+=1
        elif key=='action':
            end=group(ts,i);result[key]=ts[i+1:end-1];i=end
        else:raise ValueError('Unapproved navigation behavior: '+key)
    return result


def normalize_layout(units,reference_units,contracts):
    """Narrow layout exceptions; all other candidate content stays significant."""
    reference={unit.path:unit for unit in reference_units}
    private=PRIVATE_WIDGETS in {unit.path for unit in units}
    result=[]
    for unit in units:
        ts=list(unit.body);seen=set();output=[];i=0
        if unit.path in APPEARANCE_LISTS:
            ts,count=_appearance_load(ts)
            if count and (APPEARANCE not in reference or count!=1):
                raise ValueError('Unbound/duplicate Appearance load reference: '+unit.path)
        while i<len(ts):
            if ts[i]=='#include' and i+1<len(ts) and ts[i+1]=='"'+PRIVATE_WIDGETS+'"':
                original=reference.get(unit.path)
                if (not private or Path(unit.path).name in ('main.menu','global.menu') or original is None or
                        ['#include','"'+ORIGINAL_WIDGETS+'"'] not in [original.body[j:j+2] for j in range(len(original.body)-1)]):
                    raise ValueError('Unapproved private widget include: '+unit.path)
                output+=['#include','"'+ORIGINAL_WIDGETS+'"'];i+=2;continue
            if ts[i].lower()=='itemdef' and i+1<len(ts) and ts[i+1]=='{':
                end=group(ts,i+1);inner=ts[i+2:end-1]
                name=next((inner[j+1] for j,t in enumerate(inner[:-1]) if t.lower()=='name'),None)
                if name=='"modern_appearance_link"':
                    if unit.path!='ui/options.menu' or APPEARANCE not in reference:
                        raise ValueError('Unapproved Appearance link location: '+unit.path)
                    fields=_navigation_fields(inner,contracts)
                    expected={'name':'"modern_appearance_link"','text':'"APPEARANCE"','visible':'1',
                              'type':'ITEM_TYPE_BUTTON','action':['close','options',';','open','modern_appearance',';']}
                    if fields!=expected:raise ValueError('Appearance link differs from exact Options target')
                    i=end;continue
                if name and name.strip('"').startswith('modern_nav_'):
                    key=name.strip('"')[len('modern_nav_'):]
                    if unit.path not in SETTINGS_NAV or key not in NAV_TARGETS or key in seen:
                        raise ValueError('Unapproved/duplicate settings navigation: '+unit.path+' '+str(name))
                    fields=_navigation_fields(inner,contracts)
                    expected={'name':'"modern_nav_'+key+'"','text':'"'+key.upper()+'"','visible':'1'}
                    if 'group' in fields:expected['group']='GROUP_NAME'
                    if SETTINGS_NAV[unit.path]==key:expected['decoration']=True
                    else:
                        if 'type' in fields:expected['type']='ITEM_TYPE_BUTTON'
                        expected['action']=_back_action(reference[unit.path])+[';','close','options',';','open',NAV_TARGETS[key]]
                    if fields!=expected:raise ValueError('Settings navigation differs from exact original exit/target: '+unit.path+' '+key)
                    seen.add(key);i=end;continue
                # Items are inside a menuDef; recurse below to reach them.
                output+=ts[i:end];i=end;continue
            if ts[i].lower() in ('menudef','assetglobaldef') and i+1<len(ts) and ts[i+1]=='{':
                end=group(ts,i+1)
                nested=Unit(unit.path,ts[i+2:end-1],unit.defines,unit.raw)
                # Recurse only into container content, preserving the per-menu
                # all-four check below via the same local walk.
                inside=normalize_layout([nested],reference_units,contracts)[0]
                output += [ts[i],'{']+inside.body+['}'];i=end;continue
            output.append(ts[i]);i+=1
        if seen and seen!=set(NAV_TARGETS):raise ValueError('Incomplete settings category navigation: '+unit.path)
        result.append(Unit(unit.path,output,unit.defines,unit.raw))
    return result


def source_files(root, all_files=False):
    root = Path(root)
    result = {}
    for directory in DIRECTORIES:
        base = root / directory
        if not base.is_dir() or base.is_symlink():
            raise ValueError('Missing/unsafe original UI directory: ' + directory)
    bases = [root] if all_files else [root / directory for directory in DIRECTORIES]
    for base in bases:
        for path in sorted(base.rglob('*')):
            if path.is_symlink():
                raise ValueError('Symlink in UI inventory: ' + str(path))
            if path.is_file():
                result[path.relative_to(root).as_posix()] = path.read_bytes()
    if len(result) != len({p.casefold() for p in result}):
        raise ValueError('Case-colliding UI paths')
    return result


def inventory(root):
    files = source_files(root)
    units = [parse(p, data) for p, data in files.items() if Path(p).suffix.lower() in SEMANTIC_SUFFIXES]
    semantic, macro_contracts = signatures(units)
    counts = {d: {'files': 0, 'menus': 0, 'bytes': 0} for d in DIRECTORIES}
    rows = []
    for path, data in sorted(files.items()):
        directory = path.split('/')[0]
        counts[directory]['files'] += 1
        counts[directory]['menus'] += path.lower().endswith('.menu')
        counts[directory]['bytes'] += len(data)
        row = {'path': path, 'bytes': len(data), 'sha256': digest(data), 'menu': path.lower().endswith('.menu')}
        if path in semantic:
            row.update(encoding=decode(data)[1], semantic_sha256=digest(json.dumps(semantic[path], ensure_ascii=True).encode()))
        rows.append(row)
    return {'schema': 1, 'directories': list(DIRECTORIES), 'counts': counts,
            'files': rows, 'menu_count': sum(v['menus'] for v in counts.values()),
            'file_count': len(rows), 'semantic_file_count': len(units),
            'macro_contracts': {k: {'arity': v[0], 'semantic_arguments': sorted(v[1])} for k, v in sorted(macro_contracts.items())},
            'validator_sha256': {name: digest(Path(__file__).with_name(name).read_bytes())
                                 for name in ('semantic.py', 'inventory.py', 'check.py')},
            'limits': ['Source semantic projection, not engine/runtime/input validation.',
                       'Original scripts/lists/conditions/labels exact except numeric paint RGBA; unknown syntax significant.',
                       'Existing identities/order retained; inert decorations and four exact Back-derived settings links explicitly checked.',
                       'Declared paint, font slots0..5 and checked column position/width may vary; column count/order/maxChars protected.',
                       'Only ui/modern_widgets.h may add a private copy of the original widget contract; main/global includes protected.']}


def compare(manifest, original, edited, require_styled=False):
    current = inventory(original)
    errors = []
    if manifest != current:
        return {'passed': False, 'errors': ['Original inventory/provenance changed; regenerate only after explicit review.']}
    baseline_files = source_files(original)
    if {p:digest(data) for p,data in baseline_files.items()} != {row['path']:row['sha256'] for row in manifest['files']}:
        return {'passed':False,'errors':['Original changed while reading inventory']}
    edited_all = Path(original).resolve() != Path(edited).resolve()
    candidate_files = source_files(edited, all_files=edited_all)
    baseline = {p for p in baseline_files if Path(p).suffix.lower() in SEMANTIC_SUFFIXES}
    candidate = {p for p in candidate_files if p in baseline or Path(p).suffix.lower() in ('.menu', '.h') or
                 (Path(p).suffix.lower() == '.txt' and p.split('/')[0] in DIRECTORIES)}
    reference_files=layout_reference({p:baseline_files[p] for p in baseline},candidate_files)
    baseline=set(reference_files)
    if baseline != candidate:
        errors.append('Semantic file coverage differs: missing=' + str(sorted(baseline - candidate)) + ', extra=' + str(sorted(candidate - baseline)))
    before_units = [parse(p, reference_files[p]) for p in sorted(baseline)]
    before, macro_contracts = signatures(before_units)
    after_units = [parse(p, candidate_files[p]) for p in sorted(candidate)]
    names = set()
    for u in before_units:
        for ts in [u.body] + [d.body for d in u.defines]:
            names.update(ts[i + 1] for i, t in enumerate(ts[:-1]) if t.lower() == 'name')
    after, _ = signatures(normalize_layout(after_units,before_units,macro_contracts), macro_contracts, names)
    compared = []
    for path in sorted(baseline & candidate):
        same = before[path] == after[path]
        if not same:
            index = next((i for i, pair in enumerate(zip(before[path], after[path])) if pair[0] != pair[1]), min(len(before[path]), len(after[path])))
            errors.append(path + ': semantic difference at token ' + str(index) + ': ' + repr(before[path][max(0,index-3):index+5]) + ' -> ' + repr(after[path][max(0,index-3):index+5]))
        compared.append({'path': path, 'same_semantics': same, 'source_sha256': digest(reference_files[path]), 'edited_sha256': digest(candidate_files[path])})
    raw_before = {u.path: u.raw for u in before_units}
    unstyled = [u.path for u in after_units if u.path!=APPEARANCE and u.path.lower().endswith('.menu') and
                u.path in raw_before and u.raw == raw_before[u.path]]
    if require_styled and unstyled:
        errors.append('Menus without a real non-comment token/style change: ' + str(unstyled))
    stable = source_files(original) == baseline_files and source_files(edited, all_files=edited_all) == candidate_files
    if not stable:
        errors.append('Original or edited inputs changed during comparison')
    return {'schema': 1, 'passed': not errors, 'errors': errors, 'menu_count': manifest['menu_count'],
            'semantic_files_compared': len(compared), 'files': compared,
            'require_styled': require_styled, 'unstyled_menus': unstyled,
            'input_stable': stable,
            'additional_assets': {p: digest(data) for p, data in candidate_files.items() if p not in baseline_files},
            'limits': current['limits']}
