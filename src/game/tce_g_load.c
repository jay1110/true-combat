/* Recovered Windows server loaders: 200a01a0 and 200a0270. */
#include "g_local.h"
#include "tce_bg.h"

void G_LoadGearDef(void) {
    const char *suffix, *filename, *fallback;
    if (gearDef.parsed) return;
    switch (g_gametype.integer) {
    case 7: suffix = "_gt7"; break;
    case 2: suffix = "_gt2"; break;
    case 5: suffix = "_gt5"; break;
    default:
        /* Original has uninitialized pointers outside supported TC:E modes. */
        G_Printf("^1WARNING: unsupported gametype %i for gear loading\n", g_gametype.integer);
        return;
    }
    filename = va("maps/%s%s.gear", level.rawmapname, suffix);
    fallback = va("maps/default%s.gear", suffix);
    if (!BG_ParseGearDef(filename, fallback, &gearDef))
        G_Printf("^1WARNING: failed to register gear from %s\n", filename);
}

void G_LoadWeaponDef(void) {
    int i;
    for (i = 0; i < TCE_MAX_WEAPONS; ++i) {
        tce_weaponDef_t *weapon = &weaponDef[i];
        const char *filename;
        if (!gearDef.weaponFile[i][0] || weapon->parsed) continue;
        filename = va("custom/%s/weapons/%s.specs", gearDef.customWeaponGroup, gearDef.weaponFile[i]);
        if (!BG_ParseWeaponDef(filename, weapon)) {
            G_Printf("^1WARNING: Server failed to register specs for weapon %i from %s, trying default ...\n", i, filename);
            filename = va("custom/default/weapons/%s.specs", gearDef.weaponFile[i]);
            if (!BG_ParseWeaponDef(filename, weapon))
                G_Printf("^1WARNING: Server failed to register specs for weapon %i from %s\n", i, filename);
        }
        /* The original initializes even after both files failed to parse. */
        weapon->startClips = gearDef.startClips[i];
        if (!BG_InitializeWeaponDef(weapon))
            G_Printf("^1WARNING: Server failed to initialize weapon %i according to %s\n", i, filename);
    }
}
