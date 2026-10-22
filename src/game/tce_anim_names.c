#include "q_shared.h"
#include "bg_public.h"

/* TC scans the current item table for each of the64 original weapon slots.
 * Extra source slot64 remains the existing zero-initialized lookup sentinel. */
animStringItem_t weaponStrings[65];

long BG_StringHashValue(const char *fname)
{
	typedef char stringHashLayout[(sizeof(unsigned) == 4 && sizeof(int) == 4) ? 1 : -1];
	unsigned   hash = 0, weight = 119;
	const char *cursor = fname;
	int        result;

	if (!fname)
	{
		return -1;
	}

	while (*cursor != '\0')
	{
		int upper     = Q_isupper((signed char)*cursor);
		int character = (signed char)*cursor;
		if (upper)
		{
			character += 'a' - 'A';
		}
		/* Native IMUL and ADD retain the low 32 bits, including for signed
		 * bytes outside ASCII and for long inputs. The weight wraps too. */
		hash += (unsigned)character * weight;
		++weight;
		++cursor;
	}
	if (hash == 0xffffffffu)
	{
		hash = 0; /* Only the null-input result uses the -1 sentinel. */
	}
	memcpy(&result, &hash, sizeof(result));
	return (long)result;
}

void BG_InitWeaponStrings(void)
{
	int     i;
	gitem_t *item;
	memset(weaponStrings, 0, 64 * sizeof(weaponStrings[0]));
	for (i = 0; i < 64; ++i)
	{
		for (item = bg_itemlist + 1; item->classname; ++item)
		{
			if (item->giType == IT_WEAPON && item->giTag == i)
			{
				weaponStrings[i].string = item->pickup_name;
				weaponStrings[i].hash   = BG_StringHashValue(item->pickup_name);
				break;
			}
		}
		if (!item->classname)
		{
			weaponStrings[i].string = "(unknown)";
			weaponStrings[i].hash   = BG_StringHashValue("(unknown)");
		}
	}
}

/* Original qagame table: 0x200b1d48, 28 entries plus terminator. */
animStringItem_t animMoveTypesStr[] =
{
	{ "** UNUSED **",    -1 },
	{ "IDLE",            -1 },
	{ "IDLECR",          -1 },
	{ "WALK",            -1 },
	{ "WALKBK",          -1 },
	{ "WALKCR",          -1 },
	{ "WALKCRBK",        -1 },
	{ "RUN",             -1 },
	{ "RUNBK",           -1 },
	{ "SWIM",            -1 },
	{ "SWIMBK",          -1 },
	{ "STRAFERIGHT",     -1 },
	{ "STRAFELEFT",      -1 },
	{ "TURNRIGHT",       -1 },
	{ "TURNLEFT",        -1 },
	{ "CLIMBUP",         -1 },
	{ "CLIMBDOWN",       -1 },
	{ "FALLEN",          -1 },      // DHM - Nerve :: dead, before limbo
	{ "PRONE",           -1 },
	{ "PRONEBK",         -1 },
	{ "IDLEPRONE",       -1 },
	{ "FLAILING",        -1 },

	{ "SNEAK",           -1 },
	{ "AFTERBATTLE",     -1 },
	{ "IDLETURNRIGHT",   -1 },
	{ "IDLETURNLEFT",    -1 },
	{ "IDLECRTURNRIGHT", -1 },
	{ "IDLECRTURNLEFT",  -1 },
	{ NULL,              -1 },
};
