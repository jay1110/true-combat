#ifndef TCE_CAMERA_WAVE_H
#define TCE_CAMERA_WAVE_H

#include <float.h>
#include <math.h>

/* FSIN/FCOS leave finite out-of-range operands unchanged. */
static __inline long double TCE_CameraShakeWave(long double angle, int cosine)
{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	if ((angle >= 9223372036854775808.0L || angle <= -9223372036854775808.0L) &&
	    angle <= LDBL_MAX && angle >= -LDBL_MAX)
	{
		return angle;
	}
#endif
	return cosine ? cosl(angle) : sinl(angle);
}

#endif
