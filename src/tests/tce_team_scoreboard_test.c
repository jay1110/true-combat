#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../cgame/tce_team_scoreboard.h"
typedef struct { int kind,ints[4];float v[9];char text[256]; } event_t;
static event_t events[256],expected[256];static int count,cases;static unsigned char *base;
static float back[4]={0,0,0,.6f},bar[4]={.16f,.2f,.17f,.8f},black[4]={0,0,0,1};
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"case %d: %s\n",cases,why);exit(1);}}
static event_t *record(int kind){event_t *e;require(count<256,"capacity");e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
static void rectangle(int kind,float x,float y,float w,float h,const float *c){event_t *e=record(kind);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;if(c)memcpy(e->v+4,c,16);}
static void fill(float x,float y,float w,float h,const float *c){rectangle(1,x,y,w,h,c);}
static void border(float x,float y,float w,float h,int size,const float *c){rectangle(2,x,y,w,h,c);events[count-1].ints[0]=size;}
static void setColor(const float *c){event_t *e=record(3);e->ints[0]=c!=NULL;if(c)memcpy(e->v,c,16);}
static void topBottom(float x,float y,float w,float h,float size){rectangle(4,x,y,w,h,NULL);events[count-1].v[4]=size;}
static const char *translate(const char *s){event_t *e=record(5);snprintf(e->text,sizeof(e->text),"%s",s);return s;}
static void text(float x,float y,float scale,const float *c,const char *s){event_t *e=record(6);e->v[0]=x;e->v[1]=y;e->v[2]=scale;memcpy(e->v+3,c,16);snprintf(e->text,sizeof(e->text),"%s",s);}
static void originalText(float x,float y,float sx,float sy,const float *c,const char *s,float adjust,int limit,int style,void *font){require(sx==sy&&adjust==0&&limit==0&&style==0&&font==base+0x2588f8c,"text extras/font");text(x,y,sx,c,s);}
static void smallText(int x,int y,const char *s,float alpha){event_t *e=record(7);e->ints[0]=x;e->ints[1]=y;e->v[0]=alpha;snprintf(e->text,sizeof(e->text),"%s",s);}
static void clientRow(int x,int y,int index,const float *c,float fade){event_t *e=record(8);e->ints[0]=x;e->ints[1]=y;e->ints[2]=index;memcpy(e->v,c,16);e->v[4]=fade;}
static void originalRow(int x,int y,void *score,const float *c,float fade){clientRow(x,y,((unsigned char *)score-(base+0x407f5f8))/60,c,fade);}
typedef struct {unsigned char *p,bytes[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *fn){DWORD old,tmp;h->p=base+offset;memcpy(h->bytes,h->p,5);require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&old),"protect");h->p[0]=0xe9;*(int *)(h->p+1)=(unsigned char *)fn-h->p-5;FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,old,&tmp);}
static void unhook(hook_t *h){DWORD old,tmp;require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&old),"restore");memcpy(h->p,h->bytes,5);FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,old,&tmp);}
int main(int argc,char **argv){
 HMODULE dll;hook_t h[8];tce_teamScoreboardContext_t ctx;int clients[64],teams[64],players[4];
 int mode,team,rows,f,layout,i,j,n,a,b;const int rowCounts[]={-1,0,1,8,20};
 int (__cdecl *original)(int,int,int,float,int);
 require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load");base=(unsigned char *)dll;original=(void *)(base+0x5a0b0);
 hook(&h[0],base,0x29850,fill);hook(&h[1],base,0x29df0,border);hook(&h[2],base,0x65420,setColor);hook(&h[3],base,0x29c10,topBottom);hook(&h[4],base,0x2a830,translate);hook(&h[5],base,0x20ba0,originalText);hook(&h[6],base,0x2a540,smallText);hook(&h[7],base,0x5a960,originalRow);
 memset(&ctx,0,sizeof(ctx));ctx.teamPlayers=players;ctx.scoreClients=clients;ctx.clientTeams=teams;
 ctx.backColor=back;ctx.barColor=bar;ctx.black=black;ctx.fill=fill;ctx.border=border;ctx.setColor=setColor;ctx.topBottom=topBottom;ctx.translate=translate;ctx.text=text;ctx.smallText=smallText;ctx.clientRow=clientRow;
 memcpy(base+0xb0d94,back,16);memcpy(base+0xb0da4,bar,16);memcpy(base+0xb4694,black,16);
 for(mode=0;mode<8;++mode)for(team=1;team<=2;++team)for(rows=0;rows<5;++rows)for(f=0;f<3;++f)for(layout=0;layout<8;++layout){
  ctx.gametype=mode;ctx.gameTypeCvar=layout&1?7:mode;ctx.firstBlood=layout%3;ctx.teamScores[0]=13;ctx.teamScores[1]=-7;ctx.numScores=layout==0?0:layout==1?3:32;
  players[0]=0;players[1]=7;players[2]=9;players[3]=0;
  for(i=0;i<64;++i){clients[i]=i;teams[i]=(i+layout)%3+1;*(int *)(base+0x407f5f8+i*60)=i;*(int *)(base+0x2580a98+i*448)=teams[i];}
  *(int *)(base+0x257e100)=ctx.gametype;*(int *)(base+0x488c18c)=ctx.gameTypeCvar;*(int *)(base+0x4080db0)=ctx.firstBlood;
  *(int *)(base+0x407f5e0)=13;*(int *)(base+0x407f5e4)=-7;*(int *)(base+0x407f5d8)=ctx.numScores;memcpy(base+0x407f5e8,players,16);
  count=0;b=original(team==1?20:443,57,team,f*.5f,rowCounts[rows]);n=count;memcpy(expected,events,n*sizeof(event_t));count=0;
  a=TCE_WM_TeamScoreboard(team==1?20:443,57,team,f*.5f,rowCounts[rows],&ctx);
  require(a==b,"returned Y");require(!memcmp(players,base+0x407f5e8,16),"team counts");require(count==n,"call count");
  for(i=0;i<n;++i){if(events[i].kind!=expected[i].kind||memcmp(events[i].ints,expected[i].ints,sizeof(events[i].ints))||strcmp(events[i].text,expected[i].text)){fprintf(stderr,"event %d kind %d/%d text %s/%s\n",i,events[i].kind,expected[i].kind,events[i].text,expected[i].text);require(0,"event arguments");}for(j=0;j<9;++j)if(fabs(events[i].v[j]-expected[i].v[j])>.00001){fprintf(stderr,"event %d float %d: %g/%g\n",i,j,events[i].v[j],expected[i].v[j]);require(0,"float arguments");}}
  ++cases;
 }
 for(i=7;i>=0;--i)unhook(&h[i]);FreeLibrary(dll);printf("%d complete WM_TeamScoreboard original comparisons passed.\n",cases);return 0;
}
