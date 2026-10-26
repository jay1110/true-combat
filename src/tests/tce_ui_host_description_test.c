#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static char trace[8192];
static int height(const char*s,float scale,int limit){sprintf(trace+strlen(trace),"H:%s;",s?s:"NULL");return 13;}
static int width(const char*s,float scale,int limit){return (int)strlen(s)*7;}
static void draw(float x,float y,float scale,vec4_t c,const char*s,float a,int l,int style){sprintf(trace+strlen(trace),"T:%.9g,%.9g,%d:%s;",x,y,style,s);}
static void pic(float x,float y,float w,float h,int sh){sprintf(trace+strlen(trace),"P:%.9g,%.9g,%.9g,%.9g:%d;",x,y,w,h,sh);}
static int shader(const char*s){sprintf(trace+strlen(trace),"S:%s;",s);return 97;}
static struct{unsigned char*p,bytes[5];}hooks[6];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<6);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;unsigned char*b;static displayContextDef_t dc;static unsigned char odc[0x20100];int desc,game,align,w,net,cases=0;char expected[8192];rectDef_t r={3.13f,11.71f,80,200};vec4_t color={1,1,1,1};const char*texts[]={NULL,"","Both teams meet in a map for a fight. Unlimited lives.","Bomb attack**Bomb defense","one\ntwo\nthree","alpha\tbeta\r gamma","singleword"," leading and trailing "};int modes[]={2,5,7,99};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x4460,height);patch(Text_Height,height);patch(b+0x26f0,pic);patch(UI_DrawHandlePic,pic);patch(b+0x20730,shader);patch(trap_R_RegisterShaderNoMip,shader);dc.textWidth=width;dc.drawText=draw;Init_Display(&dc);*(void**)(odc+0x18)=width;*(void**)(odc+0x10)=draw;*(void**)(b+0x245fa4)=odc;
uiInfo.numGameTypes=*(int*)(b+0x2b3280)=3;for(game=0;game<3;++game)uiInfo.gameTypes[game].gtEnum=*(int*)(b+0x2b328c+16*game)=modes[game];
for(desc=0;desc<8;++desc)for(game=0;game<4;++game)for(align=0;align<4;++align)for(w=0;w<5;++w)for(net=0;net<2;++net){int j;r.w=(float)(w*47);ui_netGameType.integer=*(int*)(b+0x2dc50c)=modes[game];for(j=0;j<3;++j)uiInfo.gameTypes[j].gameTypeDescription=*(const char**)(b+0x2b3290+16*j)=texts[desc];trace[0]=0;((void(__cdecl*)(rectDef_t*,float,float*,float,float,int,int))(b+0x6060))(&r,.2f,color,7.31f,81.13f,3,align);strcpy(expected,trace);trace[0]=0;UI_DrawGametypeDescription(&r,.2f,color,7.31f,81.13f,3,align,net);if(strcmp(expected,trace)){fprintf(stderr,"description mismatch d%d g%d a%d w%d net%d\nO%s\nS%s\n",desc,game,align,w,net,expected,trace);return 1;}++cases;}
for(game=-2;game<=3;++game)for(net=0;net<2;++net){uiInfo.serverStatus.currentServerPreview=*(int*)(b+0x2d1310)=game;trace[0]=0;((void(__cdecl*)(rectDef_t*))(b+0x5dd0))(&r);strcpy(expected,trace);trace[0]=0;UI_DrawNetMapPreview(&r,.2f,color,net);assert(!strcmp(expected,trace));++cases;}
restore();assert(FreeLibrary(h));printf("%d original host description/preview comparisons passed\n",cases);return 0;}
