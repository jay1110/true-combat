#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern vec3_t forward,right,up,muzzleTrace,muzzleEffect;
extern void AddLean(gentity_t*,vec3_t),CalcMuzzlePoints(gentity_t*,int);
extern void CalcMuzzlePoint(gentity_t*,int,vec3_t,vec3_t,vec3_t,vec3_t);
extern void CalcMuzzlePointForActivate(gentity_t*,vec3_t,vec3_t,vec3_t,vec3_t);
static byte*base,rawEntity[0x57c],rawClient[0x16b0];static gentity_t entity;static gclient_t client;static int side,count,bad,v;static vec3_t basis[3];
typedef struct{int k,mask;float vec[3];}event;static event calls[4];
typedef struct{float out[3],globals[15];int count;event calls[4];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void lean(void*p,float*out){int i;event*e=&calls[count++];if(count>4)exit(3);e->k=1;if(p!=(side?(void*)&entity:(void*)rawEntity))bad=1;memcpy(e->vec,out,12);for(i=0;i<3;i++)out[i]+=(v%5-2)*(i+1)*.1875f;}
static void angles(const float*a,float*f,float*r,float*u){event*e=&calls[count++];if(count>4)exit(3);e->k=2;e->mask=(f?1:0)|(r?2:0)|(u?4:0);memcpy(e->vec,a,12);if(f)memcpy(f,basis[0],12);if(r)memcpy(r,basis[1],12);if(u)memcpy(u,basis[2],12);}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*effect)(void*,int,float*,float*,float*,float*);void(__cdecl*activate)(void*,float*,float*,float*,float*);void(__cdecl*both)(void*,int);int phase,i,j,total=0;float out[3];static result a,b;float origins[]={-16777216.f,-1024.25f,-1.9999999f,-1.f,-.000001f,0,.000001f,.99999994f,1,1024.75f,16777216.f};float components[]={-1,-.70710677f,-.33333334f,-.000001f,0,.000001f,.33333334f,.70710677f,1};float*globals[5]={forward,right,up,muzzleTrace,muzzleEffect};int offsets[]={0x66bba0,0x66bbb0,0x66bbd0,0x66bb90,0x66bbc0};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;effect=(void*)(base+0x9f5d0);activate=(void*)(base+0x9f6c0);both=(void*)(base+0x9f780);if(!hook(base+0x9f400,lean)||!hook(AddLean,lean)||!hook(base+0xa0de0,angles)||!hook(AngleVectors,angles))return 2;
 for(phase=0;phase<3;phase++)for(v=0;v<32768;v++)for(side=0;side<2;side++){
  result*r=side?&b:&a;count=bad=0;memset(calls,0,sizeof(calls));memset(&entity,0,sizeof(entity));memset(&client,0,sizeof(client));memset(rawEntity,0,sizeof(rawEntity));memset(rawClient,0,sizeof(rawClient));entity.client=&client;*(void**)(rawEntity+0x190)=rawClient;client.ps.viewheight=*(int*)(rawClient+0xbc)=(v%5)*17-24;
  for(i=0;i<3;i++){entity.r.currentOrigin[i]=*(float*)(rawEntity+0x168+i*4)=origins[(v/5+i*3)%11];client.ps.origin[i]=*(float*)(rawClient+0x14+i*4)=origins[(v/55+i*7)%11];client.ps.viewangles[i]=*(float*)(rawClient+0xb0+i*4)=(float)((v+i*73)%721)-360;out[i]=-999;for(j=0;j<3;j++)basis[j][i]=components[(v/605+j*2+i)%9];}
  for(i=0;i<5;i++)for(j=0;j<3;j++)globals[i][j]=*(float*)(base+offsets[i]+j*4)=1000.f+i*3+j;
  if(!phase){if(side)CalcMuzzlePoint(&entity,v%64,basis[0],v&1?NULL:basis[1],basis[2],out);else effect(rawEntity,v%64,basis[0],v&1?NULL:basis[1],basis[2],out);}else if(phase==1){if(side)CalcMuzzlePointForActivate(&entity,basis[0],v&1?NULL:basis[1],basis[2],out);else activate(rawEntity,basis[0],v&1?NULL:basis[1],basis[2],out);}else{if(side)CalcMuzzlePoints(&entity,v%64);else both(rawEntity,v%64);}
  memset(r,0,sizeof(*r));memcpy(r->out,out,12);for(i=0;i<5;i++)memcpy(r->globals+3*i,side?(void*)globals[i]:(void*)(base+offsets[i]),12);r->count=count;memcpy(r->calls,calls,sizeof(calls));
  if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Server muzzle mismatch phase%d case%d bad%d calls%d/%d\n",phase,v,bad,a.count,b.count);for(i=0;i<3;i++)if(a.out[i]!=b.out[i])printf("out%d %.9g/%.9g\n",i,a.out[i],b.out[i]);for(i=0;i<15;i++)if(a.globals[i]!=b.globals[i])printf("global%d %.9g/%.9g\n",i,a.globals[i],b.globals[i]);for(i=0;i<sizeof(a);i++)if(((byte*)&a)[i]!=((byte*)&b)[i]){printf("byte%x %02x/%02x\n",i,((byte*)&a)[i],((byte*)&b)[i]);break;}return 1;}total++;}
 }
 printf("PASS whole three-function server muzzle chain %d cases (32768 each)\n",total);return 0;
}
