#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_Debriefing_InfoRequests(void);
static unsigned char *b;static char events[4096],info[64];
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static const char *config(int id){sprintf(events+strlen(events),"get:%d;",id);return info;}
static void set(const char *k,const char *v){sprintf(events+strlen(events),"set:%s=%s;",k,v);}
static void command(const char *s){sprintf(events+strlen(events),"send:%s;",s);}
static int sound(const char *s,int compressed){strcat(events,"UNEXPECTED SOUND;");return 1;}
static void play(int s,int channel){strcat(events,"UNEXPECTED PLAY;");}
int main(int argc,char **argv){HMODULE h;int client,w,v,t,s,mode,cases=0;char expected[4096];const int times[]={-1,0,1,999,1000,1001,1999,2000,2001};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;patch(b+0x481a0,config);patch(CG_ConfigString,config);patch(b+0x64c10,set);patch(trap_Cvar_Set,set);patch(b+0x64da0,command);patch(trap_SendClientCommand,command);patch(trap_S_RegisterSound,sound);patch(trap_S_StartLocalSound,play);
 for(mode=0;mode<2;++mode)for(client=0;client<64;++client)for(w=-1;w<3;++w)for(v=0;v<8;++v)for(t=0;t<9;++t){
  cg.clientNum=*(int *)(b+0x40352a4)=client;cg.time=*(int *)(b+0x407dea4)=times[t];sprintf(info,"\\winner\\%d",w);
  cgs.dbShowing=*(int *)(b+0x3fde4d0)=v&1;cgs.dbAccuraciesRecieved=*(int *)(b+0x3fde4d4)=v&1;cgs.dbPlayerKillsDeathsRecieved=*(int *)(b+0x3fde4d8)=v&2;cgs.dbWeaponStatsRecieved=*(int *)(b+0x3fde4dc)=v&4;
  cgs.dbAwardsParsed=*(int *)(b+0x3fde4e0)=1;cgs.dbLastRequestTime=*(int *)(b+0x3fde954)=w<0?0:w*1000;cgs.dbLastScoreRequest=*(int *)(b+0x3fde958)=w<1?0:1000;cgs.dbSelectedClient=*(int *)(b+0x3fde4c8)=63-client;cgs.dbMode=*(int *)(b+0x3fde4cc)=2;
  events[0]=0;((void(__cdecl *)(void))(b+(mode?0x1d580:0x1d500)))();strcpy(expected,events);events[0]=0;if(mode)CG_Debriefing_InfoRequests();else CG_Debriefing_Startup();
  if(strcmp(expected,events)){printf("request mismatch %d %d %d %d %d: %s/%s\n",mode,client,w,v,t,expected,events);return 1;}
#define CHECK(field,offset) if(cgs.field!=*(int *)(b+offset)) {printf("state mismatch " #field "\n");return 2;}
  CHECK(dbShowing,0x3fde4d0);CHECK(dbAccuraciesRecieved,0x3fde4d4);CHECK(dbPlayerKillsDeathsRecieved,0x3fde4d8);CHECK(dbWeaponStatsRecieved,0x3fde4dc);CHECK(dbAwardsParsed,0x3fde4e0);CHECK(dbLastRequestTime,0x3fde954);CHECK(dbLastScoreRequest,0x3fde958);CHECK(dbSelectedClient,0x3fde4c8);CHECK(dbMode,0x3fde4cc);++cases;
 }
 printf("%d whole Windows debrief startup/request comparisons passed\n",cases);return 0;
}
