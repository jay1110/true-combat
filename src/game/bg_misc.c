/*
 * name:		bg_misc.c
 *
 * desc:		both games misc functions, all completely stateless
 *
*/


#include "q_shared.h"
#include "bg_public.h"
#include "tce_weapon_ammo.h"
#include "tce_trajectory.h"
#include "tce_numeric.h"
#include "tce_bg.h"
#include "tce_rotate_precision.h"

#include "../ui/menudef.h"

#ifdef CGAMEDLL
extern vmCvar_t cg_gameType;
#define gametypeCvar cg_gameType
#elif GAMEDLL
extern vmCvar_t g_developer;
extern vmCvar_t g_gametype;
#define gametypeCvar g_gametype
#else
extern vmCvar_t ui_gameType;
#define gametypeCvar ui_gameType
#endif

#define BG_IsSinglePlayerGame() (gametypeCvar.integer == GT_SINGLE_PLAYER) || (gametypeCvar.integer == GT_COOP)


const char *skillNames[SK_NUM_SKILLS] =
{
	"Battle Sense",
	"Engineering",
	"First Aid",
	"Signals",
	"Light Weapons",
	"Heavy Weapons",
	"Covert Ops"
};

const char *skillNamesLine1[SK_NUM_SKILLS] =
{
	"Battle",
	"Engineering",
	"First",
	"Signals",
	"Light",
	"Heavy",
	"Covert"
};

const char *skillNamesLine2[SK_NUM_SKILLS] =
{
	"Sense",
	"",
	"Aid",
	"",
	"Weapons",
	"Weapons",
	"Ops"
};

const char *medalNames[SK_NUM_SKILLS] =
{
	"Distinguished Service Medal",
	"Steel Star",
	"Silver Cross",
	"Signals Medal",
	"Infantry Medal",
	"Bombardment Medal",
	"Silver Snake"
};

const int skillLevels[NUM_SKILL_LEVELS] =
{
	0,      // reaching level 0
	20,     // reaching level 1
	50,     // reaching level 2
	90,     // reaching level 3
	140     // reaching level 4
//	200		// reaching level 5
};

vec3_t playerlegsProneMins = { -13.5f, -13.5f, -24.f };
vec3_t playerlegsProneMaxs = { 13.5f, 13.5f, -14.4f };

int          numSplinePaths;
splinePath_t splinePaths[MAX_SPLINE_PATHS];

int          numPathCorners;
pathCorner_t pathCorners[MAX_PATH_CORNERS];

// these defines are matched with the character torso animations
#define DELAY_LOW       100 // machineguns, tesla, spear, flame
#define DELAY_HIGH      100 // mauser, garand
#define DELAY_PISTOL    100 // colt, luger, sp5, cross
#define DELAY_SHOULDER  50  // rl
#define DELAY_THROW     250 // grenades, dynamite

// Arnout: the new loadout for WolfXP
/* TC:E Windows cgame300964b8: ten banks,22 entries each. */
int weapBanksMultiPlayer[MAX_WEAP_BANKS_MP][MAX_WEAPS_IN_BANK_MP] =
{
	{ 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0, 0  },
	{ 8,  41, 42, 48, 43, 44, 45, 33, 46, 47, 49, 50, 51, 5, 6, 13, 24, 23, 25, 32, 3, 10 },
	{ 2,  7,  37, 38, 53, 54, 39, 40, 14, 52, 0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0, 0  },
	{ 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0, 0  },
	{ 9,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0, 0  },
	{ 4,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0, 0  },
	{ 30, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0, 0  },
	{ 26, 61, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0, 0  },
	{ 20, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0, 0  },
	{ 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0, 0  }
};

// TAT 10/4/2002
//		Using one unified list for which weapons can received ammo
//		This is used both by the ammo pack code and by the bot code to determine if reloads are needed
/* TC qagame200b4408: protocol IDs, including the legacy65/66 sentinels. */
int reloadableWeapons[] =
{
	3,  8,  10, 25, 65, 66, 23, 24, 33, 32, 31, 7,  2,  35, 37, 38,
	56, 55, 53, 54, 39, 40, 51, 47, 46, 44, 45, 43, 42, 41, 5,  6, 13,
	-1
};

// [0] = maxammo		-	max player ammo carrying capacity.
// [1] = uses			-	how many 'rounds' it takes/costs to fire one cycle.
// [2] = maxclip		-	max 'rounds' in a clip.
// [3] = reloadTime		-	time from start of reload until ready to fire.
// [4] = fireDelayTime	-	time from pressing 'fire' until first shot is fired. (used for delaying fire while weapon is 'readied' in animation)
// [5] = nextShotTime	-	when firing continuously, this is the time between shots
// [6] = maxHeat		-	max active firing time before weapon 'overheats' (at which point the weapon will fail for a moment)
// [7] = coolRate		-	how fast the weapon cools down.
// [8] = mod			-	means of death

// potential inclusions in the table:
// damage			-
// splashDamage		-
// soundRange		-	distance which ai can hear the weapon
// ammoWarning		-	amount we give the player a 'low on ammo' warning (just a HUD color change or something)
// clipWarning		-	amount we give the player a 'low in clip' warning (just a HUD color change or something)
// maxclip2			-	allow the player to (mod/powerup) upgrade clip size when aplicable (luger has 8 round standard clip and 32 round snail magazine, for ex.)
//
//
//

// Separate table for SP and MP allow us to make the ammo and med packs function differently and may allow use to balance
// weapons separately for each game.
// Gordon: changed to actually use the maxammo values
/* TC also reads legacy slots beyond the 50-entry SDK weapon enum. */
ammotable_t ammoTableMP[MAX_WEAPONS] =
{
	//	MAX				USES	MAX		START	START  RELOAD	FIRE			NEXT	HEAT,	COOL,	MOD,	...
	//	AMMO			AMT.	CLIP	AMMO	CLIP	TIME	DELAY			SHOT
	{ 0,   0, 0,   0,  0,   0,    50,           0,    0,    0,   0                        },                                        // WP_NONE					// 0
	{ 999, 0, 999, 0,  0,   0,    50,           500,  0,    0,   MOD_KNIFE                },                                        // WP_KNIFE					// 1
	{ 24,  1, 8,   24, 8,   1500,           50, 150,  0,    0,   MOD_LUGER                },                                        // WP_LUGER					// 2	// NOTE: also 32 round 'snail' magazine
	{ 90,  1, 30,  30, 30,  2400,        50,     50,  0,    0,   MOD_MP40                 },                                        // WP_MP40					// 3
	{ 45,  1, 15,  0,  4,   1000,         100,  1600, 0,    0,   MOD_GRENADE_LAUNCHER     },                                        // WP_GRENADE_LAUNCHER		// 4
	{ 4,   1, 1,   0,  4,   1000, 750,          2000, 0,    0,   MOD_PANZERFAUST          },                                        // WP_PANZERFAUST			// 5	// DHM - Nerve :: updated delay so prediction is correct
	{ 200, 1, 200, 0,  200, 1000,        50,    50,   0,    0,   MOD_FLAMETHROWER         },                                        // WP_FLAMETHROWER			// 6
	{ 24,  1, 8,   24, 8,   1500,           50, 400,  0,    0,   MOD_COLT                 },                                        // WP_COLT					// 7
	{ 90,  1, 30,  30, 30,  2400,        50,     75,  0,    0,   MOD_THOMPSON             },                                        // WP_THOMPSON				// 8
	{ 45,  1, 15,  0,  4,   1000,         100,  1600, 0,    0,   MOD_GRENADE_PINEAPPLE    },                                        // WP_GRENADE_PINEAPPLE		// 9

	{ 96,  1, 32,  32, 32,  3100,        50,     50,  0,    0,   MOD_STEN                 },                                    // WP_STEN					// 10
	{ 10,  1, 1,   0,  10,  1500, 50,           1000, 0,    0,   MOD_SYRINGE              },                                        // WP_MEDIC_SYRINGE			// 11
	{ 1,   0, 1,   0,  0,   3000, 50,           1000, 0,    0,   MOD_AMMO,                },                                        // WP_AMMO					// 12
	{ 1,   0, 1,   0,  1,   3000, 50,           1000, 0,    0,   MOD_ARTY,                },                                        // WP_ARTY					// 13
	{ 24,  1, 8,   24, 8,   1500,           50, 400,  0,    0,   MOD_SILENCER             },                                        // WP_SILENCER				// 14
	{ 1,   0, 10,  0,  0,   1000,         100,  1600, 0,    0,   MOD_DYNAMITE             },                                        // WP_DYNAMITE				// 15
	{ 999, 0, 999, 0,  0,   0,    50,           0,    0,    0,   0                        },                                        // WP_SMOKETRAIL			// 16
	{ 999, 0, 999, 0,  0,   0,    50,           0,    0,    0,   0                        },                                        // WP_MAPMORTAR				// 17
	{ 999, 0, 999, 0,  0,   0,    50,           0,    0,    0,   0                        },                                        // VERYBIGEXPLOSION			// 18
	{ 999, 0, 999, 1,  1,   0,    50,           0,    0,    0,   0                        },                                        // WP_MEDKIT				// 19

	{ 999, 0, 999, 0,  0,   0,    50,           0,    0,    0,   0                        },                                        // WP_BINOCULARS			// 20
	{ 999, 0, 999, 0,  0,   0,    50,           0,    0,    0,   0                        },                                        // WP_PLIERS				// 21
	{ 999, 0, 999, 0,  1,   0,    50,           0,    0,    0,   MOD_AIRSTRIKE            },                                        // WP_SMOKE_MARKER			// 22
	{ 30,  1, 10,  20, 10,  2500,        50,    400,  0,    0,   MOD_KAR98                },                                        // WP_KAR98					// 23		K43
	{ 24,  1, 8,   16, 8,   1500,        50,    400,  0,    0,   MOD_CARBINE              },                                        // WP_CARBINE				// 24		GARAND
	{ 24,  1, 8,   16, 8,   1500,        50,    400,  0,    0,   MOD_GARAND               },                                        // WP_GARAND				// 25		GARAND
	{ 1,   0, 1,   0,  1,   100,         50,    100,  0,    0,   MOD_LANDMINE             },                                        // WP_LANDMINE				// 26
	{ 1,   0, 1,   0,  0,   3000,        50,    2000, 0,    0,   MOD_SATCHEL              },                                        // WP_SATCHEL				// 27
	{ 1,   0, 1,   0,  0,   3000, 722,          2000, 0,    0,   0,                       },                                        // WP_SATCHEL_DET			// 28
	{ 6,   1, 1,   0,  0,   2000, DELAY_HIGH,   2000, 0,    0,   MOD_TRIPMINE             },                                        // WP_TRIPMINE				// 29

	{ 1,   1, 10,  0,  1,   1000,         100,  1600, 0,    0,   MOD_SMOKEBOMB            },                                        // WP_SMOKE_BOMB			// 30
	{ 450, 1, 150, 0,  150, 3000,        50,    50,   1500, 300, MOD_MOBILE_MG42          },                                        // WP_MOBILE_MG42			// 31
	{ 30,  1, 10,  20, 10,  2500,        50,    400,  0,    0,   MOD_K43                  },                                        // WP_K43					// 32		K43
	{ 60,  1, 20,  40, 20,  2000,        50,     75,  0,    0,   MOD_FG42                 },                                        // WP_FG42					// 33
	{ 0,   0, 0,   0,  0,   0,    0,            0,    1500, 300, 0                        },                                        // WP_DUMMY_MG42			// 34
	{ 15,  1, 1,   0,  0,   0,    750,          1600, 0,    0,   MOD_MORTAR               },                                        // WP_MORTAR				// 35
	{ 999, 0, 1,   0,  0,   1000, 750,          1600, 0,    0,   0                        },                                        // WP_LOCKPICK				// 36
	{ 48,  1, 8,   48, 8,   2700,           50, 100,  0,    0,   MOD_AKIMBO_COLT          },                                        // WP_AKIMBO_COLT			// 37
	{ 48,  1, 8,   48, 8,   2700,           50, 100,  0,    0,   MOD_AKIMBO_LUGER         },                                        // WP_AKIMBO_LUGER			// 38
	{ 4,   1, 1,   4,  1,   3000,        50,    150,  0,    0,   MOD_GPG40                },                                        // WP_GPG40					// 39

	{ 4,   1, 1,   4,  1,   3000,        50,    350,  0,    0,   MOD_M7                   },                                        // WP_M7					// 40
	{ 24,  1, 8,   24, 8,   1500,           50,  75,  0,    0,   MOD_SILENCED_COLT        },                                        // WP_SILENCED_COLT			// 41
	{ 24,  1, 8,   16, 8,   1500, 50,            100,  0,    0,   MOD_GARAND_SCOPE         },                                        // WP_GARAND_SCOPE			// 42		GARAND
	{ 30,  1, 10,  20, 10,  2500, 50,            100,  0,    0,   MOD_K43_SCOPE            },                                        // WP_K43_SCOPE				// 43		K43
	{ 60,  1, 20,  40, 20,  2000,        50,     80,  0,    0,   MOD_FG42SCOPE            },                                        // WP_FG42SCOPE				// 44
	{ 16,  1, 1,   12, 0,   0,     50,            75, 0,    0,   MOD_MORTAR               },                                        // WP_MORTAR_SET			// 45
	{ 10,  1, 1,   0,  10,  1500, 50,            500, 0,    0,   MOD_SYRINGE              },                                        // WP_MEDIC_ADRENALINE		// 46
	{ 48,  1, 8,   48, 8,   2700,           50, 700,  0,    0,   MOD_AKIMBO_SILENCEDCOLT  },                                        // WP_AKIMBO_SILENCEDCOLT	// 47
	{ 48,  1, 8,   48, 8,   2700,           50, 200,  0,    0,   MOD_AKIMBO_SILENCEDLUGER },                                        // WP_AKIMBO_SILENCEDLUGER	// 48
	{ 450, 1, 150, 0,  150, 3000,        50,    350,   0,    0,   MOD_MOBILE_MG42          },                                    // WP_MOBILE_MG42_SET		// 49
	{ 0, 1, 0, 0, 0, 0, 50, 120, 0, 0, 0 }, /* TC:E slot 50: required ammo and original MP timing. */
	{ 0, 1, 0, 0, 0, 0, 50, 700, 0, 0, 0 }, /* TC:E slot 51: required ammo and original MP timing. */
};

//----(SA)	moved in here so both games can get to it
/* Original TC tables30106e78/20144908 are zero-initialized. Confirmed in
 * running original client and server before/after weapon cycling. SDK pairs
 * such as7->41 and45->35 refer to unrelated TC weapons and must not survive. */
int weapAlts[MAX_WEAPONS] = { 0 };


// new (10/18/00)
char *animStrings[] =
{
	"BOTH_DEATH1",
	"BOTH_DEAD1",
	"BOTH_DEAD1_WATER",
	"BOTH_DEATH2",
	"BOTH_DEAD2",
	"BOTH_DEAD2_WATER",
	"BOTH_DEATH3",
	"BOTH_DEAD3",
	"BOTH_DEAD3_WATER",

	"BOTH_CLIMB",
	"BOTH_CLIMB_DOWN",
	"BOTH_CLIMB_DISMOUNT",

	"BOTH_SALUTE",

	"BOTH_PAIN1",
	"BOTH_PAIN2",
	"BOTH_PAIN3",
	"BOTH_PAIN4",
	"BOTH_PAIN5",
	"BOTH_PAIN6",
	"BOTH_PAIN7",
	"BOTH_PAIN8",

	"BOTH_GRAB_GRENADE",

	"BOTH_ATTACK1",
	"BOTH_ATTACK2",
	"BOTH_ATTACK3",
	"BOTH_ATTACK4",
	"BOTH_ATTACK5",

	"BOTH_EXTRA1",
	"BOTH_EXTRA2",
	"BOTH_EXTRA3",
	"BOTH_EXTRA4",
	"BOTH_EXTRA5",
	"BOTH_EXTRA6",
	"BOTH_EXTRA7",
	"BOTH_EXTRA8",
	"BOTH_EXTRA9",
	"BOTH_EXTRA10",
	"BOTH_EXTRA11",
	"BOTH_EXTRA12",
	"BOTH_EXTRA13",
	"BOTH_EXTRA14",
	"BOTH_EXTRA15",
	"BOTH_EXTRA16",
	"BOTH_EXTRA17",
	"BOTH_EXTRA18",
	"BOTH_EXTRA19",
	"BOTH_EXTRA20",

	"TORSO_GESTURE",
	"TORSO_GESTURE2",
	"TORSO_GESTURE3",
	"TORSO_GESTURE4",

	"TORSO_DROP",

	"TORSO_RAISE",  // (low)
	"TORSO_ATTACK",
	"TORSO_STAND",
	"TORSO_STAND_ALT1",
	"TORSO_STAND_ALT2",
	"TORSO_READY",
	"TORSO_RELAX",

	"TORSO_RAISE2", // (high)
	"TORSO_ATTACK2",
	"TORSO_STAND2",
	"TORSO_STAND2_ALT1",
	"TORSO_STAND2_ALT2",
	"TORSO_READY2",
	"TORSO_RELAX2",

	"TORSO_RAISE3", // (pistol)
	"TORSO_ATTACK3",
	"TORSO_STAND3",
	"TORSO_STAND3_ALT1",
	"TORSO_STAND3_ALT2",
	"TORSO_READY3",
	"TORSO_RELAX3",

	"TORSO_RAISE4", // (shoulder)
	"TORSO_ATTACK4",
	"TORSO_STAND4",
	"TORSO_STAND4_ALT1",
	"TORSO_STAND4_ALT2",
	"TORSO_READY4",
	"TORSO_RELAX4",

	"TORSO_RAISE5", // (throw)
	"TORSO_ATTACK5",
	"TORSO_ATTACK5B",
	"TORSO_STAND5",
	"TORSO_STAND5_ALT1",
	"TORSO_STAND5_ALT2",
	"TORSO_READY5",
	"TORSO_RELAX5",

	"TORSO_RELOAD1",    // (low)
	"TORSO_RELOAD2",    // (high)
	"TORSO_RELOAD3",    // (pistol)
	"TORSO_RELOAD4",    // (shoulder)

	"TORSO_MG42",       // firing tripod mounted weapon animation

	"TORSO_MOVE",       // torso anim to play while moving and not firing (swinging arms type thing)
	"TORSO_MOVE_ALT",       // torso anim to play while moving and not firing (swinging arms type thing)

	"TORSO_EXTRA",
	"TORSO_EXTRA2",
	"TORSO_EXTRA3",
	"TORSO_EXTRA4",
	"TORSO_EXTRA5",
	"TORSO_EXTRA6",
	"TORSO_EXTRA7",
	"TORSO_EXTRA8",
	"TORSO_EXTRA9",
	"TORSO_EXTRA10",

	"LEGS_WALKCR",
	"LEGS_WALKCR_BACK",
	"LEGS_WALK",
	"LEGS_RUN",
	"LEGS_BACK",
	"LEGS_SWIM",
	"LEGS_SWIM_IDLE",

	"LEGS_JUMP",
	"LEGS_JUMPB",
	"LEGS_LAND",

	"LEGS_IDLE",
	"LEGS_IDLE_ALT", //	"LEGS_IDLE2"
	"LEGS_IDLECR",

	"LEGS_TURN",

	"LEGS_BOOT",        // kicking animation

	"LEGS_EXTRA1",
	"LEGS_EXTRA2",
	"LEGS_EXTRA3",
	"LEGS_EXTRA4",
	"LEGS_EXTRA5",
	"LEGS_EXTRA6",
	"LEGS_EXTRA7",
	"LEGS_EXTRA8",
	"LEGS_EXTRA9",
	"LEGS_EXTRA10",
};


// old
char *animStringsOld[] =
{
	"BOTH_DEATH1",
	"BOTH_DEAD1",
	"BOTH_DEATH2",
	"BOTH_DEAD2",
	"BOTH_DEATH3",
	"BOTH_DEAD3",

	"BOTH_CLIMB",
	"BOTH_CLIMB_DOWN",
	"BOTH_CLIMB_DISMOUNT",

	"BOTH_SALUTE",

	"BOTH_PAIN1",
	"BOTH_PAIN2",
	"BOTH_PAIN3",
	"BOTH_PAIN4",
	"BOTH_PAIN5",
	"BOTH_PAIN6",
	"BOTH_PAIN7",
	"BOTH_PAIN8",

	"BOTH_EXTRA1",
	"BOTH_EXTRA2",
	"BOTH_EXTRA3",
	"BOTH_EXTRA4",
	"BOTH_EXTRA5",

	"TORSO_GESTURE",
	"TORSO_GESTURE2",
	"TORSO_GESTURE3",
	"TORSO_GESTURE4",

	"TORSO_DROP",

	"TORSO_RAISE",  // (low)
	"TORSO_ATTACK",
	"TORSO_STAND",
	"TORSO_READY",
	"TORSO_RELAX",

	"TORSO_RAISE2", // (high)
	"TORSO_ATTACK2",
	"TORSO_STAND2",
	"TORSO_READY2",
	"TORSO_RELAX2",

	"TORSO_RAISE3", // (pistol)
	"TORSO_ATTACK3",
	"TORSO_STAND3",
	"TORSO_READY3",
	"TORSO_RELAX3",

	"TORSO_RAISE4", // (shoulder)
	"TORSO_ATTACK4",
	"TORSO_STAND4",
	"TORSO_READY4",
	"TORSO_RELAX4",

	"TORSO_RAISE5", // (throw)
	"TORSO_ATTACK5",
	"TORSO_ATTACK5B",
	"TORSO_STAND5",
	"TORSO_READY5",
	"TORSO_RELAX5",

	"TORSO_RELOAD1",    // (low)
	"TORSO_RELOAD2",    // (high)
	"TORSO_RELOAD3",    // (pistol)
	"TORSO_RELOAD4",    // (shoulder)

	"TORSO_MG42",       // firing tripod mounted weapon animation

	"TORSO_MOVE",       // torso anim to play while moving and not firing (swinging arms type thing)

	"TORSO_EXTRA2",
	"TORSO_EXTRA3",
	"TORSO_EXTRA4",
	"TORSO_EXTRA5",

	"LEGS_WALKCR",
	"LEGS_WALKCR_BACK",
	"LEGS_WALK",
	"LEGS_RUN",
	"LEGS_BACK",
	"LEGS_SWIM",

	"LEGS_JUMP",
	"LEGS_LAND",

	"LEGS_IDLE",
	"LEGS_IDLE2",
	"LEGS_IDLECR",

	"LEGS_TURN",

	"LEGS_BOOT",        // kicking animation

	"LEGS_EXTRA1",
	"LEGS_EXTRA2",
	"LEGS_EXTRA3",
	"LEGS_EXTRA4",
	"LEGS_EXTRA5",
};

/*QUAKED item_***** ( 0 0 0 ) (-16 -16 -16) (16 16 16) SUSPENDED SPIN PERSISTANT
DO NOT USE THIS CLASS, IT JUST HOLDS GENERAL INFORMATION.
SUSPENDED - will allow items to hang in the air, otherwise they are dropped to the next surface.
SPIN - will allow items to spin in place.
PERSISTANT - some items (ex. clipboards) can be picked up, but don't disappear

If an item is the target of another entity, it will not spawn in until fired.

An item fires all of its targets when it is picked up.  If the toucher can't carry it, the targets won't be fired.

"notfree" if set to 1, don't spawn in free for all games
"notteam" if set to 1, don't spawn in team games
"notsingle" if set to 1, don't spawn in single player games
"wait"	override the default wait before respawning.  -1 = never respawn automatically, which can be used with targeted spawning.
"random" random number of plus or minus seconds varied from the respawn time
"count" override quantity or duration on most items.
"stand" if the item has a stand (ex: mp40_stand.md3) this specifies which stand tag to attach the weapon to ("stand":"4" would mean "tag_stand4" for example)  only weapons support stands currently
*/

// JOSEPH 5-2-00
//----(SA) the addition of the 'ammotype' field was added by me, not removed by id (SA)
gitem_t bg_itemlist[] =
{
#include "tce_item_catalog.inc"
};

int bg_numItems = sizeof(bg_itemlist) / sizeof(bg_itemlist[0]) - 1;

/*
==============
BG_FindItemForHoldable
==============
*/
gitem_t *BG_FindItemForHoldable(holdable_t pw)
{
	int i;

	for ( i = 0 ; i < bg_numItems ; i++ )
	{
		if (bg_itemlist[i].giType == IT_HOLDABLE && bg_itemlist[i].giTag == pw)
		{
			return &bg_itemlist[i];
		}
	}

//	Com_Error( ERR_DROP, "HoldableItem not found" );

	return NULL;
}


/*
===============
BG_FindItemForWeapon

===============
*/
gitem_t *BG_FindItemForWeapon(weapon_t weapon)
{
	gitem_t *it;

	for ( it = bg_itemlist + 1 ; it->classname ; it++)
	{
		if (it->giType == IT_WEAPON && it->giTag == weapon)
		{
			return it;
		}
	}

	Com_Error(ERR_DROP, "Couldn't find item for weapon %i", weapon);
	return NULL;
}

//----(SA) added

#define DEATHMATCH_SHARED_AMMO 0


/*
==============
BG_FindClipForWeapon
==============
*/
weapon_t BG_FindClipForWeapon(weapon_t weapon)
{
	return (weapon_t)TCE_BG_FindClipForWeapon(weapon);
}



/*
==============
BG_FindAmmoForWeapon
==============
*/
weapon_t BG_FindAmmoForWeapon(weapon_t weapon)
{
	return (weapon_t)TCE_BG_FindAmmoForWeapon(weapon);
}

/*
==============
BG_AkimboFireSequence
	returns 'true' if it's the left hand's turn to fire, 'false' if it's the right hand's turn
==============
*/
qboolean BG_AkimboFireSequence(int weapon, int akimboClip, int mainClip)
{
	/* TC IDs apply during initialization too; UI does not load server Gear. */
	return TCE_BG_AkimboFireSequence(weapon, akimboClip, mainClip);
}

/*
==============
BG_IsAkimboWeapon
==============
*/
qboolean BG_IsAkimboWeapon(int weaponNum)
{
	/* TC IDs apply during initialization too; UI does not load server Gear. */
	return TCE_BG_IsAkimboWeapon(weaponNum);
}

/*
==============
BG_IsAkimboSideArm
==============
*/
qboolean BG_IsAkimboSideArm(int weaponNum, playerState_t *ps)
{
	switch (weaponNum)
	{
	/* TC weapon IDs differ from the inherited ET weapon enum. */
	case 39:   if (ps->weapon == 37 || ps->weapon == 53)
		{
			return qtrue;
		}
		break;
	case 2:    if (ps->weapon == 38 || ps->weapon == 54)
		{
			return qtrue;
		}
		break;
	}
	return qfalse;
}

/*
==============
BG_AkimboSidearm
==============
*/
int BG_AkimboSidearm(int weaponNum)
{
	/* TC IDs apply during initialization too; UI does not load server Gear. */
	return TCE_BG_AkimboSidearm(weaponNum);
}

/*
==============
BG_AkimboForSideArm
==============
*/
/*int BG_AkimboForSideArm( int weaponNum ) {
	switch( weaponNum )
	{
	case WP_COLT:			return WP_AKIMBO_COLT;			break;
	case WP_SILENCED_COLT:	return WP_AKIMBO_SILENCEDCOLT;	break;
	case WP_LUGER:			return WP_AKIMBO_LUGER;			break;
	case WP_SILENCER:		return WP_AKIMBO_SILENCEDLUGER;	break;
	default:				return WP_NONE;					break;
	}
}*/


//----(SA) Added keys
/*
==============
BG_FindItemForKey
==============
*/
/*gitem_t *BG_FindItemForKey(wkey_t k, int *indexreturn)
{
	int		i;

	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( bg_itemlist[i].giType == IT_KEY && bg_itemlist[i].giTag == k ) {
			{
				if(indexreturn)
					*indexreturn = i;
				return &bg_itemlist[i];
			}
		}
	}

	Com_Error( ERR_DROP, "Key %d not found", k );
	return NULL;
}*/
//----(SA) end


//----(SA) added
/*
==============
BG_FindItemForAmmo
==============
*/
gitem_t *BG_FindItemForAmmo(int ammo)
{
	int i = 0;

	for (; i < bg_numItems; i++)
	{
		if (bg_itemlist[i].giType == IT_AMMO && bg_itemlist[i].giAmmoIndex == ammo)
		{
			return &bg_itemlist[i];
		}
	}
	Com_Error(ERR_DROP, "Item not found for ammo: %d", ammo);
	return NULL;
}
//----(SA) end


/*
===============
BG_FindItem
===============
*/
gitem_t *BG_FindItem(const char *pickupName)
{
	gitem_t *it;

	for ( it = bg_itemlist + 1 ; it->classname ; it++ )
	{
		if (!Q_stricmp(it->pickup_name, pickupName))
		{
			return it;
		}
	}

	return NULL;
}

gitem_t *BG_FindItemForClassName(const char *className)
{
	gitem_t *it;

	for ( it = bg_itemlist + 1 ; it->classname ; it++ )
	{
		if (!Q_stricmp(it->classname, className))
		{
			return it;
		}
	}

	return NULL;
}


// DHM - Nerve :: returns qtrue if a weapon is indeed used in multiplayer
// Gordon: FIXME: er, we shouldnt really need this, just remove all the weapons we dont actually want :)
qboolean BG_WeaponInWolfMP(int weapon)
{
	/* TC IDs apply during initialization too; UI does not load server Gear. */
	return TCE_BG_WeaponInWolfMP(weapon);
}

/*
============
BG_PlayerTouchesItem

Items can be picked up without actually touching their physical bounds to make
grabbing them easier
============
*/
qboolean BG_PlayerTouchesItem(playerState_t *ps, entityState_t *item, int atTime)
{
	vec3_t origin;

	/* TC2002b510 uses the six-argument trajectory ABI, including raw mode13. */
	TCE_BG_EvaluateTrajectory(&item->pos, atTime, origin, qfalse, item->effect2Time, 1.0f);

	// we are ignoring ducked differences here
	/* Original uses positive ordered comparisons, so unordered coordinates
	 * cannot turn into a successful pickup through negated rejection tests. */
	return ps->origin[0] - origin[0] <= 36
	       && ps->origin[0] - origin[0] >= -36
	       && ps->origin[1] - origin[1] <= 36
	       && ps->origin[1] - origin[1] >= -36
	       && ps->origin[2] - origin[2] <= 36
	       && ps->origin[2] - origin[2] >= -36;
}


/*
=================================
BG_AddMagicAmmo:
	if numOfClips is 0, no ammo is added, it just return whether any ammo CAN be added;
	otherwise return whether any ammo was ACTUALLY added.

WARNING: when numOfClips is 0, DO NOT CHANGE ANYTHING under ps.
=================================
*/
int BG_GrenadesForClass(int cls, int *skills)
{
	switch (cls)
	{
	case PC_MEDIC:
		if (skills[SK_FIRST_AID] >= 1)
		{
			return 2;
		}
		return 1;
	case PC_SOLDIER:
		return 4;
	case PC_ENGINEER:
		return 8;
	case PC_FIELDOPS:
		if (skills[SK_SIGNALS] >= 1)
		{
			return 2;
		}
		return 1;
	case PC_COVERTOPS:
		return 2;
	}

	return 0;
}

weapon_t BG_GrenadeTypeForTeam(team_t team)
{
	switch (team)
	{
	case TEAM_AXIS:
		return WP_GRENADE_LAUNCHER;
	case TEAM_ALLIES:
		return WP_GRENADE_PINEAPPLE;
	default:
		return WP_NONE;
	}
}

// Gordon: setting numOfClips = 0 allows you to check if the client needs ammo, but doesnt give any
qboolean BG_AddMagicAmmo(playerState_t *ps, int *skill, int teamNum, int numOfClips)
{
	int i, weapon;
	int ammoAdded = qfalse;
	int maxammo;
	int clip;
	int weapNumOfClips;

	// Gordon: handle grenades first
	i      = BG_GrenadesForClass(ps->stats[STAT_PLAYER_CLASS], skill);
	weapon = BG_GrenadeTypeForTeam(teamNum);

	clip = BG_FindClipForWeapon(weapon);
	if (ps->ammoclip[clip] < i)
	{

		// Gordon: early out
		if (!numOfClips)
		{
			return qtrue;
		}

		ps->ammoclip[clip] += numOfClips;

		ammoAdded = qtrue;

		COM_BitSet(ps->weapons, weapon);

		if (ps->ammoclip[clip] > i)
		{
			ps->ammoclip[clip] = i;
		}
	}

	if (COM_BitCheck(ps->weapons, WP_MEDIC_SYRINGE))
	{
		i = skill[SK_FIRST_AID] >= 2 ? 12 : 10;

		clip = BG_FindClipForWeapon(WP_MEDIC_SYRINGE);

		if (ps->ammoclip[clip] < i)
		{
			if (!numOfClips)
			{
				return qtrue;
			}

			ps->ammoclip[clip] += numOfClips;

			ammoAdded = qtrue;

			if (ps->ammoclip[clip] > i)
			{
				ps->ammoclip[clip] = i;
			}
		}
	}

	// Gordon: now other weapons
	for (i = 0; reloadableWeapons[i] >= 0; i++)
	{
		weapon = reloadableWeapons[i];
		if (COM_BitCheck(ps->weapons, weapon))
		{
			maxammo = BG_MaxAmmoForWeapon(weapon, skill);

			// Handle weapons that just use clip, and not ammo
			if (weapon == 66)
			{
				clip = BG_FindAmmoForWeapon(weapon);
				if (ps->ammoclip[clip] < maxammo)
				{
					// early out
					if (!numOfClips)
					{
						return qtrue;
					}

					ammoAdded          = qtrue;
					ps->ammoclip[clip] = maxammo;
				}
			}
			else if (weapon == 65)
			{
				clip = BG_FindAmmoForWeapon(weapon);
				if (ps->ammoclip[clip] < maxammo)
				{
					// early out
					if (!numOfClips)
					{
						return qtrue;
					}

					ammoAdded           = qtrue;
					ps->ammoclip[clip] += numOfClips;
					if (ps->ammoclip[clip] >= maxammo)
					{
						ps->ammoclip[clip] = maxammo;
					}
				}
			}
			else
			{
				clip = BG_FindAmmoForWeapon(weapon);
				if (ps->ammo[clip] < maxammo)
				{
					// early out
					if (!numOfClips)
					{
						return qtrue;
					}
					ammoAdded = qtrue;

					if (BG_IsAkimboWeapon(weapon))
					{
						weapNumOfClips = numOfClips * 2; // double clips babeh!
					}
					else
					{
						weapNumOfClips = numOfClips;
					}

					// add and limit check
					ps->ammo[clip] += weapNumOfClips * weaponDef[weapon].maxclip;
					if (ps->ammo[clip] > maxammo)
					{
						ps->ammo[clip] = maxammo;
					}
				}
			}
		}
	}
	return ammoAdded;
}

/*
================
BG_CanUseWeapon: can a player of the specified team and class use this weapon?
extracted and adapted from Bot_GetWeaponForClassAndTeam.
================
- added by xkan, 01/02/03
*/
qboolean BG_CanUseWeapon(int classNum, int teamNum, weapon_t weapon)
{
	// TAT 1/11/2003 - is this SP game? - different weapons available in SP
	qboolean isSinglePlayer = BG_IsSinglePlayerGame() ? qtrue : qfalse;

	switch (classNum)
	{
	case PC_ENGINEER:
		if (weapon == WP_PLIERS
		    || weapon == WP_DYNAMITE
		    || weapon == WP_LANDMINE)
		{
			return qtrue;
		}
		else if (weapon == WP_MP40
		         || weapon == WP_KAR98)
		{
			return (teamNum == TEAM_AXIS);
		}
		else if (weapon == WP_THOMPSON
		         || weapon == WP_CARBINE)
		{
			return (teamNum == TEAM_ALLIES);
		}
	case PC_FIELDOPS:
		// TAT 1/11/2003 - in SP, field op can only use handgun, check after switch below
		if (isSinglePlayer && teamNum == TEAM_ALLIES)
		{
			break;
		}

		if (weapon == WP_STEN)
		{
			return qtrue;
		}
		else if (weapon == WP_MP40)
		{
			return (teamNum == TEAM_AXIS);
		}
		else if (weapon == WP_THOMPSON)
		{
			return (teamNum == TEAM_ALLIES);
		}
		break;
	case PC_SOLDIER:
		if (weapon == WP_STEN
		    || weapon == 65
		    || weapon == 66
		    // Gordon: shouldn't this only be for cvt ops?
		    || weapon == WP_FG42
		    || weapon == WP_MOBILE_MG42
		    || weapon == 62
		    || weapon == WP_MORTAR
		    || weapon == 60)
		{
			return qtrue;
		}
		else if (weapon == WP_MP40)
		{
			return (teamNum == TEAM_AXIS);
		}
		else if (weapon == WP_THOMPSON)
		{
			return (teamNum == TEAM_ALLIES);
		}
		break;

	case PC_MEDIC:
		if (weapon == WP_MEDIC_SYRINGE
		    || weapon == WP_MEDKIT)
		{
			return qtrue;
		}
		// TAT 1/11/2003 - in SP, medic can only use handgun, check after switch below
		else if (isSinglePlayer && teamNum == TEAM_ALLIES)
		{
			break;
		}
		else if (weapon == WP_MP40)
		{
			return (teamNum == TEAM_AXIS);
		}
		else if (weapon == WP_THOMPSON)
		{
			return (teamNum == TEAM_ALLIES);
		}
		break;
	case PC_COVERTOPS:
		if (weapon == WP_STEN
		    || weapon == WP_SMOKE_BOMB
		    || weapon == WP_SATCHEL
		    || weapon == WP_AMMO
		    // Gordon: this is a cvt ops weapon in single player too, right?
		    || weapon == WP_FG42)
		{
			return qtrue;
		}
		else if (weapon == WP_K43)
		{
			return (teamNum == TEAM_AXIS);
		}
		else if (weapon == WP_GARAND)
		{
			return (teamNum == TEAM_ALLIES);
		}
		break;
	}

	if (weapon == WP_NONE
	    || weapon == WP_KNIFE
	    || weapon == WP_LUGER
	    || weapon == WP_COLT)
	{
		return qtrue;
	}

	// if not any of the above
	return qfalse;
}

#define AMMOFORWEAP BG_FindAmmoForWeapon(item->giTag)
/*
================
BG_CanItemBeGrabbed

Returns false if the item should not be picked up.
This needs to be the same for client side prediction and server use.
================
*/
qboolean    BG_CanItemBeGrabbed(const entityState_t *ent, const playerState_t *ps, int *skill, int teamNum)
{
	gitem_t *item;

	if (ent->modelindex < 1 || ent->modelindex >= bg_numItems)
	{
		Com_Error(ERR_DROP, "BG_CanItemBeGrabbed: index out of range");
	}

	item = &bg_itemlist[ent->modelindex];

	switch (item->giType)
	{
	case IT_WEAPON:
		/* TC 2002ba60: VIP/restricted carrier states cannot take weapons. */
		if (ps->stats[STAT_TCE_FLAGS] & 0x500)
		{
			return qfalse;
		}
		if (item->giTag == WP_AMMO)
		{
			// magic ammo for any two-handed weapon
			// xkan, 11/21/2002 - only pick up if ammo is not full, numClips is 0, so ps will
			// NOT be changed (I know, it places the burden on the programmer, rather than the
			// compiler, to ensure that).
			return BG_AddMagicAmmo((playerState_t *)ps, skill, teamNum, 0);     // Arnout: had to cast const away
		}

		return qtrue;

	case IT_AMMO:
		/* TC uses this item class for the team's dropped bomb/VIP. */
		return ent->otherEntityNum2 == ps->persistant[PERS_TEAM];

	case IT_ARMOR:
		return qfalse;

	case IT_HEALTH:
		return ps->stats[STAT_HEALTH] < ps->stats[STAT_MAX_HEALTH] &&
		       ent->otherEntityNum2 != ps->persistant[PERS_TEAM];

	case IT_TEAM: // team items, such as flags

		// density tracks how many uses left
		if (ent->density < 1)
		{
			return qfalse;
		}

		// DHM - Nerve :: otherEntity2 is now used instead of modelindex2
		// ent->modelindex2 is non-zero on items if they are dropped
		// we need to know this because we can pick up our dropped flag (and return it)
		// but we can't pick up our flag at base
		if (ps->persistant[PERS_TEAM] == TEAM_AXIS)
		{
			if (item->giTag == PW_BLUEFLAG ||
			    (item->giTag == PW_REDFLAG && ent->otherEntityNum2))
			{
				return qtrue;
			}
		}
		else if (ps->persistant[PERS_TEAM] == TEAM_ALLIES)
		{
			if (item->giTag == PW_REDFLAG ||
			    (item->giTag == PW_BLUEFLAG && ent->otherEntityNum2))
			{
				return qtrue;
			}
		}

		return qfalse;


	case IT_HOLDABLE:
		return qtrue;

	case IT_TREASURE:   // treasure always picked up
		return qtrue;

	case IT_KEY:
		return qtrue;   // keys are always picked up

	case IT_BAD:
		Com_Error(ERR_DROP, "BG_CanItemBeGrabbed: IT_BAD");

	}
	return qfalse;
}

//======================================================================

void BG_CalculateSpline_r(splinePath_t *, vec3_t, vec3_t, float);

qboolean BG_TraverseSpline(float *, splinePath_t **);

/*
================
BG_RaySphereIntersection

================
*/

#if defined(CGAMEDLL) || defined(GAMEDLL)
/* Intersect the segment's normalized line with the sphere centered at origin.
 * Preserve the distinct TC projection and root stores on each platform. */
qboolean BG_RaySphereIntersection(float radius, vec3_t origin, splineSegment_t *path,
                                  float *t0, float *t1)
{
	long double dx, dy, dz, projection, squaredDistance, discriminant, root;
#if defined(_WIN32)
	float storedProjection;
#else
	long double     radiusSquared, firstRoot, secondRoot;
	float           storedRoot;
	volatile vec3_t storedOffsets;

	radiusSquared = (long double)radius * radius;
#endif
	dx = (long double)path->start[0] - origin[0];
#if !defined(_WIN32)
	/* Native Linux writes these float copies but keeps the differences for
	 * the following products. Volatile retains the source-level stores. */
	storedOffsets[0] = (float)dx;
#endif
	dy = (long double)path->start[1] - origin[1];
#if !defined(_WIN32)
	storedOffsets[1] = (float)dy;
#endif
	dz = (long double)path->start[2] - origin[2];
#if !defined(_WIN32)
	storedOffsets[2] = (float)dz;
#endif

#if defined(_WIN32)
	projection       = dx * path->v_norm[0] + (dy * path->v_norm[1] + dz * path->v_norm[2]);
	projection      += projection;
	storedProjection = (float)projection;
	squaredDistance  = dx * dx + (dy * dy + dz * dz);
	discriminant     = projection * storedProjection -
	                   (squaredDistance - (long double)radius * radius) * 4.0f;
#else
	projection      = (dy * path->v_norm[1] + dx * path->v_norm[0]) + dz * path->v_norm[2];
	projection     += projection;
	squaredDistance = dz * dz + (dy * dy + dx * dx);
	discriminant    = projection * projection - (squaredDistance - radiusSquared) * 4.0f;
#endif
	/* The native C0/CF gate rejects unordered as well as negative values. */
	if (!(discriminant >= 0.0L))
	{
		return qfalse;
	}

	root = sqrtl(discriminant);
#if defined(_WIN32)
	*t0 = (float)((root - storedProjection) * 0.5f);
	*t1 = (float)((-(long double)storedProjection - root) * 0.5f);
#else
	storedRoot = (float)root;
	firstRoot  = ((long double)storedRoot - projection) * 0.5f;
	secondRoot = (-projection - storedRoot) * 0.5f;
	*t0        = (float)firstRoot;
	*t1        = (float)secondRoot;
#endif
	return qtrue;
}
#else
qboolean BG_RaySphereIntersection(float radius, vec3_t origin, splineSegment_t *path, float *t0, float *t1)
{
	vec3_t v;
	float  b, c, d;

	VectorSubtract(path->start, origin, v);

	b = 2 * DotProduct(v, path->v_norm);
	c = DotProduct(v, v) - (radius * radius);

	d = (b * b) - (4 * c);
	if (d < 0)
	{
		return qfalse;
	}
	d = sqrt(d);

	*t0 = (-b + d) * 0.5f;
	*t1 = (-b - d) * 0.5f;

	return qtrue;
}
#endif

#if defined(CGAMEDLL) || defined(GAMEDLL)
/* TC spline offsets: backwards is part of the shared API, but these native
 * controllers choose traversal direction from radius and do not consume it. */

static qboolean BG_PathFractionInSegment(long double value)
{
	return value >= 0.0L && value <= 1.0L;
}

void BG_LinearPathOrigin2(float radius, splinePath_t **pSpline, float *deltaTime,
                          vec3_t result, qboolean backwards)
{
	long double position = (long double)*deltaTime * MAX_SPLINE_SEGMENTS;
	float       fraction;
	int         index;
	qboolean    first = qtrue, reverse;

	(void)backwards;
#if defined(_WIN32)
	index   = TCE_TruncateNativeInteger(floor((double)position));
	reverse = !(radius >= 0.0f);
#else
	index   = TCE_TruncateNativeInteger((double)floorl(position));
	reverse = radius < 0.0f;
#endif
	if (index >= MAX_SPLINE_SEGMENTS)
	{
		index    = MAX_SPLINE_SEGMENTS - 1;
		fraction = 1.0f;
	}
	else
	{
#if defined(_WIN32)
		/* The native Windows controller reloads delta after the CRT call. */
		fraction = (float)((long double)*deltaTime * MAX_SPLINE_SEGMENTS - index);
#else
		fraction = (float)(position - index);
#endif
	}

	for (;;)
	{
		float t0, t1;
		if (BG_RaySphereIntersection(radius, result, &(*pSpline)->segments[index], &t0, &t1))
		{
			const splineSegment_t *segment = &(*pSpline)->segments[index];
			long double           normalized0, normalized1, firstRoot, secondRoot, selected;
			qboolean              found = qfalse, preferFirst;
			int                   axis;

#if defined(_WIN32)
			normalized0 = (long double)t0 / segment->length;
			t0          = (float)normalized0;
			normalized1 = (long double)t1 / segment->length;
			t1          = (float)normalized1;
			firstRoot   = t0;
			secondRoot  = t1;
#else
			normalized0 = (1.0L / segment->length) * t0;
			t0          = (float)normalized0;
			normalized1 = (1.0L / segment->length) * t1;
			t1          = (float)normalized1;
			/* Linux retains the first normalized root only when reversing;
			 * the second is retained only after the first segment. */
			firstRoot  = reverse ? normalized0 : (long double)t0;
			secondRoot = reverse && !first ? normalized1 : (long double)t1;
#endif
			if (first)
			{
				preferFirst = reverse ? !(firstRoot >= fraction) : firstRoot > fraction;
				if (preferFirst && BG_PathFractionInSegment(firstRoot))
				{
					selected = firstRoot;
					found    = qtrue;
				}
				else if (reverse ? !(secondRoot >= fraction) : secondRoot > fraction)
				{
					selected = secondRoot;
					found    = qtrue;
				}
			}
			else
			{
				if (reverse)
				{
					preferFirst = !(firstRoot >= secondRoot);
				}
				else
				{
#if defined(_WIN32)
					preferFirst = firstRoot > secondRoot;
#else
					preferFirst = !(secondRoot >= firstRoot);
#endif
				}
				selected = preferFirst && BG_PathFractionInSegment(firstRoot) ? firstRoot : secondRoot;
				found    = qtrue;
			}

			if (found && BG_PathFractionInSegment(selected))
			{
				*deltaTime = (float)((long double)index * 0.0625f + selected * 0.0625f);
				for (axis = 0; axis < 3; ++axis)
				{
					/* Reload the spline after each visible output store. */
					segment      = &(*pSpline)->segments[index];
					result[axis] = (float)((selected * segment->length) * segment->v_norm[axis] + segment->start[axis]);
				}
				return;
			}
		}

		first = qfalse;
		if (reverse)
		{
			if (--index >= 0)
			{
				continue;
			}
			index = MAX_SPLINE_SEGMENTS - 1;
			if (!(*pSpline)->prev)
			{
				return;
			}
			*pSpline = (*pSpline)->prev;
		}
		else
		{
			if (++index < MAX_SPLINE_SEGMENTS)
			{
				continue;
			}
			index = 0;
			if (!(*pSpline)->next)
			{
				return;
			}
			*pSpline = (*pSpline)->next;
		}
	}
}
#else
void BG_LinearPathOrigin2(float radius, splinePath_t **pSpline, float *deltaTime, vec3_t result, qboolean backwards)
{
	qboolean first = qtrue;
	float    t     = 0.f;
	int      i     = floor((*deltaTime) * (MAX_SPLINE_SEGMENTS));
	float    frac;
//	int x = 0;
//	splinePath_t* start = *pSpline;

	if (i >= MAX_SPLINE_SEGMENTS)
	{
		i    = MAX_SPLINE_SEGMENTS - 1;
		frac = 1.f;
	}
	else
	{
		frac = (((*deltaTime) * (MAX_SPLINE_SEGMENTS)) - i);
	}

	while (qtrue)
	{
		float t0, t1;

		while (qtrue)
		{
			if (BG_RaySphereIntersection(radius, result, &(*pSpline)->segments[i], &t0, &t1))
			{
				qboolean found = qfalse;

				t0 /= (*pSpline)->segments[i].length;
				t1 /= (*pSpline)->segments[i].length;

				if (first)
				{
					if (radius < 0)
					{
						if (t0 < frac && (t0 >= 0.f && t0 <= 1.f))
						{
							t     = t0;
							found = qtrue;
						}
						else if (t1 < frac)
						{
							t     = t1;
							found = qtrue;
						}
					}
					else
					{
						if (t0 > frac && (t0 >= 0.f && t0 <= 1.f))
						{
							t     = t0;
							found = qtrue;
						}
						else if (t1 > frac)
						{
							t     = t1;
							found = qtrue;
						}
					}
				}
				else
				{
					if (radius < 0)
					{
						if (t0 < t1 && (t0 >= 0.f && t0 <= 1.f))
						{
							t     = t0;
							found = qtrue;
						}
						else
						{
							t     = t1;
							found = qtrue;
						}
					}
					else
					{
						if (t0 > t1 && (t0 >= 0.f && t0 <= 1.f))
						{
							t     = t0;
							found = qtrue;
						}
						else
						{
							t     = t1;
							found = qtrue;
						}
					}
				}

				if (found)
				{
					if (t >= 0.f && t <= 1.f)
					{
						*deltaTime = (i / (float)(MAX_SPLINE_SEGMENTS)) + (t / (float)(MAX_SPLINE_SEGMENTS));
						VectorMA((*pSpline)->segments[i].start, t * (*pSpline)->segments[i].length, (*pSpline)->segments[i].v_norm, result);
						return;
					}
				}
				found = qfalse;
			}

			first = qfalse;
			if (radius < 0)
			{
				i--;
				if (i < 0)
				{
					i = MAX_SPLINE_SEGMENTS - 1;
					break;
				}
			}
			else
			{
				i++;
				if (i >= MAX_SPLINE_SEGMENTS)
				{
					i = 0;
					break;
				}
			}
		}

		if (radius < 0)
		{
			if (!(*pSpline)->prev)
			{
				return;
//				Com_Error( ERR_DROP, "End of spline reached (%s)\n", start->point.name );
			}
			*pSpline = (*pSpline)->prev;
		}
		else
		{
			if (!(*pSpline)->next)
			{
				return;
//				Com_Error( ERR_DROP, "End of spline reached (%s)\n", start->point.name );
			}
			*pSpline = (*pSpline)->next;
		}
	}
}
#endif

#if defined(_WIN32) || defined(__linux__)
typedef char tceComputeSegmentLayout838[(offsetof(splineSegment_t, start) == 0 && offsetof(splineSegment_t, v_norm) == 12 && offsetof(splineSegment_t, length) == 24 && sizeof(splineSegment_t) == 28 && MAX_SPLINE_SEGMENTS == 16) ? 1 : -1];
void BG_ComputeSegments(splinePath_t *pSpline)
{
	vec3_t polygon[4], endPoint;
	int    i;

	for (i = 0; i < MAX_SPLINE_SEGMENTS; ++i)
	{
		splineSegment_t *segment      = &pSpline->segments[i];
		volatile vec_t  *start        = segment->start;
		volatile vec_t  *direction    = segment->v_norm;
		volatile vec_t  *storedEnd    = endPoint;
		volatile float  *storedLength = &segment->length;
		float           parameter     = (float)((long double)i * 0.0625f);
		float           nextParameter;

		BG_CalculateSpline_r(pSpline, polygon[0], polygon[1], parameter);
#if defined(_WIN32)
		/* Windows reloads the three rounded differences from the segment. */
		start[0] = (float)((long double)polygon[1][0] - polygon[0][0]);
		start[1] = (float)((long double)polygon[1][1] - polygon[0][1]);
		start[2] = (float)((long double)polygon[1][2] - polygon[0][2]);
		{
			long double scaled = (long double)parameter * start[0];
			start[0] = (float)(scaled + polygon[0][0]);
			scaled   = (long double)parameter * start[1];
			start[1] = (float)(scaled + polygon[0][1]);
			scaled   = (long double)parameter * start[2];
			start[2] = (float)(scaled + polygon[0][2]);
		}
#else
		{
			long double dx = (long double)polygon[1][0] - polygon[0][0];
			long double dy, dz, scaledX, scaledY, scaledZ, resultX;
			start[0] = (float)dx;
			dy       = (long double)polygon[1][1] - polygon[0][1];
			start[1] = (float)dy;
			dz       = (long double)polygon[1][2] - polygon[0][2];
			start[2] = (float)dz;
			/* Linux writes these copies but multiplies the held differences. */
			scaledX  = dx * parameter;
			scaledY  = dy * parameter;
			resultX  = scaledX + polygon[0][0];
			scaledZ  = (long double)parameter * dz;
			start[0] = (float)resultX;
			start[1] = (float)(scaledY + polygon[0][1]);
			start[2] = (float)(scaledZ + polygon[0][2]);
		}
#endif
		nextParameter = (float)((long double)(i + 1) * 0.0625f);
		BG_CalculateSpline_r(pSpline, polygon[2], polygon[3], nextParameter);
#if defined(_WIN32)
		{
			long double    dx     = (long double)polygon[3][0] - polygon[2][0];
			volatile float dy     = (float)((long double)polygon[3][1] - polygon[2][1]);
			volatile float dz     = (float)((long double)polygon[3][2] - polygon[2][2]);
			long double    scaled = (long double)nextParameter * dx;
			storedEnd[0] = (float)(scaled + polygon[2][0]);
			scaled       = (long double)nextParameter * dy;
			storedEnd[1] = (float)(scaled + polygon[2][1]);
			scaled       = (long double)nextParameter * dz;
			storedEnd[2] = (float)(scaled + polygon[2][2]);
			direction[0] = (float)((long double)storedEnd[0] - start[0]);
			direction[1] = (float)((long double)storedEnd[1] - start[1]);
			direction[2] = (float)((long double)storedEnd[2] - start[2]);
		}
#else
		{
			long double x = polygon[2][0], y = polygon[2][1], z = polygon[2][2];
			long double nextX = polygon[3][0], nextY = polygon[3][1], nextZ = polygon[3][2];
			long double dx = nextX - x, dy = nextY - y, dz = nextZ - z;
			long double scaledX = dx * nextParameter;
			long double scaledY = dy * nextParameter;
			long double scaledZ = (long double)nextParameter * dz;
			long double resultX = x + scaledX;
			long double resultY = y + scaledY;
			long double resultZ = z + scaledZ;
			storedEnd[0] = (float)resultX;
			storedEnd[1] = (float)resultY;
			storedEnd[2] = (float)resultZ;
			/* Only X remains held for the final subtraction on Linux. */
			direction[0] = (float)(resultX - start[0]);
			direction[1] = (float)((long double)storedEnd[1] - start[1]);
			direction[2] = (float)((long double)storedEnd[2] - start[2]);
		}
#endif
		*storedLength = (float)TCE_VectorLengthValue(segment->v_norm);
		(void)VectorNormalize(segment->v_norm);
	}
}
#else
void BG_ComputeSegments(splinePath_t *pSpline)
{
	int    i;
	float  granularity = 1 / ((float)(MAX_SPLINE_SEGMENTS));
	vec3_t vec[4];

	for ( i = 0; i < MAX_SPLINE_SEGMENTS; i++ )
	{
		BG_CalculateSpline_r(pSpline, vec[0], vec[1], i * granularity);
		VectorSubtract(vec[1], vec[0], pSpline->segments[i].start);
		VectorMA(vec[0], i * granularity, pSpline->segments[i].start, pSpline->segments[i].start);

		BG_CalculateSpline_r(pSpline, vec[2], vec[3], (i + 1) * granularity);
		VectorSubtract(vec[3], vec[2], vec[0]);
		VectorMA(vec[2], (i + 1) * granularity, vec[0], vec[0]);

		VectorSubtract(vec[0], pSpline->segments[i].start, pSpline->segments[i].v_norm);
		pSpline->segments[i].length = VectorLength(pSpline->segments[i].v_norm);
		VectorNormalize(pSpline->segments[i].v_norm);
	}
}
#endif

/*
================
BG_EvaluateTrajectory

================
*/
void BG_EvaluateTrajectory(const trajectory_t *tr, int atTime, vec3_t result, qboolean isAngle, int splinePath)
{
	float  deltaTime;
	float  phase;
	vec3_t v;

	splinePath_t *pSpline;
	vec3_t       vec[2];
	qboolean     backwards = qfalse;
	float        deltaTime2;

#if defined(CGAMEDLL) || defined(GAMEDLL) || defined(UIDLL)
	/* Script producers and linked-entity consumers use original TC path16.
	 * All runtime types now share the original six-argument evaluator;
	 * legacy five-argument callers have the original default gravity scale1. */
	{
		TCE_BG_EvaluateTrajectory(tr, atTime, result, isAngle, splinePath, 1.0f);
		return;
	}
#endif

	switch (tr->trType)
	{
	case TR_STATIONARY:
	case TR_INTERPOLATE:
	case TR_GRAVITY_PAUSED: //----(SA)
		VectorCopy(tr->trBase, result);
		break;
	case TR_LINEAR:
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
		break;
	case TR_SINE:
		deltaTime = (atTime - tr->trTime) / (float) tr->trDuration;
		phase     = sin(deltaTime * M_PI * 2);
		VectorMA(tr->trBase, phase, tr->trDelta, result);
		break;
//----(SA)	removed
	case TR_LINEAR_STOP:
		if (atTime > tr->trTime + tr->trDuration)
		{
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		if (deltaTime < 0)
		{
			deltaTime = 0;
		}
		VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
		break;
	case TR_GRAVITY:
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
		result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;     // FIXME: local gravity...
		break;
	// Ridah
	case TR_GRAVITY_LOW:
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
		result[2] -= 0.5 * (DEFAULT_GRAVITY * 0.3) * deltaTime * deltaTime;     // FIXME: local gravity...
		break;
	// done.
//----(SA)
	case TR_GRAVITY_FLOAT:
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
		result[2] -= 0.5 * (DEFAULT_GRAVITY * 0.2) * deltaTime;
		break;
//----(SA)	end
	// RF, acceleration
	case TR_ACCELERATE:     // trDelta is the ultimate speed
		if (atTime > tr->trTime + tr->trDuration)
		{
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		// phase is the acceleration constant
		phase = VectorLength(tr->trDelta) / (tr->trDuration * 0.001);
		// trDelta at least gives us the acceleration direction
		VectorNormalize2(tr->trDelta, result);
		// get distance travelled at current time
		VectorMA(tr->trBase, phase * 0.5 * deltaTime * deltaTime, result, result);
		break;
	case TR_DECCELERATE:    // trDelta is the starting speed
		if (atTime > tr->trTime + tr->trDuration)
		{
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		// phase is the breaking constant
		phase = VectorLength(tr->trDelta) / (tr->trDuration * 0.001);
		// trDelta at least gives us the acceleration direction
		VectorNormalize2(tr->trDelta, result);
		// get distance travelled at current time (without breaking)
		VectorMA(tr->trBase, deltaTime, tr->trDelta, v);
		// subtract breaking force
		VectorMA(v, -phase * 0.5 * deltaTime * deltaTime, result, result);
		break;
	case TR_SPLINE:
		if (!(pSpline = BG_GetSplineData(splinePath, &backwards)))
		{
			return;
		}

		deltaTime = tr->trDuration ? (atTime - tr->trTime) / ((float)tr->trDuration) : 0;

		if (deltaTime < 0.f)
		{
			deltaTime = 0.f;
		}
		else if (deltaTime > 1.f)
		{
			deltaTime = 1.f;
		}

		if (backwards)
		{
			deltaTime = 1 - deltaTime;
		}

/*		if(pSpline->isStart) {
			deltaTime = 1 - sin((1 - deltaTime) * M_PI * 0.5f);
		} else if(pSpline->isEnd) {
			deltaTime = sin(deltaTime * M_PI * 0.5f);
		}*/

		deltaTime2 = deltaTime;

		BG_CalculateSpline_r(pSpline, vec[0], vec[1], deltaTime);

		if (isAngle)
		{
			qboolean dampin  = qfalse;
			qboolean dampout = qfalse;
			float    base1;

			if (tr->trBase[0])
			{
//				int pos = 0;
				vec3_t       result2;
				splinePath_t *pSp2 = pSpline;

				deltaTime2 += tr->trBase[0] / pSpline->length;

				if (BG_TraverseSpline(&deltaTime2, &pSp2))
				{

					VectorSubtract(vec[1], vec[0], result);
					VectorMA(vec[0], deltaTime, result, result);

					BG_CalculateSpline_r(pSp2, vec[0], vec[1], deltaTime2);

					VectorSubtract(vec[1], vec[0], result2);
					VectorMA(vec[0], deltaTime2, result2, result2);

					if (tr->trBase[0] < 0)
					{
						VectorSubtract(result, result2, result);
					}
					else
					{
						VectorSubtract(result2, result, result);
					}
				}
				else
				{
					VectorSubtract(vec[1], vec[0], result);
				}
			}
			else
			{
				VectorSubtract(vec[1], vec[0], result);
			}

			vectoangles(result, result);

			base1 = tr->trBase[1];
			if (base1 >= 10000 || base1 < -10000)
			{
				dampin = qtrue;
				if (base1 < 0)
				{
					base1 += 10000;
				}
				else
				{
					base1 -= 10000;
				}
			}

			if (base1 >= 1000 || base1 < -1000)
			{
				dampout = qtrue;
				if (base1 < 0)
				{
					base1 += 1000;
				}
				else
				{
					base1 -= 1000;
				}
			}

			if (dampin && dampout)
			{
				result[ROLL] = base1 + ((sin(((deltaTime * 2) - 1) * M_PI * 0.5f) + 1) * 0.5f * tr->trBase[2]);
			}
			else if (dampin)
			{
				result[ROLL] = base1 + (sin(deltaTime * M_PI * 0.5f) * tr->trBase[2]);
			}
			else if (dampout)
			{
				result[ROLL] = base1 + ((1 - sin((1 - deltaTime) * M_PI * 0.5f)) * tr->trBase[2]);
			}
			else
			{
				result[ROLL] = base1 + (deltaTime * tr->trBase[2]);
			}
		}
		else
		{
			VectorSubtract(vec[1], vec[0], result);
			VectorMA(vec[0], deltaTime, result, result);
		}

		break;
	case TR_LINEAR_PATH:
		if (!(pSpline = BG_GetSplineData(splinePath, &backwards)))
		{
			return;
		}

		deltaTime = tr->trDuration ? (atTime - tr->trTime) / ((float)tr->trDuration) : 0;

		if (deltaTime < 0.f)
		{
			deltaTime = 0.f;
		}
		else if (deltaTime > 1.f)
		{
			deltaTime = 1.f;
		}

		if (backwards)
		{
			deltaTime = 1 - deltaTime;
		}

		if (isAngle)
		{
			int   pos = floor(deltaTime * (MAX_SPLINE_SEGMENTS));
			float frac;

			if (pos >= MAX_SPLINE_SEGMENTS)
			{
				pos  = MAX_SPLINE_SEGMENTS - 1;
				frac = pSpline->segments[pos].length;
			}
			else
			{
				frac = ((deltaTime * (MAX_SPLINE_SEGMENTS)) - pos) * pSpline->segments[pos].length;
			}

			if (tr->trBase[0])
			{
				VectorMA(pSpline->segments[pos].start, frac, pSpline->segments[pos].v_norm, result);
				VectorCopy(result, v);

				BG_LinearPathOrigin2(tr->trBase[0], &pSpline, &deltaTime, v, backwards);
				if (tr->trBase[0] < 0)
				{
					VectorSubtract(v, result, result);
				}
				else
				{
					VectorSubtract(result, v, result);
				}

				vectoangles(result, result);
			}
			else
			{
				vectoangles(pSpline->segments[pos].v_norm, result);
			}

		}
		else
		{
			int   pos = floor(deltaTime * (MAX_SPLINE_SEGMENTS));
			float frac;

			if (pos >= MAX_SPLINE_SEGMENTS)
			{
				pos  = MAX_SPLINE_SEGMENTS - 1;
				frac = pSpline->segments[pos].length;
			}
			else
			{
				frac = ((deltaTime * (MAX_SPLINE_SEGMENTS)) - pos) * pSpline->segments[pos].length;
			}

			VectorMA(pSpline->segments[pos].start, frac, pSpline->segments[pos].v_norm, result);
		}

		break;
	default:
		Com_Error(ERR_DROP, "BG_EvaluateTrajectory: unknown trType: %i", tr->trTime);
		break;
	}
}


/*
================
BG_EvaluateTrajectoryDelta

For determining velocity at a given time
================
*/
void BG_EvaluateTrajectoryDelta(const trajectory_t *tr, int atTime, vec3_t result, qboolean isAngle, int splineData)
{
	float deltaTime;
	float phase;

#if defined(CGAMEDLL) || defined(GAMEDLL) || defined(UIDLL)
	/* Script producers and linked-entity consumers use original TC path16.
	 * All runtime types now share the original six-argument evaluator;
	 * legacy five-argument callers have the original default gravity scale1. */
	{
		TCE_BG_EvaluateTrajectoryDelta(tr, atTime, result, isAngle, splineData, 1.0f);
		return;
	}
#endif

	switch (tr->trType)
	{
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorClear(result);
		break;
	case TR_LINEAR:
		VectorCopy(tr->trDelta, result);
		break;
	case TR_SINE:
		deltaTime = (atTime - tr->trTime) / (float) tr->trDuration;
		phase     = cos(deltaTime * M_PI * 2);  // derivative of sin = cos
		phase    *= 0.5;
		VectorScale(tr->trDelta, phase, result);
		break;
//----(SA)	removed
	case TR_LINEAR_STOP:
		if (atTime > tr->trTime + tr->trDuration)
		{
			VectorClear(result);
			return;
		}
		VectorCopy(tr->trDelta, result);
		break;
	case TR_GRAVITY:
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		VectorCopy(tr->trDelta, result);
		result[2] -= DEFAULT_GRAVITY * deltaTime;       // FIXME: local gravity...
		break;
	// Ridah
	case TR_GRAVITY_LOW:
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		VectorCopy(tr->trDelta, result);
		result[2] -= (DEFAULT_GRAVITY * 0.3) * deltaTime;       // FIXME: local gravity...
		break;
	// done.
//----(SA)
	case TR_GRAVITY_FLOAT:
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		VectorCopy(tr->trDelta, result);
		result[2] -= (DEFAULT_GRAVITY * 0.2) * deltaTime;
		break;
//----(SA)	end
	// RF, acceleration
	case TR_ACCELERATE: // trDelta is eventual speed
		if (atTime > tr->trTime + tr->trDuration)
		{
			VectorClear(result);
			return;
		}
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		phase     = deltaTime / (float)tr->trDuration;
		VectorScale(tr->trDelta, deltaTime * deltaTime, result);
		break;
	case TR_DECCELERATE:    // trDelta is breaking force
		if (atTime > tr->trTime + tr->trDuration)
		{
			VectorClear(result);
			return;
		}
		deltaTime = (atTime - tr->trTime) * 0.001;      // milliseconds to seconds
		VectorScale(tr->trDelta, deltaTime, result);
		break;
	case TR_SPLINE:
	case TR_LINEAR_PATH:
		VectorClear(result);
		break;
	default:
		Com_Error(ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trTime);
		break;
	}
}

/*
============
BG_GetMarkDir

  used to find a good directional vector for a mark projection, which will be more likely
  to wrap around adjacent surfaces

  dir is the direction of the projectile or trace that has resulted in a surface being hit
============
*/
void BG_GetMarkDir(const vec3_t dir, const vec3_t normal, vec3_t result)
{
	vec3_t ndir, lnormal;
	float  minDot = 0.3f;
	int    x = 0, component, normalFallback;

	/* Carry includes unordered components in this early-return condition. */
	if (!(dir[0] >= 0.001) && !(dir[1] >= 0.001))
	{
#if defined(_WIN32)
		for (component = 0; component < 3; ++component)
		{
			unsigned int bits;
			memcpy(&bits, &dir[component], sizeof(bits));
			memcpy(&result[component], &bits, sizeof(bits));
		}
#elif defined(__linux__)
		volatile long double first = (long double)dir[0];
		result[0] = (float)first;
		for (component = 1; component < 3; ++component)
		{
			unsigned int bits;
			memcpy(&bits, &dir[component], sizeof(bits));
			memcpy(&result[component], &bits, sizeof(bits));
		}
#else
		VectorCopy(dir, result);
#endif
		return;
	}

#if defined(_WIN32) || defined(__linux__)
	{
		long double squared = TCE_VectorLengthSquaredValue(normal);
#if defined(_WIN32)
		normalFallback = !(squared >= 1.0L);
#else
		/* Linux normalizes unordered squared lengths instead of falling back. */
		normalFallback = squared < 1.0L;
#endif
	}
#else
	normalFallback = VectorLengthSquared(normal) < 1.0f;
#endif
	if (normalFallback)
	{
		VectorSet(lnormal, 0.0f, 0.0f, 1.0f);
	}
	else
	{
		(void)VectorNormalize2(normal, lnormal);
	}

	for (component = 0; component < 3; ++component)
	{
#if defined(_WIN32)
		volatile long double value = (long double)dir[component];
		ndir[component] = (float)-value;
#elif defined(__linux__)
		unsigned int bits;
		memcpy(&bits, &dir[component], sizeof(bits));
		bits ^= 0x80000000u;
		memcpy(&ndir[component], &bits, sizeof(bits));
#else
		ndir[component] = -dir[component];
#endif
	}
	(void)VectorNormalize(ndir);
	if (normal[2] > 0.8f)
	{
		minDot = 0.7f;
	}

	for (;;)
	{
#if defined(_WIN32) || defined(__linux__)
		long double productX, productY, productZ, dot;
#if defined(_WIN32)
		productZ = (long double)ndir[2] * (long double)lnormal[2];
		productY = (long double)ndir[1] * (long double)lnormal[1];
		dot      = productZ + productY;
		productX = (long double)ndir[0] * (long double)lnormal[0];
		dot      = dot + productX;
#else
		productX = (long double)ndir[0] * (long double)lnormal[0];
		productY = (long double)ndir[1] * (long double)lnormal[1];
		dot      = productY + productX;
		productZ = (long double)ndir[2] * (long double)lnormal[2];
		dot      = productZ + dot;
#endif
		/* Evaluate the dot before the ten-update cap, as in the originals. */
		if (dot >= (long double)minDot || x >= 10)
		{
			break;
		}
		for (component = 0; component < 3; ++component)
		{
			long double product = (long double)lnormal[component] * 0.5L;
			ndir[component] = (float)(product + (long double)ndir[component]);
		}
#else
		if (!(DotProduct(ndir, lnormal) < minDot) || x >= 10)
		{
			break;
		}
		VectorMA(ndir, 0.5, lnormal, ndir);
#endif
		(void)VectorNormalize(ndir);
		++x;
	}

#ifdef GAMEDLL
	if (x >= 10)
	{
		if (g_developer.integer)
		{
			Com_Printf("BG_GetMarkDir loops: %i\n", x);
		}
	}
#endif // GAMEDLL

#if defined(_WIN32)
	{
		volatile long double first = (long double)ndir[0];
		result[0] = (float)first;
		for (component = 1; component < 3; ++component)
		{
			unsigned int bits;
			memcpy(&bits, &ndir[component], sizeof(bits));
			memcpy(&result[component], &bits, sizeof(bits));
		}
	}
#elif defined(__linux__)
	{
		volatile long double z     = (long double)ndir[2];
		volatile long double y     = (long double)ndir[1];
		volatile long double first = (long double)ndir[0];
		result[0] = (float)first;
		result[1] = (float)y;
		result[2] = (float)z;
	}
#else
	VectorCopy(ndir, result);
#endif
}


char *eventnames[] =
{
	"EV_NONE",
	"EV_FOOTSTEP",
	"EV_FOOTSTEP_METAL",
	"EV_FOOTSTEP_WOOD",
	"EV_FOOTSTEP_GRASS",
	"EV_FOOTSTEP_GRAVEL",
	"EV_FOOTSTEP_ROOF",
	"EV_FOOTSTEP_SNOW",
	"EV_FOOTSTEP_CARPET",
	"EV_FOOTSPLASH",
	"EV_FOOTWADE",
	"EV_SWIM",
	"EV_STEP_4",
	"EV_STEP_8",
	"EV_STEP_12",
	"EV_STEP_16",
	"EV_FALL_SHORT",
	"EV_FALL_MEDIUM",
	"EV_FALL_FAR",
	"EV_FALL_NDIE",
	"EV_FALL_DMG_10",
	"EV_FALL_DMG_15",
	"EV_FALL_DMG_25",
	"EV_FALL_DMG_50",
	"EV_JUMP",
	"EV_WATER_TOUCH",
	"EV_WATER_LEAVE",
	"EV_WATER_UNDER",
	"EV_WATER_CLEAR",
	"EV_ITEM_PICKUP",
	"EV_ITEM_PICKUP_QUIET",
	"EV_GLOBAL_ITEM_PICKUP",
	"EV_NOAMMO",
	"EV_WEAPONSWITCHED",
	"EV_EMPTYCLIP",
	"EV_FILL_CLIP",
	"EV_MG42_FIXED",
	"EV_WEAP_OVERHEAT",
	"EV_CHANGE_WEAPON",
	"EV_CHANGE_WEAPON_2",
	"EV_FIRE_WEAPON",
	"EV_FIRE_WEAPONB",
	"EV_FIRE_WEAPON_LASTSHOT",
	"EV_NOFIRE_UNDERWATER",
	"EV_FIRE_WEAPON_MG42",
	"EV_FIRE_WEAPON_MOUNTEDMG42",
	"EV_ITEM_RESPAWN",
	"EV_ITEM_POP",
	"EV_PLAYER_TELEPORT_IN",
	"EV_PLAYER_TELEPORT_OUT",
	"EV_GRENADE_BOUNCE",
	"EV_GENERAL_SOUND",
	"EV_GENERAL_SOUND_VOLUME",
	"EV_GLOBAL_SOUND",
	"EV_GLOBAL_CLIENT_SOUND",
	"EV_GLOBAL_TEAM_SOUND",
	"EV_FX_SOUND",
	"EV_BULLET_HIT_FLESH",
	"EV_BULLET_HIT_WALL",
	"EV_MISSILE_HIT",
	"EV_MISSILE_MISS",
	"EV_RAILTRAIL",
	"EV_VENOM",
	"EV_BULLET",
	"EV_LOSE_HAT",
	"EV_PAIN",
	"EV_CROUCH_PAIN",
	"EV_DEATH1",
	"EV_DEATH2",
	"EV_DEATH3",
	"EV_OBITUARY",
	"EV_STOPSTREAMINGSOUND",
	"EV_POWERUP_QUAD",
	"EV_POWERUP_BATTLESUIT",
	"EV_POWERUP_REGEN",
	"EV_GIB_PLAYER",
	"EV_DEBUG_LINE",
	"EV_STOPLOOPINGSOUND",
	"EV_TAUNT",
	"EV_SMOKE",
	"EV_SPARKS",
	"EV_SPARKS_ELECTRIC",
	"EV_EXPLODE",
	"EV_RUBBLE",
	"EV_EFFECT",
	"EV_MORTAREFX",
	"EV_SPINUP",
	"EV_SNOW_ON",
	"EV_SNOW_OFF",
	"EV_MISSILE_MISS_SMALL",
	"EV_MISSILE_MISS_LARGE",
	"EV_MORTAR_IMPACT",
	"EV_MORTAR_MISS",
	"EV_SHARD",
	"EV_JUNK",
	"EV_EMITTER",
	"EV_OILPARTICLES",
	"EV_OILSLICK",
	"EV_OILSLICKREMOVE",
	"EV_MG42EFX",
	"EV_FLAKGUN1",
	"EV_FLAKGUN2",
	"EV_FLAKGUN3",
	"EV_FLAKGUN4",
	"EV_EXERT1",
	"EV_EXERT2",
	"EV_EXERT3",
	"EV_SNOWFLURRY",
	"EV_CONCUSSIVE",
	"EV_DUST",
	"EV_RUMBLE_EFX",
	"EV_GUNSPARKS",
	"EV_FLAMETHROWER_EFFECT",
	"EV_POPUP",
	"EV_POPUPBOOK",
	"EV_GIVEPAGE",
	"EV_MG42BULLET_HIT_FLESH",
	"EV_MG42BULLET_HIT_WALL",
	"EV_SHAKE",
	"EV_DISGUISE_SOUND",
	"EV_BUILDDECAYED_SOUND",
	"EV_FIRE_WEAPON_AAGUN",
	"EV_DEBRIS",
	"EV_ALERT_SPEAKER",
	"EV_POPUPMESSAGE",
	"EV_ARTYMESSAGE",
	"EV_AIRSTRIKEMESSAGE",
	"EV_MEDIC_CALL",
	"EV_TCE_RELOAD_CYCLE",
	"EV_TCE_RELOAD_PUMP",
	"EV_TCE_RELOAD_PUMP2",
	"EV_TCE_RELOAD_BOLT",
	"EV_TCE_FIREMODE",
	"EV_TCE_TOGGLE_AIMING",
	"EV_TCE_SHOTGUN",
	"EV_TCE_PLANT",
	"EV_TCE_DEFUSE",
	"EV_TCE_OBJECTIVE_START",
	"EV_TCE_OBJECTIVE_STOP",
	"EV_TCE_OBJECTIVE_COMPLETE",
	"EV_TCE_FALL_DMG_75",
	"EV_TCE_BULLET_PIERCED_WALL",
	"EV_TCE_BULLET_NEAR_MISS",
	"EV_TCE_FOOTSTEP_SPRINT",
	"EV_TCE_FOOTSTEP_WALK",
	"EV_TCE_FENCE_TOUCH",
	"EV_MAX_EVENTS",
};

/*
===============
BG_AddPredictableEventToPlayerstate

Handles the sequence numbers
===============
*/

void    trap_Cvar_VariableStringBuffer(const char *var_name, char *buffer, int bufsize);

void BG_AddPredictableEventToPlayerstate(int newEvent, int eventParm, playerState_t *ps)
{

#ifdef _DEBUG
	{
		char buf[256];
		trap_Cvar_VariableStringBuffer("showevents", buf, sizeof(buf));
		if (atof(buf) != 0)
		{
#ifdef QAGAME
			Com_Printf(" game event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount /*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#else
			Com_Printf("Cgame event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount /*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#endif
		}
	}
#endif
	ps->events[ps->eventSequence & (MAX_EVENTS - 1)]     = newEvent;
	ps->eventParms[ps->eventSequence & (MAX_EVENTS - 1)] = eventParm;
	ps->eventSequence++;
}

// Gordon: would like to just inline this but would likely break qvm support
#define SETUP_MOUNTEDGUN_STATUS(ps)                           \
		switch (ps->persistant[PERS_HWEAPON_USE]) {                \
		case 1:                                                 \
			ps->eFlags                    |= EF_MG42_ACTIVE;                       \
			ps->eFlags                    &= ~EF_AAGUN_ACTIVE;                     \
			ps->powerups[PW_OPS_DISGUISED] = 0;                 \
			break;                                              \
		case 2:                                                 \
			ps->eFlags                    |= EF_AAGUN_ACTIVE;                      \
			ps->eFlags                    &= ~EF_MG42_ACTIVE;                      \
			ps->powerups[PW_OPS_DISGUISED] = 0;                 \
			break;                                              \
		default:                                                \
			ps->eFlags &= ~EF_MG42_ACTIVE;                      \
			ps->eFlags &= ~EF_AAGUN_ACTIVE;                     \
			break;                                              \
		}

/*
========================
BG_PlayerStateToEntityState

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
/* Shared tail of TC2002d350/2002d7b0: equipment and body-damage wire fields. */
static void BG_TCEPlayerStateFields(const playerState_t *ps, entityState_t *s)
{
	int vip       = ps->stats[STAT_TCE_FLAGS] & 0x100;
	int alternate = ps->stats[STAT_TCE_FLAGS] & 0x400;
	int i;
	s->angles2[ROLL] = ps->leanf;
	/* TCE2 freelook uses unused player angles components, preserving TC's
	 * angles2 movementDir/lean protocol. Both converters share this tail. */
	s->angles[PITCH] = ps->holdable[TCE_FREELOOK_PITCH] ? (ps->holdable[TCE_FREELOOK_PITCH] - TCE_FREELOOK_CENTER) * .01f : 0;
	s->angles[YAW]   = ps->holdable[TCE_FREELOOK_YAW] ? (ps->holdable[TCE_FREELOOK_YAW] - TCE_FREELOOK_CENTER) * .01f : 0;
	s->effect1Time   = 0;
	if (ps->pm_flags & 1)
	{
		s->effect1Time = 2;
	}
	if (ps->eFlags & 0x80000)
	{
		s->effect1Time |= 4;
	}
	if (ps->pm_flags & 4)
	{
		s->effect1Time |= 0x400;
	}
	if (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x20)
	{
		s->effect1Time |= 8;
	}
	if (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x100)
	{
		s->effect1Time |= 0x10;
	}
	if (vip)
	{
		s->effect1Time |= 0x20;
	}
	if (alternate)
	{
		s->effect1Time |= 0x40;
	}
	s->effect2Time = 0;
	if (ps->weapon != BG_WeaponOnBackToWeap(ps->holdable[10]) && !vip && !alternate)
	{
		s->effect2Time = ps->holdable[10];
	}
	s->effect3Time = ps->stats[15] & 0xff;
	s->modelindex2 = 0;
	for (i = 0; i < 3; ++i)
	{
		if (ps->holdable[i + 2] >= 67)
		{
			s->modelindex2 |= 4 << (2 * i);
		}
		else if (ps->holdable[i + 2] >= 34)
		{
			s->modelindex2 |= 2 << (2 * i);
		}
	}
}

/* The event sequence is a native wrapping 32-bit counter, not signed C
 * arithmetic. Copy its bits back without an out-of-range signed conversion. */
static int BG_SnapshotSequenceAdd(int sequence, unsigned increment)
{
	unsigned bits = (unsigned)sequence + increment;
	int      result;
	memcpy(&result, &bits, sizeof(result));
	return result;
}

/* Windows copies bits then reloads output for signed64/low32 truncation.
 * Linux retains the three input FP values across the initial stores and
 * truncates each to signed32. FPU exception/control-word parity stays open. */
static void BG_SnapshotVector(const vec3_t input, vec3_t output, qboolean snap)
{
	int component;
#if defined(_WIN32)
	for (component = 0; component < 3; ++component)
	{
		unsigned bits;
		memcpy(&bits, &input[component], sizeof(bits));
		memcpy(&output[component], &bits, sizeof(bits));
	}
	if (snap)
	{
		for (component = 0; component < 3; ++component)
			output[component] = (float)TCE_TruncateNativeInteger(output[component]);
	}
#elif defined(__linux__)
	long double held[3];
	for (component = 0; component < 3; ++component)
	{
		held[component]   = (long double)input[component];
		output[component] = (float)held[component];
	}
	if (snap)
	{
		for (component = 0; component < 3; ++component)
			output[component] = (float)TCE_TruncateNativeInteger(held[component]);
	}
#else
	VectorCopy(input, output);
	if (snap)
	{
		SnapVector(output);
	}
#endif
}

void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *s, qboolean snap)
{
	int      i;
	unsigned sequence;

	if (ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR) // || ps->pm_flags & PMF_LIMBO ) { // JPW NERVE limbo
	{
		s->eType = ET_INVISIBLE;
	}
	else if (ps->stats[STAT_HEALTH] <= GIB_HEALTH)
	{
		s->eType = ET_INVISIBLE;
	}
	else
	{
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_INTERPOLATE;
	BG_SnapshotVector(ps->origin, s->pos.trBase, snap);

	s->apos.trType = TR_INTERPOLATE;
	BG_SnapshotVector(ps->viewangles, s->apos.trBase, snap);

	/* FILD32, optional subtraction, then one float store. */
	if (ps->movementDir > 128)
	{
		s->angles2[YAW] = (float)((long double)ps->movementDir - 256.0L);
	}
	else
	{
		s->angles2[YAW] = (float)ps->movementDir;
	}

	s->legsAnim  = ps->legsAnim;
	s->torsoAnim = ps->torsoAnim;
	s->clientNum = ps->clientNum;       // ET_PLAYER looks here instead of at number
	                                    // so corpses can also reference the proper config
	// Ridah, let clients know if this person is using a mounted weapon
	// so they don't show any client muzzle flashes

	if (ps->eFlags & EF_MOUNTEDTANK)
	{
		ps->eFlags &= ~EF_MG42_ACTIVE;
		ps->eFlags &= ~EF_AAGUN_ACTIVE;
	}
	else
	{
		SETUP_MOUNTEDGUN_STATUS(ps);
	}

	s->eFlags = ps->eFlags;

	if (ps->stats[STAT_HEALTH] <= 0)
	{
		s->eFlags |= EF_DEAD;
	}
	else
	{
		s->eFlags &= ~EF_DEAD;
	}

// from MP
	if (ps->externalEvent)
	{
		s->event     = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	}
	else if (ps->entityEventSequence < ps->eventSequence)
	{
		int seq;

		if (ps->entityEventSequence < BG_SnapshotSequenceAdd(ps->eventSequence, 0u - MAX_EVENTS))
		{
			ps->entityEventSequence = BG_SnapshotSequenceAdd(ps->eventSequence, 0u - MAX_EVENTS);
		}
		seq                     = ps->entityEventSequence & (MAX_EVENTS - 1);
		s->event                = ps->events[seq] | ((ps->entityEventSequence & 3) << 8);
		s->eventParm            = ps->eventParms[seq];
		ps->entityEventSequence = BG_SnapshotSequenceAdd(ps->entityEventSequence, 1u);
	}
// end
	// Ridah, now using a circular list of events for all entities
	// add any new events that have been added to the playerState_t
	// (possibly overwriting entityState_t events)
	for (sequence = (unsigned)ps->oldEventSequence;
	     sequence != (unsigned)ps->eventSequence; ++sequence)
	{
		s->events[s->eventSequence & (MAX_EVENTS - 1)]     = ps->events[sequence & (MAX_EVENTS - 1)];
		s->eventParms[s->eventSequence & (MAX_EVENTS - 1)] = ps->eventParms[sequence & (MAX_EVENTS - 1)];
		s->eventSequence                                   = BG_SnapshotSequenceAdd(s->eventSequence, 1u);
	}
	ps->oldEventSequence = ps->eventSequence;

	s->weapon          = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ )
	{
		if (ps->powerups[i])
		{
			s->powerups |= 1 << i;
		}
	}

	s->nextWeapon = ps->nextWeapon; // Ridah
//	s->loopSound = ps->loopSound;
	s->teamNum = ps->teamNum;
	s->aiState = ps->aiState;       // xkan, 1/10/2003
	BG_TCEPlayerStateFields(ps, s);
}

/*
========================
BG_PlayerStateToEntityStateExtraPolate

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityStateExtraPolate(playerState_t *ps, entityState_t *s, int time, qboolean snap)
{
	int i;
	unsigned sequence;

	if (ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR) // || ps->pm_flags & PMF_LIMBO ) { // JPW NERVE limbo
	{
		s->eType = ET_INVISIBLE;
	}
	else if (ps->stats[STAT_HEALTH] <= GIB_HEALTH)
	{
		s->eType = ET_INVISIBLE;
	}
	else
	{
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_LINEAR_STOP;
	BG_SnapshotVector(ps->origin, s->pos.trBase, snap);
	// set the trDelta for flag direction and linear prediction
	VectorCopy(ps->velocity, s->pos.trDelta);
	// set the time for linear prediction
	s->pos.trTime = time;
	// set maximum extra polation time
	s->pos.trDuration = 50; // 1000 / sv_fps (default = 20)

	s->apos.trType = TR_INTERPOLATE;
	BG_SnapshotVector(ps->viewangles, s->apos.trBase, snap);

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim     = ps->legsAnim;
	s->torsoAnim    = ps->torsoAnim;
	s->clientNum    = ps->clientNum;    // ET_PLAYER looks here instead of at number
	                                    // so corpses can also reference the proper config

	if (ps->eFlags & EF_MOUNTEDTANK)
	{
		ps->eFlags &= ~EF_MG42_ACTIVE;
		ps->eFlags &= ~EF_AAGUN_ACTIVE;
	}
	else
	{
		SETUP_MOUNTEDGUN_STATUS(ps);
	}

	s->eFlags = ps->eFlags;
	if (ps->stats[STAT_HEALTH] <= 0)
	{
		s->eFlags |= EF_DEAD;
	}
	else
	{
		s->eFlags &= ~EF_DEAD;
	}

	if (ps->externalEvent)
	{
		s->event     = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	}
	else if (ps->entityEventSequence < ps->eventSequence)
	{
		int seq;

		if (ps->entityEventSequence < BG_SnapshotSequenceAdd(ps->eventSequence, 0u - MAX_EVENTS))
		{
			ps->entityEventSequence = BG_SnapshotSequenceAdd(ps->eventSequence, 0u - MAX_EVENTS);
		}
		seq          = ps->entityEventSequence & (MAX_EVENTS - 1);
		s->event     = ps->events[seq] | ((ps->entityEventSequence & 3) << 8);
		s->eventParm = ps->eventParms[seq];
		ps->entityEventSequence = BG_SnapshotSequenceAdd(ps->entityEventSequence, 1u);
	}

	// Ridah, now using a circular list of events for all entities
	// add any new events that have been added to the playerState_t
	// (possibly overwriting entityState_t events)
	for (sequence = (unsigned)ps->oldEventSequence;
	     sequence != (unsigned)ps->eventSequence; ++sequence)
	{
		s->events[s->eventSequence & (MAX_EVENTS - 1)]     = ps->events[sequence & (MAX_EVENTS - 1)];
		s->eventParms[s->eventSequence & (MAX_EVENTS - 1)] = ps->eventParms[sequence & (MAX_EVENTS - 1)];
		s->eventSequence = BG_SnapshotSequenceAdd(s->eventSequence, 1u);
	}
	ps->oldEventSequence = ps->eventSequence;

	s->weapon          = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ )
	{
		if (ps->powerups[i])
		{
			s->powerups |= 1 << i;
		}
	}

	s->nextWeapon = ps->nextWeapon; // Ridah
	s->teamNum    = ps->teamNum;
	s->aiState    = ps->aiState;    // xkan, 1/10/2003
	BG_TCEPlayerStateFields(ps, s);
}

// Gordon: some weapons are duplicated for code puposes.... just want to treat them as a single
weapon_t BG_DuplicateWeapon(weapon_t weap)
{
	switch ((int)weap)
	{
	/* TC:E wire IDs; the SDK enum labels collide with TC weapons. */
	case 56:    return 55;
	case 57:    return 25;
	case 58:    return 32;
	case 9:     return 4;
	default:                    return weap;
	}
}

gitem_t * BG_ValidStatWeapon(weapon_t weap)
{
	weapon_t weap2;

	switch (weap)
	{
	/* Original TC wire IDs (2002dca0/30005790), independent of SDK names. */
	case 19:
	case 21:
	case 16:
	case 11:
	case 12:
		return NULL;
	default:
		break;
	}

	if (!BG_WeaponInWolfMP(weap))
	{
		return NULL;
	}

	weap2 = BG_DuplicateWeapon(weap);
	if (weap != weap2)
	{
		return NULL;
	}

	return BG_FindItemForWeapon(weap);
}

weapon_t BG_WeaponForMOD(int MOD)
{
	weapon_t i;

	for (i = 0; i < MAX_WEAPONS; i++)
	{
		if (weaponDef[i].mod == MOD)
		{
			return i;
		}
	}

	return 0;
}

const char *rankSoundNames_Allies[NUM_EXPERIENCE_LEVELS] =
{
	"",
	"allies_hq_promo_private",
	"allies_hq_promo_corporal",
	"allies_hq_promo_sergeant",
	"allies_hq_promo_lieutenant",
	"allies_hq_promo_captain",
	"allies_hq_promo_major",
	"allies_hq_promo_colonel",
	"allies_hq_promo_general_brigadier",
	"allies_hq_promo_general_lieutenant",
	"allies_hq_promo_general",
};

const char *rankSoundNames_Axis[NUM_EXPERIENCE_LEVELS] =
{
	"",
	"axis_hq_promo_private",
	"axis_hq_promo_corporal",
	"axis_hq_promo_sergeant",
	"axis_hq_promo_lieutenant",
	"axis_hq_promo_captain",
	"axis_hq_promo_major",
	"axis_hq_promo_colonel",
	"axis_hq_promo_general_major",
	"axis_hq_promo_general_lieutenant",
	"axis_hq_promo_general",
};

const char *rankNames_Axis[NUM_EXPERIENCE_LEVELS] =
{
	"Schutze",
	"Oberschutze",
	"Gefreiter",
	"Feldwebel",
	"Leutnant",
	"Hauptmann",
	"Major",
	"Oberst",
	"Generalmajor",
	"Generalleutnant",
	"General",
};

const char *rankNames_Allies[NUM_EXPERIENCE_LEVELS] =
{
	"Private",
	"Private 1st Class",
	"Corporal",
	"Sergeant",
	"Lieutenant",
	"Captain",
	"Major",
	"Colonel",
	"Brigadier General",
	"Lieutenant General",
	"General",
};




const char *miniRankNames_Axis[NUM_EXPERIENCE_LEVELS] =
{
	"Stz",
	"Otz",
	"Gfr",
	"Fwb",
	"Ltn",
	"Hpt",
	"Mjr",
	"Obs",
	"BGn",
	"LtG",
	"Gen",
};

const char *miniRankNames_Allies[NUM_EXPERIENCE_LEVELS] =
{
	"Pvt",
	"PFC",
	"Cpl",
	"Sgt",
	"Lt",
	"Cpt",
	"Maj",
	"Cnl",
	"GMj",
	"GLt",
	"Gen",
};

/*
=============
BG_Find_PathCorner
=============
*/
pathCorner_t *BG_Find_PathCorner(const char *match)
{
	int i;

	for (i = 0 ; i < numPathCorners; i++)
	{
		if (!Q_stricmp(pathCorners[i].name, match))
		{
			return &pathCorners[i];
		}
	}

	return NULL;
}

/*
=============
BG_AddPathCorner
=============
*/
void BG_AddPathCorner(const char *name, vec3_t origin)
{
	if (numPathCorners >= MAX_PATH_CORNERS)
	{
		Com_Error(ERR_DROP, "MAX PATH CORNERS (%i) hit", MAX_PATH_CORNERS);
	}

	VectorCopy(origin, pathCorners[numPathCorners].origin);
	Q_strncpyz(pathCorners[numPathCorners].name, name, 64);
	numPathCorners++;
}

/*
=============
BG_Find_Spline
=============
*/
splinePath_t *BG_Find_Spline(const char *match)
{
	int i;

	for (i = 0 ; i < numSplinePaths; i++)
	{
		if (!Q_stricmp(splinePaths[i].point.name, match))
		{
			return &splinePaths[i];
		}
	}

	return NULL;
}

splinePath_t * BG_AddSplinePath(const char *name, const char *target, vec3_t origin)
{
	splinePath_t *spline;
	if (numSplinePaths >= MAX_SPLINE_PATHS)
	{
		Com_Error(ERR_DROP, "MAX SPLINES (%i) hit", MAX_SPLINE_PATHS);
	}

	spline = &splinePaths[numSplinePaths];

	memset(spline, 0, sizeof(splinePath_t));

	VectorCopy(origin, spline->point.origin);

	Q_strncpyz(spline->point.name, name, 64);
	Q_strncpyz(spline->strTarget, target ? target : "", 64);

	spline->numControls = 0;

	numSplinePaths++;

	return spline;
}

void BG_AddSplineControl(splinePath_t *spline, const char *name)
{
	if (spline->numControls >= MAX_SPLINE_CONTROLS)
	{
		Com_Error(ERR_DROP, "MAX SPLINE CONTROLS (%i) hit", MAX_SPLINE_CONTROLS);
	}

	Q_strncpyz(spline->controls[spline->numControls].name, name, 64);

	spline->numControls++;
}

#if defined(_WIN32) || defined(__linux__)
float BG_SplineLength(splinePath_t *pSpline)
{
	vec3_t         polygon[2], point, previousPoint, delta;
	volatile vec_t *storedPoint  = point;
	volatile vec_t *storedDelta  = delta;
	volatile float parameter     = 0.0f;
	volatile float distance      = 0.0f;
	long double    nextParameter = 0.0L;

	do
	{
		long double pointX, pointY, pointZ;
		qboolean    sampleDistance;
#if defined(__linux__)
		/* Linux stores the held loop value only when starting another probe. */
		parameter = (float)nextParameter;
#endif
		BG_CalculateSpline_r(pSpline, polygon[0], polygon[1], parameter);
#if defined(_WIN32)
		{
			long double    dx     = (long double)polygon[1][0] - polygon[0][0];
			volatile float dy     = (float)((long double)polygon[1][1] - polygon[0][1]);
			volatile float dz     = (float)((long double)polygon[1][2] - polygon[0][2]);
			long double    scaled = dx * parameter;
			pointX         = scaled + polygon[0][0];
			storedPoint[0] = (float)pointX;
			scaled         = (long double)dy * parameter;
			pointY         = scaled + polygon[0][1];
			storedPoint[1] = (float)pointY;
			scaled         = (long double)dz * parameter;
			pointZ         = scaled + polygon[0][2];
			storedPoint[2] = (float)pointZ;
			sampleDistance = !TCE_FloatIsZeroOrNaN(parameter);
		}
#else
		{
			long double x = polygon[0][0], y = polygon[0][1], z = polygon[0][2];
			long double nextX = polygon[1][0], nextY = polygon[1][1], nextZ = polygon[1][2];
			long double dx      = nextX - x;
			long double dy      = nextY - y;
			long double dz      = nextZ - z;
			long double scaledX = dx * parameter;
			long double scaledY = dy * parameter;
			long double scaledZ = dz * parameter;
			pointX         = x + scaledX;
			pointY         = y + scaledY;
			sampleDistance = !TCE_FloatIsZeroOrNaN(parameter);
			pointZ         = z + scaledZ;
			storedPoint[0] = (float)pointX;
			storedPoint[1] = (float)pointY;
			storedPoint[2] = (float)pointZ;
		}
#endif
		if (sampleDistance)
		{
			long double segmentLength;
#if defined(_WIN32)
			storedDelta[0] = (float)((long double)storedPoint[0] - previousPoint[0]);
			storedDelta[1] = (float)((long double)storedPoint[1] - previousPoint[1]);
			storedDelta[2] = (float)((long double)storedPoint[2] - previousPoint[2]);
#else
			/* Linux keeps these coordinates despite their preceding float copies. */
			storedDelta[0] = (float)(pointX - previousPoint[0]);
			storedDelta[1] = (float)(pointY - previousPoint[1]);
			storedDelta[2] = (float)(pointZ - previousPoint[2]);
#endif
			segmentLength = TCE_VectorLengthValue(delta);
			distance      = (float)(segmentLength + distance);
		}
		nextParameter = (long double)parameter + (long double)0.01f;
		memcpy(previousPoint, point, sizeof(vec3_t));
#if defined(_WIN32)
		parameter = (float)nextParameter;
#endif
	}
	while (!(nextParameter > 1.0L));
	return distance;
}
#else
float BG_SplineLength(splinePath_t *pSpline)
{
	float i;
	float granularity = 0.01f;
	float dist        = 0;
//	float tension;
	vec3_t vec[2];
	vec3_t lastPoint;
	vec3_t result;

	for (i = 0; i <= 1.f; i += granularity )
	{
/*		if(pSpline->isStart) {
			tension = 1 - sin((1 - i) * M_PI * 0.5f);
		} else if(pSpline->isEnd) {
			tension = sin(i * M_PI * 0.5f);
		} else {
			tension = i;
		}*/

		BG_CalculateSpline_r(pSpline, vec[0], vec[1], i);
		VectorSubtract(vec[1], vec[0], result);
		VectorMA(vec[0], i, result, result);

		if (i != 0)
		{
			VectorSubtract(result, lastPoint, vec[0]);
			dist += VectorLength(vec[0]);
		}

		VectorCopy(result, lastPoint);
	}

	return dist;
}
#endif

void BG_BuildSplinePaths()
{
	int          i, j;
	pathCorner_t *pnt;
	splinePath_t *spline, *st;

	for ( i = 0; i < numSplinePaths; i++ )
	{
		spline = &splinePaths[i];

		if (*spline->strTarget)
		{
			for ( j = 0; j < spline->numControls; j++)
			{
				pnt = BG_Find_PathCorner(spline->controls[j].name);

				if (!pnt)
				{
					Com_Printf("^1Cant find control point (%s) for spline (%s)\n", spline->controls[j].name, spline->point.name);
					// Gordon: Just changing to a warning for now, easier for region compiles...
					continue;

				}
				else
				{
					VectorCopy(pnt->origin, spline->controls[j].origin);
				}
			}

			st = BG_Find_Spline(spline->strTarget);
			if (!st)
			{
				Com_Printf("^1Cant find target point (%s) for spline (%s)\n", spline->strTarget, spline->point.name);
				// Gordon: Just changing to a warning for now, easier for region compiles...
				continue;
			}

			spline->next = st;

			spline->length = BG_SplineLength(spline);
			BG_ComputeSegments(spline);
		}
	}

	for ( i = 0; i < numSplinePaths; i++ )
	{
		spline = &splinePaths[i];

		if (spline->next)
		{
			spline->next->prev = spline;
		}
	}
}

#ifndef CGAMEDLL
#if defined(_WIN32) || defined(__linux__)
/* Preserve native 32-bit NEG/DEC without signed overflow, including INT_MIN. */
splinePath_t *BG_GetSplineData(int number, qboolean *backwards)
{
	unsigned magnitude = (unsigned)number;
	unsigned indexBits;
	int      index;
	*backwards = number < 0 ? qtrue : qfalse;
	if (number < 0)
	{
		magnitude = 0u - magnitude;
	}
	indexBits = magnitude - 1u;
	memcpy(&index, &indexBits, sizeof(index));
	if (index < 0 || index >= numSplinePaths)
	{
		return NULL;
	}
	return &splinePaths[index];
}
#else
splinePath_t * BG_GetSplineData(int number, qboolean *backwards)
{
	if (number < 0)
	{
		*backwards = qtrue;
		number     = -number;
	}
	else
	{
		*backwards = qfalse;
	}
	number--;

	if (number < 0 || number >= numSplinePaths)
	{
		return NULL;
	}

	return &splinePaths[number];
}
#endif
#endif /* CGAMEDLL uses the unchanged TC lookup. */

/* TC 2002e1c0: the parsed definition is the complete reserve limit. */
int BG_MaxAmmoForWeapon(weapon_t weaponNum, int *skill)
{
	(void)skill;
	return weaponDef[weaponNum].maxammo;
}

/*
================
BG_CreateRotationMatrix
================
*/
void BG_CreateRotationMatrix(const vec3_t angles, vec3_t matrix[3])
{
	AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
	VectorInverse(matrix[1]);
}

/*
================
BG_TransposeMatrix
================
*/
void BG_TransposeMatrix(const vec3_t matrix[3], vec3_t transpose[3])
{
	int i, j;
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			transpose[i][j] = matrix[j][i];
		}
	}
}

/*
================
BG_RotatePoint
================
*/
void BG_RotatePoint(vec3_t point, const vec3_t matrix[3])
{
#if defined(_WIN32) || defined(__linux__)
	/* Keep the input while writing each result; matrix rows are read only
	 * when needed, including when point overlaps the matrix storage. */
	const long double x = (long double)point[0];
	const long double y = (long double)point[1];
	const long double z = (long double)point[2];
	int               row;

	for (row = 0; row < 3; ++row)
	{
		long double productX, productY, productZ, sum;
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		if (TCE_RotateFiniteRow((float)x, (float)y, (float)z,
		                        matrix[row], &point[row]))
		{
			continue;
		}
#endif
#if defined(_WIN32)
		/* Both Windows modules add Z, then Y, then X without float spills. */
		productZ = z * (long double)matrix[row][2];
		productY = y * (long double)matrix[row][1];
		sum      = productZ + productY;
		productX = x * (long double)matrix[row][0];
		sum      = sum + productX;
#else
		/* Linux retains the vector too, but adds the X/Y pair before Z.
		 * Preserve its row-specific operand order as well as grouping. */
		if (row == 2)
		{
			productX = x * (long double)matrix[row][0];
			productY = y * (long double)matrix[row][1];
			productZ = z * (long double)matrix[row][2];
			sum      = productY + productX;
		}
		else
		{
			productX = (long double)matrix[row][0] * x;
			productY = (long double)matrix[row][1] * y;
			if (row == 1)
			{
				sum = productX + productY;
			}
			else
			{
				sum = productY + productX;
			}
			productZ = (long double)matrix[row][2] * z;
		}
		sum = sum + productZ;
#endif
		point[row] = (float)sum;
	}
#else
	vec3_t tvec;

	VectorCopy(point, tvec);
	point[0] = DotProduct(matrix[0], tvec);
	point[1] = DotProduct(matrix[1], tvec);
	point[2] = DotProduct(matrix[2], tvec);
#endif
}



/*
================
BG_AdjustAAGunMuzzleForBarrel
================
*/
#if defined(_WIN32) || defined(__linux__)
void BG_AdjustAAGunMuzzleForBarrel(vec_t *origin, vec_t *forward, vec_t *right, vec_t *up, int barrel)
{
	volatile vec_t    *storedOrigin = origin;
	const long double height        = (barrel & 1) ? 20.0L : 40.0L;
	long double       product;
	int               component;

	/* Invalid barrel numbers must not read or write any vector. */
	if ((unsigned int)barrel > 3u)
	{
		return;
	}

#if defined(_WIN32)
	/* Windows reloads the rounded position at each of the three stages. */
	for (component = 0; component < 3; ++component)
	{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		float rounded;
		if (TCE_MuzzleFiniteComponent(forward[component], 64.0f,
		                              origin[component], 0, &rounded))
		{
			storedOrigin[component] = rounded;
			continue;
		}
#endif
		product                 = (long double)forward[component] * 64.0L;
		storedOrigin[component] = (float)(product + (long double)origin[component]);
	}
	for (component = 0; component < 3; ++component)
	{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		float rounded;
		if (TCE_MuzzleFiniteComponent(right[component], 20.0f,
		                              origin[component], barrel >= 2, &rounded))
		{
			storedOrigin[component] = rounded;
			continue;
		}
#endif
		product = (long double)right[component] * 20.0L;
		if (barrel >= 2)
		{
			storedOrigin[component] = (float)((long double)origin[component] - product);
		}
		else
		{
			storedOrigin[component] = (float)(product + (long double)origin[component]);
		}
	}
	for (component = 0; component < 3; ++component)
	{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		float rounded;
		if (TCE_MuzzleFiniteComponent(up[component], (float)height,
		                              origin[component], 0, &rounded))
		{
			storedOrigin[component] = rounded;
			continue;
		}
#endif
		product                 = (long double)up[component] * height;
		storedOrigin[component] = (float)(product + (long double)origin[component]);
	}
#else
	/* Linux writes each stage too, but keeps the unrounded position for
	 * the next addition. Direction components are still loaded in order. */
	long double       position[3];
	const long double side = (barrel >= 2) ? -20.0L : 20.0L;
	for (component = 0; component < 3; ++component)
	{
		if (component == 2)
		{
			product = 64.0L * (long double)forward[component];
		}
		else
		{
			product = (long double)forward[component] * 64.0L;
		}
		position[component]     = product + (long double)origin[component];
		storedOrigin[component] = (float)position[component];
	}
	for (component = 0; component < 3; ++component)
	{
		if (component == 2 && barrel != 1)
		{
			product = side * (long double)right[component];
		}
		else
		{
			product = (long double)right[component] * side;
		}
		position[component]     = position[component] + product;
		storedOrigin[component] = (float)position[component];
	}
	for (component = 0; component < 3; ++component)
	{
		if (component == 2)
		{
			product = height * (long double)up[component];
		}
		else
		{
			product = (long double)up[component] * height;
		}
		position[component]     = position[component] + product;
		storedOrigin[component] = (float)position[component];
	}
#endif
}
#else
void BG_AdjustAAGunMuzzleForBarrel(vec_t *origin, vec_t *forward, vec_t *right, vec_t *up, int barrel)
{
	switch (barrel)
	{
	case 0:
		VectorMA(origin, 64, forward, origin);
		VectorMA(origin, 20, right, origin);
		VectorMA(origin, 40, up, origin);
		break;
	case 1:
		VectorMA(origin, 64, forward, origin);
		VectorMA(origin, 20, right, origin);
		VectorMA(origin, 20, up, origin);
		break;
	case 2:
		VectorMA(origin, 64, forward, origin);
		VectorMA(origin, -20, right, origin);
		VectorMA(origin, 40, up, origin);
		break;
	case 3:
		VectorMA(origin, 64, forward, origin);
		VectorMA(origin, -20, right, origin);
		VectorMA(origin, 20, up, origin);
		break;
	}
}
#endif

/*
=================
PC_SourceWarning
=================
*/
void PC_SourceWarning(int handle, char *format, ...)
{
	int         line;
	char        filename[128];
	va_list     argptr;
	static char string[4096];

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	filename[0] = '\0';
	line        = 0;
	trap_PC_SourceFileAndLine(handle, filename, &line);

	Com_Printf(S_COLOR_YELLOW "WARNING: %s, line %d: %s\n", filename, line, string);
}

/*
=================
PC_SourceError
=================
*/
void PC_SourceError(int handle, char *format, ...)
{
	int         line;
	char        filename[128];
	va_list     argptr;
	static char string[4096];

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	filename[0] = '\0';
	line        = 0;
	trap_PC_SourceFileAndLine(handle, filename, &line);

#ifdef GAMEDLL
	Com_Error(ERR_DROP, S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string);
#else
	Com_Printf(S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string);
#endif
}

/*
=================
PC_Float_Parse
=================
*/
qboolean PC_Float_Parse(int handle, float *f)
{
	pc_token_t token;
	int        negative = qfalse;

	if (!trap_PC_ReadToken(handle, &token))
	{
		return qfalse;
	}
	if (token.string[0] == '-')
	{
		if (!trap_PC_ReadToken(handle, &token))
		{
			return qfalse;
		}
		negative = qtrue;
	}
	if (token.type != TT_NUMBER)
	{
		PC_SourceError(handle, "expected float but found %s\n", token.string);
		return qfalse;
	}
	/* Windows uses a floating conversion; Linux below copies the token bits.
	 * The C conversion does not establish identical x87 exception/NaN behavior. */
#if defined(_WIN32)
	{
		volatile long double value = (long double)token.floatvalue;
		if (negative)
		{
			value = -value;
		}
		*f = (float)value;
	}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
	{
		unsigned int tceTokenBits771;
		memcpy(&tceTokenBits771, &token.floatvalue, sizeof(tceTokenBits771));
		if (negative)
		{
			tceTokenBits771 ^= 0x80000000u;
		}
		memcpy(f, &tceTokenBits771, sizeof(tceTokenBits771));
	}
#else
	*f = negative ? -token.floatvalue : token.floatvalue;
#endif
	return qtrue;
}

/*
=================
PC_Color_Parse
=================
*/
qboolean PC_Color_Parse(int handle, vec4_t *c)
{
	int   i;
	float f;

	for (i = 0; i < 4; i++)
	{
		if (!PC_Float_Parse(handle, &f))
		{
			return qfalse;
		}
		(*c)[i] = f;
	}
	return qtrue;
}

/*
=================
PC_Vec_Parse
=================
*/
qboolean PC_Vec_Parse(int handle, vec3_t *c)
{
	int   i;
	float f;

	for (i = 0; i < 3; i++)
	{
		if (!PC_Float_Parse(handle, &f))
		{
			return qfalse;
		}
		/* Original transfers the parsed float as integer bits, without another
		 * floating conversion or NaN quieting step. */
		memcpy(&(*c)[i], &f, sizeof(f));
	}
	return qtrue;
}

/*
=================
PC_Int_Parse
=================
*/
qboolean PC_Int_Parse(int handle, int *i)
{
	pc_token_t token;
	int        negative = qfalse;

	if (!trap_PC_ReadToken(handle, &token))
	{
		return qfalse;
	}
	if (token.string[0] == '-')
	{
		if (!trap_PC_ReadToken(handle, &token))
		{
			return qfalse;
		}
		negative = qtrue;
	}
	if (token.type != TT_NUMBER)
	{
		PC_SourceError(handle, "expected integer but found %s\n", token.string);
		return qfalse;
	}
	*i = token.intvalue;
	if (negative)
	{
		/* Original NEG wraps INT_MIN; signed C negation is undefined there. */
		unsigned int tceIntegerBits771;
		memcpy(&tceIntegerBits771, &token.intvalue, sizeof(tceIntegerBits771));
		tceIntegerBits771 = 0u - tceIntegerBits771;
		memcpy(i, &tceIntegerBits771, sizeof(tceIntegerBits771));
	}
	return qtrue;
}

#ifdef GAMEDLL
/*
=================
PC_String_Parse
=================
*/
const char * PC_String_Parse(int handle)
{
	static char buf[MAX_TOKEN_CHARS];
	pc_token_t  token;
	if (!trap_PC_ReadToken(handle, &token))
	{
		return NULL;
	}

	Q_strncpyz(buf, token.string, MAX_TOKEN_CHARS);
	return buf;
}
#else
/*
=================
PC_String_Parse
=================
*/
qboolean PC_String_Parse(int handle, const char **out)
{
	pc_token_t token;
	if (!trap_PC_ReadToken(handle, &token))
	{
		return qfalse;
	}

	*(out) = String_Alloc(token.string);
	return qtrue;
}
#endif

/*
=================
PC_String_ParseNoAlloc

Same as one above, but uses a static buff and not the string memory pool
=================
*/
qboolean PC_String_ParseNoAlloc(int handle, char *out, size_t size)
{
	pc_token_t token;

	if (!trap_PC_ReadToken(handle, &token))
	{
		return qfalse;
	}

#if !defined(Q3_VM) && (defined(_M_IX86) || defined(__i386__)) && \
	(defined(_WIN32) || defined(__linux__))
	{
		/* The native API passes all 32 size bits to Q_strncpyz, including
		 * zero/negative sizes and their original fatal-error behavior. */
		typedef char parserStringSizeLayout[(sizeof(size_t) == 4 &&
		                                     sizeof(unsigned) == 4 && sizeof(int) == 4) ? 1 : -1];
		unsigned sizeBits = (unsigned)size;
		int      nativeSize;
		memcpy(&nativeSize, &sizeBits, sizeof(nativeSize));
		Q_strncpyz(out, token.string, nativeSize);
	}
#else
	if (size == 0 || size > INT_MAX)
	{
		return qfalse;
	}
	Q_strncpyz(out, token.string, (int)size);
#endif
	return qtrue;
}

const char *bg_fireteamNames[MAX_FIRETEAMS / 2] =
{
	"Alpha",
	"Bravo",
	"Charlie",
	"Delta",
	"Echo",
	"Foxtrot",
};

const voteType_t voteToggles[] =
{
	{ "vote_allow_comp",           CV_SVF_COMP          },
	{ "vote_allow_gametype",       CV_SVF_GAMETYPE      },
	{ "vote_allow_kick",           CV_SVF_KICK          },
	{ "vote_allow_map",            CV_SVF_MAP           },
	{ "vote_allow_matchreset",     CV_SVF_MATCHRESET    },
	{ "vote_allow_mutespecs",      CV_SVF_MUTESPECS     },
	{ "vote_allow_nextmap",        CV_SVF_NEXTMAP       },
	{ "vote_allow_pub",            CV_SVF_PUB           },
	{ "vote_allow_referee",        CV_SVF_REFEREE       },
	{ "vote_allow_shuffleteamsxp", CV_SVF_SHUFFLETEAMS  },
	{ "vote_allow_swapteams",      CV_SVF_SWAPTEAMS     },
	{ "vote_allow_friendlyfire",   CV_SVF_FRIENDLYFIRE  },
	{ "vote_allow_timelimit",      CV_SVF_TIMELIMIT     },
	{ "vote_allow_warmupdamage",   CV_SVF_WARMUPDAMAGE  },
	{ "vote_allow_antilag",        CV_SVF_ANTILAG       },
	{ "vote_allow_balancedteams",  CV_SVF_BALANCEDTEAMS },
	{ "vote_allow_muting",         CV_SVF_MUTING        }
};

int numVotesAvailable = sizeof(voteToggles) / sizeof(voteType_t);

// consts to offset random reinforcement seeds
const unsigned int aReinfSeeds[MAX_REINFSEEDS] = { 11, 3, 13, 7, 2, 5, 1, 17 };

// Weapon full names + headshot capability
const weap_ws_t aWeaponInfo[WS_MAX] =
{
	{ qfalse, "KNIF", "Knife"     },    // 0
	{ qtrue,  "LUGR", "Luger"     },    // 1
	{ qtrue,  "COLT", "Colt"      },    // 2
	{ qtrue,  "MP40", "MP-40"     },    // 3
	{ qtrue,  "TMPS", "Thompson"  },    // 4
	{ qtrue,  "STEN", "Sten"      },    // 5
	{ qtrue,  "FG42", "FG-42"     },    // 6
	{ qtrue,  "PNZR", "Panzer"    },    // 7
	{ qtrue,  "FLAM", "F.Thrower" },    // 8
	{ qfalse, "GRND", "Grenade"   },    // 9
	{ qfalse, "MRTR", "Mortar"    },    // 10
	{ qfalse, "DYNA", "Dynamite"  },    // 11
	{ qfalse, "ARST", "Airstrike" },    // 12
	{ qfalse, "ARTY", "Artillery" },    // 13
	{ qfalse, "SRNG", "Syringe"   },    // 14
	{ qfalse, "SMOK", "SmokeScrn" },    // 15
	{ qfalse, "STCH", "Satchel"   },    // 16
	{ qfalse, "GRLN", "G.Launchr" },    // 17
	{ qfalse, "LNMN", "Landmine"  },    // 18
	{ qtrue,  "MG42", "MG-42 Gun" },    // 19
	{ qtrue,  "GARN", "Garand"    },    // 20
	{ qtrue,  "K-43", "K43 Rifle" }     // 21
};

// Multiview: Convert weaponstate to simpler format
int BG_simpleWeaponState(int ws)
{
	switch (ws)
	{
	case WEAPON_READY:
	case WEAPON_READYING:
	case WEAPON_RELAXING:
		return(WSTATE_IDLE);
	case WEAPON_RAISING:
	case WEAPON_DROPPING:
	case WEAPON_DROPPING_TORELOAD:
		return(WSTATE_SWITCH);
	case WEAPON_FIRING:
	case WEAPON_FIRINGALT:
		return(WSTATE_FIRE);
	case WEAPON_RELOADING:
		return(WSTATE_RELOAD);
	}

	return(WSTATE_IDLE);
}


// Multiview: Reduce hint info to 2 bits.  However, we can really
// have up to 8 values, as some hints will have a 0 value for
// cursorHintVal
int BG_simpleHintsCollapse(int hint, int val)
{
	switch (hint)
	{
	case HINT_DISARM:
		if (val > 0)
		{
			return(0);
		}
	case HINT_BUILD:
		if (val > 0)
		{
			return(1);
		}
	case HINT_BREAKABLE:
		if (val == 0)
		{
			return(1);
		}
	case HINT_DOOR_ROTATING:
	case HINT_BUTTON:
	case HINT_MG42:
		if (val == 0)
		{
			return(2);
		}
	case HINT_BREAKABLE_DYNAMITE:
		if (val == 0)
		{
			return(3);
		}
	}

	return(0);
}


// Multiview: Expand the hints.  Because we map a couple hints
// into a single value, we can't replicate the proper hint back
// in all cases.
int BG_simpleHintsExpand(int hint, int val)
{
	switch (hint)
	{
	case 0: return((val >= 0) ? HINT_DISARM : 0);
	case 1: return((val >= 0) ? HINT_BUILD : HINT_BREAKABLE);
	case 2: return((val >= 0) ? HINT_BUILD : HINT_MG42);
	case 3: return((val >= 0) ? HINT_BUILD : HINT_BREAKABLE_DYNAMITE);
	}

	return(0);
}

// Real printable charater count
int BG_drawStrlen(const char *str)
{
	int cnt = 0;

	while (*str)
	{
		if (Q_IsColorString(str))
		{
			str += 2;
		}
		else
		{
			cnt++;
			str++;
		}
	}
	return(cnt);
}


// Copies a color string, with limit of real chars to print
//		in = reference buffer w/color
//		out = target buffer
//		str_max = max size of printable string
//		out_max = max size of target buffer
//
// Returns size of printable string
int BG_colorstrncpyz(char *in, char *out, int str_max, int out_max)
{
	int       str_len = 0; // current printable string size
	int       out_len = 0; // current true string size
	const int in_len  = Q_strlenInt(in);

	out_max--;
	while (*in && out_len < out_max && str_len < str_max)
	{
		if (*in == '^')
		{
			if (out_len + 2 >= in_len && out_len + 2 >= out_max)
			{
				break;
			}

			*out++   = *in++;
			*out++   = *in++;
			out_len += 2;
			continue;
		}

		*out++ = *in++;
		str_len++;
		out_len++;
	}

	*out = 0;

	return(str_len);
}

int BG_strRelPos(char *in, int index)
{
	int        cPrintable = 0;
	const char *ref       = in;

	while (*ref && cPrintable < index)
	{
		if (Q_IsColorString(ref))
		{
			ref += 2;
		}
		else
		{
			ref++;
			cPrintable++;
		}
	}

	return(ref - in);
}

// strip colors and control codes, copying up to dwMaxLength-1 "good" chars and nul-terminating
// returns the length of the cleaned string
int BG_cleanName(const char *pszIn, char *pszOut, unsigned int dwMaxLength, qboolean fCRLF)
{
	const char *pInCopy     = pszIn;
	const char *pszOutStart = pszOut;

	while (*pInCopy && (pszOut - pszOutStart < dwMaxLength - 1))
	{
		if (*pInCopy == '^')
		{
			pInCopy += ((pInCopy[1] == 0) ? 1 : 2);
		}
		else if ((*pInCopy < 32 && (!fCRLF || *pInCopy != '\n')) || (*pInCopy > 126))
		{
			pInCopy++;
		}
		else
		{
			*pszOut++ = *pInCopy++;
		}
	}

	*pszOut = 0;
	return(pszOut - pszOutStart);
}

// Only used locally
typedef struct
{
	char *colorname;
	vec4_t *color;
} colorTable_t;

// Colors for crosshairs
colorTable_t OSP_Colortable[] =
{
	{ "white",    &colorWhite    },
	{ "red",      &colorRed      },
	{ "green",    &colorGreen    },
	{ "blue",     &colorBlue     },
	{ "yellow",   &colorYellow   },
	{ "magenta",  &colorMagenta  },
	{ "cyan",     &colorCyan     },
	{ "orange",   &colorOrange   },
	{ "mdred",    &colorMdRed    },
	{ "mdgreen",  &colorMdGreen  },
	{ "dkgreen",  &colorDkGreen  },
	{ "mdcyan",   &colorMdCyan   },
	{ "mdyellow", &colorMdYellow },
	{ "mdorange", &colorMdOrange },
	{ "mdblue",   &colorMdBlue   },
	{ "ltgrey",   &colorLtGrey   },
	{ "mdgrey",   &colorMdGrey   },
	{ "dkgrey",   &colorDkGrey   },
	{ "black",    &colorBlack    },
	{ NULL,       NULL           }
};

extern void trap_Cvar_Set(const char *var_name, const char *value);
#if defined(_MSC_VER) && defined(_M_IX86) && defined(CGAMEDLL)
typedef char tceCrossTableLayout825[(sizeof(colorTable_t) == 8 && offsetof(colorTable_t, color) == 4)?1:-1];
#endif
void BG_setCrosshair(char *colString, float *col, float alpha, char *cvarName)
{
	char         *s         = colString;
	const double byteToUnit = 0.00392156862745098;
	int          component;
#if defined(_WIN32)
	/* Windows performs its first alpha comparison before the RGB stores. */
	int aboveOne = alpha > 1.0f;
#endif

	col[0] = 1.0f;
	col[1] = 1.0f;
	col[2] = 1.0f;
#if defined(_WIN32)
	col[3] = aboveOne ? 1.0f : !(alpha >= 0.0f) ? 0.0f : alpha;
#elif defined(__linux__) || defined(UIDLL)
	/* All three Linux originals send unordered alpha to one. */
	col[3] = !(alpha <= 1.0f) ? 1.0f : alpha < 0.0f ? 0.0f : alpha;
#else
	col[3] = alpha > 1.0f ? 1.0f : alpha < 0.0f ? 0.0f : alpha;
#endif

	if (*s == '0' && (s[1] == 'x' || s[1] == 'X'))
	{
		s += 2;
		/* Exactly six valid digits; the original accepts following text. */
		if (Q_IsHexColorString(s))
		{
			for (component = 0; component < 3; ++component)
			{
				int high  = (signed char)s[component * 2];
				int low   = (signed char)s[component * 2 + 1];
				int value = gethex(high) * 16 + gethex(low);
#if defined(_WIN32) || defined(__linux__)
				long double scaled = (long double)value * (long double)byteToUnit;
				col[component] = (float)scaled;
#else
				col[component] = (float)(value * byteToUnit);
#endif
			}
			return;
		}
	}
	else
	{
		int index = 0;
		while (OSP_Colortable[index].colorname != NULL)
		{
			if (Q_stricmp(s, OSP_Colortable[index].colorname) == 0)
			{
				/* Integer copies, reloading the entry after each output store. */
				for (component = 0; component < 3; ++component)
				{
					unsigned int bits;
					memcpy(&bits, &(*OSP_Colortable[index].color)[component], sizeof(bits));
					memcpy(&col[component], &bits, sizeof(bits));
				}
				return;
			}
			++index;
		}
	}
	trap_Cvar_Set(cvarName, "White");
}


qboolean BG_isLightWeaponSupportingFastReload(int weapon)
{
	if (weapon == WP_LUGER ||
	    weapon == WP_COLT ||
	    weapon == WP_MP40 ||
	    weapon == WP_THOMPSON ||
	    weapon == WP_STEN ||
	    weapon == WP_SILENCER ||
	    weapon == WP_FG42 ||
	    weapon == 52)  /* TCE uses 52 here; the SDK uses WP_SILENCED_COLT (41). */
	{
		return qtrue;
	}
	return qfalse;
}

qboolean BG_IsScopedWeapon(int weapon)
{
	/* TC legacy protocol scope slots. SDK42/43 overlap UMP45/AK74U.
	 * Windows30006630/2002e8e0; Linux UI00052264 agrees. */
	return (unsigned int)weapon - 57u < 3u ? qtrue : qfalse;
}

skillType_t BG_SkillGroupForWeapon(int weapon)
{
	/* TC:E protocol IDs; the SDK weapon names use different slots. */
	switch (weapon)
	{
	case 3:
	case 8:
	case 10:
	case 33:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 50:
		return SK_HEAVY_WEAPONS;
	case 46:
	case 47:
	case 51:
		return SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS;
	case 48:
	case 49:
		return SK_SIGNALS;
	default:
		return SK_LIGHT_WEAPONS;
	}
}

weapon_t BG_EquivalentTeamWeapon(int weapon)
{
	/* Preserve the original TC:E mapping, including its default of zero. */
	switch (weapon)
	{
	case 2:  return 39;
	case 3:  return 10;
	case 8:  return 45;
	case 10: return 3;
	case 33: return 41;
	case 37: return 38;
	case 38: return 37;
	case 39: return 2;
	case 41: return 33;
	case 43: return 44;
	case 44: return 43;
	case 45: return 8;
	case 47: return 51;
	case 48: return 49;
	case 49: return 48;
	case 51: return 47;
	default: return WP_NONE;
	}
}

///////////////////////////////////////////////////////////////////////////////
typedef struct locInfo_s
{
	vec2_t gridStartCoord;
	vec2_t gridStep;
} locInfo_t;

static locInfo_t locInfo;

void BG_InitLocations(vec2_t world_mins, vec2_t world_maxs)
{
#if defined(_WIN32) || defined(__linux__)
	volatile locInfo_t *grid = &locInfo;
	long double        count, fraction, offset;
	int                whole;

	grid->gridStep[0] = 1200.0f;
	grid->gridStep[1] = 1200.0f;
#if defined(_WIN32)
	{
		/* The first probe uses the original rounded binary32 reciprocal. */
		const float firstInverse = 0.000833333353511989116668701171875f;
		int         axis;
		for (axis = 0; axis < 2; ++axis)
		{
			long double difference = axis == 0
			    ? (long double)world_maxs[0] - (long double)world_mins[0]
			    : (long double)world_mins[1] - (long double)world_maxs[1];
			count = difference * (long double)firstInverse;
			/* C0 repeats on unordered too; do not turn this into ordered <. */
			while (!(count >= 7.0L))
			{
				grid->gridStep[axis] = (float)((long double)locInfo.gridStep[axis] - 50.0L);
				difference           = axis == 0
				    ? (long double)world_maxs[0] - (long double)world_mins[0]
				    : (long double)world_mins[1] - (long double)world_maxs[1];
				count = difference / (long double)locInfo.gridStep[axis];
			}
		}
		count = ((long double)world_maxs[0] - (long double)world_mins[0])
		        / (long double)locInfo.gridStep[0];
		whole                   = TCE_TruncateNativeInteger(count);
		fraction                = count - (long double)whole;
		offset                  = fraction * (long double)locInfo.gridStep[0];
		offset                  = offset * 0.5L;
		grid->gridStartCoord[0] = (float)(offset + (long double)world_mins[0]);
		count                   = ((long double)world_mins[1] - (long double)world_maxs[1])
		                          / (long double)locInfo.gridStep[1];
		whole                   = TCE_TruncateNativeInteger(count);
		fraction                = count - (long double)whole;
		offset                  = fraction * (long double)locInfo.gridStep[1];
		offset                  = offset * 0.5L;
		grid->gridStartCoord[1] = (float)((long double)world_mins[1] - offset);
	}
#else
	{
		long double inverse    = 1.0L / 1200.0L;
		long double maximumX   = (long double)world_maxs[0];
		long double minimumX   = (long double)world_mins[0];
		long double difference = maximumX - minimumX;
		long double stepY, maximumY, minimumY, stepX, inverseY;
		qboolean    reducedY = qfalse;

		count = inverse * difference;
		/* Only the first X gate exits on unordered. Later gates repeat. */
		if (count < 7.0L)
		{
			do
			{
				long double step = (long double)locInfo.gridStep[0] - 50.0L;
				grid->gridStep[0] = (float)step;
				inverse           = 1.0L / step;
				maximumX          = (long double)world_maxs[0];
				minimumX          = (long double)world_mins[0];
				difference        = maximumX - minimumX;
				count             = inverse * difference;
			}
			while (!(count >= 7.0L));
		}

		stepY      = (long double)locInfo.gridStep[1];
		maximumY   = (long double)world_maxs[1];
		inverse    = 1.0L / stepY;
		difference = (long double)world_mins[1] - maximumY;
		count      = inverse * difference;
		while (!(count >= 7.0L))
		{
			/* Y retains the step between iterations despite each float store. */
			stepY             = stepY - 50.0L;
			inverse           = 1.0L / stepY;
			grid->gridStep[1] = (float)stepY;
			maximumY          = (long double)world_maxs[1];
			difference        = (long double)world_mins[1] - maximumY;
			count             = inverse * difference;
			reducedY          = qtrue;
		}
		if (reducedY)
		{
			maximumX = (long double)world_maxs[0];
			minimumX = (long double)world_mins[0];
		}

		stepX                   = (long double)locInfo.gridStep[0];
		inverseY                = 1.0L / stepY;
		inverse                 = 1.0L / stepX;
		difference              = maximumX - minimumX;
		count                   = inverse * difference;
		whole                   = TCE_TruncateNativeInteger(count);
		fraction                = count - (long double)whole;
		offset                  = stepX * fraction;
		offset                  = offset * 0.5L;
		grid->gridStartCoord[0] = (float)(minimumX + offset);
		minimumY                = (long double)world_mins[1];
		difference              = minimumY - (long double)world_maxs[1];
		count                   = inverseY * difference;
		whole                   = TCE_TruncateNativeInteger(count);
		fraction                = count - (long double)whole;
		offset                  = stepY * fraction;
		offset                  = offset * 0.5L;
		grid->gridStartCoord[1] = (float)(minimumY - offset);
	}
#endif
#else

	// keep this in sync with CG_DrawGrid
	locInfo.gridStep[0] = 1200.f;
	locInfo.gridStep[1] = 1200.f;

	// ensure minimal grid density
	while ((world_maxs[0] - world_mins[0]) / locInfo.gridStep[0] < 7)
		locInfo.gridStep[0] -= 50.f;
	while ((world_mins[1] - world_maxs[1]) / locInfo.gridStep[1] < 7)
		locInfo.gridStep[1] -= 50.f;

	locInfo.gridStartCoord[0] = world_mins[0] + .5f * ((((world_maxs[0] - world_mins[0]) / locInfo.gridStep[0]) - ((int)((world_maxs[0] - world_mins[0]) / locInfo.gridStep[0]))) * locInfo.gridStep[0]);
	locInfo.gridStartCoord[1] = world_mins[1] - .5f * ((((world_mins[1] - world_maxs[1]) / locInfo.gridStep[1]) - ((int)((world_mins[1] - world_maxs[1]) / locInfo.gridStep[1]))) * locInfo.gridStep[1]);
#endif
}

char *BG_GetLocationString(vec_t *pos)
{
	static char  coord[6];
	int          x, y, column;
	unsigned int columnBits;

	coord[0] = '\0';

#if defined(_WIN32)
	/* Windows divides each held difference directly before signed64 truncation. */
	{
		long double coordinate = (long double)pos[0] - (long double)locInfo.gridStartCoord[0];
		coordinate = coordinate / (long double)locInfo.gridStep[0];
		x          = TCE_TruncateNativeInteger(coordinate);
		coordinate = (long double)locInfo.gridStartCoord[1] - (long double)pos[1];
		coordinate = coordinate / (long double)locInfo.gridStep[1];
		y          = TCE_TruncateNativeInteger(coordinate);
	}
#elif defined(__linux__)
	/* Linux retains both reciprocals before converting the first coordinate. */
	{
		const long double inverseX   = 1.0L / (long double)locInfo.gridStep[0];
		const long double inverseY   = 1.0L / (long double)locInfo.gridStep[1];
		long double       difference = (long double)pos[0] - (long double)locInfo.gridStartCoord[0];
		long double       coordinate = inverseX * difference;
		x          = TCE_TruncateNativeInteger(coordinate);
		difference = (long double)locInfo.gridStartCoord[1] - (long double)pos[1];
		coordinate = inverseY * difference;
		y          = TCE_TruncateNativeInteger(coordinate);
	}
#else
	/* Portable fallback: original modules specify the two x86 paths above. */
	x = (pos[0] - locInfo.gridStartCoord[0]) / locInfo.gridStep[0];
	y = (locInfo.gridStartCoord[1] - pos[1]) / locInfo.gridStep[1];
#endif

	if (x < 0)
	{
		x = 0;
	}
	if (y < 0)
	{
		y = 0;
	}

	/* Native ADD wraps before the formatter consumes the character argument. */
	columnBits = (unsigned int)x + (unsigned int)'A';
	memcpy(&column, &columnBits, sizeof(column));
	Com_sprintf(coord, sizeof(coord), "%c,%i", column, y);

	return coord;
}

qboolean BG_BBoxCollision(vec3_t min1, vec3_t max1, vec3_t min2, vec3_t max2)
{
	int i;

	for ( i = 0; i < 3; i++ )
	{
		if (min1[i] > max2[i])
		{
			return qfalse;
		}
		if (min2[i] > max1[i])
		{
			return qfalse;
		}
	}

	return qtrue;
}

weapon_t bg_heavyWeapons[NUM_HEAVY_WEAPONS] =
{
	WP_FLAMETHROWER,
	WP_MOBILE_MG42,
	WP_MOBILE_MG42_SET,
	WP_PANZERFAUST,
	WP_MORTAR,
	WP_MORTAR_SET
};

/////////////////////////

int BG_FootstepForSurface(int surfaceFlags)
{
	/* TC material byte and silent-surface ID; used by actual movement callers. */
	return TCE_BG_FootstepForSurface((unsigned int)surfaceFlags);
}

/*
============
Q_vsnprintf

vsnprintf portability:

C99 standard: vsnprintf returns the number of characters (excluding the trailing
'\0') which would have been written to the final string if enough space had been available
snprintf and vsnprintf do not write more than size bytes (including the trailing '\0')

win32: _vsnprintf returns the number of characters written, not including the terminating null character,
or a negative value if an output error occurs. If the number of characters to write exceeds count,
then count characters are written and -1 is returned and no trailing '\0' is added.

Q_vsnPrintf: always append a trailing '\0', returns number of characters written or
returns -1 on failure or if the buffer would be overflowed.

copied over from common.c implementation
============
*/
int Q_vsnprintf(char *dest, int size, const char *fmt, va_list argptr)
{
	int ret;

#ifdef _WIN32
#undef _vsnprintf
	ret = _vsnprintf(dest, size - 1, fmt, argptr);
#define _vsnprintf  use_idStr_vsnPrintf
#else
#undef vsnprintf
	ret = vsnprintf(dest, size, fmt, argptr);
#define vsnprintf   use_idStr_vsnPrintf
#endif
	dest[size - 1] = '\0';
	if (ret < 0 || ret >= size)
	{
		return -1;
	}
	return ret;
}
