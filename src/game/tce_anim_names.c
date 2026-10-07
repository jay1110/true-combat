#include "q_shared.h"
#include "bg_public.h"

/* TC scans the current item table for each of the64 original weapon slots.
 * Extra native slot64 remains the existing zero-initialized lookup sentinel. */
animStringItem_t weaponStrings[65];

long BG_StringHashValue( const char *fname ) {
	int		i;
	long	hash;

	if( !fname ) {
		return -1;
	}

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		if( Q_isupper( fname[i] ) ) {
			hash += (long)(fname[i] + ('a' - 'A'))*(i+119);
		} else {
			hash += (long)(fname[i])*(i+119);
		}

		i++;
	}
	if (hash == -1) {
		hash = 0;	// never return -1
	}
	return hash;
}

void BG_InitWeaponStrings(void) {
    int i;
    gitem_t *item;
    memset(weaponStrings, 0, 64 * sizeof(weaponStrings[0]));
    for (i = 0; i < 64; ++i) {
        for (item = bg_itemlist + 1; item->classname; ++item) {
            if (item->giType == IT_WEAPON && item->giTag == i) {
                weaponStrings[i].string = item->pickup_name;
                weaponStrings[i].hash = BG_StringHashValue(item->pickup_name);
                break;
            }
        }
        if (!item->classname) {
            weaponStrings[i].string = "(unknown)";
            weaponStrings[i].hash = BG_StringHashValue("(unknown)");
        }
    }
}

/* Original qagame table: 0x200b1d48, 28 entries plus terminator. */
animStringItem_t animMoveTypesStr[] =
{
	{"** UNUSED **", -1},
	{"IDLE", -1},
	{"IDLECR", -1},
	{"WALK", -1},
	{"WALKBK", -1},
	{"WALKCR", -1},
	{"WALKCRBK", -1},
	{"RUN", -1},
	{"RUNBK", -1},
	{"SWIM", -1},
	{"SWIMBK", -1},
	{"STRAFERIGHT", -1},
	{"STRAFELEFT", -1},
	{"TURNRIGHT", -1},
	{"TURNLEFT", -1},
	{"CLIMBUP", -1},
	{"CLIMBDOWN", -1},
	{"FALLEN", -1},					// DHM - Nerve :: dead, before limbo
	{"PRONE", -1},
	{"PRONEBK", -1},
	{"IDLEPRONE", -1},
	{"FLAILING", -1},

	{"SNEAK", -1},
	{"AFTERBATTLE", -1},
	{"IDLETURNRIGHT", -1},
	{"IDLETURNLEFT", -1},
	{"IDLECRTURNRIGHT", -1},
	{"IDLECRTURNLEFT", -1},
	{NULL, -1},
};
