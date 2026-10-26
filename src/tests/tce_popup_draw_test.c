#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../cgame/tce_popup_draw.h"
typedef struct {int kind,shader,limit,style;float v[10];char text[128];} event_t;
static event_t events[32],reference[32];static int count;
static void check(int b){if(!b)exit(1);}
static event_t *event(int kind){event_t *e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
static void color(const float *c){event_t *e=event(c?1:2);if(c)memcpy(e->v,c,16);}
static void pic(float x,float y,float w,float h,int sh){event_t *e=event(3);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;e->shader=sh;}
static void paint(float x,float y,float sx,float sy,float *c,const char *text,float adjust,int limit,int style,void *font){event_t *e=event(4);e->v[0]=x;e->v[1]=y;e->v[2]=sx;e->v[3]=sy;memcpy(e->v+4,c,16);e->v[8]=adjust;e->limit=limit;e->style=style;strcpy(e->text,text);}
static int duration(int type){return type*137;}
static void hook(unsigned char *p,void *f){DWORD old,tmp;check(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){HMODULE dll;unsigned char *b,snap[1024]={0};void (__cdecl *original)(void);tce_popup_draw_item_t items[7];tce_popup_draw_api_t api={color,pic,paint,duration,0};int aspect,respawn,time,length,active,i,j,k,n,mode,tests=0;
 check(argc==2);dll=LoadLibraryA(argv[1]);check(dll!=NULL);b=(unsigned char *)dll;original=(void *)(b+0x57a10);hook(b+0x65420,color);hook(b+0x29ea0,pic);hook(b+0x20ba0,paint);hook(b+0x575e0,duration);hook(b+0x575f0,duration);*(void **)(b+0x40352c4)=snap;
 for(i=0;i<7;++i){memset(items+i,0,sizeof(items[i]));items[i].type=i;items[i].time=i*213;items[i].shader=700+i;sprintf(items[i].message,"Popup%d",i);}
 for(mode=0;mode<2;++mode)for(aspect=-1;aspect<=3;++aspect)for(respawn=-1;respawn<=1;++respawn)for(time=-1;time<9000;time+=149)for(length=0;length<6;++length)for(active=0;active<2;++active){
  for(i=1;i<7;++i)items[i].next=i<length?items+i+1:0;
  *(int *)(b+0x401496c)=aspect;*(int *)(b+0x407dea4)=time;*(int *)(snap+0x158)=respawn;*(void **)(b+0x1dc7d00)=active?items:0;*(void **)(b+0x1dc7d04)=length?items+1:0;
  original=(void *)(b+(mode?0x57c50:0x57a10));*(void **)(b+0x1dc9440)=active?items:0;
  count=0;original();n=count;memcpy(reference,events,sizeof(events));count=0;if(mode)TCE_DrawPMItemsBig(aspect,time,active?items:0,&api);else TCE_DrawPMItems(aspect,respawn,time,active?items:0,length?items+1:0,&api);check(n==count);
  for(j=0;j<n;++j){check(events[j].kind==reference[j].kind&&events[j].shader==reference[j].shader&&events[j].limit==reference[j].limit&&events[j].style==reference[j].style&&!strcmp(events[j].text,reference[j].text));for(k=0;k<10;++k)if(fabs(events[j].v[k]-reference[j].v[k])>.0001){fprintf(stderr,"popup draw mismatch case%d event%d value%d %.9g/%.9g\n",tests,j,k,events[j].v[k],reference[j].v[k]);return 1;}}++tests;
 }
 printf("%d original popup drawing comparisons passed\n",tests);FreeLibrary(dll);return 0;
}
