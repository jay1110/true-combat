#ifndef TCE_PLAYER_ANIMATION_PRECISION_H
#define TCE_PLAYER_ANIMATION_PRECISION_H

/* Distance stores its three vector differences as Float. The original Win32
 * controller then retains the root and both divisions until animSpeedScale is
 * stored. A Float moveSpeed temporary loses a bit that can move a frame by
 * one millisecond. Keep the SDK Distance implementation and public ABI intact.
 * Binary64 intermediates reproduce the normal PC53 runtime arithmetic; this
 * adapter does not claim x87 exception-state or non-default PC-mode parity. */
static __inline float TCE_PlayerAnimationSpeedScale(const float *from,
                                                    const float *to,
                                                    int elapsed, int speed)
{
#if defined(_WIN32) && !defined(Q3_VM)
	volatile float x = to[0] - from[0];
	volatile float y = to[1] - from[1];
	volatile float z = to[2] - from[2];
	volatile double xx = (double)x * x;
	volatile double yy = (double)y * y;
	volatile double zz = (double)z * z;
	volatile double xy = xx + yy;
	volatile double squared = xy + zz;
	volatile double distance = sqrt(squared);
	volatile double seconds = (double)elapsed * 0.001;
	volatile double moveSpeed = distance / seconds;
	return (float)(moveSpeed / (double)speed);
#else
	float moveSpeed = Distance(from, to) / ((float)elapsed / 1000.0);
	return moveSpeed / (float)speed;
#endif
}

/* Both native Win32 controllers divide integer frame times in the retained
 * PC53 register, subtract from Double 1, and only then store binary32. */
static __inline float TCE_PlayerAnimationBacklerp(int elapsed, int duration)
{
#if defined(_WIN32) && !defined(Q3_VM)
	volatile double fraction = (double)elapsed / (double)duration;
	return (float)(1.0 - fraction);
#else
	return 1.0 - (float)elapsed / duration;
#endif
}

#endif
