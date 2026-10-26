#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern qboolean G_IsAllowedAmmo(gentity_t*);
extern void trigger_ammo_think(gentity_t*);
extern void trigger_ammo_setup(gentity_t*);
static byte raw[0x57c],rawTarget[0x57c],rawClient[0x2000];
static gentity_t ent,target;static gclient_t client;
static int side,scenario,calls,bad,ammoResult,found;static jmp_buf jump;
static int fatals;static char fatalText[1024];static char targetName[]="supply cabinet";
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static int addAmmo(void*p,int count){calls++;if(p!=(side?(void*)&ent:(void*)raw)||count)bad=1;return ammoResult;}
static void *findTarget(void*p,char *name){calls++;if(p||name!=targetName)bad=1;return found?(side?(void*)&target:(void*)rawTarget):NULL;}
static void fatal(const char *s){fatals++;Q_strncpyz(fatalText,s,sizeof(fatalText));longjmp(jump,1);}
static void setup(int n){memset(raw,0,sizeof(raw));memset(&ent,0,sizeof(ent));ent.client=&client;*(void**)(raw+0x190)=rawClient;
 ent.health=*(int*)(raw+0x2c8)=n%33-16;ent.damage=*(int*)(raw+0x2d0)=n%19-9;ent.count=*(int*)(raw+0x2e4)=n%41-20;
 ent.nextthink=*(int*)(raw+0x29c)=1234567;ent.target=targetName;*(void**)(raw+0x254)=targetName;
 ent.target_ent=&target;*(void**)(raw+0x264)=rawTarget;ent.think=NULL;*(void**)(raw+0x2a4)=NULL;
}
int main(int argc,char **argv){HMODULE dll;byte*b;int(__cdecl*allowed)(void*);void(__cdecl*think)(void*);void(__cdecl*setupFn)(void*);int i,x,y,c,n;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;allowed=(void*)(b+0x92a50);think=(void*)(b+0x92c30);setupFn=(void*)(b+0x92c70);
 if(!hook(b+0x5c900,addAmmo)||!hook(AddMagicAmmo,addAmmo)||!hook(b+0x94960,findTarget)||!hook(G_FindByTargetname,findTarget)||!hook(b+0x8ab30,fatal)||!hook(trap_Error,fatal))return 2;
 for(i=0;i<4096;i++){
  void *arg;gentity_t *nativeArg;setup(i);ammoResult=(i%7)-3;arg=raw;nativeArg=&ent;
  if((i&3)==0){arg=NULL;nativeArg=NULL;}else if((i&3)==1){ent.client=NULL;*(void**)(raw+0x190)=NULL;}
  side=0;bad=calls=0;x=allowed(arg);c=calls;if(bad)return 3;
  side=1;bad=calls=0;y=G_IsAllowedAmmo(nativeArg);
  if(bad||x!=y||c!=calls){fprintf(stderr,"allowed ammo case%d result%d/%d calls%d/%d\n",i,y,x,calls,c);return 1;}
 }
 for(i=0;i<16384;i++){
  byte before[0x57c];gentity_t nativeBefore;setup(i);level.time=*(int*)(b+0x7febe0)=i*123;
  if(i&1024)ent.count=*(int*)(raw+0x2e4)=-9999;
  if(i&2048)ent.health=*(int*)(raw+0x2c8)=16777217;
  if(i&4096)ent.count=*(int*)(raw+0x2e4)=16777219;
  memcpy(before,raw,sizeof(raw));nativeBefore=ent;think(raw);trigger_ammo_think(&ent);
  if(ent.health!=*(int*)(raw+0x2c8)||ent.nextthink!=*(int*)(raw+0x29c)){fprintf(stderr,"ammo think case%d mismatch\n",i);return 1;}
  memcpy(before+0x2c8,raw+0x2c8,4);memcpy(before+0x29c,raw+0x29c,4);nativeBefore.health=ent.health;nativeBefore.nextthink=ent.nextthink;
  if(memcmp(before,raw,sizeof(raw))||memcmp(&nativeBefore,&ent,sizeof(ent))){fprintf(stderr,"ammo think case%d unexpected write\n",i);return 1;}
 }
 for(n=0;n<2;n++)for(i=0;i<2048;i++){
  int originalNext,originalFatal,originalCalls,originalThink;char expected[1024];setup(i);found=n;level.time=*(int*)(b+0x7febe0)=i*127;
  if(i&1)ent.count=*(int*)(raw+0x2e4)=-9999;
  /* Existing callback must remain untouched for an unlimited cabinet. */
  ent.think=G_MakeReady;*(void**)(raw+0x2a4)=b+0x543a0;
  side=0;bad=calls=fatals=0;fatalText[0]=0;if(!setjmp(jump))setupFn(raw);
  if(bad)return 3;originalNext=*(int*)(raw+0x29c);originalFatal=fatals;originalCalls=calls;originalThink=*(void**)(raw+0x2a4)==b+0x92c30;Q_strncpyz(expected,fatalText,sizeof(expected));
  side=1;bad=calls=fatals=0;fatalText[0]=0;if(!setjmp(jump))trigger_ammo_setup(&ent);
  if(bad||calls!=originalCalls||calls!=1||fatals!=originalFatal||fatals!=!found||strcmp(expected,fatalText)||ent.nextthink!=originalNext||originalThink!=(ent.think==trigger_ammo_think)||!!ent.target_ent!=!!*(void**)(raw+0x264)){
   fprintf(stderr,"ammo setup found%d case%d mismatch fatal%d/%d calls%d/%d\n",found,i,fatals,originalFatal,calls,originalCalls);return 1;
  }
  if(!found&&strcmp(fatalText,"trigger_ammo failed to find target: supply cabinet\n")){fprintf(stderr,"ammo setup fatal text mismatch: %s\n",fatalText);return 1;}
 }
 printf("4096 allowed-ammo, 16384 regeneration, 4096 setup comparisons (2048 fatal) passed\n");return 0;
}
