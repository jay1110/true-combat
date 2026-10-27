#ifndef CG_TCE_WEAPON_STATS_H
#define CG_TCE_WEAPON_STATS_H
void CG_TCEWeaponStatsRequest(int clientNum);
void CG_TCEWeaponStatsCancel(void);
void CG_TCEWeaponStatsParse(void);
void CG_TCEWeaponStatsUpdate(void);
void CG_TCEWeaponStatsReset(void);
void CG_wStatsUp_f(void);
#endif
