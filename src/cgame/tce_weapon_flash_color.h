#ifndef TCE_WEAPON_FLASH_COLOR_H
#define TCE_WEAPON_FLASH_COLOR_H

#include "../game/tce_numeric.h"
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "../game/tce_trigger_precision.h"
#endif

/* The two originals consume different integer widths after multiplying the
 * exposure by 255. Preserve the numeric byte; FP status remains separate. */
static __inline unsigned char TCE_WeaponFlashColor(float exposure)
{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	unsigned control;
	tceTriggerSum_t product;

	__control87_2(0, 0, &control, NULL);
	if (TCE_FiniteExceptionsMasked(control))
	{
		if (!TCE_TriggerProduct(&product, exposure, 255.0f, control))
		{
			/* Masked NaN/infinity converts to integer-indefinite, low byte 0. */
			return 0;
		}
		return (unsigned char)TCE_FixedSigned64Low32(&product);
	}
	/* Retain the prior floating path when original traps may be enabled. */
	return (unsigned char)(int)((double)exposure * 255.0);
#elif defined(__linux__) && !defined(Q3_VM)
	volatile long double product = (long double)exposure * 255.0L;

	/* Linux temporarily selects truncation, then uses FISTP signed16.
	 * Values between -32769 and -32768 still truncate to valid -32768. */
	if (!(product > -32769.0L && product < 32768.0L))
	{
		return 0;
	}
	return (unsigned char)(int)product;
#elif defined(_WIN32) && !defined(Q3_VM)
	return (unsigned char)TCE_TruncateNativeInteger((long double)exposure * 255.0L);
#else
	return (unsigned char)(int)((double)exposure * 255.0);
#endif
}

#endif
