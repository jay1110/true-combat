#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
extern displayContextDef_t *DC;
extern void Item_ValidateTypeData(itemDef_t *);
extern qboolean ItemParse_model_angle(itemDef_t *,int);
extern qboolean ItemParse_model_fovx(itemDef_t *,int);
extern qboolean ItemParse_model_animplay(itemDef_t *,int);
extern qboolean ItemParse_model_origin(itemDef_t *,int);
extern qboolean ItemParse_model_fovy(itemDef_t *,int);
extern qboolean ItemParse_model_rotation(itemDef_t *,int);
static int calls,failAt,seed;
static void patch(void *a,void *f){DWORD o,t;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void validate(void *i){} /* Preallocated model; allocator is a separate dependency. */
static int integer(int handle,int *v){if(calls++==failAt)return 0;*v=seed*7+calls;return 1;}
static int number(int handle,float *v){if(calls++==failAt)return 0;*v=seed*.125f;return 1;}
int main(int argc,char **argv){HMODULE h;unsigned char *b;static unsigned char item[0x400],dc[0x1e400];static displayContextDef_t ctx;itemDef_t own;modelDef_t a,c;int mode,r1,r2,n=0,expectedCalls;int offsets[]={0x1e3b0,0x1e320,0x1e3e0,0x1e2c0,0x1e350,0x1e380};
 qboolean (*parsers[])(itemDef_t *,int)={ItemParse_model_angle,ItemParse_model_fovx,ItemParse_model_animplay,ItemParse_model_origin,ItemParse_model_fovy,ItemParse_model_rotation};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;DC=&ctx;*(void **)(b+0x245fa4)=dc;
 patch(b+0x1def0,validate);patch(Item_ValidateTypeData,validate);patch(b+0x12d0,integer);patch(PC_Int_Parse,integer);patch(b+0x11e0,number);patch(PC_Float_Parse,number);
 for(mode=0;mode<6;++mode)for(seed=-8;seed<32;++seed)for(failAt=-1;failAt<5;++failAt){
  memset(&a,0x35,sizeof(a));c=a;memset(&own,0,sizeof(own));own.typeData=&c;*(void **)(item+0x250)=&a;ctx.realTime=*(int *)(dc+0x120)=seed*31;
  calls=0;r1=((int(__cdecl *)(void *,int))(b+offsets[mode]))(item,7);expectedCalls=calls;calls=0;
  r2=parsers[mode](&own,7);
  if(r1!=r2||expectedCalls!=calls||memcmp(&a,&c,sizeof(a))){printf("model parser mismatch %d %d %d\n",mode,seed,failAt);return 1;}++n;
 }
 printf("%d original model origin/rotation/angle/fov/animation parser comparisons passed\n",n);return 0;
}
