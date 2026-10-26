#include "../cgame/cg_local.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
cg_t cg;cgs_t cgs;vmCvar_t cg_complaintPopUp;
void CG_DrawVote(void);
typedef struct {float x,y,sx,sy,c[4],adjust;int limit,style;char text[1024];} event_t;
static event_t events[8],reference[8];static int count;
static void check(int b){if(!b){fprintf(stderr,"vote check failed\n");exit(1);}}
char *CG_TranslateString(const char *s){return (char *)s;}
char *BindingFromName(const char *s){return !strcmp(s,"vote yes")?"F1":"F2";}
void CG_Text_Paint_Ext(float x,float y,float sx,float sy,vec4_t c,const char *s,float adj,int lim,int style,fontInfo_t *font){event_t *e=&events[count++];memset(e,0,sizeof(*e));e->x=x;e->y=y;e->sx=sx;e->sy=sy;memcpy(e->c,c,16);e->adjust=adj;e->limit=lim;e->style=style;strncpy(e->text,s,1023);}
void QDECL Com_Printf(const char *fmt,...){}
void QDECL Com_Error(int n,const char *fmt,...){exit(2);}
#undef _vsnprintf
int Q_vsnprintf(char *out,int size,const char *fmt,va_list args){int r; r=_vsnprintf(out,size-1,fmt,args);out[size-1]=0;return r;}
static void hook(unsigned char *p,void *f){DWORD old,tmp;check(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
#define SET(off,v) (*(int *)(b+(off))=(v))
static unsigned seed=9387;
static int sample(int n){seed=seed*1664525+1013904223;return (seed>>16)%n;}
int main(int argc,char **argv){HMODULE dll;unsigned char *b,snap[1024]={0};snapshot_t sdkSnap={0};void (__cdecl *original)(void);int i,j,n;int *fields[]={&cgs.complaintClient,&cgs.applicationClient,&cgs.propositionClient,&cgs.invitationClient,&cgs.autoFireteamNum,&cgs.autoFireteamCreateNum,&cgs.autoFireteamJoinNum};int *ends[]={&cgs.complaintEndTime,&cgs.applicationEndTime,&cgs.propositionEndTime,&cgs.invitationEndTime,&cgs.autoFireteamEndTime,&cgs.autoFireteamCreateEndTime,&cgs.autoFireteamJoinEndTime};int offsets[]={0x3fb781c,0x3fc4294,0x3fc429c,0x3fc428c,0x3fc42ac,0x3fc42b4,0x3fc42bc};int endOffsets[]={0x3fb7820,0x3fc4298,0x3fc42a4,0x3fc4290,0x3fc42a8,0x3fc42b0,0x3fc42b8};
 check(argc==2);dll=LoadLibraryA(argv[1]);check(dll!=NULL);b=(unsigned char *)dll;original=(void *)(b+0x25db0);hook(b+0x2a830,CG_TranslateString);hook(b+0x84090,BindingFromName);hook(b+0x20ba0,CG_Text_Paint_Ext);*(void **)(b+0x40352c4)=snap;cg.snap=&sdkSnap;cg.clientNum=0;SET(0x40352a4,0);
 for(j=0;j<64;++j){sprintf(cgs.clientinfo[j].name,"Player%d",j);sprintf(cgs.clientinfo[j].cleanname,"Player%d",j);strcpy((char *)b+0x2580a18+j*0x1c0,cgs.clientinfo[j].name);strcpy((char *)b+0x2580a58+j*0x1c0,cgs.clientinfo[j].cleanname);cgs.clientinfo[j].infoValid=1;SET(0x2580a10+j*0x1c0,1);}
 for(i=0;i<30000;++i){cg.time=30000+sample(12000);SET(0x407dea4,cg.time);cg.demoPlayback=sample(2);SET(0x40352b0,cg.demoPlayback);cg_complaintPopUp.integer=sample(3)-1;SET(0x48471ec,cg_complaintPopUp.integer);
  for(j=0;j<7;++j){*fields[j]=sample(8)-5;*ends[j]=sample(2)?45000:0;SET(offsets[j],*fields[j]);SET(endOffsets[j],*ends[j]);}
  cgs.propositionClient2=5;SET(0x3fc42a0,5);
  for(j=0;j<64;++j){cgs.clientinfo[j].team=sample(4);SET(0x2580a98+j*0x1c0,cgs.clientinfo[j].team);}
  sdkSnap.ps.eFlags=sample(2)?EF_VOTED:0;*(int *)(snap+0x94)=sdkSnap.ps.eFlags;
  cgs.voteTime=sample(2)?sample(30000)+1:0;SET(0x257e214,cgs.voteTime);cgs.voteYes=sample(64);SET(0x257e218,cgs.voteYes);cgs.voteNo=sample(64);SET(0x257e21c,cgs.voteNo);cgs.voteModified=sample(2);SET(0x257e220,cgs.voteModified);
  strcpy(cgs.voteString,sample(2)?"kick Player3":"map obj_delta");strcpy((char *)b+0x257e224,cgs.voteString);
  count=0;original();n=count;memcpy(reference,events,sizeof(events));count=0;CG_DrawVote();
  if(count!=n||memcmp(reference,events,n*sizeof(event_t))||cgs.voteModified!=*(int *)(b+0x257e220)){fprintf(stderr,"vote case%d count%d/%d\n",i,count,n);for(j=0;j<count||j<n;++j)fprintf(stderr,"%s / %s\n",events[j].text,reference[j].text);return 1;}
 }
 printf("30000 original vote/fireteam/complaint comparisons passed\n");FreeLibrary(dll);return 0;
}
