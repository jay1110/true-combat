#include "q_shared.h"
#include "bg_public.h"

//	--> WP_* to WS_* conversion
//
// WS_MAX = no equivalent/not used
//
// FIXME: Remove everything that maps to WS_MAX to save space
//
// Gordon: why not er, just map directly to the stat?
// JK: er, several reasons: its less than half the size of a brute-force approach,
//							gives us a simple map to what we actually care about, and the
//							fact that we dont want to go above (if at all possible) 32 bits
#if defined(CGAMEDLL) || defined(GAMEDLL)
/* TC cgame300925e0 / qagame200ac880: identical numeric protocol IDs, not SDK WP_*.
 * Keep all 64 entries and first-match order, including the zero-filled tail. */
static const weap_ws_convert_t aWeapID[64] = {
	{ 0, 22 }, { 1, 0 }, { 2, 1 }, { 3, 3 }, { 4, 9 },
	{ 65, 7 }, { 66, 8 }, { 7, 2 }, { 8, 4 }, { 9, 9 },
	{ 10, 5 }, { 11, 14 }, { 12, 22 }, { 63, 13 }, { 14, 1 },
	{ 15, 11 }, { 16, 13 }, { 17, 10 }, { 18, 22 }, { 19, 22 },
	{ 20, 22 }, { 21, 22 }, { 22, 12 }, { 23, 21 }, { 24, 20 },
	{ 25, 20 }, { 26, 18 }, { 27, 22 }, { 28, 16 }, { 29, 18 },
	{ 30, 15 }, { 31, 19 }, { 62, 19 }, { 32, 21 }, { 33, 6 },
	{ 34, 19 }, { 36, 22 }, { 55, 17 }, { 56, 17 }, { 52, 2 },
	{ 57, 20 }, { 58, 21 }, { 59, 6 }, { 60, 10 }, { 35, 10 },
	{ 61, 22 }, { 53, 2 }, { 54, 1 }, { 37, 2 }, { 38, 1 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }
};
#else
static const weap_ws_convert_t aWeapID[WP_NUM_WEAPONS] = {

	{ WP_NONE,				WS_MAX },			// 0

	// German weapons
	{ WP_KNIFE,				WS_KNIFE },
	{ WP_LUGER,				WS_LUGER },
	{ WP_MP40,				WS_MP40 },
	{ WP_GRENADE_LAUNCHER,	WS_GRENADE },		// 5
	{ WP_PANZERFAUST,		WS_PANZERFAUST },
	{ WP_FLAMETHROWER,		WS_FLAMETHROWER },

	// American equivalents
	{ WP_COLT,				WS_COLT },			// 10
	{ WP_THOMPSON,			WS_THOMPSON },
	{ WP_GRENADE_PINEAPPLE,	WS_GRENADE },
	{ WP_STEN,				WS_STEN },
	{ WP_MEDIC_SYRINGE,		WS_SYRINGE },
	{ WP_AMMO,				WS_MAX },
	{ WP_ARTY,				WS_ARTILLERY },


	{ WP_SILENCER,			WS_LUGER },			// 20
	{ WP_DYNAMITE,			WS_DYNAMITE },
	{ WP_SMOKETRAIL,		WS_ARTILLERY },
	{ WP_MAPMORTAR,			WS_MORTAR },
	{ VERYBIGEXPLOSION,		WS_MAX },
	{ WP_MEDKIT,			WS_MAX },
	{ WP_BINOCULARS,		WS_MAX },

	{ WP_PLIERS,			WS_MAX },			// 30
	{ WP_SMOKE_MARKER,		WS_AIRSTRIKE },

	{ WP_KAR98,				WS_K43 },
	{ WP_CARBINE,			WS_GARAND },
	{ WP_GARAND,			WS_GARAND },
	{ WP_LANDMINE,			WS_LANDMINE },		// 35
	{ WP_SATCHEL,			WS_MAX },
	{ WP_SATCHEL_DET,		WS_SATCHEL },
	{ WP_TRIPMINE,			WS_LANDMINE },
	{ WP_SMOKE_BOMB,		WS_SMOKE },

	{ WP_MOBILE_MG42,		WS_MG42 },			// 40
	{ WP_MOBILE_MG42_SET,	WS_MG42 },
	{ WP_K43,				WS_K43 },
	{ WP_FG42,				WS_FG42 },
	{ WP_DUMMY_MG42,		WS_MG42 },	// ??
	{ WP_LOCKPICK,			WS_MAX },
	
	{ WP_GPG40,				WS_GRENADELAUNCHER },
	{ WP_M7,				WS_GRENADELAUNCHER },
	{ WP_SILENCED_COLT,		WS_COLT },			// 50
	{ WP_GARAND_SCOPE,		WS_GARAND },
	{ WP_K43_SCOPE,			WS_K43 },
	{ WP_FG42SCOPE,			WS_FG42 },
	{ WP_MORTAR_SET,		WS_MORTAR },
	{ WP_MORTAR,			WS_MORTAR },
	{ WP_MEDIC_ADRENALINE,	WS_MAX },			// 55

	{ WP_AKIMBO_SILENCEDCOLT,	WS_COLT		},
	{ WP_AKIMBO_SILENCEDLUGER,	WS_LUGER	},
	{ WP_AKIMBO_COLT,			WS_COLT		},
	{ WP_AKIMBO_LUGER,			WS_LUGER	},
};


#endif

// Get right stats index based on weapon id
extWeaponStats_t BG_WeapStatForWeapon( weapon_t iWeaponID ) {
	weapon_t i;

#if defined(CGAMEDLL) || defined(GAMEDLL)
	for( i = 0; i < 64; i++) {
#else
	for( i = WP_NONE; i < WP_NUM_WEAPONS; i++) {
#endif
		if( iWeaponID == aWeapID[i].iWeapon ) {
			return aWeapID[i].iWS;
		}
	}

#if defined(CGAMEDLL) || defined(GAMEDLL)
	return (extWeaponStats_t)22;
#else
	return WS_MAX;
#endif
}
