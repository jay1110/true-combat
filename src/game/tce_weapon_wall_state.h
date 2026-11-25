/* Mod-only wire lease: stats[15] low byte remains equipment; its previously
 * unused high byte carries wall lowering. ET transmits stats as signed shorts.
 * ET_PLAYER angles2[PITCH] was unused (YAW movement, ROLL lean). No ABI change.
 */
#ifndef TCE_WEAPON_WALL_STATE_H
#define TCE_WEAPON_WALL_STATE_H
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
#endif
