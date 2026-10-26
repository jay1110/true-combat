#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../cgame/tce_warmup.h"
typedef struct {int kind;float x,y,sx,sy,c[4],adjust;int limit,style;char text[1024];} event_t;
static event_t events[20],reference[20];static int count;static const char *key="F3";static int defender;
static void check(int b){if(!b){fprintf(stderr,"warmup mismatch at event %d\n",count);exit(1);}}
static const char *translate(const char *s){return s;}
static const char *binding(const char *s){check(!strcmp(s,"ready"));return key;}
static int width(const char *s,float f,int limit,void *font){return (int)strlen(s)*7+1;}
static void paint(float x,float y,float sx,float sy,const float *c,const char *s,float adj,int lim,int style,void *font){event_t *e=&events[count++];memset(e,0,sizeof(*e));e->kind=1;e->x=x;e->y=y;e->sx=sx;e->sy=sy;memcpy(e->c,c,16);e->adjust=adj;e->limit=lim;e->style=style;strncpy(e->text,s,1023);}
static void sound(int h,int channel){event_t *e=&events[count++];memset(e,0,sizeof(*e));e->kind=2;e->limit=h;e->style=channel;}
static const char *config(int n){check(n==13);return "config";}
static const char *info(const char *s,const char *k){check(!strcmp(k,"defender"));return defender?"1":"0";}
static void hook(unsigned char *p,void *f){DWORD old,tmp;check(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
#define SET(off,v) (*(int *)(b+(off))=(v))
int main(int argc,char **argv){HMODULE dll;unsigned char *b;unsigned char snap[1024]={0};void (__cdecl *original)(void);tce_warmup_t s; tce_warmup_api_t api={translate,binding,width,paint,sound,0,711};int i,n,j,tests=0;
 check(argc==2);dll=LoadLibraryA(argv[1]);check(dll!=NULL);b=(unsigned char *)dll;original=(void *)(b+0x26d20);
 hook(b+0x2a830,translate);hook(b+0x84090,binding);hook(b+0x20910,width);hook(b+0x20ba0,paint);hook(b+0x65070,sound);hook(b+0x481a0,config);hook(b+0x7f4c0,info);
 *(void **)(b+0x40352c4)=snap;SET(0x2588aa8,711);
 for(i=0;i<12000;++i){memset(&s,0,sizeof(s));s.warmup=i%3?10000:0;s.time=8000+(i%53)*101;s.gameState=(i/3)%5;s.minClients=i%4;s.demo=(i/7)%2;s.team=(i/11)%4;s.pmFlags=((i/17)%4)*0x1000;if(i%2)s.pmFlags|=0x4000;s.gameType=(i/19)%8;s.round=(i/23)%3;s.defender=(i/29)%2;s.count=(i/31)%5-1;key=i%2?"F3":"(???" ")";defender=s.defender;
 SET(0x4080e30,s.warmup);SET(0x407dea4,s.time);SET(0x3fb7810,s.gameState);SET(0x3fb780c,s.minClients);SET(0x40352b0,s.demo);SET(0x257e100,s.gameType);SET(0x3fb7804,s.round);SET(0x4080e34,s.count);*(int *)(snap+0x148)=s.team;*(int *)(snap+0x38)=s.pmFlags;
 count=0;original();n=count;memcpy(reference,events,sizeof(events));count=0;TCE_DrawWarmup(&s,&api);
 if(n!=count || memcmp(reference,events,n*sizeof(event_t)) || s.count!=*(int *)(b+0x4080e34)){fprintf(stderr,"case %d warmup%d time%d state%d type%d count%d/%d\n",i,s.warmup,s.time,s.gameState,s.gameType,count,n);for(j=0;j<count||j<n;++j)fprintf(stderr,"[%d] %s / %s x%f/%f y%f/%f\n",j,events[j].text,reference[j].text,events[j].x,reference[j].x,events[j].y,reference[j].y);return 1;}++tests;
 }
 printf("%d original warmup comparisons passed\n",tests);FreeLibrary(dll);return 0;
}
