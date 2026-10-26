#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_CheckPlayerstateEvents(playerState_t*,playerState_t*);
extern void CG_CheckLocalSounds(playerState_t*,playerState_t*);
static byte*base;static int n,events[8][8],client;
static void oldevent(byte*c,float*p){int*r=events[n++];r[0]=1;r[1]=c==base+0x407e46c;r[2]=*(int*)(c+0xb8);r[3]=*(int*)(c+0xbc);r[4]=p==(float*)(c+0x710);r[5]=*(int*)(base+0x407ef14);r[6]=!r[1]&&c!=base+0x488d800+client*0xa94;}
static void newevent(centity_t*c,vec3_t p){int*r=events[n++];r[0]=1;r[1]=c==&cg.predictedPlayerEntity;r[2]=c->currentState.event;r[3]=c->currentState.eventParm;r[4]=p==c->lerpOrigin;r[5]=cg.eventSequence;r[6]=!r[1]&&c!=&cg_entities[client];}
static void oldpain(byte*c,int h){int*r=events[n++];r[0]=2;r[1]=c==base+0x407e46c;r[2]=h;r[3]=*(int*)(base+0x4080f14);}
static void newpain(centity_t*c,int h,qboolean crouch){int*r=events[n++];r[0]=2;r[1]=c==&cg.predictedPlayerEntity;r[2]=h;r[3]=cg.painTime;r[4]=crouch;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte a[0x700],b[0x700];playerState_t ps,ops;void(__cdecl*original[2])(void*,void*);int k,op,j,count,copy[8][8];
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;base=(byte*)m;original[0]=(void*)(base+0x56ee0);original[1]=(void*)(base+0x56fc0);hook(base+0x37d40,oldevent);hook(CG_EntityEvent,newevent);hook(base+0x35c80,oldpain);hook(CG_PainEvent,newpain);
 for(k=0;k<8192;k++)for(op=0;op<2;op++){
  memset(a,0,sizeof(a));memset(b,0,sizeof(b));memset(&ps,0,sizeof(ps));memset(&ops,0,sizeof(ops));client=k%64;ps.clientNum=*(int*)(a+0xa0)=client;
  ps.externalEvent=*(int*)(a+0x94)=k%5;ops.externalEvent=*(int*)(b+0x94)=(k/5)%5;ps.externalEventParm=*(int*)(a+0x98)=k-77;ps.eventSequence=*(int*)(a+0x6c)=k%49-8;ops.eventSequence=*(int*)(b+0x6c)=ps.eventSequence+(k/49)%13-6;
  for(j=0;j<4;j++){ps.events[j]=*(int*)(a+0x70+j*4)=(k+j)%11;ops.events[j]=*(int*)(b+0x70+j*4)=(k&(1<<j))?ps.events[j]:99;ps.eventParms[j]=*(int*)(a+0x80+j*4)=k+j*17;}
  ps.stats[STAT_HEALTH]=*(int*)(a+0xd0)=k%105-4;ops.stats[STAT_HEALTH]=*(int*)(b+0xd0)=ps.stats[STAT_HEALTH]+(k/105)%7-3;
  for(j=2;j<=4;j++){ops.holdable[j]=*(int*)(b+0x390+j*4)=j*71;ps.holdable[j]=*(int*)(a+0x390+j*4)=ops.holdable[j]+(j==2?60:30)+(k/(j+1))%5-2;}
  cg.eventSequence=*(int*)(base+0x407ef14)=37;cg.time=*(int*)(base+0x407dea4)=k*17-1000;cg.painTime=*(int*)(base+0x4080f14)=81;cg.tceHeartbeatNext=*(int*)(base+0x40a4960)=82;cg.tceHeartbeatUntil=*(int*)(base+0x40a4964)=83;
  cg.predictedPlayerEntity.currentState.event=*(int*)(base+0x407e524)=91;cg.predictedPlayerEntity.currentState.eventParm=*(int*)(base+0x407e528)=92;cg_entities[client].currentState.event=*(int*)(base+0x488d8b8+client*0xa94)=93;cg_entities[client].currentState.eventParm=*(int*)(base+0x488d8bc+client*0xa94)=94;
  for(j=0;j<16;j++)cg.predictableEvents[j]=*(int*)(base+0x407ef18+j*4)=-777;
  n=0;memset(events,0,sizeof(events));original[op](a,b);count=n;memcpy(copy,events,sizeof(copy));n=0;memset(events,0,sizeof(events));if(op)CG_CheckLocalSounds(&ps,&ops);else CG_CheckPlayerstateEvents(&ps,&ops);
  if(count!=n||memcmp(copy,events,sizeof(copy))||cg.eventSequence!=*(int*)(base+0x407ef14)||memcmp(cg.predictableEvents,base+0x407ef18,64)||cg.painTime!=*(int*)(base+0x4080f14)||cg.tceHeartbeatNext!=*(int*)(base+0x40a4960)||cg.tceHeartbeatUntil!=*(int*)(base+0x40a4964)||cg.predictedPlayerEntity.currentState.event!=*(int*)(base+0x407e524)||cg.predictedPlayerEntity.currentState.eventParm!=*(int*)(base+0x407e528)||cg_entities[client].currentState.event!=*(int*)(base+0x488d8b8+client*0xa94)||cg_entities[client].currentState.eventParm!=*(int*)(base+0x488d8bc+client*0xa94)){printf("playerstate callbacks case%d op%d calls%d/%d\n",k,op,count,n);return 1;}
 }puts("16384 whole playerstate callback comparisons passed (8192 each)");return 0;}
