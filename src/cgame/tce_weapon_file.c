/* TC:E .weap parser. Effect selectors are stored as semantic IDs until the
 * effect implementations are recovered and the TC:E renderer is integrated. */
#include "cg_local.h"
#include "tce_weapon_media.h"

typedef enum { MODEL, SKIN, SHADER, SOUND, VECTOR, FLOAT_VALUE, INT_VALUE, TEXT } fieldKind_t;
typedef struct { const char *key; fieldKind_t kind; size_t offset; const char *error; } mediaField_t;
#define FIELD(key, kind, member, error) { key, kind, offsetof(tce_weaponInfo_t, member), error }
#define FILE_FIELD(key, kind, member) FIELD(key, kind, member, "expected " key " filename")
static const mediaField_t fields[] = {
    FILE_FIELD("standModel", MODEL, standModel),
    {"pickupModel", MODEL, offsetof(tce_weaponInfo_t,weaponModel)+2*sizeof(tce_weaponModel_t), "expected pickupModel filename"},
    FILE_FIELD("handsModel", MODEL, handsModel),
    FIELD("flashDlightColor", VECTOR, flashDlightColor, "expected flashDlightColor as r g b"),
    FILE_FIELD("readySound", SOUND, readySound), FILE_FIELD("firingSound", SOUND, firingSound),
    FILE_FIELD("overheatSound", SOUND, overheatSound), FILE_FIELD("reloadSound", SOUND, reloadSound),
    FILE_FIELD("reloadFastSound", SOUND, reloadFastSound), FILE_FIELD("spinupSound", SOUND, spinupSound),
    FILE_FIELD("spindownSound", SOUND, spindownSound), FILE_FIELD("switchSound", SOUND, switchSound),
    FILE_FIELD("weaponIcon", SHADER, weaponIcon), FILE_FIELD("weaponSelectedIcon", SHADER, weaponSelectedIcon),
    FILE_FIELD("missileModel", MODEL, missileModel), FILE_FIELD("missileSound", SOUND, missileSound),
    FIELD("missileAlliedSkin", SKIN, missileAlliedSkin, "expected skin filename"),
    FIELD("missileAxisSkin", SKIN, missileAxisSkin, "expected skin filename"),
    FIELD("missileDlight", FLOAT_VALUE, missileDlight, "expected missileDlight value"),
    FIELD("missileDlightColor", VECTOR, missileDlightColor, "expected missileDlightColor as r g b"),
    FIELD("gunViewOffset", VECTOR, gunViewOffset, "expected gunViewOffset as x y z"),
    FIELD("gunViewAimOffset", VECTOR, gunViewAimOffset, "expected gunViewAimOffset as x y z"),
    FIELD("gunViewAngles", VECTOR, gunViewAngles, "expected gunViewAngles as PITCH YAW ROLL"),
    FIELD("foreShorten", FLOAT_VALUE, foreShorten, "expected foreShorten value"),
    FIELD("tagsInMain", INT_VALUE, tagsInMain, "expected tagInMain value"),
    FIELD("flashReverbVolume", INT_VALUE, flashReverbVolume, "expected flashReverbVolume value"),
    FIELD("portalScopeWidth", INT_VALUE, portalScopeWidth, "expected portalScopeWidth value"),
    FIELD("deployMenuShortName", TEXT, deployMenuShortName, "deployMenuShortName"),
    FIELD("deployMenuDescription", TEXT, deployMenuDescription, "deployMenuDescription"),
    FIELD("deployMenuType", TEXT, deployMenuType, "deployMenuType")
};

int TCE_CG_RW_ParseClient(int handle, tce_weaponInfo_t *info) {
    pc_token_t token;
    char name[MAX_QPATH];
    size_t i;
    if (!trap_PC_ReadToken(handle,&token) || Q_stricmp(token.string,"{"))
        return TCE_CG_RW_ParseError(handle,"expected '{'");
    while (trap_PC_ReadToken(handle,&token) && token.string[0]!='}') {
        for(i=0;i<sizeof(fields)/sizeof(fields[0]);++i) if(!Q_stricmp(token.string,fields[i].key)) break;
        if(i<sizeof(fields)/sizeof(fields[0])) {
            const mediaField_t *field=&fields[i];
            void *out=(char *)info+field->offset;
            int ok=1;
            switch(field->kind) {
            case VECTOR: ok=PC_Vec_Parse(handle,(vec3_t *)out); break;
            case FLOAT_VALUE: ok=PC_Float_Parse(handle,(float *)out); break;
            case INT_VALUE: ok=PC_Int_Parse(handle,(int *)out); break;
            default:
                ok=PC_String_ParseNoAlloc(handle,name,sizeof(name));
                if(ok) switch(field->kind) {
                case MODEL: *(int *)out=trap_R_RegisterModel(name); break;
                case SKIN: *(int *)out=trap_R_RegisterSkin(name); break;
                case SHADER: *(int *)out=trap_R_RegisterShader(name); break;
                case SOUND: *(int *)out=trap_S_RegisterSound(name,qfalse); break;
                case TEXT: Q_strncpyz(out,name,64); break;
                default: break;
                }
            }
            if(!ok)return TCE_CG_RW_ParseError(handle,"%s",field->error);
        } else if(!Q_stricmp(token.string,"droppedAnglesHack")) info->droppedAnglesHack=1;
        else if(!Q_stricmp(token.string,"pickupSound") || !Q_stricmp(token.string,"weaponConfig")) {
            int config=!Q_stricmp(token.string,"weaponConfig");
            if(!PC_String_ParseNoAlloc(handle,name,sizeof(name)))
                return TCE_CG_RW_ParseError(handle,config?"expected weaponConfig filename":"expected pickupSound filename");
            /* Original ignores pickupSound and the config parser's return value. */
            if(config)TCE_CG_ParseWeaponConfig(name,info);
        } else if(!Q_stricmp(token.string,"flashSound") || !Q_stricmp(token.string,"flashEchoSound") || !Q_stricmp(token.string,"lastShotSound")) {
            int *sounds;const char *warning;int slot;
            if(!PC_String_ParseNoAlloc(handle,name,sizeof(name)))
                return TCE_CG_RW_ParseError(handle,"expected %s filename",!Q_stricmp(token.string,"flashSound")?"flashSound":!Q_stricmp(token.string,"flashEchoSound")?"flashEchoSound":"lastShotSound");
            if(!Q_stricmp(token.string,"flashSound")) {
                sounds=info->flashSound;warning="^3WARNING: only up to 4 flashSounds supported per weapon\n";
            } else if(!Q_stricmp(token.string,"flashEchoSound")) {
                sounds=info->flashEchoSound;warning="^3WARNING: only up to 4 flashEchoSounds supported per weapon\n";
            } else {sounds=info->lastShotSound;warning="^3WARNING: only up to 4 lastShotSound supported per weapon\n";}
            for(slot=0;slot<4;++slot)if(!sounds[slot]) {sounds[slot]=trap_S_RegisterSound(name,qfalse);break;}
            if(slot==4)CG_Printf("%s",warning);
        } else if(!Q_stricmp(token.string,"missileTrailFunc") || !Q_stricmp(token.string,"ejectBrassFunc")) {
            int trail=!Q_stricmp(token.string,"missileTrailFunc");
            if(!PC_String_ParseNoAlloc(handle,name,sizeof(name)))
                return TCE_CG_RW_ParseError(handle,trail?"expected missileTrailFunc":"expected ejectBrassFunc");
            if(trail) {
                if(!Q_stricmp(name,"GrenadeTrail"))info->missileTrail=TCE_TRAIL_GRENADE;
                else if(!Q_stricmp(name,"RocketTrail"))info->missileTrail=TCE_TRAIL_ROCKET;
                else if(!Q_stricmp(name,"PyroSmokeTrail"))info->missileTrail=TCE_TRAIL_PYROSMOKE;
                else if(!Q_stricmp(name,"DynamiteTrail"))info->missileTrail=TCE_TRAIL_DYNAMITE;
            } else {
                if(!Q_stricmp(name,"MachineGunEjectBrass"))info->ejectBrass=TCE_BRASS_MACHINEGUN;
                else if(!Q_stricmp(name,"PanzerFaustEjectBrass"))info->ejectBrass=TCE_BRASS_PANZERFAUST;
            }
        } else if(!Q_stricmp(token.string,"modModel")) {
            if(!TCE_CG_RW_ParseModModel(handle,info))return 0;
        } else if(!Q_stricmp(token.string,"firstPerson") || !Q_stricmp(token.string,"thirdPerson")) {
            if(!TCE_CG_RW_ParseViewType(handle,info,!Q_stricmp(token.string,"firstPerson")))return 0;
        } else return TCE_CG_RW_ParseError(handle,"unknown token '%s'",token.string);
    }
    return 1;
}

int TCE_CG_RegisterWeaponFromWeaponFile(const char *filename,tce_weaponInfo_t *info) {
    pc_token_t token;
    int handle=trap_PC_LoadSource(filename);
    if(!handle)return 0;
    if(!trap_PC_ReadToken(handle,&token) || Q_stricmp(token.string,"weaponDef"))
        return TCE_CG_RW_ParseError(handle,"expected 'weaponDef'");
    if(!trap_PC_ReadToken(handle,&token) || Q_stricmp(token.string,"{"))
        return TCE_CG_RW_ParseError(handle,"expected '{'");
    while(trap_PC_ReadToken(handle,&token) && token.string[0]!='}') {
        if(Q_stricmp(token.string,"client"))return TCE_CG_RW_ParseError(handle,"unknown token '%s'",token.string);
        if(!TCE_CG_RW_ParseClient(handle,info))return 0;
    }
    trap_PC_FreeSource(handle);
    return 1;
}
