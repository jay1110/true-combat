/* Mod-only wire lease: stats[15] low byte remains equipment; its previously
 * unused high byte carries wall lowering. ET transmits stats as signed shorts.
 * ET_PLAYER angles2[PITCH] was unused (YAW movement, ROLL lean). No ABI change.
 */
#ifndef TCE_WEAPON_WALL_STATE_H
#define TCE_WEAPON_WALL_STATE_H
static __inline int TCE_WeaponWallFirearm(int weapon)
{
    return weapon>1 && weapon<64 && weapon!=4 && weapon!=9 && weapon!=15 && weapon!=30 && weapon!=60 && weapon!=62;
}
static __inline int TCE_WeaponWallState(int equipment, float amount)
{
    unsigned int byte;
    if (!(amount > 0.f)) amount = 0.f;
    if (amount > 1.f) amount = 1.f;
    byte = (unsigned int)(amount * 255.f + .5f);
    byte = ((unsigned int)equipment & 255u) | (byte << 8);
    return byte >= 32768u ? (int)byte - 65536 : (int)byte;
}
static __inline float TCE_WeaponWallStatePitch(int equipment)
{
    float blend = ((unsigned int)equipment >> 8 & 255u) * (2.f / 255.f);
    return (blend < 1.f ? blend : 1.f) * 78.f;
}
/* Prone uses the same byte for signed side clearance; 127 is stowed. */
static __inline int TCE_WeaponWallProneState(int equipment,float yaw)
{
    unsigned int v=(unsigned int)(fabsf(yaw)*126.f/70.f+.5f);
    if(v>126u)v=127u;
    if(yaw<0.f && v)v|=128u;
    v=((unsigned int)equipment&255u)|(v<<8);
    return v>=32768u?(int)v-65536:(int)v;
}
static __inline float TCE_WeaponWallPosePitch(int eFlags,int equipment)
{
    unsigned int v=((unsigned int)equipment>>8)&255u;
    if(!(eFlags&EF_PRONE))return TCE_WeaponWallStatePitch(equipment);
    return (v&127u)==127u?78.f:((float)(v&127u)*(70.f/126.f)*(v&128u?-1.f:1.f));
}
static __inline int TCE_WeaponWallProneStowed(int eFlags,float pose)
{
    return (eFlags&EF_PRONE) && ((eFlags&EF_PRONE_MOVING)||fabsf(pose)>70.5f);
}
static __inline float TCE_WeaponWallPoseBlend(int eFlags,float older,float newer,float fraction)
{
    if((eFlags&EF_PRONE) && (fabsf(older)>70.5f || fabsf(newer)>70.5f || older*newer<0.f))return older;
    return older+fraction*(newer-older);
}
static __inline float TCE_WeaponWallPosturePitch(int eFlags, int pmFlags, float pitch)
{
    return pitch==0.f ? 0.f : (!(eFlags & EF_PRONE) && (pmFlags & PMF_DUCKED) ? -pitch : pitch);
}
static __inline float TCE_WeaponWallTorsoPitch(float pitch,float wallPose)
{
    if(pitch>180.f)pitch-=360.f;
    if(pitch< -180.f)pitch+=360.f;
    return pitch*(1.f-fabsf(wallPose)/78.f);
}
static __inline int TCE_WeaponWallProneBlocked(int eFlags, float amount)
{
    return TCE_WeaponWallProneStowed(eFlags,amount);
}
static __inline void TCE_WeaponWallCancelProneFire(playerState_t *ps, usercmd_t *cmd, float amount)
{
    if (!TCE_WeaponWallProneBlocked(ps->eFlags,amount)) return;
    cmd->buttons &= ~BUTTON_ATTACK;ps->eFlags &= ~EF_FIRING;ps->holdable[11]=0;
    if (ps->weaponstate==WEAPON_FIRING || ps->weaponstate==WEAPON_FIRINGALT) {
        ps->weaponstate=WEAPON_READY;ps->weaponDelay=0;
    }
}
#endif
