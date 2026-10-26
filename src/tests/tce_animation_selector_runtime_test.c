#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern pmove_t *pm;
extern int PM_AttackAnimForWeapon(int);
extern int PM_LastAttackAnimForWeapon(int);
extern int PM_ReloadAnimForWeapon(int);
int main(int argc,char**argv){
 HMODULE h;byte*b,rawpm[0x200]={0};pmove_t move={0};int skills[8]={0};
 int k,w,s,parsed,n=0,e,a;int(__cdecl*orig[3])(int);int(*source[3])(int)={PM_AttackAnimForWeapon,PM_LastAttackAnimForWeapon,PM_ReloadAnimForWeapon};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(byte*)h;
 orig[0]=(void*)(b+0x8770);orig[1]=(void*)(b+0x87d0);orig[2]=(void*)(b+0x8870);
 *(void**)(b+0x4ba9edc)=rawpm;*(void**)(rawpm+0x118)=skills;pm=&move;move.skill=skills;
 for(k=0;k<3;k++)for(w=-4;w<76;w++)for(s=0;s<5;s++)for(parsed=0;parsed<2;parsed++){
  gearDef.parsed=parsed;skills[SK_LIGHT_WEAPONS]=s;e=orig[k](w);a=source[k](w);
  if(e!=a){printf("selector mismatch kind%d weapon%d skill%d parsed%d %d/%d\n",k,w,s,parsed,e,a);return 1;}n++;
 }
 printf("PASS %d actual Attack/LastAttack/Reload selector comparisons, no hooks\n",n);return 0;
}
