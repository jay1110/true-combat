#include "../game/tce_attack.h"
#include <windows.h>
#include <stdio.h>
int main(int argc,char **argv) {
 HMODULE dll; unsigned char *base; int i;
 int (__cdecl *attack)(int),(__cdecl *last)(int),(__cdecl *drop)(int),(__cdecl *raise)(int);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
 base=(unsigned char *)dll; attack=(void *)(base+0x8770);last=(void *)(base+0x87d0);
 drop=(void *)(base+0x8930);raise=(void *)(base+0x88c0);
 for(i=-32;i<256;++i) {
  if(attack(i)!=TCE_PM_AttackAnimForWeapon(i)||last(i)!=TCE_PM_LastAttackAnimForWeapon(i)||drop(i)!=TCE_PM_DropAnimForWeapon(i)||raise(i)!=TCE_PM_RaiseAnimForWeapon(i)) {
   fprintf(stderr,"Attack selector mismatch: %d\n",i);return 1;
  }
 }
 FreeLibrary(dll);puts("1152 original attack/raise/drop selector results matched");return 0;
}
