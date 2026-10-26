#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void WolfRevivePushEnt(gentity_t*,gentity_t*);
static byte *base,*raw,rc[2][0x16b0];static gclient_t clients[2];
static int side,variant,calls,bad,slot=17;
typedef struct {vec3_t start,mins,maxs,end;int pass,mask;} tracecall;
typedef struct {int result,calls;tracecall trace[9];vec3_t norm,vel[4];} result;
static result out[2];
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void trace(trace_t*t,const vec3_t start,const vec3_t mins,const vec3_t maxs,const vec3_t end,int pass,int mask){tracecall*c;if(calls>=9){bad=1;return;}c=&out[side].trace[calls];memcpy(c->start,start,12);memcpy(c->mins,mins,12);memcpy(c->maxs,maxs,12);memcpy(c->end,end,12);c->pass=pass;c->mask=mask;memset(t,0,sizeof(*t));t->fraction=(calls==variant%10&&(variant/10)%2)?1.0f:0.25f;t->entityNum=(calls==variant%10&&!((variant/10)%2))?slot:ENTITYNUM_WORLD;calls++;}
static void normalize(vec3_t v){memcpy(out[side].norm,v,12);calls++;v[0]=((variant%17)-8)*0.0625f;v[1]=((variant%13)-6)*0.125f;v[2]=0;}
int main(int argc,char**argv){HMODULE dll;int(__cdecl*can)(void*,vec3_t);void(__cdecl*push)(void*,void*);int phase,v,i,j,total=0;vec3_t origin={17,-31,54};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;raw=base+0x226e2a0+slot*0x57c;can=(void*)(base+0x5b0f0);push=(void*)(base+0x47150);
 if(!hook(base+0x8ae80,trace)||!hook(trap_Trace,trace)||!hook(base+0xa0b00,normalize)||!hook(VectorNormalizeFast,normalize))return 2;
 for(phase=0;phase<2;phase++)for(v=0;v<32768;v++)for(side=0;side<2;side++){gentity_t*e=&g_entities[slot];variant=v;calls=bad=0;memset(&out[side],0,sizeof(result));memset(raw,0,2*0x57c);memset(e,0,2*sizeof(*e));memset(rc,0,sizeof(rc));memset(clients,0,sizeof(clients));g_newbbox.integer=*(int*)(base+0x7e208c)=(v/20)%2;
  for(j=0;j<2;j++){byte*p=raw+j*0x57c;gentity_t*n=e+j;n->client=(j||(v/40)%2)?&clients[j]:NULL;*(void**)(p+0x190)=(j||(v/40)%2)?rc[j]:NULL;n->s.eType=*(int*)(p+4)=v&1?ET_MOVER:ET_PLAYER;for(i=0;i<3;i++){n->r.currentOrigin[i]=*(float*)(p+0x168+i*4)=(v/80)%3?((v+i*7+j*3)%127-63)*0.25f:0;n->r.absmin[i]=*(float*)(p+0x150+i*4)=((v+i*3)%101-50)*0.25f;n->r.absmax[i]=*(float*)(p+0x15c+i*4)=((v+i*5)%67+1)*0.5f;clients[j].ps.mins[i]=*(float*)(rc[j]+0x3dc+i*4)=-10.25f-i*3;clients[j].ps.maxs[i]=*(float*)(rc[j]+0x3e8+i*4)=7.75f+i*5;n->s.pos.trDelta[i]=*(float*)(p+0x24+i*4)=((v+i+j)%257-128)*0.125f;clients[j].ps.velocity[i]=*(float*)(rc[j]+0x20+i*4)=((v+i+j)%191-95)*0.25f;}}
  if(!phase)out[side].result=side?CanDamage(e,origin):can(raw,origin);else{if(side)WolfRevivePushEnt(e,e+1);else push(raw,raw+0x57c);for(j=0;j<2;j++){memcpy(out[side].vel[j*2],side?(void*)(e+j)->s.pos.trDelta:(void*)(raw+j*0x57c+0x24),12);memcpy(out[side].vel[j*2+1],side?(void*)clients[j].ps.velocity:(void*)(rc[j]+0x20),12);}}
  out[side].calls=calls;if(bad)return 1;if(side){if(memcmp(out,out+1,sizeof(result))){printf("Visibility/revive mismatch phase%d case%d calls%d/%d result%d/%d\n",phase,v,out[0].calls,out[1].calls,out[0].result,out[1].result);for(i=0;i<sizeof(result);i++)if(((byte*)out)[i]!=((byte*)(out+1))[i]){printf("byte%x %02x/%02x\n",i,((byte*)out)[i],((byte*)(out+1))[i]);break;}return 1;}total++;}
 }
 printf("PASS CanDamage/WolfRevivePushEnt whole %d cases, 32768 each; collision/normalization boundaries controlled\n",total);return 0;
}
