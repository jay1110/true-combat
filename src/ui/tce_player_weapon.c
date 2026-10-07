/* UI40013c30 and original UI item table4002cb78. Menu-preview metadata;
 * active first-person weapon media are loaded separately from TC .weap files. */
#include "ui_local.h"

static const struct { int weapon; const char *model; } previewItems[] = {
    {0, NULL}, /* item_health_cabinet */
    {1, "models/multiplayer/knife/knife.md3"}, /* weapon_knife */
    {2, "models/weapons2/luger/luger.md3"}, /* weapon_luger */
    {39, "models/weapons2/luger/luger.md3"}, /* weapon_glock */
    {40, "models/weapons2/luger/luger.md3"}, /* weapon_deagle */
    {48, "models/weapons2/luger/luger.md3"}, /* weapon_m590 */
    {49, "models/weapons2/luger/luger.md3"}, /* weapon_m3s90 */
    {50, "models/weapons2/luger/luger.md3"}, /* weapon_g3a4 */
    {41, "models/weapons2/mp40/mp40.md3"}, /* weapon_mp5sd */
    {42, "models/weapons2/ump45/ump45.md3"}, /* weapon_ump45 */
    {44, "models/weapons2/mp40/mp40.md3"}, /* weapon_m4 */
    {45, "models/weapons2/mp40/mp40.md3"}, /* weapon_m16 */
    {43, "models/weapons2/mp40/mp40.md3"}, /* weapon_ak47 */
    {47, "models/weapons2/mp40/mp40.md3"}, /* weapon_psg1 */
    {46, "models/weapons2/mp40/mp40.md3"}, /* weapon_sr8 */
    {51, "models/weapons2/luger/luger.md3"}, /* weapon_m76 */
    {5, "models/weapons2/mp40/mp40.md3"}, /* weapon_generic_th1 */
    {6, "models/weapons2/mp40/mp40.md3"}, /* weapon_generic_th2 */
    {13, "models/weapons2/mp40/mp40.md3"}, /* weapon_generic_th3 */
    {38, "models/weapons2/luger/luger.md3"}, /* weapon_akimboluger */
    {54, "models/weapons2/luger/luger.md3"}, /* weapon_akimbosilencedluger */
    {8, "models/weapons2/thompson/thompson.md3"}, /* weapon_thompson */
    {34, NULL}, /* weapon_dummy */
    {10, "models/weapons2/sten/sten.md3"}, /* weapon_sten */
    {7, "models/weapons2/colt/colt.md3"}, /* weapon_colt */
    {37, "models/weapons2/colt/colt.md3"}, /* weapon_akimbocolt */
    {53, "models/weapons2/colt/colt.md3"}, /* weapon_akimbosilencedcolt */
    {3, "models/weapons2/mp40/mp40.md3"}, /* weapon_mp40 */
    {65, "models/weapons2/panzerfaust/pf.md3"}, /* weapon_panzerfaust */
    {4, "models/weapons2/grenade/grenade.md3"}, /* weapon_grenadelauncher */
    {9, "models/weapons2/grenade/pineapple.md3"}, /* weapon_grenadepineapple */
    {22, "models/multiplayer/smokegrenade/smokegrenade.md3"}, /* weapon_grenadesmoke */
    {16, "models/multiplayer/smokegrenade/smokegrenade.md3"}, /* weapon_smoketrail */
    {19, "models/multiplayer/medpack/medpack.md3"}, /* weapon_medic_heal */
    {15, "models/multiplayer/dynamite/dynamite_3rd.md3"}, /* weapon_dynamite */
    {66, "models/weapons2/flamethrower/flamethrower.md3"}, /* weapon_flamethrower */
    {17, "models/weapons2/grenade/grenade.md3"}, /* weapon_mapmortar */
    {21, "models/multiplayer/pliers/pliers.md3"}, /* weapon_class_special */
    {63, "models/multiplayer/syringe/syringe.md3"}, /* weapon_arty */
    {11, "models/multiplayer/syringe/syringe.md3"}, /* weapon_medic_syringe */
    {61, "models/multiplayer/syringe/syringe.md3"}, /* weapon_medic_adrenaline */
    {12, "models/multiplayer/ammopack/ammopack.md3"}, /* weapon_magicammo */
    {20, ""}, /* weapon_binoculars */
    {32, "models/multiplayer/kar98/kar98_3rd.md3"}, /* weapon_kar43 */
    {58, "models/multiplayer/kar98/kar98_3rd.md3"}, /* weapon_kar43_scope */
    {23, "models/multiplayer/kar98/kar98_3rd.md3"}, /* weapon_kar98Rifle */
    {55, "models/multiplayer/kar98/kar98_3rd.md3"}, /* weapon_gpg40 */
    {56, "models/multiplayer/m1_garand/m1_garand_3rd.md3"}, /* weapon_gpg40_allied */
    {24, "models/multiplayer/m1_garand/m1_garand_3rd.md3"}, /* weapon_M1CarbineRifle */
    {25, "models/multiplayer/m1_garand/m1_garand_3rd.md3"}, /* weapon_garandRifle */
    {57, "models/multiplayer/m1_garand/m1_garand_3rd.md3"}, /* weapon_garandRifleScope */
    {33, "models/weapons2/fg42/fg42.md3"}, /* weapon_fg42 */
    {59, "models/weapons2/fg42/fg42.md3"}, /* weapon_fg42scope */
    {35, "models/multiplayer/mortar/mortar_3rd.md3"}, /* weapon_mortar */
    {60, "models/multiplayer/mortar/mortar_3rd.md3"}, /* weapon_mortar_set */
    {26, "models/multiplayer/landmine/landmine.md3"}, /* weapon_landmine */
    {27, "models/multiplayer/satchel/satchel.md3"}, /* weapon_satchel */
    {28, "models/multiplayer/satchel/radio.md3"}, /* weapon_satchelDetonator */
    {30, "models/multiplayer/smokebomb/smokebomb.md3"}, /* weapon_smokebomb */
    {29, "models/multiplayer/dynamite/dynamite_3rd.md3"}, /* weapon_tripmine */
    {31, "models/multiplayer/mg42/mg42_3rd.md3"}, /* weapon_mobile_mg42 */
    {62, "models/multiplayer/mg42/mg42_3rd.md3"}, /* weapon_mobile_mg42_set */
    {14, "models/weapons2/silencer/silencer.md3"}, /* weapon_silencer */
    {52, "models/weapons2/colt/colt.md3"}, /* weapon_silencedcolt */
};

void TCE_UI_PlayerInfo_SetWeapon(playerInfo_t *pi, int weapon) {
    unsigned int i;
    const char *model = NULL;
    char path[MAX_QPATH];
    pi->currentWeapon = weapon;
    for (;;) {
        pi->realWeapon = weapon;
        pi->weaponModel = pi->barrelModel = pi->flashModel = 0;
        if (!weapon) return;
        /* Original legacy IDs65/66 are not indices into the 64-slot Gear. */
        if (weapon == 65) {
            pi->weaponModel = trap_R_RegisterModel("models/multiplayer/panzerfaust/multi_pf.md3");
            return;
        }
        for (i = 0; i < sizeof(previewItems)/sizeof(previewItems[0]); ++i) {
            if (previewItems[i].weapon == weapon) {
                model = previewItems[i].model;
                pi->weaponModel = trap_R_RegisterModel(model);
                break;
            }
        }
        if (pi->weaponModel) break;
        weapon = weapon == 3 ? 0 : 3;
    }
    Q_strncpyz(path, model, sizeof(path));
    COM_StripExtension(path, path);
    Q_strcat(path, sizeof(path), "_flash.md3");
    pi->flashModel = trap_R_RegisterModel(path);
    if (weapon == 4) {
        MAKERGB(pi->flashDlightColor, 1, .7, .5);
    } else if (weapon == 66) {
        MAKERGB(pi->flashDlightColor, .6, .6, 1);
    } else {
        MAKERGB(pi->flashDlightColor, 1, 1, 1);
    }
}
