/* Recovered from cgame_mp_x86.dll:30007390 and Linux BG_ParseWeaponDef.
 * Uses the SDK's engine parser, including its preprocessing and token rules.
 */
#include "q_shared.h"
#include "bg_public.h"
#include "tce_bg.h"

int TCE_PWF_ParseError(int handle, const char *format, ...) {
    char message[4096], filename[128];
    int line = 0;
    va_list args;
    va_start(args, format);
    Q_vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    filename[0] = 0;
    trap_PC_SourceFileAndLine(handle, filename, &line);
    Com_Printf("^1ERROR: %s, line %d: %s\n", filename, line, message);
    trap_PC_FreeSource(handle);
    return 0;
}

int BG_ParseWeaponDef(const char *filename, tce_weaponDef_t *definition) {
    enum { FIELD_INT, FIELD_FLOAT, FIELD_STRING };
    static const struct {
        const char *name;
        size_t offset;
        int type;
        const char *error;
    } fields[] = {
#define FIELD(name, type, message) { #name, offsetof(tce_weaponDef_t, name), type, message }
        FIELD(weapClass, FIELD_STRING, "expected weapClass identifier"),
        FIELD(caliberClass, FIELD_STRING, "expected caliberClass identifier"),
        FIELD(maxclip, FIELD_INT, "expected maxclip value"),
        FIELD(semiauto, FIELD_INT, "expected semiauto value"),
        FIELD(fullauto, FIELD_INT, "expected fullauto value"),
        FIELD(burst, FIELD_INT, "expected burst value"),
        FIELD(pump, FIELD_INT, "expected pump value"),
        FIELD(bolt, FIELD_INT, "expected bolt value"),
        FIELD(singleReload, FIELD_INT, "expected singleReload value"),
        FIELD(scoped, FIELD_FLOAT, "expected scoped value"),
        FIELD(scopeReticleType, FIELD_INT, "expected scopeReticleType value"),
        FIELD(useScopeReticleShader, FIELD_INT, "expected useScopeReticleShader value"),
        FIELD(nightVision, FIELD_INT, "expected nightVision value"),
        FIELD(longWaveIR, FIELD_INT, "expected longWaveIR value"),
        FIELD(suppressed, FIELD_INT, "expected suppressed value"),
        FIELD(subsonic, FIELD_INT, "expected subsonic value"),
        FIELD(jamPercentage, FIELD_FLOAT, "expected jamPercentage value"),
        FIELD(grenadeTimer, FIELD_INT, "expected grenadeTimer value"),
        FIELD(noTacMode, FIELD_INT, "expected noTacMode value"),
        FIELD(usesWolfAnim, FIELD_INT, "expected usesWolfAnimvalue"),
        FIELD(usesPistolAnimMod, FIELD_INT, "expected usesPistolAnimMod value"),
        FIELD(usesRecoilAnimMod, FIELD_INT, "expected usesRecoilAnimMod value")
#undef FIELD
    };
    pc_token_t token;
    unsigned int i;
    int ok, handle = trap_PC_LoadSource(filename);
    if (!handle) return 0;
    if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "weaponDef"))
        return TCE_PWF_ParseError(handle, "expected 'weaponDef'");
    if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{"))
        return TCE_PWF_ParseError(handle, "expected '{'");
    while (trap_PC_ReadToken(handle, &token) && token.string[0] != '}') {
        for (i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i)
            if (!Q_stricmp(token.string, fields[i].name)) break;
        if (i == sizeof(fields)/sizeof(fields[0]))
            return TCE_PWF_ParseError(handle, "unknown token '%s'", token.string);
        if (fields[i].type == FIELD_STRING)
            ok = PC_String_ParseNoAlloc(handle, (char *)definition + fields[i].offset, 64);
        else if (fields[i].type == FIELD_FLOAT)
            ok = PC_Float_Parse(handle, (float *)((char *)definition + fields[i].offset));
        else
            ok = PC_Int_Parse(handle, (int *)((char *)definition + fields[i].offset));
        if (!ok) return TCE_PWF_ParseError(handle, "%s", fields[i].error);
    }
    /* Original accepts EOF after the opening brace; preserve that behavior. */
    definition->parsed = 1;
    trap_PC_FreeSource(handle);
    return 1;
}
