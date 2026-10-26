#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static itemDef_t pool;static byte rawPool[0x280];static int original,failAlloc,parseOK,allocations,parses;static float floatValue;
static void*allocate(int n){assert(n==(original?0x274:sizeof(itemDef_t)));++allocations;return failAlloc?NULL:original?(void*)rawPool:(void*)&pool;}
static qboolean parse(int h,float*out){assert(h==71);++parses;if(!parseOK)return qfalse;*out=floatValue;return qtrue;}
static int metricWidth,metricHeight,fontSelected,metricCalls;
static void selectedFont(int n){fontSelected=n;metricCalls=1;}
static int height(const char*s,float z,int l){assert(metricCalls==1&&l==0&&!strcmp(s,"Tooltip"));metricCalls=2;return metricHeight;}
static int width(const char*s,float z,int l){assert(metricCalls==2&&l==0&&!strcmp(s,"Tooltip"));metricCalls=3;return metricWidth;}
static struct{byte*p,saved[5];}hooks[4];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b,parent[0x280];itemDef_t item;int mode,existing,k,v,rep,er,sr,ea,ep,cases=0;const float values[]={0.0f,-13.7f,0.33333334f,250.91f};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(itemDef_t)==0x274);
 patch(b+0x148a0,allocate);patch(UI_Alloc,allocate);patch(b+0x11e0,parse);patch(PC_Float_Parse,parse);
 for(mode=0;mode<2;++mode)for(existing=0;existing<2;++existing)for(failAlloc=0;failAlloc<2;++failAlloc)for(parseOK=0;parseOK<2;++parseOK)for(k=0;k<8;++k)for(v=0;v<4;++v){floatValue=values[v];memset(&item,0,sizeof(item));memset(parent,0,sizeof(parent));memset(&pool,k*31,sizeof(pool));memset(rawPool,k*31,sizeof(rawPool));if(existing){item.toolTipData=&pool;*(void**)(parent+0x270)=rawPool;}
  for(rep=0;rep<2;++rep){original=1;allocations=parses=0;er=((int(__cdecl*)(void*,int))(b+(mode?0x1f2a0:0x1f260)))(parent,71);ea=allocations;ep=parses;original=0;allocations=parses=0;sr=mode?ItemParse_tooltipaligny(&item,71):ItemParse_tooltipalignx(&item,71);
   assert(er==sr&&ea==allocations&&ep==parses);assert((item.toolTipData==NULL)==(*(void**)(parent+0x270)==NULL));if(item.toolTipData)assert(!memcmp(&pool,rawPool,sizeof(pool)));++cases;
  }
 }
 {static displayContextDef_t dc;static byte odc[0x20100];itemDef_t source,tip;byte op[0x280],ot[0x280];int i,m;
 dc.textFont=selectedFont;dc.multiLineTextWidth=width;dc.multiLineTextHeight=height;Init_Display(&dc);*(void**)(odc+0x30)=selectedFont;*(void**)(odc+0x20)=width;*(void**)(odc+0x2c)=height;*(void**)(b+0x245fa4)=odc;
 for(i=0;i<1000;++i)for(m=0;m<3;++m){memset(&source,0,sizeof(source));memset(&tip,0,sizeof(tip));memset(op,0,sizeof(op));memset(ot,0,sizeof(ot));
 source.window.rectClient.x=i*.713f-211.3f;source.window.rectClient.y=i*.173f-30.7f;source.window.rectClient.w=i*.137f+13.1f;source.window.rectClient.h=i*.071f+9.3f;source.parent=(void*)0x123400;source.toolTipData=&tip;tip.font=m;tip.text="Tooltip";tip.textscale=.2f;tip.window.flags=0x2080000;
 memcpy(op,&source,sizeof(source));memcpy(ot,&tip,sizeof(tip));*(void**)(op+0x270)=ot;metricWidth=m==0?0:m==1?81:721;metricHeight=m*17;
 ((void(__cdecl*)(void*))(b+0x147a0))(op);assert(metricCalls==3&&fontSelected==m);Tooltip_ComputePosition(&source);assert(metricCalls==3&&fontSelected==m);assert(!memcmp(ot,&tip,sizeof(tip)));++cases;
 }}
 restore();assert(FreeLibrary(h));printf("%d original tooltip alignment/position comparisons passed\n",cases);return 0;}
