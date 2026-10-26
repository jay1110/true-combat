#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
typedef struct {int count,order[2],entity,weapon,second,flags,last1,last2;float origin[3],color[3],wallOrigin[3],dir1[3],dir2[3];int alias;} output_t;
static output_t output;
static void bleed(vec3_t origin,int entity,vec3_t color){output.order[output.count++]=1;output.entity=entity;memcpy(output.origin,origin,12);memcpy(output.color,color,12);}
static void wall(int weapon,int second,vec3_t origin,vec3_t dir1,vec3_t dir2,unsigned flags,int a,int b){output.order[output.count++]=2;output.weapon=weapon;output.second=second;output.flags=flags;output.last1=a;output.last2=b;memcpy(output.wallOrigin,origin,12);memcpy(output.dir1,dir1,12);memcpy(output.dir2,dir2,12);output.alias=dir1==dir2;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*original)(void*,int,float*,float*,int);int c,w,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x7b150);hook(b+0x2be80,bleed);hook(CG_Bleed,bleed);hook(b+0x78260,wall);hook(CG_TCEMissileHitWall,wall);
 for(w=-2;w<=68;++w)for(c=0;c<320;++c){output_t expected;vec3_t origin={(float)c-45,(float)c*.25f,15},dir={-.5f,(float)c*.03125f,1};void*cent=(void*)(0x10000+c*16);
  memset(&output,0,sizeof(output));original(cent,w,origin,dir,c%66-1);expected=output;memset(&output,0,sizeof(output));CG_MissileHitPlayer(cent,w,origin,dir,c%66-1);if(memcmp(&expected,&output,sizeof(output))){printf("missile player weapon%d fixture%d\n",w,c);return 1;}++n;
 }
 printf("%d whole missile-hit-player comparisons passed\n",n);return 0;
}
