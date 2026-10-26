#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static char trace[4096];static menuDef_t*baseMenu;
static void paintmenu(menuDef_t*m,qboolean force){sprintf(trace+strlen(trace),"M%d,%d;",(int)(m-baseMenu),force);}
static void capture(void*p){assert(p==(void*)123);strcat(trace,"capture;");}
static void font(int i){sprintf(trace+strlen(trace),"F%d;",i);}
static void text(float x,float y,float s,vec4_t c,const char*t,float a,int l,int style){sprintf(trace+strlen(trace),"T%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%s,%.9g,%d,%d;",x,y,s,c[0],c[1],c[2],c[3],t,a,l,style);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;displayContextDef_t dc;unsigned char originalDC[0x20100];int n,mask,dbg,cap,i,cases=0;char expected[4096];assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;memset(&dc,0,sizeof(dc));dc.drawText=text;dc.textFont=font;dc.FPS=59.375f;dc.cursorx=123;dc.cursory=-19;Init_Display(&dc);memset(originalDC,0,sizeof(originalDC));*(void**)(originalDC+0x10)=text;*(void**)(originalDC+0x30)=font;*(float*)(originalDC+0x20034)=dc.FPS;*(int*)(originalDC+0x128)=dc.cursorx;*(int*)(originalDC+0x12c)=dc.cursory;*(void**)(b+0x245fa4)=originalDC;patch(b+0x1dd00,paintmenu);patch(Menu_Paint,paintmenu);
for(n=0;n<=6;++n)for(mask=0;mask<64;++mask)for(dbg=0;dbg<2;++dbg)for(cap=0;cap<2;++cap){menuCount=*(int*)(b+0x245fb8)=n;debugMode=*(int*)(b+0x245fc0)=dbg;captureFunc=cap?capture:NULL;*(void**)(b+0x245f98)=cap?capture:NULL;captureData=*(void**)(b+0x245f9c)=(void*)123;for(i=0;i<n;++i){Menus[i].window.flags=(mask&(1<<i))?WINDOW_DRAWALWAYSONTOP:0;*(int*)(b+0x255da0+i*0x718+0x48)=Menus[i].window.flags;}trace[0]=0;baseMenu=(menuDef_t*)(b+0x255da0);((void(__cdecl*)(void))(b+0x1fc00))();strcpy(expected,trace);trace[0]=0;baseMenu=Menus;Menu_PaintAll();if(strcmp(expected,trace)){fprintf(stderr,"dispatch mismatch n%d mask%d debug%d capture%d\nORIGINAL:%s\nSOURCE:%s\n",n,mask,dbg,cap,expected,trace);return 1;}++cases;}
for(n=0;n<=128;++n){menuCount=*(int*)(b+0x245fb8)=n;assert(Menu_Count()==((int(__cdecl*)(void))(b+0x1fbc0))());++cases;for(i=-1;i<=129;++i){menuDef_t*a=((menuDef_t*(__cdecl*)(int))(b+0x1fbd0))(i),*c=Menu_Get(i);assert((a==NULL)==(c==NULL));if(a)assert(a-(menuDef_t*)(b+0x255da0)==c-Menus);++cases;}}
printf("%d original UI menu dispatch/access comparisons passed\n",cases);FreeLibrary(h);return 0;}
