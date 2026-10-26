#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static multiDef_t pools[2]; static int side,at,nt,reads,floats,errors,allocs,strings,failFloat,failAlloc;
static const char*tokens[256];static char labels[34][32],interned[128][1024],expectedStrings[128][1024];
static void*allocate(int n){assert(n==sizeof(multiDef_t));++allocs;return failAlloc?NULL:&pools[side];}
static int readToken(int h,pc_token_t*t){assert(h==71);++reads;if(at==nt)return 0;memset(t,0,sizeof(*t));strcpy(t->string,tokens[at++]);return 1;}
static const char*stringAlloc(const char*s){assert(strings<128);strcpy(interned[strings],s);return interned[strings++];}
static qboolean parseFloat(int h,float*out){assert(h==71);++floats;if(floats==failFloat)return qfalse;*out=(float)(floats*0.173-3.21);return qtrue;}
static void sourceError(int h,char*fmt,...){assert(h==71&&!strcmp(fmt,"end of file inside menu item\n"));++errors;}
static struct{byte*p,saved[5];}hooks[10];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b;itemDef_t item[2],norm[2];int mode,n,shape,existing,fi,i,r[2],stats[2][6],cases=0;
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(itemDef_t)==0x274&&sizeof(multiDef_t)==0x18c);
 patch(b+0x148a0,allocate);patch(UI_Alloc,allocate);patch(b+0x20dd0,readToken);patch(trap_PC_ReadToken,readToken);patch(b+0x14910,stringAlloc);patch(String_Alloc,stringAlloc);patch(b+0x11e0,parseFloat);patch(PC_Float_Parse,parseFloat);patch(b+0x1170,sourceError);patch(PC_SourceError,sourceError);
 for(mode=0;mode<2;++mode)for(n=0;n<=33;++n)for(shape=0;shape<6;++shape)for(existing=0;existing<3;++existing)for(fi=0;fi<3;++fi){
  nt=0;failFloat=fi==0?-1:fi==1?1:17;failAlloc=existing==2;
  if(shape!=5){tokens[nt++]=shape==1?"wrong":"{";for(i=0;i<n;++i){sprintf(labels[i],"Choice %d",i);if(shape==2)tokens[nt++]=",";tokens[nt++]=labels[i];if(!mode&&!(shape==4&&i==n-1))tokens[nt++]="value";if(shape==2)tokens[nt++]=";";}if(shape!=3)tokens[nt++]="}";}
  memset(item,0,sizeof(item));memset(pools,0x5a,sizeof(pools));item[0].type=item[1].type=ITEM_TYPE_MULTI;if(existing==1){item[0].typeData=&pools[0];item[1].typeData=&pools[1];}
  for(side=1;side>=0;--side){at=reads=floats=errors=allocs=strings=0;if(side)r[side]=((int(__cdecl*)(itemDef_t*,int))(b+(mode?0x1edc0:0x1ecc0)))(&item[side],71);else r[side]=mode?ItemParse_cvarFloatList(&item[side],71):ItemParse_cvarStrList(&item[side],71);stats[side][0]=at;stats[side][1]=reads;stats[side][2]=floats;stats[side][3]=errors;stats[side][4]=allocs;stats[side][5]=strings;if(side)memcpy(expectedStrings,interned,sizeof(interned));else for(i=0;i<strings;++i)assert(!strcmp(expectedStrings[i],interned[i]));}
  assert(r[0]==r[1]&&!memcmp(stats[0],stats[1],sizeof(stats[0])));assert(!memcmp(pools,pools+1,sizeof(multiDef_t)));assert((item[0].typeData==NULL)==(item[1].typeData==NULL));norm[0]=item[0];norm[1]=item[1];norm[0].typeData=norm[1].typeData=NULL;assert(!memcmp(norm,norm+1,sizeof(itemDef_t)));++cases;
 }
 restore();assert(FreeLibrary(h));printf("%d original cvar list parser comparisons passed\n",cases);return 0;}
