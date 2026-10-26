#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
static int key,keyOK,scriptOK,keyCalls,scriptCalls;static const char*script;static menuDef_t*active;
static qboolean character(int h,char*out){assert(h==71);++keyCalls;if(!keyOK)return qfalse;*out=(char)key;return qtrue;}
static qboolean integer(int h,int*out){assert(h==71);++keyCalls;if(!keyOK)return qfalse;*out=key;return qtrue;}
static qboolean parse(int h,const char**out){assert(h==71&&out==&active->onKey[key]);++scriptCalls;if(!scriptOK)return qfalse;*out=script;return qtrue;}
static struct{byte*p,saved[5];}hooks[6];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b;menuDef_t m[2];int mode,k,rep,er,sr,ek,es,cases=0;const char*scripts[]={"","close self ;","setcvar ui_probe 1 ; open next ;"};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(menuDef_t)==0x718&&offsetof(menuDef_t,onKey)==0xf4);
 patch(b+0x14d00,character);patch(PC_Char_Parse,character);patch(b+0x12d0,integer);patch(PC_Int_Parse,integer);patch(b+0x14d40,parse);patch(PC_Script_Parse,parse);
 for(mode=0;mode<2;++mode)for(key=0;key<(mode?256:128);++key)for(keyOK=0;keyOK<2;++keyOK)for(scriptOK=0;scriptOK<2;++scriptOK)for(k=0;k<3;++k){memset(m,k*47+3,sizeof(m));script=scripts[k];for(rep=0;rep<2;++rep){active=m+1;keyCalls=scriptCalls=0;er=((int(__cdecl*)(itemDef_t*,int))(b+(mode?0x1f990:0x1f940)))((itemDef_t*)active,71);ek=keyCalls;es=scriptCalls;active=m;keyCalls=scriptCalls=0;sr=mode?MenuParse_execKeyInt((itemDef_t*)active,71):MenuParse_execKey((itemDef_t*)active,71);assert(er==sr&&ek==keyCalls&&es==scriptCalls&&!memcmp(m,m+1,sizeof(menuDef_t)));++cases;}}
 restore();assert(FreeLibrary(h));printf("%d original menu hotkey binding comparisons passed\n",cases);return 0;}
