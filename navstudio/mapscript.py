"""Read-only PK3 map-script reference and literal Omni-bot trigger comparison."""
import re
import zipfile
import hashlib
from pathlib import Path

GAMETYPES=(2,5,7)


def cvar_value(reply,name):
    """Read one Classic ET quoted cvar reply after removing display colors.

    Q_IsColorString consumes '^' plus a following non-'^' character. Apply
    that rule per line so a malformed value cannot join two protocol lines.
    This decodes console presentation only; directory validation and VFS
    resolution remain the caller's separate job.
    """
    error='ET-Cvar-Antwort ist nicht eindeutig oder ungültig.'
    if (not isinstance(reply,str)or not isinstance(name,str)or
            not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_.-]{0,127}',name)or
            any(ord(c)<32 and c not in '\r\n\t' for c in reply)):
        raise RuntimeError(error)
    pattern=re.compile(r'[ \t]*"([^"\r\n]+)"[ \t]+is:[ \t]*"([^"\r\n]*)"'
                       r'(?:[ \t]+default:[ \t]*"[^"\r\n]*")?'
                       r'(?:[ \t]+latched:[ \t]*"[^"\r\n]*")?[ \t]*')
    rows=[]
    for raw in reply.splitlines():
        clean=[];index=0
        while index<len(raw):
            if raw[index]=='^'and index+1<len(raw)and raw[index+1]!='^':
                index+=2
            else:
                clean.append(raw[index]);index+=1
        line=''.join(clean)
        match=pattern.fullmatch(line)
        if match:rows.append(match.groups())
        elif re.match(r'[ \t]*"[^"\r\n]*"[ \t]+is:',line):
            raise RuntimeError(error)
    if len(rows)!=1 or rows[0][0].lower()!=name.lower():
        raise RuntimeError(error)
    return rows[0][1]


def _paths(name,gametype,directory):
    if not isinstance(name,str)or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}',name):raise ValueError('Invalid map name')
    if gametype is not None and(type(gametype)is not int or gametype not in GAMETYPES):raise ValueError('Invalid gametype')
    if not isinstance(directory,str):raise ValueError('Invalid map script directory')
    clean=directory.rstrip('/');valid=bool(clean)and not(clean.startswith('/')or'..'in clean or':'in clean or'\\'in clean)
    suffix='_gt'+str(gametype)if gametype is not None else''
    result=[]
    if valid:
        result.append((clean+'/'+name+suffix+'.script',True))
        if suffix:result.append((clean+'/'+name+'.script',True))
    result.append(('maps/'+name+suffix+'.script',False))
    if suffix:result.append(('maps/'+name+'.script',False))
    return valid,[(relative.lower(),custom)for relative,custom in result]

def _loose(root,relative):
    current=root
    for part in relative.split('/'):
        if not current.is_dir():return None
        matches=[p for p in current.iterdir()if p.name.lower()==part]
        if len(matches)>1:raise ValueError('Case-colliding loose map scripts: '+relative)
        if not matches:return None
        current=matches[0]
    return current if current.is_file()and current.resolve().is_relative_to(root.resolve())else None

def _sources(assets,names):
    """Same ordered roots/PK3s and loose override as Assets, with actual case."""
    roots=getattr(assets,'roots',None)
    if not isinstance(roots,(list,tuple)):raise ValueError('Mapscript VFS roots are missing')
    wanted=set(names);result={}
    for value in roots:
        root=Path(value).resolve()
        if not root.is_dir():continue
        archives=sorted((p for p in root.iterdir()if p.is_file()and p.suffix.lower()=='.pk3'),key=lambda p:p.name.lower())
        for archive in archives:
            try:
                with zipfile.ZipFile(archive)as zipped:
                    for item in zipped.infolist():
                        relative=item.filename.replace('\\','/').lower()
                        if not item.is_dir()and relative in wanted:
                            if item.file_size>1024*1024:raise ValueError('Map script exceeds 1 MiB.')
                            result[relative]=(archive,item.filename)
            except (OSError,zipfile.BadZipFile):
                # Assets and the Engine exclude unreadable PK3 containers.
                continue
        for relative in wanted:
            path=_loose(root,relative)
            if path is not None:result[relative]=(path,None)
    return result

def _raw(source):
    path,member=source
    if member is None:raw=path.read_bytes()
    else:
        with zipfile.ZipFile(path)as z:raw=z.read(member)
    if len(raw)>1024*1024:raise ValueError('Map script exceeds 1 MiB.')
    return raw

def _source_record(source,identities=None):
    if identities is not None and source in identities:
        record=identities[source]
        if isinstance(record,Exception):raise record
        return dict(record)
    raw=_raw(source);path,member=source
    record=dict(path=str(path),member=member,sha256=hashlib.sha256(raw).hexdigest(),bytes=len(raw))
    if identities is not None:identities[source]=record
    return record

def _binding(sources,name,gametype,directory,identities=None):
    valid,paths=_paths(name,gametype,directory);examined=[];selected=None
    for relative,custom in paths:
        source=sources.get(relative);rows=[]
        if source:
            rows=[_source_record(source,identities)]
        examined.append(dict(relative=relative,sources=rows))
        # Empty custom overrides fall through in G_Script_ScriptLoad; an empty
        # maps/ script is selected by the engine but does not establish a GT.
        if rows and(not custom or rows[0]['bytes']>0):
            selected=dict(relative=relative,sha256=rows[0]['sha256'],bytes=rows[0]['bytes'],sources=rows);break
    return dict(gametype=gametype,requested_directory=directory,directory_valid=valid,examined=examined,selected=selected)

def binding(assets,name,gametype,directory=''):
    """Actual selected map script, including generic fallback; no GT inference."""
    _,paths=_paths(name,gametype,directory)
    return _binding(_sources(assets,[p for p,_ in paths]),name,gametype,directory)

def _bound_bytes(resolution):
    selected=resolution.get('selected')
    if not selected:raise FileNotFoundError('Mapscript fehlt in der aktuellen VFS-Auflösung.')
    sources=selected.get('sources')or[]
    if len(sources)!=1:raise RuntimeError('Mapscript-Quellbindung ist nicht eindeutig.')
    source=sources[0];raw=_raw((Path(source['path']),source.get('member')))
    if (len(raw)!=selected.get('bytes')or len(raw)!=source.get('bytes')or
        hashlib.sha256(raw).hexdigest()!=selected.get('sha256')or source.get('sha256')!=selected.get('sha256')):
        raise RuntimeError('Mapscript-Bytes stimmen nicht mit der SHA-/Größenbindung überein.')
    return raw

def read_raw(assets,name,gametype=None,directory='',expected=None):
    """Return the exact selected source bytes, without codec/newline conversion.

    Pass a previously recorded resolution as expected to bind export/render
    to that source. Generic fallback remains generic, never proof of a GT.
    """
    resolution=binding(assets,name,gametype,directory)
    if expected is not None and resolution!=expected:
        raise RuntimeError('Mapscript-Auflösung hat sich seit der Quellbindung geändert.')
    raw=_bound_bytes(resolution)
    if binding(assets,name,gametype,directory)!=resolution:
        raise RuntimeError('Mapscript-Auflösung wurde während des Lesens geändert.')
    return raw

def decode_script(raw):
    """Lossless editable text and source codec/newline, including UTF-8 BOM."""
    if type(raw)is not bytes:raise TypeError('Mapscript source must be bytes.')
    if raw.startswith(b'\xef\xbb\xbf'):codec='utf-8-sig';text=raw.decode(codec)
    else:
        try:text=raw.decode('utf-8');codec='utf-8'
        except UnicodeDecodeError:text=raw.decode('cp1252',errors='surrogateescape');codec='cp1252'
    newline=re.search(r'\r\n|\r|\n',text)
    return dict(text=text,codec=codec,newline=newline[0]if newline else'\n')

def encode_script(text,codec):
    """Encode caller-supplied text in the unchanged detected source codec."""
    if not isinstance(text,str)or codec not in('utf-8','utf-8-sig','cp1252'):raise ValueError('Invalid script text/codec.')
    raw=text.encode(codec,errors='surrogateescape'if codec=='cp1252'else'strict')
    if len(raw)>1024*1024:raise ValueError('Map script exceeds 1 MiB.')
    return raw

def render_gt7(raw,document):
    """Replace only the managed spawn block; retain source bytes elsewhere."""
    from spawns import script_block
    source=decode_script(raw)
    return _encode_edit(raw,source,script_block(source['text'],document))

def _encode_edit(raw,source,after):
    before=source['text']
    if after==before:return raw
    # script_block changes one contiguous managed span. Keep its unchanged
    # prefix/suffix literally, including mixed original newline sequences.
    prefix=0
    while prefix<min(len(before),len(after))and before[prefix]==after[prefix]:prefix+=1
    if prefix and any(prefix<len(value)and value[prefix-1:prefix+1]=='\r\n'for value in(before,after)):prefix-=1
    suffix=0
    while suffix<min(len(before)-prefix,len(after)-prefix)and before[-suffix-1]==after[-suffix-1]:suffix+=1
    if suffix and any(0<len(value)-suffix<len(value)and value[len(value)-suffix-1:len(value)-suffix+1]=='\r\n'for value in(before,after)):suffix-=1
    end=len(after)-suffix if suffix else len(after)
    inserted=re.sub(r'\r\n|\r|\n',lambda match:source['newline'],after[prefix:end])
    text=before[:prefix]+inserted+(before[len(before)-suffix:]if suffix else'')
    return encode_script(text,source['codec'])

def availability(assets,name,directory=''):
    """Only a nonempty actually selected map_gtN.script establishes TCE GTN.

    A generic fallback can be displayed, but never authorizes GT-specific
    goals, probing or generation. No Engine, process or filesystem mutation.
    """
    return availability_many(assets,[name],directory)[name]

def availability_many(assets,names,directory=''):
    """One VFS scan for a map picker containing hundreds of maps."""
    names=list(dict.fromkeys(names))
    paths=[p for name in names for gt in GAMETYPES for p,_ in _paths(name,gt,directory)[1]]
    sources=_sources(assets,paths)
    return {name:_availability(sources,name,directory)for name in names}

def _availability(sources,name,directory,identities=None):
    rows={}
    for gt in GAMETYPES:
        resolved=_binding(sources,name,gt,directory,identities);selected=resolved['selected']
        exact=name.lower()+'_gt'+str(gt)+'.script'
        available=bool(selected and selected['bytes']>0 and selected['relative'].split('/')[-1]==exact)
        rows[str(gt)]=dict(gametype=gt,available=available,exact_name=exact,resolved=resolved,
            reason=None if available else'exact_gametype_script_missing_or_shadowed')
    return dict(contract='tce_exact_resolved_gt_mapscript_v1',map=name,directory=directory,
        available=[gt for gt in GAMETYPES if rows[str(gt)]['available']],gametypes=rows)

def _catalog_archive(path,wanted,identities):
    """Read selected scripts once, retaining only their SHA and size."""
    result={}
    with zipfile.ZipFile(path)as zipped:
        for item in zipped.infolist():
            relative=item.filename.replace('\\','/').lower()
            if item.is_dir()or relative not in wanted:continue
            source=(path,item.filename);result[relative]=source
            if item.file_size>1024*1024:
                identities[source]=ValueError('Map script exceeds 1 MiB.')
                continue
            raw=zipped.read(item)
            identities[source]=dict(path=str(path),member=item.filename,
                sha256=hashlib.sha256(raw).hexdigest(),bytes=len(raw))
    return result

def _catalog_loose(root,wanted):
    # Do not enumerate the same directory once per GT/map in a large picker.
    children={};result={}
    for relative in wanted:
        current=root
        for part in relative.split('/'):
            if current not in children:
                indexed={}
                if current.is_dir():
                    for child in current.iterdir():indexed.setdefault(child.name.lower(),[]).append(child)
                children[current]=indexed
            matches=children[current].get(part,[])
            if len(matches)>1:raise ValueError('Case-colliding loose map scripts: '+relative)
            if not matches:break
            current=matches[0]
        else:
            if current.is_file()and current.resolve().is_relative_to(root):result[relative]=(current,None)
    return result

def catalog_availability(assets,catalog,directory=''):
    """Project each catalog source into the real base/mod VFS, without leasing.

    ``assets.roots`` describes the Engine's ordered base/mod roots, and
    ``catalog_base_root`` is the existing etmain lease destination. Other
    installed PK3s and loose/mod overrides remain effective. Library archives
    participate only when selected. No persistent cache or file copy is used.
    """
    rows=[dict(row)for row in catalog];tce=[row for row in rows if row.get('mod')=='tce2']
    if not tce:return rows
    def failed(row,message):
        name=row.get('map','?')
        row['gametype_availability']=dict(contract='tce_exact_resolved_gt_mapscript_v1',map=name,directory=directory,
            available=[],gametypes={str(gt):dict(gametype=gt,available=False,reason='catalog_source_unconfirmed',
                reasons=[message],exact_name=str(name)+'_gt'+str(gt)+'.script',resolved=None)for gt in GAMETYPES})
    roots=getattr(assets,'roots',None);base=getattr(assets,'catalog_base_root',None)
    if not isinstance(roots,(list,tuple))or base is None:
        for row in tce:failed(row,'Engine-VFS/etmain-Ziel für diese Mapquelle nicht eindeutig gebunden.')
        return rows
    roots=list(dict.fromkeys(Path(root).resolve()for root in roots));base=Path(base).resolve()
    if base not in roots or not base.is_dir():
        for row in tce:failed(row,'Engine-etmain-Ziel für diese Mapquelle fehlt.')
        return rows
    names=[];valid=[]
    for row in tce:
        try:
            name=row['map'];_paths(name,2,directory)
            source=Path(row['source_path']).resolve()
            if source.suffix.lower()!='.pk3'or not source.is_file()or source.name!=row['filename']:
                raise ValueError('Map-PK3-Quelle fehlt oder stimmt nicht mit dem Katalog überein.')
            names.append(name);valid.append((row,source))
        except(KeyError,OSError,TypeError,ValueError)as exc:failed(row,str(exc))
    wanted={relative for name in names for gt in GAMETYPES for relative,_ in _paths(name,gt,directory)[1]}
    archive_cache={};identities={};layers=[]
    def archive(path):
        if path not in archive_cache:
            try:archive_cache[path]=_catalog_archive(path,wanted,identities)
            except(OSError,zipfile.BadZipFile)as exc:archive_cache[path]=exc
        return archive_cache[path]
    try:
        for root in roots:
            installed={}
            if root.is_dir():
                for path in sorted(root.iterdir(),key=lambda p:p.name.lower()):
                    if path.is_file()and path.suffix.lower()=='.pk3':
                        value=archive(path)
                        if not isinstance(value,Exception):installed[path.name.lower()]=value
            layers.append((root,installed,_catalog_loose(root,wanted)))
    except(OSError,ValueError)as exc:
        for row,_ in valid:failed(row,str(exc))
        return rows
    for row,source in valid:
        try:
            selected=archive(source)
            if isinstance(selected,Exception):raise ValueError('Map-PK3 kann nicht gelesen werden: '+source.name)
            sources={}
            for root,installed,loose in layers:
                projected=dict(installed)
                if root==base:projected[source.name.lower()]=selected
                for key in sorted(projected):sources.update(projected[key])
                sources.update(loose)
            value=_availability(sources,row['map'],directory,identities)
            value['catalog_projection']=dict(source_path=str(source),target_path=str(base/source.name),
                source_id=row.get('source_id'),physical_files_changed=False)
            for item in value['gametypes'].values():
                if item['available']:continue
                picked=item['resolved']['selected']
                item['reasons']=[('Aufgelöst wird '+picked['relative']+'; 'if picked else'')+
                    'exaktes '+item['exact_name']+' fehlt, ist leer oder wird verdeckt.']
            row['gametype_availability']=value
        except(OSError,TypeError,ValueError)as exc:failed(row,str(exc))
    return rows

def require_gametype(assets,name,gametype,directory=''):
    """Pure API guard for buttons/jobs that must not manufacture a TCE GT."""
    if type(gametype)is not int or gametype not in GAMETYPES:raise ValueError('Invalid gametype')
    value=availability(assets,name,directory)
    if gametype not in value['available']:
        raise ValueError('GT%d ist für %s nicht vorhanden: exaktes %s_gt%d.script fehlt oder wird durch ein generisches Script verdeckt.'%(gametype,name,name,gametype))
    return value

def read(assets, name, gm='', gametype=None,directory=''):
    if gametype is not None:
        if str(gametype)not in('2','5','7'):raise ValueError('Invalid gametype')
        gametype=int(gametype)
    resolved=binding(assets,name,gametype,directory);selected=resolved['selected'];found=None
    wanted=_paths(name,gametype,directory)[1][0][0]
    if selected:
        source=selected['sources'][0];path=Path(source['path']);member=source['member'];raw=_bound_bytes(resolved)
        decoded=decode_script(raw);text=decoded['text']
        found=dict(name=member or path.relative_to(next(Path(r).resolve()for r in reversed(assets.roots)if path.is_relative_to(Path(r).resolve()))).as_posix(),
            path=str(path)+(' :: '+member if member else''),text=text,exists=True,bytes=len(raw),
            sha256=source['sha256'],encoding='PK3 / read-only'if member else'loose / read-only',codec=decoded['codec'],newline=decoded['newline'],resolution=resolved)
    if found is None:return dict(name=wanted,path=wanted,exists=False,text='',bytes=0,encoding='PK3 / read-only',matches=[],unmatched=[])
    # A textual cross-reference, not a simulation of ET's scripting interpreter.
    announces=re.findall(r'\bwm_announce\s+"([^"\r\n]*)"',found['text'])
    triggers=re.findall(r'\bOnTrigger\s*\(\s*"([^"\r\n]*)"\s*,\s*([^\s,)]+)',gm)
    found['matches']=[dict(message=t,callback=c) for t,c in triggers if t in announces]
    found['unmatched']=[dict(message=t,callback=c) for t,c in triggers if t not in announces]
    return found


def generate_spawns(studio, name,directory=''):
    """Create a backed-up GT7 override from the PK3 (or existing override)."""
    import math, time, hashlib
    name=studio.require_map(name)
    if studio.game_mode!='tce2': raise ValueError('GT7 spawns require TCE2.')
    proof=studio.proof_status(name)
    if not proof.get('valid'): raise ValueError('Aktuelle native Wegnetz-Pruefung erforderlich: '+str(proof.get('reasons',[])))
    report=studio.native(name)
    nodes={n['id']:n for n in report['nodes'] if n.get('accepted') and n.get('spawn_reachable') and not n.get('crouch') and n.get('nav_flags',0)==0}
    outgoing={}; incoming=set()
    for e in report['edges']:
        if e.get('accepted') and e['from'] in nodes and e['to'] in nodes:
            outgoing.setdefault(e['from'],[]).append(e['to']); incoming.add(e['to'])
    pool=[n for i,n in nodes.items() if i in outgoing and i in incoming]
    def dist(a,b): return sum((x-y)**2 for x,y in zip(a['position'],b['position']))
    selected=[]
    while pool and len(selected)<32:
        pick=max(pool,key=lambda n:min(dist(n,p) for p in selected)) if selected else min(pool,key=lambda n:n['id'])
        if selected and min(dist(pick,p) for p in selected)<256**2: break
        selected.append(pick);pool.remove(pick)
    if len(selected)<4: raise ValueError('Zu wenige angeschlossene Standpunkte fuer verteilte Spawns.')
    available=availability(studio.assets,name,directory)
    if 7 not in available['available']:raise ValueError('Exaktes GT7-Mapscript fehlt; generischer Fallback erzeugt keine GT7-Spawns.')
    source=read(studio.assets,name,gametype=7,directory=directory)
    source_raw=read_raw(studio.assets,name,7,directory,expected=source['resolution'])
    directory=studio.mod/'mapscripts';directory.mkdir(exist_ok=True)
    path=directory/(name+'_gt7.script')
    if path.resolve().parent!=directory.resolve(): raise ValueError('Invalid target')
    original=path.read_bytes() if path.exists() else None
    raw=original if original is not None else source_raw
    decoded=decode_script(raw);text=decoded['text']
    start='// NAVSTUDIO_GT7_SPAWNS_BEGIN';end='// NAVSTUDIO_GT7_SPAWNS_END'
    previous=re.search(r'\btce_dmspawns\s+([01])',text)
    enabled=previous[1] if previous else '1'
    lines=[start,'// 1 = info_player_deathmatch; 0 = standard team spawns.','tce_dmspawns '+enabled]
    for n in selected:
        target=nodes[outgoing[n['id']][0]]['position'];pos=n['position']
        yaw=math.degrees(math.atan2(target[1]-pos[1],target[0]-pos[0]))
        lines.append('tce_dmspawn '+' '.join(format(v,'.3f') for v in (*pos,yaw)))
    lines.append(end);block='\n\t\t'+'\n\t\t'.join(lines)+'\n'
    if start in text:
        if text.count(start)!=1 or text.count(end)!=1: raise ValueError('Mehrdeutiger Generatorblock.')
        text=text[:text.index(start)]+block.strip()+text[text.index(end)+len(end):]
    else:
        # Ignore comments when locating game_manager/spawn, retain source offsets.
        clean=re.sub(r'/\*.*?\*/|//[^\r\n]*',lambda m:' '*len(m[0]),text,flags=re.S)
        m=re.search(r'\bgame_manager\s*\{\s*spawn\s*\{',clean)
        if not m: raise ValueError('game_manager spawn Block nicht eindeutig gefunden.')
        text=text[:m.end()]+block+text[m.end():]
    data=_encode_edit(raw,decoded,text)
    if original is not None:
        backup=directory/'.navstudio-backups';backup.mkdir(exist_ok=True)
        (backup/(path.name+'.'+str(time.time_ns()))).write_bytes(original)
    temporary=path.with_suffix('.script.tmp');temporary.write_bytes(data);temporary.replace(path)
    return dict(path=str(path),count=len(selected),source=source['path'],sha256=hashlib.sha256(data).hexdigest(),note='GT7 Override erstellt. g_mapScriptDirectory mapscripts setzen und GT7 neu starten. tce_dmspawns 0 nutzt Standardspawns. Native Laufzeit prueft jeden Spawn erneut.')


def custom(studio,name,gametype,text=None,expected=None):
    import hashlib,time
    name=studio.require_map(name)
    if studio.game_mode!='tce2' or gametype not in (2,5,7): return dict(exists=False,path='Custom: TCE2 Gametype auswaehlen',text='')
    directory=studio.mod/'mapscripts';path=directory/(name+'_gt'+str(gametype)+'.script')
    if path.resolve().parent!=directory.resolve(): raise ValueError('Invalid custom path')
    data=path.read_bytes() if path.exists() else None
    if data is not None and len(data)>1024*1024: raise ValueError('Script exceeds 1 MiB')
    digest=hashlib.sha256(data).hexdigest() if data is not None else None
    if text is not None:
        if not isinstance(text,str) or not text.strip() or len(text)>1024*1024: raise ValueError('Invalid script text')
        if data is None or digest!=expected: raise RuntimeError('Customscript wurde geaendert; neu laden.')
        replacement=encode_script(text,decode_script(data)['codec'])
        backup=directory/'.navstudio-backups';backup.mkdir(exist_ok=True)
        (backup/(path.name+'.'+str(time.time_ns()))).write_bytes(data)
        temporary=path.with_suffix('.script.tmp');temporary.write_bytes(replacement);temporary.replace(path)
        data=replacement;digest=hashlib.sha256(data).hexdigest()
    decoded=decode_script(data)if data is not None else dict(text='',codec='cp1252')
    return dict(note='Customscript gespeichert; Map neu laden.' if text is not None else '',name=path.name,path=str(path),exists=data is not None,text=decoded['text'],sha256=digest,bytes=len(data or b''),encoding=decoded['codec'])
