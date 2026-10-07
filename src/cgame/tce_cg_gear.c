/* Recovered CG_LoadGearDef, Windows cgame:3006d120. */
#include "cg_local.h"
#include "../game/tce_bg.h"

void CG_LoadGearDef(void) {
    const char *suffix, *filename, *fallback;
    if (gearDef.parsed) return;
    switch (cgs.gametype) {
    case 7: suffix = "_gt7"; break;
    case 2: suffix = "_gt2"; break;
    case 5: suffix = "_gt5"; break;
    default:
        /* Original uses uninitialized pointers here. These modes are outside
         * TC:E's supported domain; keep that undefined behavior out of C. */
        CG_Printf("^1WARNING: unsupported gametype %i for gear loading\n", cgs.gametype);
        return;
    }
    filename = va("maps/%s%s.gear", cgs.rawmapname, suffix);
    fallback = va("maps/default%s.gear", suffix);
    if (!BG_ParseGearDef(filename, fallback, &gearDef))
        CG_Printf("^1WARNING: failed to register gear from %s\n", filename);
}
