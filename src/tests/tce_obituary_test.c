#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_event.c"
#include <windows.h>
#include <stdio.h>
static snapshot_t snap;
static unsigned char rawSnap[0x1000],rawEvent[0x200];
static char events[4096];
static void patch(void *at,void *fn){DWORD old,tmp;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void small(int type,const char *text,int shader){sprintf(events+strlen(events),"small:%d:%s:%d;",type,text,shader);}
static void big(int type,const char *text,int shader){sprintf(events+strlen(events),"big:%d:%s:%d;",type,text,shader);}
static const char *translate(const char *text){sprintf(events+strlen(events),"translate:%s;",text);return text;}
int main(int argc,char **argv){HMODULE dll;unsigned char *b;entityState_t es;int mode,kill,mod,rel,team,weapon,count=0;char expected[4096],killer[36];
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char*)dll;
 patch(b+0x57810,small);patch(CG_AddPMItem,small);patch(b+0x57970,big);patch(CG_AddPMItemBig,big);patch(b+0x2a830,translate);patch(CG_TranslateString,translate);
 cg.snap=&snap;*(void **)(b+0x40352c4)=rawSnap;snap.ps.clientNum=*(int *)(rawSnap+0xcc)=0;
 cgs.media.pmImages[PM_DEATH]=*(int *)(b+0x259d198)=765;
 for(weapon=0;weapon<64;++weapon){sprintf(tce_cg_weapons[weapon].deployMenuShortName,"TC_WEAPON_%d",weapon);strcpy((char*)(b+0x484af6c+weapon*0xfd0),tce_cg_weapons[weapon].deployMenuShortName);}
 for(mode=0;mode<8;++mode)for(kill=-1;kill<=1;++kill)for(mod=0;mod<67;++mod)for(rel=0;rel<7;++rel)for(team=0;team<4;++team)for(weapon=0;weapon<64;++weapon){
  int victim=(rel==0||rel>=5)?0:1,attacker=(rel==1||rel==5)?0:rel==2?1:(rel==3||rel==6)?1022:2;
  memset(&es,0,sizeof(es));memset(rawEvent,0,sizeof(rawEvent));
  es.otherEntityNum=*(int*)(rawEvent+0x8c)=victim;es.otherEntityNum2=*(int*)(rawEvent+0x90)=attacker;
  es.eventParm=*(int*)(rawEvent+0xbc)=mod;/* Native0xa8 is modelindex2, the existing TC weapon media slot. */
  es.modelindex2=*(int*)(rawEvent+0xa8)=weapon;
  cgs.gametype=*(int*)(b+0x257e100)=mode;cgs.tceKillMessage=*(int*)(b+0x3fdf388)=kill;
  strcpy(cgs.clientinfo[0].name,"Local");strcpy(cgs.clientinfo[1].name,weapon==2?"^2Victim01234567890123456789012345678901234567890123456789":"Victim");strcpy(cgs.clientinfo[2].name,"Other");
  {int i;for(i=0;i<3;++i){strcpy((char*)(b+0x2580a18+i*0x1c0),cgs.clientinfo[i].name);cgs.clientinfo[i].team=*(int*)(b+0x2580a98+i*0x1c0)=i==victim?team:2;}}
  strcpy(cg.killerName,"unchanged");strcpy((char*)(b+0x4080504),"unchanged");
  events[0]=0;((void(__cdecl *)(void*))(b+0x3aa50))(rawEvent);strcpy(expected,events);strcpy(killer,(char*)(b+0x4080504));events[0]=0;CG_Obituary(&es);
  if(strcmp(expected,events)||strcmp(killer,cg.killerName)){printf("obituary mismatch mode%d kill%d mod%d rel%d team%d weapon%d\n%s\n%s\nkiller %s / %s\n",mode,kill,mod,rel,team,weapon,expected,events,killer,cg.killerName);return 1;}++count;
 }
 printf("%d whole TC obituary comparisons passed\n",count);return 0;
}
