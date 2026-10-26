#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>
static char config[256],args[9][32];static int calls[16],count;
static const char*configuration(int index){calls[count++]=index;return config;}
static const char*argument(int index){calls[count++]=index;return args[index];}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*fade)(void);void(__cdecl*portal)(void);int c,i,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;fade=(void*)(b+0x5f8e0);portal=(void*)(b+0x5d2f0);hook(b+0x481a0,configuration);hook(CG_ConfigString,configuration);hook(b+0x47cb0,argument);hook(CG_Argv,argument);
 for(c=0;c<12000;++c){int ec,expectedCalls[16],es,ed;float ea,ecur;
  sprintf(config,"%s %d %d",c%4==0?"0.5":c%4==1?"-1":c%4==2?"1.25":"bad",c%1000-500,c%503-2);if(c%17==0)strcpy(config,"");cg.time=*(int*)(b+0x407dea4)=c%1500-700;cgs.fadeAlphaCurrent=*(float*)(b+0x2587ff8)=.375f;
  count=0;fade();ec=count;memcpy(expectedCalls,calls,count*sizeof(int));ea=*(float*)(b+0x2587ff4);ecur=*(float*)(b+0x2587ff8);es=*(int*)(b+0x2587ffc);ed=*(int*)(b+0x2588000);count=0;CG_ParseScreenFade();if(ec!=count||memcmp(expectedCalls,calls,count*sizeof(int))||ea!=cgs.fadeAlpha||ecur!=cgs.fadeAlphaCurrent||es!=cgs.fadeStartTime||ed!=cgs.fadeDuration){printf("fade parse%d\n",c);return 1;}++n;
  for(i=1;i<9;++i)sprintf(args[i],"%d.75",(c*i)%4001-2000);if(c%7==0)strcpy(args[c%8+1],"bad");count=0;portal();ec=count;memcpy(expectedCalls,calls,count*sizeof(int));count=0;CG_ParsePortalPos();if(ec!=count||memcmp(expectedCalls,calls,count*sizeof(int))||*(int*)(b+0x3fde3b8)!=cgs.ccCurrentCamObjective||*(int*)(b+0x3fde3b0)!=cgs.ccPortalEnt||memcmp(b+0x3fde398,cgs.ccPortalPos,12)||memcmp(b+0x3fde3a4,cgs.ccPortalAngles,12)){printf("portal parse%d\n",c);return 1;}++n;
 }
 printf("%d whole view-command parser comparisons passed\n",n);return 0;
}
