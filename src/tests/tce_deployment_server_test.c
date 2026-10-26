#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void Cmd_Team_f(gentity_t *,unsigned int,qboolean);
static const char *args[7];static int count,result;static char events[2048];
static int argcTrap(void){return count;}
static void argvTrap(int i,char *out,int size){Q_strncpyz(out,i<count?args[i]:"",size);}
static void printTrap(int client,const char *s){strcat(events,va("print:%d:%s;",client,s));}
static int setTeam(void *ent,const char *s,int force,int w,int w2,int w3,int update){strcat(events,va("team:%s:%d:%d:%d:%d:%d;",s,force,w,w2,w3,update));return result;}
static void setWeapons(void *ent,int w,int w2,int w3,int update){strcat(events,va("weapons:%d:%d:%d:%d;",w,w2,w3,update));}
static void patch(void *a,void *f){DWORD old,tmp;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){HMODULE h;unsigned char *b,*ent;static unsigned char client[0x2000];static gclient_t c;int team,cl,hp,t,n=0,ec,ef;char cls[32],token[32],expected[2048];const int health[]={-1,0,100,0xfbff,0xfc00,0x10064};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;ent=b+0x226e2a0;*(void **)(ent+400)=client;g_entities[0].client=&c;
 patch(b+0x8ab60,argcTrap);patch(trap_Argc,argcTrap);patch(b+0x8ab70,argvTrap);patch(trap_Argv,argvTrap);
 patch(b+0x8ad60,printTrap);patch(trap_SendServerCommand,printTrap);patch(b+0x50070,setTeam);patch(SetTeam,setTeam);patch(b+0x50830,setWeapons);patch(G_SetClientWeapons,setWeapons);
 args[0]="team";args[1]="b";args[2]=cls;args[3]="0459";args[4]="39";args[5]="36";args[6]=token;
 for(count=1;count<=7;++count)for(team=0;team<5;++team)for(cl=-2;cl<10;++cl)for(hp=0;hp<6;++hp)for(t=0;t<3;++t)for(result=0;result<2;++result){
  sprintf(cls,"%d",cl);sprintf(token,"%d",(health[hp]>0xfbff?health[hp]-0x400:health[hp])+t);
  c.ps.clientNum=*(int *)(client+0xa0)=0;c.ps.stats[STAT_TCE_SHOT_SEED]=*(int *)(client+0xec)=health[hp];
  c.sess.sessionTeam=*(int *)(client+0xb30)=team;c.sess.latchPlayerType=*(int *)(client+0xb50)=3;c.sess.tceSetupFlags=*(int *)(client+0xde0)=9;
  events[0]=0;((void(__cdecl *)(void *))(b+0x508f0))(ent);strcpy(expected,events);ec=*(int *)(client+0xb50);ef=*(int *)(client+0xde0);
  events[0]=0;Cmd_Team_f(&g_entities[0],0,0);
  if(strcmp(expected,events)||ec!=c.sess.latchPlayerType||ef!=c.sess.tceSetupFlags){printf("team mismatch argc%d team%d cls%d hp%d token%d result%d\n%s\n%s\n",count,team,cl,health[hp],t,result,expected,events);return 1;}++n;
 }
 printf("%d full Cmd_Team_f original comparisons passed\n",n);return 0;
}
