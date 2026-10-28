"""Presentation-only workspace layouts for the original browser/setup menus.

These transforms consume the immutable pak text after palette restyling.  They
retain the original controls, macro identities, labels, scripts and cvar lists.
Imports are delayed because restyle calls this module during menu generation.
"""
from pathlib import Path
import re


NAMES = {
    'playonline.menu', 'playonline_sub.menu',
    'hostgame.menu', 'hostgame_sub.menu', 'hostgame_advanced.menu',
    'profile.menu', 'profile_sub.menu', 'profile_create.menu',
    'profile_create_initial.menu', 'profile_rename.menu', 'viewreplay.menu',
}


def _helpers():
    from restyle import blocks, item_prop, macro_calls
    return blocks, item_prop, macro_calls


def _canvas(text):
    for name, value in (('WINDOW_X', '0'), ('WINDOW_Y', '0'),
                        ('WINDOW_WIDTH', '852'), ('WINDOW_HEIGHT', '480')):
        text = re.sub(r'(?m)^#define\s+' + name + r'\s+[^\n]+',
                      '#define ' + name + ' ' + value, text)
    return text


def _macros(text, names, transform):
    _, _, macro_calls = _helpers()
    for start, end, name, args in reversed(list(macro_calls(text, names))):
        changed = transform(name, list(args))
        if changed is not None:
            text = text[:start] + name + '( ' + ', '.join(changed) + ' )' + text[end:]
    return text


def _rect(args, geometry, offset=0):
    args[offset:offset + 4] = [str(value) for value in geometry]
    return args


def _field(args, geometry, scale='.22', baseline='15'):
    _rect(args, geometry)
    args[5:7] = [scale, baseline]
    return args


def _items(text, transform):
    blocks, item_prop, _ = _helpers()
    for start, end in reversed(list(blocks(text))):
        block = text[start:end]
        match = re.search(r'(?im)^\s*name\s+(?:"([^"]+)"|([^\s]+))', block)
        name = (match[1] or match[2]) if match else ''
        changes = transform(name, block)
        for key, value in (changes or {}).items():
            if key == 'columns':
                # Native listbox columns are (x, width, maxChars) triples.
                # Only the first two fields move; the original text limits stay.
                block = re.sub(r'(?im)^([ \t]*columns[ \t]+)[^\n]*'
                               r'(?:\n[ \t]+(?:\$evalfloat|\d)[^\n]*)*',
                               lambda m: m[1] + value, block)
            else:
                block = item_prop(block, key, str(value))
        text = text[:start] + block + text[end:]
    return text


def _background(name, _):
    if name == 'background':
        return {'rect': '0 0 852 480'}
    return {}


def _cards(text, cards):
    """Place label-free, noninteractive cards after the original window shell."""
    _, _, macro_calls = _helpers()
    shells = list(macro_calls(text, {'WINDOW', 'SUBWINDOWBLACK'}))
    if not shells:
        return text
    at = shells[0][1]
    decoration = '\n'
    for index, geometry in enumerate(cards):
        decoration += '''    itemDef {
        name "modern_browse_card_%d"
        rect %s
        style WINDOW_STYLE_FILLED
        backcolor .035 .055 .075 .94
        border WINDOW_BORDER_FULL
        bordercolor .18 .24 .29 .65
        visible 1
        decoration
    }
''' % (index, ' '.join(str(value) for value in geometry))
    return text[:at] + decoration + text[at:]


def _server(text):
    # The filters form a persistent rail.  The wide table occupies the entire
    # lower workspace, while MOTD and a true 2.13:1 map card sit above it.
    panels = {
        '"Game Filters"': (24, 60, 184, 372),
        '"Message of the Day"': (224, 60, 364, 126),
        '"Map Preview"': (600, 60, 228, 126),
        '"Server Browser"': (224, 186, 604, 246),
    }
    filters = {
        'ui_browserShowEmptyOrFull': 158,
        'ui_browserShowPasswordProtected': 182,
        'ui_browserShowFriendlyFire': 206,
        'ui_browserShowMaxlives': 230,
        'ui_browserShowWeaponsRestricted': 254,
        'ui_browserShowPunkBuster': 278,
        'ui_browserShowAntilag': 302,
        'ui_browserShowTeamBalanced': 326,
    }
    buttons = {
        '"Refresh List!"': (232, 406, 112, 22),
        '"Refresh List"': (232, 406, 112, 22),
        '"Connect to IP"': (352, 406, 112, 22),
        '"Server Info"': (472, 406, 112, 22),
        '"Enable PB"': (592, 406, 112, 22),
        '"Disable PB"': (592, 406, 112, 22),
        '"Main Menu"': (24, 442, 184, 28),
        '"Join Server"': (712, 406, 112, 22),
    }

    def change_macro(name, args):
        if name == 'SUBWINDOW' and args[4] in panels:
            return _rect(args, panels[args[4]])
        if name == 'MULTIACTIONLEFT':
            return _field(args, (36, 88 if args[4] == '"Source:"' else 118, 160, 22), '.20', '15')
        if name == 'TRICHECKBOXACTIONMULTI':
            key = args[6].strip('"')
            if key in filters:
                _rect(args, (54, filters[key], 142, 20))
                args[4:6] = ['.20', '14']
                return args
        if name in ('BUTTON', 'NAMEDBUTTON'):
            offset = int(name == 'NAMEDBUTTON')
            label = args[4 + offset]
            if label in buttons:
                _rect(args, buttons[label], offset)
                args[5 + offset:7 + offset] = ['.26' if buttons[label][1] == 442 else '.22',
                                               '20' if buttons[label][1] == 442 else '16']
                return args
        return None

    text = _macros(text, {'SUBWINDOW', 'MULTIACTIONLEFT', 'TRICHECKBOXACTIONMULTI',
                          'BUTTON', 'NAMEDBUTTON'}, change_macro)
    icons = dict(zip(('EmptyFull', 'Password', 'FriendlyFire', 'MaxLives',
                      'WeapRestrict', 'PunkBuster', 'Antilag', 'TeamBalance'), filters.values()))
    widths = (282, 88, 42, 46, 34, 84, 12)
    columns = ('server', 'map', 'player', 'gametype', 'ping', 'filters', 'favourites')
    geometry = {}
    x = 224
    for label, width in zip(columns, widths):
        geometry[label + 'ColumnTab'] = (x, 208, width + (16 if label == 'favourites' else 0), 24)
        geometry[label + 'Column'] = (x, 232, width, 168)
        x += width

    def change_item(name, block):
        values = _background(name, block)
        if name == 'messageOfTheDay':
            values.update(rect='236 88 340 72', textscale='.24', textalignx='0', textaligny='18')
        elif name == 'descriptionFilters':
            values.update(rect='36 356 160 68', textscale='.20', textalignx='4', textaligny='14',
                          backcolor='.055 .075 .10 .8', bordercolor='.18 .24 .29 .5')
        elif name == 'mapPreview':
            values.update(rect='608 82 212 99', bordercolor='.27 .34 .40 .6')
        elif name.startswith('filtericon') and name[len('filtericon'):] in icons:
            values['rect'] = '36 %d 14 14' % (icons[name[len('filtericon'):]] + 3)
        elif name in geometry:
            values.update(rect=' '.join(str(v) for v in geometry[name]),
                          bordercolor='.18 .24 .29 .35')
            if name.endswith('Tab'):
                values.update(textscale='.20', textalignx='5', textaligny='17',
                              backcolor='.10 .14 .18 .94', forecolor='.82 .87 .90 1')
        elif re.fullmatch(r'row[1-6]', name):
            # Existing stripes remain named, now aligned to the 21px row rhythm.
            values.update(rect='224 %d 588 21' % (253 + (int(name[-1]) - 1) * 42),
                          backcolor='.12 .17 .22 .16')
            # Six legacy stripes exceed the taller row rhythm. Keep the last
            # two inside the list using a thinner repeat instead of drawing
            # below the table or under the footer controls.
            if name in ('row5', 'row6'):
                values['rect'] = '224 %d 588 1' % (232 + (int(name[-1]) - 1) * 28)
        elif name == 'serverList':
            values.update(rect='224 232 604 168', elementwidth='588', elementheight='21',
                          textscale='.21', textaligny='-2', outlinecolor='.94 .75 .27 .20',
                          columns='7 0 282 39 282 88 13 370 42 10 412 46 10 458 34 4 492 84 5 576 12 1')
        return values

    # Reserve 84px for the six original 14px filter glyphs.  Their native
    # feeder renderer uses fixed icon spacing, so that column is not compressed.
    # Data columns total 588px, leaving the final 16px for the native scrollbar.
    text = _items(text, change_item)
    # Filter descriptions are cvar text: native text items otherwise render
    # one unbounded line even when a rectangle height is specified.
    blocks, _, _ = _helpers()
    for start, end in reversed(list(blocks(text))):
        block = text[start:end]
        if re.search(r'\bname\s+"descriptionFilters"', block) and not re.search(r'\bautowrapped\b', block):
            block = block[:-1] + '\n        autowrapped\n    }'
            text = text[:start] + block + text[end:]
    return text


def _host(text):
    panels = {
        '"Settings"': (440, 60, 388, 184),
        '"Features"': (440, 254, 388, 90),
        '"Name"': (24, 294, 400, 138),
        '"Preview"': (24, 60, 400, 224),
        '"Details"': (440, 354, 388, 78),
    }
    fields = {
        '"Server Name:"': 88, '"Game Type:"': 112,
        '"Maximum Players:"': 136, '"Auto-Configure Server:"': 160,
        '"Dedicated Server:"': 184, '"Password:"': 208,
    }
    features = {
        'Official Server': (478, 280), 'PunkBuster': (478, 302),
        'Anti-Lag': (478, 324), 'Max Lives': (668, 280),
        'Weapon Restrictions': (668, 302), 'Balanced Teams': (668, 324),
    }
    check_names = {'labelFriendly Fire': 'Official Server',
                   'labelPunkBuster': 'PunkBuster', 'labelAnti-Lag': 'Anti-Lag',
                   'labelMax Lives': 'Max Lives',
                   'labelWeapon Restrictions': 'Weapon Restrictions',
                   'labelBalanced Teams': 'Balanced Teams'}
    buttons = {'"Main Menu"': (24, 442, 184, 28),
               '"Advanced"': (226, 442, 184, 28),
               '"Start Server"': (638, 442, 190, 28)}

    def change_macro(name, args):
        if name == 'SUBWINDOW' and args[4] in panels:
            return _rect(args, panels[args[4]])
        if name in ('EDITFIELD', 'MULTIACTION', 'NUMERICFIELD', 'MULTI') and args[4] in fields:
            return _field(args, (448, fields[args[4]], 372, 20))
        if name == 'LABEL' and args[4].strip('"') in features:
            x, y = features[args[4].strip('"')]
            _rect(args, (x, y, 144, 18)); args[5] = '.20'; args[7:9] = ['0', '13']
            # args[6] may include cvarTest/hideCVar, and must remain exact.
            return args
        if name in ('CHECKBOXNOTEXT', 'CHECKBOXNOTEXTACTION'):
            label = args[0].strip('"')
            if label in check_names:
                x, y = features[check_names[label]]
                return _rect(args, (x - 18, y + 2, 18, 14), 1)
        if name == 'BUTTON' and args[4] in buttons:
            return _field(args, buttons[args[4]], '.27', '20')
        return None

    text = _macros(text, {'SUBWINDOW', 'EDITFIELD', 'MULTIACTION', 'NUMERICFIELD', 'MULTI',
                          'LABEL', 'CHECKBOXNOTEXT', 'CHECKBOXNOTEXTACTION', 'BUTTON'}, change_macro)
    icon_feature = {'FriendlyFire': 'Official Server', 'PunkBuster': 'PunkBuster',
                    'Antilag': 'Anti-Lag', 'MaxLives': 'Max Lives',
                    'WeapRestrict': 'Weapon Restrictions', 'TeamBalance': 'Balanced Teams'}

    def change_item(name, block):
        values = _background(name, block)
        if name == 'campaignPreview':
            values.update(rect='32 90 384 180', bordercolor='.27 .34 .40 .6')
        elif name == 'mapList':
            values.update(rect='32 318 384 106', elementwidth='368', elementheight='21',
                          textscale='.25', textaligny='-2', outlinecolor='.94 .75 .27 .20',
                          columns='1 0 368 60')
        elif name in ('labelDetailsGametype', 'labelDetailsCampaign'):
            values.update(rect='450 378 364 44', textscale='.20', textalignx='0', textaligny='14')
        elif name.startswith('filtericon') and name[len('filtericon'):] in icon_feature:
            x, y = features[icon_feature[name[len('filtericon'):]]]
            values['rect'] = '%d %d 12 12' % (x - 34, y + 3)
        return values

    return _items(text, change_item)


def _advanced(text):
    # Numeric/choice controls need wide label/value halves.  Voting checkboxes
    # need only a label and an 18px mark, so give that rail less width rather
    # than shrinking every long server-setting label to an unreadable size.
    panels = {
        '"GENERAL"': (24, 60, 224, 126),
        '"LAST MAN STANDING"': (24, 196, 224, 104),
        '"MESSAGE OF THE DAY"': (24, 310, 224, 122),
        '"MATCH"': (255, 60, 224, 372),
        '"ADMIN"': (486, 60, 224, 106),
        '"VOTING"': (486, 176, 224, 72),
        '"DOWNLOAD"': (486, 258, 224, 174),
        '"ALLOW VOTES"': (718, 60, 110, 372),
    }
    groups = {
        'general': (28, 86, 216, 20, ('sv_minping', 'sv_maxping', 'sv_maxrate', 'g_inactivity', 'g_spectatorInactivity')),
        'lms': (28, 222, 216, 20, ('g_lms_roundlimit', 'g_lms_matchlimit', 'g_lms_followTeamOnly', 'g_lms_lockTeams')),
        'match': (259, 86, 216, 20, ('g_doWarmup', 'match_minplayers', 'match_readypercent',
                    'match_latejoin', 'match_mutespecs', 'match_warmupDamage', 'team_maxplayers',
                    'team_nocontrols', 'g_warmup', 'g_complaintlimit', 'g_ipcomplaintlimit',
                    'ui_userTimeLimit', 'ui_userAlliedRespawnTime', 'ui_userAxisRespawnTime',
                    'g_alliedmaxlives', 'g_axismaxlives', 'g_altStopwatchMode')),
        'admin': (490, 86, 216, 20, ('sv_pure', 'g_password', 'refereePassword', 'rconPassword')),
        'voting': (490, 202, 216, 20, ('vote_limit', 'vote_percent')),
        'download': (490, 286, 216, 30, ('sv_allowDownload', 'sv_dl_maxRate', 'sv_wwwDownload', 'sv_wwwBaseUrl')),
        'motd': (28, 336, 216, 18, ('server_motd0', 'server_motd1', 'server_motd2', 'server_motd3', 'server_motd4')),
        'votes': (722, 86, 102, 20, ('vote_allow_comp', 'vote_allow_gametype', 'vote_allow_kick',
                    'vote_allow_map', 'vote_allow_matchreset', 'vote_allow_mutespecs', 'vote_allow_nextmap',
                    'vote_allow_balancedteams', 'vote_allow_pub', 'vote_allow_referee',
                    'vote_allow_shuffleteamsxp', 'vote_allow_swapteams', 'vote_allow_friendlyfire',
                    'vote_allow_timelimit', 'vote_allow_warmupdamage', 'vote_allow_antilag')),
    }
    positions = {cvar: (x, y + index * step, width, 18)
                 for x, y, width, step, cvars in groups.values()
                 for index, cvar in enumerate(cvars)}

    def change_macro(name, args):
        if name == 'SUBWINDOWBLACK':
            return _rect(args, (0, 0, 852, 432))
        if name == 'SUBWINDOW' and args[4] in panels:
            return _rect(args, panels[args[4]])
        if name == 'BUTTON':
            return _field(args, (24 if args[4] == '"BACK"' else 638, 442,
                                 184 if args[4] == '"BACK"' else 190, 28), '.27', '20')
        index = 8 if name == 'CHECKBOXALIGNX' else 7
        if len(args) > index:
            cvar = args[index].strip('"')
            if cvar in positions:
                _rect(args, positions[cvar]); args[5] = '.20'
                if name == 'CHECKBOXALIGNX':
                    # Right-aligned text leaves enough room for the original
                    # native checkbox at the beginning of the vote row.
                    args[6:8] = ['25', '13']
                else:
                    args[6] = '13'
                return args
        return None

    text = _macros(text, {'SUBWINDOWBLACK', 'SUBWINDOW', 'NUMERICFIELD', 'YESNO',
                          'MULTI', 'EDITFIELD', 'EDITFIELDLEFT', 'CHECKBOXALIGNX', 'BUTTON'}, change_macro)

    def change_item(name, block):
        values = _background(name, block)
        match = re.fullmatch(r'efleftbackLine ([1-5]):', name)
        if match:
            values['rect'] = '62 %d 182 18' % (336 + (int(match[1]) - 1) * 18)
        return values
    return _items(text, change_item)


def _profiles(text):
    buttons = {'"CREATE"': (574, 92, 238, 36),
               '"DELETE"': (574, 142, 238, 36),
               '"RENAME"': (574, 192, 238, 36),
               '"SET AS DEFAULT"': (574, 242, 238, 36),
               '"BACK"': (24, 442, 184, 28),
               '"SELECT PROFILE"': (638, 442, 190, 28)}

    def change_macro(name, args):
        if args[4] in buttons:
            return _field(args, buttons[args[4]], '.30', '25' if buttons[args[4]][3] == 36 else '20')
        return None

    text = _macros(text, {'BUTTON'}, change_macro)
    text = _cards(text, ((24, 60, 510, 372), (558, 60, 270, 372)))

    def change_item(name, block):
        values = _background(name, block)
        if name == 'profileList':
            values.update(rect='36 76 486 340', elementwidth='470', elementheight='28',
                          textscale='.30', textaligny='-2', outlinecolor='.94 .75 .27 .20',
                          columns='1 0 470 33')
        return values
    return _items(text, change_item)


def _create_profile(text, initial=False):
    fields = {'"Video Quality:"': 140, '"Resolution:"': 168, '"Brightness:"': 196,
              '"Connection:"': 240, '"Mouse Sensitivity:"': 284,
              '"Invert Mouse:"': 312, '"Handedness:"': 340}

    def change_macro(name, args):
        if name == 'SUBWINDOWBLACK':
            return _rect(args, (0, 0, 852, 432))
        if name == 'LABEL':
            if args[4].startswith('"PunkBuster '):
                _rect(args, (40, 252, 248, 100)); args[5] = '.20'; args[7:9] = ['0', '14']
            else:
                _rect(args, (40, 86, 248, 152)); args[5] = '.22'; args[7:9] = ['0', '15']
            return args
        if name == 'EDITFIELDLEFT':
            return _field(args, (344, 88, 468, 28), '.26', '20')
        if name == 'CVARFLOATLABEL':
            y = 196 if args[4] == '"ui_r_gamma"' else 284
            _rect(args, (344, y, 468, 22)); args[5] = '.22'; args[7:9] = ['468', '15']
            return args
        if name in ('MULTIACTION', 'SLIDER', 'YESNO', 'MULTI') and args[4] in fields:
            # Leave room after each slider for its existing numeric readout.
            width = 418 if name == 'SLIDER' else 468
            return _field(args, (344, fields[args[4]], width, 22), '.22', '15')
        if name in ('BUTTON', 'NAMEDBUTTON'):
            offset = int(name == 'NAMEDBUTTON')
            label = args[4 + offset]
            geometry = {'"DEFAULT SETTINGS"': (344, 382, 468, 30),
                        '"ENABLE PUNKBUSTER"': (40, 374, 248, 30),
                        '"BACK"': (24, 442, 184, 28),
                        '"QUIT"': (24, 442, 184, 28),
                        '"CREATE"': (638, 442, 190, 28)}.get(label)
            if geometry:
                _rect(args, geometry, offset); args[5 + offset:7 + offset] = ['.26', '21']
                return args
        return None

    text = _macros(text, {'SUBWINDOWBLACK', 'LABEL', 'EDITFIELDLEFT', 'MULTIACTION',
                          'SLIDER', 'CVARFLOATLABEL', 'YESNO', 'MULTI', 'BUTTON', 'NAMEDBUTTON'}, change_macro)
    text = _cards(text, ((24, 60, 280, 372), (328, 60, 500, 372)))

    def change_item(name, block):
        values = _background(name, block)
        if name == 'efleftbackPlayer Alias:':
            values['rect'] = '438 88 374 28'
        return values
    return _items(text, change_item)


def _rename_profile(text):
    def change_macro(name, args):
        if name == 'SUBWINDOWBLACK':
            return _rect(args, (0, 0, 852, 432))
        if name == 'EDITFIELDLEFT':
            return _field(args, (168, 204, 516, 32), '.30', '23')
        if name == 'BUTTON':
            return _field(args, (24 if args[4] == '"BACK"' else 638, 442,
                                 184 if args[4] == '"BACK"' else 190, 28), '.27', '20')
        return None
    text = _macros(text, {'SUBWINDOWBLACK', 'EDITFIELDLEFT', 'BUTTON'}, change_macro)
    text = _cards(text, ((144, 152, 564, 136),))

    def change_item(name, block):
        values = _background(name, block)
        if name == 'efleftbackNew Profile Name:':
            values['rect'] = '308 204 376 32'
        return values
    return _items(text, change_item)


def _replays(text):
    geometry = {'"BACK"': (24, 442, 184, 28),
                '"DELETE"': (438, 442, 178, 28),
                '"VIEW"': (638, 442, 190, 28)}

    def change_macro(name, args):
        if args[4] in geometry:
            return _field(args, geometry[args[4]], '.27', '20')
        return None
    text = _macros(text, {'BUTTON'}, change_macro)
    text = _cards(text, ((24, 60, 804, 372),))

    def change_item(name, block):
        values = _background(name, block)
        if name == 'demoList':
            values.update(rect='36 76 780 340', elementwidth='764', elementheight='28',
                          textscale='.28', textaligny='-2', outlinecolor='.94 .75 .27 .20',
                          columns='1 0 764 33')
        return values
    return _items(text, change_item)


def apply(text, relative):
    """Return a redesigned browse/setup menu, leaving every other file alone."""
    name = Path(relative).name
    if Path(relative).parent.name != 'ui' or name not in NAMES:
        return text
    text = _canvas(text)
    if name in ('playonline.menu', 'playonline_sub.menu'):
        return _server(text)
    if name in ('hostgame.menu', 'hostgame_sub.menu'):
        return _host(text)
    if name == 'hostgame_advanced.menu':
        return _advanced(text)
    if name in ('profile.menu', 'profile_sub.menu'):
        return _profiles(text)
    if name in ('profile_create.menu', 'profile_create_initial.menu'):
        return _create_profile(text, name == 'profile_create_initial.menu')
    if name == 'profile_rename.menu':
        return _rename_profile(text)
    if name == 'viewreplay.menu':
        return _replays(text)
    return text
