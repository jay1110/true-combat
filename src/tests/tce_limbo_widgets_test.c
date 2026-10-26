#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../cgame/tce_limbo_widgets.h"
typedef struct {int kind,a,b;float v[8];char text[64];} event_t;
static event_t events[16],expected[16];
static int count,cases,teamValue,classValue,counterValue;
static unsigned char *base;
static tce_limboButton_t button;
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"case %d: %s\n",cases,why);exit(1);}}
static event_t *record(int kind){event_t *e=&events[count++];require(count<=16,"event capacity");memset(e,0,sizeof(*e));e->kind=kind;return e;}
static int team(void){record(1);return teamValue;}
static int playerClass(void){record(2);return classValue;}
static int counter(const tce_limboButton_t *b){require(!memcmp(b,&button,sizeof(button)),"counter input");record(3);return counterValue;}
static int originalCounter(const unsigned char *b){return counter((const tce_limboButton_t *)(b+8));}
static void pic(float x,float y,float w,float h,int shader){event_t *e=record(4);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;e->a=shader;}
static void border(float x,float y,float w,float h,int a,int b){pic(x,y,w,h,a);events[count-1].kind=5;events[count-1].b=b;}
static void text(float x,float y,float sx,float sy,const float *c,const char *s){event_t *e=record(6);e->v[0]=x;e->v[1]=y;e->v[2]=sx;e->v[3]=sy;memcpy(e->v+4,c,16);snprintf(e->text,sizeof(e->text),"%s",s);}
static void originalText(float x,float y,float sx,float sy,const float *c,const char *s,float adjust,int limit,int style,void *font){require(adjust==0&&limit==0&&style==0&&font==base+0x2588f8c,"text extras/font");text(x,y,sx,sy,c,s);}
static int inside(const tce_limboRect_t *rect){event_t *e=record(7);memcpy(e->v,rect,sizeof(*rect));return teamValue;}
static void fillRect(float x,float y,float w,float h,const float *color){pic(x,y,w,h,0);events[count-1].kind=8;memcpy(events[count-1].v+4,color,16);}
static void hook(int offset,void *fn){DWORD old,tmp;unsigned char *p=base+offset;require(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old),"protect");p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){
 HMODULE dll;unsigned char originalButton[80],saved[80];int f,t,c,s,d,r,n,i,j,expectedCount;
 const int offsets[]={0x40d60,0x42fd0,0x41a60,0x42120},numbers[]={-12,-1,0,1,9,10,64,100};
 const int order[]={2,1,3};
 void (*ports[])(const tce_limboButton_t *,const tce_limboWidgets_t *)={TCE_LimboClass,TCE_LimboCounter,TCE_LimboWeaponLight,TCE_LimboBorder};
 tce_limboWidgets_t ctx={101,102,103,104,0,team,playerClass,counter,order,pic,border,text};
 require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load DLL");base=(unsigned char *)dll;
 hook(0x87af0,inside);hook(0x29850,fillRect);
 for(f=0;f<3;++f)for(t=0;t<3;++t)for(c=0;c<2;++c)for(r=0;r<20;++r){
  float x=632+r*.123f,y=50-r*.417f,w=200+r*17.5f,h=364-r*2.375f;
  teamValue=c;count=0;((void (__cdecl *)(float,float,float,float,int,int))(base+0x42020))(x,y,w,h,f,t);
  expectedCount=count;memcpy(expected,events,sizeof(events));count=0;
  TCE_LimboDrawBorder(x,y,w,h,f,t,inside,fillRect);
  require(count==expectedCount&&!memcmp(events,expected,count*sizeof(event_t)),"border fill/hover arguments");++cases;
 }
 hook(0x434f0,team);hook(0x43530,playerClass);hook(0x42820,originalCounter);hook(0x29ea0,pic);hook(0x42020,border);hook(0x20ba0,originalText);
 *(int *)(base+0x259d0f8)=101;*(int *)(base+0x259d0f4)=102;*(int *)(base+0x259d0ac)=103;*(int *)(base+0x259d0b0)=104;
 require(!memcmp(base+0xa76d8,order,sizeof(order)),"original team order");
 for(f=0;f<4;++f)for(t=0;t<4;++t)for(c=0;c<6;++c)for(s=0;s<3;++s)for(d=0;d<(f==1?3:6);++d)for(r=0;r<3;++r)for(n=0;n<8;++n){
  teamValue=t;classValue=c;ctx.selectedSlot=s;counterValue=numbers[n];*(int *)(base+0x3fde380)=s;
  memset(&button,0,sizeof(button));button.x=642+r*.123f;button.y=182-r*.417f;button.w=20+r*17.5f;button.h=14+r*2.375f;button.data[0]=d%3;button.data[1]=d;
  memset(originalButton,0x5a,sizeof(originalButton));memcpy(originalButton+8,&button,sizeof(button));memcpy(saved,originalButton,sizeof(saved));
  count=0;((void (__cdecl *)(void *))(base+offsets[f]))(originalButton);expectedCount=count;memcpy(expected,events,sizeof(events));require(!memcmp(saved,originalButton,sizeof(saved)),"original mutated input");
  count=0;ports[f](&button,&ctx);require(count==expectedCount,"call count");require(!memcmp(&button,saved+8,sizeof(button)),"port mutated input");
  for(i=0;i<count;++i){require(events[i].kind==expected[i].kind&&events[i].a==expected[i].a&&events[i].b==expected[i].b&&!strcmp(events[i].text,expected[i].text),"call order/arguments");for(j=0;j<8;++j)require(fabs(events[i].v[j]-expected[i].v[j])<.0001,"float argument");}
  ++cases;
 }
 FreeLibrary(dll);printf("%d original Limbo widget comparisons passed.\n",cases);return 0;
}
