#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_main.c"
#include <windows.h>
#include <stdio.h>

/* Complete entry-to-return comparison. Only external callees are intercepted;
 * both inline objective controllers and reinforcement arithmetic stay real. */
static byte *base,rawClients[64][0x16b0],rawPopups[2][0x57c];
static gclient_t clients[64];static gentity_t popups[2];
static int side,variant,rngValue,calls,bad,clockCalls,popupCalls,mutation;
typedef struct {int kind,a,b,c;char text[160];} call_t;
static call_t traceCalls[512],expectedCalls[512];
typedef struct {size_t offset;int rva;const char *name;} field_t;
#define FIELD(name,rva) {offsetof(level_locals_t,name),rva,#name}
static const field_t fields[]={
 FIELD(tceFrameStartTime,0x2223968),FIELD(restarted,0x7fec08),
 FIELD(match_pause,0x221bafc),FIELD(timeCurrent,0x221bc08),FIELD(timeDelta,0x221bc0c),
 FIELD(frameTime,0x7febe8),FIELD(framenum,0x7febdc),FIELD(previousTime,0x7febe4),
 FIELD(time,0x7febe0),FIELD(startTime,0x7febec),FIELD(axisBombCounter,0x2222324),
 FIELD(alliedBombCounter,0x2222328),FIELD(num_entities,0x7feb8c),
 FIELD(numConnectedClients,0x7fec0c),FIELD(maxclients,0x7febd8),FIELD(warmupTime,0x7feb90),
 FIELD(tceDemolitionTeam,0x222393c),FIELD(tceBombAssigned,0x2223918),
 FIELD(tceBombCarrier,0x222391c),FIELD(tceBombCarrierCount,0x2223938),
 FIELD(tceVipTeam,0x2223940),FIELD(tceVipAssigned,0x2223944),FIELD(tceVipCarrier,0x2223948),
 FIELD(tceSpawnPhaseTime,0x222392c),FIELD(tceSpawnPhase,0x2223930)};
#define NF (sizeof(fields)/sizeof(fields[0]))
static int snapshot[512],expected[512],snapshotCount;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void note(int kind,int a,int b,int c,const char*s){call_t*p;if(calls==512)exit(3);p=&traceCalls[calls++];memset(p,0,sizeof(*p));p->kind=kind;p->a=a;p->b=b;p->c=c;if(s)Q_strncpyz(p->text,s,sizeof(p->text));}
static void setField(int n,int value){*(int*)((byte*)&level+fields[n].offset)=*(int*)(base+fields[n].rva)=value;}
static int getField(int n){return side?*(int*)((byte*)&level+fields[n].offset):*(int*)(base+fields[n].rva);}
static int entityNumber(void *p){return side?(gentity_t*)p-g_entities:((byte*)p-(base+0x226e2a0))/0x57c;}
static int millis(void){int result=100000+clockCalls++;note(1,result,0,0,NULL);return result;}
static void setCS(int cs,const char*s){note(2,cs,0,0,s);}
static void getCS(int cs,char*s,int len){note(3,cs,len,0,NULL);Q_strncpyz(s,"\\winner\\1",len);}
static void cvars(void){note(4,0,0,0,NULL);}
static void official(void){note(5,0,0,0,NULL);}
static void bots(void){note(6,0,0,0,NULL);}
static void nodes(void){note(7,0,0,0,NULL);}
static void entity(void*p,int ms){int i=entityNumber(p),j;note(8,i,ms,0,NULL);for(j=0;j<getField(12);j++)if(side?g_entities[j].runthisframe:*(int*)(base+0x226e2a0+j*0x57c+0x55c))bad=1;
 /* A callee may grow the entity count; the loop must re-read its bound. */
 if(mutation&&i==0){if(side)level.num_entities++;else (*(int*)(base+0x7feb8c))++;}
}
static void endframe(void*p){note(9,entityNumber(p),0,0,NULL);}
static int randomHook(void){note(10,rngValue,0,0,NULL);return rngValue;}
static void spawn(void*p){int i;for(i=0;i<64;i++)if(p==(side?(void*)&clients[i]:(void*)rawClients[i]))break;if(i==64)bad=1;note(11,i,getField(21),getField(22),NULL);}
static void*popup(int type){int i=popupCalls++;if(i>=2)exit(4);note(12,type,0,0,NULL);return side?(void*)&popups[i]:(void*)rawPopups[i];}
static void QDECL print(const char*f,...){va_list a;char s[160];va_start(a,f);Q_vsnprintf(s,sizeof(s),f,a);va_end(a);note(13,0,0,0,s);}
static void QDECL dprint(const char*f,...){note(99,0,0,0,f);}
static void wolf(void){note(14,0,0,0,NULL);}static void exitRules(void){note(15,0,0,0,NULL);}
static void teams(void){note(16,0,0,0,NULL);}static void votes(void){note(17,0,0,0,NULL);}
static void checkCvars(void){note(18,0,0,0,NULL);}static void mapData(void){note(19,0,0,0,NULL);}
static int mines(team_t team){note(20,team,0,0,NULL);return (variant+team)%13;}
static void capture(void){int i; snapshotCount=0;for(i=0;i<NF;i++)snapshot[snapshotCount++]=getField(i);
 for(i=0;i<64;i++){byte*p=rawClients[i];snapshot[snapshotCount++]=side?clients[i].ps.stats[STAT_TCE_WEAPON_FLAGS]:*(int*)(p+0xf0);snapshot[snapshotCount++]=side?clients[i].ps.stats[STAT_TCE_FLAGS]:*(int*)(p+0xf4);snapshot[snapshotCount++]=side?clients[i].tceBombPossessionOrder:*(int*)(p+0x1328);snapshot[snapshotCount++]=side?g_entities[i].runthisframe:*(int*)(base+0x226e2a0+i*0x57c+0x55c);}
 for(i=0;i<2;i++){snapshot[snapshotCount++]=side?popups[i].s.effect2Time:*(int*)(rawPopups[i]+0x110);snapshot[snapshotCount++]=side?popups[i].s.effect3Time:*(int*)(rawPopups[i]+0x114);snapshot[snapshotCount++]=side?popups[i].s.density:*(int*)(rawPopups[i]+0xf4);}
 snapshot[snapshotCount++]=side?g_entities[64].s.otherEntityNum:*(int*)(base+0x226e2a0+64*0x57c+0x8c);snapshot[snapshotCount++]=side?g_entities[64].s.otherEntityNum2:*(int*)(base+0x226e2a0+64*0x57c+0x90);
}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(int);int v,r,i,j,n,nextTime,expectedCount,total=0;int rngs[]={0,1,4681,9362,18724,28086,32766,32767};int elapsed[]={0,500,501,1000,2999,3000,5000,5001,10000};int sizes[]={0,1,8,64};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x646b0);
#define H(rva,source,replacement) if(!hook(base+rva,replacement)||!hook(source,replacement))return 2
 H(0x8ab50,trap_Milliseconds,millis);H(0x8adc0,trap_SetConfigstring,setCS);H(0x8ade0,trap_GetConfigstring,getCS);H(0x60270,G_UpdateCvars,cvars);H(0x64d50,G_CheckOfficial,official);H(0x3ea60,TCE_UpdateBotInfo,bots);H(0x412d0,TCE_UpdateNodes,nodes);H(0x642c0,G_RunEntity,entity);H(0x47530,ClientEndFrame,endframe);H(0xa20e4,rand,randomHook);H(0x4b9c0,SetWolfSpawnWeapons,spawn);H(0x95050,G_PopupMessage,popup);H(0x5ed80,G_Printf,print);H(0x5edc0,G_DPrintf,dprint);H(0x635a0,CheckWolfMP,wolf);H(0x62f10,CheckExitRules,exitRules);H(0x8e530,CheckTeamStatus,teams);H(0x63720,CheckVote,votes);H(0x638f0,CheckCvars,checkCvars);H(0x912f0,G_UpdateTeamMapData,mapData);H(0x6c3f0,G_CountTeamLandmines,mines);
 for(v=0;v<8192;v++)for(r=0;r<8;r++)for(side=0;side<2;side++){
  variant=v;rngValue=rngs[r];calls=bad=clockCalls=popupCalls=0;memset(traceCalls,0,sizeof(traceCalls));memset(rawPopups,0,sizeof(rawPopups));memset(popups,0,sizeof(popups));memset(rawClients,0,sizeof(rawClients));memset(clients,0,sizeof(clients));
  for(i=0;i<NF;i++)setField(i,0);n=sizes[v%4];nextTime=10000+elapsed[(v/4)%9];mutation=(v/36)%3==1;
  setField(0,-4);setField(1,(v/108)%17==1);setField(2,(v/108)%3);setField(3,11000);setField(4,(v/324)%3*100);setField(5,-8);setField(6,(v/36)%7);setField(7,-1);setField(8,11000+(v%2));setField(9,10000);setField(10,(v/2)%5*100);setField(11,(v/7)%5*500);setField(12,n);setField(13,n);setField(14,n);setField(15,(v/972)%7==1?1000:0);
  setField(16,(v/36)%4);setField(17,(v/3888)%2);setField(18,-3);setField(19,(v/108)%4);setField(20,(v/144)%4);setField(21,(v/2916)%2);setField(22,-4);setField(23,(v/72)%3*2500);setField(24,-5);
  g_gametype.integer=*(int*)(base+0x6d8d4c)=((v/432)%4==0?2:(v/432)%4==1?5:(v/432)%4==2?7:0);g_gamestate.integer=*(int*)(base+0x784f6c)=(v/1728)%3==2?1:0;bot_enable.integer=*(int*)(base+0x784aec)=(v/7)%2;g_redlimbotime.integer=*(int*)(base+0x6d90ac)=(v/216)%3*1000;
  level.clients=clients;level.gameManager=v%2?&g_entities[64]:NULL;*(void**)(base+0x221b71c)=v%2?base+0x226e2a0+64*0x57c:NULL;
  for(i=0;i<65;i++){byte*e=base+0x226e2a0+i*0x57c;memset(e,0,0x57c);memset(&g_entities[i],0,sizeof(g_entities[i]));g_entities[i].runthisframe=*(int*)(e+0x55c)=i<n?1:0;if(i==64)continue;
   g_entities[i].client=&clients[i];*(void**)(e+0x190)=rawClients[i];clients[i].pers.connected=*(int*)(rawClients[i]+0x5ac)=2;clients[i].ps.clientNum=*(int*)(rawClients[i]+0xa0)=(v/4000)%2&&n?(i+1)%n:i;clients[i].sess.sessionTeam=*(int*)(rawClients[i]+0xb30)=1+(i+(v/288))%2;clients[i].ps.stats[STAT_HEALTH]=*(int*)(rawClients[i]+0xd0)=100;clients[i].ps.stats[STAT_TCE_FLAGS]=*(int*)(rawClients[i]+0xf4)=(i%3==0?2:0);clients[i].ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int*)(rawClients[i]+0xf0)=0x100;clients[i].tceBombPossessionOrder=*(int*)(rawClients[i]+0x1328)=(v+i)%4;
   if((i+v)%9==0){g_entities[i].client=NULL;*(void**)(e+0x190)=NULL;}
   switch((i+v)%11){case 1:clients[i].pers.connected=*(int*)(rawClients[i]+0x5ac)=1;break;case 2:g_entities[i].s.eFlags=*(int*)(e+8)=EF_CONNECTION;break;case 3:clients[i].sess.sessionTeam=*(int*)(rawClients[i]+0xb30)=3;break;case 4:clients[i].ps.pm_flags=*(int*)(rawClients[i]+0xc)=PMF_LIMBO;break;case 5:clients[i].ps.pm_flags=*(int*)(rawClients[i]+0xc)=PMF_FOLLOW;break;case 6:clients[i].ps.stats[STAT_HEALTH]=*(int*)(rawClients[i]+0xd0)=49;break;case 7:clients[i].ps.stats[STAT_HEALTH]=*(int*)(rawClients[i]+0xd0)=50;break;}
   level.sortedClients[i]=*(int*)(base+0x7fec18+i*4)=n?n-1-(i%n):0;
  }
  /* Aliased client numbers remain valid pointers, even when the selected slot
   * differs from ps.clientNum; source must perform the original second lookup. */
  if((v/4000)%2)for(i=0;i<n;i++){g_entities[i].client=&clients[i];*(void**)(base+0x226e2a0+i*0x57c+0x190)=rawClients[i];}
  if(side)G_RunFrame(nextTime);else original(nextTime);capture();
  if(!side){memcpy(expected,snapshot,sizeof(snapshot));memcpy(expectedCalls,traceCalls,sizeof(traceCalls));expectedCount=calls;}
  else{if(bad||calls!=expectedCount||memcmp(expected,snapshot,snapshotCount*4)||memcmp(expectedCalls,traceCalls,sizeof(traceCalls))){printf("RunFrame mismatch case%d rng%d bad%d calls%d/%d\n",v,rngValue,bad,expectedCount,calls);for(j=0;j<snapshotCount;j++)if(expected[j]!=snapshot[j])printf("state%d(%s) %d/%d\n",j,j<NF?fields[j].name:"client/popup/manager",expected[j],snapshot[j]);for(j=0;j<calls||j<expectedCount;j++)if(memcmp(&expectedCalls[j],&traceCalls[j],sizeof(call_t)))printf("call%d %d,%d,%d,%d[%s] / %d,%d,%d,%d[%s]\n",j,expectedCalls[j].kind,expectedCalls[j].a,expectedCalls[j].b,expectedCalls[j].c,expectedCalls[j].text,traceCalls[j].kind,traceCalls[j].a,traceCalls[j].b,traceCalls[j].c,traceCalls[j].text);return 1;}total++;}
 }
 printf("PASS whole G_RunFrame %d cases including combined objective and frame tail\n",total);return 0;
}

