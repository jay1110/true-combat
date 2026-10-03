/* Both parsers receive the same token stream. The recovered parser uses the
 * original DLL's PC_* primitive helpers here; module builds use SDK PC_*.
 * This isolates field mapping, control flow, errors and resource ownership.
 */
#include "../game/q_shared.h"
#include "../game/bg_public.h"
#include "../game/tce_bg.h"
#include <windows.h>

static char *input, *cursor;
static char pendingNumber[MAX_TOKEN_CHARS];
static int freed, loadCount;
static const char *availableSource;
static char loadNames[3][128];
static qboolean (__cdecl *pcVec)(int, vec3_t *);
static int (__cdecl *originalGear)(const char *, const char *, tce_gearDef_t *);
static int testGearParser(const char *directory);
static qboolean (__cdecl *pcInt)(int, int *);
static qboolean (__cdecl *pcFloat)(int, float *);
static qboolean (__cdecl *pcString)(int, char *, size_t);
static int (__cdecl *originalParse)(const char *, tce_weaponDef_t *);
void compareWeaponInit(const tce_weaponDef_t *input);

int trap_PC_LoadSource(const char *filename) {
    if (loadCount < 3) Q_strncpyz(loadNames[loadCount], filename, 128);
    ++loadCount;
    cursor = input;
    pendingNumber[0] = 0;
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

static int QDECL parserSyscall(int command, ...) {
    int handle, result = 0;
    va_list args;
    va_start(args, command);
    switch(command) {
    case 0: (void)va_arg(args, char *); break; /* diagnostic */
    case 0x5f: result = trap_PC_LoadSource(va_arg(args, char *)); break;
    case 0x60: result = trap_PC_FreeSource(va_arg(args, int)); break;
    case 0x61:
        handle = va_arg(args, int);
        result = trap_PC_ReadToken(handle, va_arg(args, pc_token_t *)); break;
    case 0x62: {
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
    tce_weaponDef_t a, b;
    int resultA, resultB, freedA;
    memset(&a, 0x5a, sizeof(a)); b = a;
    input = text; freed = 0;
    resultA = BG_ParseWeaponDef(filename, &a); freedA = freed;
    freed = 0;
    resultB = originalParse(filename, &b);
    if (resultA != resultB || freedA != freed || memcmp(&a, &b, sizeof(a))) {
        fprintf(stderr, "Parser differs from original: %s\n", filename); exit(1);
    }
}

int testWeaponParser(unsigned char *base, const char *directory) {
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
    pcVec = (void *)(base + 0x60a0); originalGear = (void *)(base + 0x7b90);
    pcInt = (void *)(base + 0x60e0); pcFloat = (void *)(base + 0x5fb0);
    pcString = (void *)(base + 0x61d0); originalParse = (void *)(base + 0x7390);
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
    return count + testGearParser(directory);
}

static void compareGear(char *text, const char *available) {
    tce_gearDef_t a, b;
    int resultA, resultB, freedA, loadsA;
    char namesA[3][128];
    memset(&a, 0x5a, sizeof(a)); b = a;
    input = text; availableSource = available;
    freed = loadCount = 0; memset(loadNames, 0, sizeof(loadNames));
    resultA = BG_ParseGearDef("primary", "fallback", &a);
    freedA = freed; loadsA = loadCount; memcpy(namesA, loadNames, sizeof(namesA));
    freed = loadCount = 0; memset(loadNames, 0, sizeof(loadNames));
    resultB = originalGear("primary", "fallback", &b);
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
        "gear_manager { gm_weapon { weaponID WP_MP40 team terrorists slot secondary requiredSkill 1.9 - 2.9 3.1 startClips 4 weaponFile foo equivalentWeaponID WP_LUGER } gm_weapon { startClips 9 } }",
        "gear_manager { gm_weapon { weaponID unknown team specops slot primary } gm_weapon { weaponID WP_LUGER team other slot other } }",
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
