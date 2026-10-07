/* Recovered client media parser subtree. Addresses and validation in ledger.
 * TCE_ names keep the different TC:E media ABI separate from the ET renderer.
 */
#include "cg_local.h"
#include "tce_weapon_media.h"

static void FormatDiagnostic(char *destination, const char *format, ...) {
    va_list args;
    va_start(args, format); Q_vsnprintf(destination, 1024, format, args); va_end(args);
}

int TCE_CG_RW_ParseError(int handle, const char *format, ...) {
    char message[4096], filename[128], diagnostic[1024];
    int line = 0;
    va_list args;
    va_start(args, format); Q_vsnprintf(message, sizeof(message), format, args); va_end(args);
    filename[0] = 0;
    trap_PC_SourceFileAndLine(handle, filename, &line);
    /* The original Com_Printf intermediary has a 1024-byte buffer. */
    FormatDiagnostic(diagnostic, "^1ERROR: %s, line %d: %s\n", filename, line, message);
    CG_Printf("%s", diagnostic);
    trap_PC_FreeSource(handle);
    return 0;
}

int TCE_CG_RW_ParseWeaponLinkPart(int handle, tce_weaponInfo_t *info, int view) {
    pc_token_t token;
    char filename[64];
    int index;
    tce_partModel_t *part;
    if (!PC_Int_Parse(handle, &index)) return TCE_CG_RW_ParseError(handle, "expected part index");
    if (index < 0 || index >= 7) return TCE_CG_RW_ParseError(handle, "part index out of bounds");
    part = &info->partModels[view][index];
    memset(part, 0, sizeof(*part));
    if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{"))
        return TCE_CG_RW_ParseError(handle, "expected '{'");
    while (trap_PC_ReadToken(handle, &token) && token.string[0] != '}') {
        if (!Q_stricmp(token.string, "tag")) {
            if (!PC_String_ParseNoAlloc(handle, part->tagName, sizeof(part->tagName)))
                return TCE_CG_RW_ParseError(handle, "expected tag name");
        } else if (!Q_stricmp(token.string, "model")) {
            if (!PC_String_ParseNoAlloc(handle, filename, sizeof(filename)))
                return TCE_CG_RW_ParseError(handle, "expected model filename");
            part->model = trap_R_RegisterModel(filename);
        } else if (!Q_stricmp(token.string, "skin") || !Q_stricmp(token.string, "axisSkin") || !Q_stricmp(token.string, "alliedSkin")) {
            int skin = !Q_stricmp(token.string, "skin") ? 0 : !Q_stricmp(token.string, "axisSkin") ? 1 : 2;
            if (!PC_String_ParseNoAlloc(handle, filename, sizeof(filename)))
                return TCE_CG_RW_ParseError(handle, "expected skin filename");
            part->skin[skin] = trap_R_RegisterSkin(filename);
        } else if (!Q_stricmp(token.string, "RDF_PORTALSCOPE")) part->portalScope = 1;
        else if (!Q_stricmp(token.string, "RDF_NOPORTALSCOPE")) part->noPortalScope = 1;
        else if (!Q_stricmp(token.string, "RDF_TACVIEW")) part->tacView = 1;
        else if (!Q_stricmp(token.string, "RDF_NOTACVIEW")) part->noTacView = 1;
        else if (!Q_stricmp(token.string, "modFlags")) {
            if (!PC_Int_Parse(handle, &part->modFlags)) return TCE_CG_RW_ParseError(handle, "expected modFlags value");
        } else return TCE_CG_RW_ParseError(handle, "unknown token '%s'", token.string);
    }
    return 1;
}

int TCE_CG_RW_ParseWeaponLink(int handle, tce_weaponInfo_t *info, int view) {
    pc_token_t token;
    if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{"))
        return TCE_CG_RW_ParseError(handle, "expected '{'");
    while (trap_PC_ReadToken(handle, &token) && token.string[0] != '}') {
        if (Q_stricmp(token.string, "part")) return TCE_CG_RW_ParseError(handle, "unknown token '%s'", token.string);
        if (!TCE_CG_RW_ParseWeaponLinkPart(handle, info, view)) return 0;
    }
    return 1;
}

int TCE_CG_RW_ParseViewType(int handle, tce_weaponInfo_t *info, int view) {
    pc_token_t token;
    char filename[64];
    if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{"))
        return TCE_CG_RW_ParseError(handle, "expected '{'");
    while (trap_PC_ReadToken(handle, &token) && token.string[0] != '}') {
        if (!Q_stricmp(token.string, "weaponLink")) {
            if (!TCE_CG_RW_ParseWeaponLink(handle, info, view)) return 0;
        } else if (!Q_stricmp(token.string, "model") || !Q_stricmp(token.string, "flashModel")) {
            int flash = !Q_stricmp(token.string, "flashModel");
            if (!PC_String_ParseNoAlloc(handle, filename, sizeof(filename)))
                return TCE_CG_RW_ParseError(handle, flash ? "expected flashModel filename" : "expected model filename");
            if (flash) info->flashModel[view] = trap_R_RegisterModel(filename);
            else info->weaponModel[view].model = trap_R_RegisterModel(filename);
        } else if (!Q_stricmp(token.string, "skin") || !Q_stricmp(token.string, "axisSkin") || !Q_stricmp(token.string, "alliedSkin")) {
            int skin = !Q_stricmp(token.string, "skin") ? 0 : !Q_stricmp(token.string, "axisSkin") ? 1 : 2;
            if (!PC_String_ParseNoAlloc(handle, filename, sizeof(filename)))
                return TCE_CG_RW_ParseError(handle, "expected skin filename");
            info->weaponModel[view].skin[skin] = trap_R_RegisterSkin(filename);
        } else return TCE_CG_RW_ParseError(handle, "unknown token '%s'", token.string);
    }
    return 1;
}

int TCE_CG_RW_ParseModModel(int handle, tce_weaponInfo_t *info) {
    int index;
    char filename[64];
    if (!PC_Int_Parse(handle, &index)) return TCE_CG_RW_ParseError(handle, "expected mod index");
    if (index < 0 || index >= 6) return TCE_CG_RW_ParseError(handle, "mod index out of bounds");
    if (!PC_String_ParseNoAlloc(handle, filename, sizeof(filename)))
        return TCE_CG_RW_ParseError(handle, "expected model filename");
    info->modModels[index] = trap_R_RegisterModel(filename);
    if (!info->modModels[index]) info->modModels[index] = trap_R_RegisterShader(filename);
    return 1;
}
