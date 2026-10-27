#ifndef G_TCE_WEAPON_STATS_H
#define G_TCE_WEAPON_STATS_H

#include "tce_weapon_stats.h"
struct gentity_s;
typedef struct { unsigned token, generation; int weapon; } g_tceStatsShot_t;
void G_TCEWeaponStatsResetAll(void);
void G_TCEWeaponStatsReset(int clientNum);
g_tceStatsShot_t G_TCEWeaponStatsBegin(struct gentity_s *ent, int weapon);
void G_TCEWeaponStatsEnd(struct gentity_s *ent, g_tceStatsShot_t previous);
void G_TCEWeaponStatsDamage(struct gentity_s *target, struct gentity_s *inflictor,
    struct gentity_s *attacker, int mod, int take, int wasAlive, int head);
qboolean G_TCEWeaponStatsRequest(struct gentity_s *ent);

#endif
