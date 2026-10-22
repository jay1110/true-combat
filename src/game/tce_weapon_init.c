/* TC:E Windows cgame 0x30006a50. Field names not yet proven remain offsets.
 * Characteristic-table values are recovered game data, not executable code.
 */
#include "q_shared.h"
#include "tce_bg.h"
#include "tce_weapon_characteristics.h"

/* Original shared, writable characteristic data: 30 rows of 28 int32 fields. */
int weapCharTable[30][28] =
{
	{ 0,   0, 0,   0,  0,  0,    50,  0,    0,   0,    0,  0,   0,   0,    0,    0,    0,    0,    0,   0,   0,   0,   0,    0,   0,   0,   0,    0 }, /* 0 */
	{ 999, 0, 999, 0,  0,  0,    50,  500,  0,   0,    6,  0,   0,   0,    0,    0,    0,    0,    0,   0,   0,   0,   0,    0,   0,   0,   0,    0 }, /* 1 */
	{ 45,  1, 15,  45, 15, 2200, 0,   100,  0,   0,    8,  699, 400, 800,  200,  1000, 1000, 400,  200, 150, 54,  40,  483,  115, 556, 250, 1500, 0 }, /* 2 */
	{ 45,  1, 15,  45, 15, 2200, 0,   100,  0,   0,    8,  699, 400, 800,  200,  1000, 1000, 400,  200, 150, 54,  40,  483,  165, 576, 250, 1500, 0 }, /* 3 */
	{ 45,  1, 15,  45, 15, 2200, 0,   100,  0,   0,    8,  699, 400, 800,  200,  1000, 1000, 400,  200, 150, 54,  40,  483,  185, 610, 200, 1500, 0 }, /* 4 */
	{ 21,  1, 7,   21, 7,  2200, 0,   200,  0,   0,    8,  799, 600, 800,  300,  1000, 1000, 600,  700, 500, 77,  40,  1725, 215, 556, 200, 1500, 0 }, /* 5 */
	{ 45,  1, 15,  45, 15, 2700, 0,   100,  0,   0,    8,  699, 400, 1600, 200,  1000, 1000, 400,  200, 150, 54,  40,  483,  115, 556, 250, 1500, 1 }, /* 6 */
	{ 45,  1, 15,  45, 15, 2700, 0,   100,  0,   0,    8,  699, 400, 1600, 200,  1000, 1000, 400,  200, 150, 54,  40,  483,  165, 576, 250, 1500, 1 }, /* 7 */
	{ 45,  1, 15,  45, 15, 2700, 0,   100,  0,   0,    8,  699, 400, 1600, 200,  1000, 1000, 400,  200, 150, 54,  40,  483,  185, 610, 250, 1500, 1 }, /* 8 */
	{ 45,  1, 30,  45, 30, 2200, 0,   50,   0,   0,    9,  699, 400, 800,  500,  1000, 1000, 200,  150, 100, 40,  50,  483,  115, 556, 250, 1500, 2 }, /* 9 */
	{ 60,  1, 30,  60, 30, 2200, 0,   50,   0,   0,    9,  699, 400, 800,  500,  1000, 1000, 200,  150, 100, 40,  50,  483,  185, 610, 200, 1500, 2 }, /* 10 */
	{ 90,  1, 30,  90, 30, 2300, 0,   75,   0,   0,    9,  500, 300, 200,  400,  1000, 1000, 200,  150, 100, 45,  50,  483,  115, 556, 250, 2500, 3 }, /* 11 */
	{ 90,  1, 30,  90, 30, 2300, 0,   75,   0,   0,    9,  500, 300, 200,  400,  1000, 1000, 200,  150, 100, 45,  50,  483,  165, 576, 250, 2500, 3 }, /* 12 */
	{ 75,  1, 25,  75, 25, 2300, 0,   100,  0,   0,    9,  500, 300, 200,  400,  1000, 1000, 200,  150, 100, 50,  50,  483,  185, 610, 200, 2500, 3 }, /* 13 */
	{ 90,  1, 30,  90, 30, 2300, 0,   80,   0,   0,    9,  500, 300, 100,  400,  1000, 1000, 200,  150, 100, 45,  200, 2352, 115, 920, 300, 2500, 4 }, /* 14 */
	{ 90,  1, 30,  90, 30, 2300, 0,   75,   0,   0,    9,  500, 300, 200,  400,  1000, 1000, 200,  150, 100, 45,  50,  483,  115, 556, 250, 2500, 3 }, /* 15 */
	{ 90,  1, 30,  90, 30, 2300, 0,   80,   800, 900,  9,  500, 200, 100,  500,  1000, 1000, 200,  150, 100, 45,  200, 2352, 115, 920, 300, 2500, 4 }, /* 16 */
	{ 90,  1, 30,  90, 30, 2300, 0,   100,  800, 900,  9,  500, 200, 100,  500,  1000, 1000, 200,  180, 120, 47,  200, 2352, 115, 920, 300, 3500, 4 }, /* 17 */
	{ 80,  1, 20,  80, 20, 2300, 0,   120,  800, 900,  9,  500, 200, 100,  600,  1000, 1000, 200,  150, 100, 54,  400, 3788, 115, 920, 300, 3500, 4 }, /* 18 */
	{ 32,  1, 8,   32, 8,  583,  0,   150,  500, 1200, 9,  800, 500, 100,  500,  1000, 1000, 400,  700, 500, 150, 40,  483,  115, 920, 300, 2500, 4 }, /* 19 */
	{ 32,  1, 6,   24, 6,  583,  0,   150,  500, 1200, 9,  800, 500, 100,  500,  1000, 1000, 400,  800, 600, 160, 40,  483,  115, 920, 300, 2500, 4 }, /* 20 */
	{ 40,  1, 20,  40, 20, 2300, 0,   80,   800, 900,  9,  800, 400, 0,    1000, 800,  800,  400,  200, 50,  45,  200, 2352, 115, 920, 300, 2500, 5 }, /* 21 */
	{ 30,  1, 10,  30, 10, 2300, 0,   150,  800, 900,  9,  800, 400, 0,    1000, 800,  800,  1000, 400, 150, 54,  400, 3788, 115, 920, 300, 4500, 5 }, /* 22 */
	{ 30,  1, 10,  30, 10, 2300, 0,   150,  800, 900,  9,  800, 400, 0,    1000, 800,  800,  1000, 400, 150, 54,  400, 3788, 115, 920, 300, 4500, 5 }, /* 23 */
	{ 20,  1, 5,   20, 5,  1538, 0,   500,  800, 900,  9,  800, 400, 0,    1000, 800,  800,  1000, 600, 450, 90,  600, 6788, 115, 920, 300, 6500, 5 }, /* 24 */
	{ 20,  1, 5,   20, 5,  1538, 0,   500,  800, 900,  9,  800, 400, 0,    1000, 800,  800,  1000, 700, 700, 90,  600, 6788, 115, 920, 300, 6500, 5 }, /* 25 */
	{ 4,   1, 15,  0,  4,  1000, 100, 1600, 0,   0,    20, 0,   0,   0,    0,    0,    0,    0,    0,   0,   0,   0,   0,    0,   0,   0,   0,    0 }, /* 26 */
	{ 4,   1, 10,  0,  1,  1000, 100, 1600, 0,   0,    48, 0,   0,   0,    0,    0,    0,    0,    0,   0,   0,   0,   0,    0,   0,   0,   0,    0 }, /* 27 */
	{ 4,   1, 15,  0,  4,  1000, 100, 1600, 0,   0,    18, 0,   0,   0,    0,    0,    0,    0,    0,   0,   0,   0,   0,    0,   0,   0,   0,    0 }, /* 28 */
	{ 1,   0, 10,  0,  0,  1000, 100, 1600, 0,   0,    26, 0,   0,   0,    0,    0,    0,    0,    0,   0,   0,   0,   0,    0,   0,   0,   0,    0 }, /* 29 */
};


static int IsCaliber(const tce_weaponDef_t *w, const char *name)
{
	return Q_stricmp(w->caliberClass, name) == 0;
}

/* Linux i386's FCOM/JNC also takes the clamp path for unordered values.
 * Windows tests the unordered flags and preserves the original NaN instead. */
static int TCE_WeaponScopeExceeds(float value, float maximum)
{
#if defined(__linux__) && defined(__i386__)
	return !(value <= maximum);
#else
	return value > maximum;
#endif
}

int BG_InitializeWeaponDef(tce_weaponDef_t *w)
{
	const int *table;
	int       i, defaultClip, clipLimit;
	unsigned int clipLimitBits;
	if (!Q_stricmp(w->weapClass, "K") || !Q_stricmp(w->weapClass, "DC") ||
	    !Q_stricmp(w->weapClass, "G"))
	{
		if (!Q_stricmp(w->weapClass, "K"))
		{
			w->characteristic = 1;
		}
		else if (!Q_stricmp(w->weapClass, "DC"))
		{
			w->characteristic = 29;
		}
		else
		{
			w->characteristic = IsCaliber(w, "STUN") ? 28 : IsCaliber(w, "SMOKE") ? 27 : 26;
		}
		w->weaponCategory = 0;
		w->bolt           = w->burst = w->fullauto = w->pump = 0;
		w->scoped         = 0;
		w->noTacMode      = 1;
	}
	else if (!Q_stricmp(w->weapClass, "P") || !Q_stricmp(w->weapClass, "AP"))
	{
		if (!Q_stricmp(w->weapClass, "P"))
		{
			w->characteristic = IsCaliber(w, "9x19") ? 2 : IsCaliber(w, "40SW") ? 3 :
			                    IsCaliber(w, "45ACP") ? 4 : IsCaliber(w, "50AE") ? 5 : 2;
		}
		else
		{
			w->characteristic = IsCaliber(w, "40SW") ? 7 : IsCaliber(w, "45ACP") ? 8 : 6;
		}
		w->weaponCategory = 1;
		w->bolt           = w->burst = w->fullauto = w->pump = 0;
		w->scoped         = 0;
		w->unknown_1b0    = 1;
		w->unknown_1ac    = 85;
	}
	else if (!Q_stricmp(w->weapClass, "MP"))
	{
		w->characteristic = IsCaliber(w, "45ACP") ? 10 : 9;
		w->weaponCategory = 2;
		w->bolt           = w->burst = w->pump = 0;
		w->scoped         = 0;
		w->unknown_1b0    = 1;
		w->unknown_1ac    = 75;
	}
	else if (!Q_stricmp(w->weapClass, "SMG"))
	{
		w->characteristic = IsCaliber(w, "40SW") ? 12 : IsCaliber(w, "45ACP") ? 13 : 11;
		w->weaponCategory = 2;
		w->bolt           = w->pump = 0;
		w->unknown_1b0    = 1;
		if (TCE_WeaponScopeExceeds(w->scoped, 2))
		{
			w->scoped = 2;
		}
		w->unknown_1ac = 75;
	}
	else if (!Q_stricmp(w->weapClass, "CAR"))
	{
		w->characteristic = IsCaliber(w, "545x39") ? 15 : 14;
		w->weaponCategory = 3;
		w->pump           = w->bolt = 0;
		w->unknown_1b0    = 1;
		if (TCE_WeaponScopeExceeds(w->scoped, 4))
		{
			w->scoped = 4;
		}
		w->unknown_1ac = 75;
	}
	else if (!Q_stricmp(w->weapClass, "MBR"))
	{
		w->characteristic = IsCaliber(w, "762x39") ? 17 : IsCaliber(w, "762x51") ? 18 : 16;
		w->weaponCategory = 3;
		w->pump           = 0;
		w->unknown_1b0    = 1;
		if (TCE_WeaponScopeExceeds(w->scoped, 4))
		{
			w->scoped = 4;
		}
		w->unknown_1ac = 60;
	}
	else if (!Q_stricmp(w->weapClass, "SG"))
	{
		w->characteristic = IsCaliber(w, "76") ? 20 : 19;
		w->pelletCount    = 9;
		w->weaponCategory = 4;
		w->bolt           = w->burst = 0;
		w->scoped         = 0;
		w->unknown_1b0    = 2;
		w->unknown_1ac    = 75;
	}
	else if (!Q_stricmp(w->weapClass, "SR"))
	{
		w->unknown_1b0 = 1;
		if (IsCaliber(w, "556x45"))
		{
			w->characteristic = 21;
			w->weaponCategory = 3;
		}
		else if (IsCaliber(w, "338LAPUA") || IsCaliber(w, "50BMG"))
		{
			w->characteristic = IsCaliber(w, "338LAPUA") ? 24 : 25;
			w->weaponCategory = 5;
			w->burst          = 0;
			w->subsonic       = w->suppressed = 0;
			w->unknown_1a8    = 1;
			w->unknown_1b0    = 2;
		}
		else
		{
			w->characteristic = IsCaliber(w, "792x57") ? 23 : 22;
			w->weaponCategory = 3;
			w->burst          = 0;
		}
		w->fullauto = w->pump = 0;
		if (TCE_WeaponScopeExceeds(w->scoped, 8))
		{
			w->scoped = 8;
		}
		w->unknown_1c0 = 1;
		w->unknown_1ac = 60;
	}

	/* Unknown classes preserve the incoming characteristic, as the original
	 * does. Callers must supply a valid entry (0..29), even on that path. */
	table           = weapCharTable[w->characteristic];
	w->maxammo      = table[0]; w->uses = table[1]; defaultClip = table[2];
	w->startingAmmo = table[3]; w->startingClip = table[4];
	w->reloadTime   = table[5]; w->fireDelayTime = table[6]; w->nextShotTime = table[7];
	w->maxHeat      = table[8]; w->coolRate = table[9]; w->mod = table[10];
	for (i = 0; i < 9; ++i)
		w->unknown_0f8[i] = table[11 + i];
	w->unknown_12c = table[20]; w->unknown_130 = table[21]; w->unknown_134 = table[22];
	w->unknown_154 = table[23]; w->unknown_138 = table[24]; w->unknown_158 = table[25];
	w->unknown_120 = table[26]; w->loadoutWeight = table[27];
	w->unknown_19c = 300;
	w->unknown_1a0 = 800;
	clipLimitBits = (unsigned int)defaultClip + 3u;
	memcpy(&clipLimit, &clipLimitBits, sizeof(clipLimit));
	if (w->maxclip == 0)
	{
		w->maxclip = defaultClip;
	}
	else if (w->maxclip > clipLimit)
	{
		w->maxclip = clipLimit;
	}
	else if (w->maxclip < 1)
	{
		w->maxclip = 1;
	}
	w->startingClip = w->maxclip;
	if (w->startClips >= 0)
	{
		if (w->singleReload)
		{
			w->startingAmmo = w->startClips;
		}
		else
		{
			/* Both originals retain the low 32 bits before the signed clamp. */
			unsigned int ammo = (unsigned int)w->startClips * (unsigned int)w->maxclip;
			memcpy(&w->startingAmmo, &ammo, sizeof(w->startingAmmo));
		}
		if (w->startingAmmo > w->maxammo)
		{
			w->startingAmmo = w->maxammo;
		}
	}
	if (w->pelletCount && w->singleReload && w->maxclip < 7)
	{
		w->loadoutWeight = 3;
	}
	if (w->suppressed || w->subsonic)
	{
		/* x87 multiplies integers by binary32 constants then truncates with
		 * __ftol, without rounding the product to float first. */
		w->unknown_134 = (int)((long double)w->unknown_134 * (long double)0.6f);
		w->unknown_130 = (int)((long double)w->unknown_130 * (long double)0.8f);
		w->unknown_12c = (int)((long double)w->unknown_12c * (long double)0.9f);
		w->unknown_138 = 290;
		w->unknown_158 = 600;
	}
	if (w->scoped > 0 && w->scoped < 2)
	{
		w->scoped = 2;
	}
	return 1;
}
