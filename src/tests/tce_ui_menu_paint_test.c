#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static char trace[2048];static int ownerOK,tooltip,original;static void*active;static void*objects[4];
static void add(const char*s){strcat(trace,s);}
static qboolean visible(int flags){assert(flags==19);add("V;");return ownerOK;}
static float cvar(const char*s){assert(!strcmp(s,"ui_showtooltips"));add("C;");return (float)tooltip;}
static void pic(float x,float y,float w,float h,int s){assert(x==0&&y==0&&w==640&&h==480&&s==73);add("B;");}
static void rect(float x,float y,float w,float h,float size,const float*c){assert(x==1.3f&&y==-2.7f&&w==513.1f&&h==311.7f&&size==1);assert(c[0]==1&&c[1]==0&&c[2]==1&&c[3]==1);add("D;");}
static void window(Window*w,float a,float c,float cycle){assert((void*)w==active&&a==.13f&&c==.71f&&cycle==137);add("W;");}
static void paint(itemDef_t*i){int n;char s[20];for(n=0;n<4;++n)if(i==objects[n])break;assert(n<4);sprintf(s,"I%d;",n);add(s);}
static void script(itemDef_t*i,qboolean*a,const char*s){void*p=original?*(void**)((unsigned char*)i+0xec):i->parent;assert(p==active&&a==NULL&&!strcmp(s,"timeout"));add("T;");}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;static unsigned char odc[0x20100],om[0x718],oi[4][0x280];static displayContextDef_t dc;menuDef_t m;itemDef_t it[4];int mask,tip,tim,j,cases=0;const int opens[]={0,1,70,80,81};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x14f70,window);patch(Window_Paint,window);patch(b+0x1d3d0,paint);patch(Item_Paint,paint);patch(b+0x17810,script);patch(Item_RunScript,script);
*(void**)(odc+8)=pic;dc.drawHandlePic=pic;*(void**)(odc+0x40)=rect;dc.drawRect=rect;*(void**)(odc+0x74)=cvar;dc.getCVarValue=cvar;*(int*)(odc+0x120)=dc.realTime=100;*(void**)(b+0x245fa4)=odc;DC=&dc;
((void(__cdecl*)(void*,int))(b+0x1dd00))(NULL,1);Menu_Paint(NULL,1);++cases;
for(mask=0;mask<1024;++mask)for(tip=0;tip<4;++tip)for(tim=0;tim<5;++tim){char expected[2048];int flags,open;memset(om,0,sizeof(om));memset(oi,0,sizeof(oi));memset(&m,0,sizeof(m));memset(it,0,sizeof(it));*(float*)om=m.window.rect.x=1.3f;*(float*)(om+4)=m.window.rect.y=-2.7f;*(float*)(om+8)=m.window.rect.w=513.1f;*(float*)(om+12)=m.window.rect.h=311.7f;*(int*)(om+0x48)=m.window.flags=(mask&1)?4:0;*(int*)(om+0x40)=m.window.ownerDrawFlags=(mask&4)?19:0;ownerOK=!!(mask&8);*(void**)(odc+0x64)=(mask&16)?visible:NULL;dc.ownerDrawVisible=(mask&16)?visible:NULL;*(int*)(om+0xbc)=m.fullScreen=!!(mask&32);*(int*)(om+0xb4)=m.window.background=73;*(float*)(om+0xd4)=m.fadeAmount=.13f;*(float*)(om+0xd0)=m.fadeClamp=.71f;*(int*)(om+0xcc)=m.fadeCycle=137;*(int*)(om+0xc0)=m.itemCount=(mask&64)?3:0;tooltip=(mask&128)?1:0;*(int*)(om+0xe8)=m.timeout=(mask&256)?20:0;*(int*)(om+0xec)=m.openTime=opens[tim];*(char**)(om+0xf0)=m.onTimeout=tip==0?NULL:"timeout";*(int*)(b+0x245fc0)=debugMode=!!(mask&512);
for(j=0;j<4;++j){*(void**)(om+0x514+4*j)=oi[j];m.items[j]=it+j;*(int*)(oi[j]+0x48)=it[j].window.flags=j<2?1:0;objects[j]=oi[j];}*(void**)(oi[1]+0x270)=tip==0?NULL:oi[3];it[1].toolTipData=tip==0?NULL:it+3;*(char**)(oi[3]+0xe8)=it[3].text=tip==1?NULL:tip==2?"":"tip";
original=1;active=om;trace[0]=0;((void(__cdecl*)(void*,int))(b+0x1dd00))(om,!!(mask&2));strcpy(expected,trace);flags=*(int*)(om+0x48);open=*(int*)(om+0xec);original=0;active=&m;for(j=0;j<4;++j)objects[j]=it+j;trace[0]=0;Menu_Paint(&m,!!(mask&2));if(strcmp(trace,expected)||flags!=m.window.flags||open!=m.openTime){fprintf(stderr,"paint mismatch mask%d tip%d time%d original%s source%s\n",mask,tip,tim,expected,trace);return 1;}++cases;}
printf("%d original Menu_Paint comparisons passed\n",cases);FreeLibrary(h);return 0;}

