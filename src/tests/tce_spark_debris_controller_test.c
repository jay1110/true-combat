#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_trajectory.h"
#include <windows.h>
#include <stdio.h>
extern void CG_AddSparkElements(localEntity_t*);
extern void CG_AddDebrisElements(localEntity_t*);
extern void CG_FreeLocalEntity(localEntity_t *le);
static byte raw[448];static localEntity_t le;static int run,n,solid;static float frac,speed;static int events[256][24];
static int owner(void*p){return p==(run?(void*)raw:&le);}
static void evaluate(const trajectory_t*t,int time,vec3_t out,qboolean angle,int spline,float scale){int*p=events[n++];p[0]=1;p[1]=time;p[2]=angle;p[3]=spline;memcpy(p+4,&scale,4);p[5]=t==(run?(void*)(raw+32):&le.pos);out[0]=time*.13f;out[1]=-1.7f;out[2]=3.3f;}
static void trace(trace_t*t,const vec3_t start,const vec3_t mins,const vec3_t maxs,const vec3_t end,int skip,int mask){int*p=events[n++];p[0]=2;memcpy(p+1,start,12);memcpy(p+4,end,12);p[7]=mins==NULL;p[8]=maxs==NULL;p[9]=skip;p[10]=mask;memset(t,0,sizeof(*t));t->fraction=frac;t->startsolid=solid;t->endpos[0]=2.37f;t->endpos[1]=-6.21f;t->endpos[2]=.07f;}
static void freele(localEntity_t*l){int*p=events[n++];p[0]=3;p[1]=owner(l);}
static void reflect(localEntity_t*l,trace_t*t){int*p=events[n++];float*d=run?(float*)(raw+56):le.pos.trDelta;p[0]=4;p[1]=owner(l);memcpy(p+2,&t->fraction,4);d[0]=speed;d[1]=d[2]=0;}
static int spark(int h,void*l,int shader,vec3_t pos,int life,float a,float b,float c,float d){int*p=events[n++];p[0]=5;p[1]=h;p[2]=owner(l);p[3]=shader;memcpy(p+4,pos,12);p[7]=life;memcpy(p+8,&a,4);memcpy(p+9,&b,4);memcpy(p+10,&c,4);memcpy(p+11,&d,4);return h+3;}
static int smoke(int h,void*l,int shader,vec3_t pos,int life,float a,float b,float c){return spark(h,l,shader,pos,life,a,b,c,0);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*f[2])(void*);int k,op,j,count,copy[256][24];
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;f[0]=(void*)(b+0x45b10);f[1]=(void*)(b+0x46670);
#define H(off,fn,to) hook(b+off,to);hook(fn,to)
 H(0x43f0,TCE_BG_EvaluateTrajectory,evaluate);H(0x58420,CG_Trace,trace);H(0x448b0,CG_FreeLocalEntity,freele);H(0x44f20,CG_ReflectVelocity,reflect);H(0x65e10,CG_AddSparkJunc,spark);H(0x65f40,CG_AddSmokeJunc,smoke);
 for(k=0;k<8192;k++)for(op=0;op<2;op++){
  memset(raw,0,sizeof(raw));memset(&le,0,sizeof(le));le.startTime=*(int*)(raw+16)=k%37-100;le.endTime=*(int*)(raw+20)=le.startTime+1+k%777;le.lastTrailTime=*(int*)(raw+396)=le.startTime+k%91;le.headJuncIndex=*(int*)(raw+400)=k%11;le.headJuncIndex2=*(int*)(raw+404)=k%13;le.effectFlags=*(int*)(raw+412)=k%4;le.refEntity.customShader=*(int*)(raw+332)=55;
  cg.time=*(int*)(b+0x407dea4)=le.lastTrailTime+k%301-1;cg.frametime=*(int*)(b+0x407dea0)=17;cgs.media.smokeTrailShader=*(int*)(b+0x25884ec)=117;frac=(k%3==0)?1.f:(k%3==1)?.37f:0.f;solid=(k/3)%2;speed=(k/6)%3==0?.5f:2.1f;
  run=1;n=0;memset(events,0,sizeof(events));f[op](raw);count=n;memcpy(copy,events,sizeof(copy));run=0;n=0;memset(events,0,sizeof(events));if(op)CG_AddDebrisElements(&le);else CG_AddSparkElements(&le);
  if(n!=count||memcmp(copy,events,sizeof(copy))||memcmp(le.refEntity.origin,raw+260,12)||le.lastTrailTime!=*(int*)(raw+396)||le.headJuncIndex!=*(int*)(raw+400)||le.headJuncIndex2!=*(int*)(raw+404)||memcmp(&le.pos,raw+32,sizeof(le.pos))){printf("spark/debris case%d op%d calls%d/%d\n",k,op,count,n);for(j=0;j<sizeof(copy)/4;j++)if(((int*)copy)[j]!=((int*)events)[j]){printf("word%d expected%08x actual%08x\n",j,((int*)copy)[j],((int*)events)[j]);break;}return 1;}
 }puts("16384 whole spark/debris comparisons passed (8192 each)");return 0;}
