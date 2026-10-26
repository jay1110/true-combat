#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
/* Whole ClientDisconnect: all calls return normally, no interior early cutoff. */
static byte *base, rawClients[4][0x16b0], rawDrops[3][0x57c], rawPopups[2][0x57c];
static gclient_t clients[4];static gentity_t drops[3],popups[2];
static int side,variant,number,count,ndrop,npopup,ntrace,randomValue,bad;
static int nodes[3],singleIndex[2];
typedef struct{int kind,a,b,c;float x[12];char text[128];}call;
static call calls[96];
typedef struct{int count,ndrop,npopup;call calls[96];int state[48];int drop[3][9],popup[2][3];char classname[32];}result;
static void note(int kind,int a,int b,int c,const char *s){call *p;if(count>=96)exit(3);p=&calls[count++];memset(p,0,sizeof(*p));p->kind=kind;p->a=a;p->b=b;p->c=c;if(s)Q_strncpyz(p->text,s,sizeof(p->text));}
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static int idx(void*p){return side?(int)((gentity_t*)p-g_entities):(int)(((byte*)p-base-0x226e2a0)/0x57c);}
static int team(void*p){return side?(int)((mapEntityData_Team_t*)p-mapEntityData):(int)(((byte*)p-base-0x76c280)/0xc034);}
static void fireteam(int n,int a,int b){note(1,n,a,b,NULL);}static void ignore(int n){note(2,n,0,0,NULL);}static void tank(void*p,int f){note(3,idx(p),f,0,NULL);}
static void stopFollow(void*p){int i=idx(p);note(4,i,0,0,NULL);if(variant&1){if(side)clients[i].sess.spectatorClient=-1;else*(int*)(rawClients[i]+0xb3c)=-1;}}
static void cycle(void*p,int dir){note(5,idx(p),dir,0,NULL);}
static void send(int n,const char*s){note(6,n,0,0,s);}static void mines(void*p){note(7,idx(p),0,0,NULL);}static void fade(void*p,int m){note(8,idx(p),m,0,NULL);}
static void*findMap(void*p,int n){note(9,team(p),n,0,NULL);return variant&2?nodes:NULL;}
static void*findSingle(void*p,void*prev,int n,int owner){int t=team(p),next=singleIndex[t]++;note(10,t,n,owner,NULL);if(prev!=NULL&&prev!=&nodes[1]&&prev!=&nodes[2])bad=1;return next<(variant>>2&3)%3?&nodes[next+1]:NULL;}
static void freeMap(void*p,void*n){note(11,team(p),(int*)n-nodes,0,NULL);}
static void*lookupClass(const char*s){int n=!strcmp(s,"ammo_VIP")?2:1;note(12,n,0,0,s);return (void*)(0x12340000+n*4);}
static void*lookupName(const char*s){int n=!strcmp(s,"Red Flag")?3:!strcmp(s,"Blue Flag")?4:5;note(13,n,0,0,s);if((variant&16)&&n!=5)return NULL;if((variant&32)&&n==5)return NULL;return(void*)(0x12340000+n*4);}
static void angles(const float*a,float*f,float*r,float*u){note(14,0,0,0,NULL);memcpy(calls[count-1].x,a,12);if(r||u)bad=1;f[0]=.25f;f[1]=-.5f;f[2]=.125f;}
static int rnd(void){note(15,randomValue,0,0,NULL);return randomValue;}
static void traceHook(trace_t*t,const float*s,const float*mi,const float*ma,const float*end,int pass,int mask){note(16,pass,mask,0,NULL);memcpy(calls[count-1].x,s,12);memcpy(calls[count-1].x+3,mi,12);memcpy(calls[count-1].x+6,ma,12);memcpy(calls[count-1].x+9,end,12);memset(t,0,sizeof(*t));t->fraction=.5f;t->endpos[0]=11+ntrace;t->endpos[1]=22+ntrace;t->endpos[2]=33+ntrace++;}
static void*launch(void*it,const float*o,const float*v,int owner){int n=ndrop++;if(n>=3)exit(3);note(17,((int)it-0x12340000)/4,owner,0,NULL);memcpy(calls[count-1].x,o,12);memcpy(calls[count-1].x+3,v,12);return side?(void*)&drops[n]:(void*)rawDrops[n];}
static void*popup(int type){int n=npopup++;if(n>=2)exit(3);note(18,type,0,0,NULL);return side?(void*)&popups[n]:(void*)rawPopups[n];}
static char*stats(void*p){note(19,idx(p),0,0,NULL);return "TESTSTATS";}
static void QDECL logger(const char*f,...){va_list a;char s[128];va_start(a,f);Q_vsnprintf(s,sizeof(s),f,a);va_end(a);note(20,0,0,0,s);}
static void unlinker(void*p){note(21,idx(p),0,0,NULL);}static void config(int n,const char*s){note(22,n,0,0,s);}static void ranks(void){note(23,0,0,0,NULL);}static int bot(int n){note(24,n,0,0,NULL);return 0;}static void verify(int n){note(25,n,0,0,NULL);}static void smv(int n){note(26,n,0,0,NULL);}
static void capture(result*r){int i,k=0;gentity_t*e=&g_entities[number];gclient_t*c=&clients[number];byte*q=rawClients[number],*p=base+0x226e2a0+number*0x57c;memset(r,0,sizeof(*r));r->count=count;r->ndrop=ndrop;r->npopup=npopup;memcpy(r->calls,calls,sizeof(calls));
#define C(field,off) r->state[k++]=side?c->field:*(int*)(q+off)
#define E(field,off) r->state[k++]=side?e->field:*(int*)(p+off)
#define L(field,off) r->state[k++]=side?level.field:*(int*)(base+off)
 C(pers.connected,0x5ac);C(sess.sessionTeam,0xb30);C(ps.persistant[PERS_TEAM],0x11c);C(ps.stats[STAT_TCE_WEAPON_FLAGS],0xf0);C(ps.stats[STAT_TCE_FLAGS],0xf4);C(tceBombPossessionOrder,0x1328);C(ps.powerups[PW_REDFLAG],0x168);C(ps.powerups[PW_BLUEFLAG],0x16c);E(s.modelindex,0xa4);E(inuse,0x194);E(active,0x36c);E(s.otherEntityNum2,0x90);r->state[k++]=side?e->message!=NULL:*(int*)(p+0x248)!=0;
 L(tceBombCarrier,0x222391c);L(tceBombDropCount,0x2223934);L(tceBombCarrierCount,0x2223938);L(tceVipCarrier,0x2223948);
 for(i=0;i<4;i++){r->state[k++]=side?clients[i].pers.complaintClient:*(int*)(rawClients[i]+0x66c);r->state[k++]=side?clients[i].pers.complaintEndTime:*(int*)(rawClients[i]+0x670);r->state[k++]=side?clients[i].sess.spectatorClient:*(int*)(rawClients[i]+0xb3c);}
 Q_strncpyz(r->classname,side?(e->classname?e->classname:""):(*(char**)(p+0x1a4)?*(char**)(p+0x1a4):""),sizeof(r->classname));
 for(i=0;i<ndrop;i++){e=&drops[i];p=rawDrops[i];k=0;
#define D(field,off) r->drop[i][k++]=side?e->field:*(int*)(p+off)
 D(s.otherEntityNum,0x8c);D(s.otherEntityNum2,0x90);D(s.modelindex2,0xa8);D(s.density,0xf4);D(s.effect2Time,0x110);D(s.effect3Time,0x114);D(r.svFlags,0x128);r->drop[i][k++]=side?e->message!=NULL:*(int*)(p+0x248)!=0;}
 for(i=0;i<npopup;i++){p=rawPopups[i];r->popup[i][0]=side?popups[i].s.effect2Time:*(int*)(p+0x110);r->popup[i][1]=side?popups[i].s.effect3Time:*(int*)(p+0x114);r->popup[i][2]=side?popups[i].s.density:*(int*)(p+0xf4);}
}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(int);result a,b;int v,mode,n=0,i,k;int gametypes[]={2,5,7};float pitches[]={-60,-30,0,30,60};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x4d840);
#define H(rva,fn,repl) if(!hook(base+rva,repl)||!hook(fn,repl))return 2
 H(0x5c230,G_RemoveClientFromFireteams,fireteam);H(0x4a5d0,G_RemoveFromAllIgnoreLists,ignore);H(0x535f0,G_LeaveTank,tank);H(0x505f0,StopFollowing,stopFollow);H(0x50eb0,Cmd_FollowCycle_f,cycle);H(0x8ad60,trap_SendServerCommand,send);H(0x6c580,G_ExplodeMines,mines);H(0x6c370,G_FadeItems,fade);H(0x90250,G_FindMapEntityData,findMap);H(0x90290,G_FindMapEntityDataSingleClient,findSingle);H(0x90190,G_FreeMapEntityData,freeMap);H(0x2b280,BG_FindItemForClassName,lookupClass);H(0x2b240,BG_FindItem,lookupName);H(0xa0de0,AngleVectors,angles);H(0xa20e4,rand,rnd);H(0x8ae80,trap_Trace,traceHook);H(0x5df70,LaunchItem,launch);H(0x95050,G_PopupMessage,popup);H(0x656f0,G_createStats,stats);H(0x62600,G_LogPrintf,logger);H(0x8afc0,trap_UnlinkEntity,unlinker);H(0x8adc0,trap_SetConfigstring,config);H(0x61b60,CalculateRanks,ranks);H(0x1f8f0,BotAIShutdownClient,bot);H(0x8fa60,G_verifyMatchState,verify);H(0x75ea0,G_smvAllRemoveSingleClient,smv);
 for(mode=0;mode<3;mode++)for(v=0;v<16384;v++)for(side=0;side<2;side++){
  variant=v;number=v%4;count=ndrop=npopup=ntrace=bad=0;randomValue=(v*4681)&32767;memset(calls,0,sizeof(calls));memset(singleIndex,0,sizeof(singleIndex));memset(clients,0,sizeof(clients));memset(rawClients,0,sizeof(rawClients));memset(drops,0,sizeof(drops));memset(rawDrops,0,sizeof(rawDrops));memset(popups,0,sizeof(popups));memset(rawPopups,0,sizeof(rawPopups));
  for(i=0;i<4;i++){gentity_t*e=&g_entities[i];gclient_t*c=&clients[i];byte*p=base+0x226e2a0+i*0x57c,*q=rawClients[i];memset(e,0,sizeof(*e));memset(p,0,0x57c);e->client=c;*(void**)(p+0x190)=q;e->s.number=*(int*)p=i;c->ps.clientNum=*(int*)(q+0xa0)=i;
   c->sess.sessionTeam=*(int*)(q+0xb30)=i==number?1+(v/64)%3:3;c->sess.spectatorState=*(int*)(q+0xb38)=i==number?((v&128)?2:0):2;c->sess.spectatorClient=*(int*)(q+0xb3c)=number;c->ps.pm_flags=*(int*)(q+0xc)=i==number?((v&256)?PMF_FOLLOW:0):PMF_LIMBO;
   c->pers.connected=*(int*)(q+0x5ac)=i==number?(v/512)%3:2;c->pers.complaintClient=*(int*)(q+0x66c)=(v&1024)?number:-1;c->pers.complaintEndTime=*(int*)(q+0x670)=77;
   c->ps.persistant[PERS_TEAM]=*(int*)(q+0x11c)=2;c->ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int*)(q+0xf0)=(v&2048)?0x100:0;c->ps.stats[STAT_TCE_FLAGS]=*(int*)(q+0xf4)=(v&4096)?0x100:0;c->tceBombPossessionOrder=*(int*)(q+0x1328)=9;
   c->ps.powerups[PW_REDFLAG]=*(int*)(q+0x168)=v&4;c->ps.powerups[PW_BLUEFLAG]=*(int*)(q+0x16c)=v&8;c->ps.viewangles[0]=*(float*)(q+0xb0)=pitches[v%5];c->ps.viewangles[1]=*(float*)(q+0xb4)=42;c->ps.viewheight=*(int*)(q+0xbc)=48;VectorSet(c->ps.origin,100,200,300);memcpy(q+0x14,c->ps.origin,12);
   e->r.svFlags=*(int*)(p+0x128)=(v&16)?SVF_BOT:0;e->s.modelindex=*(int*)(p+0xa4)=11;e->s.otherEntityNum2=*(int*)(p+0x90)=23;e->message=*(char**)(p+0x248)="FLAG";e->inuse=*(int*)(p+0x194)=1;e->active=*(int*)(p+0x36c)=1;e->classname=*(char**)(p+0x1a4)="player";
   level.sortedClients[i]=*(int*)(base+0x7fec18+i*4)=i;
  }
  level.numConnectedClients=*(int*)(base+0x7fec0c)=(v/32)%5;g_landminetimeout.integer=*(int*)(base+0x6d856c)=v&1;g_gametype.integer=*(int*)(base+0x6d8d4c)=gametypes[mode];level.tceBombAssigned=*(int*)(base+0x2223918)=v&1;level.tceBombPlanted=*(int*)(base+0x2223914)=v&2;level.tceBombDropCount=*(int*)(base+0x2223934)=v%3;level.tceBombCarrierCount=*(int*)(base+0x2223938)=3;level.tceBombCarrier=*(int*)(base+0x222391c)=(v&8192)?number:-1;level.tceVipAssigned=*(int*)(base+0x2223944)=v&1;level.tceVipCarrier=*(int*)(base+0x2223948)=(v&8192)?number:-1;
  /* Explicit null-client and entity-vs-PS ownership variants. */
  if(v%37==0){g_entities[number].client=NULL;*(void**)(base+0x226e2a0+number*0x57c+0x190)=NULL;}else if(v%11==0){clients[number].ps.clientNum=*(int*)(rawClients[number]+0xa0)=(number+1)%4;level.tceBombCarrier=*(int*)(base+0x222391c)=clients[number].ps.clientNum;level.tceVipCarrier=*(int*)(base+0x2223948)=clients[number].ps.clientNum;}
  if(side)ClientDisconnect(number);else original(number);capture(&b);
  if(!side)a=b;else{if(bad||memcmp(&a,&b,sizeof(a))){printf("ClientDisconnect mismatch gt%d case%d bad%d calls%d/%d drops%d/%d\n",gametypes[mode],v,bad,a.count,b.count,a.ndrop,b.ndrop);for(k=0;k<a.count||k<b.count;k++)if(memcmp(&a.calls[k],&b.calls[k],sizeof(call)))printf("call%d kind%d/%d args%d,%d,%d /%d,%d,%d text[%s]/[%s] floats %.9g/%.9g\n",k,a.calls[k].kind,b.calls[k].kind,a.calls[k].a,a.calls[k].b,a.calls[k].c,b.calls[k].a,b.calls[k].b,b.calls[k].c,a.calls[k].text,b.calls[k].text,a.calls[k].x[5],b.calls[k].x[5]);for(k=0;k<48;k++)if(a.state[k]!=b.state[k])printf("state%d %d/%d\n",k,a.state[k],b.state[k]);for(i=0;i<3;i++)for(k=0;k<9;k++)if(a.drop[i][k]!=b.drop[i][k])printf("drop%d field%d %d/%d\n",i,k,a.drop[i][k],b.drop[i][k]);return 1;}n++;}
 }
 printf("PASS ClientDisconnect %d whole-controller cases (mocked external dependencies)\n",n);return 0;
}
