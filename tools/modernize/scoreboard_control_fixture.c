/* Actual command/event bodies with controlled engine input and clock traps. */
#define CGAMEDLL
#include "src/cgame/cg_local.h"
#include <stdio.h>
#include <string.h>

cg_t cg;
cgs_t cgs;
qboolean ccInitial;
static snapshot_t snapshot;
static int catcher, scoreRequests, setupCalls, checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; printf("FAIL %d: %s\n",__LINE__,#c); } } while (0)
void trap_SendClientCommand(const char *command) { CHECK(!strcmp(command,"score") || !strcmp(command,"rs")); if (!strcmp(command,"score")) ++scoreRequests; }
int trap_Key_GetCatcher(void) { return catcher; }
void trap_Key_SetCatcher(int value) { catcher=value; }
void trap_Cvar_Set(const char *name,const char *value) { (void)name;(void)value; }
void trap_S_FadeBackgroundTrack(float volume,int time,int num) { (void)volume;(void)time;(void)num; }
void trap_S_StopStreamingSound(int num) { (void)num; }
void trap_UI_Popup(int menu) { (void)menu; }
void CG_ShowHelp_Off(int *status) { *status=SHOW_OFF; }
void CG_LimboPanel_Setup(void) { ++setupCalls; }
void CG_DemoClick(int key,qboolean down) { (void)key;(void)down; }
void CG_LoadPanel_KeyHandling(int key,qboolean down) { (void)key;(void)down; }
void CG_Fireteams_KeyHandling(int key,qboolean down) { (void)key;(void)down; }
void CG_LimboPanel_KeyHandling(int key,qboolean down) { (void)key;(void)down; }
void CG_SpeakerEditor_KeyHandling(int key,qboolean down) { (void)key;(void)down; }
void CG_Debriefing_KeyEvent(int key,qboolean down) { (void)key;(void)down; }

/* ACTUAL_FUNCTIONS */

static void reset(int pmType)
{
    memset(&cg,0,sizeof(cg));memset(&cgs,0,sizeof(cgs));memset(&snapshot,0,sizeof(snapshot));
    cg.snap=&snapshot;snapshot.ps.pm_type=cg.predictedPlayerState.pm_type=pmType;
    cg.time=10000;catcher=scoreRequests=setupCalls=0;
}
int main(void)
{
    int mode;
    CHECK(CGAME_EVENT_NONE==0 && CGAME_EVENT_GAMEVIEW==1 && CGAME_EVENT_FIRETEAMMSG==5);
    for (mode=PM_NORMAL;mode<=PM_SPECTATOR;++mode) {
        reset(mode);CG_ScoresDown_f();CHECK(cg.showScores && !cg.tceScoreboardPinned && !catcher && scoreRequests==1);
        CG_ScoresUp_f();CHECK(!cg.showScores && cg.scoreFadeTime==10000 && !catcher);
        CG_ToggleScoreboard_f();CHECK(cg.showScores && cg.tceScoreboardPinned && catcher==KEYCATCH_CGAME && cgs.eventHandling==CGAME_EVENT_SCOREBOARD);
        CG_ScoresUp_f();CHECK(cg.showScores && cg.tceScoreboardPinned); /* Releasing TAB cannot dismiss a pinned board. */
        CG_KeyEvent(K_ESCAPE,qfalse);CG_KeyEvent('x',qtrue);CHECK(cg.showScores && cg.tceScoreboardPinned && catcher==KEYCATCH_CGAME);
        cg.time+=1000;CG_ScoresDown_f();CHECK(scoreRequests==1);
        cg.time+=1001;CG_ScoresDown_f();CHECK(scoreRequests==2);
        CG_KeyEvent(K_ESCAPE,qtrue);CHECK(!cg.showScores && !cg.tceScoreboardPinned && !catcher && cgs.eventHandling==CGAME_EVENT_NONE);
        CHECK(cg.scoreFadeTime==cg.time);
        CG_ToggleScoreboard_f();CG_ToggleScoreboard_f();CHECK(!cg.showScores && !cg.tceScoreboardPinned && !catcher);
        CG_ToggleScoreboard_f();CG_EventHandling(CGAME_EVENT_NONE,qtrue); /* ET2.60b routes Escape through this VM event. */
        CHECK(!cg.showScores && !cg.tceScoreboardPinned && !catcher);
        CG_ToggleScoreboard_f();CG_EventHandling(CGAME_EVENT_GAMEVIEW,qfalse);
        CHECK(!cg.showScores && !cg.tceScoreboardPinned && cg.tceShowLimboPanel && setupCalls==1);
        CG_ToggleScoreboard_f();CHECK(!cg.tceShowLimboPanel && cg.tceScoreboardPinned && cg.showScores);
        CG_KeyEvent(K_ESCAPE,qtrue);CHECK(!catcher);
    }
    reset(PM_SPECTATOR);cg.demoPlayback=qtrue;CG_ToggleScoreboard_f();CG_KeyEvent(K_ESCAPE,qtrue);
    CHECK(!cg.showScores && !cg.tceScoreboardPinned && cgs.eventHandling==CGAME_EVENT_DEMO);
    CG_EventHandling(CGAME_EVENT_NONE,qtrue);CHECK(!catcher && cgs.eventHandling==CGAME_EVENT_NONE);
    printf("%d checks, %d failures; pinned scoreboard / TAB / Escape / VM forced NONE\n",checks,failures);
    return failures?1:0;
}
