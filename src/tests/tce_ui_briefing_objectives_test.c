#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static char trace[8192];static int found;static mapInfo row;static byte raw[0x88];static int original;
static int height(const char*s,float scale,int limit){sprintf(trace+strlen(trace),"H:%s;",s?s:"NULL");return 13;}
static int width(const char*s,float scale,int limit){return strlen(s)*7;}
static void draw(float x,float y,float scale,vec4_t c,const char*s,float a,int l,int style){sprintf(trace+strlen(trace),"T:%.9g,%.9g,%d:%s;",x,y,style,s);}
static void string(const char*n,char*b,int size){assert(!strcmp(n,"mapname")&&size==64);strcpy(b,"obj_northport");}
static mapInfo* find(const char*n){assert(!strcmp(n,"obj_northport"));return found?(original?(mapInfo*)raw:&row):NULL;}
static struct{unsigned char*p,bytes[5];}hooks[10];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<10);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}

int main(int argc,char**argv){HMODULE h;byte*b;int d,align,w,y,cases=0;char expected[8192];rectDef_t r={3.13f,11.71f,80,200};vec4_t color={1,1,1,1};const char*texts[]={NULL,"","Both teams have objectives. One life.","first\nsecond\nthird","alpha\tbeta\r gamma","oneword"," trailing and leading ","literal**asterisks"};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
patch(b+0x4460,height);patch(Text_Height,height);patch(b+0x4270,width);patch(Text_Width,width);patch(b+0x48f0,draw);patch(Text_Paint,draw);patch(b+0x20570,string);patch(trap_Cvar_VariableStringBuffer,string);
/* Separate lookup detours need two more slots. */
patch(b+0x2d80,find);patch(UI_FindMapInfoByMapname,find);
for(found=0;found<2;++found)for(d=0;d<8;++d)for(align=0;align<4;++align)for(w=0;w<5;++w)for(y=0;y<2;++y){r.w=w*47;row.objectives=*(const char**)(raw+0x80)=texts[d];trace[0]=0;original=1;((void(__cdecl*)(rectDef_t*,float,float*,float,float,int,int))(b+0xf960))(&r,.2f,color,7.31f,y?23.17f:0,3,align);strcpy(expected,trace);trace[0]=0;original=0;UI_DrawMissionBriefingObjectives(&r,.2f,color,7.31f,y?23.17f:0,3,align);if(strcmp(expected,trace)){fprintf(stderr,"briefing mismatch found%d d%d align%d w%d y%d\nO%s\nS%s\n",found,d,align,w,y,expected,trace);return 1;}++cases;}
restore();assert(FreeLibrary(h));printf("%d original briefing-objectives comparisons passed\n",cases);return 0;}
