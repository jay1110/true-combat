#include "tce_reload.h"
int TCE_PM_IdleAnimForWeapon(int weapon) {
    switch(weapon) {
    case 28: case 55: case 56: case 60: case 61: case 62: return 1;
    default: return 0;
    }
}
int TCE_PM_StartWeaponAnim(int animation, int current, int moveType, int timer, int commandWeapon) {
    if (moveType >= 3 || timer > 0 || !commandWeapon) return current;
    return (~current & 512) | animation;
}
int TCE_PM_ContinueWeaponAnim(int animation, int current, int moveType, int timer, int commandWeapon) {
    if (!commandWeapon || (current & ~512) == animation || timer > 0) return current;
    return TCE_PM_StartWeaponAnim(animation,current,moveType,timer,commandWeapon);
}
static void clear(tce_reloadEffects_t *effects) {
    effects->bodyAnimation = effects->weaponAnimation = effects->event = -1;
    effects->missingItem = 0;
}
void TCE_PM_BeginWeaponReload(int weapon, tce_reloadState_t *s,
    const tce_weaponDef_t *defs, tce_reloadEffects_t *effects) {
    int ai, empty;
    clear(effects);
    if (s->state != 0 && s->state != 7) return;
    if ((weapon == 31 || weapon == 62) && s->clip[31]) return;
    if (!((weapon > 0 && weapon < 16) || (weapon > 22 && weapon < 64))) return;
    ai = TCE_BG_FindAmmoForWeapon(weapon);
    /* Original BG_FindItemForWeapon has no item for36. All remaining allowed
     * IDs have nonzero ammo indices, preserved in the recovered item table. */
    if (!ai) { effects->missingItem = 1; return; }
    if (s->clip[ai] >= defs[weapon].maxclip) return;
    if (weapon != 4 && weapon != 9 && weapon != 15 && weapon != 30)
        effects->bodyAnimation = s->prone ? 33 : 11;
    if (weapon != 35 && weapon != 60) {
        empty = TCE_PM_WeaponClipEmpty(weapon, s->noClips, s->ammo, s->clip);
        effects->weaponAnimation = empty ? s->emptyAnimation : s->nonemptyAnimation;
    }
    if (!s->state) s->time += defs[weapon].reloadTime;
    else if (s->time < defs[weapon].reloadTime) s->time = defs[weapon].reloadTime;
    if (defs[weapon].singleReload) s->state = 10;
    else { s->state = 9; effects->event = 35; }
}
void TCE_PM_ReloadSingleRound(int weapon, tce_reloadState_t *s,
    const tce_weaponDef_t *defs, tce_reloadEffects_t *effects) {
    clear(effects);
    if ((s->state != 9 && s->state != 10) || !defs[weapon].singleReload) return;
    if (weapon != 4 && weapon != 9 && weapon != 15) effects->bodyAnimation = 11;
    effects->weaponAnimation = 8;
    s->time += defs[weapon].maxHeat; /* Original offset0xf0: reused reload-stage duration. */
    s->state = 9;
    effects->event = 130;
}
static void closeReload(tce_reloadState_t *s, const tce_weaponDef_t *def,
    tce_reloadEffects_t *effects, int event) {
    effects->bodyAnimation=11; effects->weaponAnimation=9; effects->event=event;
    s->time+=def->coolRate; /* Original0xf4 is the closing-stage duration. */
    s->state=11;
}
void TCE_PM_CheckForReload(int weapon, tce_reloadState_t *s,
    const tce_weaponDef_t *defs, const int *legacyClipLimit, const int *alternates,
    tce_reloadEffects_t *effects, tce_reloadAction_t *action) {
    int ci=TCE_BG_FindClipForWeapon(weapon), ai=TCE_BG_FindAmmoForWeapon(weapon);
    clear(effects); action->action=TCE_RELOAD_NONE; action->alternate=0;
    if (defs[weapon].singleReload && s->state==9 && s->time<=0 &&
        (!s->ammo[ci] || s->clip[ci]>=defs[weapon].maxclip || !s->reloadRequested))
        closeReload(s,&defs[weapon],effects,131);
    if (defs[weapon].bolt && s->state==9 && s->time<=0)
        closeReload(s,&defs[weapon],effects,133);
    if (s->noClips || weapon==55 || weapon==56) return;
    if ((s->state>=1 && s->state<=6) || s->state==9) {
        if (defs[weapon].singleReload && s->time<=0 && s->reloadRequested &&
            s->ammo[ai] && s->clip[ci]<defs[weapon].maxclip)
            action->action=TCE_RELOAD_ROUND;
        return;
    }
    if (s->state==10) {
        if (defs[weapon].singleReload && s->time<=0) action->action=TCE_RELOAD_ROUND;
        return;
    }
    if (weapon>=57 && weapon<=59) {
        if (s->reloadRequested && s->ammo[ai] && s->clip[ci]<legacyClipLimit[weapon]) {
            action->action=TCE_RELOAD_CHANGE;action->alternate=alternates[weapon];
        }
        return;
    }
    if (s->time>0 || !s->reloadRequested || !s->ammo[ai] || s->attack || s->attackHeld) return;
    if (TCE_BG_IsAkimboWeapon(weapon)) {
        int side=TCE_BG_FindClipForWeapon(TCE_BG_AkimboSidearm(weapon));
        if (s->clip[side]<defs[side].maxclip) { action->action=TCE_RELOAD_BEGIN; return; }
    }
    if (s->clip[ci]<defs[weapon].maxclip) action->action=TCE_RELOAD_BEGIN;
}
void TCE_PM_FinishWeaponReload(int weapon, tce_reloadState_t *s,
    const tce_weaponDef_t *defs, int *ammo, int *clip, tce_reloadEffects_t *effects) {
    clear(effects);
    if (!defs[weapon].singleReload && s->state!=12) TCE_PM_ReloadClip(weapon,ammo,clip,defs);
    s->state=0; effects->weaponAnimation=s->idleAnimation;
}
