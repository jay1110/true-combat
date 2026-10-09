/* Optional Omni-bot extension; not an original TC:E function reconstruction. */
#include "g_local.h"
#include "tce_bg.h"
#include "tce_omnibot_loadout.h"

qboolean TCE_OmniClassValid(int playerClass) {
    return playerClass == PC_SOLDIER || playerClass == PC_ENGINEER ||
        playerClass == PC_COVERTOPS;
}

qboolean TCE_OmniWeaponAvailable(gentity_t *ent, int playerClass,
    int weapon, int slot, int primary) {
    int role, skill;
    /* The recovered parser folds slot "none" into 1. These fixed utility
     * slots are equipped separately by native spawn, never as a primary gun. */
    if (weapon == TCE_WP_K1 || weapon == TCE_WP_G1 || weapon == TCE_WP_G2 ||
        weapon == TCE_WP_G3 || weapon == TCE_WP_DC1) return qfalse;
    if (!ent || !ent->client || !(ent->r.svFlags & SVF_BOT) ||
        !TCE_OmniClassValid(playerClass) || !gearDef.parsed ||
        (ent->client->sess.sessionTeam != TEAM_AXIS &&
         ent->client->sess.sessionTeam != TEAM_ALLIES) ||
        weapon <= 0 || weapon >= TCE_MAX_WEAPONS ||
        (slot != 1 && slot != 2) || gearDef.slot[weapon] != slot ||
        !weaponDef[weapon].parsed || g_knifeonly.integer == 1) return qfalse;
    role = BG_WolfClassToTCE(playerClass);
    skill = (int)ent->client->sess.skillpoints[role] + 1;
    if (!BG_WeaponIsAvailable(weapon, gearDef.requiredSkill[weapon][role],
            gearDef.team[weapon], skill, ent->client->sess.sessionTeam)) return qfalse;
    if (slot == 1 && G_IsWeaponDisabled(ent, (weapon_t)weapon)) return qfalse;
    if (slot == 2 && (primary <= 0 || primary >= TCE_MAX_WEAPONS ||
            !BG_SidearmAvailableForPrimary(weapon, primary))) return qfalse;
    return qtrue;
}

static int TCE_OmniChooseWeapon(gentity_t *ent, int playerClass,
    int requested, int slot, int primary) {
    int choices[TCE_MAX_WEAPONS], count = 0, weapon;
    if (requested != -1)
        return TCE_OmniWeaponAvailable(ent, playerClass, requested, slot, primary) ?
            requested : -1;
    for (weapon = 1; weapon < TCE_MAX_WEAPONS; ++weapon)
        if (TCE_OmniWeaponAvailable(ent, playerClass, weapon, slot, primary))
            choices[count++] = weapon;
    return count ? choices[rand() % count] : -1;
}

static qboolean TCE_OmniChoosePair(gentity_t *ent, int pc, int primary,
    int secondary, int *outPrimary, int *outSecondary) {
    int primaries[TCE_MAX_WEAPONS], sidearms[TCE_MAX_WEAPONS];
    int weapon, sidearm, count = 0, index;
    for (weapon = 1; weapon < TCE_MAX_WEAPONS; ++weapon) {
        if (primary != -1 && primary != weapon) continue;
        if (!TCE_OmniWeaponAvailable(ent, pc, weapon, 1, 0)) continue;
        sidearm = TCE_OmniChooseWeapon(ent, pc, secondary, 2, weapon);
        if (sidearm < 0) continue;
        primaries[count] = weapon;
        sidearms[count++] = sidearm;
    }
    if (!count) return qfalse;
    index = rand() % count;
    *outPrimary = primaries[index];
    *outSecondary = sidearms[index];
    return qtrue;
}

qboolean TCE_OmniSelectLoadout(gentity_t *ent, int playerClass,
    int primary, int secondary) {
    static const int classes[3] = { PC_SOLDIER, PC_ENGINEER, PC_COVERTOPS };
    int candidates[3], count = 0, i, pc, selectedPrimary, selectedSecondary;
    int classPrimaries[3], classSecondaries[3];
    if (!ent || !ent->client || !(ent->r.svFlags & SVF_BOT) ||
        (playerClass != -1 && !TCE_OmniClassValid(playerClass)) ||
        (ent->client->sess.sessionTeam != TEAM_AXIS &&
         ent->client->sess.sessionTeam != TEAM_ALLIES)) return qfalse;
    /* Candidate construction is atomic: failed explicit selections leave the
     * current and latched equipment untouched. No skill or gear unlocks. */
    for (i = 0; i < 3; ++i) {
        pc = classes[i];
        if (playerClass != -1 && playerClass != pc) continue;
        if (g_knifeonly.integer == 1 && primary == -1 && secondary == -1) {
            selectedPrimary = selectedSecondary = 0;
        } else {
            if (!TCE_OmniChoosePair(ent, pc, primary, secondary,
                    &selectedPrimary, &selectedSecondary)) continue;
        }
        candidates[count] = pc;
        classPrimaries[count] = selectedPrimary;
        classSecondaries[count++] = selectedSecondary;
    }
    if (!count) return qfalse;
    i = rand() % count;
    ent->client->sess.latchPlayerType = candidates[i];
    ent->client->sess.latchPlayerWeapon = classPrimaries[i];
    ent->client->sess.latchPlayerWeapon2 = classSecondaries[i];
    /* Clear an old class's optional demolition charge; native spawn equips
     * knife/grenades and applies VIP, prisoner and knife-only state itself. */
    ent->client->sess.latchPlayerWeapon3 = 0;
    return qtrue;
}
