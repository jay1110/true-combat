#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_ShakeCamera(void);
int main(int argc,char**argv){HMODULE dll;byte*b;int k,j;void(__cdecl*original)(void);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x21cd0);
 for(k=0;k<60000;++k){float expected[12],before[12];
  cg.time=*(int*)(b+0x407dea4)=5000;
  cg.cameraShakeTime=*(int*)(b+0x4081560)=5000+(k%3001-1000);
  cg.cameraShakeLength=*(float*)(b+0x408156c)=(k%2000+1)*.5f;
  cg.cameraShakeScale=*(float*)(b+0x4081568)=(k%401-200)*.01f;
  cg.cameraShakePhase=*(float*)(b+0x4081564)=(k%631-315)*.01f;
  for(j=0;j<3;++j){cg.refdef.vieworg[j]=((float*)(b+0x407f01c))[j]=(k%(47+j*6)-20)*.5f;
   cg.refdefViewAngles[j]=((float*)(b+0x407f1b0))[j]=(k%(90+j*17)-30)*.5f;}
  for(j=0;j<9;++j)((float*)cg.refdef.viewaxis)[j]=((float*)(b+0x407f028))[j]=j*.25f;
  original();memcpy(expected,b+0x407f01c,sizeof(expected));CG_ShakeCamera();
  memcpy(before,cg.refdef.vieworg,12);memcpy(before+3,cg.refdef.viewaxis,36);
  if(cg.cameraShakeScale!=*(float*)(b+0x4081568)){printf("scale case%d\n",k);return 1;}
  for(j=0;j<12;++j)if(fabs(before[j]-expected[j])>0.0002){printf("shake case%d field%d %.9g/%.9g\n",k,j,before[j],expected[j]);return 1;}
 }
 puts("60000 whole shake-camera comparisons passed");return 0;
}
