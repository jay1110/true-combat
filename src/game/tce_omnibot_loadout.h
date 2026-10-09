#ifndef TCE_OMNIBOT_LOADOUT_H
#define TCE_OMNIBOT_LOADOUT_H

/* Include g_local.h first. Native TC weapon IDs and Wolf class IDs (0/2/4).
 * -1 requests automatic selection. Selection changes next-spawn latches only. */
qboolean TCE_OmniClassValid(int playerClass);
qboolean TCE_OmniWeaponAvailable(gentity_t *ent, int playerClass,
    int weapon, int slot, int primary);
qboolean TCE_OmniSelectLoadout(gentity_t *ent, int playerClass,
    int primary, int secondary);

#endif
