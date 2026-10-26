/* Windows cgame:3006e870. Preserve original finite-input behavior, including
 * zero-filled remaining animation fields when the body ends early.
 */
#include "cg_local.h"
#include "tce_weapon_media.h"
#include "tce_weapon_fps_precision.h"
#include <float.h>
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
#include "../game/tce_numeric.h"
#endif

#if defined(__linux__) && !defined(Q3_VM)
#include <errno.h>

/* The original ELF inlines atoi as 32-bit strtol(base10, grouping0).
 * Keep saturation and errno even when the host long is wider than int. */
static int TCE_WeaponConfigInteger(const char *token)
{
	long value = strtol(token, NULL, 10);
	if (value > INT_MAX)
	{
		errno = ERANGE;
		return INT_MAX;
	}
	if (value < INT_MIN)
	{
		errno = ERANGE;
		return INT_MIN;
	}
	return (int)value;
}
#elif defined(_WIN32) && !defined(Q3_VM)
#include <ctype.h>

/* The bundled Windows atoi/atol accumulates in 32-bit registers.
 * Overflow wraps; it neither saturates nor sets errno. */
static int TCE_WeaponConfigInteger(const char *token)
{
	const unsigned char *cursor = (const unsigned char *)token;
	unsigned value = 0;
	int negative;

	while (isspace(*cursor))
	{
		++cursor;
	}
	negative = *cursor == '-';
	if (*cursor == '-' || *cursor == '+')
	{
		++cursor;
	}
	while (isdigit(*cursor))
	{
		value = value * 10u + (unsigned)*cursor - (unsigned)'0';
		++cursor;
	}
	if (negative)
	{
		value = 0u - value;
	}
	return TCE_Int32FromUnsigned(value);
}
#else
#define TCE_WeaponConfigInteger atoi
#endif

int TCE_CG_ParseWeaponConfig(const char *filename, tce_weaponInfo_t *info)
{
	char         text[20000], *cursor, *previous, *token;
	fileHandle_t file;
	int          length, i, newFormat = 0;
	length = trap_FS_FOpenFile(filename, &file, FS_READ);
	if (length <= 0)
	{
		return 0;
	}
	if (length >= sizeof(text) - 1)
	{
		/* Original returns without closing this oversized source. */
		CG_Printf("File %s too long\n", filename);
		return 0;
	}
	trap_FS_Read(text, length, file); text[length] = 0;
	trap_FS_FCloseFile(file);
	cursor = text;
	for (;;)
	{
		previous = cursor; token = COM_Parse(&cursor);
		if (!token[0])
		{
			/* Original loops forever printing empty unknown tokens here.
			 * End the malformed header explicitly; no valid file is affected. */
			CG_Printf("Error parsing weapon animation file: %s", filename);
			return 0;
		}
		if (!Q_stricmp(token, "newfmt"))
		{
			newFormat = 1;
		}
		else if (token[0] >= '0' && token[0] <= '9')
		{
			cursor = previous; break;
		}
		else
		{
			CG_Printf("unknown token in weapon cfg '%s' is %s\n", token, filename);
		}
	}
	for (i = 0; i < 13; ++i)
	{
		tce_weaponAnimation_t *animation = &info->animations[i];
		double                fps;
		animation->firstFrame = TCE_WeaponConfigInteger(COM_Parse(&cursor));
		animation->numFrames  = TCE_WeaponConfigInteger(COM_Parse(&cursor));
		fps                   = atof(COM_Parse(&cursor));
#if defined(__linux__) && (defined(__i386__) || \
    (!defined(Q3_VM) && LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384))
		{
			/* The original stores FPS as binary32 before x87 arithmetic. */
			volatile float storedFps = (float)fps;
			long double    nativeFps = (long double)storedFps;
			long double    duration;
			volatile long double reciprocal;
			int            frameLerp;

			/* FCOM's equal flag is also set for an unordered comparison. */
			if (nativeFps == 0.0L || nativeFps != nativeFps)
			{
				nativeFps = 1.0L;
			}
			reciprocal = 1.0L / nativeFps;
			duration = reciprocal * 1000.0L;
			/* Masked FISTP returns INT_MIN if truncation is out of range. */
			if (duration <= -2147483649.0L || duration >= 2147483648.0L)
			{
				frameLerp = -2147483647 - 1;
			}
			else
			{
				frameLerp = (int)duration;
			}
			animation->frameLerp = animation->initialLerp = frameLerp;
		}
#elif defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		if (fps == 0 || fps != fps)
		{
			fps = 1;
		}
		if (!TCE_WeaponFrameLerp(fps, &animation->frameLerp))
		{
			animation->frameLerp = (int)(1000.0 / fps);
		}
		animation->initialLerp = animation->frameLerp;
#else
		if (fps == 0 || fps != fps)
		{
			fps = 1;
		}
		/* Windows keeps atof's double in x87; no intermediate float cast. */
#if defined(_WIN32) && !defined(Q3_VM)
		animation->frameLerp = TCE_TruncateNativeInteger(1000.0L / (long double)fps);
		animation->initialLerp = animation->frameLerp;
#else
		animation->frameLerp = animation->initialLerp = (int)(1000.0 / fps);
#endif
#endif
		animation->loopFrames = TCE_WeaponConfigInteger(COM_Parse(&cursor));
		if (animation->loopFrames > animation->numFrames)
		{
			animation->loopFrames = animation->numFrames;
		}
		else if (animation->loopFrames < 0)
		{
			animation->loopFrames = 0;
		}
		animation->moveSpeed = 0;
		if (newFormat)
		{
			animation->moveSpeed = TCE_WeaponConfigInteger(COM_Parse(&cursor));
			if (TCE_WeaponConfigInteger(COM_Parse(&cursor)))
			{
				animation->moveSpeed |= 0x80;
			}
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
			animation->moveSpeed = TCE_Int32FromUnsigned((unsigned)animation->moveSpeed |
			                                           ((unsigned)TCE_WeaponConfigInteger(COM_Parse(&cursor)) << 8));
#else
			animation->moveSpeed |= (unsigned int)TCE_WeaponConfigInteger(COM_Parse(&cursor)) << 8;
#endif
		}
	}
	return 1;
}
