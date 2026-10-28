/* Actual quality helpers; only the engine cvar/command services are modeled. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define Q_stricmp _stricmp
#else
#include <strings.h>
#define Q_stricmp strcasecmp
#endif
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define Q_strncmp strncmp
#define MAX_CVAR_VALUE_STRING 256
#define EXEC_APPEND 2
typedef int qboolean;
enum { qfalse, qtrue };
typedef struct { char name[80], value[256], latched[256]; int latch, engine; } fakeCvar;
static fakeCvar cvars[96];
static int cvarCount, engineWrites, unexpectedWrites, unsupportedCreates, commandCount;
static int checks, failures;
static char currentCase[64];
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; printf("FAIL %s:%d %s\n", currentCase, __LINE__, #c); } } while (0)
static const char *qualityNames[] = {
    "r_picmip", "r_textureMode", "r_ext_texture_filter_anisotropic",
    "r_ext_max_anisotropy", "r_textureAnisotropy", "r_ext_multisample", "r_fbo",
    "r_texturebits", "r_ext_compressed_textures", "r_subdivisions", "r_lodbias",
    "r_detailtextures", "r_simpleMipMaps"
};
static fakeCvar *find(const char *name) {
    int i; for (i = 0; i < cvarCount; ++i) if (!Q_stricmp(name, cvars[i].name)) return &cvars[i];
    return NULL;
}
static fakeCvar *add(const char *name, const char *value, int latch, int engine) {
    fakeCvar *v = &cvars[cvarCount++];
    if (cvarCount > ARRAY_LEN(cvars)) abort();
    strcpy(v->name, name); strcpy(v->value, value); v->latch = latch; v->engine = engine;
    return v;
}
static const char *get(const char *name) { fakeCvar *v = find(name); return v ? v->value : ""; }
static void trap_Cvar_VariableStringBuffer(const char *name, char *value, int size) {
    snprintf(value, (size_t)size, "%s", get(name));
}
static float trap_Cvar_VariableValue(const char *name) { return (float)atof(get(name)); }
static void trap_Cvar_Set(const char *name, const char *value) {
    fakeCvar *v = find(name); int i, quality = 0;
    for (i = 0; i < ARRAY_LEN(qualityNames); ++i) if (!Q_stricmp(name, qualityNames[i])) quality = 1;
    if (!v) {
        if (strncmp(name, "ui_tceGfx_", 10)) { ++unsupportedCreates; ++unexpectedWrites; }
        v = add(name, "", 0, 0);
    }
    if (v->engine) { ++engineWrites; if (!quality) ++unexpectedWrites; }
    if (v->latch) strcpy(v->latched, value); else strcpy(v->value, value);
}
static void trap_Cmd_ExecuteText(int when, const char *command) {
    CHECK(when == EXEC_APPEND); CHECK(!strcmp(command, "vid_restart\n")); ++commandCount;
}
/* PRODUCTION_HELPERS */
static void dispatch(const char *name) {
    /* PRODUCTION_DISPATCH_PREFIX */
    CHECK(0);
}
enum { KEYCATCH_UI = 2, CA_DISCONNECTED = 0, CA_ACTIVE = 8, GT_SINGLE_PLAYER = 2, EXEC_INSERT = 1 };
typedef struct { int connState; } uiClientState_t;
static struct {
    struct { int realTime, frameTime; } uiDC;
    int campaignCount;
    struct { int typeBits, unlocked, progress, mapCount; } campaignList[4];
} uiInfo;
static int menuExists, menuActive, closeCalls, openCalls, keyCalls, catcher, unrelatedCalls, clientState;
static char consoleCommand[80], consolePrint[256];
static char *UI_Argv(int arg) { CHECK(arg == 0); return consoleCommand; }
static void Menus_CloseAll(void) { ++closeCalls; menuActive = 0; }
static int Menus_ActivateByName(const char *name, qboolean focus) {
    CHECK(!strcmp(name, "options_quality")); CHECK(focus == qtrue); ++openCalls;
    if (!menuExists) return 0;
    menuActive = 1; dispatch("tceGraphicsGet"); return 1;
}
static void trap_Key_SetCatcher(int value) { ++keyCalls; catcher = value; }
static void trap_Print(const char *text) { snprintf(consolePrint, sizeof(consolePrint), "%s", text); }
static void trap_GetClientState(uiClientState_t *state) { state->connState = clientState; }
static int trap_Argc(void) { ++unrelatedCalls; return 0; }
static void trap_Argv(int arg, char *value, int size) { (void)arg; (void)size; ++unrelatedCalls; value[0] = 0; }
static void TCE_UI_ApplyVideoMode(int mode) { (void)mode; ++unrelatedCalls; }
static void UI_ShowPostGame(int show) { (void)show; ++unrelatedCalls; }
static void UI_Report(void) { ++unrelatedCalls; }
static void UI_Load(void) { ++unrelatedCalls; }
static void UI_CalcPostGameStats(void) { ++unrelatedCalls; }
static void UI_Cache_f(void) { ++unrelatedCalls; }
static void UI_Campaign_f(void) { ++unrelatedCalls; }
static void UI_ListCampaigns_f(void) { ++unrelatedCalls; }
/* PRODUCTION_CONSOLE_COMMAND */
static void resetConsole(void) {
    memset(&uiInfo, 0, sizeof(uiInfo));
    menuExists = 1; menuActive = closeCalls = openCalls = keyCalls = unrelatedCalls = 0;
    catcher = 1; clientState = CA_ACTIVE; consolePrint[0] = 0;
}
static void restartUI(void) {
    int i;
    /* Cvars belong to the engine and survive a UI VM unload/reload. */
    for (i = 0; i < cvarCount; ++i) if (cvars[i].latched[0]) {
        strcpy(cvars[i].value, cvars[i].latched); cvars[i].latched[0] = 0;
    }
    resetConsole();
    dispatch("tceGraphicsGet");
}
static void protectedState(void) {
    CHECK(!strcmp(get("r_gamma"), "1.3")); CHECK(!strcmp(get("r_mode"), "-1"));
    CHECK(!strcmp(get("r_customwidth"), "1280")); CHECK(!strcmp(get("r_customheight"), "720"));
    CHECK(!strcmp(get("r_fullscreen"), "0")); CHECK(!strcmp(get("r_mapoverbrightbits"), "0"));
    CHECK(!strcmp(get("cg_shadows"), "1")); CHECK(!strcmp(get("bind_mouse1"), "+attack"));
    CHECK(!strcmp(get("r_maxpolyverts"), "16384")); CHECK(!strcmp(get("r_maxpolys"), "4096"));
    CHECK(!unexpectedWrites); CHECK(!unsupportedCreates);
}
static void setup(int legacy, int stale) {
    int i; const char *names[] = { "r_gamma", "r_mode", "r_customwidth", "r_customheight", "r_fullscreen",
        "r_mapoverbrightbits", "cg_shadows", "bind_mouse1", "r_maxpolyverts", "r_maxpolys" };
    const char *values[] = { "1.3", "-1", "1280", "720", "0", "0", "1", "+attack", "16384", "4096" };
    memset(cvars, 0, sizeof(cvars)); cvarCount = engineWrites = unexpectedWrites = unsupportedCreates = commandCount = 0;
    /* ETLegacy deliberately spoofs version for old mod compatibility. */
    add("version", "ET 2.60b win-x86 May  8 2006", 0, 1);
    if (legacy) add("etVersion", "ET Legacy v2.86.0 win-x86 Sep 17 2026", 0, 1);
    add("r_picmip", "1", 1, 1); add("r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", 0, 1);
    add("r_ext_texture_filter_anisotropic", "0", 1, 1);
    if (legacy) { add("r_ext_max_anisotropy", "2", 1, 1); add("r_ext_multisample", "0", 1, 1); add("r_fbo", "1", 1, 1); }
    if (!legacy || stale) add("r_textureAnisotropy", "1.0", 0, 1);
    add("r_texturebits", "32", 1, 1); add("r_ext_compressed_textures", "1", 1, 1);
    add("r_subdivisions", "12", 1, 1); add("r_lodbias", "0", 0, 1);
    add("r_detailtextures", "0", 1, 1); add("r_simpleMipMaps", "1", 1, 1);
    for (i = 0; i < ARRAY_LEN(names); ++i) add(names[i], values[i], 0, 1);
    strcpy(currentCase, legacy ? (stale ? "ETL with original-config debris" : "ETL") : "original ET");
}
static void testStageCancel(int legacy) {
    int before;
    setup(legacy, 1); dispatch("tceGraphicsGet");
    CHECK(!strcmp(get("ui_tceGfx_hasAA"), legacy ? "1" : "0"));
    CHECK(!strcmp(get("ui_tceGfx_texture"), "1")); CHECK(!strcmp(get("ui_tceGfx_filter"), "GL_LINEAR_MIPMAP_NEAREST"));
    before = engineWrites; dispatch("tceGraphicsHigh");
    CHECK(!strcmp(get("ui_tceGfx_texture"), "0")); CHECK(!strcmp(get("ui_tceGfx_filter"), "GL_LINEAR_MIPMAP_LINEAR"));
    CHECK(!strcmp(get("ui_tceGfx_af"), "16")); CHECK(engineWrites == before); CHECK(commandCount == 0);
    CHECK(!strcmp(get("r_picmip"), "1"));
    /* Cancel closes the menu without Apply; reopen must discard the staged preset. */
    dispatch("tceGraphicsGet"); CHECK(!strcmp(get("ui_tceGfx_texture"), "1"));
    CHECK(!strcmp(get("ui_tceGfx_filter"), "GL_LINEAR_MIPMAP_NEAREST"));
    CHECK(engineWrites == before); CHECK(commandCount == 0); protectedState();
}
static void testApplyRestore(int legacy, int ultra, int stale) {
    fakeCvar before[96]; int count, i, writes, commands;
    setup(legacy, stale); memcpy(before, cvars, sizeof(cvars)); count = cvarCount;
    dispatch("tceGraphicsGet"); dispatch(ultra ? "tceGraphicsUltra" : "tceGraphicsHigh");
    dispatch("tceGraphicsApply"); CHECK(commandCount == 1);
    /* Active latched textures still use old values until restart. */
    CHECK(!strcmp(get("r_picmip"), "1")); CHECK(!strcmp(find("r_picmip")->latched, "0"));
    CHECK(!strcmp(get("ui_tceGfx_canRestore"), "1")); restartUI();
    CHECK(!strcmp(get("r_picmip"), "0")); CHECK(!strcmp(get("r_textureMode"), "GL_LINEAR_MIPMAP_LINEAR"));
    CHECK(!strcmp(get("r_ext_texture_filter_anisotropic"), legacy ? "16" : "1"));
    if (legacy) CHECK(!strcmp(get("r_ext_multisample"), ultra ? "16" : "8"));
    else { CHECK(!find("r_ext_multisample")); CHECK(!find("r_fbo")); CHECK(!strcmp(get("r_textureAnisotropy"), "16")); }
    CHECK(!strcmp(get("r_subdivisions"), "4")); CHECK(!strcmp(get("r_ext_compressed_textures"), "0"));
    CHECK(!strcmp(get("r_simpleMipMaps"), "0")); CHECK(!strcmp(get("r_detailtextures"), "1"));
    protectedState(); dispatch("tceGraphicsRestore"); CHECK(commandCount == 2); restartUI();
    for (i = 0; i < count; ++i) CHECK(!strcmp(get(before[i].name), before[i].value));
    CHECK(!strcmp(get("ui_tceGfx_canRestore"), "0"));
    writes = engineWrites; commands = commandCount; dispatch("tceGraphicsRestore");
    CHECK(engineWrites == writes); CHECK(commandCount == commands); protectedState();
}
static void testOneUndo(void) {
    setup(1, 1); dispatch("tceGraphicsGet"); dispatch("tceGraphicsHigh"); dispatch("tceGraphicsApply"); restartUI();
    trap_Cvar_Set("ui_tceGfx_aa", "4"); trap_Cvar_Set("ui_tceGfx_texture", "2");
    dispatch("tceGraphicsApply"); restartUI(); CHECK(!strcmp(get("r_picmip"), "2")); CHECK(!strcmp(get("r_ext_multisample"), "4"));
    dispatch("tceGraphicsRestore"); restartUI(); CHECK(!strcmp(get("r_picmip"), "0")); CHECK(!strcmp(get("r_ext_multisample"), "8"));
    protectedState();
}
static void testOriginalReadAndOff(void) {
    setup(0, 0); strcpy(currentCase, "original ET enabled AF getter");
    strcpy(find("r_ext_texture_filter_anisotropic")->value, "1");
    strcpy(find("r_textureAnisotropy")->value, "8");
    dispatch("tceGraphicsGet"); CHECK(!strcmp(get("ui_tceGfx_af"), "8"));
    trap_Cvar_Set("ui_tceGfx_af", "0"); dispatch("tceGraphicsApply"); restartUI();
    CHECK(!strcmp(get("r_ext_texture_filter_anisotropic"), "0"));
    CHECK(!strcmp(get("ui_tceGfx_af"), "0")); protectedState();
    dispatch("tceGraphicsRestore"); restartUI();
    CHECK(!strcmp(get("r_ext_texture_filter_anisotropic"), "1"));
    CHECK(!strcmp(get("r_textureAnisotropy"), "8")); CHECK(!strcmp(get("ui_tceGfx_af"), "8"));
    /* Legacy engine identity wins over a spoofed old version and stale AF degree. */
    setup(1, 1); strcpy(currentCase, "ETL spoofed version + stale degree");
    strcpy(find("r_ext_texture_filter_anisotropic")->value, "4");
    strcpy(find("r_textureAnisotropy")->value, "16");
    dispatch("tceGraphicsGet"); CHECK(!strcmp(get("ui_tceGfx_af"), "4"));
    CHECK(engineWrites == 0); protectedState();
}
static void testConsoleCommand(void) {
    int state, writes, commands;
    for (state = 0; state < 2; ++state) {
        setup(1, 1); resetConsole(); strcpy(currentCase, "actual UI_ConsoleCommand");
        clientState = state ? CA_ACTIVE : CA_DISCONNECTED;
        uiInfo.uiDC.realTime = 100; strcpy(consoleCommand, "TcE_GrApHiCs");
        CHECK(UI_ConsoleCommand(150) == qtrue);
        CHECK(uiInfo.uiDC.realTime == 150); CHECK(uiInfo.uiDC.frameTime == 50);
        CHECK(closeCalls == 1 && openCalls == 1 && keyCalls == 1 && catcher == KEYCATCH_UI);
        CHECK(menuActive == 1 && !consolePrint[0] && !unrelatedCalls);
        CHECK(!strcmp(get("ui_tceGfx_texture"), "1")); CHECK(!engineWrites && !commandCount);
        /* Unknown similarly named commands remain available to the engine. */
        strcpy(consoleCommand, "tce_graphics_extra");
        CHECK(UI_ConsoleCommand(175) == qfalse); CHECK(closeCalls == 1 && openCalls == 1 && keyCalls == 1);
        CHECK(menuActive == 1 && !consolePrint[0] && !unrelatedCalls);
        protectedState();
        /* Missing assets report a handled error without stealing key input. */
        resetConsole(); menuExists = 0; strcpy(consoleCommand, "tce_graphics");
        CHECK(UI_ConsoleCommand(200) == qtrue);
        CHECK(closeCalls == 1 && openCalls == 1 && !keyCalls && catcher == 1);
        CHECK(!menuActive && strstr(consolePrint, "matching TCE2 client package") != NULL);
        CHECK(!engineWrites && !commandCount && !unrelatedCalls); protectedState();
    }
    /* No UI static state is required to reopen, or to restore after vid_restart. */
    setup(1, 1); resetConsole(); strcpy(consoleCommand, "tce_graphics");
    CHECK(UI_ConsoleCommand(200)); dispatch("tceGraphicsHigh"); dispatch("tceGraphicsApply"); restartUI();
    writes = engineWrites; commands = commandCount; strcpy(consoleCommand, "tce_graphics");
    CHECK(UI_ConsoleCommand(300)); CHECK(menuActive && catcher == KEYCATCH_UI);
    CHECK(!strcmp(get("ui_tceGfx_texture"), "0")); CHECK(!strcmp(get("ui_tceGfx_canRestore"), "1"));
    CHECK(engineWrites == writes && commandCount == commands);
    dispatch("tceGraphicsRestore"); restartUI(); strcpy(consoleCommand, "tce_graphics");
    CHECK(UI_ConsoleCommand(400)); CHECK(!strcmp(get("ui_tceGfx_texture"), "1"));
    CHECK(!strcmp(get("ui_tceGfx_canRestore"), "0")); protectedState();
}
int main(void) {
    int legacy, ultra, stale;
    for (legacy = 0; legacy <= 1; ++legacy) {
        testStageCancel(legacy);
        for (ultra = 0; ultra <= 1; ++ultra) for (stale = 0; stale <= 1; ++stale) testApplyRestore(legacy, ultra, stale);
    }
    testOneUndo(); testOriginalReadAndOff();
    testConsoleCommand();
    printf("Graphics quality regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
