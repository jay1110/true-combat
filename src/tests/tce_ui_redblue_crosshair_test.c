#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static const char*team;static char trace[4096];static int shader;
static void cvarstring(const char*n,char*out,int size){sprintf(trace+strlen(trace),"G%s;",n);Q_strncpyz(out,team,size);}
static void colorfn(const float*c){if(c)sprintf(trace+strlen(trace),"C%.9g,%.9g,%.9g,%.9g;",c[0],c[1],c[2],c[3]);else strcat(trace,"Cnull;");}
static int reg(const char*n){sprintf(trace+strlen(trace),"R%s;",n);return ++shader;}
static void pic(float x,float y,float w,float h,int icon){sprintf(trace+strlen(trace),"D%.9g,%.9g,%.9g,%.9g,%d;",x,y,w,h,icon);}
static void paint(float x,float y,float scale,vec4_t c,const char*t,float adjust,int limit,int style){sprintf(trace+strlen(trace),"P%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%s,%.9g,%d,%d;",x,y,scale,c[0],c[1],c[2],c[3],t,adjust,limit,style);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int i,j,k,style,cases=0;rectDef_t r={-3.75f,8.99f,96,48};vec4_t c={.1f,.2f,.3f,.4f};vec4_t alt={.9f,.7f,.5f,.3f};char expected[4096];int sizes[]={-10,0,23,24,25,48,95,96,97,200};float widths[]={48,96,192,77.25f};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x20810,colorfn);patch(trap_R_SetColor,colorfn);patch(b+0x26f0,pic);patch(UI_DrawHandlePic,pic);patch(b+0x48f0,paint);patch(Text_Paint,paint);
for(i=-2;i<=2;++i)for(style=0;style<6;++style){*(int*)(b+0x2b3488)=uiInfo.redBlue=i;trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t,int))(b+0x10310))(&r,.3f,c,style);strcpy(expected,trace);trace[0]=0;UI_DrawRedBlue(&r,.3f,c,style);assert(!strcmp(trace,expected));++cases;}
memcpy(b+0x2d8370,c,16);memcpy(uiInfo.xhairColor,c,16);memcpy(b+0x2d8380,alt,16);memcpy(uiInfo.xhairColorAlt,alt,16);for(i=0;i<10;++i){*(int*)(b+0x2b0394+i*4)=uiInfo.uiDC.Assets.crosshairShader[i]=100+i;*(int*)(b+0x2b03bc+i*4)=uiInfo.uiDC.Assets.crosshairAltShader[i]=200+i;}
for(i=-1;i<=10;++i)for(j=0;j<10;++j)for(k=0;k<4;++k){*(int*)(b+0x2d3d48)=uiInfo.currentCrosshair=i;*(int*)(b+0x290acc)=cg_crosshairSize.integer=sizes[j];r.w=widths[k];trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t))(b+0x10350))(&r,.3f,c);strcpy(expected,trace);trace[0]=0;UI_DrawCrosshair(&r,.3f,c);if(strcmp(trace,expected)){fprintf(stderr,"crosshair mismatch index%d size%d width%.9g\noriginal:%s\nsource:%s\n",i,sizes[j],r.w,expected,trace);return 1;}assert(*(int*)(b+0x2d3d48)==uiInfo.currentCrosshair);++cases;}
printf("%d original UI redblue/crosshair comparisons passed\n",cases);FreeLibrary(h);return 0;}
