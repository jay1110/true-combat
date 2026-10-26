#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../cgame/tce_score_objectives.h"
typedef struct {int kind;float v[8];char text[1024];} event_t;
static event_t events[64],expected[64];static int count,reinf;static unsigned char *b;static char info[128];
static event_t *record(int kind){event_t *e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
static void fill(float x,float y,float w,float h,const float *c){event_t *e=record(1);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;memcpy(e->v+4,c,16);}
static void border(float x,float y,float w,float h,int n,const float *c){fill(x,y,w,h,c);events[count-1].kind=2;if(n!=1)exit(3);}
static void text(float x,float y,float scale,const float *c,const char *s){event_t *e=record(3);e->v[0]=x;e->v[1]=y;e->v[2]=scale;memcpy(e->v+3,c,16);strcpy(e->text,s);}
static void originalText(float x,float y,float sx,float sy,const float *c,const char *s,float adjust,int limit,int style,void *font){if(sx!=sy||adjust||limit||style||font!=b+0x2588f8c)exit(4);text(x,y,sx,c,s);}
static int width(const char *s,float scale){event_t *e=record(4);strcpy(e->text,s);e->v[0]=scale;return (int)(strlen(s)*9*scale);}
static int originalWidth(const char *s,float scale,int limit,void *font){if(limit||font!=b+0x2588f8c)exit(5);return width(s,scale);}
static const char *translate(const char *s){event_t *e=record(5);strcpy(e->text,s);return s;}
static const char *config(int id){record(6)->v[0]=(float)id;return info;}
static int reinforce(int flag){record(7)->v[0]=(float)flag;return reinf;}
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static int compare(int n){int i,j;if(n!=count){printf("count %d/%d\n",n,count);return 0;}for(i=0;i<n;++i){if(events[i].kind!=expected[i].kind||strcmp(events[i].text,expected[i].text)){printf("event%d kind%d/%d text[%s]/[%s]\n",i,events[i].kind,expected[i].kind,events[i].text,expected[i].text);return 0;}for(j=0;j<8;++j)if(fabs(events[i].v[j]-expected[i].v[j])>.001){printf("event%d arg%d %g/%g\n",i,j,events[i].v[j],expected[i].v[j]);return 0;}}return 1;}
int main(int argc,char **argv){HMODULE h;tce_objectivesContext_t c;unsigned char snap[512];int gt,gs,pm,team,t,lim,win,a,r,n,cases=0;const int times[]={-500,0,4999,5000,60001,1200001};const float limits[]={0,.0017f,20,.333333f,1.234567f,100,-1};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 memset(&c,0,sizeof(c));memset(snap,0,sizeof(snap));*(void **)(b+0x40352c4)=snap;
 c.backColor=(float *)(b+0xb0d94);c.barColor=(float *)(b+0xb0da4);c.black=(float *)(b+0xb4694);
 c.config=config;c.translate=translate;c.reinforce=reinforce;c.width=width;c.fill=fill;c.border=border;c.text=text;c.infoValue=(void *)(b+0x7f4c0);
 patch(b+0x29850,fill);patch(b+0x29df0,border);patch(b+0x20ba0,originalText);patch(b+0x20910,originalWidth);patch(b+0x2a830,translate);patch(b+0x481a0,config);patch(b+0x21430,reinforce);
 for(gt=0;gt<8;++gt)for(gs=0;gs<4;++gs)for(pm=0;pm<2;++pm)for(team=0;team<4;++team)for(t=0;t<6;++t)for(lim=0;lim<7;++lim)for(win=-1;win<2;++win){
  c.gameType=*(int *)(b+0x257e100)=gt;c.gameState=*(int *)(b+0x3fb7810)=gs;c.pmType=*(int *)(snap+0x30)=pm?5:0;c.viewedTeam=*(int *)(b+0x2580a98)=team;c.time=*(int *)(b+0x407dea4)=times[t]+100;c.startTime=*(int *)(b+0x257e544)=100;c.intermissionStartTime=*(int *)(b+0x257e548)=100;c.timelimit=*(float *)(b+0x257e108)=limits[lim];
  c.currentRound=*(int *)(b+0x3fb7804)=2;c.wonRounds[0]=*(int *)(b+0x4080db4)=7;c.wonRounds[1]=*(int *)(b+0x4080db8)=3;c.campaignMap=*(int *)(b+0x3fb7818)=2;c.campaignMaps=*(int *)(b+0x3fc314c)=6;reinf=t==0?0:t==1?-61:t*17;sprintf(info,"\\winner\\%d",win);
  count=0;a=((int(__cdecl *)(int,int,int,float))(b+0x596b0))(20,10,817,.35f);n=count;memcpy(expected,events,n*sizeof(event_t));count=0;r=TCE_WM_DrawObjectives(20,10,817,.35f,&c);
  if(a!=r||!compare(n)){printf("objectives gt%d gs%d pm%d team%d t%d limit%d winner%d\n",gt,gs,pm,team,t,lim,win);return 1;}++cases;
 }
 for(a=0;a<4;++a)for(t=0;t<3;++t)for(n=0;n<2;++n){float rect[]={13.25f,27.5f,400.5f,37};const char *title=n?"WINDOW":NULL;int saved;count=0;((void(__cdecl *)(float *,const char *,int,int,float,int))(b+0x1f420))(rect,title,a,18,t*.25f,16);saved=count;memcpy(expected,events,saved*sizeof(event_t));count=0;TCE_DrawScoreWindow(rect,title,a,18,t*.25f,16,&c);if(!compare(saved))return 1;++cases;}
 printf("%d whole Windows objectives/window comparisons passed\n",cases);return 0;
}
