#include "cg_local.h"
#include "../game/tce_bg.h"
#include "tce_weapon_media.h"

tce_weaponInfo_t tce_cg_weapons[TCE_MAX_WEAPONS];

void TCE_CG_RegisterWeapon(int weaponNum,int force) {
    tce_weaponInfo_t *info;
    const char *name;
    char base[160],filename[176];
    /* Original reads CS_MULTI_INFO here but does not use its result. */
    if(weaponNum<=0 || weaponNum>=TCE_MAX_WEAPONS)return;
    info=&tce_cg_weapons[weaponNum];
    if(info->registered && !force)return;
    memset(info,0,sizeof(*info));info->registered=1;
    name=gearDef.weaponFile[weaponNum];
    if(!name[0])return;
    Com_sprintf(base,sizeof(base),"custom/%s/weapons/%s",gearDef.customWeaponGroup,name);
    Com_sprintf(filename,sizeof(filename),"%s.weap",base);
    if(!TCE_CG_RegisterWeaponFromWeaponFile(filename,info)) {
        CG_Printf("^1WARNING: client failed to register media for weapon %i from %s.weap, trying default ...\n",weaponNum,base);
        Com_sprintf(base,sizeof(base),"custom/default/weapons/%s",name);
        Com_sprintf(filename,sizeof(filename),"%s.weap",base);
        /* Original retains any fields written before the custom parse failed. */
        if(!TCE_CG_RegisterWeaponFromWeaponFile(filename,info))
            CG_Printf("^1WARNING: client failed to register media for weapon %i from %s.weap\n",weaponNum,base);
    }
    Com_sprintf(filename,sizeof(filename),"%s.specs",base);
    if(!BG_ParseWeaponDef(filename,&weaponDef[weaponNum]))
        CG_Printf("^1WARNING: client failed to register specs for weapon %i from %s.specs\n",weaponNum,base);
    weaponDef[weaponNum].startClips=gearDef.startClips[weaponNum];
    if(!BG_InitializeWeaponDef(&weaponDef[weaponNum]))
        CG_Printf("^1WARNING: client failed to initialize weapon %i according to weapons/%s.specs\n",weaponNum,name);
}
