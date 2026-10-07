#include "tce_weapon_ammo.h"
/* Original Windows cgame item table, first-match IT_WEAPON projection.
 * Includes inherited item aliases; IDs 65/66 are NOT valid gear indices.
 * Evidence: ammo_300037c0.c.txt, ammo_30003800.c.txt and weapon_item_ids.json. */
static const struct { int weapon, ammo, clip; } items[] = {
    { 0, 0, 0 }, /* item_health_cabinet */
    { 1, 1, 1 }, /* weapon_knife */
    { 2, 2, 2 }, /* weapon_luger */
    { 39, 39, 39 }, /* weapon_glock */
    { 40, 40, 40 }, /* weapon_deagle */
    { 48, 48, 48 }, /* weapon_m590 */
    { 49, 49, 49 }, /* weapon_m3s90 */
    { 50, 50, 50 }, /* weapon_g3a4 */
    { 41, 41, 41 }, /* weapon_mp5sd */
    { 42, 42, 42 }, /* weapon_ump45 */
    { 44, 44, 44 }, /* weapon_m4 */
    { 45, 45, 45 }, /* weapon_m16 */
    { 43, 43, 43 }, /* weapon_ak47 */
    { 47, 47, 47 }, /* weapon_psg1 */
    { 46, 46, 46 }, /* weapon_sr8 */
    { 51, 51, 51 }, /* weapon_m76 */
    { 5, 5, 5 }, /* weapon_generic_th1 */
    { 6, 6, 6 }, /* weapon_generic_th2 */
    { 13, 13, 13 }, /* weapon_generic_th3 */
    { 38, 2, 38 }, /* weapon_akimboluger */
    { 54, 2, 38 }, /* weapon_akimbosilencedluger */
    { 8, 8, 8 }, /* weapon_thompson */
    { 34, 34, 34 }, /* weapon_dummy */
    { 10, 10, 10 }, /* weapon_sten */
    { 7, 7, 7 }, /* weapon_colt */
    { 37, 39, 37 }, /* weapon_akimbocolt */
    { 53, 39, 37 }, /* weapon_akimbosilencedcolt */
    { 3, 3, 3 }, /* weapon_mp40 */
    { 65, 65, 65 }, /* weapon_panzerfaust */
    { 4, 4, 4 }, /* weapon_grenadelauncher */
    { 9, 9, 9 }, /* weapon_grenadepineapple */
    { 22, 22, 22 }, /* weapon_grenadesmoke */
    { 16, 16, 16 }, /* weapon_smoketrail */
    { 19, 19, 19 }, /* weapon_medic_heal */
    { 15, 15, 15 }, /* weapon_dynamite */
    { 66, 66, 66 }, /* weapon_flamethrower */
    { 17, 17, 17 }, /* weapon_mapmortar */
    { 21, 21, 21 }, /* weapon_class_special */
    { 63, 63, 63 }, /* weapon_arty */
    { 11, 11, 11 }, /* weapon_medic_syringe */
    { 61, 11, 11 }, /* weapon_medic_adrenaline */
    { 12, 12, 12 }, /* weapon_magicammo */
    { 12, 12, 12 }, /* weapon_magicammo2 */
    { 20, 20, 20 }, /* weapon_binoculars */
    { 32, 32, 32 }, /* weapon_kar43 */
    { 58, 32, 32 }, /* weapon_kar43_scope */
    { 23, 23, 23 }, /* weapon_kar98Rifle */
    { 55, 55, 55 }, /* weapon_gpg40 */
    { 56, 56, 56 }, /* weapon_gpg40_allied */
    { 24, 24, 24 }, /* weapon_M1CarbineRifle */
    { 25, 25, 25 }, /* weapon_garandRifle */
    { 57, 25, 25 }, /* weapon_garandRifleScope */
    { 33, 33, 33 }, /* weapon_fg42 */
    { 59, 33, 33 }, /* weapon_fg42scope */
    { 35, 35, 35 }, /* weapon_mortar */
    { 60, 35, 35 }, /* weapon_mortar_set */
    { 26, 26, 26 }, /* weapon_landmine */
    { 27, 27, 27 }, /* weapon_satchel */
    { 28, 28, 28 }, /* weapon_satchelDetonator */
    { 30, 30, 30 }, /* weapon_smokebomb */
    { 29, 29, 29 }, /* weapon_tripmine */
    { 31, 31, 31 }, /* weapon_mobile_mg42 */
    { 62, 31, 31 }, /* weapon_mobile_mg42_set */
    { 14, 2, 2 }, /* weapon_silencer */
    { 52, 7, 7 }, /* weapon_silencedcolt */
    { 19, 19, 19 }, /* weapon_medic_heal */
};
int TCE_BG_FindAmmoForWeapon(int weapon) {
    unsigned i;
    for (i=0; i<sizeof(items)/sizeof(items[0]); ++i)
        if (items[i].weapon == weapon) return items[i].ammo;
    return 0;
}
int TCE_BG_FindClipForWeapon(int weapon) {
    unsigned i;
    for (i=0; i<sizeof(items)/sizeof(items[0]); ++i)
        if (items[i].weapon == weapon) return items[i].clip;
    return 0;
}
int TCE_BG_IsAkimboWeapon(int weapon) {
    return weapon == 37 || weapon == 53 || weapon == 38 || weapon == 54;
}
int TCE_BG_AkimboSidearm(int weapon) {
    if (weapon == 37 || weapon == 53) return 39;
    if (weapon == 38 || weapon == 54) return 2;
    return 0;
}
int TCE_BG_AkimboFireSequence(int weapon, int akimboClip, int mainClip) {
    if (!TCE_BG_IsAkimboWeapon(weapon) || !akimboClip) return 0;
    if (!mainClip) return 1;
    return !(((unsigned)akimboClip + (unsigned)mainClip) & 1);
}
static int TCE_FiringClip(int weapon, const int *clip) {
    int index = TCE_BG_FindClipForWeapon(weapon);
    if (TCE_BG_IsAkimboWeapon(weapon) &&
        !TCE_BG_AkimboFireSequence(weapon, clip[index],
            clip[TCE_BG_FindClipForWeapon(TCE_BG_AkimboSidearm(weapon))]))
        index = TCE_BG_AkimboSidearm(weapon);
    return index;
}
void TCE_PM_WeaponUseAmmo(int weapon, int amount, int noClips, int *ammo, int *clip) {
    /* TC20031cac/20031d2f: SUB wraps the stored 32-bit payload, no clamp. */
    if (noClips)
        ((unsigned int *)ammo)[TCE_BG_FindAmmoForWeapon(weapon)] -= (unsigned int)amount;
    else
        ((unsigned int *)clip)[TCE_FiringClip(weapon, clip)] -= (unsigned int)amount;
}
int TCE_PM_WeaponAmmoAvailable(int weapon, int noClips, const int *ammo, const int *clip) {
    return noClips ? ammo[TCE_BG_FindAmmoForWeapon(weapon)] : clip[TCE_FiringClip(weapon, clip)];
}
int TCE_PM_WeaponClipEmpty(int weapon, int noClips, const int *ammo, const int *clip) {
    /* Original checks the own clip, not the alternating firing side. */
    return (noClips ? ammo[TCE_BG_FindAmmoForWeapon(weapon)] : clip[TCE_BG_FindClipForWeapon(weapon)]) == 0;
}
void TCE_PM_ReloadClip(int weapon, int *ammo, int *clip, const tce_weaponDef_t *defs) {
    do {
        int ai = TCE_BG_FindAmmoForWeapon(weapon);
        int ci = TCE_BG_FindClipForWeapon(weapon);
        int move = ammo[ai] < defs[weapon].maxclip ? ammo[ai] : defs[weapon].maxclip;
        if (defs[weapon].singleReload && move >= 2) move = 1;
        if (move) {
            ((unsigned int *)ammo)[ai] -= (unsigned int)move;
            if (defs[weapon].singleReload) ((unsigned int *)clip)[ci] += (unsigned int)move;
            else clip[ci] = move;
        }
        if (!TCE_BG_IsAkimboWeapon(weapon)) return;
        weapon = TCE_BG_AkimboSidearm(weapon);
    } while (1);
}
