#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static byte pools[2][1024];static char names[2][128];static int side,allocs,sz,strings,floats,failStage;
static void*allocate(int n){assert(n>0&&n<=1024);++allocs;sz=n;return pools[side];}
static qboolean parseString(int h,const char**out){assert(h==71);++strings;if(failStage==1)return qfalse;*out=names[side];return qtrue;}
static qboolean parseFloat(int h,float*out){assert(h==71);++floats;if(failStage==floats+1)return qfalse;*out=(float)(floats*0.713-5.93);return qtrue;}
static struct{byte*p,saved[5];}hooks[6];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b;itemDef_t items[2],norm[2];int mode,type,existing,k,n,rep,r[2],stats[2][4],cases=0;const char*inputs[]={"","UI_R_Mode","cg_toggleAiming","Mixed_CASE-123"};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(itemDef_t)==0x274);
 patch(b+0x148a0,allocate);patch(UI_Alloc,allocate);patch(b+0x1370,parseString);patch(PC_String_Parse,parseString);patch(b+0x11e0,parseFloat);patch(PC_Float_Parse,parseFloat);
 for(mode=0;mode<2;++mode)for(type=-1;type<=18;++type)for(existing=0;existing<2;++existing)for(k=0;k<3;++k)for(n=0;n<4;++n)for(failStage=0;failStage<5;++failStage){
  memset(items,0,sizeof(items));memset(pools,k*71+13,sizeof(pools));items[0].type=items[1].type=type;strcpy(names[0],inputs[n]);strcpy(names[1],inputs[n]);if(existing){items[0].typeData=pools[0];items[1].typeData=pools[1];}
  for(rep=0;rep<2;++rep){for(side=1;side>=0;--side){allocs=sz=strings=floats=0;if(side)r[side]=((int(__cdecl*)(itemDef_t*,int))(b+(mode?0x1ec50:0x1eb30)))(&items[side],71);else r[side]=mode?ItemParse_cvarFloat(&items[side],71):ItemParse_cvar(&items[side],71);stats[side][0]=allocs;stats[side][1]=sz;stats[side][2]=strings;stats[side][3]=floats;}
   assert(r[0]==r[1]&&!memcmp(stats[0],stats[1],sizeof(stats[0])));assert(!memcmp(pools[0],pools[1],1024));assert(!strcmp(names[0],names[1]));assert((items[0].cvar==NULL)==(items[1].cvar==NULL));assert((items[0].typeData==NULL)==(items[1].typeData==NULL));norm[0]=items[0];norm[1]=items[1];norm[0].typeData=norm[1].typeData=NULL;norm[0].cvar=norm[1].cvar=NULL;assert(!memcmp(norm,norm+1,sizeof(itemDef_t)));++cases;
  }
 }
 restore();assert(FreeLibrary(h));printf("%d original cvar binding comparisons passed\n",cases);return 0;}
