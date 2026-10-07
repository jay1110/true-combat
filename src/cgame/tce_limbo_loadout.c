/* Windows 30043590/30043790; class table30095480/30095a68.
 * All identities share the candidate order; gear supplies team/role gating. */
#include "../game/q_shared.h"
#include "../game/tce_bg.h"
#include "tce_limbo_loadout.h"
static const int primaryWeapons[24] = {
    10,3,8,45,41,33,42,5,44,43,50,24,48,49,47,51,46,23,32,25,6,13,0,0
};
static const int sidearms[24] = {2,39,52,14,40,7,38,37,54,53};
static int available(const tce_limboLoadout_t *s,int weapon) {
    int role=BG_WolfClassToTCE(s->playerClass);
    int rating=role==BG_WolfClassToTCE(s->scoreClass)?s->rating:s->previousRating;
    return BG_WeaponIsAvailable(weapon,gearDef.requiredSkill[weapon][role],
                                gearDef.team[weapon],rating+20,s->team);
}
int TCE_LimboWeaponCount(const tce_limboLoadout_t *s,int slot) {
    const int *weapons=slot==1?primaryWeapons:sidearms;
    int i,count=0;
    if(slot==2)return s->gametype!=7 && BG_WolfClassToTCE(s->playerClass)==1?2:1;
    for(i=0;i<24 && weapons[i];++i)
        if((slot==1 || BG_SidearmAvailableForPrimary(weapons[i],s->primary)) &&
           available(s,weapons[i]))++count;
    return count;
}
int TCE_LimboWeaponForNumber(const tce_limboLoadout_t *s,int number,int slot) {
    const int *weapons=slot==1?primaryWeapons:sidearms;
    int i,count=0;
    if(s->team==3 || number<0)return 0;
    if(slot==2)return number?36:BG_GrenadeSelectionForPrimary(s->primary);
    if(slot!=1) {
        if(s->heavySkill>=4 && s->playerClass==0 && number==(s->lightSkill>=4?2:1))
            return s->team==1?3:8;
        if(s->lightSkill>=4 && number>0)
            return s->playerClass==4?(s->team==1?54:53):(s->team==1?38:37);
    }
    for(i=0;i<24;++i)if(available(s,weapons[i])) {
        if(count==number)return weapons[i];
        ++count;
    }
    if(slot!=1 && number==0)
        return s->playerClass==4?(s->team==1?14:52):(s->team==1?2:7);
    return 0;
}
