/* Both parsers receive the same token stream. The recovered parser uses the
 * original DLL's PC_* primitive helpers here; module builds use SDK PC_*.
 * This isolates field mapping, control flow, errors and resource ownership.
 */
#include "../game/q_shared.h"
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include "../cgame/tce_weapon_media.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <setjmp.h>
static jmp_buf parserAbort;

static char *input, *cursor;
static char pendingNumber[MAX_TOKEN_CHARS];
static int freed, loadCount;
static const char *availableSource;
static int weaponSourcePolicy;
static int registrationPolicy;
static const char *fixtureDirectory;
static char fileContents[65536];
static char loadNames[128][128];
static qboolean (__cdecl *pcVec)(int, vec3_t *);
static int (__cdecl *originalGear)(const char *, const char *, tce_gearDef_t *);
static int testGearParser(const char *directory);
static void testGearLoader(unsigned char *base, int server);
static void testMediaParsers(unsigned char *base, const char *directory);
static void testWeaponConfig(unsigned char *base, const char *directory);
static void testWeaponFiles(unsigned char *base,const char *directory);
static void testClientRegistration(unsigned char *base);
static void testCharacterParser(unsigned char *base, const char *directory, int server);
static void testAnimationNames(unsigned char *base);
static void testServerWeaponLoader(unsigned char *base);
static void testServerLoadPipeline(unsigned char *base, const char *directory);
void G_LoadGearDef(void);
void G_LoadWeaponDef(void);
void setServerLoaderContext(int mode, const char *map);
cgs_t cgs;
/* Current registration reads the replicated engine configstring before parsing.
 * This fixture supplies the same zero-initialized gameState as its reference. */
const char *CG_ConfigString(int index) {
    if(index<0 || index>=MAX_CONFIGSTRINGS)exit(2);
    return cgs.gameState.stringData+cgs.gameState.stringOffsets[index];
}
/* Use the production catalog for the complete weapon-name/hash comparison. */
gitem_t bg_itemlist[]={
#include "../game/tce_item_catalog.inc"
};
static char loaderWarning[32768];
void captureLoaderWarning(const char *message) {
    Q_strcat(loaderWarning, sizeof(loaderWarning), message);
}
void QDECL CG_Printf(const char *format, ...) {
    va_list args; va_start(args, format);
    char message[2048];
    Q_vsnprintf(message, sizeof(message), format, args);
    captureLoaderWarning(message);
    va_end(args);
}
static qboolean (__cdecl *pcInt)(int, int *);
static qboolean (__cdecl *pcFloat)(int, float *);
static qboolean (__cdecl *pcString)(int, char *, size_t);
static int (__cdecl *originalParse)(const char *, tce_weaponDef_t *);
void compareWeaponInit(const tce_weaponDef_t *input);

int trap_PC_LoadSource(const char *filename) {
    if (loadCount < 128) Q_strncpyz(loadNames[loadCount], filename, 128);
    ++loadCount;
    cursor = input;
    pendingNumber[0] = 0;
    if (fixtureDirectory) {
        char path[MAX_PATH]; FILE *file; size_t size;
        const char *basename = strrchr(filename, '/');
        sprintf(path, "%s/%s", fixtureDirectory, basename ? basename + 1 : filename);
        file = fopen(path, "rb");
        if (!file) return 0;
        size = fread(fileContents, 1, sizeof(fileContents) - 1, file);
        if (ferror(file) || !feof(file)) { fclose(file); fprintf(stderr, "Fixture read failed\n"); exit(1); }
        fclose(file); fileContents[size] = 0; cursor = fileContents;
        return 1;
    }
    if(registrationPolicy) {
        int custom=strstr(filename,"custom/default/")!=filename;
        int media=strstr(filename,".weap")!=NULL;
        if(media) {
            if((registrationPolicy==2 && custom) || registrationPolicy==3)return 0;
            cursor=registrationPolicy==4 && custom ? "weaponDef { client { standModel partial unknown } }" : "weaponDef { client { weaponIcon icon flashSound shot firstPerson { model gun } } }";
        } else {
            if(registrationPolicy==5)return 0;
            cursor=registrationPolicy==6 ? "weaponDef { scoped invalid }" : registrationPolicy==7 ? "weaponDef { weapClass invalid }" : "weaponDef { weapClass CAR caliberClass 556x45 maxclip 30 fullauto 1 }";
        }
        return 1;
    }
    if (weaponSourcePolicy) {
        int isDefault = strstr(filename, "custom/default/") == filename;
        if (weaponSourcePolicy == 4 && !isDefault) cursor = "weaponDef { maxclip 17 scoped wrong }";
        return weaponSourcePolicy == 1 ? !isDefault : weaponSourcePolicy == 2 ? isDefault : weaponSourcePolicy == 3 ? 0 : 1;
    }
    return availableSource ? !strcmp(filename, availableSource) : !!strcmp(filename, "missing");
}
int trap_PC_FreeSource(int handle) { (void)handle; ++freed; return 1; }
int trap_PC_SourceFileAndLine(int handle, char *filename, int *line) {
    (void)handle; strcpy(filename, "fixture.specs"); *line = 1; return 1;
}
int trap_PC_ReadToken(int handle, pc_token_t *token) {
    char *word, *end;
    double number;
    (void)handle;
    memset(token, 0, sizeof(*token));
    if (pendingNumber[0]) {
        strcpy(token->string, pendingNumber);
        pendingNumber[0] = 0;
    } else {
        if (!cursor) return 0;
        word = COM_ParseExt(&cursor, qtrue);
        if (!cursor && !word[0]) return 0;
        Q_strncpyz(token->string, word, sizeof(token->string));
        if (token->string[0] == '-' && token->string[1]) {
            strcpy(pendingNumber, token->string + 1);
            strcpy(token->string, "-");
        }
    }
    number = strtod(token->string, &end);
    if (token->string[0] && !*end) {
        token->type = TT_NUMBER;
        token->intvalue = (int)number;
        token->floatvalue = (float)number;
    } else token->type = TT_NAME;
    return 1;
}
qboolean PC_Vec_Parse(int h, vec3_t *v) { return pcVec(h, v); }
qboolean PC_Int_Parse(int h, int *v) { return pcInt(h, v); }
qboolean PC_Float_Parse(int h, float *v) { return pcFloat(h, v); }
qboolean PC_String_ParseNoAlloc(int h, char *v, size_t n) { return pcString(h, v, n); }

static int captureMediaDiagnostics, diagnosticBudget;
int captureSharedDiagnostic(const char *message) {
    if(!captureMediaDiagnostics)return 0;captureLoaderWarning(message);return 1;
}
static const char *fsInput;
static int fsLength, fsOpens, fsReads, fsCloses;
static const char *mediaFilesDirectory;
static char mediaConfigBuffer[20000];
int trap_FS_FOpenFile(const char *filename, fileHandle_t *handle, fsMode_t mode) {
    (void)filename;
    if (mode != FS_READ) { fprintf(stderr, "Unexpected FS mode\n"); exit(1); }
    if(mediaFilesDirectory) {
        char path[MAX_PATH]; FILE *file;
        sprintf(path,"%s/%s",mediaFilesDirectory,filename);file=fopen(path,"rb");fsLength=-1;
        if(file) {fseek(file,0,SEEK_END);fsLength=ftell(file);rewind(file);
            if(fsLength<sizeof(mediaConfigBuffer))fread(mediaConfigBuffer,1,fsLength,file);
            fclose(file);fsInput=mediaConfigBuffer;
        }
    }
    ++fsOpens; *handle = fsLength < 0 ? 0 : 7; return fsLength;
}
void trap_FS_Read(void *buffer, int length, fileHandle_t handle) {
    if (handle != 7 || length != fsLength) { fprintf(stderr, "Unexpected FS read\n"); exit(1); }
    ++fsReads; memcpy(buffer, fsInput, length);
}
void trap_FS_FCloseFile(fileHandle_t handle) {
    if (handle != 7) { fprintf(stderr, "Unexpected FS close\n"); exit(1); }
    ++fsCloses;
}
static char rendererCalls[16384];
static int renderCallCount, failModel, failSkin, failShader, failSound;
static int registerTestMedia(int command, const char *filename) {
    char call[256];
    sprintf(call, "%d:%s\n", command, filename);
    Q_strcat(rendererCalls, sizeof(rendererCalls), call);
    ++renderCallCount;
    if ((command == 0x35 && failModel) || (command == 0x36 && failSkin) || (command == 0x37 && failShader) || (command == 0x2d && failSound)) return 0;
    return command * 1000 + renderCallCount;
}
qhandle_t trap_R_RegisterModel(const char *filename) { return registerTestMedia(0x35, filename); }
qhandle_t trap_R_RegisterSkin(const char *filename) { return registerTestMedia(0x36, filename); }
qhandle_t trap_R_RegisterShader(const char *filename) { return registerTestMedia(0x37, filename); }

sfxHandle_t trap_S_RegisterSound(const char *name,qboolean compressed) {
    if(compressed)exit(1);return registerTestMedia(0x2d,name);
}

static int QDECL parserSyscall(int command, ...) {
    int handle, result = 0;
    va_list args;
    va_start(args, command);
    switch(command) {
    case 0: {
        const char *message = va_arg(args, char *);
        if (diagnosticBudget && --diagnosticBudget == 0) { va_end(args); longjmp(parserAbort, 1); }
        if (captureMediaDiagnostics || strstr(message, "WARNING:")) captureLoaderWarning(message);
        break;
    }
    case 0xb: {
        const char *filename = va_arg(args, char *);
        fileHandle_t *file = va_arg(args, fileHandle_t *);
        result = trap_FS_FOpenFile(filename, file, va_arg(args, int)); break;
    }
    case 0xc: {
        void *buffer = va_arg(args, void *); int length = va_arg(args, int);
        trap_FS_Read(buffer, length, va_arg(args, int)); break;
    }
    case 0xe: trap_FS_FCloseFile(va_arg(args, int)); break;
    case 0x2d: {
        const char *name=va_arg(args,char *);result=trap_S_RegisterSound(name,va_arg(args,int));break;
    }
    case 0x35: case 0x36: case 0x37:
        result = registerTestMedia(command, va_arg(args, char *)); break;
    case 2: result = 1000; break; /* deterministic engine milliseconds */
    case 1:
        captureLoaderWarning(va_arg(args, char *));
        va_end(args);
        longjmp(parserAbort, 1);
        break;
    case 0x243: case 0x5f: result = trap_PC_LoadSource(va_arg(args, char *)); break;
    case 0x244: case 0x60: result = trap_PC_FreeSource(va_arg(args, int)); break;
    case 0x245: case 0x61:
        handle = va_arg(args, int);
        result = trap_PC_ReadToken(handle, va_arg(args, pc_token_t *)); break;
    case 0x246: case 0x62: {
        char *filename; int *line;
        handle = va_arg(args, int); filename = va_arg(args, char *); line = va_arg(args, int *);
        result = trap_PC_SourceFileAndLine(handle, filename, line); break;
    }
    default: fprintf(stderr, "Unexpected parser syscall %d\n", command); exit(1);
    }
    va_end(args);
    return result;
}

static void compareParser(char *text, const char *filename) {
    static tce_weaponDef_t a, b; /* Survive setjmp/longjmp error callbacks. */
    int resultA, resultB, freedA;
    memset(&a, 0x5a, sizeof(a)); b = a;
    input = text; freed = 0;
    resultA = setjmp(parserAbort) ? -1 : BG_ParseWeaponDef(filename, &a); freedA = freed;
    freed = 0;
    resultB = setjmp(parserAbort) ? -1 : originalParse(filename, &b);
    if (resultA != resultB || freedA != freed || memcmp(&a, &b, sizeof(a))) {
        fprintf(stderr, "Parser differs from original: %s\n", filename); exit(1);
    }
}

int testWeaponParser(unsigned char *base, const char *directory, int server) {
    void (__cdecl *entry)(int (QDECL *)(int,...));
    const char *cases[] = {
        "", "other { }", "weaponDef", "weaponDef x", "weaponDef { }",
        "weaponDef {", "weaponDef { maxclip", "weaponDef { maxclip wrong }",
        "weaponDef { scoped wrong }", "weaponDef { weapClass",
        "weaponDef { unknown 1 }", "weaponDef { maxclip - 12 }",
        "weaponDef { scoped - 2.5 }", "weaponDef { MAXCLIP 7 maxclip 9 }",
        "weaponDef { weapClass AR caliberClass 556x45 maxclip 30 semiauto 1 fullauto 1 burst 3 pump 0 bolt 0 singleReload 0 scoped 1.5 scopeReticleType 2 useScopeReticleShader 1 nightVision 1 longWaveIR 0 suppressed 1 subsonic 1 jamPercentage 0.01 grenadeTimer 3000 noTacMode 0 usesWolfAnim 1 usesPistolAnimMod 1 usesRecoilAnimMod 1 }"
    };
    char path[MAX_PATH], pattern[MAX_PATH];
    WIN32_FIND_DATAA found;
    HANDLE search;
    int i, count = 0;
    pcVec = (void *)(base + (server ? 0x2e5c0 : 0x60a0)); originalGear = (void *)(base + (server ? 0x2fe30 : 0x7b90));
    pcInt = (void *)(base + (server ? 0x2e600 : 0x60e0)); pcFloat = (void *)(base + (server ? 0x2e510 : 0x5fb0));
    pcString = (void *)(base + (server ? 0x2e6f0 : 0x61d0)); originalParse = (void *)(base + (server ? 0x2f630 : 0x7390));
    entry = (void *)GetProcAddress((HMODULE)base, "dllEntry");
    if (!entry) exit(1);
    entry(parserSyscall);
    for (i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        compareParser((char *)cases[i], "fixture"); ++count;
    }
    compareParser("", "missing"); ++count;
    sprintf(pattern, "%s/*.specs", directory);
    search = FindFirstFileA(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) { fprintf(stderr, "No weapon fixtures\n"); exit(1); }
    do {
        FILE *file; long size; char *text;
        sprintf(path, "%s/%s", directory, found.cFileName);
        file = fopen(path, "rb"); if (!file) exit(1);
        fseek(file,0,SEEK_END); size = ftell(file); rewind(file);
        text = malloc(size + 1); if (!text) exit(1);
        if (fread(text,1,size,file) != size) exit(1);
        text[size] = 0; fclose(file);
        compareParser(text, found.cFileName); ++count;
        {
            tce_weaponDef_t weapon;
            memset(&weapon, 0, sizeof(weapon)); weapon.startClips = -1;
            input = text;
            if (!BG_ParseWeaponDef(found.cFileName, &weapon)) exit(1);
            compareWeaponInit(&weapon);
        }
        free(text);
    } while (FindNextFileA(search, &found));
    FindClose(search);
    printf("%d parser cases matched return values, all 460 output bytes and source releases.\n", count);
    printf("38 original weapon specifications also passed parser-to-initializer comparison.\n");
    count += testGearParser(directory);
    testGearLoader(base, server);
    if (server) { testServerWeaponLoader(base); testServerLoadPipeline(base, directory); testCharacterParser(base, directory, 1); testAnimationNames(base); }
    else { testMediaParsers(base, directory); testWeaponConfig(base, directory); testWeaponFiles(base,directory); testClientRegistration(base); testCharacterParser(base,directory,0); }
    return count;
}

static void compareGear(char *text, const char *available) {
    static tce_gearDef_t a, b; /* Survive setjmp/longjmp error callbacks. */
    int resultA, resultB, freedA, loadsA;
    char namesA[3][128];
    memset(&a, 0x5a, sizeof(a)); b = a;
    input = text; availableSource = available;
    freed = loadCount = 0; memset(loadNames, 0, sizeof(loadNames));
    resultA = setjmp(parserAbort) ? -1 : BG_ParseGearDef("primary", "fallback", &a);
    freedA = freed; loadsA = loadCount; memcpy(namesA, loadNames, sizeof(namesA));
    freed = loadCount = 0; memset(loadNames, 0, sizeof(loadNames));
    resultB = setjmp(parserAbort) ? -1 : originalGear("primary", "fallback", &b);
    if (resultA != resultB || freedA != freed || loadsA != loadCount ||
        memcmp(namesA, loadNames, sizeof(namesA)) || memcmp(&a, &b, sizeof(a))) {
        fprintf(stderr, "Gear parser differs: source=%s text=%s\n", available, text); exit(1);
    }
    availableSource = NULL;
}

static int testGearParser(const char *directory) {
    const char *cases[] = {
        "", "wrong { }", "gear_manager", "gear_manager x", "gear_manager { }",
        "gear_manager {", "gear_manager { unknown 1 }", "gear_manager { gm_weapon",
        "gear_manager { gm_weapon x", "gear_manager { gm_weapon {",
        "gear_manager { gm_weapon { ignored 99 } }",
        "gear_manager { gm_weapon { weaponID TH1 team terrorists slot secondary requiredSkill 1.9 - 2.9 3.1 startClips 4 weaponFile foo equivalentWeaponID SH1 } gm_weapon { startClips 9 } }",
        "gear_manager { gm_weapon { weaponID unknown team specops slot primary } gm_weapon { weaponID SH1 team other slot other } }",
        "GEAR_MANAGER { gm_playerSkinGroup skin gm_playerVoiceGroup voice gm_playerIconGroup icons gm_custom_weaponGroup weapons gm_mapoverbrightbits - 2 }"
    };
    const char *outer[] = { "gm_playerSkinGroup", "gm_playerVoiceGroup", "gm_playerIconGroup", "gm_custom_weaponGroup", "gm_mapoverbrightbits" };
    const char *inner[] = { "weaponID", "weaponFile", "team", "slot", "requiredSkill", "startClips", "equivalentWeaponID" };
    const char *sources[] = { "primary", "fallback", "maps/default.gear", "none" };
    char buffer[512], pattern[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAA found; HANDLE search;
    int i, j, count = 0, fixtures = 0;
    for (i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i)
        for (j = 0; j < 4; ++j) { compareGear((char *)cases[i], sources[j]); ++count; }
    for (i = 0; i < sizeof(outer)/sizeof(outer[0]); ++i) {
        sprintf(buffer, "gear_manager { %s", outer[i]); compareGear(buffer, "primary"); ++count;
    }
    for (i = 0; i < sizeof(inner)/sizeof(inner[0]); ++i) {
        sprintf(buffer, "gear_manager { gm_weapon { %s", inner[i]); compareGear(buffer, "primary"); ++count;
    }
    compareGear("gear_manager { gm_weapon { requiredSkill 1 wrong 3 } }", "primary"); ++count;
    compareGear("gear_manager { gm_weapon { startClips wrong } }", "primary"); ++count;
    compareGear("gear_manager { gm_mapoverbrightbits wrong }", "primary"); ++count;
    sprintf(pattern, "%s/*.gear", directory);
    search = FindFirstFileA(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) { fprintf(stderr, "No gear fixtures\n"); exit(1); }
    do {
        FILE *file; long size; char *text;
        sprintf(path, "%s/%s", directory, found.cFileName);
        file = fopen(path, "rb"); if (!file) exit(1);
        fseek(file, 0, SEEK_END); size = ftell(file); rewind(file);
        text = malloc(size + 1); if (!text) exit(1);
        if (fread(text, 1, size, file) != size) exit(1);
        text[size] = 0; fclose(file);
        for (j = 0; j < 4; ++j) { compareGear(text, sources[j]); ++count; }
        ++fixtures; free(text);
    } while (FindNextFileA(search, &found));
    FindClose(search);
    printf("%d gear parser cases (%d original fixtures) matched all 6152 bytes, return values, source fallback and releases.\n", count, fixtures);
    return count;
}

static void testGearLoader(unsigned char *base, int server) {
    void (__cdecl *original)(void) = (void *)(base + (server ? 0xa01a0 : 0x6d120));
    tce_gearDef_t *originalDef = (void *)(base + (server ? 0x23cf5a0 : 0x4ba9f00));
    const int modes[] = {2, 5, 7};
    const char *maps[] = {"obj_snow", "", "a_map_with_a_long_name"};
    const char *texts[] = {"gear_manager { gm_custom_weaponGroup default gm_mapoverbrightbits 2 }", "wrong { }"};
    char primary[128], fallback[128], savedNames[3][128], savedWarning[32768];
    int i, j, k, cached, source, savedLoads, savedFreed, count = 0;
    for (i = 0; i < 3; ++i) for (j = 0; j < 3; ++j)
    for (k = 0; k < 2; ++k) for (cached = 0; cached < 2; ++cached)
    for (source = 0; source < 4; ++source) {
        sprintf(primary, "maps/%s_gt%d.gear", maps[j], modes[i]);
        sprintf(fallback, "maps/default_gt%d.gear", modes[i]);
        availableSource = source == 0 ? primary : source == 1 ? fallback : source == 2 ? "maps/default.gear" : "none";
        cgs.gametype = modes[i]; strcpy(cgs.rawmapname, maps[j]);
        *(int *)(base + (server ? 0x6d8d4c : 0x257e100)) = modes[i]; strcpy((char *)base + (server ? 0x7feb98 : 0x257e150), maps[j]);
        memset(&gearDef, 0x5a, sizeof(gearDef)); gearDef.parsed = cached;
        *originalDef = gearDef; input = (char *)texts[k];
        loadCount = freed = 0; memset(loadNames, 0, sizeof(loadNames)); loaderWarning[0] = 0;
        if (server) { setServerLoaderContext(modes[i], maps[j]); G_LoadGearDef(); }
        else CG_LoadGearDef();
        savedLoads = loadCount; savedFreed = freed;
        memcpy(savedNames, loadNames, sizeof(savedNames)); strcpy(savedWarning, loaderWarning);
        loadCount = freed = 0; memset(loadNames, 0, sizeof(loadNames)); loaderWarning[0] = 0;
        original();
        if (memcmp(&gearDef, originalDef, sizeof(gearDef)) || savedLoads != loadCount ||
            savedFreed != freed || memcmp(savedNames, loadNames, sizeof(savedNames)) || strcmp(savedWarning, loaderWarning)) {
            fprintf(stderr, "CG_LoadGearDef mismatch: mode=%d map=%s source=%d cached=%d\n", modes[i], maps[j], source, cached); exit(1);
        }
        ++count;
    }
    availableSource = NULL;
    printf("%d gear loader cases matched state, source paths, caching and warnings.\n", count);
}

static void testServerWeaponLoader(unsigned char *base) {
    void (__cdecl *original)(void) = (void *)(base + 0xa0270);
    tce_gearDef_t *originalGearDef = (void *)(base + 0x23cf5a0);
    tce_weaponDef_t *originalWeapons = (void *)(base + 0x24435c0);
    static char savedNames[128][128], savedWarnings[32768];
    int index, scenario, i, savedLoads, savedFreed, count = 0, abortedA, abortedB;
    for (index = 0; index <= 64; ++index) for (scenario = 0; scenario < 12; ++scenario) {
        memset(&gearDef, 0, sizeof(gearDef)); memset(weaponDef, 0, sizeof(weaponDef));
        strcpy(gearDef.customWeaponGroup, scenario == 11 ? "default" : "custom_group");
        for (i = 0; i < 64; ++i) {
            weaponDef[i].maxclip = 30; weaponDef[i].characteristic = i % 30;
            weaponDef[i].startClips = -1;
            if (i == index || index == 64) {
                if (scenario != 9) strcpy(gearDef.weaponFile[i], "rifle");
                gearDef.startClips[i] = i % 7 - 2;
                if (scenario == 8) weaponDef[i].parsed = 1;
            }
        }
        *originalGearDef = gearDef; memcpy(originalWeapons, weaponDef, sizeof(weaponDef));
        input = scenario == 5 ? "wrong { }" : scenario == 6 ? "weaponDef { maxclip 17 scoped wrong }" :
            scenario == 7 ? "weaponDef { }" : "weaponDef { weapClass CAR caliberClass 556x45 maxclip 30 fullauto 1 suppressed 1 }";
        weaponSourcePolicy = scenario < 4 ? scenario + 1 : scenario == 10 ? 3 : 4;
        availableSource = NULL; loadCount = freed = 0;
        memset(loadNames, 0, sizeof(loadNames)); loaderWarning[0] = 0;
        abortedA = setjmp(parserAbort);
        if (!abortedA) G_LoadWeaponDef(); savedLoads = loadCount; savedFreed = freed;
        memcpy(savedNames, loadNames, sizeof(savedNames)); strcpy(savedWarnings, loaderWarning);
        loadCount = freed = 0; memset(loadNames, 0, sizeof(loadNames)); loaderWarning[0] = 0;
        abortedB = setjmp(parserAbort);
        if (!abortedB) original();
        if (abortedA != abortedB || memcmp(weaponDef, originalWeapons, sizeof(weaponDef)) || memcmp(&gearDef, originalGearDef, sizeof(gearDef)) ||
            savedLoads != loadCount || savedFreed != freed || memcmp(savedNames, loadNames, sizeof(savedNames)) ||
            strcmp(savedWarnings, loaderWarning)) {
            fprintf(stderr, "G_LoadWeaponDef mismatch: index=%d scenario=%d loads=%d/%d frees=%d/%d\n", index, scenario, savedLoads, loadCount, savedFreed, freed);
            fprintf(stderr, "warnings actual=%s expected=%s\n", savedWarnings, loaderWarning); exit(1);
        }
        ++count;
    }
    weaponSourcePolicy = 0; availableSource = NULL;
    printf("%d G_LoadWeaponDef cases matched all 64 weapons, source order, warnings and releases.\n", count);
}

static void testServerLoadPipeline(unsigned char *base, const char *directory) {
    void (__cdecl *originalGearLoad)(void) = (void *)(base + 0xa01a0);
    void (__cdecl *originalWeaponLoad)(void) = (void *)(base + 0xa0270);
    tce_gearDef_t *originalGearDef = (void *)(base + 0x23cf5a0);
    tce_weaponDef_t *originalWeapons = (void *)(base + 0x24435c0);
    const int modes[] = {2, 5, 7};
    const char *maps[] = {"obj_snow", "missing_map"};
    static char savedNames[128][128], savedWarnings[32768];
    int mode, map, i, savedLoads, savedFreed, a, b, initialized, total = 0;
    fixtureDirectory = directory;
    for (mode = 0; mode < 3; ++mode) for (map = 0; map < 2; ++map) {
        memset(&gearDef, 0, sizeof(gearDef)); memset(weaponDef, 0, sizeof(weaponDef));
        memset(originalGearDef, 0, sizeof(*originalGearDef)); memset(originalWeapons, 0, sizeof(weaponDef));
        setServerLoaderContext(modes[mode], maps[map]);
        *(int *)(base + 0x6d8d4c) = modes[mode]; strcpy((char *)base + 0x7feb98, maps[map]);
        loadCount = freed = 0; memset(loadNames, 0, sizeof(loadNames)); loaderWarning[0] = 0;
        a = setjmp(parserAbort);
        if (!a) { G_LoadGearDef(); G_LoadWeaponDef(); }
        savedLoads = loadCount; savedFreed = freed; memcpy(savedNames, loadNames, sizeof(savedNames)); strcpy(savedWarnings, loaderWarning);
        loadCount = freed = 0; memset(loadNames, 0, sizeof(loadNames)); loaderWarning[0] = 0;
        b = setjmp(parserAbort);
        if (!b) { originalGearLoad(); originalWeaponLoad(); }
        if (a || b || memcmp(weaponDef, originalWeapons, sizeof(weaponDef)) || memcmp(&gearDef, originalGearDef, sizeof(gearDef)) ||
            savedLoads != loadCount || savedFreed != freed || memcmp(savedNames, loadNames, sizeof(savedNames)) || strcmp(savedWarnings, loaderWarning)) {
            fprintf(stderr, "Server asset pipeline mismatch: mode=%d map=%s\n", modes[mode], maps[map]); exit(1);
        }
        initialized = 0;
        for (i = 0; i < 64; ++i) if (gearDef.weaponFile[i][0]) {
            if (!weaponDef[i].parsed) { fprintf(stderr, "Missing real weapon specification: %s\n", gearDef.weaponFile[i]); exit(1); }
            ++initialized;
        }
        if (!gearDef.parsed || !initialized) { fprintf(stderr, "Empty gear pipeline\n"); exit(1); }
        total += initialized;
        /* Repeated loading must be a no-op once both layers are cached. */
        loadCount = 0; G_LoadGearDef(); G_LoadWeaponDef(); originalGearLoad(); originalWeaponLoad();
        if (loadCount) { fprintf(stderr, "Cached pipeline accessed files\n"); exit(1); }
    }
    fixtureDirectory = NULL;
    printf("6 real gear-to-weapon pipelines matched (%d initialized weapon slots), including repeat-load caching.\n", total);
}

static void compareMedia(unsigned char *base, int kind, int view, char *text) {
    static tce_weaponInfo_t actual, expected;
    static char savedCalls[16384], savedDiagnostics[32768];
    int a, b, released, calls;
    const unsigned int addresses[] = {0x6ee40, 0x6ed60, 0x6eac0, 0x6f1b0};
    int (__cdecl *original)(int, tce_weaponInfo_t *, int) = (void *)(base + addresses[kind]);
    memset(&actual, 0x5a, sizeof(actual)); expected = actual;
    input = text; trap_PC_LoadSource("fixture"); freed = 0;
    renderCallCount = 0; rendererCalls[0] = loaderWarning[0] = 0;
    if (kind == 0) a = TCE_CG_RW_ParseWeaponLinkPart(1, &actual, view);
    else if (kind == 1) a = TCE_CG_RW_ParseWeaponLink(1, &actual, view);
    else if (kind == 2) a = TCE_CG_RW_ParseViewType(1, &actual, view);
    else a = TCE_CG_RW_ParseModModel(1, &actual);
    released = freed; calls = renderCallCount;
    strcpy(savedCalls, rendererCalls); strcpy(savedDiagnostics, loaderWarning);
    trap_PC_LoadSource("fixture"); freed = 0;
    renderCallCount = 0; rendererCalls[0] = loaderWarning[0] = 0;
    b = original(1, &expected, view);
    if (a != b || released != freed || calls != renderCallCount || memcmp(&actual, &expected, sizeof(actual)) ||
        strcmp(savedCalls, rendererCalls) || strcmp(savedDiagnostics, loaderWarning)) {
        fprintf(stderr, "Media mismatch kind=%d view=%d text=%s\nactual=%soriginal=%s\n", kind, view, text, savedDiagnostics, loaderWarning); exit(1);
    }
}

static void testMediaParsers(unsigned char *base, const char *directory) {
    const char *partCases[] = {"", "wrong", "- 1 { }", "7 { }", "2147483647 { }", "0", "0 wrong", "0 {", "0 { }", "0 { tag", "0 { model", "0 { skin", "0 { axisSkin", "0 { alliedSkin", "0 { modFlags", "0 { modFlags wrong }", "0 { unknown }", "0 { tag foo model m skin s axisSkin a alliedSkin b RDF_PORTALSCOPE RDF_NOPORTALSCOPE RDF_TACVIEW RDF_NOTACVIEW modFlags - 3 }"};
    const char *viewCases[] = {"", "wrong", "{", "{ }", "{ model", "{ skin", "{ axisSkin", "{ alliedSkin", "{ flashModel", "{ weaponLink", "{ unknown }", "{ model m skin s axisSkin a alliedSkin b flashModel f weaponLink { part 0 { tag muzzle model p RDF_TACVIEW } part 6 { RDF_PORTALSCOPE modFlags 8 } } }"};
    const char *linkCases[] = {"", "wrong", "{", "{ }", "{ unknown }", "{ part", "{ part 0 { model a } part 0 { skin b } }", "{ part - 1 { } }", "{ part 7 { } }"};
    const char *modCases[] = {"", "wrong", "- 1 model", "6 model", "0", "0 model", "5 shader"};
    char text[1024]; int mode, view, i, index, count = 0;
    /* Disable the original renderer wrappers' loading-screen painting only.
     * Registration calls still run through the original engine syscall ABI. */
    *(int *)(base + 0x40352c4) = 1;
    captureMediaDiagnostics = 1;
    for (mode = 0; mode < 8; ++mode) {
        failModel = mode & 1; failSkin = mode & 2; failShader = mode & 4;
        for (view = 0; view < 3; ++view) {
            for (i = 0; i < sizeof(partCases)/sizeof(partCases[0]); ++i) { compareMedia(base, 0, view, (char *)partCases[i]); ++count; }
            for (i = 0; i < sizeof(viewCases)/sizeof(viewCases[0]); ++i) { compareMedia(base, 2, view, (char *)viewCases[i]); ++count; }
            for (i = 0; i < sizeof(linkCases)/sizeof(linkCases[0]); ++i) { compareMedia(base, 1, view, (char *)linkCases[i]); ++count; }
            for (index = 0; index < 7; ++index) {
                sprintf(text, "%d { TAG foo MODEL bar skin baz RDF_PORTALSCOPE RDF_NOPORTALSCOPE RDF_TACVIEW RDF_NOTACVIEW modFlags %d }", index, index * 13);
                compareMedia(base, 0, view, text); ++count;
            }
        }
        for (i = 0; i < sizeof(modCases)/sizeof(modCases[0]); ++i) { compareMedia(base, 3, 0, (char *)modCases[i]); ++count; }
        for (index = 0; index < 6; ++index) { sprintf(text, "%d model", index); compareMedia(base, 3, 0, text); ++count; }
    }
    {
        WIN32_FIND_DATAA found; HANDLE search;
        char pattern[MAX_PATH], path[MAX_PATH]; int fixtures = 0;
        sprintf(pattern, "%s/media_*", directory);
        search = FindFirstFileA(pattern, &found);
        if (search == INVALID_HANDLE_VALUE) { fprintf(stderr, "No media fixtures\n"); exit(1); }
        do {
            FILE *file; long size; char *contents; int kind;
            sprintf(path, "%s/%s", directory, found.cFileName);
            file = fopen(path, "rb"); if (!file) exit(1);
            fseek(file, 0, SEEK_END); size = ftell(file); rewind(file);
            contents = malloc(size + 1); if (!contents) exit(1);
            if (fread(contents, 1, size, file) != size) exit(1);
            contents[size] = 0; fclose(file);
            kind = strstr(found.cFileName, ".mod") ? 3 : 2;
            view = strstr(found.cFileName, ".view1") ? 1 : 0;
            for (mode = 0; mode < 2; ++mode) {
                failModel = mode; failSkin = failShader = 0;
                compareMedia(base, kind, view, contents); ++count;
            }
            ++fixtures; free(contents);
        } while (FindNextFileA(search, &found));
        FindClose(search);
        printf("%d original .weap view/mod fragments passed renderer success and model-fallback comparisons.\n", fixtures);
    }
    {
        int (__cdecl *originalError)(int, const char *, ...) = (void *)(base + 0x6d490);
        const int lengths[] = {0, 1, 100, 980, 1000, 1023, 1024, 4094, 4095, 5000};
        char message[5001], saved[32768]; int result, releases;
        for (i = 0; i < sizeof(lengths)/sizeof(lengths[0]); ++i) {
            memset(message, 'x', lengths[i]); message[lengths[i]] = 0;
            freed = 0; loaderWarning[0] = 0;
            result = TCE_CG_RW_ParseError(1, "%s/%d", message, 99);
            releases = freed; strcpy(saved, loaderWarning);
            freed = 0; loaderWarning[0] = 0;
            if (result != originalError(1, "%s/%d", message, 99) || releases != freed || strcmp(saved, loaderWarning)) {
                fprintf(stderr, "Media diagnostic mismatch length=%d\n", lengths[i]); exit(1);
            }
        }
        printf("10 media error-formatting and truncation comparisons passed.\n");
    }
    captureMediaDiagnostics = 0;
    printf("%d client media subtree cases matched all 4048 bytes, renderer calls, diagnostics and releases.\n", count);
}

static void compareConfig(unsigned char *base, const char *text, int length, int headerWouldHang) {
    int (__cdecl *original)(const char *, tce_weaponInfo_t *) = (void *)(base + 0x6e870);
    static tce_weaponInfo_t actual, expected;
    static char diagnostics[32768];
    int a, b, opened, read, closed;
    fsInput = text; fsLength = length;
    memset(&actual, 0x5a, sizeof(actual)); expected = actual;
    fsOpens = fsReads = fsCloses = 0; loaderWarning[0] = 0;
    a = TCE_CG_ParseWeaponConfig("fixture.cfg", &actual);
    opened = fsOpens; read = fsReads; closed = fsCloses; strcpy(diagnostics, loaderWarning);
    fsOpens = fsReads = fsCloses = 0; loaderWarning[0] = 0;
    diagnosticBudget = 32;
    b = setjmp(parserAbort) ? -1 : original("fixture.cfg", &expected);
    diagnosticBudget = 0;
    if (headerWouldHang) {
        if (a != 0 || b != -1 || memcmp(&actual, &expected, sizeof(actual))) {
            fprintf(stderr, "Malformed animation header guard differs from expectation\n"); exit(1);
        }
    } else if (a != b || memcmp(&actual, &expected, sizeof(actual)) || strcmp(diagnostics, loaderWarning)) {
        int i;
        fprintf(stderr, "Animation config mismatch length=%d return=%d/%d text=%.200s\nactual=%soriginal=%s\n", length, a, b, text, diagnostics, loaderWarning);
        for (i=0; i<sizeof(actual); ++i) if (((unsigned char *)&actual)[i] != ((unsigned char *)&expected)[i])
            fprintf(stderr, "offset %x: %02x/%02x\n", i, ((unsigned char *)&actual)[i], ((unsigned char *)&expected)[i]);
        exit(1);
    }
    if (opened != fsOpens || read != fsReads || closed != fsCloses) { fprintf(stderr, "Animation FS ownership mismatch\n"); exit(1); }
}

static void testWeaponConfig(unsigned char *base, const char *directory) {
    const char *fps[] = {"0", "1", "2", "3", "24", "25", "29.97", "30", "60", "0.3", "333.3333333333333", "-30", "nonsense"};
    const char *shortFiles[] = {"0", "0 1", "0 1 30", "newfmt 0", "newfmt 0 2 29.97 1 3", "junk newfmt 0 2 20 1 1 1 1", "NEWFMT 0 2 0 -1 2 -1 -1"};
    static char text[20001]; char row[256], path[MAX_PATH], pattern[MAX_PATH];
    WIN32_FIND_DATAA found; HANDLE search;
    int format, rate, loops, i, count = 0, fixtures = 0;
    captureMediaDiagnostics = 1;
    for (format=0; format<2; ++format) for (rate=0; rate<sizeof(fps)/sizeof(fps[0]); ++rate)
    for (loops=-1; loops<=3; ++loops) {
        strcpy(text, format ? "newfmt\n" : "");
        for (i=0; i<13; ++i) {
            sprintf(row, "%d %d %s %d", i*5, i-2, fps[rate], loops*5);
            strcat(text, row);
            if (format) { sprintf(row, " %d %d %d", i, i&1, i*3); strcat(text,row); }
            strcat(text, "\n");
        }
        compareConfig(base, text, strlen(text), 0); ++count;
    }
    for (i=0; i<sizeof(shortFiles)/sizeof(shortFiles[0]); ++i) { compareConfig(base, shortFiles[i], strlen(shortFiles[i]), 0); ++count; }
    compareConfig(base, "", -1, 0); compareConfig(base, "", 0, 0); count+=2;
    memset(text, ' ', sizeof(text)); text[0]='0'; text[20000]=0;
    compareConfig(base, text, 19998, 0); compareConfig(base, text, 19999, 0); compareConfig(base, text, 20000, 0); count+=3;
    compareConfig(base, " ", 1, 1); compareConfig(base, "newfmt", 6, 1); compareConfig(base, "garbage", 7, 1);
    sprintf(pattern, "%s/*.cfgtest", directory); search = FindFirstFileA(pattern,&found);
    if (search == INVALID_HANDLE_VALUE) { fprintf(stderr, "No animation fixtures\n"); exit(1); }
    do {
        FILE *file; long size; char *contents;
        sprintf(path, "%s/%s", directory, found.cFileName); file=fopen(path,"rb"); if(!file)exit(1);
        fseek(file,0,SEEK_END);size=ftell(file);rewind(file);contents=malloc(size+1);if(!contents)exit(1);
        if(fread(contents,1,size,file)!=size)exit(1);contents[size]=0;fclose(file);
        compareConfig(base,contents,size,0);++count;++fixtures;free(contents);
    } while(FindNextFileA(search,&found)); FindClose(search);
    captureMediaDiagnostics = 0;
    printf("%d animation config comparisons (%d real files) passed; 3 original header loops safely bounded.\n",count,fixtures);
}

static int characterParserOffset;
static void compareCharacter(unsigned char *base, char *text, const char *filename) {
    int (__cdecl *original)(const char *, bg_characterDef_t *) = (void *)(base + characterParserOffset);
    bg_characterDef_t a,b; int ra,rb,releases;
    /* Original G_RegisterCharacter zeroes336dwords (cache1884), not11fields only. */
    if (sizeof(a)!=0x540) { fprintf(stderr,"Character ABI mismatch\n");exit(1); }
    memset(&a,0x5a,sizeof(a));b=a;input=text;freed=0;
    ra=BG_ParseCharacterFile(filename,&a);releases=freed;freed=0;
    rb=original(filename,&b);
    if(ra!=rb||releases!=freed||memcmp(&a,&b,sizeof(a))) {
        fprintf(stderr,"Character parser mismatch: %s\n",text);exit(1);
    }
}
static void testCharacterParser(unsigned char *base,const char *directory,int server) {
    const char *keys[]={"mesh","animationGroup","animationScript","skin","skinGroup","skinRoot","undressedCorpseModel","undressedCorpseSkin","hudhead","hudheadskin","hudheadanims"};
    const char *cases[]={"","other","characterDef","characterDef wrong","characterDef {","characterDef { }","characterDef { unknown }","CHARACTERDEF { SKINGROUP first skinGroup second skinRoot root }"};
    char text[2048],path[MAX_PATH],pattern[MAX_PATH]; int i,count=0,fixtures=0;
    WIN32_FIND_DATAA found;HANDLE search;
    characterParserOffset=server?0x2a990:0x3050;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i){compareCharacter(base,(char *)cases[i],"fixture");++count;}
    compareCharacter(base,"","missing");++count;
    for(i=0;i<sizeof(keys)/sizeof(keys[0]);++i){
        sprintf(text,"characterDef { %s",keys[i]);compareCharacter(base,text,"fixture");++count;
        sprintf(text,"characterDef { %s abc }",keys[i]);compareCharacter(base,text,"fixture");++count;
    }
    sprintf(pattern,"%s/*.chartest",directory);search=FindFirstFileA(pattern,&found);
    if(search==INVALID_HANDLE_VALUE)exit(1);
    do {
        FILE *file;long size;char *contents;
        sprintf(path,"%s/%s",directory,found.cFileName);file=fopen(path,"rb");if(!file)exit(1);
        fseek(file,0,SEEK_END);size=ftell(file);rewind(file);contents=malloc(size+1);if(!contents)exit(1);
        if(fread(contents,1,size,file)!=size)exit(1);contents[size]=0;fclose(file);
        compareCharacter(base,contents,"fixture");++count;++fixtures;free(contents);
    }while(FindNextFileA(search,&found));FindClose(search);
    printf("%d %s character parser comparisons (%d original files) matched all 704 bytes and releases.\n",count,server?"server":"client",fixtures);
}

static void testAnimationNames(unsigned char *base) {
    extern animStringItem_t weaponStrings[65];
    void (__cdecl *original)(void) = (void *)(base + 0x28cb0);
    animStringItem_t *reference = (void *)(base + 0x2454920);
    int i, pass;
    for (pass=0; pass<2; ++pass) {
        /* Both originals initialize 64 slots; the source-only sentinel keeps
         * its static zero initialization, just as in a real module load. */
        memset(weaponStrings, 0x5a, 64 * sizeof(weaponStrings[0]));
        BG_InitWeaponStrings(); original();
        for (i=0; i<64; ++i) {
            if (strcmp(weaponStrings[i].string, reference[i].string) || weaponStrings[i].hash != reference[i].hash) {
                fprintf(stderr,"Weapon animation name/hash mismatch: %d\n",i);exit(1);
            }
        }
        if(weaponStrings[64].string || weaponStrings[64].hash){fprintf(stderr,"source sentinel unexpectedly modified, pass%d\n",pass);exit(1);}
    }
    {
        extern animStringItem_t animMoveTypesStr[];
        animStringItem_t *moves = (void *)(base + 0xb1d48);
        for (i=0; i<NUM_ANIM_MOVETYPES; ++i) {
            if (!moves[i].string || strcmp(moves[i].string,animMoveTypesStr[i].string)){fprintf(stderr,"movement name mismatch index%d\n",i);exit(1);}
        }
        if(moves[i].string || animMoveTypesStr[i].string || i!=28){fprintf(stderr,"movement name terminator mismatch index%d\n",i);exit(1);}
    }
    printf("128 weapon animation name/hash comparisons and 28 movement names passed.\n");
}

static void compareWeaponFile(unsigned char *base,char *text,int wrapper,int fill) {
    tce_weaponInfo_t a,b;int ra,rb,releaseA,callsA,opensA,readsA,closesA,loadsA,i;
    char calls[16384],diagnostics[32768];
    const unsigned int trails[]={0,0x22a30,0x6ca10,0x6c670,0x6e6c0};
    const unsigned int brass[]={0,0x6c0d0,0x6e480};
    int (__cdecl *originalClient)(int,tce_weaponInfo_t *)=(void *)(base+0x6d510);
    int (__cdecl *originalFile)(const char *,tce_weaponInfo_t *)=(void *)(base+0x6d350);
    memset(&a,fill,sizeof(a));b=a;
    input=cursor=text;pendingNumber[0]=0;freed=loadCount=renderCallCount=fsOpens=fsReads=fsCloses=0;
    rendererCalls[0]=loaderWarning[0]=0;
    ra=wrapper?TCE_CG_RegisterWeaponFromWeaponFile(wrapper==2?"missing":"fixture",&a):TCE_CG_RW_ParseClient(1,&a);
    releaseA=freed;callsA=renderCallCount;opensA=fsOpens;readsA=fsReads;closesA=fsCloses;loadsA=loadCount;
    strcpy(calls,rendererCalls);strcpy(diagnostics,loaderWarning);
    input=cursor=text;pendingNumber[0]=0;freed=loadCount=renderCallCount=fsOpens=fsReads=fsCloses=0;
    rendererCalls[0]=loaderWarning[0]=0;
    rb=wrapper?originalFile(wrapper==2?"missing":"fixture",&b):originalClient(1,&b);
    /* Relocated executable pointers become semantic IDs in the isolated media ABI.
     * Their implementation is deliberately not claimed by this parser test. */
    for(i=1;i<5;++i)if(b.missileTrail==(int)(base+trails[i]))b.missileTrail=i;
    for(i=1;i<3;++i)if(b.ejectBrass==(int)(base+brass[i]))b.ejectBrass=i;
    if(ra!=rb || releaseA!=freed || callsA!=renderCallCount || opensA!=fsOpens || readsA!=fsReads || closesA!=fsCloses || loadsA!=loadCount ||
       memcmp(&a,&b,sizeof(a)) || strcmp(calls,rendererCalls) || strcmp(diagnostics,loaderWarning)) {
        for(i=0;i<sizeof(a);++i)if(((unsigned char *)&a)[i]!=((unsigned char *)&b)[i]){fprintf(stderr,"First media byte mismatch: %x\n",i);break;}
        fprintf(stderr,"Weapon file mismatch wrapper=%d text=%s\nactual=%soriginal=%s\n",wrapper,text,diagnostics,loaderWarning);exit(1);
    }
}

static void testWeaponFiles(unsigned char *base,const char *directory) {
    const char *strings[]={"standModel","pickupModel","pickupSound","weaponConfig","handsModel","flashSound","flashEchoSound","lastShotSound","readySound","firingSound","overheatSound","reloadSound","reloadFastSound","spinupSound","spindownSound","switchSound","weaponIcon","weaponSelectedIcon","missileModel","missileAlliedSkin","missileAxisSkin","missileSound","missileTrailFunc","ejectBrassFunc","deployMenuShortName","deployMenuDescription","DeployMenuType"};
    const char *numbers[]={"missileDlight","foreShorten","tagsInMain","flashReverbVolume","portalScopeWidth"};
    const char *vectors[]={"flashDlightColor","missileDlightColor","gunViewOffset","gunViewAimOffset","gunViewAngles"};
    const char *invalid[]={"","wrong","{","{ }","{ unknown }","{ droppedAnglesHack droppedAnglesHack }","{ firstPerson wrong","{ thirdPerson { unknown }","{ modModel 6 model","{ firstPerson { weaponLink { part 7 { } } } }"};
    const char *outer[]={"","wrong","weaponDef","weaponDef wrong","weaponDef {","weaponDef { }","weaponDef { unknown","weaponDef { client","weaponDef { client { } client { standModel m } }","WEAPONDEF { CLIENT { DROPPEDANGLESHACK } }","weaponDef { client { firstPerson { weaponLink { part 0 { model x } } } } }"};
    const char *effects[]={"{ missileTrailFunc GrenadeTrail }","{ missileTrailFunc RocketTrail }","{ missileTrailFunc PyroSmokeTrail }","{ missileTrailFunc DynamiteTrail }","{ ejectBrassFunc MachineGunEjectBrass }","{ ejectBrassFunc PanzerFaustEjectBrass }","{ missileTrailFunc RocketTrail missileTrailFunc unknown ejectBrassFunc MachineGunEjectBrass ejectBrassFunc unknown }"};
    char text[4096],row[128],path[MAX_PATH],pattern[MAX_PATH];int mode,fill,i,j,n=0,fixtures=0;
    WIN32_FIND_DATAA found;HANDLE search;
    captureMediaDiagnostics=1;mediaFilesDirectory=directory;
    *(int *)(base+0x40352c4)=1;
    for(mode=0;mode<4;++mode)for(fill=0;fill<2;++fill) {
        failModel=failSkin=failShader=mode&1;failSound=mode&2;
        for(i=0;i<sizeof(strings)/sizeof(strings[0]);++i) {
            sprintf(text,"{ %s",strings[i]);compareWeaponFile(base,text,0,fill?0x5a:0);++n;
            sprintf(text,"{ %s file %s again }",strings[i],strings[i]);compareWeaponFile(base,text,0,fill?0x5a:0);++n;
            sprintf(text,"{ %s",strings[i]);Q_strupr(text);compareWeaponFile(base,text,0,fill?0x5a:0);++n;
        }
        for(i=0;i<sizeof(numbers)/sizeof(numbers[0]);++i)for(j=0;j<3;++j) {
            sprintf(text,"{ %s %s",numbers[i],j==0?"":j==1?"invalid }":"- 1.25 }");compareWeaponFile(base,text,0,fill?0x5a:0);++n;
        }
        for(i=0;i<sizeof(vectors)/sizeof(vectors[0]);++i)for(j=0;j<4;++j) {
            sprintf(text,"{ %s %s",vectors[i],j==0?"":j==1?"1":j==2?"1 2":"- 1.25 2.5 0 }");compareWeaponFile(base,text,0,fill?0x5a:0);++n;
        }
        for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i){compareWeaponFile(base,(char *)invalid[i],0,fill?0x5a:0);++n;}
        for(i=0;i<sizeof(outer)/sizeof(outer[0]);++i){compareWeaponFile(base,(char *)outer[i],1,fill?0x5a:0);++n;}
        for(i=0;i<sizeof(effects)/sizeof(effects[0]);++i){compareWeaponFile(base,(char *)effects[i],0,fill?0x5a:0);++n;}
        strcpy(text,"{");
        for(i=0;i<6;++i){sprintf(row," flashSound f%d flashEchoSound e%d lastShotSound l%d",i,i,i);strcat(text,row);}
        strcat(text," }");compareWeaponFile(base,text,0,fill?0x5a:0);++n;
        compareWeaponFile(base,"weaponDef { client { } }",2,fill?0x5a:0);++n;
    }
    sprintf(pattern,"%s/*.weaptest",directory);search=FindFirstFileA(pattern,&found);
    if(search==INVALID_HANDLE_VALUE)exit(1);
    do {
        FILE *file;long size;char *contents;
        sprintf(path,"%s/%s",directory,found.cFileName);file=fopen(path,"rb");if(!file)exit(1);
        fseek(file,0,SEEK_END);size=ftell(file);rewind(file);contents=malloc(size+1);if(!contents)exit(1);
        if(fread(contents,1,size,file)!=size)exit(1);contents[size]=0;fclose(file);
        for(mode=0;mode<2;++mode){failModel=failSkin=failShader=failSound=mode;compareWeaponFile(base,contents,1,0);++n;}
        ++fixtures;free(contents);
    }while(FindNextFileA(search,&found));FindClose(search);
    failModel=failSkin=failShader=failSound=0;captureMediaDiagnostics=0;mediaFilesDirectory=NULL;
    printf("%d complete weapon-media parser comparisons (%d original files) passed, including normalized callback selection.\n",n,fixtures);
}

static void testClientRegistration(unsigned char *base) {
    void (__cdecl *original)(int,int)=(void *)(base+0x6d1f0);
    tce_weaponInfo_t *media=(void *)(base+0x484a080);
    tce_weaponDef_t *weapons=(void *)(base+0x4c1df20);
    tce_gearDef_t *gear=(void *)(base+0x4ba9f00);
    char savedNames[128][128],savedCalls[16384],savedWarnings[32768];
    int id,scenario,cached,force,i,n=0,loads,releases,calls;
    captureMediaDiagnostics=1;*(int *)(base+0x40352c4)=1;
    for(id=-1;id<=64;++id)for(scenario=1;scenario<=10;++scenario)
    for(cached=0;cached<2;++cached)for(force=0;force<2;++force) {
        memset(tce_cg_weapons,0x5a,sizeof(tce_cg_weapons));memset(weaponDef,0,sizeof(weaponDef));memset(&gearDef,0,sizeof(gearDef));
        strcpy(gearDef.customWeaponGroup,scenario==10?"default":"test_group");
        for(i=0;i<64;++i) {
            tce_cg_weapons[i].registered=cached;weaponDef[i].maxclip=17;weaponDef[i].startClips=-1;weaponDef[i].parsed=scenario==9;
            if(scenario!=8)strcpy(gearDef.weaponFile[i],"test_rifle");gearDef.startClips[i]=i%7-2;
        }
        memcpy(media,tce_cg_weapons,sizeof(tce_cg_weapons));memcpy(weapons,weaponDef,sizeof(weaponDef));*gear=gearDef;
        registrationPolicy=scenario;loadCount=freed=renderCallCount=0;memset(loadNames,0,sizeof(loadNames));rendererCalls[0]=loaderWarning[0]=0;
        TCE_CG_RegisterWeapon(id,force);
        loads=loadCount;releases=freed;calls=renderCallCount;
        memcpy(savedNames,loadNames,sizeof(savedNames));strcpy(savedCalls,rendererCalls);strcpy(savedWarnings,loaderWarning);
        loadCount=freed=renderCallCount=0;memset(loadNames,0,sizeof(loadNames));rendererCalls[0]=loaderWarning[0]=0;
        original(id,force);
        if(loads!=loadCount || releases!=freed || calls!=renderCallCount || memcmp(savedNames,loadNames,sizeof(savedNames)) || strcmp(savedCalls,rendererCalls) || strcmp(savedWarnings,loaderWarning) ||
           memcmp(media,tce_cg_weapons,sizeof(tce_cg_weapons)) || memcmp(weapons,weaponDef,sizeof(weaponDef)) || memcmp(gear,&gearDef,sizeof(gearDef))) {
            fprintf(stderr,"Client registration mismatch id=%d scenario=%d cached=%d force=%d\n%s\n%s",id,scenario,cached,force,savedWarnings,loaderWarning);exit(1);
        }
        ++n;
    }
    registrationPolicy=0;captureMediaDiagnostics=0;
    printf("%d client weapon registration comparisons passed: all-slot state, cache/force, fallback, media/spec errors.\n",n);
}
