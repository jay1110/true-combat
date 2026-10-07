#ifndef TCE_WEAPON_AMMO_H
#define TCE_WEAPON_AMMO_H
#include "tce_bg.h"
int TCE_BG_FindAmmoForWeapon(int weapon);
int TCE_BG_FindClipForWeapon(int weapon);
int TCE_BG_IsAkimboWeapon(int weapon);
int TCE_BG_AkimboSidearm(int weapon);
int TCE_BG_AkimboFireSequence(int weapon, int akimboClip, int mainClip);
void TCE_PM_WeaponUseAmmo(int weapon, int amount, int noClips, int *ammo, int *clip);
int TCE_PM_WeaponAmmoAvailable(int weapon, int noClips, const int *ammo, const int *clip);
int TCE_PM_WeaponClipEmpty(int weapon, int noClips, const int *ammo, const int *clip);
/* Valid weapon-definition indices are 0..63, as in the original caller. */
void TCE_PM_ReloadClip(int weapon, int *ammo, int *clip, const tce_weaponDef_t *defs);
#endif
