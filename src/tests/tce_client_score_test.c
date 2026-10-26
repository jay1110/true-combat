#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "../cgame/tce_client_score.h"
typedef struct { int kind,shader;float v[8];char text[256]; } event_t;
static event_t events[64],expected[64];static int count,cases;static unsigned char *base;
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"case %d: %s\n",cases,why);exit(1);}}
static event_t *record(int kind){event_t *e;require(count<64,"capacity");e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
static void fill(float x,float y,float w,float h,const float *c){event_t *e=record(1);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;memcpy(e->v+4,c,16);}
static void pic(float x,float y,float w,float h,int shader){event_t *e=record(2);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;e->shader=shader;}
static void text(float x,float y,float scale,const float *c,const char *s){event_t *e=record(3);e->v[0]=x;e->v[1]=y;e->v[2]=scale;memcpy(e->v+3,c,16);snprintf(e->text,sizeof(e->text),"%s",s);}
static void originalText(float x,float y,float sx,float sy,const float *c,const char *s,float adjust,int limit,int style,void *font){require(sx==sy&&adjust==0&&limit==0&&style==0&&font==base+0x2588f8c,"text extras/font");text(x,y,sx,c,s);}
static int drawStrlen(const char *s){event_t *e=record(4);snprintf(e->text,sizeof(e->text),"%s",s);return 17;}
static const char *translate(const char *s){event_t *e=record(5);snprintf(e->text,sizeof(e->text),"%s",s);return "Translated spectator";}
static void locate(int client){event_t *e=record(6);e->shader=client;}
static int originalLocate(int client){locate(client);return 1;}
typedef struct {unsigned char *p,bytes[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *fn){DWORD old,tmp;h->p=base+offset;memcpy(h->bytes,h->p,5);require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&old),"protect");h->p[0]=0xe9;*(int *)(h->p+1)=(unsigned char *)fn-h->p-5;FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,old,&tmp);}
static void unhook(hook_t *h){DWORD old,tmp;require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&old),"restore");memcpy(h->p,h->bytes,5);FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,old,&tmp);}

#define SETI(offset,value) (*(int *)(base+(offset))=(value))
int main(int argc,char **argv){
 HMODULE dll;hook_t h[6];tce_clientScoreContext_t c; tce_score_t s,before;
 unsigned char snap[512],snapBefore[512];float inputColor[4]={.2f,.3f,.4f,.5f};
 int mode,team,local,flags,boundary,f,i,j,n;int ys[]={-16,80,453,454,470,1000};
 int healths[]={-1,0,100};int damages[]={-99999,-51,-1,0,1,51,12345,INT_MAX};
 void (__cdecl *original)(int,int,const tce_score_t *,const float *,float);
 require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load");base=(unsigned char *)dll;original=(void *)(base+0x5a960);
 hook(&h[0],base,0x29850,fill);hook(&h[1],base,0x29ea0,pic);hook(&h[2],base,0x20ba0,originalText);hook(&h[3],base,0x2a5c0,drawStrlen);hook(&h[4],base,0x2a830,translate);hook(&h[5],base,0x4e110,originalLocate);
 memset(&c,0,sizeof(c));c.fill=fill;c.pic=pic;c.text=text;c.drawStrlen=drawStrlen;c.translate=translate;c.locateMergedClient=locate;
 c.objectiveShader=111;c.eliminatedShader=222;c.medicShader=333;
 SETI(0x2588104,c.objectiveShader);SETI(0x25880f4,c.eliminatedShader);SETI(0x2588f38,c.medicShader);
 *(void **)(base+0x40352c4)=snap;SETI(0x40352a4,1);
 for(mode=0;mode<8;++mode)for(team=1;team<=3;++team)for(local=1;local<=3;++local)
 for(flags=0;flags<64;++flags)for(boundary=0;boundary<6;++boundary)for(f=0;f<3;++f){
  memset(&s,0,sizeof(s));s.client=2;s.score=flags-23;s.ping=flags==0?-1:flags*37;s.time=4242;s.powerUps=123;
  s.respawnsLeft=flags&1?-2:0;s.classRating=flags==63?INT_MAX:flags-8;s.damageRating=damages[flags%8];s.kills=flags-10;s.deaths=123-flags;s.suicides=flags%3;s.teamKills=flags%7;
  c.gameType=mode;c.team=team;c.localTeam=local;c.viewedClient=flags&2?2:3;
  c.powerups=(flags%4)*64;c.health=healths[(flags/4)%3];c.pmType=flags&8?5:0;
  c.killMessage=flags&16?0:1;c.viewedHealth=healths[(flags/2)%3];c.viewedTeam=flags&32?team:0;
  c.name=flags&1?"^1Colored Player With A Long Name":"player";
  memset(snap,0,sizeof(snap));*(int *)(snap+0xcc)=c.viewedClient;*(int *)(snap+0x30)=c.pmType;*(int *)(snap+0xfc)=c.viewedHealth;*(int *)(snap+0x148)=c.viewedTeam;
  SETI(0x2580a98+448,local);SETI(0x2580a98+896,team);SETI(0x2580ab4+896,c.powerups);SETI(0x2580aac+896,c.health);
  strcpy((char *)(base+0x2580a18+896),c.name);SETI(0x488c18c,mode);SETI(0x3fdf388,c.killMessage);
  before=s;memcpy(snapBefore,snap,sizeof(snap));count=0;
  original(flags&1?443:20,ys[boundary],&s,inputColor,f*.5f);n=count;memcpy(expected,events,n*sizeof(event_t));
  require(!memcmp(&s,&before,sizeof(s)),"original score mutation");require(!memcmp(snap,snapBefore,sizeof(snap)),"snapshot mutation");
  count=0;TCE_WM_DrawClientScore(flags&1?443:20,ys[boundary],&s,inputColor,f*.5f,&c);
  require(!memcmp(&s,&before,sizeof(s)),"port score mutation");require(count==n,"call count");
  for(i=0;i<n;++i){if(events[i].kind!=expected[i].kind||events[i].shader!=expected[i].shader||strcmp(events[i].text,expected[i].text)){fprintf(stderr,"event %d kind %d/%d shader %d/%d text [%s]/[%s]\n",i,events[i].kind,expected[i].kind,events[i].shader,expected[i].shader,events[i].text,expected[i].text);require(0,"event arguments");}for(j=0;j<8;++j)if(!isfinite(events[i].v[j])||fabs(events[i].v[j]-expected[i].v[j])>.000001){fprintf(stderr,"event %d float %d: %.9g/%.9g\n",i,j,events[i].v[j],expected[i].v[j]);require(0,"float arguments");}}
  ++cases;
 }
 for(i=5;i>=0;--i)unhook(&h[i]);FreeLibrary(dll);printf("%d complete WM_DrawClientScore original comparisons passed.\n",cases);return 0;
}
