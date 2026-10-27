#define CGAMEDLL
#include "src/cgame/cg_local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef vsnprintf

cg_t cg;
cgs_t cgs;
displayContextDef_t cgDC;
vmCvar_t cf_wstats;

static int checks, failures, unrelatedCalls, requestCount, warnings, now = 1000;
static int registeredCount, drawCount, rectCount;
static char registered[256][MAX_QPATH], request[128], commandName[128];
static char words[64][128], drawn[16384];
static snapshot_t snapshot;

#define CHECK(c) do { ++checks; if (!(c)) { ++failures; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void appendDraw(const char *text, float alpha)
{
    if (alpha > 0 && strlen(drawn) + strlen(text) + 2 < sizeof(drawn))
    {
        strcat(drawn, text); strcat(drawn, "\n"); ++drawCount;
    }
}
void trap_AddCommand(const char *name)
{
    if (registeredCount >= 256) abort();
    Q_strncpyz(registered[registeredCount++], name, MAX_QPATH);
}
void trap_SendClientCommand(const char *text)
{
    ++requestCount; Q_strncpyz(request, text, sizeof(request));
}
int trap_Milliseconds(void) { return now; }
void CG_Printf(const char *format, ...) { (void)format; ++warnings; }
const char *CG_LocalizeServerCommand(const char *text) { return text; }
void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_Error(int level, const char *format, ...) { (void)level; (void)format; abort(); }
int Q_vsnprintf(char *dest, int size, const char *format, va_list args)
{ return vsnprintf(dest, (size_t)size, format, args); }
const char *CG_Argv(int index) { return index == 0 ? commandName : words[index]; }
void CG_demoAviFPSDraw(void) {}
void CG_demoTimescaleDraw(void) {}
void CG_cursorUpdate(void) {}
void CG_mvDraw(cg_window_t *window) { (void)window; }
void CG_mvOverlayDisplay(void) {}
void CG_DrawPic(float x, float y, float w, float h, qhandle_t shader)
{ (void)x; (void)y; (void)w; (void)h; (void)shader; }
void CG_FillRect(float x, float y, float w, float h, const float *color)
{ (void)x; (void)y; (void)w; (void)h; if (color[3] > 0) ++rectCount; }
void CG_DrawRect(float x, float y, float w, float h, float size, const float *color)
{ (void)x; (void)y; (void)w; (void)h; (void)size; (void)color; }
void CG_Text_Paint_Ext(float x, float y, float sx, float sy, vec4_t color,
                       const char *text, float adjust, int limit, int style, fontInfo_t *font)
{ (void)x; (void)y; (void)sx; (void)sy; (void)adjust; (void)limit; (void)style; (void)font; appendDraw(text, color[3]); }
void CG_DrawStringExt2(int x, int y, const char *text, const float *color,
                       qboolean force, qboolean shadow, int w, int h, int max)
{ (void)x; (void)y; (void)force; (void)shadow; (void)w; (void)h; (void)max; appendDraw(text, color[3]); }
int CG_DrawStrlen(const char *text) { return (int)strlen(text); }
int CG_Text_Width_Ext(const char *text, float scale, int limit, fontInfo_t *font)
{ (void)limit; (void)font; return (int)(strlen(text) * scale * 16); }
int CG_Text_Height_Ext(const char *text, float scale, int limit, fontInfo_t *font)
{ (void)text; (void)limit; (void)font; return (int)(scale * 16); }

typedef struct { char *cmd; void (*function)(void); } consoleCommand_t;
/* UNRELATED_COMMAND_STUBS */
/* PRODUCTION_FUNCTIONS */

static int registrations(const char *name)
{
    int i, total = 0;
    for (i = 0; i < registeredCount; ++i) if (!strcmp(name, registered[i])) ++total;
    return total;
}
static qboolean dispatch(const char *name)
{
    Q_strncpyz(commandName, name, sizeof(commandName)); return CG_ConsoleCommand();
}
static void clearDraw(void) { drawCount = rectCount = 0; drawn[0] = 0; }
static void reset(void)
{
    memset(&cg, 0, sizeof(cg)); memset(&cgs, 0, sizeof(cgs));
    memset(&snapshot, 0, sizeof(snapshot)); memset(words, 0, sizeof(words));
    cg.snap = &snapshot; cg.time = now = 1000; cf_wstats.value = 1;
    cg.snap->ps.clientNum = 7; cg.snap->ps.persistant[PERS_TEAM] = TEAM_AXIS;
    Q_strncpyz(cgs.clientinfo[7].name, "PopupPlayer", sizeof(cgs.clientinfo[7].name));
    cgs.clientinfo[7].team = TEAM_AXIS;
    CG_windowInit(); CG_initStrings(); requestCount = warnings = 0; clearDraw();
}
static void reply(void)
{
    /* Real existing wws layout: client, rounds, mask; five weapon counters;
     * damage given/received/team, then skill mask. WS_LUGER has headshots. */
    int values[] = { 7, 2, 1 << WS_LUGER, 6, 10, 3, 4, 2, 123, 45, 6, 0 };
    unsigned i;
    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) sprintf(words[i + 1], "%d", values[i]);
    CG_wstatsParse_cmd();
}
int main(void)
{
    cg_window_t *window;
    int i, before;
    CG_InitConsoleCommands();
    CHECK(registrations("+wstats") == 1); CHECK(registrations("-wstats") == 1);
    CHECK(registrations("+stats") == 1); CHECK(registrations("-stats") == 1);
#ifdef FEATURE_NAVSTUDIO
    CHECK(registrations("wp_add") == 1); CHECK(registrations("listbotgoals") == 1);
#else
    for (i = 0; i < registeredCount; ++i) CHECK(strncmp(registered[i], "wp_", 3) != 0);
    CHECK(registrations("listbotgoals") == 0);
    CHECK(registrations("setGoalName") == 0);
#endif
    reset(); cg.snap = NULL;
    CHECK(!dispatch("+wstats")); CHECK(!cg.showStats && requestCount == 0);
    reset();
    /* Existing aliases retain their historical no-op semantics. */
    CHECK(dispatch("+stats")); CHECK(!cg.showStats && requestCount == 0);
    CHECK(dispatch("-stats")); CHECK(!cg.showStats && requestCount == 0);
    if (!registrations("+wstats"))
    {
        CHECK(dispatch("+wstats")); CHECK(cg.showStats); CHECK(requestCount == 1);
        printf("stats popup: %d checks, %d failures (missing production registration)\n", checks, failures);
        return 1;
    }
    CHECK(dispatch("+wstats")); CHECK(cg.showStats); CHECK(requestCount == 1);
    CHECK(!strcmp(request, "wstats 7")); CHECK(cg.statsRequestTime == 1500);
    CHECK(cg.statsWindow == NULL);
    reply(); window = cg.statsWindow;
    CHECK(window != NULL && window->id == WID_STATS && window->inuse);
    CHECK(window && window->lineCount > 4);
    now += 111; clearDraw(); CG_windowDraw();
    CHECK(drawCount > 4 && rectCount > 0);
    CHECK(strstr(drawn, "PopupPlayer") != NULL);
    CHECK(strstr(drawn, "Acrcy Hits/Atts Kll Dth HS") != NULL);
    CHECK(strstr(drawn, "60.0") != NULL && strstr(drawn, "6/10") != NULL);
    CHECK(strstr(drawn, "^2  3 ^1  4 ^3 2") != NULL);
    CHECK(strstr(drawn, "Damage Given: ^7123") != NULL);
    CHECK(window->w > 10 && window->h > 0);
    CHECK(window->x == 4 && window->y >= 0 && window->y + window->h <= 480);
    before = requestCount; cg.time = 1500;
    CHECK(dispatch("+wstats")); CHECK(requestCount == before);
    cg.time = 1501; CHECK(dispatch("+wstats")); CHECK(requestCount == before + 1);
    reply(); CHECK(cg.statsWindow == window && cg.winHandler.numActiveWindows == 1);
    CHECK(dispatch("-wstats")); CHECK(!cg.showStats && cg.statsWindow == NULL);
    CHECK(window->state == WSTATE_SHUTDOWN);
    reply(); CHECK(cg.statsWindow == NULL && !cg.showStats);
    now += 111; clearDraw(); CG_windowDraw();
    now += 1; clearDraw(); CG_windowDraw();
    CHECK(drawCount == 0 && cg.winHandler.numActiveWindows == 0);
    CHECK(dispatch("-wstats")); CHECK(cg.winHandler.numActiveWindows == 0);
    CHECK(dispatch("+wstats")); reply(); CHECK(cg.statsWindow != NULL);
    reset(); cg.snap->ps.persistant[PERS_TEAM] = TEAM_SPECTATOR;
    CHECK(dispatch("+wstats")); CHECK(!cg.showStats && requestCount == 0 && warnings == 1);
    reset(); cg.snap->ps.pm_flags |= PMF_FOLLOW;
    cg.snap->ps.clientNum = 12;
    CHECK(dispatch("+wstats")); CHECK(!strcmp(request, "wstats 12"));
    reset(); cg.mvTotalClients = 1; cg.mvCurrentActive = &cg.winHandler.window[0];
    cg.mvCurrentActive->mvInfo = 23;
    CHECK(dispatch("+wstats")); CHECK(!strcmp(request, "wstats 23"));
    reset(); cg.winHandler.numActiveWindows = MAX_WINDOW_COUNT;
    CHECK(dispatch("+wstats")); reply(); CHECK(cg.statsWindow == NULL && !cg.showStats);
    CHECK(dispatch("-wstats"));
    CHECK(!dispatch("no_such_popup_command"));
    printf("stats popup: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
