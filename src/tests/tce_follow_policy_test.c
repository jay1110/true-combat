#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c],rawClient[0x2000];static gentity_t ent;static gclient_t client;static int bad,calls,teams[4],populations[2];
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static int teamCount(int ignore,int team){if(ignore!=-1||team<1||team>2||calls>=4){bad=1;return 0;}teams[calls++]=team;return populations[team-1];}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*allow)(void*,int);int(__cdecl*desired)(void*,int);int mode,gt,own,candidate,invite,pref,scene,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;allow=(void*)(b+0x8fda0);desired=(void*)(b+0x8fe70);if(!hook(b+0x4b8a0,teamCount)||!hook(TeamCount,teamCount))return 2;ent.client=&client;*(void**)(raw+0x190)=rawClient;
 for(mode=0;mode<2;++mode)for(gt=0;gt<10;++gt)for(own=0;own<4;++own)for(candidate=0;candidate<4;++candidate)for(invite=0;invite<8;++invite)for(pref=0;pref<4;++pref)for(scene=0;scene<64;++scene){int i,x,y,expectedCalls,expectedTeams[4];
  g_gametype.integer=*(int*)(b+0x6d8d4c)=gt;g_lms_followTeamOnly.integer=*(int*)(b+0x7e1e4c)=((scene>>5)&1);level.time=*(int*)(b+0x7febe0)=100000;level.startTime=*(int*)(b+0x7febec)=level.time-(scene&16?2501:2500);client.sess.sessionTeam=*(int*)(rawClient+0xb30)=own;client.sess.spec_invite=*(int*)(rawClient+0xc00)=invite;client.sess.spec_team=*(int*)(rawClient+0xc04)=pref;
  for(i=0;i<4;++i)teamInfo[i].spec_lock=*(int*)(b+0x66fde0+i*0x28)=(scene>>i)&1;
  for(i=0;i<2;++i){populations[i]=(scene>>(i+2))&1;level.numTeamClients[i]=*(int*)(b+0x7ff240+i*4)=(invite+i)%3;level.numFinalDead[i]=*(int*)(b+0x221a418+i*4)=(pref+i)%4-1;}
  bad=calls=0;x=mode?desired(raw,candidate):allow(raw,candidate);if(bad)return 3;expectedCalls=calls;memcpy(expectedTeams,teams,sizeof(teams));bad=calls=0;y=mode?G_desiredFollow(&ent,candidate):G_allowFollow(&ent,candidate);
  if(bad||x!=y||calls!=expectedCalls||memcmp(teams,expectedTeams,calls*sizeof(int))){fprintf(stderr,"follow mode%d gt%d own%d target%d invite%d pref%d scene%d mismatch result%d/%d calls%d/%d\n",mode,gt,own,candidate,invite,pref,scene,y,x,calls,expectedCalls);return 1;}
  for(i=0;i<4;++i)if(teamInfo[i].spec_lock!=*(int*)(b+0x66fde0+i*0x28)){fprintf(stderr,"follow lock mode%d scene%d team%d mismatch\n",mode,scene,i);return 1;}++n;
 }
 printf("%d whole follow-policy comparisons passed (327680 each)\n",n);return 0;
}
