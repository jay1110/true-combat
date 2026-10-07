#include "tce_weapon_cycle.h"
/* Windows3004e6c0. Caller guarantees a nonnegative protocol selection. */
void TCE_CG_DrawSelectedWeapon(const tce_weaponCycle_t *s,const float *rect,
    const float *color,const tce_weaponCycleServices_t *api) {
    float ink[4]={color[0],color[1],color[2],1};
    if(s->time-s->weaponSelectTime<1501 && s->weaponSelect<55) {
        float x=(rect[0]+82)-12-(float)api->width(s->weaponSelect);
        float y=((((rect[1]-18)+12)-4)-12)-12;
        api->text(x,y,ink,s->weaponSelect);
    }
}
/* Windows30075120/300751c0. direction is +1 (next) or -1 (previous). */
void TCE_CG_CycleWeaponCommand(tce_weaponCycle_t *s,int direction) {
    if(!s->hasSnapshot || (s->snapshotFlags & 0x1000000) ||
       (s->snapshotPmFlags & 0x1000) || s->time-s->weaponSelectTime<1600 ||
       s->snapshotWeaponState!=0 || s->predictedWeaponState!=0 ||
       s->snapshotWeapon!=s->predictedWeapon) return;
    if(s->time-s->cycleTime>1500) s->cycleOffset=0;
    s->cycleTime=s->time;
    s->cycleOffset+=direction;
    s->weaponSelect=56;
}

/* Windows 3004e7b0. Original traversal counts every selectable entry, retaining
 * only the last entry in each bank. Do not compact bank positions or replace
 * this with a generic list: even sparse loadouts use these exact rules. */
void TCE_CG_DrawWeaponCycle(tce_weaponCycle_t *s,const int banks[10][22],
    const float *rect,const float *color,float hudAlpha,
    const tce_weaponCycleServices_t *api) {
    int slots[6]={-1,-1,-1,-1,-1,-1};
    int bank,col,count=0,index=0,drawIndex=0,weapon;
    float ink[4]={color[0],color[1],color[2],hudAlpha};
    float shadow[4]={0,0,0,hudAlpha};
    if(s->time-s->cycleTime>=2001 || (s->snapshotPmFlags&0x1000) ||
       s->time-s->weaponSelectTime<1600 || s->snapshotWeaponState!=0 ||
       s->predictedWeaponState!=0 || s->snapshotWeapon!=s->predictedWeapon) {
        if(s->weaponSelect==56)s->weaponSelect=s->predictedWeapon;
        return;
    }
    for(col=0;col<22;++col)for(bank=0;bank<6;++bank) {
        weapon=banks[bank+1][col];
        if(api->selectable(weapon)) {
            slots[bank]=weapon;++count;
            if(weapon==s->predictedWeapon)index=bank;
        }
    }
    index+=s->cycleOffset;
    /* The x87 code divides index+1 on the negative branch. Empty banks draw
     * nothing; avoid its invalid FP division without changing observable state. */
    if(count) {
        if(index<0)index+=(1-(index+1)/count)*count;
        else index-=(index/count)*count;
    }
    for(bank=0;bank<6;++bank) {
        float width,x,y;
        weapon=slots[bank];
        if(weapon<=0 || weapon>=64)continue;
        width=(float)api->width(weapon);
        x=(rect[0]+82)-12-width;
        y=((((rect[1]-18)+12)-4)-12)-12-(80-bank*16);
        if(index==drawIndex) {
            ink[3]=1;s->cycleSelected=weapon;
            api->border(x-2+1,y-26,width+6,14,shadow);
            api->border(x-2,y-27,width+6,14,ink);
        } else ink[3]=.5f;
        api->text(x,y-16,ink,weapon);
        ++drawIndex;
    }
    if((s->buttons&1) && s->cycleDelay<s->time-s->weaponSelectTime) {
        s->weaponSelectTime=s->time;s->weaponSelect=s->predictedWeapon;
        s->cycleTime=0;
        if(s->predictedWeapon!=s->cycleSelected)
            api->finish(s->predictedWeapon,s->cycleSelected);
    }
}
