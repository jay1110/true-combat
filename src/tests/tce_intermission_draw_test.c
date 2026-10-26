#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_DrawIntermission(void);
extern void CG_Debriefing_InfoRequests(void);
extern int QDECL CG_SortPlayersByXP(const void *,const void *);
extern panel_button_t *teamDebriefPanelButtons[],*chatPanelButtons[],*debriefPanelButtons[];
static unsigned char *b;static char events[8192];
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void stats(void){strcat(events,"stats;");}
static void screen(void){strcat(events,"screenshot;");}
static void console(const char *s){strcat(events,s);}
static void draw(void){strcat(events,"debrief;");}
static void startup(void){strcat(events,"startup;");cgs.dbShowing=1;*(int *)(b+0x3fde4d0)=1;}
static void requests(void){strcat(events,"requests;");}
static int score(void){strcat(events,"score;");return 1;}
static int sort(const void *a,const void *c){return *(int *)c-*(int *)a;}
static void panel(panel_button_t **p){int id=-1;if(p==teamDebriefPanelButtons||(void *)p==b+0xa2060)id=1;if(p==chatPanelButtons||(void *)p==b+0xa2400)id=2;if(p==debriefPanelButtons||(void *)p==b+0xa1338)id=3;sprintf(events+strlen(events),"panel%d;",id);}
static void pic(float x,float y,float w,float h,qhandle_t shader){sprintf(events+strlen(events),"pic:%g,%g,%g,%g,%d;",x,y,w,h,shader);}
int main(int argc,char **argv){HMODULE h;int demo,actions,gt,round,t,mode,show,i,cases=0,a,c;char expected[8192];unsigned char saved[5],ours[5];const int times[]={100,1100,1101,5100,5101,9000};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x1cb90,stats);patch(CG_dumpStats_f,stats);patch(b+0x1ca80,screen);patch(CG_autoScreenShot_f,screen);patch(b+0x64d60,console);patch(trap_SendConsoleCommand,console);
 memcpy(saved,b+0x1d620,5);memcpy(ours,CG_Debriefing_Draw,5);patch(b+0x1d620,draw);patch(CG_Debriefing_Draw,draw);
 for(demo=0;demo<2;++demo)for(actions=0;actions<8;++actions)for(gt=0;gt<8;++gt)for(round=0;round<2;++round){
  cg.latchAutoActions=*(int *)(b+0x4081570)=0;cg.demoPlayback=*(int *)(b+0x40352b0)=demo;cg_autoAction.integer=*(int *)(b+0x4846e8c)=actions;cgs.gametype=*(int *)(b+0x257e100)=gt;cgs.currentRound=*(int *)(b+0x3fb7804)=round;
  for(t=0;t<6;++t){cg.time=*(int *)(b+0x407dea4)=times[t];events[0]=0;((void(__cdecl *)(void))(b+0x26760))();strcpy(expected,events);events[0]=0;CG_DrawIntermission();if(strcmp(events,expected)||cg.latchAutoActions!=*(int *)(b+0x4081570)){printf("auto mismatch %d %d %d %d %d %s/%s\n",demo,actions,gt,round,t,expected,events);return 1;}++cases;}
 }
 /* Restore actual debriefing functions, then compare complete dispatch. */
 {DWORD o;VirtualProtect(b+0x1d620,5,PAGE_EXECUTE_READWRITE,&o);memcpy(b+0x1d620,saved,5);VirtualProtect(b+0x1d620,5,o,&o);FlushInstructionCache(GetCurrentProcess(),b+0x1d620,5);}
 {DWORD o;VirtualProtect(CG_Debriefing_Draw,5,PAGE_EXECUTE_READWRITE,&o);memcpy(CG_Debriefing_Draw,ours,5);VirtualProtect(CG_Debriefing_Draw,5,o,&o);FlushInstructionCache(GetCurrentProcess(),CG_Debriefing_Draw,5);}
 patch(b+0x1d500,startup);patch(CG_Debriefing_Startup,startup);patch(b+0x1d580,requests);patch(CG_Debriefing_InfoRequests,requests);patch(b+0x59df0,score);patch(CG_DrawScoreboard,score);
 patch(b+0x880f0,panel);patch(BG_PanelButtonsRender,panel);patch(b+0x1de10,sort);patch(CG_SortPlayersByXP,sort);patch(b+0x29ea0,pic);patch(CG_DrawPic,pic);
 for(mode=-1;mode<4;++mode)for(show=0;show<2;++show)for(t=0;t<5;++t){
  cgs.dbMode=*(int *)(b+0x3fde4cc)=mode;cgs.dbShowing=*(int *)(b+0x3fde4d0)=show;cgDC.cursorx=*(int *)(b+0x4014cc8)=t*71;cgDC.cursory=*(int *)(b+0x4014ccc)=t*53;cgs.media.cursorIcon=*(int *)(b+0x259d17c)=42;
  for(i=0;i<64;++i)cgs.dbSortedClients[i]=*(int *)(b+0x3fde3c8+i*4)=99;
  events[0]=0;a=((int(__cdecl *)(void))(b+0x1d620))();strcpy(expected,events);events[0]=0;cgs.dbShowing=show;c=CG_Debriefing_Draw();
  if(a!=c||strcmp(expected,events)){printf("dispatch mismatch mode%d show%d: %s/%s\n",mode,show,expected,events);return 1;}
  for(i=0;i<64;++i)if(cgs.dbSortedClients[i]!=*(int *)(b+0x3fde3c8+i*4))return 1;++cases;
 }
 printf("%d whole Windows intermission/draw comparisons passed\n",cases);return 0;
}
