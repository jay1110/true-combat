"""Offline completeness/freshness checks for the desktop package. Never launches it."""
import argparse
import ast
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import time


MANIFEST = "navstudio-package.json"
MODULES = ("layout", "completion", "desktop", "server", "edit", "watch", "settings", "projects", "spawns",
           "goalgen", "mapscript", "navbundle", "botmenu", "repair", "doorrepair", "pathrepair", "objectiveanchors", "quiet", "etsnapshot")
WORKERS = ("navstudio/workers/tce_bsp_navmesh.py", "navstudio/workers/tce_bsp_patch.py", "navstudio/workers/tce_navgen.py",
           "navstudio/navmesh/waygraph.py", "navstudio/navmesh/objective_contact.py",
           "navstudio/navmesh/report.py", "navstudio/workers/tce_nav_livecheck.py", "src/game/surfaceflags.h")
WEBVIEW = ("webview/lib/Microsoft.Web.WebView2.Core.dll", "webview/lib/Microsoft.Web.WebView2.WinForms.dll",
           "webview/lib/runtimes/win-x64/native/WebView2Loader.dll", "pythonnet/runtime/Python.Runtime.dll")


def sha256(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''): digest.update(block)
    return digest.hexdigest()


def bounded(root, relative):
    item=PurePosixPath(relative)
    if not item.parts or item.is_absolute() or any(p in ('..', '.') or ':' in p or '\\' in p for p in item.parts):
        raise ValueError('Invalid package-relative path: '+relative)
    root=Path(root).resolve(); target=(root/Path(*item.parts)).resolve()
    if not target.is_relative_to(root): raise ValueError('Package path escapes its root: '+relative)
    return target


def static_files(workspace):
    tree=ast.parse((Path(workspace)/'navstudio/server.py').read_text(encoding='utf8'))
    for statement in tree.body:
        if isinstance(statement,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='STATIC_FILES' for t in statement.targets):
            names=ast.literal_eval(statement.value)
            if not isinstance(names,(set,list,tuple)) or not all(isinstance(n,str) and '/' not in n and '\\' not in n for n in names):
                raise ValueError('STATIC_FILES must contain plain local filenames.')
            return sorted(names)
    raise ValueError('Server STATIC_FILES declaration is missing.')


def required_files(workspace, name):
    if not re.fullmatch(r'[A-Za-z0-9_-]+',name): raise ValueError('Invalid package name.')
    source=['navstudio/'+s for s in static_files(workspace)]
    source+=['navstudio/'+m+'.py' for m in MODULES]
    source+=['navstudio/entry.py','navstudio/start-test.ps1',
             'navstudio/navstudio-display.lua','navstudio/README.md',*WORKERS]
    # The local voice DMS is a separately installed authoring addon; its exact
    # menu pages must nevertheless be present and current in the desktop tool.
    source += ['navstudio/editor-ui/'+name for name in
               ('editor-files.txt','tce_omnibot.menu','menus.txt','menus_gt2.txt','menus_gt5.txt','menus_gt7.txt',
                'wm_quickmessage.menu','wm_quickmessageAlt.menu',
                'wm_ftquickmessage.menu','wm_ftquickmessageAlt.menu')]
    source += [p.relative_to(Path(workspace)).as_posix()
               for p in (Path(workspace)/'navstudio/editor-ui').glob('*')
               if p.is_file() and (p.suffix=='.menu' or p.name.startswith('menus') and p.suffix=='.txt')]
    for menu in (Path(workspace)/'navstudio/editor-ui').glob('menus*.txt'):
        source += ['navstudio/editor-ui/'+name for name in
                   re.findall(r'loadMenu\s*\{\s*"ui/(tce_[A-Za-z0-9_]+\.menu)"',menu.read_text())]
    # Stage the same source closure that the frozen application binds. Do not
    # carry test fixtures, __pycache__, source binaries or unrelated workspaces.
    for directory in ('navstudio', 'navstudio/navmesh', 'navstudio/workers'):
        source += [p.relative_to(Path(workspace)).as_posix()
                   for p in (Path(workspace)/directory).glob('*.py')
                   if not p.name.startswith('test_')]
    source += ['navstudio/requirements-desktop.txt']
    payload=['_internal/'+s for s in source]
    payload+=['_internal/'+s for s in WEBVIEW]
    payload+=['_internal/bin/tce_navmesh_preview.exe','_internal/licenses/recast/License.txt',
              name+'.exe','README.md','Start.cmd','verify-package.ps1']
    return sorted(set(source)),sorted(set(payload))


def stage(destination, workspace, name='TCE2-NavStudio'):
    destination,workspace=Path(destination).resolve(),Path(workspace).resolve()
    if destination == workspace or destination.is_relative_to(workspace/'navstudio'):
        raise ValueError('Staging directory must be separate from the source tree.')
    if destination.exists() and any(destination.iterdir()):
        raise ValueError('Staging directory must be empty; use a new build directory.')
    sources,_=required_files(workspace,name)
    for relative in sources:
        original,target=bounded(workspace,relative),bounded(destination,relative)
        if not original.is_file(): raise ValueError('Missing package input: '+relative)
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(original,target)
    return dict(source_files=sources)


def create(package, workspace, recast, name='TCE2-NavStudio'):
    package,workspace=Path(package).resolve(),Path(workspace).resolve()
    sources,payload=required_files(workspace,name)
    # Sidecar scripts/assets must be the same revision as the frozen entry.
    for source in sources:
        original,installed=bounded(workspace,source),bounded(package,'_internal/'+source)
        if not original.is_file() or not installed.is_file() or sha256(original)!=sha256(installed):
            raise ValueError('Missing or stale package source: '+source)
    for relative in payload:
        if not bounded(package,relative).is_file(): raise ValueError('Missing package file: '+relative)
    if sha256(recast)!=sha256(package/'_internal/bin/tce_navmesh_preview.exe'):
        raise ValueError('Packaged Recast executable differs from the selected current build.')
    if sha256(workspace/'navstudio/README.md')!=sha256(package/'README.md'):
        raise ValueError('Root package README differs from the in-app guide source.')
    # Include all production Python sources used by lazy jobs, even if a future
    # module is not added explicitly to the required entry point list yet.
    sources=sorted(set(sources)|{p.relative_to(workspace).as_posix() for p in (workspace/'navstudio').glob('*.py') if not p.name.startswith('test_')})
    value=dict(schema=1,entry_kind='desktop-projects',package_name=name,created=time.time(),
               source_files=[dict(path=s,sha256=sha256(bounded(workspace,s))) for s in sources],
               package_files=[dict(path=s,sha256=sha256(bounded(package,s))) for s in payload])
    (package/MANIFEST).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    verify(package,workspace)
    return value


def verify(package, workspace=None):
    package=Path(package).resolve()
    try: value=json.loads((package/MANIFEST).read_text(encoding='utf8'))
    except (OSError,ValueError) as error: raise ValueError('Desktop package manifest is missing or unreadable; rebuild the package.') from error
    if value.get('schema')!=1 or value.get('entry_kind')!='desktop-projects' or not value.get('package_files'):
        raise ValueError('Unsupported or old NavStudio package; rebuild the desktop package.')
    for key,root in [('package_files',package)]+([('source_files',Path(workspace).resolve())] if workspace is not None else []):
        for record in value[key]:
            path=bounded(root,record['path'])
            if not re.fullmatch(r'[0-9a-f]{64}',record['sha256']) or not path.is_file() or sha256(path)!=record['sha256']:
                raise ValueError('Missing, changed or stale '+key+': '+record['path'])
    return value


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation',choices=('create','verify','stage'));parser.add_argument('--package',type=Path,required=True)
    parser.add_argument('--workspace',type=Path);parser.add_argument('--recast',type=Path);parser.add_argument('--name',default='TCE2-NavStudio')
    args=parser.parse_args()
    if args.operation=='stage':
        if args.workspace is None: parser.error('stage requires --workspace')
        result=stage(args.package,args.workspace,args.name)
        print('Desktop sources staged: '+str(args.package)+' · '+str(len(result['source_files']))+' files')
        return
    if args.operation=='create':
        if args.workspace is None or args.recast is None: parser.error('create requires --workspace and --recast')
        result=create(args.package,args.workspace,args.recast,args.name)
    else: result=verify(args.package,args.workspace)
    print('Desktop package verified: '+str(args.package)+' Â· '+str(len(result['package_files']))+' required files')


if __name__=='__main__': main()
