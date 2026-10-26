#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_CheckAmmo(void);
static int mapping[64],trace[64],calls;
static int lookup(int weapon){trace[calls++]=weapon;return mapping[weapon];}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;static byte snapshot[4096];static snapshot_t snap;void(__cdecl*original)(void);int c,w,expected,expectedCalls,expectedTrace[64];unsigned masks[]={0,1,0x80000000u,0xffffffffu,0x55555555u,0xaaaaaaaaU};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x568b0);*(void**)(b+0x40352c4)=snapshot;cg.snap=&snap;
 hook(b+0x3800,lookup);hook(BG_FindAmmoForWeapon,lookup);
 for(c=0;c<36000;++c){
  *(unsigned*)(snapshot+0x400)=snap.ps.weapons[0]=masks[c%6];*(unsigned*)(snapshot+0x404)=snap.ps.weapons[1]=(c/6)%3?0:0xffffffffu;
  for(w=0;w<64;++w){mapping[w]=(w+(c/18))%64;*(int*)(snapshot+0x1bc+4*w)=snap.ps.ammo[w]=((c/6+w*7)%12)-3;}
  cg.lowAmmoWarning=*(int*)(b+0x4080d8c)=c%5-1;
  calls=0;original();expected=*(int*)(b+0x4080d8c);expectedCalls=calls;memcpy(expectedTrace,trace,calls*sizeof(int));
  calls=0;CG_CheckAmmo();if(cg.lowAmmoWarning!=expected||calls!=expectedCalls||memcmp(trace,expectedTrace,calls*sizeof(int))){printf("ammo case%d warning%d/%d calls%d/%d\n",c,cg.lowAmmoWarning,expected,calls,expectedCalls);return 1;}
 }
 printf("36000 whole CG_CheckAmmo controller comparisons passed\n");return 0;
}
