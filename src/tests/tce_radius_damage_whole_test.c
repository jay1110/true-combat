#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte *base,*raw;static gclient_t client;static int side,variant,count,bad;
typedef struct {int kind,a,b,c,d,e,f,g;vec3_t x,y,z,w;} event;
typedef struct {int ret,count,loop[12];event events[128];} result;
static result out[2];
static int idx(void*p){return !p?-1:side?(int)((gentity_t*)p-g_entities):(int)(((byte*)p-raw)/0x57c);}
static event *record(int kind){event*e;if(count>=128)exit(3);e=&out[side].events[count++];e->kind=kind;return e;}
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static int box(const vec3_t mins,const vec3_t maxs,int*list,int max){int i,n=variant%13;event*e=record(1);memcpy(e->x,mins,12);memcpy(e->y,maxs,12);e->a=max;for(i=0;i<n;i++)list[i]=(i*3)%8;return n;}
static int visible(void*t,vec3_t origin){event*e=record(2);e->a=idx(t);memcpy(e->x,origin,12);return ((variant/13+e->a)%3)!=0;}
static int accuracy(void*t,void*a){event*e=record(3);e->a=idx(t);e->b=idx(a);return (variant+e->a)%2;}
static float damage(void*t,void*i,void*a,vec3_t dir,vec3_t point,int amount,int flags,int mod){event*e=record(4);e->a=idx(t);e->b=idx(i);e->c=idx(a);e->d=amount;e->e=flags;e->f=mod;memcpy(e->x,dir,12);memcpy(e->y,point,12);if(variant&1){if(side)((gentity_t*)t)->dmginloop=1;else *(int*)((byte*)t+0x4b8)=1;}return 0;}
static void trace(trace_t*t,const vec3_t start,const vec3_t mins,const vec3_t maxs,const vec3_t end,int pass,int mask){event*e=record(5);memcpy(e->x,start,12);memcpy(e->y,mins,12);memcpy(e->z,maxs,12);memcpy(e->w,end,12);e->a=pass;e->b=mask;memset(t,0,sizeof(*t));t->fraction=(variant/39)%2?0.5f:1.0f;}
int main(int argc,char**argv){HMODULE dll;int(__cdecl*radiusfn)(vec3_t,void*,void*,float,float,void*,int),(__cdecl*selectfn)(vec3_t,void*,void*,float,float,void*,int,int);int phase,v,i,j,total=0,mods[]={4,26,45,46,65};float radii[]={-1,0,0.99f,1,1.01f,15.75f,80.1f,250,600.125f};vec3_t origin;float radius,amount;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;raw=base+0x226e2a0;radiusfn=(void*)(base+0x5b750);selectfn=(void*)(base+0x5bb90);
 if(!hook(base+0x8afe0,box)||!hook(trap_EntitiesInBox,box)||!hook(base+0x5b0f0,visible)||!hook(CanDamage,visible)||!hook(base+0x9f570,accuracy)||!hook(AccuracyHit,accuracy)||!hook(base+0x589c0,damage)||!hook(G_Damage,damage)||!hook(base+0x8ae80,trace)||!hook(trap_Trace,trace))return 2;
 for(phase=0;phase<2;phase++)for(v=0;v<65536;v++)for(side=0;side<2;side++){void*inf,*att,*ignore;variant=v;count=bad=0;memset(&out[side],0,sizeof(result));memset(raw,0,12*0x57c);memset(g_entities,0,12*sizeof(gentity_t));level.num_entities=*(int*)(base+0x7feb8c)=12;g_newbbox.integer=*(int*)(base+0x7e208c)=(v/117)%2;radius=radii[(v/2)%9];amount=(v%997)*0.375f;for(j=0;j<3;j++)origin[j]=((v+j*7)%71-35)*0.125f;
  for(i=0;i<12;i++){byte*p=raw+i*0x57c;gentity_t*e=&g_entities[i];e->takedamage=*(int*)(p+0x2cc)=((i+v/9)%3)!=0;e->dmginloop=*(int*)(p+0x4b8)=77;e->r.bmodel=*(int*)(p+0x130)=(i+v/351)%2;e->client=(i+v/234)%2?&client:NULL;*(void**)(p+0x190)=e->client?(void*)(base+0x200):(void*)0;if(i<7&&(v/702+i)%3==0){e->dmgparent=&g_entities[7];*(void**)(p+0x4b4)=raw+7*0x57c;}for(j=0;j<3;j++){float pos=origin[j]+((v+i*11+j*3)%101-50)*0.25f;e->r.currentOrigin[j]=*(float*)(p+0x168+j*4)=pos;e->r.absmin[j]=*(float*)(p+0x150+j*4)=pos-2.25f-j;e->r.absmax[j]=*(float*)(p+0x15c+j*4)=pos+3.75f+j;}}
  inf=(v&2)?(side?(void*)&g_entities[10]:raw+10*0x57c):NULL;att=(v&4)?(side?(void*)&g_entities[11]:raw+11*0x57c):NULL;ignore=(v/1404)%3?(side?(void*)&g_entities[(v/13)%8]:raw+((v/13)%8)*0x57c):NULL;
  if(!phase)out[side].ret=side?G_RadiusDamage(origin,inf,att,amount,radius,ignore,mods[(v/4)%5]):radiusfn(origin,inf,att,amount,radius,ignore,mods[(v/4)%5]);else out[side].ret=side?etpro_RadiusDamage(origin,inf,att,amount,radius,ignore,mods[(v/4)%5],(v/2808)%2):selectfn(origin,inf,att,amount,radius,ignore,mods[(v/4)%5],(v/2808)%2);
  out[side].count=count;for(i=0;i<12;i++)out[side].loop[i]=side?g_entities[i].dmginloop:*(int*)(raw+i*0x57c+0x4b8);if(side){if(memcmp(out,out+1,sizeof(result))){printf("Radius mismatch phase%d case%d calls%d/%d ret%d/%d\n",phase,v,out[0].count,out[1].count,out[0].ret,out[1].ret);for(i=0;i<128;i++)if(memcmp(&out[0].events[i],&out[1].events[i],sizeof(event))){printf("event%d kind%d/%d target%d/%d damage%d/%d bounds %.9g/%.9g\n",i,out[0].events[i].kind,out[1].events[i].kind,out[0].events[i].a,out[1].events[i].a,out[0].events[i].d,out[1].events[i].d,out[0].events[i].x[0],out[1].events[i].x[0]);break;}return 1;}total++;}
 }
 printf("PASS whole G_RadiusDamage/etpro_RadiusDamage %d cases (65536 each) exact callback payloads/order/state\n",total);return 0;
}
