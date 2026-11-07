#!/usr/bin/env python3
"""Package selected CGame/UI ABIs with complete pak resources; never build/deploy."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import sys
import zipfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/release'))
import client_packages as clients
import resource_assets as resources
from verify_omnibot_boundary import verify_module_bytes

VERSION='1.0.3'
TARGETS=('windows-x86','linux-x86_64')
OPTIONAL_TARGET='linux-i386'
VARIANTS={'game':'Game','navstudio':'Editor'}
UI_AUTHORING=('omnibot_menu','omnibot_goalname','omnibot_goalproperty')


def sha(data):return hashlib.sha256(data).hexdigest()


def helper_paths():
    paths={name:ROOT/'tools/release'/name for name in
           ('client_packages.py','resource_assets.py','verify_omnibot_boundary.py','targets.json')}
    paths['editor_ui.py']=ROOT/'tools/assets/editor_ui.py'
    for name in ('base-game-reference.json','base-ui-reference.json'):
        paths[name]=ROOT/'tools/assets'/name
    return paths


def known_input_paths(manifests):
    """Protect discovery/tool inputs even when native validation fails early."""
    paths={Path(__file__).resolve(),*(p.resolve() for p in helper_paths().values()),
           *(Path(p).resolve() for p in manifests.values())}
    for path in manifests.values():
        try:
            metadata=json.loads(Path(path).read_bytes())
        except (OSError,ValueError):
            continue
        modules=metadata.get('modules') if isinstance(metadata,dict) else None
        if not isinstance(modules,dict):continue
        for value in modules.values():
            if not isinstance(value,str) or not value:continue
            module=Path(value)
            if not module.is_absolute():module=Path(path).resolve().parent/module
            try:paths.add(module.resolve())
            except (OSError,ValueError):continue
    return paths


def create_packages(manifests):
    """Bind exact native inputs, validate both variants, then return ZIP bytes."""
    required={(variant,target) for variant in VARIANTS for target in TARGETS}
    optional={(variant,OPTIONAL_TARGET) for variant in VARIANTS}
    if set(manifests) not in (required,required|optional):
        raise ValueError('Game/Editor x Win32/Linux64 and an optional complete Linux-i386 pair are required')
    targets=TARGETS+(OPTIONAL_TARGET,) if set(manifests)==required|optional else TARGETS
    snapshots={};rows={variant:[] for variant in VARIANTS};boundaries={};input_reports={}
    def read(path):
        path=Path(path).resolve();data=path.read_bytes()
        if path in snapshots and snapshots[path]!=data:raise ValueError('Input changed during collection: '+str(path))
        snapshots[path]=data
        return data
    tool_path=Path(__file__).resolve()
    helpers=helper_paths()
    read(tool_path)
    for path in helpers.values():read(path)
    for (variant,target),path in sorted(manifests.items()):
        metadata=json.loads(read(path))
        if (not isinstance(metadata,dict) or metadata.get('schema')!=1 or
                metadata.get('variant')!=variant or metadata.get('configuration')!='Release' or
                ('target' in metadata and metadata['target']!=target)):
            raise ValueError('Wrong module build identity/configuration: '+str(path))
        names=clients.module_names(target);modules=metadata.get('modules')
        if not isinstance(modules,dict) or not {'cgame','ui'}<=set(modules):
            raise ValueError('Missing CGame/UI discovery paths: '+str(path))
        # Transfers carry modules only. Large resources are frozen once per
        # variant after every native input has passed validation.
        payload={};checks={}
        for role,name in names.items():
            module=Path(modules[role])
            if not module.is_absolute():module=Path(path).resolve().parent/module
            if module.name!=name:raise ValueError('Wrong discovered native filename: '+str(module))
            data=read(module);clients.check_module(data,role,target,variant)
            boundary=verify_module_bytes(role,data,variant)
            if role=='ui':
                # These are the actual current source endpoints, supplementing
                # the older generic checker marker navstudio_bot_menu.
                actual={literal:literal.encode()+b'\0' in data for literal in UI_AUTHORING}
                if (variant=='game' and any(actual.values()) or variant=='navstudio' and not all(actual.values())):
                    raise ValueError('Wrong actual UI authoring boundary: '+target+'/'+variant)
                boundary['actual_ui_editor_literals']=actual
            elif variant=='navstudio' and not any(boundary['editor_literals'].values()):
                raise ValueError('Missing Editor CGame authoring boundary: '+target)
            payload[name]=data;checks[role]=boundary
        clients.check_target_payload(payload,target,variant,VERSION,module_only=True)
        rows[variant].append({'metadata':{'target':target,'variant':variant,'configuration':'Release'},'payload':payload})
        boundaries[target+'/'+variant]=checks
        input_reports[target+'/'+variant]={'manifest':str(Path(path).resolve()),'manifest_sha256':sha(snapshots[Path(path).resolve()]),
                                           'modules':{role:{'filename':name,'sha256':sha(payload[name]),'bytes':len(payload[name])} for role,name in names.items()}}
    packages={};outputs={};frozen={};resource_paths={};resource_reports={}
    marker='tce2/'+VERSION+'/client.manifest'
    base_marker='tce2/'+VERSION+'/base-game.json'
    for variant in VARIANTS:
        paths=resources.resource_paths(variant,ROOT)
        assets=clients.client_assets(VERSION,variant,ROOT)
        if set(assets)!=set(paths)|{marker,base_marker} or assets[marker]!=VERSION.encode('ascii'):
            raise ValueError('Incomplete/generated resource closure: '+variant)
        base,base_hash=resources.base_game_reference(ROOT,VERSION)
        dependency=json.loads(assets[base_marker])
        if (dependency.get('reference_sha256')!=base_hash or dependency.get('release_version')!=VERSION or
                dependency.get('baseline_id')!=base['baseline_id'] or dependency.get('source_packages')!=base['source_packages']):
            raise ValueError('Wrong frozen stock dependency marker: '+variant)
        resource_paths[variant]=paths;frozen[variant]=assets
        source_files={}
        for name,path in paths.items():
            path=Path(path).resolve();value=assets[name]
            if path in snapshots and snapshots[path]!=value:
                raise ValueError('Resource changed during collection: '+name)
            snapshots[path]=value
            source_files[name]={'bytes':len(value),'sha256':sha(value)}
        encoded=(json.dumps(source_files,sort_keys=True,separators=(',',':'))+'\n').encode()
        resource_reports[variant]={'schema':1,'kind':'mod-delta','base_reference_sha256':base_hash,'files':len(source_files),
                                  'bytes':sum(row['bytes'] for row in source_files.values()),'sha256':sha(encoded)}
    # The Editor deliberately replaces menu/registry resources. Every differing
    # or additional file must come from its explicit overlay, while all other
    # production assets stay byte-identical to Game.
    overlay=(ROOT/'navstudio/editor-pak').resolve()
    allowed={name for name,path in resource_paths['navstudio'].items() if Path(path).resolve().is_relative_to(overlay)}
    actual={name for name in frozen['navstudio'] if frozen['game'].get(name)!=frozen['navstudio'][name]}
    missing=set(frozen['game'])-set(frozen['navstudio'])
    if missing or not actual<=allowed:
        raise ValueError('Game/Editor assets differ outside the explicit editor overlay')
    for variant in VARIANTS:
        assets=frozen[variant]
        data,metadata=clients.canonical_client(rows[variant],targets,VERSION,variant,assets=assets)
        if clients.check_canonical(data,VERSION,variant,targets,assets=assets)!=metadata:raise ValueError('Canonical client metadata differs')
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            names=archive.namelist()
            if archive.testzip() is not None:raise ValueError('Client PK3 CRC failure')
            if any('qagame' in name.lower() or name.endswith(('.pk3','.zip','.pdb')) for name in names):
                raise ValueError('Server/nested payload in client mod PK3')
            if variant=='game' and 'ui/tce_omnibot.menu' in names:
                raise ValueError('Editor menu leaked into Game')
        packages[variant]=data
        outputs[variant]={'sha256':sha(data),'bytes':len(data),'entries':len(names),'metadata':metadata}
    for path,data in snapshots.items():
        # Stream the final comparison instead of materializing another full pak
        # copy. snapshots already owns the frozen bytes used for both archives.
        value=hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda:stream.read(1024*1024),b''):value.update(block)
        if value.hexdigest()!=sha(data):raise ValueError('Native/resource input changed during packaging: '+str(path))
    for variant,paths in resource_paths.items():
        current=resources.resource_paths(variant,ROOT)
        if {name:Path(path).resolve() for name,path in current.items()}!={name:Path(path).resolve() for name,path in paths.items()}:
            raise ValueError('Resource path inventory changed during packaging: '+variant)
    report={'schema':1,'scope':'local-diagnostic','version':VERSION,'targets':list(targets),'inputs':input_reports,
            'outputs':outputs,'module_boundaries':boundaries,'input_stable':True,'shared_assets_equal':True,
            'shared_assets_policy':'Base resources stay equal; only explicit navstudio/editor-pak replacements/additions may differ.',
            'resource_trees':resource_reports,'editor_resource_differences':sorted(actual),'resource_overlay_verified':True,
            'qagame_included':False,'engine_started':False,'runtime_copied':False,
            'input_files':{str(path):sha(data) for path,data in snapshots.items()},
            'tool_sha256':sha(snapshots[tool_path]),
            'helper_sha256':{name:sha(snapshots[path.resolve()]) for name,path in helpers.items()},
            'limits':[f'Exactly {len(targets)} selected client ABIs, not complete ten-target release closure.',
                      'Native module bytes/ABI/authoring boundary/resources validated; rebuild freshness and runtime acceptance are separate evidence.']}
    return packages,report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('windows-game','windows-editor','linux-game','linux-editor'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('linux-i386-game','linux-i386-editor'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--out',type=Path,required=True);parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    manifests={('game','windows-x86'):args.windows_game,('navstudio','windows-x86'):args.windows_editor,
               ('game','linux-x86_64'):args.linux_game,('navstudio','linux-x86_64'):args.linux_editor}
    if (args.linux_i386_game is None)!=(args.linux_i386_editor is None):
        parser.error('Linux-i386 requires both Game and Editor manifests')
    if args.linux_i386_game is not None:
        manifests.update({('game',OPTIONAL_TARGET):args.linux_i386_game,
                          ('navstudio',OPTIONAL_TARGET):args.linux_i386_editor})
    protected=[ROOT/'pak',ROOT/'pak_original_tce',ROOT/'src/assets',ROOT/'navstudio/editor-pak',ROOT/'tools/ui_modernize/source']
    if args.report.suffix.lower()!='.json':parser.error('Report must be a JSON receipt')
    if any(path.resolve().is_relative_to(base.resolve()) for path in (args.out,args.report) for base in protected):
        parser.error('Client output/report must be outside original/theme/production asset sources')
    if args.report.resolve() in {path.resolve() for path in manifests.values()}:
        parser.error('Report cannot overwrite an input manifest')
    destinations={variant:args.out/folder/('tce2-'+VERSION+'.pk3') for variant,folder in VARIANTS.items()}
    if known_input_paths(manifests)&{path.resolve() for path in (args.report,*destinations.values())}:
        parser.error('Client output/report cannot overwrite native/discovery/tool inputs')
    try:
        packages,report=create_packages(manifests)
        if str(args.report.resolve()) in report['input_files'] or any(str(path.resolve()) in report['input_files'] for path in destinations.values()):
            parser.error('Client output/report cannot overwrite native/discovery/tool inputs')
        if args.report.resolve() in {path.resolve() for path in destinations.values()}:
            raise ValueError('Report cannot overwrite a client PK3')
        for variant,path in destinations.items():
            path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(packages[variant])
            if path.read_bytes()!=packages[variant]:raise ValueError('Client PK3 readback mismatch')
            report['outputs'][variant]['path']=str(path.resolve())
        report['passed']=True
    except (ValueError,OSError,KeyError,TypeError,zipfile.BadZipFile) as error:
        report={'passed':False,'errors':[str(error)]}
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'passed':report['passed'],'outputs':{v:{k:row[k] for k in ('path','bytes','sha256','entries')} for v,row in report.get('outputs',{}).items()},'errors':report.get('errors',[])}))
    return 0 if report['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
