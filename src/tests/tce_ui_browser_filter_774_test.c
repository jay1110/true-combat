#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static char trace[2048];
static void build(int f){sprintf(trace+strlen(trace),"BUILD:%d;",f);}
static void draw(float x,float y,float scale,vec4_t c,const char*s,float adj,int limit,int style){sprintf(trace+strlen(trace),"DRAW:%.9g:%.9g:%.9g:%s:%.9g:%d:%d;",x,y,scale,s,adj,limit,style);}
static struct{unsigned char*p,bytes[5];}hooks[4];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<4);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b;int index,key,flags,specialIndex,er,sr,ei,cases=0;char expected[2048];float os,ss;rectDef_t r={3.13f,11.71f,81.3f,122.7f};vec4_t color={1,1,1,1};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;patch(b+0x48f0,draw);patch(Text_Paint,draw);patch(b+0xb420,build);patch(UI_BuildServerDisplayList,build);
for(index=-2;index<=4;++index)for(key=-1;key<256;++key)for(flags=0;flags<2;++flags)for(specialIndex=0;specialIndex<2;++specialIndex){ui_serverFilterType.integer=*(int*)(b+0x29160c)=index;os=ss=specialIndex?13.7f:-.3f;trace[0]=0;er=((int(__cdecl*)(int,float*,int))(b+0x11240))(flags,&os,key);ei=*(int*)(b+0x29160c);strcpy(expected,trace);trace[0]=0;sr=UI_NetFilter_HandleKey(flags,&ss,key);assert(er==sr&&ui_serverFilterType.integer==ei&&os==ss&&!strcmp(trace,expected));++cases;}
for(index=-2;index<=4;++index)if(index!=1)for(key=0;key<4;++key){ui_serverFilterType.integer=*(int*)(b+0x29160c)=index;trace[0]=0;((void(__cdecl*)(rectDef_t*,float,float*,int))(b+0xfe50))(&r,.2f,color,key);ei=*(int*)(b+0x29160c);strcpy(expected,trace);trace[0]=0;UI_DrawNetFilter(&r,.2f,color,key);assert(ui_serverFilterType.integer==ei&&!strcmp(trace,expected));++cases;}
restore();assert(FreeLibrary(h));printf("%d original browser filter input/display comparisons passed\n",cases);return 0;}
