/* Shared prediction/server wall clearance. No client setting or protocol extension. */
#ifndef TCE_WEAPON_WALL_H
#define TCE_WEAPON_WALL_H
static float TCE_WeaponWallFraction(const playerState_t *ps,
    void (*trace)(trace_t *, const vec3_t, const vec3_t, const vec3_t,
                  const vec3_t, int, int))
{
    trace_t hit;
    vec3_t start, end, forward, right;
    vec3_t mins = { -2, -2, -2 }, maxs = { 2, 2, 2 };
    if (ps->weapon <= 0 || ps->weapon >= 64 || ps->weapon == 60 || ps->weapon == 62)
        return 1.f;
    AngleVectors(ps->viewangles, forward, right, NULL);
    VectorCopy(ps->origin, start);
    start[2] += ps->viewheight;
    VectorMA(start, ps->leanf, right, start);
    VectorMA(start, 48.f, forward, end);
    trace(&hit, start, mins, maxs, end, ps->clientNum, MASK_SOLID);
    return hit.startsolid || hit.allsolid ? 0.f : hit.fraction;
}
static float TCE_WeaponWallPitch(int enabled, int weapon, float amount)
{
    if (!enabled || weapon <= 0 || weapon >= 64 || weapon == 60 || weapon == 62) return 0.f;
    amount *= 2.f;
    if (amount > 1.f) amount = 1.f;
    if (!(amount > 0.f)) amount = 0.f;
    return 78.f * amount;
}

/* Blend toward a downward low-ready direction, including when looking down;
 * adding a fixed angle could turn a floor-obstructed shot backwards. */
static float TCE_WeaponWallAimPitch(float basePitch, float wallPitch)
{
    if (!(wallPitch>0.f)) return basePitch;
    if (basePitch > 180.f) basePitch -= 360.f;
    if (basePitch < -180.f) basePitch += 360.f;
    return basePitch + (78.f-basePitch)*(wallPitch/78.f);
}

/* Bounded command-time approach: the low-ready threshold takes at least 250 ms. */
static float TCE_WeaponWallApproach(float current, float target, int msec)
{
    float step;
    if (msec < 0) msec = 0;
    if (msec > 50) msec = 50;
    step = msec * .002f;
    if (current < target) return current + step < target ? current + step : target;
    return current - step > target ? current - step : target;
}
#endif
