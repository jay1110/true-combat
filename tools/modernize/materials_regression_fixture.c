#include "src/cgame/cg_local.h"
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <strings.h>
#endif

static int checks, failures, enabled = 1, cvarCalls, opens, reads, closes, loadCalls, addedCount;
static char message[256];
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL line%d %s\n", __LINE__, #x); } } while (0)
/* ASSET_BODIES */
static struct { const char *body; int length, handle, exists; } files[5];
static char definitions[5][8193];
static const char foreignDefinition[] = "unrelated/renderer_feature { { map unrelated.tga } }";
static const char *foreign = foreignDefinition;

int Q_stricmp(const char *a, const char *b) {
#ifdef _WIN32
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}
void trap_Cvar_Register(vmCvar_t *value, const char *name, const char *def, int flags) {
    CHECK(!strcmp(name, "cg_materialEffects")); CHECK(!strcmp(def, "1")); CHECK(flags == CVAR_ARCHIVE);
    value->integer = enabled; ++cvarCalls;
}
int trap_FS_FOpenFile(const char *path, fileHandle_t *file, fsMode_t mode);
void trap_FS_Read(void *buffer, int length, fileHandle_t file);
void trap_FS_FCloseFile(fileHandle_t file);
qboolean trap_R_LoadDynamicShader(const char *name, const char *text);
void QDECL CG_Printf(const char *format, ...);
#include "production.c"

int trap_FS_FOpenFile(const char *path, fileHandle_t *file, fsMode_t mode) {
    int i;
    ++opens; CHECK(mode == FS_READ);
    for (i = 0; i < (int)(sizeof(files) / sizeof(files[0])); ++i) if (!strcmp(path, tceMaterials[i].path)) {
        *file = files[i].handle ? i + 1 : 0; return files[i].length;
    }
    CHECK(0); *file = 0; return -1;
}
void trap_FS_Read(void *buffer, int length, fileHandle_t file) {
    CHECK(file > 0 && file <= 5); CHECK(length > 0 && length <= 8192);
    CHECK(files[file - 1].body != NULL && length == files[file - 1].length);
    ++reads; memcpy(buffer, files[file - 1].body, length);
}
void trap_FS_FCloseFile(fileHandle_t file) { CHECK(file > 0 && file <= 5); ++closes; }
qboolean trap_R_LoadDynamicShader(const char *name, const char *text) {
    int i;
    ++loadCalls; CHECK(name != NULL && text != NULL); /* No deletes or purge calls. */
    for (i = 0; i < (int)(sizeof(files) / sizeof(files[0])); ++i) if (!strcmp(name, tceMaterials[i].name)) {
        CHECK(CG_TCEMaterialText(text, (int)strlen(text), name));
        if (files[i].exists) return qfalse;
        strcpy(definitions[i], text); files[i].exists = 1; ++addedCount; return qtrue;
    }
    CHECK(0); return qfalse;
}
void QDECL CG_Printf(const char *format, ...) {
    va_list args; va_start(args, format); vsprintf(message, format, args); va_end(args);
}
static void reset(void) {
    int i;
    enabled = 1; cvarCalls = opens = reads = closes = loadCalls = addedCount = 0; message[0] = 0;
    memset(definitions, 0, sizeof(definitions));
    for (i = 0; i < (int)(sizeof(files) / sizeof(files[0])); ++i) {
        files[i].body = assetBodies[i]; files[i].length = (int)strlen(assetBodies[i]);
        files[i].handle = 1; files[i].exists = 0;
    }
    CHECK(foreign == foreignDefinition && !strcmp(foreign, foreignDefinition));
}
static void mapGates(void) {
    const char *allowed[] = { "maps/obj_delta.bsp", "maps/obj_northport.bsp", "maps/obj_railhouse.bsp",
        "maps/obj_snow.bsp", "maps/obj_stadtrand.bsp", "maps/obj_village.bsp" };
    const char *denied[] = { NULL, "", "obj_delta", "maps/obj_rain.bsp", "maps/obj_bloc.bsp", "maps/goldrush.bsp",
        "maps/oasis.bsp", "maps/obj_delta.bsp.bak", "maps/../maps/obj_delta.bsp", "maps/obj_deltax.bsp" };
    int i;
    for (i = 0; i < (int)(sizeof(allowed) / sizeof(allowed[0])); ++i) {
        reset(); CG_TCEInstallMaterials(allowed[i]);
        CHECK(cvarCalls == 1 && opens == 5 && reads == 5 && closes == 5 && loadCalls == 5 && addedCount == 5);
        CHECK(strstr(message, "5 added, 0 unchanged, 0 files skipped") != NULL);
    }
    for (i = 0; i < (int)(sizeof(denied) / sizeof(denied[0])); ++i) {
        reset(); CG_TCEInstallMaterials(denied[i]); CHECK(cvarCalls == 1 && !opens && !loadCalls && !message[0]);
    }
    reset(); enabled = 0; CG_TCEInstallMaterials(allowed[0]); CHECK(cvarCalls == 1 && !opens && !loadCalls);
}
static void missingAndBounds(void) {
    char boundary[8193], embedded[1024]; int i, length;
    reset(); for (i = 0; i < 5; ++i) { files[i].length = -1; files[i].handle = 0; }
    CG_TCEInstallMaterials("maps/obj_delta.bsp"); CHECK(opens == 5 && !reads && !closes && !loadCalls);
    reset(); files[0].length = 0; files[1].length = 8193; files[2].handle = 0;
    CG_TCEInstallMaterials("maps/obj_delta.bsp"); CHECK(reads == 2 && closes == 4 && addedCount == 2);
    reset(); memset(boundary, ' ', 8192); length = (int)strlen(assetBodies[0]); memcpy(boundary, assetBodies[0], length);
    boundary[8192] = 0; files[0].body = boundary; files[0].length = 8192;
    CG_TCEInstallMaterials("maps/obj_delta.bsp"); CHECK(reads == 5 && addedCount == 5);
    reset(); length = (int)strlen(assetBodies[0]); memcpy(embedded, assetBodies[0], length); embedded[length / 2] = 0;
    files[0].body = embedded; files[0].length = length;
    CG_TCEInstallMaterials("maps/obj_delta.bsp"); CHECK(reads == 5 && closes == 5 && addedCount == 4 && !files[0].exists);
}
static void containerSyntax(void) {
    static const char good[] = "/* } comment */\n\"textures/coroner/asphalt_e1\"\n{\n{ map \"ignore } // /* literal\" }\n} // trailing {";
    static const char *bad[] = {
        "textures/coroner/asphalt_e10 { { map x } }", "textures/coroner/asphalt_e1 { }",
        "textures/coroner/asphalt_e1 { { map x }", "textures/coroner/asphalt_e1 { { map x } } }",
        "textures/coroner/asphalt_e1 { { map x } } other/name { { map x } }",
        "textures/coroner/asphalt_e1 { { map x } } /* unfinished",
        "textures/coroner/asphalt_e1 { { map \"unfinished } }",
        "textures/coroner/asphalt_e1 { { map x } } arbitraryTail",
        "textures/coroner/asphalt_e1{ { map x } }",
        "textures/coroner/asphalt_e1 { { { { { { { { { map x } } } } } } } } }"
    };
    int i;
    CHECK(CG_TCEMaterialText(good, (int)strlen(good), tceMaterials[0].name));
    for (i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); ++i) CHECK(!CG_TCEMaterialText(bad[i], (int)strlen(bad[i]), tceMaterials[0].name));
    CHECK(!CG_TCEMaterialText(good, -1, tceMaterials[0].name)); CHECK(!CG_TCEMaterialText(good, 0, tceMaterials[0].name));
    CHECK(!CG_TCEMaterialText(NULL, 4, tceMaterials[0].name)); CHECK(!CG_TCEMaterialText(good, 4, NULL));
    reset(); files[0].body = bad[4]; files[0].length = (int)strlen(bad[4]);
    CG_TCEInstallMaterials("maps/obj_delta.bsp"); CHECK(addedCount == 4 && !files[0].exists);
}
static void reinit(void) {
    int i;
    reset(); CG_TCEInstallMaterials("maps/obj_delta.bsp"); CHECK(addedCount == 5);
    CG_TCEInstallMaterials("maps/obj_delta.bsp"); CHECK(addedCount == 5 && loadCalls == 10);
    CHECK(strstr(message, "0 added, 5 unchanged") != NULL);
    for (i = 0; i < 5; ++i) CHECK(!strcmp(definitions[i], assetBodies[i]));
    /* Renderer restart clears the native list; no stale CGame-only skip cache. */
    for (i = 0; i < 5; ++i) files[i].exists = 0;
    CG_TCEInstallMaterials("maps/obj_delta.bsp"); CHECK(addedCount == 10 && loadCalls == 15);
    reset(); enabled = 0; CG_TCEInstallMaterials("maps/obj_delta.bsp"); CHECK(!addedCount && !loadCalls);
    reset(); CG_TCEInstallMaterials("maps/obj_fracture_v2.bsp"); CHECK(!addedCount && !loadCalls);
}
int main(void) {
    CHECK((int)(sizeof(assetBodies) / sizeof(assetBodies[0])) == (int)(sizeof(tceMaterials) / sizeof(tceMaterials[0])));
    mapGates(); missingAndBounds(); containerSyntax(); reinit();
    printf("Material loader regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
