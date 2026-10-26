"""Shared, bounded DMS actions for the local listen game and TCE2 menus.

Commands are taken from Omni-bot GoalManager/PathPlannerWaypointCmds. No
arbitrary console strings or DMS extension scripts are accepted from HTTP.
"""
import re
import hashlib
from pathlib import Path
import shutil
import time
import uuid

ACTIONS = []
def add(group, key, label, command, modes=('tce2', 'et'), confirm=False):
    ACTIONS.append(dict(group=group, id=key, label=label, command=command,
                        modes=modes, confirm=confirm))

for key, label, command in [
    ('goals_on', 'Goals anzeigen / Show goals', 'draw_goals on'),
    ('goals_off', 'Goals ausblenden / Hide goals', 'draw_goals off'),
    ('goals_list', 'Goals auflisten / List goals', 'show_goals'),
    ('goal_edit', 'Naechstes Goal bearbeiten / Edit nearest', 'goal_edit'),
    ('goal_editx', 'Goal im Fadenkreuz / Edit aimed goal', 'goal_editx'),
    ('goal_move', 'Goal verschieben: Start / Ende', 'goal_move'),
    ('goal_player', 'Goal zum Spieler / Move to player', 'goal_move toplayer'),
    ('goal_finish', 'Bearbeitung beenden / Finish edit', 'goal_finish'),
    ('goal_delete', 'Ausgewaehltes Goal loeschen / Delete', 'goal_delete'),
    ('goal_help', 'Goal-Eigenschaften / Help', 'goal_help'),
    ('goal_save', 'Save Goals', 'goal_save'),
    ('goal_load', 'Gespeicherte Goals laden / Reload goals', 'goal_load'),
    ('routes_on', 'Goal-Routen anzeigen / Show routes', 'draw_goalroutes on'),
    ('routes_off', 'Goal-Routen ausblenden / Hide routes', 'draw_goalroutes off')]:
    add('Goals', key, label, command)

# TCE-compatible tactical types; ET-specific entity goals are not fabricated.
GOAL_TYPES = ('Attack', 'Defend', 'CAMP', 'Snipe', 'GRENADE', 'SMOKEBOMB', 'ROUTE', 'SWITCH')
for kind in GOAL_TYPES:
    add('Neue Goals / New goals', 'create_' + kind, kind, 'goal_create ' + kind)
for key, label, command in [
    ('way_on', 'Wegpunkte anzeigen / Show waypoints', 'waypoint_view on'),
    ('way_off', 'Wegpunkte ausblenden / Hide waypoints', 'waypoint_view off'),
    ('way_add', 'Punkt am Spieler / Add at player', 'waypoint_add'),
    ('way_addx', 'Punkt am Fadenkreuz / Add at aim', 'waypoint_addx'),
    ('way_move', 'Punkt verschieben / Move waypoint', 'waypoint_move'),
    ('way_del', 'Punkt loeschen / Delete waypoint', 'waypoint_del'),
    ('way_connect', 'Einseitig verbinden / Connect', 'waypoint_connect'),
    ('way_biconnect', 'Beidseitig verbinden / Biconnect', 'waypoint_biconnect'),
    ('way_connectx', 'Einseitig per Fadenkreuz / Connect aim', 'waypoint_connectx'),
    ('way_biconnectx', 'Beidseitig per Fadenkreuz / Biconnect aim', 'waypoint_biconnectx'),
    ('way_clearcon', 'Verbindungen entfernen / Disconnect point', 'waypoint_clearcon'),
    ('way_facing', 'Blickrichtung setzen / Set facing', 'waypoint_setfacing'),
    ('way_stats', 'Wegpunktstatistik / Statistics', 'waypoint_stats'),
    ('way_save', 'Save Waypoints', 'waypoint_save'),
    ('way_load', 'Wegpunkte neu laden / Reload waypoints', 'waypoint_load')]:
    add('Wegpunkte / Waypoints', key, label, command)
for flag in ('climb', 'crouch', 'prone', 'jump', 'jumpgap', 'jumplow', 'sneak', 'sprint', 'door', 'axis', 'allies'):
    label = {'axis': 'Terrorists (ET: Axis)', 'allies': 'Specops (ET: Allies)'}.get(flag, flag)
    add('Flags (umschalten / toggle)', 'flag_' + flag, label, 'waypoint_addflag ' + flag)
for radius in (10, 20, 35, 60, 80, 100):
    add('Wegpunkt-Radius', 'radius_' + str(radius), str(radius), 'waypoint_setradius ' + str(radius))
    add('Goal-Radius', 'goalradius_' + str(radius), str(radius), 'goal_setproperty radius ' + str(radius))
for prop, values in [('facing', ('0','1','2','3','clear')), ('stance', ('stand','crouch','prone','random')),
                     ('mincamptime', ('0','5','10','20','30')), ('maxcamptime', ('5','10','20','30','60'))]:
    for value in values:
        add('Goal: '+prop, prop+'_'+value, prop+' '+value, 'goal_setproperty '+prop+' '+value)
# The original OB_DMS is an ET menu, not a runtime implementation. Offer only
# types whose mapgoal scripts exist, with ET equipment goals kept in ET mode.
ET_GOAL_TYPES = ('BUILD', 'PLANT', 'DYNAMITE', 'PLANTMINE', 'TRIPMINE', 'ESCORT',
                 'RIDE', 'MOUNT', 'MOVER', 'AIRSTRIKE', 'CALLARTILLERY',
                 'ARTILLERY_D', 'ARTILLERY_S', 'MOBILEMG42', 'MOUNTMG42',
                 'MOBILEMORTAR', 'CapPoint')
for kind in ET_GOAL_TYPES:
    add('ET: Neue Goals / New goals', 'create_' + kind, kind, 'goal_create ' + kind, ('et',))

for key, label, cmd in [
    ('goal_echo', 'Goal als GM-Text / Echo selected goal', 'goal_echo'),
    ('goal_routes_list', 'Routen auflisten / List routes', 'show_goalroutes'),
    ('goal_priority_list', 'Prioritaeten auflisten / List priorities', 'show_goals .* p'),
    ('goal_player_air', 'Goal zum Spieler ohne Boden / Player without grounding', 'goal_move toplayer false'),
    ('way_delx', 'Punkt am Fadenkreuz loeschen / Delete aimed point', 'waypoint_delx'),
    ('way_info', 'Punktinformation / Waypoint information', 'waypoint_info'),
    ('way_props', 'Punkteigenschaften / Waypoint properties', 'waypoint_showproperty'),
    ('way_names', 'Punktnamen auflisten / List waypoint names', 'waypoint_shownames'),
    ('way_facing_on', 'Blickrichtungen anzeigen / Show facing', 'waypoint_viewfacing on'),
    ('way_facing_off', 'Blickrichtungen ausblenden / Hide facing', 'waypoint_viewfacing off'),
    ('way_clearflags', 'Flags der Auswahl leeren / Clear selected flags', 'waypoint_clearallflags'),
    ('way_boxselect', 'Punkte mit Box auswaehlen / Box select', 'waypoint_boxselect'),
    ('way_boxroom', 'Raumpunkte auswaehlen / Select room', 'waypoint_boxselectroom'),
    ('way_lockselected', 'Auswahl vor Verschieben schuetzen / Lock selection', 'waypoint_lockselected'),
    ('way_unlock', 'Alle Punkte entsperren / Unlock all', 'waypoint_unlockall'),
    ('way_split', 'Verbindung am Spieler teilen / Split connection', 'waypoint_split'),
    ('way_unsplit', 'Zweierverbindung zusammenfassen / Unsplit', 'waypoint_unsplit'),
    ('way_autofeature', 'Spawns und Teleporter hinzufuegen / Auto features', 'waypoint_autofeature')]:
    add('Goals' if key.startswith('goal_') else 'Wegpunkte / Waypoints', key, label, cmd,
        confirm=key=='way_clearflags')
add('Goals', 'goal_removeall', 'Alle manuell erstellten Goals entfernen / Remove authored goals',
    'goal_removeall', confirm=True)

for flag in ('closed', 'elevator', 'teleport', 'strafe_l', 'strafe_r', 'inwater',
             'underwater', 'blockwall', 'blockbridge', 'blockwater', 'disguise', 'infiltrator',
             'team1','team2','team3','team4','teamonly','snipe','health','armor','ammo',
             'dynamic','defend','attack','script','route','mobilemg42','mobilemortar','plantmine',
             'cappoint','callartillery','artillery_s','artillery_d','flame','panzer','ugoal','usepath'):
    add('Flags (umschalten / toggle)', 'flag_' + flag, flag, 'waypoint_addflag ' + flag)
for radius in (50, 75, 90, 125, 150, 200, 300):
    add('Wegpunkt-Radius', 'radius_' + str(radius), str(radius), 'waypoint_setradius ' + str(radius))
for radius in (35, 60, 80, 100):
    add('Neue Wegpunkte: Standardradius', 'defradius_' + str(radius), str(radius),
        'waypoint_setdefaultradius ' + str(radius))
for radius in (50, 75, 125, 150, 175, 200, 250, 300):
    add('Goal-Radius', 'goalradius_' + str(radius), str(radius), 'goal_setproperty radius ' + str(radius))

ROLES = ('ATTACKER', 'ATTACKER1', 'ATTACKER2', 'ATTACKER3', 'DEFENDER', 'DEFENDER1',
         'DEFENDER2', 'DEFENDER3', 'AMBUSHER', 'ROAMER', 'SNIPER', 'INFILTRATOR',
         'TEAMCAPTAIN', 'OFFENSECAPTAIN', 'DEFENSECAPTAIN', 'FIRETEAMCAPTAIN', 'ESCORT')
# Native SetProperty calls the active goal's schema. Values are not silently
# invented when a property does not apply; Goal Help lists that goal's contract.
PROPERTY_SPECS = {
    'name': dict(label='Name', kind='name'),
    'group': dict(label='Gruppe / Group (leer entfernt / empty clears)', kind='name', allow_empty=True),
    'radius': dict(label='Radius', kind='number', minimum=0, maximum=999),
    'priority': dict(label='Standardprioritaet / Priority', kind='number', minimum=0, maximum=1),
    'available': dict(label='Team-Verfuegbarkeit / Available teams', kind='enum', values=('AXIS','ALLIES','AXIS|ALLIES',''),
                      labels={'AXIS':'Terrorists / Axis','ALLIES':'Specops / Allies','AXIS|ALLIES':'Beide Teams / Both teams','':'Keine Teams / No teams'}),
    'range': dict(label='Reichweite / Range', kind='number', minimum=0, maximum=9999),
    'randomusepoint': dict(label='Zufaelliger Use-Punkt / Random use point', kind='enum', values=('0','1')),
    'mincamptime': dict(label='Minimale Campzeit (s)', kind='number', minimum=0, maximum=9999),
    'maxcamptime': dict(label='Maximale Campzeit (s)', kind='number', minimum=0, maximum=9999),
    'stance': dict(label='Haltung / Stance', kind='enum', values=('stand','crouch','prone','peek'), et_values=('stand','crouch','prone','random','jump','peek')),
    'timeout': dict(label='Timeout (ms)', kind='integer', minimum=0, maximum=5000),
    'use': dict(label='Use-Taste / Use button', kind='enum', values=('0','1')),
    'facing': dict(label='Blickrichtung / Facing', kind='enum', values=('0','1','2','3','4','5','99','clear')),
    'aim': dict(label='Zielrichtung / Aim', kind='enum', values=('', 'clear'), et_values=('', 'clear','opposite','position','random','velocity'), labels={'':'Fadenkreuz erfassen / Capture aim'}),
    'position': dict(label='Position', kind='enum', values=('<position>','<aimpoint>')),
    'roles': dict(label='Rolle / Role', kind='enum', values=(*ROLES,''), labels={'':'Alle Rollen / All roles'}),
    'button': dict(label='Aktionstaste / Action button', kind='enum', values=('USE','JUMP','CROUCH','clear'), modes=('et',)),
    'prime': dict(label='Granate vorhalten (s) / Prime', kind='enum', values=('0','1','2','3')),
    'count': dict(label='Anzahl / Count', kind='integer', minimum=0, maximum=9),
    'trace': dict(label='Zielobjekt erfassen / Trace entity', kind='enum', values=('', 'clear'), labels={'':'Fadenkreuz-Objekt / Aimed entity'}),
    'targetgoal': dict(label='Ziel-Goal / Target goal', kind='name'),
    'vehicle': dict(label='Fahrzeug-Goal / Vehicle goal', kind='name', modes=('et',)),
    'offset': dict(label='Offset', kind='enum', values=('99','clear'), modes=('et',)),
    'board': dict(label='Einsteigen / Board', kind='enum', values=('99','clear'), modes=('et',)),
    'seat': dict(label='Sitz / Seat', kind='enum', values=('99','clear'), modes=('et',)),
    'coverspot': dict(label='Deckungspunkt / Cover spot', kind='enum', values=('99','clear'), modes=('et',)),
    'coverfacing': dict(label='Deckungsrichtung / Cover facing', kind='enum', values=('99','clear'), modes=('et',)),
    'coverstance': dict(label='Deckungshaltung / Cover stance', kind='enum', values=('stand','crouch','prone'), modes=('et',)),
    'etype': dict(label='Sprengstoff / Explosive type', kind='enum', values=('both','dyno','satchel'), modes=('et',)),
    'detonatespot': dict(label='Sprengpunkt / Detonate spot', kind='enum', values=('0',), modes=('et',)),
    'weapon': dict(label='Waffe / Weapon', kind='enum', values=('PANZERFAUST','BAZOOKA','FLAMETHROWER','clear'), modes=('et',)),
}
for prop in ('stance','timeout','use','aim','roles','button','prime','count','trace','available','randomusepoint'):
    spec=PROPERTY_SPECS[prop]
    values=spec.get('et_values',spec.get('values', (0,500,1000,1500,3000,5000) if prop=='timeout' else (0,1,2,3)))
    for value in values:
        key=prop+'_'+str(value or 'here')
        if not any(a['id']==key for a in ACTIONS):
            modes=('et',) if value not in spec.get('values', values) else spec.get('modes',('tce2','et'))
            add('Goal: '+prop, key, prop+' '+spec.get('labels',{}).get(value,str(value)),
                ('goal_setproperty '+prop+' '+str(value)).rstrip(),modes)
# The former limited menu offered two ET building stances in TCE camp menus.
for a in ACTIONS:
    if a['id'] in ('stance_random','stance_jump'): a['modes']=('et',)
for prop in ('mincamptime','maxcamptime'):
    for value in (0,1,2,3,4,6,7,8,9,12,15,25,35,40,45,50,60,999):
        if not any(a['id']==prop+'_'+str(value) for a in ACTIONS):
            add('Goal: '+prop, prop+'_'+str(value), prop+' '+str(value), 'goal_setproperty '+prop+' '+str(value))
for value in ('4','5','99'):
    add('Goal: facing', 'facing_'+value, 'facing '+value, 'goal_setproperty facing '+value)
for prop,spec in PROPERTY_SPECS.items():
    if spec.get('modes')==('et',) and spec['kind']=='enum':
        for value in spec['values']:
            if any(a['id']==prop+'_'+value for a in ACTIONS): continue
            add('ET Goal: '+prop, prop+'_'+value, prop+' '+value,
                'goal_setproperty '+prop+' '+value, ('et',))

for key,label,cmd in [
    ('debugtriggers','Trigger in Konsole / Debug triggers','debugtriggers'),
    ('drawtriggers_on','Triggerboxen anzeigen / Draw triggers','drawtriggers 1'),
    ('drawtriggers_off','Triggerboxen ausblenden / Hide triggers','drawtriggers 0'),
    ('trigger_aimpos','Triggerposition am Fadenkreuz / Trigger aim','trigger_aimpos'),
    ('tracebox','Tracebox Anfang/Ende / Trace box','tracebox'),
    ('dbg_mapscripts','Mapscripts verfolgen / Debug map scripts','dbg_mapscripts'),
    ('dbg_goals','Goal-Verfuegbarkeit / Goal availability','dbg_goalavailability'),
    ('dbg_scriptgoals','Skriptgoals verfolgen / Debug script goals','dbg_scriptgoals'),
    ('debug_fpinfo','Bot-Pfadinformationen / Bot path info','debugbot all fpinfo'),
    ('draw_paththrough_on','Sonderpfade anzeigen / Draw paththrough','draw_paththrough 1'),
    ('draw_paththrough_off','Sonderpfade ausblenden / Hide paththrough','draw_paththrough 0'),
    ('draw_blocktests','Blockadentests anzeigen / Draw block tests','drawblocktests'),
    ('draw_spawns','Spawns anzeigen / Draw spawns','draw_spawns'),
    ('draw_oids','Objective-Objekte anzeigen / Draw objective info','draw_oids'),
    ('showentityinfo','Objektinformation am Fadenkreuz / Entity info','showentityinfo'),
    ('sag','Aktive Goals / Active goals','sag 0 0 1'),
    ('mypos','Spielerposition / Player position','mypos'),
    ('aim_pos','Fadenkreuzposition / Aim position','aim_pos'),
    ('saveplayerpos','Spielerposition als Text / Save player position','saveplayerpos'),
    ('saveusepoint','Use-Punkt als Text / Save use point','saveusepoint'),
    ('dist_on','Entfernung anzeigen / Show distance','dist'),
    ('dist_off','Entfernung ausblenden / Hide distance','dist off'),
    ('ip_on','Spieler ignorieren / Ignore player','ip on'),
    ('ip_off','Spieler beachten / Include player','ip off'),
    ('dontshoot_on','Schusswaffen pausieren / Stop shooting','dontshoot on'),
    ('dontshoot_off','Schusswaffen aktivieren / Enable shooting','dontshoot off'),
    ('campstart','Ersten Camp-Punkt besuchen / First camp','campstart'),
    ('campnext','Naechsten Camp-Punkt besuchen / Next camp','campnext'),
    ('stuckstart','Erste Festhaengstelle besuchen / First stuck','stuckstart'),
    ('stucknext','Naechste Festhaengstelle besuchen / Next stuck','stucknext'),
    ('makemapgm','Map-GM Vorlage erstellen / Make map GM','makemapgm'),
    ('makemapgoallist','Map-Goal-Liste erstellen / Goal list','makemapgoallist')]:
    add('Diagnose / Debug',key,label,cmd,confirm=key in ('makemapgm','makemapgoallist'))
for number in range(1,11):
    add('Triggerboxen / Trigger boxes','trigger_'+str(number), 'Trigger '+str(number)+' Anfang/Ende', 'trigger_bounds Trigger'+str(number))
for key,label,cmd,modes in [
    ('mywpmode','Autosicherung alle 60 s / Autosave every 60 s','mywpmode 60',('tce2','et')),
    ('fix_waterwps','Wasserflags neu erfassen / Refresh water flags','fix_waterwps',('tce2','et')),
    ('sgn','Goal-Namen im Umkreis / Names within 300','sgn 300',('tce2','et')),
    ('warpbot','Beobachteten Bot versetzen / Warp selected bot','warpbot',('tce2','et')),
    ('plantstart','Erstes Sprengziel / First plant goal','plantstart',('et',)),
    ('plantnext','Naechstes Sprengziel / Next plant goal','plantnext',('et',)),
    ('tracearty','Artillerie-Ziel testen / Artillery trace','tracearty',('et',))]:
    add('Diagnose / Debug',key,label,cmd,modes,confirm=key=='fix_waterwps')
for kind in GOAL_TYPES+ET_GOAL_TYPES:
    modes=('tce2','et') if kind in GOAL_TYPES else ('et',)
    for state in ('on','off'):
        add('Goal-Anzeige / Goal display','draw_'+kind+'_'+state, kind+' '+state,
            'draw_goals '+state+' '+kind.upper()+'.*', modes)
for kind in ('FLAG','CHECKPOINT','HEALTHCAB','AMMOCAB','REPAIR','MOBILEMG','MOUNTMG','MOUNT_','PLANT_','ARTILLERY'):
    for state in ('on','off'):
        add('ET Goal-Anzeige','draw_'+kind+'_'+state,kind+' '+state,'draw_goals '+state+' '+kind+'.*',('et',))
for variant,label in (('','Standard'),('0','Modus 0'),('1','Modus 1'),('2','Modus 2')):
    add('Diagnose / Debug','sag_'+(variant or 'default'),'Aktive Goals / Active goals '+label,
        ('sag '+variant).rstrip())
for team,label in ((1,'Terrorists'),(2,'Specops')):
    add('Testbots / Test bots','addbot_'+str(team),label,'addbot '+str(team))
    for cls,role in ((1,'Assault'),(3,'Recon'),(5,'Sniper')):
        add('Testbots / Test bots','addbot_'+str(team)+'_'+str(cls),label+' / '+role,
            'addbot '+str(team)+' '+str(cls),('tce2',))
        add('Testbots / Test bots','testbot_'+str(team)+'_'+str(cls),'Testbot '+label+' / '+role,
            'testbot '+str(team)+' '+str(cls),('tce2',))
    for cls,role in ((1,'Soldier'),(2,'Medic'),(3,'Engineer'),(4,'Fieldops'),(5,'Covertops')):
        add('ET Testbots','et_addbot_'+str(team)+'_'+str(cls),('Axis' if team==1 else 'Allies')+' / '+role,
            'addbot '+str(team)+' '+str(cls),('et',))
        add('ET Testbots','et_testbot_'+str(team)+'_'+str(cls),'Testbot '+('Axis' if team==1 else 'Allies')+' / '+role,
            'testbot '+str(team)+' '+str(cls),('et',))
add('Testbots / Test bots','addbot','Zufaelliger Bot / Random bot','addbot')
add('Testbots / Test bots','kickall','Alle Bots entfernen / Remove bots','kickall',confirm=True)

# Keep each section contiguous in the web UI as well as the generated menu.
_group_order = list(dict.fromkeys(a['group'] for a in ACTIONS))
ACTIONS.sort(key=lambda a: _group_order.index(a['group']))

def catalog(mode='tce2'):
    properties={p:dict(s) for p,s in PROPERTY_SPECS.items() if mode in s.get('modes',('tce2','et'))}
    if mode=='et':
        for spec in properties.values():
            if 'et_values' in spec: spec['values']=spec['et_values']
    return dict(actions=[a for a in ACTIONS if mode in a['modes']],
                types=GOAL_TYPES+(ET_GOAL_TYPES if mode=='et' else ()),
                properties=properties,
                note='Goal Help zeigt die Eigenschaften des ausgewaehlten Goal-Typs. ET-Ausrüstungstypen gelten nur im ET-Modus.')

def command(payload, mode='tce2'):
    key = payload.get('action')
    if key == 'goal_create_named':
        kind, name = payload.get('type'), payload.get('name', '')
        if kind not in catalog(mode)['types'] or not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9_]{1,64}', name):
            raise ValueError('Goal-Typ oder Name ungueltig (Buchstaben, Ziffern, Unterstrich).')
        return 'bot goal_create ' + kind + ' ' + name
    if key == 'goal_property':
        prop, value = payload.get('property'), payload.get('value')
        spec=catalog(mode)['properties'].get(prop)
        if spec is None:
            raise ValueError('Unbekannte Goal-Eigenschaft.')
        if not isinstance(value, str): raise ValueError('Textwert erwartet.')
        valid = (value=='' and spec.get('allow_empty') or re.fullmatch(r'[A-Za-z0-9_]{1,64}', value)) if spec['kind']=='name' else (
            value in spec['values'] if spec['kind']=='enum' else
            bool(re.fullmatch(r'\d{1,4}(?:\.\d{1,2})?' if spec['kind']=='number' else r'\d{1,4}',value)) and spec['minimum']<=float(value)<=spec['maximum'])
        if not valid: raise ValueError('Ungueltiger Eigenschaftswert.')
        return ('bot goal_setproperty ' + ('tagname' if prop == 'name' else prop) + ' ' + value).rstrip()
    if key=='waypoint_name':
        value=payload.get('name','')
        if not isinstance(value,str) or not re.fullmatch(r'[A-Za-z0-9_ .-]{0,64}',value):
            raise ValueError('Ungueltiger Wegpunktname.')
        return 'bot waypoint_setname'+(' '+value if value else '')
    for item in ACTIONS:
        if item['id'] == key and mode in item['modes']: return 'bot ' + item['command']
    raise ValueError('Unbekannte Omni-bot-Aktion.')

def backup_file(path, root):
    if not path.is_file(): return None
    destination=root/'.navstudio-backups';destination.mkdir(parents=True,exist_ok=True)
    backup=destination/(path.name+'.'+str(time.time_ns())+'.bak')
    with backup.open('xb') as stream: stream.write(path.read_bytes())
    return backup


def file_stamp(path):
    if not path.is_file(): return None
    state=path.stat()
    return state.st_mtime_ns,state.st_size,hashlib.sha256(path.read_bytes()).hexdigest()


def copy_saved(source, destination, expected):
    """Copy only the just-confirmed serializer bytes, with exclusive temp name."""
    if source.resolve()==destination.resolve(): return
    destination.parent.mkdir(parents=True,exist_ok=True)
    temporary=destination.with_name('.live-save-'+uuid.uuid4().hex+'.tmp')
    try:
        data=source.read_bytes()
        if hashlib.sha256(data).hexdigest()!=expected: raise RuntimeError('Live-Datei wurde während der Übernahme verändert.')
        with temporary.open('xb') as stream: stream.write(data)
        if hashlib.sha256(source.read_bytes()).hexdigest()!=expected: raise RuntimeError('Live-Datei wurde während der Übernahme verändert.')
        temporary.replace(destination)
        if hashlib.sha256(destination.read_bytes()).hexdigest()!=expected: raise RuntimeError('Projektkopie stimmt nicht mit der Live-Speicherung überein.')
    finally:
        if temporary.exists(): temporary.unlink()


def execute(studio, payload):
    cmd = command(payload, studio.game_mode)
    with studio.lock:
        if studio.busy: raise RuntimeError('Zuerst laufenden Auftrag abschliessen.')
        if payload.get('game_mode') != studio.game_mode: raise RuntimeError('Spielmodus geaendert; neu oeffnen.')
        name = studio.require_map(payload.get('map'))
        studio.own_test_process(required=True)
        studio.require_loaded(name)
        gametype=studio.live_gametype() if studio.game_mode=='tce2' else None
        if studio.game_mode=='tce2' and payload.get('gametype') is not None and payload['gametype']!=gametype:
            raise RuntimeError('Gametype hat sich geaendert; Live-Editor neu oeffnen.')
        bundles = studio.nav_bundles(gametype)
        runtime = studio.runtime_nav_bundles(gametype) if hasattr(studio,'runtime_nav_bundles') else bundles
        save_kind = {'goal_save': 'goals', 'way_save': 'way'}.get(payload.get('action'))
        path = bundles.path(name, save_kind) if save_kind else None
        runtime_path=runtime.path(name,save_kind) if save_kind else None
        if save_kind=='way' and getattr(studio,'active_project',None):
            draft=studio.drafts.get(name)
            if draft and draft.get('saved_revision')!=draft.get('revision'):
                raise RuntimeError('Ungespeicherte Studio-Änderungen vorhanden. Projekt zuerst sichern; danach können die Live-Wegpunkte übernommen werden.')
        backup=backup_file(path,bundles.root) if path else None
        runtime_backup=backup_file(runtime_path,runtime.root) if runtime_path and runtime_path.resolve()!=path.resolve() else backup
        before=file_stamp(runtime_path) if runtime_path else None
        # A prior legacy pair may be the currently loaded map logic. Capture its
        # script reference before goal_save creates a new gametype goals file.
        companion=None
        if save_kind=='goals' and not bundles.path(name,'script').exists():
            candidate=runtime.path(name,'script')
            if candidate.exists(): companion=candidate
            elif studio.game_mode=='tce2' and not runtime.path(name,'goals').exists():
                from navbundle import NavBundles
                legacy=NavBundles(runtime.root)
                if legacy.path(name,'script').is_file() and legacy.path(name,'goals').is_file(): companion=legacy.path(name,'script')
        # One send only; an ambiguous transport failure must not create twice.
        reply = studio.rcon(cmd)
        result = dict(command=cmd, reply=reply, map=name, backup=str(backup) if backup else None,
                      runtime_backup=str(runtime_backup) if runtime_backup else None)
        if path:
            plain=re.sub(r'\^[0-9]','',reply)
            failed = re.search(r'(\d+) Goals could not save', reply)
            if failed and int(failed[1]):
                raise RuntimeError('Omni-bot konnte nicht alle Goals speichern. ' + reply)
            expected_reply = r'\b\d+ Goals Saved Successfully,\s*0 Goals could not save\b' if save_kind=='goals' else r'\bSaved Waypoints\.'
            if not re.search(expected_reply,plain):
                raise RuntimeError('Speicherung vom Spiel nicht bestätigt. '+reply)
            after=file_stamp(runtime_path)
            if not after or (before and after[0]==before[0]):
                raise RuntimeError('Speicherung nicht bestaetigt: Zieldatei nicht neu geschrieben. ' + reply)
            if save_kind=='way':
                import tce_navgen
                tce_navgen.read_way(runtime_path)
            elif not re.search(rb'global\s+_MG\s*=',runtime_path.read_bytes()):
                raise RuntimeError('Neue Goal-Datei enthält keine Omni-bot _MG-Tabelle; Projektstand bleibt unverändert.')
            copy_saved(runtime_path,path,after[2])
            if companion:
                script_path=bundles.path(name,'script')
                copy_saved(companion,script_path,hashlib.sha256(companion.read_bytes()).hexdigest())
                result['script_imported']=str(script_path)
            if save_kind=='way' and getattr(studio,'active_project',None):
                studio.live_way_saved(name)
                result['draft']=studio.drafts.view(name,studio.draft_environment(name))
                result['native_checked']=False
                result['note']='Live-Wegnetz ins Projekt übernommen. Alte Prüfung ist ungültig; vor erneutem Übernehmen die aktuelle Navigation prüfen.'
            result['saved'] = str(path)
            result['runtime_saved']=str(runtime_path)
            result['sha256']=after[2]
            result['bundle'] = bundles.read(name)
        return result
