#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../cgame/tce_hud_messages.h"
typedef struct {int kind;float x,y,sx,sy,c[4],adjust;int limit,style;char text[1024];} event_t;
static event_t events[20],reference[20];static int count;static const char *key="F3";
static void check(int b){if(!b){fprintf(stderr,"warmup mismatch at event %d\n",count);exit(1);}}
static const char *translate(const char *s){return s;}
static const char *binding(const char *s){return !strcmp(s,"+moveup")?"SPACE":key;}
static int width(const char *s,float f,int limit,void *font){return (int)strlen(s)*7+1;}
static void paint(float x,float y,float sx,float sy,const float *c,const char *s,float adj,int lim,int style,void *font){event_t *e=&events[count++];memset(e,0,sizeof(*e));e->kind=1;e->x=x;e->y=y;e->sx=sx;e->sy=sy;memcpy(e->c,c,16);e->adjust=adj;e->limit=lim;e->style=style;strncpy(e->text,s,1023);}
static void hook(unsigned char *p,void *f){DWORD old,tmp;check(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
#define SET(off,v) (*(int *)(b+(off))=(v))

static int view,seconds,expired;static float fadeColor[4]={1,1,1,.6f};
static int viewing(void){return view;}
static int reinf(int menu){return seconds;}
static float *fade(int time,int duration){check(duration==250);return expired?0:fadeColor;}
static void setColor(const float *c){event_t *e=&events[count++];memset(e,0,sizeof(*e));e->kind=3;if(c)memcpy(e->c,c,16);}
static void small(int x,int y,const char *s,const float *c){paint((float)x,(float)y,0,0,c,s,0,0,0,0);}
static int strlenHook(const char *s){return (int)strlen(s);}
int main(int argc,char **argv){
 HMODULE dll;unsigned char *b,snap[2048]={0};void (__cdecl *limbo)(void),(__cdecl *objective)(void);int (__cdecl *follow)(void);
 tce_hud_messages_t s;tce_hud_messages_api_t a={translate,binding,width,paint,small,setColor,fade,viewing,reinf,0};int i,j,n,r1,r2,total=0;
 check(argc==2);dll=LoadLibraryA(argv[1]);check(dll!=NULL);b=(unsigned char *)dll;
 limbo=(void *)(b+0x26920);follow=(void *)(b+0x26ac0);objective=(void *)(b+0x27d40);
 hook(b+0x2a830,translate);hook(b+0x84090,binding);hook(b+0x20910,width);hook(b+0x20ba0,paint);hook(b+0x2a590,small);hook(b+0x65420,setColor);hook(b+0x2a730,fade);hook(b+0x3f3f0,viewing);hook(b+0x21430,reinf);hook(b+0x2a5c0,strlenHook);
 *(void **)(b+0x40352c4)=snap;
 for(i=0;i<16000;++i){
  memset(&s,0,sizeof(s));s.health=i%3-1;s.pmFlags=((i/3)%4)*0x1000;if(i%2)s.pmFlags|=0x4000;
  s.localTeam=(i/7)%4;s.descriptive=(i/11)%2;s.gameType=(i/13)%9;s.respawns=(i/17)%3-1;s.penalty=(i/19)%5-1;
  s.localClient=0;s.client=(i/23)%2;s.followTeam=(i/29)%3;s.redTime=(i%5)*1000+30000;s.blueTime=(i%7)*999+5000;s.name="Test Player";
  key=i%2?"L":(i%3?"(openlimbomenu)":"(OPENLIMBOMENU)");seconds=i%31;view=(i/37)%2;
  SET(0x40352a4,0);SET(0x2580a98,s.localTeam);if(s.client)SET(0x2580a98+0x1c0,s.followTeam);else s.followTeam=s.localTeam;
  strcpy((char *)b+0x2580a18+s.client*0x1c0,s.name);
  SET(0x40139ac,s.descriptive);SET(0x257e100,s.gameType);
  *(int *)(snap+0xfc)=s.health;*(int *)(snap+0x38)=s.pmFlags;*(int *)(snap+0x158)=s.respawns;*(int *)(snap+0x15c)=s.penalty;*(int *)(snap+0xcc)=s.client;
  SET(0x488a32c,s.redTime);SET(0x488b2ec,s.blueTime);
  count=0;limbo();n=count;memcpy(reference,events,sizeof(events));count=0;TCE_DrawLimboMessage(&s,&a);
  if(n!=count || memcmp(reference,events,n*sizeof(event_t)))goto mismatch;++total;
  /* Penalty branch tested after supplying the original cvar offsets below. */

  count=0;r1=follow();n=count;memcpy(reference,events,sizeof(events));count=0;r2=TCE_DrawFollow(&s,&a);
  if(r1!=r2 || n!=count || memcmp(reference,events,n*sizeof(event_t)))goto mismatch;++total;
 }
 for(i=0;i<1000;++i){
  const char *texts[]={"You are near to the specops flag [A]\n","one\ntwo","","1234567890123456789012345678901234567890123456789012345678901234567890\nlast"};
  s.objective=texts[i%4];s.objectiveTime=i%3?100:0;s.objectiveLines=1+i%4;s.objectiveCharWidth=6+i%8;expired=i%5==0;
  SET(0x4081118,s.objectiveTime);SET(0x408111c,s.objectiveCharWidth);SET(0x4081524,s.objectiveLines);strcpy((char *)b+0x4081124,s.objective);
  count=0;objective();n=count;memcpy(reference,events,sizeof(events));count=0;TCE_DrawObjectiveInfo(&s,&a);
  if(n!=count || memcmp(reference,events,n*sizeof(event_t)) || s.objectiveTime!=*(int *)(b+0x4081118))goto mismatch;++total;
 }
 printf("%d original HUD message comparisons passed\n",total);return 0;
 mismatch:fprintf(stderr,"case %d gameType%d events %d/%d\n",i,s.gameType,count,n);for(j=0;j<count||j<n;++j)fprintf(stderr,"%d: '%s' / '%s' xy%g,%g/%g,%g scale%g/%g\n",j,events[j].text,reference[j].text,events[j].x,events[j].y,reference[j].x,reference[j].y,events[j].sx,reference[j].sx);return 1;
}
