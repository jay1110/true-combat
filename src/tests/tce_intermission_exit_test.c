#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void CheckIntermissionExit(void);
extern void ExitLevel(void);
extern qboolean ScoreIsTied(void);
static unsigned char *b,raw[64][0x16b0];
static gclient_t clients[64];
static char events[2048],winner[32];
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void dump(int kind){sprintf(events+strlen(events),"dump%d;",kind);}
static void leave(void){strcat(events,"exit;");}
static void config(int id,char *out,int size){sprintf(events+strlen(events),"config%d/%d;",id,size);snprintf(out,size,"\\winner\\%s",winner);}
int main(int argc,char **argv){HMODULE h;int n,v,i,t,r,l,p,cases=0;char expected[2048];int originalRef;
 const int sizes[]={0,1,2,7,64},times[]={0,300,301,800,801,4999,5000,7999,8000,60000},limits[]={-1,0,2,3,5,9},percents[]={0,1,50,99,100,101};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x65e20,dump);patch(G_matchInfoDump,dump);patch(b+0x62440,leave);patch(ExitLevel,leave);
 patch(b+0x8ade0,config);patch(trap_GetConfigstring,config);
 level.clients=clients;*(void **)(b+0x7feb80)=raw;
 for(n=0;n<5;++n)for(v=0;v<4;++v)for(l=0;l<6;++l)for(r=-1;r<10;++r)for(p=0;p<6;++p)for(t=0;t<10;++t){
  level.numConnectedClients=*(int *)(b+0x7fec0c)=sizes[n];
  for(i=0;i<sizes[n];++i){int j=sizes[n]-1-i;level.sortedClients[i]=*(int *)(b+0x7fec18+4*i)=j;clients[j].pers.connected=*(int *)(raw[j]+0x5ac)=v==0?2:(i+v)%3;clients[j].sess.sessionTeam=*(int *)(raw[j]+0xb30)=v==0?1:i%4;clients[j].pers.ready=*(int *)(raw[j]+0xafc)=(i+v)%2;g_entities[j].r.svFlags=*(int *)(b+0x226e3c8+0x57c*j)=v==2?SVF_BOT:0;}
  level.time=*(int *)(b+0x7febe0)=1000+times[t];level.intermissiontime=*(int *)(b+0x7ffc60)=1000;
  level.ref_allready=*(int *)(b+0x221bb00)=v==3;g_lms_roundlimit.integer=*(int *)(b+0x7e19cc)=limits[l];g_currentRound.integer=*(int *)(b+0x2223cec)=r;match_readypercent.integer=*(int *)(b+0x2223bcc)=percents[p];
  events[0]=0;((void(__cdecl *)(void))(b+0x62d40))();strcpy(expected,events);originalRef=*(int *)(b+0x221bb00);events[0]=0;CheckIntermissionExit();
  if(strcmp(events,expected)||level.ref_allready!=originalRef){printf("exit mismatch n%d v%d limit%d round%d percent%d time%d: %s / %s\n",sizes[n],v,limits[l],r,percents[p],times[t],expected,events);return 1;}++cases;
 }
 for(i=-5;i<6;++i){int a,c;sprintf(winner,"%d",i);events[0]=0;a=((int(__cdecl *)(void))(b+0x62ed0))();strcpy(expected,events);events[0]=0;c=ScoreIsTied();if(a!=c||strcmp(events,expected))return 1;++cases;}
 printf("%d whole Windows intermission-exit/tie comparisons passed\n",cases);return 0;
}
