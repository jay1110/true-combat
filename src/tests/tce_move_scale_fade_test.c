#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_trajectory.h"
#include <windows.h>
#include <stdio.h>
extern void CG_AddMoveScaleFade(localEntity_t *);
extern void CG_AddMoveGravityFade(localEntity_t *);
extern void CG_FreeLocalEntity(localEntity_t *);
static int calls,kind,atTime,angle,spline;static float gravity;static refdef_t view;
static void eval(const trajectory_t *tr,int t,vec3_t out,int a,int s,float g) {atTime=t;angle=a;spline=s;gravity=g;VectorCopy(tr->trBase,out);out[0]+=13;out[2]-=7;++calls;}
static void render(const refEntity_t*r){kind=1;++calls;}
static void release(localEntity_t*r){kind=2;++calls;}
static void hook(void *at,void *fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void fromRaw(localEntity_t*out,byte*in){memset(out,0,sizeof(*out));memcpy(out,in,0x6c);memcpy((byte*)out+0x6c,in+0x70,offsetof(localEntity_t,tceFragment)-0x6c);out->tceGravity=*(float*)(in+0x6c);}
static void toRaw(byte*out,localEntity_t*in){memset(out,0,448);memcpy(out,in,0x6c);memcpy(out+0x70,(byte*)in+0x6c,offsetof(localEntity_t,tceFragment)-0x6c);*(float*)(out+0x6c)=in->tceGravity;}
int main(int argc,char**argv){HMODULE dll;byte*b;int k,i,j,wkind,wcalls;float wg;byte raw[448];localEntity_t le,want;void(__cdecl*original)(void*);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x46a50);
 hook(b+0x43f0,eval);hook(TCE_BG_EvaluateTrajectory,eval);hook(b+0x65290,render);hook(trap_R_AddRefEntityToScene,render);hook(b+0x448b0,release);hook(CG_FreeLocalEntity,release);
 VectorClear(cg.refdef.vieworg);memset(b+0x407f01c,0,12);cg.refdef_current=&view;*(void**)(b+0x40863ac)=&view;
 for(k=0;k<131072;++k){memset(&le,0,sizeof(le));cg.time=*(int*)(b+0x407dea4)=10000+k%300;
 le.startTime=10000;le.endTime=10300;le.fadeInTime=k%3==0?10100:10000;le.lifeRate=1.f/(le.endTime-le.fadeInTime);le.leFlags=k&511;
 le.radius=k%47+.25f;le.color[3]=(k%31)*.03f;le.angles.trBase[0]=13.5f;le.angles.trDelta[0]=-47.25f;le.tceGravity=k%4==0?0:(k%17)*.13f;
 le.pos.trBase[0]=k%59;le.pos.trBase[1]=13;le.pos.trBase[2]=11;le.refEntity.radius=41;le.refEntity.rotation=81;le.refEntity.shaderRGBA[3]=137;
 le.pos.trDelta[0]=31;le.pos.trDelta[1]=-13;le.pos.trDelta[2]=53;
 toRaw(raw,&le);calls=kind=0;original=(void*)(b+(k<65536?0x46a50:0x46f60));original(raw);wkind=kind;wcalls=calls;wg=gravity;fromRaw(&want,raw);
 calls=kind=0;if(k<65536)CG_AddMoveScaleFade(&le);else CG_AddMoveGravityFade(&le);
 if(calls!=wcalls||kind!=wkind||(k<65536&&(gravity!=wg||atTime!=cg.time||angle||spline!=-1))){printf("callback%d\n",k);return 1;}
 if(!_finite(le.refEntity.radius)||!_finite(want.refEntity.radius)||!_finite(le.refEntity.rotation)||!_finite(want.refEntity.rotation)||fabs(le.refEntity.radius-want.refEntity.radius)>.00002||fabs(le.refEntity.rotation-want.refEntity.rotation)>.00002){printf("float%d radius%g/%g rotation%g/%g\n",k,le.refEntity.radius,want.refEntity.radius,le.refEntity.rotation,want.refEntity.rotation);return 1;}
 for(i=0;i<3;++i) { if(!_finite(le.refEntity.origin[i])||!_finite(want.refEntity.origin[i])||fabs(le.refEntity.origin[i]-want.refEntity.origin[i])>.0001) {printf("origin case%d axis%d %g/%g\n",k,i,le.refEntity.origin[i],want.refEntity.origin[i]);return 1;}le.refEntity.origin[i]=want.refEntity.origin[i];}
 le.refEntity.radius=want.refEntity.radius;le.refEntity.rotation=want.refEntity.rotation;
 if(memcmp(&le,&want,sizeof(le))){for(j=0;j<sizeof(le);++j)if(((byte*)&le)[j]!=((byte*)&want)[j]){printf("state%d offset%x %x/%x\n",k,j,((byte*)&le)[j],((byte*)&want)[j]);break;}return 1;}
 }
 printf("131072 complete move-scale-fade and move-gravity-fade original comparisons passed\n");return 0;
}
