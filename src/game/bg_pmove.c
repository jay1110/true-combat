

// bg_pmove.c -- both games player movement code
// takes a playerstate and a usercmd as input and returns a modifed playerstate


#ifdef CGAMEDLL
#include "../cgame/cg_local.h"
#else
#include "q_shared.h"
#include "bg_public.h"
#endif // CGAMEDLL

#include "bg_local.h"
#include "tce_bg.h"
#include "tce_numeric.h"
#include "tce_weapon_ammo.h"
#include "tce_reload.h"
#include "tce_movement.h"
#include "tce_attack.h"
#include "tce_aim.h"

/* Native movement passes ps, character->animModelInfo, event, continue, force.
 * Cgame media extends the character layout; qagame has only its file and model. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && defined(__i386__))
typedef char movementCharacterPointers[(sizeof(void *) == 4 && sizeof(qhandle_t) == 4 &&
    MAX_QPATH == 64 && offsetof(pmove_t, ps) == 0 && offsetof(pmove_t, pmext) == 4 &&
    offsetof(pmove_t, character) == 8) ? 1 : -1];
#ifdef CGAMEDLL
typedef char movementCharacterMedia[(ACC_MAX == 13 && MAX_HD_ANIMATIONS == 16 &&
    sizeof(animation_t) == 0x70 && offsetof(bg_character_t, bodyDamageSkins) == 0x808 &&
    offsetof(bg_character_t, armsSkins) == 0x828 &&
    offsetof(bg_character_t, headDamageSkins) == 0x834 &&
    offsetof(bg_character_t, animModelInfo) == 0x83c &&
    sizeof(bg_character_t) == 0x840) ? 1 : -1];
#else
typedef char movementCharacterModel[(offsetof(bg_character_t, animModelInfo) == 0x40 &&
    sizeof(bg_character_t) == 0x44) ? 1 : -1];
#endif
#endif

#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
/* Interpret wrapped native posture timestamps without signed C overflow. */
typedef char proneTimeLayout[(sizeof(unsigned) == 4 && sizeof(int) == 4) ? 1 : -1];
static int PM_ProneTimeValue(unsigned bits) {
    int value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

#endif

#ifdef CGAMEDLL
#define PM_GameType cg_gameType.integer
#elif GAMEDLL
#define PM_GameType g_gametype.integer
#endif

#define PM_IsSinglePlayerGame() (PM_GameType == GT_SINGLE_PLAYER || PM_GameType == GT_COOP)


// JPW NERVE -- stuck this here so it can be seen client & server side
float Com_GetFlamethrowerRange(void) {
	return 2500; // multiplayer range is longer for balance
}
// jpw

pmove_t		*pm;
pml_t		pml;

// movement parameters
float	pm_stopspeed = 100;
//float	pm_duckScale = 0.25;

//----(SA)	modified
float	pm_waterSwimScale	= 0.5;
float	pm_waterWadeScale	= 0.70;
float	pm_slagSwimScale	= 0.30;
float	pm_slagWadeScale	= 0.70;

float	pm_proneSpeedScale	= 0.12f; // TC original movement scale (mutable).

float	pm_accelerate		= 10;
float	pm_airaccelerate	= 1;
float	pm_wateraccelerate	= 4;
float	pm_slagaccelerate	= 2;
float	pm_flyaccelerate	= 8;

float	pm_friction			= 6;
float	pm_waterfriction	= 1;
float	pm_slagfriction		= 1;
float	pm_flightfriction	= 3;
float	pm_ladderfriction	= 14;
float	pm_spectatorfriction = 5.0f;
float pm_VC100friction = 2.0f;

//----(SA)	end

int		c_pmove = 0;

#define TRIPMINE_RANGE 512.f


#ifdef GAMEDLL

// In just the GAME DLL, we want to store the groundtrace surface stuff,
// so we don't have to keep tracing.
void ClientStoreSurfaceFlags( int clientNum, int surfaceFlags);

#endif


/*
===============
PM_AddEvent

===============
*/
void PM_AddEvent( int newEvent ) {
	BG_AddPredictableEventToPlayerstate( newEvent, 0, pm->ps );
}

void PM_AddEventExt( int newEvent, int eventParm ) {
	BG_AddPredictableEventToPlayerstate( newEvent, eventParm, pm->ps );
}

int PM_IdleAnimForWeapon ( int weapon ) {
    return TCE_PM_IdleAnimForWeapon(weapon);
}

int PM_AltSwitchFromForWeapon(int weapon) { return 10; }

int PM_AltSwitchToForWeapon(int weapon) {
    switch (weapon) { case 31: case 35: case 55: case 56: return 10; default: return 11; }
}

int PM_AttackAnimForWeapon(int weapon) {
    return TCE_PM_AttackAnimForWeapon(weapon);
}

int PM_LastAttackAnimForWeapon(int weapon) {
    return TCE_PM_LastAttackAnimForWeapon(weapon);
}

int PM_NonemptyReloadAnimForWeapon(int weapon) {
    if (weapon == TCE_WP_SH1 || weapon == TCE_WP_SH2 || weapon == TCE_WP_SH5) {
        return 9;
    }
    if (pm->skill[SK_LIGHT_WEAPONS] >= 2 &&
        BG_isLightWeaponSupportingFastReload(weapon)) {
        return 8;
    }
    return 7;
}

int PM_ReloadAnimForWeapon(int weapon) {
    if (weapon == 55 || weapon == 56) {
        return 8;
    }
    if (weapon == 62) {
        return 9;
    }
    if (pm->skill[SK_LIGHT_WEAPONS] >= 2 &&
        BG_isLightWeaponSupportingFastReload(weapon)) {
        return 8;
    }
    return 7;
}

int PM_RaiseAnimForWeapon(int weapon) { return TCE_PM_RaiseAnimForWeapon(weapon); }

int PM_DropAnimForWeapon(int weapon) { return TCE_PM_DropAnimForWeapon(weapon); }

/*
===============
PM_AddTouchEnt
===============
*/
void PM_AddTouchEnt( int entityNum ) {
	int		i;

	if ( entityNum == ENTITYNUM_WORLD ) {
		return;
	}
	if ( pm->numtouch == MAXTOUCH ) {
		return;
	}

	// see if it is already added
	for ( i = 0 ; i < pm->numtouch ; i++ ) {
		if ( pm->touchents[ i ] == entityNum ) {
			return;
		}
	}

	// add it
	pm->touchents[pm->numtouch] = entityNum;
	pm->numtouch++;
}

/*
==============
PM_StartWeaponAnim
==============
*/
static void PM_StartWeaponAnim( int anim ) {
	if ( pm->ps->pm_type >= PM_DEAD )
		return;
	
	if ( pm->pmext->weapAnimTimer > 0 )
		return;

	if(pm->cmd.weapon == WP_NONE)
		return;

	pm->ps->weapAnim = ( ( pm->ps->weapAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;	
}

void PM_ContinueWeaponAnim( int anim ) {
	if(pm->cmd.weapon == WP_NONE)
		return;

	if ( ( pm->ps->weapAnim & ~ANIM_TOGGLEBIT ) == anim ) {
		return;
	}
	if ( pm->pmext->weapAnimTimer > 0 ) {
		return;		// a high priority animation is running
	}
	PM_StartWeaponAnim( anim );
}

/*
==================
PM_ClipVelocity

Slide off of the impacting surface
==================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
/* Windows retains the dot product until the sequential output stores.
 * Compiler precision and floating-point exceptions remain unverified. */
void PM_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce) {
    long double backoff = (long double)in[2] * normal[2];
    int i;

    backoff += (long double)in[1] * normal[1];
    backoff += (long double)in[0] * normal[0];
    /* Native C0 selects multiplication for negative or unordered values. */
    if (backoff < 0.0L || backoff != backoff) {
        backoff *= overbounce;
    } else {
        backoff /= overbounce;
    }
    for (i = 0; i < 3; ++i) {
        long double change = backoff * normal[i];
        out[i] = (float)((long double)in[i] - change);
    }
}
#else
void PM_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce ) {
	float	backoff;
	float	change;
	int		i;
	
	backoff = DotProduct (in, normal);
	
	if ( backoff < 0 ) {
		backoff *= overbounce;
	} else {
		backoff /= overbounce;
	}

	for ( i=0 ; i<3 ; i++ ) {
		change = normal[i]*backoff;
		out[i] = in[i] - change;
	}
}
#endif

/*
==================
PM_TraceAll

finds worst trace of body/legs, for collision.
==================
*/

void PM_TraceLegs( trace_t *trace, float *legsOffset, vec3_t start, vec3_t end, trace_t *bodytrace, vec3_t viewangles, void (tracefunc)( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask ), int ignoreent, int tracemask )
{
	trace_t steptrace;
	vec3_t ofs, org, point;

#if !defined(_MSC_VER) || !defined(_M_IX86)
	vec3_t flatforward;
	float angle;
#endif
    vec3_t legMins, legMaxs;
    float sizeScale = (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) ? 1.25f : 1.0f;
    int tryStep;

    /* TC:E30008a80 / Linux000da4e0: freeze scaled bounds before callbacks. */
    VectorCopy(playerlegsProneMins, legMins);
    VectorCopy(playerlegsProneMaxs, legMaxs);
    if (sizeScale != 1.0f) {
        VectorScale(legMins, sizeScale, legMins);
        VectorScale(legMaxs, sizeScale, legMaxs);
    }

	// zinx - don't let players block legs
	tracemask &= ~(CONTENTS_BODY | CONTENTS_CORPSE);

	if (legsOffset) {
		*legsOffset = 0;
	}

#if defined(_MSC_VER) && defined(_M_IX86)
    {
        long double angleValue = (long double)viewangles[YAW] * 3.1415927410125732421875f;
        volatile float traceCos, traceSin;
        long double distance = (long double)sizeScale * -32.0f;

        angleValue *= 0.0055555556900799274444580078125f;
        /* Original hardware trig outputs are stored as float before scaling. */
        traceCos = (float)cosl(angleValue);
        traceSin = (float)sinl(angleValue);
        ofs[0] = (float)(distance * traceCos);
        ofs[1] = (float)(distance * traceSin);
        ofs[2] = 0.0f;
    }
#else
	angle = DEG2RAD(viewangles[YAW]);
	flatforward[0] = cos(angle);
	flatforward[1] = sin(angle);
	flatforward[2] = 0;

	VectorScale(flatforward, sizeScale * -32.0f, ofs);
#endif

	VectorAdd(ofs, start, org);
	VectorAdd(ofs, end, point);
	tracefunc(trace, org, legMins, legMaxs, point, ignoreent, tracemask);
    tryStep = !bodytrace;
    if (bodytrace) {
#if defined(_MSC_VER) && defined(_M_IX86)
        /* Native C0 also accepts an unordered comparison. */
        tryStep = trace->fraction < bodytrace->fraction ||
            TCE_FloatIsNaN(trace->fraction) || TCE_FloatIsNaN(bodytrace->fraction);
#else
        tryStep = trace->fraction < bodytrace->fraction;
#endif
    }
	if (tryStep || trace->allsolid) {
		// legs are clipping sooner than body
		// see if our legs can step up

		// give it a try with the new height
		ofs[2] += STEPSIZE;

		VectorAdd(ofs, start, org);
		VectorAdd(ofs, end, point);
		tracefunc(&steptrace, org, legMins, legMaxs, point, ignoreent, tracemask);
        tryStep = 0;
        if (!steptrace.allsolid && !steptrace.startsolid) {
#if defined(_MSC_VER) && defined(_M_IX86)
            tryStep = steptrace.fraction > trace->fraction;
#else
            tryStep = steptrace.fraction > trace->fraction;
#endif
        }
		if (tryStep) {
			// the step trace did better -- use it instead
			*trace = steptrace;

			// get legs offset
			if (legsOffset) {
				*legsOffset = ofs[2];

				VectorCopy(steptrace.endpos, org);
				VectorCopy(steptrace.endpos, point);
				point[2] -= STEPSIZE;

				tracefunc(&steptrace, org, legMins, legMaxs, point, ignoreent, tracemask);
				if (!steptrace.allsolid) {
#if defined(_MSC_VER) && defined(_M_IX86)
                    *legsOffset = (float)((long double)ofs[2] -
                        ((long double)org[2] - steptrace.endpos[2]));
#else
					*legsOffset = ofs[2] - (org[2] - steptrace.endpos[2]);
#endif
				}
			}
		}
	}
}

/* Traces all player bboxes -- body and legs */
void PM_TraceAllLegs( trace_t *trace, float *legsOffset, vec3_t start, vec3_t end ) {
	pm->trace( trace, start, pm->mins, pm->maxs, end, pm->ps->clientNum, pm->tracemask );

	/* legs */
	if ( pm->ps->eFlags & EF_PRONE ) {
		trace_t legtrace;

		PM_TraceLegs( &legtrace, legsOffset, start, end, trace, pm->ps->viewangles, pm->trace, pm->ps->clientNum, pm->tracemask );

		if ( legtrace.fraction < trace->fraction ||
			 legtrace.startsolid ||
			 legtrace.allsolid ) {
			VectorSubtract( end, start, legtrace.endpos );
			VectorMA( start, legtrace.fraction, legtrace.endpos, legtrace.endpos );
			*trace = legtrace;
		}
	}
}

void PM_TraceAll( trace_t *trace, vec3_t start, vec3_t end )
{
	PM_TraceAllLegs( trace, NULL, start, end );
}

/*
========================
PM_ExertSound

plays random exertion sound when sprint key is press
========================
*/
/*static void PM_ExertSound (void)
{
	int rval;
	static int	oldexerttime = 0;
	static int	oldexertcnt = 0;

	if (pm->cmd.serverTime > oldexerttime + 500)
		oldexerttime = pm->cmd.serverTime;
	else
		return;
	
	rval = rand()%3;

	if (oldexertcnt != rval)
		 oldexertcnt = rval;
	else
		oldexertcnt++;
	
	if (oldexertcnt > 2)
		oldexertcnt = 0;
				
	if (oldexertcnt == 1)
		PM_AddEvent (EV_EXERT2);
	else if (oldexertcnt == 2)
		PM_AddEvent (EV_EXERT3);
	else
		PM_AddEvent (EV_EXERT1);
}*/


/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
/* Whole TC:E3000b950 / Linux000dee7e. */
#if defined(_MSC_VER) && defined(_M_IX86)
static void PM_Friction(void) {
    float *velocity = pm->ps->velocity;
    vec3_t vec;
    float preservedZ = 0.0f;
    volatile float speed, ratio;
    long double heldSpeed, drop = 0.0L, control, remaining;
    unsigned elapsedBits;
    int elapsed, type;

    VectorCopy(velocity, vec);
    if (pml.walking) {
        if (pml.tceGroundExtension) preservedZ = vec[2];
        vec[2] = 0.0f;
    }
    heldSpeed = TCE_VectorLengthValue(vec);
    speed = (float)heldSpeed;
    /* Native C0 also takes the low-speed branch for unordered values. */
    if ((heldSpeed < 1.0L || heldSpeed != heldSpeed) &&
        pm->ps->pm_type != PM_SPECTATOR && pm->ps->pm_type != PM_NOCLIP) {
        velocity[0] = velocity[1] = 0.0f;
        return;
    }
    elapsedBits = (unsigned)pm->cmd.serverTime - (unsigned)pm->pmext->dodgeTime;
    memcpy(&elapsed, &elapsedBits, sizeof(elapsed));
    if (elapsed > 250 && elapsed < 350) {
        drop = (long double)pml.frametime * speed;
        drop *= 20.0f;
    }
    if (pm->waterlevel < 2 && pml.walking &&
        !(pml.groundTrace.surfaceFlags & SURF_SLICK) &&
        !(pm->ps->pm_flags & PMF_TIME_KNOCKBACK)) {
        control = (long double)pm_stopspeed * 0.5f;
        if (speed >= control) control = speed;
        drop += (long double)pm_friction * pml.frametime * control;
    }
    if (pm->waterlevel) {
        long double waterDrop = pm->waterlevel;
        waterDrop *= pm->watertype == CONTENTS_SLIME ? pm_slagfriction : pm_waterfriction;
        waterDrop *= pml.frametime;
        waterDrop *= speed;
        drop += waterDrop;
    }
    type = pm->ps->pm_type;
    if (type == PM_SPECTATOR)
        drop += (long double)pm_spectatorfriction * pml.frametime * speed;
    if (pm->ps->eFlags & 0x01000000)
        drop += (long double)pm_VC100friction * pml.frametime * speed;
    if (pml.ladder)
        drop += (long double)pm_ladderfriction * pml.frametime * speed;
    remaining = (long double)speed - drop;
    if (remaining < 0.0L || remaining != remaining) remaining = 0.0L;
    ratio = (float)(remaining / speed);
    if ((type == PM_SPECTATOR || type == PM_NOCLIP) &&
        (drop < 1.0L || drop != drop) && (speed < 3.0f || speed != speed)) {
        ratio = 0.0f;
    }
    velocity[0] = (float)((long double)ratio * velocity[0]);
    velocity[1] = (float)((long double)ratio * velocity[1]);
    velocity[2] = (float)((long double)ratio * velocity[2]);
    /* Keep the prior defined fallback; native saved-Z reachability is unresolved. */
    if (pml.tceGroundExtension && (preservedZ < 0.0f || preservedZ != preservedZ))
        velocity[2] = preservedZ;
}
#else
static void PM_Friction(void) {
    vec3_t vec;
    float speed, ratio, preservedZ = 0;
    /* Original keeps accumulated friction in x87 until the final ratio spill. */
    double drop = 0, control, remaining;
    int elapsed;
    VectorCopy(pm->ps->velocity, vec);
    if (pml.walking) {
        if (pml.tceGroundExtension) preservedZ = vec[2];
        vec[2] = 0;
    }
    speed = VectorLength(vec);
    if (speed < 1.f && pm->ps->pm_type != PM_SPECTATOR && pm->ps->pm_type != PM_NOCLIP) {
        pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
        return;
    }
    elapsed = pm->cmd.serverTime - pm->pmext->dodgeTime;
    if (elapsed > 250 && elapsed < 350) drop = (double)pml.frametime * speed * 20.f;
    if (pm->waterlevel < 2 && pml.walking && !(pml.groundTrace.surfaceFlags & SURF_SLICK) &&
        !(pm->ps->pm_flags & PMF_TIME_KNOCKBACK)) {
        control = (double)pm_stopspeed * 0.5f;
        if (control <= speed) control = speed;
        drop += (double)pm_friction * pml.frametime * control;
    }
    if (pm->waterlevel)
        drop += (double)pm->waterlevel * (pm->watertype == CONTENTS_SLIME ? pm_slagfriction : pm_waterfriction) * pml.frametime * speed;
    if (pm->ps->pm_type == PM_SPECTATOR) drop += (double)pm_spectatorfriction*pml.frametime*speed;
    if (pm->ps->eFlags & 0x01000000) drop += (double)pm_VC100friction*pml.frametime*speed;
    if (pml.ladder) drop += (double)pm_ladderfriction*pml.frametime*speed;
    remaining = speed - drop;
    if (remaining < 0) remaining = 0;
    ratio = (float)(remaining / speed);
    if ((pm->ps->pm_type == PM_SPECTATOR || pm->ps->pm_type == PM_NOCLIP) && drop < 1.f && speed < 3.f)
        ratio = 0;
    VectorScale(pm->ps->velocity, ratio, pm->ps->velocity);
    if (pml.tceGroundExtension && preservedZ < 0) pm->ps->velocity[2] = preservedZ;
}

#endif


/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/
/* Whole TC:E3000bb80; also inlined by Linux Fly/Noclip/Ladder. */
static void PM_Accelerate(vec3_t wishdir, float wishspeed, float accel) {
#if defined(_MSC_VER) && defined(_M_IX86)
    vec3_t push;
    long double desiredX = (long double)wishspeed * wishdir[0];
    volatile float desiredY = (float)((long double)wishspeed * wishdir[1]);
    volatile float desiredZ = (float)((long double)wishspeed * wishdir[2]);
    volatile float length;
    long double amount;

    push[0] = (float)(desiredX - pm->ps->velocity[0]);
    push[1] = (float)((long double)desiredY - pm->ps->velocity[1]);
    push[2] = (float)((long double)desiredZ - pm->ps->velocity[2]);
    length = VectorNormalize(push);
    amount = (long double)pml.frametime * wishspeed;
    amount *= accel;
    /* Native C0/C3 retains amount on less, equal or unordered comparison. */
    if (amount > (long double)length) {
        amount = length;
    }
    pm->ps->velocity[0] = (float)(amount * push[0] + pm->ps->velocity[0]);
    pm->ps->velocity[1] = (float)(amount * push[1] + pm->ps->velocity[1]);
    pm->ps->velocity[2] = (float)(amount * push[2] + pm->ps->velocity[2]);
#else
    vec3_t push;
    float length, amount;
    VectorScale(wishdir, wishspeed, push);
    VectorSubtract(push, pm->ps->velocity, push);
    length = VectorNormalize(push);
    amount = pml.frametime * wishspeed * accel;
    if (length < amount) amount = length;
    VectorMA(pm->ps->velocity, amount, push, pm->ps->velocity);
#endif
}


// JPW NERVE -- added because I need to check single/multiplayer instances and branch accordingly
#ifdef CGAMEDLL
	extern vmCvar_t			cg_gameType;
	extern vmCvar_t			cg_movespeed;
#endif
#ifdef GAMEDLL
	extern	vmCvar_t		g_gametype;
	extern	vmCvar_t		g_movespeed;
#endif

/*
============
PM_CmdScale

Returns the scale factor to apply to cmd movements
This allows the clients to use axial -127 to 127 values for all directions
without getting a sqrt(2) distortion in speed.
============
*/
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
/* Native TCE input scaling. Native held values and float stores are
 * represented explicitly. The private return keeps the value for native
 * callers to store or consume; compiled x87 precision/ABI remains open. */
typedef char cmdScaleLayout[(sizeof(int) == 4 && sizeof(usercmd_t) == 28 &&
    offsetof(usercmd_t, forwardmove) == 20 && offsetof(usercmd_t, rightmove) == 21 &&
    offsetof(usercmd_t, upmove) == 22 && offsetof(usercmd_t, buttons) == 4 &&
    BUTTON_SPRINT == 0x20 && BUTTON_WALKING == 0x10) ? 1 : -1];
#ifdef CGAMEDLL
#define CS_MOVE_CVAR cg_movespeed
#define CS_GAME_CVAR cg_gameType
#else
#define CS_MOVE_CVAR g_movespeed
#define CS_GAME_CVAR g_gametype
#endif
static long double PM_CmdScale(usercmd_t *cmd) {
    int movespeed = CS_MOVE_CVAR.integer;
#if defined(__linux__)
    int entryGametype = CS_GAME_CVAR.integer;
#endif
    int forward = cmd->forwardmove, right = cmd->rightmove, up = cmd->upmove;
    int maximum = abs(forward), buttons, ducked, weight, gametype;
    volatile float total;
#if !defined(__linux__)
    volatile float adjustedForward;
#endif
    float backwardsDivisor, sidewaysDivisor;
    long double scale, sideways, squared;
#if defined(__linux__)
    volatile float baseScale, directionRatio;
    long double linuxForward;
#endif
    qboolean leaning, tactical;

    if (abs(right) > maximum) maximum = abs(right);
    if (abs(up) > maximum) maximum = abs(up);
    if (!maximum) return 0.0L;
    total = (float)sqrtl((long double)(forward * forward + up * up + right * right));
#if defined(__linux__)
    baseScale = (float)(((long double)pm->ps->speed * maximum) *
        (1.0L / ((long double)total * 127.0f)));
    scale = baseScale;
#else
    scale = (long double)pm->ps->speed * maximum / ((long double)total * 127.0);
#endif
    buttons = pm->cmd.buttons;
    ducked = pm->ps->pm_flags & PMF_DUCKED;
    tactical = (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) != 0;
    /* Native C3 takes the zero-lean branch for zero or unordered lean. */
    leaning = pm->ps->leanf != 0.0f && pm->ps->leanf == pm->ps->leanf;
    if ((buttons & BUTTON_SPRINT) && !pml.ladder && !ducked) {
        if (pm->pmext->sprintTime > 50) scale *= pm->ps->sprintSpeedScale;
        else {
            scale *= pm->ps->runSpeedScale;
            scale *= 1.4f;
        }
        if (buttons & BUTTON_WALKING) scale += scale;
    } else if (ducked) {
        scale *= pm->ps->crouchSpeedScale;
        if (buttons & BUTTON_WALKING) scale += scale;
        if (tactical || leaning || (buttons & BUTTON_WALKING)) scale *= 0.85f;
    } else {
        scale *= pm->ps->runSpeedScale;
        if (tactical || leaning) {
            scale *= 0.55f;
            if (buttons & BUTTON_WALKING) scale *= 1.818f;
        }
        if (buttons & BUTTON_WALKING) scale *= 0.833f;
    }
    if (pm->ps->pm_type == PM_NOCLIP) scale *= 3.0f;
    /* The original directly indexes the definition, without parser gating. */
    weight = weaponDef[pm->ps->weapon].loadoutWeight;
    if (weight != 4) {
        if (weight == 5) scale *= 0.95f;
        else if (pm->ps->holdable[9] < 4) scale *= 1.05f;
        else if (pm->ps->holdable[9] < 5) scale *= 1.025f;
    }
#if defined(__linux__)
    gametype = entryGametype;
#else
    gametype = CS_GAME_CVAR.integer;
#endif
    if (gametype == 0 || gametype == 1) {
#if defined(__linux__)
        scale *= movespeed;
        scale *= 0.007874015718698502f;
#else
        scale *= (long double)movespeed * 0.007874015718698502f;
#endif
    }
    buttons = cmd->buttons;
    if (((buttons & BUTTON_WALKING) && !(buttons & BUTTON_SPRINT)) || tactical || leaning || ducked) {
        backwardsDivisor = 1.5625f;
        sidewaysDivisor = 1.25f;
    } else if (buttons & BUTTON_SPRINT) {
        backwardsDivisor = 2.5f;
        sidewaysDivisor = 2.0f;
    } else {
        backwardsDivisor = 1.875f;
        sidewaysDivisor = 1.5f;
    }
#if defined(__linux__)
    /* Linux keeps both corrected components held, then stores the ratio. */
    linuxForward = forward;
    if (forward < 0) linuxForward *= 1.0L / backwardsDivisor;
    sideways = (long double)right * (1.0L / sidewaysDivisor);
    squared = linuxForward * linuxForward;
    squared += sideways * sideways;
    squared += up * up;
    directionRatio = (float)(sqrtl(squared) * (1.0L / total));
    return (long double)directionRatio * scale;
#else
    adjustedForward = (float)forward;
    if (forward < 0) adjustedForward = (float)((long double)adjustedForward / backwardsDivisor);
    sideways = (long double)right / sidewaysDivisor;
    squared = sideways * sideways;
    squared += (long double)adjustedForward * adjustedForward;
    squared += up * up;
    return (sqrtl(squared) / total) * scale;
#endif
}
#undef CS_MOVE_CVAR
#undef CS_GAME_CVAR
#else
static float PM_CmdScale( usercmd_t *cmd ) {
	int		max;
	float	total;
	float	scale;

#ifdef CGAMEDLL
	int gametype = cg_gameType.integer;
	int movespeed = cg_movespeed.integer;
#elif GAMEDLL
	int gametype = g_gametype.integer;
	int movespeed = g_movespeed.integer;
#endif

    /* TC20033fa0 reads the definition directly, independent of parser readiness. */
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        tce_moveScale_t state;
        state.forward = cmd->forwardmove; state.right = cmd->rightmove; state.up = cmd->upmove;
        state.commandButtons = cmd->buttons; state.moveButtons = pm->cmd.buttons;
        state.speed = pm->ps->speed; state.ducked = (pm->ps->pm_flags & PMF_DUCKED) != 0;
        state.ladder = pml.ladder; state.sprintTime = pm->pmext->sprintTime;
        state.noclip = pm->ps->pm_type == PM_NOCLIP;
        state.tactical = (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) != 0;
        state.weaponWeight = weaponDef[pm->ps->weapon].loadoutWeight;
        state.carriedWeight = pm->ps->holdable[9];
        state.gametype = gametype; state.movespeed = movespeed;
        state.lean = pm->ps->leanf; state.runScale = pm->ps->runSpeedScale;
        state.sprintScale = pm->ps->sprintSpeedScale; state.crouchScale = pm->ps->crouchSpeedScale;
        return TCE_PM_CmdScale(&state);
    }

	max = abs( cmd->forwardmove );
	if ( abs( cmd->rightmove ) > max ) {
		max = abs( cmd->rightmove );
	}
	if ( abs( cmd->upmove ) > max ) {
		max = abs( cmd->upmove );
	}
	if ( !max ) {
		return 0;
	}

	total = sqrt( cmd->forwardmove * cmd->forwardmove
		+ cmd->rightmove * cmd->rightmove + cmd->upmove * cmd->upmove );
	scale = (float)pm->ps->speed * max / ( 127.0 * total );

	if (pm->cmd.buttons & BUTTON_SPRINT && pm->pmext->sprintTime > 50)
	{
		scale *= pm->ps->sprintSpeedScale;
	}
	else
		scale *= pm->ps->runSpeedScale;
	
	if (pm->ps->pm_type == PM_NOCLIP)
		scale *= 3;

// JPW NERVE -- half move speed if heavy weapon is carried
// this is the counterstrike way of doing it -- ie you can switch to a non-heavy weapon and move at
// full speed.  not completely realistic (well, sure, you can run faster with the weapon strapped to your
// back than in carry position) but more fun to play.  If it doesn't play well this way we'll bog down the
// player if the own the weapon at all.
//
	if ((pm->ps->weapon == WP_PANZERFAUST) ||
		(pm->ps->weapon == WP_MOBILE_MG42) ||
		(pm->ps->weapon == WP_MOBILE_MG42_SET) ||
		(pm->ps->weapon == WP_MORTAR)) {
		if( pm->skill[SK_HEAVY_WEAPONS] >= 3 ) {
			scale *= 0.75;
		} else {
			scale *= 0.5;
		}
	}
	
	if (pm->ps->weapon == WP_FLAMETHROWER) { // trying some different balance for the FT
		if( !(pm->skill[SK_HEAVY_WEAPONS] >= 3) || pm->cmd.buttons & BUTTON_ATTACK )
			scale *= 0.7;
	}


	if (gametype == GT_SINGLE_PLAYER || gametype == GT_COOP) {
		// Adjust the movespeed
		scale *= (((float) movespeed)/(float) 127);

	} // if (gametype == GT_SINGLE_PLAYER)...

	return scale;
}

#endif


/*
================
PM_SetMovementDir

Determine the rotation of the legs reletive
to the facing dir
================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC:E movement protocol and command-layout requirements. */
typedef char moveCommandSize827[(sizeof(usercmd_t) == 28) ? 1 : -1];
typedef char moveCommandTime827[(offsetof(usercmd_t, serverTime) == 0) ? 1 : -1];
typedef char moveProtocol827[(DT_MOVELEFT == 1 && EF_PRONE == 0x80000 && ENTITYNUM_NONE == 0x3ff) ? 1 : -1];
static void PM_SetMovementDir(void) {
    vec3_t moved, direction;
    long double speed;
    volatile float backwardAngle;
    unsigned bits, magnitude, low;
    int moveYaw, signedMagnitude, turnedYaw, i;

    for (i = 0; i < 3; ++i) {
        moved[i] = (float)((long double)pm->ps->origin[i] - pml.previous_origin[i]);
    }
    if (!(pm->cmd.forwardmove || pm->cmd.rightmove) ||
        pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        pm->ps->movementDir = 0;
        return;
    }
    speed = TCE_VectorLengthValue(moved);
    if (speed == 0.0L || speed != speed ||
        !(speed > (long double)pml.frametime * 5.0f)) {
        pm->ps->movementDir = 0;
        return;
    }
    VectorNormalize2(moved, direction);
    vectoangles(direction, direction);
    moveYaw = TCE_TruncateNativeInteger(
        (long double)AngleDelta(direction[YAW], pm->ps->viewangles[YAW]));
    if (pm->cmd.forwardmove < 0) {
        bits = (unsigned)moveYaw + 180u;
        memcpy(&turnedYaw, &bits, sizeof(turnedYaw));
        backwardAngle = (float)turnedYaw;
        moveYaw = TCE_TruncateNativeInteger((long double)AngleNormalize180(backwardAngle));
    }
    /* Native abs(INT_MIN) wraps and stays negative in the signed clamp test. */
    magnitude = moveYaw < 0 ? 0u - (unsigned)moveYaw : (unsigned)moveYaw;
    memcpy(&signedMagnitude, &magnitude, sizeof(signedMagnitude));
    if (signedMagnitude > 75) moveYaw = moveYaw > 0 ? 75 : -75;
    low = (unsigned)moveYaw & 255u;
    pm->ps->movementDir = low < 128u ? (int)low : (int)low - 256;
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
static void PM_SetMovementDir( void ) {
// Ridah, changed this for more realistic angles (at the cost of more network traffic?)
#if 1
	float	speed;
	vec3_t	moved;
	int moveyaw;

	VectorSubtract (pm->ps->origin, pml.previous_origin, moved);

	if (	(pm->cmd.forwardmove || pm->cmd.rightmove)
		&&	(pm->ps->groundEntityNum != ENTITYNUM_NONE)
		&&	(speed = VectorLength( moved ))
		&&	(speed > pml.frametime*5))		// if moving slower than 20 units per second, just face head angles
	{
		vec3_t	dir;

		VectorNormalize2( moved, dir );
		vectoangles( dir, dir );

		moveyaw = (int)AngleDelta( dir[YAW], pm->ps->viewangles[YAW] );

		if (pm->cmd.forwardmove < 0)
			moveyaw = (int)AngleNormalize180(moveyaw + 180);

		if (abs(moveyaw) > 75)
		{
			if (moveyaw > 0)
			{
				moveyaw = 75;
			}
			else
			{
				moveyaw = -75;
			}
		}

		pm->ps->movementDir = (signed char)moveyaw;
	}
	else
	{
		pm->ps->movementDir = 0;
	}
#else
	if ( pm->cmd.forwardmove || pm->cmd.rightmove ) {
		if ( pm->cmd.rightmove == 0 && pm->cmd.forwardmove > 0 ) {
			pm->ps->movementDir = 0;
		} else if ( pm->cmd.rightmove < 0 && pm->cmd.forwardmove > 0 ) {
			pm->ps->movementDir = 1;
		} else if ( pm->cmd.rightmove < 0 && pm->cmd.forwardmove == 0 ) {
			pm->ps->movementDir = 2;
		} else if ( pm->cmd.rightmove < 0 && pm->cmd.forwardmove < 0 ) {
			pm->ps->movementDir = 3;
		} else if ( pm->cmd.rightmove == 0 && pm->cmd.forwardmove < 0 ) {
			pm->ps->movementDir = 4;
		} else if ( pm->cmd.rightmove > 0 && pm->cmd.forwardmove < 0 ) {
			pm->ps->movementDir = 5;
		} else if ( pm->cmd.rightmove > 0 && pm->cmd.forwardmove == 0 ) {
			pm->ps->movementDir = 6;
		} else if ( pm->cmd.rightmove > 0 && pm->cmd.forwardmove > 0 ) {
			pm->ps->movementDir = 7;
		}
	} else {
		// if they aren't actively going directly sideways,
		// change the animation to the diagonal so they
		// don't stop too crooked
		if ( pm->ps->movementDir == 2 ) {
			pm->ps->movementDir = 1;
		} else if ( pm->ps->movementDir == 6 ) {
			pm->ps->movementDir = 7;
		} 
	}
#endif
}
#endif


/*
=============
PM_CheckJump
=============
*/
/* TC:E CheckJump: native Windows gates, float stores and event ordering.
 * Linux retains its existing fallback; FPU/ABI parity is tracked separately. */
#if defined(_MSC_VER) && defined(_M_IX86)
static void PM_WeaponTruncateST0(void);
typedef char movementScalarLayout833[(sizeof(float) == 4 && sizeof(int) == 4 &&
    sizeof(vec3_t) == 12 && EV_JUMP == 24 && ANIM_ET_JUMP == 4 && ANIM_ET_JUMPBK == 5) ? 1 : -1];
static qboolean PM_CheckJump(void) {
    playerState_t *ps = pm->ps;
    volatile float scale = 1.0f, horizontal;
    long double value, length;
    vec3_t direction;
    int seed;

    if (ps->eFlags & EF_PRONE) return qfalse;
    if (ps->pm_flags & PMF_DUCKED) return qfalse;
    if (ps->eFlags & EF_PRONE_MOVING) return qfalse;
    if (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x1000) return qfalse;
    if (ps->groundEntityNum < MAX_CLIENTS) return qfalse;
    if (ps->pm_flags & PMF_LADDER) return qfalse;
    if (ps->groundEntityNum == ENTITYNUM_NONE) return qfalse;
    if (ps->pm_flags & PMF_RESPAWNED) return qfalse;
    if (pm->cmd.upmove < 10) return qfalse;
    if (ps->pm_flags & PMF_JUMP_HELD) {
        pm->cmd.upmove = 0;
        return qfalse;
    }
    if (!(ps->stats[STAT_TCE_FLAGS] & 0x40)) {
        if (ps->pm_time > 500) {
            pm->cmd.upmove = 0;
            return qfalse;
        }
        if ((ps->pm_flags & PMF_TIME_LAND) && ps->pm_time) {
            if (ps->pm_time > 600) {
                pm->cmd.upmove = 0;
                return qfalse;
            }
            value = 1.0L - (long double)ps->pm_time * 0.001f;
            scale = (float)value;
            /* Compare the held value; C0 also selects zero for unordered. */
            if (value < 0.0L || value != value) scale = 0.0f;
        }
    }
    value = (long double)ps->velocity[0] * ps->velocity[0];
    value += (long double)ps->velocity[1] * ps->velocity[1];
    horizontal = (float)sqrtl(value);
    length = TCE_VectorLengthValue(ps->velocity);
    if ((length >= 50.0L || !pm->cmd.forwardmove) &&
        !(pm->ps->stats[STAT_TCE_FLAGS] & 0x40)) {
        scale = (float)((long double)scale * 0.707f);
        if (pm->pmext->sprintTime < 750) {
            pm->cmd.upmove = 0;
            return qfalse;
        }
    }
    if (horizontal > 10.0f && (pm->cmd.forwardmove || pm->cmd.rightmove)) {
        direction[0] = (float)((long double)pm->cmd.forwardmove * 0.007874015718698502f);
        direction[1] = (float)((long double)pm->cmd.rightmove * 0.007874015718698502f);
        direction[2] = 0.0f;
        (void)TCE_VectorNormalizeValue(direction);
        value = ((long double)direction[0] + 1.0f) * 0.5f;
        if (value < 0.5L || value != value) value = 0.5L;
        scale = (float)(value * scale);
    }
    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x1000;
    pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
    if (pm->ps->weaponstate != WEAPON_FIRING) {
        seed = pm->ps->stats[STAT_TCE_SHOT_SEED];
        value = (long double)Q_random(&seed) * 1000.0f;
        pm->ps->stats[STAT_TCE_AIM_PHASE] = TCE_TruncateNativeInteger(value);
        if (pm->ps->stats[STAT_TCE_AIM_PHASE] > 1000) {
            pm->ps->stats[STAT_TCE_AIM_PHASE] -= 1000;
        }
    }
    if (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) {
        scale = (float)((long double)scale * 1.25f);
    }
    pml.groundPlane = qfalse;
    pml.walking = qfalse;
    pm->ps->pm_flags |= PMF_JUMP_HELD;
    value = (long double)scale * 270.0f;
    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pm->ps->velocity[2] = (float)value;
    PM_AddEvent(EV_JUMP);
    if (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) {
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~4;
    }
    if ((pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 8) && !(pm->ps->persistant[14] & 8)) {
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~8;
    }
    if (pm->cmd.forwardmove >= 0) {
        BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, ANIM_ET_JUMP, qfalse, qtrue);
        pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
    } else {
        BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, ANIM_ET_JUMPBK, qfalse, qtrue);
        pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
    }
    return qtrue;
}
#else
static qboolean PM_CheckJump( void ) {
    playerState_t *ps = pm->ps;
    float scale = 1.0f, horizontal, directionScale;
    vec3_t direction;
    int seed;
    if ((ps->eFlags & (EF_PRONE | EF_PRONE_MOVING)) || (ps->pm_flags & PMF_DUCKED) ||
        (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x1000) || ps->groundEntityNum < MAX_CLIENTS ||
        (ps->pm_flags & PMF_LADDER) || ps->groundEntityNum == ENTITYNUM_NONE ||
        (ps->pm_flags & PMF_RESPAWNED) || pm->cmd.upmove < 10) return qfalse;
    if (ps->pm_flags & PMF_JUMP_HELD) { pm->cmd.upmove = 0; return qfalse; }
    if (!(ps->stats[STAT_TCE_FLAGS] & 0x40)) {
        if (ps->pm_time > 500) { pm->cmd.upmove = 0; return qfalse; }
        if ((ps->pm_flags & PMF_TIME_LAND) && ps->pm_time) {
            if (ps->pm_time > 600) { pm->cmd.upmove = 0; return qfalse; }
            scale = 1.0f - ps->pm_time * 0.001f;
            if (scale < 0) scale = 0;
        }
    }
    horizontal = sqrt(ps->velocity[1]*ps->velocity[1] + ps->velocity[0]*ps->velocity[0]);
    if ((VectorLength(ps->velocity) >= 50.0f || !pm->cmd.forwardmove) &&
        !(ps->stats[STAT_TCE_FLAGS] & 0x40)) {
        scale *= 0.707f;
        if (pm->pmext->sprintTime < 750) { pm->cmd.upmove = 0; return qfalse; }
    }
    if (horizontal > 10.0f && (pm->cmd.forwardmove || pm->cmd.rightmove)) {
        VectorSet(direction, pm->cmd.forwardmove * 0.007874015718698502f,
                  pm->cmd.rightmove * 0.007874015718698502f, 0);
        VectorNormalize(direction);
        directionScale = (direction[0] + 1.0f) * 0.5f;
        if (directionScale < 0.5f) directionScale = 0.5f;
        scale *= directionScale;
    }
    ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x1000;
    ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
    if (ps->weaponstate != WEAPON_FIRING) {
        seed = ps->stats[STAT_TCE_SHOT_SEED];
        ps->stats[STAT_TCE_AIM_PHASE] = (int)(Q_random(&seed) * 1000.0f);
        if (ps->stats[STAT_TCE_AIM_PHASE] > 1000) ps->stats[STAT_TCE_AIM_PHASE] -= 1000;
    }
    if (ps->stats[STAT_TCE_FLAGS] & 0x200) scale *= 1.25f;
    pml.groundPlane = pml.walking = qfalse;
    ps->pm_flags |= PMF_JUMP_HELD;
    ps->groundEntityNum = ENTITYNUM_NONE;
    ps->velocity[2] = scale * 270.0f;
    PM_AddEvent(EV_JUMP);
    ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~4;
    /* Original persistent[14] (ps+148): locked aiming mode. */
    if (!(ps->persistant[14] & 8)) ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~8;
    if (pm->cmd.forwardmove >= 0) {
        BG_AnimScriptEvent(ps, pm->character->animModelInfo, ANIM_ET_JUMP, qfalse, qtrue);
        ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
    } else {
        BG_AnimScriptEvent(ps, pm->character->animModelInfo, ANIM_ET_JUMPBK, qfalse, qtrue);
        ps->pm_flags |= PMF_BACKWARDS_JUMP;
    }
    return qtrue;
}
#endif

/*
=============
PM_CheckWaterJump
=============
*/
#if defined(_MSC_VER) && defined(_M_IX86)
/* Whole TC200356a0/20035490, exact Windows instruction schedule and native fields. */
static const float waterK200ac190 = 30.0;
static const float waterK200ac2bc = 4.0;
static const float waterK200ac384 = 16.0;
static const float waterK200ac1a4 = 200.0;
typedef char waterProtocol830[(CONTENTS_SOLID==1 && CONTENTS_SLIME==16 && PMF_TIME_WATERJUMP==0x100) ? 1 : -1];
enum {
    waterPlayerState830=offsetof(pmove_t,ps),
    waterLevel830=offsetof(pmove_t,waterlevel),
    waterType830=offsetof(pmove_t,watertype),
    waterContents830=offsetof(pmove_t,pointcontents),
    waterCommand830=offsetof(pmove_t,cmd),
    waterForwardCmd830=offsetof(pmove_t,cmd)+offsetof(usercmd_t,forwardmove),
    waterRightCmd830=offsetof(pmove_t,cmd)+offsetof(usercmd_t,rightmove),
    waterUpCmd830=offsetof(pmove_t,cmd)+offsetof(usercmd_t,upmove),
    waterPmTime830=offsetof(playerState_t,pm_time),
    waterPmFlags830=offsetof(playerState_t,pm_flags),
    waterOrigin830=offsetof(playerState_t,origin),
    waterVelocity830=offsetof(playerState_t,velocity),
    waterClient830=offsetof(playerState_t,clientNum),
    waterSpeed830=offsetof(playerState_t,speed),
    waterForward830=offsetof(pml_t,forward),
    waterRight830=offsetof(pml_t,right),
    waterGround830=offsetof(pml_t,groundPlane),
    waterNormal830=offsetof(pml_t,groundTrace)+offsetof(trace_t,plane)+offsetof(cplane_t,normal)
};
static qboolean PM_CheckWaterJump(void) {
    vec3_t flatforward, spot;
    int contents;

    if (pm->ps->pm_time || pm->waterlevel != 2) return qfalse;
    flatforward[0] = pml.forward[0];
    flatforward[1] = pml.forward[1];
    flatforward[2] = 0.0f;
    VectorNormalize(flatforward);
    spot[0] = (float)((long double)flatforward[0] * 30.0f + pm->ps->origin[0]);
    spot[1] = (float)((long double)flatforward[1] * 30.0f + pm->ps->origin[1]);
    spot[2] = (float)((long double)flatforward[2] * 30.0f + pm->ps->origin[2] + 4.0f);
    contents = pm->pointcontents(spot, pm->ps->clientNum);
    if (!(contents & CONTENTS_SOLID)) return qfalse;
    spot[2] = (float)((long double)spot[2] + 16.0f);
    /* Reload the callback and player after the preceding callback. */
    contents = pm->pointcontents(spot, pm->ps->clientNum);
    if (contents) return qfalse;
    pm->ps->velocity[0] = (float)((long double)pml.forward[0] * 200.0f);
    pm->ps->velocity[1] = (float)((long double)pml.forward[1] * 200.0f);
    pm->ps->velocity[2] = (float)((long double)pml.forward[2] * 200.0f);
    pm->ps->velocity[2] = 350.0f;
    pm->ps->pm_flags |= PMF_TIME_WATERJUMP;
    pm->ps->pm_time = 2000;
    return qtrue;
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
static qboolean	PM_CheckWaterJump( void ) {
	vec3_t	spot;
	int		cont;
	vec3_t	flatforward;

	if (pm->ps->pm_time) {
		return qfalse;
	}

	// check for water jump
	if ( pm->waterlevel != 2 ) {
		return qfalse;
	}

	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalize (flatforward);

	VectorMA (pm->ps->origin, 30, flatforward, spot);
	spot[2] += 4;
	cont = pm->pointcontents (spot, pm->ps->clientNum );
	if ( !(cont & CONTENTS_SOLID) ) {
		return qfalse;
	}

	spot[2] += 16;
	cont = pm->pointcontents (spot, pm->ps->clientNum );
	if ( cont ) {
		return qfalse;
	}

	// jump out of water
	VectorScale (pml.forward, 200, pm->ps->velocity);
	pm->ps->velocity[2] = 350;

	pm->ps->pm_flags |= PMF_TIME_WATERJUMP;
	pm->ps->pm_time = 2000;

	return qtrue;
}

#endif

/*
==============
PM_CheckProne

Sets mins, maxs, and pm->ps->viewheight
==============
*/
/* TC Windows3000c9a0: ground-supported leg traces and 750ms posture phase. */
static qboolean PM_TCECheckProne(void) {
    playerState_t *ps=pm->ps;
    int time=pm->cmd.serverTime, w;
    trace_t tr;
    vec3_t mins={-13.5f,-13.5f,-24},maxs={13.5f,13.5f,-14.4f},start,end;
    float scale,speed; int i;
    if(!(ps->eFlags & EF_PRONE)) {
        if((ps->eFlags & EF_PRONE_MOVING) && time+pm->pmext->proneTime>750)
            ps->eFlags &= ~(EF_PRONE_MOVING|0x800000);
        if((ps->pm_flags & PMF_LADDER) || ps->persistant[PERS_HWEAPON_USE] ||
           (ps->eFlags & EF_MOUNTEDTANK) || (ps->weaponDelay && ps->weapon==65) ||
           ps->weapon==60 || (ps->eFlags & 0x1000000) ||
           (ps->stats[STAT_TCE_WEAPON_FLAGS]&0x1000) || ps->groundEntityNum==ENTITYNUM_NONE ||
           pml.tceContentRestriction || ps->groundEntityNum<64 || pm->waterlevel>1)return qfalse;
        if((((ps->pm_flags & PMF_DUCKED) && pm->cmd.doubleTap==DT_FORWARD) ||
            (pm->cmd.wbuttons & WBUTTON_PRONE)) && time+pm->pmext->proneTime>750) {
            scale=(ps->stats[STAT_TCE_FLAGS]&0x200)?1.25f:1.0f;
            VectorScale(mins,scale,mins);VectorScale(maxs,scale,maxs);
            start[0]=ps->origin[0]-pml.forward[0]*scale*32;
            start[1]=ps->origin[1]-pml.forward[1]*scale*32;
            start[2]=ps->origin[2]+24;VectorCopy(start,end);end[2]=(start[2]-21.6f)-24;
            pm->trace(&tr,start,mins,maxs,end,ps->clientNum,pm->tracemask);
            if((tr.startsolid && tr.entityNum>=64) || tr.fraction==1)return qfalse;
            VectorCopy(tr.endpos,start);VectorCopy(start,end);end[2]+=21.6f;
            pm->trace(&tr,start,mins,maxs,end,ps->clientNum,pm->tracemask);
            if(!tr.allsolid || tr.entityNum<64) {
                ps->pm_flags|=PMF_DUCKED;ps->eFlags|=EF_PRONE;
                pm->pmext->proneTime=pm->pmext->proneGroundTime=time;
            }
        }
    }
    if((ps->eFlags & EF_PRONE) &&
       (pm->waterlevel>1 || ps->pm_type==PM_DEAD || (ps->eFlags & EF_MOUNTEDTANK) ||
        ps->groundEntityNum==ENTITYNUM_NONE || ps->groundEntityNum<64 ||
        (ps->pm_flags & PMF_LADDER) || pml.tceContentRestriction ||
        ((pm->cmd.doubleTap==DT_BACK || pm->cmd.upmove>10 || pm->cmd.upmove< -10 ||
          (pm->cmd.wbuttons & WBUTTON_PRONE)) && time-pm->pmext->proneTime>750))) {
        VectorCopy(ps->mins,pm->mins);VectorCopy(ps->maxs,pm->maxs);pm->maxs[2]=ps->crouchMaxZ;
        pm->trace(&tr,ps->origin,pm->mins,pm->maxs,ps->origin,ps->clientNum,pm->tracemask);
        if(!tr.allsolid) {
            ps->pm_flags|=PMF_DUCKED;ps->eFlags=(ps->eFlags & ~EF_PRONE)|EF_PRONE_MOVING|0x800000;
            pm->pmext->proneTime=-time;
            if(pm->cmd.upmove>10 && (ps->stats[STAT_TCE_WEAPON_FLAGS]&0x200))ps->stats[STAT_TCE_WEAPON_FLAGS]&=~0x200;
            else if(pm->cmd.upmove< -10 && (ps->persistant[14]&4))ps->stats[STAT_TCE_WEAPON_FLAGS]|=0x600;
            if(ps->weapon==62)PM_BeginWeaponChange(62,31,qfalse);
            pm->pmext->jumpTime=ps->jumpTime=time-650;
        }
    }
    if(!(ps->eFlags & EF_PRONE))return qfalse;
    speed=VectorLength(ps->velocity);
    if(abs(pm->cmd.forwardmove)+abs(pm->cmd.rightmove)<11) {
        if(speed<20)ps->eFlags&=~EF_PRONE_MOVING;
    } else if(speed>20 && !(ps->eFlags & EF_PRONE_MOVING)) {
        ps->eFlags|=EF_PRONE_MOVING;w=ps->weapon;
        if(w==57)PM_BeginWeaponChange(57,25,qfalse);
        else if(w==58)PM_BeginWeaponChange(58,32,qfalse);
        else if(w==59)PM_BeginWeaponChange(59,33,qfalse);
    }
    if(time-pm->pmext->proneTime<750)ps->eFlags|=EF_PRONE_MOVING|0x800000;
    else ps->eFlags&=~0x800000;
    VectorCopy(ps->mins,pm->mins);VectorCopy(ps->maxs,pm->maxs);
    i=(ps->stats[STAT_TCE_FLAGS]&0x200)?-10:-8;
    pm->maxs[2]=ps->maxs[2]-ps->standViewHeight-i;ps->viewheight=i;
    return qtrue;
}

/* TC qagame posture/footstep controllers 20034cf0 and 20037720.
 * Operand-level Windows x87 / integer schedule, mapped to native structures. */
#if defined(_MSC_VER) && defined(_M_IX86)
typedef char posture832TraceLayout[(sizeof(trace_t)==56 && offsetof(trace_t,allsolid)==0 && offsetof(trace_t,startsolid)==4 && offsetof(trace_t,fraction)==8 && offsetof(trace_t,endpos)==12 && offsetof(trace_t,entityNum)==52) ? 1 : -1];
static qboolean PM_CheckProne(void) {
    trace_t trace;
    vec3_t mins = {-13.5f, -13.5f, -24.0f}, maxs = {13.5f, 13.5f, -14.4f};
    vec3_t start, end;
    float scale = 1.0f;
    long double speed, maxZ;
    int axis, movement, weapon;
    qboolean leave;

    if (!(pm->ps->eFlags & EF_PRONE)) {
        if ((pm->ps->eFlags & EF_PRONE_MOVING) &&
            PM_ProneTimeValue((unsigned)pm->pmext->proneTime + (unsigned)pm->cmd.serverTime) > 750) {
            pm->ps->eFlags &= ~EF_PRONE_MOVING;
            pm->ps->eFlags &= ~0x800000;
        }
        if (pm->ps->pm_flags & PMF_LADDER) return qfalse;
        if (pm->ps->persistant[PERS_HWEAPON_USE]) return qfalse;
        if (pm->ps->eFlags & EF_MOUNTEDTANK) return qfalse;
        if (pm->ps->weaponDelay && pm->ps->weapon == 65) return qfalse;
        if (pm->ps->weapon == 60) return qfalse;
        if (pm->ps->eFlags & 0x1000000) return qfalse;
        if (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x1000) return qfalse;
        if (pm->ps->groundEntityNum == ENTITYNUM_NONE) return qfalse;
        if (pml.tceContentRestriction) return qfalse;
        if (pm->ps->groundEntityNum < MAX_CLIENTS) return qfalse;
        if (pm->waterlevel > 1) return qfalse;
        if ((((pm->ps->pm_flags & PMF_DUCKED) && pm->cmd.doubleTap == DT_FORWARD) ||
            (pm->cmd.wbuttons & WBUTTON_PRONE)) &&
            PM_ProneTimeValue((unsigned)pm->pmext->proneTime + (unsigned)pm->cmd.serverTime) > 750) {
            if (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) {
                for (axis = 0; axis < 3; ++axis) mins[axis] = (float)((long double)mins[axis] * 1.25f);
                for (axis = 0; axis < 3; ++axis) maxs[axis] = (float)((long double)maxs[axis] * 1.25f);
                scale = 1.25f;
            }
            start[0] = (float)((long double)pm->ps->origin[0] - (long double)pml.forward[0] * scale * 32.0f);
            start[1] = (float)((long double)pm->ps->origin[1] - (long double)scale * pml.forward[1] * 32.0f);
            start[2] = (float)((long double)pm->ps->origin[2] + 24.0f);
            end[0] = start[0];
            end[1] = start[1];
            end[2] = (float)((long double)start[2] - 21.6f - 24.0f);
            pm->trace(&trace, start, mins, maxs, end, pm->ps->clientNum, pm->tracemask);
            if (trace.startsolid && trace.entityNum >= MAX_CLIENTS) return qfalse;
            if (trace.fraction == 1.0f || trace.fraction != trace.fraction) return qfalse;
            VectorCopy(trace.endpos, start);
            VectorCopy(start, end);
            end[2] = (float)((long double)start[2] + 21.6f);
            pm->trace(&trace, start, mins, maxs, end, pm->ps->clientNum, pm->tracemask);
            if (!trace.allsolid || trace.entityNum < MAX_CLIENTS) {
                pm->ps->pm_flags |= PMF_DUCKED;
                pm->ps->eFlags |= EF_PRONE;
                pm->pmext->proneTime = pm->cmd.serverTime;
                pm->pmext->proneGroundTime = pm->cmd.serverTime;
            }
        }
    }
    if (pm->ps->eFlags & EF_PRONE) {
        leave = pm->waterlevel > 1 || pm->ps->pm_type == PM_DEAD ||
            (pm->ps->eFlags & EF_MOUNTEDTANK) || pm->ps->groundEntityNum == ENTITYNUM_NONE ||
            pm->ps->groundEntityNum < MAX_CLIENTS || (pm->ps->pm_flags & PMF_LADDER) ||
            pml.tceContentRestriction;
        if (!leave && (pm->cmd.doubleTap == DT_BACK || pm->cmd.upmove > 10 ||
            pm->cmd.upmove < -10 || (pm->cmd.wbuttons & WBUTTON_PRONE))) {
            leave = PM_ProneTimeValue((unsigned)pm->cmd.serverTime - (unsigned)pm->pmext->proneTime) > 750;
        }
        if (leave) {
            pm->mins[0] = pm->ps->mins[0];
            pm->mins[1] = pm->ps->mins[1];
            pm->maxs[0] = pm->ps->maxs[0];
            pm->maxs[1] = pm->ps->maxs[1];
            pm->mins[2] = pm->ps->mins[2];
            pm->maxs[2] = pm->ps->crouchMaxZ;
            pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin,
                pm->ps->clientNum, pm->tracemask);
            if (!trace.allsolid) {
                pm->ps->pm_flags |= PMF_DUCKED;
                pm->ps->eFlags &= ~EF_PRONE;
                pm->ps->eFlags |= EF_PRONE_MOVING;
                pm->ps->eFlags |= 0x800000;
                pm->pmext->proneTime = PM_ProneTimeValue(0u - (unsigned)pm->cmd.serverTime);
                if (pm->cmd.upmove > 10 && (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x200)) {
                    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x200;
                } else if (pm->cmd.upmove < -10 && (pm->ps->persistant[14] & 4)) {
                    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x200;
                    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x400;
                }
                if (pm->ps->weapon == 62) PM_BeginWeaponChange(62, 31, qfalse);
                pm->pmext->jumpTime = PM_ProneTimeValue((unsigned)pm->cmd.serverTime - 650u);
                pm->ps->jumpTime = PM_ProneTimeValue((unsigned)pm->cmd.serverTime - 650u);
            }
        }
    }
    if (!(pm->ps->eFlags & EF_PRONE)) return qfalse;
    speed = TCE_VectorLengthValue(pm->ps->velocity);
    movement = abs((int)pm->cmd.rightmove) + abs((int)pm->cmd.forwardmove);
    if (movement < 11) {
        if ((speed < 20.0L || speed != speed) && (pm->ps->eFlags & EF_PRONE_MOVING)) {
            pm->ps->eFlags &= ~EF_PRONE_MOVING;
        }
    } else if (speed > 20.0L && !(pm->ps->eFlags & EF_PRONE_MOVING)) {
        pm->ps->eFlags |= EF_PRONE_MOVING;
        weapon = pm->ps->weapon;
        if (weapon == 57) PM_BeginWeaponChange(57, 25, qfalse);
        else if (weapon == 58) PM_BeginWeaponChange(58, 32, qfalse);
        else if (weapon == 59) PM_BeginWeaponChange(59, 33, qfalse);
    }
    if (PM_ProneTimeValue((unsigned)pm->cmd.serverTime - (unsigned)pm->pmext->proneTime) < 750) {
        pm->ps->eFlags |= EF_PRONE_MOVING;
        pm->ps->eFlags |= 0x800000;
    } else {
        pm->ps->eFlags &= ~0x800000;
    }
    pm->mins[0] = pm->ps->mins[0];
    pm->mins[1] = pm->ps->mins[1];
    pm->maxs[0] = pm->ps->maxs[0];
    pm->maxs[1] = pm->ps->maxs[1];
    pm->mins[2] = pm->ps->mins[2];
    maxZ = (long double)pm->ps->maxs[2] - pm->ps->standViewHeight;
    if (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) {
        pm->maxs[2] = (float)(maxZ + 10.0f);
        pm->ps->viewheight = -10;
    } else {
        pm->maxs[2] = (float)(maxZ + 8.0f);
        pm->ps->viewheight = -8;
    }
    return qtrue;
}
#else
static qboolean PM_CheckProne(void) {
    return PM_TCECheckProne();
}
#endif

/*
=============
PM_CheckDodge
=============
*/
static qboolean PM_CheckDodge( void ) {
	// JPW NERVE -- jumping in multiplayer uses and requires sprint juice (to prevent turbo skating, sprint + jumps)
		// don't allow jump accel
 		//if (pm->cmd.serverTime - pm->pmext->jumpTime < 850)
		//	return qfalse;

		// don't allow if player tired 
//		if (pm->pmext->sprintTime < 2500) // JPW pulled this per id request; made airborne jumpers wildly inaccurate with gunfire to compensate
//			return qfalse;
	// jpw

	// Disabled for now
	return qfalse;

/*	if ( pm->pmext->sprintTime < 3000 )
		return qfalse;

	if( pm->cmd.serverTime - pm->pmext->dodgeTime < 350 )
		return qtrue; 	// Already dodging

	if ( pm->ps->pm_flags & PMF_RESPAWNED || pm->ps->pm_flags & PMF_DUCKED || pm->ps->eFlags & EF_PRONE ) {
		return qfalse;		// don't allow dodge until all buttons are up, player is prone or crouching
	}

	if ( pm->cmd.doubleTap != DT_MOVELEFT && pm->cmd.doubleTap != DT_MOVERIGHT ) {
		// no dodge issued
		return qfalse;
	}

	// must wait for jump to be released
	//if ( pm->ps->pm_flags & PMF_JUMP_HELD ) {
	//	// clear upmove so cmdscale doesn't lower running speed
	//	pm->cmd.upmove = 0;
	//	return qfalse;
	//}

	pml.groundPlane = qfalse;		// jumping away
	pml.walking = qfalse;
	//pm->ps->pm_flags |= PMF_JUMP_HELD;

	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pm->ps->velocity[2] = 130;
	PM_AddEvent( EV_JUMP );
	
	//if ( pm->cmd.forwardmove >= 0 ) {
	//	BG_AnimScriptEvent( pm->ps, ANIM_ET_JUMP, qfalse, qtrue );
	//	pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
	//} else {
	//	BG_AnimScriptEvent( pm->ps, ANIM_ET_JUMPBK, qfalse, qtrue );
	//	pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
	//}

	pm->pmext->dtmove = pm->cmd.doubleTap;
	pm->pmext->dodgeTime = pm->cmd.serverTime;
	pm->pmext->jumpTime = pm->cmd.serverTime - 200;	// Arnout: prevent bunnyhopping
	pm->ps->jumpTime = pm->cmd.serverTime - 200;	// Arnout: prevent bunnyhopping

	pm->pmext->sprintTime -= 3000;

	return qtrue;*/
}

//============================================================================

/*
===================
PM_WaterJumpMove

Flying out of the water
===================
*/
static void PM_WaterJumpMove( void ) {
	int falling;
	// waterjump has no control, but falls

	PM_StepSlideMove( qtrue );

#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
    {
        long double gravityStep = (long double)pm->ps->gravity * pml.frametime;
        float verticalSpeed;

        pm->ps->velocity[2] = (float)((long double)pm->ps->velocity[2] - gravityStep);
        verticalSpeed = pm->ps->velocity[2];
        /* Native C0 also cancels the timer for an unordered stored speed. */
        falling = verticalSpeed < 0.0f || verticalSpeed != verticalSpeed;
    }
#else
	pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
    falling = pm->ps->velocity[2] < 0;
#endif
	if (falling) {
		// cancel as soon as we are falling down again
		pm->ps->pm_flags &= ~PMF_ALL_TIMES;
		pm->ps->pm_time = 0;
	}
}

/*
===================
PM_WaterMove

===================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static void PM_WaterMove(void) {
    vec3_t wishvel, wishdir;
    long double scale, limit, dot;
    volatile float wishspeed, speed;
    int forwardMove, rightMove, i;

    if (PM_CheckWaterJump()) {
        PM_WaterJumpMove();
        return;
    }
    PM_Friction();
    scale = PM_CmdScale(&pm->cmd);
    if (scale == 0.0L || scale != scale) {
        wishvel[0] = 0.0f;
        wishvel[1] = 0.0f;
        wishvel[2] = -60.0f;
    } else {
        forwardMove = pm->cmd.forwardmove;
        rightMove = pm->cmd.rightmove;
        for (i = 0; i < 3; ++i) {
            long double right = (long double)rightMove * pml.right[i];
            long double forward = (long double)forwardMove * pml.forward[i];
            wishvel[i] = (float)((right + forward) * scale);
        }
        wishvel[2] = (float)(scale * pm->cmd.upmove + wishvel[2]);
    }
    VectorCopy(wishvel, wishdir);
    wishspeed = (float)TCE_VectorNormalizeValue(wishdir);
    if (pm->watertype == CONTENTS_SLIME) {
        limit = (long double)pm->ps->speed * pm_slagSwimScale;
        if ((long double)wishspeed > limit) wishspeed = (float)limit;
        PM_Accelerate(wishdir, wishspeed, pm_slagaccelerate);
    } else {
        limit = (long double)pm->ps->speed * pm_waterSwimScale;
        if ((long double)wishspeed > limit) wishspeed = (float)limit;
        PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);
    }
    if (pml.groundPlane) {
        dot = (long double)pml.groundTrace.plane.normal[2] * pm->ps->velocity[2];
        dot += (long double)pml.groundTrace.plane.normal[1] * pm->ps->velocity[1];
        dot += (long double)pml.groundTrace.plane.normal[0] * pm->ps->velocity[0];
        /* Native C0 enters the slope adjustment for negative or unordered dot. */
        if (dot < 0.0L || dot != dot) {
            speed = (float)TCE_VectorLengthValue(pm->ps->velocity);
            PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                pm->ps->velocity, OVERCLIP);
            (void)TCE_VectorNormalizeValue(pm->ps->velocity);
            pm->ps->velocity[0] = (float)((long double)speed * pm->ps->velocity[0]);
            pm->ps->velocity[1] = (float)((long double)speed * pm->ps->velocity[1]);
            pm->ps->velocity[2] = (float)((long double)speed * pm->ps->velocity[2]);
        }
    }
    PM_SlideMove(qfalse);
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
static void PM_WaterMove( void ) {
	int		i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	float	scale;
	float	vel;

	if ( PM_CheckWaterJump() ) {
		PM_WaterJumpMove();
		return;
	}
#if 0
	// jump = head for surface
	if ( pm->cmd.upmove >= 10 ) {
		if (pm->ps->velocity[2] > -300) {
			if ( pm->watertype == CONTENTS_WATER ) {
				pm->ps->velocity[2] = 100;
			} else if (pm->watertype == CONTENTS_SLIME) {
				pm->ps->velocity[2] = 80;
			} else {
				pm->ps->velocity[2] = 50;
			}
		}
	}
#endif
	PM_Friction ();

	scale = PM_CmdScale( &pm->cmd );
	//
	// user intentions
	//
	if ( !scale ) {
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = -60;		// sink towards bottom
//		wishvel[2] = -10;	//----(SA)	mod for DM
	} else {
		for (i=0 ; i<3 ; i++)
			wishvel[i] = (pm->cmd.forwardmove * pml.forward[i] + pm->cmd.rightmove * pml.right[i]) * scale;

		wishvel[2] += scale * pm->cmd.upmove;
	}

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if ( pm->watertype == CONTENTS_SLIME ) {	//----(SA)	slag
		if ( wishspeed > pm->ps->speed * pm_slagSwimScale ) {
			wishspeed = pm->ps->speed * pm_slagSwimScale;
		}

		PM_Accelerate (wishdir, wishspeed, pm_slagaccelerate);
	}
	else {
		if ( wishspeed > pm->ps->speed * pm_waterSwimScale ) {
			wishspeed = pm->ps->speed * pm_waterSwimScale;
		}

		PM_Accelerate (wishdir, wishspeed, pm_wateraccelerate);
	}


	// make sure we can go up slopes easily under water
	if ( pml.groundPlane && DotProduct( pm->ps->velocity, pml.groundTrace.plane.normal ) < 0 ) {
		vel = VectorLength(pm->ps->velocity);
		// slide along the ground plane
		PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, 
			pm->ps->velocity, OVERCLIP );

		VectorNormalize(pm->ps->velocity);
		VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
	}

	PM_SlideMove( qfalse );
}


#endif
// TTimo gcc: defined but not used
#if 0
/*
===================
PM_InvulnerabilityMove

Only with the invulnerability powerup
===================
*/
static void PM_InvulnerabilityMove( void ) {
	pm->cmd.forwardmove = 0;
	pm->cmd.rightmove = 0;
	pm->cmd.upmove = 0;
	VectorClear(pm->ps->velocity);
}
#endif

/*
===================
PM_FlyMove

Only with the flight powerup
===================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static void PM_FlyMove(void) {
    long double scale;
    volatile float wishspeed;
    vec3_t wishvel, wishdir;
    int forwardMove, rightMove, i;

    PM_Friction();
    scale = PM_CmdScale(&pm->cmd);
    /* Native C3 takes the zero-input path for zero or unordered scale. */
    if (scale == 0.0L || scale != scale) {
        wishvel[0] = 0.0f;
        wishvel[1] = 0.0f;
        wishvel[2] = 0.0f;
    } else {
        forwardMove = pm->cmd.forwardmove;
        rightMove = pm->cmd.rightmove;
        for (i = 0; i < 3; ++i) {
            long double right = (long double)rightMove * pml.right[i];
            long double forward = (long double)forwardMove * pml.forward[i];
            wishvel[i] = (float)((right + forward) * scale);
        }
        wishvel[2] = (float)(scale * pm->cmd.upmove + wishvel[2]);
    }
    VectorCopy(wishvel, wishdir);
    wishspeed = (float)TCE_VectorNormalizeValue(wishdir);
    PM_Accelerate(wishdir, wishspeed, pm_flyaccelerate);
    PM_StepSlideMove(qfalse);
}
#else
static void PM_FlyMove( void ) {
	int		i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	float	scale;

	// normal slowdown
	PM_Friction ();

	scale = PM_CmdScale( &pm->cmd );

	//
	// user intentions
	//
	if ( !scale ) {
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = 0;
	} else {
		for (i=0 ; i<3 ; i++) {
			wishvel[i] = (pm->cmd.forwardmove*pml.forward[i] + pm->cmd.rightmove*pml.right[i])*scale;
		}

		wishvel[2] += scale * pm->cmd.upmove;
	}

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	PM_Accelerate (wishdir, wishspeed, pm_flyaccelerate);

	PM_StepSlideMove( qfalse );
}

#endif


/*
===================
PM_AirMove

===================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static void PM_AirMove(void) {
    vec3_t wishvel, wishdir;
    usercmd_t cmd;
    float forwardMove, rightMove;
    volatile float scale, wishspeed;
    unsigned elapsedBits;
    int elapsed, i;

    pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
    pm->ps->stats[STAT_TCE_FLAGS] |= 4;
    PM_Friction();
    forwardMove = pm->cmd.forwardmove;
    rightMove = pm->cmd.rightmove;
    elapsedBits = (unsigned)pm->cmd.serverTime - (unsigned)pm->pmext->dodgeTime;
    memcpy(&elapsed, &elapsedBits, sizeof(elapsed));
    if (elapsed < 350) {
        pml.forward[2] = forwardMove = 0.0f;
        rightMove = pm->pmext->dtmove == DT_MOVELEFT ? -2070.0f : 2070.0f;
        scale = 1.0f;
    } else {
        memcpy(&cmd, &pm->cmd, sizeof(cmd));
        scale = PM_CmdScale(&cmd);
        pml.forward[2] = 0.0f;
        pml.right[2] = 0.0f;
    }
    (void)TCE_VectorNormalizeValue(pml.forward);
    (void)TCE_VectorNormalizeValue(pml.right);
    for (i = 0; i < 2; ++i) {
        long double right = (long double)rightMove * pml.right[i];
        long double forward = (long double)forwardMove * pml.forward[i];
        wishvel[i] = (float)(right + forward);
    }
    wishdir[0] = wishvel[0];
    wishdir[1] = wishvel[1];
    wishdir[2] = 0.0f;
    wishspeed = (float)(TCE_VectorNormalizeValue(wishdir) * scale);
    PM_Accelerate(wishdir, wishspeed, pm_airaccelerate);
    if (pml.groundPlane) {
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
            pm->ps->velocity, OVERCLIP);
    }
    if (pm->ps->eFlags & EF_PRONE) PM_StepSlideMoveProne(qtrue);
    else PM_StepSlideMove(qtrue);
    PM_SetMovementDir();
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
static void PM_AirMove( void ) {
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		scale;
	usercmd_t	cmd;
    unsigned elapsedBits;
    int elapsed;

	pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
	pm->ps->stats[STAT_TCE_FLAGS] |= 4;
	PM_Friction();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;

	// project moves down to flat plane
    elapsedBits = (unsigned)pm->cmd.serverTime - (unsigned)pm->pmext->dodgeTime;
    memcpy(&elapsed, &elapsedBits, sizeof(elapsed));
    if (elapsed < 350) {
		pml.forward[2] = fmove = 0;
		smove = pm->pmext->dtmove == DT_MOVELEFT ? -2070 : 2070;
		scale = 1.f;
	} else {
		cmd = pm->cmd;
		scale = PM_CmdScale( &cmd );

	// Ridah, moved this down, so we use the actual movement direction
		// set the movementDir so clients can rotate the legs for strafing
	//	PM_SetMovementDir();


		pml.forward[2] = 0;
		pml.right[2] = 0;
	}
	VectorNormalize (pml.forward);
	VectorNormalize (pml.right);

	for ( i = 0 ; i < 2 ; i++ ) {
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	}
	wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	wishspeed *= scale;

	// not on ground, so little effect on velocity
	PM_Accelerate (wishdir, wishspeed, pm_airaccelerate);

	// we may have a ground plane that is very steep, even
	// though we don't have a groundentity
	// slide along the steep plane
	if ( pml.groundPlane ) {
		PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, 
			pm->ps->velocity, OVERCLIP );
	}

#if 0
	//ZOID:  If we are on the grapple, try stair-stepping
	//this allows a player to use the grapple to pull himself
	//over a ledge
	if (pm->ps->pm_flags & PMF_GRAPPLE_PULL)
		PM_StepSlideMove ( qtrue );
	else
		PM_SlideMove ( qtrue );
#endif

	if (pm->ps->eFlags & EF_PRONE) PM_StepSlideMoveProne(qtrue);
	else PM_StepSlideMove(qtrue);

// Ridah, moved this down, so we use the actual movement direction
	// set the movementDir so clients can rotate the legs for strafing
	PM_SetMovementDir();
}

#endif


/*
===================
PM_WalkMove

===================
*/
/* Windows original WalkMove: TCE ground restrictions and jump stamina.
 * Explicit float stores retain native rounding boundaries; FPU flags and
 * compiler extended precision remain separately tracked reconstruction work. */
#if defined(_MSC_VER) && defined(_M_IX86)
typedef char walkCommandLayout[(sizeof(usercmd_t) == 28 && offsetof(usercmd_t, serverTime) == 0) ? 1 : -1];
static void PM_WalkMove(void) {
    vec3_t wishvel, wishdir;
    usercmd_t cmd;
    float forwardMove, rightMove, accelerate;
    volatile float scale, wishspeed, speed;
    long double groundDot, limit;
    unsigned elapsedBits;
    int elapsed, i;

    if (pm->waterlevel > 2) {
        groundDot = (long double)pml.groundTrace.plane.normal[2] * pml.forward[2];
        groundDot += (long double)pml.groundTrace.plane.normal[1] * pml.forward[1];
        groundDot += (long double)pml.groundTrace.plane.normal[0] * pml.forward[0];
        if (groundDot > 0.0L) {
            PM_WaterMove();
            return;
        }
    }
    if (PM_CheckJump()) {
        if (pm->waterlevel > 1) PM_WaterMove();
        else PM_AirMove();
        elapsedBits = (unsigned)pm->cmd.serverTime - (unsigned)pm->pmext->jumpTime;
        memcpy(&elapsed, &elapsedBits, sizeof(elapsed));
        if (elapsed >= 850) {
            if (pm->pmext->sprintTime < 1500) {
                pm->pmext->sprintTime = 0;
            } else {
                float factor = pm->ps->holdable[9] == 4 ? 0.5555555820465088f :
                    pm->ps->holdable[9] == 5 ? 0.5f : 0.625f;
                pm->pmext->sprintTime = TCE_TruncateNativeInteger(
                    (long double)pm->pmext->sprintTime * factor);
            }
            if (pm->pmext->sprintTime < 0) pm->pmext->sprintTime = 0;
            pm->pmext->jumpTime = pm->cmd.serverTime;
        }
        pm->ps->jumpTime = pm->cmd.serverTime;
        return;
    }
    if (pm->waterlevel <= 1 && PM_CheckDodge()) {
        PM_AirMove();
        return;
    }
    PM_Friction();
    forwardMove = pm->cmd.forwardmove;
    rightMove = pm->cmd.rightmove;
    memcpy(&cmd, &pm->cmd, sizeof(cmd));
    scale = PM_CmdScale(&cmd);
    pml.forward[2] = 0.0f;
    pml.right[2] = 0.0f;
    if (!pml.tceContentRestriction || !pml.tceGroundExtension) {
        PM_ClipVelocity(pml.forward, pml.groundTrace.plane.normal, pml.forward, OVERCLIP);
        PM_ClipVelocity(pml.right, pml.groundTrace.plane.normal, pml.right, OVERCLIP);
    }
    (void)TCE_VectorNormalizeValue(pml.forward);
    (void)TCE_VectorNormalizeValue(pml.right);
    for (i = 0; i < 3; ++i) {
        long double right = (long double)rightMove * pml.right[i];
        long double forward = (long double)forwardMove * pml.forward[i];
        wishvel[i] = (float)(right + forward);
    }
    VectorCopy(wishvel, wishdir);
    if (pml.tceGroundExtension && pml.tceContentRestriction >= 1 &&
        pml.tceContentRestriction <= 3) {
        long double horizontal = (long double)wishvel[1] * wishvel[1];
        float factor;
        horizontal += (long double)wishvel[0] * wishvel[0];
        horizontal = sqrtl(horizontal);
        if (pml.tceContentRestriction == 1) {
            factor = 0.7072135806083679f;
            horizontal = -horizontal;
        } else if (pml.tceContentRestriction == 2) {
            factor = 0.4472271800041199f;
            horizontal *= -2.0L;
        } else {
            factor = 0.2425418347120285f;
            horizontal *= -4.0L;
        }
        wishdir[0] = (float)((long double)wishvel[0] * factor);
        wishdir[1] = (float)((long double)wishvel[1] * factor);
        wishdir[2] = (float)(horizontal * factor);
    }
    wishspeed = (float)(TCE_VectorNormalizeValue(wishdir) * scale);
    if (pm->ps->eFlags & (EF_PRONE | EF_PRONE_MOVING)) {
        limit = (long double)pm->ps->speed * pm_proneSpeedScale;
        if ((long double)wishspeed > limit) wishspeed = (float)limit;
    } else if (pm->ps->pm_flags & PMF_DUCKED) {
        limit = (long double)pm->ps->speed * pm->ps->crouchSpeedScale;
        if ((long double)wishspeed > limit) wishspeed = (float)limit;
    }
    if (pm->waterlevel) {
        long double depth = (long double)pm->waterlevel * (long double)0.33333333333333331;
        float swimScale = pm->watertype == CONTENTS_SLIME ? pm_slagSwimScale : pm_waterSwimScale;
        limit = (1.0L - depth * (1.0L - swimScale)) * pm->ps->speed;
        if ((long double)wishspeed > limit) wishspeed = (float)limit;
    }
    if (pml.tceContentRestriction) {
        /* Native C3 accepts unordered lean as well as zero. */
        float factor = !(pm->cmd.buttons & BUTTON_WALKING) &&
            (pm->ps->leanf == 0.0f || TCE_FloatIsNaN(pm->ps->leanf)) &&
            !(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) ? 0.5f : 0.25f;
        limit = (long double)pm->ps->speed * factor;
        if ((long double)wishspeed > limit) wishspeed = (float)limit;
    }
    accelerate = (pml.groundTrace.surfaceFlags & SURF_SLICK) ||
        (pm->ps->pm_flags & PMF_TIME_KNOCKBACK) ? pm_airaccelerate : pm_accelerate;
    PM_Accelerate(wishdir, wishspeed, accelerate);
    if ((pml.groundTrace.surfaceFlags & SURF_SLICK) ||
        (pm->ps->pm_flags & PMF_TIME_KNOCKBACK)) {
        pm->ps->velocity[2] = (float)((long double)pm->ps->velocity[2] -
            (long double)pm->ps->gravity * pml.frametime);
    }
    if ((pml.groundTrace.surfaceFlags & 0xff000000) == 0x0d000000 &&
        pm->ps->stats[STAT_HEALTH] > 0) pm->ps->eFlags |= EF_BREATH;
    else pm->ps->eFlags &= ~EF_BREATH;
    speed = (float)TCE_VectorLengthValue(pm->ps->velocity);
    if (!pml.tceGroundExtension) {
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
            pm->ps->velocity, OVERCLIP);
    }
    if ((pm->ps->velocity[0] == 0.0f || TCE_FloatIsNaN(pm->ps->velocity[0])) &&
        (pm->ps->velocity[1] == 0.0f || TCE_FloatIsNaN(pm->ps->velocity[1]))) {
        if (pm->ps->eFlags & EF_PRONE) pm->pmext->proneGroundTime = pm->cmd.serverTime;
        if (!pml.tceGroundExtension) return;
    }
    (void)TCE_VectorNormalizeValue(pm->ps->velocity);
    for (i = 0; i < 3; ++i) {
        pm->ps->velocity[i] = (float)((long double)speed * pm->ps->velocity[i]);
    }
    if (pm->ps->eFlags & EF_PRONE) PM_StepSlideMoveProne(qfalse);
    else PM_StepSlideMove(qfalse);
    PM_SetMovementDir();
}
#else
static void PM_WalkMove( void ) {
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		scale;
	usercmd_t	cmd;
	float		accelerate;
	float		vel;
//	float botBonus = 1.0;

/*#ifdef CGAMEDLL
	int gametype = cg_gameType.integer;
#elif GAMEDLL
	int gametype = g_gametype.integer;
#endif*/

	if ( pm->waterlevel > 2 && DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0 ) {
		// begin swimming
		PM_WaterMove();
		return;
	}

	if ( PM_CheckJump () ) {
		// jumped away
		if ( pm->waterlevel > 1 ) {
			PM_WaterMove();
		} else {
			PM_AirMove();
		}

		if (!(pm->cmd.serverTime - pm->pmext->jumpTime < 850)) {

			if (pm->pmext->sprintTime < 1500) pm->pmext->sprintTime = 0;
            else pm->pmext->sprintTime = (int)(pm->pmext->sprintTime *
                (pm->ps->holdable[9] == 4 ? 0.5555555820465088f :
                 pm->ps->holdable[9] == 5 ? 0.5f : 0.625f));
			if (pm->pmext->sprintTime < 0)
				pm->pmext->sprintTime = 0;

			pm->pmext->jumpTime = pm->cmd.serverTime;
		}

		// JPW NERVE
		pm->ps->jumpTime = pm->cmd.serverTime;	// Arnout: NOTE : TEMP DEBUG
		
		return;
	} else/* if( !PM_CheckProne() )*/ {
		if( pm->waterlevel <= 1 && PM_CheckDodge () ) {
			PM_AirMove();
			return;
		}
	}

	/*if( pm->waterlevel <= 1 && PM_CheckDodge () ) {
		PM_AirMove();
		return;
	}*/

	PM_Friction ();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );

// Ridah, moved this down, so we use the actual movement direction
	// set the movementDir so clients can rotate the legs for strafing
//	PM_SetMovementDir();

	// project moves down to flat plane
	pml.forward[2] = 0;
	pml.right[2] = 0;

	// project the forward and right directions onto the ground plane
    if (!pml.tceContentRestriction || !pml.tceGroundExtension) {
        PM_ClipVelocity(pml.forward, pml.groundTrace.plane.normal, pml.forward, OVERCLIP);
        PM_ClipVelocity(pml.right, pml.groundTrace.plane.normal, pml.right, OVERCLIP);
    }
	//
	VectorNormalize (pml.forward);
	VectorNormalize (pml.right);

	for ( i = 0 ; i < 3 ; i++ ) {
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	}
	// when going up or down slopes the wish velocity should Not be zero
//	wishvel[2] = 0;

    if (pml.tceGroundExtension) {
        float h = sqrt(wishvel[0]*wishvel[0] + wishvel[1]*wishvel[1]);
        float factor = 1.0f;
        if (pml.tceContentRestriction == 1) { factor = 0.7072135806083679f; wishvel[2] = -h * factor; }
        else if (pml.tceContentRestriction == 2) { factor = 0.4472271800041199f; wishvel[2] = h * -2.0f * factor; }
        else if (pml.tceContentRestriction == 3) { factor = 0.2425418347120285f; wishvel[2] = h * -4.0f * factor; }
        wishvel[0] *= factor; wishvel[1] *= factor;
    }
	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	wishspeed *= scale;

	// clamp the speed lower if prone
	if ( pm->ps->eFlags & (EF_PRONE | EF_PRONE_MOVING) ) {
		if ( wishspeed > pm->ps->speed * pm_proneSpeedScale ) {
			wishspeed = pm->ps->speed * pm_proneSpeedScale;
		}
	} else if ( pm->ps->pm_flags & PMF_DUCKED ) { // clamp the speed lower if ducking
		/*if ( wishspeed > pm->ps->speed * pm_duckScale ) {
			wishspeed = pm->ps->speed * pm_duckScale;
		}*/
		if ( wishspeed > pm->ps->speed * pm->ps->crouchSpeedScale ) {
			wishspeed = pm->ps->speed * pm->ps->crouchSpeedScale;
		}
	}

	// clamp the speed lower if wading or walking on the bottom
	if ( pm->waterlevel ) {
		float	waterScale;

		waterScale = pm->waterlevel / 3.0;
		if ( pm->watertype == CONTENTS_SLIME )	//----(SA)	slag
			waterScale = 1.0 - ( 1.0 - pm_slagSwimScale ) * waterScale;
		else
			waterScale = 1.0 - ( 1.0 - pm_waterSwimScale ) * waterScale;

		if ( wishspeed > pm->ps->speed * waterScale ) {
			wishspeed = pm->ps->speed * waterScale;
		}
	}

    if (pml.tceContentRestriction) {
        float limit = pm->ps->speed *
            (!(pm->cmd.buttons & BUTTON_WALKING) && pm->ps->leanf == 0 &&
             !(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) ? 0.5f : 0.25f);
        if (wishspeed > limit) wishspeed = limit;
    }
	// when a player gets hit, they temporarily lose
	// full control, which allows them to be moved a bit
	if ( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK ) {
		accelerate = pm_airaccelerate;
	} 
	else {
		accelerate = pm_accelerate;
	}

	PM_Accelerate (wishdir, wishspeed, accelerate);

	//Com_Printf("velocity = %1.1f %1.1f %1.1f\n", pm->ps->velocity[0], pm->ps->velocity[1], pm->ps->velocity[2]);
	//Com_Printf("velocity1 = %1.1f\n", VectorLength(pm->ps->velocity));

	if ( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK ) {
		pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
	} 
	else {
		// don't reset the z velocity for slopes
		//pm->ps->velocity[2] = 0;
	}

//----(SA)	added
	// show breath when standing on 'snow' surfaces
	if ((pml.groundTrace.surfaceFlags & 0xff000000) == 0x0d000000 && pm->ps->stats[STAT_HEALTH] > 0)
		pm->ps->eFlags |= EF_BREATH;
	else
		pm->ps->eFlags &= ~EF_BREATH;
//----(SA)	end

	vel = VectorLength(pm->ps->velocity);

	// slide along the ground plane
	if (!pml.tceGroundExtension) PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity, OVERCLIP );

	// don't do anything if standing still
	if (!pm->ps->velocity[0] && !pm->ps->velocity[1]) {
		if( pm->ps->eFlags & EF_PRONE ) {
			pm->pmext->proneGroundTime = pm->cmd.serverTime;
		}
		if (!pml.tceGroundExtension) return;
	}

	// don't decrease velocity when going up or down a slope
	VectorNormalize(pm->ps->velocity);
	VectorScale(pm->ps->velocity, vel, pm->ps->velocity);

	if (pm->ps->eFlags & EF_PRONE) PM_StepSlideMoveProne(qfalse);
	else PM_StepSlideMove(qfalse);

// Ridah, moved this down, so we use the actual movement direction
	// set the movementDir so clients can rotate the legs for strafing
	PM_SetMovementDir();
}
#endif


/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove( void ) {
	float	forward;
    int stopDead;

	if ( !pml.walking ) {
		return;
	}

	// extra friction

#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
    {
        long double remaining = TCE_VectorLengthValue(pm->ps->velocity) - 20.0f;
        volatile float storedForward = (float)remaining;

        /* Compare the held value, then use the stored float for scaling. */
        forward = storedForward;
#if defined(__linux__)
        /* Inlined ELF path scales an unordered remaining speed instead of stopping. */
        stopDead = remaining <= 0.0L;
#else
        stopDead = remaining <= 0.0L || remaining != remaining;
#endif
    }
#else
	forward = VectorLength (pm->ps->velocity);
	forward -= 20;
    stopDead = forward <= 0;
#endif
	if ( stopDead ) {
        pm->ps->velocity[2] = 0;
        pm->ps->velocity[1] = 0;
        pm->ps->velocity[0] = 0;
	} else {
		VectorNormalize (pm->ps->velocity);
		VectorScale (pm->ps->velocity, forward, pm->ps->velocity);
	}
}


/*
===============
PM_NoclipMove
===============
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static void PM_NoclipMove(void) {
    long double speed, control, friction, drop, remaining, ratio;
    volatile float scale, wishspeed;
    vec3_t wishvel, wishdir;
    int forwardMove, rightMove, i;

    pm->ps->viewheight = 42;
    speed = TCE_VectorLengthValue(pm->ps->velocity);
    if (speed < 1.0L || speed != speed) {
        pm->ps->velocity[0] = vec3_origin[0];
        pm->ps->velocity[1] = vec3_origin[1];
        pm->ps->velocity[2] = vec3_origin[2];
    } else {
        friction = (long double)pm_friction * 1.5;
        control = pm_stopspeed;
        /* Native C0 keeps stop speed when either operand is unordered. */
        if (speed >= control) control = speed;
        drop = (long double)pml.frametime * control;
        drop *= friction;
        remaining = speed - drop;
        if (remaining < 0.0L || remaining != remaining) remaining = 0.0L;
        ratio = remaining / speed;
        pm->ps->velocity[0] = (float)(ratio * pm->ps->velocity[0]);
        pm->ps->velocity[1] = (float)(ratio * pm->ps->velocity[1]);
        pm->ps->velocity[2] = (float)(ratio * pm->ps->velocity[2]);
    }
    scale = PM_CmdScale(&pm->cmd);
    forwardMove = pm->cmd.forwardmove;
    rightMove = pm->cmd.rightmove;
    for (i = 0; i < 3; ++i) {
        long double right = (long double)rightMove * pml.right[i];
        long double forward = (long double)forwardMove * pml.forward[i];
        wishvel[i] = (float)(right + forward);
    }
    wishdir[0] = wishvel[0];
    wishdir[1] = wishvel[1];
    wishdir[2] = (float)((long double)pm->cmd.upmove + wishvel[2]);
    wishspeed = (float)(TCE_VectorNormalizeValue(wishdir) * scale);
    PM_Accelerate(wishdir, wishspeed, pm_accelerate);
    for (i = 0; i < 3; ++i) {
        long double movement = (long double)pml.frametime * pm->ps->velocity[i];
        pm->ps->origin[i] = (float)(movement + pm->ps->origin[i]);
    }
}
#else
static void PM_NoclipMove( void ) {
	float	speed, drop, friction, control, newspeed;
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		scale;

	pm->ps->viewheight = 42; /* Original3000e2c0, TC standing eye height. */

	// friction

	speed = VectorLength (pm->ps->velocity);
	if (speed < 1)
	{
		VectorCopy (vec3_origin, pm->ps->velocity);
	}
	else
	{
		drop = 0;

		friction = pm_friction*1.5;	// extra friction
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control*friction*pml.frametime;

		// scale the velocity
		newspeed = speed - drop;
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;

		VectorScale (pm->ps->velocity, newspeed, pm->ps->velocity);
	}

	// accelerate
	scale = PM_CmdScale( &pm->cmd );

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;
	
	for (i=0 ; i<3 ; i++)
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	wishvel[2] += pm->cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	wishspeed *= scale;

	PM_Accelerate( wishdir, wishspeed, pm_accelerate );

	// move
	VectorMA (pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin);
}
#endif

//============================================================================

/*
================
PM_FootstepForSurface

Returns an event number apropriate for the groundsurface
================
*/
static int PM_FootstepForSurface( void ) 
{
#ifdef GAMEDLL
	// In just the GAME DLL, we want to store the groundtrace surface stuff,
	// so we don't have to keep tracing.
	ClientStoreSurfaceFlags(pm->ps->clientNum, pml.groundTrace.surfaceFlags);

#endif // GAMEDLL

    if (pm->ps->eFlags & EF_PRONE)
        return (pml.groundTrace.surfaceFlags & 0x2000) ? 23 : 15;
	return BG_FootstepForSurface( pml.groundTrace.surfaceFlags );
}

/*
=================
PM_CrashLand

Check for hard landings that generate sound events
=================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
typedef char groundTraceLayout831[(sizeof(trace_t)==56 && offsetof(trace_t,allsolid)==0 && offsetof(trace_t,fraction)==8 && offsetof(trace_t,plane)==24 && offsetof(trace_t,surfaceFlags)==44 && offsetof(trace_t,entityNum)==52) ? 1 : -1];
typedef char groundProtocol831[(EF_MG42_ACTIVE==0x20 && EF_AAGUN_ACTIVE==0x400000 && EF_MOUNTEDTANK==0x8000 && EF_PRONE==0x80000 && PMF_BACKWARDS_JUMP==8 && PMF_TIME_WATERJUMP==0x100 && PMF_TIME_LAND==0x20 && ENTITYNUM_NONE==1023 && ANIM_ET_JUMP==4 && ANIM_ET_JUMPBK==5 && ANIM_ET_LAND==6) ? 1 : -1];
#endif
/* TCE landing severity and events. Held values model the original path;
 * compiled FPU/libm precision and lower-call ABI remain open. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
typedef char crashEventProtocol[(EV_FALL_NDIE == 0x13 && EV_TCE_FALL_DMG_75 == 0xa2 &&
    EV_FALL_DMG_50 == 0x17 && EV_FALL_DMG_25 == 0x16 && EV_FALL_DMG_15 == 0x15 &&
    EV_FALL_DMG_10 == 0x14 && EV_FALL_SHORT == 0x10 && EV_FOOTSTEP == 1) ? 1 : -1];
static void PM_CrashLand(qboolean steep) {
    unsigned accelerationBits = 0u - (unsigned)pm->ps->gravity;
    int accelerationInteger, event = -1;
    long double velocity, acceleration, halfAcceleration, distance, discriminant;
    long double duration, impact, severity;
    volatile float delta;
#if defined(__linux__)
    volatile float stored;
#endif

    memcpy(&accelerationInteger, &accelerationBits, sizeof(accelerationInteger));
    velocity = pml.previous_velocity[2];
    acceleration = accelerationInteger;
    halfAcceleration = acceleration * 0.5f;
    distance = (long double)pm->ps->origin[2] - pml.previous_origin[2];
    discriminant = velocity * velocity - ((-distance * halfAcceleration) * 4.0f);
    /* Native C0 rejects a negative or unordered discriminant. */
    if (!(discriminant >= 0.0L)) return;
#if defined(__linux__)
    duration = (1.0L / (halfAcceleration + halfAcceleration)) * (-velocity - sqrtl(discriminant));
    stored = (float)duration;
    duration = stored;
#else
    duration = (-velocity - sqrtl(discriminant)) / (halfAcceleration + halfAcceleration);
#endif
    impact = velocity + duration * acceleration;
#if defined(__linux__)
    /* The modifier consumes held impact; the other branch reloads its store. */
    stored = (float)impact;
    if (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) stored = (float)(impact * 0.8f);
    impact = stored;
#else
    if (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) impact *= 0.8f;
#endif
    severity = impact * impact;
    severity *= 0.0001;
#if defined(__linux__)
    stored = (float)severity;
    severity = stored;
#endif
    if (pm->waterlevel == 3) return;
#if defined(__linux__)
    if (pm->waterlevel == 2) {
        stored = (float)(severity * 0.25f);
        severity = stored;
    }
    if (pm->waterlevel == 1) {
        stored = (float)(severity * 0.5f);
        severity = stored;
    }
#else
    if (pm->waterlevel == 2) severity *= 0.25;
    if (pm->waterlevel == 1) severity *= 0.5;
#endif
    if (!(severity >= 1.0f)) return;
    delta = (float)(severity + severity);
    if (!(pml.groundTrace.surfaceFlags & SURF_NODAMAGE)) {
        if (pm->debugLevel) Com_Printf("delta: %5.2f\n", (double)delta);
        if (delta > 75.0f) event = EV_FALL_NDIE;
        else if (delta > 56.0f) event = EV_TCE_FALL_DMG_75;
        else if (delta > 48.0f) event = EV_FALL_DMG_50;
        else if (delta > 44.0f) {
            if (pm->ps->stats[STAT_HEALTH] > 0) event = EV_FALL_DMG_25;
        } else if (delta > 40.0f) {
            if (pm->ps->stats[STAT_HEALTH] > 0) event = EV_FALL_DMG_15;
        } else if (delta > 38.0f) {
            if (pm->ps->stats[STAT_HEALTH] > 0) event = EV_FALL_DMG_10;
        } else if (delta > 13.0f && !steep) event = EV_FALL_SHORT;
        else if (!(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 2) && !steep) event = EV_FOOTSTEP;
        if (event >= 0) PM_AddEventExt(event, PM_FootstepForSurface());
    }
    if (delta > 38.0f) {
        pm->ps->velocity[2] = 0.0f;
        pm->ps->velocity[1] = 0.0f;
        pm->ps->velocity[0] = 0.0f;
    }
    if (steep && !(delta > 38.0f)) return;
    if (!pm->ps->legsTimer &&
        (pml.previous_velocity[2] < -220.0f || TCE_FloatIsNaN(pml.previous_velocity[2]))) {
        BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
            ANIM_ET_LAND, qfalse, qtrue);
    }
    pm->ps->bobCycle = 0;
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
static void PM_CrashLand(qboolean steep) {
    float dist=pm->ps->origin[2]-pml.previous_origin[2];
    float vel=pml.previous_velocity[2], acc=-pm->ps->gravity;
    float a=acc*0.5f, den=vel*vel-4*a*(-dist), t, delta;
    if (den<0) return;
    t=(-vel-sqrt(den))/(2*a);
    delta=vel+t*acc;
    if (pm->ps->stats[STAT_TCE_FLAGS]&0x200) delta*=0.8f;
    delta=delta*delta*0.0001;
    if (pm->waterlevel==3) return;
    if (pm->waterlevel==2) delta*=0.25f;
    if (pm->waterlevel==1) delta*=0.5f;
    if (delta<1) return;
    delta+=delta;
    if (!(pml.groundTrace.surfaceFlags&SURF_NODAMAGE)) {
        int event=-1;
        if (pm->debugLevel) Com_Printf("delta: %5.2f\n",delta);
        if (delta>75) event=EV_FALL_NDIE;
        else if (delta>56) event=EV_TCE_FALL_DMG_75;
        else if (delta>48) event=EV_FALL_DMG_50;
        else if (delta>44) { if(pm->ps->stats[STAT_HEALTH]>0) event=EV_FALL_DMG_25; }
        else if (delta>40) { if(pm->ps->stats[STAT_HEALTH]>0) event=EV_FALL_DMG_15; }
        else if (delta>38) { if(pm->ps->stats[STAT_HEALTH]>0) event=EV_FALL_DMG_10; }
        else if (delta>13 && !steep) event=EV_FALL_SHORT;
        else if (!(pm->ps->stats[STAT_TCE_WEAPON_FLAGS]&2) && !steep) event=EV_FOOTSTEP;
        if(event>=0) PM_AddEventExt(event,PM_FootstepForSurface());
    }
    if(delta>38) VectorClear(pm->ps->velocity);
    if(steep && delta<=38) return;
    if(!pm->ps->legsTimer && pml.previous_velocity[2]<-220)
        BG_AnimScriptEvent(pm->ps,pm->character->animModelInfo,ANIM_ET_LAND,qfalse,qtrue);
    pm->ps->bobCycle=0;
}
#endif

/*
=============
PM_CorrectAllSolid
=============
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static int PM_CorrectAllSolid(trace_t *trace) {
    vec3_t point;
    int i, j, k;

    if (pm->debugLevel) Com_Printf("%i:allsolid\n", c_pmove);
    for (i = -1; i <= 1; ++i) {
        for (j = -1; j <= 1; ++j) {
            for (k = -1; k <= 1; ++k) {
                long double originZ;
                memcpy(point, pm->ps->origin, 2 * sizeof(float));
                originZ = pm->ps->origin[2];
                point[0] = (float)((long double)i + point[0]);
                point[1] = (float)((long double)j + point[1]);
                point[2] = (float)((long double)k + originZ);
                PM_TraceAll(trace, point, point);
                if (!trace->allsolid) {
                    memcpy(point, pm->ps->origin, 2 * sizeof(float));
                    point[2] = (float)((long double)pm->ps->origin[2] - 0.25L);
                    PM_TraceAll(trace, pm->ps->origin, point);
                    memcpy(&pml.groundTrace, trace, sizeof(*trace));
                    return qtrue;
                }
            }
        }
    }
    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = qfalse;
    pml.walking = qfalse;
    return qfalse;
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
static int PM_CorrectAllSolid( trace_t *trace ) {
	int			i, j, k;
	vec3_t		point;

	if ( pm->debugLevel ) {
		Com_Printf("%i:allsolid\n", c_pmove);
	}

	// jitter around
 	for (i = -1; i <= 1; i++) {
		for (j = -1; j <= 1; j++) {
			for (k = -1; k <= 1; k++) {
				VectorCopy(pm->ps->origin, point);
				point[0] += (float) i;
				point[1] += (float) j;
				point[2] += (float) k;
				PM_TraceAll(trace, point, point);
				if ( !trace->allsolid ) {
					point[0] = pm->ps->origin[0];
					point[1] = pm->ps->origin[1];
					point[2] = pm->ps->origin[2] - 0.25;

					PM_TraceAll(trace, pm->ps->origin, point);
					pml.groundTrace = *trace;
					return qtrue;
				}
			}
		}
	}

	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pml.groundPlane = qfalse;
	pml.walking = qfalse;

	return qfalse;
}
#endif


/*
=============
PM_GroundTraceMissed

The ground trace didn't hit a surface, so we are in freefall
=============
*/
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
static void PM_GroundTraceMissed(void) {
    trace_t trace;
    vec3_t point;

    if (pm->ps->groundEntityNum != ENTITYNUM_NONE) {
        if (pm->debugLevel) Com_Printf("%i:lift\n", c_pmove);
        memcpy(point, pm->ps->origin, 2 * sizeof(float));
        point[2] = (float)((long double)pm->ps->origin[2] - 64.0L);
        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point,
            pm->ps->clientNum, pm->tracemask);
        /* Native C3 accepts equality and unordered trace fractions. */
        if (trace.fraction == 1.0f || TCE_FloatIsNaN(trace.fraction)) {
            if (pm->cmd.forwardmove >= 0) {
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                    ANIM_ET_JUMP, qfalse, qtrue);
                pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
            } else {
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                    ANIM_ET_JUMPBK, qfalse, qtrue);
                pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
            }
        }
    }
    if (pm->ps->groundEntityNum != -1) pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = qfalse;
    pml.walking = qfalse;
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
static void PM_GroundTraceMissed( void ) {
	trace_t		trace;
	vec3_t		point;

	if ( pm->ps->groundEntityNum != ENTITYNUM_NONE ) {
		// we just transitioned into freefall
		if ( pm->debugLevel ) {
			Com_Printf("%i:lift\n", c_pmove);
		}

		// if they aren't in a jumping animation and the ground is a ways away, force into it
		// if we didn't do the trace, the player would be backflipping down staircases
		VectorCopy( pm->ps->origin, point );
		point[2] -= 64;

		pm->trace(&trace,pm->ps->origin,pm->mins,pm->maxs,point,pm->ps->clientNum,pm->tracemask);
		if ( trace.fraction == 1.0 ) {
			if ( pm->cmd.forwardmove >= 0 ) {
				BG_AnimScriptEvent( pm->ps, pm->character->animModelInfo, ANIM_ET_JUMP, qfalse, qtrue );
				pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
			} else {
				BG_AnimScriptEvent( pm->ps, pm->character->animModelInfo, ANIM_ET_JUMPBK, qfalse, qtrue );
				pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
			}
		}
	}

	// If we've never yet touched the ground, it's because we're spawning, so don't 
	// set to "in air"
	if (pm->ps->groundEntityNum != -1)
	{
		// Signify that we're in mid-air
		pm->ps->groundEntityNum = ENTITYNUM_NONE;

	} // if (pm->ps->groundEntityNum != -1)...
	pml.groundPlane = qfalse;
	pml.walking = qfalse;
}
#endif


/*
=============
PM_GroundTrace
=============
*/
/* TCE capsule ground controller, including content/prone ground extension.
 * Native predicates and callback ordering are explicit; compiled ABI remains open. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && defined(__i386__))
typedef char groundCallbackBounds[(offsetof(pmove_t, mins) == 0xf4 &&
    offsetof(pmove_t, maxs) == 0x100 && offsetof(pmove_t, tracemask) == 0x44 &&
    offsetof(playerState_t, origin) == 0x14 && offsetof(playerState_t, clientNum) == 0xa0) ? 1 : -1];
#ifdef CGAMEDLL
typedef char groundCallbackSlot[(offsetof(pmove_t, trace) == 0x124) ? 1 : -1];
#else
typedef char groundCallbackSlot[(offsetof(pmove_t, trace) == 0x128) ? 1 : -1];
#endif
#endif
static void PM_GroundTrace(void) {
    vec3_t point;
    trace_t trace;
    long double separation;
    qboolean proneGround = qfalse, candidateHit;

    point[0] = pm->ps->origin[0];
    point[1] = pm->ps->origin[1];
    if (pm->ps->eFlags & (EF_MG42_ACTIVE | EF_AAGUN_ACTIVE))
        point[2] = (float)((long double)pm->ps->origin[2] - 1.0f);
    else point[2] = (float)((long double)pm->ps->origin[2] - 0.25f);
    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point,
        pm->ps->clientNum, pm->tracemask);
    memcpy(&pml.groundTrace, &trace, sizeof(trace));
    if (trace.allsolid && !(pm->ps->eFlags & EF_MOUNTEDTANK) &&
        !PM_CorrectAllSolid(&trace)) return;

    /* Native C3 takes the no-hit path for fraction one or unordered. */
    if (trace.fraction == 1.0f || TCE_FloatIsNaN(trace.fraction)) {
        if (pml.tceContentRestriction &&
            (pm->ps->velocity[2] < 10.0f || TCE_FloatIsNaN(pm->ps->velocity[2]))) {
            point[2] = (float)((long double)point[2] - 12.0f);
            pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point,
                pm->ps->clientNum, pm->tracemask);
            candidateHit = trace.fraction < 1.0f;
#if !defined(__linux__)
            candidateHit = candidateHit || TCE_FloatIsNaN(trace.fraction);
#endif
            if (candidateHit &&
                !trace.allsolid && (long double)trace.plane.normal[2] > 0.7) {
                pml.tceGroundExtension = qtrue;
                memcpy(&pml.groundTrace, &trace, sizeof(trace));
            }
        } else if (pm->ps->eFlags & EF_PRONE) {
            point[2] = (float)((long double)point[2] - 12.0f);
            pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point,
                pm->ps->clientNum, pm->tracemask);
            candidateHit = trace.fraction < 1.0f;
#if !defined(__linux__)
            candidateHit = candidateHit || TCE_FloatIsNaN(trace.fraction);
#endif
            if (candidateHit && !trace.allsolid) {
                proneGround = qtrue;
                memcpy(&pml.groundTrace, &trace, sizeof(trace));
            }
        }
        if (!pml.tceGroundExtension && !proneGround) {
            PM_GroundTraceMissed();
            pml.groundPlane = qfalse;
            pml.walking = qfalse;
            return;
        }
    }
    if (pm->ps->velocity[2] > 0.0f) {
        separation = (long double)trace.plane.normal[1] * pm->ps->velocity[1];
        separation += (long double)trace.plane.normal[0] * pm->ps->velocity[0];
        separation += (long double)trace.plane.normal[2] * pm->ps->velocity[2];
        if (separation > 80.0f && !(pm->ps->eFlags & EF_PRONE)) {
            if (pm->debugLevel) Com_Printf("%i:kickoff\n", c_pmove);
            if (pm->cmd.forwardmove < 0) {
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                    ANIM_ET_JUMPBK, qfalse, qfalse);
                pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
            } else {
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                    ANIM_ET_JUMP, qfalse, qfalse);
                pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
            }
            pm->ps->groundEntityNum = ENTITYNUM_NONE;
            pml.groundPlane = qfalse;
            pml.walking = qfalse;
            return;
        }
    }
    if ((long double)trace.plane.normal[2] < 0.7 || TCE_FloatIsNaN(trace.plane.normal[2])) {
        if (pm->debugLevel) Com_Printf("%i:steep\n", c_pmove);
        if (!(pm->ps->stats[STAT_TCE_FLAGS] & 4) || pm->ps->groundEntityNum != ENTITYNUM_NONE) {
            pm->ps->groundEntityNum = ENTITYNUM_NONE;
            pml.walking = qfalse;
            pml.groundPlane = qtrue;
            return;
        }
    }
    pml.groundPlane = qtrue;
    pml.walking = qtrue;
    if (pm->ps->pm_flags & PMF_TIME_WATERJUMP) {
        pm->ps->pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND);
        pm->ps->pm_time = 0;
    }
    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        if (pm->debugLevel) Com_Printf("%i:Land\n", c_pmove);
        PM_CrashLand((long double)trace.plane.normal[2] < 0.7 ||
            TCE_FloatIsNaN(trace.plane.normal[2]));
        if (pml.previous_velocity[2] < -200.0f || TCE_FloatIsNaN(pml.previous_velocity[2])) {
            pm->ps->pm_flags |= PMF_TIME_LAND;
            pm->ps->pm_time = 250;
            pm->pmext->proneTime = PM_ProneTimeValue(0u - (unsigned)pm->cmd.serverTime);
            pm->pmext->jumpTime = PM_ProneTimeValue((unsigned)pm->cmd.serverTime - 600u);
            pm->ps->jumpTime = PM_ProneTimeValue((unsigned)pm->cmd.serverTime - 600u);
        }
        if (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x1000) {
            pm->ps->pm_time = 1000;
            pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x1000;
            pm->pmext->proneTime = PM_ProneTimeValue(0u - (unsigned)pm->cmd.serverTime);
            pm->pmext->jumpTime = PM_ProneTimeValue((unsigned)pm->cmd.serverTime - 600u);
            pm->ps->jumpTime = PM_ProneTimeValue((unsigned)pm->cmd.serverTime - 600u);
        }
        if (pm->ps->stats[STAT_TCE_FLAGS] & 4) pm->ps->stats[STAT_TCE_FLAGS] &= ~4;
    }
    pm->ps->groundEntityNum = trace.entityNum;
    if (pm->ps->groundEntityNum < 64) pml.groundTrace.surfaceFlags |= 2;
    PM_AddTouchEnt(trace.entityNum);
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
static void PM_GroundTrace(void) {
    vec3_t point;
    trace_t trace;
    qboolean proneGround=qfalse;
    VectorCopy(pm->ps->origin,point);
    point[2]-=(pm->ps->eFlags&(EF_MG42_ACTIVE|EF_AAGUN_ACTIVE))?1.f:0.25f;
    pm->trace(&trace,pm->ps->origin,pm->mins,pm->maxs,point,pm->ps->clientNum,pm->tracemask);
    pml.groundTrace=trace;
    if(trace.allsolid && !(pm->ps->eFlags&EF_MOUNTEDTANK) && !PM_CorrectAllSolid(&trace)) return;
    if(trace.fraction==1.f) {
        if(pml.tceContentRestriction && pm->ps->velocity[2]<10.f) {
            point[2]-=12.f;
            pm->trace(&trace,pm->ps->origin,pm->mins,pm->maxs,point,pm->ps->clientNum,pm->tracemask);
            if(trace.fraction<1.f && !trace.allsolid && trace.plane.normal[2]>MIN_WALK_NORMAL) {
                pml.tceGroundExtension=qtrue;
                pml.groundTrace=trace;
            }
        } else if(pm->ps->eFlags&EF_PRONE) {
            point[2]-=12.f;
            pm->trace(&trace,pm->ps->origin,pm->mins,pm->maxs,point,pm->ps->clientNum,pm->tracemask);
            if(trace.fraction<1.f && !trace.allsolid) {
                proneGround=qtrue;
                pml.groundTrace=trace;
            }
        }
        if(!pml.tceGroundExtension && !proneGround) {
            PM_GroundTraceMissed(); pml.groundPlane=pml.walking=qfalse; return;
        }
    }
    if(pm->ps->velocity[2]>0 && DotProduct(pm->ps->velocity,trace.plane.normal)>80.f && !(pm->ps->eFlags&EF_PRONE)) {
        if(pm->debugLevel) Com_Printf("%i:kickoff\n",c_pmove);
        BG_AnimScriptEvent(pm->ps,pm->character->animModelInfo,
            pm->cmd.forwardmove<0?ANIM_ET_JUMPBK:ANIM_ET_JUMP,qfalse,qfalse);
        if(pm->cmd.forwardmove<0) pm->ps->pm_flags|=PMF_BACKWARDS_JUMP;
        else pm->ps->pm_flags&=~PMF_BACKWARDS_JUMP;
        pm->ps->groundEntityNum=ENTITYNUM_NONE;pml.groundPlane=pml.walking=qfalse;return;
    }
    if(trace.plane.normal[2]<MIN_WALK_NORMAL) {
        if(pm->debugLevel) Com_Printf("%i:steep\n",c_pmove);
        if(!(pm->ps->stats[STAT_TCE_FLAGS]&4) || pm->ps->groundEntityNum!=ENTITYNUM_NONE) {
            pm->ps->groundEntityNum=ENTITYNUM_NONE;pml.walking=qfalse;pml.groundPlane=qtrue;return;
        }
    }
    pml.groundPlane=pml.walking=qtrue;
    if(pm->ps->pm_flags&PMF_TIME_WATERJUMP) {
        pm->ps->pm_flags&=~(PMF_TIME_WATERJUMP|PMF_TIME_LAND);pm->ps->pm_time=0;
    }
    if(pm->ps->groundEntityNum==ENTITYNUM_NONE) {
        if(pm->debugLevel) Com_Printf("%i:Land\n",c_pmove);
        PM_CrashLand(trace.plane.normal[2]<MIN_WALK_NORMAL);
        if(pml.previous_velocity[2]<-200) {
            pm->ps->pm_flags|=PMF_TIME_LAND;pm->ps->pm_time=250;
            pm->pmext->proneTime=-pm->cmd.serverTime;
            pm->pmext->jumpTime=pm->ps->jumpTime=pm->cmd.serverTime-600;
        }
        if(pm->ps->stats[STAT_TCE_WEAPON_FLAGS]&0x1000) {
            pm->ps->pm_time=1000;pm->ps->stats[STAT_TCE_WEAPON_FLAGS]&=~0x1000;
            pm->pmext->proneTime=-pm->cmd.serverTime;
            pm->pmext->jumpTime=pm->ps->jumpTime=pm->cmd.serverTime-600;
        }
        pm->ps->stats[STAT_TCE_FLAGS]&=~4;
    }
    pm->ps->groundEntityNum=trace.entityNum;
    if(trace.entityNum<64) pml.groundTrace.surfaceFlags|=2;
    PM_AddTouchEnt(trace.entityNum);
}
#endif

/*
=============
PM_SetWaterLevel	FIXME: avoid this twice?  certainly if not moving
=============
*/
/* TC:E posture trace layout requirements. */
#if defined(_MSC_VER) && defined(_M_IX86)
typedef char posture831PsAtZero[(offsetof(pmove_t, ps) == 0) ? 1 : -1];
typedef char posture831TraceLayout[(sizeof(trace_t) == 56 && offsetof(trace_t, allsolid) == 0) ? 1 : -1];
static void PM_SetWaterLevel(void) {
    vec3_t point;
    int contents, sampleHeight, middleHeight;

    pm->waterlevel = 0;
    pm->watertype = 0;
    memcpy(point, pm->ps->origin, 2 * sizeof(float));
    point[2] = (float)((long double)pm->ps->mins[2] + pm->ps->origin[2] + 1.0L);
    contents = pm->pointcontents(point, pm->ps->clientNum);
    if (contents & MASK_WATER) {
        sampleHeight = TCE_TruncateNativeInteger(
            (long double)pm->ps->viewheight - pm->ps->mins[2]);
        pm->watertype = contents;
        pm->waterlevel = 1;
        middleHeight = sampleHeight / 2;
        point[2] = (float)((long double)middleHeight + pm->ps->mins[2] + pm->ps->origin[2]);
        contents = pm->pointcontents(point, pm->ps->clientNum);
        if (contents & MASK_WATER) {
            pm->waterlevel = 2;
            point[2] = (float)((long double)pm->ps->mins[2] + pm->ps->origin[2] + sampleHeight);
            contents = pm->pointcontents(point, pm->ps->clientNum);
            if (contents & MASK_WATER) pm->waterlevel = 3;
        }
    }
    /* Native retains the earlier restriction when the last sample has no bit. */
    if (contents & 0x1000) pml.tceContentRestriction = 1;
    else if (contents & 0x800) pml.tceContentRestriction = 2;
    else if (contents & 0x400) pml.tceContentRestriction = 3;
    BG_UpdateConditionValue(pm->ps->clientNum, ANIM_COND_UNDERWATER,
        pm->waterlevel > 2, qtrue);
}
#else
static void PM_SetWaterLevel( void ) {
	vec3_t		point;
	int			cont;
	int			sample1;
	int			sample2;

	//
	// get waterlevel, accounting for ducking
	// Original x87 sample coordinates round only at the point-array store.
	//
	pm->waterlevel = 0;
	pm->watertype = 0;

	// Ridah, modified this
	point[0] = pm->ps->origin[0];
	point[1] = pm->ps->origin[1];
	point[2] = (float)((double)pm->ps->mins[2] + pm->ps->origin[2] + 1.0);	
	cont = pm->pointcontents( point, pm->ps->clientNum );

	if ( cont & MASK_WATER ) {
		sample2 = (int)((double)pm->ps->viewheight - pm->ps->mins[2]);
		sample1 = sample2 / 2;

		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = (float)((double)sample1 + pm->ps->mins[2] + pm->ps->origin[2]);
		cont = pm->pointcontents (point, pm->ps->clientNum );
		if ( cont & MASK_WATER ) {
			pm->waterlevel = 2;
			point[2] = (float)((double)pm->ps->mins[2] + pm->ps->origin[2] + sample2);
			cont = pm->pointcontents (point, pm->ps->clientNum );
			if ( cont & MASK_WATER ){
				pm->waterlevel = 3;
			}
		}
	}
	// done.

	/* Original uses the last sampled contents and preserves an earlier sample
     * within this PmoveSingle when none of these bits is present. */
    if(cont & 0x1000)pml.tceContentRestriction=1;
    else if(cont & 0x800)pml.tceContentRestriction=2;
    else if(cont & 0x400)pml.tceContentRestriction=3;

	// UNDERWATER
	BG_UpdateConditionValue( pm->ps->clientNum, ANIM_COND_UNDERWATER, (pm->waterlevel > 2), qtrue );

}
#endif

/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->ps->viewheight
==============
*/
/* TC Windows 3000f100 / Linux 000e16d2. Persistent slot 14 bit 4
 * selects toggle crouch; weapon-flags 0x200/0x400 are its state/key latch. */
static int PM_ViewHeightInteger(float height) {
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
    return TCE_TruncateNativeInteger((long double)height);
#else
    return (int)height;
#endif
}

static void PM_CheckDuck(void) {
    trace_t trace;
    playerState_t *ps = pm->ps;
    qboolean toggle;
    unsigned jumpBits;

    memcpy(pm->mins, ps->mins, 2 * sizeof(float));
    memcpy(pm->maxs, ps->maxs, 2 * sizeof(float));
    memcpy(&pm->mins[2], &ps->mins[2], sizeof(float));
    if (ps->pm_type == PM_DEAD) {
        pm->maxs[2] = ps->maxs[2];
        ps->viewheight = PM_ViewHeightInteger(ps->deadViewHeight);
        return;
    }
    if (ps->pm_type == PM_SPECTATOR) {
        pm->maxs[2] = ps->crouchMaxZ;
        ps->viewheight = PM_ViewHeightInteger(ps->crouchViewHeight);
        return;
    }
    toggle = (ps->persistant[14] & 4) != 0;
    if (toggle && !(ps->eFlags & 0x1008000) && !(ps->pm_flags & PMF_LADDER)) {
        if (pm->cmd.upmove < 0) {
            if (!(ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x400)) {
                ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x400;
                ps->stats[STAT_TCE_WEAPON_FLAGS] ^= 0x200;
            }
        } else {
            ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x400;
        }
    }
    if ((pm->cmd.upmove < 11 && toggle && (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x200)) ||
        (pm->cmd.upmove < 0 && !toggle && !(ps->eFlags & 0x1008000) && !(ps->pm_flags & PMF_LADDER)) ||
        ps->weapon == 60) {
        ps->pm_flags |= PMF_DUCKED;
    } else if (ps->pm_flags & PMF_DUCKED) {
        pm->maxs[2] = ps->maxs[2];
        pm->trace(&trace, ps->origin, pm->mins, pm->maxs, ps->origin, ps->clientNum, pm->tracemask);
        ps = pm->ps;
        if (!trace.allsolid) ps->pm_flags &= ~PMF_DUCKED;
        if (pm->cmd.upmove > 10) {
            if (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x200)
                ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x200;
            /* Native SUB wraps separately for each destination. */
            jumpBits = (unsigned)pm->cmd.serverTime - 650u;
            memcpy(&pm->pmext->jumpTime, &jumpBits, sizeof(jumpBits));
            jumpBits = (unsigned)pm->cmd.serverTime - 650u;
            memcpy(&pm->ps->jumpTime, &jumpBits, sizeof(jumpBits));
            pm->ps->pm_flags |= PMF_JUMP_HELD;
        }
    }
    ps = pm->ps;
    pm->maxs[2] = (ps->pm_flags & PMF_DUCKED) ? ps->crouchMaxZ : ps->maxs[2];
    ps->viewheight = PM_ViewHeightInteger(
        (ps->pm_flags & PMF_DUCKED) ? ps->crouchViewHeight : ps->standViewHeight);
}



//===================================================================


/*
===============
PM_Footsteps
===============
*/
/* TC:E cgame3000fb90: the surface of the forward ladder trace. */
static int PM_TCELadderFootstepForSurface(void) {
    unsigned int flags = pml.tceLadderSurfaceFlags;
    if (flags & 0x2000) return 23;
    if ((flags & 0xff000000) == 0x14000000) return 11;
    if (!(flags & 8)) return 14;
    return 1 + ((flags & 0xff000000) == 0x05000000);
}

/* TCE animation, bob cycle and step-sound controller.
 * Native value paths/callback reads are explicit; compiled FPU and ABI remain open. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
static void PM_Footsteps(void) {
    float bobmove;
#if defined(__linux__)
    volatile float storedRate;
#endif
    long double rate, speed;
    int oldCycle, soundClass = 0, animResult, moveType;
    qboolean backwards, quiet, walking;
    playerState_t *cyclePlayer;

    if (pm->ps->eFlags & EF_DEAD) {
        if (pm->ps->pm_flags & PMF_FLAILING) {
            BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, ANIM_MT_FLAILING, qtrue);
            if (!pm->ps->pm_time) pm->ps->pm_flags &= ~PMF_FLAILING;
        } else if (!pm->ps->pm_time && !(pm->ps->pm_flags & PMF_LIMBO) &&
            pm->ps->groundEntityNum == ENTITYNUM_NONE) {
            pm->ps->pm_flags |= PMF_FLAILING;
            BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, ANIM_MT_FLAILING, qtrue);
        }
        return;
    }
    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x20;
    pm->ps->eFlags |= 0x10000;
    speed = (long double)pm->ps->velocity[0] * pm->ps->velocity[0];
    speed += (long double)pm->ps->velocity[1] * pm->ps->velocity[1];
    pm->xyspeed = (float)sqrtl(speed);
    if (pm->ps->persistant[PERS_HWEAPON_USE]) goto idle;
    if (pm->waterlevel > 2) {
        BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo,
            (pm->ps->pm_flags & PMF_BACKWARDS_RUN) ? ANIM_MT_SWIMBK : ANIM_MT_SWIM, qtrue);
        return;
    }
    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        if (!(pm->ps->pm_flags & PMF_LADDER)) return;
#if defined(__linux__)
        /* Linux compares zero against velocity: unordered selects climb-up. */
        moveType = !(pm->ps->velocity[2] < 0.0f) ? ANIM_MT_CLIMBUP : ANIM_MT_CLIMBDOWN;
#else
        moveType = (pm->ps->velocity[2] < 0.0f || TCE_FloatIsNaN(pm->ps->velocity[2])) ?
            ANIM_MT_CLIMBDOWN : ANIM_MT_CLIMBUP;
#endif
        BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, moveType, qtrue);
        if (!pm->cmd.forwardmove && !pm->cmd.rightmove && !pm->cmd.upmove) return;
        rate = TCE_VectorLengthValue(pm->ps->velocity) * 0.0033333333333333335;
#if defined(__linux__)
        /* Both Linux originals spill the scaled length before the thresholds. */
        storedRate = (float)rate;
        rate = storedRate;
#endif
        if (rate > 0.5f) rate = 0.5f;
        else if (!(rate >= 0.1f)) return;
        cyclePlayer = pm->ps;
        oldCycle = cyclePlayer->bobCycle;
        cyclePlayer->bobCycle = TCE_TruncateNativeInteger((long double)pml.msec * rate + oldCycle) & 255;
        if ((((unsigned)oldCycle + 64u) ^ ((unsigned)pm->ps->bobCycle + 64u)) & 128u) {
            if (!pm->noFootsteps) PM_AddEventExt(EV_FOOTSTEP, PM_TCELadderFootstepForSurface());
        }
        return;
    }
    if (!pm->cmd.forwardmove && !pm->cmd.rightmove) {
        if (pm->xyspeed < 5.0f || TCE_FloatIsNaN(pm->xyspeed)) pm->ps->bobCycle = 0;
        if (pm->xyspeed > 120.0f) return;
        if (pm->ps->eFlags & EF_PRONE) moveType = ANIM_MT_IDLEPRONE;
        else if (pm->ps->pm_flags & PMF_DUCKED) moveType = ANIM_MT_IDLECR;
        else goto idle;
        animResult = BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, moveType, qtrue);
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x20;
        pm->ps->eFlags &= ~0x10000;
        if (animResult >= 0) return;
        goto idle;
    }
    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 2;
    backwards = (pm->ps->pm_flags & PMF_BACKWARDS_RUN) != 0;
    if (pm->ps->eFlags & EF_PRONE) {
        bobmove = 0.2f;
        animResult = BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo,
            backwards ? ANIM_MT_PRONEBK : ANIM_MT_PRONE, qtrue);
        soundClass = 2;
    } else if (pm->ps->pm_flags & PMF_DUCKED) {
        bobmove = 0.2f;
        animResult = BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo,
            backwards ? ANIM_MT_WALKCRBK : ANIM_MT_WALKCR, qtrue);
        if (!(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) &&
            TCE_FloatIsZeroOrNaN(pm->ps->leanf) && !(pm->cmd.buttons & BUTTON_WALKING)) soundClass = 1;
    } else {
        walking = (pm->cmd.buttons & (BUTTON_WALKING | BUTTON_SPRINT)) == BUTTON_WALKING;
        if (walking) bobmove = 0.28f;
        else {
            quiet = (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) ||
                !TCE_FloatIsZeroOrNaN(pm->ps->leanf);
            if (quiet) { bobmove = 0.28f; soundClass = 1; }
            else if (pm->xyspeed > 220.0f) { bobmove = 0.45f; soundClass = 3; }
            else { bobmove = 0.36f; soundClass = 2; }
            if (!backwards && (pm->ps->eFlags & EF_PRONE_MOVING)) soundClass = 0;
        }
        if (pm->cmd.rightmove && !pm->cmd.forwardmove) {
            animResult = BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo,
                pm->cmd.rightmove > 0 ? ANIM_MT_STRAFERIGHT : ANIM_MT_STRAFELEFT, qtrue);
            if (animResult >= 0) goto bob;
        }
        if (walking) moveType = backwards ? ANIM_MT_WALKBK : ANIM_MT_WALK;
        else {
            /* Native branches re-read tactical/lean state after a failed strafe animation. */
            quiet = (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) ||
                !TCE_FloatIsZeroOrNaN(pm->ps->leanf);
            moveType = quiet ? (backwards ? ANIM_MT_WALKBK : ANIM_MT_WALK) :
                (backwards ? ANIM_MT_RUNBK : ANIM_MT_RUN);
        }
        animResult = BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, moveType, qtrue);
    }
    if (animResult < 0) {
        BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, ANIM_MT_IDLE, qtrue);
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x20;
        pm->ps->eFlags &= ~0x10000;
    }
bob:
    rate = bobmove;
    if (pml.tceContentRestriction) rate *= 1.3f;
#if defined(__linux__)
    storedRate = (float)rate;
    rate = storedRate;
#endif
    cyclePlayer = pm->ps;
    oldCycle = cyclePlayer->bobCycle;
    cyclePlayer->bobCycle = TCE_TruncateNativeInteger((long double)pml.msec * rate + oldCycle) & 255;
    if ((((unsigned)oldCycle + 64u) ^ ((unsigned)pm->ps->bobCycle + 64u)) & 128u) {
        if (!pm->waterlevel) {
            if (!pm->noFootsteps && soundClass) {
                PM_AddEventExt(soundClass == 1 ? EV_TCE_FOOTSTEP_WALK :
                    soundClass == 3 ? EV_TCE_FOOTSTEP_SPRINT : EV_FOOTSTEP, PM_FootstepForSurface());
            }
        } else if (pm->waterlevel == 1) PM_AddEvent(EV_FOOTSPLASH);
        else if (pm->waterlevel == 2) PM_AddEvent(EV_SWIM);
    }
    if (soundClass) pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~2;
    return;
idle:
    BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, ANIM_MT_IDLE, qtrue);
    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x20;
    pm->ps->eFlags &= ~0x10000;
}
#else
static void PM_Footsteps(void) {
    float bobmove;
    int old, soundClass = 0, animResult = -1, moveType;
    qboolean backwards, quiet;
    if (pm->ps->eFlags & EF_DEAD) {
        if (pm->ps->pm_flags & PMF_FLAILING) {
            BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, ANIM_MT_FLAILING, qtrue);
            if (!pm->ps->pm_time) pm->ps->pm_flags &= ~PMF_FLAILING;
        } else if (!pm->ps->pm_time && !(pm->ps->pm_flags & PMF_LIMBO) &&
                   pm->ps->groundEntityNum == ENTITYNUM_NONE) {
            pm->ps->pm_flags |= PMF_FLAILING;
            BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, ANIM_MT_FLAILING, qtrue);
        }
        return;
    }
    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x20;
    pm->ps->eFlags |= 0x10000;
    pm->xyspeed = sqrt(pm->ps->velocity[0]*pm->ps->velocity[0] + pm->ps->velocity[1]*pm->ps->velocity[1]);
    if (pm->ps->persistant[PERS_HWEAPON_USE]) goto idle;
    backwards = (pm->ps->pm_flags & PMF_BACKWARDS_RUN) != 0;
    if (pm->waterlevel > 2) {
        BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo,
            backwards ? ANIM_MT_SWIMBK : ANIM_MT_SWIM, qtrue);
        return;
    }
    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        if (!(pm->ps->pm_flags & PMF_LADDER)) return;
        BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo,
            pm->ps->velocity[2] < 0 ? ANIM_MT_CLIMBDOWN : ANIM_MT_CLIMBUP, qtrue);
        if (!pm->cmd.forwardmove && !pm->cmd.rightmove && !pm->cmd.upmove) return;
        bobmove = (float)(VectorLength(pm->ps->velocity) / 300.0);
        if (bobmove > 0.5f) bobmove = 0.5f;
        if (bobmove < 0.1f) return;
        old = pm->ps->bobCycle;
        pm->ps->bobCycle = (int)(old + bobmove*pml.msec) & 255;
        if (((old+64) ^ (pm->ps->bobCycle+64)) & 128)
            if (!pm->noFootsteps) PM_AddEventExt(EV_FOOTSTEP, PM_TCELadderFootstepForSurface());
        return;
    }
    if (!pm->cmd.forwardmove && !pm->cmd.rightmove) {
        if (pm->xyspeed < 5) pm->ps->bobCycle = 0;
        if (pm->xyspeed > 120) return;
        if (pm->ps->eFlags & EF_PRONE) moveType = ANIM_MT_IDLEPRONE;
        else if (pm->ps->pm_flags & PMF_DUCKED) moveType = ANIM_MT_IDLECR;
        else goto idle;
        animResult = BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, moveType, qtrue);
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x20;
        pm->ps->eFlags &= ~0x10000;
        if (animResult >= 0) return;
        goto idle;
    }
    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 2;
    quiet = (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) || pm->ps->leanf != 0;
    if (pm->ps->eFlags & EF_PRONE) {
        bobmove = 0.2f;
        soundClass = 2;
        moveType = backwards ? ANIM_MT_PRONEBK : ANIM_MT_PRONE;
    } else if (pm->ps->pm_flags & PMF_DUCKED) {
        bobmove = 0.2f;
        moveType = backwards ? ANIM_MT_WALKCRBK : ANIM_MT_WALKCR;
        if (!quiet && !(pm->cmd.buttons & BUTTON_WALKING)) soundClass = 1;
    } else {
        if ((pm->cmd.buttons & (BUTTON_WALKING|BUTTON_SPRINT)) == BUTTON_WALKING) {
            bobmove = 0.28f;
            moveType = backwards ? ANIM_MT_WALKBK : ANIM_MT_WALK;
        } else {
            if (quiet) { bobmove = 0.28f; soundClass = 1; }
            else if (pm->xyspeed <= 220.f) { bobmove = 0.36f; soundClass = 2; }
            else { bobmove = 0.45f; soundClass = 3; }
            /* Original suppresses this flag only in the forward branch. */
            if (!backwards && (pm->ps->eFlags & EF_PRONE_MOVING)) soundClass = 0;
            moveType = quiet ? (backwards ? ANIM_MT_WALKBK : ANIM_MT_WALK) :
                               (backwards ? ANIM_MT_RUNBK : ANIM_MT_RUN);
        }
        if (pm->cmd.rightmove && !pm->cmd.forwardmove)
            animResult = BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo,
                pm->cmd.rightmove > 0 ? ANIM_MT_STRAFERIGHT : ANIM_MT_STRAFELEFT, qtrue);
    }
    if (animResult < 0) {
        animResult = BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, moveType, qtrue);
        if (animResult < 0) {
            BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, ANIM_MT_IDLE, qtrue);
            pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x20;
            pm->ps->eFlags &= ~0x10000;
        }
    }
    if (pml.tceContentRestriction) bobmove *= 1.3f;
    old = pm->ps->bobCycle;
    pm->ps->bobCycle = (int)(old + bobmove*pml.msec) & 255;
    if (((old+64) ^ (pm->ps->bobCycle+64)) & 128) {
        if (!pm->waterlevel) {
            if (!pm->noFootsteps && soundClass)
                PM_AddEventExt(soundClass == 1 ? EV_TCE_FOOTSTEP_WALK :
                    soundClass == 3 ? EV_TCE_FOOTSTEP_SPRINT : EV_FOOTSTEP, PM_FootstepForSurface());
        } else if (pm->waterlevel == 1) PM_AddEvent(EV_FOOTSPLASH);
        else if (pm->waterlevel == 2) PM_AddEvent(EV_SWIM);
    }
    if (soundClass) pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~2;
    return;
idle:
    BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, ANIM_MT_IDLE, qtrue);
    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x20;
    pm->ps->eFlags &= ~0x10000;
}
#endif

/*
==============
PM_WaterEvents

Generate sound events for entering and leaving water
==============
*/
static void PM_WaterEvents( void ) {		// FIXME?
	//
	// if just entered a water volume, play a sound
	//
	if (!pml.previous_waterlevel && pm->waterlevel) {
		PM_AddEvent( EV_WATER_TOUCH );
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (pml.previous_waterlevel && !pm->waterlevel) {
		PM_AddEvent( EV_WATER_LEAVE );
	}

	//
	// check for head just going under water
	//
	if (pml.previous_waterlevel != 3 && pm->waterlevel == 3) {
		PM_AddEvent( EV_WATER_UNDER );
	}

	//
	// check for head just coming out of water
	//
	if (pml.previous_waterlevel == 3 && pm->waterlevel != 3) {
 		if( pm->pmext->airleft < 6000 ) {
 			PM_AddEventExt( EV_WATER_CLEAR, 1 );
		} else {
			PM_AddEventExt( EV_WATER_CLEAR, 0 );
		}
	}
}


/*
==============
PM_BeginWeaponReload
==============
*/
/* Whole TC300096d0: no SDK fallback when gear metadata is not ready. */
static void PM_BeginWeaponReload(int weapon) {
    gitem_t *reloadItem;
    int reloadDuration;
    if (pm->ps->weaponstate != 0 && pm->ps->weaponstate != 7) return;
    if ((weapon == 31 || weapon == 62) && pm->ps->ammoclip[31]) return;
    if (!((weapon > 0 && weapon < 16) || (weapon > 22 && weapon < 64))) return;
    reloadItem = BG_FindItemForWeapon(weapon);
    if (!reloadItem) return;
    if (pm->ps->ammoclip[reloadItem->giAmmoIndex] >= weaponDef[weapon].maxclip) return;
    if (weapon != 4 && weapon != 9 && weapon != 15 && weapon != 30)
        BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
            (pm->ps->eFlags & EF_PRONE) ? ANIM_ET_RELOADPRONE : ANIM_ET_RELOAD,
            qfalse, qtrue);
    if (weapon != 35 && weapon != 60) {
        int reloadAnim = TCE_PM_WeaponClipEmpty(weapon, pm->noWeapClips,
            pm->ps->ammo, pm->ps->ammoclip)
            ? PM_ReloadAnimForWeapon(pm->ps->weapon)
            : PM_NonemptyReloadAnimForWeapon(pm->ps->weapon);
        PM_ContinueWeaponAnim(reloadAnim);
    }
    /* Original reads current state/time after the body-animation callback. */
    reloadDuration = weaponDef[weapon].reloadTime;
    if (!pm->ps->weaponstate)
        *(unsigned int *)&pm->ps->weaponTime += (unsigned int)reloadDuration;
    else if (pm->ps->weaponTime < reloadDuration)
        pm->ps->weaponTime = reloadDuration;
    if (weaponDef[weapon].singleReload) {
        pm->ps->weaponstate = 10;
        return;
    }
    pm->ps->weaponstate = 9;
    PM_AddEvent(EV_FILL_CLIP);
}

static void PM_ReloadClip( int weapon );
static void PM_WeaponAddTime(int interval);

/*
===============
PM_BeginWeaponChange
===============
*/
/* Windows TC:E30008e90. Shared by server and client prediction. */
void PM_BeginWeaponChange(int oldweapon,int newweapon,qboolean reload) {
    qboolean alt;
    int bodyEvent=ANIM_ET_DROPWEAPON;
    if(pm->ps->pm_flags & PMF_RESPAWNED) return;
    if(newweapon<=0 || newweapon>=TCE_MAX_WEAPONS) return;
    if(!COM_BitCheck(pm->ps->weapons,newweapon)) return;
    if(pm->ps->weaponstate==WEAPON_DROPPING || pm->ps->weaponstate==WEAPON_DROPPING_TORELOAD) return;
    if(pm->ps->weaponDelay || pm->ps->grenadeTimeLeft>0) return;
    pm->ps->nextWeapon=newweapon;
    alt=newweapon==weapAlts[oldweapon];
    switch(newweapon) {
    case 4:case 9:case 15:case 30:
        pm->ps->grenadeTimeLeft=0;
        PM_AddEvent(EV_CHANGE_WEAPON);break;
    case 23:case 24:
        if(!alt) PM_AddEvent(EV_CHANGE_WEAPON);
        break;
    case 60:
        if((pm->ps->eFlags & EF_PRONE) || pm->waterlevel==3) return;
        PM_AddEvent(EV_CHANGE_WEAPON);break;
    default:PM_AddEvent(reload?EV_CHANGE_WEAPON_2:EV_CHANGE_WEAPON);break;
    }
    /* Event emission and animation selection use separate native reads. */
    alt=newweapon==weapAlts[oldweapon];
    PM_StartWeaponAnim(alt ? PM_AltSwitchFromForWeapon(oldweapon) : PM_DropAnimForWeapon(oldweapon));
    alt=newweapon==weapAlts[oldweapon];
    if(alt) {
        switch(oldweapon) {
        case 14:case 52:
            bodyEvent=(pm->ps->eFlags & EF_PRONE)?ANIM_ET_UNDO_ALT_WEAPON_MODE_PRONE:ANIM_ET_UNDO_ALT_WEAPON_MODE;
            break;
        case 23:case 24:
            if(!pm->ps->ammoclip[newweapon] && pm->ps->ammo[newweapon]) PM_ReloadClip(newweapon);
            break;
        case 31:case 35: {
            vec3_t axis[3];
            VectorCopy(pml.forward,axis[0]);VectorCopy(pml.right,axis[2]);
            CrossProduct(axis[0],axis[2],axis[1]);
            AxisToAngles(axis,pm->pmext->mountedWeaponAngles);
            break;
        }
        }
    }
    BG_AnimScriptEvent(pm->ps,pm->character->animModelInfo,bodyEvent,qfalse,qfalse);
    pm->ps->weaponstate=reload?WEAPON_DROPPING_TORELOAD:WEAPON_DROPPING;
    PM_WeaponAddTime(250);
}




/*
===============
PM_FinishWeaponChange
===============
*/
static void PM_FinishWeaponChange(void) {
    int oldweapon,newweapon=pm->ps->nextWeapon,bodyEvent;
    qboolean altSwitch=qfalse,animate=qtrue,alt;
    if(newweapon<0 || newweapon>=TCE_MAX_WEAPONS) newweapon=0;
    if(!COM_BitCheck(pm->ps->weapons,newweapon)) newweapon=0;
    oldweapon=pm->ps->weapon;
    pm->ps->weapon=newweapon;
    pm->ps->weaponstate=pm->ps->weaponstate==WEAPON_DROPPING_TORELOAD?WEAPON_RAISING_TORELOAD:WEAPON_RAISING;
    switch(newweapon) {
    case 2:case 7:pm->pmext->silencedSideArm &= ~1;break;
    case 14:case 52:pm->pmext->silencedSideArm |= 1;break;
    case 23:case 24:pm->pmext->silencedSideArm &= ~2;break;
    case 55:case 56:pm->pmext->silencedSideArm |= 2;break;
    case 57:case 58:case 59:pm->ps->aimSpreadScale=255;pm->ps->aimSpreadScaleFloat=255;break;
    }
    if(oldweapon==newweapon) return;
    alt=weapAlts[oldweapon]==newweapon;
    switch(newweapon) {
    case 2:case 7:case 14:case 35:case 52:case 55:case 56:case 60:
        altSwitch=alt;break;
    case 23:case 24:
        if(alt) {
            altSwitch=qtrue;
            if(!pm->ps->ammoclip[BG_FindAmmoForWeapon(oldweapon)]) animate=qfalse;
        }
        break;
    }
    PM_WeaponAddTime(250);
    BG_UpdateConditionValue(pm->ps->clientNum,ANIM_COND_WEAPON,newweapon,qtrue);
    if(!animate) return;
    if(altSwitch) bodyEvent=(pm->ps->eFlags & EF_PRONE)?ANIM_ET_DO_ALT_WEAPON_MODE_PRONE:ANIM_ET_DO_ALT_WEAPON_MODE;
    else bodyEvent=(pm->ps->eFlags & EF_PRONE)?ANIM_ET_RAISEWEAPONPRONE:ANIM_ET_RAISEWEAPON;
    BG_AnimScriptEvent(pm->ps,pm->character->animModelInfo,bodyEvent,qfalse,qfalse);
    /* The body-animation callback precedes this fresh alternate-pair lookup. */
    PM_StartWeaponAnim(weapAlts[oldweapon] == newweapon ?
        PM_AltSwitchToForWeapon(newweapon) : PM_RaiseAnimForWeapon(newweapon));
}




/*
==============
PM_ReloadClip
==============
*/
static void PM_ReloadClip(int weapon) {
    do {
        int reserve = pm->ps->ammo[BG_FindAmmoForWeapon(weapon)];
        int move;

        /* Retain the original lookup before reading the weapon definition. */
        (void)BG_FindClipForWeapon(weapon);
        move = weaponDef[weapon].maxclip;
        if (reserve < move) {
            move = reserve;
        }
        if (weaponDef[weapon].singleReload) {
            if (move >= 2) {
                move = 1;
            }
            if (move) {
                int index = BG_FindAmmoForWeapon(weapon);
                ((unsigned int *)pm->ps->ammo)[index] -= (unsigned int)move;
                index = BG_FindClipForWeapon(weapon);
                ((unsigned int *)pm->ps->ammoclip)[index] += (unsigned int)move;
            }
        } else if (move) {
            int index = BG_FindAmmoForWeapon(weapon);
            ((unsigned int *)pm->ps->ammo)[index] -= (unsigned int)move;
            index = BG_FindClipForWeapon(weapon);
            pm->ps->ammoclip[index] = move;
        }
        if (!BG_IsAkimboWeapon(weapon)) {
            return;
        }
        weapon = BG_AkimboSidearm(weapon);
    } while (1);
}

/*
==============
PM_FinishWeaponReload
==============
*/

/* TC qagame2003b1b0: refill, publish READY, then select the idle animation. */
static void PM_FinishWeaponReload(void) {
    int weapon=pm->ps->weapon;
    if (weapon < 0 || weapon >= TCE_MAX_WEAPONS) return;
    if (!weaponDef[weapon].singleReload && pm->ps->weaponstate != 12)
        PM_ReloadClip(weapon);
    pm->ps->weaponstate = WEAPON_READY;
    PM_StartWeaponAnim(PM_IdleAnimForWeapon(pm->ps->weapon));
}

/*
==============
PM_CheckforReload
==============
*/
/* Whole TC30009350. Original legacy-limit table is only used by IDs57..59;
 * all other paths use TC weapon definitions, regardless of parser readiness. */
void PM_ReloadSingleRound(int weapon) {
    if (weapon < 0 || weapon >= TCE_MAX_WEAPONS) return;
    if ((pm->ps->weaponstate != 9 && pm->ps->weaponstate != 10) ||
        !weaponDef[weapon].singleReload) return;
    if (weapon != 4 && weapon != 9 && weapon != 15)
        BG_AnimScriptEvent(pm->ps,pm->character->animModelInfo,ANIM_ET_RELOAD,qfalse,qtrue);
    PM_ContinueWeaponAnim(8);
    *(unsigned int *)&pm->ps->weaponTime += (unsigned int)weaponDef[weapon].maxHeat;
    pm->ps->weaponstate = 9;
    PM_AddEvent(EV_TCE_RELOAD_CYCLE);
}

void PM_CheckForReload(int weapon) {
    int clipIndex, ammoIndex, currentClip, maximumClip;
    if (weapon < 0 || weapon >= TCE_MAX_WEAPONS) return;
    if (weaponDef[weapon].singleReload && pm->ps->weaponstate == 9 &&
        pm->ps->weaponTime <= 0 &&
        (!pm->ps->ammo[TCE_BG_FindClipForWeapon(weapon)] ||
         pm->ps->ammoclip[TCE_BG_FindClipForWeapon(weapon)] >= weaponDef[weapon].maxclip ||
         !(pm->cmd.wbuttons & WBUTTON_RELOAD))) {
        BG_AnimScriptEvent(pm->ps,pm->character->animModelInfo,ANIM_ET_RELOAD,qfalse,qtrue);
        PM_ContinueWeaponAnim(9);
        *(unsigned int *)&pm->ps->weaponTime += (unsigned int)weaponDef[weapon].coolRate;
        pm->ps->weaponstate = 11;
        PM_AddEvent(EV_TCE_RELOAD_CYCLE + 1);
    }
    if (weaponDef[weapon].bolt && pm->ps->weaponstate == 9 && pm->ps->weaponTime <= 0) {
        BG_AnimScriptEvent(pm->ps,pm->character->animModelInfo,ANIM_ET_RELOAD,qfalse,qtrue);
        PM_ContinueWeaponAnim(9);
        *(unsigned int *)&pm->ps->weaponTime += (unsigned int)weaponDef[weapon].coolRate;
        pm->ps->weaponstate = 11;
        PM_AddEvent(EV_TCE_RELOAD_CYCLE + 3);
    }
    if (pm->noWeapClips || weapon == 55 || weapon == 56) return;
    switch (pm->ps->weaponstate) {
    case 1: case 2: case 3: case 4: case 5: case 6: case 9:
        if (weaponDef[weapon].singleReload && pm->ps->weaponTime <= 0 &&
            (pm->cmd.wbuttons & WBUTTON_RELOAD) &&
            pm->ps->ammo[TCE_BG_FindAmmoForWeapon(weapon)] &&
            pm->ps->ammoclip[TCE_BG_FindClipForWeapon(weapon)] < weaponDef[weapon].maxclip) {
            PM_ReloadSingleRound(weapon);
            PM_ReloadClip(weapon);
        }
        return;
    case 10:
        if (weaponDef[weapon].singleReload && pm->ps->weaponTime <= 0) {
            PM_ReloadSingleRound(weapon);
            PM_ReloadClip(weapon);
        }
        return;
    default:
        clipIndex = TCE_BG_FindClipForWeapon(weapon);
        ammoIndex = TCE_BG_FindAmmoForWeapon(weapon);
        if (weapon >= 57 && weapon <= 59) {
            /* Legacy scoped slots use the ammo table, not weaponDef. */
            if (!(pm->cmd.wbuttons & WBUTTON_RELOAD) || !pm->ps->ammo[ammoIndex] ||
                pm->ps->ammoclip[clipIndex] >= GetAmmoTableData(weapon)->maxclip) return;
            PM_BeginWeaponChange(weapon,weapAlts[weapon],qtrue);
            return;
        }
        if (pm->ps->weaponTime > 0 || !(pm->cmd.wbuttons & WBUTTON_RELOAD) ||
            !pm->ps->ammo[ammoIndex] || (pm->cmd.buttons & BUTTON_ATTACK) ||
            (pm->ps->pm_flags & 0x400)) return;
        currentClip = pm->ps->ammoclip[clipIndex];
        maximumClip = weaponDef[weapon].maxclip;
        if (TCE_BG_IsAkimboWeapon(weapon)) {
            int sideClip = TCE_BG_FindClipForWeapon(TCE_BG_AkimboSidearm(weapon));
            if (pm->ps->ammoclip[sideClip] < weaponDef[sideClip].maxclip) {
                PM_BeginWeaponReload(weapon);
                return;
            }
        }
        if (currentClip < maximumClip) PM_BeginWeaponReload(weapon);
    }
}

/*
==============
PM_SwitchIfEmpty
==============
*/
/* TC:E Windows30012ee0: flash/frag/bomb/smoke are depleted weapons;
 * mine26 emits no-ammo but retains its inventory bit. */
static void PM_SwitchIfEmpty(void) {
    int weapon = pm->ps->weapon;
    if (weapon != 4 && weapon != 9 && weapon != 15 && weapon != 30 && weapon != 26) return;
    if (pm->ps->ammoclip[BG_FindClipForWeapon(weapon)] ||
        pm->ps->ammo[BG_FindAmmoForWeapon(weapon)]) return;
    if (weapon == 4 || weapon == 9 || weapon == 15 || weapon == 30)
        COM_BitClear(pm->ps->weapons, weapon);
    PM_AddEvent(EV_NOAMMO);
}

/*
==============
PM_WeaponUseAmmo
	accounts for clips being used/not used
==============
*/
void PM_WeaponUseAmmo( int wp, int amount ) {
    /* TC ammo layout applies before parser completion as well. */
    TCE_PM_WeaponUseAmmo(wp, amount, pm->noWeapClips, pm->ps->ammo, pm->ps->ammoclip);
}


/*
==============
PM_WeaponAmmoAvailable
	accounts for clips being used/not used
==============
*/
int PM_WeaponAmmoAvailable( int wp ) {
    /* TC ammo layout applies before parser completion as well. */
    return TCE_PM_WeaponAmmoAvailable(wp, pm->noWeapClips, pm->ps->ammo, pm->ps->ammoclip);
}

/*
==============
PM_WeaponClipEmpty
	accounts for clips being used/not used
==============
*/
int PM_WeaponClipEmpty( int wp ) {
    /* TC ammo layout applies before parser completion as well. */
    return TCE_PM_WeaponClipEmpty(wp, pm->noWeapClips, pm->ps->ammo, pm->ps->ammoclip);
}


/*
==============
PM_CoolWeapons
==============
*/
/* Original legacy ammo-table thermal columns300968d0/d4, stride84.
 * Only TC slots31 (MG42) and34 (mounted dummy) have thermal storage. */
static int PM_TCEMaxHeat(int weapon) {
    return GetAmmoTableData(weapon)->maxHeat;
}
static int PM_TCECoolRate(int weapon) {
    return GetAmmoTableData(weapon)->coolRate;
}

/* Original cooling keeps its arithmetic value until integer conversion.
 * Long-double expressions preserve the operation order; generated x87 precision
 * and exception behavior still require separate compiler/ABI verification. */
void PM_CoolWeapons(void) {
    int weapon;
    playerState_t *state;
    long double display;
    double roundedDisplay;

    for (weapon = 0; weapon < 64; ++weapon) {
        int heat, rate;
        qboolean doubleCooling;
        long double cooling;

        if (!COM_BitCheck(pm->ps->weapons, weapon)) continue;
        state = pm->ps;
        heat = state->weapHeat[weapon];
        if (!heat) continue;
        doubleCooling = pm->skill[SK_HEAVY_WEAPONS] >= 2 &&
            state->stats[STAT_PLAYER_CLASS] == PC_SOLDIER;
        rate = PM_TCECoolRate(weapon);
#if defined(__linux__)
        cooling = (long double)rate;
        if (doubleCooling) cooling += cooling;
        cooling *= (long double)pml.frametime;
#else
        cooling = (long double)rate * (long double)pml.frametime;
        if (doubleCooling) cooling += cooling;
#endif
        state->weapHeat[weapon] = TCE_TruncateNativeInteger(
            (long double)heat - cooling);
        state = pm->ps;
        if (state->weapHeat[weapon] < 0) state->weapHeat[weapon] = 0;
    }

    state = pm->ps;
    weapon = state->weapon;
    if (!weapon) return;
    if (state->persistant[PERS_HWEAPON_USE] ||
        (state->eFlags & EF_MOUNTEDTANK)) {
#if defined(__linux__)
        display = (long double)state->weapHeat[34] *
            (long double)0.17000000178813934326171875f;
#else
        display = (long double)state->weapHeat[34] *
            (long double)0.000666666659526526927947998046875f;
        display *= (long double)255.0f;
#endif
    } else {
        int maxHeat = PM_TCEMaxHeat(weapon);
        if (!maxHeat) {
            state->curWeapHeat = 0;
            return;
        }
#if defined(__linux__)
        display = 1.0L / (long double)maxHeat;
        display *= (long double)state->weapHeat[weapon];
#else
        display = (long double)state->weapHeat[weapon] / (long double)maxHeat;
#endif
        display *= (long double)255.0f;
    }
#if defined(__linux__)
    roundedDisplay = (double)floorl(display);
#else
    roundedDisplay = floor((double)display);
    state = pm->ps;
#endif
    state->curWeapHeat = TCE_TruncateNativeInteger((long double)roundedDisplay);
}


/*
==============
PM_AdjustAimSpreadScale
==============
*/
//#define	AIMSPREAD_DECREASE_RATE		300.0f
#define	AIMSPREAD_DECREASE_RATE		200.0f		// (SA) when I made the increase/decrease floats (so slower weapon recover could happen for scoped weaps) the average rate increased significantly
#define	AIMSPREAD_INCREASE_RATE		800.0f
#define	AIMSPREAD_VIEWRATE_MIN		30.0f		// degrees per second
#define	AIMSPREAD_VIEWRATE_RANGE	120.0f		// degrees per second


#if defined(_MSC_VER) && defined(_M_IX86)
static void PM_WeaponTruncateST0(void);
#endif

/* PmoveSingle 3000c180: movement/reload can cancel active aim after PM_Weapon. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
/* Native TCE PmoveSingle aim-cancellation tail; controller integration remains separate. */
static qboolean PM_TCECancelAim(void) {
    playerState_t *ps = pm->ps;
    int flags = ps->stats[STAT_TCE_WEAPON_FLAGS];
    int state = ps->weaponstate;
    int seed, phase;
    long double random, angle, wave;
#if defined(__linux__)
    playerState_t *phasePlayer, *offsetPlayer;
    volatile float storedAngle;
#endif

    if (!(flags & 4)) return qfalse;
    if (state != WEAPON_DROPPING && !(state == WEAPON_RAISING && !(flags & 8)) &&
        state != WEAPON_RELOADING && state != 10 && state != 11 && pm->waterlevel <= 2 &&
        (pm->waterlevel <= 0 || ps->groundEntityNum != ENTITYNUM_NONE) &&
        ps->velocity[2] >= -360.0f && !TCE_FloatIsNaN(ps->velocity[2]) &&
        (!(pm->cmd.buttons & BUTTON_SPRINT) || (!pm->cmd.forwardmove && !pm->cmd.rightmove) ||
            (ps->pm_flags & PMF_DUCKED) || (ps->eFlags & EF_PRONE)) &&
        !pml.ladder && ps->stats[STAT_HEALTH] > 0 && !(flags & 0x1000) &&
        PM_ProneTimeValue((unsigned)pm->cmd.serverTime - (unsigned)pm->pmext->proneTime) >= 200 &&
        PM_ProneTimeValue((unsigned)pm->cmd.serverTime + (unsigned)pm->pmext->proneTime) >= 200 &&
        !(ps->eFlags & EF_PRONE_MOVING)) return qfalse;

    ps->stats[STAT_TCE_WEAPON_FLAGS] = flags & ~4;
    if (pm->ps->weaponstate == WEAPON_DROPPING &&
        (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 8))
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~8;
    if ((pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 8) && !(pm->ps->persistant[14] & 8))
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~8;
    pm->ps->aimSpreadScaleFloat = (float)((long double)pm->ps->aimSpreadScaleFloat + 255.0f);
    pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
    if (pm->ps->weaponstate != WEAPON_FIRING) {
        seed = pm->ps->stats[STAT_TCE_SHOT_SEED];
#if defined(__linux__)
        phasePlayer = pm->ps;
        random = Q_random(&seed);
        phasePlayer->stats[STAT_TCE_AIM_PHASE] = TCE_TruncateNativeInteger(random * 1000.0f);
#else
        random = Q_random(&seed);
        pm->ps->stats[STAT_TCE_AIM_PHASE] = TCE_TruncateNativeInteger(random * 1000.0f);
#endif
        phase = pm->ps->stats[STAT_TCE_AIM_PHASE];
        if (phase > 1000) pm->ps->stats[STAT_TCE_AIM_PHASE] = phase - 1000;
        /* Original re-reads the definition and aim bit after the random call. */
        if (!weaponDef[pm->ps->weapon].noTacMode &&
            (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4)) {
#if defined(__linux__)
            /* ELF stores the double-turn product as float before hardware trig. */
            storedAngle = (float)((long double)Q_random(&seed) * 6.2831853071795862);
            angle = storedAngle;
            offsetPlayer = pm->ps;
            wave = cosl(angle);
            offsetPlayer->holdable[5] =
                TCE_TruncateNativeInteger((wave + wave) * 100.0f + 2000.0f);
            offsetPlayer = pm->ps;
            wave = sinl(angle);
            offsetPlayer->holdable[6] = TCE_TruncateNativeInteger(wave * 400.0f + 2000.0f);
#else
            int pitch;
            angle = (long double)Q_random(&seed) * 6.2831854820251465f;
            pitch = TCE_TruncateNativeInteger(cosl(angle) * 200.0 + 2000.0);
            wave = sinl(angle);
            pm->ps->holdable[5] = pitch;
            pm->ps->holdable[6] = TCE_TruncateNativeInteger(wave * 400.0 + 2000.0);
#endif
        }
        pm->ps->stats[STAT_TCE_SHOT_SEED] = (unsigned)seed & 0xffffu;
    }
    PM_AddEvent(EV_TCE_TOGGLE_AIMING);
    return qtrue;
}
#else
static qboolean PM_TCECancelAim(void) {
    playerState_t *ps = pm->ps;
    int flags = ps->stats[STAT_TCE_WEAPON_FLAGS];
    int state = ps->weaponstate;
    if (!(flags & 4)) return qfalse;
    if (state != WEAPON_DROPPING && !(state == WEAPON_RAISING && !(flags & 8)) &&
        state != WEAPON_RELOADING && state != 10 && state != 11 && pm->waterlevel < 3 &&
        (pm->waterlevel < 1 || ps->groundEntityNum != ENTITYNUM_NONE) && ps->velocity[2] >= -360.0f &&
        (!(pm->cmd.buttons & BUTTON_SPRINT) || (!pm->cmd.forwardmove && !pm->cmd.rightmove) ||
         (ps->pm_flags & PMF_DUCKED) || (ps->eFlags & EF_PRONE)) && !pml.ladder &&
        ps->stats[STAT_HEALTH] > 0 && !(flags & 0x1000) &&
        pm->cmd.serverTime - pm->pmext->proneTime >= 200 &&
        pm->cmd.serverTime + pm->pmext->proneTime >= 200 && !(ps->eFlags & EF_PRONE_MOVING)) return qfalse;
    ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~4;
    if (state == WEAPON_DROPPING || !(ps->persistant[14] & 8)) ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~8;
    ps->aimSpreadScaleFloat += 255.0f;
    ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
    if (state != WEAPON_FIRING) {
        int seed = ps->stats[STAT_TCE_SHOT_SEED];
        ps->stats[STAT_TCE_AIM_PHASE] = (int)(Q_random(&seed) * 1000.0f);
        if (ps->stats[STAT_TCE_AIM_PHASE] > 1000) ps->stats[STAT_TCE_AIM_PHASE] -= 1000;
        ps->stats[STAT_TCE_SHOT_SEED] = seed & 0xffff;
    }
    PM_AddEvent(EV_TCE_TOGGLE_AIMING);
    return qtrue;
}

#endif

#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
/* TCE movement/shot instability controller. Explicit stores model the
 * native value path; compiled x87 precision and conversion ABI remain open. */
typedef char aimRecoveryLayout[(sizeof(tce_weaponDef_t) == 0x1cc &&
    offsetof(tce_weaponDef_t, unknown_0f8) + 4 * sizeof(int) == 0x108 &&
    offsetof(tce_weaponDef_t, unknown_0f8) + 5 * sizeof(int) == 0x10c &&
    offsetof(tce_weaponDef_t, scoped) == 0x174) ? 1 : -1];
void PM_AdjustAimSpreadScale(void) {
    playerState_t *ps = pm->ps;
    volatile float seconds, decrease;
#if defined(__linux__)
    volatile float storedMotion;
    long double increase;
    playerState_t *phasePlayer;
#else
    volatile float increase;
#endif
    long double motion, rate, excess, value;
    unsigned elapsedBits;
    int elapsed, recovery, phaseAdvance, seed, i;
    qboolean postureChanged = qfalse;

    if (ps->eFlags & EF_ZOOMING) {
        ps->aimSpreadScale = 255;
        pm->ps->aimSpreadScaleFloat = 255.0f;
        return;
    }
    elapsedBits = (unsigned)pm->cmd.serverTime - (unsigned)pm->oldcmd.serverTime;
    memcpy(&elapsed, &elapsedBits, sizeof(elapsed));
    seconds = (float)((long double)elapsed * 0.001);
    recovery = weaponDef[ps->weapon].unknown_0f8[4];
    if (recovery) {
        decrease = (float)((long double)recovery * seconds);
        motion = 0.0L;
        for (i = 0; i < 2; ++i) {
            value = (long double)pm->cmd.angles[i] * 0.0054931640625;
            value -= (long double)pm->oldcmd.angles[i] * 0.0054931640625;
            motion += fabsl(value) * 1.5;
#if defined(__linux__)
            storedMotion = (float)motion;
            motion = storedMotion;
#endif
        }
        for (i = 0; i < 2; ++i) {
            motion += fabsl((long double)ps->velocity[i]);
#if defined(__linux__)
            storedMotion = (float)motion;
            motion = storedMotion;
#endif
        }
        if (ps->stats[STAT_TCE_FLAGS] & 0x200) motion *= 0.8f;
#if defined(__linux__)
        rate = (1.0L / seconds) * motion;
#else
        rate = motion / seconds;
#endif
        excess = rate - 30.0f;
#if defined(__linux__)
        increase = excess;
#else
        increase = (float)excess;
#endif
        /* Native C0/C3 clamps negative, zero and unordered values to zero. */
        if (!(excess > 0.0L)) increase = 0.0f;
        else if (increase > 120.0f) increase = 120.0f;
        if (!(rate > 0.0L)) rate = 0.0L;
        else if (rate > 120.0f) rate = 120.0f;
#if defined(__linux__)
        value = (1.0L / 120.0f) * increase;
#else
        value = (long double)increase * 0.008333333767950535f;
#endif
        value *= seconds;
        value *= 3200.0f;
#if defined(__linux__)
        increase = TCE_TruncateNativeInteger(value);
#else
        increase = (float)TCE_TruncateNativeInteger(value);
#endif
        value = rate * 0.008333333767950535f;
        value *= seconds;
        value *= 3200.0f;
        phaseAdvance = TCE_TruncateNativeInteger(value);
    } else {
        increase = 0.0f;
        decrease = 1000.0f;
        phaseAdvance = 0;
    }
    if ((ps->pm_flags & PMF_DUCKED) && !(ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x10)) {
        ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x10;
        pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 10000;
        postureChanged = qtrue;
    } else if (!(ps->pm_flags & PMF_DUCKED) && (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x10)) {
        ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x10;
        pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 10000;
        postureChanged = qtrue;
    }
    ps = pm->ps;
    if (ps->weaponstate != 7) {
        value = (long double)phaseAdvance + phaseAdvance;
        value += ps->stats[STAT_TCE_AIM_PHASE];
        ps->stats[STAT_TCE_AIM_PHASE] = TCE_TruncateNativeInteger(value);
        if (pm->ps->weaponstate != 0 || postureChanged) {
            /* Original randomization uses a local seed and does not write it back. */
            seed = pm->ps->stats[STAT_TCE_SHOT_SEED];
#if defined(__linux__)
            phasePlayer = pm->ps;
            /* The native call spills the pending increase while preserving ps. */
            storedMotion = (float)increase;
            increase = storedMotion;
            value = (long double)Q_random(&seed) * 1000.0f;
            phasePlayer->stats[STAT_TCE_AIM_PHASE] = TCE_TruncateNativeInteger(value);
#else
            value = (long double)Q_random(&seed) * 1000.0f;
            pm->ps->stats[STAT_TCE_AIM_PHASE] = TCE_TruncateNativeInteger(value);
#endif
        }
        if (pm->ps->stats[STAT_TCE_AIM_PHASE] > 1000)
            pm->ps->stats[STAT_TCE_AIM_PHASE] -= 1000;
    }
    ps = pm->ps;
    if ((ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) &&
        TCE_FloatIsZeroOrNaN(weaponDef[ps->weapon].scoped)) {
#if defined(__linux__)
        increase *= 0.67f;
#else
        increase = (float)((long double)increase * 0.67f);
#endif
        if (ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] > 200) increase = 0.0f;
    }
    value = (long double)increase - decrease;
    value += ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
    ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = TCE_TruncateNativeInteger(value);
    if (pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] > 1000)
        pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
    else if (pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] < 0)
        pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 0;
    ps = pm->ps;
    value = (long double)weaponDef[ps->weapon].unknown_0f8[5] * seconds;
    value = (long double)ps->stats[STAT_TCE_SHOT_INSTABILITY] - value;
    ps->stats[STAT_TCE_SHOT_INSTABILITY] = TCE_TruncateNativeInteger(value);
    if (pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] > 1000)
        pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] = 1000;
    else if (pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] < 0)
        pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] = 0;
}
#else

void PM_AdjustAimSpreadScale(void) {
    int i;
    /* Portable TC calculation; Linux x87 instruction parity is not claimed. */
        tce_aimState_t state;
        const tce_weaponDef_t *w = &weaponDef[pm->ps->weapon];
        state.zooming = (pm->ps->eFlags & EF_ZOOMING) != 0;
        state.ducked = (pm->ps->pm_flags & PMF_DUCKED) != 0;
        state.weaponState = pm->ps->weaponstate;
        state.commandTime = pm->cmd.serverTime;
        state.oldCommandTime = pm->oldcmd.serverTime;
        for (i = 0; i < 2; ++i) {
            state.angles[i] = pm->cmd.angles[i];
            state.oldAngles[i] = pm->oldcmd.angles[i];
            state.velocity[i] = pm->ps->velocity[i];
        }
        state.movementRecovery = w->unknown_0f8[4];
        state.shotRecovery = w->unknown_0f8[5];
        state.scoped = w->scoped;
        state.seed = pm->ps->stats[STAT_TCE_SHOT_SEED];
        state.weaponFlags = pm->ps->stats[STAT_TCE_WEAPON_FLAGS];
        state.movementFlags = pm->ps->stats[STAT_TCE_FLAGS];
        state.movementInstability = pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
        state.shotInstability = pm->ps->stats[STAT_TCE_SHOT_INSTABILITY];
        state.phase = pm->ps->stats[STAT_TCE_AIM_PHASE];
        state.aimSpreadFloat = pm->ps->aimSpreadScaleFloat;
        state.aimSpread = pm->ps->aimSpreadScale;
        TCE_PM_AdjustAimSpreadScale(&state);
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] = state.weaponFlags;
        pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = state.movementInstability;
        pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] = state.shotInstability;
        pm->ps->stats[STAT_TCE_AIM_PHASE] = state.phase;
        pm->ps->aimSpreadScaleFloat = state.aimSpreadFloat;
        pm->ps->aimSpreadScale = state.aimSpread;
}
#endif

#define weaponstateFiring (pm->ps->weaponstate == WEAPON_FIRING || pm->ps->weaponstate == WEAPON_FIRINGALT)

#define GRENADE_DELAY	250

/*
==============
PM_Weapon

Generates weapon events and modifes the weapon counter
==============
*/

#define VENOM_LOW_IDLE	WEAP_IDLE1
#define VENOM_HI_IDLE	WEAP_IDLE2
#define VENOM_RAISE		WEAP_ATTACK1
#define VENOM_ATTACK	WEAP_ATTACK2
#define VENOM_LOWER		WEAP_ATTACK_LASTSHOT

//#define DO_WEAPON_DBG 1

#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) void PM_WeaponTruncateST0(void) {
    __asm {
        push ebp
        mov ebp, esp
        add esp, -12 /* Original flags already observable at the following FWAIT. */
        fwait
        fnstcw word ptr [ebp-2]
        fwait
        mov ax, word ptr [ebp-2]
        or ah, 0ch
        mov word ptr [ebp-4], ax
        fldcw word ptr [ebp-4]
        fistp qword ptr [ebp-12]
        fldcw word ptr [ebp-2]
        mov eax, dword ptr [ebp-12]
        mov edx, dword ptr [ebp-8]
        leave
        ret
    }
}


#endif

/* Original random recoil expressions; only the final value is binary32.
 * Linux has folded pitch multipliers in its own constant pool. */
static void PM_WeaponRandomPitch(float *destination, float scale) {
    int randomBits = rand() & 0x7fff;
    long double value;
#if defined(_WIN32)
    value = (long double)randomBits * 3.0518509447574615e-05f * scale;
#else
    float multiplier = scale == 0.03750000149011612f ? 1.1444441270214156e-06f :
        scale == 0.15000000596046448f ? 4.5777765080856625e-06f : 2.0599993604264455e-06f;
    value = (long double)randomBits * multiplier;
#endif
    *destination = (float)value;
}

static void PM_WeaponRandomYaw(float *destination, double scale) {
    int randomBits = rand() & 0x7fff;
    long double value = (long double)randomBits * 3.0518509447574615e-05f - 0.5L;
    *destination = (float)((value + value) * scale);
}

/* Original inlined class-charge comparison: threshold > elapsed, ordered.
 * C models the held product; compiled intermediate precision remains open. */
static qboolean PM_WeaponChargePending(int elapsed, int chargeTime, float fraction) {
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
    return (long double)chargeTime * fraction > (long double)elapsed;
#else
    return elapsed < chargeTime * fraction;
#endif
}

/* Advance the command timer with the protocol's 32-bit wrap. */
static void PM_WeaponAddTime(int interval) {
    unsigned bits = (unsigned)pm->ps->weaponTime + (unsigned)interval;
    memcpy(&pm->ps->weaponTime, &bits, sizeof(bits));
}

#if defined(_MSC_VER) && defined(_M_IX86) && (defined(GAMEDLL) || defined(CGAMEDLL))
/* Native weapon decision protocol bounds retained after C reconstruction. */
typedef char attack837Protocol[(offsetof(pmove_t,ps)==0 && SK_HEAVY_WEAPONS==5 && SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS==6 && SK_EXPLOSIVES_AND_CONSTRUCTION==1 && SK_SIGNALS==3 && SK_FIRST_AID==2 && PC_FIELDOPS==3 && WEAPON_READY==0 && BUTTON_ATTACK==1) ? 1 : -1];
#endif
#if (defined(_MSC_VER) && defined(_M_IX86) && (defined(GAMEDLL) || defined(CGAMEDLL))) || (defined(__linux__) && !defined(Q3_VM))
static qboolean PM_TCEWeaponEligibility837(qboolean delayedFire) {
    int weapon = pm->ps->weapon;
    int elapsed = PM_ProneTimeValue((unsigned)pm->cmd.serverTime - (unsigned)pm->ps->classWeaponTime);
    qboolean grenade;

    if (!weapon) return qtrue;
    /* Preserve the existing SDK-derived charge gates; no new class behavior. */
    if (weapon == 65) {
        if (pm->ps->eFlags & EF_PRONE) return qtrue;
        if (pm->skill[SK_HEAVY_WEAPONS] >= 1) {
            if (PM_WeaponChargePending(elapsed, pm->soldierChargeTime, 0.66f)) return qtrue;
        } else if (elapsed < pm->soldierChargeTime) return qtrue;
    }
    if ((weapon == 55 || weapon == 56) &&
        PM_WeaponChargePending(elapsed, pm->engineerChargeTime, 0.5f)) return qtrue;
    if (weapon == 60) {
        if (PM_WeaponChargePending(elapsed, pm->soldierChargeTime,
            pm->skill[SK_HEAVY_WEAPONS] >= 1 ? 0.35f : 0.5f)) return qtrue;
        if (!delayedFire) pm->ps->weaponstate = WEAPON_READY;
    }
    weapon = pm->ps->weapon;
    if (weapon == 27) {
        if (pm->skill[SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS] >= 2) {
            if (PM_WeaponChargePending(elapsed, pm->covertopsChargeTime, 0.66f)) return qtrue;
        } else if (elapsed < pm->covertopsChargeTime) return qtrue;
    }
    if (weapon == 26 && PM_WeaponChargePending(elapsed, pm->engineerChargeTime,
        pm->skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 2 ? 0.33f : 0.5f)) return qtrue;
    if (weapon == 15) {
        if (pm->skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 3) {
            if (PM_WeaponChargePending(elapsed, pm->engineerChargeTime, 0.66f)) return qtrue;
        } else if (elapsed < pm->engineerChargeTime) return qtrue;
    }
    if ((weapon == 12 && PM_WeaponChargePending(elapsed, pm->ltChargeTime,
            pm->skill[SK_SIGNALS] >= 1 ? 0.15f : 0.25f)) ||
        (weapon == 19 && PM_WeaponChargePending(elapsed, pm->medicChargeTime,
            pm->skill[SK_FIRST_AID] >= 2 ? 0.15f : 0.25f))) {
        if (pm->cmd.buttons & BUTTON_ATTACK)
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, ANIM_ET_NOPOWER, qtrue, qfalse);
        return qtrue;
    }
    if (weapon == 22) {
        if (pm->skill[SK_SIGNALS] >= 2) {
            if (PM_WeaponChargePending(elapsed, pm->ltChargeTime, 0.66f)) return qtrue;
        } else if (elapsed < pm->ltChargeTime) return qtrue;
    }
    if (weapon == 61 && elapsed < pm->medicChargeTime) return qtrue;

    grenade = weapon == 4 || weapon == 9 || weapon == 30;
    if ((!(pm->cmd.buttons & BUTTON_ATTACK) && !delayedFire) ||
        (!TCE_FloatIsZeroOrNaN(pm->ps->leanf) && !grenade &&
            !(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4)) ||
        ((pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x4000) && !(delayedFire && grenade))) {
        pm->ps->weaponTime = 0;
        pm->ps->weaponDelay = 0;
        if (pm->ps->weaponstate == WEAPON_FIRING || pm->ps->weaponstate == WEAPON_FIRINGALT)
            PM_ContinueWeaponAnim(TCE_PM_IdleAnimForWeapon(pm->ps->weapon));
        if (pm->ps->holdable[11] > 0) {
            if (BG_FiremodeWeapon(pm->ps->weapon) && pm->ps->persistant[10] == 1)
                pm->ps->weaponTime = 50;
            pm->ps->holdable[11] = 0;
            pm->ps->stats[STAT_TCE_FLAGS] &= ~8;
        }
        pm->ps->weaponstate = WEAPON_READY;
        return qtrue;
    }
    if (weapon == 35) return qtrue;
    if (pm->ps->eFlags & EF_ZOOMING) {
#ifdef GAMEDLL
        if (pm->ps->stats[STAT_PLAYER_CLASS] == PC_FIELDOPS) {
            PM_WeaponAddTime(500);
            PM_AddEvent(EV_FIRE_WEAPON);
        }
#endif
        return qtrue;
    }
    if (pm->waterlevel == 3 && weapon != 1 && weapon != 4 && weapon != 9 &&
        weapon != 15 && weapon != 26 && weapon != 29 && weapon != 30) {
        PM_AddEvent(EV_NOFIRE_UNDERWATER);
        pm->ps->weaponTime = 500;
        pm->ps->weaponDelay = 0;
        return qtrue;
    }
    return qfalse;
}

#endif

#if defined(__GNUC__) && defined(__i386__) && defined(__linux__)
/* Keep the existing GNU/i386 caller contract while sharing the C decision path. */
static qboolean PM_TCEWeaponEligibilityLinux856(qboolean delayed) {
    return PM_TCEWeaponEligibility837(delayed);
}

#endif

/* TC:E firing pipeline shared by prediction and the server. These are TC gear
 * IDs, not the ET SDK weapon_t aliases. The two legacy columns are the original
 * initial MP ammo values; mutable 84-byte ammo-table loader parity is still open.
 */
typedef struct {
    int fireDelay;
    int nextShot;
} tceLegacyWeaponTiming_t;

static const tceLegacyWeaponTiming_t pm_tceLegacyTiming[TCE_WEAPON_CAPACITY] = {
    {50,0}, {50,500}, {50,150}, {50,50}, {100,1600}, {750,2000}, {50,50}, {50,400},
    {50,75}, {100,1600}, {50,50}, {50,1000}, {50,1000}, {50,1000}, {50,400}, {100,1600},
    {50,0}, {50,0}, {50,0}, {50,0}, {50,0}, {50,0}, {50,0}, {50,400},
    {50,400}, {50,400}, {50,100}, {50,2000}, {722,2000}, {100,2000}, {100,1600}, {50,50},
    {50,400}, {50,75}, {0,0}, {750,1600}, {750,1600}, {50,100}, {50,100}, {50,150},
    {50,350}, {50,75}, {50,100}, {50,100}, {50,80}, {50,75}, {50,500}, {50,700},
    {50,200}, {50,350}, {50,120}, {50,700}, {0,0}, {0,0}, {0,0}, {0,0},
    {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}
};

/* Timers and recoil accumulators in the protocol wrap at 32 bits. */
static int PM_TCEAddWeaponCounter(int value, int change) {
    unsigned bits = (unsigned)value + (unsigned)change;
    int result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static int PM_TCEWeaponAngleChange(int current, int previous) {
    unsigned bits = (unsigned)current - (unsigned)previous;
    int result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static qboolean PM_TCEWeaponIsFiring(void) {
    return pm->ps->weaponstate == WEAPON_FIRING ||
        pm->ps->weaponstate == WEAPON_FIRINGALT;
}

/* Advance countdowns in the signed 32-bit command protocol without signed
 * overflow. This also handles wrap at the original grenade/cadence gates. */
static int PM_TCEElapsedWeaponCounter(int value, int elapsed) {
    unsigned bits = (unsigned)value - (unsigned)elapsed;
    int result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

/* The mounted heat slot is separate from the selected hand-held weapon.
 * Windows calls floor after a stored double; ELF floors the folded product
 * before its double store. Keep those numeric boundaries explicit. */
static void PM_TCECoolMountedWeapon(void) {
    int heat = pm->ps->weapHeat[WP_DUMMY_MG42];
    double display;

    if (!heat) return;
    pm->ps->weapHeat[WP_DUMMY_MG42] = TCE_TruncateNativeInteger(
        (long double)heat - (long double)pml.frametime * 300.0f);
    if (pm->ps->weapHeat[WP_DUMMY_MG42] < 0)
        pm->ps->weapHeat[WP_DUMMY_MG42] = 0;
#if defined(_WIN32)
    display = floor((double)((long double)pm->ps->weapHeat[WP_DUMMY_MG42] *
        0.0006666666595265269f * 255.0f));
#else
    display = (double)floorl((long double)pm->ps->weapHeat[WP_DUMMY_MG42] *
        0.17000000178813934f);
#endif
    pm->ps->curWeapHeat = TCE_TruncateNativeInteger((long double)display);
}

/* True means an original whole-PM_Weapon early exit. Only an ordinary,
 * living, non-spectating player continues into the hand-held weapon phase. */
static qboolean PM_TCEUpdateMountedWeapon(void) {
    enum { mountedMG42 = 1, mountedAAGun = 2 };
    int mounted;
    int interval;

    if (pm->ps->pm_flags & PMF_RESPAWNED) return qtrue;
    if (pm->ps->persistant[PERS_TEAM] == TEAM_SPECTATOR) return qtrue;
    if (pm->ps->stats[STAT_HEALTH] <= 0) return qtrue;

    mounted = pm->ps->persistant[PERS_HWEAPON_USE];
    if (mounted != mountedMG42 && mounted != mountedAAGun &&
        !(pm->ps->eFlags & EF_MOUNTEDTANK)) return qfalse;

    if (mounted != mountedAAGun) PM_TCECoolMountedWeapon();
    if (pm->ps->weaponTime > 0) {
        pm->ps->weaponTime = PM_TCEElapsedWeaponCounter(pm->ps->weaponTime, pml.msec);
        if (pm->ps->weaponTime > 0) return qtrue;
        if (!(pm->cmd.buttons & BUTTON_ATTACK)) {
            pm->ps->weaponTime = 0;
            return qtrue;
        }
    }
    if (!(pm->cmd.buttons & BUTTON_ATTACK)) return qtrue;

    if (mounted == mountedAAGun) {
        PM_AddEvent(EV_FIRE_WEAPON_AAGUN);
        PM_WeaponAddTime(100);
        BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
            ANIM_ET_FIREWEAPON, qfalse, qtrue);
        return qtrue;
    }

    interval = mounted == mountedMG42 && (unsigned)PM_GameType <= 1u ? 100 : 66;
    pm->ps->weapHeat[WP_DUMMY_MG42] = PM_TCEAddWeaponCounter(
        pm->ps->weapHeat[WP_DUMMY_MG42], interval);
    PM_AddEvent(mounted == mountedMG42 ? EV_FIRE_WEAPON_MG42 : EV_FIRE_WEAPON_MOUNTEDMG42);
    /* The MG42 re-reads gametype after the event; its heat and time increments
     * need not use the same value when a callback changes the current state. */
    interval = mounted == mountedMG42 && (unsigned)PM_GameType <= 1u ? 100 : 66;
    PM_WeaponAddTime(interval);
    BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
        ANIM_ET_FIREWEAPON, qfalse, qtrue);
    if (mounted == mountedMG42) pm->ps->viewlocked = 2;
    if (pm->ps->weapHeat[WP_DUMMY_MG42] >= 1500) {
        /* The original changes weaponTime, not heat, before the event. */
        pm->ps->weaponTime = 1500;
        PM_AddEvent(EV_WEAP_OVERHEAT);
        pm->ps->weaponTime = 2000;
    }
    return qtrue;
}


/* Preserve the original clip-query order and saved player-state lifetimes. */
static qboolean PM_TCESelectAkimboShot(void) {
    playerState_t *sidearmState;
    int sidearm, sidearmClip, mainClip, sidearmAmmo, mainAmmo, weapon;
#if !defined(_WIN32)
    playerState_t *mainState;
#endif

    pm->watertype = 0;
    if (!BG_IsAkimboWeapon(pm->ps->weapon)) return qfalse;
    sidearmState = pm->ps;
    sidearm = BG_AkimboSidearm(sidearmState->weapon);
    sidearmClip = BG_FindClipForWeapon(sidearm);
#if defined(_WIN32)
    sidearmAmmo = sidearmState->ammoclip[sidearmClip];
    mainClip = BG_FindClipForWeapon(sidearmState->weapon);
    mainAmmo = sidearmState->ammoclip[mainClip];
    weapon = sidearmState->weapon;
#else
    mainState = pm->ps;
    mainClip = BG_FindClipForWeapon(mainState->weapon);
    sidearmAmmo = sidearmState->ammoclip[sidearmClip];
    mainAmmo = mainState->ammoclip[mainClip];
    weapon = pm->ps->weapon;
#endif
    return BG_AkimboFireSequence(weapon, mainAmmo, sidearmAmmo);
}

/* A callback can change the selected weapon; check the fresh definition. */
static void PM_TCEReconcileWeaponFiremode(void) {
    int mode, weapon;

    PM_CoolWeapons();
    if (!BG_FiremodeWeapon(pm->ps->weapon)) return;
    weapon = pm->ps->weapon;
    if ((unsigned)weapon >= TCE_MAX_WEAPONS) return;
    mode = pm->ps->persistant[10];
    if (mode == 0 && weaponDef[weapon].fullauto < 1) {
        if (weaponDef[weapon].burst > 1) pm->ps->persistant[10] = 1;
    } else if (mode == 1 && weaponDef[weapon].burst < 2) {
        pm->ps->persistant[10] = 0;
    }
}

static void PM_TCERecoverHipRecoil(void) {
    int remaining = pm->ps->stats[STAT_TCE_RECOIL_REMAINDER];
    int recovered;

    if (remaining <= 0) return;
    recovered = TCE_TruncateNativeInteger((long double)pml.msec * 1.3329999446868896f);
    if (remaining < recovered) {
        recovered = remaining;
        pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] = 0;
    } else {
        pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] =
            PM_TCEElapsedWeaponCounter(remaining, recovered);
    }
    pm->ps->delta_angles[PITCH] = TCE_TruncateNativeInteger(
        ((long double)recovered + recovered) + pm->ps->delta_angles[PITCH]);
}

/* sinl expresses the numerical operation. Exact original FSIN/FPU exception
 * and compiler precision behavior remains a separate open boundary. */
static void PM_TCEUpdateWeaponSway(void) {
    enum { weaponSwayPhase = 14 };
    const float instabilityScale = 5.8823530707741156e-05f;
    const float angleUnits = 182.04444885253906f;
#if defined(_WIN32)
    const float frequency1 = 0.00125663704238832f;
    const float frequency2 = 0.0031415927223861217f;
    const float frequency3 = 0.0018849557964131236f;
    const float frequency5 = 0.002199114765971899f;
#else
    const double frequency1 = 0.0012566370801612688;
    const double frequency2 = 0.0031415926535897933;
    const double frequency3 = 0.0018849556670552817;
    const double frequency5 = 0.002199114820062152;
#endif
    int amount, i, phase;
    float baseWave, crossWave, thirdWave, pitchWave, driftWave;
    float amplitude = 0.07000000029802322f;
    long double cubicWave, amplitudeScale, pitch, yawOffset;
    vec3_t angles;

    if (!(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) ||
        (unsigned)pm->ps->weapon >= TCE_MAX_WEAPONS ||
        pm->ps->weapon == TCE_WP_G1 || pm->ps->weapon == TCE_WP_G2 ||
        pm->ps->weapon == TCE_WP_G3) return;

    phase = pm->ps->stats[weaponSwayPhase];
    VectorCopy(pm->ps->viewangles, angles);
    baseWave = (float)sinl((long double)phase * frequency1);
#if !defined(_WIN32)
    phase = pm->ps->stats[weaponSwayPhase];
#endif
    crossWave = (float)sinl((long double)phase * frequency2 + 1.36);
#if !defined(_WIN32)
    phase = pm->ps->stats[weaponSwayPhase];
#endif
    thirdWave = (float)sinl((long double)phase * frequency3 + 0.5f);
#if !defined(_WIN32)
    phase = pm->ps->stats[weaponSwayPhase];
#endif
    pitchWave = (float)sinl((long double)phase * frequency1 + 0.89);
#if !defined(_WIN32)
    phase = pm->ps->stats[weaponSwayPhase];
#endif
    driftWave = (float)sinl((long double)phase * frequency5 + 4.71);
    cubicWave = (long double)baseWave * baseWave * baseWave;
#if defined(_WIN32)
    cubicWave = (float)cubicWave;
#endif
    if (pm->ps->eFlags & EF_CROUCHING) {
        /* ELF spills its held cubic wave before each posture callback. */
#if !defined(_WIN32)
        cubicWave = (float)cubicWave;
#endif
        if (!(VectorLength(pm->ps->velocity) >= 30.0f))
            amplitude = 0.04500000178813934f;
    }
    if (pm->ps->eFlags & EF_PRONE) {
#if !defined(_WIN32)
        cubicWave = (float)cubicWave;
#endif
        if (!(VectorLength(pm->ps->velocity) >= 30.0f))
            amplitude = 0.02500000037252903f;
    }

    if ((unsigned)pm->ps->weapon >= TCE_MAX_WEAPONS) return;
    amplitudeScale = (long double)amplitude * 0.20000000298023224f;
    amount = pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
#if defined(_WIN32)
    {
        float bias = 0.004000000189989805f;
        long double instability;
        float storedInstability;

        if (weaponDef[pm->ps->weapon].scoped > 0.0f) {
            amount = PM_TCEAddWeaponCounter(pm->ps->stats[STAT_TCE_SHOT_INSTABILITY], amount);
        } else {
            amplitudeScale *= 0.125f;
            bias = 0.00024500000290572643f;
        }
        instability = (long double)amount * instabilityScale;
        storedInstability = (float)instability;
        pitch = (long double)angles[PITCH] -
            (instability * pitchWave + amplitudeScale * cubicWave);
        yawOffset = (((long double)driftWave + thirdWave) + crossWave) + cubicWave;
        yawOffset *= (long double)storedInstability + bias;
    }
#else
    {
        long double bias = 0.004000000189989805f;
        float storedPitchProduct;

        if (weaponDef[pm->ps->weapon].scoped > 0.0f) {
            amount = PM_TCEAddWeaponCounter(pm->ps->stats[STAT_TCE_SHOT_INSTABILITY], amount);
        } else {
            amplitudeScale *= 0.125f;
            bias *= 0.061250001192092896f;
            amount = pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
        }
        storedPitchProduct = (float)((long double)pitchWave * amount);
        pitch = (long double)angles[PITCH] -
            ((long double)instabilityScale * storedPitchProduct + amplitudeScale * cubicWave);
        yawOffset = ((cubicWave + crossWave) + thirdWave) + driftWave;
        yawOffset *= (long double)amount * instabilityScale + bias;
    }
#endif
    angles[PITCH] = (float)pitch;
    angles[YAW] = (float)(yawOffset + angles[YAW]);
    for (i = 0; i < 3; ++i) {
        int converted = TCE_TruncateNativeInteger((long double)angles[i] * angleUnits);
        pm->ps->delta_angles[i] = PM_TCEWeaponAngleChange(converted & 65535, pm->cmd.angles[i]);
    }
#if defined(_WIN32)
    pm->ps->viewangles[PITCH] = (float)pitch;
#else
    pm->ps->viewangles[PITCH] = angles[PITCH];
#endif
    pm->ps->viewangles[YAW] = angles[YAW];
    pm->ps->viewangles[ROLL] = angles[ROLL];
    pm->ps->stats[weaponSwayPhase] =
        PM_TCEAddWeaponCounter(pm->ps->stats[weaponSwayPhase], pml.msec);
    if (pm->ps->stats[weaponSwayPhase] > 10000)
        pm->ps->stats[weaponSwayPhase] =
            PM_TCEElapsedWeaponCounter(pm->ps->stats[weaponSwayPhase], 10000);
}

static void PM_TCEApplyQueuedWeaponRecoil(void) {
    if (pm->pmext->weapRecoilTime) {
        pm->ps->delta_angles[PITCH] = TCE_TruncateNativeInteger(
            (long double)pm->ps->delta_angles[PITCH] - pm->pmext->weapRecoilPitch);
        pm->ps->delta_angles[YAW] = TCE_TruncateNativeInteger(
            (long double)pm->ps->delta_angles[YAW] - pm->pmext->weapRecoilYaw);
        pm->pmext->weapRecoilTime = 0;
    }
    PM_TCERecoverHipRecoil();
}


/* Original grenade/shot countdown, mechanical cycling and selection phase.
 * True is a whole PM_Weapon early exit; otherwise publish delayedFire for the
 * following aim/action gates. No native stack/register adapter is required. */
static qboolean PM_TCEUpdateWeaponCycle(qboolean akimboFire, qboolean *delayedResult) {
    qboolean delayedFire = qfalse;
    int weapon = pm->ps->weapon;

    if ((weapon == TCE_WP_G3 || weapon == TCE_WP_G1 ||
         weapon == TCE_WP_DC1 || weapon == TCE_WP_G2) && pm->ps->grenadeTimeLeft > 0) {
        qboolean forceThrow = qfalse;
        int releaseGate;
        if (weapon == TCE_WP_DC1) {
            pm->ps->grenadeTimeLeft = PM_TCEAddWeaponCounter(pm->ps->grenadeTimeLeft, pml.msec);
            if (pm->ps->grenadeTimeLeft < 5000) pm->ps->grenadeTimeLeft = 5000;
        } else {
            pm->ps->grenadeTimeLeft = PM_TCEElapsedWeaponCounter(pm->ps->grenadeTimeLeft, pml.msec);
            if (pm->ps->grenadeTimeLeft <= 100) {
                forceThrow = qtrue;
                pm->ps->grenadeTimeLeft = 100;
            }
        }
        if ((pm->cmd.buttons & BUTTON_ATTACK) && !forceThrow) return qtrue;
        releaseGate = PM_TCEElapsedWeaponCounter(weaponDef[pm->ps->weapon].grenadeTimer, 250);
        if (pm->ps->grenadeTimeLeft >= releaseGate) return qtrue;
        if (pm->ps->weaponDelay == weaponDef[pm->ps->weapon].fireDelayTime || forceThrow) {
            int animation;
            if (pm->ps->eFlags & EF_PRONE)
                animation = akimboFire ? ANIM_ET_FIREWEAPON2PRONE : ANIM_ET_FIREWEAPONPRONE;
            else
                animation = akimboFire ? ANIM_ET_FIREWEAPON2 : ANIM_ET_FIREWEAPON;
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, animation, qfalse, qtrue);
        }
        /* The animation callback precedes the fresh delay read. */
        if (!pm->ps->weaponDelay) delayedFire = qtrue;
    }
    if (pm->ps->weaponDelay > 0) {
        pm->ps->weaponDelay = PM_TCEElapsedWeaponCounter(pm->ps->weaponDelay, pml.msec);
        if (pm->ps->weaponDelay <= 0) {
            pm->ps->weaponDelay = 0;
            delayedFire = qtrue;
        }
    }
    if (pm->ps->weaponstate == WEAPON_RELAXING) {
        pm->ps->weaponstate = WEAPON_READY;
        return qtrue;
    }
    if ((pm->ps->eFlags & EF_PRONE_MOVING) && !delayedFire) {
        pm->ps->stats[STAT_TCE_FLAGS] |= 8;
        return qtrue;
    }
    if (pm->ps->weaponTime > 0) {
        pm->ps->weaponTime = PM_TCEElapsedWeaponCounter(pm->ps->weaponTime, pml.msec);
        if (!(pm->cmd.buttons & BUTTON_ATTACK) && pm->ps->weaponTime < 0)
            pm->ps->weaponTime = 0;
    }

    /* A bolt's clip query occurs before its firing/timer checks. Keep the
     * original callback order and reload the definition after callbacks. */
    if ((unsigned)pm->ps->weapon < TCE_WEAPON_CAPACITY &&
        weaponDef[pm->ps->weapon].bolt && !PM_WeaponClipEmpty(pm->ps->weapon) &&
        pm->ps->weaponstate == WEAPON_FIRING && pm->ps->weaponDelay <= 0 &&
        !delayedFire && pm->ps->weaponTime <= 0) {
        PM_ContinueWeaponAnim(8);
        PM_WeaponAddTime(weaponDef[pm->ps->weapon].unknown_1a0);
        pm->ps->weaponstate = WEAPON_TCE_CYCLE;
        PM_AddEvent(EV_TCE_RELOAD_BOLT);
    }
    if ((unsigned)pm->ps->weapon < TCE_WEAPON_CAPACITY &&
        weaponDef[pm->ps->weapon].pump && pm->ps->weaponstate == WEAPON_FIRING &&
        pm->ps->weaponDelay <= 0 && !delayedFire && pm->ps->weaponTime <= 0) {
        if (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x800) {
            pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x800;
        } else if (pm->ps->persistant[10] && weaponDef[pm->ps->weapon].semiauto) {
            pm->ps->weaponstate = WEAPON_TCE_CYCLE;
            PM_WeaponAddTime(100);
        } else {
            PM_ContinueWeaponAnim(WEAP_ATTACK2);
            PM_WeaponAddTime(weaponDef[pm->ps->weapon].unknown_19c);
            pm->ps->weaponstate = WEAPON_TCE_CYCLE;
            PM_AddEvent(EV_TCE_RELOAD_PUMP2);
        }
    }
    if (pm->ps->weaponstate == WEAPON_FIRING && pm->ps->weaponDelay <= 0 &&
        !delayedFire && pm->ps->weaponTime <= 0)
        pm->ps->holdable[11] = PM_TCEAddWeaponCounter(pm->ps->holdable[11], 1);

    if ((pm->ps->weaponTime <= 0 || (!PM_TCEWeaponIsFiring() && pm->ps->weaponDelay <= 0)) &&
        !delayedFire) {
        if (pm->cmd.weapon < 55 && pm->ps->weapon != pm->cmd.weapon && !(pm->ps->pm_flags & 0x400))
            PM_BeginWeaponChange(pm->ps->weapon, pm->cmd.weapon, qfalse);
        /* Command 56 confirms the selection; it is not a new gear slot. */
        if (pm->cmd.weapon == 56 && (pm->cmd.buttons & BUTTON_ATTACK)) {
            PM_WeaponAddTime(50);
            return qtrue;
        }
    }
    if (pm->cmd.weapon == 55 && pm->ps->weaponTime <= 0 && !delayedFire &&
        pm->ps->weaponstate == WEAPON_READY && (unsigned)pm->ps->weapon < TCE_WEAPON_CAPACITY &&
        BG_FiremodeWeapon(pm->ps->weapon)) {
        if (!pm->ps->persistant[10])
            pm->ps->persistant[10] = weaponDef[pm->ps->weapon].burst <= 1 ? 2 : 1;
        else if (pm->ps->persistant[10] == 1)
            pm->ps->persistant[10] = 2;
        else if (weaponDef[pm->ps->weapon].fullauto || weaponDef[pm->ps->weapon].pump)
            pm->ps->persistant[10] = 0;
        else if (weaponDef[pm->ps->weapon].burst > 1)
            pm->ps->persistant[10] = 1;
        PM_WeaponAddTime(750);
        PM_AddEvent(EV_TCE_FIREMODE);
        return qtrue;
    }
    *delayedResult = delayedFire;
    return qfalse;
}


/* TC aim input and movement/action latches precede the trigger controller. */
enum {
    tceAimActive = 4,
    tceAimRequested = 8,
    tceAimBlocked = 0x1000,
    tceSprintLatched = 0x4000,
    tceAimButtonLatched = 0x8000,
    tceAttackButtonLatched = 0x400
};

static qboolean PM_TCEAimPostureSettled(void) {
    int commandTime = pm->cmd.serverTime;
    int proneTime = pm->pmext->proneTime;
    return PM_TCEElapsedWeaponCounter(commandTime, proneTime) >= 200 &&
        PM_TCEAddWeaponCounter(proneTime, commandTime) >= 200;
}

static void PM_TCEBeginAimTransition(void) {
    int seed;
    long double random;

    pm->ps->stats[STAT_TCE_WEAPON_FLAGS] ^= tceAimActive;
    pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
    if (pm->ps->weaponstate != WEAPON_FIRING) {
        seed = pm->ps->stats[STAT_TCE_SHOT_SEED];
#if defined(_WIN32)
        random = Q_random(&seed);
        pm->ps->stats[STAT_TCE_AIM_PHASE] = TCE_TruncateNativeInteger(random * 1000.0f);
#else
        /* Linux selects the phase destination before the random callback. */
        playerState_t *phasePlayer = pm->ps;
        random = Q_random(&seed);
        phasePlayer->stats[STAT_TCE_AIM_PHASE] = TCE_TruncateNativeInteger(random * 1000.0f);
#endif
        if (pm->ps->stats[STAT_TCE_AIM_PHASE] > 1000)
            pm->ps->stats[STAT_TCE_AIM_PHASE] =
                PM_TCEElapsedWeaponCounter(pm->ps->stats[STAT_TCE_AIM_PHASE], 1000);

        /* A callback must not turn the native64-slot definition lookup into
         * an out-of-bounds access. Such invalid weapons have no invented row. */
        if ((unsigned)pm->ps->weapon < TCE_WEAPON_CAPACITY &&
            !weaponDef[pm->ps->weapon].noTacMode &&
            (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & tceAimActive)) {
            long double angle, wave;
#if defined(_WIN32)
            int pitch;
            angle = (long double)Q_random(&seed) * 6.2831854820251465f;
            pitch = TCE_TruncateNativeInteger(cosl(angle) * 200.0L + 2000.0L);
            wave = sinl(angle);
            pm->ps->holdable[5] = pitch;
            pm->ps->holdable[6] = TCE_TruncateNativeInteger(wave * 400.0L + 2000.0L);
#else
            playerState_t *offsetPlayer;
            /* The ELF controller stores the double-turn product as float,
             * and doubles cosine before its separate100-unit multiplier. */
            angle = (float)((long double)Q_random(&seed) * 6.2831853071795862);
            offsetPlayer = pm->ps;
            wave = cosl(angle);
            offsetPlayer->holdable[5] =
                TCE_TruncateNativeInteger((wave + wave) * 100.0f + 2000.0f);
            offsetPlayer = pm->ps;
            wave = sinl(angle);
            offsetPlayer->holdable[6] = TCE_TruncateNativeInteger(wave * 400.0f + 2000.0f);
#endif
        }
        pm->ps->stats[STAT_TCE_SHOT_SEED] = (unsigned)seed & 0xffffu;
    }
    PM_WeaponAddTime(300);
    PM_AddEvent(EV_TCE_TOGGLE_AIMING);
}

static qboolean PM_TCEUpdateAimInput(qboolean delayedFire) {
    playerState_t *ps = pm->ps;
    int flags = ps->stats[STAT_TCE_WEAPON_FLAGS];
    qboolean pressed = (pm->cmd.wbuttons & WBUTTON_ZOOM) != 0;
    qboolean requested;

    if (!pressed && (flags & tceAimButtonLatched)) {
        ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~tceAimButtonLatched;
    } else {
        requested = pressed && !(flags & tceAimButtonLatched) &&
            ps->weaponTime <= 0 && !delayedFire && ps->weaponstate == WEAPON_READY;
        if (!requested)
            requested = ps->weaponTime <= 0 &&
                (flags & (tceAimActive | tceAimRequested)) == tceAimRequested;
        if (requested && ps->weaponstate != WEAPON_DROPPING &&
            ps->weaponstate != WEAPON_RELOADING &&
            ps->weaponstate != WEAPON_TCE_RELOAD_START &&
            ps->weaponstate != WEAPON_TCE_RELOAD_END &&
            ps->weapon != TCE_WP_K1 && ps->weapon != TCE_ITEM_WEAPON_CLASS_SPECIAL &&
            ps->weapon != TCE_ITEM_WEAPON_MEDIC_HEAL &&
            ps->weapon != TCE_ITEM_WEAPON_MAGICAMMO && ps->weapon != TCE_WP_DC1 &&
            (!(pm->cmd.buttons & BUTTON_SPRINT) ||
             (!pm->cmd.forwardmove && !pm->cmd.rightmove) ||
             (ps->pm_flags & PMF_DUCKED) || (ps->eFlags & EF_PRONE)) &&
            pm->waterlevel <= 2 &&
            (pm->waterlevel <= 0 || ps->groundEntityNum != ENTITYNUM_NONE) &&
            !pml.ladder && ps->velocity[2] >= -360.0f && !(flags & tceAimBlocked) &&
            PM_TCEAimPostureSettled() && !(ps->eFlags & EF_PRONE_MOVING) &&
            ps->weapon != TCE_WP_NONE) {
            if (pressed) {
                ps->stats[STAT_TCE_WEAPON_FLAGS] |= tceAimButtonLatched;
                pm->ps->stats[STAT_TCE_WEAPON_FLAGS] ^= tceAimRequested;
            }
            flags = pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & (tceAimActive | tceAimRequested);
            if (flags == tceAimActive || flags == tceAimRequested) {
                PM_TCEBeginAimTransition();
                return qtrue;
            }
        }
    }
    if (pm->cmd.wbuttons & WBUTTON_ZOOM) {
        flags = pm->ps->stats[STAT_TCE_WEAPON_FLAGS];
        if ((flags & (tceAimButtonLatched | tceAimActive | tceAimRequested)) == tceAimRequested) {
            pm->ps->stats[STAT_TCE_WEAPON_FLAGS] = flags | tceAimButtonLatched;
            pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~tceAimRequested;
        }
    }
    return qfalse;
}

static void PM_TCEUpdateObjectiveAction(void) {
    int hint = pm->ps->serverCursorHint;
    qboolean target = hint == HINT_DISARM ||
        hint == HINT_BREAKABLE_DYNAMITE || hint == HINT_ACTIVATE;

    if (!(pm->cmd.buttons & BUTTON_ACTIVATE)) {
        if (pm->ps->pm_flags & PMF_TCE_OBJECTIVE_ACTION) {
            pm->ps->pm_flags &= ~PMF_TCE_OBJECTIVE_ACTION;
            pm->ps->weaponTime = 100;
            PM_AddEvent(EV_TCE_OBJECTIVE_STOP);
        }
        return;
    }
    if (target && pm->ps->weaponTime <= 0 && pm->ps->weaponDelay <= 0 &&
        pm->ps->weaponstate == WEAPON_READY &&
        !(pm->ps->pm_flags & PMF_TCE_OBJECTIVE_ACTION)) {
        pm->ps->pm_flags |= PMF_TCE_OBJECTIVE_ACTION;
        pm->ps->weaponTime = (pm->ps->stats[STAT_TCE_FLAGS] & 2) ? 2000 : 4000;
        if (pm->ps->serverCursorHint == HINT_DISARM)
            pm->ps->weaponTime = (pm->ps->stats[STAT_TCE_FLAGS] & 2) ? 5000 : 10000;
        if (pm->ps->serverCursorHint == HINT_ACTIVATE) PM_AddEvent(EV_TCE_OBJECTIVE_START);
    } else if (pm->ps->pm_flags & PMF_TCE_OBJECTIVE_ACTION) {
        if (pm->ps->weaponTime <= 0) {
            if (hint == HINT_DISARM) PM_AddEvent(EV_TCE_DEFUSE);
            else if (hint == HINT_BREAKABLE_DYNAMITE) PM_AddEvent(EV_TCE_PLANT);
            else if (hint == HINT_ACTIVATE) PM_AddEvent(EV_TCE_OBJECTIVE_COMPLETE);
            /* Completion may publish a new movement/player root. */
            PM_WeaponAddTime(500);
            pm->ps->pm_flags &= ~PMF_TCE_OBJECTIVE_ACTION;
        } else if (!target) {
            pm->ps->pm_flags &= ~PMF_TCE_OBJECTIVE_ACTION;
            pm->ps->weaponDelay = 0;
            pm->ps->weaponTime = 100;
            PM_AddEvent(EV_TCE_OBJECTIVE_STOP);
        }
    }
}

static qboolean PM_TCEUpdateWeaponActions(qboolean delayedFire) {
    if ((unsigned)pm->ps->weapon < TCE_WEAPON_CAPACITY && PM_TCEUpdateAimInput(delayedFire))
        return qtrue;

    if (pml.ladder) {
        if (pm->ps->weaponTime <= 500) pm->ps->weaponTime = 500;
        pm->ps->stats[STAT_TCE_FLAGS] |= 8;
        return qtrue;
    }
    if ((pm->cmd.buttons & BUTTON_SPRINT) && (pm->cmd.forwardmove || pm->cmd.rightmove) &&
        !(pm->ps->pm_flags & PMF_DUCKED) && !(pm->ps->eFlags & EF_PRONE)) {
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= tceSprintLatched;
        if ((pm->ps->weapon == TCE_WP_G1 || pm->ps->weapon == TCE_WP_G2 ||
             pm->ps->weapon == TCE_WP_G3) && delayedFire) {
            pm->ps->weaponTime = 0;
        } else if (pm->ps->weaponstate == WEAPON_READY) {
            if (pm->ps->weaponTime <= 250) pm->ps->weaponTime = 250;
            return qtrue;
        }
    } else if (pm->ps->weaponTime <= 0 &&
               (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & tceSprintLatched)) {
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~tceSprintLatched;
        pm->ps->weaponTime = 250;
    }
    PM_TCEUpdateObjectiveAction();
    if (pm->ps->weaponDelay > 0) return qtrue;
    PM_CheckForReload(pm->ps->weapon);
    if (pm->ps->weaponTime > 0 || pm->ps->weaponDelay > 0) return qtrue;
    if (pm->ps->weaponstate == WEAPON_DROPPING ||
        pm->ps->weaponstate == WEAPON_DROPPING_TORELOAD) {
        PM_FinishWeaponChange();
        pm->ps->pm_flags &= ~tceAttackButtonLatched;
        return qtrue;
    }
    return qfalse;
}


/* Continue a burst after key release, then apply the held-trigger rules. */
enum { tceFiremodeBurst = 1, tceFiremodeSingle = 2 };

static void PM_TCEContinueBurst(void) {
    playerState_t *clipPlayer, *statePlayer;
    int clip;

    if ((pm->cmd.buttons & BUTTON_ATTACK) ||
        !(pm->ps->pm_flags & tceAttackButtonLatched) ||
        !BG_FiremodeWeapon(pm->ps->weapon)) return;
    clipPlayer = pm->ps;
    if ((unsigned)clipPlayer->weapon >= TCE_WEAPON_CAPACITY ||
        clipPlayer->persistant[10] != tceFiremodeBurst ||
        clipPlayer->holdable[11] >= weaponDef[clipPlayer->weapon].burst) return;
    clip = BG_FindClipForWeapon(clipPlayer->weapon);
    if (!clipPlayer->ammoclip[clip]) return;
#if defined(_WIN32)
    /* Windows retains the player across clip lookup for the final gates. */
    statePlayer = clipPlayer;
#else
    statePlayer = pm->ps;
#endif
    if (!(statePlayer->stats[STAT_TCE_FLAGS] & 8) &&
        (statePlayer->weaponstate == WEAPON_FIRING ||
         statePlayer->weaponstate == WEAPON_FIRINGALT))
        pm->cmd.buttons |= BUTTON_ATTACK;
}

static qboolean PM_TCEHeldTriggerStops(playerState_t **timerPlayer) {
    playerState_t *ps;
    int clip, weapon;

    if ((unsigned)pm->ps->weapon >= TCE_WEAPON_CAPACITY) return qfalse;
    if (!BG_FiremodeWeapon(pm->ps->weapon)) {
        ps = pm->ps;
        if ((unsigned)ps->weapon >= TCE_WEAPON_CAPACITY) return qfalse;
        if (weaponDef[ps->weapon].semiauto || weaponDef[ps->weapon].pump ||
            weaponDef[ps->weapon].bolt) {
            *timerPlayer = ps;
            return qtrue;
        }
    }
    ps = pm->ps;
    if ((unsigned)ps->weapon >= TCE_WEAPON_CAPACITY) return qfalse;
    clip = BG_FindClipForWeapon(ps->weapon);
#if defined(_WIN32)
    weapon = ps->weapon;
#else
    weapon = pm->ps->weapon;
#endif
    if (!ps->ammoclip[clip] && weapon != TCE_WP_K1 && weapon != TCE_WP_G3 &&
        weapon != TCE_WP_G2 && weapon != TCE_WP_G1) {
#if defined(_WIN32)
        *timerPlayer = ps;
#else
        *timerPlayer = pm->ps;
#endif
        return qtrue;
    }
    /* Windows supplies the weapon retained across clip lookup. Linux loads
     * it from the current player before this second firemode callback. */
#if defined(_WIN32)
    weapon = ps->weapon;
#else
    weapon = pm->ps->weapon;
#endif
    if (BG_FiremodeWeapon(weapon)) {
        ps = pm->ps;
        if (ps->persistant[10] == tceFiremodeSingle) {
            *timerPlayer = ps;
            return qtrue;
        }
        if ((unsigned)ps->weapon >= TCE_WEAPON_CAPACITY) return qfalse;
        if ((ps->persistant[10] == tceFiremodeBurst &&
             ps->holdable[11] > PM_TCEElapsedWeaponCounter(weaponDef[ps->weapon].burst, 1)) ||
            weaponDef[ps->weapon].singleReload) {
            *timerPlayer = ps;
            return qtrue;
        }
    }
    return qfalse;
}

static void PM_TCEStopTrigger(playerState_t *timerPlayer) {
    playerState_t *clipPlayer;
    int clip;

    /* In the Windows empty-clip exit only this first timer target was saved
     * before lookup; the delay and final READY stores reload the player. */
    timerPlayer->weaponTime = 0;
    pm->ps->weaponDelay = 0;
    if (PM_TCEWeaponIsFiring() && (unsigned)pm->ps->weapon < TCE_WEAPON_CAPACITY) {
        clipPlayer = pm->ps;
        clip = BG_FindClipForWeapon(clipPlayer->weapon);
#if defined(_WIN32)
        clipPlayer = pm->ps;
#endif
        if (clipPlayer->ammoclip[clip]) PM_ContinueWeaponAnim(WEAP_IDLE1);
        else PM_StartWeaponAnim(WEAP_IDLE2);
    }
    pm->ps->weaponstate = WEAPON_READY;
}

static qboolean PM_TCEUpdateWeaponTrigger(qboolean delayedFire) {
    playerState_t *timerPlayer;

    if ((unsigned)pm->ps->weapon < TCE_WEAPON_CAPACITY) {
        if (!delayedFire) PM_TCEContinueBurst();
        if ((pm->cmd.buttons & BUTTON_ATTACK) || delayedFire) {
            if (!(pm->ps->pm_flags & tceAttackButtonLatched) || delayedFire) {
                pm->ps->pm_flags |= tceAttackButtonLatched;
            } else {
                if (pm->ps->weapon != TCE_WP_K1 && !(pm->ps->stats[STAT_TCE_FLAGS] & 0x800) &&
                    PM_TCEHeldTriggerStops(&timerPlayer)) {
                    PM_TCEStopTrigger(timerPlayer);
                    return qtrue;
                }
                if (!(pm->cmd.buttons & BUTTON_ATTACK))
                    pm->ps->pm_flags &= ~tceAttackButtonLatched;
            }
        } else {
            pm->ps->pm_flags &= ~tceAttackButtonLatched;
        }
    }
    if (pm->ps->weaponstate == WEAPON_RELOADING ||
        pm->ps->weaponstate == WEAPON_TCE_RELOAD_END ||
        pm->ps->weaponstate == WEAPON_TCE_CYCLE)
        PM_FinishWeaponReload();
    if (pm->ps->weaponstate == WEAPON_RAISING) {
        int animation;
        pm->ps->weaponstate = WEAPON_READY;
        animation = PM_IdleAnimForWeapon(pm->ps->weapon);
        PM_StartWeaponAnim(animation);
        return qtrue;
    }
    if (pm->ps->weaponstate == WEAPON_RAISING_TORELOAD) {
        pm->ps->weaponstate = WEAPON_READY;
        PM_BeginWeaponReload(pm->ps->weapon);
        return qtrue;
    }
    return qfalse;
}


static void PM_TCEStartAttack(qboolean delayedFire, qboolean akimboFire) {
    int weapon = pm->ps->weapon;
    int animation = (pm->ps->eFlags & EF_PRONE) ?
        ANIM_ET_FIREWEAPONPRONE : ANIM_ET_FIREWEAPON;
    qboolean firing = PM_TCEWeaponIsFiring();

    switch (weapon) {
    case TCE_WP_K1:
        if (!delayedFire)
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                animation, qfalse, qfalse);
        break;
    case 2: case 5: case 6: case 7: case 13: case 14: case 23: case 24:
    case 25: case 32: case 37: case 38: case 39: case 40: case 46: case 47:
    case 51: case 52: case 53: case 54: case 55: case 56: case 57: case 58:
        if (firing || !weaponDef[weapon].fireDelayTime) {
            if (akimboFire)
                animation = (pm->ps->eFlags & EF_PRONE) ?
                    ANIM_ET_FIREWEAPON2PRONE : ANIM_ET_FIREWEAPON2;
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                animation, qfalse, qtrue);
        } else {
            pm->ps->weaponDelay = weaponDef[pm->ps->weapon].fireDelayTime;
        }
        break;
    case 3: case 8: case 10: case 19: case 21: case 22: case 31: case 33:
    case 36: case 41: case 42: case 43: case 44: case 45: case 59: case 62:
        if (firing || !weaponDef[weapon].fireDelayTime)
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                animation, qtrue, qtrue);
        else
            pm->ps->weaponDelay = weaponDef[pm->ps->weapon].fireDelayTime;
        break;
    case TCE_WP_G3: case TCE_WP_G1: case TCE_WP_DC1: case TCE_WP_G2:
        if (!delayedFire) {
            if (PM_WeaponAmmoAvailable(weapon)) {
                if (pm->ps->weapon == TCE_WP_DC1) {
                    pm->ps->grenadeTimeLeft = 50;
                } else {
                    pm->ps->grenadeTimeLeft = PM_TCEAddWeaponCounter(
                        weaponDef[pm->ps->weapon].grenadeTimer, 500);
                    PM_AddEvent(EV_TCE_GRENADE_PRIME);
                }
                PM_StartWeaponAnim(PM_AttackAnimForWeapon(pm->ps->weapon));
            }
            /* Event/animation callbacks precede this definition reload. */
            pm->ps->weaponDelay = weaponDef[pm->ps->weapon].fireDelayTime;
        }
        break;
    case TCE_ITEM_WEAPON_LANDMINE:
        if (!delayedFire) {
            if (PM_WeaponAmmoAvailable(weapon))
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                    (pm->ps->eFlags & EF_PRONE) ? ANIM_ET_FIREWEAPON2PRONE :
                    ANIM_ET_FIREWEAPON, qfalse, qtrue);
            pm->ps->weaponDelay = pm_tceLegacyTiming[pm->ps->weapon].fireDelay;
        }
        break;
    case TCE_ITEM_WEAPON_SATCHEL: case TCE_ITEM_WEAPON_TRIPMINE:
        if (!delayedFire) {
            if (PM_WeaponAmmoAvailable(weapon))
                PM_StartWeaponAnim(PM_AttackAnimForWeapon(pm->ps->weapon));
            pm->ps->weaponDelay = pm_tceLegacyTiming[pm->ps->weapon].fireDelay;
        }
        break;
    case TCE_ITEM_WEAPON_SATCHELDETONATOR:
        if (!firing) {
            PM_AddEvent(EV_SPINUP);
            pm->ps->weaponDelay = pm_tceLegacyTiming[pm->ps->weapon].fireDelay;
            PM_ContinueWeaponAnim(PM_AttackAnimForWeapon(weapon));
        } else {
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                animation, qfalse, qtrue);
        }
        break;
    case TCE_ITEM_WEAPON_MORTAR_SET:
        if (!firing) {
            PM_AddEvent(EV_SPINUP);
            PM_StartWeaponAnim(PM_AttackAnimForWeapon(weapon));
            pm->ps->weaponDelay = pm_tceLegacyTiming[pm->ps->weapon].fireDelay;
        } else {
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                animation, qfalse, qtrue);
        }
        break;
    default:
        if (firing || !weaponDef[weapon].fireDelayTime)
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                animation, qfalse, qtrue);
        else
            pm->ps->weaponDelay = weaponDef[pm->ps->weapon].fireDelayTime;
        break;
    }
    pm->ps->weaponstate = WEAPON_FIRING;
}

static qboolean PM_TCEAllowsAutoReload(int weapon) {
    switch (weapon) {
    case 2: case 3: case 5: case 6: case 7: case 8: case 10: case 13: case 14:
    case 23: case 24: case 25: case 31: case 32: case 33: case 37: case 39:
    case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
    case 48: case 49: case 50: case 51: case 52: case 57: case 58: case 59:
        return qtrue;
    default:
        return qfalse;
    }
}

static void PM_TCEEmptyShot(int ammoNeeded) {
    int weapon = pm->ps->weapon;
    int ammoIndex = BG_FindAmmoForWeapon(weapon);
    qboolean canReload = ammoNeeded <= pm->ps->ammo[ammoIndex];
    qboolean quiet = qfalse;
    qboolean scoped = qfalse;

    if (!pm->pmext->bAutoReload && PM_TCEAllowsAutoReload(pm->ps->weapon) &&
        !(pm->cmd.wbuttons & WBUTTON_RELOAD))
        canReload = qfalse;
    switch (pm->ps->weapon) {
    case TCE_WP_G3: case TCE_WP_G1: case TCE_WP_DC1: case TCE_WP_G2:
    case TCE_ITEM_WEAPON_LANDMINE: case TCE_ITEM_WEAPON_TRIPMINE:
        quiet = qtrue;
        break;
    case 57: case 58: case 59:
        scoped = qtrue;
        break;
    default:
        break;
    }
    /* Scoped legacy weapons report NOAMMO and idle even with reserve ammo. */
    if (!quiet) PM_AddEvent(canReload && !scoped ? EV_EMPTYCLIP : EV_NOAMMO);
    if (canReload && !scoped)
        PM_ContinueWeaponAnim(WEAP_IDLE2);
    else
        PM_ContinueWeaponAnim(PM_IdleAnimForWeapon(pm->ps->weapon));
    pm->ps->weaponTime = PM_TCEAddWeaponCounter(pm->ps->weaponTime, 500);
    if (weaponDef[pm->ps->weapon].singleReload)
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x800;
}

static void PM_TCESlickShotKick(void) {
    float strength;
    float impulse[3];
    long double scale;
    int axis;

    if ((pm->ps->eFlags & EF_PRONE) || !(pml.groundTrace.surfaceFlags & SURF_SLICK))
        return;
    if (pm->ps->weapon != TCE_ITEM_WEAPON_MOBILE_MG42) return;
    strength = 400.0f;
#if defined(_WIN32)
    scale = -(long double)strength * (long double)0.005f;
    /* Windows keeps the first product extended, then reloads a stored scale
     * for the other two components. Linux stores all impulse components. */
    impulse[1] = (float)((long double)(float)scale * pml.forward[1]);
    impulse[2] = (float)((long double)(float)scale * pml.forward[2]);
    pm->ps->velocity[0] = (float)((long double)pm->ps->velocity[0] + scale * pml.forward[0]);
    for (axis = 1; axis < 3; ++axis)
        pm->ps->velocity[axis] = (float)((long double)pm->ps->velocity[axis] + impulse[axis]);
#else
    scale = -((long double)1.0f / 200.0f) * strength;
    for (axis = 0; axis < 3; ++axis)
        impulse[axis] = (float)(scale * pml.forward[axis]);
    for (axis = 0; axis < 3; ++axis)
        pm->ps->velocity[axis] = (float)((long double)pm->ps->velocity[axis] + impulse[axis]);
#endif
    if (!pm->ps->pm_time) {
        pm->ps->pm_time = 100;
        pm->ps->pm_flags |= PMF_TIME_KNOCKBACK;
    }
}

/* False means this command either primed a delayed shot or ran out of ammo. */
static qboolean PM_TCEConsumeShotAmmo(void) {
    int ammoNeeded = weaponDef[pm->ps->weapon].uses;
    if (pm->ps->weapon && ammoNeeded > PM_WeaponAmmoAvailable(pm->ps->weapon)) {
        PM_TCEEmptyShot(ammoNeeded);
        return qfalse;
    }
    if (pm->ps->weaponDelay > 0) return qfalse;
    PM_TCESlickShotKick();
    if (PM_WeaponAmmoAvailable(pm->ps->weapon) != -1 &&
        !pm->ps->persistant[PERS_HWEAPON_USE] && !(pm->ps->eFlags & EF_MOUNTEDTANK))
        PM_WeaponUseAmmo(pm->ps->weapon, ammoNeeded);
    return qtrue;
}

static int PM_TCENotifyShot(qboolean akimboFire) {
    int animation;
    int fireEvent;
    if (BG_IsAkimboWeapon(pm->ps->weapon))
        animation = akimboFire ? WEAP_ATTACK1 : WEAP_ATTACK2;
    else if (PM_WeaponClipEmpty(pm->ps->weapon))
        animation = PM_LastAttackAnimForWeapon(pm->ps->weapon);
    else
        animation = PM_AttackAnimForWeapon(pm->ps->weapon);

    switch (pm->ps->weapon) {
    case 3: case 8: case 10: case 19: case 21: case 22: case 28:
    case 31: case 36: case 41: case 42: case 43: case 44: case 45: case 62:
        PM_ContinueWeaponAnim(animation);
        break;
    case TCE_WP_G3: case TCE_WP_G1: case TCE_WP_G2:
        PM_StartWeaponAnim(WEAP_ATTACK2);
        break;
    case TCE_ITEM_WEAPON_MORTAR_SET:
        break;
    default:
        PM_StartWeaponAnim(animation);
        break;
    }
    if (pm->ps->weapon == TCE_ITEM_WEAPON_GRENADESMOKE ||
        pm->ps->weapon == TCE_ITEM_WEAPON_LANDMINE ||
        pm->ps->weapon == TCE_ITEM_WEAPON_SATCHEL)
        PM_AddEvent(EV_NOAMMO);
    if (pm->ps->weapon == TCE_ITEM_WEAPON_SATCHEL) {
        pm->ps->ammoclip[TCE_ITEM_WEAPON_SATCHELDETONATOR] = 1;
        pm->ps->ammo[TCE_ITEM_WEAPON_SATCHEL] = 0;
        pm->ps->ammoclip[TCE_ITEM_WEAPON_SATCHEL] = 0;
        PM_BeginWeaponChange(TCE_ITEM_WEAPON_SATCHEL, TCE_ITEM_WEAPON_SATCHELDETONATOR, qfalse);
    }
    if ((pm->ps->weapon == 55 || pm->ps->weapon == 56) &&
        !pm->ps->ammo[BG_FindAmmoForWeapon(pm->ps->weapon)])
        PM_AddEvent(EV_NOAMMO);
    if (pm->ps->weapon == TCE_ITEM_WEAPON_MORTAR_SET &&
        !pm->ps->ammo[TCE_ITEM_WEAPON_MORTAR])
        PM_AddEvent(EV_NOAMMO);

    if (BG_IsAkimboWeapon(pm->ps->weapon)) {
        PM_AddEvent(akimboFire ? EV_FIRE_WEAPON : EV_FIRE_WEAPONB);
    } else {
        qboolean pumpMode = weaponDef[pm->ps->weapon].pump &&
            pm->ps->persistant[10] > 0 && BG_FiremodeWeapon(pm->ps->weapon);
        fireEvent = PM_WeaponClipEmpty(pm->ps->weapon) ? EV_FIRE_WEAPON_LASTSHOT : EV_FIRE_WEAPON;
        if (pumpMode) PM_AddEventExt(fireEvent, 1);
        else PM_AddEvent(fireEvent);
    }
    pm->pmext->releasedFire = qfalse;
    pm->ps->lastFireTime = pm->cmd.serverTime;
    return animation;
}

static int PM_TCEShotCadence(qboolean akimboFire, int attackAnimation, int *spread) {
    int weapon = pm->ps->weapon;
    int interval = 0;
    qboolean doubleInterval = qfalse;
    *spread = 0;
    switch (weapon) {
    case 1: case 4: case 9: case 15: case 26: case 29: case 30:
    case 55: case 56: case 60:
        interval = weaponDef[weapon].nextShotTime;
        break;
    case 2: case 5: case 6: case 13: case 14: case 23: case 24: case 25:
    case 32: case 33: case 39: case 40: case 41: case 42: case 43: case 44:
    case 45: case 46: case 47: case 48: case 49: case 50: case 51:
        if (weaponDef[weapon].pump) pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x800;
        *spread = 35;
        interval = weaponDef[pm->ps->weapon].nextShotTime;
        break;
    case 3: case 8: case 10:
        interval = weaponDef[weapon].nextShotTime;
        *spread = 15 + rand() % 10;
        break;
    case 7: case 52:
        *spread = 20;
        interval = weaponDef[weapon].nextShotTime;
        break;
    case 11: case 12: case 36: case 61:
        interval = pm_tceLegacyTiming[weapon].nextShot;
        break;
    case 19: case 22:
        interval = 1000;
        break;
    case 21:
        interval = 50;
        break;
    case 31: case 62:
        interval = attackAnimation == WEAP_ATTACK_LASTSHOT ? 2000 : pm_tceLegacyTiming[weapon].nextShot;
        *spread = 20;
        break;
    case 37: case 53: case 38: case 54:
        interval = weaponDef[weapon].nextShotTime;
        if (!pm->ps->ammoclip[BG_FindClipForWeapon(pm->ps->weapon)]) {
            if (!akimboFire) doubleInterval = qtrue;
        } else {
            int sidearm = BG_AkimboSidearm(pm->ps->weapon);
            if (!pm->ps->ammoclip[BG_FindClipForWeapon(sidearm)] && akimboFire)
                doubleInterval = qtrue;
        }
        if (doubleInterval) {
            interval = weaponDef[pm->ps->weapon].nextShotTime;
            interval = PM_TCEAddWeaponCounter(interval, interval);
        }
        *spread = weapon == 37 || weapon == 53 ? 20 : 35;
        break;
    case 57: case 58:
        *spread = 200;
        interval = weaponDef[weapon].nextShotTime;
        break;
    case 59:
        *spread = 100;
        interval = weaponDef[weapon].nextShotTime;
        break;
    default:
        break;
    }
    return interval;
}

static void PM_TCELegacyShotRecoil(void) {
    pm->pmext->lastRecoilDeltaTime = 0;
    switch (pm->ps->weapon) {
    case 57: case 58:
        pm->pmext->weapRecoilTime = pm->cmd.serverTime;
        pm->pmext->weapRecoilDuration = 300;
        PM_WeaponRandomYaw(&pm->pmext->weapRecoilYaw, 0.5);
        pm->pmext->weapRecoilPitch =
            pm->skill[SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS] >= 3 ? 0.25f : 0.5f;
        break;
    case TCE_ITEM_WEAPON_MOBILE_MG42:
        pm->pmext->weapRecoilTime = pm->cmd.serverTime;
        pm->pmext->weapRecoilDuration = 200;
        if ((pm->ps->pm_flags & PMF_DUCKED) || (pm->ps->eFlags & EF_PRONE)) {
            PM_WeaponRandomYaw(&pm->pmext->weapRecoilYaw, 0.5);
            PM_WeaponRandomPitch(&pm->pmext->weapRecoilPitch, 0.06750000268220901f);
        } else {
            PM_WeaponRandomYaw(&pm->pmext->weapRecoilYaw, 0.25);
            PM_WeaponRandomPitch(&pm->pmext->weapRecoilPitch, 0.15000000596046448f);
        }
        break;
    case 59:
        pm->pmext->weapRecoilTime = pm->cmd.serverTime;
        pm->pmext->weapRecoilDuration = 100;
        pm->pmext->weapRecoilYaw = 0.0f;
        PM_WeaponRandomPitch(&pm->pmext->weapRecoilPitch, 0.06750000268220901f);
        if (pm->skill[SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS] >= 3)
            pm->pmext->weapRecoilPitch *= 0.5f;
        break;
    case 2: case 14: case 38: case 54: case 7: case 39: case 40: case 52: case 37: case 53:
        pm->pmext->weapRecoilTime = pm->cmd.serverTime;
        pm->pmext->weapRecoilDuration = pm->skill[SK_LIGHT_WEAPONS] >= 3 ? 70 : 100;
        pm->pmext->weapRecoilYaw = 0.0f;
        PM_WeaponRandomPitch(&pm->pmext->weapRecoilPitch,
            pm->skill[SK_LIGHT_WEAPONS] >= 3 ? 0.03750000149011612f : 0.06750000268220901f);
        break;
    default:
        pm->pmext->weapRecoilTime = 0;
        pm->pmext->weapRecoilYaw = 0.0f;
        break;
    }
}

static void PM_TCEApplyShotRecoil(int interval, int seed, int spread) {
    int recoil;
    int half, kick, minimum, maximum;
    int previous, oldPitch, oldYaw;
    float posture = 1.0f;
    float randomScale, oldAngle, kickScale;
#if defined(_WIN32)
    float minimumGrowth;
#else
    long double minimumGrowth;
#endif
    long double growth;
    long double random, pitchKick;

    pm->ps->aimSpreadScaleFloat = (float)((long double)pm->ps->aimSpreadScaleFloat + 3.0L * spread);
    if (pm->ps->aimSpreadScaleFloat > 255.0f) pm->ps->aimSpreadScaleFloat = 255.0f;
    pm->ps->aimSpreadScale = TCE_TruncateNativeInteger(pm->ps->aimSpreadScaleFloat);
    pm->ps->holdable[0] = pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
    pm->ps->holdable[1] = pm->ps->stats[STAT_TCE_SHOT_INSTABILITY];

    previous = pm->ps->stats[STAT_TCE_SHOT_INSTABILITY];
    growth = sqrtl((long double)previous * 0.001f);
#if defined(_WIN32)
    minimumGrowth = (float)((long double)weaponDef[pm->ps->weapon].unknown_0f8[6] * 0.001f);
    if (!(growth >= minimumGrowth)) growth = minimumGrowth;
#else
    growth = (float)growth;
    minimumGrowth = (long double)weaponDef[pm->ps->weapon].unknown_0f8[6] * 0.001f;
    if (!(growth >= minimumGrowth)) growth = minimumGrowth;
#endif
    pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] =
        TCE_TruncateNativeInteger((long double)previous + growth * 1000.0f);
    if (pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] > 1000)
        pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] = 1000;
    recoil = weaponDef[pm->ps->weapon].unknown_0f8[
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4 ? 8 : 7];
    if (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) {
        if ((pm->ps->eFlags & EF_CROUCHING) && !(VectorLength(pm->ps->velocity) >= 30.0f)) posture = 0.7f;
        if ((pm->ps->eFlags & EF_PRONE) && !(VectorLength(pm->ps->velocity) >= 30.0f)) posture = 0.6f;
    }
    VectorCopy(pm->ps->viewangles, pm->pmext->tceShotAngles);
    if ((pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) && !weaponDef[pm->ps->weapon].noTacMode) {
        int axis;
        for (axis = PITCH; axis <= YAW; ++axis) {
            long double offset = ((long double)pm->ps->holdable[5 + axis] - 2000.0f) * 0.01f;
#if !defined(_WIN32)
            offset = (float)offset;
#endif
            pm->pmext->tceShotAngles[axis] = (float)((long double)pm->pmext->tceShotAngles[axis] + offset);
        }
    }
    if (!recoil) {
        pm->pmext->weapRecoilTime = 0;
        return;
    }
    oldPitch = pm->ps->delta_angles[PITCH];
    oldYaw = pm->ps->delta_angles[YAW];
    half = recoil / 2;
    kick = pm->ps->stats[STAT_TCE_RECOIL_REMAINDER];
    if (kick > half) kick = half;
    kick = TCE_TruncateNativeInteger((long double)PM_TCEAddWeaponCounter(kick, recoil) * posture);
    minimum = TCE_TruncateNativeInteger((long double)interval * 1.3333332538604736f + 4.0f);
    if (minimum > 250) minimum = 250;
    if (kick < minimum) kick = minimum;
    pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] =
        PM_TCEAddWeaponCounter(pm->ps->stats[STAT_TCE_RECOIL_REMAINDER], kick);
    maximum = PM_TCEAddWeaponCounter(interval, PM_TCEAddWeaponCounter(half, half));
    maximum = PM_TCEAddWeaponCounter(maximum, PM_TCEAddWeaponCounter(half, half));
    if (pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] > maximum)
        pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] = maximum;
    if (pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] > 500)
        pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] = 500;
    seed = PM_TCEAddWeaponCounter(seed, 1);
    randomScale = (float)((long double)pm->ps->holdable[1] * 0.001f);
    pm->pmext->weapRecoilTime = 0;
    pitchKick = kick;
#if defined(_WIN32)
    kickScale = (float)kick;
    pitchKick = kickScale;
#endif
    oldAngle = (float)pm->ps->delta_angles[PITCH];
    random = Q_random(&seed);
    pm->ps->delta_angles[PITCH] = TCE_TruncateNativeInteger(
        (long double)oldAngle - (random * randomScale + 2.0f) * pitchKick);
    oldAngle = (float)pm->ps->delta_angles[YAW];
    /* Linux stores the integer kick as float across its second Q_random call. */
#if !defined(_WIN32)
    kickScale = (float)kick;
#endif
    random = Q_random(&seed);
    pm->ps->delta_angles[YAW] = TCE_TruncateNativeInteger(
        (long double)oldAngle - random * randomScale * kickScale);

    /* Existing local free-look extension: retain the recoil accumulated by the
     * native firing path when the independent camera view is in use. */
    pm->pmext->tceFreelookRecoil[PITCH] += AngleNormalize180(SHORT2ANGLE(
        PM_TCEWeaponAngleChange(pm->ps->delta_angles[PITCH], oldPitch)));
    pm->pmext->tceFreelookRecoil[YAW] += AngleNormalize180(SHORT2ANGLE(
        PM_TCEWeaponAngleChange(pm->ps->delta_angles[YAW], oldYaw)));
}

static void PM_TCEFireWeapon(qboolean delayedFire, qboolean akimboFire) {
    int animation, interval, spread;
    int seed;
    /* The original definition and gear arrays each contain exactly 64 rows.
     * Legacy protocol IDs 65/66 have no valid definition object here. */
    if ((unsigned)pm->ps->weapon >= TCE_WEAPON_CAPACITY) return;
    PM_TCEStartAttack(delayedFire, akimboFire);
    if (!PM_TCEConsumeShotAmmo()) return;
    animation = PM_TCENotifyShot(akimboFire);
    seed = pm->ps->stats[STAT_TCE_SHOT_SEED];
    Q_rand(&seed);
    seed &= 65535;
    pm->ps->stats[STAT_TCE_SHOT_SEED] = seed;
    interval = PM_TCEShotCadence(akimboFire, animation, &spread);
    PM_TCELegacyShotRecoil();
    PM_TCEApplyShotRecoil(interval, seed, spread);
    pm->ps->weaponTime = PM_TCEAddWeaponCounter(pm->ps->weaponTime, interval);
    PM_SwitchIfEmpty();
}


static void PM_Weapon( void ) {
	qboolean	delayedFire;	//----(SA)  true if the delay time has just expired and this is the frame to send the fire event
	qboolean	akimboFire;
#ifdef DO_WEAPON_DBG
	static int weaponstate_last = -1;
#endif

    if (PM_TCEUpdateMountedWeapon()) return;

    akimboFire = PM_TCESelectAkimboShot();

  // TTimo
  // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=416
#ifdef DO_WEAPON_DBG
  if (pm->ps->weaponstate != weaponstate_last) {
    #ifdef CGAMEDLL
    Com_Printf(" CGAMEDLL\n");
    #else
    Com_Printf("!CGAMEDLL\n");
    #endif
		switch(pm->ps->weaponstate) {
			case WEAPON_READY:
				Com_Printf(" -- WEAPON_READY\n");
				break;
			case WEAPON_RAISING:
				Com_Printf(" -- WEAPON_RAISING\n");
				break;
			case WEAPON_RAISING_TORELOAD:
				Com_Printf(" -- WEAPON_RAISING_TORELOAD\n");
				break;
			case WEAPON_DROPPING:
				Com_Printf(" -- WEAPON_DROPPING\n");
				break;
			case WEAPON_READYING:
				Com_Printf(" -- WEAPON_READYING\n");
				break;
			case WEAPON_RELAXING:
				Com_Printf(" -- WEAPON_RELAXING\n");
				break;
			case WEAPON_DROPPING_TORELOAD:
				Com_Printf(" -- WEAPON_DROPPING_TORELOAD\n");
				break;
			case WEAPON_FIRING:
				Com_Printf(" -- WEAPON_FIRING\n");
				break;
			case WEAPON_FIRINGALT:
				Com_Printf(" -- WEAPON_FIRINGALT\n");
				break;
			case WEAPON_RELOADING:
				Com_Printf(" -- WEAPON_RELOADING\n");
				break;
		}
    weaponstate_last = pm->ps->weaponstate;
  }
#endif


    PM_TCEReconcileWeaponFiremode();
    PM_TCEUpdateWeaponSway();
    PM_TCEApplyQueuedWeaponRecoil();

    if (PM_TCEUpdateWeaponCycle(akimboFire, &delayedFire)) return;

    if (PM_TCEUpdateWeaponActions(delayedFire)) return;

    if (PM_TCEUpdateWeaponTrigger(delayedFire)) return;

#if defined(__GNUC__) && defined(__i386__) && defined(__linux__)
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        if (PM_TCEWeaponEligibilityLinux856(delayedFire)) return;
    } else
#elif defined(_MSC_VER) && defined(_M_IX86) && (defined(GAMEDLL) || defined(CGAMEDLL))
    if (PM_TCEWeaponEligibility837(delayedFire)) return;
#endif
#if !defined(_MSC_VER) || !defined(_M_IX86) || (!defined(GAMEDLL) && !defined(CGAMEDLL))
    {
	if(pm->ps->weapon == WP_NONE)	// this is possible since the player starts with nothing
		return;


    /* Original charge eligibility,300110..300113. TC slot IDs are not
     * SDK enum aliases: e.g. Glock39/40 must never enter riflegrenade gates. */
    {
        int w = pm->ps->weapon;
        int elapsed;
#if defined(_MSC_VER) && defined(_M_IX86)
        int chargeNow = pm->cmd.serverTime, chargeStarted = pm->ps->classWeaponTime;
        elapsed = PM_ProneTimeValue((unsigned)chargeNow - (unsigned)chargeStarted);
#else
        elapsed = pm->cmd.serverTime - pm->ps->classWeaponTime;
#endif
        if (w == 65) {
            if (pm->ps->eFlags & EF_PRONE) return;
            if (pm->skill[SK_HEAVY_WEAPONS] >= 1) {
                if (PM_WeaponChargePending(elapsed, pm->soldierChargeTime, 0.66f)) return;
            } else if (elapsed < pm->soldierChargeTime) return;
        }
        if ((w == 55 || w == 56) && PM_WeaponChargePending(elapsed, pm->engineerChargeTime, 0.5f)) return;
        if (w == 60) {
            if (PM_WeaponChargePending(elapsed, pm->soldierChargeTime,
                pm->skill[SK_HEAVY_WEAPONS] >= 1 ? 0.35f : 0.5f)) return;
            if (!delayedFire) pm->ps->weaponstate = WEAPON_READY;
        }
        if (w == 27) {
            if (pm->skill[SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS] >= 2) {
                if (PM_WeaponChargePending(elapsed, pm->covertopsChargeTime, 0.66f)) return;
            } else if (elapsed < pm->covertopsChargeTime) return;
        }
        if (w == 26 && PM_WeaponChargePending(elapsed, pm->engineerChargeTime,
            pm->skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 2 ? 0.33f : 0.5f)) return;
        if (w == 15) {
            if (pm->skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 3) {
                if (PM_WeaponChargePending(elapsed, pm->engineerChargeTime, 0.66f)) return;
            } else if (elapsed < pm->engineerChargeTime) return;
        }
        if ((w == 12 && PM_WeaponChargePending(elapsed, pm->ltChargeTime,
             pm->skill[SK_SIGNALS] >= 1 ? 0.15f : 0.25f)) ||
            (w == 19 && PM_WeaponChargePending(elapsed, pm->medicChargeTime,
             pm->skill[SK_FIRST_AID] >= 2 ? 0.15f : 0.25f))) {
            if (pm->cmd.buttons & BUTTON_ATTACK)
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, ANIM_ET_NOPOWER, qtrue, qfalse);
            return;
        }
        if (w == 22) {
            if (pm->skill[SK_SIGNALS] >= 2) {
                if (PM_WeaponChargePending(elapsed, pm->ltChargeTime, 0.66f)) return;
            } else if (elapsed < pm->ltChargeTime) return;
        }
        if (w == 61 && elapsed < pm->medicChargeTime) return;
    }

    /* Original300113..300114: release/lean/action eligibility. A pending
     * grenade4/9/30 is the sole delayed exception to flag0x4000. Tactical
     * aiming permits leaning; unrelated secondary-button bits do not fire. */
    if ((!(pm->cmd.buttons & BUTTON_ATTACK) && !delayedFire) ||
        (pm->ps->leanf != 0 && pm->ps->weapon != 4 && pm->ps->weapon != 9 &&
         pm->ps->weapon != 30 && !(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4)) ||
        ((pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x4000) &&
         (!delayedFire || (pm->ps->weapon != 4 && pm->ps->weapon != 9 && pm->ps->weapon != 30)))) {
        pm->ps->weaponTime = 0;
        pm->ps->weaponDelay = 0;
        if (weaponstateFiring) PM_ContinueWeaponAnim(TCE_PM_IdleAnimForWeapon(pm->ps->weapon));
        if (pm->ps->holdable[11] > 0) {
            if (BG_FiremodeWeapon(pm->ps->weapon) && pm->ps->persistant[10] == 1)
                pm->ps->weaponTime = 50;
            pm->ps->holdable[11] = 0;
            pm->ps->stats[STAT_TCE_FLAGS] &= ~8;
        }
        pm->ps->weaponstate = WEAPON_READY;
        return;
    }

    /* TC server adds the zoomed class3 event; the original client returns.
     * This portable fallback preserves that module distinction as well. */
    if (pm->ps->weapon == 35) return;
    if (pm->ps->eFlags & EF_ZOOMING) {
#ifdef GAMEDLL
        if (pm->ps->stats[STAT_PLAYER_CLASS] == PC_FIELDOPS) {
            pm->ps->weaponTime = (int)((unsigned int)pm->ps->weaponTime + 500u);
            PM_AddEvent(EV_FIRE_WEAPON);
        }
#endif
        return;
    }

	// player is underwater - no fire
	if(pm->waterlevel == 3) {
		if(	pm->ps->weapon != WP_KNIFE &&
			pm->ps->weapon != WP_GRENADE_LAUNCHER &&
			pm->ps->weapon != WP_GRENADE_PINEAPPLE &&
			pm->ps->weapon != WP_DYNAMITE &&
			pm->ps->weapon != WP_LANDMINE &&
			pm->ps->weapon != WP_TRIPMINE &&
			pm->ps->weapon != WP_SMOKE_BOMB ) {
				PM_AddEvent(EV_NOFIRE_UNDERWATER);	// event for underwater 'click' for nofire
				pm->ps->weaponTime	= 500;
				pm->ps->weaponDelay	= 0;			// avoid insta-fire after water exit on delayed weapon attacks
				return;
		}
	}

    }
#endif

    PM_TCEFireWeapon(delayedFire, akimboFire);
}


/*
================
PM_Animate
================
*/
#define MYTIMER_SALUTE	 1133	// 17 frames, 15 fps
#define MYTIMER_DISMOUNT 667	// 10 frames, 15 fps

/*
================
PM_DropTimers
================
*/
static void PM_DropTimers( void ) {
	// drop misc timing counter
	if ( pm->ps->pm_time ) {
		if ( pml.msec >= pm->ps->pm_time ) {
			pm->ps->pm_flags &= ~PMF_ALL_TIMES;
			pm->ps->pm_time = 0;
		} else {
			pm->ps->pm_time -= pml.msec;
		}
	}

	// drop animation counter
	if ( pm->ps->legsTimer > 0 ) {
		pm->ps->legsTimer -= pml.msec;
		if ( pm->ps->legsTimer < 0 ) {
			pm->ps->legsTimer = 0;
		}
	}

	if ( pm->ps->torsoTimer > 0 ) {
		pm->ps->torsoTimer -= pml.msec;
		if ( pm->ps->torsoTimer < 0 ) {
			pm->ps->torsoTimer = 0;
		}
	}

	// first person weapon counter
	if ( pm->pmext->weapAnimTimer > 0 ) {
		pm->pmext->weapAnimTimer -= pml.msec;
		if ( pm->pmext->weapAnimTimer < 0 ) {
			pm->pmext->weapAnimTimer = 0;
		}
	}
}



#define LEAN_MAX	28.0f
#define LEAN_TIME_TO	200.0f	// time to get to/from full lean
#define LEAN_TIME_FR	300.0f	// time to get to/from full lean

/*
==============
PM_CalcLean

==============
*/
/* Windows30009fe0. TC lean traces the full target before approaching it.
 * This is shared by server movement and client prediction. */
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC qagame20032340/20032710. Each foreign data offset is mapped to its native base. */
enum {
    posturePs4 = offsetof(playerState_t, pm_type),
    posturePsc = offsetof(playerState_t, pm_flags),
    posturePs14 = offsetof(playerState_t, origin),
    posturePs18 = offsetof(playerState_t, origin) + sizeof(float),
    posturePs1c = offsetof(playerState_t, origin) + 2 * sizeof(float),
    posturePs3c = offsetof(playerState_t, leanf),
    posturePs44 = offsetof(playerState_t, delta_angles),
    posturePs48 = offsetof(playerState_t, delta_angles) + sizeof(int),
    posturePs68 = offsetof(playerState_t, eFlags),
    posturePsa0 = offsetof(playerState_t, clientNum),
    posturePsa4 = offsetof(playerState_t, weapon),
    posturePsa8 = offsetof(playerState_t, weaponstate),
    posturePsb0 = offsetof(playerState_t, viewangles),
    posturePsb4 = offsetof(playerState_t, viewangles) + sizeof(float),
    posturePsb8 = offsetof(playerState_t, viewangles) + 2 * sizeof(float),
    posturePsbc = offsetof(playerState_t, viewheight),
    posturePsd0 = offsetof(playerState_t, stats) + STAT_HEALTH * sizeof(int),
    posturePsd8 = offsetof(playerState_t, stats) + STAT_DEAD_YAW * sizeof(int),
    posturePsf0 = offsetof(playerState_t, stats) + STAT_TCE_WEAPON_FLAGS * sizeof(int),
    posturePsf4 = offsetof(playerState_t, stats) + STAT_TCE_FLAGS * sizeof(int),
    posturePs148 = offsetof(playerState_t, persistant) + 14 * sizeof(int),
    posturePs3a4 = offsetof(playerState_t, holdable) + 5 * sizeof(int),
    posturePs3a8 = offsetof(playerState_t, holdable) + 6 * sizeof(int),
    postureExt18 = offsetof(pmoveExt_t, varc),
    postureExt1c = offsetof(pmoveExt_t, harc),
    postureExt20 = offsetof(pmoveExt_t, centerangles),
    postureExt24 = offsetof(pmoveExt_t, centerangles) + sizeof(float),
    postureExt3c = offsetof(pmoveExt_t, proneLegsOffset),
    postureExt40 = offsetof(pmoveExt_t, mountedWeaponAngles),
    postureExt44 = offsetof(pmoveExt_t, mountedWeaponAngles) + sizeof(float),
    postureCmdButtons = offsetof(usercmd_t, wbuttons),
    postureCmdUp = offsetof(usercmd_t, upmove),
    postureCmdAngles = offsetof(usercmd_t, angles),
    posturePmTrace = offsetof(pmove_t, trace),
    posturePmMask = offsetof(pmove_t, tracemask),
    posturePmlFrame = offsetof(pml_t, frametime),
    posturePmlMsec = offsetof(pml_t, msec),
    posturePmlLadder = offsetof(pml_t, ladder),
    postureDefStride = sizeof(tce_weaponDef_t),
    postureNoTac = offsetof(tce_weaponDef_t, noTacMode),
    postureTpmSpace = (sizeof(pmove_t) + 3) & ~3,
    postureTraceFraction = offsetof(trace_t, fraction),
    postureTraceStartsolid = offsetof(trace_t, startsolid),
    postureTraceAllsolid = offsetof(trace_t, allsolid),
    postureTraceEntity = offsetof(trace_t, entityNum),
    postureTraceEnd = offsetof(trace_t, endpos),
    postureTraceEndY = offsetof(trace_t, endpos) + sizeof(float),
    postureTraceEndZ = offsetof(trace_t, endpos) + 2 * sizeof(float),
    postureViewStack = 0x1bc + postureTpmSpace
};
typedef char postureTraceBuffer832[sizeof(trace_t) <= 0x38 ? 1 : -1];
typedef char postureElementSize832[sizeof(int) == 4 && sizeof(float) == 4 ? 1 : -1];
static const unsigned int postureConstant200ac100 = 0x00000000u;
static const unsigned int postureConstant200ac11c = 0x42700000u;
static const unsigned int postureConstant200ac138 = 0x43340000u;
static const unsigned int postureConstant200ac160 = 0x44fa0000u;
static const unsigned int postureConstant200ac170[2] = { 0x00000000u, 0x3fd00000u };
static const unsigned int postureConstant200ac180 = 0x3f000000u;
static const unsigned int postureConstant200ac190 = 0x41f00000u;
static const unsigned int postureConstant200ac19c = 0x41a00000u;
static const unsigned int postureConstant200ac1a0 = 0x42c80000u;
static const unsigned int postureConstant200ac290 = 0x41c00000u;
static const unsigned int postureConstant200ac2bc = 0x40800000u;
static const unsigned int postureConstant200ac2d4 = 0x42000000u;
static const unsigned int postureConstant200ac2e0 = 0x41000000u;
static const unsigned int postureConstant200ac2f8 = 0x3c23d70au;
static const unsigned int postureConstant200ac30c = 0x40400000u;
static const unsigned int postureConstant200ac380 = 0x43b40000u;
static const unsigned int postureConstant200ac388 = 0x40490fdbu;
static const unsigned int postureConstant200ac464 = 0x43960000u;
static const unsigned int postureConstant200ac46c = 0xc1a00000u;
static const unsigned int postureConstant200ac474 = 0xc1f00000u;
static const unsigned int postureConstant200ac478 = 0xc3340000u;
static const unsigned int postureConstant200ac480 = 0x43360b61u;
static const unsigned int postureConstant200ac488[2] = { 0x00000000u, 0x3f768000u };
static const unsigned int postureConstant200ac698 = 0x3ba3d70au;
static const unsigned int postureConstant200ac6e4 = 0x3bb60b61u;
static const unsigned int postureConstant200ac6e8 = 0x3fa00000u;
static const unsigned int postureConstant200ac700 = 0x42f00000u;
static const unsigned int postureConstant200ac710 = 0x41600000u;
static const unsigned int postureConstant200ac714 = 0x41e00000u;
static const unsigned int postureConstant200ac718 = 0x3b5a740eu;
static const unsigned int postureConstant200ac71c = 0x41accccdu;
static const unsigned int postureConstant200b4080 = 0xc1580000u;
static const unsigned int postureConstant200b4084 = 0xc1580000u;
static const unsigned int postureConstant200b4088 = 0xc1c00000u;
static const unsigned int postureConstant200b408c = 0x41580000u;
static const unsigned int postureConstant200b4090 = 0x41580000u;
static const unsigned int postureConstant200b4094 = 0xc1666666u;
#endif

void PM_UpdateLean(playerState_t *ps, usercmd_t *cmd, pmove_t *tpm) {
    int direction=0,i;
    float lean=ps->leanf,target;
    vec3_t start,end,angles,forward,right,up,mins={-8,-8,-8},maxs={8,8,8};
    trace_t trace;
    if((cmd->wbuttons & (WBUTTON_LEANLEFT|WBUTTON_LEANRIGHT)) && cmd->upmove<=0) {
        if(cmd->wbuttons & WBUTTON_LEANLEFT)--direction;
        if(cmd->wbuttons & WBUTTON_LEANRIGHT)++direction;
    }
    if((ps->eFlags & 0x408020) || (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x4000) ||
       pml.ladder || ps->stats[STAT_HEALTH]<1 ||
       (ps->weaponstate==7 && ps->weapon==15) || (ps->eFlags & EF_PRONE) || ps->weapon==60)
        direction=0;
    if(!direction) {
        double step=(double)pml.msec*(double)(1.0f/300.0f)*28.0;
        if(lean>0) { lean=(float)(lean-step);if(lean<0)lean=0; }
        else if(lean<0) { lean=(float)(lean+step);if(lean>0)lean=0; }
    } else {
        VectorCopy(ps->origin,start);start[2]+=ps->viewheight;
        VectorCopy(ps->viewangles,angles);angles[ROLL]+=direction*14.0f;
        AngleVectors(angles,forward,right,up);
        for(i=0;i<3;++i)end[i]=(float)(start[i]+(double)right[i]*(direction*28.0));
        end[2]-=8;
        for(i=0;i<3;++i)end[i]=(float)(end[i]+(double)forward[i]*4.0+(double)up[i]*8.0);
        (pm ? pm : tpm)->trace(&trace,start,mins,maxs,end,ps->clientNum,0x2010001);
        target=trace.fraction*28.0f;
        if(direction<0) {
            target=-target;
#if defined(__linux__)
            /* Native JNC skips the step only for an ordered target >= lean. */
            if (!(target >= lean))
#else
            if (target < lean)
#endif
                lean = (float)(lean - (double)pml.msec * (double).005f * 28.0);
            if(lean<target)lean=target;
        } else {
            if(lean<target)lean=(float)(lean+(double)pml.msec*(double).005f*28.0);
#if defined(__linux__)
            /* The original retains target when the comparison is unordered. */
            if (!(target >= lean)) lean = target;
#else
            if (lean > target) lean = target;
#endif
        }
    }
    ps->leanf=lean;
}




/*
================
PM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated isntead of a full move

	!! NOTE !! Any changes to mounted/prone view should be duplicated in BotEntityWithinView()
================
*/
// rain - take a tracemask as well - we can't use anything out of pm
/* Windows3000a3b0 prone branch. The shared TC player bounds differ from
 * SDK PM_TraceLegs; rejection restores yaw and recomputes its command delta. */
static void PM_TCEProneView(playerState_t *ps, pmoveExt_t *ext, usercmd_t *cmd, float oldYaw) {
    float yaw=ps->viewangles[YAW],diff,limit=40,scale;
    int delta=ps->delta_angles[YAW];
    vec3_t forward,start,end,mins={-13.5f,-13.5f,-24},maxs={13.5f,13.5f,-14.4f};
    trace_t tr;
    if(yaw-oldYaw>180)yaw-=360;
    if(yaw-oldYaw< -180)yaw+=360;
    if(yaw>oldYaw && yaw-oldYaw>120*pml.frametime) {
        yaw=oldYaw+120*pml.frametime;ps->viewangles[YAW]=yaw;
        delta=ANGLE2SHORT(yaw)-cmd->angles[YAW];
    } else if(yaw<oldYaw && oldYaw-yaw>120*pml.frametime) {
        yaw=oldYaw-120*pml.frametime;ps->viewangles[YAW]=yaw;
        delta=ANGLE2SHORT(yaw)-cmd->angles[YAW];
    }
    if(ps->weapon==62) {
        limit=20;diff=ps->viewangles[YAW]-ext->mountedWeaponAngles[YAW];
        if(diff>180)diff-=360;else if(diff< -180)diff+=360;
        if(diff>20 || diff< -20) {
            ps->viewangles[YAW]=AngleNormalize180(ext->mountedWeaponAngles[YAW]+(diff>20?20:-20));
            ps->delta_angles[YAW]=ANGLE2SHORT(ps->viewangles[YAW])-cmd->angles[YAW];
        }
    }
    diff=ps->viewangles[PITCH]-ext->mountedWeaponAngles[PITCH];
    if(diff>180)diff-=360;else if(diff< -180)diff+=360;
    if(diff>limit || diff< -limit) {
        ps->viewangles[PITCH]=AngleNormalize180(ext->mountedWeaponAngles[PITCH]+(diff>limit?limit:-limit));
        ps->delta_angles[PITCH]=ANGLE2SHORT(ps->viewangles[PITCH])-cmd->angles[PITCH];
    }
    if(ps->viewangles[YAW]==oldYaw)return;
    scale=(ps->stats[STAT_TCE_FLAGS]&0x200)?1.25f:1;
    VectorScale(mins,scale,mins);VectorScale(maxs,scale,maxs);
    AngleVectors(ps->viewangles,forward,NULL,NULL);forward[2]=0;VectorNormalizeFast(forward);
    start[0]=ps->origin[0]-forward[0]*scale*32;
    start[1]=ps->origin[1]-forward[1]*scale*32;start[2]=ps->origin[2]+24;
    VectorCopy(start,end);end[2]=(start[2]-21.6f)-24;
    pm->trace(&tr,start,mins,maxs,end,ps->clientNum,pm->tracemask);
    if(tr.startsolid && tr.entityNum>=64) { ps->viewangles[YAW]=oldYaw;ps->delta_angles[YAW]=ANGLE2SHORT(oldYaw)-cmd->angles[YAW];return; }
    VectorCopy(tr.endpos,start);VectorCopy(start,end);end[2]+=21.6f;
    pm->trace(&tr,start,mins,maxs,end,ps->clientNum,pm->tracemask);
    if(tr.allsolid && tr.entityNum>=64) { ps->viewangles[YAW]=oldYaw;ps->delta_angles[YAW]=ANGLE2SHORT(oldYaw)-cmd->angles[YAW];return; }
    ps->delta_angles[YAW]=delta;ext->proneLegsOffset=start[2]-ps->origin[2];
}

/* Windows3000a483..3000a63b tactical view ellipse. Offsets are encoded
 * in hundredths of a degree around2000 in holdable5/6 (ps+3a4/3a8). */
#if defined(_MSC_VER) && defined(_M_IX86)
/* Windows tactical-view dependency. The native controller still has its own
 * inline implementation until the remaining view constraints are reconstructed. */
static void PM_TCETacticalView(playerState_t *ps, const vec3_t oldAngles) {
    float pitchDelta, yawOffset, halfPitch, halfYaw;
    float pitchTarget, yawTarget, totalPitch, clampedYaw;
    long double yawDelta, pitchOffset, totalYaw, radius, clampedPitch, yawClamp;

    if (!(ps->persistant[14] & 0x10)) return;
    if (weaponDef[pm->ps->weapon].noTacMode) return;
    if (!(ps->stats[STAT_TCE_WEAPON_FLAGS] & 4)) return;

    pitchDelta = AngleNormalize180((float)((long double)ps->viewangles[PITCH] - oldAngles[PITCH]));
    yawDelta = AngleNormalize180((float)((long double)ps->viewangles[YAW] - oldAngles[YAW]));
    pitchOffset = ((long double)ps->holdable[5] - 2000.0f) * .01f;
    yawOffset = (float)(((long double)ps->holdable[6] - 2000.0f) * .01f);
    halfPitch = (float)((long double)pitchDelta * .5f);
    pitchTarget = (float)((long double)pitchDelta * .5f + pitchOffset);
    halfYaw = (float)(yawDelta * .5f);
    yawTarget = (float)(yawDelta * .5f + yawOffset);
    totalPitch = (float)(pitchOffset + pitchDelta);
    totalYaw = yawDelta + yawOffset;
    radius = sqrtl((long double)pitchTarget * pitchTarget +
        (long double)yawTarget * yawTarget * .25);
    if (radius > 3.0f) {
        clampedPitch = (long double)pitchTarget / radius * 3.0f;
        ps->holdable[5] = TCE_TruncateNativeInteger(clampedPitch * 100.0f + 2000.0f);
        yawClamp = (long double)yawTarget / radius * 3.0f;
        clampedYaw = (float)yawClamp;
        ps->holdable[6] = TCE_TruncateNativeInteger(yawClamp * 100.0f + 2000.0f);
        ps->viewangles[PITCH] = (float)(((long double)totalPitch - clampedPitch) + oldAngles[PITCH]);
        ps->viewangles[YAW] = (float)((totalYaw - clampedYaw) + oldAngles[YAW]);
    } else {
        ps->viewangles[PITCH] = (float)((long double)halfPitch + oldAngles[PITCH]);
        ps->viewangles[YAW] = (float)((long double)halfYaw + oldAngles[YAW]);
        ps->holdable[5] = TCE_TruncateNativeInteger((long double)pitchTarget * 100.0f + 2000.0f);
        ps->holdable[6] = TCE_TruncateNativeInteger((long double)yawTarget * 100.0f + 2000.0f);
    }
}
#else
static void PM_TCETacticalView(playerState_t *ps,const vec3_t oldAngles) {
    float dp,dy,oy,hp,hy,totalPitch,halfPitch,halfYaw,clampedYaw;
    double op,radius,clampedPitch;
    int w;
    if(!(ps->persistant[14]&0x10) || !(ps->stats[STAT_TCE_WEAPON_FLAGS]&4) || !pm)return;
    w=pm->ps->weapon;
    if(w<0 || w>=64 || weaponDef[w].noTacMode)return;
    dp=AngleNormalize180(ps->viewangles[PITCH]-oldAngles[PITCH]);
    dy=AngleNormalize180(ps->viewangles[YAW]-oldAngles[YAW]);
    op=(ps->holdable[5]-2000.0)*(double).01f;
    oy=(float)((ps->holdable[6]-2000.0)*(double).01f);
    halfPitch=dp*.5f;halfYaw=dy*.5f;
    hp=(float)(dp*.5+op);hy=(float)(dy*.5+(double)oy);
    totalPitch=(float)(op+dp);
    radius=sqrt((double)hp*hp+(double)hy*hy*.25);
    if(radius>3) {
        clampedPitch=hp/radius*3;
        clampedYaw=(float)(hy/radius*3);
        ps->holdable[5]=(int)(clampedPitch*100+2000);
        ps->holdable[6]=(int)((hy/radius*3)*100+2000);
        ps->viewangles[PITCH]=(float)((totalPitch-clampedPitch)+oldAngles[PITCH]);
        ps->viewangles[YAW]=(float)(((double)dy+oy-clampedYaw)+oldAngles[YAW]);
    } else {
        ps->viewangles[PITCH]=halfPitch+oldAngles[PITCH];
        ps->viewangles[YAW]=halfYaw+oldAngles[YAW];
        ps->holdable[5]=(int)((double)hp*100+2000);
        ps->holdable[6]=(int)((double)hy*100+2000);
    }
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void PM_TCEUpdateViewAngles(playerState_t *ps, pmoveExt_t *pmext, usercmd_t *cmd, void (trace)(trace_t *, const vec3_t, const vec3_t, const vec3_t, const vec3_t, int, int), int tracemask) {
    __asm {

        SUB ESP,postureViewStack

        PUSH ESI

        MOV ESI,dword ptr [ESP + 01c4h + postureTpmSpace]

        MOV EAX,dword ptr [ESI + posturePs4]

        CMP EAX,05h

        JZ postureAt20033471

        MOV ECX,dword ptr [ESI + posturePsc]

        TEST CH,080h

        JNZ postureAt20033471

        CMP EAX,02h

        JZ postureAt20032765

        MOV EAX,dword ptr [ESI + posturePsd0]

        TEST EAX,EAX

        JG postureAt20032765

        MOV ECX,dword ptr [ESP + 01cch + postureTpmSpace]

        MOV AX,word ptr [ESI + posturePs48]

        ADD AX,word ptr [ECX + postureCmdAngles + 4]

        MOVSX EDX,AX

        MOV dword ptr [ESI + posturePsd8],EDX

        POP ESI

        ADD ESP,postureViewStack

        RET
    postureAt20032765:
        MOV ECX,dword ptr [ESI + posturePsb4]

        MOV EAX,dword ptr [ESI + posturePsb0]

        PUSH EBX

        PUSH EBP

        PUSH EDI

        MOV EDI,dword ptr [ESP + 01d8h + postureTpmSpace]

        MOV dword ptr [ESP + 01ch],ECX

        MOV dword ptr [ESP + 018h],EAX

        XOR ECX,ECX
    postureAt20032785:
        MOV AX,word ptr [ESI + ECX*4 + posturePs44]

        ADD AX,word ptr [EDI + ECX*4 + postureCmdAngles]

        TEST ECX,ECX

        JNZ postureAt200327c5

        CMP AX,03e80h

        JLE postureAt200327ad

        MOV EAX,dword ptr [EDI + postureCmdAngles]

        MOV EDX,03e80h

        SUB EDX,EAX

        MOV EAX,03e80h

        MOV dword ptr [ESI + posturePs44],EDX

        JMP postureAt200327c5
    postureAt200327ad:
        CMP AX,0c180h

        JGE postureAt200327c5

        MOV EDX,dword ptr [EDI + postureCmdAngles]

        MOV EAX,0ffffc180h

        SUB EAX,EDX

        MOV dword ptr [ESI + posturePs44],EAX

        MOV EAX,0ffffc180h
    postureAt200327c5:
        MOVSX EDX,AX

        MOV dword ptr [ESP + 014h],EDX

        INC ECX

        FILD dword ptr [ESP + 014h]

        CMP ECX,03h

        FMUL qword ptr [postureConstant200ac488]

        FSTP dword ptr [ESI + ECX*4 + posturePsb0 - 4]

        JL postureAt20032785

        TEST byte ptr [ESI + posturePs148],010h

        JZ postureAt2003299b

        MOV EAX,[pm]

        MOV ECX,dword ptr [EAX]

        MOV ECX,dword ptr [ECX + posturePsa4]

        IMUL EDX,ECX,postureDefStride

        NOP

        NOP

        NOP

        MOV EAX,dword ptr [weaponDef + EDX + postureNoTac]

        TEST EAX,EAX

        JNZ postureAt2003299b

        TEST byte ptr [ESI + posturePsf0],04h

        JZ postureAt2003299b

        FLD dword ptr [ESI + posturePsb0]

        FSUB dword ptr [ESP + 018h]

        PUSH ECX

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FSTP dword ptr [ESP + 058h]

        FLD dword ptr [ESI + posturePsb4]

        FSUB dword ptr [ESP + 020h]

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FILD dword ptr [ESI + posturePs3a4]

        ADD ESP,04h

        FSUB dword ptr [postureConstant200ac160]

        FMUL dword ptr [postureConstant200ac2f8]

        FILD dword ptr [ESI + posturePs3a8]

        FSUB dword ptr [postureConstant200ac160]

        FMUL dword ptr [postureConstant200ac2f8]

        FSTP dword ptr [ESP + 040h]

        FLD dword ptr [ESP + 054h]

        FMUL dword ptr [postureConstant200ac180]

        FST dword ptr [ESP + 014h]

        FADD ST(0),ST(1)

        FSTP dword ptr [ESP + 024h]

        FLD ST(1)

        FMUL dword ptr [postureConstant200ac180]

        FST dword ptr [ESP + 010h]

        FADD dword ptr [ESP + 040h]

        FSTP dword ptr [ESP + 028h]

        FADD dword ptr [ESP + 054h]

        FSTP dword ptr [ESP + 054h]

        FADD dword ptr [ESP + 040h]

        FLD dword ptr [ESP + 028h]

        FLD dword ptr [ESP + 024h]

        FMUL dword ptr [ESP + 024h]

        FLD ST(1)

        FMUL ST(0),ST(2)

        FMUL qword ptr [postureConstant200ac170]

        FADDP ST(1),ST(0)

        FSQRT

        FSTP ST(1)

        FCOM dword ptr [postureConstant200ac30c]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032945

        FLD dword ptr [ESP + 024h]

        FDIV ST(0),ST(1)

        FMUL dword ptr [postureConstant200ac30c]

        FLD ST(0)

        FMUL dword ptr [postureConstant200ac1a0]

        FADD dword ptr [postureConstant200ac160]

        CALL PM_WeaponTruncateST0

        FLD dword ptr [ESP + 028h]

        FDIV ST(0),ST(2)

        MOV dword ptr [ESI + posturePs3a4],EAX

        FMUL dword ptr [postureConstant200ac30c]

        FST dword ptr [ESP + 014h]

        FMUL dword ptr [postureConstant200ac1a0]

        FADD dword ptr [postureConstant200ac160]

        CALL PM_WeaponTruncateST0

        FLD dword ptr [ESP + 054h]

        FSUB ST(0),ST(1)

        MOV dword ptr [ESI + posturePs3a8],EAX

        FADD dword ptr [ESP + 018h]

        FSTP dword ptr [ESI + posturePsb0]

        FSTP ST(0)

        FSTP ST(0)

        FSUB dword ptr [ESP + 014h]

        FADD dword ptr [ESP + 01ch]

        FSTP dword ptr [ESI + posturePsb4]

        JMP postureAt2003299b
    postureAt20032945:
        FSTP ST(0)

        FSTP ST(0)

        FLD dword ptr [ESP + 014h]

        FADD dword ptr [ESP + 018h]

        FSTP dword ptr [ESI + posturePsb0]

        FLD dword ptr [ESP + 010h]

        FADD dword ptr [ESP + 01ch]

        FSTP dword ptr [ESI + posturePsb4]

        FLD dword ptr [ESP + 024h]

        FMUL dword ptr [postureConstant200ac1a0]

        FADD dword ptr [postureConstant200ac160]

        CALL PM_WeaponTruncateST0

        FLD dword ptr [ESP + 028h]

        FMUL dword ptr [postureConstant200ac1a0]

        MOV dword ptr [ESI + posturePs3a4],EAX

        FADD dword ptr [postureConstant200ac160]

        CALL PM_WeaponTruncateST0

        MOV dword ptr [ESI + posturePs3a8],EAX
    postureAt2003299b:
        MOV EBX,dword ptr [ESI + posturePs68]

        TEST EBX,01000000h

        JZ postureAt20032b04

        FLD dword ptr [ESI + posturePsb4]

        FLD ST(0)

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [postureConstant200ac138]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt200329c9

        FSUB dword ptr [postureConstant200ac380]
    postureAt200329c9:
        FLD ST(0)

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [postureConstant200ac478]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt200329e2

        FADD dword ptr [postureConstant200ac380]
    postureAt200329e2:
        FCOM dword ptr [ESP + 01ch]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032a16

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac700]

        FSTP dword ptr [ESP + 010h]

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [ESP + 010h]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032a70

        FLD dword ptr [ESP + 010h]

        FADD dword ptr [ESP + 01ch]

        JMP postureAt20032a4e
    postureAt20032a16:
        FLD dword ptr [ESP + 01ch]

        FCOMP

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032a6e

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac700]

        FSTP dword ptr [ESP + 010h]

        FLD dword ptr [ESP + 01ch]

        FSUB ST(0),ST(1)

        FCOMP dword ptr [ESP + 010h]

        FNSTSW AX

        TEST AH,041h

        FSTP ST(0)

        JNZ postureAt20032a70

        FLD dword ptr [ESP + 01ch]

        FSUB dword ptr [ESP + 010h]
    postureAt20032a4e:
        FST dword ptr [ESI + posturePsb4]

        FMUL dword ptr [postureConstant200ac480]

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles + 4]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs48],EAX

        JMP postureAt20032a70
    postureAt20032a6e:
        FSTP ST(0)
    postureAt20032a70:
        MOV EBP,dword ptr [ESP + 01d4h + postureTpmSpace]

        PUSH ECX

        FLD dword ptr [EBP + postureExt24]

        FSUB dword ptr [ESI + posturePsb4]

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FMUL dword ptr [postureConstant200ac388]

        FMUL dword ptr [postureConstant200ac6e4]

        FCOS

        FSTP dword ptr [ESP + 018h]

        FLD dword ptr [EBP + postureExt20]

        FCHS

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FMUL dword ptr [ESP + 018h]

        FSTP dword ptr [ESP]

        CALL AngleNormalize360

        FCHS

        FST dword ptr [EBP + postureExt20]

        FLD dword ptr [ESI + posturePsb0]

        FSUB ST(0),ST(1)

        FSTP dword ptr [ESP]

        FSTP ST(0)

        CALL AngleNormalize180

        FCOM dword ptr [postureConstant200ac19c]

        ADD ESP,04h

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032ae5

        FSTP ST(0)

        FLD dword ptr [EBP + postureExt20]

        JMP postureAt20032d89
    postureAt20032ae5:
        FCOMP dword ptr [postureConstant200ac46c]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt2003344e

        FLD dword ptr [EBP + postureExt20]

        FSUB dword ptr [postureConstant200ac19c]

        JMP postureAt20032d8f
    postureAt20032b04:
        TEST BL,020h

        JNZ postureAt20033216

        TEST EBX,0408000h

        JNZ postureAt20033216

        MOV EBP,dword ptr [ESI + posturePsa4]

        CMP EBP,03ch

        JNZ postureAt20032dda

        FLD dword ptr [ESI + posturePsb4]

        FLD ST(0)

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [postureConstant200ac138]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032b47

        FSUB dword ptr [postureConstant200ac380]
    postureAt20032b47:
        FLD ST(0)

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [postureConstant200ac478]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032b60

        FADD dword ptr [postureConstant200ac380]
    postureAt20032b60:
        FCOM dword ptr [ESP + 01ch]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032b94

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac11c]

        FSTP dword ptr [ESP + 010h]

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [ESP + 010h]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032bee

        FLD dword ptr [ESP + 010h]

        FADD dword ptr [ESP + 01ch]

        JMP postureAt20032bcc
    postureAt20032b94:
        FLD dword ptr [ESP + 01ch]

        FCOMP

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032bec

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac11c]

        FSTP dword ptr [ESP + 010h]

        FLD dword ptr [ESP + 01ch]

        FSUB ST(0),ST(1)

        FCOMP dword ptr [ESP + 010h]

        FNSTSW AX

        TEST AH,041h

        FSTP ST(0)

        JNZ postureAt20032bee

        FLD dword ptr [ESP + 01ch]

        FSUB dword ptr [ESP + 010h]
    postureAt20032bcc:
        FST dword ptr [ESI + posturePsb4]

        FMUL dword ptr [postureConstant200ac480]

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles + 4]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs48],EAX

        JMP postureAt20032bee
    postureAt20032bec:
        FSTP ST(0)
    postureAt20032bee:
        FLD dword ptr [ESI + posturePsb0]

        FLD ST(0)

        FSUB dword ptr [ESP + 018h]

        FCOMP dword ptr [postureConstant200ac138]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032c0d

        FSUB dword ptr [postureConstant200ac380]
    postureAt20032c0d:
        FLD ST(0)

        FSUB dword ptr [ESP + 018h]

        FCOMP dword ptr [postureConstant200ac478]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032c26

        FADD dword ptr [postureConstant200ac380]
    postureAt20032c26:
        FCOM dword ptr [ESP + 018h]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032c5a

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac11c]

        FSTP dword ptr [ESP + 010h]

        FSUB dword ptr [ESP + 018h]

        FCOMP dword ptr [ESP + 010h]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032cb4

        FLD dword ptr [ESP + 010h]

        FADD dword ptr [ESP + 018h]

        JMP postureAt20032c92
    postureAt20032c5a:
        FLD dword ptr [ESP + 018h]

        FCOMP

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032cb2

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac11c]

        FSTP dword ptr [ESP + 010h]

        FLD dword ptr [ESP + 018h]

        FSUB ST(0),ST(1)

        FCOMP dword ptr [ESP + 010h]

        FNSTSW AX

        TEST AH,041h

        FSTP ST(0)

        JNZ postureAt20032cb4

        FLD dword ptr [ESP + 018h]

        FSUB dword ptr [ESP + 010h]
    postureAt20032c92:
        FST dword ptr [ESI + posturePsb0]

        FMUL dword ptr [postureConstant200ac480]

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs44],EAX

        JMP postureAt20032cb4
    postureAt20032cb2:
        FSTP ST(0)
    postureAt20032cb4:
        MOV EBP,dword ptr [ESP + 01d4h + postureTpmSpace]

        FLD dword ptr [ESI + posturePsb4]

        FSUB dword ptr [EBP + postureExt44]

        FCOM dword ptr [postureConstant200ac138]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032cd9

        FSUB dword ptr [postureConstant200ac380]

        JMP postureAt20032cec
    postureAt20032cd9:
        FCOM dword ptr [postureConstant200ac478]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032cec

        FADD dword ptr [postureConstant200ac380]
    postureAt20032cec:
        FCOM dword ptr [postureConstant200ac190]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032d06

        FSTP ST(0)

        FLD dword ptr [EBP + postureExt44]

        FADD dword ptr [postureConstant200ac190]

        JMP postureAt20032d1c
    postureAt20032d06:
        FCOMP dword ptr [postureConstant200ac474]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032d46

        FLD dword ptr [EBP + postureExt44]

        FSUB dword ptr [postureConstant200ac190]
    postureAt20032d1c:
        PUSH ECX

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FST dword ptr [ESI + posturePsb4]

        FMUL dword ptr [postureConstant200ac480]

        ADD ESP,04h

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles + 4]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs48],EAX
    postureAt20032d46:
        FLD dword ptr [ESI + posturePsb0]

        FSUB dword ptr [EBP + postureExt40]

        FCOM dword ptr [postureConstant200ac138]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032d64

        FSUB dword ptr [postureConstant200ac380]

        JMP postureAt20032d77
    postureAt20032d64:
        FCOM dword ptr [postureConstant200ac478]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032d77

        FADD dword ptr [postureConstant200ac380]
    postureAt20032d77:
        FCOM dword ptr [postureConstant200ac19c]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032dbe

        FSTP ST(0)

        FLD dword ptr [EBP + postureExt40]
    postureAt20032d89:
        FADD dword ptr [postureConstant200ac19c]
    postureAt20032d8f:
        PUSH ECX

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FST dword ptr [ESI + posturePsb0]

        FMUL dword ptr [postureConstant200ac480]

        ADD ESP,04h

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs44],EAX

        JMP postureAt2003344e
    postureAt20032dbe:
        FCOMP dword ptr [postureConstant200ac474]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt2003344e

        FLD dword ptr [EBP + postureExt40]

        FSUB dword ptr [postureConstant200ac190]

        JMP postureAt20032d8f
    postureAt20032dda:
        TEST EBX,080000h

        JZ postureAt2003344e

        FLD dword ptr [ESI + posturePsb4]

        FLD ST(0)

        FSUB dword ptr [ESP + 01ch]

        MOV EBX,dword ptr [ESI + posturePs48]

        MOV dword ptr [ESP + 014h],042200000h

        FCOMP dword ptr [postureConstant200ac138]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032e10

        FSUB dword ptr [postureConstant200ac380]
    postureAt20032e10:
        FLD ST(0)

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [postureConstant200ac478]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032e29

        FADD dword ptr [postureConstant200ac380]
    postureAt20032e29:
        FCOM dword ptr [ESP + 01ch]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032e5d

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac700]

        FSTP dword ptr [ESP + 010h]

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [ESP + 010h]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032eb7

        FLD dword ptr [ESP + 010h]

        FADD dword ptr [ESP + 01ch]

        JMP postureAt20032e95
    postureAt20032e5d:
        FLD dword ptr [ESP + 01ch]

        FCOMP

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032eb5

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac700]

        FSTP dword ptr [ESP + 010h]

        FLD dword ptr [ESP + 01ch]

        FSUB ST(0),ST(1)

        FCOMP dword ptr [ESP + 010h]

        FNSTSW AX

        TEST AH,041h

        FSTP ST(0)

        JNZ postureAt20032eb7

        FLD dword ptr [ESP + 01ch]

        FSUB dword ptr [ESP + 010h]
    postureAt20032e95:
        FST dword ptr [ESI + posturePsb4]

        FMUL dword ptr [postureConstant200ac480]

        CALL PM_WeaponTruncateST0

        MOV EBX,EAX

        MOV EAX,dword ptr [EDI + postureCmdAngles + 4]

        AND EBX,0ffffh

        SUB EBX,EAX

        JMP postureAt20032eb7
    postureAt20032eb5:
        FSTP ST(0)
    postureAt20032eb7:
        CMP EBP,03eh

        MOV EBP,dword ptr [ESP + 01d4h + postureTpmSpace]

        JNZ postureAt20032f5a

        FLD dword ptr [ESI + posturePsb4]

        FSUB dword ptr [EBP + postureExt44]

        MOV dword ptr [ESP + 014h],041a00000h

        FCOM dword ptr [postureConstant200ac138]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032eed

        FSUB dword ptr [postureConstant200ac380]

        JMP postureAt20032f00
    postureAt20032eed:
        FCOM dword ptr [postureConstant200ac478]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032f00

        FADD dword ptr [postureConstant200ac380]
    postureAt20032f00:
        FCOM dword ptr [postureConstant200ac19c]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032f1a

        FSTP ST(0)

        FLD dword ptr [EBP + postureExt44]

        FADD dword ptr [postureConstant200ac19c]

        JMP postureAt20032f30
    postureAt20032f1a:
        FCOMP dword ptr [postureConstant200ac46c]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032f5a

        FLD dword ptr [EBP + postureExt44]

        FSUB dword ptr [postureConstant200ac19c]
    postureAt20032f30:
        PUSH ECX

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FST dword ptr [ESI + posturePsb4]

        FMUL dword ptr [postureConstant200ac480]

        ADD ESP,04h

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles + 4]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs48],EAX
    postureAt20032f5a:
        FLD dword ptr [ESI + posturePsb0]

        FSUB dword ptr [EBP + postureExt40]

        FCOM dword ptr [postureConstant200ac138]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032f78

        FSUB dword ptr [postureConstant200ac380]

        JMP postureAt20032f8b
    postureAt20032f78:
        FCOM dword ptr [postureConstant200ac478]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032f8b

        FADD dword ptr [postureConstant200ac380]
    postureAt20032f8b:
        FCOM dword ptr [ESP + 014h]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032fa1

        FSTP ST(0)

        FLD dword ptr [ESP + 014h]

        FADD dword ptr [EBP + postureExt40]

        JMP postureAt20032fb9
    postureAt20032fa1:
        FLD dword ptr [ESP + 014h]

        FCHS

        FXCH ST(1)

        FCOMPP

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032fe3

        FLD dword ptr [EBP + postureExt40]

        FSUB dword ptr [ESP + 014h]
    postureAt20032fb9:
        PUSH ECX

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FST dword ptr [ESI + posturePsb0]

        FMUL dword ptr [postureConstant200ac480]

        ADD ESP,04h

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs44],EAX
    postureAt20032fe3:
        FLD dword ptr [ESI + posturePsb4]

        FCOMP dword ptr [ESP + 01ch]

        FNSTSW AX

        TEST AH,040h

        JNZ postureAt2003344e

        MOV EAX,[postureConstant200b4080]

        MOV ECX,dword ptr [postureConstant200b4084]

        MOV EDX,dword ptr [postureConstant200b4088]

        MOV dword ptr [ESP + 030h],EAX

        MOV EAX,[postureConstant200b408c]

        MOV dword ptr [ESP + 034h],ECX

        MOV ECX,dword ptr [postureConstant200b4090]

        MOV dword ptr [ESP + 048h],EAX

        MOV EAX,dword ptr [ESI + posturePsf4]

        MOV dword ptr [ESP + 038h],EDX

        MOV EDX,dword ptr [postureConstant200b4094]

        MOV dword ptr [ESP + 014h],03f800000h

        TEST AH,02h

        MOV dword ptr [ESP + 04ch],ECX

        MOV dword ptr [ESP + 050h],EDX

        JZ postureAt200330a1

        FLD dword ptr [ESP + 030h]

        FMUL dword ptr [postureConstant200ac6e8]

        MOV dword ptr [ESP + 014h],03fa00000h

        FSTP dword ptr [ESP + 030h]

        FLD dword ptr [ESP + 034h]

        FMUL dword ptr [postureConstant200ac6e8]

        FSTP dword ptr [ESP + 034h]

        FLD dword ptr [ESP + 038h]

        FMUL dword ptr [postureConstant200ac6e8]

        FSTP dword ptr [ESP + 038h]

        FLD dword ptr [ESP + 048h]

        FMUL dword ptr [postureConstant200ac6e8]

        FSTP dword ptr [ESP + 048h]

        FLD dword ptr [ESP + 04ch]

        FMUL dword ptr [postureConstant200ac6e8]

        FSTP dword ptr [ESP + 04ch]

        FLD dword ptr [ESP + 050h]

        FMUL dword ptr [postureConstant200ac6e8]

        FSTP dword ptr [ESP + 050h]
    postureAt200330a1:
        PUSH 00h

        LEA EAX,[ESP + 058h]

        PUSH 00h

        PUSH EAX

        LEA EAX,[ESI + posturePsb0]

        PUSH EAX

        CALL AngleVectors

        LEA ECX,[ESP + 064h]

        MOV dword ptr [ESP + 06ch],00h

        PUSH ECX

        CALL VectorNormalizeFast

        FLD dword ptr [ESP + 068h]

        FMUL dword ptr [ESP + 028h]

        MOV EAX,[pm]

        FMUL dword ptr [postureConstant200ac2d4]

        FSUBR dword ptr [ESI + posturePs14]

        FST dword ptr [ESP + 038h]

        FLD dword ptr [ESP + 06ch]

        FMUL dword ptr [ESP + 028h]

        FMUL dword ptr [postureConstant200ac2d4]

        FSUBR dword ptr [ESI + posturePs18]

        FSTP dword ptr [ESP + 03ch]

        FLD dword ptr [ESI + posturePs1c]

        FADD dword ptr [postureConstant200ac290]

        MOV EDX,dword ptr [ESP + 03ch]

        MOV dword ptr [ESP + 054h],EDX

        MOV EDX,dword ptr [ESI + posturePsa0]

        FSTP dword ptr [ESP + 040h]

        FSTP dword ptr [ESP + 050h]

        FLD dword ptr [ESP + 040h]

        FSUB dword ptr [postureConstant200ac71c]

        FSUB dword ptr [postureConstant200ac290]

        FSTP dword ptr [ESP + 058h]

        MOV ECX,dword ptr [EAX + posturePmMask]

        PUSH ECX

        LEA ECX,[ESP + 054h]

        PUSH EDX

        PUSH ECX

        LEA EDX,[ESP + 068h]

        LEA ECX,[ESP + 050h]

        PUSH EDX

        PUSH ECX

        LEA EDX,[ESP + 04ch]

        LEA ECX,[ESP + 08ch]

        PUSH EDX

        PUSH ECX

        CALL dword ptr [EAX + posturePmTrace]

        MOV EAX,dword ptr [ESP + 094h + postureTraceStartsolid]

        ADD ESP,030h

        TEST EAX,EAX

        JZ postureAt2003317c

        CMP dword ptr [ESP + 064h + postureTraceEntity],040h

        JL postureAt2003317c

        MOV EDX,dword ptr [ESP + 01ch]

        FLD dword ptr [ESP + 01ch]

        MOV dword ptr [ESI + posturePsb4],EDX

        JMP postureAt20033436
    postureAt2003317c:
        FLD dword ptr [ESP + 064h + postureTraceEndZ]

        MOV EAX,dword ptr [ESP + 064h + postureTraceEnd]

        MOV ECX,dword ptr [ESP + 064h + postureTraceEndY]

        FADD dword ptr [postureConstant200ac71c]

        MOV EDX,dword ptr [ESP + 064h + postureTraceEndZ]

        MOV dword ptr [ESP + 024h],EAX

        MOV dword ptr [ESP + 03ch],EAX

        MOV EAX,[pm]

        FSTP dword ptr [ESP + 044h]

        MOV dword ptr [ESP + 028h],ECX

        MOV dword ptr [ESP + 02ch],EDX

        MOV dword ptr [ESP + 040h],ECX

        MOV EDX,dword ptr [EAX + posturePmMask]

        MOV ECX,dword ptr [ESI + posturePsa0]

        PUSH EDX

        LEA EDX,[ESP + 040h]

        PUSH ECX

        PUSH EDX

        LEA ECX,[ESP + 054h]

        LEA EDX,[ESP + 03ch]

        PUSH ECX

        PUSH EDX

        LEA ECX,[ESP + 038h]

        LEA EDX,[ESP + 078h]

        PUSH ECX

        PUSH EDX

        CALL dword ptr [EAX + posturePmTrace]

        MOV EAX,dword ptr [ESP + 080h + postureTraceAllsolid]

        ADD ESP,01ch

        TEST EAX,EAX

        JZ postureAt20033204

        CMP dword ptr [ESP + 064h + postureTraceEntity],040h

        JL postureAt20033204

        MOV EAX,dword ptr [ESP + 01ch]

        FLD dword ptr [ESP + 01ch]

        MOV dword ptr [ESI + posturePsb4],EAX

        JMP postureAt20033436
    postureAt20033204:
        FLD dword ptr [ESP + 02ch]

        FSUB dword ptr [ESI + posturePs1c]

        MOV dword ptr [ESI + posturePs48],EBX

        FSTP dword ptr [EBP + postureExt3c]

        JMP postureAt2003344e
    postureAt20033216:
        FLD dword ptr [ESI + posturePsb4]

        FLD ST(0)

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [postureConstant200ac138]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20033235

        FSUB dword ptr [postureConstant200ac380]
    postureAt20033235:
        FLD ST(0)

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [postureConstant200ac478]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt2003324e

        FADD dword ptr [postureConstant200ac380]
    postureAt2003324e:
        FCOM dword ptr [ESP + 01ch]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20033282

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac464]

        FSTP dword ptr [ESP + 014h]

        FSUB dword ptr [ESP + 01ch]

        FCOMP dword ptr [ESP + 014h]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt200332dc

        FLD dword ptr [ESP + 014h]

        FADD dword ptr [ESP + 01ch]

        JMP postureAt200332ba
    postureAt20033282:
        FLD dword ptr [ESP + 01ch]

        FCOMP

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt200332da

        FLD dword ptr [pml + posturePmlFrame]

        FMUL dword ptr [postureConstant200ac464]

        FSTP dword ptr [ESP + 014h]

        FLD dword ptr [ESP + 01ch]

        FSUB ST(0),ST(1)

        FCOMP dword ptr [ESP + 014h]

        FNSTSW AX

        TEST AH,041h

        FSTP ST(0)

        JNZ postureAt200332dc

        FLD dword ptr [ESP + 01ch]

        FSUB dword ptr [ESP + 014h]
    postureAt200332ba:
        FST dword ptr [ESI + posturePsb4]

        FMUL dword ptr [postureConstant200ac480]

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles + 4]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs48],EAX

        JMP postureAt200332dc
    postureAt200332da:
        FSTP ST(0)
    postureAt200332dc:
        MOV EBP,dword ptr [ESP + 01d4h + postureTpmSpace]

        TEST EBX,0400000h

        MOV ECX,dword ptr [EBP + postureExt18]

        MOV dword ptr [ESP + 010h],ECX

        JZ postureAt200332fc

        MOV dword ptr [ESP + 014h],00h

        JMP postureAt20033365
    postureAt200332fc:
        TEST BH,080h

        JZ postureAt20033358

        FLD dword ptr [EBP + postureExt24]

        FSUB dword ptr [ESI + posturePsb4]

        PUSH ECX

        MOV dword ptr [ESP + 018h],041600000h

        MOV dword ptr [ESP + 014h],042480000h

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FMUL dword ptr [postureConstant200ac388]

        FMUL dword ptr [postureConstant200ac6e4]

        FCOS

        FSTP dword ptr [ESP + 064h]

        FLD dword ptr [EBP + postureExt20]

        FCHS

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FMUL dword ptr [ESP + 064h]

        FSTP dword ptr [ESP]

        CALL AngleNormalize360

        FCHS

        FSTP dword ptr [EBP + postureExt20]

        ADD ESP,04h

        JMP postureAt20033365
    postureAt20033358:
        FLD dword ptr [EBP + postureExt18]

        FMUL dword ptr [postureConstant200ac180]

        FSTP dword ptr [ESP + 014h]
    postureAt20033365:
        FLD dword ptr [ESI + posturePsb0]

        FSUB dword ptr [EBP + postureExt20]

        PUSH ECX

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FCOM dword ptr [ESP + 018h]

        ADD ESP,04h

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20033390

        FSTP ST(0)

        FLD dword ptr [ESP + 014h]

        FADD dword ptr [EBP + postureExt20]

        JMP postureAt200333a8
    postureAt20033390:
        FLD dword ptr [ESP + 010h]

        FCHS

        FXCH ST(1)

        FCOMPP

        FNSTSW AX

        TEST AH,01h

        JZ postureAt200333d2

        FLD dword ptr [EBP + postureExt20]

        FSUB dword ptr [ESP + 010h]
    postureAt200333a8:
        PUSH ECX

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FST dword ptr [ESI + posturePsb0]

        FMUL dword ptr [postureConstant200ac480]

        ADD ESP,04h

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs44],EAX
    postureAt200333d2:
        MOV EAX,dword ptr [ESI + posturePs68]

        TEST AH,080h

        JNZ postureAt2003344e

        FLD dword ptr [ESI + posturePsb4]

        FSUB dword ptr [EBP + postureExt24]

        MOV EDX,dword ptr [EBP + postureExt1c]

        PUSH ECX

        MOV dword ptr [ESP + 014h],EDX

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FCOM dword ptr [ESP + 014h]

        ADD ESP,04h

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt2003340c

        FSTP ST(0)

        FLD dword ptr [ESP + 010h]

        FADD dword ptr [EBP + postureExt24]

        JMP postureAt20033424
    postureAt2003340c:
        FLD dword ptr [ESP + 010h]

        FCHS

        FXCH ST(1)

        FCOMPP

        FNSTSW AX

        TEST AH,01h

        JZ postureAt2003344e

        FLD dword ptr [EBP + postureExt24]

        FSUB dword ptr [ESP + 010h]
    postureAt20033424:
        PUSH ECX

        FSTP dword ptr [ESP]

        CALL AngleNormalize180

        FST dword ptr [ESI + posturePsb4]

        ADD ESP,04h
    postureAt20033436:
        FMUL dword ptr [postureConstant200ac480]

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [EDI + postureCmdAngles + 4]

        AND EAX,0ffffh

        SUB EAX,ECX

        MOV dword ptr [ESI + posturePs48],EAX
    postureAt2003344e:
        MOV EAX,dword ptr [ESP + 01dch + postureTpmSpace]

        LEA ECX,[ESP + 01cch]

        PUSH ECX

        PUSH EDI

        PUSH ESI

        MOV dword ptr [ESP + 01d8h + posturePmTrace],EAX

        CALL PM_UpdateLean

        ADD ESP,0ch

        POP EDI

        POP EBP

        POP EBX
    postureAt20033471:
        POP ESI

        ADD ESP,postureViewStack

        RET
    }
}
#else

void PM_TCEUpdateViewAngles( playerState_t *ps, pmoveExt_t *pmext, usercmd_t *cmd, void (trace)( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask ), int tracemask ) {	//----(SA)	modified
	short	temp;
	int		i;
	pmove_t tpm;
	vec3_t oldViewAngles;

	// DHM - Nerve :: Added support for PMF_TIME_LOCKPLAYER
	if ( ps->pm_type == PM_INTERMISSION || ps->pm_flags & PMF_TIME_LOCKPLAYER ) {
		return;		// no view changes at all
	}

	if ( ps->pm_type != PM_SPECTATOR && ps->stats[STAT_HEALTH] <= 0 ) {

		// DHM - Nerve :: Allow players to look around while 'wounded' or lock to a medic if nearby
		temp = cmd->angles[1] + ps->delta_angles[1];
		// rain - always allow this.  viewlocking will take precedence
		// if a medic is found
		// rain - using a full short and converting on the client so that
		// we get >1 degree resolution
		ps->stats[STAT_DEAD_YAW] = temp;
		return;		// no view changes at all
	}

	VectorCopy( ps->viewangles, oldViewAngles );

	// circularly clamp the angles with deltas
	for (i=0 ; i<3 ; i++) {
		temp = cmd->angles[i] + ps->delta_angles[i];
		if ( i == PITCH ) {
			// don't let the player look up or down more than 90 degrees
			if ( temp > 16000 ) {
				ps->delta_angles[i] = 16000 - cmd->angles[i];
				temp = 16000;
			} else if ( temp < -16000 ) {
				ps->delta_angles[i] = -16000 - cmd->angles[i];
				temp = -16000;
			}
		}
		ps->viewangles[i] = SHORT2ANGLE(temp);
	}

    PM_TCETacticalView(ps,oldViewAngles);

	/* TC:E EF0x01000000 view constraint precedes ordinary mounts. */
    if (ps->eFlags & 0x01000000) {
        float yaw = ps->viewangles[YAW], oldYaw = oldViewAngles[YAW];
        float diff, center;
        if (yaw - oldYaw > 180.0f) yaw -= 360.0f;
        if (yaw - oldYaw < -180.0f) yaw += 360.0f;
        if (yaw > oldYaw && yaw - oldYaw > 120.0f * pml.frametime) {
            ps->viewangles[YAW] = oldYaw + 120.0f * pml.frametime;
            ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
        } else if (yaw < oldYaw && oldYaw - yaw > 120.0f * pml.frametime) {
            ps->viewangles[YAW] = oldYaw - 120.0f * pml.frametime;
            ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
        }
        center = cos(DEG2RAD(AngleNormalize180(pmext->centerangles[YAW] - ps->viewangles[YAW])));
        center = -AngleNormalize360(center * AngleNormalize180(-pmext->centerangles[PITCH]));
        pmext->centerangles[PITCH] = center;
        diff = AngleNormalize180(ps->viewangles[PITCH] - center);
        if (diff > 20.0f || diff < -20.0f) {
            ps->viewangles[PITCH] = AngleNormalize180(center + (diff > 20.0f ? 20.0f : -20.0f));
            ps->delta_angles[PITCH] = ANGLE2SHORT(ps->viewangles[PITCH]) - cmd->angles[PITCH];
        }
    } else if( BG_PlayerMounted(ps->eFlags) ) {
		float yaw, oldYaw;
		float degsSec = MG42_YAWSPEED;
		float arcMin, arcMax, arcDiff;

		yaw = ps->viewangles[YAW];
		oldYaw = oldViewAngles[YAW];

		if ( yaw - oldYaw > 180 ) {
			yaw -= 360;
		}
		if ( yaw - oldYaw < -180 ) {
			yaw += 360;
		}

		if( yaw > oldYaw ) {
			if( yaw - oldYaw > degsSec * pml.frametime ) {
				ps->viewangles[YAW] = oldYaw + degsSec * pml.frametime;

				// Set delta_angles properly
				ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
			}
		} else if( oldYaw > yaw ) {
			if( oldYaw - yaw > degsSec * pml.frametime ) {
				ps->viewangles[YAW] = oldYaw - degsSec * pml.frametime;

				// Set delta_angles properly
				ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
			}
		}

		// limit harc and varc
		
		// pitch (varc)
		arcMax = pmext->varc;
		if(ps->eFlags & EF_AAGUN_ACTIVE) {
			arcMin = 0;
		} else if (ps->eFlags & EF_MOUNTEDTANK) {
			float angle;

			arcMin = 14;
			arcMax = 50;

			angle = cos( DEG2RAD( AngleNormalize180( pmext->centerangles[ 1 ] - ps->viewangles[ 1 ] )));
			angle = -AngleNormalize360(angle * AngleNormalize180( 0 - pmext->centerangles[ 0 ] ));

			pmext->centerangles[ PITCH ] = angle;
		} else {
			arcMin = pmext->varc / 2;
		}

		arcDiff = AngleNormalize180( ps->viewangles[PITCH] - pmext->centerangles[PITCH] );

		if( arcDiff > arcMin ) {
			ps->viewangles[PITCH] = AngleNormalize180( pmext->centerangles[PITCH] + arcMin );

			// Set delta_angles properly
			ps->delta_angles[PITCH] = ANGLE2SHORT(ps->viewangles[PITCH]) - cmd->angles[PITCH];
		} else if( arcDiff < -arcMax ) {
			ps->viewangles[PITCH] = AngleNormalize180( pmext->centerangles[PITCH] - arcMax );
			
			// Set delta_angles properly
			ps->delta_angles[PITCH] = ANGLE2SHORT(ps->viewangles[PITCH]) - cmd->angles[PITCH];
		}

		if (!(ps->eFlags & EF_MOUNTEDTANK)) {
			// yaw (harc)
			arcMin = arcMax = pmext->harc;
			arcDiff = AngleNormalize180( ps->viewangles[YAW] - pmext->centerangles[YAW] );

			if( arcDiff > arcMin ) {
				ps->viewangles[YAW] = AngleNormalize180( pmext->centerangles[YAW] + arcMin );

				// Set delta_angles properly
				ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
			} else if( arcDiff < -arcMax ) {
				ps->viewangles[YAW] = AngleNormalize180( pmext->centerangles[YAW] - arcMax );
				
				// Set delta_angles properly
				ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
			}
		}
	} else if( ps->weapon == 60 ) {
		float degsSec = 60.f;
		float yaw, oldYaw;
		float pitch, oldPitch;
		float pitchMax = 30.f;
		float yawDiff, pitchDiff;

		yaw = ps->viewangles[YAW];
		oldYaw = oldViewAngles[YAW];

		if ( yaw - oldYaw > 180 ) {
			yaw -= 360;
		}
		if ( yaw - oldYaw < -180 ) {
			yaw += 360;
		}

		if( yaw > oldYaw ) {
			if( yaw - oldYaw > degsSec * pml.frametime ) {
				ps->viewangles[YAW] = oldYaw + degsSec * pml.frametime;

				// Set delta_angles properly
				ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
			}
		} else if( oldYaw > yaw ) {
			if( oldYaw - yaw > degsSec * pml.frametime ) {
				ps->viewangles[YAW] = oldYaw - degsSec * pml.frametime;

				// Set delta_angles properly
				ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
			}
		}

		pitch = ps->viewangles[PITCH];
		oldPitch = oldViewAngles[PITCH];

		if ( pitch - oldPitch > 180 ) {
			pitch -= 360;
		}
		if ( pitch - oldPitch < -180 ) {
			pitch += 360;
		}

		if( pitch > oldPitch ) {
			if( pitch - oldPitch > degsSec * pml.frametime ) {
				ps->viewangles[PITCH] = oldPitch + degsSec * pml.frametime;

				// Set delta_angles properly
				ps->delta_angles[PITCH] = ANGLE2SHORT(ps->viewangles[PITCH]) - cmd->angles[PITCH];
			}
		} else if( oldPitch > pitch ) {
			if( oldPitch - pitch > degsSec * pml.frametime ) {
				ps->viewangles[PITCH] = oldPitch - degsSec * pml.frametime;

				// Set delta_angles properly
				ps->delta_angles[PITCH] = ANGLE2SHORT(ps->viewangles[PITCH]) - cmd->angles[PITCH];
			}
		}

		// yaw
		yawDiff = ps->viewangles[YAW] - pmext->mountedWeaponAngles[YAW];

		if( yawDiff > 180 ) {
			yawDiff -= 360;
		} else if( yawDiff < -180 ) {
			yawDiff += 360;
		}

		if( yawDiff > 30 ) {
			ps->viewangles[YAW] = AngleNormalize180( pmext->mountedWeaponAngles[YAW] + 30.f );

			// Set delta_angles properly
			ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
		} else if( yawDiff < -30 ) {
			ps->viewangles[YAW] = AngleNormalize180( pmext->mountedWeaponAngles[YAW] - 30.f );
			
			// Set delta_angles properly
			ps->delta_angles[YAW] = ANGLE2SHORT(ps->viewangles[YAW]) - cmd->angles[YAW];
		}

		// pitch
		pitchDiff = ps->viewangles[PITCH] - pmext->mountedWeaponAngles[PITCH];

		if( pitchDiff > 180 ) {
			pitchDiff -= 360;
		} else if( pitchDiff < -180 ) {
			pitchDiff += 360;
		}

		if( pitchDiff > (pitchMax -10.f) ) {
			ps->viewangles[PITCH] = AngleNormalize180( pmext->mountedWeaponAngles[PITCH] + (pitchMax -10.f) );

			// Set delta_angles properly
			ps->delta_angles[PITCH] = ANGLE2SHORT(ps->viewangles[PITCH]) - cmd->angles[PITCH];
		} else if( pitchDiff < -(pitchMax) ) {
			ps->viewangles[PITCH] = AngleNormalize180( pmext->mountedWeaponAngles[PITCH] - (pitchMax) );
			
			// Set delta_angles properly
			ps->delta_angles[PITCH] = ANGLE2SHORT(ps->viewangles[PITCH]) - cmd->angles[PITCH];
		}
    } else if (ps->eFlags & EF_PRONE) {
        PM_TCEProneView(ps, pmext, cmd, oldViewAngles[YAW]);
    }

	tpm.trace = trace;
//	tpm.trace (&trace, start, tmins, tmaxs, end, ps->clientNum, MASK_PLAYERSOLID);

	PM_UpdateLean( ps, cmd, &tpm );
}

#endif

/* CQB PM_UpdateViewAngles: shared free-look ellipse and return spring.
 * TC keeps holdable13 for locations; the two free network shorts14/15 carry
 * hundredths of a degree centered on8000 instead of CQB's13/14. */
void PM_UpdateViewAngles(playerState_t *ps, pmoveExt_t *ext, usercmd_t *cmd,
    void (trace)(trace_t *, const vec3_t, const vec3_t, const vec3_t, const vec3_t, int, int), int tracemask) {
    float pitch, yaw, radius, step, dt, kick[2], desired[2], raw[2], limit;
    int i;
    pmove_t leanMove;
    dt = pml.frametime;
    if (dt <= 0 || dt > .2f) dt = .008f;
    kick[0] = ext->tceFreelookRecoil[0];
    kick[1] = ext->tceFreelookRecoil[1];
    ext->tceFreelookRecoil[0] = ext->tceFreelookRecoil[1] = 0;
    if (ps->holdable[TCE_FREELOOK_PITCH] < 500 || ps->holdable[TCE_FREELOOK_PITCH] > 15500 ||
        ps->holdable[TCE_FREELOOK_YAW] < 500 || ps->holdable[TCE_FREELOOK_YAW] > 15500) {
        ps->holdable[TCE_FREELOOK_PITCH] = ps->holdable[TCE_FREELOOK_YAW] = TCE_FREELOOK_CENTER;
    }
    if (ps->pm_type != PM_NORMAL || ps->stats[STAT_HEALTH] <= 0 ||
        (ps->pm_flags & (PMF_TIME_LOCKPLAYER|PMF_LIMBO)) ||
        BG_PlayerMounted(ps->eFlags) || (ps->eFlags & 0x01000000)) {
        ps->holdable[TCE_FREELOOK_PITCH] = ps->holdable[TCE_FREELOOK_YAW] = TCE_FREELOOK_CENTER;
        PM_TCEUpdateViewAngles(ps, ext, cmd, trace, tracemask);
        return;
    }
    pitch = (ps->holdable[TCE_FREELOOK_PITCH] - TCE_FREELOOK_CENTER) * .01f;
    yaw = (ps->holdable[TCE_FREELOOK_YAW] - TCE_FREELOOK_CENTER) * .01f;
    if (!(cmd->buttons & BUTTON_FREELOOK)) {
        radius = sqrt(pitch*pitch + yaw*yaw);
        step = (8.533333778381348f * radius + 15.f) * dt;
        if (radius <= step) pitch = yaw = 0;
        else { pitch *= (radius-step)/radius; yaw *= (radius-step)/radius; }
        ps->holdable[TCE_FREELOOK_PITCH] = (int)(pitch*100 + TCE_FREELOOK_CENTER);
        ps->holdable[TCE_FREELOOK_YAW] = (int)(yaw*100 + TCE_FREELOOK_CENTER);
        PM_TCEUpdateViewAngles(ps, ext, cmd, trace, tracemask);
        return;
    }
    limit = 640.f * dt;
    for (i = 0; i < 2; ++i) {
        raw[i] = SHORT2ANGLE((short)(cmd->angles[i]+ps->delta_angles[i]));
        raw[i] = AngleNormalize180(raw[i] - kick[i] - ps->viewangles[i]);
        if (raw[i] > limit) raw[i] = limit;
        else if (raw[i] < -limit) raw[i] = -limit;
    }
    pitch += raw[PITCH]; yaw += raw[YAW];
    radius = sqrt(pitch*pitch*(pitch > 0 ? 56.f : 14.f) + yaw*yaw);
    desired[PITCH] = ps->viewangles[PITCH] + kick[PITCH];
    desired[YAW] = ps->viewangles[YAW] + kick[YAW];
    if (radius > 75.f) {
        desired[PITCH] += pitch - pitch*75.f/radius;
        desired[YAW] += yaw - yaw*75.f/radius;
        pitch *= 75.f/radius; yaw *= 75.f/radius;
    }
    ps->holdable[TCE_FREELOOK_PITCH] = (int)(pitch*100 + TCE_FREELOOK_CENTER);
    ps->holdable[TCE_FREELOOK_YAW] = (int)(yaw*100 + TCE_FREELOOK_CENTER);
    for (i = 0; i < 2; ++i)
        ps->delta_angles[i] = ANGLE2SHORT(desired[i]) - cmd->angles[i];
    if (radius > 75.f) {
        PM_TCEUpdateViewAngles(ps, ext, cmd, trace, tracemask);
    } else {
        ps->viewangles[PITCH] = AngleNormalize180(desired[PITCH]);
        ps->viewangles[YAW] = AngleNormalize180(desired[YAW]);
        memset(&leanMove, 0, sizeof(leanMove));
        leanMove.trace = trace;
        PM_UpdateLean(ps, cmd, &leanMove);
    }
}

/*
================
PM_CheckLadderMove

  Checks to see if we are on a ladder
================
*/
qboolean	ladderforward;
vec3_t		laddervec;

/* TCE free-climb clearance and stamina gate; no SDK counterpart.
 * Native value stores and callback order are explicit, compiled ABI remains open. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
typedef char climbTraceLayout[(sizeof(trace_t) == 56 && offsetof(trace_t, allsolid) == 0 &&
    offsetof(trace_t, fraction) == 8) ? 1 : -1];
static qboolean PM_ClimbSlideMove(int unused, const vec3_t normal, qboolean wasClimbing) {
    vec3_t origin, point, end, direction, maxs;
    trace_t trace;
    float scale;
    volatile float below, rise;
    long double belowValue, height;
    int axis, attempt;
#if !defined(__linux__)
    int stamina;
#endif
    qboolean nearGround;

    (void)unused;
    scale = (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) ? 1.25f : 1.0f;
    for (axis = 0; axis < 3; ++axis) origin[axis] = pm->ps->origin[axis];
    memcpy(maxs, pm->maxs, sizeof(maxs));
    pm->ps->stats[STAT_TCE_FLAGS] &= ~0x20;
    if (pm->ps->velocity[2] < -200.0f || TCE_FloatIsNaN(pm->ps->velocity[2])) return qfalse;
    memcpy(end, origin, sizeof(end));
    end[2] = (float)((long double)origin[2] - 256.0f);
    pm->trace(&trace, origin, pm->mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    belowValue = (long double)trace.fraction * -256.0f;
    below = (float)belowValue;
    nearGround = belowValue > -17.0f;
    rise = (float)((long double)scale * 96.0f);
    memcpy(end, origin, sizeof(end));
    end[2] = (float)((long double)origin[2] + rise);
    pm->trace(&trace, origin, pm->mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    if (trace.fraction < 1.0f || TCE_FloatIsNaN(trace.fraction))
        rise = (float)((long double)trace.fraction * rise);
    if (!(rise >= 18.0f)) return qfalse;

    for (attempt = 0; attempt < 2; ++attempt) {
        if (attempt) rise = (float)(((long double)rise - 18.0f) * 0.5f + 18.0f);
        memcpy(point, origin, sizeof(point));
        point[2] = (float)((long double)origin[2] + rise);
        direction[0] = -normal[0];
        direction[1] = -normal[1];
        direction[2] = 0.0f;
        VectorNormalize(direction);
        for (axis = 0; axis < 3; ++axis)
            point[axis] = (float)((long double)direction[axis] * 4.0f + point[axis]);
        /* The clearance trace deliberately aliases its start and end pointers. */
        pm->trace(&trace, point, pm->mins, maxs, point, pm->ps->clientNum, pm->tracemask);
        if (!trace.allsolid) break;
    }
    if (trace.allsolid) return qfalse;
    memcpy(point, origin, sizeof(point));
    point[2] = (float)((long double)origin[2] + rise);
    direction[0] = -normal[0];
    direction[1] = -normal[1];
    direction[2] = 0.0f;
    VectorNormalize(direction);
    for (axis = 0; axis < 3; ++axis)
        point[axis] = (float)((long double)direction[axis] * 4.0f + point[axis]);
    memcpy(end, point, sizeof(end));
    end[2] = (float)((long double)point[2] - 256.0f);
    pm->trace(&trace, point, pm->mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    height = (long double)rise - below;
    height -= (long double)trace.fraction * 256.0f;
    if (!(height >= 63.0f)) {
        pm->ps->stats[STAT_TCE_FLAGS] |= 0x40;
        return qfalse;
    }
    if (height > (long double)scale * 101.0f) return qfalse;
#if defined(__linux__)
    if (pm->pmext->sprintTime < 900 && !wasClimbing) return qfalse;
    if (pm->pmext->sprintTime < 10) return qfalse;
#else
    stamina = pm->pmext->sprintTime;
    if (stamina < 900 && !wasClimbing) return qfalse;
    if (stamina < 10) return qfalse;
#endif
    pm->ps->stats[STAT_TCE_FLAGS] |= 0x20;
    return !nearGround;
}
#else
static qboolean PM_ClimbSlideMove(int unused, const vec3_t normal, qboolean wasClimbing) {
    vec3_t origin, end, direction, maxs;
    trace_t trace;
    float scale, below, rise, height;
    (void)unused;
    scale = (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) ? 1.25f : 1.f;
    VectorCopy(pm->ps->origin, origin);
    VectorCopy(pm->maxs, maxs);
    pm->ps->stats[STAT_TCE_FLAGS] &= ~0x20;
    if (pm->ps->velocity[2] < -200.f) return qfalse;
    VectorCopy(origin, end); end[2] -= 256.f;
    pm->trace(&trace, origin, pm->mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    below = trace.fraction * -256.f;
    rise = scale * 96.f;
    VectorCopy(origin, end); end[2] += rise;
    pm->trace(&trace, origin, pm->mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    if (trace.fraction < 1.f) rise *= trace.fraction;
    if (rise < 18.f) return qfalse;
    VectorSet(direction, -normal[0], -normal[1], 0);
    VectorNormalize(direction);
    VectorMA(origin, 4.f, direction, end); end[2] += rise;
    pm->trace(&trace, end, pm->mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    if (trace.allsolid) {
        rise = (rise - 18.f) * 0.5f + 18.f;
        VectorMA(origin, 4.f, direction, end); end[2] += rise;
        pm->trace(&trace, end, pm->mins, maxs, end, pm->ps->clientNum, pm->tracemask);
        if (trace.allsolid) return qfalse;
    }
    VectorMA(origin, 4.f, direction, origin); origin[2] += rise;
    VectorCopy(origin, end); end[2] -= 256.f;
    pm->trace(&trace, origin, pm->mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    height = (rise - below) - trace.fraction * 256.f;
    if (height < 63.f) { pm->ps->stats[STAT_TCE_FLAGS] |= 0x40; return qfalse; }
    if (height > scale * 101.f) return qfalse;
    if (pm->pmext->sprintTime < 900 && !wasClimbing) return qfalse;
    if (pm->pmext->sprintTime < 10) return qfalse;
    pm->ps->stats[STAT_TCE_FLAGS] |= 0x20;
    return below <= -17.f;
}
#endif

/* Preserve the TC bot identity0x800 used by weapon logic. Only a trusted
 * server-local navigation command bypasses its original free-climb ban;
 * every clearance, surface, stamina and movement gate remains unchanged. */
#if !defined(_MSC_VER) || !defined(_M_IX86)
static qboolean PM_FreeClimbActorAllowed(void) {
 if(!(pm->ps->stats[STAT_TCE_FLAGS]&0x800))return qtrue;
#ifdef GAMEDLL
 return pm->tceBotFreeClimb;
#else
 return qfalse;
#endif
}
#endif
/* TC qagame20033480: original control flow; undefined freeClimb remains a documented boundary. */
#if defined(_MSC_VER) && defined(_M_IX86)
/* Native trace layout and movement protocol bounds retained after C reconstruction. */
typedef char move832TraceLayout[(sizeof(trace_t)==56 && offsetof(trace_t,fraction)==8 && offsetof(trace_t,plane)==24 && offsetof(trace_t,surfaceFlags)==44 && offsetof(trace_t,contents)==48 && offsetof(trace_t,entityNum)==52) ? 1 : -1];
typedef char move832Protocol[(offsetof(pmove_t,ps)==0 && SK_BATTLE_SENSE==0 && offsetof(cplane_t,normal)==0 && PMF_LADDER==4 && PMF_DUCKED==1 && EF_PRONE==0x80000 && EF_PRONE_MOVING==0x100000 && EV_FOOTSTEP==1 && ANIM_ET_CLIMB_MOUNT==9 && ANIM_ET_CLIMB_DISMOUNT==10 && sizeof(tce_weaponDef_t)==460) ? 1 : -1];
#endif
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
void PM_CheckLadderMove(void) {
    vec3_t spot, flatforward, mins;
    trace_t trace;
    float tracedist;
    long double distance, normalAlignment;
    int i;
    qboolean wasOnLadder, wasClimbing, freeClimb = qfalse;
    qboolean ladderHit, backTrace, acceptHit;

    /* Keep the existing defined fallback for the original unset freeClimb local. */
    pm->ps->stats[STAT_TCE_FLAGS] &= ~0x40;
    pm->ps->stats[STAT_TCE_FLAGS] &= ~0x20;
    tracedist = pml.walking ? 1.0f : 48.0f;
    VectorSet(flatforward, pml.forward[0], pml.forward[1], 0);
    VectorNormalize(flatforward);
    for (i = 0; i < 3; ++i)
        spot[i] = (float)((long double)flatforward[i] * tracedist + pm->ps->origin[i]);
    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, spot,
        pm->ps->clientNum, pm->tracemask);
    if (trace.fraction >= 1.0f) pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x80;
    else if ((trace.surfaceFlags & 0xff000000) == 0x14000000 &&
        !(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x80)) {
        PM_AddEvent(EV_TCE_FENCE_TOUCH);
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x80;
    }
    pml.tceLadderSurfaceFlags = trace.surfaceFlags;
    wasOnLadder = (pm->ps->pm_flags & PMF_LADDER) != 0;
    wasClimbing = (pm->ps->stats[STAT_TCE_FLAGS] & 0x80) != 0;
    pml.ladder = qfalse;
    pm->ps->pm_flags &= ~PMF_LADDER;
    pm->ps->stats[STAT_TCE_FLAGS] &= ~0x80;
    ladderforward = qfalse;
    if (pm->ps->stats[STAT_HEALTH] < 1) {
        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = qfalse;
        pml.walking = qfalse;
        return;
    }
    if (pm->ps->eFlags & (EF_PRONE | EF_PRONE_MOVING)) return;
    if (pm->ps->weaponstate == 9 || pm->ps->weaponstate == 10 || pm->ps->weaponstate == 11) return;
    distance = (long double)trace.fraction * tracedist;
#if defined(__linux__)
    ladderHit = trace.fraction < 1.0f;
#else
    ladderHit = trace.fraction < 1.0f || TCE_FloatIsNaN(trace.fraction);
#endif
    if (ladderHit && (trace.surfaceFlags & SURF_LADDER)) {
        if (pml.walking || (pm->ps->velocity[2] >= -400.0f &&
            !TCE_FloatIsNaN(pm->ps->velocity[2]))) pml.ladder = qtrue;
    } else if ((distance < 1.0f || distance != distance) &&
        PM_FreeClimbActorAllowed() && !(trace.surfaceFlags & 0x40000) &&
        !(trace.contents & 0x6000000) && trace.entityNum == ENTITYNUM_WORLD) {
#if defined(__linux__)
        /* Linux retains the complete dot product with the fixed up vector. */
        normalAlignment = (long double)trace.plane.normal[0] * 0.0f;
        normalAlignment += (long double)trace.plane.normal[1] * 0.0f;
        normalAlignment += (long double)trace.plane.normal[2] * 1.0f;
#else
        normalAlignment = trace.plane.normal[2];
#endif
        if ((fabsl(normalAlignment) < 0.001 || normalAlignment != normalAlignment) &&
            PM_ClimbSlideMove(1, trace.plane.normal, wasClimbing)) {
            pml.ladder = qtrue;
            freeClimb = qtrue;
            pm->ps->stats[STAT_TCE_FLAGS] |= 0x80;
        }
    }
    if (pml.ladder) {
        VectorCopy(trace.plane.normal, laddervec);
        /* The climb helper may have changed movement state; read walking afresh. */
        distance = (long double)trace.fraction * tracedist;
        backTrace = distance > 1.0;
#if defined(__linux__)
        backTrace = backTrace || distance != distance;
#endif
        if (!pml.walking && backTrace) {
            pml.ladder = qfalse;
            VectorCopy(pm->mins, mins);
            mins[2] = -1.0f;
            for (i = 0; i < 3; ++i)
                spot[i] = (float)(-(long double)tracedist * trace.plane.normal[i] + pm->ps->origin[i]);
            pm->trace(&trace, pm->ps->origin, mins, pm->maxs, spot,
                pm->ps->clientNum, pm->tracemask);
            acceptHit = freeClimb;
#if defined(__linux__)
            /* Compiled Linux accepts an ordered hit even without ladder material. */
            ladderHit = trace.fraction < 1.0f;
            acceptHit = qtrue;
#else
            ladderHit = trace.fraction < 1.0f || TCE_FloatIsNaN(trace.fraction);
#endif
            if (ladderHit && ((trace.surfaceFlags & SURF_LADDER) || acceptHit)) {
                ladderforward = qtrue;
                pml.ladder = qtrue;
                pm->ps->pm_flags |= PMF_LADDER;
                if (!(trace.surfaceFlags & SURF_LADDER)) pm->ps->stats[STAT_TCE_FLAGS] |= 0x80;
            }
        } else pm->ps->pm_flags |= PMF_LADDER;
        if (pml.ladder && pml.walking && pm->cmd.forwardmove <= 0) pml.ladder = qfalse;
    }
    if (wasClimbing != ((pm->ps->stats[STAT_TCE_FLAGS] & 0x80) != 0))
        PM_AddEventExt(EV_FOOTSTEP, wasClimbing ? 17 : 16);
    if (!pml.ladder && wasOnLadder && pm->ps->velocity[2] > 0.0f)
        BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, ANIM_ET_CLIMB_DISMOUNT, qfalse, qfalse);
    if (pml.ladder && !wasOnLadder &&
        (pm->ps->velocity[2] < 0.0f || TCE_FloatIsNaN(pm->ps->velocity[2])))
        BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, ANIM_ET_CLIMB_MOUNT, qfalse, qfalse);
}
#else
/* Portable fallback; no instruction-level Linux parity claim. */
void PM_CheckLadderMove(void) {
    vec3_t spot, flatforward, mins;
    trace_t trace;
    float tracedist;
    qboolean wasOnLadder, wasClimbing, freeClimb = qfalse;
    pm->ps->stats[STAT_TCE_FLAGS] &= ~(0x40 | 0x20);
    tracedist = pml.walking ? 1.f : 48.f;
    VectorSet(flatforward, pml.forward[0], pml.forward[1], 0);
    VectorNormalize(flatforward);
    VectorMA(pm->ps->origin, tracedist, flatforward, spot);
    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, spot, pm->ps->clientNum, pm->tracemask);
    if (trace.fraction >= 1.f) pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x80;
    else if ((trace.surfaceFlags & 0xff000000) == 0x14000000 &&
             !(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x80)) {
        PM_AddEvent(EV_TCE_FENCE_TOUCH);
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x80;
    }
    pml.tceLadderSurfaceFlags = trace.surfaceFlags;
    wasOnLadder = (pm->ps->pm_flags & PMF_LADDER) != 0;
    wasClimbing = (pm->ps->stats[STAT_TCE_FLAGS] & 0x80) != 0;
    pml.ladder = qfalse;
    pm->ps->pm_flags &= ~PMF_LADDER;
    pm->ps->stats[STAT_TCE_FLAGS] &= ~0x80;
    ladderforward = qfalse;
    if (pm->ps->stats[STAT_HEALTH] < 1) {
        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = pml.walking = qfalse;
        return;
    }
    if (pm->ps->eFlags & (EF_PRONE | EF_PRONE_MOVING)) return;
    if (pm->ps->weaponstate >= 9 && pm->ps->weaponstate <= 11) return;
    if (trace.fraction < 1.f && (trace.surfaceFlags & SURF_LADDER)) {
        if (pml.walking || pm->ps->velocity[2] >= -400.f) pml.ladder = qtrue;
    } else if (trace.fraction * tracedist < 1.f &&
               PM_FreeClimbActorAllowed() &&
               !(trace.surfaceFlags & 0x40000) && !(trace.contents & 0x6000000) &&
               trace.entityNum == ENTITYNUM_WORLD && fabs(trace.plane.normal[2]) < 0.001 &&
               PM_ClimbSlideMove(1, trace.plane.normal, wasClimbing)) {
        pml.ladder = qtrue;
        freeClimb = qtrue;
        pm->ps->stats[STAT_TCE_FLAGS] |= 0x80;
    }
    if (pml.ladder) {
        VectorCopy(trace.plane.normal, laddervec);
        if (!pml.walking && trace.fraction * tracedist > 1.f) {
            pml.ladder = qfalse;
            VectorCopy(pm->mins, mins); mins[2] = -1.f;
            VectorMA(pm->ps->origin, -tracedist, laddervec, spot);
            pm->trace(&trace, pm->ps->origin, mins, pm->maxs, spot, pm->ps->clientNum, pm->tracemask);
            if (trace.fraction < 1.f && ((trace.surfaceFlags & SURF_LADDER) || freeClimb)) {
                ladderforward = pml.ladder = qtrue;
                pm->ps->pm_flags |= PMF_LADDER;
                if (!(trace.surfaceFlags & SURF_LADDER)) pm->ps->stats[STAT_TCE_FLAGS] |= 0x80;
            }
        } else pm->ps->pm_flags |= PMF_LADDER;
        if (pml.ladder && pml.walking && pm->cmd.forwardmove <= 0) pml.ladder = qfalse;
    }
    if (wasClimbing != ((pm->ps->stats[STAT_TCE_FLAGS] & 0x80) != 0))
        PM_AddEventExt(EV_FOOTSTEP, wasClimbing ? 17 : 16);
    if (!pml.ladder && wasOnLadder && pm->ps->velocity[2] > 0)
        BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, ANIM_ET_CLIMB_DISMOUNT, qfalse, qfalse);
    if (pml.ladder && !wasOnLadder && pm->ps->velocity[2] < 0)
        BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, ANIM_ET_CLIMB_MOUNT, qfalse, qfalse);
}
#endif

/*
============
PM_LadderMove
============
*/
/* SDK ladder controller: native TCE flow matches this routine.
 * Original FPU predicates/schedules and lower-level ABI remain separately open. */
void PM_LadderMove( void ) {
	float wishspeed, scale;
	vec3_t wishdir, wishvel;
	float upscale;

	if ( ladderforward ) {
		// move towards the ladder
		VectorScale( laddervec, -200.0, wishvel );
		pm->ps->velocity[0] = wishvel[0];
		pm->ps->velocity[1] = wishvel[1];
	}

	upscale = ( pml.forward[2] + 0.5 ) * 2.5;
	if ( upscale > 1.0 ) {
		upscale = 1.0;
	} else if ( upscale < -1.0 ) {
		upscale = -1.0;
	}

	// forward/right should be horizontal only
	pml.forward[2] = 0;
	pml.right[2] = 0;
	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	// move depending on the view, if view is straight forward, then go up
	// if view is down more then X degrees, start going down
	// if they are back pedalling, then go in reverse of above
	scale = PM_CmdScale( &pm->cmd );
	VectorClear( wishvel );

	if ( pm->cmd.forwardmove ) {
		wishvel[2] = 0.9 * upscale * scale * (float)pm->cmd.forwardmove;
	}
//Com_Printf("wishvel[2] = %i, fwdmove = %i\n", (int)wishvel[2], (int)pm->cmd.forwardmove );

	if ( pm->cmd.rightmove ) {
		// strafe, so we can jump off ladder
		vec3_t ladder_right, ang;
		vectoangles( laddervec, ang );
		AngleVectors( ang, NULL, ladder_right, NULL );

		// if we are looking away from the ladder, reverse the right vector
		if ( DotProduct( laddervec, pml.forward ) < 0 ) {
			VectorInverse( ladder_right );
		}

		//VectorMA( wishvel, 0.5 * scale * (float)pm->cmd.rightmove, pml.right, wishvel );
		VectorMA( wishvel, 0.5 * scale * (float)pm->cmd.rightmove, ladder_right, wishvel );
	}

	// do strafe friction
	PM_Friction();

	if ( pm->ps->velocity[0] < 1 && pm->ps->velocity[0] > -1  ) {
		pm->ps->velocity[0] = 0;
	}
	if ( pm->ps->velocity[1] < 1 && pm->ps->velocity[1] > -1 ) {
		pm->ps->velocity[1] = 0;
	}

	wishspeed = VectorNormalize2( wishvel, wishdir );

	PM_Accelerate( wishdir, wishspeed, pm_accelerate );
	if ( !wishvel[2] ) {
		if ( pm->ps->velocity[2] > 0 ) {
			pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
			if ( pm->ps->velocity[2] < 0 ) {
				pm->ps->velocity[2]  = 0;
			}
		} else
		{
			pm->ps->velocity[2] += pm->ps->gravity * pml.frametime;
			if ( pm->ps->velocity[2] > 0 ) {
				pm->ps->velocity[2]  = 0;
			}
		}
	}

//Com_Printf("vel[2] = %i\n", (int)pm->ps->velocity[2] );

	PM_StepSlideMove( qfalse );  // no gravity while going up ladder

	// always point legs forward
	pm->ps->movementDir = 0;
}

/*
==============
PM_Sprint
==============
*/
/* Native TCE sprint controller. Held arithmetic models native values;
 * compiler x87 precision, exceptions and conversion ABI remain open. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
void PM_Sprint(void) {
    long double cost, carriedCost, speed, value;
#if defined(__linux__)
    volatile float storedSpeed;
#endif
    unsigned counter;
    int recharge;
    pmoveExt_t *ext;
    qboolean sprinting = (pm->cmd.buttons & BUTTON_SPRINT) &&
        (pm->cmd.forwardmove || pm->cmd.rightmove) &&
        !(pm->ps->pm_flags & PMF_DUCKED) && !(pm->ps->eFlags & EF_PRONE);

    if (sprinting) {
        if (pm->ps->powerups[PW_ADRENALINE]) {
            pm->pmext->sprintTime = 20000;
        } else if (pm->ps->powerups[PW_NOFATIGUE]) {
            /* Native ADD wraps before the signed lower clamp. */
            counter = (unsigned)pm->ps->powerups[PW_NOFATIGUE] - 50u;
            memcpy(&pm->ps->powerups[PW_NOFATIGUE], &counter, sizeof(counter));
            counter = (unsigned)pm->pmext->sprintTime + 10u;
            memcpy(&pm->pmext->sprintTime, &counter, sizeof(counter));
            if (pm->pmext->sprintTime > 20000) pm->pmext->sprintTime = 20000;
            if (pm->ps->powerups[PW_NOFATIGUE] < 0) pm->ps->powerups[PW_NOFATIGUE] = 0;
        } else {
            cost = weaponDef[pm->ps->weapon].loadoutWeight == 4 ? 3000.0f :
                weaponDef[pm->ps->weapon].loadoutWeight == 5 ? 3500.0f : 2500.0f;
            carriedCost = pm->ps->holdable[9] == 4 ? 3000.0f :
                pm->ps->holdable[9] == 5 ? 3500.0f : 2500.0f;
            if (carriedCost > cost) cost += (carriedCost - cost) * 0.33f;
            ext = pm->pmext;
            value = (long double)pml.frametime * cost;
            value *= 0.5f;
            value = (long double)ext->sprintTime - value;
            ext->sprintTime = TCE_TruncateNativeInteger(value);
        }
        if (pm->pmext->sprintTime < 0) pm->pmext->sprintTime = 0;
    } else if (pm->ps->stats[STAT_TCE_FLAGS] & 0x80) {
        ext = pm->pmext;
        value = (long double)pml.frametime * 1250.0f;
        value = (long double)ext->sprintTime - value;
        ext->sprintTime = TCE_TruncateNativeInteger(value);
        if (pm->pmext->sprintTime < 0) pm->pmext->sprintTime = 0;
    } else {
        if (pm->ps->powerups[PW_ADRENALINE]) {
            pm->pmext->sprintTime = 20000;
        } else if (pm->ps->powerups[PW_NOFATIGUE]) {
            counter = (unsigned)pm->pmext->sprintTime + 10u;
            memcpy(&pm->pmext->sprintTime, &counter, sizeof(counter));
        } else {
            recharge = 750;
#ifdef GAMEDLL
            if (pm->leadership) recharge = 1000;
            else
#endif
            if (pm->skill[SK_BATTLE_SENSE] >= 2) recharge = 1200;
            speed = (long double)pm->ps->velocity[0] * pm->ps->velocity[0];
            speed += (long double)pm->ps->velocity[1] * pm->ps->velocity[1];
            speed = sqrtl(speed);
#if defined(__linux__)
            /* Both ELF originals store horizontal speed before comparing. */
            storedSpeed = (float)speed;
            speed = storedSpeed;
#endif
            /* Native C0/C3 also recharges for an unordered speed. */
            if (!(speed > 120.0f) || (!pm->cmd.forwardmove && !pm->cmd.rightmove) ||
                (pm->ps->pm_flags & PMF_DUCKED) || (pm->ps->eFlags & EF_PRONE) ||
                (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4)) {
                ext = pm->pmext;
                value = (long double)recharge * pml.frametime;
                value += ext->sprintTime;
                ext->sprintTime = TCE_TruncateNativeInteger(value);
            }
            ext = pm->pmext;
            if (ext->sprintTime > 5000) {
                value = (long double)recharge * pml.frametime;
                value += ext->sprintTime;
                ext->sprintTime = TCE_TruncateNativeInteger(value);
            }
        }
        if (pm->pmext->sprintTime > 20000) pm->pmext->sprintTime = 20000;
        pm->ps->sprintExertTime = 0;
        return;
    }
    if (!pm->ps->sprintExertTime) pm->ps->sprintExertTime = 1;
}
#else
/* Portable fallback; no instruction-level Linux parity claim. */
void PM_Sprint(void) {
    float cost, carriedCost, speed;
    int recharge;
    qboolean sprinting = (pm->cmd.buttons & BUTTON_SPRINT) &&
        (pm->cmd.forwardmove || pm->cmd.rightmove) &&
        !(pm->ps->pm_flags & PMF_DUCKED) && !(pm->ps->eFlags & EF_PRONE);
    if (sprinting) {
        if (pm->ps->powerups[PW_ADRENALINE]) pm->pmext->sprintTime = 20000;
        else if (pm->ps->powerups[PW_NOFATIGUE]) {
            pm->ps->powerups[PW_NOFATIGUE] -= 50;
            pm->pmext->sprintTime += 10;
            if (pm->pmext->sprintTime > 20000) pm->pmext->sprintTime = 20000;
            if (pm->ps->powerups[PW_NOFATIGUE] < 0) pm->ps->powerups[PW_NOFATIGUE] = 0;
        } else {
            cost = weaponDef[pm->ps->weapon].loadoutWeight == 4 ? 3000.f :
                   weaponDef[pm->ps->weapon].loadoutWeight == 5 ? 3500.f : 2500.f;
            carriedCost = pm->ps->holdable[9] == 4 ? 3000.f :
                          pm->ps->holdable[9] == 5 ? 3500.f : 2500.f;
            if (cost < carriedCost) cost += (carriedCost-cost)*0.33f;
            pm->pmext->sprintTime = (int)(pm->pmext->sprintTime - cost*pml.frametime*0.5f);
        }
    } else if (pm->ps->stats[STAT_TCE_FLAGS] & 0x80) {
        pm->pmext->sprintTime = (int)(pm->pmext->sprintTime - 1250.f*pml.frametime);
    } else {
        if (pm->ps->powerups[PW_ADRENALINE]) pm->pmext->sprintTime = 20000;
        else if (pm->ps->powerups[PW_NOFATIGUE]) pm->pmext->sprintTime += 10;
        else {
            recharge = pm->skill[SK_BATTLE_SENSE] >= 2 ? 1200 : 750;
#ifdef GAMEDLL
            if (pm->leadership) recharge = 1000;
#endif
            speed = sqrt(pm->ps->velocity[0]*pm->ps->velocity[0] + pm->ps->velocity[1]*pm->ps->velocity[1]);
            if (speed <= 120.f || (!pm->cmd.forwardmove && !pm->cmd.rightmove) ||
                (pm->ps->pm_flags & PMF_DUCKED) || (pm->ps->eFlags & EF_PRONE) ||
                (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4))
                pm->pmext->sprintTime = (int)(pm->pmext->sprintTime + recharge*pml.frametime);
            if (pm->pmext->sprintTime > 5000)
                pm->pmext->sprintTime = (int)(pm->pmext->sprintTime + recharge*pml.frametime);
        }
        if (pm->pmext->sprintTime > 20000) pm->pmext->sprintTime = 20000;
        pm->ps->sprintExertTime = 0;
        return;
    }
    if (pm->pmext->sprintTime < 0) pm->pmext->sprintTime = 0;
    if (!pm->ps->sprintExertTime) pm->ps->sprintExertTime = 1;
}
#endif

/*
================
PmoveSingle

================
*/
void trap_SnapVector( float *v );

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
/* Native movement layout and protocol bounds retained after C reconstruction. */
typedef char single833Layouts[(sizeof(pml_t)==156 && offsetof(pmove_t,ps)==0 && sizeof(tce_weaponDef_t)==460 && offsetof(tce_weaponDef_t,noTacMode)==0x194) ? 1 : -1];
typedef char single833Protocol[(WEAPON_READY==0 && WEAPON_FIRING==7 && BUTTON_ATTACK==1 && BUTTON_TALK==2 && BUTTON_WALKING==16 && BUTTON_SPRINT==32 && PMF_LIMBO==0x4000 && PMF_TIME_LOCKPLAYER==0x8000 && PM_NORMAL==0 && PM_NOCLIP==1 && PM_SPECTATOR==2 && PM_DEAD==3 && PM_FREEZE==4 && PM_INTERMISSION==5 && PMF_RESPAWNED==0x200 && EF_TALK==0x200 && EF_FIRING==0x80 && EF_ZOOMING==0x40000 && EF_PRONE==0x80000 && EF_PRONE_MOVING==0x100000 && EF_MOUNTEDTANK==0x8000 && ANIM_MT_IDLE==1) ? 1 : -1];
#endif
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
/* TCE single movement step. Original helper order and fresh callback-state reads
 * are retained; compiled callback ABI and FPU behavior remain separately open. */
void PmoveSingle(pmove_t *pmove) {
    BG_AnimUpdatePlayerStateConditions(pmove);
    c_pmove = PM_ProneTimeValue((unsigned)c_pmove + 1u);
    pm = pmove;
    pm->numtouch = 0;
    pm->watertype = 0;
    pm->waterlevel = 0;
    if (pm->ps->stats[STAT_HEALTH] <= 0) {
        pm->tracemask &= ~CONTENTS_BODY;
        pm->ps->eFlags &= ~EF_ZOOMING;
    }
    if (abs(pm->cmd.forwardmove) > 64 || abs(pm->cmd.rightmove) > 64)
        pm->cmd.buttons &= ~BUTTON_WALKING;
    if (pm->cmd.buttons & BUTTON_TALK) pm->ps->eFlags |= EF_TALK;
    else pm->ps->eFlags &= ~EF_TALK;
    pm->ps->eFlags &= ~(EF_FIRING | EF_ZOOMING);
    if (!(pm->ps->pm_flags & PMF_RESPAWNED) && pm->ps->pm_type != PM_INTERMISSION &&
        PM_WeaponAmmoAvailable(pm->ps->weapon)) {
        if (!(pm->ps->eFlags & EF_ZOOMING) &&
            (TCE_FloatIsZeroOrNaN(pm->ps->leanf) || (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4)) &&
            (pm->ps->weaponstate == WEAPON_READY || pm->ps->weaponstate == WEAPON_FIRING) &&
            (pm->cmd.buttons & BUTTON_ATTACK) && !(pm->cmd.buttons & BUTTON_TALK))
            pm->ps->eFlags |= EF_FIRING;
    }
    if ((pm->ps->pm_flags & PMF_RESPAWNED) && pm->ps->stats[STAT_PLAYER_CLASS] == PC_COVERTOPS)
        pm->pmext->silencedSideArm |= 1;
    if (pm->ps->stats[STAT_HEALTH] > 0 && !(pm->cmd.buttons & BUTTON_ATTACK))
        pm->ps->pm_flags &= ~PMF_RESPAWNED;
    if (pmove->cmd.buttons & BUTTON_TALK) {
        pmove->cmd.buttons = BUTTON_TALK;
        pmove->cmd.wbuttons = 0;
        pmove->cmd.forwardmove = 0;
        pmove->cmd.rightmove = 0;
        pmove->cmd.upmove = 0;
        pmove->cmd.doubleTap = 0;
    }
    if (pm->ps->persistant[PERS_HWEAPON_USE]) {
        pmove->cmd.forwardmove = 0;
        pmove->cmd.rightmove = 0;
        pmove->cmd.upmove = 0;
    }
    memset(&pml, 0, sizeof(pml));
    pml.msec = PM_ProneTimeValue((unsigned)pmove->cmd.serverTime - (unsigned)pm->ps->commandTime);
    if (pml.msec < 1) pml.msec = 1;
    else if (pml.msec > 200) pml.msec = 200;
    pm->ps->commandTime = pmove->cmd.serverTime;
    VectorCopy(pm->ps->origin, pml.previous_origin);
    VectorCopy(pm->ps->velocity, pml.previous_velocity);
    pml.frametime = (float)((long double)pml.msec * 0.001);
    if (pm->ps->pm_type != PM_FREEZE && !(pm->ps->pm_flags & PMF_LIMBO))
        PM_UpdateViewAngles(pm->ps, pm->pmext, &pm->cmd, pm->trace, pm->tracemask);
    AngleVectors(pm->ps->viewangles, pml.forward, pml.right, pml.up);
    if (pm->cmd.upmove < 10) pm->ps->pm_flags &= ~PMF_JUMP_HELD;
    if (pm->cmd.forwardmove < 0) pm->ps->pm_flags |= PMF_BACKWARDS_RUN;
    else if (pm->cmd.forwardmove > 0 || (!pm->cmd.forwardmove && pm->cmd.rightmove))
        pm->ps->pm_flags &= ~PMF_BACKWARDS_RUN;
    if (pm->ps->pm_type >= PM_DEAD || (pm->ps->pm_flags & (PMF_LIMBO | PMF_TIME_LOCKPLAYER))) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
    }
    if (pm->ps->pm_type == PM_SPECTATOR) {
        PM_CheckDuck();
        PM_FlyMove();
        PM_DropTimers();
        return;
    }
    if (pm->ps->pm_type == PM_NOCLIP) {
        PM_NoclipMove();
        PM_DropTimers();
        return;
    }
    if (pm->ps->pm_type == PM_FREEZE) return;
    if (pm->ps->stats[STAT_TCE_FLAGS] & TCE_STAT_WARMUP_LOCK) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
        pm->cmd.buttons = 0;
        pm->cmd.wbuttons = 0;
        pm->cmd.weapon = pm->ps->weapon;
        pm->ps->leanf = 0;
        pm->cmd.doubleTap = 0;
    }
    if (pm->ps->pm_type == PM_INTERMISSION) return;
    if (pm->ps->weapon == 60 && pm->ps->pm_type == PM_NORMAL) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
    }
    PM_SetWaterLevel();
    pml.previous_waterlevel = pmove->waterlevel;
    if (!PM_CheckProne()) PM_CheckDuck();
    PM_GroundTrace();
    if (pm->ps->pm_type == PM_DEAD) {
        PM_DeadMove();
        if (pm->ps->weapon == 60) pm->ps->weapon = WP_MORTAR;
    } else {
        if (pm->ps->weapon == 62 && !(pm->ps->eFlags & EF_PRONE)) {
            PM_BeginWeaponChange(62, WP_MOBILE_MG42, qfalse);
#ifdef CGAMEDLL
            cg.weaponSelect = WP_MOBILE_MG42;
#endif
        }
        if (pm->ps->weapon == WP_SATCHEL_DET && !pm->ps->ammoclip[WP_SATCHEL_DET]) {
            PM_BeginWeaponChange(WP_SATCHEL_DET, WP_SATCHEL, qtrue);
#ifdef CGAMEDLL
            cg.weaponSelect = WP_SATCHEL;
#endif
        }
    }
    PM_CheckLadderMove();
    PM_DropTimers();
    if (pml.ladder) PM_LadderMove();
    else if (pm->ps->pm_flags & PMF_TIME_WATERJUMP) PM_WaterJumpMove();
    else if (pm->waterlevel > 1) PM_WaterMove();
    else if (pml.walking && !(pm->ps->eFlags & EF_MOUNTEDTANK)) PM_WalkMove();
    else if (!(pm->ps->eFlags & EF_MOUNTEDTANK)) PM_AirMove();
    if (pm->ps->eFlags & EF_MOUNTEDTANK) {
        pm->ps->velocity[2] = 0;
        pm->ps->velocity[1] = 0;
        pm->ps->velocity[0] = 0;
        pm->ps->viewheight = 42;
        BG_AnimScriptAnimation(pm->ps, pm->character->animModelInfo, ANIM_MT_IDLE, qtrue);
    }
    PM_Sprint();
    PM_GroundTrace();
    PM_SetWaterLevel();
    PM_Weapon();
    if (PM_TCECancelAim()) return;
    PM_Footsteps();
    PM_WaterEvents();
    trap_SnapVector(pm->ps->velocity);
}
#else
/* Portable/client fallback; no Windows instruction parity claim for this branch. */
void PmoveSingle (pmove_t *pmove) {
	// RF, update conditional values for anim system
	BG_AnimUpdatePlayerStateConditions( pmove );

	pm = pmove;

	// this counter lets us debug movement problems with a journal
	// by setting a conditional breakpoint fot the previous frame
	c_pmove++;

	// clear results
	pm->numtouch = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;

	if ( pm->ps->stats[STAT_HEALTH] <= 0 ) {
		pm->tracemask &= ~CONTENTS_BODY;	// corpses can fly through bodies
		pm->ps->eFlags &= ~EF_ZOOMING;
	}

	// make sure walking button is clear if they are running, to avoid
	// proxy no-footsteps cheats
	if ( abs( pm->cmd.forwardmove ) > 64 || abs( pm->cmd.rightmove ) > 64 ) {
		pm->cmd.buttons &= ~BUTTON_WALKING;
	}

	// set the talk balloon flag
	if ( pm->cmd.buttons & BUTTON_TALK ) {
		pm->ps->eFlags |= EF_TALK;
	} else {
		pm->ps->eFlags &= ~EF_TALK;
	}

	// set the firing flag for continuous beam weapons

	pm->ps->eFlags &= ~(EF_FIRING | EF_ZOOMING);


	if ( !(pm->ps->pm_flags & PMF_RESPAWNED) &&
		  (pm->ps->pm_type != PM_INTERMISSION) ) {

		// check for ammo
		if(PM_WeaponAmmoAvailable(pm->ps->weapon)) {
			// check if zooming
			// DHM - Nerve :: Let's use the same flag we just checked above, Ok?
			if(!(pm->ps->eFlags & EF_ZOOMING)) {
				if(!pm->ps->leanf || (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4)) {
					if ( pm->ps->weaponstate == WEAPON_READY || pm->ps->weaponstate == WEAPON_FIRING ) {

						// all clear, fire!
						if( pm->cmd.buttons & BUTTON_ATTACK && !(pm->cmd.buttons & BUTTON_TALK) )
							pm->ps->eFlags |= EF_FIRING;
					}
				}
			}
		}
	}


	if ( pm->ps->pm_flags & PMF_RESPAWNED ) {
		if( pm->ps->stats[STAT_PLAYER_CLASS] == PC_COVERTOPS ) {
			pm->pmext->silencedSideArm |= 1;
		}
	}

	// clear the respawned flag if attack and use are cleared
	if ( pm->ps->stats[STAT_HEALTH] > 0 && 
		!( pm->cmd.buttons & (BUTTON_ATTACK /*| BUTTON_USE_HOLDABLE*/) ) ) {
		pm->ps->pm_flags &= ~PMF_RESPAWNED;
	}

	// if talk button is down, dissallow all other input
	// this is to prevent any possible intercept proxy from
	// adding fake talk balloons
	if ( pmove->cmd.buttons & BUTTON_TALK ) {
		// keep the talk button set tho for when the cmd.serverTime > 66 msec
		// and the same cmd is used multiple times in Pmove
		pmove->cmd.buttons = BUTTON_TALK;
		pmove->cmd.wbuttons = 0;
		pmove->cmd.forwardmove = 0;
		pmove->cmd.rightmove = 0;
		pmove->cmd.upmove = 0;
		pmove->cmd.doubleTap = 0;
	}

	// fretn, moved from engine to this very place
	// no movement while using a static mg42
	if (pm->ps->persistant[PERS_HWEAPON_USE])
	{
		pmove->cmd.forwardmove = 0;
		pmove->cmd.rightmove = 0;
		pmove->cmd.upmove = 0;
	}

	// clear all pmove local vars
	memset (&pml, 0, sizeof(pml));

	// determine the time
	pml.msec = pmove->cmd.serverTime - pm->ps->commandTime;
	if ( pml.msec < 1 ) {
		pml.msec = 1;
	} else if ( pml.msec > 200 ) {
		pml.msec = 200;
	}
	pm->ps->commandTime = pmove->cmd.serverTime;

	// save old org in case we get stuck
	VectorCopy (pm->ps->origin, pml.previous_origin);

	// save old velocity for crashlanding
	VectorCopy (pm->ps->velocity, pml.previous_velocity);

	pml.frametime = pml.msec * 0.001;

	// update the viewangles
	if( pm->ps->pm_type != PM_FREEZE )	// Arnout: added PM_FREEZE
		if (!(pm->ps->pm_flags & PMF_LIMBO)) // JPW NERVE
			// rain - added tracemask
			PM_UpdateViewAngles( pm->ps, pm->pmext, &pm->cmd, pm->trace, pm->tracemask );	//----(SA)	modified

	AngleVectors (pm->ps->viewangles, pml.forward, pml.right, pml.up);

	if ( pm->cmd.upmove < 10 ) {
		// not holding jump
		pm->ps->pm_flags &= ~PMF_JUMP_HELD;
	}

	// decide if backpedaling animations should be used
	if ( pm->cmd.forwardmove < 0 ) {
		pm->ps->pm_flags |= PMF_BACKWARDS_RUN;
	} else if ( pm->cmd.forwardmove > 0 || ( pm->cmd.forwardmove == 0 && pm->cmd.rightmove ) ) {
		pm->ps->pm_flags &= ~PMF_BACKWARDS_RUN;
	}

	if ( pm->ps->pm_type >= PM_DEAD || pm->ps->pm_flags & (PMF_LIMBO|PMF_TIME_LOCKPLAYER) ) {			// DHM - Nerve
		pm->cmd.forwardmove = 0;
		pm->cmd.rightmove = 0;
		pm->cmd.upmove = 0;
	}

	if ( pm->ps->pm_type == PM_SPECTATOR ) {
		PM_CheckDuck ();
		PM_FlyMove ();
		PM_DropTimers ();
		return;
	}

	if ( pm->ps->pm_type == PM_NOCLIP ) {
		PM_NoclipMove ();
		PM_DropTimers ();
		return;
	}

	if (pm->ps->pm_type == PM_FREEZE) {
		return;		// no movement at all
	}

	/* TC:E PmoveSingle 3000c180: keep view control and gravity, suppress input. */
	if (pm->ps->stats[STAT_TCE_FLAGS] & TCE_STAT_WARMUP_LOCK) {
		pm->cmd.forwardmove = pm->cmd.rightmove = pm->cmd.upmove = 0;
		pm->cmd.buttons = pm->cmd.wbuttons = 0;
		pm->cmd.weapon = pm->ps->weapon;
		pm->ps->leanf = 0;
		pm->cmd.doubleTap = 0;
	}

	if ( pm->ps->pm_type == PM_INTERMISSION ) {
		return;		// no movement at all
	}

	// ydnar: need gravity etc to affect a player with a set mortar
	if( pm->ps->weapon == 60 && pm->ps->pm_type == PM_NORMAL ) {
		pm->cmd.forwardmove = 0;
		pm->cmd.rightmove = 0;
		pm->cmd.upmove = 0;
		
		#if 0
			VectorClear(pm->ps->velocity);
			
			// set watertype, and waterlevel
			PM_SetWaterLevel();
			pml.previous_waterlevel = pmove->waterlevel;

			// set mins, maxs, and viewheight
			PM_CheckDuck ();

			// set groundentity
			PM_GroundTrace();
			
			// handle movement (gravity)
			{
				vec3_t	endVelocity;
				VectorCopy( pm->ps->velocity, endVelocity );
				endVelocity[2] -= pm->ps->gravity * pml.frametime;
				pm->ps->velocity[2] = ( pm->ps->velocity[2] + endVelocity[2] ) * 0.5;
				if ( pml.groundPlane )
					PM_ClipVelocity( pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity, OVERCLIP );
			}

			PM_Weapon();

			BG_AnimScriptAnimation( pm->ps, pm->character->animModelInfo, ANIM_MT_IDLE, qtrue );

			return;
		#endif
	}

	// set watertype, and waterlevel
	PM_SetWaterLevel();
	pml.previous_waterlevel = pmove->waterlevel;

	// set mins, maxs, and viewheight
	//if( !PM_CheckProne() ) {
//		PM_CheckDuck ();
	//}
	if( !PM_CheckProne() ) {
		PM_CheckDuck();
	}

	// set groundentity
	PM_GroundTrace();

	/*if( pm->ps->eFlags & EF_PRONE && !pml.walking ) {
	// this is the one we were using
		// can't be prone in midair
		pm->ps->eFlags &= ~EF_PRONE;
		pm->ps->eFlags &= ~EF_PRONE_MOVING;
		pm->pmext->proneTime = -pm->cmd.serverTime;	// timestamp 'stop prone'

		if( pm->ps->weapon == WP_MOBILE_MG42_SET ) {
			PM_BeginWeaponChange( WP_MOBILE_MG42_SET, WP_MOBILE_MG42, qfalse );
		}
	}*/

	if ( pm->ps->pm_type == PM_DEAD ) {
		PM_DeadMove ();

		/* Original TC PmoveSingle: mounted mortar is 60; SDK45 is TC M16. */
		if( pm->ps->weapon == 60 ) {
			pm->ps->weapon = WP_MORTAR;
		}
	} else {
		/* SDK49 is a TC shotgun, not the deployed MG (62). */
		if( pm->ps->weapon == 62 ) {
			if( !( pm->ps->eFlags & EF_PRONE ) ) {
				PM_BeginWeaponChange( pm->ps->weapon, WP_MOBILE_MG42, qfalse );
#ifdef CGAMEDLL
				cg.weaponSelect = WP_MOBILE_MG42;
#endif // CGAMEDLL
			}
		}
		if( pm->ps->weapon == WP_SATCHEL_DET ) {
			if( !( pm->ps->ammoclip[ WP_SATCHEL_DET ] ) ) {
				PM_BeginWeaponChange( WP_SATCHEL_DET, WP_SATCHEL, qtrue );
#ifdef CGAMEDLL
				cg.weaponSelect = WP_SATCHEL;
#endif // CGAMEDLL
			}
		}
	}

	// Ridah, ladders
	PM_CheckLadderMove();

	PM_DropTimers();

	if (pml.ladder) {
		PM_LadderMove();
	} else if (pm->ps->pm_flags & PMF_TIME_WATERJUMP) {
		PM_WaterJumpMove();
	} else if ( pm->waterlevel > 1 ) {
		// swimming
		PM_WaterMove();
	} else if ( pml.walking && !(pm->ps->eFlags & EF_MOUNTEDTANK)) {
		// walking on ground
		PM_WalkMove();
	} else if (!(pm->ps->eFlags & EF_MOUNTEDTANK)) {
		// airborne
		PM_AirMove();
	}

	if( pm->ps->eFlags & EF_MOUNTEDTANK ) {
		VectorClear( pm->ps->velocity );

		pm->ps->viewheight = 42;

		BG_AnimScriptAnimation( pm->ps, pm->character->animModelInfo, ANIM_MT_IDLE, qtrue );
	}


	/*if( pm->ps->eFlags & EF_PRONE && !pml.walking ) {
		// can't be prone in midair
		pm->ps->eFlags &= ~EF_PRONE;
		pm->ps->eFlags &= ~EF_PRONE_MOVING;
		pm->pmext->proneTime = -pm->cmd.serverTime;	// timestamp 'stop prone'

		if( pm->ps->weapon == WP_MOBILE_MG42_SET ) {
			PM_BeginWeaponChange( WP_MOBILE_MG42_SET, WP_MOBILE_MG42, qfalse );
		}
	}*/
	
	PM_Sprint();

	// set groundentity, watertype, and waterlevel
	PM_GroundTrace();
	PM_SetWaterLevel();

	// weapons
	PM_Weapon();
	if (PM_TCECancelAim()) return;

	// footstep events / legs animations
	PM_Footsteps();

	// entering / leaving water splashes
	PM_WaterEvents();

	// snap some parts of playerstate to save network bandwidth
	trap_SnapVector( pm->ps->velocity );
//	SnapVector( pm->ps->velocity );
}
#endif


/*
================
Pmove

Can be called by either the server or the client
================
*/
/* TC:E retains SDK substep scheduling but always returns zero.
 * Explicit unsigned arithmetic preserves the original 32-bit time wrapping. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || (defined(__linux__) && !defined(Q3_VM))
int Pmove(pmove_t *pmove) {
    int finalTime = pmove->cmd.serverTime;
    int msec;

    if (finalTime < pmove->ps->commandTime) return 0;
    if (finalTime > PM_ProneTimeValue((unsigned)pmove->ps->commandTime + 1000u))
        pmove->ps->commandTime = PM_ProneTimeValue((unsigned)finalTime - 1000u);
    if ((pmove->ps->pm_flags & PMF_TIME_LOAD) &&
        PM_ProneTimeValue((unsigned)finalTime - (unsigned)pmove->ps->commandTime) > 50)
        pmove->ps->commandTime = PM_ProneTimeValue((unsigned)finalTime - 50u);

    pmove->ps->pmove_framecount = ((unsigned)pmove->ps->pmove_framecount + 1u) &
        ((1u << PS_PMOVEFRAMECOUNTBITS) - 1u);
    pm = pmove;
    PM_AdjustAimSpreadScale();
    while (pmove->ps->commandTime != finalTime) {
        msec = PM_ProneTimeValue((unsigned)finalTime - (unsigned)pmove->ps->commandTime);
        if (pmove->pmove_fixed) {
            if (msec > pmove->pmove_msec) msec = pmove->pmove_msec;
        } else if (msec > 50) msec = 50;
        pmove->cmd.serverTime = PM_ProneTimeValue((unsigned)pmove->ps->commandTime + (unsigned)msec);
        PmoveSingle(pmove);
        if (pmove->ps->pm_flags & PMF_JUMP_HELD) pmove->cmd.upmove = 20;
    }
    if (pmove->ps->curWeapHeat > 255) pmove->ps->curWeapHeat = 255;
    else if (pmove->ps->curWeapHeat < 0) pmove->ps->curWeapHeat = 0;
    return 0;
}
#else
int Pmove (pmove_t *pmove) {
	int			finalTime;

	// Ridah
/*	if (pmove->ps->eFlags & EF_DUMMY_PMOVE) {
		PmoveSingle( pmove );
		return (0);
	}*/
	// done.

	finalTime = pmove->cmd.serverTime;

	if ( finalTime < pmove->ps->commandTime ) {
		return (0);	// should not happen
	}

	if ( finalTime > pmove->ps->commandTime + 1000 ) {
		pmove->ps->commandTime = finalTime - 1000;
	}

	// after a loadgame, prevent huge pmove's
	if ( pmove->ps->pm_flags & PMF_TIME_LOAD ) {
		if ( finalTime - pmove->ps->commandTime > 50 ) {
			pmove->ps->commandTime = finalTime - 50;
		}
	}

	pmove->ps->pmove_framecount = (pmove->ps->pmove_framecount+1) & ((1<<PS_PMOVEFRAMECOUNTBITS)-1);

	// RF
	pm = pmove;
	PM_AdjustAimSpreadScale ();

//	startedTorsoAnim = -1;
//	startedLegAnim = -1;

	// chop the move up if it is too long, to prevent framerate
	// dependent behavior
	while ( pmove->ps->commandTime != finalTime ) {
		int		msec;

		msec = finalTime - pmove->ps->commandTime;

		if ( pmove->pmove_fixed ) {
			if ( msec > pmove->pmove_msec ) {
				msec = pmove->pmove_msec;
			}
		}
		else {
			// rain - this was 66 (15fps), but I've changed it to
			// 50 (20fps, max rate of mg42) to alleviate some of the
			// framerate dependency with the mg42.
			// in reality, this should be split according to sv_fps,
			// and pmove() shouldn't handle weapon firing
			if ( msec > 50 ) {
				msec = 50;
			}
		}
		pmove->cmd.serverTime = pmove->ps->commandTime + msec;
		PmoveSingle( pmove );

		if ( pmove->ps->pm_flags & PMF_JUMP_HELD ) {
			pmove->cmd.upmove = 20;
		}
	}

	// rain - sanity check weapon heat
	if (pmove->ps->curWeapHeat > 255)
		pmove->ps->curWeapHeat = 255;
	else if (pmove->ps->curWeapHeat < 0)
		pmove->ps->curWeapHeat = 0;

    /* Original30013060 always returns zero, including corpse movement. */
    return 0;
}
#endif
