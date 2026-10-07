#include "q_shared.h"
#include "bg_public.h"
#include "bg_classes.h"

/* TC tables match in all three Windows modules (see class_equipment evidence).
 * Preserve the original Axis VIP row classNum=5. Gear restrictions apply later. */
bg_playerclass_t bg_allies_playerclasses[NUM_PLAYER_CLASSES] = {
    { 0, "characters/temperate/allied/soldier.char",
      "ui/assets/mp_gun_blue.tga", "ui/assets/mp_arrow_blue.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 1, "characters/temperate/allied/medic.char",
      "ui/assets/mp_health_blue.tga", "ui/assets/mp_arrow_blue.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 2, "characters/temperate/allied/engineer.char",
      "ui/assets/mp_wrench_blue.tga", "ui/assets/mp_arrow_blue.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 3, "characters/temperate/allied/fieldops.char",
      "ui/assets/mp_ammo_blue.tga", "ui/assets/mp_arrow_blue.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 4, "characters/temperate/allied/cvops.char",
      "ui/assets/mp_spy_blue.tga", "ui/assets/mp_arrow_blue.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 5, "characters/temperate/allied/elite.char",
      "ui/assets/mp_spy_blue.tga", "ui/assets/mp_arrow_blue.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 6, "characters/temperate/allied/vip.char",
      "ui/assets/mp_spy_blue.tga", "ui/assets/mp_arrow_blue.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
};

bg_playerclass_t bg_axis_playerclasses[NUM_PLAYER_CLASSES] = {
    { 0, "characters/temperate/axis/soldier.char",
      "ui/assets/mp_gun_red.tga", "ui/assets/mp_arrow_red.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 1, "characters/temperate/axis/medic.char",
      "ui/assets/mp_health_red.tga", "ui/assets/mp_arrow_red.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 2, "characters/temperate/axis/engineer.char",
      "ui/assets/mp_wrench_red.tga", "ui/assets/mp_arrow_red.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 3, "characters/temperate/axis/fieldops.char",
      "ui/assets/mp_ammo_red.tga", "ui/assets/mp_arrow_red.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 4, "characters/temperate/axis/cvops.char",
      "ui/assets/mp_spy_red.tga", "ui/assets/mp_arrow_red.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 5, "characters/temperate/axis/elite.char",
      "ui/assets/mp_spy_red.tga", "ui/assets/mp_arrow_red.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { 5, "characters/temperate/axis/vip.char",
      "ui/assets/mp_spy_red.tga", "ui/assets/mp_arrow_red.tga",
      { 10, 3, 8, 45, 41, 33, 42, 5, 44, 43, 50, 24, 48, 49, 47, 51, 46, 23, 32, 25, 6, 13, 0, 0 },
      { 2, 39, 52, 14, 40, 7, 38, 37, 54, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0, 0 },
};

bg_playerclass_t* BG_GetPlayerClassInfo( int team, int cls ) {
	bg_playerclass_t* teamList;

	if( cls < PC_SOLDIER || cls >= NUM_PLAYER_CLASSES ) {
		cls = PC_SOLDIER;
	}

	switch( team ) {
		default:
		case TEAM_AXIS:
			teamList = bg_axis_playerclasses;
			break;
		case TEAM_ALLIES:
			teamList = bg_allies_playerclasses;
			break;
	}

	return &teamList[cls];
}

bg_playerclass_t* BG_PlayerClassForPlayerState(playerState_t* ps) {
	return BG_GetPlayerClassInfo(ps->persistant[PERS_TEAM], ps->stats[STAT_PLAYER_CLASS]);
}

qboolean BG_ClassHasWeapon(bg_playerclass_t* classInfo, weapon_t weap) {
	int i;

	if(!weap) {
		return qfalse;
	}

	for( i = 0; i < MAX_WEAPS_PER_CLASS; i++) {
		if(classInfo->classWeapons[i] == weap) {
			return qtrue;
		}
	}
	return qfalse;
}

qboolean BG_WeaponIsPrimaryForClassAndTeam( int classnum, team_t team, weapon_t weapon )
{
	bg_playerclass_t *classInfo;

	if( team == TEAM_ALLIES ) {
		classInfo = &bg_allies_playerclasses[classnum];

		if( BG_ClassHasWeapon( classInfo, weapon ) ) {
			return qtrue;
		}
	} else if( team == TEAM_AXIS ) {
		classInfo = &bg_axis_playerclasses[classnum];

		if( BG_ClassHasWeapon( classInfo, weapon ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

const char* BG_ShortClassnameForNumber( int classNum ) {
	switch( classNum ) {
		case PC_SOLDIER:
			return "Soldr";
		case PC_MEDIC:
			return "Medic";
		case PC_ENGINEER:
			return "Engr";
		case PC_FIELDOPS:
			return "FdOps";
		case PC_COVERTOPS:
			return "CvOps";
		default:
			return "^1ERROR";
	}
}

const char* BG_ClassnameForNumber( int classNum ) {
	switch( classNum ) {
		case PC_SOLDIER:
			return "Soldier";
		case PC_MEDIC:
			return "Medic";
		case PC_ENGINEER:
			return "Engineer";
		case PC_FIELDOPS:
			return "Field Ops";
		case PC_COVERTOPS:
			return "Covert Ops";
		case PC_ELITE: return "Elite";
		case PC_VIP: return "Vip";
		default:
			return "^1ERROR";
	}
}

const char* BG_ClassLetterForNumber( int classNum ) {
	switch( classNum ) {
		case PC_SOLDIER:
			return "S";
		case PC_MEDIC:
			return "M";
		case PC_ENGINEER:
			return "E";
		case PC_FIELDOPS:
			return "F";
		case PC_COVERTOPS:
			return "C";
		default:
			return "^1E";
	}
}

int BG_ClassTextToClass(char *token) {
	if (!Q_stricmp(token, "soldier")) {
		return PC_SOLDIER;
	} else if (!Q_stricmp(token, "medic")) {
		return PC_MEDIC;
	} else if (!Q_stricmp(token, "lieutenant")) { // FIXME: remove from missionpack
		return PC_FIELDOPS;
	} else if (!Q_stricmp(token, "fieldops")) {
		return PC_FIELDOPS;
	} else if (!Q_stricmp(token, "engineer")) {
		return PC_ENGINEER;
	} else if (!Q_stricmp(token, "covertops")) {
		return PC_COVERTOPS;
	}

	return -1;
}

skillType_t BG_ClassSkillForClass( int classnum ) {
	skillType_t classskill[NUM_PLAYER_CLASSES] = { SK_HEAVY_WEAPONS, SK_FIRST_AID, SK_EXPLOSIVES_AND_CONSTRUCTION, SK_SIGNALS, SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS };

	if( classnum < 0 || classnum >= NUM_PLAYER_CLASSES ) {
		return SK_BATTLE_SENSE;
	}

	return classskill[ classnum ];
}

