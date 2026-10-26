#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
int main(int argc,char**argv){
 HMODULE h;byte*b,raw[0x700],rawpm[0x200];playerState_t ps;pmove_t move;
 int material,bits,prone,which,cases=0,expected,actual;unsigned flags;
 int(__cdecl*ground)(void);char(__cdecl*ladder)(void);
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(byte*)h;ground=(void*)(b+0xec70);ladder=(void*)(b+0xfb90);
 memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));
 pm=&move;move.ps=&ps;*(void**)(b+0x4ba9edc)=rawpm;*(void**)rawpm=raw;
 for(material=0;material<256;material++)for(bits=0;bits<64;bits++)for(prone=0;prone<2;prone++){
  flags=((unsigned)material<<24)|((bits&1)?8:0)|((bits&2)?0x2000:0)|((bits&4)?0x4000:0)|((bits&8)?0x40000:0)|((bits&16)?0x100:0)|((bits&32)?0x40:0);
  pml.groundTrace.surfaceFlags=*(int*)(b+0x4ba9ea0)=flags;pml.tceLadderSurfaceFlags=*(int*)(b+0x4ba9ed8)=flags;
  ps.eFlags=*(int*)(raw+0x68)=prone?EF_PRONE:0;
  for(which=0;which<2;which++){expected=which?ladder():ground();actual=which?PM_TCELadderFootstepForSurface():PM_FootstepForSurface();
   if(expected!=actual){printf("step mismatch %08x prone%d kind%d %d/%d\n",flags,prone,which,expected,actual);return 1;}cases++;}
 }
 printf("PASS whole ground/ladder material dispatch %d cases (32768 each)\n",cases);return 0;
}
