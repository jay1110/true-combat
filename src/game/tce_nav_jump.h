#ifndef TCE_NAV_JUMP_H
#define TCE_NAV_JUMP_H
/* Explicit low-speed forward jump: PM_CheckJump does not apply .707 when
 * total velocity is below50 and forwardmove is nonzero. Never use this
 * impulse for an ordinary running jump. No player state is changed here. */
static __inline float TCE_NavStandingImpulse(int enlarged)
{
	return 270.f * (enlarged?1.25f:1.f);
}
static __inline int TCE_NavStandingSpeed(float x, float y, float z)
{
	float horizontal = x * x + y * y;
	return horizontal >= 36.f * 36.f && horizontal <= 44.f * 44.f && horizontal + z * z < 50.f * 50.f;
}
#define TCE_NAV_STAND_PRIME_BUTTON 62
#define TCE_NAV_STAND_LAUNCH_BUTTON 63
#endif
