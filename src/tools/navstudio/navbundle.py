"""Lossless, non-executing access to a map's Omni-bot navigation companions."""
import hashlib
import io
import json
import math
from pathlib import Path
import re
import time
import uuid
import zipfile

SUFFIXES = {'way': '.way', 'script': '.gm', 'goals': '_goals.gm'}
LIMIT = 1024 * 1024

# mapgoal_switch.gm v1 Schema/SaveToTable: these native defaults are omitted
# by goal_save. Keep them separate from literal saved properties so previews
# can show the same values without changing GM bytes or inventing live state.
SWITCH_SCHEMA_DEFAULTS = {'Stance': 'stand', 'Timeout': 1500, 'Use': 1}


def goal_overlay(text):
    """Read serializer literals only. Never run GM or infer live availability.

    The native serializer emits tables, strings, numbers and Vec3 constructors.
    A hand-written expression is reported as unsupported, rather than evaluated
    or silently drawn at an invented position. The original GM stays untouched.
    """
    pattern = re.compile(r'\s+|//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|'
                         r'-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?|'
                         r'[A-Za-z_][A-Za-z_0-9]*|[{}\[\](),;=]')
    tokens, end = [], 0
    for match in pattern.finditer(text):
        if text[end:match.start()].strip():
            return dict(goals=[], error='Nichtliteraler GM-Inhalt / Non-literal GM content.', source='saved_goals')
        token = match.group(); end = match.end()
        if not token.isspace() and not token.startswith(('//', '/*')): tokens.append(token)
    if text[end:].strip() or len(tokens) > 100000:
        return dict(goals=[], error='GM-Vorschau überschreitet den Literalvertrag.', source='saved_goals')
    cursor = 0
    def take(expected=None):
        nonlocal cursor
        if cursor >= len(tokens): raise ValueError('Unvollständige GM-Tabelle.')
        token=tokens[cursor]; cursor+=1
        if expected is not None and token != expected: raise ValueError('Nichtliteraler GM-Inhalt: '+token)
        return token
    def value(depth=0):
        nonlocal cursor
        if depth>16: raise ValueError('GM-Tabelle zu tief verschachtelt.')
        token=take()
        if token=='{':
            result={};array_index=0
            while cursor<len(tokens) and tokens[cursor]!='}':
                if tokens[cursor]=='[':
                    take('[');key=value(depth+1);take(']');take('=')
                elif cursor+1<len(tokens) and tokens[cursor+1]=='=':
                    key=take();take('=')
                    if key.startswith('"'):key=json.loads(key)
                else:
                    # gmCodeGenPrivate::GenExprTable assigns zero-based indices
                    # only to unkeyed fields; explicit keys do not advance it.
                    key=array_index;array_index+=1
                if not isinstance(key,(str,int,float)):raise ValueError('Nichtliteraler Tabellenschlüssel.')
                result[str(key)]=value(depth+1)
                if cursor<len(tokens) and tokens[cursor] in (',',';'):take()
                elif cursor<len(tokens) and tokens[cursor]!='}':raise ValueError('GM-Tabellentrenner fehlt.')
            take('}');return result
        if token=='Vec3':
            take('(');vector=[value(depth+1)];take(',');vector.append(value(depth+1));take(',');vector.append(value(depth+1));take(')')
            if any(type(v) not in (int,float) or not math.isfinite(v) for v in vector):raise ValueError('Ungültiger Goal-Vektor.')
            return vector
        if token.startswith('"'):return json.loads(token)
        if token in ('null','true','false'):return {'null':None,'true':True,'false':False}[token]
        if re.fullmatch(r'-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?',token):
            number=float(token)
            if not math.isfinite(number):raise ValueError('Ungültiger Goal-Zahlenwert.')
            return int(number) if number.is_integer() else number
        raise ValueError('GM-Ausdruck wird nicht ausgeführt: '+token)
    try:
        take('global');take('_MG');take('=');table=value()
        if cursor<len(tokens) and tokens[cursor]==';':take()
        if cursor!=len(tokens) or not isinstance(table,dict):raise ValueError('Zusätzlicher GM-Code wird nicht ausgeführt.')
        goals=[];skipped=[]
        for name,properties in table.items():
            if not isinstance(properties,dict):continue
            pos=properties.get('Position');kind=properties.get('GoalType')
            goal_name=properties.get('Name')
            if goal_name is None:goal_name=name
            if not isinstance(goal_name,str) or not goal_name:
                skipped.append(name);continue
            if not isinstance(kind,str) or not kind:
                # GoalManager::Load uses the saved goal name when SaveToTable
                # omits GoalType. Keep the literal properties/GM bytes intact.
                split=goal_name.find('_')
                if split==9 and len(goal_name)>11 and goal_name[11]=='_' and goal_name.startswith('ARTILLERY'):
                    split+=2
                kind=goal_name[:split] if split>=0 else None
            if not isinstance(kind,str) or not kind or not isinstance(pos,list) or len(pos)!=3:
                skipped.append(name);continue
            defaults = {key: item for key, item in SWITCH_SCHEMA_DEFAULTS.items()
                        if key not in properties} if kind.upper() == 'SWITCH' else {}
            goals.append(dict(id=name,name=goal_name,type=kind,position=pos,properties=properties,
                              effective_properties=dict(defaults, **properties), schema_defaults=defaults,
                              schema_default_source='mapgoal_switch.gm · SWITCH v1' if defaults else None,
                              radius=properties.get('Radius',0),source='saved_goals',live=False))
        return dict(goals=goals, skipped=skipped, source='saved_goals',
                    note='Gespeicherte GM-Eigenschaften. Active/Triggerzustand und dynamische Positionen sind nur im laufenden Spiel belegt.')
    except (ValueError,TypeError,json.JSONDecodeError) as error:
        return dict(goals=[], error=str(error), source='saved_goals')

class NavBundles:
    @staticmethod
    def archive_project(name, way_bytes, goal_files, map_scripts, metadata):
        """Package supplied, already-validated project bytes without disk access.

        Mission/native/live receipts remain the caller's responsibility. This
        method validates names and complete companion pairs, preserves every
        payload byte and binds all archive contents to a SHA256 manifest.
        """
        if (not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', name)
                or re.fullmatch(r'(?:con|prn|aux|nul|com[1-9]|lpt[1-9])', name, re.IGNORECASE)):
            raise ValueError('Invalid map filename.')
        if not isinstance(metadata, dict):
            raise ValueError('Project metadata must be a JSON object.')
        try:
            metadata = json.loads(json.dumps(metadata, allow_nan=False))
        except (TypeError, ValueError) as error:
            raise ValueError('Project metadata must contain finite JSON values.') from error
        if metadata.get('map', name) != name:
            raise ValueError('Project metadata names another map.')
        mode = metadata.get('mod', metadata.get('game_mode', 'tce2'))
        if mode not in ('tce2', 'et'):
            raise ValueError('Project mod must be tce2 or et.')
        if not isinstance(way_bytes, bytes) or not 0 < len(way_bytes) <= 16 * LIMIT:
            raise ValueError('A non-empty WAY file up to 16 MiB is required.')
        if not isinstance(goal_files, dict) or not isinstance(map_scripts, dict):
            raise ValueError('Complete goal/script companion pairs are required.')
        if not goal_files and not (mode == 'tce2' and metadata.get('gametypes') == [7] and
                                   set(map_scripts) == {name + '_gt7.script'}):
            raise ValueError('Only an explicit GT7-only spawn package may omit goal companions.')
        files = {'nav/' + name + '.way': way_bytes}
        stems, scripts, aliases = {}, set(), set()
        for folder, supplied in (('nav', goal_files), ('mapscripts', map_scripts)):
            for basename, data in supplied.items():
                if not isinstance(basename, str) or not isinstance(data, bytes) or not data.strip() or len(data) > LIMIT:
                    raise ValueError('Package scripts need safe basenames and non-empty bytes up to 1 MiB.')
                if folder == 'nav':
                    pattern = re.escape(name) + (r'_gt([257])' if mode == 'tce2' else '') + r'(_goals)?\.gm'
                else:
                    pattern = re.escape(name) + (r'_gt([257])' if mode == 'tce2' else '') + r'\.script'
                match = re.fullmatch(pattern, basename, re.IGNORECASE)
                if not match or basename.casefold() in aliases:
                    raise ValueError('Unsafe, duplicate or mismatched package filename: ' + basename)
                aliases.add(basename.casefold())
                gt = int(match.group(1)) if mode == 'tce2' else 2
                if folder == 'nav':
                    kind = 'goals' if basename.lower().endswith('_goals.gm') else 'script'
                    stems.setdefault(gt, set()).add(kind)
                else:
                    scripts.add(gt)
                files[folder + '/' + basename] = data
        if any(kinds != {'script', 'goals'} for kinds in stems.values()):
            raise ValueError('Every gametype requires both mission GM and goals GM.')
        if mode == 'tce2' and (not set(stems).issubset(scripts) or scripts - set(stems) - {7}):
            raise ValueError('Every goal pair requires its matching gametype mapscript; only GT7 spawn scripts may stand alone.')
        mode_note = ('TCE2: select the packaged gametype with +set g_gametype N and +set sv_official 0 before devmap ' + name + '.\n'
                     'GT7 custom DM spawns: the mapscript tce_dmspawns 1 switch enables them; tce_dmspawns 0 uses standard team spawns.\n'
                     'Use the switch already present in the packaged script; do not add a second conflicting block.\n') if mode == 'tce2' else (
                     'ET: use the original Omni-bot mod with fs_game omnibot and Objective GT2.\n')
        readme = ('TCE2 NavStudio · ' + name + '\n\n'
                  'Copy nav/* into your Omni-bot et/nav directory.\n'
                  'Copy mapscripts/* into the active game mod mapscripts directory.\n'
                  'Set g_mapScriptDirectory "mapscripts" before loading the map.\n'
                  + mode_note + 'WAY/GM/mapscript bytes are preserved. Manifest hashes detect changes; export does not certify live bot behavior.\n')
        files['README.txt'] = readme.encode('utf-8')
        manifest = dict(schema=1, format='tce2-navstudio-project-package', map=name, mod=mode,
                        gametypes=sorted(set(stems) | scripts), goal_gametypes=sorted(stems), mapscript_gametypes=sorted(scripts), metadata=metadata,
                        files={path: dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
                               for path, data in sorted(files.items())})
        encoded = json.dumps(manifest, ensure_ascii=False, allow_nan=False, sort_keys=True, indent=2).encode('utf-8') + b'\n'
        if len(encoded) > LIMIT:
            raise ValueError('Project metadata exceeds the 1 MiB package limit.')
        files['manifest.json'] = encoded
        result = io.BytesIO()
        with zipfile.ZipFile(result, 'w', zipfile.ZIP_DEFLATED) as archive:
            for path, data in sorted(files.items()):
                entry = zipfile.ZipInfo(path, (1980, 1, 1, 0, 0, 0))
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, data)
        return result.getvalue()

    @staticmethod
    def verify_project_archive(data):
        """Verify a package in memory, without extraction or executing scripts."""
        try:
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                entries = archive.infolist()
                names = [entry.filename for entry in entries]
                if len(names) != len(set(path.casefold() for path in names)) or len(names) > 32:
                    raise ValueError('Duplicate or excessive package entries.')
                if sum(entry.file_size for entry in entries) > 32 * LIMIT:
                    raise ValueError('Project package exceeds its size limit.')
                for entry in entries:
                    path = entry.filename
                    if path not in ('manifest.json', 'README.txt') and not re.fullmatch(r'(?:nav|mapscripts)/[A-Za-z0-9_-]+\.(?:way|gm|script)', path):
                        raise ValueError('Unsafe package entry.')
                    if entry.file_size > (16 * LIMIT if path.endswith('.way') else LIMIT):
                        raise ValueError('Package entry exceeds its size limit.')
                    if entry.is_dir() or ((entry.external_attr >> 16) & 0o170000) not in (0, 0o100000):
                        raise ValueError('Package contains a non-file entry.')
                manifest = json.loads(archive.read('manifest.json'))
                if not isinstance(manifest, dict) or manifest.get('schema') != 1 or manifest.get('format') != 'tce2-navstudio-project-package':
                    raise ValueError('Unknown project package format.')
                expected = manifest['files']
                if set(names) != set(expected) | {'manifest.json'}:
                    raise ValueError('Package manifest does not match archive entries.')
                payload = {path: archive.read(path) for path in expected}
                for path, raw in payload.items():
                    if expected[path] != dict(bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()):
                        raise ValueError('Package content changed: ' + path)
                name = manifest['map']
                rebuilt = NavBundles.archive_project(name, payload['nav/' + name + '.way'],
                    {p[4:]: raw for p, raw in payload.items() if p.startswith('nav/') and p.endswith('.gm')},
                    {p[11:]: raw for p, raw in payload.items() if p.startswith('mapscripts/')}, manifest['metadata'])
                with zipfile.ZipFile(io.BytesIO(rebuilt)) as canonical:
                    if json.loads(canonical.read('manifest.json')) != manifest:
                        raise ValueError('Project package metadata or completeness changed.')
                return manifest
        except (OSError, zipfile.BadZipFile, KeyError, TypeError, UnicodeDecodeError, json.JSONDecodeError) as error:
            raise ValueError('Invalid project package: ' + str(error)) from error

    def __init__(self, root, gametype=None):
        self.root = Path(root).resolve()
        if gametype is not None and str(gametype) not in ('2', '5', '7'):
            raise ValueError('TCE2 gametype must be 2, 5 or 7.')
        self.gametype = int(gametype) if gametype is not None else None

    def path(self, name, kind):
        if not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', name):
            raise ValueError('Invalid map filename.')
        if kind not in SUFFIXES:
            raise ValueError('Unknown navigation file type.')
        stem = name + ('_gt' + str(self.gametype) if self.gametype is not None and kind != 'way' else '')
        path = self.root / (stem + SUFFIXES[kind])
        if path.resolve().parent != self.root:
            raise ValueError('Navigation file escapes its directory.')
        return path

    def names(self):
        names = set()
        for path in self.root.glob('*'):
            if not path.is_file(): continue
            for suffix in ('_goals.gm', '.way', '.gm'):
                if path.name.endswith(suffix):
                    name = path.name[:-len(suffix)]
                    name = re.sub(r'_gt(?:2|5|7)$', '', name)
                    if re.fullmatch(r'[A-Za-z0-9_-]{1,64}', name): names.add(name)
                    break
        return sorted(names)

    def read(self, name):
        files = {}
        for kind in SUFFIXES:
            path = self.path(name, kind)
            item = {'name': path.name, 'path': str(path), 'exists': path.is_file()}
            if item['exists']:
                if path.stat().st_size > 16 * LIMIT: raise ValueError('Navigation file too large.')
                data = path.read_bytes()
                item.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
                if kind != 'way':
                    if len(data) > LIMIT: raise ValueError('Script exceeds 1 MiB editor limit.')
                    try: text = data.decode('utf-8-sig'); encoding = 'utf-8-sig' if data.startswith(b'\xef\xbb\xbf') else 'utf-8'
                    except UnicodeDecodeError: text = data.decode('cp1252'); encoding = 'cp1252'
                    item.update(text=text, encoding=encoding)
            files[kind] = item
        pair = [files[k]['exists'] for k in ('script','goals')]
        overlay=dict(goal_overlay(files['goals'].get('text','global _MG = {};')),sha256=files['goals'].get('sha256'),file=files['goals']['path'],fallback=False)
        if self.gametype is not None and not any(pair):
            legacy=NavBundles(self.root).read(name)
            if legacy['pair_status']=='complete':
                overlay=dict(legacy['goal_overlay'],fallback=True)
        return {'map': name, 'gametype': self.gametype, 'pair_status': 'complete' if all(pair) else 'incomplete' if any(pair) else 'absent', 'directory': str(self.root), 'files': files,
                'goal_overlay': overlay,
                'complete': all(f['exists'] for f in files.values()),
                'note': 'Files are associated by exact basename. Reading does not execute GM or validate mission logic.'}

    def create_pair(self, name):
        if self.gametype is None: raise ValueError('Select a TCE2 gametype first.')
        legacy = NavBundles(self.root)
        existing = [k for k in ('script','goals') if self.path(name,k).exists()]
        if len(existing)==2:
            raise ValueError('Pair already exists; no existing file is overwritten.')
        source = legacy.read(name)['files']
        if not existing and source['script']['exists'] != source['goals']['exists']:
            raise ValueError('Legacy pair incomplete; complete it before copying.')
        data = {k:legacy.path(name,k).read_bytes() for k in ('script','goals')} if not existing and source['script']['exists'] else {
            'script': b'// New gametype mission: add map-specific triggers here.\nglobal Map = {};\nglobal OnMapLoad = function() {};\n',
            'goals': b'global _MG = { Version = 1, };\n'}
        created=[]
        try:
            for kind in ('script','goals'):
                if kind in existing: continue
                p=self.path(name,kind)
                with p.open('xb') as out: out.write(data[kind])
                created.append(p)
        except Exception:
            for p in created: p.unlink()
            raise
        return {'bundle':self.read(name), 'note':'Missing companion created empty; existing data kept. Author mission logic before use.' if existing else 'Gametype pair copied from legacy.' if source['script']['exists'] else 'Empty gametype pair created. Mission logic still needs authoring.'}

    def save(self, name, kind, text, expected):
        if kind not in ('script', 'goals'): raise ValueError('WAY is edited in the waypoint editor.')
        if not isinstance(text, str) or len(text.encode('utf-8')) > LIMIT:
            raise ValueError('Invalid script text / maximum 1 MiB.')
        current = self.read(name)['files'][kind]
        if not current['exists']: raise ValueError('Create map logic explicitly; no placeholder scripts are generated.')
        if expected != current['sha256']: raise RuntimeError('File changed externally. Reload before saving.')
        path = self.path(name, kind)
        original = path.read_bytes()
        if hashlib.sha256(original).hexdigest() != expected: raise RuntimeError('File changed externally. Reload before saving.')
        # Preserve source encoding and CRLF convention; unrelated companion files
        # are never rewritten. Back up the exact original before replacement.
        normalized = text.replace('\r\n', '\n')
        if b'\r\n' in original: normalized = normalized.replace('\n', '\r\n')
        data = normalized.encode(current['encoding'])
        if data == original: return {'saved': False, 'backup': None, 'bundle': self.read(name)}
        backup_dir = self.root / '.navstudio-backups'
        if backup_dir.resolve().parent != self.root: raise ValueError('Backup directory escapes navigation root.')
        backup_dir.mkdir(exist_ok=True)
        backup = backup_dir / (path.name + '.' + str(time.time_ns()) + '.bak')
        backup.write_bytes(original)
        temporary = path.with_name('.navstudio-' + uuid.uuid4().hex + '.tmp')
        try:
            temporary.write_bytes(data)
            if hashlib.sha256(path.read_bytes()).hexdigest() != expected: raise RuntimeError('File changed externally. Save cancelled.')
            temporary.replace(path)
        finally:
            if temporary.exists(): temporary.unlink()
        return {'saved': True, 'backup': str(backup), 'bundle': self.read(name),
                'note': 'Saved on disk only. GM syntax/mission behavior not validated. Reload the map to apply both GM files; waypoint_load alone is insufficient.'}

    def archive(self, name):
        result = io.BytesIO()
        with zipfile.ZipFile(result, 'w', zipfile.ZIP_DEFLATED) as archive:
            for kind, item in self.read(name)['files'].items():
                if item['exists']: archive.writestr(item['name'], self.path(name, kind).read_bytes())
        return result.getvalue()
