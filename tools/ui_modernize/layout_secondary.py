"""Structural layouts for credits, voting, calibration and authoring pages.

The immutable originals retain every label, command, condition and feeder.  Only
geometry/paint changes and label-free decoration are introduced here.
"""
from pathlib import Path
import re


def _helpers():
    from restyle import blocks, item_prop, macro_calls
    return blocks, item_prop, macro_calls


def _canvas(text):
    for key, value in (('WINDOW_X', '0'), ('WINDOW_Y', '0'),
                       ('WINDOW_WIDTH', '852'), ('WINDOW_HEIGHT', '480')):
        text = re.sub(r'(?m)^#define\s+' + key + r'\s+[^\n]+',
                      '#define ' + key + ' ' + value, text)
    return text


def _calls(text, names, change):
    _, _, macro_calls = _helpers()
    for start, end, name, args in reversed(list(macro_calls(text, names))):
        result = change(name, list(args))
        if result is not None:
            text = text[:start] + name + '( ' + ', '.join(result) + ' )' + text[end:]
    return text


def _rect(args, rect, offset=0):
    args[offset:offset + 4] = list(map(str, rect))
    return args


def _button(args, rect, offset=0, scale='.28'):
    _rect(args, rect, offset)
    args[5 + offset:7 + offset] = [scale, str(round(rect[3] * .68))]
    return args


def _name(block):
    match = re.search(r'\bname\s+(?:"([^"]+)"|([^\s{}]+))', block)
    return (match[1] or match[2]) if match else ''


def _items(text, change):
    blocks, item_prop, _ = _helpers()
    for start, end in reversed(list(blocks(text))):
        block = text[start:end]
        for key, value in (change(_name(block), block) or {}).items():
            if key == 'columns':
                block = re.sub(r'(?im)^([ \t]*columns[ \t]+)[^\n]*'
                               r'(?:\n[ \t]+(?:\$evalfloat|\d)[^\n]*)*',
                               lambda m: m[1] + value, block)
            else:
                block = item_prop(block, key, str(value))
        text = text[:start] + block + text[end:]
    return text


def _card(name, rect):
    return '''
    itemDef {
        name "%s"
        rect %s
        style WINDOW_STYLE_FILLED
        backcolor .035 .055 .075 .94
        border WINDOW_BORDER_FULL
        bordercolor .18 .24 .29 .65
        visible 1
        decoration
    }
''' % (name, ' '.join(map(str, rect)))


def _cards(text, rects):
    _, _, macro_calls = _helpers()
    calls = list(macro_calls(text, {'WINDOW'}))
    if calls:
        at = calls[0][1]
    else:
        blocks, _, _ = _helpers()
        windows = [(s, e) for s, e in blocks(text) if _name(text[s:e]) == 'window']
        if not windows:
            return text
        at = windows[0][1]
    return text[:at] + ''.join(_card('modern_secondary_%d' % i, r)
                               for i, r in enumerate(rects)) + text[at:]


def _background(name):
    return {'rect': '0 0 852 480'} if name == 'background' else {}


def _credits(text, filename):
    # The original name/role pairs become an aligned editorial table beside a
    # permanent studio/logo rail.  Paragraph credits keep their full width.
    text = _canvas(text)
    _, _, macro_calls = _helpers()
    buttons = list(macro_calls(text, {'BUTTON', 'BUTTONEXT'}))

    def offset(expr):
        return sum(int(n) for n in re.findall(r'[+-]?\d+', expr.replace('CREDITS_Y', '0')))

    def change(name, args):
        if name.startswith('LABEL'):
            y = 98 + offset(args[1])
            align = args[6]
            if 'ITEM_ALIGN_RIGHT' in align:
                _rect(args, (236, y, 170, 18)); args[7] = '170'
            elif 'ITEM_ALIGN_LEFT' in align and name == 'LABELWHITE':
                _rect(args, (426, y, 390, 18)); args[7] = '0'
            else:
                _rect(args, (236, y, 580, 18)); args[6:8] = ['ITEM_ALIGN_LEFT', '0']
            args[5] = '.23' if len(args[4]) < 75 else '.21'
            args[8] = '14'
            return args
        if name in ('BUTTON', 'BUTTONEXT'):
            label = args[4]
            if label == '"BACK"':
                rect = (24, 442, 184, 28)
            else:
                # Keep the original left-to-right studio navigation order.
                index = next((i for i, (_, _, _, a) in enumerate(buttons) if a[4] == label), 1)
                rect = (364 if index == 1 else 638, 442, 252 if index == 1 else 190, 28)
            return _button(args, rect)
        return None

    text = _calls(text, {'LABEL', 'LABELWHITE', 'BUTTON', 'BUTTONEXT'}, change)
    text = _cards(text, ((24, 60, 176, 372), (224, 60, 604, 372)))
    groove = filename in ('credits_groovesix.menu', 'credits_groovesix_sub.menu', 'credits_quit.menu')

    def change_item(name, block):
        result = _background(name)
        if name == 'headerLogo':
            result.update(rect='24 8 804 36', textalign='ITEM_ALIGN_LEFT', textalignx='0',
                          textaligny='27', textscale='.40')
        elif name.startswith('header'):
            original = re.search(r'(?im)^\s*rect\s+(.+)', block)
            line = original[1] if original else ''
            if 'CREDITS_Y' in line:
                expr = re.search(r'\$evalfloat\((CREDITS_Y[^)]*)\)', line)
                y = 98 + offset(expr[1]) if expr else 82
            elif name == 'headerGS':
                y = 82
            else:
                y = 82
            result.update(rect='236 %d 580 18' % y, textalign='ITEM_ALIGN_LEFT', textalignx='0',
                          textaligny='14', textscale='.23')
        elif name.startswith('logo'):
            shadow = 'Shadow' in name
            key = name.replace('Shadow', '')
            if filename == 'credits_additional.menu':
                rect = {'logoGM': (60, 84, 104, 52), 'logoNerve': (48, 168, 128, 32),
                        'logoOSP': (76, 236, 72, 72), 'logoPB': (76, 332, 72, 72)}.get(key)
            elif groove:
                rect = (48, 114, 128, 64) if key == 'logo' else (36, 250, 152, 45)
            elif filename == 'credits_activision.menu':
                rect = (36, 128, 152, 38)
            else:
                rect = (48, 104, 128, 128)
            if rect:
                x, y, w, h = rect
                result['rect'] = '%d %d %d %d' % (x + int(shadow), y + int(shadow), w, h)
        return result
    return _items(text, change_item)


def _vote(text, filename):
    text = _canvas(text)
    if filename == 'ingame_vote.menu':
        positions = {'"MISC"': (24, 116, 256, 184),
                     '"MAP"': (298, 116, 256, 184),
                     '"PLAYERS"': (572, 116, 256, 184),
                     '"BACK"': (24, 442, 184, 28)}
        text = _calls(text, {'BUTTON', 'BUTTONEXT'},
                      lambda n, a: _button(a, positions[a[4]], scale='.42' if a[4] != '"BACK"' else '.28'))
        def change_item(name, block):
            result = _background(name)
            if name == 'windowtitle':
                result.update(rect='24 8 804 36', textscale='.44', textalign='ITEM_ALIGN_LEFT',
                              textalignx='0', textaligny='28')
            return result
        return _items(text, change_item)

    if filename in ('ingame_vote_map.menu', 'ingame_vote_players.menu'):
        players = filename == 'ingame_vote_players.menu'
        rows = {'KICK': 78, 'MUTE': 120, 'UN-MUTE': 120, 'MAKE REFEREE': 162,
                'UN-REFEREE': 162, 'MAKE AXIS': 204, 'MAKE ALLIED': 246,
                'MAKE SPEC': 288, 'WARN': 330, 'BAN': 372}
        def change(name, args):
            label = args[4].strip('"')
            if label == 'BACK':
                return _button(args, (24, 442, 184, 28))
            return _button(args, (606, rows[label] if players else 92, 206, 32 if players else 48),
                           scale='.27' if players else '.36')
        text = _calls(text, {'BUTTON', 'BUTTONEXT'}, change)
        text = _cards(text, ((24, 60, 542, 372), (590, 60, 238, 372)))
        def change_item(name, block):
            result = _background(name)
            if name in ('playerList', 'mapList_maps'):
                result.update(rect='36 76 518 340', elementwidth='502', elementheight='26',
                              textscale='.28', textaligny='-2', outlinecolor='.94 .75 .27 .20',
                              columns='1 0 502 33')
            return result
        return _items(text, change_item)

    # On/off and privilege-specific alternatives intentionally share one slot.
    referee = filename == 'ingame_vote_misc_refrcon.menu'
    positions = {'START MATCH': (0, 0), 'RESET MATCH': (1, 0), 'RESTART MAP': (2, 0),
                 'NEXT MAP': (0, 1), 'NEXT CAMPAIGN': (0, 1), 'SWAP TEAMS': (1, 1),
                 'MUTE SPECS': (2, 1), 'UNMUTE SPECS': (2, 1),
                 'ANTI-LAG ON': (0, 2), 'ANTI-LAG OFF': (0, 2),
                 'BALANCED TEAMS ON': (1, 2), 'BALANCED TEAMS OFF': (1, 2),
                 'PAUSE': (2, 2), 'UNPAUSE': (2, 2),
                 'LOCK TEAMS': (0, 3), 'UNLOCK TEAMS': (0, 3),
                 'LOCK SPECS': (1, 3), 'UNLOCK SPECS': (1, 3)}
    time_y = 336 if referee else 248
    def change(name, args):
        if name == 'NUMERICFIELDLEFTEXT':
            _rect(args, (588, time_y + 24, 146, 24)); args[5:7] = ['.22', '17']
            return args
        offset = int(name.startswith('NAMEDBUTTON'))
        label = args[4 + offset].strip('"')
        if label == 'BACK':
            return _button(args, (24, 442, 184, 28), offset)
        if label == 'OK':
            return _button(args, (746, time_y + 16, 66, 36), offset)
        if label in positions:
            col, row = positions[label]
            return _button(args, (24 + 274 * col, 72 + 88 * row, 256, 68), offset, '.30')
        return None
    text = _calls(text, {'BUTTON', 'BUTTONEXT', 'NAMEDBUTTONEXT', 'NUMERICFIELDLEFTEXT'}, change)
    text = _cards(text, ((572, time_y, 256, 68),))
    def change_item(name, block):
        result = _background(name)
        if name == 'nfleftbackTimelimit:':
            result['rect'] = '662 %d 72 24' % (time_y + 24)
        return result
    return _items(text, change_item)


def _gamma(text):
    text = _canvas(text)
    _, _, macro_calls = _helpers()
    labels = list(macro_calls(text, {'LABEL', 'LABELWHITE'}))
    indices = {start: index for index, (start, *_) in enumerate(labels)}
    locations = ((24, 60, 804, 22), (24, 90, 804, 18), (24, 108, 804, 18),
                 (24, 126, 804, 18), (24, 144, 804, 18),
                 (40, 186, 358, 24), (40, 214, 358, 18),
                 (454, 186, 358, 24), (454, 214, 358, 18))
    # Work in source order so repeated LABEL macro names keep their identities.
    for start, end, name, args in reversed(labels):
        index = indices[start]
        _rect(args, locations[index]); args[5] = '.28' if index in (0, 5, 7) else '.22'
        args[7:9] = ['0', '17' if index in (0, 5, 7) else '14']
        text = text[:start] + name + '( ' + ', '.join(args) + ' )' + text[end:]
    def change(name, args):
        if name == 'BUTTON':
            return _button(args, (24, 442, 184, 28))
        if name == 'SLIDER':
            _rect(args, (236, 408, 320, 22)); args[5:7] = ['.26', '16']
            return args
        if name == 'CVARFLOATLABEL':
            _rect(args, (568, 408, 148, 22)); args[5] = '.26'; args[7:9] = ['148', '16']
            return args
        return None
    text = _calls(text, {'BUTTON', 'SLIDER', 'CVARFLOATLABEL'}, change)
    text = _cards(text, ((24, 170, 390, 222), (438, 170, 390, 222)))
    def change_item(name, block):
        result = _background(name)
        dynamic = 'gammabar16de' in block or 'hardlyvisiblede' in block
        if name == 'gammaBar':
            result['rect'] = '%d 248 358 32' % (454 if dynamic else 40)
        elif name == 'threesquares':
            result['rect'] = '%d 296 288 70' % (489 if dynamic else 75)
        return result
    return _items(text, change_item)


def _containers(text, keyword):
    token = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:[^"\\]|\\.)*"|\b' + keyword + r'\b|[{}]', re.S | re.I)
    begin = None; depth = 0
    for match in token.finditer(text):
        value = match[0]
        if begin is None and value.lower() == keyword.lower():
            begin = match.start(); depth = 0
        elif begin is not None:
            depth += (value == '{') - (value == '}')
            if value == '}' and depth == 0:
                yield begin, match.end(); begin = None


def _inline_prop(block, key, value, arity=1):
    # Generated authoring menus place each item on one line.  Edit only tokens
    # at the item's own depth; never match quoted text or nested action scripts.
    token = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:[^"\\]|\\.)*"|[{}]|[^\s{}]+', re.S)
    tokens = list(token.finditer(block)); depth = 0
    for index, match in enumerate(tokens):
        depth += (match[0] == '{') - (match[0] == '}')
        if depth == 1 and match[0].lower() == key.lower():
            return block[:match.end()] + ' ' + value + block[tokens[index + arity].end():]
    at = block.index('{') + 1
    return block[:at] + ' ' + key + ' ' + value + ' ' + block[at:]


def _editor(text):
    blocks, _, _ = _helpers()
    for start, end in reversed(list(_containers(text, 'menuDef'))):
        menu = text[start:end]
        menu_name = _name(menu)
        items = list(blocks(menu))
        controls = [(s, e) for s, e in items if _name(menu[s:e]) not in ('title', 'back', 'close')]
        order = {s: i for i, (s, _) in enumerate(controls)}
        for item_start, item_end in reversed(items):
            block = menu[item_start:item_end]; name = _name(block)
            values = {'textfont': 'UI_FONT_COURBD_21', 'textscale': '.28',
                      'textalignx': '18', 'textaligny': '36',
                      'style': 'WINDOW_STYLE_FILLED', 'backcolor': '.055 .075 .10 .97',
                      'border': 'WINDOW_BORDER_FULL', 'bordercolor': '.18 .24 .29 .65'}
            if name == 'title':
                rect = (24, 8, 804, 36)
                values.update(textfont='UI_FONT_ARIBLK_27', textscale='.44', textalignx='0',
                              textaligny='28', backcolor='0 0 0 0', bordercolor='0 0 0 0')
            elif name in ('back', 'close'):
                rect = (24 if name == 'back' else 638, 442, 184 if name == 'back' else 190, 28)
                values['textaligny'] = '20'
            elif menu_name == 'tce_ob_name':
                rect = {'goalname': (40, 96, 772, 42), 'applyname': (40, 168, 374, 60),
                        'applygroup': (438, 168, 374, 60)}[name]
                if name == 'goalname':
                    values['textaligny'] = '28'
            else:
                index = order[item_start]
                columns = 3 if menu_name == 'tce_omnibot' else 2
                width = 256 if columns == 3 else 390
                stride = 274 if columns == 3 else 414
                rect = (24 + (index % columns) * stride, 76 + (index // columns) * 70, width, 56)
                if columns == 3:
                    values['textscale'] = '.25'
            values['rect'] = ' '.join(map(str, rect))
            for key, value in values.items():
                block = _inline_prop(block, key, value, 4 if key in ('rect', 'backcolor', 'bordercolor') else 1)
            menu = menu[:item_start] + block + menu[item_end:]
        menu = _inline_prop(menu, 'rect', '0 0 852 480', 4)
        menu = _inline_prop(menu, 'backcolor', '.025 .040 .055 .97', 4)
        text = text[:start] + menu + text[end:]
    return text


def apply(text, relative):
    path = Path(relative)
    if path.parent.name != 'ui':
        return text
    name = path.name
    if name.startswith('credits_') and name.endswith('.menu'):
        return _credits(text, name)
    if name.startswith('ingame_vote') and name.endswith('.menu') and name not in (
            'ingame_vote_players_warn.menu', 'ingame_vote_disabled.menu'):
        return _vote(text, name)
    if name == 'options_system_gamma.menu':
        return _gamma(text)
    if name == 'tce_omnibot.menu':
        return _editor(text)
    return text
