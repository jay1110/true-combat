#!/usr/bin/env python3
"""Local two-family server-adapter oracle/assets, without engine/game access."""
from pathlib import Path
import argparse,hashlib,json,math,re,struct
import pose_eval_fixtures as o
import pose_skin_fixtures as skin

ROOT=Path(__file__).resolve().parents[2]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mod-pk3',type=Path,default=ROOT/'build/navgen-overkill/tce2/pak2.pk3')
    p.add_argument('--base-pk3',type=Path,default=ROOT/'build/navgen-overkill/etmain/pak0.pk3')
    p.add_argument('--out',type=Path,required=True);args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    assets=o.Assets(args.mod_pk3,args.base_pk3);arithmetic=o.Arithmetic(4096,True,True,True,False)
    paths=sorted({name for archive in assets.archives for name in archive.namelist()
                  if name.startswith('characters/temperate/') and name.endswith('.char')})
    o.require(len(paths)==14,'fourteen shipped character declarations required')
    chars=[]
    for path in paths:
        character=assets.character(path);raw=assets.read(path).decode('ascii')
        script=re.search(r'\banimationScript\s+"([^"\r\n]+)"',raw,re.I)
        o.require(script is not None,'character script missing')
        chars.append([path,character['mesh'],character['hudhead'],character['animationgroup'],script[1]])
        for asset in character['mesh'],character['hudhead']:
            destination=args.out/'assets'/asset;destination.parent.mkdir(parents=True,exist_ok=True)
            if not destination.exists():destination.write_bytes(assets.read(asset))
    # Exact supplied rows; derive their raw fields/model association from assets.
    rows={};full_rows=[]
    text=assets.read(chars[0][3]).decode('ascii')
    for model,block in re.findall(r'\banimfile\s+"([^"]+)"\s*\{(.*?)\}',text,re.S|re.I):
        for include in re.findall(r'#include\s+"([^"]+)"',block):
            for line in assets.read(include).decode('ascii').splitlines():
                match=re.match(r'\s*(\w+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)',line)
                if match:
                    name=match[1];first,n,loops,fps,speed,blend,reverse=map(int,match.groups()[1:]);f=int(1000/fps)
                    value={'model':model,'first':first,'n':n,'loops':loops,'f':f,'i':40 if 'firing' in name else f,'speed':speed,
                           'flags':(1 if name.startswith('climb') else 0)|(2 if 'firing' in name else 0)|(4 if reverse else 0),'movetype':0}
                    full_rows.append(dict(value,name=name))
                    if name in ('climb_up','e_prone_rifle_idle'):
                        rows[name]=dict(value,movetype=1<<(15 if name.startswith('climb') else 20))
    o.require(len(rows)==2,'two actual fixture clips missing')
    animations=list(rows);head=skin.Head(assets.read(chars[0][2]));cases=[];rays=[];gap_count=0;speeds=[];foot_traces=[]
    for character in chars:
        resolved=[]
        for name in animations:
            row=rows[name]
            if row['speed']>=0:resolved.append(row['speed']);continue
            model=assets.skeleton(row['model']);mesh=o.Mesh(assets.read(character[1]),model);previous=None;distance=0;trace=[]
            for frame in range(row['first'],row['first']+row['n']):
                pair=o.FramePair(model,frame,model,frame);bones=arithmetic.evaluate(pair,pair,skin.IDENTITY)
                feet=[]
                for tag in ('tag_footleft','tag_footright'):
                    point,_=o.tag_pose(mesh,bones,tag)
                    feet.append([int(math.ceil(value*64-.5) if value<0 else math.floor(value*64+.5)) for value in point])
                trace.append(feet)
                if previous is not None:
                    ladder=bool(row['flags']&1);contact=(0 if feet[0][0]>feet[1][0] else 1) if ladder else (0 if feet[0][2]<feet[1][2] else 1)
                    axis=2 if ladder else 0;distance+=abs(feet[contact][axis]-previous[contact][axis])
                previous=feet
            denominator=(row['n']-1)*row['f']*64;resolved.append((distance*1000+denominator//2)//denominator)
            foot_traces.append({'character':character[0],'animation':name,'q6':trace,'distance_q6':distance,'denominator':denominator,'speed':resolved[-1]})
        speeds.append(resolved)
    specs=[('allied/engineer',0,False,None,None,False),('allied/engineer',1,False,None,None,False),
           ('allied/soldier',0,False,None,None,False),('allied/soldier',1,False,None,None,False),
           ('allied/engineer',0,True,None,None,False),('allied/soldier',1,True,None,None,False),
           ('allied/vip',0,False,'allied/engineer',None,True),('allied/vip',1,False,'allied/engineer',None,True),
           ('axis/engineer',0,False,'allied/engineer',2,False),('allied/vip',0,False,'allied/engineer',2,True)]
    for character_key,anim,rotated,original_key,disguise_class,carrier in specs:
        ci=next(i for i,row in enumerate(chars) if character_key+'.char' in row[0]);row=rows[animations[anim]]
        original=ci if original_key is None else next(i for i,r in enumerate(chars) if original_key+'.char' in r[0])
        animation_character=(next(i for i,r in enumerate(chars) if 'axis/engineer.char' in r[0]) if disguise_class is not None else original)
        maximum=((row['i']+(row['n']-row['loops'])*row['f'])+row['loops']*row['f'])*65536-1
        phase=(row['i']+(10 if anim==0 else 0)*row['f'])*65536;phase=int(phase)
        low=min(16383,phase*16383//maximum);options={low,min(16383,low+1)}
        decode=lambda index:(maximum*index+8191)//16383
        ordinal=min(options,key=lambda index:(abs(decode(index)-phase),index));elapsed=decode(ordinal)
        progress=max(0,elapsed-row['i']*65536);frame=row['first']+progress//(row['f']*65536)
        following=frame+1
        if following>=row['first']+row['n']:following=row['first']+row['n']-row['loops']
        back=1-(progress%(row['f']*65536))/(row['f']*65536) if frame!=following else 0.
        # Compact sample oldFrame=frame, currentFrame=following.
        model=assets.skeleton(row['model']);pair=o.FramePair(model,following,model,frame,o.f32(back))
        body_axis=tuple(tuple(o.f32(v*1.25) for v in axis) for axis in o.angles_axis((0,90 if rotated else 0,0)))
        torso=tuple(tuple(o.f32(v) for v in axis) for axis in o.angles_axis((0,20 if rotated else 0,0)))
        relative=tuple(tuple(o.f32(v) for v in axis) for axis in o.angles_axis((0,-15 if rotated else 0,0)))
        origin=(120.,200.,100.);mesh=o.Mesh(assets.read(chars[ci][1]),model);bones=arithmetic.evaluate(pair,pair,torso)
        tag_origin,tag_axis=o.tag_pose(mesh,bones,'tag_head')
        head_origin=tuple(o.f32(v) for v in o.add(origin,o.basisvec(body_axis,tag_origin)))
        world_tag=tuple(tuple(o.f32(v) for v in axis) for axis in o.matmul(tag_axis,body_axis))
        head_axis=tuple(tuple(o.f32(v) for v in axis) for axis in o.matmul(relative,world_tag))
        points=skin.skin(mesh,bones,origin,body_axis);body_count=len(points)
        points.extend(tuple(o.f32(v) for v in o.add(head_origin,o.basisvec(head_axis,point))) for point in head.vertices(0,0,0))
        body_triangles,regions=skin.topology(mesh,model);triangles=body_triangles+[tuple(body_count+i for i in tri) for tri in head.triangles];regions += [0]*len(head.triangles)
        case=len(cases);candidates=[]
        for region in range(3):
            for tri in (tri for tri,part in zip(triangles,regions) if part==region):
                a,b,c=[points[i] for i in tri];direction=o.normalize(skin.cross(o.sub(b,a),o.sub(c,a)))
                center=tuple((x+y+z)/3 for x,y,z in zip(a,b,c));start=tuple(o.f32(x) for x in o.sub(center,o.scale(direction,8)));end=tuple(o.f32(x) for x in o.add(center,o.scale(direction,8)))
                hit=skin.ray(points,triangles,regions,start,end,1.)
                if hit[1]==region and hit[2]>.002:
                    candidates.append(('positive-'+str(region),start,end,1.))
                    candidates.append(('cover-'+str(region),start,end,hit[2]-.001));break
            else:raise ValueError('no positive/cover ray for fixture region')
        candidates.append(('far-miss',o.add(origin,(1000,1000,1000)),o.add(origin,(1100,1000,1000)),1.))
        if case==0:
            # A real empty corridor between two adjacent leg-positive rays.
            for z in (-19.375,-21.875,-15.,-25.,-10.,-5.):
                if gap_count:break
                segment=lambda yy:(o.add(origin,(-125,yy,z)),o.add(origin,(125,yy,z)))
                strip=[(y,skin.ray(points,triangles,regions,*segment(y),1.)) for y in [v*.5 for v in range(-24,25)]]
                for at,(y,hit) in enumerate(strip):
                    if hit[0]:continue
                    left=next((yy for yy,h in reversed(strip[:at]) if h[1]==2),None)
                    right=next((yy for yy,h in strip[at+1:] if h[1]==2),None)
                    if left is not None and right is not None:
                        candidates.extend([('leg-gap',*segment(y),1.),('gap-left-positive',*segment(left),1.),('gap-right-positive',*segment(right),1.)]);gap_count+=1;break
            o.require(gap_count==1,'actual fixture leg corridor not found')
        cases.append({'character':ci,'original_character':original,'animation_character':animation_character,
                      'carrier':carrier,'disguise_class':disguise_class,
                      'animation':anim,'mode':1 if anim==0 else 3,'code':0xc000|ordinal,
                      'origin':origin,'body':body_axis,'torso':torso,'head':relative,'head_anchor':head_origin,'elapsed':elapsed})
        for label,start,end,limit in candidates:
            start=tuple(o.f32(x) for x in start);end=tuple(o.f32(x) for x in end)
            hit=skin.ray(points,triangles,regions,start,end,limit)
            rays.append({'pose':case,'label':label,'start':start,'end':end,'limit':limit,'state':2 if hit[0] else 1,'region':hit[1],'fraction':hit[2]})
    lines=['/* LOCAL independent asset/ray expectations; never public assets. */',
           'typedef struct {const char *character,*mesh,*head,*group,*script;} AdapterCharacter;',
           'static const AdapterCharacter adapterCharacters[]={']
    lines += ['{'+','.join(json.dumps(value) for value in row)+'},' for row in chars];lines+=['};',
        'typedef struct {const char *name,*model;int first,n,loops,f,i,speed,flags,movetype;} AdapterAnimation;',
        'static const AdapterAnimation adapterAnimations[]={']
    lines += ['{'+','.join([json.dumps(name),json.dumps(rows[name]['model'])]+[str(rows[name][field]) for field in ('first','n','loops','f','i','speed','flags','movetype')])+'},' for name in animations]
    lines += ['};','static const AdapterAnimation adapterFullAnimations[]={']
    lines += ['{'+','.join([json.dumps(row['name']),json.dumps(row['model'])]+[str(row[field]) for field in ('first','n','loops','f','i','speed','flags','movetype')])+'},' for row in full_rows]
    lines += ['};','typedef struct {unsigned int character,originalCharacter,animationCharacter,animation;int mode,effect1Time,powerups;uint16_t code;uint64_t elapsed;float origin[3],body[3][3],torso[3][3],head[3][3],headAnchor[3];} AdapterCase;', 'static const AdapterCase adapterCases[]={']
    for case in cases:
        powerups='0' if case['disguise_class'] is None else '((1<<PW_OPS_DISGUISED)|('+str(case['disguise_class'])+'<<PW_OPS_CLASS_1))'
        lines.append('{'+','.join([str(case['character']),str(case['original_character']),str(case['animation_character']),str(case['animation']),str(case['mode']),'0x20' if case['carrier'] else '0',powerups,str(case['code']),'UINT64_C('+str(case['elapsed'])+')']+[o.c_array(case[key]) for key in ('origin','body','torso','head','head_anchor')])+'},')
    lines += ['};','typedef struct {unsigned int pose;const char *label;float start[3],end[3];double limit;int state,region;double fraction;} AdapterRay;','static const AdapterRay adapterRays[]={']
    for ray in rays:
        lines.append('{'+','.join([str(ray['pose']),json.dumps(ray['label']),o.c_array(ray['start']),o.c_array(ray['end']),format(ray['limit'],'.17g'),str(ray['state']),str(ray['region']),format(ray['fraction'],'.17g')])+'},')
    lines+=['};','static const int adapterExpectedSpeeds[][2] = {'+','.join('{'+','.join(map(str,row))+'}' for row in speeds)+'};',
            '#define ADAPTER_UNIQUE_MESHES '+str(len({row[1] for row in chars}))]
    (args.out/'pose_adapter_cases.inc').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
    manifest={'characters':chars,'animations':rows,'cases':cases,'rays':rays,'leg_gap_controls':gap_count,'assets':assets.hashes,
              'resolved_speeds':speeds,'foot_oracles':foot_traces,'full_raw_animations':full_rows,
              'source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'files':{str(path.relative_to(args.out)).replace('\\','/'):{'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()} for path in args.out.rglob('*') if path.is_file() and path.name!='manifest.json'}}
    (args.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');assets.close()
    print(json.dumps({'characters':len(chars),'cases':len(cases),'rays':len(rays),'leg_gaps':gap_count}))


if __name__=='__main__':main()
