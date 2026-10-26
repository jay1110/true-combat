#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_ParseServerVersionInfo(const char*);
extern void CG_ParseReinforcementTimes(const char*);
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*version)(const char*);void(__cdecl*reinforce)(const char*);int c,j,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;version=(void*)(b+0x5baa0);reinforce=(void*)(b+0x5bac0);
 for(c=0;c<16000;++c){char text[512],part[32];int expected[2];sprintf(text,"%d %d",((c%12)-2)*8,(((c/12)%12)-2)*4);
  for(j=0;j<c%9;++j){sprintf(part," %d",(c*13+j*711)%25001-1000);strcat(text,part);}if(c%17==0)strcpy(text,"");if(c%19==0)strcpy(text,"invalid missing tokens");if(c%23==0)strcpy(text,"   16   8 111 222 333 444 555 666 777 888");
  cgs.aReinfOffset[TEAM_AXIS]=*(int*)(b+0x3fc42d8)=-111;cgs.aReinfOffset[TEAM_ALLIES]=*(int*)(b+0x3fc42dc)=-222;
  reinforce(text);expected[0]=*(int*)(b+0x3fc42d8);expected[1]=*(int*)(b+0x3fc42dc);CG_ParseReinforcementTimes(text);if(expected[0]!=cgs.aReinfOffset[TEAM_AXIS]||expected[1]!=cgs.aReinfOffset[TEAM_ALLIES]){printf("reinforcement case%d input%s\n",c,text);return 1;}++n;
  version(text);CG_ParseServerVersionInfo(text);if(*(int*)(b+0x3fc42f4)!=cgs.game_versioninfo){printf("version case%d\n",c);return 1;}++n;
 }
 printf("%d whole reinforcement/version parse comparisons passed\n",n);return 0;
}
