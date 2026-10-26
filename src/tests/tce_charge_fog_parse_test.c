#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>
static char input[1024];static int indexSeen,requests,count;
typedef struct {int type,nearDist,farDist;float r,g,b,density;} fog_t;
static fog_t fogs[2];
static const char*config(int i){++requests;indexSeen=i;return input;}
static void fog(int type,int ne,int fa,float r,float g,float b,float d){fog_t*f=&fogs[count++];f->type=type;f->nearDist=ne;f->farDist=fa;f->r=r;f->g=g;f->b=b;f->density=d;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*charge)(void);void(__cdecl*parseFog)(void);int c,i,n=0;const char*keys[]={"axs_sld","ald_sld","axs_mdc","ald_mdc","axs_eng","ald_eng","axs_lnt","ald_lnt","axs_cvo","ald_cvo"};int offsets[]={0x408a364,0x408a368,0x408a374,0x408a378,0x408a36c,0x408a370,0x408a35c,0x408a360,0x408a37c,0x408a380};int*fields[]={&cg.soldierChargeTime[0],&cg.soldierChargeTime[1],&cg.medicChargeTime[0],&cg.medicChargeTime[1],&cg.engineerChargeTime[0],&cg.engineerChargeTime[1],&cg.ltChargeTime[0],&cg.ltChargeTime[1],&cg.covertopsChargeTime[0],&cg.covertopsChargeTime[1]};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;charge=(void*)(b+0x5bf10);parseFog=(void*)(b+0x5bc70);hook(b+0x481a0,config);hook(CG_ConfigString,config);hook(b+0x65380,fog);hook(trap_R_SetFog,fog);
 for(c=0;c<12000;++c){int er,ei,ec;fog_t expected[2];input[0]=0;for(i=0;i<10;++i)if((c>>i)&1){char part[80];sprintf(part,"\\%s\\%d",keys[i],c*i-15000);strcat(input,part);}requests=0;indexSeen=-1;charge();er=requests;ei=indexSeen;requests=0;indexSeen=-1;CG_ChargeTimesChanged();if(requests!=er||indexSeen!=ei)return 1;for(i=0;i<10;++i)if(*(int*)(b+offsets[i])!=*fields[i]){printf("charge%d/%d\n",c,i);return 1;}++n;
  sprintf(input,"%d.25 %d.75 0.25 0.5 0.75 1 %d %d",c%251-100,c%7?c%901:0,c-5000,c%3==0);if(c%7==0)sprintf(input,"0 0 0 0 0 0 %d 0",c);if(c%31==0)strcpy(input,"");count=requests=0;indexSeen=-1;memset(fogs,0,sizeof(fogs));parseFog();ec=count;er=requests;ei=indexSeen;memcpy(expected,fogs,sizeof(fogs));count=requests=0;indexSeen=-1;memset(fogs,0,sizeof(fogs));CG_ParseFog();if(count!=ec||requests!=er||indexSeen!=ei||memcmp(expected,fogs,sizeof(fogs))){printf("fog%d\n",c);return 1;}++n;
 }
 printf("%d whole charge/fog parser comparisons passed\n",n);return 0;
}
