#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../cgame/tce_scoreboard.h"
typedef struct {int kind,x,y,z,rows;float fade;} event_t;
static event_t events[8],expected[8];static int count,cases,fadeMode;
static float color[4];
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"case %d: %s\n",cases,why);exit(1);}}
static void record(int kind,int x,int y,int z,int rows,float fade){event_t *e;require(count<8,"events");e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;e->x=x;e->y=y;e->z=z;e->rows=rows;e->fade=fade;}
static float *fadeColor(int start,int duration){record(1,start,duration,0,0,0);return fadeMode?color:NULL;}
static int objectives(int x,int y,int width,float fade){record(2,x,y,width,0,fade);return 57;}
static int infoLine(int x,int y,float fade){record(3,x,y,0,0,fade);return 179;}
static int teamBoard(int x,int y,int team,float fade,int rows){record(4,x,y,team,rows,fade);return 333;}
typedef struct {unsigned char *p,bytes[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *fn){DWORD old,tmp;h->p=base+offset;memcpy(h->bytes,h->p,5);require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&old),"protect");h->p[0]=0xe9;*(int *)(h->p+1)=(unsigned char *)fn-h->p-5;FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,old,&tmp);}
static void unhook(hook_t *h){DWORD old,tmp;require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&old),"restore");memcpy(h->p,h->bytes,5);FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,old,&tmp);}
int main(int argc,char **argv){
    HMODULE dll;unsigned char *base;hook_t h[4];tce_scoreboardContext_t ctx;
    int mask,pm,pred,game,f,n,a,b,i;char name[64],savedName[64];
    unsigned char snap[64]={0};int (__cdecl *original)(void);
    const int offsets[]={0x4013bec,0x4080e30,0x40352b0,0x40804f8,0x408110c,0x407dec4,0x4080500,0x257e100};int saved[8];void *savedSnap;
    require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load");base=(unsigned char *)dll;original=(void *)(base+0x59df0);
    for(i=0;i<8;++i)saved[i]=*(int *)(base+offsets[i]);savedSnap=*(void **)(base+0x40352c4);memcpy(savedName,base+0x4080504,64);
    hook(&h[0],base,0x2a730,fadeColor);hook(&h[1],base,0x596b0,objectives);hook(&h[2],base,0x59f20,infoLine);hook(&h[3],base,0x5a0b0,teamBoard);
    memset(&ctx,0,sizeof(ctx));ctx.killerName=name;ctx.fadeColor=fadeColor;ctx.objectives=objectives;ctx.infoLine=infoLine;ctx.teamBoard=teamBoard;
    for(mask=0;mask<32;++mask)for(pm=0;pm<2;++pm)for(pred=0;pred<2;++pred)for(game=2;game<=7;++game)for(f=0;f<4;++f){
        ctx.paused=mask&1;ctx.warmup=mask&2;ctx.demoPlayback=mask&4;ctx.showScores=mask&8;ctx.cameraMode=mask&16;
        ctx.snapshotPmType=pm?5:0;ctx.predictedPmType=pred?5:0;ctx.scoreFadeTime=-12345;ctx.gametype=game;
        *(int *)(base+offsets[0])=ctx.paused;*(int *)(base+offsets[1])=ctx.warmup;*(int *)(base+offsets[2])=ctx.demoPlayback;*(int *)(base+offsets[3])=ctx.showScores;*(int *)(base+offsets[4])=ctx.cameraMode;
        *(int *)(base+offsets[5])=ctx.predictedPmType;*(int *)(base+offsets[6])=ctx.scoreFadeTime;*(int *)(base+offsets[7])=ctx.gametype;
        *(int *)(snap+0x30)=ctx.snapshotPmType;*(void **)(base+0x40352c4)=snap;
        memset(name,'K',sizeof(name));name[63]=0;memcpy(base+0x4080504,name,64);fadeMode=f;color[3]=(f-1)*.5f;
        count=0;b=original();n=count;memcpy(expected,events,sizeof(events));count=0;a=TCE_CG_DrawScoreboard(&ctx);
        require(a==b,"return");if(count!=n||memcmp(events,expected,n*sizeof(event_t))){for(i=0;i<count||i<n;++i)fprintf(stderr,"event %d: %d %d %d %d %d %g vs %d %d %d %d %d %g\n",i,events[i].kind,events[i].x,events[i].y,events[i].z,events[i].rows,events[i].fade,expected[i].kind,expected[i].x,expected[i].y,expected[i].z,expected[i].rows,expected[i].fade);}require(count==n&&!memcmp(events,expected,n*sizeof(event_t)),"drawing call sequence/arguments");require(!memcmp(name,base+0x4080504,64),"killer name");++cases;
    }
    for(i=3;i>=0;--i)unhook(&h[i]);for(i=0;i<8;++i)*(int *)(base+offsets[i])=saved[i];*(void **)(base+0x40352c4)=savedSnap;memcpy(base+0x4080504,savedName,64);
    FreeLibrary(dll);printf("%d complete CG_DrawScoreboard original comparisons passed.\n",cases);return 0;
}
