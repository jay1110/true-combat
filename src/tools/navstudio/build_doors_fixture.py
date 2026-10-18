"""Compile one isolated door-activation fixture with existing Q3Map2; never launch ET."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zipfile


def main():
    root=Path(__file__).resolve().parents[3];source=Path(__file__).with_name('fixtures')
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',type=Path,default=Path('C:/q3map_2.5.16_win32_x86/q3map3.exe'))
    parser.add_argument('--output',type=Path,default=root/'etmain/all_maps/obj_navstudio_doors.pk3')
    args=parser.parse_args();compiler=args.compiler.resolve();destination=args.output.resolve()
    if not compiler.is_file():parser.error('Existing Q3Map2 executable is required; no installer is run.')
    work=root/'build/navstudio-doorfixture';game=work/'etmain';maps=game/'maps';scripts=game/'scripts'
    maps.mkdir(parents=True,exist_ok=True);scripts.mkdir(exist_ok=True)
    shutil.copy2(source/'obj_navstudio_doors.map',maps/'obj_navstudio_doors.map')
    shutil.copy2(source/'navstudio_doors.shader',scripts/'navstudio_doors.shader')
    (scripts/'shaderlist.txt').write_text('navstudio_doors\n',encoding='ascii')
    textures=game/'textures/navstudio_doors';textures.mkdir(parents=True,exist_ok=True)
    colors={'red':(170,58,58),'green':(55,145,88),'blue':(65,95,175),'wall':(112,115,120),
            'door':(205,205,195),'button':(235,185,45),'locked':(200,75,185)}
    for name,(r,g,b) in colors.items():
        header=bytearray(18);header[2]=2;struct.pack_into('<HH',header,12,16,16);header[16]=24;header[17]=32
        (textures/(name+'.tga')).write_bytes(header+bytes([b,g,r])*256)
    for name in ('obj_navstudio_doors.script','obj_navstudio_doors_gt7.script'):
        shutil.copy2(source/'obj_navstudio_doors_gt7.script',maps/name)
    base=[str(compiler),'-threads','1','-game','et','-fs_basepath',str(work),'-fs_game','etmain']
    leak=maps/'obj_navstudio_doors.lin'
    if leak.exists():leak.unlink()
    commands=[]
    for stage,options,file in [('bsp',['-meta'],maps/'obj_navstudio_doors.map'),
                               ('vis',['-vis','-fast'],maps/'obj_navstudio_doors.bsp'),
                               ('light',['-light','-fast','-samples','2'],maps/'obj_navstudio_doors.bsp')]:
        command=base+options+[str(file)];commands.append(command)
        result=subprocess.run(command,cwd=compiler.parent,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                              timeout=180,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        (work/(stage+'.log')).write_bytes(result.stdout)
        if result.returncode or leak.is_file():
            raise RuntimeError(stage+' failed or leaked; inspect '+str(work/(stage+'.log')))
    sys.path.insert(0,str(root/'src/tools'))
    from tce_bsp_navmesh import read_lumps,parse_entities
    bsp=maps/'obj_navstudio_doors.bsp';data=bsp.read_bytes();lumps=read_lumps(data);lump=lumps[0]
    entities=parse_entities(data[lump['offset']:lump['offset']+lump['length']])
    models=lumps[7];interesting=[]
    for index,entity in enumerate(entities):
        if entity.get('classname') not in ('func_door','func_door_rotating','func_button','target_relay','info_player_deathmatch'):continue
        item=dict(bsp_entity=index,properties=entity)
        if entity.get('model','').startswith('*'):
            values=struct.unpack_from('<6f4i',data,models['offset']+int(entity['model'][1:])*40)
            origin=[float(v) for v in entity.get('origin','0 0 0').split()]
            item['model_mins']=list(values[:3]);item['model_maxs']=list(values[3:6])
            item['world_center']=[origin[i]+(values[i]+values[i+3])*.5 for i in range(3)]
        interesting.append(item)
    target=work/'obj_navstudio_doors.pk3'
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as package:
        for directory in (maps,scripts,textures):
            for file in sorted(directory.iterdir()):
                if file.suffix not in ('.bsp','.script','.shader','.tga'):continue
                if directory==maps and file.name not in ('obj_navstudio_doors.bsp','obj_navstudio_doors.script','obj_navstudio_doors_gt7.script'):continue
                entry=zipfile.ZipInfo(file.relative_to(game).as_posix(),date_time=(1980,1,1,0,0,0));entry.compress_type=zipfile.ZIP_DEFLATED
                package.writestr(entry,file.read_bytes())
    if destination.exists() and destination.read_bytes()!=target.read_bytes():
        previous=json.loads((work/'fixture.json').read_text(encoding='utf8')) if (work/'fixture.json').is_file() else {}
        actual=hashlib.sha256(destination.read_bytes()).hexdigest()
        if Path(previous.get('pk3','')).resolve()!=destination or previous.get('pk3_sha256')!=actual:
            raise RuntimeError('Existing different unowned fixture PK3 was not replaced: '+str(destination))
        backup=work/'evidence'/(destination.name+'.'+actual[:16]+'.previous')
        backup.parent.mkdir(exist_ok=True)
        if backup.exists() and backup.read_bytes()!=destination.read_bytes():raise RuntimeError('Fixture backup conflict.')
        if not backup.exists():shutil.copy2(destination,backup)
        shutil.copy2(target,destination)
    destination.parent.mkdir(parents=True,exist_ok=True)
    if not destination.exists():shutil.copy2(target,destination)
    digest=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    (work/'fixture.json').write_text(json.dumps(dict(map='obj_navstudio_doors',compiler=str(compiler),compiler_sha256=digest(compiler),
        source_sha256=digest(source/'obj_navstudio_doors.map'),bsp_bytes=len(data),bsp_sha256=digest(bsp),pk3=str(destination),
        pk3_bytes=destination.stat().st_size,pk3_sha256=digest(destination),commands=commands,entities=interesting),indent=2),encoding='utf8')
    print('Door fixture built, never launched: '+str(destination)+' ('+str(destination.stat().st_size)+' bytes)')


if __name__=='__main__':main()
