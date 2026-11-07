"""Read canonical UI resources; production packaging lives in tools/release.

The sole editable Game resource tree is pak, with explicit final Editor deltas.
This reader never regenerates menus or falls back to duplicate authoring copies.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import zipfile

ROOT=Path(__file__).resolve().parents[2]


def version():
    return re.search(r'^#define TCE_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"',
                     (ROOT/'src/game/tce_version.h').read_text(),re.M)[1]


def sha(data): return hashlib.sha256(data).hexdigest()


def ui_path(name):
    return (name.split('/')[0] in ('ui','ui_obj','ui_robj','ui_tdm') or
            name.startswith(('tce2_ui/','fonts/tce2_ui/')) or
            name=='scripts/tce2_backgrounds.shader' or
            (name.startswith('scripts/tce2_ui') and name.endswith('.shader')))


def collect(variant,repo=None):
    """Return installed VFS bytes, including final Editor overlays unchanged."""
    if variant not in ('game','editor'):
        raise ValueError('Unknown modern UI resource variant')
    repo=Path(repo or ROOT);files={};folded={}
    trees=[repo/'pak']+([repo/'navstudio/editor-pak'] if variant=='editor' else [])
    for tree in trees:
        if not tree.is_dir() or tree.is_symlink():raise ValueError('Missing/unsafe canonical UI tree: '+str(tree))
        for path in sorted(tree.rglob('*')):
            name=path.relative_to(tree).as_posix()
            if not ui_path(name):continue
            if path.is_symlink():raise ValueError('Symlink in canonical UI resources: '+name)
            if not path.is_file():continue
            previous=folded.get(name.casefold())
            if previous is not None and previous!=name:raise ValueError('Case-colliding UI resources: '+name)
            files[name]=path.read_bytes();folded[name.casefold()]=name
    if files.get('tce2_ui/runtime.cfg')!=b'TCE2_MODERN_UI 1\n':raise ValueError('Missing canonical modern UI runtime marker')
    for name,data in files.items():
        if (Path(name).suffix.lower() in ('.dll','.so','.exe','.bsp','.way','.gm','.pk3','.zip') or
                data.startswith((b'MZ',b'\x7fELF',b'\0asm'))):raise ValueError('Non-UI payload in canonical UI resources: '+name)
    if variant=='game':
        for name in files:
            if name.endswith('.menu') and name.split('/')[-1].startswith('tce_'):raise ValueError('Editor UI in Game resources: '+name)
        for name in ('menus.txt','menus_gt2.txt','menus_gt5.txt','menus_gt7.txt'):
            if b'tce_omnibot' in files.get('ui/'+name,b''):raise ValueError('Editor loader in Game UI')
    return files


def build(variant, destination,repo=None):
    """Small diagnostic/test archive, never the distributable mod package."""
    files=collect(variant,repo)
    destination.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
            archive.writestr(info,data)
    with zipfile.ZipFile(destination) as archive:
        assert archive.testzip() is None
        assert all(archive.read(n)==data for n,data in files.items())
    return {'path':str(destination),'entries':len(files),'bytes':destination.stat().st_size,'sha256':sha(destination.read_bytes())}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--variant',choices=('game','editor'),default='game')
    p.add_argument('--check',action='store_true',help='Read-only canonical UI inventory (default)')
    args=p.parse_args();files=collect(args.variant)
    print(json.dumps({'variant':args.variant,'entries':len(files),'bytes':sum(map(len,files.values())),
                      'source':'pak + explicit Editor overlay' if args.variant=='editor' else 'pak'}))


if __name__=='__main__':main()
