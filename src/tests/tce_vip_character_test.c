#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static int calls,teamArg,classArg;
static bg_character_t *lookup(int team,int cls){calls++;teamArg=team;classArg=cls;return (bg_character_t*)(0x100000+team*0x1000+cls*16);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;byte*b;bg_character_t*(__cdecl*original)(void*,void*);byte rawCi[512],rawCent[512];clientInfo_t ci;centity_t cent;int mode,team,cls,custom,disguise,disguiseClass,total=0;void*expected,*actual;int ec,et,ek;
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;original=(void*)(b+0x19120);hook(b+0x34f0,lookup);hook(BG_GetCharacter,lookup);
 for(mode=0;mode<5;mode++)for(team=0;team<4;team++)for(cls=0;cls<7;cls++)for(custom=0;custom<2;custom++)for(disguise=0;disguise<2;disguise++)for(disguiseClass=0;disguiseClass<8;disguiseClass++){
  memset(&ci,0,sizeof(ci));memset(&cent,0,sizeof(cent));memset(rawCi,0,sizeof(rawCi));memset(rawCent,0,sizeof(rawCent));
  ci.team=*(int*)(rawCi+0x88)=team;ci.cls=*(int*)(rawCi+0xac)=cls;ci.character=*(bg_character_t**)(rawCi+0x16c)=custom?(void*)0x765430:NULL;
  cent.currentState.eType=mode>=2?ET_CORPSE:ET_PLAYER;*(int*)(rawCent+4)=mode>=2?41:1;
  cent.currentState.modelindex=*(int*)(rawCent+0xa4)=mode==4?team+4:team;/* Corpse class is onFireEnd in both native layouts. */
  cent.currentState.onFireEnd=*(int*)(rawCent+0x100)=cls;
  cent.currentState.onFireStart=*(int*)(rawCent+0xfc)=mode==3?3:-1;
  cgs.gameCharacters[3]=*(bg_character_t**)(b+0x257f5cc+12)=(void*)0x654320;
  cent.currentState.powerups=disguise?((1<<PW_OPS_DISGUISED)|(disguiseClass<<PW_OPS_CLASS_1)):0;*(int*)(rawCent+0xe4)=disguise?(0x100|(disguiseClass<<9)):0;
  calls=0;teamArg=classArg=-999;expected=original(rawCi,mode?rawCent:NULL);ec=calls;et=teamArg;ek=classArg;
  calls=0;teamArg=classArg=-999;actual=CG_CharacterForClientinfo(&ci,mode?&cent:NULL);
  if(expected!=actual||ec!=calls||et!=teamArg||ek!=classArg){printf("character case%d mode%d team%d class%d mismatch\n",total,mode,team,cls);return 1;}total++;
 }
 printf("%d whole CG_CharacterForClientinfo comparisons passed including VIP class6\n",total);return 0;
}
