#!/usr/bin/env python3
"""Independent local-asset full-skin oracle; no game/engine code is imported.

Requires original local PK3s and the six existing test-only renderer captures.
Generates bounded binary expectations and compact MDX input, not public assets.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
import pose_eval_fixtures as o
import pose_skin_bound_inputs as bound

ROOT=Path(__file__).resolve().parents[2]
IDENTITY=((1.,0.,0.),(0.,1.,0.),(0.,0.,1.))

class Head:
    def __init__(self,data):
        self.data=data; r=o.Reader(data)
        o.require(r.span(0,4)==b'IDP3' and r.unpack('<i',4)[0]==15,'not MD3 v15')
        _,self.frames,tags,count,_,frames,tag_offset,at,end=r.unpack('<9i',72)
        o.require(end==len(data) and 0<self.frames<=8192 and 0<=count<=32 and 0<=tags<=128,'MD3 bounds')
        r.span(frames,self.frames*56);r.span(tag_offset,self.frames*tags*112)
        self.surfaces=[];self.triangles=[];total=0
        for _ in range(count):
            o.require(r.span(at,4)==b'IDP3','MD3 surface magic')
            _,frames,shaders,verts,tris,to,so,st,xyz,size=r.unpack('<10i',at+68)
            o.require(frames==self.frames and 0<=verts<=4096 and 0<=tris<=8192 and 0<=shaders<=256,'MD3 counts')
            r.span(at,size)
            for offset,amount in ((to,tris*12),(so,shaders*68),(st,verts*8),(xyz,frames*verts*8)):
                o.require(offset>=108 and offset+amount<=size,'MD3 array bounds')
            triangles=list(zip(*[iter(r.unpack('<'+str(tris*3)+'i',at+to))]*3))
            o.require(all(0<=index<verts for triangle in triangles for index in triangle),'MD3 index')
            self.surfaces.append((r.name(at+4),verts,at+xyz))
            self.triangles.extend(tuple(total+index for index in tri) for tri in triangles)
            total+=verts;at+=size
        self.vertex_count=total
    def vertices(self,current,old,back):
        o.require(0<=current<self.frames and 0<=old<self.frames and 0<=back<=1,'MD3 frame tuple')
        points=[]
        for _,count,offset in self.surfaces:
            for index in range(count):
                a=struct.unpack_from('<3h',self.data,offset+8*(current*count+index))
                b=struct.unpack_from('<3h',self.data,offset+8*(old*count+index))
                points.append(tuple(((1-back)*x+back*y)/64 for x,y in zip(a,b)))
        return points

def leg_bone(skeleton,index):
    while index>=0:
        if skeleton.bones[index].name in ('Bip01 L Thigh','Bip01 R Thigh'):
            return True
        index=skeleton.bones[index].parent
    return False

def topology(mesh,skeleton):
    triangles=[];regions=[];first=0
    for surface in mesh.surfaces:
        for tri in surface['triangles']:
            lower=upper=0.
            for index in tri:
                for bone,weight,_ in surface['vertices'][index]:
                    if leg_bone(skeleton,bone):lower+=weight
                    else:upper+=weight
            triangles.append(tuple(first+index for index in tri))
            regions.append(2 if surface['name'].startswith('l_') and lower>upper else 1)
        first+=len(surface['vertices'])
    return triangles,regions

def skin(mesh,bones,origin,axis):
    points=[]
    for surface in mesh.surfaces:
        for vertex in surface['vertices']:
            point=(0.,0.,0.)
            for bone,weight,offset in vertex:
                point=o.add(point,o.scale(o.add(bones[0][bone],o.matvec(bones[1][bone],offset)),weight))
            points.append(tuple(o.f32(value) for value in o.add(origin,o.basisvec(axis,point))))
    return points

def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])

def ray(points,triangles,regions,start,end,limit):
    delta=o.sub(end,start);fractions=[limit]*3;mask=0;first=limit;region=-1
    for indices,part in zip(triangles,regions):
        a,b,c=[points[index] for index in indices]
        edge1,edge2=o.sub(b,a),o.sub(c,a);h=cross(delta,edge2);det=dot(edge1,h)
        if det==0:continue
        s=o.sub(start,a);u=dot(s,h)/det
        if not -1e-10<=u<=1+1e-10:continue
        q=cross(s,edge1);v=dot(delta,q)/det
        if v < -1e-10 or u+v>1+1e-10:continue
        t=dot(edge2,q)/det
        if not 0<=t<=limit:continue
        if not(mask & (1<<part)) or t<fractions[part]:fractions[part]=t
        if not mask or t<first or (t==first and part<region):first=t;region=part
        mask|=1<<part
    return mask,region,first,fractions

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mod-pk3',type=Path,default=ROOT/'build/navgen-overkill/tce2/pak2.pk3')
    parser.add_argument('--base-pk3',type=Path,default=ROOT/'build/navgen-overkill/etmain/pak0.pk3')
    parser.add_argument('--captures',type=Path,default=ROOT/'src/reconstruction/modernization/runtime-bugfix-20261026')
    parser.add_argument('--bound-capture',type=Path,action='append',default=[],
                        help='Append explicitly bound normal-CGame read-only input tuples')
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    assets=o.Assets(args.mod_pk3,args.base_pk3);arithmetic=o.Arithmetic(4096,True,True,True,False)
    samples=[];capture_hashes={}
    for stem in o.CAPTURES:
        data=(args.captures/(stem+'.jsonl')).read_bytes();capture_hashes[stem]=o.digest(data)
        for original_id,sample in sorted(o.read_records(data.decode()).items()):
            character=assets.character(sample['character'])
            mapping={handle:character['animations'][animation.lower()] for handle,animation in sample['models'].items()}
            v=sample['frames']
            paths=[mapping[v[i]] for i in (0,2,5,7)];frames=[v[i] for i in (1,3,6,8)]
            samples.append({'name':stem+':'+str(original_id),'mesh':character['mesh'],'models':paths,'frames':frames,
                            'back':[v[4],v[9]],'origin':sample['body_origin'],'axis':sample['body_axis'],'torso':sample['torso_axis'],
                            'head_origin':sample['head_origin'],'head_axis':sample['head_axis'],'face':sample['head'][1:4]})
    o.require(len(samples)==176,'six captured files/176 tuples required')
    for clip,frame in [('climb_up',2991),('climb_down',3096),('climb_start',3058),('climb_dis',3018)]:
        model=assets.skeleton('animations/human/base/body.mdx');pair=o.FramePair(model,frame,model,frame)
        mesh=o.Mesh(assets.read(samples[0]['mesh']),model);bones=arithmetic.evaluate(pair,pair,IDENTITY)
        origin,axis=o.tag_pose(mesh,bones,'tag_head')
        samples.append({'name':clip+':'+str(frame),'mesh':samples[0]['mesh'],'models':[model.name]*4,'frames':[frame]*4,
                        'back':[0,0],'origin':[0,0,0],'axis':IDENTITY,'torso':IDENTITY,
                        'head_origin':origin,'head_axis':axis,'face':[148,148,0]})
    bound_sources=[]
    for path in args.bound_capture:
        raw=path.read_bytes();document=json.loads(raw)
        added,summary=bound.samples(document,assets,path.stem,samples[0]['mesh'],'models/players/hud/head.md3')
        samples.extend(added)
        bound_sources.append({'file':path.name,'sha256':o.digest(raw),'qagame_sha256':document['qagame_sha256'],
                              'cgame_sha256':document['cgame_sha256'],'count':len(added),'phases':summary})
    names=sorted({name for sample in samples for name in sample['models']});remaps={};model_info=[]
    for index,name in enumerate(names):
        frames={frame for sample in samples for model,frame in zip(sample['models'],sample['frames']) if model==name}
        raw,original,remap=o.compact_mdx(assets.read(name),frames,o.pose)
        (args.out/f'model-{index}.mdx').write_bytes(raw);remaps[name]=remap
        model_info.append({'name':name,'file':f'model-{index}.mdx','sha256':o.digest(raw),'frames':original})
    mesh_name=samples[0]['mesh'];raw_mesh=assets.read(mesh_name);(args.out/'body.mdm').write_bytes(raw_mesh)
    raw_head=assets.read('models/players/hud/head.md3');(args.out/'head.md3').write_bytes(raw_head);head=Head(raw_head)
    mesh=o.Mesh(raw_mesh,assets.skeleton(names[0]));body_triangles,body_regions=topology(mesh,assets.skeleton(names[0]))
    body_count=sum(len(surface['vertices']) for surface in mesh.surfaces)
    triangles=body_triangles+[tuple(body_count+i for i in tri) for tri in head.triangles]
    regions=body_regions+[0]*len(head.triangles)
    lines=['/* LOCAL generated exact-surface fixtures, independent Python oracle. */',
           'typedef struct { unsigned int model[4],frame[4]; float back[2],origin[3],axis[3][3],torso[3][3]; tcePoseHeadInput_t head; } SkinCase;',
           'static const SkinCase skinCases[] = {']
    ray_lines=[];all_points=bytearray();ray_count=0
    for case,sample in enumerate(samples):
        pairs=[o.FramePair(assets.skeleton(sample['models'][2*i]),sample['frames'][2*i],assets.skeleton(sample['models'][2*i+1]),sample['frames'][2*i+1],sample['back'][i]) for i in range(2)]
        bones=arithmetic.evaluate(*pairs,sample['torso'])
        points=skin(mesh,bones,sample['origin'],sample['axis'])
        points.extend(tuple(o.f32(v) for v in o.add(sample['head_origin'],o.basisvec(sample['head_axis'],value))) for value in head.vertices(*sample['face']))
        for point in points:all_points.extend(struct.pack('<3f',*point))
        integer_array=lambda values:'{'+','.join(str(value) for value in values)+'}'
        values=[integer_array([names.index(name) for name in sample['models']]),integer_array([remaps[name][frame] for name,frame in zip(sample['models'],sample['frames'])]),o.c_array(sample['back']),o.c_array(sample['origin']),o.c_array(sample['axis']),o.c_array(sample['torso'])]
        head_values='{'+','.join([str(sample['face'][0]),str(sample['face'][1]),o.number(sample['face'][2]),o.c_array(sample['head_origin']),o.c_array(sample['head_axis'])])+'}'
        lines.append('{'+','.join(values+[head_values])+'},')
        candidates=[]
        # Three anatomically labeled triangle centroids; nearest whole skin is
        # independently tested, so another region may correctly occlude one.
        for part in (0,1,2):
            indices=next(tri for tri,region in zip(triangles,regions) if region==part)
            center=tuple(sum(points[index][axis] for index in indices)/3 for axis in range(3))
            direction=o.normalize(sample['axis'][(case+part)%3])
            candidates.append(('centroid-'+str(part),o.sub(center,o.scale(direction,80)),o.add(center,o.scale(direction,80)),1.))
        # Whole-mesh broadphase and endpoint/cover controls on every pose.
        candidates.append(('far-miss',o.add(sample['origin'],(1000,1000,1000)),o.add(sample['origin'],(1100,1000,1000)),1.))
        extras={176:[(-100,1,-15.5),(100,1,-15.5),(.5,6)],177:[(-100,.5,-17.5),(100,.5,-17.5),(0,1)],
                178:[(-100,1,-11),(100,1,-11),(.5,1.5)],179:[(-100,-.5,-22),(100,-.5,-22),(-1,0)]}
        if case in extras:
            start,end,controls=extras[case];candidates.append(('leg-gap',start,end,1.))
            for y in controls:
                candidates.append(('leg-positive',(start[0],y,start[2]),(end[0],y,end[2]),1.))
        if sample['name']=='pose-32152-1:1':
            a=(1194.006793,2533.915486,70.330196);b=(1127.188783,2345.407254,70.330196)
            candidates.extend([('standing-cover',a,b,.4764393888345786),('standing-open',a,b,1.)])
        if sample['name']=='pose-control-prone-aim:180':
            a=(1075.179846,2503.706023,23.125);b=(1226.959259,2373.463903,23.125)
            candidates.extend([('prone-cover',a,b,.5491715853856642),('prone-open',a,b,1.),
                ('prone-head',(1107.002751,2350.558227,27.165429),(1237.244871,2502.337639,27.165429),1.)])
        if sample.get('bound_readonly'):
            # Generated geometry controls, not captured shots or measured VBOs:
            # a high head triangle with a finite crossing and the same ray
            # stopped just before its nearest surface by a synthetic cover cap.
            choices=sorted(head.triangles,key=lambda tri:sum(points[body_count+i][2] for i in tri),reverse=True)
            for tri in choices:
                a,b,c=[points[body_count+i] for i in tri]
                direction=o.normalize(cross(o.sub(b,a),o.sub(c,a)))
                center=tuple((x+y+z)/3 for x,y,z in zip(a,b,c))
                start=o.sub(center,o.scale(direction,8));end=o.add(center,o.scale(direction,8))
                start=tuple(o.f32(x) for x in start);end=tuple(o.f32(x) for x in end)
                mask,region,first,_=ray(points,triangles,regions,start,end,1.)
                if region==0 and first>.002:
                    candidates.extend([('bound-head-surface',start,end,1.),('bound-head-cover',start,end,first-.001)])
                    break
            else:raise ValueError('no finite bound head/cover control for '+sample['name'])
        for label,start,end,limit in candidates:
            start=tuple(o.f32(x) for x in start);end=tuple(o.f32(x) for x in end)
            mask,region,fraction,per_region=ray(points,triangles,regions,start,end,limit)
            ray_lines.append('{'+','.join([str(case),'"'+label+'"',o.c_array(start),o.c_array(end),format(limit,'.17g'),str(mask),str(region),format(fraction,'.17g'),'{'+','.join(format(x,'.17g') for x in per_region)+'}'])+'},')
            ray_count+=1
    lines+=['};','typedef struct { unsigned int pose; const char *name; float start[3],end[3]; double limit; unsigned int mask; int region; double fraction,perRegion[3]; } SkinRay;',
            'static const SkinRay skinRays[] = {',*ray_lines,'};',
            'static const unsigned char expectedBodyRegions[] = {'+','.join(map(str,body_regions))+'};',
            '#define SKIN_MODEL_COUNT '+str(len(names)),
            '#define SKIN_CASE_COUNT '+str(len(samples)), '#define SKIN_RAY_COUNT '+str(ray_count),
            '#define SKIN_BODY_VERTICES '+str(body_count),'#define SKIN_HEAD_VERTICES '+str(head.vertex_count)]
    lines.append('#define SKIN_BOUND_CASE_COUNT '+str(sum(bool(sample.get('bound_readonly')) for sample in samples)))
    (args.out/'pose_skin_cases.inc').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
    (args.out/'skin_expected.bin').write_bytes(all_points)
    manifest={'kind':'actual weighted asset oracle;176 captured +4 frozen climb plus explicitly bound input-only tuples',
              'captures':capture_hashes,'bound_input_sources':bound_sources,
              'models':model_info,'body':mesh_name,'assets':assets.hashes,'case_names':[sample['name'] for sample in samples],
              'vertices_per_case':len(points),'triangles':len(triangles),'ray_count':ray_count,
              'files':{path.name:{'sha256':o.digest(path.read_bytes()),'bytes':path.stat().st_size} for path in args.out.iterdir() if path.is_file() and path.name!='manifest.json'},
              'oracle_source_sha256':o.digest(Path(__file__).read_bytes()),'bone_oracle_sha256':o.digest(Path(o.__file__).read_bytes())}
    manifest['bound_adapter_sha256']=o.digest(Path(bound.__file__).read_bytes())
    manifest['legacy_vertex_prefix_sha256']=o.digest(all_points[:180*len(points)*12])
    (args.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    assets.close();print(json.dumps({'cases':len(samples),'vertices':len(points),'triangles':len(triangles),'rays':ray_count,'expected_bytes':len(all_points)}))

if __name__=='__main__':main()
