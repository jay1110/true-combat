#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c],rawClient[0x2000];static gentity_t ent;static gclient_t client;static int bad,calls,teams[4],populations[2];
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static int teamCount(int ignore,int team){if(ignore!=-1||team<1||team>2||calls>=4){bad=1;return 0;}teams[calls++]=team;return populations[team-1];}
static int testBlock(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*allow)(void*,int);int(__cdecl*desired)(void*,int);int mode,gt,own,candidate,invite,pref,scene,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;allow=(void*)(b+0x8fd80);desired=(void*)(b+0x8fe70);if(!hook(b+0x4b8a0,teamCount)||!hook(TeamCount,teamCount))return 2;ent.client=&client;*(void**)(raw+0x190)=rawClient;
 for(mode=0;mode<1;++mode)for(gt=0;gt<10;++gt)for(own=0;own<4;++own)for(candidate=0;candidate<4;++candidate)for(invite=0;invite<8;++invite)for(pref=0;pref<4;++pref)for(scene=0;scene<64;++scene){int i,x,y,expectedCalls,expectedTeams[4];
  g_gametype.integer=*(int*)(b+0x6d8d4c)=gt;g_lms_followTeamOnly.integer=*(int*)(b+0x7e1e4c)=((scene>>5)&1);level.time=*(int*)(b+0x7febe0)=100000;level.startTime=*(int*)(b+0x7febec)=level.time-(scene&16?2501:2500);client.sess.sessionTeam=*(int*)(rawClient+0xb30)=own;client.sess.spec_invite=*(int*)(rawClient+0xc00)=invite;client.sess.spec_team=*(int*)(rawClient+0xc04)=pref;
  for(i=0;i<4;++i)teamInfo[i].spec_lock=*(int*)(b+0x66fde0+i*0x28)=(scene>>i)&1;
  for(i=0;i<2;++i){populations[i]=(scene>>(i+2))&1;level.numTeamClients[i]=*(int*)(b+0x7ff240+i*4)=(invite+i)%3;level.numFinalDead[i]=*(int*)(b+0x221a418+i*4)=(pref+i)%4-1;}
  bad=calls=0;x=mode?desired(raw,candidate):allow(raw,candidate);if(bad)return 3;expectedCalls=calls;memcpy(expectedTeams,teams,sizeof(teams));bad=calls=0;y=mode?G_desiredFollow(&ent,candidate):G_blockoutTeam(&ent,candidate);
  if(bad||x!=y||calls!=expectedCalls||memcmp(teams,expectedTeams,calls*sizeof(int))){fprintf(stderr,"follow mode%d gt%d own%d target%d invite%d pref%d scene%d mismatch result%d/%d calls%d/%d\n",mode,gt,own,candidate,invite,pref,scene,y,x,calls,expectedCalls);return 1;}
  for(i=0;i<4;++i)if(teamInfo[i].spec_lock!=*(int*)(b+0x66fde0+i*0x28)){fprintf(stderr,"follow lock mode%d scene%d team%d mismatch\n",mode,scene,i);return 1;}++n;
 }
 printf("%d whole blockout comparisons passed\n",n);return 0;
}

static byte *base, *entities; static byte clients[4][0x2000]; static gclient_t nativeClients[4];
static int nativeSide, scenario, lockTeam, eventCount, events[24][4], fail;
static int clientIndex(void *p) {
 int i; for(i=0;i<4;i++)if(p==(nativeSide?(void*)&g_entities[i]:(void*)(entities+i*0x57c)))return i;
 fail=1;return 0;
}
static void callback(void *p,int kind,int team) {
 int i=clientIndex(p), *mv,*invite,*pref;
 if(eventCount>=24){fail=1;return;}
 mv=nativeSide?&nativeClients[i].pers.mvCount:(int*)(clients[i]+0xae8);
 invite=nativeSide?&nativeClients[i].sess.spec_invite:(int*)(clients[i]+0xc00);
 pref=nativeSide?&nativeClients[i].sess.spec_team:(int*)(clients[i]+0xc04);
 events[eventCount][0]=kind;events[eventCount][1]=i;events[eventCount][2]=team;events[eventCount++][3]=*invite;
 if(kind==2 && team!=lockTeam)fail=1;
 if(scenario&1024)*mv=(scenario&2048)?2:0;
 if(scenario&4096){*invite^=4;*pref^=3;}
 if(scenario&8192){if(nativeSide)level.numConnectedClients=1;else *(int*)(base+0x7fec0c)=1;}
}
static void stop(void *p){callback(p,1,0);}
static void removeInvalid(void *p,int team){callback(p,2,team);}
static int setTeam(void *p,char *s,int force,int w1,int w2,int w3,int setweapons){
 if(strcmp(s,"s")||force!=1||w1!=-1||w2!=-1||w3!=-1||setweapons)fail=1;
 callback(p,3,0);return (scenario&1);
}
static void setupLocks(int sc) {
 int i,j;memset(clients,0,sizeof(clients));memset(nativeClients,0,sizeof(nativeClients));
 level.numConnectedClients=*(int*)(base+0x7fec0c)=(sc&512)?0:4;
 for(i=0;i<4;i++){
  int v=(sc+i*53)&511;
  level.sortedClients[i]=*(int*)(base+0x7fec18+i*4)=(i+(sc&3))%4;
  g_entities[i].client=&nativeClients[i];*(void**)(entities+i*0x57c+0x190)=clients[i];
  nativeClients[i].sess.referee=*(int*)(clients[i]+0xbf8)=(v&1)?1:0;
  nativeClients[i].sess.coach_team=*(int*)(clients[i]+0xbe0)=(v&2)?2:0;
  nativeClients[i].sess.sessionTeam=*(int*)(clients[i]+0xb30)=(v>>2)&3;
  nativeClients[i].sess.spectatorState=*(int*)(clients[i]+0xb38)=(v>>4)&3;
  nativeClients[i].pers.mvCount=*(int*)(clients[i]+0xae8)=((v>>6)&3)-1;
  nativeClients[i].sess.spec_invite=*(int*)(clients[i]+0xc00)=(sc+i)&7;
  nativeClients[i].sess.spec_team=*(int*)(clients[i]+0xc04)=(sc+2*i)&7;
 }
 for(i=0;i<4;i++)for(j=0;j<40;j++)((byte*)&teamInfo[i])[j]=base[0x66fde0+i*40+j]=(byte)(sc+i*31+j);
}
int main(int argc,char **argv){
 HMODULE dll;void(__cdecl*update)(int,int);void(__cdecl*swap)(void);int team,lock,sc,i,n=0;
 if(argc!=2||sizeof(team_info)!=40||!(dll=LoadLibraryA(argv[1])))return 2;
 base=(byte*)dll;entities=base+0x226e2a0;update=(void*)(base+0x8fc40);swap=(void*)(base+0x8fd50);
 if(!hook(base+0x505f0,stop)||!hook(StopFollowing,stop)||!hook(base+0x75e30,removeInvalid)||!hook(G_smvRemoveInvalidClients,removeInvalid)||!hook(base+0x50070,setTeam)||!hook(SetTeam,setTeam))return 2;
 for(team=1;team<=2;team++)for(lock=0;lock<3;lock++)for(sc=0;sc<16384;sc++){
  int saved[24][4],count;scenario=sc;lockTeam=team;setupLocks(sc);nativeSide=0;fail=eventCount=0;update(team,lock);
  if(fail)return 3;count=eventCount;memcpy(saved,events,sizeof(saved));nativeSide=1;fail=eventCount=0;G_updateSpecLock(team,lock);
  if(fail||eventCount!=count||memcmp(saved,events,count*sizeof(saved[0]))||level.numConnectedClients!=*(int*)(base+0x7fec0c)||memcmp(teamInfo,base+0x66fde0,160)){fprintf(stderr,"speclock case%d team%d lock%d callbacks%d/%d failed\n",sc,team,lock,eventCount,count);return 1;}
  for(i=0;i<4;i++)if(nativeClients[i].pers.mvCount!=*(int*)(clients[i]+0xae8)||nativeClients[i].sess.spec_invite!=*(int*)(clients[i]+0xc00)||nativeClients[i].sess.spec_team!=*(int*)(clients[i]+0xc04)){fprintf(stderr,"speclock case%d client%d state failed\n",sc,i);return 1;}
  n++;
 }
 for(sc=0;sc<4096;sc++){setupLocks(sc);swap();G_swapTeamLocks();if(memcmp(teamInfo,base+0x66fde0,160)){fprintf(stderr,"swap case%d failed\n",sc);return 1;}}
 printf("%d whole spec-lock and 4096 whole swap comparisons passed\n",n);
 return testBlock(argc,argv);
}
