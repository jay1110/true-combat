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
#include "tce_bg.h"

#include "../ui/menudef.h"

#ifdef CGAMEDLL
	extern	vmCvar_t cg_gameType;
#define gametypeCvar cg_gameType
#elif GAMEDLL
	extern	vmCvar_t g_developer;
	extern	vmCvar_t g_gametype;
#define gametypeCvar g_gametype
#else
	extern	vmCvar_t ui_gameType;
#define gametypeCvar ui_gameType
#endif

#define BG_IsSinglePlayerGame() (gametypeCvar.integer == GT_SINGLE_PLAYER) || (gametypeCvar.integer == GT_COOP)


const char* skillNames[SK_NUM_SKILLS] = {
	"Battle Sense",
	"Engineering",
	"First Aid",
	"Signals",
	"Light Weapons",
	"Heavy Weapons",
	"Covert Ops"
};

const char* skillNamesLine1[SK_NUM_SKILLS] = {
	"Battle",
	"Engineering",
	"First",
	"Signals",
	"Light",
	"Heavy",
	"Covert"
};

const char* skillNamesLine2[SK_NUM_SKILLS] = {
	"Sense",
	"",
	"Aid",
	"",
	"Weapons",
	"Weapons",
	"Ops"
};

const char* medalNames[SK_NUM_SKILLS] = {
	"Distinguished Service Medal",
	"Steel Star",
	"Silver Cross",
	"Signals Medal",
	"Infantry Medal",
	"Bombardment Medal",
	"Silver Snake"
};

const int skillLevels[NUM_SKILL_LEVELS] = {
	0,		// reaching level 0
	20,		// reaching level 1
	50,		// reaching level 2
	90,		// reaching level 3
	140		// reaching level 4
//	200		// reaching level 5
};

vec3_t	playerlegsProneMins = { -13.5f, -13.5f, -24.f };
vec3_t	playerlegsProneMaxs = { 13.5f, 13.5f, -14.4f };

int					numSplinePaths;
splinePath_t		splinePaths[MAX_SPLINE_PATHS];

int					numPathCorners;
pathCorner_t		pathCorners[MAX_PATH_CORNERS];

// these defines are matched with the character torso animations
#define DELAY_LOW		100	// machineguns, tesla, spear, flame
#define DELAY_HIGH		100	// mauser, garand
#define DELAY_PISTOL	100	// colt, luger, sp5, cross
#define DELAY_SHOULDER	50	// rl
#define DELAY_THROW		250	// grenades, dynamite

// Arnout: the new loadout for WolfXP
/* TC:E Windows cgame300964b8: ten banks,22 entries each. */
int weapBanksMultiPlayer[MAX_WEAP_BANKS_MP][MAX_WEAPS_IN_BANK_MP] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {8,41,42,48,43,44,45,33,46,47,49,50,51,5,6,13,24,23,25,32,3,10},
    {2,7,37,38,53,54,39,40,14,52,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {30,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {26,61,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

// TAT 10/4/2002
//		Using one unified list for which weapons can received ammo
//		This is used both by the ammo pack code and by the bot code to determine if reloads are needed
/* TC qagame200b4408: protocol IDs, including the legacy65/66 sentinels. */
int reloadableWeapons[] = {
	3, 8, 10, 25, 65, 66, 23, 24, 33, 32, 31, 7, 2, 35, 37, 38,
	56, 55, 53, 54, 39, 40, 51, 47, 46, 44, 45, 43, 42, 41, 5, 6, 13,
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
ammotable_t ammoTableMP[WP_NUM_WEAPONS] = {
	//	MAX				USES	MAX		START	START  RELOAD	FIRE			NEXT	HEAT,	COOL,	MOD,	...
	//	AMMO			AMT.	CLIP	AMMO	CLIP	TIME	DELAY			SHOT
	{	0,				0,		0,		0,		0,		0,		50,				0,		0,		0,		0						},	// WP_NONE					// 0
	{	999,			0,		999,	0,		0,		0,		50,				200,	0,		0,		MOD_KNIFE				},	// WP_KNIFE					// 1
	{	24,				1,		8,		24,		8,		1500,	DELAY_PISTOL,	400,	0,		0,		MOD_LUGER				},	// WP_LUGER					// 2	// NOTE: also 32 round 'snail' magazine
	{	90,				1,		30,		30,		30,		2400,	DELAY_LOW,		150,	0,		0,		MOD_MP40				},	// WP_MP40					// 3
	{	45,				1,		15,		0,		4,		1000,	DELAY_THROW,	1600,	0,		0,		MOD_GRENADE_LAUNCHER	},	// WP_GRENADE_LAUNCHER		// 4
	{	4,				1,		1,		0,		4,		1000,	750	,			2000,	0,		0,		MOD_PANZERFAUST			},	// WP_PANZERFAUST			// 5	// DHM - Nerve :: updated delay so prediction is correct
	{	200,			1,		200,	0,		200,	1000,	DELAY_LOW,		50,		0,		0,		MOD_FLAMETHROWER		},	// WP_FLAMETHROWER			// 6
	{	24,				1,		8,		24,		8,		1500,	DELAY_PISTOL,	400,	0,		0,		MOD_COLT				},	// WP_COLT					// 7
	{	90,				1,		30,		30,		30,		2400,	DELAY_LOW,		150,	0,		0,		MOD_THOMPSON			},	// WP_THOMPSON				// 8
	{	45,				1,		15,		0,		4,		1000,	DELAY_THROW,	1600,	0,		0,		MOD_GRENADE_PINEAPPLE	},	// WP_GRENADE_PINEAPPLE		// 9

	{	96,				1,		32,		32,		32,		3100,	DELAY_LOW,		150,	1200,	450,	MOD_STEN				},	// WP_STEN					// 10
	{	10,				1,		1,		0,		10,		1500,	50,				1000,	0,		0,		MOD_SYRINGE				},	// WP_MEDIC_SYRINGE			// 11
	{	1,				0,		1,		0,		0,		3000,	50,				1000,	0,		0,		MOD_AMMO,				},	// WP_AMMO					// 12
	{	1,				0,		1,		0,		1,		3000,	50,				1000,	0,		0,		MOD_ARTY,				},	// WP_ARTY					// 13												
	{	24,				1,		8,		24,		8,		1500,	DELAY_PISTOL,	400,	0,		0,		MOD_SILENCER			},	// WP_SILENCER				// 14
	{	1,				0,		10,		0,		0,		1000,	DELAY_THROW,	1600,	0,		0,		MOD_DYNAMITE			},	// WP_DYNAMITE				// 15
	{	999,			0,		999,	0,		0,		0,		50,				0,		0,		0,		0						},	// WP_SMOKETRAIL			// 16
	{	999,			0,		999,	0,		0,		0,		50,				0,		0,		0,		0						},	// WP_MAPMORTAR				// 17
	{	999,			0,		999,	0,		0,		0,		50,				0,		0,		0,		0						},	// VERYBIGEXPLOSION			// 18
	{	999,			0,		999,	1,		1,		0,		50,				0,		0,		0,		0						},	// WP_MEDKIT				// 19

	{	999,			0,		999,	0,		0,		0,		50,				0,		0,		0,		0						},	// WP_BINOCULARS			// 20
	{	999,			0,		999,	0,		0,		0,		50,				0,		0,		0,		0						},	// WP_PLIERS				// 21
	{	999,			0,		999,	0,		1,		0,		50,				0,		0,		0,		MOD_AIRSTRIKE			},	// WP_SMOKE_MARKER			// 22
	{	30,				1,		10,		20,		10,		2500,	DELAY_LOW,		400,	0,		0,		MOD_KAR98				},	// WP_KAR98					// 23		K43
	{	24,				1,		8,		16,		8,		1500,	DELAY_LOW,		400,	0,		0,		MOD_CARBINE				},	// WP_CARBINE				// 24		GARAND
	{	24,				1,		8,		16,		8,		1500,	DELAY_LOW,		400,	0,		0,		MOD_GARAND				},	// WP_GARAND				// 25		GARAND
	{	1,				0,		1,		0,		1,		100,	DELAY_LOW,		100,	0,		0,		MOD_LANDMINE			},	// WP_LANDMINE				// 26
	{	1,				0,		1,		0,		0,		3000,	DELAY_LOW,		2000,	0,		0,		MOD_SATCHEL				},	// WP_SATCHEL				// 27
	{	1,				0,		1,		0,		0,		3000,	722,			2000,	0,		0,		0,						},	// WP_SATCHEL_DET			// 28
	{	6,				1,		1,		0,		0,		2000,	DELAY_HIGH,		2000,	0,		0,		MOD_TRIPMINE			},	// WP_TRIPMINE				// 29

	{	1,				0,		10,		0,		1,		1000,	DELAY_THROW,	1600,	0,		0,		MOD_SMOKEBOMB			},	// WP_SMOKE_BOMB			// 30
	{	450,			1,		150,	0,		150,	3000,	DELAY_LOW,		66,		1500,	300,	MOD_MOBILE_MG42			},	// WP_MOBILE_MG42			// 31
	{	30,				1,		10,		20,		10,		2500,	DELAY_LOW,		400,	0,		0,		MOD_K43					},	// WP_K43					// 32		K43
	{	60,				1,		20,		40,		20,		2000,	DELAY_LOW,		100,	0,		0,		MOD_FG42				},	// WP_FG42					// 33
	{	0,				0,		0,		0,		0,		0,	    0,		        0,	    1500,	300,	0					    },	// WP_DUMMY_MG42			// 34
	{	15,				1,		1,		0,		0,		0,		750,			1600,	0,		0,		MOD_MORTAR				},	// WP_MORTAR				// 35
	{	999,			0,		1,		0,		0,		1000,	750,			1600,	0,		0,		0						},	// WP_LOCKPICK				// 36 
	{	48,				1,		8,		48,		8,		2700,	DELAY_PISTOL,	200,	0,		0,		MOD_AKIMBO_COLT			},	// WP_AKIMBO_COLT			// 37
	{	48,				1,		8,		48,		8,		2700,	DELAY_PISTOL,	200,	0,		0,		MOD_AKIMBO_LUGER		},	// WP_AKIMBO_LUGER			// 38
	{	4,				1,		1,		4,		1,		3000,	DELAY_LOW,		400,	0,		0,		MOD_GPG40				},	// WP_GPG40					// 39

	{	4,				1,		1,		4,		1,		3000,	DELAY_LOW,		400,	0,		0,		MOD_M7					},	// WP_M7					// 40
	{	24,				1,		8,		24,		8,		1500,	DELAY_PISTOL,	400,	0,		0,		MOD_SILENCED_COLT		},	// WP_SILENCED_COLT			// 41
	{	24,				1,		8,		16,		8,		1500,	0,				400,	0,		0,		MOD_GARAND_SCOPE		},	// WP_GARAND_SCOPE			// 42		GARAND
	{	30,				1,		10,		20,		10,		2500,	0,				400,	0,		0,		MOD_K43_SCOPE			},	// WP_K43_SCOPE				// 43		K43
	{	60,				1,		20,		40,		20,		2000,	DELAY_LOW,		400,	0,		0,		MOD_FG42SCOPE			},	// WP_FG42SCOPE				// 44
	{	16,				1,		1,		12,		0,		0,		750,			1400,	0,		0,		MOD_MORTAR				},	// WP_MORTAR_SET			// 45
	{	10,				1,		1,		0,		10,		1500,	50,				1000,	0,		0,		MOD_SYRINGE				},	// WP_MEDIC_ADRENALINE		// 46
	{	48,				1,		8,		48,		8,		2700,	DELAY_PISTOL,	200,	0,		0,		MOD_AKIMBO_SILENCEDCOLT	},	// WP_AKIMBO_SILENCEDCOLT	// 47
	{	48,				1,		8,		48,		8,		2700,	DELAY_PISTOL,	200,	0,		0,		MOD_AKIMBO_SILENCEDLUGER},	// WP_AKIMBO_SILENCEDLUGER	// 48
	{	450,			1,		150,	0,		150,	3000,	DELAY_LOW,		66,		1500,	300,	MOD_MOBILE_MG42			},	// WP_MOBILE_MG42_SET		// 49
};

//----(SA)	moved in here so both games can get to it
/* Original TC tables30106e78/20144908 are zero-initialized. Confirmed in
 * running original client and server before/after weapon cycling. SDK pairs
 * such as7->41 and45->35 refer to unrelated TC weapons and must not survive. */
int weapAlts[MAX_WEAPONS] = {0};


// new (10/18/00)
char *animStrings[] = {
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

	"TORSO_RAISE",	// (low)
	"TORSO_ATTACK",
	"TORSO_STAND",
	"TORSO_STAND_ALT1",
	"TORSO_STAND_ALT2",
	"TORSO_READY",
	"TORSO_RELAX",

	"TORSO_RAISE2",	// (high)
	"TORSO_ATTACK2",
	"TORSO_STAND2",
	"TORSO_STAND2_ALT1",
	"TORSO_STAND2_ALT2",
	"TORSO_READY2",
	"TORSO_RELAX2",

	"TORSO_RAISE3",	// (pistol)
	"TORSO_ATTACK3",
	"TORSO_STAND3",
	"TORSO_STAND3_ALT1",
	"TORSO_STAND3_ALT2",
	"TORSO_READY3",
	"TORSO_RELAX3",

	"TORSO_RAISE4",	// (shoulder)
	"TORSO_ATTACK4",
	"TORSO_STAND4",
	"TORSO_STAND4_ALT1",
	"TORSO_STAND4_ALT2",
	"TORSO_READY4",
	"TORSO_RELAX4",

	"TORSO_RAISE5",	// (throw)
	"TORSO_ATTACK5",
	"TORSO_ATTACK5B",
	"TORSO_STAND5",
	"TORSO_STAND5_ALT1",
	"TORSO_STAND5_ALT2",
	"TORSO_READY5",
	"TORSO_RELAX5",

	"TORSO_RELOAD1",	// (low)
	"TORSO_RELOAD2",	// (high)
	"TORSO_RELOAD3",	// (pistol)
	"TORSO_RELOAD4",	// (shoulder)

	"TORSO_MG42",		// firing tripod mounted weapon animation

	"TORSO_MOVE",		// torso anim to play while moving and not firing (swinging arms type thing)
	"TORSO_MOVE_ALT",		// torso anim to play while moving and not firing (swinging arms type thing)

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

	"LEGS_BOOT",		// kicking animation

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
char *animStringsOld[] = {
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

	"TORSO_RAISE",	// (low)
	"TORSO_ATTACK",
	"TORSO_STAND",
	"TORSO_READY",
	"TORSO_RELAX",

	"TORSO_RAISE2",	// (high)
	"TORSO_ATTACK2",
	"TORSO_STAND2",
	"TORSO_READY2",
	"TORSO_RELAX2",

	"TORSO_RAISE3",	// (pistol)
	"TORSO_ATTACK3",
	"TORSO_STAND3",
	"TORSO_READY3",
	"TORSO_RELAX3",

	"TORSO_RAISE4",	// (shoulder)
	"TORSO_ATTACK4",
	"TORSO_STAND4",
	"TORSO_READY4",
	"TORSO_RELAX4",

	"TORSO_RAISE5",	// (throw)
	"TORSO_ATTACK5",
	"TORSO_ATTACK5B",
	"TORSO_STAND5",
	"TORSO_READY5",
	"TORSO_RELAX5",

	"TORSO_RELOAD1",	// (low)
	"TORSO_RELOAD2",	// (high)
	"TORSO_RELOAD3",	// (pistol)
	"TORSO_RELOAD4",	// (shoulder)

	"TORSO_MG42",		// firing tripod mounted weapon animation

	"TORSO_MOVE",		// torso anim to play while moving and not firing (swinging arms type thing)

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

	"LEGS_BOOT",		// kicking animation

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
gitem_t bg_itemlist[] = {
#include "tce_item_catalog.inc"
};

int		bg_numItems = sizeof(bg_itemlist) / sizeof(bg_itemlist[0]) - 1;

/*
==============
BG_FindItemForHoldable
==============
*/
gitem_t	*BG_FindItemForHoldable( holdable_t pw ) {
	int		i;

	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( bg_itemlist[i].giType == IT_HOLDABLE && bg_itemlist[i].giTag == pw ) {
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
gitem_t	*BG_FindItemForWeapon( weapon_t weapon ) {
	gitem_t	*it;
	
	for ( it = bg_itemlist + 1 ; it->classname ; it++) {
		if ( it->giType == IT_WEAPON && it->giTag == weapon ) {
			return it;
		}
	}

	Com_Error( ERR_DROP, "Couldn't find item for weapon %i", weapon);
	return NULL;
}

//----(SA) added

#define DEATHMATCH_SHARED_AMMO 0


/*
==============
BG_FindClipForWeapon
==============
*/
weapon_t BG_FindClipForWeapon(weapon_t weapon) {
    return (weapon_t)TCE_BG_FindClipForWeapon(weapon);
}



/*
==============
BG_FindAmmoForWeapon
==============
*/
weapon_t BG_FindAmmoForWeapon(weapon_t weapon) {
    return (weapon_t)TCE_BG_FindAmmoForWeapon(weapon);
}

/*
==============
BG_AkimboFireSequence
	returns 'true' if it's the left hand's turn to fire, 'false' if it's the right hand's turn
==============
*/
qboolean BG_AkimboFireSequence( int weapon, int akimboClip, int mainClip ) {
    /* TC IDs apply during initialization too; UI does not load server Gear. */
    return TCE_BG_AkimboFireSequence(weapon, akimboClip, mainClip);
}

/*
==============
BG_IsAkimboWeapon
==============
*/
qboolean BG_IsAkimboWeapon( int weaponNum ) {
    /* TC IDs apply during initialization too; UI does not load server Gear. */
    return TCE_BG_IsAkimboWeapon(weaponNum);
}

/*
==============
BG_IsAkimboSideArm
==============
*/
qboolean BG_IsAkimboSideArm( int weaponNum, playerState_t *ps ) {
	switch( weaponNum )
	{
	case WP_COLT:	if( ps->weapon == WP_AKIMBO_COLT || ps->weapon == WP_AKIMBO_SILENCEDCOLT  )		return qtrue;	break;
	case WP_LUGER:	if( ps->weapon == WP_AKIMBO_LUGER || ps->weapon == WP_AKIMBO_SILENCEDLUGER )	return qtrue;	break;
	}
	return qfalse;
}

/*
==============
BG_AkimboSidearm
==============
*/
int BG_AkimboSidearm( int weaponNum ) {
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
	int		i = 0;

	for (;i < bg_numItems; i++)
	{
		if ( bg_itemlist[i].giType == IT_AMMO && bg_itemlist[i].giAmmoIndex == ammo )
			return &bg_itemlist[i];
	}
	Com_Error( ERR_DROP, "Item not found for ammo: %d", ammo );
	return NULL;
}
//----(SA) end


/*
===============
BG_FindItem
===============
*/
gitem_t	*BG_FindItem( const char *pickupName ) {
	gitem_t	*it;
	
	for ( it = bg_itemlist + 1 ; it->classname ; it++ ) {
		if ( !Q_stricmp( it->pickup_name, pickupName ) )
			return it;
	}

	return NULL;
}

gitem_t	*BG_FindItemForClassName( const char *className ) {
	gitem_t	*it;
	
	for ( it = bg_itemlist + 1 ; it->classname ; it++ ) {
		if ( !Q_stricmp( it->classname, className ) )
			return it;
	}

	return NULL;
}


// DHM - Nerve :: returns qtrue if a weapon is indeed used in multiplayer
// Gordon: FIXME: er, we shouldnt really need this, just remove all the weapons we dont actually want :)
qboolean BG_WeaponInWolfMP( int weapon ) {
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
qboolean BG_PlayerTouchesItem( playerState_t *ps, entityState_t *item, int atTime ) {
	vec3_t		origin;

	/* TC2002b510 uses the six-argument trajectory ABI, including raw mode13. */
	TCE_BG_EvaluateTrajectory( &item->pos, atTime, origin, qfalse, item->effect2Time, 1.0f );

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
int BG_GrenadesForClass( int cls, int* skills ) {
	switch( cls ) {
		case PC_MEDIC:
			if( skills[SK_FIRST_AID] >= 1 ) {
				return 2;
			}
			return 1;
		case PC_SOLDIER:
			return 4;
		case PC_ENGINEER:
			return 8;
		case PC_FIELDOPS:
			if( skills[SK_SIGNALS] >= 1 ) {
				return 2;
			}
			return 1;
		case PC_COVERTOPS:
			return 2;
	}

	return 0;
}

weapon_t BG_GrenadeTypeForTeam( team_t team ) {
	switch( team ) {
		case TEAM_AXIS:
			return WP_GRENADE_LAUNCHER;
		case TEAM_ALLIES:
			return WP_GRENADE_PINEAPPLE;
		default:
			return WP_NONE;
	}
}

// Gordon: setting numOfClips = 0 allows you to check if the client needs ammo, but doesnt give any
qboolean BG_AddMagicAmmo( playerState_t *ps, int *skill, int teamNum, int numOfClips ) {
	int			i, weapon;
	int			ammoAdded = qfalse;
	int			maxammo;
	int			clip;
	int			weapNumOfClips;

	// Gordon: handle grenades first
	i = BG_GrenadesForClass( ps->stats[STAT_PLAYER_CLASS], skill );
	weapon = BG_GrenadeTypeForTeam( teamNum );

	clip = BG_FindClipForWeapon(weapon);
	if( ps->ammoclip[clip] < i ) {

		// Gordon: early out
		if( !numOfClips ) {
			return qtrue;
		}

		ps->ammoclip[clip] += numOfClips;
		
		ammoAdded = qtrue;

		COM_BitSet(ps->weapons, weapon);

		if( ps->ammoclip[clip] > i) {
			ps->ammoclip[clip] = i;
		}
	}

	if( COM_BitCheck( ps->weapons, WP_MEDIC_SYRINGE ) ) {
		i = skill[ SK_FIRST_AID ] >= 2 ? 12 : 10;

		clip = BG_FindClipForWeapon( WP_MEDIC_SYRINGE );

		if( ps->ammoclip[ clip ] < i ) {
			if( !numOfClips ) {
				return qtrue;
			}

			ps->ammoclip[ clip ] += numOfClips;
			
			ammoAdded = qtrue;

			if( ps->ammoclip[ clip ] > i) {
				ps->ammoclip[ clip ] = i;
			}
		}
	}

	// Gordon: now other weapons
	for(i = 0; reloadableWeapons[i] >= 0; i++) {
		weapon = reloadableWeapons[i];
		if (COM_BitCheck(ps->weapons, weapon)) {
			maxammo = BG_MaxAmmoForWeapon( weapon, skill );

			// Handle weapons that just use clip, and not ammo
			if( weapon == 66 ) {
				clip = BG_FindAmmoForWeapon( weapon );
				if( ps->ammoclip[clip] < maxammo ) {
					// early out
					if(!numOfClips) {
						return qtrue;
					}

					ammoAdded = qtrue;
					ps->ammoclip[clip] = maxammo;
				}
			} else if( weapon == 65 ) {
				clip = BG_FindAmmoForWeapon( weapon );
				if( ps->ammoclip[clip] < maxammo ) {
					// early out
					if(!numOfClips) {
						return qtrue;
					}

					ammoAdded = qtrue;
					ps->ammoclip[clip] += numOfClips;
					if( ps->ammoclip[clip] >= maxammo ) {
						ps->ammoclip[clip] = maxammo;
					}
				}
			} else {
				clip = BG_FindAmmoForWeapon(weapon);
				if( ps->ammo[clip] < maxammo ) {
					// early out
					if(!numOfClips) {
						return qtrue;
					}
					ammoAdded = qtrue;

					if( BG_IsAkimboWeapon( weapon ) ) {
						weapNumOfClips = numOfClips * 2; // double clips babeh!
					} else {
						weapNumOfClips = numOfClips;
					}

					// add and limit check
					ps->ammo[clip] += weapNumOfClips * weaponDef[weapon].maxclip;
					if (ps->ammo[clip] > maxammo) {
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
qboolean BG_CanUseWeapon(int classNum, int teamNum, weapon_t weapon) {
	// TAT 1/11/2003 - is this SP game? - different weapons available in SP
	qboolean isSinglePlayer = BG_IsSinglePlayerGame() ? qtrue : qfalse;

	switch (classNum) {
		case PC_ENGINEER:
			if (weapon == WP_PLIERS
				|| weapon == WP_DYNAMITE
				|| weapon == WP_LANDMINE)
				return qtrue;
			else if (weapon == WP_MP40 
					|| weapon == WP_KAR98)
				return (teamNum == TEAM_AXIS);
			else if (weapon == WP_THOMPSON
					|| weapon == WP_CARBINE)
				return (teamNum == TEAM_ALLIES);
		case PC_FIELDOPS:	
			// TAT 1/11/2003 - in SP, field op can only use handgun, check after switch below
			if (isSinglePlayer && teamNum == TEAM_ALLIES)
				break;

			if (weapon == WP_STEN)
				return qtrue;
			else if (weapon == WP_MP40)
				return (teamNum == TEAM_AXIS);
			else if (weapon == WP_THOMPSON)
				return (teamNum == TEAM_ALLIES);
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
				|| weapon == 60 )
				return qtrue;
			else if (weapon == WP_MP40)
				return (teamNum == TEAM_AXIS);
			else if (weapon == WP_THOMPSON)
				return (teamNum == TEAM_ALLIES);
			break;

		case PC_MEDIC:		
			if (weapon == WP_MEDIC_SYRINGE
				|| weapon == WP_MEDKIT)
				return qtrue;

			// TAT 1/11/2003 - in SP, medic can only use handgun, check after switch below
			else if (isSinglePlayer && teamNum == TEAM_ALLIES)
				break;

			else if (weapon == WP_MP40)
				return (teamNum == TEAM_AXIS);
			else if (weapon == WP_THOMPSON)
				return (teamNum == TEAM_ALLIES);
			break;
		case PC_COVERTOPS:
			if (weapon == WP_STEN
				|| weapon == WP_SMOKE_BOMB
				|| weapon == WP_SATCHEL
				|| weapon == WP_AMMO
				// Gordon: this is a cvt ops weapon in single player too, right?
				|| weapon == WP_FG42)
				return qtrue;
			else if (weapon == WP_K43)
				return (teamNum == TEAM_AXIS);
			else if (weapon == WP_GARAND)
				return (teamNum == TEAM_ALLIES);
			break;
	}

	if (weapon == WP_NONE
		|| weapon == WP_KNIFE
		|| weapon == WP_LUGER
		|| weapon == WP_COLT)
		return qtrue;

	// if not any of the above
	return qfalse;
}

#define AMMOFORWEAP	BG_FindAmmoForWeapon(item->giTag)
/*
================
BG_CanItemBeGrabbed

Returns false if the item should not be picked up.
This needs to be the same for client side prediction and server use.
================
*/
qboolean	BG_CanItemBeGrabbed( const entityState_t *ent, const playerState_t *ps, int *skill, int teamNum ) {
	gitem_t	*item;

	if ( ent->modelindex < 1 || ent->modelindex >= bg_numItems ) {
		Com_Error( ERR_DROP, "BG_CanItemBeGrabbed: index out of range" );
	}

	item = &bg_itemlist[ent->modelindex];

	switch( item->giType ) {
	case IT_WEAPON:
		/* TC 2002ba60: VIP/restricted carrier states cannot take weapons. */
		if (ps->stats[STAT_TCE_FLAGS] & 0x500) {
			return qfalse;
		}
		if( item->giTag == WP_AMMO ) {
			// magic ammo for any two-handed weapon
			// xkan, 11/21/2002 - only pick up if ammo is not full, numClips is 0, so ps will
			// NOT be changed (I know, it places the burden on the programmer, rather than the 
			// compiler, to ensure that).
			return BG_AddMagicAmmo( (playerState_t *)ps, skill, teamNum, 0);	// Arnout: had to cast const away
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
		if(ent->density < 1)
			return qfalse;

		// DHM - Nerve :: otherEntity2 is now used instead of modelindex2
		// ent->modelindex2 is non-zero on items if they are dropped
		// we need to know this because we can pick up our dropped flag (and return it)
		// but we can't pick up our flag at base
		if (ps->persistant[PERS_TEAM] == TEAM_AXIS) {
			if (item->giTag == PW_BLUEFLAG ||
				(item->giTag == PW_REDFLAG && ent->otherEntityNum2))
				return qtrue;
		} else if (ps->persistant[PERS_TEAM] == TEAM_ALLIES) {
			if (item->giTag == PW_REDFLAG ||
				(item->giTag == PW_BLUEFLAG && ent->otherEntityNum2))
				return qtrue;
		}

		return qfalse;

	
	case IT_HOLDABLE:
		return qtrue;

	case IT_TREASURE:	// treasure always picked up
		return qtrue;

	case IT_KEY:
		return qtrue;	// keys are always picked up

	case IT_BAD:
		Com_Error( ERR_DROP, "BG_CanItemBeGrabbed: IT_BAD" );

	}
	return qfalse;
}

//======================================================================

#ifdef CGAMEDLL
void BG_CalculateSpline_r(splinePath_t *, vec3_t, vec3_t, float);
#else
void BG_CalculateSpline_r(splinePath_t* spline, vec3_t out1, vec3_t out2, float tension) {
	vec3_t points[18];
	int i;
	int count = spline->numControls + 2;
	vec3_t dist;

	VectorCopy( spline->point.origin, points[0] );
	for( i = 0; i < spline->numControls; i++ ) {
		VectorCopy( spline->controls[i].origin, points[i+1] );
	}
	if(!spline->next) {
		return;
//		Com_Error( ERR_DROP, "Spline (%s) with no target referenced", spline->point.name );
	}
	VectorCopy( spline->next->point.origin, points[i+1] );


	while(count > 2) {
		for( i = 0; i < count-1; i++ ) {
#ifdef _WIN32
			/* TC2002bc5a: X stays extended; Y/Z differences are stored
			 * before the multiply/add, then each result is stored once. */
			double dx = (double)points[i+1][0] - (double)points[i][0];
			dist[1] = points[i+1][1] - points[i][1];
			dist[2] = points[i+1][2] - points[i][2];
			points[i][0] = (float)(dx * (double)tension + (double)points[i][0]);
			points[i][1] = (float)((double)dist[1] * (double)tension + (double)points[i][1]);
			points[i][2] = (float)((double)dist[2] * (double)tension + (double)points[i][2]);
#else
			VectorSubtract( points[i+1], points[i], dist );
			VectorMA(points[i], tension, dist, points[i]);
#endif
		}
		count--;
	}

	VectorCopy( points[0], out1 );
	VectorCopy( points[1], out2 );
}

#endif

#ifdef CGAMEDLL
qboolean BG_TraverseSpline(float *, splinePath_t **);
#else
qboolean BG_TraverseSpline( float* deltaTime, splinePath_t** pSpline) {
	float dist;

	while( (*deltaTime) > 1 ) {
		(*deltaTime) -= 1;
		dist = (*pSpline)->length * (*deltaTime);

		if(!(*pSpline)->next || !(*pSpline)->next->length) {
			return qfalse;
//			Com_Error( ERR_DROP, "Spline path end passed (%s)", (*pSpline)->point.name );
		}

		(*pSpline) = (*pSpline)->next;
		*deltaTime = dist / (*pSpline)->length;
	}

	while( (*deltaTime) < 0 ) {
		dist = -((*pSpline)->length * (*deltaTime));

		if(!(*pSpline)->prev || !(*pSpline)->prev->length) {
			return qfalse;
//			Com_Error( ERR_DROP, "Spline path end passed (%s)", (*pSpline)->point.name );
		}

		(*pSpline) = (*pSpline)->prev;
		(*deltaTime) = 1 - (dist / (*pSpline)->length);
	}

	return qtrue;
}

#endif

/*
================
BG_RaySphereIntersection

================
*/

#if defined(_MSC_VER) && defined(_M_IX86) && defined(CGAMEDLL)
/* TC cgame 30003ed0: original x87 operand/store order. */
static const float tceSplineZero828 = 0.0f;
static const float tceSplineHalf828 = 0.5f;
static const float tceSplineFour828 = 4.0f;
static const float tceSplineGranularity828 = 0.0625f;
typedef char tceSplineSegmentLayout828[(offsetof(splineSegment_t,start)==0 && offsetof(splineSegment_t,v_norm)==12 && offsetof(splineSegment_t,length)==24 && sizeof(splineSegment_t)==28 && MAX_SPLINE_SEGMENTS==16) ? 1 : -1];
__declspec(naked) qboolean BG_RaySphereIntersection( float radius, vec3_t origin, splineSegment_t* path, float *t0, float *t1 ) {
    __asm {
        MOV EAX,dword ptr [ESP + 0xc]
        MOV ECX,dword ptr [ESP + 0x8]
        FLD dword ptr [EAX]
        FSUB dword ptr [ECX]
        FLD dword ptr [EAX + 0x4]
        FSUB dword ptr [ECX + 0x4]
        FLD dword ptr [EAX + 0x8]
        FSUB dword ptr [ECX + 0x8]
        FLD ST(0)
        FMUL dword ptr [EAX + 0x14]
        FLD ST(2)
        FMUL dword ptr [EAX + 0x10]
        FADDP ST(1),ST(0)
        FLD ST(3)
        FMUL dword ptr [EAX + 0xc]
        FADDP ST(1),ST(0)
        FADD ST(0),ST(0)
        FST dword ptr [ESP + 0xc]
        FMUL dword ptr [ESP + 0xc]
        FLD ST(1)
        FMUL ST(0),ST(2)
        FLD ST(3)
        FMUL ST(0),ST(4)
        FADDP ST(1),ST(0)
        FLD ST(4)
        FMUL ST(0),ST(5)
        FADDP ST(1),ST(0)
        FLD dword ptr [ESP + 0x4]
        FMUL dword ptr [ESP + 0x4]
        FSUBP ST(1),ST(0)
        FMUL dword ptr [tceSplineFour828]
        FSUBP ST(1),ST(0)
        FSTP ST(3)
        FSTP ST(0)
        FSTP ST(0)
        FCOM dword ptr [tceSplineZero828]
        FNSTSW AX
        TEST AH,0x1
        JZ spline828_30003f3f
        FSTP ST(0)
        XOR EAX,EAX
        RET
spline828_30003f3f:
        FSQRT
        MOV EAX,dword ptr [ESP + 0x10]
        MOV ECX,dword ptr [ESP + 0x14]
        FLD ST(0)
        FSUB dword ptr [ESP + 0xc]
        FMUL dword ptr [tceSplineHalf828]
        FSTP dword ptr [EAX]
        FLD dword ptr [ESP + 0xc]
        FCHS
        FSUB ST(0),ST(1)
        MOV EAX,0x1
        FMUL dword ptr [tceSplineHalf828]
        FSTP dword ptr [ECX]
        FSTP ST(0)
        RET
    }
}
#else
qboolean BG_RaySphereIntersection( float radius, vec3_t origin, splineSegment_t* path, float *t0, float *t1 ) {
	vec3_t v;
	float b, c, d;

	VectorSubtract( path->start, origin, v );
	
	b = 2 * DotProduct( v, path->v_norm );
	c = DotProduct( v, v ) - (radius * radius);

	d = (b * b) - (4 * c);
	if( d < 0 ) {
		return qfalse;
	}
	d = sqrt( d );

	*t0 = (-b + d) * 0.5f;
	*t1 = (-b - d) * 0.5f;

	return qtrue;
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86) && defined(CGAMEDLL)
/* TC30003f70: original ST0/argument-slot contract; fifth native argument unused. */
static const float tcePathSegments829 = 16.0f, tcePathGranularity829 = 0.0625f;
static const float tcePathZero829 = 0.0f, tcePathOne829 = 1.0f;
static double (__cdecl *const tcePathFloor829)(double) = floor;
enum {
 path829StartX=offsetof(splinePath_t,segments)+offsetof(splineSegment_t,start),
 path829StartY=path829StartX+sizeof(float), path829StartZ=path829StartX+2*sizeof(float),
 path829NormX=offsetof(splinePath_t,segments)+offsetof(splineSegment_t,v_norm),
 path829NormY=path829NormX+sizeof(float), path829NormZ=path829NormX+2*sizeof(float),
 path829Length=offsetof(splinePath_t,segments)+offsetof(splineSegment_t,length),
 path829Next=offsetof(splinePath_t,next),path829Prev=offsetof(splinePath_t,prev)
};
/* Private original __ftol ST0 ABI; not a C float argument or spilled floor result. */
__declspec(naked) static int TCE_PathInteger829(void) {
 __asm {
  push ebp
  mov ebp,esp
  sub esp,12
  fstcw word ptr [ebp-2]
  wait
  mov ax,word ptr [ebp-2]
  or ah,0ch
  mov word ptr [ebp-4],ax
  fldcw word ptr [ebp-4]
  fistp qword ptr [ebp-12]
  fldcw word ptr [ebp-2]
  mov eax,dword ptr [ebp-12]
  mov edx,dword ptr [ebp-8]
  leave
  ret
 }
}
__declspec(naked) void BG_LinearPathOrigin2(float radius, splinePath_t** pSpline, float *deltaTime, vec3_t result, qboolean backwards) {
    __asm {
        SUB ESP,0xc
        PUSH EBX
        PUSH EBP
        PUSH ESI
        MOV ESI,dword ptr [ESP + 0x24]
        PUSH EDI
        MOV dword ptr [ESP + 0x18],0x1
        FLD dword ptr [ESI]
        FMUL dword ptr [tcePathSegments829]
        SUB ESP,0x8
        FSTP qword ptr [ESP]
        CALL dword ptr [tcePathFloor829]
        ADD ESP,0x8
        CALL TCE_PathInteger829
        MOV EBX,EAX
        CMP EBX,0x10
        MOV dword ptr [ESP + 0x10],EBX
        JL path829_30003fbc
        MOV EBX,0xf
        MOV dword ptr [ESP + 0x14],0x3f800000
        MOV dword ptr [ESP + 0x10],EBX
        JMP path829_30003fcc
path829_30003fbc:
        FLD dword ptr [ESI]
        FMUL dword ptr [tcePathSegments829]
        FISUB dword ptr [ESP + 0x10]
        FSTP dword ptr [ESP + 0x14]
path829_30003fcc:
        MOV EBP,dword ptr [ESP + 0x2c]
        MOV EDI,dword ptr [ESP + 0x24]
path829_30003fd4:
        LEA ESI,[EBX*0x8 + 0x0]
        SUB ESI,EBX
        SHL ESI,0x2
path829_30003fe0:
        MOV EDX,dword ptr [EDI]
        LEA EAX,[ESP + 0x24]
        PUSH EAX
        LEA ECX,[ESP + 0x30]
        LEA EAX,[EDX + ESI*0x1 + path829StartX]
        PUSH ECX
        MOV ECX,dword ptr [ESP + 0x28]
        PUSH EAX
        PUSH EBP
        PUSH ECX
        CALL BG_RaySphereIntersection
        ADD ESP,0x14
        TEST EAX,EAX
        JZ path829_300040b7
        MOV EDX,dword ptr [EDI]
        FLD dword ptr [ESP + 0x2c]
        FDIV dword ptr [EDX + ESI*0x1 + path829Length]
        LEA EAX,[EDX + ESI*0x1 + path829Length]
        FSTP dword ptr [ESP + 0x2c]
        FLD dword ptr [ESP + 0x24]
        FDIV dword ptr [EAX]
        MOV EAX,dword ptr [ESP + 0x18]
        TEST EAX,EAX
        FSTP dword ptr [ESP + 0x24]
        FLD dword ptr [ESP + 0x20]
        FCOMP dword ptr [tcePathZero829]
        FNSTSW AX
        JZ path829_30004128
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [ESP + 0x14]
        TEST AH,0x1
        FNSTSW AX
        JZ path829_300040e7
        TEST AH,0x1
        JZ path829_30004084
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [tcePathZero829]
        FNSTSW AX
        TEST AH,0x1
        JNZ path829_30004084
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [tcePathOne829]
        FNSTSW AX
        TEST AH,0x41
        JZ path829_30004084
        FLD dword ptr [ESP + 0x2c]
        JMP path829_30004097
path829_30004084:
        FLD dword ptr [ESP + 0x24]
        FCOMP dword ptr [ESP + 0x14]
        FNSTSW AX
        TEST AH,0x1
        JZ path829_300040b7
path829_30004093:
        FLD dword ptr [ESP + 0x24]
path829_30004097:
        FCOM dword ptr [tcePathZero829]
        FNSTSW AX
        TEST AH,0x1
        JNZ path829_300040b5
        FCOM dword ptr [tcePathOne829]
        FNSTSW AX
        TEST AH,0x41
        JNZ path829_30004210
path829_300040b5:
        FSTP ST(0)
path829_300040b7:
        FLD dword ptr [ESP + 0x20]
        FCOMP dword ptr [tcePathZero829]
        MOV dword ptr [ESP + 0x18],0x0
        FNSTSW AX
        TEST AH,0x1
        JZ path829_300041af
        DEC EBX
        SUB ESI,0x1c
        MOV dword ptr [ESP + 0x10],EBX
        JS path829_300041c4
        JMP path829_30003fe0
path829_300040e7:
        TEST AH,0x41
        JNZ path829_30004114
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [tcePathZero829]
        FNSTSW AX
        TEST AH,0x1
        JNZ path829_30004114
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [tcePathOne829]
        FNSTSW AX
        TEST AH,0x41
        JZ path829_30004114
        FLD dword ptr [ESP + 0x2c]
        JMP path829_30004097
path829_30004114:
        FLD dword ptr [ESP + 0x24]
        FCOMP dword ptr [ESP + 0x14]
        FNSTSW AX
        TEST AH,0x41
        JNZ path829_300040b7
        JMP path829_30004093
path829_30004128:
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [ESP + 0x24]
        TEST AH,0x1
        FNSTSW AX
        JZ path829_30004173
        TEST AH,0x1
        JZ path829_30004093
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [tcePathZero829]
        FNSTSW AX
        TEST AH,0x1
        JNZ path829_30004093
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [tcePathOne829]
        FNSTSW AX
        TEST AH,0x41
        JZ path829_30004093
        FLD dword ptr [ESP + 0x2c]
        JMP path829_30004097
path829_30004173:
        TEST AH,0x41
        JNZ path829_30004093
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [tcePathZero829]
        FNSTSW AX
        TEST AH,0x1
        JNZ path829_30004093
        FLD dword ptr [ESP + 0x2c]
        FCOMP dword ptr [tcePathOne829]
        FNSTSW AX
        TEST AH,0x41
        JZ path829_30004093
        FLD dword ptr [ESP + 0x2c]
        JMP path829_30004097
path829_300041af:
        ADD ESI,0x1c
        INC EBX
        CMP ESI,0x1c0
        MOV dword ptr [ESP + 0x10],EBX
        JGE path829_300041cb
        JMP path829_30003fe0
path829_300041c4:
        MOV EBX,0xf
        JMP path829_300041cd
path829_300041cb:
        XOR EBX,EBX
path829_300041cd:
        FLD dword ptr [ESP + 0x20]
        FCOMP dword ptr [tcePathZero829]
        MOV dword ptr [ESP + 0x10],EBX
        FNSTSW AX
        TEST AH,0x1
        JZ path829_300041f9
        MOV EAX,dword ptr [EDI]
        MOV EAX,dword ptr [EAX + path829Prev]
        TEST EAX,EAX
        JZ path829_30004290
        MOV dword ptr [EDI],EAX
        JMP path829_30003fd4
path829_300041f9:
        MOV ECX,dword ptr [EDI]
        MOV EAX,dword ptr [ECX + path829Next]
        TEST EAX,EAX
        JZ path829_30004290
        MOV dword ptr [EDI],EAX
        JMP path829_30003fd4
path829_30004210:
        FILD dword ptr [ESP + 0x10]
        MOV EDX,dword ptr [ESP + 0x28]
        LEA EAX,[EBX*0x8 + 0x0]
        SUB EAX,EBX
        FMUL dword ptr [tcePathGranularity829]
        FLD ST(1)
        FMUL dword ptr [tcePathGranularity829]
        SHL EAX,0x2
        FADDP ST(1),ST(0)
        FSTP dword ptr [EDX]
        MOV ECX,dword ptr [EDI]
        ADD ECX,EAX
        FLD ST(0)
        FMUL dword ptr [ECX + path829Length]
        FMUL dword ptr [ECX + path829NormX]
        FADD dword ptr [ECX + path829StartX]
        FSTP dword ptr [EBP]
        MOV EDX,dword ptr [EDI]
        FLD ST(0)
        FMUL dword ptr [EDX + EAX*0x1 + path829Length]
        LEA ECX,[EDX + EAX*0x1]
        FMUL dword ptr [ECX + path829NormY]
        FADD dword ptr [ECX + path829StartY]
        LEA ECX,[EBX + 0x11]
        LEA EDX,[ECX*0x8 + 0x0]
        FSTP dword ptr [EBP + 0x4]
        MOV EDI,dword ptr [EDI]
        SUB EDX,ECX
        FMUL dword ptr [EDI + EAX*0x1 + path829Length]
        FMUL dword ptr [EDI + EAX + path829NormZ]
        FADD dword ptr [EDI + EAX*0x1 + path829StartZ]
        FSTP dword ptr [EBP + 0x8]
path829_30004290:
        POP EDI
        POP ESI
        POP EBP
        POP EBX
        ADD ESP,0xc
        RET
    }
}
#else
void BG_LinearPathOrigin2(float radius, splinePath_t** pSpline, float *deltaTime, vec3_t result, qboolean backwards) {
	qboolean first = qtrue;
	float t = 0.f;
	int i = floor((*deltaTime) * (MAX_SPLINE_SEGMENTS));
	float frac;
//	int x = 0;
//	splinePath_t* start = *pSpline;

	if(i >= MAX_SPLINE_SEGMENTS) {
		i = MAX_SPLINE_SEGMENTS - 1;
		frac = 1.f;
	} else {
		frac = (((*deltaTime) * (MAX_SPLINE_SEGMENTS)) - i);
	}

	while(qtrue) {
		float t0, t1;

		while(qtrue) {
			if(BG_RaySphereIntersection( radius, result, &(*pSpline)->segments[i], &t0, &t1 )) {
				qboolean found = qfalse;
				
				t0 /= (*pSpline)->segments[i].length;
				t1 /= (*pSpline)->segments[i].length;

				if(first) {
					if(radius < 0) {
						if(t0 < frac && (t0 >= 0.f && t0 <= 1.f)) {
							t = t0;
							found = qtrue;
						} else if (t1 < frac) {
							t = t1;
							found = qtrue;
						}
					} else {
						if(t0 > frac && (t0 >= 0.f && t0 <= 1.f)) {
							t = t0;
							found = qtrue;
						} else if (t1 > frac) {
							t = t1;
							found = qtrue;
						}
					}
				} else {
					if(radius < 0) {
						if(t0 < t1 && (t0 >= 0.f && t0 <= 1.f)) {
							t = t0;
							found = qtrue;
						} else {
							t = t1;
							found = qtrue;
						}
					} else {
						if(t0 > t1 && (t0 >= 0.f && t0 <= 1.f)) {
							t = t0;
							found = qtrue;
						} else {
							t = t1;
							found = qtrue;
						}
					}
				}

				if( found ) {
					if(t >= 0.f && t <= 1.f) {
						*deltaTime = (i / (float)(MAX_SPLINE_SEGMENTS)) + (t / (float)(MAX_SPLINE_SEGMENTS));
						VectorMA( (*pSpline)->segments[i].start, t * (*pSpline)->segments[i].length, (*pSpline)->segments[i].v_norm, result );
						return;
					}
				}
				found = qfalse;
			}
			
			first = qfalse;
			if(radius < 0) {
				i--;
				if(i < 0) {
					i = MAX_SPLINE_SEGMENTS - 1;
					break;
				}
			} else {
				i++;
				if(i >= MAX_SPLINE_SEGMENTS) {
					i = 0;
					break;
				}
			}
		}

		if( radius < 0 ) {
			if(!(*pSpline)->prev) {
				return;
//				Com_Error( ERR_DROP, "End of spline reached (%s)\n", start->point.name );
			}
			*pSpline = (*pSpline)->prev;
		} else {
			if(!(*pSpline)->next) {
				return;
//				Com_Error( ERR_DROP, "End of spline reached (%s)\n", start->point.name );
			}
			*pSpline = (*pSpline)->next;
		}
	}
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86) && defined(CGAMEDLL)
/* TC cgame 300042a0: original x87 operand/store order. */
enum { spline828SegmentY = offsetof(splinePath_t,segments) + sizeof(float) };
__declspec(naked) void BG_ComputeSegments(splinePath_t* pSpline) {
    __asm {
        SUB ESP,0x34
        PUSH EBX
        MOV EBX,dword ptr [ESP + 0x3c]
        PUSH EBP
        PUSH ESI
        PUSH EDI
        XOR EDI,EDI
        MOV dword ptr [ESP + 0x10],EDI
        LEA ESI,[EBX + spline828SegmentY]
spline828_300042b7:
        FILD dword ptr [ESP + 0x10]
        LEA ECX,[ESP + 0x20]
        LEA EDX,[ESP + 0x14]
        FMUL dword ptr [tceSplineGranularity828]
        FSTP dword ptr [ESP + 0x48]
        MOV EAX,dword ptr [ESP + 0x48]
        PUSH EAX
        PUSH ECX
        PUSH EDX
        PUSH EBX
        CALL BG_CalculateSpline_r
        FLD dword ptr [ESP + 0x30]
        FSUB dword ptr [ESP + 0x24]
        LEA EBP,[EDI + 0x1]
        LEA ECX,[ESP + 0x48]
        LEA EDX,[ESP + 0x3c]
        FSTP dword ptr [ESI + -0x4]
        FLD dword ptr [ESP + 0x34]
        FSUB dword ptr [ESP + 0x28]
        FSTP dword ptr [ESI]
        FLD dword ptr [ESP + 0x38]
        FSUB dword ptr [ESP + 0x2c]
        FSTP dword ptr [ESI + 0x4]
        FLD dword ptr [ESP + 0x58]
        FMUL dword ptr [ESI + -0x4]
        FADD dword ptr [ESP + 0x24]
        FSTP dword ptr [ESI + -0x4]
        FLD dword ptr [ESP + 0x58]
        FMUL dword ptr [ESI]
        FADD dword ptr [ESP + 0x28]
        FSTP dword ptr [ESI]
        FLD dword ptr [ESP + 0x58]
        FMUL dword ptr [ESI + 0x4]
        MOV dword ptr [ESP + 0x58],EBP
        FADD dword ptr [ESP + 0x2c]
        FSTP dword ptr [ESI + 0x4]
        FILD dword ptr [ESP + 0x58]
        FMUL dword ptr [tceSplineGranularity828]
        FSTP dword ptr [ESP + 0x58]
        MOV EAX,dword ptr [ESP + 0x58]
        PUSH EAX
        PUSH ECX
        PUSH EDX
        PUSH EBX
        CALL BG_CalculateSpline_r
        FLD dword ptr [ESP + 0x58]
        FSUB dword ptr [ESP + 0x4c]
        FLD dword ptr [ESP + 0x5c]
        FSUB dword ptr [ESP + 0x50]
        LEA EDI,[ESI + 0x8]
        PUSH EDI
        FSTP dword ptr [ESP + 0x3c]
        FLD dword ptr [ESP + 0x64]
        FSUB dword ptr [ESP + 0x58]
        FSTP dword ptr [ESP + 0x40]
        FLD dword ptr [ESP + 0x6c]
        FMUL ST(0),ST(1)
        FADD dword ptr [ESP + 0x50]
        FSTP dword ptr [ESP + 0x38]
        FSTP ST(0)
        FLD dword ptr [ESP + 0x6c]
        FMUL dword ptr [ESP + 0x3c]
        FADD dword ptr [ESP + 0x54]
        FSTP dword ptr [ESP + 0x3c]
        FLD dword ptr [ESP + 0x6c]
        FMUL dword ptr [ESP + 0x40]
        FADD dword ptr [ESP + 0x58]
        FSTP dword ptr [ESP + 0x40]
        FLD dword ptr [ESP + 0x38]
        FSUB dword ptr [ESI + -0x4]
        FSTP dword ptr [EDI]
        FLD dword ptr [ESP + 0x3c]
        FSUB dword ptr [ESI]
        FSTP dword ptr [ESI + 0xc]
        FLD dword ptr [ESP + 0x40]
        FSUB dword ptr [ESI + 0x4]
        FSTP dword ptr [ESI + 0x10]
        CALL VectorLength
        FSTP dword ptr [ESI + 0x14]
        PUSH EDI
        CALL VectorNormalize
        MOV EDI,EBP
        ADD ESP,0x28
        ADD ESI,0x1c
        CMP EDI,0x10
        FSTP ST(0)
        MOV dword ptr [ESP + 0x10],EDI
        JL spline828_300042b7
        POP EDI
        POP ESI
        POP EBP
        POP EBX
        ADD ESP,0x34
        RET
    }
}
#else
void BG_ComputeSegments(splinePath_t* pSpline) {
	int i;
	float granularity = 1 / ((float)(MAX_SPLINE_SEGMENTS));
	vec3_t	vec[4];

	for( i = 0; i < MAX_SPLINE_SEGMENTS; i++ ) {
		BG_CalculateSpline_r( pSpline, vec[0], vec[1], i * granularity );
		VectorSubtract( vec[1], vec[0], pSpline->segments[i].start );
		VectorMA(vec[0], i * granularity, pSpline->segments[i].start, pSpline->segments[i].start );

		BG_CalculateSpline_r( pSpline, vec[2], vec[3], (i + 1) * granularity );
		VectorSubtract( vec[3], vec[2], vec[0] );
		VectorMA(vec[2], (i + 1) * granularity, vec[0], vec[0] );

		VectorSubtract( vec[0], pSpline->segments[i].start, pSpline->segments[i].v_norm );
		pSpline->segments[i].length = VectorLength( pSpline->segments[i].v_norm );
		VectorNormalize( pSpline->segments[i].v_norm );
	}
}
#endif

/*
================
BG_EvaluateTrajectory

================
*/
void BG_EvaluateTrajectory( const trajectory_t *tr, int atTime, vec3_t result, qboolean isAngle, int splinePath ) {
	float		deltaTime;
	float		phase;
	vec3_t		v;

	splinePath_t* pSpline;
	vec3_t vec[2];
	qboolean backwards = qfalse;
	float deltaTime2;

#if defined(CGAMEDLL) || defined(GAMEDLL)
	/* Script producers and linked-entity consumers use original TC path16.
	 * All runtime types now share the original six-argument evaluator;
	 * legacy five-argument callers have the original default gravity scale1. */
	{
		TCE_BG_EvaluateTrajectory( tr, atTime, result, isAngle, splinePath, 1.0f );
		return;
	}
#endif

	switch( tr->trType ) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
	case TR_GRAVITY_PAUSED:	//----(SA)	
		VectorCopy( tr->trBase, result );
		break;
	case TR_LINEAR:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		break;
	case TR_SINE:
		deltaTime = ( atTime - tr->trTime ) / (float) tr->trDuration;
		phase = sin( deltaTime * M_PI * 2 );
		VectorMA( tr->trBase, phase, tr->trDelta, result );
		break;
//----(SA)	removed
	case TR_LINEAR_STOP:
		if ( atTime > tr->trTime + tr->trDuration ) {
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		if ( deltaTime < 0 ) {
			deltaTime = 0;
		}
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		break;
	case TR_GRAVITY:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;		// FIXME: local gravity...
		break;
	// Ridah
	case TR_GRAVITY_LOW:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		result[2] -= 0.5 * (DEFAULT_GRAVITY * 0.3) * deltaTime * deltaTime;		// FIXME: local gravity...
		break;
	// done.
//----(SA)	
	case TR_GRAVITY_FLOAT:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		result[2] -= 0.5 * (DEFAULT_GRAVITY * 0.2) * deltaTime;
		break;
//----(SA)	end
	// RF, acceleration
	case TR_ACCELERATE:		// trDelta is the ultimate speed
		if ( atTime > tr->trTime + tr->trDuration ) {
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		// phase is the acceleration constant
		phase = VectorLength( tr->trDelta ) / (tr->trDuration*0.001);
		// trDelta at least gives us the acceleration direction
		VectorNormalize2( tr->trDelta, result );
		// get distance travelled at current time
		VectorMA( tr->trBase, phase * 0.5 * deltaTime * deltaTime, result, result );
		break;
	case TR_DECCELERATE:	// trDelta is the starting speed
		if ( atTime > tr->trTime + tr->trDuration ) {
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		// phase is the breaking constant
		phase = VectorLength( tr->trDelta ) / (tr->trDuration*0.001);
		// trDelta at least gives us the acceleration direction
		VectorNormalize2( tr->trDelta, result );
		// get distance travelled at current time (without breaking)
		VectorMA( tr->trBase, deltaTime, tr->trDelta, v );
		// subtract breaking force
		VectorMA( v, -phase * 0.5 * deltaTime * deltaTime, result, result );
		break;
	case TR_SPLINE:
		if(!(pSpline = BG_GetSplineData( splinePath, &backwards ))) {
			return;
		}

		deltaTime = tr->trDuration ? (atTime - tr->trTime) / ((float)tr->trDuration) : 0;

		if(deltaTime < 0.f) {
			deltaTime = 0.f;
		} else if(deltaTime > 1.f) {
			deltaTime = 1.f;
		}

		if(backwards) {
			deltaTime = 1 - deltaTime;
		}

/*		if(pSpline->isStart) {
			deltaTime = 1 - sin((1 - deltaTime) * M_PI * 0.5f);
		} else if(pSpline->isEnd) {
			deltaTime = sin(deltaTime * M_PI * 0.5f);
		}*/

		deltaTime2 = deltaTime;

		BG_CalculateSpline_r( pSpline, vec[0], vec[1], deltaTime );

		if(isAngle) {
			qboolean dampin = qfalse;
			qboolean dampout = qfalse;
			float base1;

			if(tr->trBase[0]) {
//				int pos = 0;
				vec3_t result2;
				splinePath_t* pSp2 = pSpline;

				deltaTime2 += tr->trBase[0] / pSpline->length;

				if(BG_TraverseSpline( &deltaTime2, &pSp2 )) {

					VectorSubtract( vec[1], vec[0], result );
					VectorMA(vec[0], deltaTime, result, result);	

					BG_CalculateSpline_r( pSp2, vec[0], vec[1], deltaTime2 );

					VectorSubtract( vec[1], vec[0], result2 );
					VectorMA(vec[0], deltaTime2, result2, result2);

					if(	tr->trBase[0] < 0 ) {
						VectorSubtract( result, result2, result );
					} else {
						VectorSubtract( result2, result, result );
					}
				} else {
					VectorSubtract( vec[1], vec[0], result );
				}
			} else {
				VectorSubtract( vec[1], vec[0], result );
			}

			vectoangles( result, result );

			base1 = tr->trBase[1];
			if(base1 >= 10000 || base1 < -10000) {
				dampin = qtrue;
				if(base1 < 0) {
					base1 += 10000;
				} else {
					base1 -= 10000;
				}
			}

			if(base1 >= 1000 || base1 < -1000) {
				dampout = qtrue;
				if(base1 < 0) {
					base1 += 1000;
				} else {
					base1 -= 1000;
				}
			}

			if(dampin && dampout) {
				result[ROLL] = base1 + ((sin(((deltaTime * 2) - 1) * M_PI * 0.5f) + 1) * 0.5f * tr->trBase[2]);
			} else if(dampin) {
				result[ROLL] = base1 + (sin(deltaTime * M_PI * 0.5f) * tr->trBase[2]);
			} else if(dampout) {
				result[ROLL] = base1 + ((1 - sin((1 - deltaTime) * M_PI * 0.5f)) * tr->trBase[2]);
			} else {
				result[ROLL] = base1 + (deltaTime * tr->trBase[2]);
			}
		} else {
			VectorSubtract( vec[1], vec[0], result );
			VectorMA(vec[0], deltaTime, result, result);	
		}

		break;
	case TR_LINEAR_PATH:
		if(!(pSpline = BG_GetSplineData( splinePath, &backwards ))) {
			return;
		}

		deltaTime = tr->trDuration ? (atTime - tr->trTime) / ((float)tr->trDuration) : 0;

		if(deltaTime < 0.f) {
			deltaTime = 0.f;
		} else if(deltaTime > 1.f) {
			deltaTime = 1.f;
		}

		if(backwards) {
			deltaTime = 1 - deltaTime;
		}

		if(isAngle) {
			int pos = floor(deltaTime * (MAX_SPLINE_SEGMENTS));
			float frac;

			if(pos >= MAX_SPLINE_SEGMENTS) {
				pos = MAX_SPLINE_SEGMENTS - 1;
				frac = pSpline->segments[pos].length;
			} else {
				frac = ((deltaTime * (MAX_SPLINE_SEGMENTS)) - pos) * pSpline->segments[pos].length;
			}

			if(tr->trBase[0]) {
				VectorMA( pSpline->segments[pos].start, frac, pSpline->segments[pos].v_norm, result );
				VectorCopy( result, v );

				BG_LinearPathOrigin2( tr->trBase[0], &pSpline, &deltaTime, v, backwards );
				if(	tr->trBase[0] < 0 ) {
					VectorSubtract( v, result, result );
				} else {
					VectorSubtract( result, v, result );
				}

				vectoangles( result, result );
			} else {
				vectoangles( pSpline->segments[pos].v_norm, result );
			}

		} else {
			int pos = floor(deltaTime * (MAX_SPLINE_SEGMENTS));
			float frac;

			if(pos >= MAX_SPLINE_SEGMENTS) {
				pos = MAX_SPLINE_SEGMENTS - 1;
				frac = pSpline->segments[pos].length;
			} else {
				frac = ((deltaTime * (MAX_SPLINE_SEGMENTS)) - pos) * pSpline->segments[pos].length;
			}

			VectorMA( pSpline->segments[pos].start, frac, pSpline->segments[pos].v_norm, result );
		}

		break;
	default:
		Com_Error( ERR_DROP, "BG_EvaluateTrajectory: unknown trType: %i", tr->trTime );
		break;
	}
}


/*
================
BG_EvaluateTrajectoryDelta

For determining velocity at a given time
================
*/
void BG_EvaluateTrajectoryDelta( const trajectory_t *tr, int atTime, vec3_t result, qboolean isAngle, int splineData ) {
	float	deltaTime;
	float	phase;

#if defined(CGAMEDLL) || defined(GAMEDLL)
	/* Script producers and linked-entity consumers use original TC path16.
	 * All runtime types now share the original six-argument evaluator;
	 * legacy five-argument callers have the original default gravity scale1. */
	{
		TCE_BG_EvaluateTrajectoryDelta( tr, atTime, result, isAngle, splineData, 1.0f );
		return;
	}
#endif

	switch( tr->trType ) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorClear( result );
		break;
	case TR_LINEAR:
		VectorCopy( tr->trDelta, result );
		break;
	case TR_SINE:
		deltaTime = ( atTime - tr->trTime ) / (float) tr->trDuration;
		phase = cos( deltaTime * M_PI * 2 );	// derivative of sin = cos
		phase *= 0.5;
		VectorScale( tr->trDelta, phase, result );
		break;
//----(SA)	removed
	case TR_LINEAR_STOP:
		if ( atTime > tr->trTime + tr->trDuration ) {
			VectorClear( result );
			return;
		}
		VectorCopy( tr->trDelta, result );
		break;
	case TR_GRAVITY:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorCopy( tr->trDelta, result );
		result[2] -= DEFAULT_GRAVITY * deltaTime;		// FIXME: local gravity...
		break;
	// Ridah
	case TR_GRAVITY_LOW:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorCopy( tr->trDelta, result );
		result[2] -= (DEFAULT_GRAVITY * 0.3) * deltaTime;		// FIXME: local gravity...
		break;
	// done.
//----(SA)	
	case TR_GRAVITY_FLOAT:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorCopy( tr->trDelta, result );
		result[2] -= (DEFAULT_GRAVITY * 0.2) * deltaTime;
		break;
//----(SA)	end
	// RF, acceleration
	case TR_ACCELERATE:	// trDelta is eventual speed
		if ( atTime > tr->trTime + tr->trDuration ) {
			VectorClear( result );
			return;
		}
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		phase = deltaTime / (float)tr->trDuration;
		VectorScale( tr->trDelta, deltaTime * deltaTime, result );
		break;
	case TR_DECCELERATE:	// trDelta is breaking force
		if ( atTime > tr->trTime + tr->trDuration ) {
			VectorClear( result );
			return;
		}
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorScale( tr->trDelta, deltaTime, result );
		break;
	case TR_SPLINE:
	case TR_LINEAR_PATH:
		VectorClear( result );
		break;
	default:
		Com_Error( ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trTime );
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
void BG_GetMarkDir( const vec3_t dir, const vec3_t normal, vec3_t result ) {
	vec3_t	ndir, lnormal;
	float	minDot = 0.3;
	int x = 0;
	int normalFallback;
	static const float one = 1.0f;
	static const double half = 0.5;

	/* Original x87 carry branch includes unordered components. */
	if( !(dir[0] >= 0.001) && !(dir[1] >= 0.001) ) {
#if defined(_MSC_VER) && defined(_M_IX86)
		/* Preserve original integer copies even for signaling NaN bits. */
		__asm {
			mov ecx,dir
			mov edx,result
			mov eax,dword ptr [ecx]
			mov dword ptr [edx],eax
			mov eax,dword ptr [ecx+4]
			mov dword ptr [edx+4],eax
			mov eax,dword ptr [ecx+8]
			mov dword ptr [edx+8],eax
		}
#elif defined(__GNUC__) && defined(__i386__)
		/* Linux84385 stores X through x87, then copies Y/Z integer bits. */
		__asm__ volatile (
			"flds 0(%%ecx)\n\t" "fstps 0(%%edx)\n\t"
			"movl 4(%%ecx), %%eax\n\t" "movl %%eax, 4(%%edx)\n\t"
			"movl 8(%%ecx), %%eax\n\t" "movl %%eax, 8(%%edx)"
			: : "c" (dir), "d" (result) : "eax", "memory", "st"
		);
#else
		/* Portable fallback; no exact exceptional-value ABI claim. */
		VectorCopy( dir, result );
#endif
		return;
	}

#if defined(_MSC_VER) && defined(_M_IX86)
	/* Compare the callee's live ST0 return, without a C float spill. */
	__asm {
		push normal
		call VectorLengthSquared
		add esp,4
		fcomp one
		fnstsw ax
		test ah,1
		setnz al
		movzx eax,al
		mov normalFallback,eax
	}
#elif defined(__GNUC__) && defined(__i386__)
	__asm__ volatile (
		"pushl %1\n\t"
		"call *%2\n\t"
		"addl $4, %%esp\n\t"
		"fld1\n\t"
		"fcom %%st(1)\n\t"
		"fnstsw %%ax\n\t"
		"fstp %%st(0)\n\t"
		"fstp %%st(0)\n\t"
		"testb $0x41, %%ah\n\t"
		"setz %%al\n\t"
		"movzbl %%al, %%eax"
		: "=a" (normalFallback)
		: "r" (normal), "r" (VectorLengthSquared)
		: "ecx", "edx", "cc", "memory", "st", "st(1)", "st(2)", "st(3)"
	);
#elif defined(_WIN32)
	normalFallback = !(VectorLengthSquared(normal) >= 1.0f);
#else
	normalFallback = VectorLengthSquared(normal) < 1.0f;
#endif
	if( normalFallback ) {
		VectorSet( lnormal, 0.f, 0.f, 1.f );
	} else {
		//VectorCopy( normal, lnormal );
		//VectorNormalizeFast( lnormal );
		VectorNormalize2( normal, lnormal );
	}

#if defined(_MSC_VER) && defined(_M_IX86)
	__asm {
		mov ecx,dir
		lea edx,ndir
		fld dword ptr [ecx]
		fchs
		fstp dword ptr [edx]
		fld dword ptr [ecx+4]
		fchs
		fstp dword ptr [edx+4]
		fld dword ptr [ecx+8]
		fchs
		fstp dword ptr [edx+8]
	}
#elif defined(__GNUC__) && defined(__i386__)
	__asm__ volatile (
		"movl 0(%%ecx), %%eax\n\t" "xorl $0x80000000, %%eax\n\t" "movl %%eax, 0(%%edx)\n\t"
		"movl 4(%%ecx), %%eax\n\t" "xorl $0x80000000, %%eax\n\t" "movl %%eax, 4(%%edx)\n\t"
		"movl 8(%%ecx), %%eax\n\t" "xorl $0x80000000, %%eax\n\t" "movl %%eax, 8(%%edx)"
		: : "c" (dir), "d" (ndir) : "eax", "cc", "memory"
	);
#else
	VectorNegate( dir, ndir );
#endif
	VectorNormalize( ndir );
	if( normal[2] > .8f ) {
		minDot = .7f;
	}

	/* Windows2002d243/2002d2b5 adds (z*z + y*y) + x*x on x87
	 * and branches on C0, including unordered, without a float spill. */
	for( ;; ) {
#if defined(_MSC_VER) && defined(_M_IX86)
		int below;
		__asm {
			lea ecx, ndir
			lea edx, lnormal
			fld dword ptr [ecx+8]
			fmul dword ptr [edx+8]
			fld dword ptr [ecx+4]
			fmul dword ptr [edx+4]
			faddp st(1), st(0)
			fld dword ptr [ecx]
			fmul dword ptr [edx]
			faddp st(1), st(0)
			fcomp minDot
			fnstsw ax
			test ah, 1
			setnz al
			movzx eax, al
			mov below, eax
		}
		if( !below || x >= 10 ) break;
#elif defined(__GNUC__) && defined(__i386__)
		int below;
		/* Linux8441b/844aa: (y product + x product), then z + sum. */
		__asm__ volatile (
			"flds 0(%%ecx)\n\t"
			"fmuls 0(%%edx)\n\t"
			"flds 4(%%ecx)\n\t"
			"fmuls 4(%%edx)\n\t"
			"fxch %%st(1)\n\t"
			"faddp %%st, %%st(1)\n\t"
			"flds 8(%%ecx)\n\t"
			"fmuls 8(%%edx)\n\t"
			"fxch %%st(1)\n\t"
			"faddp %%st, %%st(1)\n\t"
			"fcomps %1\n\t"
			"fnstsw %%ax\n\t"
			"testb $1, %%ah\n\t"
			"setnz %%al\n\t"
			"movzbl %%al, %%eax"
			: "=a" (below)
			: "m" (minDot), "c" (ndir), "d" (lnormal)
			: "cc", "memory", "st", "st(1)"
		);
		if( !below || x >= 10 ) break;
#else
		if( !(DotProduct( ndir, lnormal ) < minDot) || x >= 10 ) break;
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
		__asm {
			lea ecx,ndir
			lea edx,lnormal
			fld dword ptr [edx]
			fmul half
			fadd dword ptr [ecx]
			fstp dword ptr [ecx]
			fld dword ptr [edx+4]
			fmul half
			fadd dword ptr [ecx+4]
			fstp dword ptr [ecx+4]
			fld dword ptr [edx+8]
			fmul half
			fadd dword ptr [ecx+8]
			fstp dword ptr [ecx+8]
		}
#elif defined(__GNUC__) && defined(__i386__)
		__asm__ volatile (
			"fldl %0\n\t" "fmuls 0(%%edx)\n\t" "fadds 0(%%ecx)\n\t" "fstps 0(%%ecx)\n\t"
			"fldl %0\n\t" "fmuls 4(%%edx)\n\t" "fadds 4(%%ecx)\n\t" "fstps 4(%%ecx)\n\t"
			"fldl %0\n\t" "fmuls 8(%%edx)\n\t" "fadds 8(%%ecx)\n\t" "fstps 8(%%ecx)"
			: : "m" (half), "c" (ndir), "d" (lnormal) : "memory", "st"
		);
#else
		VectorMA( ndir, .5, lnormal, ndir );
#endif
		VectorNormalize( ndir );

		x++;
	}

#ifdef GAMEDLL
	if( x >= 10 ) {
		if( g_developer.integer ) {
			Com_Printf( "BG_GetMarkDir loops: %i\n", x );
		}
	}
#endif // GAMEDLL

#if defined(_MSC_VER) && defined(_M_IX86)
	/* Original final X is stored through x87; Y/Z are integer copies. */
	__asm {
		lea ecx,ndir
		mov edx,result
		fld dword ptr [ecx]
		fstp dword ptr [edx]
		mov eax,dword ptr [ecx+4]
		mov dword ptr [edx+4],eax
		mov eax,dword ptr [ecx+8]
		mov dword ptr [edx+8],eax
	}
#elif defined(__GNUC__) && defined(__i386__)
	/* Original84513 writes all three carried float components via x87. */
	__asm__ volatile (
		"flds 8(%%ecx)\n\t" "flds 4(%%ecx)\n\t" "flds 0(%%ecx)\n\t"
		"fstps 0(%%edx)\n\t" "fstps 4(%%edx)\n\t" "fstps 8(%%edx)"
		: : "c" (ndir), "d" (result) : "memory", "st", "st(1)", "st(2)"
	);
#else
	VectorCopy( ndir, result );
#endif
}


char *eventnames[] = {
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

void	trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

void BG_AddPredictableEventToPlayerstate( int newEvent, int eventParm, playerState_t *ps ) {

#ifdef _DEBUG
	{
		char buf[256];
		trap_Cvar_VariableStringBuffer("showevents", buf, sizeof(buf));
		if ( atof(buf) != 0 ) {
#ifdef QAGAME
			Com_Printf(" game event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#else
			Com_Printf("Cgame event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#endif
		}
	}
#endif
	ps->events[ps->eventSequence & (MAX_EVENTS-1)] = newEvent;
	ps->eventParms[ps->eventSequence & (MAX_EVENTS-1)] = eventParm;
	ps->eventSequence++;
}

// Gordon: would like to just inline this but would likely break qvm support
#define SETUP_MOUNTEDGUN_STATUS( ps )							\
	switch( ps->persistant[PERS_HWEAPON_USE] ) {				\
		case 1:													\
			ps->eFlags |= EF_MG42_ACTIVE;						\
			ps->eFlags &= ~EF_AAGUN_ACTIVE;						\
			ps->powerups[PW_OPS_DISGUISED] = 0;					\
			break;												\
		case 2:													\
			ps->eFlags |= EF_AAGUN_ACTIVE;						\
			ps->eFlags &= ~EF_MG42_ACTIVE;						\
			ps->powerups[PW_OPS_DISGUISED] = 0;					\
			break;												\
		default:												\
			ps->eFlags &= ~EF_MG42_ACTIVE;						\
			ps->eFlags &= ~EF_AAGUN_ACTIVE;						\
			break;												\
	}

/*
========================
BG_PlayerStateToEntityState

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
/* Shared tail of TC2002d350/2002d7b0: equipment and body-damage wire fields. */
static void BG_TCEPlayerStateFields(const playerState_t *ps, entityState_t *s) {
	int vip = ps->stats[STAT_TCE_FLAGS] & 0x100;
	int alternate = ps->stats[STAT_TCE_FLAGS] & 0x400;
	int i;
	s->angles2[ROLL] = ps->leanf;
	s->effect1Time = 0;
	if (ps->pm_flags & 1) s->effect1Time = 2;
	if (ps->eFlags & 0x80000) s->effect1Time |= 4;
	if (ps->pm_flags & 4) s->effect1Time |= 0x400;
	if (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x20) s->effect1Time |= 8;
	if (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x100) s->effect1Time |= 0x10;
	if (vip) s->effect1Time |= 0x20;
	if (alternate) s->effect1Time |= 0x40;
	s->effect2Time = 0;
	if (ps->weapon != BG_WeaponOnBackToWeap(ps->holdable[10]) && !vip && !alternate)
		s->effect2Time = ps->holdable[10];
	s->effect3Time = ps->stats[15] & 0xff;
	s->modelindex2 = 0;
	for (i = 0; i < 3; ++i) {
		if (ps->holdable[i + 2] >= 67) s->modelindex2 |= 4 << (2 * i);
		else if (ps->holdable[i + 2] >= 34) s->modelindex2 |= 2 << (2 * i);
	}
}

#if defined(_MSC_VER) && defined(_M_IX86) && defined(CGAMEDLL)
/* TC300052b0: field-mapped original snapshot producer, including low32 ftol snapping. */
static const float tceSnapshotTurn830 = 256.0f;
enum {
 snapshot830P4=offsetof(playerState_t,pm_type),
 snapshot830P14=offsetof(playerState_t,origin),
 snapshot830P18=offsetof(playerState_t,origin)+1*sizeof(float),
 snapshot830P1c=offsetof(playerState_t,origin)+2*sizeof(float),
 snapshot830Pa0=offsetof(playerState_t,clientNum),
 snapshot830Pb0=offsetof(playerState_t,viewangles),
 snapshot830Pb4=offsetof(playerState_t,viewangles)+1*sizeof(float),
 snapshot830Pb8=offsetof(playerState_t,viewangles)+2*sizeof(float),
 snapshot830P64=offsetof(playerState_t,movementDir),
 snapshot830P58=offsetof(playerState_t,legsAnim),
 snapshot830P60=offsetof(playerState_t,torsoAnim),
 snapshot830P68=offsetof(playerState_t,eFlags),
 snapshot830P6c=offsetof(playerState_t,eventSequence),
 snapshot830P70=offsetof(playerState_t,events),
 snapshot830P80=offsetof(playerState_t,eventParms),
 snapshot830P90=offsetof(playerState_t,oldEventSequence),
 snapshot830P94=offsetof(playerState_t,externalEvent),
 snapshot830P98=offsetof(playerState_t,externalEventParm),
 snapshot830P470=offsetof(playerState_t,entityEventSequence),
 snapshot830Pa4=offsetof(playerState_t,weapon),
 snapshot830P50=offsetof(playerState_t,groundEntityNum),
 snapshot830P150=offsetof(playerState_t,powerups),
 snapshot830P41c=offsetof(playerState_t,nextWeapon),
 snapshot830P420=offsetof(playerState_t,teamNum),
 snapshot830P5a8=offsetof(playerState_t,aiState),
 snapshot830P3c=offsetof(playerState_t,leanf),
 snapshot830Pc=offsetof(playerState_t,pm_flags),
 snapshot830Pd0=offsetof(playerState_t,stats)+0*sizeof(int),
 snapshot830P140=offsetof(playerState_t,persistant)+PERS_HWEAPON_USE*sizeof(int),
 snapshot830P170=offsetof(playerState_t,powerups)+PW_OPS_DISGUISED*sizeof(int),
 snapshot830Pf0=offsetof(playerState_t,stats)+8*sizeof(int),
 snapshot830Pf4=offsetof(playerState_t,stats)+9*sizeof(int),
 snapshot830P10c=offsetof(playerState_t,stats)+15*sizeof(int),
 snapshot830P3b8=offsetof(playerState_t,holdable)+10*sizeof(int),
 snapshot830P398=offsetof(playerState_t,holdable)+2*sizeof(int),
 snapshot830P39c=offsetof(playerState_t,holdable)+3*sizeof(int),
 snapshot830P3a0=offsetof(playerState_t,holdable)+4*sizeof(int),
 snapshot830E0=offsetof(entityState_t,number),
 snapshot830E4=offsetof(entityState_t,eType),
 snapshot830E8=offsetof(entityState_t,eFlags),
 snapshot830Ec=offsetof(entityState_t,pos)+offsetof(trajectory_t,trType),
 snapshot830E18=offsetof(entityState_t,pos)+offsetof(trajectory_t,trBase)+0*sizeof(float),
 snapshot830E1c=offsetof(entityState_t,pos)+offsetof(trajectory_t,trBase)+1*sizeof(float),
 snapshot830E20=offsetof(entityState_t,pos)+offsetof(trajectory_t,trBase)+2*sizeof(float),
 snapshot830E30=offsetof(entityState_t,apos)+offsetof(trajectory_t,trType),
 snapshot830E3c=offsetof(entityState_t,apos)+offsetof(trajectory_t,trBase)+0*sizeof(float),
 snapshot830E40=offsetof(entityState_t,apos)+offsetof(trajectory_t,trBase)+1*sizeof(float),
 snapshot830E44=offsetof(entityState_t,apos)+offsetof(trajectory_t,trBase)+2*sizeof(float),
 snapshot830E84=offsetof(entityState_t,angles2)+sizeof(float),
 snapshot830E88=offsetof(entityState_t,angles2)+2*sizeof(float),
 snapshot830Eec=offsetof(entityState_t,legsAnim),
 snapshot830Ef0=offsetof(entityState_t,torsoAnim),
 snapshot830Eac=offsetof(entityState_t,clientNum),
 snapshot830Eb8=offsetof(entityState_t,event),
 snapshot830Ebc=offsetof(entityState_t,eventParm),
 snapshot830Ec0=offsetof(entityState_t,eventSequence),
 snapshot830Ec4=offsetof(entityState_t,events),
 snapshot830Ed4=offsetof(entityState_t,eventParms),
 snapshot830Ee8=offsetof(entityState_t,weapon),
 snapshot830Ee4=offsetof(entityState_t,powerups),
 snapshot830E94=offsetof(entityState_t,groundEntityNum),
 snapshot830E104=offsetof(entityState_t,nextWeapon),
 snapshot830E108=offsetof(entityState_t,teamNum),
 snapshot830E118=offsetof(entityState_t,aiState),
 snapshot830E10c=offsetof(entityState_t,effect1Time),
 snapshot830E110=offsetof(entityState_t,effect2Time),
 snapshot830E114=offsetof(entityState_t,effect3Time),
 snapshot830Ea8=offsetof(entityState_t,modelindex2)
};
typedef char tceSnapshotArrays830[(MAX_EVENTS==4 && MAX_POWERUPS==16)?1:-1];
__declspec(naked) void BG_PlayerStateToEntityState( playerState_t *ps, entityState_t *s, qboolean snap ) {
    __asm {
        PUSH ECX
        PUSH EBX
        PUSH EBP
        PUSH ESI
        PUSH EDI
        MOV EDI,dword ptr [ESP + 0x18]
        XOR EBX,EBX
        MOV dword ptr [ESP + 0x10],EBX
        MOV EAX,dword ptr [EDI + snapshot830P4]
        CMP EAX,0x5
        JZ snapshot830_300052e6
        CMP EAX,0x2
        JZ snapshot830_300052e6
        MOV EAX,dword ptr [EDI + snapshot830Pd0]
        MOV ESI,dword ptr [ESP + 0x1c]
        CMP EAX,0xffffff51
        JLE snapshot830_300052ea
        MOV dword ptr [ESI + snapshot830E4],0x1
        JMP snapshot830_300052f1
snapshot830_300052e6:
        MOV ESI,dword ptr [ESP + 0x1c]
snapshot830_300052ea:
        MOV dword ptr [ESI + snapshot830E4],0xa
snapshot830_300052f1:
        MOV EAX,dword ptr [EDI + snapshot830Pa0]
        MOV EBP,dword ptr [ESP + 0x20]
        MOV dword ptr [ESI + snapshot830E0],EAX
        MOV dword ptr [ESI + snapshot830Ec],0x1
        MOV ECX,dword ptr [EDI + snapshot830P14]
        CMP EBP,EBX
        MOV dword ptr [ESI + snapshot830E18],ECX
        MOV EDX,dword ptr [EDI + snapshot830P18]
        MOV dword ptr [ESI + snapshot830E1c],EDX
        MOV EAX,dword ptr [EDI + snapshot830P1c]
        MOV dword ptr [ESI + snapshot830E20],EAX
        JZ snapshot830_30005353
        FLD dword ptr [ESI + snapshot830E18]
        CALL TCE_PathInteger829
        MOV dword ptr [ESP + 0x1c],EAX
        FILD dword ptr [ESP + 0x1c]
        FSTP dword ptr [ESI + snapshot830E18]
        FLD dword ptr [ESI + snapshot830E1c]
        CALL TCE_PathInteger829
        MOV dword ptr [ESP + 0x1c],EAX
        FILD dword ptr [ESP + 0x1c]
        FSTP dword ptr [ESI + snapshot830E1c]
        FLD dword ptr [ESI + snapshot830E20]
        CALL TCE_PathInteger829
        MOV dword ptr [ESP + 0x1c],EAX
        FILD dword ptr [ESP + 0x1c]
        FSTP dword ptr [ESI + snapshot830E20]
snapshot830_30005353:
        MOV dword ptr [ESI + snapshot830E30],0x1
        MOV ECX,dword ptr [EDI + snapshot830Pb0]
        MOV dword ptr [ESI + snapshot830E3c],ECX
        MOV EDX,dword ptr [EDI + snapshot830Pb4]
        MOV dword ptr [ESI + snapshot830E40],EDX
        MOV EAX,dword ptr [EDI + snapshot830Pb8]
        CMP EBP,EBX
        MOV dword ptr [ESI + snapshot830E44],EAX
        JZ snapshot830_300053b2
        FLD dword ptr [ESI + snapshot830E3c]
        CALL TCE_PathInteger829
        MOV dword ptr [ESP + 0x1c],EAX
        FILD dword ptr [ESP + 0x1c]
        FSTP dword ptr [ESI + snapshot830E3c]
        FLD dword ptr [ESI + snapshot830E40]
        CALL TCE_PathInteger829
        MOV dword ptr [ESP + 0x1c],EAX
        FILD dword ptr [ESP + 0x1c]
        FSTP dword ptr [ESI + snapshot830E40]
        FLD dword ptr [ESI + snapshot830E44]
        CALL TCE_PathInteger829
        MOV dword ptr [ESP + 0x1c],EAX
        FILD dword ptr [ESP + 0x1c]
        FSTP dword ptr [ESI + snapshot830E44]
snapshot830_300053b2:
        MOV EAX,dword ptr [EDI + snapshot830P64]
        MOV dword ptr [ESP + 0x1c],EAX
        CMP EAX,0x80
        FILD dword ptr [ESP + 0x1c]
        JLE snapshot830_300053ca
        FSUB dword ptr [tceSnapshotTurn830]
snapshot830_300053ca:
        FSTP dword ptr [ESI + snapshot830E84]
        MOV ECX,dword ptr [EDI + snapshot830P58]
        MOV dword ptr [ESI + snapshot830Eec],ECX
        MOV EDX,dword ptr [EDI + snapshot830P60]
        MOV dword ptr [ESI + snapshot830Ef0],EDX
        MOV EAX,dword ptr [EDI + snapshot830Pa0]
        MOV dword ptr [ESI + snapshot830Eac],EAX
        MOV EAX,dword ptr [EDI + snapshot830P68]
        TEST AH,0x80
        JZ snapshot830_30005400
        AND EAX,0xffbfffdf
        MOV dword ptr [EDI + snapshot830P68],EAX
        JMP snapshot830_3000542f
snapshot830_30005400:
        MOV ECX,dword ptr [EDI + snapshot830P140]
        DEC ECX
        JZ snapshot830_3000541f
        DEC ECX
        JZ snapshot830_30005416
        AND EAX,0xffbfffdf
        MOV dword ptr [EDI + snapshot830P68],EAX
        JMP snapshot830_3000542f
snapshot830_30005416:
        AND AL,0xdf
        OR EAX,0x400000
        JMP snapshot830_30005426
snapshot830_3000541f:
        AND EAX,0xffbfffff
        OR AL,0x20
snapshot830_30005426:
        MOV dword ptr [EDI + snapshot830P68],EAX
        MOV dword ptr [EDI + snapshot830P170],EBX
snapshot830_3000542f:
        MOV EAX,dword ptr [EDI + snapshot830P68]
        MOV dword ptr [ESI + snapshot830E8],EAX
        MOV ECX,dword ptr [EDI + snapshot830Pd0]
        CMP ECX,EBX
        JG snapshot830_30005443
        OR AL,0x1
        JMP snapshot830_30005445
snapshot830_30005443:
        AND AL,0xfe
snapshot830_30005445:
        MOV dword ptr [ESI + snapshot830E8],EAX
        MOV EAX,dword ptr [EDI + snapshot830P94]
        CMP EAX,EBX
        JZ snapshot830_30005466
        MOV dword ptr [ESI + snapshot830Eb8],EAX
        MOV ECX,dword ptr [EDI + snapshot830P98]
        MOV dword ptr [ESI + snapshot830Ebc],ECX
        JMP snapshot830_300054b4
snapshot830_30005466:
        MOV EAX,dword ptr [EDI + snapshot830P470]
        MOV ECX,dword ptr [EDI + snapshot830P6c]
        CMP EAX,ECX
        JGE snapshot830_300054b4
        ADD ECX,-0x4
        CMP EAX,ECX
        JGE snapshot830_30005480
        MOV dword ptr [EDI + snapshot830P470],ECX
snapshot830_30005480:
        MOV EAX,dword ptr [EDI + snapshot830P470]
        AND EAX,0x3
        MOV EDX,EAX
        MOV ECX,dword ptr [EDI + EAX*0x4 + snapshot830P70]
        SHL EDX,0x8
        OR ECX,EDX
        MOV dword ptr [ESI + snapshot830Eb8],ECX
        MOV EDX,dword ptr [EDI + EAX*0x4 + snapshot830P80]
        MOV dword ptr [ESI + snapshot830Ebc],EDX
        MOV EAX,dword ptr [EDI + snapshot830P470]
        INC EAX
        MOV dword ptr [EDI + snapshot830P470],EAX
snapshot830_300054b4:
        MOV ECX,dword ptr [EDI + snapshot830P90]
        MOV EAX,dword ptr [EDI + snapshot830P6c]
        CMP ECX,EAX
        JZ snapshot830_30005506
snapshot830_300054c1:
        MOV EDX,dword ptr [ESI + snapshot830Ec0]
        MOV EAX,ECX
        AND EAX,0x3
        AND EDX,0x3
        MOV EBP,dword ptr [EDI + EAX*0x4 + snapshot830P70]
        MOV dword ptr [ESI + EDX*0x4 + snapshot830Ec4],EBP
        MOV EDX,dword ptr [ESI + snapshot830Ec0]
        MOV EAX,dword ptr [EDI + EAX*0x4 + snapshot830P80]
        AND EDX,0x3
        MOV dword ptr [ESI + EDX*0x4 + snapshot830Ed4],EAX
        MOV EBP,dword ptr [ESI + snapshot830Ec0]
        INC EBP
        INC ECX
        MOV dword ptr [ESI + snapshot830Ec0],EBP
        MOV EAX,dword ptr [EDI + snapshot830P6c]
        CMP ECX,EAX
        JNZ snapshot830_300054c1
snapshot830_30005506:
        MOV ECX,dword ptr [EDI + snapshot830P6c]
        MOV EDX,dword ptr [EDI + snapshot830Pa4]
        MOV dword ptr [EDI + snapshot830P90],ECX
        MOV dword ptr [ESI + snapshot830Ee8],EDX
        MOV EAX,dword ptr [EDI + snapshot830P50]
        MOV dword ptr [ESI + snapshot830Ee4],EBX
        MOV dword ptr [ESI + snapshot830E94],EAX
        XOR ECX,ECX
        LEA EAX,[EDI + snapshot830P150]
snapshot830_30005532:
        CMP dword ptr [EAX],EBX
        JZ snapshot830_3000554b
        MOV EBP,dword ptr [ESI + snapshot830Ee4]
        MOV EDX,0x1
        SHL EDX,CL
        OR EBP,EDX
        MOV dword ptr [ESI + snapshot830Ee4],EBP
snapshot830_3000554b:
        INC ECX
        ADD EAX,0x4
        CMP ECX,0x10
        JL snapshot830_30005532
        MOV EAX,dword ptr [EDI + snapshot830P41c]
        MOV dword ptr [ESI + snapshot830E104],EAX
        MOV ECX,dword ptr [EDI + snapshot830P420]
        MOV dword ptr [ESI + snapshot830E108],ECX
        MOV EDX,dword ptr [EDI + snapshot830P5a8]
        MOV dword ptr [ESI + snapshot830E118],EDX
        MOV EAX,dword ptr [EDI + snapshot830P3c]
        MOV dword ptr [ESI + snapshot830E88],EAX
        MOV dword ptr [ESI + snapshot830E10c],EBX
        TEST byte ptr [EDI + snapshot830Pc],0x1
        JZ snapshot830_30005597
        MOV dword ptr [ESI + snapshot830E10c],0x2
snapshot830_30005597:
        TEST dword ptr [EDI + snapshot830P68],0x80000
        JZ snapshot830_300055ae
        MOV EAX,dword ptr [ESI + snapshot830E10c]
        OR AL,0x4
        MOV dword ptr [ESI + snapshot830E10c],EAX
snapshot830_300055ae:
        MOV AL,byte ptr [EDI + snapshot830Pc]
        MOV ECX,0x400
        TEST AL,0x4
        JZ snapshot830_300055c0
        OR dword ptr [ESI + snapshot830E10c],ECX
snapshot830_300055c0:
        MOV AL,byte ptr [EDI + snapshot830Pf0]
        MOV EBX,0x8
        TEST AL,0x20
        JZ snapshot830_300055d5
        OR dword ptr [ESI + snapshot830E10c],EBX
snapshot830_300055d5:
        MOV EDX,dword ptr [EDI + snapshot830Pf0]
        MOV EAX,0x100
        TEST EAX,EDX
        JZ snapshot830_300055eb
        OR dword ptr [ESI + snapshot830E10c],0x10
snapshot830_300055eb:
        TEST dword ptr [EDI + snapshot830Pf4],EAX
        JZ snapshot830_30005609
        MOV EAX,dword ptr [ESI + snapshot830E10c]
        MOV dword ptr [ESP + 0x10],0x1
        OR AL,0x20
        MOV dword ptr [ESI + snapshot830E10c],EAX
snapshot830_30005609:
        MOV EAX,dword ptr [EDI + snapshot830Pf4]
        MOV EBP,0x40
        TEST ECX,EAX
        JZ snapshot830_3000562e
        MOV EAX,dword ptr [ESI + snapshot830E10c]
        MOV dword ptr [ESP + 0x10],0x1
        OR EAX,EBP
        MOV dword ptr [ESI + snapshot830E10c],EAX
snapshot830_3000562e:
        MOV dword ptr [ESI + snapshot830E110],0x0
        MOV ECX,dword ptr [EDI + snapshot830P3b8]
        PUSH ECX
        CALL BG_WeaponOnBackToWeap
        MOV ECX,dword ptr [EDI + snapshot830Pa4]
        ADD ESP,0x4
        CMP ECX,EAX
        JZ snapshot830_30005665
        MOV EAX,dword ptr [ESP + 0x10]
        TEST EAX,EAX
        JNZ snapshot830_30005665
        MOV EDX,dword ptr [EDI + snapshot830P3b8]
        MOV dword ptr [ESI + snapshot830E110],EDX
snapshot830_30005665:
        MOV EAX,dword ptr [EDI + snapshot830P10c]
        MOV dword ptr [ESI + snapshot830Ea8],0x0
        AND EAX,0xff
        MOV dword ptr [ESI + snapshot830E114],EAX
        MOV EAX,dword ptr [EDI + snapshot830P398]
        CMP EAX,0x42
        JLE snapshot830_30005697
        MOV dword ptr [ESI + snapshot830Ea8],0x4
        JMP snapshot830_300056a6
snapshot830_30005697:
        CMP EAX,0x21
        JLE snapshot830_300056a6
        MOV dword ptr [ESI + snapshot830Ea8],0x2
snapshot830_300056a6:
        MOV EAX,dword ptr [EDI + snapshot830P39c]
        CMP EAX,0x42
        JLE snapshot830_300056bb
        MOV EAX,dword ptr [ESI + snapshot830Ea8]
        OR AL,0x10
        JMP snapshot830_300056c8
snapshot830_300056bb:
        CMP EAX,0x21
        JLE snapshot830_300056ce
        MOV EAX,dword ptr [ESI + snapshot830Ea8]
        OR EAX,EBX
snapshot830_300056c8:
        MOV dword ptr [ESI + snapshot830Ea8],EAX
snapshot830_300056ce:
        MOV EDI,dword ptr [EDI + snapshot830P3a0]
        CMP EDI,0x42
        JLE snapshot830_300056ed
        MOV EAX,dword ptr [ESI + snapshot830Ea8]
        POP EDI
        OR EAX,EBP
        MOV dword ptr [ESI + snapshot830Ea8],EAX
        POP ESI
        POP EBP
        POP EBX
        POP ECX
        RET
snapshot830_300056ed:
        CMP EDI,0x21
        JLE snapshot830_30005700
        MOV EAX,dword ptr [ESI + snapshot830Ea8]
        OR AL,0x20
        MOV dword ptr [ESI + snapshot830Ea8],EAX
snapshot830_30005700:
        POP EDI
        POP ESI
        POP EBP
        POP EBX
        POP ECX
        RET
    }
}
#else
void BG_PlayerStateToEntityState( playerState_t *ps, entityState_t *s, qboolean snap ) {
	int		i;

	if(ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR) {// || ps->pm_flags & PMF_LIMBO ) { // JPW NERVE limbo
		s->eType = ET_INVISIBLE;
	} else if ( ps->stats[STAT_HEALTH] <= GIB_HEALTH ) {
		s->eType = ET_INVISIBLE;
	} else {
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_INTERPOLATE;
	VectorCopy( ps->origin, s->pos.trBase );
	if ( snap ) {
		SnapVector( s->pos.trBase );
	}

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );
	if ( snap ) {
		SnapVector( s->apos.trBase );
	}

	if (ps->movementDir > 128)
		s->angles2[YAW] = (float)ps->movementDir - 256;
	else
		s->angles2[YAW] = ps->movementDir;

	s->legsAnim		= ps->legsAnim;
	s->torsoAnim	= ps->torsoAnim;
	s->clientNum	= ps->clientNum;	// ET_PLAYER looks here instead of at number
										// so corpses can also reference the proper config
	// Ridah, let clients know if this person is using a mounted weapon
	// so they don't show any client muzzle flashes

	if( ps->eFlags & EF_MOUNTEDTANK ) {
		ps->eFlags &= ~EF_MG42_ACTIVE;
		ps->eFlags &= ~EF_AAGUN_ACTIVE;
	} else {
		SETUP_MOUNTEDGUN_STATUS( ps );
	}

	s->eFlags = ps->eFlags;

	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		s->eFlags |= EF_DEAD;
	} else {
		s->eFlags &= ~EF_DEAD;
	}

// from MP
	if ( ps->externalEvent ) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	} else if ( ps->entityEventSequence < ps->eventSequence ) {
		int		seq;

		if ( ps->entityEventSequence < ps->eventSequence - MAX_EVENTS) {
			ps->entityEventSequence = ps->eventSequence - MAX_EVENTS;
		}
		seq = ps->entityEventSequence & (MAX_EVENTS-1);
		s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		s->eventParm = ps->eventParms[ seq ];
		ps->entityEventSequence++;
	}
// end
	// Ridah, now using a circular list of events for all entities
	// add any new events that have been added to the playerState_t
	// (possibly overwriting entityState_t events)
	for (i = ps->oldEventSequence; i != ps->eventSequence; i++) {
		s->events[s->eventSequence & (MAX_EVENTS-1)] = ps->events[i & (MAX_EVENTS-1)];
		s->eventParms[s->eventSequence & (MAX_EVENTS-1)] = ps->eventParms[i & (MAX_EVENTS-1)];
		s->eventSequence++;
	}
	ps->oldEventSequence = ps->eventSequence;

	s->weapon = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( ps->powerups[ i ] ) {
			s->powerups |= 1 << i;
		}
	}

	s->nextWeapon = ps->nextWeapon;	// Ridah
//	s->loopSound = ps->loopSound;
	s->teamNum = ps->teamNum;
	s->aiState = ps->aiState;		// xkan, 1/10/2003
	BG_TCEPlayerStateFields(ps, s);
}
#endif

/*
========================
BG_PlayerStateToEntityStateExtraPolate

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityStateExtraPolate( playerState_t *ps, entityState_t *s, int time, qboolean snap ) {
	int		i;

	if(ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR) {// || ps->pm_flags & PMF_LIMBO ) { // JPW NERVE limbo
		s->eType = ET_INVISIBLE;
	} else if ( ps->stats[STAT_HEALTH] <= GIB_HEALTH ) {
		s->eType = ET_INVISIBLE;
	} else {
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_LINEAR_STOP;
	VectorCopy( ps->origin, s->pos.trBase );
	if ( snap ) {
		SnapVector( s->pos.trBase );
	}
	// set the trDelta for flag direction and linear prediction
	VectorCopy( ps->velocity, s->pos.trDelta );
	// set the time for linear prediction
	s->pos.trTime = time;
	// set maximum extra polation time
	s->pos.trDuration = 50; // 1000 / sv_fps (default = 20)

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );
	if ( snap ) {
		SnapVector( s->apos.trBase );
	}

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim = ps->legsAnim;
	s->torsoAnim = ps->torsoAnim;
	s->clientNum = ps->clientNum;		// ET_PLAYER looks here instead of at number
										// so corpses can also reference the proper config

	if( ps->eFlags & EF_MOUNTEDTANK ) {
		ps->eFlags &= ~EF_MG42_ACTIVE;
		ps->eFlags &= ~EF_AAGUN_ACTIVE;
	} else {
		SETUP_MOUNTEDGUN_STATUS( ps );
	}

	s->eFlags = ps->eFlags;
	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		s->eFlags |= EF_DEAD;
	} else {
		s->eFlags &= ~EF_DEAD;
	}

	if ( ps->externalEvent ) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	} else if ( ps->entityEventSequence < ps->eventSequence ) {
		int		seq;

		if ( ps->entityEventSequence < ps->eventSequence - MAX_EVENTS) {
			ps->entityEventSequence = ps->eventSequence - MAX_EVENTS;
		}
		seq = ps->entityEventSequence & (MAX_EVENTS-1);
		s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		s->eventParm = ps->eventParms[ seq ];
		ps->entityEventSequence++;
	}

	// Ridah, now using a circular list of events for all entities
	// add any new events that have been added to the playerState_t
	// (possibly overwriting entityState_t events)
	for (i = ps->oldEventSequence; i != ps->eventSequence; i++) {
		s->events[s->eventSequence & (MAX_EVENTS-1)] = ps->events[i & (MAX_EVENTS-1)];
		s->eventParms[s->eventSequence & (MAX_EVENTS-1)] = ps->eventParms[i & (MAX_EVENTS-1)];
		s->eventSequence++;
	}
	ps->oldEventSequence = ps->eventSequence;

	s->weapon = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( ps->powerups[ i ] ) {
			s->powerups |= 1 << i;
		}
	}

	s->nextWeapon = ps->nextWeapon;	// Ridah
	s->teamNum = ps->teamNum;
	s->aiState = ps->aiState;		// xkan, 1/10/2003
	BG_TCEPlayerStateFields(ps, s);
}

// Gordon: some weapons are duplicated for code puposes.... just want to treat them as a single 
weapon_t BG_DuplicateWeapon( weapon_t weap ) {
	switch( weap ) {
		/* TC:E wire IDs; the SDK enum labels collide with TC weapons. */
		case 56:	return 55;
		case 57:	return 25;
		case 58:	return 32;
		case 9:		return 4;
		default:					return weap;
	}
}

gitem_t* BG_ValidStatWeapon( weapon_t weap ) {
	weapon_t weap2;

	switch(weap) {
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

	if(!BG_WeaponInWolfMP(weap)) {
		return NULL;
	}

	weap2 = BG_DuplicateWeapon( weap );
	if( weap != weap2 ) {
		return NULL;
	}

	return BG_FindItemForWeapon( weap );
}

weapon_t BG_WeaponForMOD( int MOD ) {
	weapon_t i;

	for(i = 0; i < MAX_WEAPONS; i++) {
		if(weaponDef[i].mod == MOD) {
			return i;
		}
	}

	return 0;
}

const char* rankSoundNames_Allies[NUM_EXPERIENCE_LEVELS] = {
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

const char* rankSoundNames_Axis[NUM_EXPERIENCE_LEVELS] = {
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

const char* rankNames_Axis[NUM_EXPERIENCE_LEVELS] = {
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

const char* rankNames_Allies[NUM_EXPERIENCE_LEVELS] = {
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




const char* miniRankNames_Axis[NUM_EXPERIENCE_LEVELS] = {
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

const char* miniRankNames_Allies[NUM_EXPERIENCE_LEVELS] = {
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
pathCorner_t *BG_Find_PathCorner (const char *match)
{
	int i;

	for (i = 0 ; i < numPathCorners; i++) {
		if (!Q_stricmp (pathCorners[i].name, match))
			return &pathCorners[i];
	}

	return NULL;
}

/*
=============
BG_AddPathCorner
=============
*/
void BG_AddPathCorner(const char* name, vec3_t origin) {
	if(numPathCorners >= MAX_PATH_CORNERS ) {
		Com_Error( ERR_DROP, "MAX PATH CORNERS (%i) hit", MAX_PATH_CORNERS );
	}

	VectorCopy( origin, pathCorners[numPathCorners].origin );
	Q_strncpyz( pathCorners[numPathCorners].name, name, 64 );
	numPathCorners++;
}

/*
=============
BG_Find_Spline
=============
*/
splinePath_t *BG_Find_Spline (const char *match) {
	int i;

	for (i = 0 ; i < numSplinePaths; i++) {
		if (!Q_stricmp (splinePaths[i].point.name, match))
			return &splinePaths[i];
	}

	return NULL;
}

splinePath_t* BG_AddSplinePath(const char* name, const char* target, vec3_t origin) {
	splinePath_t* spline;
	if(numSplinePaths >= MAX_SPLINE_PATHS) {
		Com_Error( ERR_DROP, "MAX SPLINES (%i) hit", MAX_SPLINE_PATHS );
	}

	spline = &splinePaths[numSplinePaths];

	memset( spline, 0, sizeof( splinePath_t ) );

	VectorCopy( origin, spline->point.origin );

	Q_strncpyz( spline->point.name,	name,						64 );
	Q_strncpyz( spline->strTarget,	target ? target : "",		64 );

	spline->numControls = 0;

	numSplinePaths++;

	return spline;
}

void BG_AddSplineControl(splinePath_t* spline, const char* name) {
	if(spline->numControls >= MAX_SPLINE_CONTROLS) {
		Com_Error( ERR_DROP, "MAX SPLINE CONTROLS (%i) hit", MAX_SPLINE_CONTROLS );
	}

	Q_strncpyz( spline->controls[spline->numControls].name, name, 64 );

	spline->numControls++;
}

#if defined(_MSC_VER) && defined(_M_IX86) && defined(CGAMEDLL)
/* TC cgame 30005a10: preserve retained x87 intermediates and loop comparison. */
static const float tceSplineZero826 = 0.0f;
static const float tceSplineOne826 = 1.0f;
static const float tceSplineStep826 = 0.01f;
__declspec(naked) float BG_SplineLength(splinePath_t *pSpline) {
    __asm {
        SUB ESP,0x38
        PUSH ESI
        MOV ESI,dword ptr [ESP + 0x40]
        MOV dword ptr [ESP + 0x8],0x0
        MOV dword ptr [ESP + 0x4],0x0
spline826_30005a28:
        MOV EAX,dword ptr [ESP + 0x4]
        LEA ECX,[ESP + 0x30]
        PUSH EAX
        LEA EDX,[ESP + 0x28]
        PUSH ECX
        PUSH EDX
        PUSH ESI
        CALL BG_CalculateSpline_r
        FLD dword ptr [ESP + 0x40]
        FSUB dword ptr [ESP + 0x34]
        FLD dword ptr [ESP + 0x44]
        FSUB dword ptr [ESP + 0x38]
        ADD ESP,0x10
        FSTP dword ptr [ESP + 0x10]
        FLD dword ptr [ESP + 0x38]
        FSUB dword ptr [ESP + 0x2c]
        FSTP dword ptr [ESP + 0x14]
        FMUL dword ptr [ESP + 0x4]
        FADD dword ptr [ESP + 0x24]
        FSTP dword ptr [ESP + 0xc]
        FLD dword ptr [ESP + 0x10]
        FMUL dword ptr [ESP + 0x4]
        FADD dword ptr [ESP + 0x28]
        FSTP dword ptr [ESP + 0x10]
        FLD dword ptr [ESP + 0x14]
        FMUL dword ptr [ESP + 0x4]
        FADD dword ptr [ESP + 0x2c]
        FSTP dword ptr [ESP + 0x14]
        FLD dword ptr [ESP + 0x4]
        FCOMP dword ptr [tceSplineZero826]
        FNSTSW AX
        TEST AH,0x40
        JNZ spline826_30005ad6
        FLD dword ptr [ESP + 0xc]
        FSUB dword ptr [ESP + 0x18]
        LEA EAX,[ESP + 0x24]
        PUSH EAX
        FSTP dword ptr [ESP + 0x28]
        FLD dword ptr [ESP + 0x14]
        FSUB dword ptr [ESP + 0x20]
        FSTP dword ptr [ESP + 0x2c]
        FLD dword ptr [ESP + 0x18]
        FSUB dword ptr [ESP + 0x24]
        FSTP dword ptr [ESP + 0x30]
        CALL VectorLength
        FADD dword ptr [ESP + 0xc]
        ADD ESP,0x4
        FSTP dword ptr [ESP + 0x8]
spline826_30005ad6:
        FLD dword ptr [ESP + 0x4]
        FADD dword ptr [tceSplineStep826]
        MOV EAX,dword ptr [ESP + 0x14]
        MOV ECX,dword ptr [ESP + 0xc]
        MOV EDX,dword ptr [ESP + 0x10]
        MOV dword ptr [ESP + 0x20],EAX
        MOV dword ptr [ESP + 0x18],ECX
        MOV dword ptr [ESP + 0x1c],EDX
        FST dword ptr [ESP + 0x4]
        FCOMP dword ptr [tceSplineOne826]
        FNSTSW AX
        TEST AH,0x41
        JNZ spline826_30005a28
        FLD dword ptr [ESP + 0x8]
        POP ESI
        ADD ESP,0x38
        RET
    }
}
#else
float BG_SplineLength(splinePath_t* pSpline) {
	float i;
	float granularity = 0.01f;
	float dist = 0;
//	float tension;
	vec3_t	vec[2];
	vec3_t lastPoint;
	vec3_t result;

	for(i = 0; i <= 1.f; i += granularity ) {
/*		if(pSpline->isStart) {
			tension = 1 - sin((1 - i) * M_PI * 0.5f);
		} else if(pSpline->isEnd) {
			tension = sin(i * M_PI * 0.5f);
		} else {
			tension = i;
		}*/

		BG_CalculateSpline_r( pSpline, vec[0], vec[1], i );
		VectorSubtract( vec[1], vec[0], result );
		VectorMA(vec[0], i, result, result);

		if( i != 0 ) {
			VectorSubtract( result, lastPoint, vec[0] );
			dist += VectorLength( vec[0] );
		}

		VectorCopy( result, lastPoint );
	}

	return dist;
}
#endif

void BG_BuildSplinePaths() {
	int i, j;
	pathCorner_t* pnt;
	splinePath_t *spline, *st;

	for( i = 0; i < numSplinePaths; i++ ) {
		spline = &splinePaths[i];

		if(*spline->strTarget) {
			for( j = 0; j < spline->numControls; j++) {
				pnt = BG_Find_PathCorner( spline->controls[j].name );
		
				if( !pnt ) {
					Com_Printf( "^1Cant find control point (%s) for spline (%s)\n", spline->controls[j].name, spline->point.name );
					// Gordon: Just changing to a warning for now, easier for region compiles...
					continue;

				} else {			
					VectorCopy( pnt->origin, spline->controls[j].origin );
				}
			}

			st = BG_Find_Spline( spline->strTarget );
			if( !st ) {
				Com_Printf( "^1Cant find target point (%s) for spline (%s)\n", spline->strTarget, spline->point.name );
				// Gordon: Just changing to a warning for now, easier for region compiles...
				continue;
			}

			spline->next = st;

			spline->length = BG_SplineLength(spline);
			BG_ComputeSegments( spline );
		}
	}

	for( i = 0; i < numSplinePaths; i++ ) {
		spline = &splinePaths[i];

		if(spline->next) {
			spline->next->prev = spline;
		}
	}
}

#ifndef CGAMEDLL
splinePath_t* BG_GetSplineData( int number, qboolean* backwards ) {
	if( number < 0 ) {
		*backwards = qtrue;
		number = -number;
	} else {
		*backwards = qfalse;
	}
	number--;

	if( number < 0 || number >= numSplinePaths ) {
		return NULL;
	}

	return &splinePaths[number];
}
#endif /* CGAMEDLL uses the verified TC lookup. */

/* TC 2002e1c0: the parsed definition is the complete reserve limit. */
int BG_MaxAmmoForWeapon( weapon_t weaponNum, int *skill ) {
	(void)skill;
	return weaponDef[weaponNum].maxammo;
}

/*
================
BG_CreateRotationMatrix
================
*/
void BG_CreateRotationMatrix(const vec3_t angles, vec3_t matrix[3]) {
	AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
	VectorInverse(matrix[1]);
}

/*
================
BG_TransposeMatrix
================
*/
void BG_TransposeMatrix(const vec3_t matrix[3], vec3_t transpose[3]) {
	int i, j;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			transpose[i][j] = matrix[j][i];
		}
	}
}

/*
================
BG_RotatePoint
================
*/
void BG_RotatePoint(vec3_t point, const vec3_t matrix[3]) {
#if defined(_MSC_VER) && defined(_M_IX86)
	/* TC30005ce0 retains the input and accumulates Z, Y, X in ST0. */
	__asm {
		mov ecx, point
		mov eax, matrix
		fld dword ptr [ecx]
		fld dword ptr [ecx+4]
		fld dword ptr [ecx+8]
		fld st(0)
		fmul dword ptr [eax+8]
		fld st(2)
		fmul dword ptr [eax+4]
		faddp st(1), st(0)
		fld st(3)
		fmul dword ptr [eax]
		faddp st(1), st(0)
		fstp dword ptr [ecx]
		fld st(0)
		fmul dword ptr [eax+20]
		fld st(2)
		fmul dword ptr [eax+16]
		faddp st(1), st(0)
		fld st(3)
		fmul dword ptr [eax+12]
		faddp st(1), st(0)
		fstp dword ptr [ecx+4]
		fmul dword ptr [eax+32]
		fxch st(1)
		fmul dword ptr [eax+28]
		faddp st(1), st(0)
		fxch st(1)
		fmul dword ptr [eax+24]
		faddp st(1), st(0)
		fstp dword ptr [ecx+8]
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
#if defined(_MSC_VER) && defined(_M_IX86) && defined(CGAMEDLL)
/* TC cgame 30005d30: sequential x87 stores, positive-product FSUBR on left barrels. */
static const float tceAAGunHigh827 = 40.0f;
static const float tceAAGunSide827 = 20.0f;
static const float tceAAGunForward827 = 64.0f;
__declspec(naked) void BG_AdjustAAGunMuzzleForBarrel(vec_t *origin, vec_t *forward, vec_t *right, vec_t *up, int barrel) {
    __asm {
        MOV EAX,dword ptr [ESP + 0x14]
        CMP EAX,0x3
        JA aagun827_30005f25
        CMP EAX,0
        JE aagun827_30005d44
        CMP EAX,1
        JE aagun827_30005da6
        CMP EAX,2
        JE aagun827_30005e08
        JMP aagun827_30005e97
aagun827_30005d44:
        MOV ECX,dword ptr [ESP + 0x8]
        MOV EAX,dword ptr [ESP + 0x4]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunForward827]
        FADD dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunForward827]
        FADD dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunForward827]
        MOV ECX,dword ptr [ESP + 0xc]
        FADD dword ptr [EAX + 0x8]
        FSTP dword ptr [EAX + 0x8]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunSide827]
        FADD dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunSide827]
        FADD dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunSide827]
        FADD dword ptr [EAX + 0x8]
        JMP aagun827_30005e65
aagun827_30005da6:
        MOV ECX,dword ptr [ESP + 0x8]
        MOV EAX,dword ptr [ESP + 0x4]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunForward827]
        FADD dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunForward827]
        FADD dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunForward827]
        MOV ECX,dword ptr [ESP + 0xc]
        FADD dword ptr [EAX + 0x8]
        FSTP dword ptr [EAX + 0x8]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunSide827]
        FADD dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunSide827]
        FADD dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunSide827]
        FADD dword ptr [EAX + 0x8]
        JMP aagun827_30005ef4
aagun827_30005e08:
        MOV ECX,dword ptr [ESP + 0x8]
        MOV EAX,dword ptr [ESP + 0x4]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunForward827]
        FADD dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunForward827]
        FADD dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunForward827]
        MOV ECX,dword ptr [ESP + 0xc]
        FADD dword ptr [EAX + 0x8]
        FSTP dword ptr [EAX + 0x8]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunSide827]
        FSUBR dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunSide827]
        FSUBR dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunSide827]
        FSUBR dword ptr [EAX + 0x8]
aagun827_30005e65:
        MOV ECX,dword ptr [ESP + 0x10]
        FSTP dword ptr [EAX + 0x8]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunHigh827]
        FADD dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunHigh827]
        FADD dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunHigh827]
        FADD dword ptr [EAX + 0x8]
        FSTP dword ptr [EAX + 0x8]
        RET
aagun827_30005e97:
        MOV ECX,dword ptr [ESP + 0x8]
        MOV EAX,dword ptr [ESP + 0x4]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunForward827]
        FADD dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunForward827]
        FADD dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunForward827]
        MOV ECX,dword ptr [ESP + 0xc]
        FADD dword ptr [EAX + 0x8]
        FSTP dword ptr [EAX + 0x8]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunSide827]
        FSUBR dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunSide827]
        FSUBR dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunSide827]
        FSUBR dword ptr [EAX + 0x8]
aagun827_30005ef4:
        MOV ECX,dword ptr [ESP + 0x10]
        FSTP dword ptr [EAX + 0x8]
        FLD dword ptr [ECX]
        FMUL dword ptr [tceAAGunSide827]
        FADD dword ptr [EAX]
        FSTP dword ptr [EAX]
        FLD dword ptr [ECX + 0x4]
        FMUL dword ptr [tceAAGunSide827]
        FADD dword ptr [EAX + 0x4]
        FSTP dword ptr [EAX + 0x4]
        FLD dword ptr [ECX + 0x8]
        FMUL dword ptr [tceAAGunSide827]
        FADD dword ptr [EAX + 0x8]
        FSTP dword ptr [EAX + 0x8]
aagun827_30005f25:
        RET
    }
}
#else
void BG_AdjustAAGunMuzzleForBarrel( vec_t* origin, vec_t* forward, vec_t* right, vec_t* up, int barrel ) {
	switch( barrel ) {
		case 0:
			VectorMA( origin, 64,	forward,	origin );
			VectorMA( origin, 20,	right,		origin );
			VectorMA( origin, 40,	up,			origin );
			break;
		case 1:
			VectorMA( origin, 64,	forward,	origin );
			VectorMA( origin, 20,	right,		origin );
			VectorMA( origin, 20,	up,			origin );
			break;
		case 2:
			VectorMA( origin, 64,	forward,	origin );
			VectorMA( origin, -20,	right,		origin );
			VectorMA( origin, 40,	up,			origin );
			break;
		case 3:
			VectorMA( origin, 64,	forward,	origin );
			VectorMA( origin, -20,	right,		origin );
			VectorMA( origin, 20,	up,			origin );
			break;
	}
}
#endif

/*
=================
PC_SourceWarning
=================
*/
void PC_SourceWarning(int handle, char *format, ...) {
	int line;
	char filename[128];
	va_list argptr;
	static char string[4096];

	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format, argptr);
	va_end (argptr);

	filename[0] = '\0';
	line = 0;
	trap_PC_SourceFileAndLine(handle, filename, &line);

	Com_Printf(S_COLOR_YELLOW "WARNING: %s, line %d: %s\n", filename, line, string);
}

/*
=================
PC_SourceError
=================
*/
void PC_SourceError(int handle, char *format, ...) {
	int line;
	char filename[128];
	va_list argptr;
	static char string[4096];

	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format, argptr);
	va_end (argptr);

	filename[0] = '\0';
	line = 0;
	trap_PC_SourceFileAndLine(handle, filename, &line);

#ifdef GAMEDLL
	Com_Error( ERR_DROP, S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string);
#else
	Com_Printf(S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string);
#endif
}

/*
=================
PC_Float_Parse
=================
*/
qboolean PC_Float_Parse(int handle, float *f) {
	pc_token_t token;
	int negative = qfalse;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (token.string[0] == '-') {
		if (!trap_PC_ReadToken(handle, &token))
			return qfalse;
		negative = qtrue;
	}
	if (token.type != TT_NUMBER) {
		PC_SourceError(handle, "expected float but found %s\n", token.string);
		return qfalse;
	}
	/* TC771: Windows converts through x87; Linux copies the token bits and
	 * flips only the sign bit. Preserve their different NaN handling. */
#if defined(_MSC_VER) && defined(_M_IX86)
	__asm {
		lea eax, token
		mov edx, f
		fld dword ptr [eax + 12]
		cmp negative, 0
		je tce_pcfloat771_store
		fchs
	tce_pcfloat771_store:
		fstp dword ptr [edx]
	}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
	{
		unsigned int tceTokenBits771;
		memcpy(&tceTokenBits771, &token.floatvalue, sizeof(tceTokenBits771));
		if (negative) tceTokenBits771 ^= 0x80000000u;
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
qboolean PC_Color_Parse(int handle, vec4_t *c) {
	int i;
	float f;

	for (i = 0; i < 4; i++) {
		if (!PC_Float_Parse(handle, &f)) {
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
qboolean PC_Vec_Parse(int handle, vec3_t *c) {
	int i;
	float f;

	for (i = 0; i < 3; i++) {
		if (!PC_Float_Parse(handle, &f)) {
			return qfalse;
		}
		(*c)[i] = f;
	}
	return qtrue;
}

/*
=================
PC_Int_Parse
=================
*/
qboolean PC_Int_Parse(int handle, int *i) {
	pc_token_t token;
	int negative = qfalse;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (token.string[0] == '-') {
		if (!trap_PC_ReadToken(handle, &token))
			return qfalse;
		negative = qtrue;
	}
	if (token.type != TT_NUMBER) {
		PC_SourceError(handle, "expected integer but found %s\n", token.string);
		return qfalse;
	}
	*i = token.intvalue;
	if (negative) {
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
const char* PC_String_Parse(int handle) {
	static char buf[MAX_TOKEN_CHARS];
	pc_token_t token;
	if (!trap_PC_ReadToken(handle, &token))
		return NULL;
	
	Q_strncpyz( buf, token.string, MAX_TOKEN_CHARS );
    return buf;
}
#else
/*
=================
PC_String_Parse
=================
*/
qboolean PC_String_Parse(int handle, const char **out) {
	pc_token_t token;
	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	
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
qboolean PC_String_ParseNoAlloc(int handle, char *out, size_t size) {
	pc_token_t token;

	if( !trap_PC_ReadToken(handle, &token) )
		return qfalse;
	
	Q_strncpyz( out, token.string, size );
    return qtrue;
}

const char* bg_fireteamNames[MAX_FIRETEAMS / 2] = {
	"Alpha",
	"Bravo",
	"Charlie",
	"Delta",
	"Echo",
	"Foxtrot",
};

const voteType_t voteToggles[] =
	{
		{ "vote_allow_comp",			CV_SVF_COMP },
		{ "vote_allow_gametype",		CV_SVF_GAMETYPE },
		{ "vote_allow_kick",			CV_SVF_KICK },
		{ "vote_allow_map",				CV_SVF_MAP },
		{ "vote_allow_matchreset",		CV_SVF_MATCHRESET },
		{ "vote_allow_mutespecs",		CV_SVF_MUTESPECS },
		{ "vote_allow_nextmap",			CV_SVF_NEXTMAP },
		{ "vote_allow_pub",				CV_SVF_PUB },
		{ "vote_allow_referee",			CV_SVF_REFEREE },
		{ "vote_allow_shuffleteamsxp",	CV_SVF_SHUFFLETEAMS },
		{ "vote_allow_swapteams",		CV_SVF_SWAPTEAMS },
		{ "vote_allow_friendlyfire",	CV_SVF_FRIENDLYFIRE },
		{ "vote_allow_timelimit",		CV_SVF_TIMELIMIT },
		{ "vote_allow_warmupdamage",	CV_SVF_WARMUPDAMAGE },
		{ "vote_allow_antilag",			CV_SVF_ANTILAG },
		{ "vote_allow_balancedteams",	CV_SVF_BALANCEDTEAMS },
		{ "vote_allow_muting",			CV_SVF_MUTING }
	};

int numVotesAvailable = sizeof(voteToggles) / sizeof(voteType_t);

// consts to offset random reinforcement seeds
const unsigned int aReinfSeeds[MAX_REINFSEEDS] = { 11, 3, 13, 7, 2, 5, 1, 17 };

// Weapon full names + headshot capability
const weap_ws_t aWeaponInfo[WS_MAX] = {
	{ qfalse,	"KNIF",	"Knife"		},	// 0
	{ qtrue,	"LUGR",	"Luger"		},	// 1
	{ qtrue,	"COLT",	"Colt"		},	// 2
	{ qtrue,	"MP40",	"MP-40"		},	// 3
	{ qtrue,	"TMPS",	"Thompson"	},	// 4
	{ qtrue,	"STEN",	"Sten"		},	// 5
	{ qtrue,	"FG42",	"FG-42"		},	// 6
	{ qtrue,	"PNZR",	"Panzer"	},	// 7
	{ qtrue,	"FLAM",	"F.Thrower"	},	// 8
	{ qfalse,	"GRND",	"Grenade"	},	// 9
	{ qfalse,	"MRTR",	"Mortar"	},	// 10
	{ qfalse,	"DYNA",	"Dynamite"	},	// 11
	{ qfalse,	"ARST",	"Airstrike"	},	// 12
	{ qfalse,	"ARTY",	"Artillery"	},	// 13
	{ qfalse,	"SRNG",	"Syringe"	},	// 14
	{ qfalse,	"SMOK", "SmokeScrn"	},	// 15
	{ qfalse,	"STCH",	"Satchel"	},	// 16
	{ qfalse,	"GRLN", "G.Launchr"	},	// 17
	{ qfalse,	"LNMN", "Landmine"	},	// 18
	{ qtrue,	"MG42",	"MG-42 Gun"	},	// 19
	{ qtrue,	"GARN",	"Garand"	},	// 20
	{ qtrue,	"K-43",	"K43 Rifle"	}	// 21
};

// Multiview: Convert weaponstate to simpler format
int BG_simpleWeaponState(int ws)
{
	switch(ws)
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
	switch(hint) {
		case HINT_DISARM:
			if(val > 0)	return(0);
		case HINT_BUILD:
			if(val > 0) return(1);
		case HINT_BREAKABLE:
			if(val == 0) return(1);
		case HINT_DOOR_ROTATING:
		case HINT_BUTTON:
		case HINT_MG42:
			if(val == 0) return(2);
		case HINT_BREAKABLE_DYNAMITE:
			if(val == 0) return(3);
	}

	return(0);
}


// Multiview: Expand the hints.  Because we map a couple hints
// into a single value, we can't replicate the proper hint back
// in all cases.
int BG_simpleHintsExpand(int hint, int val)
{
	switch(hint) {
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

	while(*str) {
		if(Q_IsColorString(str)) str += 2;
		else {
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
	int str_len = 0;	// current printable string size
	int out_len = 0;	// current true string size
	const int in_len = strlen(in);

	out_max--;
	while(*in && out_len < out_max && str_len < str_max) {
		if(*in == '^') {
			if(out_len + 2 >= in_len && out_len + 2 >= out_max)
				break;

			*out++ = *in++;
			*out++ = *in++;
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
	int cPrintable = 0;
	const char *ref = in;

	while(*ref && cPrintable < index) {
		if(Q_IsColorString(ref)) ref += 2;
		else {
			ref++;
			cPrintable++;
		}
	}

	return(ref - in);
}

// strip colors and control codes, copying up to dwMaxLength-1 "good" chars and nul-terminating
// returns the length of the cleaned string
int BG_cleanName( const char *pszIn, char *pszOut, unsigned int dwMaxLength, qboolean fCRLF )
{
	const char *pInCopy = pszIn;
	const char *pszOutStart = pszOut;

	while( *pInCopy && ( pszOut - pszOutStart < dwMaxLength - 1 ) ) {
		if( *pInCopy == '^' )
			pInCopy += ((pInCopy[1] == 0) ? 1 : 2);
		else if( (*pInCopy < 32 && (!fCRLF || *pInCopy != '\n')) || (*pInCopy > 126))
			pInCopy++;
		else
			*pszOut++ = *pInCopy++;
	}

	*pszOut = 0;
	return( pszOut - pszOutStart );
}

// Only used locally
typedef struct {
	char *colorname;
	vec4_t *color;
} colorTable_t;

// Colors for crosshairs
colorTable_t OSP_Colortable[] =
						{
							{ "white",		&colorWhite },
							{ "red",		&colorRed },
							{ "green",		&colorGreen },
							{ "blue",		&colorBlue },
							{ "yellow",		&colorYellow },
							{ "magenta",	&colorMagenta },
							{ "cyan",		&colorCyan },
							{ "orange",		&colorOrange },
							{ "mdred",		&colorMdRed },
							{ "mdgreen",	&colorMdGreen },
							{ "dkgreen",	&colorDkGreen },
							{ "mdcyan",		&colorMdCyan },
							{ "mdyellow",	&colorMdYellow },
							{ "mdorange",	&colorMdOrange },
							{ "mdblue",		&colorMdBlue },
							{ "ltgrey",		&colorLtGrey },
							{ "mdgrey",		&colorMdGrey },
							{ "dkgrey",		&colorDkGrey },
							{ "black",		&colorBlack },
							{ NULL,			NULL }
						};

extern void trap_Cvar_Set( const char *var_name, const char *value );
#if defined(_MSC_VER) && defined(_M_IX86) && defined(CGAMEDLL)
static const float tceCrossOne825=1.0f, tceCrossZero825=0.0f;
static const double tceCrossScale825=0.00392156862745098;
static const char tceCrossWhite825[]="White";
typedef char tceCrossTableLayout825[(sizeof(colorTable_t)==8 && offsetof(colorTable_t,color)==4)?1:-1];
/* TC30006300 whole Windows controller, including unordered alpha and FILD/FMUL. */
__declspec(naked) void BG_setCrosshair(char *colString, float *col, float alpha, char *cvarName) {
 __asm {
 FLD dword ptr [ESP + 0xc]
 FCOMP dword ptr tceCrossOne825
 PUSH EBX
 PUSH EBP
 MOV EBP,dword ptr [ESP + 0x10]
 MOV EAX,0x3f800000
 PUSH ESI
 PUSH EDI
 MOV dword ptr [EBP],EAX
 MOV dword ptr [EBP + 0x4],EAX
 MOV dword ptr [EBP + 0x8],EAX
 FNSTSW AX
 TEST AH,0x41
 JNZ tceCross825_3000632f
 FLD dword ptr tceCrossOne825
 JMP tceCross825_3000634c
tceCross825_3000632f:
 FLD dword ptr [ESP + 0x1c]
 FCOMP dword ptr tceCrossZero825
 FNSTSW AX
 TEST AH,0x1
 JZ tceCross825_30006348
 FLD dword ptr tceCrossZero825
 JMP tceCross825_3000634c
tceCross825_30006348:
 FLD dword ptr [ESP + 0x1c]
tceCross825_3000634c:
 MOV EBX,dword ptr [ESP + 0x14]
 FSTP dword ptr [EBP + 0xc]
 CMP byte ptr [EBX],0x30
 JNZ tceCross825_30006576
 MOV AL,byte ptr [EBX + 0x1]
 CMP AL,0x78
 JZ tceCross825_3000636b
 CMP AL,0x58
 JNZ tceCross825_30006576
tceCross825_3000636b:
 MOV CL,byte ptr [EBX + 0x2]
 TEST CL,CL
 JZ tceCross825_300065a5
 CMP CL,0x30
 JL tceCross825_30006380
 CMP CL,0x39
 JLE tceCross825_3000639c
tceCross825_30006380:
 CMP CL,0x41
 JL tceCross825_3000638a
 CMP CL,0x46
 JLE tceCross825_3000639c
tceCross825_3000638a:
 CMP CL,0x61
 JL tceCross825_300065a5
 CMP CL,0x66
 JG tceCross825_300065a5
tceCross825_3000639c:
 MOV DL,byte ptr [EBX + 0x3]
 TEST DL,DL
 JZ tceCross825_300065a5
 CMP DL,0x30
 JL tceCross825_300063b1
 CMP DL,0x39
 JLE tceCross825_300063cd
tceCross825_300063b1:
 CMP DL,0x41
 JL tceCross825_300063bb
 CMP DL,0x46
 JLE tceCross825_300063cd
tceCross825_300063bb:
 CMP DL,0x61
 JL tceCross825_300065a5
 CMP DL,0x66
 JG tceCross825_300065a5
tceCross825_300063cd:
 MOV AL,byte ptr [EBX + 0x4]
 TEST AL,AL
 JZ tceCross825_300065a5
 CMP AL,0x30
 JL tceCross825_300063e0
 CMP AL,0x39
 JLE tceCross825_300063f8
tceCross825_300063e0:
 CMP AL,0x41
 JL tceCross825_300063e8
 CMP AL,0x46
 JLE tceCross825_300063f8
tceCross825_300063e8:
 CMP AL,0x61
 JL tceCross825_300065a5
 CMP AL,0x66
 JG tceCross825_300065a5
tceCross825_300063f8:
 MOV AL,byte ptr [EBX + 0x5]
 TEST AL,AL
 JZ tceCross825_300065a5
 CMP AL,0x30
 JL tceCross825_3000640b
 CMP AL,0x39
 JLE tceCross825_30006423
tceCross825_3000640b:
 CMP AL,0x41
 JL tceCross825_30006413
 CMP AL,0x46
 JLE tceCross825_30006423
tceCross825_30006413:
 CMP AL,0x61
 JL tceCross825_300065a5
 CMP AL,0x66
 JG tceCross825_300065a5
tceCross825_30006423:
 MOV AL,byte ptr [EBX + 0x6]
 TEST AL,AL
 JZ tceCross825_300065a5
 CMP AL,0x30
 JL tceCross825_30006436
 CMP AL,0x39
 JLE tceCross825_3000644e
tceCross825_30006436:
 CMP AL,0x41
 JL tceCross825_3000643e
 CMP AL,0x46
 JLE tceCross825_3000644e
tceCross825_3000643e:
 CMP AL,0x61
 JL tceCross825_300065a5
 CMP AL,0x66
 JG tceCross825_300065a5
tceCross825_3000644e:
 MOV AL,byte ptr [EBX + 0x7]
 TEST AL,AL
 JZ tceCross825_300065a5
 CMP AL,0x30
 JL tceCross825_30006461
 CMP AL,0x39
 JLE tceCross825_30006479
tceCross825_30006461:
 CMP AL,0x41
 JL tceCross825_30006469
 CMP AL,0x46
 JLE tceCross825_30006479
tceCross825_30006469:
 CMP AL,0x61
 JL tceCross825_300065a5
 CMP AL,0x66
 JG tceCross825_300065a5
tceCross825_30006479:
 CMP CL,0x39
 JLE tceCross825_30006490
 CMP CL,0x61
 MOVSX EAX,CL
 JL tceCross825_3000648b
 SUB EAX,0x57
 JMP tceCross825_30006496
tceCross825_3000648b:
 SUB EAX,0x37
 JMP tceCross825_30006496
tceCross825_30006490:
 MOVSX EAX,CL
 SUB EAX,0x30
tceCross825_30006496:
 CMP DL,0x39
 JLE tceCross825_300064ad
 CMP DL,0x61
 MOVSX ECX,DL
 JL tceCross825_300064a8
 SUB ECX,0x57
 JMP tceCross825_300064b3
tceCross825_300064a8:
 SUB ECX,0x37
 JMP tceCross825_300064b3
tceCross825_300064ad:
 MOVSX ECX,DL
 SUB ECX,0x30
tceCross825_300064b3:
 SHL EAX,0x4
 ADD EAX,ECX
 MOV dword ptr [ESP + 0x1c],EAX
 FILD dword ptr [ESP + 0x1c]
 FMUL qword ptr tceCrossScale825
 FSTP dword ptr [EBP]
 MOV AL,byte ptr [EBX + 0x4]
 CMP AL,0x39
 JLE tceCross825_300064e1
 CMP AL,0x61
 MOVSX EAX,AL
 JL tceCross825_300064dc
 SUB EAX,0x57
 JMP tceCross825_300064e7
tceCross825_300064dc:
 SUB EAX,0x37
 JMP tceCross825_300064e7
tceCross825_300064e1:
 MOVSX EAX,AL
 SUB EAX,0x30
tceCross825_300064e7:
 MOV CL,byte ptr [EBX + 0x5]
 CMP CL,0x39
 JLE tceCross825_30006501
 CMP CL,0x61
 MOVSX ECX,CL
 JL tceCross825_300064fc
 SUB ECX,0x57
 JMP tceCross825_30006507
tceCross825_300064fc:
 SUB ECX,0x37
 JMP tceCross825_30006507
tceCross825_30006501:
 MOVSX ECX,CL
 SUB ECX,0x30
tceCross825_30006507:
 SHL EAX,0x4
 ADD EAX,ECX
 MOV dword ptr [ESP + 0x1c],EAX
 FILD dword ptr [ESP + 0x1c]
 FMUL qword ptr tceCrossScale825
 FSTP dword ptr [EBP + 0x4]
 MOV AL,byte ptr [EBX + 0x6]
 CMP AL,0x39
 JLE tceCross825_30006535
 CMP AL,0x61
 MOVSX EAX,AL
 JL tceCross825_30006530
 SUB EAX,0x57
 JMP tceCross825_3000653b
tceCross825_30006530:
 SUB EAX,0x37
 JMP tceCross825_3000653b
tceCross825_30006535:
 MOVSX EAX,AL
 SUB EAX,0x30
tceCross825_3000653b:
 MOV CL,byte ptr [EBX + 0x7]
 CMP CL,0x39
 JLE tceCross825_30006555
 CMP CL,0x61
 MOVSX ECX,CL
 JL tceCross825_30006550
 SUB ECX,0x57
 JMP tceCross825_3000655b
tceCross825_30006550:
 SUB ECX,0x37
 JMP tceCross825_3000655b
tceCross825_30006555:
 MOVSX ECX,CL
 SUB ECX,0x30
tceCross825_3000655b:
 SHL EAX,0x4
 ADD EAX,ECX
 POP EDI
 MOV dword ptr [ESP + 0x18],EAX
 POP ESI
 FILD dword ptr [ESP + 0x14]
 FMUL qword ptr tceCrossScale825
 FSTP dword ptr [EBP + 0x8]
 POP EBP
 POP EBX
 RET
tceCross825_30006576:
 mov eax,dword ptr OSP_Colortable
 XOR EDI,EDI
 TEST EAX,EAX
 JZ tceCross825_300065a5
 mov eax,OFFSET OSP_Colortable
 MOV ESI,EAX
tceCross825_30006588:
 MOV EAX,dword ptr [EAX]
 PUSH EAX
 PUSH EBX
 call Q_stricmp
 ADD ESP,0x8
 TEST EAX,EAX
 JZ tceCross825_300065bc
 MOV ECX,dword ptr [ESI + 0x8]
 ADD ESI,0x8
 INC EDI
 MOV EAX,ESI
 TEST ECX,ECX
 JNZ tceCross825_30006588
tceCross825_300065a5:
 MOV ECX,dword ptr [ESP + 0x20]
 push OFFSET tceCrossWhite825
 PUSH ECX
 call trap_Cvar_Set
 ADD ESP,0x8
 POP EDI
 POP ESI
 POP EBP
 POP EBX
 RET
tceCross825_300065bc:
 MOV ECX,dword ptr OSP_Colortable[edi*8+4]
 MOV EDX,dword ptr [ECX]
 MOV dword ptr [EBP],EDX
 MOV EAX,dword ptr OSP_Colortable[edi*8+4]
 MOV ECX,dword ptr [EAX + 0x4]
 MOV dword ptr [EBP + 0x4],ECX
 MOV EDX,dword ptr OSP_Colortable[edi*8+4]
 POP EDI
 POP ESI
 MOV EAX,dword ptr [EDX + 0x8]
 MOV dword ptr [EBP + 0x8],EAX
 POP EBP
 POP EBX
 RET
 }
}
#else
void BG_setCrosshair(char *colString, float *col, float alpha, char *cvarName)
{
	char *s = colString;
	/* Original UI40001580 uses this double reciprocal before a float store. */
	const double byteToUnit = 0.00392156862745098;

	col[0] = 1.0f;
	col[1] = 1.0f;
	col[2] = 1.0f;
#if defined(UIDLL)
#if defined(_WIN32)
	/* Windows x87 unordered compares reach the zero-alpha arm. */
	col[3] = alpha > 1.0f ? 1.0f : !(alpha >= 0.0f) ? 0.0f : alpha;
#else
	/* Linux's reversed first comparison sends unordered alpha to one. */
	col[3] = !(alpha <= 1.0f) ? 1.0f : alpha < 0.0f ? 0.0f : alpha;
#endif
#else
	col[3] = (alpha > 1.0f) ? 1.0f : (alpha < 0.0f) ? 0.0f : alpha;
#endif

	if(*s == '0' && (*(s+1) == 'x' || *(s+1) == 'X')) {
		s +=2;
		//parse rrggbb
		if(Q_IsHexColorString(s)) {
			col[0] = (float)((gethex(*(s)) * 16 + gethex(*(s+1))) * byteToUnit);
			col[1] = (float)((gethex(*(s+2)) * 16 + gethex(*(s+3))) * byteToUnit);
			col[2] = (float)((gethex(*(s+4)) * 16 + gethex(*(s+5))) * byteToUnit);
			return;
		}
	} else {
		int i = 0;
		while(OSP_Colortable[i].colorname != NULL) {
			if(Q_stricmp(s, OSP_Colortable[i].colorname) == 0) {
				col[0] = (*OSP_Colortable[i].color)[0];
				col[1] = (*OSP_Colortable[i].color)[1];
				col[2] = (*OSP_Colortable[i].color)[2];
				return;
			}
			i++;
		}
	}

	trap_Cvar_Set(cvarName, "White");
}

#endif

qboolean BG_isLightWeaponSupportingFastReload( int weapon ) {
	if( weapon == WP_LUGER ||
		weapon == WP_COLT ||
		weapon == WP_MP40 ||
		weapon == WP_THOMPSON ||
		weapon == WP_STEN ||
		weapon == WP_SILENCER ||
		weapon == WP_FG42 ||
		weapon == 52 ) /* TC2002e8a0; SDK47 is TC's PSG1. */
		return qtrue;
	return qfalse;
}

qboolean BG_IsScopedWeapon( int weapon ) {
	/* TC legacy protocol scope slots. SDK42/43 overlap UMP45/AK74U.
	 * Windows30006630/2002e8e0; Linux UI00052264 agrees. */
	return (unsigned int)weapon - 57u < 3u ? qtrue : qfalse;
}

///////////////////////////////////////////////////////////////////////////////
typedef struct locInfo_s {
	vec2_t gridStartCoord;
	vec2_t gridStep;
} locInfo_t;

static locInfo_t locInfo;

void BG_InitLocations( vec2_t world_mins, vec2_t world_maxs )
{
#if defined(_MSC_VER) && defined(_M_IX86)
	/* TC2002e9d0: first probe uses rounded reciprocal, subsequent probes
	 * divide; ftol truncates to 64 bits and consumes the signed low32. */
	static const unsigned int reciprocalBits = 0x3a5a740e;
	static const float density = 7.f, decrement = 50.f, half = .5f;
	unsigned short savedCW, truncateCW;
	__int64 converted;
	__asm {
		mov ecx,world_mins
		mov edx,world_maxs
		lea esi,locInfo
		mov dword ptr [esi+8],044960000h
		mov dword ptr [esi+12],044960000h
		fld dword ptr [edx]
		fsub dword ptr [ecx]
		fmul dword ptr reciprocalBits
		fcomp density
		fnstsw ax
		test ah,1
		jz grid_y_probe
	grid_x_reduce:
		fld dword ptr [esi+8]
		fsub decrement
		fstp dword ptr [esi+8]
		fld dword ptr [edx]
		fsub dword ptr [ecx]
		fdiv dword ptr [esi+8]
		fcomp density
		fnstsw ax
		test ah,1
		jnz grid_x_reduce
	grid_y_probe:
		fld dword ptr [ecx+4]
		fsub dword ptr [edx+4]
		fmul dword ptr reciprocalBits
		fcomp density
		fnstsw ax
		test ah,1
		jz grid_starts
	grid_y_reduce:
		fld dword ptr [esi+12]
		fsub decrement
		fstp dword ptr [esi+12]
		fld dword ptr [ecx+4]
		fsub dword ptr [edx+4]
		fdiv dword ptr [esi+12]
		fcomp density
		fnstsw ax
		test ah,1
		jnz grid_y_reduce
	grid_starts:
		fstcw savedCW
		mov ax,savedCW
		or ah,0ch
		mov truncateCW,ax
		fld dword ptr [edx]
		fsub dword ptr [ecx]
		fdiv dword ptr [esi+8]
		fld st(0)
		fldcw truncateCW
		fistp qword ptr converted
		fldcw savedCW
		fild dword ptr converted
		fsubr st(0),st(1)
		fmul dword ptr [esi+8]
		fmul half
		fadd dword ptr [ecx]
		fstp dword ptr [esi]
		fstp st(0)
		fld dword ptr [ecx+4]
		fsub dword ptr [edx+4]
		fdiv dword ptr [esi+12]
		fld st(0)
		fldcw truncateCW
		fistp qword ptr converted
		fldcw savedCW
		fild dword ptr converted
		fsubr st(0),st(1)
		fmul dword ptr [esi+12]
		fmul half
		fsubr dword ptr [ecx+4]
		fstp dword ptr [esi+4]
		fstp st(0)
	}
#elif defined(__GNUC__) && defined(__i386__)
	/* Linux0008656c: retain the original carried x87 stack across both
	 * density loops and the two FIST32 origin calculations. */
	static const float constants[5] = { 1.f, 1200.f, 7.f, 50.f, .5f };
	const float *mins = world_mins;
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"sub esp, 12\n\t"
		"fld dword ptr [edi]\n\t"
		"fld st(0)\n\t"
		"mov eax, dword ptr [edi+4]\n\t"
		"mov dword ptr [esp], eax\n\t"
		"fld dword ptr [esp]\n\t"
		"mov dword ptr [ebx+8], eax\n\t"
		"fdiv st(1), st(0)\n\t"
		"fstp dword ptr [ebx+12]\n\t"
		"fld dword ptr [ecx]\n\t"
		"fld dword ptr [edx]\n\t"
		"fld st(1)\n\t"
		"fsub st(0), st(1)\n\t"
		"fld dword ptr [edi+8]\n\t"
		"fxch st(1)\n\t"
		"fmulp st(4), st(0)\n\t"
		"fcom st(3)\n\t"
		"fnstsw ax\n\t"
		"fstp st(3)\n\t"
		"sahf\n\t"
		"jbe 3f\n\t"
		"fstp st(0)\n\t"
		"fstp st(0)\n\t"
		"fld dword ptr [edi+12]\n\t"
		"1:\n\t"
		"fld dword ptr [ebx+8]\n\t"
		"fsub st(0), st(1)\n\t"
		"fst dword ptr [ebx+8]\n\t"
		"fdivr st(0), st(3)\n\t"
		"fld dword ptr [ecx]\n\t"
		"fld dword ptr [edx]\n\t"
		"fld st(1)\n\t"
		"fsub st(0), st(1)\n\t"
		"fmulp st(3), st(0)\n\t"
		"fxch st(2)\n\t"
		"fcomp st(4)\n\t"
		"fnstsw ax\n\t"
		"sahf\n\t"
		"jnc 2f\n\t"
		"fstp st(0)\n\t"
		"fstp st(0)\n\t"
		"jmp 1b\n\t"
		"2: fstp st(2)\n\t"
		"3: fstp st(2)\n\t"
		"fld dword ptr [ebx+12]\n\t"
		"fld st(3)\n\t"
		"fld dword ptr [ecx+4]\n\t"
		"fxch st(1)\n\t"
		"fdiv st(0), st(2)\n\t"
		"fld st(2)\n\t"
		"fxch st(2)\n\t"
		"fsubr dword ptr [edx+4]\n\t"
		"fmulp st(1), st(0)\n\t"
		"fcomp dword ptr [edi+8]\n\t"
		"fnstsw ax\n\t"
		"sahf\n\t"
		"jnc 8f\n\t"
		"fstp st(1)\n\t"
		"fstp st(1)\n\t"
		"fstp st(1)\n\t"
		"fld dword ptr [edi+12]\n\t"
		"4:\n\t"
		"fsub st(1), st(0)\n\t"
		"fld st(2)\n\t"
		"fdiv st(0), st(2)\n\t"
		"fld st(2)\n\t"
		"fst dword ptr [ebx+12]\n\t"
		"fld dword ptr [ecx+4]\n\t"
		"fsubr dword ptr [edx+4]\n\t"
		"fmulp st(2), st(0)\n\t"
		"fxch st(1)\n\t"
		"fcomp dword ptr [edi+8]\n\t"
		"fnstsw ax\n\t"
		"sahf\n\t"
		"jnc 5f\n\t"
		"fstp st(0)\n\t"
		"jmp 4b\n\t"
		"5: fstp st(1)\n\t"
		"fstp st(1)\n\t"
		"fld dword ptr [ecx]\n\t"
		"fld dword ptr [edx]\n\t"
		"6: fnstcw word ptr [esp+10]\n\t"
		"fld st(3)\n\t"
		"fld dword ptr [ebx+8]\n\t"
		"fxch st(5)\n\t"
		"fdiv st(0), st(4)\n\t"
		"fxch st(1)\n\t"
		"movzx eax, word ptr [esp+10]\n\t"
		"or ax, 0xc00\n\t"
		"mov word ptr [esp+8], ax\n\t"
		"fdiv st(0), st(5)\n\t"
		"fxch st(3)\n\t"
		"fsub st(0), st(2)\n\t"
		"fld dword ptr [edi+16]\n\t"
		"fxch st(1)\n\t"
		"fmulp st(4), st(0)\n\t"
		"fxch st(3)\n\t"
		"fldcw word ptr [esp+8]\n\t"
		"fist dword ptr [esp+4]\n\t"
		"fldcw word ptr [esp+10]\n\t"
		"fild dword ptr [esp+4]\n\t"
		"fsubp st(1), st(0)\n\t"
		"fmulp st(5), st(0)\n\t"
		"fxch st(4)\n\t"
		"fmul st(0), st(2)\n\t"
		"faddp st(1), st(0)\n\t"
		"fstp dword ptr [ebx]\n\t"
		"fld dword ptr [edx+4]\n\t"
		"fld st(0)\n\t"
		"fsub dword ptr [ecx+4]\n\t"
		"fmulp st(4), st(0)\n\t"
		"fxch st(3)\n\t"
		"fldcw word ptr [esp+8]\n\t"
		"fist dword ptr [esp+4]\n\t"
		"fldcw word ptr [esp+10]\n\t"
		"fild dword ptr [esp+4]\n\t"
		"fsubp st(1), st(0)\n\t"
		"fmulp st(2), st(0)\n\t"
		"fmulp st(1), st(0)\n\t"
		"fsubp st(1), st(0)\n\t"
		"fstp dword ptr [ebx+4]\n\t"
		"add esp, 12\n\t"
		"jmp 9f\n\t"
		"8: fstp st(0)\n\t"
		"fxch st(2)\n\t"
		"jmp 6b\n\t"
		"9:\n\t"
		".att_syntax prefix"
		: : "c" (world_maxs), "d" (mins), "b" (&locInfo), "D" (constants)
		: "eax", "cc", "memory", "st", "st(1)", "st(2)", "st(3)",
		  "st(4)", "st(5)", "st(6)", "st(7)"
	);

#else

	// keep this in sync with CG_DrawGrid
	locInfo.gridStep[0] = 1200.f;
	locInfo.gridStep[1] = 1200.f;

	// ensure minimal grid density
	while( ( world_maxs[0] - world_mins[0] ) / locInfo.gridStep[0] < 7 )
		locInfo.gridStep[0] -= 50.f;
	while( ( world_mins[1] - world_maxs[1] ) / locInfo.gridStep[1] < 7 )
		locInfo.gridStep[1] -= 50.f;

	locInfo.gridStartCoord[0] = world_mins[0] + .5f * ( ( ( ( world_maxs[0] - world_mins[0] ) / locInfo.gridStep[0] ) - ( (int)(( world_maxs[0] - world_mins[0] ) / locInfo.gridStep[0] ) ) ) * locInfo.gridStep[0] );
	locInfo.gridStartCoord[1] = world_mins[1] - .5f * ( ( ( ( world_mins[1] - world_maxs[1] ) / locInfo.gridStep[1] ) - ( (int)(( world_mins[1] - world_maxs[1] ) / locInfo.gridStep[1] ) ) ) * locInfo.gridStep[1] );
#endif
}

char *BG_GetLocationString( vec_t* pos )
{
    static char coord[6];
	int x, y;

	coord[0] = '\0';

#if defined(_MSC_VER) && defined(_M_IX86)
	/* TC2002eae0: extended division, then legacy _ftol's 64-bit store. */
	{
		unsigned short savedCW, truncateCW;
		__int64 converted;
		__asm {
			mov ecx, pos
			lea edx, locInfo
			fld dword ptr [ecx]
			fsub dword ptr [edx]
			fdiv dword ptr [edx+8]
			fstcw savedCW
			fwait
			mov ax, savedCW
			or ax, 0c00h
			mov truncateCW, ax
			fldcw truncateCW
			fistp qword ptr converted
			fldcw savedCW
			mov eax, dword ptr converted
			mov x, eax
			fld dword ptr [edx+4]
			fsub dword ptr [ecx+4]
			fdiv dword ptr [edx+12]
			fstcw savedCW
			fwait
			fldcw truncateCW
			fistp qword ptr converted
			fldcw savedCW
			mov eax, dword ptr converted
			mov y, eax
		}
	}
#elif defined(__GNUC__) && defined(__i386__)
	/* Linux00086700 keeps both reciprocals on x87; FISTP is 32-bit. */
	{
		unsigned short savedCW, truncateCW;
		__asm__ volatile (
			"fld1\n\t"
			"flds 0(%%edx)\n\t"
			"fld %%st(1)\n\t"
			"fdivs 8(%%edx)\n\t"
			"fxch %%st(2)\n\t"
			"fdivs 12(%%edx)\n\t"
			"fxch %%st(1)\n\t"
			"fsubrs 0(%%ecx)\n\t"
			"fnstcw %2\n\t"
			"movzwl %2, %%eax\n\t"
			"fmulp %%st, %%st(2)\n\t"
			"fxch %%st(1)\n\t"
			"orw $0xc00, %%ax\n\t"
			"movw %%ax, %3\n\t"
			"fldcw %3\n\t"
			"fistpl %0\n\t"
			"fldcw %2\n\t"
			"flds 4(%%ecx)\n\t"
			"fsubrs 4(%%edx)\n\t"
			"fmulp %%st, %%st(1)\n\t"
			"fldcw %3\n\t"
			"fistpl %1\n\t"
			"fldcw %2"
			: "=m" (x), "=m" (y), "=m" (savedCW), "=m" (truncateCW)
			: "c" (pos), "d" (&locInfo)
			: "eax", "memory", "st", "st(1)", "st(2)"
		);
	}
#else
	/* Portable fallback: original modules specify the two x86 paths above. */
	x = (pos[0] - locInfo.gridStartCoord[0]) / locInfo.gridStep[0];
	y = (locInfo.gridStartCoord[1] - pos[1]) / locInfo.gridStep[1];
#endif

	if( x < 0 ) x = 0;
	if( y < 0 ) y = 0;

	Com_sprintf( coord, sizeof(coord), "%c,%i", 'A' + x, y );

	return coord;
}

qboolean BG_BBoxCollision( vec3_t min1, vec3_t max1, vec3_t min2, vec3_t max2 ) {
	int i;

	for( i = 0; i < 3; i++ ) {
		if( min1[i] > max2[i] ) {
			return qfalse;
		}
		if( min2[i] > max1[i] ) {
			return qfalse;
		}
	}

	return qtrue;
}

weapon_t bg_heavyWeapons[NUM_HEAVY_WEAPONS] = {
	WP_FLAMETHROWER,
	WP_MOBILE_MG42,
	WP_MOBILE_MG42_SET,
	WP_PANZERFAUST,
	WP_MORTAR,
	WP_MORTAR_SET
};

/////////////////////////

int BG_FootstepForSurface( int surfaceFlags ) {
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
int Q_vsnprintf( char *dest, int size, const char *fmt, va_list argptr ) {
	int ret;

#ifdef _WIN32
#undef _vsnprintf
	ret = _vsnprintf( dest, size-1, fmt, argptr );
#define _vsnprintf	use_idStr_vsnPrintf
#else
#undef vsnprintf
	ret = vsnprintf( dest, size, fmt, argptr );
#define vsnprintf	use_idStr_vsnPrintf
#endif
	dest[size-1] = '\0';
	if ( ret < 0 || ret >= size ) {
		return -1;
	}
	return ret;
}
