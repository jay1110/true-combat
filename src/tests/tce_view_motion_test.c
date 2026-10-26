#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_view.c"
#include <windows.h>
#include <stdio.h>
static int sets;static char value[80];
static void readvar(const char*n,char*b,int len){Q_strncpyz(b,"0",len);}
static void writevar(const char*n,const char*v){++sets;Q_strncpyz(value,v,sizeof(value));}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;refdef_t ref;int k,j,total=0;void(__cdecl*step)(void),(__cdecl*kick)(void);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;step=(void*)(b+0x6aaa0);kick=(void*)(b+0x67760);
 hook(b+0x64c30,readvar);hook(trap_Cvar_VariableStringBuffer,readvar);hook(b+0x64c10,writevar);hook(trap_Cvar_Set,writevar);
 memset(&ref,0,sizeof(ref));cg.refdef_current=&ref;*(void**)(b+0x40863ac)=&ref;
 for(k=0;k<30000;++k){float z,want[8];int wantTime,wantSets;char wantValue[80];
  cg.time=*(int*)(b+0x407dea4)=1000;cg.stepTime=*(int*)(b+0x407ef5c)=1000-(k%701-200);cg.stepChange=*(float*)(b+0x407ef58)=(k%129-64)*.25f;
  ref.vieworg[2]=32.5f;step();z=ref.vieworg[2];wantTime=*(int*)(b+0x407ef5c);ref.vieworg[2]=32.5f;CG_StepOffset();
  if(cg.stepTime!=wantTime||fabs(ref.vieworg[2]-z)>.00002){printf("step%d %.9g/%.9g\n",k,ref.vieworg[2],z);return 1;}++total;
  cg.frametime=*(int*)(b+0x407dea0)=k%203-2;
  for(j=0;j<3;++j){cg.kickAVel[j]=((float*)(b+0x40810ec))[j]=(k%(67+j*19)-30)*1.5f;cg.kickAngles[j]=((float*)(b+0x40810f8))[j]=(k%(41+j*13)-20)*.25f;}
  cg.recoilPitch=*(float*)(b+0x4081104)=(k%161-80)*.5f;
  sets=0;value[0]=0;kick();wantSets=sets;strcpy(wantValue,value);memcpy(want,b+0x40810ec,sizeof(want));sets=0;value[0]=0;CG_KickAngles();
  if(sets!=wantSets){printf("kickcalls%d\n",k);return 2;}
  for(j=0;j<8;++j){float actual=j<3?cg.kickAVel[j]:j<6?cg.kickAngles[j-3]:j==6?cg.recoilPitch:cg.recoilPitchAngle;if(fabs(actual-want[j])>.00005){printf("kick%d field%d %.9g/%.9g\n",k,j,actual,want[j]);return 3;}}
  if(sets&&fabs(atof(value)-atof(wantValue))>.0001){printf("kickcvar%d %s/%s\n",k,value,wantValue);return 4;}++total;
 }
 printf("%d whole step-offset/kick-angle comparisons passed\n",total);return 0;
}
