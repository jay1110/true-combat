#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static jmp_buf failure;
static int trace[32],used;
static void hook(byte*p,void*f){DWORD a,b;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a);p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);}
static int oldBody(void*a,void*b,int anim,int cont,int force){trace[used++]=1000+anim;return 0;}
static int newBody(playerState_t*a,animModelInfo_t*b,scriptAnimEventTypes_t anim,qboolean cont,qboolean force){return oldBody(a,b,anim==ANIM_ET_RELOADPRONE?33:anim==ANIM_ET_RELOAD?11:anim,cont,force);}
static void oldEvent(int event){trace[used++]=2000+event;}
static void newEvent(int event,int parm,playerState_t*state){oldEvent(event==EV_FILL_CLIP?35:event);}
static void fatal(int code,const char*format,...){trace[used++]=3000+code;longjmp(failure,1);}
int main(int argc,char**argv){
 HMODULE h;byte*b,raw[0x700],rawpm[0x200],rawext[0x100],rawchar[0x900];
 playerState_t ps;pmove_t move;pmoveExt_t ext;bg_character_t character;int skills[8];
 int k,w,v,j,n=0,expect[32],ec;void(__cdecl*begin)(int);void(__cdecl*finish)(void);
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(byte*)h;begin=(void*)(b+0x96d0);finish=(void*)(b+0x12e70);
 hook(b+0x2520,oldBody);hook((byte*)BG_AnimScriptEvent,newBody);
 hook(b+0x8680,oldEvent);hook((byte*)BG_AddPredictableEventToPlayerstate,newEvent);
 hook(b+0x47c30,fatal);hook((byte*)Com_Error,fatal);
 for(k=0;k<2;k++)for(w=0;w<64;w++)for(v=0;v<448;v++){
  memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));memset(rawext,0,sizeof(rawext));memset(rawchar,0,sizeof(rawchar));
  memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));memset(&character,0,sizeof(character));memset(skills,0,sizeof(skills));
  pm=&move;move.ps=&ps;move.pmext=&ext;move.character=&character;move.skill=skills;
  *(void**)(b+0x4ba9edc)=rawpm;*(void**)rawpm=raw;*(void**)(rawpm+4)=rawext;*(void**)(rawpm+8)=rawchar;*(void**)(rawpm+0x118)=skills;
  skills[SK_LIGHT_WEAPONS]=(v/7)%5;
  ps.weapon=*(int*)(raw+0xa4)=w;ps.weaponstate=*(int*)(raw+0xa8)=v%14;
  ps.weaponTime=*(int*)(raw+0x2c)=(v%7)*250-250;
  ps.pm_type=*(int*)(raw+4)=(v/14)%5;ps.eFlags=*(int*)(raw+0x68)=(v&32)?EF_PRONE:0;
  ps.weapAnim=*(int*)(raw+0x480)=(v&64)?512|8:1;
  move.noWeapClips=*(int*)(rawpm+0x50)=!!(v&128);move.cmd.weapon=rawpm[0x12]=(v&16)?w:0;
  ext.weapAnimTimer=*(int*)(rawext+8)=(v&256)?50:0;
  for(j=0;j<64;j++){
   ps.ammo[j]=(v+j)%4;ps.ammoclip[j]=(v+j)%5;
   memset(&weaponDef[j],0,sizeof(weaponDef[j]));weaponDef[j].maxclip=3;weaponDef[j].reloadTime=777;weaponDef[j].singleReload=!!(v&8);
  }
  memcpy(raw+0x190,ps.ammo,256);memcpy(raw+0x290,ps.ammoclip,256);memcpy(b+0x4c1df20,weaponDef,sizeof(weaponDef));
  used=0;if(!setjmp(failure)){if(k)finish();else begin(w);}ec=used;memcpy(expect,trace,sizeof(trace));
  used=0;if(!setjmp(failure)){if(k)PM_FinishWeaponReload();else PM_BeginWeaponReload(w);}
  if(ec!=used||memcmp(expect,trace,ec*sizeof(int))||ps.weaponstate!=*(int*)(raw+0xa8)||ps.weaponTime!=*(int*)(raw+0x2c)||ps.weapAnim!=*(int*)(raw+0x480)||memcmp(ps.ammo,raw+0x190,256)||memcmp(ps.ammoclip,raw+0x290,256)){
   printf("reload whole mismatch kind%d w%d v%d state%d/%d time%d/%d anim%d/%d trace%d/%d\n",k,w,v,*(int*)(raw+0xa8),ps.weaponstate,*(int*)(raw+0x2c),ps.weaponTime,*(int*)(raw+0x480),ps.weapAnim,ec,used);return 1;
  }n++;
 }
 printf("PASS whole reload begin/finish %d cases (%d each), real inventory and weapon animation callees\n",n,n/2);return 0;
}
