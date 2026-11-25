"""Build the mod's arm-only high-ready MDX from the installed ET skeleton.

No engine code is imported. Two-bone IK keeps limb lengths, while a rigid hand
rotation retains each finger grip. Chest/head/leg channels remain byte-identical
with stand_rifle. This script requires the original installed asset archives.
"""
import argparse,hashlib,math,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'modernize'))
import pose_eval_fixtures as o

def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def rotation(a,b):
 a=o.normalize(a);b=o.normalize(b);v=cross(a,b);c=dot(a,b)
 if c<-.999999:raise ValueError('ambiguous antipodal limb rotation')
 k=((0,-v[2],v[1]),(v[2],0,-v[0]),(-v[1],v[0],0));kk=o.matmul(k,k)
 return tuple(tuple((i==j)+k[i][j]+kk[i][j]/(1+c) for j in range(3)) for i in range(3))
def euler(m):
 p=math.atan2(-m[0][2],math.hypot(m[0][0],m[0][1]));y=math.atan2(m[0][1],m[0][0]);r=math.atan2(m[1][2],m[2][2])
 assert max(abs(x-y) for aa,bb in zip(o.angles_axis((p,y,r)),m) for x,y in zip(aa,bb))<.001
 return p,y,r
def short(a):return (round(a*65536/math.tau)+32768)%65536-32768
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--mod-pk3',type=Path,required=True);p.add_argument('--base-pk3',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 assets=o.Assets(a.mod_pk3,a.base_pk3);raw=assets.read('animations/human/base/body.mdx');model=o.Skeleton(raw)
 frame=next(int(w[1]) for line in assets.read("animations/human/base/body.aninc").decode().splitlines() if (w:=line.split()) and w[0]=="stand_rifle");pair=o.FramePair(model,frame,model,frame);maths=o.Arithmetic(4096,True,True,True,False)
 identity=((1.,0,0),(0,1.,0),(0,0,1.));positions,matrices=maths.evaluate(pair,pair,identity)
 mesh=o.Mesh(assets.read('models/players/temperate/allied/soldier/body.mdm'),model)
 weapon,axes=o.tag_pose(mesh,(positions,matrices),'tag_weapon');target=(math.cos(math.radians(78)),0,math.sin(math.radians(78)))
 hand_rotation=rotation(axes[0],target);updated={};directions={};new_positions=list(positions)
 def arm(upper,fore,hand,target_pos):
  shoulder=positions[upper];toward=o.sub(target_pos,shoulder);distance=o.length(toward);u=o.normalize(toward)
  l1=model.bones[fore].distance;l2=model.bones[hand].distance;assert abs(l1-l2)<distance<l1+l2
  along=(l1*l1-l2*l2+distance*distance)/(2*distance);height=math.sqrt(l1*l1-along*along)
  bend=o.sub(positions[fore],shoulder);bend=o.normalize(o.sub(bend,o.scale(u,dot(bend,u))))
  elbow=o.add(shoulder,o.add(o.scale(u,along),o.scale(bend,height)))
  ru=rotation(o.sub(positions[fore],shoulder),o.sub(elbow,shoulder));rf=rotation(o.sub(positions[hand],positions[fore]),o.sub(target_pos,elbow))
  updated[upper]=o.matmul(ru,matrices[upper]);updated[fore]=o.matmul(rf,matrices[fore]);updated[hand]=o.matmul(hand_rotation,matrices[hand])
  directions[fore]=o.normalize(o.sub(elbow,shoulder));directions[hand]=o.normalize(o.sub(target_pos,elbow))
  new_positions[fore]=elbow;new_positions[hand]=target_pos
  for i,b in enumerate(model.bones):
   at=b.parent
   while at>=0 and at!=hand:at=model.bones[at].parent
   if at!=hand:continue
   updated[i]=o.matmul(hand_rotation,matrices[i]);v=o.matvec(hand_rotation,o.sub(positions[i],positions[b.parent]));directions[i]=o.normalize(v)
   new_positions[i]=o.add(new_positions[b.parent],v)
 arm(27,28,29,(4.,-8.,31.))
 new_weapon=o.add(new_positions[29],o.matvec(updated[29],mesh.tags['tag_weapon'][1]))
 left_target=o.add(new_weapon,(2.,3.,11.));arm(8,9,10,left_target)
 data=bytearray(raw[model.frame_offset+frame*model.stride:model.frame_offset+(frame+1)*model.stride])
 for i,m in updated.items():struct.pack_into('<3h',data,52+i*12,*map(short,euler(m)))
 for i,v in directions.items():struct.pack_into('<2h',data,52+i*12+8,short(math.atan2(-v[2],math.hypot(v[0],v[1]))),short(math.atan2(v[1],v[0])))
 struct.pack_into('<6f',data,0,-64,-64,-48,64,64,96);struct.pack_into('<f',data,36,150)
 bone_offset=struct.unpack_from('<i',raw,84)[0];metadata=raw[bone_offset:bone_offset+model.count*80];header=bytearray(raw[:96])
 struct.pack_into('<6i',header,72,1,model.count,96,96+len(data),model.torso_parent,96+len(data)+len(metadata))
 result=bytes(header)+data+metadata;ready=o.Skeleton(result);model.compatible(ready)
 posed=maths.evaluate(pair,o.FramePair(ready,0,ready,0),identity);new_weapon,new_axes=o.tag_pose(mesh,posed,'tag_weapon')
 assert dot(o.normalize(new_axes[0]),target)>.999
 for name in ['tag_head','tag_chest','tag_torso']:
  before,_=o.tag_pose(mesh,(positions,matrices),name);after,_=o.tag_pose(mesh,posed,name);assert o.length(o.sub(before,after))<.001
 a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(result)
 print('Source MDX SHA256:',hashlib.sha256(raw).hexdigest());print('High-ready MDX SHA256:',hashlib.sha256(result).hexdigest());print('Weapon grip:',new_weapon,'barrel axis:',new_axes[0]);print('Head/chest/torso unchanged; limb lengths preserved.')
if __name__=='__main__':main()
