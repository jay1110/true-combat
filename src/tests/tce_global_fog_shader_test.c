#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>
static char input[512],remaps[1024];static int requests,indexSeen,count,restore,duration;static float values[4];
static const char*config(int i){++requests;indexSeen=i;return input;}
static void fog(int re,int du,float r,float g,float b,float depth){++count;restore=re;duration=du;values[0]=r;values[1]=g;values[2]=b;values[3]=depth;}
static void remap(const char*a,const char*b,const char*c){++count;strcat(remaps,a);strcat(remaps,"|");strcat(remaps,b);strcat(remaps,"|");strcat(remaps,c);strcat(remaps,";");}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*originalFog)(void);void(__cdecl*originalShader)(void);int c,i,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;originalFog=(void*)(b+0x5f950);originalShader=(void*)(b+0x5be00);hook(b+0x481a0,config);hook(CG_ConfigString,config);hook(b+0x653b0,fog);hook(trap_R_SetGlobalFog,fog);hook(b+0x65530,remap);hook(trap_R_RemapShader,remap);
 for(i=0;i<16;++i){sprintf(cgs.gameShaderNames[i],"shader/%d",i);strcpy((char*)b+0x257e94c+64*i,cgs.gameShaderNames[i]);}
 for(c=0;c<12000;++c){int ec,er,ei,re,du;float ev[4];char expected[1024];sprintf(input,"%d %d 0.25 -0.5 1.5 %d.75",c%3-1,c-4000,c%999);if(c%13==0)strcpy(input,"");count=requests=0;indexSeen=-1;restore=duration=-999;memset(values,0x55,16);originalFog();ec=count;er=requests;ei=indexSeen;re=restore;du=duration;memcpy(ev,values,16);count=requests=0;indexSeen=-1;restore=duration=-999;memset(values,0x55,16);CG_ParseGlobalFog();if(ec!=count||er!=requests||ei!=indexSeen||re!=restore||du!=duration||memcmp(ev,values,16)){printf("global fog%d\n",c);return 1;}++n;
  sprintf(input,"%d=%d:%d@%d=%d:0.5@",c%16,(c+5)%16,c-5000,(c+1)%16,(c+7)%16);if(c%7==0)strcpy(input,"1=2:3");if(c%11==0)strcpy(input,"1=2");if(c%17==0)strcpy(input,"");count=requests=0;indexSeen=-1;remaps[0]=0;originalShader();ec=count;er=requests;ei=indexSeen;strcpy(expected,remaps);count=requests=0;indexSeen=-1;remaps[0]=0;CG_ShaderStateChanged();if(ec!=count||er!=requests||ei!=indexSeen||strcmp(expected,remaps)){printf("shader%d\n",c);return 1;}++n;
 }
 printf("%d whole global-fog/shader parser comparisons passed\n",n);return 0;
}

