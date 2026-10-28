"""Build opt-in, UI-only PK3s; never change the original pak folder."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import zipfile
import restyle

ROOT=Path(__file__).resolve().parents[2]


def sha(data): return hashlib.sha256(data).hexdigest()


def build(variant, destination):
    edited=ROOT/'ui-modern'; files={}
    for folder in restyle.DIRS:
        for p in sorted((edited/folder).glob('*')):
            if p.is_file() and p.suffix.lower() in ('.menu','.h','.txt'):
                files[p.relative_to(edited).as_posix()]=p.read_bytes()
    differences=[]
    if variant=='game':
        files.pop('ui/tce_omnibot.menu')
        for name in ('menus.txt','menus_gt2.txt','menus_gt5.txt','menus_gt7.txt'):
            key='ui/'+name
            files[key]=re.sub(rb'(?m)^\s*loadMenu\s*\{\s*"ui/tce_omnibot.menu"\s*\}\s*\n',b'',files[key])
        # Preserve already shipped normal-game HUD controls and editor boundary.
        # Their actual contents are styled by the same transformation as pak.
        for name in ('ingame_main.menu','options_controls.menu','options_customise_hud.menu'):
            source=ROOT/'src/assets/ui'/name
            files['ui/'+name]=restyle.menu(restyle.read(source),Path('ui')/name).encode('latin1')
            differences.append({'path':'ui/'+name,'content_source':source.relative_to(ROOT).as_posix(),
                                'reason':'Existing normal Game functionality/public editor boundary retained.'})
    for p in sorted((edited/'assets').rglob('*')):
        if p.is_file():files[p.relative_to(edited/'assets').as_posix()]=p.read_bytes()
    aliases=json.loads(files['tce2_ui/SOURCE.json'])['native_aliases']
    for target,source in aliases.items():files[target]=files[source]
    # Native scoreboard/connect renderers are dormant without this exact marker.
    files['tce2_ui/runtime.cfg']=b'TCE2_MODERN_UI 1\n'
    assert all(not Path(n).suffix.lower() in ('.dll','.so','.exe','.bsp','.way','.gm') for n in files)
    assert all(not n.startswith('/') and '..' not in Path(n).parts for n in files)
    if variant=='game':
        assert 'ui/tce_omnibot.menu' not in files
        for key in ('ui/menus.txt','ui/menus_gt2.txt','ui/menus_gt5.txt','ui/menus_gt7.txt','ui/ingame_main.menu'):
            assert b'tce_omnibot' not in files[key]
    manifest={'schema':1,'theme':'TCE2 Modern UI','version':'1.0.2','variant':variant,
              'scope':'Optional theme resources; native scoreboard/connect require matching modern client modules. No gameplay changes.',
              'all_original_menu_sources':69,'normal_game_content_sources':differences,
              'files':{n:{'bytes':len(d),'sha256':sha(d)} for n,d in sorted(files.items())}}
    files['tce2_ui/package.json']=(json.dumps(manifest,indent=2)+'\n').encode()
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
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'dist/TCE2-Modern-UI');args=p.parse_args()
    outputs={v:build(v,args.out/('Game' if v=='game' else 'Editor')/('zz_tce2-1.0.2-modern-ui'+('-editor' if v=='editor' else '')+'.pk3')) for v in ('game','editor')}
    (args.out/'packages.json').write_text(json.dumps(outputs,indent=2)+'\n')
    print(json.dumps(outputs))


if __name__=='__main__':main()
