#ifndef TCE_WEAPON_MEDIA_H
#define TCE_WEAPON_MEDIA_H
#include <stddef.h>
/* Windows TC:E media ABI. Unknown regions remain explicit until recovered. */
typedef struct {
    char tagName[64];
    int model, skin[3];
    int portalScope, noPortalScope, tacView, noTacView, modFlags;
} tce_partModel_t;
typedef struct { int model, skin[3]; } tce_weaponModel_t;
#ifdef CGAMEDLL
/* Identical, checked cgame animation ABI; no incompatible pointer casts. */
typedef animation_t tce_weaponAnimation_t;
#else
typedef struct {
    int mdxFile;
    char name[64];
    int firstFrame, numFrames, loopFrames, frameLerp, initialLerp, moveSpeed;
    int animBlend, duration, nameHash, flags, movetype;
} tce_weaponAnimation_t;
#endif
typedef struct {
    int registered;
    tce_weaponAnimation_t animations[13];
    int handsModel, standModel, droppedAnglesHack;
    tce_weaponModel_t weaponModel[3];
    tce_partModel_t partModels[3][7];
    int flashModel[3], modModels[6];
    vec3_t flashDlightColor;
    int flashSound[4], flashEchoSound[4], lastShotSound[4];
    int weaponIcon, weaponSelectedIcon, unknown_e8c;
    int missileModel, missileAlliedSkin, missileAxisSkin, missileSound;
    int missileTrail; /* semantic callback ID; original stores a code pointer */
    float missileDlight;
    vec3_t missileDlightColor;
    int missileRenderfx;
    vec3_t gunViewOffset, gunViewAimOffset, gunViewAngles;
    float foreShorten;
    int tagsInMain, portalScopeWidth, flashReverbVolume;
    char deployMenuShortName[64], deployMenuDescription[64], deployMenuType[64];
    int ejectBrass; /* semantic callback ID; original stores a code pointer */
    int readySound, firingSound, overheatSound, reloadSound, reloadFastSound;
    int spinupSound, spindownSound, switchSound;
} tce_weaponInfo_t;
typedef char tce_animation_size_check[sizeof(tce_weaponAnimation_t) == 0x70 ? 1 : -1];
typedef char tce_firstframe_offset_check[(offsetof(tce_weaponInfo_t, animations) + offsetof(tce_weaponAnimation_t, firstFrame)) == 0x48 ? 1 : -1];
typedef char tce_media_size_check[sizeof(tce_weaponInfo_t) == 0xfd0 ? 1 : -1];
typedef char tce_part_size_check[sizeof(tce_partModel_t) == 100 ? 1 : -1];
typedef char tce_parts_offset_check[offsetof(tce_weaponInfo_t, partModels) == 0x5f0 ? 1 : -1];
typedef char tce_mod_offset_check[offsetof(tce_weaponInfo_t, modModels) == 0xe30 ? 1 : -1];
enum { TCE_TRAIL_NONE, TCE_TRAIL_GRENADE, TCE_TRAIL_ROCKET, TCE_TRAIL_PYROSMOKE, TCE_TRAIL_DYNAMITE };
enum { TCE_BRASS_NONE, TCE_BRASS_MACHINEGUN, TCE_BRASS_PANZERFAUST };
extern tce_weaponInfo_t tce_cg_weapons[64];
void TCE_CG_Missile(centity_t *cent);
void TCE_CG_RegisterWeapon(int weaponNum, int force);
int TCE_CG_RW_ParseClient(int handle, tce_weaponInfo_t *info);
int TCE_CG_RegisterWeaponFromWeaponFile(const char *filename, tce_weaponInfo_t *info);
int TCE_CG_ParseWeaponConfig(const char *filename, tce_weaponInfo_t *info);
int TCE_CG_RW_ParseError(int handle, const char *format, ...);
int TCE_CG_RW_ParseWeaponLinkPart(int handle, tce_weaponInfo_t *info, int view);
int TCE_CG_RW_ParseWeaponLink(int handle, tce_weaponInfo_t *info, int view);
int TCE_CG_RW_ParseViewType(int handle, tce_weaponInfo_t *info, int view);
int TCE_CG_RW_ParseModModel(int handle, tce_weaponInfo_t *info);
#endif
