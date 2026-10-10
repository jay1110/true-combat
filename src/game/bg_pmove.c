

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
#include "tce_weapon_ammo.h"
#include "tce_reload.h"
#include "tce_movement.h"
#include "tce_attack.h"
#include "tce_aim.h"

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

int PM_ReloadAnimForWeapon(int weapon) {
    return TCE_PM_ReloadAnimForWeapon(weapon, pm->skill[SK_LIGHT_WEAPONS]);
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
static const float tceClipZero818 = 0.0f;
/* TC20030d90: retained Z+Y+X dot product and sequential alias-safe stores. */
__declspec(naked) void PM_ClipVelocity( vec3_t in, vec3_t normal, vec3_t clipOutput, float overbounce ) {
    __asm {
        mov edx, dword ptr [esp + 4]
        mov ecx, dword ptr [esp + 8]
        fld dword ptr [edx + 8]
        fmul dword ptr [ecx + 8]
        fld dword ptr [edx + 4]
        fmul dword ptr [ecx + 4]
        faddp st(1), st(0)
        fld dword ptr [edx]
        fmul dword ptr [ecx]
        faddp st(1), st(0)
        fcom dword ptr tceClipZero818
        fnstsw ax
        test ah, 1
        jz clip818_divide
        fmul dword ptr [esp + 16]
        jmp clip818_components
    clip818_divide:
        fdiv dword ptr [esp + 16]
    clip818_components:
        push esi
        mov esi, dword ptr [esp + 16]
        sub edx, ecx
        mov eax, ecx
        sub esi, ecx
        mov ecx, 3
    clip818_loop:
        fld st(0)
        fmul dword ptr [eax]
        add eax, 4
        dec ecx
        fsubr dword ptr [edx + eax - 4]
        fstp dword ptr [esi + eax - 4]
        jnz clip818_loop
        fstp st(0)
        pop esi
        ret
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
        const float tracePi = 3.1415927410125732421875f;
        const float traceRadians = 0.0055555556900799274444580078125f;
        const float traceBack = -32.0f;
        float traceCos, traceSin;
        float *traceYaw = &viewangles[YAW];
        float *traceOfs = ofs;
        /* TC20030edd..f33: retained angle, hardware trig, f32 trig outputs. */
        __asm {
            fld sizeScale
            mov ecx, traceYaw
            fld dword ptr [ecx]
            fmul tracePi
            fmul traceRadians
            fld st(0)
            fcos
            fstp traceCos
            fsin
            fstp traceSin
            fmul traceBack
            fld st(0)
            fmul traceCos
            mov ecx, traceOfs
            fstp dword ptr [ecx]
            fmul traceSin
            fstp dword ptr [ecx + 4]
            mov dword ptr [ecx + 8], 0
        }
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
        float *traceFraction = &trace->fraction;
        float *bodyFraction = &bodytrace->fraction;
        __asm {
            mov ecx, traceFraction
            mov edx, bodyFraction
            fld dword ptr [ecx]
            fcomp dword ptr [edx]
            fnstsw ax
            and eax, 100h
            mov tryStep, eax
        }
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
            float *stepFraction = &steptrace.fraction;
            float *traceFraction = &trace->fraction;
            __asm {
                mov ecx, stepFraction
                mov edx, traceFraction
                fld dword ptr [ecx]
                fcomp dword ptr [edx]
                fnstsw ax
                test ah, 41h
                sete al
                movzx eax, al
                mov tryStep, eax
            }
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
                    float *traceOrgZ = &org[2];
                    float *traceEndZ = &steptrace.endpos[2];
                    float *traceOfsZ = &ofs[2];
                    __asm {
                        mov ecx, traceOrgZ
                        mov edx, traceEndZ
                        fld dword ptr [ecx]
                        fsub dword ptr [edx]
                        mov ecx, traceOfsZ
                        fsubr dword ptr [ecx]
                        mov edx, legsOffset
                        fstp dword ptr [edx]
                    }
#else
					*legsOffset = ofs[2] - (org[2] - steptrace.endpos[2]);
#endif
				}
			}
		}
	}
}

/* Traces all player bboxes -- body and legs */
void	PM_TraceAllLegs( trace_t *trace, float *legsOffset, vec3_t start, vec3_t end )
{
	pm->trace(trace, start, pm->mins, pm->maxs, end, pm->ps->clientNum, pm->tracemask);
	
	/* legs */
	if ( pm->ps->eFlags & EF_PRONE ) {
		trace_t legtrace;
		int nearerLeg;

		PM_TraceLegs( &legtrace, legsOffset, start, end, trace, pm->ps->viewangles, pm->trace, pm->ps->clientNum, pm->tracemask );

#if defined(_MSC_VER) && defined(_M_IX86)
		/* TC2003115f: C0 includes an unordered fraction comparison. */
		{
			float *legFraction = &legtrace.fraction;
			float *bodyFraction = &trace->fraction;
			__asm {
				mov ecx, legFraction
				mov edx, bodyFraction
				fld dword ptr [ecx]
				fcomp dword ptr [edx]
				fnstsw ax
				and eax, 100h
				mov nearerLeg, eax
			}
		}
#else
		nearerLeg = legtrace.fraction < trace->fraction;
#endif
		if (nearerLeg ||
		    legtrace.startsolid ||
		    legtrace.allsolid) {
#if defined(_MSC_VER) && defined(_M_IX86)
			/* TC20031180: retain X; round Y/Z differences before scaling. */
			float *legEnd = legtrace.endpos;
			float *legFraction = &legtrace.fraction;
			__asm {
				mov ecx, end
				mov edx, start
				mov eax, legEnd
				fld dword ptr [ecx]
				fsub dword ptr [edx]
				fld dword ptr [ecx + 4]
				fsub dword ptr [edx + 4]
				fstp dword ptr [eax + 4]
				fld dword ptr [ecx + 8]
				fsub dword ptr [edx + 8]
				fstp dword ptr [eax + 8]
				mov ecx, legFraction
				fmul dword ptr [ecx]
				fadd dword ptr [edx]
				fstp dword ptr [eax]
				fld dword ptr [eax + 4]
				fmul dword ptr [ecx]
				fadd dword ptr [edx + 4]
				fstp dword ptr [eax + 4]
				fld dword ptr [eax + 8]
				fmul dword ptr [ecx]
				fadd dword ptr [edx + 8]
				fstp dword ptr [eax + 8]
			}
#else
			VectorSubtract( end, start, legtrace.endpos );
			VectorMA( start, legtrace.fraction, legtrace.endpos, legtrace.endpos );
#endif
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
/* TC20033cb0 x87 schedule; original saved-Z state contract remains open. */
static const unsigned int frictionK200ac100 = 0x0u;
static const unsigned int frictionK200ac110 = 0x3f800000u;
static const unsigned int frictionK200ac180 = 0x3f000000u;
static const unsigned int frictionK200ac19c = 0x41a00000u;
static const unsigned int frictionK200ac30c = 0x40400000u;
static const float frictionMounted = 2.0f;
enum {
    frVel = offsetof(playerState_t,velocity),
    frType = offsetof(playerState_t,pm_type),
    frFlags = offsetof(playerState_t,pm_flags),
    frEFlags = offsetof(playerState_t,eFlags),
    frExt = offsetof(pmove_t,pmext),
    frTime = offsetof(pmove_t,cmd)+offsetof(usercmd_t,serverTime),
    frDodge = offsetof(pmoveExt_t,dodgeTime),
    frWater = offsetof(pmove_t,waterlevel),
    frWaterType = offsetof(pmove_t,watertype),
    frWalk = offsetof(pml_t,walking),
    frGround = offsetof(pml_t,tceGroundExtension),
    frFrame = offsetof(pml_t,frametime),
    frSurface = offsetof(pml_t,groundTrace)+offsetof(trace_t,surfaceFlags),
    frLadder = offsetof(pml_t,ladder)
};
static __declspec(naked) void PM_Friction(void) {
    __asm {
        sub esp, 018h
        mov eax, dword ptr [pm]
        push esi
        mov dword ptr [esp + 0ch], 0 /* Defined fallback for original uninitialized saved Z. */
        mov esi, dword ptr [eax]
        add esi, frVel
        mov ecx, dword ptr [esi]
        mov dword ptr [esp + 010h], ecx
        mov edx, dword ptr [esi + 4]
        mov dword ptr [esp + 014h], edx
        mov eax, dword ptr [esi + 8]
        mov dword ptr [esp + 018h], eax
        mov eax, dword ptr [pml + frWalk]
        test eax, eax
        je frL20033cf4
        mov eax, dword ptr [pml + frGround]
        test eax, eax
        je frL20033cec
        mov ecx, dword ptr [esp + 018h]
        mov dword ptr [esp + 0ch], ecx
    frL20033cec:
        mov dword ptr [esp + 018h], 0
    frL20033cf4:
        lea edx, [esp + 010h]
        push edx
        call VectorLength
        fst dword ptr [esp + 8]
        fcomp dword ptr [frictionK200ac110]
        mov ecx, dword ptr [pm]
        add esp, 4
        fnstsw ax
        test ah, 1
        je frL20033d39
        mov eax, dword ptr [ecx]
        mov eax, dword ptr [eax + frType]
        cmp eax, 2
        je frL20033d39
        cmp eax, 1
        je frL20033d39
        mov dword ptr [esi], 0
        mov dword ptr [esi + 4], 0
        pop esi
        add esp, 018h
        ret 
    frL20033d39:
        mov edx, dword ptr [ecx + frExt]
        mov eax, dword ptr [ecx + frTime]
        fld dword ptr [frictionK200ac100]
        push edi
        mov edi, dword ptr [edx + frDodge]
        sub eax, edi
        pop edi
        cmp eax, 015eh
        jge frL20033d6c
        cmp eax, 0fah
        jle frL20033d6c
        fstp st(0)
        fld dword ptr [pml + frFrame]
        fmul dword ptr [esp + 4]
        fmul dword ptr [frictionK200ac19c]
    frL20033d6c:
        mov edx, dword ptr [ecx + frWater]
        cmp edx, 1
        mov dword ptr [esp + 8], edx
        jg frL20033dc6
        mov eax, dword ptr [pml + frWalk]
        test eax, eax
        je frL20033dc6
        test byte ptr [pml + frSurface], 2
        jne frL20033dc6
        mov eax, dword ptr [ecx]
        test byte ptr [eax + frFlags], 040h
        jne frL20033dc6
        fld dword ptr [pm_stopspeed]
        fmul dword ptr [frictionK200ac180]
        fld dword ptr [esp + 4]
        fcomp st(1)
        fnstsw ax
        test ah, 1
        jne frL20033db4
        fstp st(0)
        fld dword ptr [esp + 4]
    frL20033db4:
        fld dword ptr [pm_friction]
        fmul dword ptr [pml + frFrame]
        fmul st(0), st(1)
        faddp st(2), st(0)
        fstp st(0)
    frL20033dc6:
        test edx, edx
        je frL20033df3
        mov eax, dword ptr [ecx + frWaterType]
        fild dword ptr [esp + 8]
        cmp eax, 010h
        jne frL20033de1
        fmul dword ptr [pm_slagfriction]
        jmp frL20033de7
    frL20033de1:
        fmul dword ptr [pm_waterfriction]
    frL20033de7:
        fmul dword ptr [pml + frFrame]
        fmul dword ptr [esp + 4]
        faddp st(1), st(0)
    frL20033df3:
        mov ecx, dword ptr [ecx]
        mov edx, dword ptr [ecx + frType]
        cmp edx, 2
        jne frL20033e0f
        fld dword ptr [pm_spectatorfriction]
        fmul dword ptr [pml + frFrame]
        fmul dword ptr [esp + 4]
        faddp st(1), st(0)
    frL20033e0f:
        test dword ptr [ecx + frEFlags], 01000000h
        je frL20033e2a
        fld dword ptr [frictionMounted]
        fmul dword ptr [pml + frFrame]
        fmul dword ptr [esp + 4]
        faddp st(1), st(0)
    frL20033e2a:
        mov eax, dword ptr [pml + frLadder]
        test eax, eax
        je frL20033e45
        fld dword ptr [pm_ladderfriction]
        fmul dword ptr [pml + frFrame]
        fmul dword ptr [esp + 4]
        faddp st(1), st(0)
    frL20033e45:
        fld dword ptr [esp + 4]
        fsub st(0), st(1)
        fcom dword ptr [frictionK200ac100]
        fnstsw ax
        test ah, 1
        je frL20033e60
        fstp st(0)
        fld dword ptr [frictionK200ac100]
    frL20033e60:
        fdiv dword ptr [esp + 4]
        cmp edx, 2
        fstp dword ptr [esp + 8]
        je frL20033e72
        cmp edx, 1
        jne frL20033e9a
    frL20033e72:
        fcomp dword ptr [frictionK200ac110]
        fnstsw ax
        test ah, 1
        je frL20033e9c
        fld dword ptr [esp + 4]
        fcomp dword ptr [frictionK200ac30c]
        fnstsw ax
        test ah, 1
        je frL20033e9c
        mov dword ptr [esp + 8], 0
        jmp frL20033e9c
    frL20033e9a:
        fstp st(0)
    frL20033e9c:
        fld dword ptr [esp + 8]
        fmul dword ptr [esi]
        fstp dword ptr [esi]
        fld dword ptr [esp + 8]
        fmul dword ptr [esi + 4]
        fstp dword ptr [esi + 4]
        fld dword ptr [esp + 8]
        fmul dword ptr [esi + 8]
        fstp dword ptr [esi + 8]
        mov eax, dword ptr [pml + frGround]
        test eax, eax
        je frL20033ed9
        fld dword ptr [esp + 0ch]
        fcomp dword ptr [frictionK200ac100]
        fnstsw ax
        test ah, 1
        je frL20033ed9
        mov ecx, dword ptr [esp + 0ch]
        mov dword ptr [esi + 8], ecx
    frL20033ed9:
        pop esi
        add esp, 018h
        ret 
    }
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
    if (pm->ps->eFlags & 0x01000000) drop += 2.0*pml.frametime*speed;
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
    enum { accelPsOffset = offsetof(pmove_t, ps),
           accelVelocityOffset = offsetof(playerState_t, velocity) };
    vec3_t accelerationPush;
    float accelerationY, accelerationZ, accelerationLength;
    float *accelerationFrame = &pml.frametime;
    /* TC20033ee0: retain X product and acceleration; stage Y/Z and length. */
    __asm {
        mov ecx, wishdir
        fld wishspeed
        fmul dword ptr [ecx]
        fld wishspeed
        fmul dword ptr [ecx + 4]
        fstp accelerationY
        fld wishspeed
        fmul dword ptr [ecx + 8]
        mov eax, pm
        mov ecx, dword ptr [eax + accelPsOffset]
        fstp accelerationZ
        fsub dword ptr [ecx + accelVelocityOffset]
        lea ecx, accelerationPush
        push ecx
        fstp dword ptr accelerationPush
        mov edx, dword ptr [eax + accelPsOffset]
        fld accelerationY
        fsub dword ptr [edx + accelVelocityOffset + 4]
        fstp dword ptr accelerationPush[4]
        mov eax, dword ptr [eax + accelPsOffset]
        fld accelerationZ
        fsub dword ptr [eax + accelVelocityOffset + 8]
        fstp dword ptr accelerationPush[8]
        call VectorNormalize
        fstp accelerationLength
        mov edx, accelerationFrame
        fld dword ptr [edx]
        fmul wishspeed
        add esp, 4
        fmul accel
        fcom accelerationLength
        fnstsw ax
        test ah, 41h
        jnz accel820_selected
        fstp st(0)
        fld accelerationLength
    accel820_selected:
        mov edx, pm
        fld st(0)
        fmul dword ptr accelerationPush
        mov eax, dword ptr [edx + accelPsOffset]
        fadd dword ptr [eax + accelVelocityOffset]
        fstp dword ptr [eax + accelVelocityOffset]
        mov eax, pm
        fld st(0)
        fmul dword ptr accelerationPush[4]
        mov eax, dword ptr [eax + accelPsOffset]
        fadd dword ptr [eax + accelVelocityOffset + 4]
        fstp dword ptr [eax + accelVelocityOffset + 4]
        mov ecx, pm
        fmul dword ptr accelerationPush[8]
        mov eax, dword ptr [ecx + accelPsOffset]
        fadd dword ptr [eax + accelVelocityOffset + 8]
        fstp dword ptr [eax + accelVelocityOffset + 8]
    }
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
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC20033fa0: native offsets, original x87 stack lifetime and return ABI. */
static const unsigned int cmdScaleK200ac100 = 0x0u;
static const unsigned int cmdScaleK200ac270 = 0x40000000u;
static const unsigned int cmdScaleK200ac30c = 0x40400000u;
static const unsigned int cmdScaleK200ac6e8 = 0x3fa00000u;
static const unsigned int cmdScaleK200ac740 = 0x3fc00000u;
static const unsigned int cmdScaleK200ac744 = 0x3c010204u;
static const unsigned int cmdScaleK200ac748 = 0x3f733333u;
static const unsigned int cmdScaleK200ac74c = 0x3f833333u;
static const unsigned int cmdScaleK200ac750 = 0x3f866666u;
static const unsigned int cmdScaleK200ac754 = 0x3f553f7du;
static const unsigned int cmdScaleK200ac758 = 0x3fe8b439u;
static const unsigned int cmdScaleK200ac75c = 0x3f0ccccdu;
static const unsigned int cmdScaleK200ac760 = 0x3f59999au;
static const unsigned int cmdScaleK200ac764 = 0x3fb33333u;
static const unsigned __int64 cmdScaleK200ac768 = 0x405fc00000000000ui64;
enum {
    csCmdForward = offsetof(usercmd_t,forwardmove),
    csCmdRight = offsetof(usercmd_t,rightmove),
    csCmdUp = offsetof(usercmd_t,upmove),
    csCmdButtons = offsetof(usercmd_t,buttons),
    csPmButtons = offsetof(pmove_t,cmd)+offsetof(usercmd_t,buttons),
    csPmExt = offsetof(pmove_t,pmext),
    csPsSpeed = offsetof(playerState_t,speed),
    csPsFlags = offsetof(playerState_t,pm_flags),
    csPsType = offsetof(playerState_t,pm_type),
    csPsRun = offsetof(playerState_t,runSpeedScale),
    csPsSprint = offsetof(playerState_t,sprintSpeedScale),
    csPsCrouch = offsetof(playerState_t,crouchSpeedScale),
    csPsStats = offsetof(playerState_t,stats)+8*sizeof(int),
    csPsLean = offsetof(playerState_t,leanf),
    csPsWeapon = offsetof(playerState_t,weapon),
    csPsWeight = offsetof(playerState_t,holdable)+9*sizeof(int),
    csExtSprint = offsetof(pmoveExt_t,sprintTime),
    csLadder = offsetof(pml_t,ladder),
    csCvarInt = offsetof(vmCvar_t,integer),
    csDefWeight = offsetof(tce_weaponDef_t,loadoutWeight)
};
#ifdef CGAMEDLL
#define CS_MOVE_CVAR cg_movespeed
#define CS_GAME_CVAR cg_gameType
#else
#define CS_MOVE_CVAR g_movespeed
#define CS_GAME_CVAR g_gametype
#endif
static __declspec(naked) float PM_CmdScale(usercmd_t *cmd) {
    __asm {
        sub esp, 014h
        mov eax, dword ptr [CS_MOVE_CVAR + csCvarInt]
        push ebx
        push ebp
        mov ebp, dword ptr [esp + 020h]
        push esi
        mov dword ptr [esp + 0ch], eax
        movsx esi, byte ptr [ebp + csCmdForward]
        mov eax, esi
        push edi
        movsx edi, byte ptr [ebp + csCmdRight]
        cdq 
        mov ecx, eax
        mov eax, edi
        xor ecx, edx
        mov dword ptr [esp + 018h], esi
        sub ecx, edx
        mov dword ptr [esp + 01ch], edi
        cdq 
        xor eax, edx
        mov dword ptr [esp + 028h], ecx
        sub eax, edx
        cmp eax, ecx
        jle csL20033fe2
        mov ecx, eax
        mov dword ptr [esp + 028h], ecx
    csL20033fe2:
        movsx ebx, byte ptr [ebp + csCmdUp]
        mov eax, ebx
        cdq 
        xor eax, edx
        sub eax, edx
        cmp eax, ecx
        jle csL20033ff7
        mov ecx, eax
        mov dword ptr [esp + 028h], ecx
    csL20033ff7:
        test ecx, ecx
        jne csL20034009
        fld dword ptr [cmdScaleK200ac100]
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 014h
        ret 
    csL20034009:
        mov eax, ebx
        mov ecx, edi
        imul eax, ebx
        imul ecx, edi
        mov edx, esi
        mov dword ptr [esp + 020h], eax
        imul edx, esi
        add eax, ecx
        mov ebx, 4
        add edx, eax
        mov eax, dword ptr [pm]
        mov dword ptr [esp + 014h], edx
        fild dword ptr [esp + 014h]
        mov ecx, dword ptr [eax]
        mov dl, byte ptr [eax + csPmButtons]
        test dl, 020h
        fsqrt 
        fstp dword ptr [esp + 014h]
        fild dword ptr [ecx + csPsSpeed]
        fimul dword ptr [esp + 028h]
        fld dword ptr [esp + 014h]
        fmul qword ptr [cmdScaleK200ac768]
        fdivp st(1), st(0)
        je csL2003408d
        mov esi, dword ptr [pml + csLadder]
        test esi, esi
        jne csL2003408d
        mov esi, dword ptr [ecx + csPsFlags]
        and esi, 1
        jne csL2003408d
        mov eax, dword ptr [eax + csPmExt]
        cmp dword ptr [eax + csExtSprint], 032h
        jle csL20034078
        fmul dword ptr [ecx + csPsSprint]
        jmp csL20034084
    csL20034078:
        fmul dword ptr [ecx + csPsRun]
        fmul dword ptr [cmdScaleK200ac764]
    csL20034084:
        test dl, 010h
        je csL20034102
        fadd st(0), st(0)
        jmp csL20034102
    csL2003408d:
        mov esi, dword ptr [ecx + csPsFlags]
        and esi, 1
        je csL200340c6
        fmul dword ptr [ecx + csPsCrouch]
        and dl, 010h
        je csL200340a2
        fadd st(0), st(0)
    csL200340a2:
        test byte ptr [ecx + csPsStats], bl
        jne csL200340be
        fld dword ptr [ecx + csPsLean]
        fcomp dword ptr [cmdScaleK200ac100]
        fnstsw ax
        test ah, 040h
        je csL200340be
        test dl, dl
        je csL20034102
    csL200340be:
        fmul dword ptr [cmdScaleK200ac760]
        jmp csL20034102
    csL200340c6:
        mov al, byte ptr [ecx + csPsStats]
        fmul dword ptr [ecx + csPsRun]
        test bl, al
        jne csL200340e6
        fld dword ptr [ecx + csPsLean]
        fcomp dword ptr [cmdScaleK200ac100]
        fnstsw ax
        test ah, 040h
        jne csL200340f7
    csL200340e6:
        fmul dword ptr [cmdScaleK200ac75c]
        test dl, 010h
        je csL20034102
        fmul dword ptr [cmdScaleK200ac758]
    csL200340f7:
        test dl, 010h
        je csL20034102
        fmul dword ptr [cmdScaleK200ac754]
    csL20034102:
        cmp dword ptr [ecx + csPsType], 1
        jne csL2003410e
        fmul dword ptr [cmdScaleK200ac30c]
    csL2003410e:
        mov edx, dword ptr [ecx + csPsWeapon]
        lea eax, [edx + edx*2]
        shl eax, 3
        sub eax, edx
        lea edx, [eax + eax*4]
        mov eax, dword ptr [edx*4 + weaponDef + csDefWeight]
        sub eax, ebx
        je csL20034152
        dec eax
        je csL2003414c
        mov eax, dword ptr [ecx + csPsWeight]
        cmp eax, ebx
        jge csL2003413f
        fmul dword ptr [cmdScaleK200ac750]
        jmp csL20034152
    csL2003413f:
        cmp eax, 5
        jge csL20034152
        fmul dword ptr [cmdScaleK200ac74c]
        jmp csL20034152
    csL2003414c:
        fmul dword ptr [cmdScaleK200ac748]
    csL20034152:
        mov eax, dword ptr [CS_GAME_CVAR + csCvarInt]
        test eax, eax
        je csL20034160
        cmp eax, 1
        jne csL2003416c
    csL20034160:
        fild dword ptr [esp + 010h]
        fmul dword ptr [cmdScaleK200ac744]
        fmulp st(1), st(0)
    csL2003416c:
        mov dl, byte ptr [ebp + csCmdButtons]
        test dl, 010h
        je csL20034179
        test dl, 020h
        je csL200341ba
    csL20034179:
        test byte ptr [ecx + csPsStats], bl
        jne csL200341ba
        fld dword ptr [ecx + csPsLean]
        fcomp dword ptr [cmdScaleK200ac100]
        fnstsw ax
        test ah, 040h
        je csL200341ba
        test esi, esi
        jne csL200341ba
        test dl, 020h
        je csL200341aa
        fld dword ptr [cmdScaleK200ac270]
        mov dword ptr [esp + 010h], 040200000h
        jmp csL200341c8
    csL200341aa:
        fld dword ptr [cmdScaleK200ac740]
        mov dword ptr [esp + 010h], 03ff00000h
        jmp csL200341c8
    csL200341ba:
        fld dword ptr [cmdScaleK200ac6e8]
        mov dword ptr [esp + 010h], 03fc80000h
    csL200341c8:
        fild dword ptr [esp + 018h]
        fst dword ptr [esp + 028h]
        fcomp dword ptr [cmdScaleK200ac100]
        fnstsw ax
        test ah, 1
        je csL200341e9
        fld dword ptr [esp + 028h]
        fdiv dword ptr [esp + 010h]
        fstp dword ptr [esp + 028h]
    csL200341e9:
        fidivr dword ptr [esp + 01ch]
        pop edi
        pop esi
        pop ebp
        pop ebx
        fld st(0)
        fmulp st(1), st(0)
        fld dword ptr [esp + 018h]
        fmul dword ptr [esp + 018h]
        faddp st(1), st(0)
        fiadd dword ptr [esp + 010h]
        fsqrt 
        fdiv dword ptr [esp + 4]
        fmulp st(1), st(0)
        add esp, 014h
        ret 
    }
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
/* TC20035aa0/200358e0: native layouts, exact Windows x87 stores and gates. */
static const float moveZero827 = 0.0f;
static const float moveFive827 = 5.0f;
typedef char moveCommandSize827[(sizeof(usercmd_t) == 28) ? 1 : -1];
typedef char moveCommandTime827[(offsetof(usercmd_t, serverTime) == 0) ? 1 : -1];
typedef char moveProtocol827[(DT_MOVELEFT == 1 && EF_PRONE == 0x80000 && ENTITYNUM_NONE == 0x3ff) ? 1 : -1];
enum {
    movePlayerState827 = offsetof(pmove_t, ps),
    moveExtension827 = offsetof(pmove_t, pmext),
    moveCommand827 = offsetof(pmove_t, cmd),
    moveForwardCmd827 = offsetof(pmove_t, cmd) + offsetof(usercmd_t, forwardmove),
    moveRightCmd827 = offsetof(pmove_t, cmd) + offsetof(usercmd_t, rightmove),
    moveDodgeTime827 = offsetof(pmoveExt_t, dodgeTime),
    moveDt827 = offsetof(pmoveExt_t, dtmove),
    moveInstability827 = offsetof(playerState_t, stats) + STAT_TCE_MOVEMENT_INSTABILITY * sizeof(int),
    moveFlags827 = offsetof(playerState_t, stats) + STAT_TCE_FLAGS * sizeof(int),
    moveVelocity827 = offsetof(playerState_t, velocity),
    moveEFlags827 = offsetof(playerState_t, eFlags),
    moveOrigin827 = offsetof(playerState_t, origin),
    moveGroundEntity827 = offsetof(playerState_t, groundEntityNum),
    moveViewYaw827 = offsetof(playerState_t, viewangles) + 4,
    moveDirection827 = offsetof(playerState_t, movementDir),
    moveForward827 = offsetof(pml_t, forward),
    moveRight827 = offsetof(pml_t, right),
    moveFrame827 = offsetof(pml_t, frametime),
    moveGround827 = offsetof(pml_t, groundPlane),
    moveNormal827 = offsetof(pml_t, groundTrace) + offsetof(trace_t, plane) + offsetof(cplane_t, normal),
    movePrevious827 = offsetof(pml_t, previous_origin)
};
/* Private original __ftol ABI extraction: consumes ST0, returns low EAX/high EDX. */
static __declspec(naked) void PM_MovementTruncate827(void) {
    __asm {
        push ebp
        mov ebp, esp
        add esp, -12
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
static __declspec(naked) void PM_SetMovementDir(void) {
    __asm {
        sub esp, 0x1c
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + movePlayerState827]
        fld dword ptr [ecx + moveOrigin827]
        fsub dword ptr [pml+movePrevious827]
        fstp dword ptr [esp + 4]
        mov edx, dword ptr [eax + movePlayerState827]
        fld dword ptr [edx + moveOrigin827+4]
        fsub dword ptr [pml+movePrevious827+4]
        fstp dword ptr [esp + 8]
        mov ecx, dword ptr [eax + movePlayerState827]
        fld dword ptr [ecx + moveOrigin827+8]
        fsub dword ptr [pml+movePrevious827+8]
        fstp dword ptr [esp + 0xc]
        mov cl, byte ptr [eax + moveForwardCmd827]
        test cl, cl
        jne move827_20035ae7
        mov cl, byte ptr [eax + moveRightCmd827]
        test cl, cl
        je move827_20035bd9
move827_20035ae7:
        mov edx, dword ptr [eax + movePlayerState827]
        cmp dword ptr [edx + moveGroundEntity827], 0x3ff
        je move827_20035bd9
        lea eax, [esp + 4]
        push eax
        call VectorLength
        fld st(0)
        fcomp dword ptr [moveZero827]
        add esp, 4
        fnstsw ax
        test ah, 0x40
        jne move827_20035bd2
        fld dword ptr [pml+moveFrame827]
        fmul dword ptr [moveFive827]
        fxch st(1)
        fcompp 
        fnstsw ax
        test ah, 0x41
        jne move827_20035bd4
        lea ecx, [esp + 0x10]
        lea edx, [esp + 4]
        push ecx
        push edx
        call VectorNormalize2
        lea eax, [esp + 0x18]
        lea ecx, [esp + 0x18]
        push eax
        push ecx
        fstp st(0)
        call vectoangles
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx + movePlayerState827]
        mov edx, dword ptr [esp + 0x24]
        mov ecx, dword ptr [eax + moveViewYaw827]
        push ecx
        push edx
        call AngleDelta
        add esp, 0x18
        call PM_MovementTruncate827
        mov ecx, eax
        mov eax, dword ptr [pm]
        mov dl, byte ptr [eax + moveForwardCmd827]
        test dl, dl
        jge move827_20035ba1
        add ecx, 0xb4
        mov dword ptr [esp], ecx
        push ecx
        fild dword ptr [esp + 4]
        fstp dword ptr [esp]
        call AngleNormalize180
        add esp, 4
        call PM_MovementTruncate827
        mov ecx, eax
move827_20035ba1:
        mov eax, ecx
        cdq 
        xor eax, edx
        sub eax, edx
        cmp eax, 0x4b
        jle move827_20035bc0
        xor edx, edx
        test ecx, ecx
        setle dl
        dec edx
        and edx, 0x96
        add edx, -0x4b
        mov ecx, edx
move827_20035bc0:
        movsx eax, cl
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx + movePlayerState827]
        mov dword ptr [edx + moveDirection827], eax
        add esp, 0x1c
        ret 
move827_20035bd2:
        fstp st(0)
move827_20035bd4:
        mov eax, dword ptr [pm]
move827_20035bd9:
        mov eax, dword ptr [eax + movePlayerState827]
        mov dword ptr [eax + moveDirection827], 0
        add esp, 0x1c
        ret 
    }
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
/* TC:E qagame200361a0 / Linux00094d6a; MSVC32 instruction schedule below. */
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC qagame200361a0/20036610: retained x87 values, native field bindings. */
static void PM_WeaponTruncateST0(void);
enum {
    movementPs833 = offsetof(pmove_t,ps),
    movementExt833 = offsetof(pmove_t,pmext),
    movementCharacter833 = offsetof(pmove_t,character),
    movementCmd833 = offsetof(pmove_t,cmd),
    movementForwardCmd833 = offsetof(pmove_t,cmd)+offsetof(usercmd_t,forwardmove),
    movementRightCmd833 = offsetof(pmove_t,cmd)+offsetof(usercmd_t,rightmove),
    movementUpCmd833 = offsetof(pmove_t,cmd)+offsetof(usercmd_t,upmove),
    movementSprint833 = offsetof(pmoveExt_t,sprintTime),
    movementAnim833 = offsetof(bg_character_t,animModelInfo),
    movementFlags833 = offsetof(playerState_t,pm_flags),
    movementTime833 = offsetof(playerState_t,pm_time),
    movementGround833 = offsetof(playerState_t,groundEntityNum),
    movementEFlags833 = offsetof(playerState_t,eFlags),
    movementWeaponState833 = offsetof(playerState_t,weaponstate),
    movementViewHeight833 = offsetof(playerState_t,viewheight),
    movementOrigin833 = offsetof(playerState_t,origin)+0*sizeof(float),
    movementOriginY833 = offsetof(playerState_t,origin)+1*sizeof(float),
    movementOriginZ833 = offsetof(playerState_t,origin)+2*sizeof(float),
    movementVelocity833 = offsetof(playerState_t,velocity)+0*sizeof(float),
    movementVelocityY833 = offsetof(playerState_t,velocity)+1*sizeof(float),
    movementVelocityZ833 = offsetof(playerState_t,velocity)+2*sizeof(float),
    movementSeed833 = offsetof(playerState_t,stats)+STAT_TCE_SHOT_SEED*sizeof(int),
    movementWeaponFlags833 = offsetof(playerState_t,stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    movementTceFlags833 = offsetof(playerState_t,stats)+STAT_TCE_FLAGS*sizeof(int),
    movementInstability833 = offsetof(playerState_t,stats)+STAT_TCE_MOVEMENT_INSTABILITY*sizeof(int),
    movementPhase833 = offsetof(playerState_t,stats)+STAT_TCE_AIM_PHASE*sizeof(int),
    movementPersistent833 = offsetof(playerState_t,persistant)+14*sizeof(int),
    movementFrameTime833 = offsetof(pml_t,frametime),
    movementGroundPlane833 = offsetof(pml_t,groundPlane),
    movementWalking833 = offsetof(pml_t,walking),
    movementForward833 = offsetof(pml_t,forward),
    movementRight833 = offsetof(pml_t,right)
};
static const unsigned int movementK200ac100 = 0x00000000u;
static const unsigned int movementK200ac110 = 0x3f800000u;
static const unsigned int movementK200ac180 = 0x3f000000u;
static const unsigned int movementK200ac2b8 = 0x41200000u;
static const unsigned int movementK200ac330 = 0x42480000u;
static const unsigned int movementK200ac3ac = 0x447a0000u;
static const unsigned int movementK200ac484 = 0x3a83126fu;
static const unsigned int movementK200ac6e8 = 0x3fa00000u;
static const unsigned int movementK200ac708[2] = { 0x00000000u, 0x3ff80000u };
static const unsigned int movementK200ac744 = 0x3c010204u;
static const unsigned int movementK200ac7cc = 0x43870000u;
static const unsigned int movementK200ac7d0 = 0x3f34fdf4u;
typedef char movementScalarLayout833[(sizeof(float)==4 && sizeof(int)==4 && sizeof(vec3_t)==12 && EV_JUMP==24 && ANIM_ET_JUMP==4 && ANIM_ET_JUMPBK==5) ? 1 : -1];
static __declspec(naked) qboolean PM_CheckJump(void) {
    __asm {

        SUB ESP,0x14

        PUSH ESI

        MOV ESI,dword ptr [pm]

        MOV dword ptr [ESP + 0x4],0x3f800000

        MOV ECX,dword ptr [ESI + movementPs833]

        MOV EAX,dword ptr [ECX + movementEFlags833]

        TEST EAX,0x80000

        JZ movementAt200361c5

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt200361c5:
        MOV EDX,dword ptr [ECX + movementFlags833]

        TEST DL,0x1

        JZ movementAt200361d4

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt200361d4:
        TEST EAX,0x100000

        JZ movementAt200361e2

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt200361e2:
        MOV EAX,dword ptr [ECX + movementWeaponFlags833]

        TEST AH,0x10

        JZ movementAt200361f4

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt200361f4:
        MOV EAX,dword ptr [ECX + movementGround833]

        CMP EAX,0x40

        JGE movementAt20036203

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt20036203:
        TEST DL,0x4

        JZ movementAt2003620f

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt2003620f:
        CMP EAX,0x3ff

        JNZ movementAt2003621d

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt2003621d:
        TEST DH,0x2

        JZ movementAt20036229

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt20036229:
        CMP byte ptr [ESI + movementUpCmd833],0xa

        JGE movementAt20036236

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt20036236:
        TEST DL,0x2

        JZ movementAt20036246

        MOV byte ptr [ESI + movementUpCmd833],0x0

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt20036246:
        TEST byte ptr [ECX + movementTceFlags833],0x40

        JNZ movementAt200362ac

        MOV EAX,dword ptr [ECX + movementTime833]

        CMP EAX,0x1f4

        MOV dword ptr [ESP + 0x8],EAX

        JLE movementAt20036268

        MOV byte ptr [ESI + movementUpCmd833],0x0

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt20036268:
        TEST DL,0x20

        JZ movementAt200362ac

        TEST EAX,EAX

        JZ movementAt200362ac

        CMP EAX,0x258

        JLE movementAt20036283

        MOV byte ptr [ESI + movementUpCmd833],0x0

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt20036283:
        FILD dword ptr [ESP + 0x8]

        FMUL dword ptr [movementK200ac484]

        FSUBR dword ptr [movementK200ac110]

        FST dword ptr [ESP + 0x4]

        FCOMP dword ptr [movementK200ac100]

        FNSTSW AX

        TEST AH,0x1

        JZ movementAt200362ac

        MOV dword ptr [ESP + 0x4],0x0
    movementAt200362ac:
        FLD dword ptr [ECX + movementVelocityY833]

        FLD dword ptr [ECX + movementVelocity833]

        LEA EAX,[ECX + movementVelocity833]

        FLD ST(0)

        FMUL ST(0),ST(1)

        FLD ST(2)

        FMUL ST(0),ST(3)

        PUSH EAX

        FADDP ST(1),ST(0)

        FSQRT

        FSTP dword ptr [ESP + 0xc]

        FSTP ST(0)

        FSTP ST(0)

        CALL VectorLength

        FCOMP dword ptr [movementK200ac330]

        MOV ECX,dword ptr [pm]

        ADD ESP,0x4

        FNSTSW AX

        TEST AH,0x1

        JZ movementAt200362ec

        MOV AL,byte ptr [ECX + movementForwardCmd833]

        TEST AL,AL

        JNZ movementAt2003631c
    movementAt200362ec:
        MOV EAX,dword ptr [ECX + movementPs833]

        TEST byte ptr [EAX + movementTceFlags833],0x40

        JNZ movementAt2003631c

        FLD dword ptr [ESP + 0x4]

        MOV EDX,dword ptr [ECX + movementExt833]

        FMUL dword ptr [movementK200ac7d0]

        CMP dword ptr [EDX + movementSprint833],0x2ee

        FSTP dword ptr [ESP + 0x4]

        JGE movementAt2003631c

        MOV byte ptr [ECX + movementUpCmd833],0x0

        XOR EAX,EAX

        POP ESI

        ADD ESP,0x14

        RET
    movementAt2003631c:
        FLD dword ptr [ESP + 0x8]

        FCOMP dword ptr [movementK200ac2b8]

        FNSTSW AX

        TEST AH,0x41

        JNZ movementAt200363b5

        MOV AL,byte ptr [ECX + movementForwardCmd833]

        TEST AL,AL

        JNZ movementAt2003633f

        MOV AL,byte ptr [ECX + movementRightCmd833]

        TEST AL,AL

        JZ movementAt200363b5
    movementAt2003633f:
        MOVSX EAX,byte ptr [ECX + movementForwardCmd833]

        MOV dword ptr [ESP + 0x8],EAX

        LEA EDX,[ESP + 0xc]

        FILD dword ptr [ESP + 0x8]

        PUSH EDX

        FMUL dword ptr [movementK200ac744]

        FSTP dword ptr [ESP + 0x10]

        MOVSX ECX,byte ptr [ECX + movementRightCmd833]

        MOV dword ptr [ESP + 0xc],ECX

        MOV dword ptr [ESP + 0x18],0x0

        FILD dword ptr [ESP + 0xc]

        FMUL dword ptr [movementK200ac744]

        FSTP dword ptr [ESP + 0x14]

        CALL VectorNormalize

        FSTP ST(0)

        FLD dword ptr [ESP + 0x10]

        FADD dword ptr [movementK200ac110]

        ADD ESP,0x4

        FMUL dword ptr [movementK200ac180]

        FCOM dword ptr [movementK200ac180]

        FNSTSW AX

        TEST AH,0x1

        JZ movementAt200363a7

        FSTP ST(0)

        FLD dword ptr [movementK200ac180]
    movementAt200363a7:
        FMUL dword ptr [ESP + 0x4]

        MOV ECX,dword ptr [pm]

        FSTP dword ptr [ESP + 0x4]
    movementAt200363b5:
        MOV ECX,dword ptr [ECX + movementPs833]

        MOV EDX,dword ptr [ECX + movementWeaponFlags833]

        OR DH,0x10

        MOV dword ptr [ECX + movementWeaponFlags833],EDX

        MOV EAX,[pm]

        MOV ECX,dword ptr [EAX + movementPs833]

        MOV dword ptr [ECX + movementInstability833],0x3e8

        MOV EAX,[pm]

        MOV EDX,dword ptr [EAX + movementPs833]

        CMP dword ptr [EDX + movementWeaponState833],0x7

        JZ movementAt2003643f

        MOV EAX,EDX

        LEA EDX,[ESP + 0x8]

        PUSH EDX

        MOV ECX,dword ptr [EAX + movementSeed833]

        MOV dword ptr [ESP + 0xc],ECX

        CALL Q_random

        FMUL dword ptr [movementK200ac3ac]

        ADD ESP,0x4

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [pm]

        MOV EDX,dword ptr [ECX + movementPs833]

        MOV dword ptr [EDX + movementPhase833],EAX

        MOV EAX,[pm]

        MOV ECX,dword ptr [EAX + movementPs833]

        MOV EDX,dword ptr [ECX + movementPhase833]

        CMP EDX,0x3e8

        JLE movementAt2003643f

        ADD EDX,0xfffffc18

        MOV dword ptr [ECX + movementPhase833],EDX

        MOV EAX,[pm]
    movementAt2003643f:
        MOV ECX,dword ptr [EAX + movementPs833]

        MOV EDX,dword ptr [ECX + movementTceFlags833]

        TEST DH,0x2

        JZ movementAt2003645a

        FLD dword ptr [ESP + 0x4]

        FMUL dword ptr [movementK200ac6e8]

        FSTP dword ptr [ESP + 0x4]
    movementAt2003645a:
        XOR ESI,ESI

        PUSH 0x18

        MOV dword ptr [pml + movementGroundPlane833],ESI

        MOV dword ptr [pml + movementWalking833],ESI

        MOV EAX,dword ptr [EAX + movementPs833]

        FLD dword ptr [ESP + 0x8]

        MOV EDX,dword ptr [EAX + movementFlags833]

        OR EDX,0x2

        MOV dword ptr [EAX + movementFlags833],EDX

        MOV EDX,dword ptr [pm]

        FMUL dword ptr [movementK200ac7cc]

        MOV EAX,dword ptr [EDX + movementPs833]

        MOV dword ptr [EAX + movementGround833],0x3ff

        MOV ECX,dword ptr [pm]

        MOV EDX,dword ptr [ECX + movementPs833]

        FSTP dword ptr [EDX + movementVelocityZ833]

        CALL PM_AddEvent

        MOV EAX,[pm]

        ADD ESP,0x4

        MOV EDX,dword ptr [EAX + movementPs833]

        MOV ECX,dword ptr [EDX + movementWeaponFlags833]

        TEST CL,0x4

        JZ movementAt200364c1

        AND ECX,0xfffffffb

        MOV dword ptr [EDX + movementWeaponFlags833],ECX

        MOV EAX,[pm]
    movementAt200364c1:
        MOV EDX,dword ptr [EAX + movementPs833]

        MOV ECX,dword ptr [EDX + movementWeaponFlags833]

        TEST CL,0x8

        JZ movementAt200364e5

        TEST byte ptr [EDX + movementPersistent833],0x8

        JNZ movementAt200364e5

        AND ECX,0xfffffff7

        MOV dword ptr [EDX + movementWeaponFlags833],ECX

        MOV EAX,[pm]
    movementAt200364e5:
        MOV CL,byte ptr [EAX + movementForwardCmd833]

        PUSH 0x1

        TEST CL,CL

        PUSH ESI

        JL movementAt2003651e

        MOV ECX,dword ptr [EAX + movementCharacter833]

        MOV EAX,dword ptr [EAX + movementPs833]

        PUSH 0x4

        MOV EDX,dword ptr [ECX + movementAnim833]

        PUSH EDX

        PUSH EAX

        CALL BG_AnimScriptEvent

        MOV ECX,dword ptr [pm]

        ADD ESP,0x14

        MOV EAX,dword ptr [ECX + movementPs833]

        POP ESI

        MOV ECX,dword ptr [EAX + movementFlags833]

        AND ECX,0xfffffff7

        MOV dword ptr [EAX + movementFlags833],ECX

        MOV EAX,0x1

        ADD ESP,0x14

        RET
    movementAt2003651e:
        MOV EDX,dword ptr [EAX + movementCharacter833]

        PUSH 0x5

        MOV ECX,dword ptr [EDX + movementAnim833]

        MOV EDX,dword ptr [EAX + movementPs833]

        PUSH ECX

        PUSH EDX

        CALL BG_AnimScriptEvent

        MOV EAX,[pm]

        ADD ESP,0x14

        MOV EAX,dword ptr [EAX + movementPs833]

        POP ESI

        MOV ECX,dword ptr [EAX + movementFlags833]

        OR ECX,0x8

        MOV dword ptr [EAX + movementFlags833],ECX

        MOV EAX,0x1

        ADD ESP,0x14

        RET
    }
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
static const float waterK200ac100 = 0.0;
static const float waterK200ac260 = -60.0;
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
static __declspec(naked) qboolean PM_CheckWaterJump(void) {
    __asm {
        mov eax, dword ptr [pm]
        sub esp, 0x18
        mov ecx, dword ptr [eax+waterPlayerState830]
        mov edx, dword ptr [ecx+waterPmTime830]
        test edx, edx
        je water830_200356b7
        xor eax, eax
        add esp, 0x18
        ret 
water830_200356b7:
        cmp dword ptr [eax+waterLevel830], 2
        je water830_200356c6
        xor eax, eax
        add esp, 0x18
        ret 
water830_200356c6:
        mov edx, dword ptr [pml+waterForward830]
        mov eax, dword ptr [pml+waterForward830+4]
        lea ecx, [esp]
        mov dword ptr [esp], edx
        push ecx
        mov dword ptr [esp + 8], eax
        mov dword ptr [esp + 0xc], 0
        call VectorNormalize
        mov eax, dword ptr [pm]
        fstp st(0)
        fld dword ptr [esp + 4]
        fmul dword ptr [waterK200ac190]
        mov edx, dword ptr [eax+waterPlayerState830]
        fadd dword ptr [edx+waterOrigin830]
        fstp dword ptr [esp + 0x10]
        fld dword ptr [esp + 8]
        fmul dword ptr [waterK200ac190]
        mov ecx, dword ptr [eax+waterPlayerState830]
        fadd dword ptr [ecx+waterOrigin830+4]
        fstp dword ptr [esp + 0x14]
        fld dword ptr [esp + 0xc]
        fmul dword ptr [waterK200ac190]
        mov edx, dword ptr [eax+waterPlayerState830]
        fadd dword ptr [edx+waterOrigin830+8]
        fadd dword ptr [waterK200ac2bc]
        fstp dword ptr [esp + 0x18]
        mov ecx, dword ptr [eax+waterPlayerState830]
        mov edx, dword ptr [ecx+waterClient830]
        lea ecx, [esp + 0x10]
        push edx
        push ecx
        call dword ptr [eax+waterContents830]
        add esp, 0xc
        test al, 1
        jne water830_20035752
        xor eax, eax
        add esp, 0x18
        ret 
water830_20035752:
        fld dword ptr [esp + 0x14]
        fadd dword ptr [waterK200ac384]
        mov eax, dword ptr [pm]
        fstp dword ptr [esp + 0x14]
        mov edx, dword ptr [eax+waterPlayerState830]
        mov ecx, dword ptr [edx+waterClient830]
        lea edx, [esp + 0xc]
        push ecx
        push edx
        call dword ptr [eax+waterContents830]
        add esp, 8
        test eax, eax
        je water830_20035786
        xor eax, eax
        add esp, 0x18
        ret 
water830_20035786:
        fld dword ptr [pml+waterForward830]
        mov eax, dword ptr [pm]
        fmul dword ptr [waterK200ac1a4]
        mov ecx, dword ptr [eax+waterPlayerState830]
        fstp dword ptr [ecx+waterVelocity830]
        fld dword ptr [pml+waterForward830+4]
        mov edx, dword ptr [pm]
        fmul dword ptr [waterK200ac1a4]
        mov eax, dword ptr [edx+waterPlayerState830]
        fstp dword ptr [eax+waterVelocity830+4]
        fld dword ptr [pml+waterForward830+8]
        mov ecx, dword ptr [pm]
        fmul dword ptr [waterK200ac1a4]
        mov edx, dword ptr [ecx+waterPlayerState830]
        fstp dword ptr [edx+waterVelocity830+8]
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+waterPlayerState830]
        mov dword ptr [ecx+waterVelocity830+8], 0x43af0000
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx+waterPlayerState830]
        mov ecx, dword ptr [eax+waterPmFlags830]
        or ch, 1
        mov dword ptr [eax+waterPmFlags830], ecx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+waterPlayerState830]
        mov eax, 1
        mov dword ptr [ecx+waterPmTime830], 0x7d0
        add esp, 0x18
        ret 
    }
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
static const unsigned int posture832Const200ac100 = 0x00000000u;
static const unsigned int posture832Const200ac110 = 0x3f800000u;
static const unsigned int posture832Const200ac180 = 0x3f000000u;
static const unsigned int posture832Const200ac19c = 0x41a00000u;
static const unsigned int posture832Const200ac290 = 0x41c00000u;
static const unsigned int posture832Const200ac2d4 = 0x42000000u;
static const unsigned int posture832Const200ac340 = 0x40a00000u;
static const unsigned int posture832Const200ac3e8 = 0x3dcccccdu;
static const unsigned int posture832Const200ac6e8 = 0x3fa00000u;
static const unsigned int posture832Const200ac700 = 0x42f00000u;
static const unsigned int posture832Const200ac71c = 0x41accccdu;
static const unsigned int posture832Const200ac78c = 0xc1000000u;
static const unsigned int posture832Const200ac790 = 0xc1200000u;
static const unsigned int posture832Const200ac7ec = 0x3fa66666u;
static const unsigned int posture832Const200ac7f0 = 0x435c0000u;
static const unsigned int posture832Const200ac7f8[2] = { 0xb4e81b4fu, 0x3f6b4e81u };
enum {
    posture832pm0 = offsetof(pmove_t, ps),
    posture832ps68 = offsetof(playerState_t, eFlags),
    posture832pm4 = offsetof(pmove_t, pmext),
    posture832pmc = offsetof(pmove_t, cmd)+offsetof(usercmd_t,serverTime),
    posture832ext34 = offsetof(pmoveExt_t, proneTime),
    posture832psc = offsetof(playerState_t, pm_flags),
    posture832ps140 = offsetof(playerState_t, persistant)+PERS_HWEAPON_USE*sizeof(int),
    posture832ps30 = offsetof(playerState_t, weaponDelay),
    posture832psa4 = offsetof(playerState_t, weapon),
    posture832psf0 = offsetof(playerState_t, stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    posture832ps50 = offsetof(playerState_t, groundEntityNum),
    posture832pm110 = offsetof(pmove_t, waterlevel),
    posture832pm23 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,doubleTap),
    posture832pm11 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,wbuttons),
    posture832psf4 = offsetof(playerState_t, stats)+STAT_TCE_FLAGS*sizeof(int),
    posture832ps14 = offsetof(playerState_t, origin),
    posture832ps18 = offsetof(playerState_t, origin)+4,
    posture832ps1c = offsetof(playerState_t, origin)+8,
    posture832pm44 = offsetof(pmove_t, tracemask),
    posture832psa0 = offsetof(playerState_t, clientNum),
    posture832pm128 = offsetof(pmove_t, trace),
    posture832ext38 = offsetof(pmoveExt_t, proneGroundTime),
    posture832ps4 = offsetof(playerState_t, pm_type),
    posture832pm22 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,upmove),
    posture832ps3dc = offsetof(playerState_t, mins),
    posture832pmf4 = offsetof(pmove_t, mins),
    posture832ps3e0 = offsetof(playerState_t, mins)+4,
    posture832pmf8 = offsetof(pmove_t, mins)+4,
    posture832ps3e8 = offsetof(playerState_t, maxs),
    posture832pm100 = offsetof(pmove_t, maxs),
    posture832ps3ec = offsetof(playerState_t, maxs)+4,
    posture832pm104 = offsetof(pmove_t, maxs)+4,
    posture832ps3e4 = offsetof(playerState_t, mins)+8,
    posture832pmfc = offsetof(pmove_t, mins)+8,
    posture832ps3f4 = offsetof(playerState_t, crouchMaxZ),
    posture832pm108 = offsetof(pmove_t, maxs)+8,
    posture832ps148 = offsetof(playerState_t, persistant)+14*sizeof(int),
    posture832ext4 = offsetof(pmoveExt_t, jumpTime),
    posture832ps47c = offsetof(playerState_t, jumpTime),
    posture832ps20 = offsetof(playerState_t, velocity),
    posture832pm21 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,rightmove),
    posture832pm20 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,forwardmove),
    posture832ps3f0 = offsetof(playerState_t, maxs)+8,
    posture832ps3fc = offsetof(playerState_t, standViewHeight),
    posture832psbc = offsetof(playerState_t, viewheight),
    posture832pm8 = offsetof(pmove_t, character),
    posture832character40 = offsetof(bg_character_t, animModelInfo),
    posture832ps10 = offsetof(playerState_t, pm_time),
    posture832ps24 = offsetof(playerState_t, velocity)+4,
    posture832pm114 = offsetof(pmove_t, xyspeed),
    posture832ps28 = offsetof(playerState_t, velocity)+8,
    posture832ps8 = offsetof(playerState_t, bobCycle),
    posture832pm4c = offsetof(pmove_t, noFootsteps),
    posture832ps3c = offsetof(playerState_t, leanf),
    posture832pm10 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,buttons)
};
static __declspec(naked) qboolean PM_CheckProne(void) {
    __asm {
        MOV EAX,[pm]
        SUB ESP,0x68
        MOV EDX,dword ptr [EAX + posture832pm0]
        PUSH EBX
        PUSH ESI
        PUSH EDI
        MOV ECX,dword ptr [EDX + posture832ps68]
        MOV ESI,0x40
        TEST ECX,0x80000
        JNZ posture832_20035028
        TEST ECX,0x100000
        JZ posture832_20034d4d
        MOV EDI,dword ptr [EAX + posture832pm4]
        MOV EBX,dword ptr [EAX + posture832pmc]
        MOV EDI,dword ptr [EDI + posture832ext34]
        ADD EDI,EBX
        CMP EDI,0x2ee
        JLE posture832_20034d4d
        AND ECX,0xffefffff
        MOV dword ptr [EDX + posture832ps68],ECX
        MOV EAX,[pm]
        MOV EAX,dword ptr [EAX + posture832pm0]
        MOV ECX,dword ptr [EAX + posture832ps68]
        AND ECX,0xff7fffff
        MOV dword ptr [EAX + posture832ps68],ECX
        MOV EAX,[pm]
posture832_20034d4d:
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EBX,dword ptr [ECX + posture832psc]
        TEST BL,0x4
        JNZ posture832_2003541d
        MOV EDX,dword ptr [ECX + posture832ps140]
        TEST EDX,EDX
        JNZ posture832_2003541d
        MOV EDX,dword ptr [ECX + posture832ps68]
        TEST DH,0x80
        JNZ posture832_2003541d
        MOV EDI,dword ptr [ECX + posture832ps30]
        TEST EDI,EDI
        JZ posture832_20034d89
        CMP dword ptr [ECX + posture832psa4],0x41
        JZ posture832_2003541d
posture832_20034d89:
        CMP dword ptr [ECX + posture832psa4],0x3c
        JZ posture832_2003541d
        TEST EDX,0x1000000
        JNZ posture832_2003541d
        MOV EDX,dword ptr [ECX + posture832psf0]
        TEST DH,0x10
        JNZ posture832_2003541d
        MOV ECX,dword ptr [ECX + posture832ps50]
        CMP ECX,0x3ff
        JZ posture832_2003541d
        MOV EDX,dword ptr [pml.tceContentRestriction]
        TEST EDX,EDX
        JNZ posture832_2003541d
        CMP ECX,ESI
        JL posture832_2003541d
        CMP dword ptr [EAX + posture832pm110],0x1
        JG posture832_2003541d
        TEST BL,0x1
        JZ posture832_20034dee
        CMP byte ptr [EAX + posture832pm23],0x3
        JZ posture832_20034df8
posture832_20034dee:
        TEST byte ptr [EAX + posture832pm11],0x80
        JZ posture832_20035028
posture832_20034df8:
        MOV ECX,dword ptr [EAX + posture832pm4]
        MOV EDI,dword ptr [EAX + posture832pmc]
        MOV EDX,dword ptr [ECX + posture832ext34]
        ADD EDX,EDI
        CMP EDX,0x2ee
        JLE posture832_20035028
        MOV ECX,dword ptr [playerlegsProneMins]
        MOV EDX,dword ptr [playerlegsProneMins+4]
        MOV dword ptr [ESP + 0x18],ECX
        MOV ECX,dword ptr [playerlegsProneMins+8]
        MOV dword ptr [ESP + 0x1c],EDX
        MOV EDX,dword ptr [playerlegsProneMaxs]
        MOV dword ptr [ESP + 0x20],ECX
        MOV ECX,dword ptr [playerlegsProneMaxs+4]
        MOV dword ptr [ESP + 0xc],EDX
        MOV EDX,dword ptr [playerlegsProneMaxs+8]
        FLD dword ptr [posture832Const200ac110]
        MOV dword ptr [ESP + 0x10],ECX
        MOV dword ptr [ESP + 0x14],EDX
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832psf4]
        TEST DH,0x2
        JZ posture832_20034eba
        FSTP ST(0)
        FLD dword ptr [posture832Const200ac6e8]
        FLD dword ptr [ESP + 0x18]
        FMUL dword ptr [posture832Const200ac6e8]
        FSTP dword ptr [ESP + 0x18]
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [posture832Const200ac6e8]
        FSTP dword ptr [ESP + 0x1c]
        FLD dword ptr [ESP + 0x20]
        FMUL dword ptr [posture832Const200ac6e8]
        FSTP dword ptr [ESP + 0x20]
        FLD dword ptr [ESP + 0xc]
        FMUL dword ptr [posture832Const200ac6e8]
        FSTP dword ptr [ESP + 0xc]
        FLD dword ptr [ESP + 0x10]
        FMUL dword ptr [posture832Const200ac6e8]
        FSTP dword ptr [ESP + 0x10]
        FLD dword ptr [ESP + 0x14]
        FMUL dword ptr [posture832Const200ac6e8]
        FSTP dword ptr [ESP + 0x14]
posture832_20034eba:
        FLD dword ptr [pml.forward]
        MOV EDX,dword ptr [pml.forward+4]
        FMUL ST(0),ST(1)
        MOV dword ptr [ESP + 0x34],EDX
        MOV ECX,dword ptr [EAX + posture832pm0]
        FMUL dword ptr [posture832Const200ac2d4]
        FSUBR dword ptr [ECX + posture832ps14]
        FSTP dword ptr [ESP + 0x24]
        MOV EDX,dword ptr [EAX + posture832pm0]
        FMUL dword ptr [ESP + 0x34]
        FMUL dword ptr [posture832Const200ac2d4]
        FSUBR dword ptr [EDX + posture832ps18]
        MOV EDX,dword ptr [ESP + 0x24]
        FST dword ptr [ESP + 0x28]
        MOV ECX,dword ptr [EAX + posture832pm0]
        FLD dword ptr [ECX + posture832ps1c]
        FADD dword ptr [posture832Const200ac290]
        MOV dword ptr [ESP + 0x30],EDX
        FSTP dword ptr [ESP + 0x2c]
        FSTP dword ptr [ESP + 0x34]
        FLD dword ptr [ESP + 0x2c]
        FSUB dword ptr [posture832Const200ac71c]
        FSUB dword ptr [posture832Const200ac290]
        FSTP dword ptr [ESP + 0x38]
        MOV ECX,dword ptr [EAX + posture832pm44]
        MOV EDX,dword ptr [EAX + posture832pm0]
        PUSH ECX
        MOV ECX,dword ptr [EDX + posture832psa0]
        LEA EDX,[ESP + 0x34]
        PUSH ECX
        PUSH EDX
        LEA ECX,[ESP + 0x18]
        LEA EDX,[ESP + 0x24]
        PUSH ECX
        PUSH EDX
        LEA ECX,[ESP + 0x38]
        LEA EDX,[ESP + 0x50]
        PUSH ECX
        PUSH EDX
        CALL dword ptr [EAX + posture832pm128]
        MOV EAX,dword ptr [ESP + 0x5c]
        ADD ESP,0x1c
        TEST EAX,EAX
        JZ posture832_20034f5e
        CMP dword ptr [ESP + 0x70],ESI
        JGE posture832_2003541d
posture832_20034f5e:
        FLD dword ptr [ESP + 0x44]
        FCOMP dword ptr [posture832Const200ac110]
        FNSTSW AX
        TEST AH,0x40
        JNZ posture832_2003541d
        FLD dword ptr [ESP + 0x50]
        MOV EAX,dword ptr [ESP + 0x48]
        MOV ECX,dword ptr [ESP + 0x4c]
        FADD dword ptr [posture832Const200ac71c]
        MOV EDX,dword ptr [ESP + 0x50]
        MOV dword ptr [ESP + 0x24],EAX
        MOV dword ptr [ESP + 0x30],EAX
        MOV EAX,[pm]
        FSTP dword ptr [ESP + 0x38]
        MOV dword ptr [ESP + 0x28],ECX
        MOV dword ptr [ESP + 0x2c],EDX
        MOV dword ptr [ESP + 0x34],ECX
        MOV EDX,dword ptr [EAX + posture832pm44]
        MOV ECX,dword ptr [EAX + posture832pm0]
        PUSH EDX
        MOV EDX,dword ptr [ECX + posture832psa0]
        LEA ECX,[ESP + 0x34]
        PUSH EDX
        PUSH ECX
        LEA EDX,[ESP + 0x18]
        LEA ECX,[ESP + 0x24]
        PUSH EDX
        PUSH ECX
        LEA EDX,[ESP + 0x38]
        LEA ECX,[ESP + 0x50]
        PUSH EDX
        PUSH ECX
        CALL dword ptr [EAX + posture832pm128]
        MOV EAX,dword ptr [ESP + 0x58]
        ADD ESP,0x1c
        TEST EAX,EAX
        JZ posture832_20034fe3
        CMP dword ptr [ESP + 0x70],ESI
        JGE posture832_20035023
posture832_20034fe3:
        MOV EDX,dword ptr [pm]
        MOV EAX,dword ptr [EDX + posture832pm0]
        MOV EDX,dword ptr [EAX + posture832psc]
        OR EDX,0x1
        MOV dword ptr [EAX + posture832psc],EDX
        MOV EAX,[pm]
        MOV EAX,dword ptr [EAX + posture832pm0]
        MOV ECX,dword ptr [EAX + posture832ps68]
        OR ECX,0x80000
        MOV dword ptr [EAX + posture832ps68],ECX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pm4]
        MOV EDX,dword ptr [EAX + posture832pmc]
        MOV dword ptr [ECX + posture832ext34],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pm4]
        MOV EDX,dword ptr [EAX + posture832pmc]
        MOV dword ptr [ECX + posture832ext38],EDX
posture832_20035023:
        MOV EAX,[pm]
posture832_20035028:
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EDI,0x800000
        MOV EDX,dword ptr [ECX + posture832ps68]
        TEST EDX,0x80000
        JZ posture832_2003524d
        CMP dword ptr [EAX + posture832pm110],0x1
        JG posture832_200350a3
        CMP dword ptr [ECX + posture832ps4],0x3
        JZ posture832_200350a3
        TEST DH,0x80
        JNZ posture832_200350a3
        MOV EDX,dword ptr [ECX + posture832ps50]
        CMP EDX,0x3ff
        JZ posture832_200350a3
        CMP EDX,ESI
        JL posture832_200350a3
        TEST byte ptr [ECX + posture832psc],0x4
        JNZ posture832_200350a3
        MOV ECX,dword ptr [pml.tceContentRestriction]
        TEST ECX,ECX
        JNZ posture832_200350a3
        CMP byte ptr [EAX + posture832pm23],0x4
        JZ posture832_2003508e
        MOV CL,byte ptr [EAX + posture832pm22]
        CMP CL,0xa
        JG posture832_2003508e
        CMP CL,0xf6
        JL posture832_2003508e
        TEST byte ptr [EAX + posture832pm11],0x80
        JZ posture832_2003524d
posture832_2003508e:
        MOV ECX,dword ptr [EAX + posture832pm4]
        MOV EDX,dword ptr [EAX + posture832pmc]
        SUB EDX,dword ptr [ECX + posture832ext34]
        CMP EDX,0x2ee
        JLE posture832_2003524d
posture832_200350a3:
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832ps3dc]
        MOV dword ptr [EAX + posture832pmf4],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832ps3e0]
        MOV dword ptr [EAX + posture832pmf8],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832ps3e8]
        MOV dword ptr [EAX + posture832pm100],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832ps3ec]
        MOV dword ptr [EAX + posture832pm104],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832ps3e4]
        MOV dword ptr [EAX + posture832pmfc],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832ps3f4]
        MOV dword ptr [EAX + posture832pm108],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV ESI,dword ptr [EAX + posture832pm44]
        PUSH ESI
        LEA EDX,[ECX + posture832ps14]
        MOV ECX,dword ptr [ECX + posture832psa0]
        PUSH ECX
        LEA ECX,[EAX + posture832pm100]
        PUSH EDX
        PUSH ECX
        LEA ECX,[EAX + posture832pmf4]
        PUSH ECX
        PUSH EDX
        LEA EDX,[ESP + 0x54]
        PUSH EDX
        CALL dword ptr [EAX + posture832pm128]
        MOV EAX,dword ptr [ESP + 0x58]
        ADD ESP,0x1c
        TEST EAX,EAX
        MOV EAX,[pm]
        JNZ posture832_2003524d
        MOV EAX,dword ptr [EAX + posture832pm0]
        MOV ESI,dword ptr [EAX + posture832psc]
        OR ESI,0x1
        MOV dword ptr [EAX + posture832psc],ESI
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ECX + posture832pm0]
        MOV EDX,dword ptr [EAX + posture832ps68]
        AND EDX,0xfff7ffff
        MOV dword ptr [EAX + posture832ps68],EDX
        MOV EDX,dword ptr [pm]
        MOV EAX,dword ptr [EDX + posture832pm0]
        MOV ECX,dword ptr [EAX + posture832ps68]
        OR ECX,0x100000
        MOV dword ptr [EAX + posture832ps68],ECX
        MOV EAX,[pm]
        MOV EAX,dword ptr [EAX + posture832pm0]
        MOV EBX,dword ptr [EAX + posture832ps68]
        OR EBX,EDI
        MOV dword ptr [EAX + posture832ps68],EBX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pmc]
        MOV EDX,dword ptr [EAX + posture832pm4]
        NEG ECX
        MOV dword ptr [EDX + posture832ext34],ECX
        MOV EAX,[pm]
        MOV CL,byte ptr [EAX + posture832pm22]
        CMP CL,0xa
        JLE posture832_200351cb
        MOV ESI,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ESI + posture832psf0]
        TEST DH,0x2
        JZ posture832_200351cb
        AND DH,0xfd
        MOV dword ptr [ESI + posture832psf0],EDX
        JMP posture832_20035200
posture832_200351cb:
        CMP CL,0xf6
        JGE posture832_20035205
        MOV ECX,dword ptr [EAX + posture832pm0]
        TEST byte ptr [ECX + posture832ps148],0x4
        JZ posture832_20035205
        MOV EDX,dword ptr [ECX + posture832psf0]
        OR DH,0x2
        MOV dword ptr [ECX + posture832psf0],EDX
        MOV EAX,[pm]
        MOV EAX,dword ptr [EAX + posture832pm0]
        MOV ECX,dword ptr [EAX + posture832psf0]
        OR CH,0x4
        MOV dword ptr [EAX + posture832psf0],ECX
posture832_20035200:
        MOV EAX,[pm]
posture832_20035205:
        MOV ECX,dword ptr [EAX + posture832pm0]
        CMP dword ptr [ECX + posture832psa4],0x3e
        JNZ posture832_20035223
        PUSH 0x0
        PUSH 0x1f
        PUSH 0x3e
        CALL PM_BeginWeaponChange
        MOV EAX,[pm]
        ADD ESP,0xc
posture832_20035223:
        MOV EDX,dword ptr [EAX + posture832pmc]
        MOV EAX,dword ptr [EAX + posture832pm4]
        SUB EDX,0x28a
        MOV dword ptr [EAX + posture832ext4],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture832pmc]
        MOV EDX,dword ptr [EAX + posture832pm0]
        SUB ECX,0x28a
        MOV dword ptr [EDX + posture832ps47c],ECX
        MOV EAX,[pm]
posture832_2003524d:
        MOV EAX,dword ptr [EAX + posture832pm0]
        TEST dword ptr [EAX + posture832ps68],0x80000
        JZ posture832_2003541d
        ADD EAX,posture832ps20
        PUSH EAX
        CALL VectorLength
        MOV ECX,dword ptr [pm]
        ADD ESP,0x4
        FCOMP dword ptr [posture832Const200ac19c]
        MOVSX EAX,byte ptr [ECX + posture832pm21]
        CDQ
        MOV ESI,EAX
        MOVSX EAX,byte ptr [ECX + posture832pm20]
        XOR ESI,EDX
        SUB ESI,EDX
        CDQ
        XOR EAX,EDX
        SUB EAX,EDX
        ADD ESI,EAX
        XOR EAX,EAX
        CMP ESI,0xa
        SETG AL
        TEST EAX,EAX
        FNSTSW AX
        JZ posture832_200352fa
        TEST AH,0x41
        JNZ posture832_20035319
        MOV EDX,dword ptr [ECX + posture832pm0]
        MOV EAX,dword ptr [EDX + posture832ps68]
        TEST EAX,0x100000
        JNZ posture832_20035319
        OR EAX,0x100000
        MOV dword ptr [EDX + posture832ps68],EAX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ECX + posture832pm0]
        MOV EAX,dword ptr [EAX + posture832psa4]
        SUB EAX,0x39
        JZ posture832_200352ea
        DEC EAX
        JZ posture832_200352da
        DEC EAX
        JNZ posture832_20035319
        PUSH 0x0
        PUSH 0x21
        PUSH 0x3b
        CALL PM_BeginWeaponChange
        ADD ESP,0xc
        JMP posture832_20035313
posture832_200352da:
        PUSH 0x0
        PUSH 0x20
        PUSH 0x3a
        CALL PM_BeginWeaponChange
        ADD ESP,0xc
        JMP posture832_20035313
posture832_200352ea:
        PUSH 0x0
        PUSH 0x19
        PUSH 0x39
        CALL PM_BeginWeaponChange
        ADD ESP,0xc
        JMP posture832_20035313
posture832_200352fa:
        TEST AH,0x1
        JZ posture832_20035319
        MOV EDX,dword ptr [ECX + posture832pm0]
        MOV EAX,dword ptr [EDX + posture832ps68]
        TEST EAX,0x100000
        JZ posture832_20035319
        AND EAX,0xffefffff
        MOV dword ptr [EDX + posture832ps68],EAX
posture832_20035313:
        MOV ECX,dword ptr [pm]
posture832_20035319:
        MOV EDX,dword ptr [ECX + posture832pm4]
        MOV EAX,dword ptr [ECX + posture832pmc]
        MOV ECX,dword ptr [ECX + posture832pm0]
        SUB EAX,dword ptr [EDX + posture832ext34]
        CMP EAX,0x2ee
        JGE posture832_20035344
        MOV EDX,dword ptr [ECX + posture832ps68]
        OR EDX,0x100000
        MOV dword ptr [ECX + posture832ps68],EDX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ECX + posture832pm0]
        OR dword ptr [EAX + posture832ps68],EDI
        JMP posture832_2003534b
posture832_20035344:
        AND dword ptr [ECX + posture832ps68],0xff7fffff
posture832_2003534b:
        MOV EAX,[pm]
        MOV EDX,dword ptr [EAX + posture832pm0]
        MOV ECX,dword ptr [EDX + posture832ps3dc]
        MOV dword ptr [EAX + posture832pmf4],ECX
        MOV EAX,[pm]
        MOV EDX,dword ptr [EAX + posture832pm0]
        MOV ECX,dword ptr [EDX + posture832ps3e0]
        MOV dword ptr [EAX + posture832pmf8],ECX
        MOV EAX,[pm]
        MOV EDX,dword ptr [EAX + posture832pm0]
        MOV ECX,dword ptr [EDX + posture832ps3e8]
        MOV dword ptr [EAX + posture832pm100],ECX
        MOV EAX,[pm]
        MOV EDX,dword ptr [EAX + posture832pm0]
        MOV ECX,dword ptr [EDX + posture832ps3ec]
        MOV dword ptr [EAX + posture832pm104],ECX
        MOV EAX,[pm]
        MOV EDX,dword ptr [EAX + posture832pm0]
        MOV ECX,dword ptr [EDX + posture832ps3e4]
        MOV dword ptr [EAX + posture832pmfc],ECX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ECX + posture832pm0]
        MOV EDX,dword ptr [EAX + posture832psf4]
        FLD dword ptr [EAX + posture832ps3f0]
        FSUB dword ptr [EAX + posture832ps3fc]
        TEST DH,0x2
        JZ posture832_200353f3
        FSUB dword ptr [posture832Const200ac790]
        POP EDI
        POP ESI
        POP EBX
        FSTP dword ptr [ECX + posture832pm108]
        MOV EDX,dword ptr [pm]
        MOV EAX,dword ptr [EDX + posture832pm0]
        MOV dword ptr [EAX + posture832psbc],0xfffffff6
        MOV EAX,0x1
        ADD ESP,0x68
        RET
posture832_200353f3:
        FSUB dword ptr [posture832Const200ac78c]
        POP EDI
        POP ESI
        MOV EAX,0x1
        POP EBX
        FSTP dword ptr [ECX + posture832pm108]
        MOV ECX,dword ptr [pm]
        MOV EDX,dword ptr [ECX + posture832pm0]
        MOV dword ptr [EDX + posture832psbc],0xfffffff8
        ADD ESP,0x68
        RET
posture832_2003541d:
        POP EDI
        POP ESI
        XOR EAX,EAX
        POP EBX
        ADD ESP,0x68
        RET
    }
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

#if defined(_MSC_VER) && defined(_M_IX86)
    {
        int *jumpGravity = &pm->ps->gravity;
        float *jumpVelocity = &pm->ps->velocity[2];
        float *jumpFrame = &pml.frametime;
        const float jumpZero = 0.0f;
        __asm {
            mov ecx, jumpGravity
            mov edx, jumpFrame
            fild dword ptr [ecx]
            fmul dword ptr [edx]
            mov ecx, jumpVelocity
            fsubr dword ptr [ecx]
            fstp dword ptr [ecx]
            fld dword ptr [ecx]
            fcomp jumpZero
            fnstsw ax
            and eax, 100h
            mov falling, eax
        }
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
static __declspec(naked) void PM_WaterMove(void) {
    __asm {
        sub esp, 0x1c
        call PM_CheckWaterJump
        test eax, eax
        je water830_200354a5
        call PM_WaterJumpMove
        add esp, 0x1c
        ret 
water830_200354a5:
        call PM_Friction
        mov eax, dword ptr [pm]
        add eax, waterCommand830
        push eax
        call PM_CmdScale
        fcom dword ptr [waterK200ac100]
        add esp, 4
        fnstsw ax
        test ah, 0x40
        je water830_200354e2
        fstp st(0)
        fld dword ptr [waterK200ac260]
        mov dword ptr [esp + 4], 0
        mov dword ptr [esp + 8], 0
        jmp water830_20035536
water830_200354e2:
        mov ecx, dword ptr [pm]
        movsx edx, byte ptr [ecx+waterRightCmd830]
        movsx eax, byte ptr [ecx+waterForwardCmd830]
        mov dword ptr [esp], edx
        fild dword ptr [esp]
        mov dword ptr [esp], eax
        xor eax, eax
        fild dword ptr [esp]
water830_20035502:
        fld st(1)
        fmul dword ptr [eax + pml+waterRight830]
        fld st(1)
        fmul dword ptr [eax + pml+waterForward830]
        add eax, 4
        cmp eax, 0xc
        faddp st(1), st(0)
        fmul st(0), st(3)
        fstp dword ptr [esp + eax]
        jl water830_20035502
        movsx ecx, byte ptr [ecx+waterUpCmd830]
        fstp st(0)
        fstp st(0)
        mov dword ptr [esp], ecx
        fimul dword ptr [esp]
        fadd dword ptr [esp + 0xc]
water830_20035536:
        mov edx, dword ptr [esp + 4]
        mov eax, dword ptr [esp + 8]
        fstp dword ptr [esp + 0x18]
        lea ecx, [esp + 0x10]
        mov dword ptr [esp + 0x10], edx
        push ecx
        mov dword ptr [esp + 0x18], eax
        call VectorNormalize
        mov eax, dword ptr [pm]
        add esp, 4
        fstp dword ptr [esp]
        cmp dword ptr [eax+waterType830], 0x10
        jne water830_2003559b
        mov edx, dword ptr [eax+waterPlayerState830]
        fild dword ptr [edx+waterSpeed830]
        fmul dword ptr [pm_slagSwimScale]
        fld dword ptr [esp]
        fcomp st(1)
        fnstsw ax
        test ah, 0x41
        jne water830_20035587
        fstp dword ptr [esp]
        jmp water830_20035589
water830_20035587:
        fstp st(0)
water830_20035589:
        mov eax, dword ptr [pm_slagaccelerate]
        mov ecx, dword ptr [esp]
        push eax
        lea edx, [esp + 0x14]
        push ecx
        push edx
        jmp water830_200355cc
water830_2003559b:
        mov eax, dword ptr [eax+waterPlayerState830]
        fild dword ptr [eax+waterSpeed830]
        fmul dword ptr [pm_waterSwimScale]
        fld dword ptr [esp]
        fcomp st(1)
        fnstsw ax
        test ah, 0x41
        jne water830_200355b9
        fstp dword ptr [esp]
        jmp water830_200355bb
water830_200355b9:
        fstp st(0)
water830_200355bb:
        mov ecx, dword ptr [pm_wateraccelerate]
        mov edx, dword ptr [esp]
        push ecx
        lea eax, [esp + 0x14]
        push edx
        push eax
water830_200355cc:
        call PM_Accelerate
        mov eax, dword ptr [pml+waterGround830]
        add esp, 0xc
        test eax, eax
        je water830_20035687
        mov ecx, dword ptr [pm]
        fld dword ptr [pml+waterNormal830+8]
        mov eax, dword ptr [ecx+waterPlayerState830]
        fmul dword ptr [eax+waterVelocity830+8]
        fld dword ptr [pml+waterNormal830+4]
        fmul dword ptr [eax+waterVelocity830+4]
        lea ecx, [eax+waterVelocity830]
        faddp st(1), st(0)
        fld dword ptr [pml+waterNormal830]
        fmul dword ptr [ecx]
        faddp st(1), st(0)
        fcomp dword ptr [waterK200ac100]
        fnstsw ax
        test ah, 1
        je water830_20035687
        push ecx
        call VectorLength
        mov edx, dword ptr [pm]
        push 0x3f8020c5
        fstp dword ptr [esp + 8]
        mov eax, dword ptr [edx+waterPlayerState830]
        add eax, waterVelocity830
        push eax
        push offset pml+waterNormal830
        push eax
        call PM_ClipVelocity
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+waterPlayerState830]
        add ecx, waterVelocity830
        push ecx
        call VectorNormalize
        mov edx, dword ptr [pm]
        add esp, 0x18
        fstp st(0)
        mov eax, dword ptr [edx+waterPlayerState830]
        fld dword ptr [esp]
        fmul dword ptr [eax+waterVelocity830]
        fstp dword ptr [eax+waterVelocity830]
        mov eax, dword ptr [pm]
        fld dword ptr [esp]
        mov eax, dword ptr [eax+waterPlayerState830]
        fmul dword ptr [eax+waterVelocity830+4]
        fstp dword ptr [eax+waterVelocity830+4]
        mov ecx, dword ptr [pm]
        fld dword ptr [esp]
        mov eax, dword ptr [ecx+waterPlayerState830]
        fmul dword ptr [eax+waterVelocity830+8]
        fstp dword ptr [eax+waterVelocity830+8]
water830_20035687:
        push 0
        call PM_SlideMove
        add esp, 4
        add esp, 0x1c
        ret 
    }
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
static const float flyZero825 = 0.0f;
enum { flyCmd825 = offsetof(pmove_t,cmd), flyForwardCmd825 = offsetof(pmove_t,cmd)+offsetof(usercmd_t,forwardmove), flyRightCmd825 = offsetof(pmove_t,cmd)+offsetof(usercmd_t,rightmove), flyUpCmd825 = offsetof(pmove_t,cmd)+offsetof(usercmd_t,upmove), flyForward825 = offsetof(pml_t,forward), flyRight825 = offsetof(pml_t,right) };
static __declspec(naked) void PM_FlyMove(void) {
    __asm {
        sub esp, 01ch
        call PM_Friction
        mov eax, dword ptr [pm]
        add eax, flyCmd825
        push eax
        call PM_CmdScale
        fcom dword ptr [flyZero825]
        add esp, 4
        fnstsw ax
        test ah, 040h
        je flyL20035840
        fstp st(0)
        fld dword ptr [flyZero825]
        mov dword ptr [esp + 4], 0
        mov dword ptr [esp + 8], 0
        jmp flyL20035894
    flyL20035840:
        mov ecx, dword ptr [pm]
        movsx edx, byte ptr [ecx + flyRightCmd825]
        movsx eax, byte ptr [ecx + flyForwardCmd825]
        mov dword ptr [esp], edx
        fild dword ptr [esp]
        mov dword ptr [esp], eax
        xor eax, eax
        fild dword ptr [esp]
    flyL20035860:
        fld st(1)
        fmul dword ptr [eax + pml + flyRight825]
        fld st(1)
        fmul dword ptr [eax + pml + flyForward825]
        add eax, 4
        cmp eax, 0ch
        faddp st(1), st(0)
        fmul st(0), st(3)
        fstp dword ptr [esp + eax]
        jl flyL20035860
        movsx ecx, byte ptr [ecx + flyUpCmd825]
        fstp st(0)
        fstp st(0)
        mov dword ptr [esp], ecx
        fimul dword ptr [esp]
        fadd dword ptr [esp + 0ch]
    flyL20035894:
        mov edx, dword ptr [esp + 4]
        mov eax, dword ptr [esp + 8]
        fstp dword ptr [esp + 018h]
        lea ecx, [esp + 010h]
        mov dword ptr [esp + 010h], edx
        push ecx
        mov dword ptr [esp + 018h], eax
        call VectorNormalize
        fstp dword ptr [esp + 4]
        mov edx, dword ptr [pm_flyaccelerate]
        mov eax, dword ptr [esp + 4]
        push edx
        lea ecx, [esp + 018h]
        push eax
        push ecx
        call PM_Accelerate
        push 0
        call PM_StepSlideMove
        add esp, 030h
        ret 
    }
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
static __declspec(naked) void PM_AirMove(void) {
    __asm {
        sub esp, 0x40
        mov eax, dword ptr [pm]
        push esi
        push edi
        mov ecx, dword ptr [eax + movePlayerState827]
        mov dword ptr [ecx + moveInstability827], 0x3e8
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx + movePlayerState827]
        or dword ptr [eax + moveFlags827], 4
        call PM_Friction
        mov eax, dword ptr [pm]
        movsx ecx, byte ptr [eax + moveForwardCmd827]
        movsx edx, byte ptr [eax + moveRightCmd827]
        mov dword ptr [esp + 0x10], ecx
        mov ecx, dword ptr [eax + moveExtension827]
        fild dword ptr [esp + 0x10]
        mov edi, dword ptr [ecx + moveDodgeTime827]
        lea esi, [eax + moveCommand827]
        mov dword ptr [esp + 0x10], edx
        fstp dword ptr [esp + 0xc]
        mov edx, dword ptr [esi]
        fild dword ptr [esp + 0x10]
        sub edx, edi
        cmp edx, 0x15e
        fstp dword ptr [esp + 8]
        jge move827_20035983
        mov dword ptr [pml+moveForward827+8], 0
        mov eax, dword ptr [eax + moveExtension827]
        mov dword ptr [esp + 0xc], 0
        mov ecx, dword ptr [eax + moveDt827]
        dec ecx
        neg ecx
        sbb ecx, ecx
        and ecx, 0x102c
        add ecx, 0xfffff7ea
        mov dword ptr [esp + 0x10], ecx
        fild dword ptr [esp + 0x10]
        mov dword ptr [esp + 0x10], 0x3f800000
        fstp dword ptr [esp + 8]
        jmp move827_200359b3
move827_20035983:
        mov ecx, 7
        lea edi, [esp + 0x2c]
        lea edx, [esp + 0x2c]
        rep movsd 
        push edx
        call PM_CmdScale
        fstp dword ptr [esp + 0x14]
        add esp, 4
        mov dword ptr [pml+moveForward827+8], 0
        mov dword ptr [pml+moveRight827+8], 0
move827_200359b3:
        push offset pml+moveForward827
        call VectorNormalize
        fstp st(0)
        push offset pml+moveRight827
        call VectorNormalize
        add esp, 8
        xor eax, eax
        fstp st(0)
        pop edi
        pop esi
move827_200359d2:
        fld dword ptr [esp]
        fmul dword ptr [eax + pml+moveRight827]
        fld dword ptr [esp + 4]
        fmul dword ptr [eax + pml+moveForward827]
        add eax, 4
        cmp eax, 8
        faddp st(1), st(0)
        fstp dword ptr [esp + eax + 0x14]
        jl move827_200359d2
        mov eax, dword ptr [esp + 0x18]
        mov ecx, dword ptr [esp + 0x1c]
        lea edx, [esp + 0xc]
        mov dword ptr [esp + 0xc], eax
        push edx
        mov dword ptr [esp + 0x14], ecx
        mov dword ptr [esp + 0x18], 0
        call VectorNormalize
        mov eax, dword ptr [pm_airaccelerate]
        lea edx, [esp + 0x10]
        fmul dword ptr [esp + 0xc]
        push eax
        fstp dword ptr [esp + 0x10]
        mov ecx, dword ptr [esp + 0x10]
        push ecx
        push edx
        call PM_Accelerate
        mov eax, dword ptr [pml+moveGround827]
        add esp, 0x10
        test eax, eax
        je move827_20035a5d
        mov eax, dword ptr [pm]
        push 0x3f8020c5
        mov eax, dword ptr [eax + movePlayerState827]
        add eax, moveVelocity827
        push eax
        push offset pml+moveNormal827
        push eax
        call PM_ClipVelocity
        add esp, 0x10
move827_20035a5d:
        mov ecx, dword ptr [pm]
        push 1
        mov edx, dword ptr [ecx + movePlayerState827]
        test dword ptr [edx + moveEFlags827], 0x80000
        je move827_20035a81
        call PM_StepSlideMoveProne
        add esp, 4
        call PM_SetMovementDir
        add esp, 0x40
        ret 
move827_20035a81:
        call PM_StepSlideMove
        add esp, 4
        call PM_SetMovementDir
        add esp, 0x40
        ret 
    }
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

	pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
	pm->ps->stats[STAT_TCE_FLAGS] |= 4;
	PM_Friction();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;

	// project moves down to flat plane
	if( pm->cmd.serverTime - pm->pmext->dodgeTime < 350 ) {
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
/* TC WalkMove 20035bf0: retained x87 schedule and native field bindings. */
#if defined(_MSC_VER) && defined(_M_IX86)
typedef char walk833CommandLayout[(sizeof(usercmd_t)==28 && offsetof(usercmd_t,serverTime)==0) ? 1 : -1];
static const unsigned int walk833Const200ac100 = 0x00000000u;
static const unsigned int walk833Const200ac130[2] = {0x00000000u, 0x3ff00000u};
static const unsigned int walk833Const200ac180 = 0x3f000000u;
static const unsigned int walk833Const200ac198 = 0x3e800000u;
static const unsigned int walk833Const200ac798[2] = {0x55555555u, 0x3fd55555u};
static const unsigned int walk833Const200ac7a0 = 0x3e785ce3u;
static const unsigned int walk833Const200ac7a8[2] = {0x00000000u, 0xc0100000u};
static const unsigned int walk833Const200ac7b0 = 0x3ee4faf6u;
static const unsigned int walk833Const200ac7b8[2] = {0x00000000u, 0xc0000000u};
static const unsigned int walk833Const200ac7c0 = 0x3f350bf3u;
static const unsigned int walk833Const200ac7c4 = 0x3f0e38e4u;
static const unsigned int walk833Const200ac7c8 = 0x3f200000u;
enum {
    walk833pml223cf460 = offsetof(pml_t, forward),
    walk833pml223cf464 = offsetof(pml_t, forward)+4,
    walk833pml223cf468 = offsetof(pml_t, forward)+8,
    walk833pml223cf46c = offsetof(pml_t, right),
    walk833pml223cf470 = offsetof(pml_t, right)+4,
    walk833pml223cf474 = offsetof(pml_t, right)+8,
    walk833pml223cf484 = offsetof(pml_t, frametime),
    walk833pml223cf4ac = offsetof(pml_t, groundTrace)+offsetof(trace_t,plane)+offsetof(cplane_t,normal),
    walk833pml223cf4b0 = offsetof(pml_t, groundTrace)+offsetof(trace_t,plane)+offsetof(cplane_t,normal)+4,
    walk833pml223cf4b4 = offsetof(pml_t, groundTrace)+offsetof(trace_t,plane)+offsetof(cplane_t,normal)+8,
    walk833pml223cf4c0 = offsetof(pml_t, groundTrace)+offsetof(trace_t,surfaceFlags),
    walk833pml223cf4f0 = offsetof(pml_t, tceContentRestriction),
    walk833pml223cf4f4 = offsetof(pml_t, tceGroundExtension),
    walk833pm110 = offsetof(pmove_t, waterlevel),
    walk833pm4 = offsetof(pmove_t, pmext),
    walk833pmc = offsetof(pmove_t, cmd),
    walk833ext4 = offsetof(pmoveExt_t, jumpTime),
    walk833ext10 = offsetof(pmoveExt_t, sprintTime),
    walk833pm0 = offsetof(pmove_t, ps),
    walk833ps3b4 = offsetof(playerState_t, holdable)+9*sizeof(int),
    walk833ps47c = offsetof(playerState_t, jumpTime),
    walk833pm20 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,forwardmove),
    walk833pm21 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,rightmove),
    walk833ps68 = offsetof(playerState_t, eFlags),
    walk833psc = offsetof(playerState_t, pm_flags),
    walk833ps40 = offsetof(playerState_t, speed),
    walk833ps40c = offsetof(playerState_t, crouchSpeedScale),
    walk833pm10c = offsetof(pmove_t, watertype),
    walk833pm10 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,buttons),
    walk833ps3c = offsetof(playerState_t, leanf),
    walk833psf0 = offsetof(playerState_t, stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    walk833ps38 = offsetof(playerState_t, gravity),
    walk833ps28 = offsetof(playerState_t, velocity)+8,
    walk833psd0 = offsetof(playerState_t, stats)+STAT_HEALTH*sizeof(int),
    walk833ps20 = offsetof(playerState_t, velocity),
    walk833ps24 = offsetof(playerState_t, velocity)+4,
    walk833ext38 = offsetof(pmoveExt_t, proneGroundTime)
};
static __declspec(naked) void PM_WalkMove(void) {
    __asm {
        MOV EAX,[pm]
        SUB ESP,0x40
        MOV ECX,dword ptr [EAX + walk833pm110]
        PUSH EBX
        PUSH ESI
        CMP ECX,0x2
        PUSH EDI
        JLE walk833_20035c47
        FLD dword ptr [pml + walk833pml223cf4b4]
        FMUL dword ptr [pml + walk833pml223cf468]
        FLD dword ptr [pml + walk833pml223cf4b0]
        FMUL dword ptr [pml + walk833pml223cf464]
        FADDP ST(1),ST(0)
        FLD dword ptr [pml + walk833pml223cf4ac]
        FMUL dword ptr [pml + walk833pml223cf460]
        FADDP ST(1),ST(0)
        FCOMP dword ptr [walk833Const200ac100]
        FNSTSW AX
        TEST AH,0x41
        JNZ walk833_20035c47
        CALL PM_WaterMove
        POP EDI
        POP ESI
        POP EBX
        ADD ESP,0x40
        RET
walk833_20035c47:
        CALL PM_CheckJump
        TEST EAX,EAX
        JZ walk833_20035d10
        MOV ECX,dword ptr [pm]
        CMP dword ptr [ECX + walk833pm110],0x1
        JLE walk833_20035c6a
        CALL PM_WaterMove
        JMP walk833_20035c6f
walk833_20035c6a:
        CALL PM_AirMove
walk833_20035c6f:
        MOV EAX,[pm]
        MOV ESI,dword ptr [EAX + walk833pm4]
        MOV EDX,dword ptr [EAX + walk833pmc]
        SUB EDX,dword ptr [ESI + walk833ext4]
        CMP EDX,0x352
        JL walk833_20035cfe
        MOV ECX,dword ptr [ESI + walk833ext10]
        CMP ECX,0x5dc
        MOV dword ptr [ESP + 0xc],ECX
        JGE walk833_20035c9d
        MOV dword ptr [ESI + walk833ext10],0x0
        JMP walk833_20035cd3
walk833_20035c9d:
        MOV EAX,dword ptr [EAX + walk833pm0]
        MOV EAX,dword ptr [EAX + walk833ps3b4]
        SUB EAX,0x4
        JZ walk833_20035cc1
        FILD dword ptr [ESP + 0xc]
        DEC EAX
        JZ walk833_20035cb9
        FMUL dword ptr [walk833Const200ac7c8]
        JMP walk833_20035ccb
walk833_20035cb9:
        FMUL dword ptr [walk833Const200ac180]
        JMP walk833_20035ccb
walk833_20035cc1:
        FILD dword ptr [ESP + 0xc]
        FMUL dword ptr [walk833Const200ac7c4]
walk833_20035ccb:
        CALL PM_MovementTruncate827
        MOV dword ptr [ESI + walk833ext10],EAX
walk833_20035cd3:
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ECX + walk833pm4]
        MOV EDX,dword ptr [EAX + walk833ext10]
        TEST EDX,EDX
        JGE walk833_20035cf0
        MOV dword ptr [EAX + walk833ext10],0x0
        MOV ECX,dword ptr [pm]
walk833_20035cf0:
        MOV EDX,dword ptr [ECX + walk833pm4]
        MOV EAX,dword ptr [ECX + walk833pmc]
        MOV dword ptr [EDX + walk833ext4],EAX
        MOV EAX,[pm]
walk833_20035cfe:
        MOV ECX,dword ptr [EAX + walk833pm0]
        MOV EDX,dword ptr [EAX + walk833pmc]
        POP EDI
        POP ESI
        MOV dword ptr [ECX + walk833ps47c],EDX
        POP EBX
        ADD ESP,0x40
        RET
walk833_20035d10:
        MOV EAX,[pm]
        MOV EBX,0x1
        CMP dword ptr [EAX + walk833pm110],EBX
        JG walk833_20035d37
        CALL PM_CheckDodge
        TEST EAX,EAX
        JZ walk833_20035d37
        CALL PM_AirMove
        POP EDI
        POP ESI
        POP EBX
        ADD ESP,0x40
        RET
walk833_20035d37:
        CALL PM_Friction
        MOV EAX,[pm]
        LEA EDI,[ESP + 0x30]
        MOVSX ECX,byte ptr [EAX + walk833pm20]
        MOVSX EDX,byte ptr [EAX + walk833pm21]
        MOV dword ptr [ESP + 0x10],ECX
        LEA ESI,[EAX + walk833pmc]
        FILD dword ptr [ESP + 0x10]
        MOV dword ptr [ESP + 0x10],EDX
        MOV ECX,0x7
        LEA EAX,[ESP + 0x30]
        FSTP dword ptr [ESP + 0xc]
        FILD dword ptr [ESP + 0x10]
        REP MOVSD
        FSTP dword ptr [ESP + 0x10]
        PUSH EAX
        CALL PM_CmdScale
        MOV EAX,[pml + walk833pml223cf4f0]
        ADD ESP,0x4
        FSTP dword ptr [ESP + 0x14]
        TEST EAX,EAX
        MOV dword ptr [pml + walk833pml223cf468],0x0
        MOV dword ptr [pml + walk833pml223cf474],0x0
        JZ walk833_20035da6
        MOV EAX,[pml + walk833pml223cf4f4]
        TEST EAX,EAX
        JNZ walk833_20035ddb
walk833_20035da6:
        PUSH 0x3f8020c5
        PUSH OFFSET pml + walk833pml223cf460
        PUSH OFFSET pml + walk833pml223cf4ac
        PUSH OFFSET pml + walk833pml223cf460
        CALL PM_ClipVelocity
        PUSH 0x3f8020c5
        PUSH OFFSET pml + walk833pml223cf46c
        PUSH OFFSET pml + walk833pml223cf4ac
        PUSH OFFSET pml + walk833pml223cf46c
        CALL PM_ClipVelocity
        ADD ESP,0x20
walk833_20035ddb:
        PUSH OFFSET pml + walk833pml223cf460
        CALL VectorNormalize
        FSTP ST(0)
        PUSH OFFSET pml + walk833pml223cf46c
        CALL VectorNormalize
        ADD ESP,0x8
        XOR EAX,EAX
        FSTP ST(0)
walk833_20035df8:
        FLD dword ptr [ESP + 0x10]
        FMUL dword ptr [EAX + pml + walk833pml223cf46c]
        FLD dword ptr [ESP + 0xc]
        FMUL dword ptr [EAX + pml + walk833pml223cf460]
        ADD EAX,0x4
        CMP EAX,0xc
        FADDP ST(1),ST(0)
        FSTP dword ptr [ESP + EAX*0x1 + 0x14]
        JL walk833_20035df8
        MOV EAX,[pml + walk833pml223cf4f4]
        TEST EAX,EAX
        JZ walk833_20035edb
        MOV EAX,[pml + walk833pml223cf4f0]
        CMP EAX,EBX
        JNZ walk833_20035e65
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [ESP + 0x1c]
        FLD dword ptr [ESP + 0x18]
        FMUL dword ptr [ESP + 0x18]
        FADDP ST(1),ST(0)
        FSQRT
        FCHS
        FMUL dword ptr [walk833Const200ac7c0]
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [walk833Const200ac7c0]
        FLD dword ptr [ESP + 0x18]
        FMUL dword ptr [walk833Const200ac7c0]
        JMP walk833_20035ee7
walk833_20035e65:
        CMP EAX,0x2
        JNZ walk833_20035ea0
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [ESP + 0x1c]
        FLD dword ptr [ESP + 0x18]
        FMUL dword ptr [ESP + 0x18]
        FADDP ST(1),ST(0)
        FSQRT
        FMUL qword ptr [walk833Const200ac7b8]
        FMUL dword ptr [walk833Const200ac7b0]
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [walk833Const200ac7b0]
        FLD dword ptr [ESP + 0x18]
        FMUL dword ptr [walk833Const200ac7b0]
        JMP walk833_20035ee7
walk833_20035ea0:
        CMP EAX,0x3
        JNZ walk833_20035edb
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [ESP + 0x1c]
        FLD dword ptr [ESP + 0x18]
        FMUL dword ptr [ESP + 0x18]
        FADDP ST(1),ST(0)
        FSQRT
        FMUL qword ptr [walk833Const200ac7a8]
        FMUL dword ptr [walk833Const200ac7a0]
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [walk833Const200ac7a0]
        FLD dword ptr [ESP + 0x18]
        FMUL dword ptr [walk833Const200ac7a0]
        JMP walk833_20035ee7
walk833_20035edb:
        FLD dword ptr [ESP + 0x20]
        FLD dword ptr [ESP + 0x1c]
        FLD dword ptr [ESP + 0x18]
walk833_20035ee7:
        FSTP dword ptr [ESP + 0x24]
        LEA ECX,[ESP + 0x24]
        FSTP dword ptr [ESP + 0x28]
        PUSH ECX
        FSTP dword ptr [ESP + 0x30]
        CALL VectorNormalize
        MOV EDX,dword ptr [pm]
        ADD ESP,0x4
        MOV ECX,dword ptr [EDX + walk833pm0]
        FMUL dword ptr [ESP + 0x14]
        TEST dword ptr [ECX + walk833ps68],0x180000
        FSTP dword ptr [ESP + 0xc]
        JNZ walk833_20035f29
        TEST byte ptr [ECX + walk833psc],BL
        JZ walk833_20035f47
        FILD dword ptr [ECX + walk833ps40]
        FMUL dword ptr [ECX + walk833ps40c]
        JMP walk833_20035f32
walk833_20035f29:
        FILD dword ptr [ECX + walk833ps40]
        FMUL dword ptr [pm_proneSpeedScale]
walk833_20035f32:
        FLD dword ptr [ESP + 0xc]
        FCOMP ST(1)
        FNSTSW AX
        TEST AH,0x41
        JNZ walk833_20035f45
        FSTP dword ptr [ESP + 0xc]
        JMP walk833_20035f47
walk833_20035f45:
        FSTP ST(0)
walk833_20035f47:
        MOV EAX,dword ptr [EDX + walk833pm110]
        MOV EBX,0x10
        TEST EAX,EAX
        MOV dword ptr [ESP + 0x14],EAX
        JZ walk833_20035fa2
        FILD dword ptr [ESP + 0x14]
        CMP dword ptr [EDX + walk833pm10c],EBX
        FMUL qword ptr [walk833Const200ac798]
        JNZ walk833_20035f74
        FLD dword ptr [pm_slagSwimScale]
        JMP walk833_20035f7a
walk833_20035f74:
        FLD dword ptr [pm_waterSwimScale]
walk833_20035f7a:
        FSUBR qword ptr [walk833Const200ac130]
        FXCH ST(1)
        FMULP ST(1),ST(0)
        FSUBR qword ptr [walk833Const200ac130]
        FIMUL dword ptr [ECX + walk833ps40]
        FLD dword ptr [ESP + 0xc]
        FCOMP ST(1)
        FNSTSW AX
        TEST AH,0x41
        JNZ walk833_20035fa0
        FSTP dword ptr [ESP + 0xc]
        JMP walk833_20035fa2
walk833_20035fa0:
        FSTP ST(0)
walk833_20035fa2:
        MOV EAX,[pml + walk833pml223cf4f0]
        TEST EAX,EAX
        JZ walk833_20035ff2
        TEST byte ptr [EDX + walk833pm10],BL
        JNZ walk833_20035fd4
        FLD dword ptr [ECX + walk833ps3c]
        FCOMP dword ptr [walk833Const200ac100]
        FNSTSW AX
        TEST AH,0x40
        JZ walk833_20035fd4
        TEST byte ptr [ECX + walk833psf0],0x4
        JNZ walk833_20035fd4
        FILD dword ptr [ECX + walk833ps40]
        FMUL dword ptr [walk833Const200ac180]
        JMP walk833_20035fdd
walk833_20035fd4:
        FILD dword ptr [ECX + walk833ps40]
        FMUL dword ptr [walk833Const200ac198]
walk833_20035fdd:
        FLD dword ptr [ESP + 0xc]
        FCOMP ST(1)
        FNSTSW AX
        TEST AH,0x41
        JNZ walk833_20035ff0
        FSTP dword ptr [ESP + 0xc]
        JMP walk833_20035ff2
walk833_20035ff0:
        FSTP ST(0)
walk833_20035ff2:
        MOV AL,[pml + walk833pml223cf4c0]
        MOV BL,0x40
        TEST AL,0x2
        JNZ walk833_2003600e
        TEST byte ptr [ECX + walk833psc],BL
        JNZ walk833_2003600e
        MOV EDX,dword ptr [pm_accelerate]
        MOV dword ptr [ESP + 0x10],EDX
        JMP walk833_20036017
walk833_2003600e:
        MOV EAX,[pm_airaccelerate]
        MOV dword ptr [ESP + 0x10],EAX
walk833_20036017:
        MOV ECX,dword ptr [ESP + 0x10]
        MOV EDX,dword ptr [ESP + 0xc]
        PUSH ECX
        LEA EAX,[ESP + 0x28]
        PUSH EDX
        PUSH EAX
        CALL PM_Accelerate
        MOV ECX,dword ptr [pml + walk833pml223cf4c0]
        MOV EAX,[pm]
        ADD ESP,0xc
        TEST CL,0x2
        JNZ walk833_20036045
        MOV EDX,dword ptr [EAX + walk833pm0]
        TEST byte ptr [EDX + walk833psc],BL
        JZ walk833_20036061
walk833_20036045:
        MOV EAX,dword ptr [EAX + walk833pm0]
        FILD dword ptr [EAX + walk833ps38]
        FMUL dword ptr [pml + walk833pml223cf484]
        FSUBR dword ptr [EAX + walk833ps28]
        FSTP dword ptr [EAX + walk833ps28]
        MOV EAX,[pm]
        MOV ECX,dword ptr [pml + walk833pml223cf4c0]
walk833_20036061:
        AND ECX,0xff000000
        CMP ECX,0xd000000
        JNZ walk833_20036086
        MOV ECX,dword ptr [EAX + walk833pm0]
        MOV EDX,dword ptr [ECX + walk833psd0]
        TEST EDX,EDX
        JLE walk833_20036086
        MOV EAX,dword ptr [ECX + walk833ps68]
        OR AH,0x1
        MOV dword ptr [ECX + walk833ps68],EAX
        JMP walk833_20036091
walk833_20036086:
        MOV EAX,dword ptr [EAX + walk833pm0]
        MOV ECX,dword ptr [EAX + walk833ps68]
        AND CH,0xfe
        MOV dword ptr [EAX + walk833ps68],ECX
walk833_20036091:
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + walk833pm0]
        ADD ECX,walk833ps20
        PUSH ECX
        CALL VectorLength
        MOV EAX,[pml + walk833pml223cf4f4]
        ADD ESP,0x4
        FSTP dword ptr [ESP + 0xc]
        TEST EAX,EAX
        JNZ walk833_200360d0
        MOV EDX,dword ptr [pm]
        PUSH 0x3f8020c5
        MOV EAX,dword ptr [EDX + walk833pm0]
        ADD EAX,walk833ps20
        PUSH EAX
        PUSH OFFSET pml + walk833pml223cf4ac
        PUSH EAX
        CALL PM_ClipVelocity
        ADD ESP,0x10
walk833_200360d0:
        MOV ECX,dword ptr [pm]
        MOV ESI,0x80000
        MOV EDX,dword ptr [ECX + walk833pm0]
        FLD dword ptr [EDX + walk833ps20]
        FCOMP dword ptr [walk833Const200ac100]
        FNSTSW AX
        TEST AH,0x40
        JZ walk833_2003611a
        FLD dword ptr [EDX + walk833ps24]
        FCOMP dword ptr [walk833Const200ac100]
        FNSTSW AX
        TEST AH,0x40
        JZ walk833_2003611a
        TEST dword ptr [EDX + walk833ps68],ESI
        JZ walk833_20036111
        MOV EAX,dword ptr [ECX + walk833pm4]
        MOV ECX,dword ptr [ECX + walk833pmc]
        MOV dword ptr [EAX + walk833ext38],ECX
        MOV ECX,dword ptr [pm]
walk833_20036111:
        MOV EAX,[pml + walk833pml223cf4f4]
        TEST EAX,EAX
        JZ walk833_2003618e
walk833_2003611a:
        MOV EDX,dword ptr [ECX + walk833pm0]
        ADD EDX,walk833ps20
        PUSH EDX
        CALL VectorNormalize
        MOV EAX,[pm]
        ADD ESP,0x4
        FSTP ST(0)
        MOV EAX,dword ptr [EAX + walk833pm0]
        PUSH 0x0
        FLD dword ptr [ESP + 0x10]
        FMUL dword ptr [EAX + walk833ps20]
        FSTP dword ptr [EAX + walk833ps20]
        MOV ECX,dword ptr [pm]
        FLD dword ptr [ESP + 0x10]
        MOV EAX,dword ptr [ECX + walk833pm0]
        FMUL dword ptr [EAX + walk833ps24]
        FSTP dword ptr [EAX + walk833ps24]
        MOV EDX,dword ptr [pm]
        FLD dword ptr [ESP + 0x10]
        MOV EAX,dword ptr [EDX + walk833pm0]
        FMUL dword ptr [EAX + walk833ps28]
        FSTP dword ptr [EAX + walk833ps28]
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + walk833pm0]
        TEST dword ptr [ECX + walk833ps68],ESI
        JZ walk833_20036181
        CALL PM_StepSlideMoveProne
        ADD ESP,0x4
        CALL PM_SetMovementDir
        POP EDI
        POP ESI
        POP EBX
        ADD ESP,0x40
        RET
walk833_20036181:
        CALL PM_StepSlideMove
        ADD ESP,0x4
        CALL PM_SetMovementDir
walk833_2003618e:
        POP EDI
        POP ESI
        POP EBX
        ADD ESP,0x40
        RET
    }
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

#if defined(_MSC_VER) && defined(_M_IX86)
    {
        float *deadVelocity = pm->ps->velocity;
        const float deadDrag = 20.0f, deadZero = 0.0f;
        __asm {
            push deadVelocity
            call VectorLength
            add esp, 4
            fsub deadDrag
            fst forward
            fcomp deadZero
            fnstsw ax
            and eax, 4100h
            mov stopDead, eax
        }
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
static __declspec(naked) void PM_NoclipMove(void) {
    __asm {

        MOV EAX,[pm]

        SUB ESP,0x20

        MOV ECX,dword ptr [EAX + movementPs833]

        MOV dword ptr [ECX + movementViewHeight833],0x2a

        MOV EDX,dword ptr [pm]

        MOV EAX,dword ptr [EDX + movementPs833]

        ADD EAX,movementVelocity833

        PUSH EAX

        CALL VectorLength

        FCOM dword ptr [movementK200ac110]

        ADD ESP,0x4

        FNSTSW AX

        TEST AH,0x1

        JZ movementAt20036679

        MOV ECX,dword ptr [pm]

        MOV EAX,[vec3_origin]

        FSTP ST(0)

        MOV EDX,dword ptr [ECX + movementPs833]

        MOV dword ptr [EDX + movementVelocity833],EAX

        MOV ECX,dword ptr [pm]

        MOV EAX,[vec3_origin + 4]

        MOV EDX,dword ptr [ECX + movementPs833]

        MOV dword ptr [EDX + movementVelocityY833],EAX

        MOV ECX,dword ptr [pm]

        MOV EAX,[vec3_origin + 8]

        MOV EDX,dword ptr [ECX + movementPs833]

        MOV dword ptr [EDX + movementVelocityZ833],EAX

        JMP movementAt200366f4
    movementAt20036679:
        FLD dword ptr [pm_friction]

        FMUL qword ptr [movementK200ac708]

        FLD ST(1)

        FCOMP dword ptr [pm_stopspeed]

        FNSTSW AX

        TEST AH,0x1

        JZ movementAt2003669c

        FLD dword ptr [pm_stopspeed]

        JMP movementAt2003669e
    movementAt2003669c:
        FLD ST(1)
    movementAt2003669e:
        FLD dword ptr [pml + movementFrameTime833]

        FMUL ST(0),ST(1)

        FMULP ST(2),ST(0)

        FXCH ST(1)

        FSUBR ST(0),ST(2)

        FXCH ST(1)

        FSTP ST(0)

        FCOM dword ptr [movementK200ac100]

        FNSTSW AX

        TEST AH,0x1

        JZ movementAt200366c5

        FSTP ST(0)

        FLD dword ptr [movementK200ac100]
    movementAt200366c5:
        FDIVRP ST(1),ST(0)

        MOV ECX,dword ptr [pm]

        MOV EAX,dword ptr [ECX + movementPs833]

        FLD ST(0)

        FMUL dword ptr [EAX + movementVelocity833]

        FSTP dword ptr [EAX + movementVelocity833]

        MOV EDX,dword ptr [pm]

        FLD ST(0)

        MOV EAX,dword ptr [EDX + movementPs833]

        FMUL dword ptr [EAX + movementVelocityY833]

        FSTP dword ptr [EAX + movementVelocityY833]

        MOV EAX,[pm]

        MOV EAX,dword ptr [EAX + movementPs833]

        FMUL dword ptr [EAX + movementVelocityZ833]

        FSTP dword ptr [EAX + movementVelocityZ833]
    movementAt200366f4:
        MOV ECX,dword ptr [pm]

        ADD ECX,movementCmd833

        PUSH ECX

        CALL PM_CmdScale

        MOV ECX,dword ptr [pm]

        ADD ESP,0x4

        FSTP dword ptr [ESP + 0x4]

        MOVSX EDX,byte ptr [ECX + movementForwardCmd833]

        MOVSX EAX,byte ptr [ECX + movementRightCmd833]

        MOV dword ptr [ESP],EDX

        FILD dword ptr [ESP]

        MOV dword ptr [ESP],EAX

        XOR EAX,EAX

        FILD dword ptr [ESP]
    movementAt2003672a:
        FLD ST(0)

        FMUL dword ptr [EAX + pml + movementRight833]

        FLD ST(2)

        FMUL dword ptr [EAX + pml + movementForward833]

        ADD EAX,0x4

        CMP EAX,0xc

        FADDP ST(1),ST(0)

        FSTP dword ptr [ESP + EAX*0x1 + 0x10]

        JL movementAt2003672a

        MOVSX ECX,byte ptr [ECX + movementUpCmd833]

        FSTP ST(0)

        FSTP ST(0)

        MOV dword ptr [ESP],ECX

        MOV EDX,dword ptr [ESP + 0x14]

        FILD dword ptr [ESP]

        MOV EAX,dword ptr [ESP + 0x18]

        LEA ECX,[ESP + 0x8]

        PUSH ECX

        MOV dword ptr [ESP + 0xc],EDX

        FADD dword ptr [ESP + 0x20]

        MOV dword ptr [ESP + 0x10],EAX

        FSTP dword ptr [ESP + 0x14]

        CALL VectorNormalize

        MOV EDX,dword ptr [pm_accelerate]

        LEA ECX,[ESP + 0xc]

        FMUL dword ptr [ESP + 0x8]

        PUSH EDX

        FSTP dword ptr [ESP + 0xc]

        MOV EAX,dword ptr [ESP + 0xc]

        PUSH EAX

        PUSH ECX

        CALL PM_Accelerate

        MOV EDX,dword ptr [pm]

        FLD dword ptr [pml + movementFrameTime833]

        MOV EAX,dword ptr [EDX + movementPs833]

        FMUL dword ptr [EAX + movementVelocity833]

        FADD dword ptr [EAX + movementOrigin833]

        FSTP dword ptr [EAX + movementOrigin833]

        MOV EAX,[pm]

        FLD dword ptr [pml + movementFrameTime833]

        MOV EAX,dword ptr [EAX + movementPs833]

        FMUL dword ptr [EAX + movementVelocityY833]

        FADD dword ptr [EAX + movementOriginY833]

        FSTP dword ptr [EAX + movementOriginY833]

        MOV ECX,dword ptr [pm]

        FLD dword ptr [pml + movementFrameTime833]

        MOV EAX,dword ptr [ECX + movementPs833]

        FMUL dword ptr [EAX + movementVelocityZ833]

        FADD dword ptr [EAX + movementOriginZ833]

        FSTP dword ptr [EAX + movementOriginZ833]

        ADD ESP,0x30

        RET
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
/* TC ground/landing controllers: original Windows flow, native layouts. */
static const float groundK200ac180 = 0.5;
static const float groundK200ac2bc = 4.0;
static const float groundK200ac100 = 0.0;
static const float groundK200ac704 = 0.800000011920929;
static const double groundK200ac368 = 0.0001;
static const double groundK200ac170 = 0.25;
static const double groundK200ac128 = 0.5;
static const float groundK200ac110 = 1.0;
static const float groundK200ac38c = 75.0;
static const float groundK200ac7e8 = 56.0;
static const float groundK200ac208 = 48.0;
static const float groundK200ac7e4 = 44.0;
static const float groundK200ac2b0 = 40.0;
static const float groundK200ac7e0 = 38.0;
static const float groundK200ac7dc = 13.0;
static const float groundK200ac7d8 = -220.0;
static const float groundK200ac1cc = 64.0;
static const double groundK200ac130 = 1.0;
static const float groundK200ac198 = 0.25;
static const float groundK200ac2b8 = 10.0;
static const float groundK200ac148 = 12.0;
static const double groundK200ac268 = 0.7;
static const float groundK200ac2dc = 80.0;
static const float groundK200ac7d4 = -200.0;
static const char groundS200bc610[] = "delta: %5.2f\n";
static const char groundS200bc620[] = "%i:allsolid\n";
static const char groundS200bc630[] = "%i:lift\n";
static const char groundS200bc604[] = "%i:kickoff\n";
static const char groundS200bc5f8[] = "%i:steep\n";
static const char groundS200bc5ec[] = "%i:Land\n";
enum {
    groundPs831=offsetof(pmove_t,ps),
    groundExt831=offsetof(pmove_t,pmext),
    groundCharacter831=offsetof(pmove_t,character),
    groundCommandTime831=offsetof(pmove_t,cmd)+offsetof(usercmd_t,serverTime),
    groundForwardCmd831=offsetof(pmove_t,cmd)+offsetof(usercmd_t,forwardmove),
    groundTraceMask831=offsetof(pmove_t,tracemask),
    groundDebug831=offsetof(pmove_t,debugLevel),
    groundMins831=offsetof(pmove_t,mins),
    groundMaxs831=offsetof(pmove_t,maxs),
    groundTrace831=offsetof(pmove_t,trace),
    groundWater831=offsetof(pmove_t,waterlevel),
    groundOrigin831=offsetof(playerState_t,origin),
    groundVelocity831=offsetof(playerState_t,velocity),
    groundFlags831=offsetof(playerState_t,pm_flags),
    groundTime831=offsetof(playerState_t,pm_time),
    groundEflags831=offsetof(playerState_t,eFlags),
    groundClient831=offsetof(playerState_t,clientNum),
    groundGround831=offsetof(playerState_t,groundEntityNum),
    groundGravity831=offsetof(playerState_t,gravity),
    groundLegsTimer831=offsetof(playerState_t,legsTimer),
    groundHealth831=offsetof(playerState_t,stats)+STAT_HEALTH*sizeof(int),
    groundWeaponFlags831=offsetof(playerState_t,stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    groundTcFlags831=offsetof(playerState_t,stats)+STAT_TCE_FLAGS*sizeof(int),
    groundJumpTime831=offsetof(playerState_t,jumpTime),
    groundBob831=offsetof(playerState_t,bobCycle),
    groundExtProne831=offsetof(pmoveExt_t,proneTime),
    groundExtJump831=offsetof(pmoveExt_t,jumpTime),
    groundAnimModel831=offsetof(bg_character_t,animModelInfo),
    groundWalking831=offsetof(pml_t,walking),
    groundGroundPlane831=offsetof(pml_t,groundPlane),
    groundGroundTrace831=offsetof(pml_t,groundTrace),
    groundSurface831=offsetof(pml_t,groundTrace)+offsetof(trace_t,surfaceFlags),
    groundOldZ831=offsetof(pml_t,previous_origin)+2*sizeof(float),
    groundOldVelZ831=offsetof(pml_t,previous_velocity)+2*sizeof(float),
    groundContent831=offsetof(pml_t,tceContentRestriction),
    groundExtension831=offsetof(pml_t,tceGroundExtension)
};
typedef char groundTraceLayout831[(sizeof(trace_t)==56 && offsetof(trace_t,allsolid)==0 && offsetof(trace_t,fraction)==8 && offsetof(trace_t,plane)==24 && offsetof(trace_t,surfaceFlags)==44 && offsetof(trace_t,entityNum)==52) ? 1 : -1];
typedef char groundProtocol831[(EF_MG42_ACTIVE==0x20 && EF_AAGUN_ACTIVE==0x400000 && EF_MOUNTEDTANK==0x8000 && EF_PRONE==0x80000 && PMF_BACKWARDS_JUMP==8 && PMF_TIME_WATERJUMP==0x100 && PMF_TIME_LAND==0x20 && ENTITYNUM_NONE==1023 && ANIM_ET_JUMP==4 && ANIM_ET_JUMPBK==5 && ANIM_ET_LAND==6) ? 1 : -1];
#endif
/* Whole TC3000e9c0: steep landings suppress the small landing sounds. */
#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) void PM_CrashLand(qboolean steep) {
    __asm {
        push ecx
        mov edx, dword ptr [pm]
        push esi
        fld dword ptr [pml+groundOldVelZ831]
        mov ecx, dword ptr [edx+groundPs831]
        mov eax, dword ptr [ecx+groundGravity831]
        neg eax
        mov dword ptr [esp + 4], eax
        fild dword ptr [esp + 4]
        fld st(0)
        fmul dword ptr [groundK200ac180]
        fld st(2)
        fmul st(0), st(3)
        fld dword ptr [ecx+groundOrigin831+8]
        fsub dword ptr [pml+groundOldZ831]
        fchs 
        fmul st(0), st(2)
        fmul dword ptr [groundK200ac2bc]
        fsubp st(1), st(0)
        fcom dword ptr [groundK200ac100]
        fnstsw ax
        test ah, 1
        jne ground831_20036fad
        fld st(3)
        fchs 
        fxch st(1)
        fsqrt 
        mov eax, dword ptr [ecx+groundTcFlags831]
        test ah, 2
        fsubp st(1), st(0)
        fxch st(1)
        fadd st(0), st(0)
        fdivp st(1), st(0)
        fmul st(0), st(1)
        faddp st(2), st(0)
        fstp st(0)
        je ground831_20036d86
        fmul dword ptr [groundK200ac704]
ground831_20036d86:
        mov eax, dword ptr [edx+groundWater831]
        fld st(0)
        fmulp st(1), st(0)
        cmp eax, 3
        fmul qword ptr [groundK200ac368]
        je ground831_20036fb3
        cmp eax, 2
        jne ground831_20036daa
        fmul qword ptr [groundK200ac170]
ground831_20036daa:
        cmp eax, 1
        jne ground831_20036db5
        fmul qword ptr [groundK200ac128]
ground831_20036db5:
        fcom dword ptr [groundK200ac110]
        fnstsw ax
        test ah, 1
        jne ground831_20036fb3
        mov al, byte ptr [pml+groundSurface831]
        xor esi, esi
        fadd st(0), st(0)
        push edi
        mov edi, dword ptr [esp + 0x10]
        test al, 1
        fstp dword ptr [esp + 8]
        jne ground831_20036f24
        cmp dword ptr [edx+groundDebug831], esi
        je ground831_20036dfc
        fld dword ptr [esp + 8]
        sub esp, 8
        fstp qword ptr [esp]
        push offset groundS200bc610
        call Com_Printf
        add esp, 0xc
ground831_20036dfc:
        fld dword ptr [esp + 8]
        fcomp dword ptr [groundK200ac38c]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036e1a
        call PM_FootstepForSurface
        push eax
        push EV_FALL_NDIE
        jmp ground831_20036f1c
ground831_20036e1a:
        fld dword ptr [esp + 8]
        fcomp dword ptr [groundK200ac7e8]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036e3b
        call PM_FootstepForSurface
        push eax
        push EV_TCE_FALL_DMG_75
        jmp ground831_20036f1c
ground831_20036e3b:
        fld dword ptr [esp + 8]
        fcomp dword ptr [groundK200ac208]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036e59
        call PM_FootstepForSurface
        push eax
        push EV_FALL_DMG_50
        jmp ground831_20036f1c
ground831_20036e59:
        fld dword ptr [esp + 8]
        fcomp dword ptr [groundK200ac7e4]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036e8b
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx+groundPs831]
        cmp dword ptr [edx+groundHealth831], esi
        jle ground831_20036f24
        call PM_FootstepForSurface
        push eax
        push EV_FALL_DMG_25
        jmp ground831_20036f1c
ground831_20036e8b:
        fld dword ptr [esp + 8]
        fcomp dword ptr [groundK200ac2b0]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036eb5
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundPs831]
        cmp dword ptr [ecx+groundHealth831], esi
        jle ground831_20036f24
        call PM_FootstepForSurface
        push eax
        push EV_FALL_DMG_15
        jmp ground831_20036f1c
ground831_20036eb5:
        fld dword ptr [esp + 8]
        fcomp dword ptr [groundK200ac7e0]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036ee0
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx+groundPs831]
        cmp dword ptr [eax+groundHealth831], esi
        jle ground831_20036f24
        call PM_FootstepForSurface
        push eax
        push EV_FALL_DMG_10
        jmp ground831_20036f1c
ground831_20036ee0:
        fld dword ptr [esp + 8]
        fcomp dword ptr [groundK200ac7dc]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036eff
        cmp edi, esi
        jne ground831_20036eff
        call PM_FootstepForSurface
        push eax
        push EV_FALL_SHORT
        jmp ground831_20036f1c
ground831_20036eff:
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx+groundPs831]
        test byte ptr [edx+groundWeaponFlags831], 2
        jne ground831_20036f24
        cmp edi, esi
        jne ground831_20036f24
        call PM_FootstepForSurface
        push eax
        push EV_FOOTSTEP
ground831_20036f1c:
        call PM_AddEventExt
        add esp, 8
ground831_20036f24:
        fld dword ptr [esp + 8]
        fcomp dword ptr [groundK200ac7e0]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036f55
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundPs831]
        mov dword ptr [ecx+groundVelocity831+8], esi
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx+groundPs831]
        mov dword ptr [eax+groundVelocity831+4], esi
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx+groundPs831]
        mov dword ptr [edx+groundVelocity831], esi
ground831_20036f55:
        cmp edi, esi
        pop edi
        je ground831_20036f6b
        fld dword ptr [esp + 4]
        fcomp dword ptr [groundK200ac7e0]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036fb5
ground831_20036f6b:
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx+groundPs831]
        cmp dword ptr [ecx+groundLegsTimer831], esi
        jne ground831_20036fa0
        fld dword ptr [pml+groundOldVelZ831]
        fcomp dword ptr [groundK200ac7d8]
        fnstsw ax
        test ah, 1
        je ground831_20036fa0
        mov eax, dword ptr [edx+groundCharacter831]
        push 1
        push esi
        push 6
        mov edx, dword ptr [eax+groundAnimModel831]
        push edx
        push ecx
        call BG_AnimScriptEvent
        add esp, 0x14
ground831_20036fa0:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundPs831]
        mov dword ptr [ecx+groundBob831], esi
        pop esi
        pop ecx
        ret 
ground831_20036fad:
        fstp st(0)
        fstp st(0)
        fstp st(0)
ground831_20036fb3:
        fstp st(0)
ground831_20036fb5:
        pop esi
        pop ecx
        ret 
    }
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
static __declspec(naked) int PM_CorrectAllSolid(trace_t *trace) {
    __asm {
        mov eax, dword ptr [pm]
        sub esp, 0x14
        mov ecx, dword ptr [eax+groundDebug831]
        test ecx, ecx
        je ground831_20037037
        mov eax, dword ptr [c_pmove]
        push eax
        push offset groundS200bc620
        call Com_Printf
        mov eax, dword ptr [pm]
        add esp, 8
ground831_20037037:
        push ebx
        push ebp
        or ebp, 0xffffffff
        or ebx, 0xffffffff
        mov dword ptr [esp + 0xc], ebp
        mov dword ptr [esp + 8], ebx
        fild dword ptr [esp + 0xc]
        push esi
        mov esi, dword ptr [esp + 0x24]
        push edi
        or edi, 0xffffffff
        fstp dword ptr [esp + 0x14]
        fild dword ptr [esp + 0x10]
        mov dword ptr [esp + 0x28], edi
        fstp dword ptr [esp + 0x10]
        jmp ground831_200370a6
ground831_20037066:
        fild dword ptr [esp + 0x14]
        mov eax, dword ptr [pm]
        or ebx, 0xffffffff
        mov dword ptr [esp + 0x10], ebx
        or edi, 0xffffffff
        fstp dword ptr [esp + 0x14]
        fild dword ptr [esp + 0x10]
        mov dword ptr [esp + 0x28], edi
        fstp dword ptr [esp + 0x10]
        jmp ground831_200370a6
ground831_2003708b:
        fild dword ptr [esp + 0x10]
        mov eax, dword ptr [pm]
        or edi, 0xffffffff
        mov dword ptr [esp + 0x28], edi
        fstp dword ptr [esp + 0x10]
        jmp ground831_200370a6
ground831_200370a1:
        mov eax, dword ptr [pm]
ground831_200370a6:
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [ecx+groundOrigin831]
        mov dword ptr [esp + 0x18], edx
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [ecx+groundOrigin831+4]
        lea ecx, [esp + 0x18]
        mov dword ptr [esp + 0x1c], edx
        mov eax, dword ptr [eax+groundPs831]
        lea edx, [esp + 0x18]
        push ecx
        fld dword ptr [eax+groundOrigin831+8]
        fld dword ptr [esp + 0x18]
        fadd dword ptr [esp + 0x1c]
        push edx
        push esi
        fstp dword ptr [esp + 0x24]
        fld dword ptr [esp + 0x1c]
        fadd dword ptr [esp + 0x28]
        fstp dword ptr [esp + 0x28]
        fild dword ptr [esp + 0x34]
        fadd st(0), st(1)
        fstp dword ptr [esp + 0x2c]
        fstp st(0)
        call PM_TraceAll
        mov eax, dword ptr [esi]
        add esp, 0xc
        test eax, eax
        je ground831_20037143
        inc edi
        cmp edi, 1
        mov dword ptr [esp + 0x28], edi
        jle ground831_200370a1
        inc ebx
        cmp ebx, 1
        mov dword ptr [esp + 0x10], ebx
        jle ground831_2003708b
        inc ebp
        cmp ebp, 1
        mov dword ptr [esp + 0x14], ebp
        jle ground831_20037066
        mov ecx, dword ptr [pm]
        pop edi
        xor eax, eax
        pop esi
        mov edx, dword ptr [ecx+groundPs831]
        pop ebp
        pop ebx
        mov dword ptr [edx+groundGround831], 0x3ff
        mov dword ptr [pml+groundGroundPlane831], eax
        mov dword ptr [pml+groundWalking831], eax
        add esp, 0x14
        ret 
ground831_20037143:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [ecx+groundOrigin831]
        mov dword ptr [esp + 0x18], edx
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [ecx+groundOrigin831+4]
        mov dword ptr [esp + 0x1c], edx
        mov ecx, dword ptr [eax+groundPs831]
        lea edx, [esp + 0x18]
        fld dword ptr [ecx+groundOrigin831+8]
        fsub qword ptr [groundK200ac170]
        push edx
        fstp dword ptr [esp + 0x24]
        mov eax, dword ptr [eax+groundPs831]
        add eax, groundOrigin831
        push eax
        push esi
        call PM_TraceAll
        mov ecx, 0xe
        mov edi, offset pml+groundGroundTrace831
        add esp, 0xc
        mov eax, 1
        rep movsd 
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x14
        ret 
    }
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
#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) void PM_GroundTraceMissed(void) {
    __asm {
        mov eax, dword ptr [pm]
        sub esp, 0x44
        mov ecx, dword ptr [eax+groundPs831]
        push ebx
        xor ebx, ebx
        cmp dword ptr [ecx+groundGround831], 0x3ff
        je ground831_20037298
        cmp dword ptr [eax+groundDebug831], ebx
        je ground831_200371d8
        mov edx, dword ptr [c_pmove]
        push edx
        push offset groundS200bc630
        call Com_Printf
        mov eax, dword ptr [pm]
        add esp, 8
ground831_200371d8:
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [ecx+groundOrigin831]
        mov dword ptr [esp + 4], edx
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [ecx+groundOrigin831+4]
        mov dword ptr [esp + 8], edx
        mov ecx, dword ptr [eax+groundPs831]
        fld dword ptr [ecx+groundOrigin831+8]
        fsub dword ptr [groundK200ac1cc]
        fstp dword ptr [esp + 0xc]
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [eax+groundTraceMask831]
        push edx
        mov edx, dword ptr [ecx+groundClient831]
        add ecx, groundOrigin831
        push edx
        lea edx, [esp + 0xc]
        push edx
        lea edx, [eax+groundMaxs831]
        push edx
        lea edx, [eax+groundMins831]
        push edx
        push ecx
        lea ecx, [esp + 0x28]
        push ecx
        call dword ptr [eax+groundTrace831]
        fld dword ptr [esp + 0x34]
        fcomp qword ptr [groundK200ac130]
        add esp, 0x1c
        fnstsw ax
        test ah, 0x40
        mov eax, dword ptr [pm]
        je ground831_20037298
        mov cl, byte ptr [eax+groundForwardCmd831]
        push 1
        cmp cl, bl
        push ebx
        jl ground831_2003726e
        mov edx, dword ptr [eax+groundCharacter831]
        push 4
        mov ecx, dword ptr [edx+groundAnimModel831]
        mov edx, dword ptr [eax+groundPs831]
        push ecx
        push edx
        call BG_AnimScriptEvent
        mov eax, dword ptr [pm]
        add esp, 0x14
        mov eax, dword ptr [eax+groundPs831]
        mov ecx, dword ptr [eax+groundFlags831]
        and ecx, 0xfffffff7
        jmp ground831_20037290
ground831_2003726e:
        mov ecx, dword ptr [eax+groundCharacter831]
        mov eax, dword ptr [eax+groundPs831]
        push 5
        mov edx, dword ptr [ecx+groundAnimModel831]
        push edx
        push eax
        call BG_AnimScriptEvent
        mov ecx, dword ptr [pm]
        add esp, 0x14
        mov eax, dword ptr [ecx+groundPs831]
        mov ecx, dword ptr [eax+groundFlags831]
        or ecx, 8
ground831_20037290:
        mov dword ptr [eax+groundFlags831], ecx
        mov eax, dword ptr [pm]
ground831_20037298:
        mov eax, dword ptr [eax+groundPs831]
        cmp dword ptr [eax+groundGround831], -1
        je ground831_200372a7
        mov dword ptr [eax+groundGround831], 0x3ff
ground831_200372a7:
        mov dword ptr [pml+groundGroundPlane831], ebx
        mov dword ptr [pml+groundWalking831], ebx
        pop ebx
        add esp, 0x44
        ret 
    }
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
/* Whole TC3000e490. Direct original capsule trace, including content and
 * prone ground-extension paths absent from the SDK ground controller. */
#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) void PM_GroundTrace(void) {
    __asm {
        sub esp, 0x44
        mov eax, dword ptr [pm]
        push ebp
        xor ebp, ebp
        push esi
        mov ecx, dword ptr [eax+groundPs831]
        push edi
        mov edx, dword ptr [ecx+groundOrigin831]
        mov dword ptr [esp + 0xc], edx
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [ecx+groundOrigin831+4]
        mov dword ptr [esp + 0x10], edx
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [ecx+groundEflags831]
        test dl, 0x20
        jne ground831_2003681c
        test edx, 0x400000
        jne ground831_2003681c
        fld dword ptr [ecx+groundOrigin831+8]
        fsub dword ptr [groundK200ac198]
        jmp ground831_20036825
ground831_2003681c:
        fld dword ptr [ecx+groundOrigin831+8]
        fsub dword ptr [groundK200ac110]
ground831_20036825:
        fstp dword ptr [esp + 0x14]
        mov ecx, dword ptr [eax+groundPs831]
        mov edx, dword ptr [eax+groundTraceMask831]
        push edx
        mov edx, dword ptr [ecx+groundClient831]
        add ecx, groundOrigin831
        push edx
        lea edx, [esp + 0x14]
        push edx
        lea edx, [eax+groundMaxs831]
        push edx
        lea edx, [eax+groundMins831]
        push edx
        push ecx
        lea ecx, [esp + 0x30]
        push ecx
        call dword ptr [eax+groundTrace831]
        mov eax, dword ptr [esp + 0x34]
        add esp, 0x1c
        mov ecx, 0xe
        lea esi, [esp + 0x18]
        mov edi, offset pml+groundGroundTrace831
        test eax, eax
        rep movsd 
        je ground831_20036898
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx+groundPs831]
        mov eax, dword ptr [edx+groundEflags831]
        test ah, 0x80
        jne ground831_2003689e
        lea eax, [esp + 0x18]
        push eax
        call PM_CorrectAllSolid
        add esp, 4
        test eax, eax
        je ground831_20036cfd
ground831_20036898:
        mov ecx, dword ptr [pm]
ground831_2003689e:
        fld dword ptr [esp + 0x20]
        fcomp qword ptr [groundK200ac130]
        fnstsw ax
        test ah, 0x40
        je ground831_20036a1f
        mov eax, dword ptr [pml+groundContent831]
        xor edi, edi
        cmp eax, edi
        je ground831_20036974
        mov edx, dword ptr [ecx+groundPs831]
        fld dword ptr [edx+groundVelocity831+8]
        fcomp dword ptr [groundK200ac2b8]
        fnstsw ax
        test ah, 1
        je ground831_20036974
        fld dword ptr [esp + 0x14]
        fsub dword ptr [groundK200ac148]
        fstp dword ptr [esp + 0x14]
        mov eax, dword ptr [ecx+groundPs831]
        mov edx, dword ptr [ecx+groundTraceMask831]
        push edx
        mov edx, dword ptr [eax+groundClient831]
        add eax, groundOrigin831
        push edx
        lea edx, [esp + 0x14]
        push edx
        lea edx, [ecx+groundMaxs831]
        push edx
        lea edx, [ecx+groundMins831]
        push edx
        push eax
        lea eax, [esp + 0x30]
        push eax
        call dword ptr [ecx+groundTrace831]
        fld dword ptr [esp + 0x3c]
        fcomp qword ptr [groundK200ac130]
        add esp, 0x1c
        fnstsw ax
        test ah, 1
        je ground831_200369f5
        cmp dword ptr [esp + 0x18], edi
        jne ground831_200369f5
        fld dword ptr [esp + 0x38]
        fcomp qword ptr [groundK200ac268]
        fnstsw ax
        test ah, 0x41
        jne ground831_200369f5
        mov ebp, 1
        mov ecx, 0xe
        lea esi, [esp + 0x18]
        mov edi, offset pml+groundGroundTrace831
        mov dword ptr [pml+groundExtension831], ebp
        rep movsd 
        mov ecx, dword ptr [pm]
        xor edi, edi
        jmp ground831_20036a26
ground831_20036974:
        mov edx, dword ptr [ecx+groundPs831]
        test dword ptr [edx+groundEflags831], 0x80000
        je ground831_200369fb
        fld dword ptr [esp + 0x14]
        fsub dword ptr [groundK200ac148]
        fstp dword ptr [esp + 0x14]
        mov eax, dword ptr [ecx+groundPs831]
        mov edx, dword ptr [ecx+groundTraceMask831]
        push edx
        mov edx, dword ptr [eax+groundClient831]
        add eax, groundOrigin831
        push edx
        lea edx, [esp + 0x14]
        push edx
        lea edx, [ecx+groundMaxs831]
        push edx
        lea edx, [ecx+groundMins831]
        push edx
        push eax
        lea eax, [esp + 0x30]
        push eax
        call dword ptr [ecx+groundTrace831]
        fld dword ptr [esp + 0x3c]
        fcomp qword ptr [groundK200ac130]
        add esp, 0x1c
        fnstsw ax
        test ah, 1
        je ground831_200369f5
        cmp dword ptr [esp + 0x18], edi
        jne ground831_200369f5
        mov ecx, 0xe
        lea esi, [esp + 0x18]
        mov edi, offset pml+groundGroundTrace831
        mov ebp, 1
        rep movsd 
        mov ecx, dword ptr [pm]
        xor edi, edi
        jmp ground831_200369fb
ground831_200369f5:
        mov ecx, dword ptr [pm]
ground831_200369fb:
        cmp dword ptr [pml+groundExtension831], edi
        jne ground831_20036a21
        cmp ebp, edi
        jne ground831_20036a21
        call PM_GroundTraceMissed
        mov dword ptr [pml+groundGroundPlane831], edi
        mov dword ptr [pml+groundWalking831], edi
        pop edi
        pop esi
        pop ebp
        add esp, 0x44
        ret 
ground831_20036a1f:
        xor edi, edi
ground831_20036a21:
        mov ebp, 1
ground831_20036a26:
        mov edx, dword ptr [ecx+groundPs831]
        fld dword ptr [edx+groundVelocity831+8]
        fcomp dword ptr [groundK200ac100]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036b06
        fld dword ptr [esp + 0x34]
        fmul dword ptr [edx+groundVelocity831+4]
        fld dword ptr [esp + 0x30]
        fmul dword ptr [edx+groundVelocity831]
        faddp st(1), st(0)
        fld dword ptr [esp + 0x38]
        fmul dword ptr [edx+groundVelocity831+8]
        faddp st(1), st(0)
        fcomp dword ptr [groundK200ac2dc]
        fnstsw ax
        test ah, 0x41
        jne ground831_20036b06
        test dword ptr [edx+groundEflags831], 0x80000
        jne ground831_20036b06
        cmp dword ptr [ecx+groundDebug831], edi
        je ground831_20036a92
        mov ecx, dword ptr [c_pmove]
        push ecx
        push offset groundS200bc604
        call Com_Printf
        mov ecx, dword ptr [pm]
        add esp, 8
ground831_20036a92:
        mov al, byte ptr [ecx+groundForwardCmd831]
        push edi
        test al, al
        push edi
        jl ground831_20036abf
        mov edx, dword ptr [ecx+groundCharacter831]
        mov ecx, dword ptr [ecx+groundPs831]
        push 4
        mov eax, dword ptr [edx+groundAnimModel831]
        push eax
        push ecx
        call BG_AnimScriptEvent
        mov edx, dword ptr [pm]
        add esp, 0x14
        mov eax, dword ptr [edx+groundPs831]
        mov ecx, dword ptr [eax+groundFlags831]
        and ecx, 0xfffffff7
        jmp ground831_20036ae1
ground831_20036abf:
        mov eax, dword ptr [ecx+groundCharacter831]
        push 5
        mov edx, dword ptr [eax+groundAnimModel831]
        mov eax, dword ptr [ecx+groundPs831]
        push edx
        push eax
        call BG_AnimScriptEvent
        mov ecx, dword ptr [pm]
        add esp, 0x14
        mov eax, dword ptr [ecx+groundPs831]
        mov ecx, dword ptr [eax+groundFlags831]
        or ecx, 8
ground831_20036ae1:
        mov dword ptr [eax+groundFlags831], ecx
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx+groundPs831]
        mov dword ptr [eax+groundGround831], 0x3ff
        mov dword ptr [pml+groundGroundPlane831], edi
        mov dword ptr [pml+groundWalking831], edi
        pop edi
        pop esi
        pop ebp
        add esp, 0x44
        ret 
ground831_20036b06:
        fld dword ptr [esp + 0x38]
        fcomp qword ptr [groundK200ac268]
        mov esi, 0x3ff
        fnstsw ax
        test ah, 1
        je ground831_20036b61
        cmp dword ptr [ecx+groundDebug831], edi
        je ground831_20036b3b
        mov ecx, dword ptr [c_pmove]
        push ecx
        push offset groundS200bc5f8
        call Com_Printf
        mov ecx, dword ptr [pm]
        add esp, 8
ground831_20036b3b:
        mov eax, dword ptr [ecx+groundPs831]
        test byte ptr [eax+groundTcFlags831], 4
        je ground831_20036b4b
        cmp dword ptr [eax+groundGround831], esi
        je ground831_20036b61
ground831_20036b4b:
        mov dword ptr [eax+groundGround831], esi
        mov dword ptr [pml+groundWalking831], edi
        pop edi
        mov dword ptr [pml+groundGroundPlane831], ebp
        pop esi
        pop ebp
        add esp, 0x44
        ret 
ground831_20036b61:
        mov dword ptr [pml+groundGroundPlane831], ebp
        mov dword ptr [pml+groundWalking831], ebp
        mov edx, dword ptr [ecx+groundPs831]
        mov eax, dword ptr [edx+groundFlags831]
        test ah, 1
        je ground831_20036b90
        and eax, 0xfffffedf
        mov dword ptr [edx+groundFlags831], eax
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx+groundPs831]
        mov dword ptr [eax+groundTime831], edi
        mov ecx, dword ptr [pm]
ground831_20036b90:
        mov edx, dword ptr [ecx+groundPs831]
        cmp dword ptr [edx+groundGround831], esi
        jne ground831_20036ccd
        cmp dword ptr [ecx+groundDebug831], edi
        je ground831_20036bb3
        mov eax, dword ptr [c_pmove]
        push eax
        push offset groundS200bc5ec
        call Com_Printf
        add esp, 8
ground831_20036bb3:
        fld dword ptr [esp + 0x38]
        fcomp qword ptr [groundK200ac268]
        fnstsw ax
        test ah, 1
        je ground831_20036bc8
        mov eax, ebp
        jmp ground831_20036bca
ground831_20036bc8:
        xor eax, eax
ground831_20036bca:
        push eax
        call PM_CrashLand
        fld dword ptr [pml+groundOldVelZ831]
        fcomp dword ptr [groundK200ac7d4]
        add esp, 4
        fnstsw ax
        test ah, 1
        je ground831_20036c40
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx+groundPs831]
        mov ebp, dword ptr [eax+groundFlags831]
        or ebp, 0x20
        mov dword ptr [eax+groundFlags831], ebp
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx+groundPs831]
        mov dword ptr [eax+groundTime831], 0xfa
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundCommandTime831]
        mov edx, dword ptr [eax+groundExt831]
        neg ecx
        mov dword ptr [edx+groundExtProne831], ecx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundCommandTime831]
        mov edx, dword ptr [eax+groundExt831]
        sub ecx, 0x258
        mov dword ptr [edx+groundExtJump831], ecx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundCommandTime831]
        mov edx, dword ptr [eax+groundPs831]
        sub ecx, 0x258
        mov dword ptr [edx+groundJumpTime831], ecx
ground831_20036c40:
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx+groundPs831]
        mov edx, dword ptr [eax+groundWeaponFlags831]
        test dh, 0x10
        je ground831_20036cb3
        mov dword ptr [eax+groundTime831], 0x3e8
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax+groundPs831]
        mov ebp, dword ptr [eax+groundWeaponFlags831]
        and ebp, 0xffffefff
        mov dword ptr [eax+groundWeaponFlags831], ebp
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundCommandTime831]
        mov edx, dword ptr [eax+groundExt831]
        neg ecx
        mov dword ptr [edx+groundExtProne831], ecx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundCommandTime831]
        mov edx, dword ptr [eax+groundExt831]
        sub ecx, 0x258
        mov dword ptr [edx+groundExtJump831], ecx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+groundCommandTime831]
        mov edx, dword ptr [eax+groundPs831]
        sub ecx, 0x258
        mov dword ptr [edx+groundJumpTime831], ecx
        mov ecx, dword ptr [pm]
ground831_20036cb3:
        mov edx, dword ptr [ecx+groundPs831]
        mov eax, dword ptr [edx+groundTcFlags831]
        test al, 4
        je ground831_20036ccd
        and al, 0xfb
        mov dword ptr [edx+groundTcFlags831], eax
        mov ecx, dword ptr [pm]
ground831_20036ccd:
        mov eax, dword ptr [ecx+groundPs831]
        mov ecx, dword ptr [esp + 0x4c]
        mov dword ptr [eax+groundGround831], ecx
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx+groundPs831]
        cmp dword ptr [eax+groundGround831], 0x40
        jge ground831_20036cf0
        mov eax, dword ptr [pml+groundSurface831]
        or al, 2
        mov dword ptr [pml+groundSurface831], eax
ground831_20036cf0:
        mov ecx, dword ptr [esp + 0x4c]
        push ecx
        call PM_AddTouchEnt
        add esp, 4
ground831_20036cfd:
        pop edi
        pop esi
        pop ebp
        add esp, 0x44
        ret 
    }
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
/* TC qagame 200372c0/20037460: integer field copies and retained x87
 * sampling / __ftol return schedule. Native offsets replace original ABI slots. */
#if defined(_MSC_VER) && defined(_M_IX86)
static const float posture831One = 1.0f;
typedef char posture831PsAtZero[(offsetof(pmove_t, ps) == 0) ? 1 : -1];
typedef char posture831TraceLayout[(sizeof(trace_t) == 56 && offsetof(trace_t, allsolid) == 0) ? 1 : -1];
enum {
    posture831pm110 = offsetof(pmove_t, waterlevel),
    posture831pm10c = offsetof(pmove_t, watertype),
    posture831ps14 = offsetof(playerState_t, origin),
    posture831ps18 = offsetof(playerState_t, origin)+4,
    posture831ps3e4 = offsetof(playerState_t, mins)+8,
    posture831ps1c = offsetof(playerState_t, origin)+8,
    posture831psa0 = offsetof(playerState_t, clientNum),
    posture831pm12c = offsetof(pmove_t, pointcontents),
    posture831psbc = offsetof(playerState_t, viewheight),
    posture831ps3dc = offsetof(playerState_t, mins),
    posture831pmf4 = offsetof(pmove_t, mins),
    posture831ps3e0 = offsetof(playerState_t, mins)+4,
    posture831pmf8 = offsetof(pmove_t, mins)+4,
    posture831ps3e8 = offsetof(playerState_t, maxs),
    posture831pm100 = offsetof(pmove_t, maxs),
    posture831ps3ec = offsetof(playerState_t, maxs)+4,
    posture831pm104 = offsetof(pmove_t, maxs)+4,
    posture831pmfc = offsetof(pmove_t, mins)+8,
    posture831ps4 = offsetof(playerState_t, pm_type),
    posture831ps3f0 = offsetof(playerState_t, maxs)+8,
    posture831pm108 = offsetof(pmove_t, maxs)+8,
    posture831ps400 = offsetof(playerState_t, deadViewHeight),
    posture831ps3f4 = offsetof(playerState_t, crouchMaxZ),
    posture831ps3f8 = offsetof(playerState_t, crouchViewHeight),
    posture831ps148 = offsetof(playerState_t, persistant)+14*sizeof(int),
    posture831ps68 = offsetof(playerState_t, eFlags),
    posture831psc = offsetof(playerState_t, pm_flags),
    posture831pm22 = offsetof(pmove_t, cmd)+offsetof(usercmd_t,upmove),
    posture831psf0 = offsetof(playerState_t, stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    posture831psa4 = offsetof(playerState_t, weapon),
    posture831pm44 = offsetof(pmove_t, tracemask),
    posture831pm128 = offsetof(pmove_t, trace),
    posture831pmc = offsetof(pmove_t, cmd)+offsetof(usercmd_t,serverTime),
    posture831pm4 = offsetof(pmove_t, pmext),
    posture831ExtJump = offsetof(pmoveExt_t, jumpTime),
    posture831ps47c = offsetof(playerState_t, jumpTime),
    posture831ps3fc = offsetof(playerState_t, standViewHeight)
};
static __declspec(naked) void PM_SetWaterLevel(void) {
    __asm {
        SUB ESP,0x14
        MOV ECX,dword ptr [pm]
        XOR EAX,EAX
        PUSH EBX
        MOV dword ptr [ECX + posture831pm110],EAX
        MOV EDX,dword ptr [pm]
        MOV dword ptr [EDX + posture831pm10c],EAX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX]
        MOV EDX,dword ptr [ECX + posture831ps14]
        MOV dword ptr [ESP + 0xc],EDX
        MOV ECX,dword ptr [EAX]
        MOV EDX,dword ptr [ECX + posture831ps18]
        MOV dword ptr [ESP + 0x10],EDX
        MOV ECX,dword ptr [EAX]
        FLD dword ptr [ECX + posture831ps3e4]
        FADD dword ptr [ECX + posture831ps1c]
        FADD dword ptr [posture831One]
        FSTP dword ptr [ESP + 0x14]
        MOV ECX,dword ptr [EAX]
        MOV EDX,dword ptr [ECX + posture831psa0]
        LEA ECX,[ESP + 0xc]
        PUSH EDX
        PUSH ECX
        CALL dword ptr [EAX + posture831pm12c]
        MOV EBX,EAX
        ADD ESP,0x8
        TEST BL,0x38
        JZ posture831_200373f6
        PUSH ESI
        MOV ESI,dword ptr [pm]
        MOV EAX,dword ptr [ESI]
        FILD dword ptr [EAX + posture831psbc]
        FSUB dword ptr [EAX + posture831ps3e4]
        CALL PM_MovementTruncate827
        MOV dword ptr [ESI + posture831pm10c],EBX
        MOV EDX,dword ptr [pm]
        MOV dword ptr [ESP + 0xc],EAX
        MOV dword ptr [EDX + posture831pm110],0x1
        MOV ECX,dword ptr [pm]
        CDQ
        MOV ESI,dword ptr [ECX]
        SUB EAX,EDX
        SAR EAX,0x1
        MOV dword ptr [ESP + 0x8],EAX
        FILD dword ptr [ESP + 0x8]
        FADD dword ptr [ESI + posture831ps3e4]
        FADD dword ptr [ESI + posture831ps1c]
        FSTP dword ptr [ESP + 0x18]
        MOV EAX,dword ptr [ECX]
        MOV EDX,dword ptr [EAX + posture831psa0]
        LEA EAX,[ESP + 0x10]
        PUSH EDX
        PUSH EAX
        CALL dword ptr [ECX + posture831pm12c]
        ADD ESP,0x8
        MOV EBX,EAX
        TEST BL,0x38
        POP ESI
        JZ posture831_200373f6
        MOV ECX,dword ptr [pm]
        MOV dword ptr [ECX + posture831pm110],0x2
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX]
        FLD dword ptr [ECX + posture831ps3e4]
        FADD dword ptr [ECX + posture831ps1c]
        FIADD dword ptr [ESP + 0x8]
        FSTP dword ptr [ESP + 0x14]
        MOV EDX,dword ptr [EAX]
        MOV ECX,dword ptr [EDX + posture831psa0]
        LEA EDX,[ESP + 0xc]
        PUSH ECX
        PUSH EDX
        CALL dword ptr [EAX + posture831pm12c]
        MOV EBX,EAX
        ADD ESP,0x8
        TEST BL,0x38
        JZ posture831_200373f6
        MOV EAX,[pm]
        MOV dword ptr [EAX + posture831pm110],0x3
posture831_200373f6:
        TEST BH,0x10
        JZ posture831_20037407
        MOV dword ptr [pml.tceContentRestriction],0x1
        JMP posture831_20037427
posture831_20037407:
        TEST BH,0x8
        JZ posture831_20037418
        MOV dword ptr [pml.tceContentRestriction],0x2
        JMP posture831_20037427
posture831_20037418:
        TEST BH,0x4
        JZ posture831_20037427
        MOV dword ptr [pml.tceContentRestriction],0x3
posture831_20037427:
        MOV EAX,[pm]
        XOR ECX,ECX
        PUSH 0x1
        MOV EBX,dword ptr [EAX + posture831pm110]
        MOV EDX,dword ptr [EAX]
        CMP EBX,0x2
        MOV EAX,dword ptr [EDX + posture831psa0]
        SETG CL
        PUSH ECX
        PUSH 0x3
        PUSH EAX
        CALL BG_UpdateConditionValue
        ADD ESP,0x10
        POP EBX
        ADD ESP,0x14
        RET
    }
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
static void PM_TCECheckDuck(void) {
    trace_t trace;
    playerState_t *ps = pm->ps;
    qboolean toggle = (ps->persistant[14] & 4) != 0;
    VectorCopy(ps->mins, pm->mins);
    pm->maxs[0] = ps->maxs[0];
    pm->maxs[1] = ps->maxs[1];
    if (ps->pm_type == PM_DEAD) {
        pm->maxs[2] = ps->maxs[2];
        ps->viewheight = (int)ps->deadViewHeight;
        return;
    }
    if (ps->pm_type == PM_SPECTATOR) {
        pm->maxs[2] = ps->crouchMaxZ;
        ps->viewheight = (int)ps->crouchViewHeight;
        return;
    }
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
        if (!trace.allsolid) ps->pm_flags &= ~PMF_DUCKED;
        if (pm->cmd.upmove > 10) {
            ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x200;
            pm->pmext->jumpTime = ps->jumpTime = pm->cmd.serverTime - 650;
            ps->pm_flags |= PMF_JUMP_HELD;
        }
    }
    pm->maxs[2] = (ps->pm_flags & PMF_DUCKED) ? ps->crouchMaxZ : ps->maxs[2];
    ps->viewheight = (int)((ps->pm_flags & PMF_DUCKED) ? ps->crouchViewHeight : ps->standViewHeight);
}

#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) void PM_CheckDuck(void) {
    __asm {
        MOV EAX,[pm]
        SUB ESP,0x38
        MOV ECX,dword ptr [EAX]
        PUSH ESI
        MOV EDX,dword ptr [ECX + posture831ps3dc]
        MOV dword ptr [EAX + posture831pmf4],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX]
        MOV EDX,dword ptr [ECX + posture831ps3e0]
        MOV dword ptr [EAX + posture831pmf8],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX]
        MOV EDX,dword ptr [ECX + posture831ps3e8]
        MOV dword ptr [EAX + posture831pm100],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX]
        MOV EDX,dword ptr [ECX + posture831ps3ec]
        MOV dword ptr [EAX + posture831pm104],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX]
        MOV EDX,dword ptr [ECX + posture831ps3e4]
        MOV dword ptr [EAX + posture831pmfc],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX]
        MOV EDX,dword ptr [ECX + posture831ps4]
        CMP EDX,0x3
        JNZ posture831_200374fc
        MOV ECX,dword ptr [ECX + posture831ps3f0]
        MOV dword ptr [EAX + posture831pm108],ECX
        MOV EDX,dword ptr [pm]
        MOV ESI,dword ptr [EDX]
        FLD dword ptr [ESI + posture831ps400]
        CALL PM_MovementTruncate827
        MOV dword ptr [ESI + posture831psbc],EAX
        POP ESI
        ADD ESP,0x38
        RET
posture831_200374fc:
        CMP EDX,0x2
        JNZ posture831_2003752b
        MOV ECX,dword ptr [ECX + posture831ps3f4]
        MOV dword ptr [EAX + posture831pm108],ECX
        MOV EDX,dword ptr [pm]
        MOV ESI,dword ptr [EDX]
        FLD dword ptr [ESI + posture831ps3f8]
        CALL PM_MovementTruncate827
        MOV dword ptr [ESI + posture831psbc],EAX
        POP ESI
        ADD ESP,0x38
        RET
posture831_2003752b:
        MOV DL,byte ptr [ECX + posture831ps148]
        PUSH EBX
        TEST DL,0x4
        JZ posture831_20037595
        TEST dword ptr [ECX + posture831ps68],0x1008000
        JNZ posture831_20037595
        TEST byte ptr [ECX + posture831psc],0x4
        JNZ posture831_20037595
        MOV BL,byte ptr [EAX + posture831pm22]
        TEST BL,BL
        JGE posture831_20037581
        MOV EDX,dword ptr [ECX + posture831psf0]
        TEST DH,0x4
        JNZ posture831_2003757d
        OR DH,0x4
        MOV dword ptr [ECX + posture831psf0],EDX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX]
        MOV EAX,dword ptr [ECX + posture831psf0]
        TEST AH,0x2
        JZ posture831_20037578
        AND AH,0xfd
        JMP posture831_2003758a
posture831_20037578:
        OR AH,0x2
        JMP posture831_2003758a
posture831_2003757d:
        TEST BL,BL
        JL posture831_20037595
posture831_20037581:
        MOV EAX,dword ptr [ECX + posture831psf0]
        AND AH,0xfb
posture831_2003758a:
        MOV dword ptr [ECX + posture831psf0],EAX
        MOV EAX,[pm]
posture831_20037595:
        MOV DL,byte ptr [EAX + posture831pm22]
        CMP DL,0xa
        JG posture831_200375c6
        MOV ECX,dword ptr [EAX]
        TEST dword ptr [ECX + posture831psf0],0x200
        JZ posture831_200375c6
        TEST byte ptr [ECX + posture831ps148],0x4
        JZ posture831_200375c6
        MOV EAX,dword ptr [ECX + posture831psc]
        MOV EBX,0x1
        OR EAX,EBX
        MOV dword ptr [ECX + posture831psc],EAX
        JMP posture831_200376b9
posture831_200375c6:
        TEST DL,DL
        JGE posture831_200375e4
        MOV ECX,dword ptr [EAX]
        TEST byte ptr [ECX + posture831ps148],0x4
        JNZ posture831_200375e4
        TEST dword ptr [ECX + posture831ps68],0x1008000
        JNZ posture831_200375e4
        TEST byte ptr [ECX + posture831psc],0x4
        JZ posture831_200375ef
posture831_200375e4:
        MOV ECX,dword ptr [EAX]
        CMP dword ptr [ECX + posture831psa4],0x3c
        JNZ posture831_20037601
posture831_200375ef:
        MOV EAX,dword ptr [ECX + posture831psc]
        MOV EBX,0x1
        OR EAX,EBX
        MOV dword ptr [ECX + posture831psc],EAX
        JMP posture831_200376b9
posture831_20037601:
        MOV DL,byte ptr [ECX + posture831psc]
        MOV EBX,0x1
        TEST BL,DL
        JZ posture831_200376be
        MOV ECX,dword ptr [ECX + posture831ps3f0]
        MOV dword ptr [EAX + posture831pm108],ECX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX]
        MOV ESI,dword ptr [EAX + posture831pm44]
        PUSH ESI
        LEA EDX,[ECX + posture831ps14]
        MOV ECX,dword ptr [ECX + posture831psa0]
        PUSH ECX
        LEA ECX,[EAX + posture831pm100]
        PUSH EDX
        PUSH ECX
        LEA ECX,[EAX + posture831pmf4]
        PUSH ECX
        PUSH EDX
        LEA EDX,[ESP + 0x20]
        PUSH EDX
        CALL dword ptr [EAX + posture831pm128]
        MOV EAX,dword ptr [ESP + 0x24]
        ADD ESP,0x1c
        TEST EAX,EAX
        JNZ posture831_20037663
        MOV EAX,[pm]
        MOV EAX,dword ptr [EAX]
        AND dword ptr [EAX + posture831psc],0xfffffffe
posture831_20037663:
        MOV EAX,[pm]
        CMP byte ptr [EAX + posture831pm22],0xa
        JLE posture831_200376be
        MOV EDX,dword ptr [EAX]
        MOV ECX,dword ptr [EDX + posture831psf0]
        TEST CH,0x2
        JZ posture831_20037689
        AND CH,0xfd
        MOV dword ptr [EDX + posture831psf0],ECX
        MOV EAX,[pm]
posture831_20037689:
        MOV ECX,dword ptr [EAX + posture831pmc]
        MOV EDX,dword ptr [EAX + posture831pm4]
        SUB ECX,0x28a
        MOV dword ptr [EDX + posture831ExtJump],ECX
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + posture831pmc]
        MOV EDX,dword ptr [EAX]
        SUB ECX,0x28a
        MOV dword ptr [EDX + posture831ps47c],ECX
        MOV EAX,[pm]
        MOV EAX,dword ptr [EAX]
        OR dword ptr [EAX + posture831psc],0x2
posture831_200376b9:
        MOV EAX,[pm]
posture831_200376be:
        MOV ECX,dword ptr [EAX]
        TEST byte ptr [ECX + posture831psc],BL
        POP EBX
        JZ posture831_200376f0
        MOV ECX,dword ptr [ECX + posture831ps3f4]
        MOV dword ptr [EAX + posture831pm108],ECX
        MOV EDX,dword ptr [pm]
        MOV ESI,dword ptr [EDX]
        FLD dword ptr [ESI + posture831ps3f8]
        CALL PM_MovementTruncate827
        MOV dword ptr [ESI + posture831psbc],EAX
        POP ESI
        ADD ESP,0x38
        RET
posture831_200376f0:
        MOV ECX,dword ptr [ECX + posture831ps3f0]
        MOV dword ptr [EAX + posture831pm108],ECX
        MOV EDX,dword ptr [pm]
        MOV ESI,dword ptr [EDX]
        FLD dword ptr [ESI + posture831ps3fc]
        CALL PM_MovementTruncate827
        MOV dword ptr [ESI + posture831psbc],EAX
        POP ESI
        ADD ESP,0x38
        RET
    }
}
#else
static void PM_CheckDuck(void) {
    PM_TCECheckDuck();
}
#endif



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

/* Whole TC:E cgame3000f3c0 / Linux000e1998, shared prediction/server. */
#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) void PM_Footsteps(void) {
    __asm {
        PUSH ECX
        MOV EDX,dword ptr [pm]
        PUSH EBX
        PUSH EBP
        PUSH ESI
        MOV EAX,dword ptr [EDX + posture832pm0]
        PUSH EDI
        TEST byte ptr [EAX + posture832ps68],0x1
        JZ posture832_200377bc
        MOV ECX,dword ptr [EAX + posture832psc]
        TEST CH,0x8
        JZ posture832_20037774
        MOV ECX,dword ptr [EDX + posture832pm8]
        PUSH 0x1
        PUSH 0x15
        MOV EDX,dword ptr [ECX + posture832character40]
        PUSH EDX
        PUSH EAX
        CALL BG_AnimScriptAnimation
        MOV EAX,[pm]
        ADD ESP,0x10
        MOV EAX,dword ptr [EAX + posture832pm0]
        MOV ECX,dword ptr [EAX + posture832ps10]
        TEST ECX,ECX
        JNZ posture832_20037eb6
        MOV ECX,dword ptr [EAX + posture832psc]
        POP EDI
        AND CH,0xf7
        POP ESI
        POP EBP
        MOV dword ptr [EAX + posture832psc],ECX
        POP EBX
        POP ECX
        RET
posture832_20037774:
        MOV EDX,dword ptr [EAX + posture832ps10]
        TEST EDX,EDX
        JNZ posture832_20037eb6
        TEST CH,0x40
        JNZ posture832_20037eb6
        CMP dword ptr [EAX + posture832ps50],0x3ff
        JNZ posture832_20037eb6
        OR CH,0x8
        PUSH 0x1
        MOV dword ptr [EAX + posture832psc],ECX
        MOV EAX,[pm]
        PUSH 0x15
        MOV ECX,dword ptr [EAX + posture832pm8]
        MOV EAX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832character40]
        PUSH EDX
        PUSH EAX
        CALL BG_AnimScriptAnimation
        ADD ESP,0x10
        POP EDI
        POP ESI
        POP EBP
        POP EBX
        POP ECX
        RET
posture832_200377bc:
        MOV EDX,dword ptr [EAX + posture832psf0]
        OR EDX,0x20
        MOV dword ptr [EAX + posture832psf0],EDX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ECX + posture832pm0]
        MOV ECX,dword ptr [EAX + posture832ps68]
        OR ECX,0x10000
        MOV dword ptr [EAX + posture832ps68],ECX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ECX + posture832pm0]
        FLD dword ptr [EAX + posture832ps24]
        FLD dword ptr [EAX + posture832ps20]
        FLD ST(0)
        FMUL ST(0),ST(1)
        FLD ST(2)
        FMUL ST(0),ST(3)
        FADDP ST(1),ST(0)
        FSQRT
        FSTP dword ptr [ECX + posture832pm114]
        MOV ECX,dword ptr [pm]
        FSTP ST(0)
        MOV EDX,dword ptr [ECX + posture832pm0]
        FSTP ST(0)
        MOV EAX,dword ptr [EDX + posture832ps140]
        TEST EAX,EAX
        JZ posture832_20037854
        MOV EAX,dword ptr [ECX + posture832pm8]
        PUSH 0x1
        PUSH 0x1
        MOV ECX,dword ptr [EAX + posture832character40]
        PUSH ECX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        MOV EDX,dword ptr [pm]
        ADD ESP,0x10
        MOV EAX,dword ptr [EDX + posture832pm0]
        POP EDI
        POP ESI
        POP EBP
        MOV EDX,dword ptr [EAX + posture832psf0]
        POP EBX
        AND EDX,0xffffffdf
        MOV dword ptr [EAX + posture832psf0],EDX
        MOV EAX,[pm]
        MOV EAX,dword ptr [EAX + posture832pm0]
        AND dword ptr [EAX + posture832ps68],0xfffeffff
        POP ECX
        RET
posture832_20037854:
        MOV EAX,dword ptr [ECX + posture832pm110]
        MOV EBX,0x2
        CMP EAX,EBX
        JLE posture832_20037888
        MOV AL,byte ptr [EDX + posture832psc]
        PUSH 0x1
        TEST AL,0x10
        JZ posture832_20037884
        PUSH 0xa
posture832_2003786e:
        MOV ECX,dword ptr [ECX + posture832pm8]
        MOV EAX,dword ptr [ECX + posture832character40]
        PUSH EAX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        ADD ESP,0x10
        POP EDI
        POP ESI
        POP EBP
        POP EBX
        POP ECX
        RET
posture832_20037884:
        PUSH 0x9
        JMP posture832_2003786e
posture832_20037888:
        CMP dword ptr [EDX + posture832ps50],0x3ff
        JNZ posture832_20037987
        TEST byte ptr [EDX + posture832psc],0x4
        JZ posture832_20037eb6
        FLD dword ptr [EDX + posture832ps28]
        FCOMP dword ptr [posture832Const200ac100]
        PUSH 0x1
        FNSTSW AX
        TEST AH,0x1
        JNZ posture832_200378b5
        PUSH 0xf
        JMP posture832_200378b7
posture832_200378b5:
        PUSH 0x10
posture832_200378b7:
        MOV ECX,dword ptr [ECX + posture832pm8]
        MOV EAX,dword ptr [ECX + posture832character40]
        PUSH EAX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        MOV EAX,[pm]
        ADD ESP,0x10
        MOV CL,byte ptr [EAX + posture832pm20]
        TEST CL,CL
        JNZ posture832_200378e5
        MOV CL,byte ptr [EAX + posture832pm21]
        TEST CL,CL
        JNZ posture832_200378e5
        MOV CL,byte ptr [EAX + posture832pm22]
        TEST CL,CL
        JZ posture832_20037eb6
posture832_200378e5:
        MOV ECX,dword ptr [EAX + posture832pm0]
        ADD ECX,posture832ps20
        PUSH ECX
        CALL VectorLength
        FMUL qword ptr [posture832Const200ac7f8]
        ADD ESP,0x4
        FCOM dword ptr [posture832Const200ac180]
        FNSTSW AX
        TEST AH,0x41
        JNZ posture832_20037910
        FSTP ST(0)
        FLD dword ptr [posture832Const200ac180]
        JMP posture832_20037921
posture832_20037910:
        FCOM dword ptr [posture832Const200ac3e8]
        FNSTSW AX
        TEST AH,0x1
        JNZ posture832_20037eb4
posture832_20037921:
        MOV EDX,dword ptr [pm]
        FILD dword ptr [pml.msec]
        MOV ESI,dword ptr [EDX + posture832pm0]
        MOV EDI,dword ptr [ESI + posture832ps8]
        FMUL ST(0),ST(1)
        MOV dword ptr [ESP + 0x10],EDI
        FIADD dword ptr [ESP + 0x10]
        CALL PM_MovementTruncate827
        AND EAX,0xff
        ADD EDI,0x40
        MOV dword ptr [ESI + posture832ps8],EAX
        MOV EAX,[pm]
        FSTP ST(0)
        MOV ECX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832ps8]
        ADD EDX,0x40
        XOR EDX,EDI
        TEST DL,0x80
        JZ posture832_20037eb6
        MOV ECX,dword ptr [EAX + posture832pm4c]
        TEST ECX,ECX
        JNZ posture832_20037eb6
        CALL PM_TCELadderFootstepForSurface
        PUSH EAX
        PUSH 0x1
        CALL PM_AddEventExt
        ADD ESP,0x8
        POP EDI
        POP ESI
        POP EBP
        POP EBX
        POP ECX
        RET
posture832_20037987:
        MOV AL,byte ptr [ECX + posture832pm20]
        TEST AL,AL
        JNZ posture832_20037a7e
        MOV AL,byte ptr [ECX + posture832pm21]
        TEST AL,AL
        JNZ posture832_20037a7e
        FLD dword ptr [ECX + posture832pm114]
        FCOMP dword ptr [posture832Const200ac340]
        FNSTSW AX
        TEST AH,0x1
        JZ posture832_200379bd
        MOV dword ptr [EDX + posture832ps8],0x0
        MOV ECX,dword ptr [pm]
posture832_200379bd:
        FLD dword ptr [ECX + posture832pm114]
        FCOMP dword ptr [posture832Const200ac700]
        FNSTSW AX
        TEST AH,0x41
        JZ posture832_20037eb6
        MOV EAX,dword ptr [ECX + posture832pm0]
        MOV EDI,0xffffffdf
        MOV ESI,0xfffeffff
        TEST dword ptr [EAX + posture832ps68],0x80000
        JZ posture832_200379ef
        PUSH 0x1
        PUSH 0x14
        JMP posture832_200379f8
posture832_200379ef:
        TEST byte ptr [EAX + posture832psc],0x1
        JZ posture832_20037a3c
        PUSH 0x1
        PUSH EBX
posture832_200379f8:
        MOV ECX,dword ptr [ECX + posture832pm8]
        MOV EDX,dword ptr [ECX + posture832character40]
        PUSH EDX
        PUSH EAX
        CALL BG_AnimScriptAnimation
        MOV ECX,dword ptr [pm]
        ADD ESP,0x10
        MOV ECX,dword ptr [ECX + posture832pm0]
        MOV EBX,dword ptr [ECX + posture832psf0]
        AND EBX,EDI
        MOV dword ptr [ECX + posture832psf0],EBX
        MOV EDX,dword ptr [pm]
        MOV ECX,dword ptr [EDX + posture832pm0]
        MOV EDX,dword ptr [ECX + posture832ps68]
        AND EDX,ESI
        TEST EAX,EAX
        MOV dword ptr [ECX + posture832ps68],EDX
        JGE posture832_20037eb6
        MOV ECX,dword ptr [pm]
posture832_20037a3c:
        MOV EAX,dword ptr [ECX + posture832pm8]
        PUSH 0x1
        PUSH 0x1
        MOV EDX,dword ptr [EAX + posture832character40]
        MOV EAX,dword ptr [ECX + posture832pm0]
        PUSH EDX
        PUSH EAX
        CALL BG_AnimScriptAnimation
        MOV ECX,dword ptr [pm]
        ADD ESP,0x10
        MOV EAX,dword ptr [ECX + posture832pm0]
        MOV EDX,dword ptr [EAX + posture832psf0]
        AND EDX,EDI
        POP EDI
        MOV dword ptr [EAX + posture832psf0],EDX
        MOV EDX,dword ptr [pm]
        MOV EAX,dword ptr [EDX + posture832pm0]
        MOV ECX,dword ptr [EAX + posture832ps68]
        AND ECX,ESI
        POP ESI
        POP EBP
        MOV dword ptr [EAX + posture832ps68],ECX
        POP EBX
        POP ECX
        RET
posture832_20037a7e:
        MOV ECX,dword ptr [EDX + posture832psf0]
        XOR EBP,EBP
        OR ECX,EBX
        MOV dword ptr [EDX + posture832psf0],ECX
        MOV ECX,dword ptr [pm]
        MOV EDX,dword ptr [ECX + posture832pm0]
        MOV ESI,dword ptr [EDX + posture832ps68]
        TEST ESI,0x80000
        JZ posture832_20037ad1
        MOV AL,byte ptr [EDX + posture832psc]
        MOV dword ptr [ESP + 0x10],0x3e4ccccd
        TEST AL,0x10
        PUSH 0x1
        JZ posture832_20037ab6
        PUSH 0x13
        JMP posture832_20037ab8
posture832_20037ab6:
        PUSH 0x12
posture832_20037ab8:
        MOV EAX,dword ptr [ECX + posture832pm8]
        MOV ECX,dword ptr [EAX + posture832character40]
        PUSH ECX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        ADD ESP,0x10
        MOV ECX,EAX
        MOV EBP,EBX
        JMP posture832_20037d8f
posture832_20037ad1:
        MOV EAX,dword ptr [EDX + posture832psc]
        TEST AL,0x1
        JZ posture832_20037b3b
        TEST AL,0x10
        MOV dword ptr [ESP + 0x10],0x3e4ccccd
        PUSH 0x1
        JZ posture832_20037aea
        PUSH 0x6
        JMP posture832_20037aec
posture832_20037aea:
        PUSH 0x5
posture832_20037aec:
        MOV EAX,dword ptr [ECX + posture832pm8]
        MOV ECX,dword ptr [EAX + posture832character40]
        PUSH ECX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        MOV EDX,dword ptr [pm]
        MOV ECX,EAX
        ADD ESP,0x10
        MOV EAX,dword ptr [EDX + posture832pm0]
        TEST byte ptr [EAX + posture832psf0],0x4
        JNZ posture832_20037d8f
        FLD dword ptr [EAX + posture832ps3c]
        FCOMP dword ptr [posture832Const200ac100]
        FNSTSW AX
        TEST AH,0x40
        JZ posture832_20037d8f
        TEST byte ptr [EDX + posture832pm10],0x10
        JNZ posture832_20037d8f
        MOV EBP,0x1
        JMP posture832_20037d8f
posture832_20037b3b:
        TEST AL,0x10
        MOV AL,byte ptr [ECX + posture832pm10]
        JZ posture832_20037c60
        TEST AL,0x10
        JZ posture832_20037ba1
        TEST AL,0x20
        JNZ posture832_20037ba1
        MOV AL,byte ptr [ECX + posture832pm21]
        MOV dword ptr [ESP + 0x10],0x3e8f5c29
        TEST AL,AL
        JZ posture832_20037b8e
        MOV BL,byte ptr [ECX + posture832pm20]
        TEST BL,BL
        JNZ posture832_20037b8e
        TEST AL,AL
        PUSH 0x1
        JLE posture832_20037b6e
        PUSH 0xb
        JMP posture832_20037b70
posture832_20037b6e:
        PUSH 0xc
posture832_20037b70:
        MOV EAX,dword ptr [ECX + posture832pm8]
        MOV ECX,dword ptr [EAX + posture832character40]
        PUSH ECX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        ADD ESP,0x10
        TEST EAX,EAX
        JGE posture832_20037dd3
        MOV ECX,dword ptr [pm]
posture832_20037b8e:
        MOV EDX,dword ptr [ECX + posture832pm8]
        MOV ECX,dword ptr [ECX + posture832pm0]
        PUSH 0x1
        PUSH 0x4
        MOV EAX,dword ptr [EDX + posture832character40]
        PUSH EAX
        PUSH ECX
        JMP posture832_20037d85
posture832_20037ba1:
        MOV AL,byte ptr [EDX + posture832psf0]
        MOV EBX,0x4
        TEST BL,AL
        JNZ posture832_20037bf1
        FLD dword ptr [EDX + posture832ps3c]
        FCOMP dword ptr [posture832Const200ac100]
        FNSTSW AX
        TEST AH,0x40
        JZ posture832_20037bf1
        FLD dword ptr [ECX + posture832pm114]
        FCOMP dword ptr [posture832Const200ac7f0]
        FNSTSW AX
        TEST AH,0x41
        JNZ posture832_20037be2
        MOV dword ptr [ESP + 0x10],0x3ee66666
        MOV EBP,0x3
        JMP posture832_20037bfe
posture832_20037be2:
        MOV dword ptr [ESP + 0x10],0x3eb851ec
        MOV EBP,0x2
        JMP posture832_20037bfe
posture832_20037bf1:
        MOV dword ptr [ESP + 0x10],0x3e8f5c29
        MOV EBP,0x1
posture832_20037bfe:
        MOV AL,byte ptr [ECX + posture832pm21]
        TEST AL,AL
        JZ posture832_20037c35
        CMP byte ptr [ECX + posture832pm20],0x0
        JNZ posture832_20037c35
        TEST AL,AL
        PUSH 0x1
        JLE posture832_20037c15
        PUSH 0xb
        JMP posture832_20037c17
posture832_20037c15:
        PUSH 0xc
posture832_20037c17:
        MOV EAX,dword ptr [ECX + posture832pm8]
        MOV ECX,dword ptr [EAX + posture832character40]
        PUSH ECX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        ADD ESP,0x10
        TEST EAX,EAX
        JGE posture832_20037dd3
        MOV ECX,dword ptr [pm]
posture832_20037c35:
        MOV EDX,dword ptr [ECX + posture832pm0]
        TEST byte ptr [EDX + posture832psf0],BL
        JNZ posture832_20037c58
        FLD dword ptr [EDX + posture832ps3c]
        FCOMP dword ptr [posture832Const200ac100]
        FNSTSW AX
        TEST AH,0x40
        JZ posture832_20037c58
        PUSH 0x1
        PUSH 0x8
        JMP posture832_20037d7d
posture832_20037c58:
        PUSH 0x1
        PUSH EBX
        JMP posture832_20037d7d
posture832_20037c60:
        TEST AL,0x10
        JZ posture832_20037cbb
        TEST AL,0x20
        JNZ posture832_20037cbb
        MOV AL,byte ptr [ECX + posture832pm21]
        MOV dword ptr [ESP + 0x10],0x3e8f5c29
        TEST AL,AL
        JZ posture832_20037ca8
        MOV BL,byte ptr [ECX + posture832pm20]
        TEST BL,BL
        JNZ posture832_20037ca8
        TEST AL,AL
        PUSH 0x1
        JLE posture832_20037c88
        PUSH 0xb
        JMP posture832_20037c8a
posture832_20037c88:
        PUSH 0xc
posture832_20037c8a:
        MOV EAX,dword ptr [ECX + posture832pm8]
        MOV ECX,dword ptr [EAX + posture832character40]
        PUSH ECX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        ADD ESP,0x10
        TEST EAX,EAX
        JGE posture832_20037dd3
        MOV ECX,dword ptr [pm]
posture832_20037ca8:
        MOV EDX,dword ptr [ECX + posture832pm8]
        MOV ECX,dword ptr [ECX + posture832pm0]
        PUSH 0x1
        PUSH 0x3
        MOV EAX,dword ptr [EDX + posture832character40]
        PUSH EAX
        PUSH ECX
        JMP posture832_20037d85
posture832_20037cbb:
        MOV AL,byte ptr [EDX + posture832psf0]
        MOV EBX,0x4
        TEST BL,AL
        JNZ posture832_20037d0b
        FLD dword ptr [EDX + posture832ps3c]
        FCOMP dword ptr [posture832Const200ac100]
        FNSTSW AX
        TEST AH,0x40
        JZ posture832_20037d0b
        FLD dword ptr [ECX + posture832pm114]
        FCOMP dword ptr [posture832Const200ac7f0]
        FNSTSW AX
        TEST AH,0x41
        JNZ posture832_20037cfc
        MOV dword ptr [ESP + 0x10],0x3ee66666
        MOV EBP,0x3
        JMP posture832_20037d18
posture832_20037cfc:
        MOV dword ptr [ESP + 0x10],0x3eb851ec
        MOV EBP,0x2
        JMP posture832_20037d18
posture832_20037d0b:
        MOV dword ptr [ESP + 0x10],0x3e8f5c29
        MOV EBP,0x1
posture832_20037d18:
        TEST ESI,0x100000
        JZ posture832_20037d22
        XOR EBP,EBP
posture832_20037d22:
        MOV AL,byte ptr [ECX + posture832pm21]
        TEST AL,AL
        JZ posture832_20037d59
        CMP byte ptr [ECX + posture832pm20],0x0
        JNZ posture832_20037d59
        TEST AL,AL
        PUSH 0x1
        JLE posture832_20037d39
        PUSH 0xb
        JMP posture832_20037d3b
posture832_20037d39:
        PUSH 0xc
posture832_20037d3b:
        MOV EAX,dword ptr [ECX + posture832pm8]
        MOV ECX,dword ptr [EAX + posture832character40]
        PUSH ECX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        ADD ESP,0x10
        TEST EAX,EAX
        JGE posture832_20037dd3
        MOV ECX,dword ptr [pm]
posture832_20037d59:
        MOV EDX,dword ptr [ECX + posture832pm0]
        TEST byte ptr [EDX + posture832psf0],BL
        JNZ posture832_20037d79
        FLD dword ptr [EDX + posture832ps3c]
        FCOMP dword ptr [posture832Const200ac100]
        FNSTSW AX
        TEST AH,0x40
        JZ posture832_20037d79
        PUSH 0x1
        PUSH 0x7
        JMP posture832_20037d7d
posture832_20037d79:
        PUSH 0x1
        PUSH 0x3
posture832_20037d7d:
        MOV EAX,dword ptr [ECX + posture832pm8]
        MOV ECX,dword ptr [EAX + posture832character40]
        PUSH ECX
        PUSH EDX
posture832_20037d85:
        CALL BG_AnimScriptAnimation
        ADD ESP,0x10
        MOV ECX,EAX
posture832_20037d8f:
        TEST ECX,ECX
        JGE posture832_20037dd3
        MOV EAX,[pm]
        PUSH 0x1
        PUSH 0x1
        MOV EDX,dword ptr [EAX + posture832pm8]
        MOV ECX,dword ptr [EDX + posture832character40]
        MOV EDX,dword ptr [EAX + posture832pm0]
        PUSH ECX
        PUSH EDX
        CALL BG_AnimScriptAnimation
        MOV EAX,[pm]
        ADD ESP,0x10
        MOV EAX,dword ptr [EAX + posture832pm0]
        MOV EDX,dword ptr [EAX + posture832psf0]
        AND EDX,0xffffffdf
        MOV dword ptr [EAX + posture832psf0],EDX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ECX + posture832pm0]
        AND dword ptr [EAX + posture832ps68],0xfffeffff
posture832_20037dd3:
        MOV EAX,[pml.tceContentRestriction]
        FLD dword ptr [ESP + 0x10]
        TEST EAX,EAX
        JZ posture832_20037de6
        FMUL dword ptr [posture832Const200ac7ec]
posture832_20037de6:
        MOV EDX,dword ptr [pm]
        FILD dword ptr [pml.msec]
        MOV ESI,dword ptr [EDX + posture832pm0]
        MOV EDI,dword ptr [ESI + posture832ps8]
        FMUL ST(0),ST(1)
        MOV dword ptr [ESP + 0x10],EDI
        FIADD dword ptr [ESP + 0x10]
        CALL PM_MovementTruncate827
        AND EAX,0xff
        ADD EDI,0x40
        MOV dword ptr [ESI + posture832ps8],EAX
        MOV ECX,dword ptr [pm]
        FSTP ST(0)
        MOV EAX,dword ptr [ECX + posture832pm0]
        MOV EDX,dword ptr [EAX + posture832ps8]
        ADD EDX,0x40
        XOR EDX,EDI
        TEST DL,0x80
        JZ posture832_20037e9c
        MOV EAX,dword ptr [ECX + posture832pm110]
        TEST EAX,EAX
        JNZ posture832_20037e84
        MOV EAX,dword ptr [ECX + posture832pm4c]
        TEST EAX,EAX
        JNZ posture832_20037e9c
        CMP EBP,0x1
        JNZ posture832_20037e53
        CALL PM_FootstepForSurface
        PUSH EAX
        PUSH 0x8b
        CALL PM_AddEventExt
        ADD ESP,0x8
        JMP posture832_20037ea0
posture832_20037e53:
        CMP EBP,0x2
        JNZ posture832_20037e6a
        CALL PM_FootstepForSurface
        PUSH EAX
        PUSH 0x1
        CALL PM_AddEventExt
        ADD ESP,0x8
        JMP posture832_20037ea0
posture832_20037e6a:
        CMP EBP,0x3
        JNZ posture832_20037e9c
        CALL PM_FootstepForSurface
        PUSH EAX
        PUSH 0x8a
        CALL PM_AddEventExt
        ADD ESP,0x8
        JMP posture832_20037ea0
posture832_20037e84:
        CMP EAX,0x1
        JNZ posture832_20037e8d
        PUSH 0x9
        JMP posture832_20037e94
posture832_20037e8d:
        CMP EAX,0x2
        JNZ posture832_20037e9c
        PUSH 0xb
posture832_20037e94:
        CALL PM_AddEvent
        ADD ESP,0x4
posture832_20037e9c:
        TEST EBP,EBP
        JZ posture832_20037eb6
posture832_20037ea0:
        MOV EAX,[pm]
        POP EDI
        POP ESI
        POP EBP
        MOV EAX,dword ptr [EAX + posture832pm0]
        POP EBX
        AND dword ptr [EAX + posture832psf0],0xfffffffd
        POP ECX
        RET
posture832_20037eb4:
        FSTP ST(0)
posture832_20037eb6:
        POP EDI
        POP ESI
        POP EBP
        POP EBX
        POP ECX
        RET
    }
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
            ? TCE_PM_ReloadAnimForWeapon(pm->ps->weapon, pm->skill[SK_LIGHT_WEAPONS])
            : TCE_PM_NonemptyReloadAnimForWeapon(pm->ps->weapon, pm->skill[SK_LIGHT_WEAPONS]);
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
    PM_StartWeaponAnim(alt ? PM_AltSwitchFromForWeapon(oldweapon) : PM_DropAnimForWeapon(oldweapon));
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
    pm->ps->weaponTime+=250;
}




/*
===============
PM_FinishWeaponChange
===============
*/
static void PM_FinishWeaponChange(void) {
    int oldweapon=pm->ps->weapon,newweapon=pm->ps->nextWeapon,bodyEvent;
    qboolean altSwitch=qfalse,animate=qtrue,alt;
    if(newweapon<0 || newweapon>=TCE_MAX_WEAPONS) newweapon=0;
    if(!COM_BitCheck(pm->ps->weapons,newweapon)) newweapon=0;
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
    pm->ps->weaponTime+=250;
    BG_UpdateConditionValue(pm->ps->clientNum,ANIM_COND_WEAPON,newweapon,qtrue);
    if(!animate) return;
    if(altSwitch) bodyEvent=(pm->ps->eFlags & EF_PRONE)?ANIM_ET_DO_ALT_WEAPON_MODE_PRONE:ANIM_ET_DO_ALT_WEAPON_MODE;
    else bodyEvent=(pm->ps->eFlags & EF_PRONE)?ANIM_ET_RAISEWEAPONPRONE:ANIM_ET_RAISEWEAPON;
    BG_AnimScriptEvent(pm->ps,pm->character->animModelInfo,bodyEvent,qfalse,qfalse);
    PM_StartWeaponAnim(alt ? PM_AltSwitchToForWeapon(newweapon) : PM_RaiseAnimForWeapon(newweapon));
}




/*
==============
PM_ReloadClip
==============
*/
static void PM_ReloadClip(int weapon) {
    /* TC reserve/clip semantics also apply before gearDef.parsed is set. */
    if (weapon >= 0 && weapon < TCE_MAX_WEAPONS)
        TCE_PM_ReloadClip(weapon, pm->ps->ammo, pm->ps->ammoclip, weaponDef);
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
            /* Original immutable legacy maxclip entries57..59 are zero. */
            if (!(pm->cmd.wbuttons & WBUTTON_RELOAD) || !pm->ps->ammo[ammoIndex] ||
                pm->ps->ammoclip[clipIndex] >= 0) return;
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
    return weapon == 31 || weapon == 34 ? 1500 : 0;
}
static int PM_TCECoolRate(int weapon) {
    return weapon == 31 || weapon == 34 ? 300 : 0;
}

#if defined(_MSC_VER) && defined(_M_IX86)
static void PM_WeaponTruncateST0(void);
#endif

/* TC:E qagame20031e50. The Windows arithmetic retains ST0 until __ftol. */
void PM_CoolWeapons(void) {
    int weapon, maxHeat;
    for (weapon = 0; weapon < 64; ++weapon) {
        if (COM_BitCheck(pm->ps->weapons, weapon) && pm->ps->weapHeat[weapon]) {
#if defined(_MSC_VER) && defined(_M_IX86)
            int heatValue = pm->ps->weapHeat[weapon];
            int coolingRate = PM_TCECoolRate(weapon);
            float coolingFrame = pml.frametime;
            int coolingDouble = pm->skill[SK_HEAVY_WEAPONS] >= 2 &&
                pm->ps->stats[STAT_PLAYER_CLASS] == PC_SOLDIER;
            __asm {
                fild heatValue
                fild coolingRate
                fmul coolingFrame
                cmp coolingDouble, 0
                je coolingSubtract
                fadd st(0), st(0)
            coolingSubtract:
                fsubp st(1), st(0)
                call PM_WeaponTruncateST0
                mov heatValue, eax
            }
            pm->ps->weapHeat[weapon] = heatValue;
#else
            float cooling = PM_TCECoolRate(weapon) * pml.frametime;
            if (pm->skill[SK_HEAVY_WEAPONS] >= 2 &&
                pm->ps->stats[STAT_PLAYER_CLASS] == PC_SOLDIER) cooling += cooling;
            pm->ps->weapHeat[weapon] = (int)(pm->ps->weapHeat[weapon] - cooling);
#endif
            if (pm->ps->weapHeat[weapon] < 0) pm->ps->weapHeat[weapon] = 0;
        }
    }
    if (!pm->ps->weapon) return;
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        static const float coolingMountedScale = 0.0006666666595265269f;
        static const float coolingDisplayScale = 255.0f;
        static double (__cdecl *const coolingFloor)(double) = floor;
        int heatValue;
        int mountedHeat = pm->ps->persistant[PERS_HWEAPON_USE] ||
            (pm->ps->eFlags & EF_MOUNTEDTANK);
        if (mountedHeat) heatValue = pm->ps->weapHeat[34];
        else {
            maxHeat = PM_TCEMaxHeat(pm->ps->weapon);
            if (!maxHeat) { pm->ps->curWeapHeat = 0; return; }
            heatValue = pm->ps->weapHeat[pm->ps->weapon];
        }
        __asm {
            fild heatValue
            cmp mountedHeat, 0
            je coolingDivide
            fmul coolingMountedScale
            jmp coolingDisplay
        coolingDivide:
            fidiv maxHeat
        coolingDisplay:
            fmul coolingDisplayScale
            sub esp, 8
            fstp qword ptr [esp]
            call dword ptr [coolingFloor]
            add esp, 8
            call PM_WeaponTruncateST0
            mov heatValue, eax
        }
        pm->ps->curWeapHeat = heatValue;
    }
#else
    if (pm->ps->persistant[PERS_HWEAPON_USE] || (pm->ps->eFlags & EF_MOUNTEDTANK)) {
        pm->ps->curWeapHeat = (int)floor((pm->ps->weapHeat[34] * (1.0f / 1500.0f)) * 255.0f);
    } else {
        maxHeat = PM_TCEMaxHeat(pm->ps->weapon);
        if (!maxHeat) { pm->ps->curWeapHeat = 0; return; }
        pm->ps->curWeapHeat = (int)floor(((float)pm->ps->weapHeat[pm->ps->weapon] / maxHeat) * 255.0f);
    }
#endif
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

/* Original signed gates after wrapping SUB/ADD,3001086c..30010893. */
static qboolean PM_TCEAimPostureReady(int commandTime, int proneTime) {
#if defined(_MSC_VER) && defined(_M_IX86)
    int aimPostureReady;
    __asm {
        mov eax, commandTime
        mov edx, proneTime
        mov ecx, eax
        sub ecx, edx
        cmp ecx, 200
        jl aim_posture_wait
        add edx, eax
        cmp edx, 200
        jl aim_posture_wait
        mov aimPostureReady, 1
        jmp aim_posture_done
aim_posture_wait:
        mov aimPostureReady, 0
aim_posture_done:
    }
    return aimPostureReady;
#else
    return (int)((unsigned int)commandTime - (unsigned int)proneTime) >= 200 &&
        (int)((unsigned int)proneTime + (unsigned int)commandTime) >= 200;
#endif
}

/* Windows PM_Weapon 30010740..30010a49: requested/active aiming and
 * key-edge latch. This is a complete branch, not the whole weapon controller. */
static qboolean PM_TCEAimInput(qboolean delayedFire) {
    playerState_t *ps = pm->ps;
    int flags = ps->stats[STAT_TCE_WEAPON_FLAGS];
    qboolean pressed = (pm->cmd.wbuttons & WBUTTON_ZOOM) != 0;
    qboolean tryAim = qfalse;
    if (!pressed && (flags & 0x8000)) {
        ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x8000;
    } else {
        if (pressed && !(flags & 0x8000) && ps->weaponTime <= 0 &&
            !delayedFire && ps->weaponstate == WEAPON_READY) tryAim = qtrue;
        else if (ps->weaponTime <= 0 && (flags & 12) == 8) tryAim = qtrue;
        if (tryAim && ps->weaponstate != WEAPON_DROPPING &&
            ps->weaponstate != WEAPON_RELOADING && ps->weaponstate != 10 && ps->weaponstate != 11 &&
            ps->weapon != 1 && ps->weapon != 21 && ps->weapon != 19 && ps->weapon != 12 && ps->weapon != 15 &&
            (!(pm->cmd.buttons & BUTTON_SPRINT) || (!pm->cmd.forwardmove && !pm->cmd.rightmove) ||
             (ps->pm_flags & PMF_DUCKED) || (ps->eFlags & EF_PRONE)) &&
            pm->waterlevel < 3 && (pm->waterlevel < 1 || ps->groundEntityNum != ENTITYNUM_NONE) &&
            !pml.ladder && ps->velocity[2] >= -360.0f && !(flags & 0x1000) &&
            PM_TCEAimPostureReady(pm->cmd.serverTime, pm->pmext->proneTime) &&
            !(ps->eFlags & EF_PRONE_MOVING) && ps->weapon) {
            if (pressed) ps->stats[STAT_TCE_WEAPON_FLAGS] = (flags | 0x8000) ^ 8;
            flags = ps->stats[STAT_TCE_WEAPON_FLAGS];
            if ((flags & 12) == 4 || (flags & 12) == 8) {
                ps->stats[STAT_TCE_WEAPON_FLAGS] ^= 4;
                ps->stats[STAT_TCE_MOVEMENT_INSTABILITY] = 1000;
                if (ps->weaponstate != WEAPON_FIRING) {
                    unsigned int seed = (unsigned int)ps->stats[STAT_TCE_SHOT_SEED];
#if defined(_MSC_VER) && defined(_M_IX86)
                    const float aimPhaseScale = 1000.0f;
                    int *aimPhase = &ps->stats[STAT_TCE_AIM_PHASE];
                    __asm {
                        lea eax, seed
                        push eax
                        call Q_random
                        fmul aimPhaseScale
                        add esp, 4
                        call PM_WeaponTruncateST0
                        mov ecx, aimPhase
                        mov dword ptr [ecx], eax
                    }
#else
                    /* Original3007dd50 returns the masked random value in x87
                     * precision; do not round its scaled result through float. */
                    seed = 69069u * seed + 1u;
                    ps->stats[STAT_TCE_AIM_PHASE] = (int)((double)(seed & 65535u) * (1000.0 / 65536.0));
#endif
                    if (ps->stats[STAT_TCE_AIM_PHASE] > 1000) ps->stats[STAT_TCE_AIM_PHASE] -= 1000;
                    if (!weaponDef[ps->weapon].noTacMode && (ps->stats[STAT_TCE_WEAPON_FLAGS] & 4)) {
#if defined(_MSC_VER) && defined(_M_IX86)
                        const float aimTurn = 6.2831854820251465f;
                        const double aimPitchScale = 200.0, aimYawScale = 400.0, aimBias = 2000.0;
                        int *aimOffsets = &ps->holdable[5];
                        __asm {
                            lea eax, seed
                            push eax
                            call Q_random
                            fmul aimTurn
                            add esp, 4
                            fld st(0)
                            fcos
                            fmul aimPitchScale
                            fadd aimBias
                            call PM_WeaponTruncateST0
                            fsin
                            mov ecx, aimOffsets
                            mov dword ptr [ecx], eax
                            fmul aimYawScale
                            fadd aimBias
                            call PM_WeaponTruncateST0
                            mov ecx, aimOffsets
                            mov dword ptr [ecx+4], eax
                        }
#else
                        double angle;
                        seed = 69069u * seed + 1u;
                        angle = ((double)(seed & 65535u) / 65536.0) * (double)6.2831854820251465f;
                        ps->holdable[5] = (int)(cos(angle) * 200.0 + 2000.0);
                        ps->holdable[6] = (int)(sin(angle) * 400.0 + 2000.0);
#endif
                    }
                    ps->stats[STAT_TCE_SHOT_SEED] = seed & 0xffff;
                }
#if defined(_MSC_VER) && defined(_M_IX86)
                {
                    int *aimTimer = &ps->weaponTime;
                    __asm {
                        mov eax, aimTimer
                        add dword ptr [eax], 300
                    }
                }
#else
                ps->weaponTime = (int)((unsigned int)ps->weaponTime + 300u);
#endif
                PM_AddEvent(EV_TCE_TOGGLE_AIMING);
                return qtrue;
            }
        }
    }
    flags = ps->stats[STAT_TCE_WEAPON_FLAGS];
    if (pressed && (flags & 0x800c) == 8)
        ps->stats[STAT_TCE_WEAPON_FLAGS] = (flags | 0x8000) & ~8;
    return qfalse;
}

/* PmoveSingle 3000c180: movement/reload can cancel active aim after PM_Weapon. */
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

#if defined(_MSC_VER) && defined(_M_IX86)
/* Whole qagame20031f90; original stack locals, native structure offsets. */
enum {
    aimPs68 = offsetof(playerState_t, eFlags),
    aimPs48c = offsetof(playerState_t, aimSpreadScale),
    aimPs488 = offsetof(playerState_t, aimSpreadScaleFloat),
    aimPsa4 = offsetof(playerState_t, weapon),
    aimPsa8 = offsetof(playerState_t, weaponstate),
    aimPsc = offsetof(playerState_t, pm_flags),
    aimPs20 = offsetof(playerState_t, velocity),
    aimPsf4 = offsetof(playerState_t, stats) + STAT_TCE_FLAGS * sizeof(int),
    aimPsf0 = offsetof(playerState_t, stats) + STAT_TCE_WEAPON_FLAGS * sizeof(int),
    aimPsfc = offsetof(playerState_t, stats) + STAT_TCE_MOVEMENT_INSTABILITY * sizeof(int),
    aimPs104 = offsetof(playerState_t, stats) + STAT_TCE_AIM_PHASE * sizeof(int),
    aimPsec = offsetof(playerState_t, stats) + STAT_TCE_SHOT_SEED * sizeof(int),
    aimPs100 = offsetof(playerState_t, stats) + STAT_TCE_SHOT_INSTABILITY * sizeof(int),
    aimCmdTime = offsetof(pmove_t, cmd.serverTime),
    aimOldTime = offsetof(pmove_t, oldcmd.serverTime),
    aimOldAngles = offsetof(pmove_t, oldcmd.angles),
    aimAngleDelta = offsetof(pmove_t, cmd.angles) - offsetof(pmove_t, oldcmd.angles),
    aimDefStride = sizeof(tce_weaponDef_t),
    aimMoveRecovery = offsetof(tce_weaponDef_t, unknown_0f8) + 4 * sizeof(int),
    aimShotRecovery = offsetof(tce_weaponDef_t, unknown_0f8) + 5 * sizeof(int),
    aimScoped = offsetof(tce_weaponDef_t, scoped)
};
static const unsigned int aimConstant200ac378[2] = { 0xd2f1a9fcu, 0x3f50624du };
static const unsigned int aimConstant200ac488[2] = { 0x00000000u, 0x3f768000u };
static const unsigned int aimConstant200ac708[2] = { 0x00000000u, 0x3ff80000u };
static const unsigned int aimConstant200ac100 = 0x00000000u;
static const unsigned int aimConstant200ac704 = 0x3f4ccccdu;
static const unsigned int aimConstant200ac190 = 0x41f00000u;
static const unsigned int aimConstant200ac700 = 0x42f00000u;
static const unsigned int aimConstant200ac6fc = 0x3c088889u;
static const unsigned int aimConstant200ac6f8 = 0x45480000u;
static const unsigned int aimConstant200ac3ac = 0x447a0000u;
static const unsigned int aimConstant200ac6f4 = 0x3f2b851fu;
__declspec(naked) void PM_AdjustAimSpreadScale(void) {
    __asm {

        SUB ESP,010h

        PUSH EBX

        PUSH ESI

        PUSH EDI

        MOV EDI,dword ptr [pm]

        XOR EBX,EBX

        MOV ESI,dword ptr [EDI]

        TEST dword ptr [ESI + aimPs68],040000h

        JZ aimAt20031fcb

        MOV dword ptr [ESI + aimPs48c],0ffh

        MOV EAX,[pm]

        POP EDI

        POP ESI

        MOV ECX,dword ptr [EAX]

        POP EBX

        MOV dword ptr [ECX + aimPs488],0437f0000h

        ADD ESP,010h

        RET
    aimAt20031fcb:
        MOV EDX,dword ptr [EDI + aimCmdTime]

        MOV EAX,dword ptr [EDI + aimOldTime]

        MOV ECX,dword ptr [ESI + aimPsa4]

        SUB EDX,EAX

        MOV dword ptr [ESP + 014h],EDX

        FILD dword ptr [ESP + 014h]

        IMUL EAX,ECX,aimDefStride

        NOP

        FMUL qword ptr [aimConstant200ac378]

        NOP

        NOP

        FSTP dword ptr [ESP + 010h]

        MOV EAX,dword ptr [weaponDef + EAX + aimMoveRecovery]

        TEST EAX,EAX

        MOV dword ptr [ESP + 014h],EAX

        JZ aimAt20032120

        FILD dword ptr [ESP + 014h]

        LEA EAX,[EDI + aimOldAngles]

        MOV ECX,02h

        FMUL dword ptr [ESP + 010h]

        FSTP dword ptr [ESP + 014h]

        FLD dword ptr [aimConstant200ac100]
    aimAt20032023:
        FILD dword ptr [EAX + aimAngleDelta]

        ADD EAX,04h

        DEC ECX

        FMUL qword ptr [aimConstant200ac488]

        FILD dword ptr [EAX + -04h]

        FMUL qword ptr [aimConstant200ac488]

        FSUBP ST(1),ST(0)

        FABS

        FMUL qword ptr [aimConstant200ac708]

        FXCH ST(1)

        FADDP ST(1),ST(0)

        JNZ aimAt20032023

        LEA EAX,[ESI + aimPs20]

        MOV ECX,02h
    aimAt20032051:
        FLD dword ptr [EAX]

        FABS

        FXCH ST(1)

        FADDP ST(1),ST(0)

        ADD EAX,04h

        DEC ECX

        JNZ aimAt20032051

        MOV EAX,dword ptr [ESI + aimPsf4]

        TEST AH,02h

        JZ aimAt20032070

        FMUL dword ptr [aimConstant200ac704]
    aimAt20032070:
        FDIV dword ptr [ESP + 010h]

        FLD ST(0)

        FSUB dword ptr [aimConstant200ac190]

        FST dword ptr [ESP + 0ch]

        FCOMP dword ptr [aimConstant200ac100]

        FNSTSW AX

        TEST AH,041h

        JZ aimAt20032097

        MOV dword ptr [ESP + 0ch],00h

        JMP aimAt200320b0
    aimAt20032097:
        FLD dword ptr [ESP + 0ch]

        FCOMP dword ptr [aimConstant200ac700]

        FNSTSW AX

        TEST AH,041h

        JNZ aimAt200320b0

        MOV dword ptr [ESP + 0ch],042f00000h
    aimAt200320b0:
        FCOM dword ptr [aimConstant200ac100]

        FNSTSW AX

        TEST AH,041h

        JZ aimAt200320c7

        FSTP ST(0)

        FLD dword ptr [aimConstant200ac100]

        JMP aimAt200320dc
    aimAt200320c7:
        FCOM dword ptr [aimConstant200ac700]

        FNSTSW AX

        TEST AH,041h

        JNZ aimAt200320dc

        FSTP ST(0)

        FLD dword ptr [aimConstant200ac700]
    aimAt200320dc:
        FLD dword ptr [ESP + 0ch]

        FMUL dword ptr [aimConstant200ac6fc]

        FMUL dword ptr [ESP + 010h]

        FMUL dword ptr [aimConstant200ac6f8]

        CALL PM_WeaponTruncateST0

        MOV dword ptr [ESP + 0ch],EAX

        FILD dword ptr [ESP + 0ch]

        FSTP dword ptr [ESP + 0ch]

        FMUL dword ptr [aimConstant200ac6fc]

        FMUL dword ptr [ESP + 010h]

        FMUL dword ptr [aimConstant200ac6f8]

        CALL PM_WeaponTruncateST0

        MOV dword ptr [ESP + 018h],EAX

        FILD dword ptr [ESP + 018h]

        JMP aimAt20032136
    aimAt20032120:
        FLD dword ptr [aimConstant200ac100]

        MOV dword ptr [ESP + 0ch],00h

        MOV dword ptr [ESP + 014h],0447a0000h
    aimAt20032136:
        MOV ECX,dword ptr [ESI + aimPsc]

        AND ECX,01h

        JZ aimAt20032169

        MOV EAX,dword ptr [ESI + aimPsf0]

        TEST AL,010h

        JNZ aimAt20032169

        OR AL,010h

        MOV EBX,01h

        MOV dword ptr [ESI + aimPsf0],EAX

        MOV ECX,dword ptr [pm]

        MOV EDX,dword ptr [ECX]

        MOV dword ptr [EDX + aimPsfc],02710h

        JMP aimAt20032195
    aimAt20032169:
        TEST ECX,ECX

        JNZ aimAt2003219b

        MOV EAX,dword ptr [ESI + aimPsf0]

        TEST AL,010h

        JZ aimAt2003219b

        AND AL,0efh

        MOV EBX,01h

        MOV dword ptr [ESI + aimPsf0],EAX

        MOV EAX,[pm]

        MOV ECX,dword ptr [EAX]

        MOV dword ptr [ECX + aimPsfc],02710h
    aimAt20032195:
        MOV EDI,dword ptr [pm]
    aimAt2003219b:
        MOV ESI,dword ptr [EDI]

        CMP dword ptr [ESI + aimPsa8],07h

        JZ aimAt2003222f

        FADD ST(0),ST(0)

        FIADD dword ptr [ESI + aimPs104]

        CALL PM_WeaponTruncateST0

        MOV dword ptr [ESI + aimPs104],EAX

        MOV EDI,dword ptr [pm]

        MOV EDX,dword ptr [EDI]

        MOV EAX,dword ptr [EDX + aimPsa8]

        TEST EAX,EAX

        JNZ aimAt200321d3

        TEST EBX,EBX

        JZ aimAt2003220b
    aimAt200321d3:
        MOV EAX,dword ptr [EDI]

        LEA EDX,[ESP + 018h]

        PUSH EDX

        MOV ECX,dword ptr [EAX + aimPsec]

        MOV dword ptr [ESP + 01ch],ECX

        CALL Q_random

        FMUL dword ptr [aimConstant200ac3ac]

        ADD ESP,04h

        CALL PM_WeaponTruncateST0

        MOV ECX,dword ptr [pm]

        MOV EDX,dword ptr [ECX]

        MOV dword ptr [EDX + aimPs104],EAX

        MOV EDI,dword ptr [pm]
    aimAt2003220b:
        MOV EAX,dword ptr [EDI]

        MOV ECX,dword ptr [EAX + aimPs104]

        CMP ECX,03e8h

        JLE aimAt20032231

        ADD ECX,0fffffc18h

        MOV dword ptr [EAX + aimPs104],ECX

        MOV EDI,dword ptr [pm]

        JMP aimAt20032231
    aimAt2003222f:
        FSTP ST(0)
    aimAt20032231:
        MOV EDI,dword ptr [EDI]

        TEST byte ptr [EDI + aimPsf0],04h

        JZ aimAt20032283

        MOV ECX,dword ptr [EDI + aimPsa4]

        IMUL EAX,ECX,aimDefStride

        NOP

        NOP

        NOP

        FLD dword ptr [weaponDef + EAX + aimScoped]

        FCOMP dword ptr [aimConstant200ac100]

        FNSTSW AX

        TEST AH,040h

        JZ aimAt20032283

        FLD dword ptr [ESP + 0ch]

        FMUL dword ptr [aimConstant200ac6f4]

        CMP dword ptr [EDI + aimPsfc],0c8h

        FSTP dword ptr [ESP + 0ch]

        JLE aimAt20032283

        MOV dword ptr [ESP + 0ch],00h
    aimAt20032283:
        FLD dword ptr [ESP + 0ch]

        FSUB dword ptr [ESP + 014h]

        FIADD dword ptr [EDI + aimPsfc]

        CALL PM_WeaponTruncateST0

        MOV dword ptr [EDI + aimPsfc],EAX

        MOV ECX,dword ptr [pm]

        MOV EAX,dword ptr [ECX]

        MOV ECX,dword ptr [EAX + aimPsfc]

        CMP ECX,03e8h

        JLE aimAt200322be

        MOV dword ptr [EAX + aimPsfc],03e8h

        JMP aimAt200322cc
    aimAt200322be:
        TEST ECX,ECX

        JGE aimAt200322cc

        MOV dword ptr [EAX + aimPsfc],00h
    aimAt200322cc:
        MOV EDX,dword ptr [pm]

        MOV ESI,dword ptr [EDX]

        MOV ECX,dword ptr [ESI + aimPsa4]

        FILD dword ptr [ESI + aimPs100]

        IMUL EAX,ECX,aimDefStride

        NOP

        NOP

        NOP

        FILD dword ptr [weaponDef + EAX + aimShotRecovery]

        FMUL dword ptr [ESP + 010h]

        FSUBP ST(1),ST(0)

        CALL PM_WeaponTruncateST0

        MOV dword ptr [ESI + aimPs100],EAX

        MOV ECX,dword ptr [pm]

        MOV EAX,dword ptr [ECX]

        MOV ECX,dword ptr [EAX + aimPs100]

        CMP ECX,03e8h

        JLE aimAt2003232a

        POP EDI

        POP ESI

        MOV dword ptr [EAX + aimPs100],03e8h

        POP EBX

        ADD ESP,010h

        RET
    aimAt2003232a:
        TEST ECX,ECX

        JGE aimAt20032338

        MOV dword ptr [EAX + aimPs100],00h
    aimAt20032338:
        POP EDI

        POP ESI

        POP EBX

        ADD ESP,010h

        RET
    }
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

/* Original common recoil recovery3001020b, shared by all weapons. */
#if defined(_MSC_VER) && defined(_M_IX86)
static void PM_WeaponTruncateST0(void);
#endif
static void PM_TCERecoverHipRecoil(void) {
    int *remaining = &pm->ps->stats[STAT_TCE_RECOIL_REMAINDER];
    if (*remaining > 0) {
#if defined(_MSC_VER) && defined(_M_IX86)
        const float recoveryRate = 1.333f;
        int recoveryMsec = pml.msec;
        int recovered;
        int *recoveryPitch = &pm->ps->delta_angles[PITCH];
        __asm {
            fild recoveryMsec
            fmul recoveryRate
            call PM_WeaponTruncateST0
            mov recovered, eax
            mov ecx, remaining
            mov edx, dword ptr [ecx]
            cmp edx, eax
            jge recovery_subtract
            mov recovered, edx
            mov dword ptr [ecx], 0
            jmp recovery_pitch
        recovery_subtract:
            sub edx, eax
            mov dword ptr [ecx], edx
        recovery_pitch:
            fild recovered
            fadd st(0), st(0)
            mov ecx, recoveryPitch
            fiadd dword ptr [ecx]
            call PM_WeaponTruncateST0
            mov ecx, recoveryPitch
            mov dword ptr [ecx], eax
        }
#else
        int recovered = (int)((double)pml.msec * (double)1.333f);
        if (recovered > *remaining) recovered = *remaining;
        *remaining -= recovered;
        pm->ps->delta_angles[PITCH] += 2 * recovered;
#endif
    }
}

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
/* Private PM_Weapon2003a73d..2003a91c common shot prelude. */
static const double pre842K200ac1e0 = 3.0;
static const float pre842K200ac6ec = 255.0f;
static const float pre842K200ac484 = 0.0010000000474974513f;
static const float pre842K200ac3ac = 1000.0f;
static const float pre842K200ac190 = 30.0f;
enum {
    pre842SpreadF = offsetof(playerState_t,aimSpreadScaleFloat),
    pre842SpreadI = offsetof(playerState_t,aimSpreadScale),
    pre842Move = offsetof(playerState_t,stats)+STAT_TCE_MOVEMENT_INSTABILITY*4,
    pre842Shot = offsetof(playerState_t,stats)+STAT_TCE_SHOT_INSTABILITY*4,
    pre842Hold0 = offsetof(playerState_t,holdable),
    pre842Hold1 = offsetof(playerState_t,holdable)+4,
    pre842Weapon = offsetof(playerState_t,weapon),
    pre842Flags = offsetof(playerState_t,stats)+STAT_TCE_WEAPON_FLAGS*4,
    pre842Eflags = offsetof(playerState_t,eFlags),
    pre842Velocity = offsetof(playerState_t,velocity),
    pre842View0 = offsetof(playerState_t,viewangles),
    pre842View1 = offsetof(playerState_t,viewangles)+4,
    pre842View2 = offsetof(playerState_t,viewangles)+8,
    pre842Ext = offsetof(pmove_t,pmext),
    pre842Angles0 = offsetof(pmoveExt_t,tceShotAngles),
    pre842Angles1 = offsetof(pmoveExt_t,tceShotAngles)+4,
    pre842Angles2 = offsetof(pmoveExt_t,tceShotAngles)+8,
    pre842Minimum = offsetof(tce_weaponDef_t,unknown_0f8)+6*4,
    pre842Tac = offsetof(tce_weaponDef_t,unknown_0f8)+8*4,
    pre842Hip = offsetof(tce_weaponDef_t,unknown_0f8)+7*4
};
static __declspec(naked) void PM_TCEShotPrelude842(int spreadAddition, int *recoilResult, float *postureResult) {
    __asm {
        sub esp, 0x38
        push ebx
        push ebp
        push esi
        push edi
        mov eax, dword ptr [esp+0x4c]
        mov dword ptr [esp+0x18], eax
        mov ebp, 1000
        fild dword ptr [esp + 0x18]
        mov edx, dword ptr [pm]
        fmul qword ptr [pre842K200ac1e0]
        mov eax, dword ptr [edx]
        fadd dword ptr [eax+pre842SpreadF]
        fstp dword ptr [eax+pre842SpreadF]
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        fld dword ptr [ecx+pre842SpreadF]
        fcomp dword ptr [pre842K200ac6ec]
        fnstsw ax
        test ah, 0x41
        jne pre842_2003a77f
        mov dword ptr [ecx+pre842SpreadF], 0x437f0000
pre842_2003a77f:
        mov ecx, dword ptr [pm]
        mov esi, dword ptr [ecx]
        fld dword ptr [esi+pre842SpreadF]
        call PM_WeaponTruncateST0
        mov dword ptr [esi+pre842SpreadI], eax
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+pre842Move]
        mov dword ptr [eax+pre842Hold0], ecx
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+pre842Shot]
        mov dword ptr [eax+pre842Hold1], ecx
        mov edx, dword ptr [pm]
        mov esi, dword ptr [edx]
        fild dword ptr [esi+pre842Shot]
        mov ecx, dword ptr [esi+pre842Weapon]
        fld st(0)
        fmul dword ptr [pre842K200ac484]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        fsqrt 
        sub eax, ecx
        lea eax, [eax + eax*4]
        fild dword ptr [eax*4 + weaponDef+pre842Minimum]
        fmul dword ptr [pre842K200ac484]
        fstp dword ptr [esp + 0x28]
        fcom dword ptr [esp + 0x28]
        fnstsw ax
        test ah, 1
        je pre842_2003a80b
        fstp st(0)
        fld dword ptr [esp + 0x28]
pre842_2003a80b:
        fmul dword ptr [pre842K200ac3ac]
        fadd st(0), st(1)
        call PM_WeaponTruncateST0
        mov dword ptr [esi+pre842Shot], eax
        mov ecx, dword ptr [pm]
        fstp st(0)
        mov eax, dword ptr [ecx]
        cmp dword ptr [eax+pre842Shot], ebp
        jle pre842_2003a836
        mov dword ptr [eax+pre842Shot], ebp
pre842_2003a836:
        mov edx, dword ptr [pm]
        mov dword ptr [esp + 0x1c], 0x3f800000
        mov ecx, dword ptr [edx]
        test byte ptr [ecx+pre842Flags], 4
        je pre842_2003a8c3
        mov edx, dword ptr [ecx+pre842Weapon]
        lea eax, [edx + edx*2]
        shl eax, 3
        sub eax, edx
        lea eax, [eax + eax*4]
        mov edi, dword ptr [eax*4 + weaponDef+pre842Tac]
        mov al, byte ptr [ecx +pre842Eflags]
        test al, 0x10
        je pre842_2003a88f
        add ecx, pre842Velocity
        push ecx
        call VectorLength
        fcomp dword ptr [pre842K200ac190]
        add esp, 4
        fnstsw ax
        test ah, 1
        je pre842_2003a88f
        mov dword ptr [esp + 0x1c], 0x3f333333
pre842_2003a88f:
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        test dword ptr [eax +pre842Eflags], 0x80000
        je pre842_2003a8e3
        add eax, pre842Velocity
        push eax
        call VectorLength
        fcomp dword ptr [pre842K200ac190]
        add esp, 4
        fnstsw ax
        test ah, 1
        je pre842_2003a8e3
        mov dword ptr [esp + 0x1c], 0x3f19999a
        jmp pre842_2003a8e3
pre842_2003a8c3:
        mov ecx, dword ptr [ecx+pre842Weapon]
        mov dword ptr [esp + 0x1c], 0x3f800000
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea edx, [eax + eax*4]
        mov edi, dword ptr [edx*4 + weaponDef+pre842Hip]
pre842_2003a8e3:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [eax +pre842Ext]
        mov eax, dword ptr [ecx+pre842View0]
        mov dword ptr [edx +pre842Angles0], eax
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [eax +pre842Ext]
        mov eax, dword ptr [ecx+pre842View1]
        mov dword ptr [edx +pre842Angles1], eax
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [eax +pre842Ext]
        mov eax, dword ptr [ecx+pre842View2]
        mov dword ptr [edx +pre842Angles2], eax
        mov eax, dword ptr [esp+0x50]
        mov dword ptr [eax], edi
        mov eax, dword ptr [esp+0x54]
        mov ecx, dword ptr [esp+0x1c]
        mov dword ptr [eax], ecx
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        ret
    }
}
#endif

static void PM_TCEShotRecoil(int interval, unsigned int seed, int spreadAddition) {
    const tce_weaponDef_t *w = &weaponDef[pm->ps->weapon];
    int oldInstability = pm->ps->holdable[1];
    int oldRecoilPitch = pm->ps->delta_angles[PITCH];
    int oldRecoilYaw = pm->ps->delta_angles[YAW];
    int recoil;
    int half, kick, minimum;
    float randomScale, kickFloat;
#if !defined(_MSC_VER) || !defined(_M_IX86) || !defined(GAMEDLL)
    float increment;
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
    const float instabilityScale = 0.001f, instabilityUnits = 1000.0f;
    int *instabilityState = &pm->ps->stats[STAT_TCE_SHOT_INSTABILITY];
    const int *instabilityMinimum = &w->unknown_0f8[6];
#else
    double growth;
#endif
    float postureScale = 1.0f;
#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
    PM_TCEShotPrelude842(spreadAddition, &recoil, &postureScale);
    w = &weaponDef[pm->ps->weapon];
#else
    (void)spreadAddition;
    /* Seed and snapshots were captured by the common firing-tail producer. */
#if defined(_MSC_VER) && defined(_M_IX86)
    /* Original 30012480..300124dc: keep the integer addend and square
     * root on the x87 stack; only the definition-derived minimum spills. */
    __asm {
        mov ecx, instabilityState
        fild dword ptr [ecx]
        fld st(0)
        fmul instabilityScale
        fsqrt
        mov ecx, instabilityMinimum
        fild dword ptr [ecx]
        fmul instabilityScale
        fstp increment
        fcom increment
        fnstsw ax
        test ah, 1
        jz instability_selected
        fstp st(0)
        fld increment
    instability_selected:
        fmul instabilityUnits
        fadd st(0), st(1)
        call PM_WeaponTruncateST0
        mov ecx, instabilityState
        mov dword ptr [ecx], eax
        fstp st(0)
    }
#else
    increment = (float)((double)w->unknown_0f8[6] * (double)0.001f);
    growth = sqrt((double)oldInstability * (double)0.001f);
    if (growth < increment) growth = increment;
    pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] = (int)(oldInstability + growth * 1000.0f);
#endif
    if (pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] > 1000)
        pm->ps->stats[STAT_TCE_SHOT_INSTABILITY] = 1000;
    recoil = w->unknown_0f8[(pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) ? 8 : 7]; /* definition offsets114/118 */
    if (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) {
#if defined(_MSC_VER) && defined(_M_IX86)
        const float postureSpeedLimit = 30.0f;
        const float *postureVelocity;
        if (pm->ps->eFlags & EF_CROUCHING) {
            postureVelocity = pm->ps->velocity;
            __asm {
                push postureVelocity
                call VectorLength
                fcomp postureSpeedLimit
                add esp, 4
                fnstsw ax
                test ah, 1
                jz recoil_not_crouched
                mov dword ptr postureScale, 03f333333h
            recoil_not_crouched:
            }
        }
        if (pm->ps->eFlags & EF_PRONE) {
            postureVelocity = pm->ps->velocity;
            __asm {
                push postureVelocity
                call VectorLength
                fcomp postureSpeedLimit
                add esp, 4
                fnstsw ax
                test ah, 1
                jz recoil_not_prone
                mov dword ptr postureScale, 03f19999ah
            recoil_not_prone:
            }
        }
#else
        double speed = sqrt((double)pm->ps->velocity[0] * pm->ps->velocity[0] +
                            (double)pm->ps->velocity[1] * pm->ps->velocity[1] +
                            (double)pm->ps->velocity[2] * pm->ps->velocity[2]);
        if ((pm->ps->eFlags & EF_CROUCHING) && speed < 30.0f) postureScale = 0.7f;
        if ((pm->ps->eFlags & EF_PRONE) && speed < 30.0f) postureScale = 0.6f;
#endif
    }
    /* Original3001259b: the angle snapshot follows instability and posture. */
    VectorCopy(pm->ps->viewangles, pm->pmext->tceShotAngles);
#endif
    if ((pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) && !w->noTacMode) {
#if defined(_MSC_VER) && defined(_M_IX86)
        const float shotAngleBias = 2000.0f, shotAngleScale = 0.01f;
        const int *shotAngleInputs = &pm->ps->holdable[5];
        float *shotAngleOutputs = pm->pmext->tceShotAngles;
        __asm {
            mov ecx, shotAngleInputs
            mov edx, shotAngleOutputs
            fild dword ptr [ecx]
            fsub shotAngleBias
            fmul shotAngleScale
            fld st(0)
            fadd dword ptr [edx]
            fstp dword ptr [edx]
            fstp st(0)
            fild dword ptr [ecx+4]
            fsub shotAngleBias
            fmul shotAngleScale
            fld st(0)
            fadd dword ptr [edx+4]
            fstp dword ptr [edx+4]
            fstp st(0)
        }
#else
        /* Original FILD/FSUB/FMUL/FADD retains the offset product until the
         * final angle store. Avoid an extra binary32 product rounding in the
         * portable prediction/server path. */
        pm->pmext->tceShotAngles[PITCH] = (float)((double)pm->pmext->tceShotAngles[PITCH] +
            ((double)pm->ps->holdable[5] - 2000.0) * (double)0.01f);
        pm->pmext->tceShotAngles[YAW] = (float)((double)pm->pmext->tceShotAngles[YAW] +
            ((double)pm->ps->holdable[6] - 2000.0) * (double)0.01f);
#endif
    }
    if (!recoil) {
        pm->pmext->weapRecoilTime = 0;
        return;
    }
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        int *recoilRemainder = &pm->ps->stats[STAT_TCE_RECOIL_REMAINDER];
        __asm {
            mov eax, recoil
            cdq
            sub eax, edx
            sar eax, 1
            mov half, eax
            mov ecx, recoilRemainder
            mov edx, dword ptr [ecx]
            cmp edx, eax
            jle recoil_kick_selected
            mov edx, eax
        recoil_kick_selected:
            add edx, recoil
            mov kick, edx
        }
    }
#else
    half = recoil / 2;
    kick = pm->ps->stats[STAT_TCE_RECOIL_REMAINDER];
    if (kick > half) kick = half;
    kick += recoil;
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        const float recoilIntervalScale = 1.3333332538604736f;
        const float recoilIntervalBias = 4.0f;
        /* 3001267e..300126a6: neither product spills to binary64 before
         * the original signed64 truncation / low32 return. */
        __asm {
            fild kick
            fmul postureScale
            call PM_WeaponTruncateST0
            fild interval
            mov kick, eax
            fmul recoilIntervalScale
            fadd recoilIntervalBias
            call PM_WeaponTruncateST0
            mov minimum, eax
        }
    }
#else
    kick = (int)((double)kick * (double)postureScale);
    minimum = (int)((double)interval * (double)1.3333332538604736f + 4.0f);
#endif
    if (minimum > 250) minimum = 250;
    if (kick < minimum) kick = minimum;
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        int *recoilRemainder = &pm->ps->stats[STAT_TCE_RECOIL_REMAINDER];
        __asm {
            mov ecx, recoilRemainder
            mov edx, dword ptr [ecx]
            add edx, kick
            mov dword ptr [ecx], edx
            mov eax, half
            mov edx, interval
            lea eax, [edx+eax*4]
            cmp dword ptr [ecx], eax
            jle recoil_interval_clamped
            mov dword ptr [ecx], eax
        recoil_interval_clamped:
            cmp dword ptr [ecx], 500
            jle recoil_maximum_clamped
            mov dword ptr [ecx], 500
        recoil_maximum_clamped:
        }
    }
#else
    pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] += kick;
    if (pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] > interval + half * 4)
        pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] = interval + half * 4;
    if (pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] > 500)
        pm->ps->stats[STAT_TCE_RECOIL_REMAINDER] = 500;
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        int *recoilAngles = pm->ps->delta_angles;
        int *recoilResetTime = &pm->pmext->weapRecoilTime;
        const float recoilPitchBias = 2.0f;
        float recoilOldAngle;
        ++seed;
#if defined(GAMEDLL)
        /* Original2003aa58 reads the snapshot after the842 prelude wrote it. */
        oldInstability = pm->ps->holdable[1];
#endif
        __asm {
            fild oldInstability
            mov ecx, recoilResetTime
            mov dword ptr [ecx], 0
            fmul instabilityScale
            fstp randomScale
            fild kick
            fstp kickFloat
            mov ecx, recoilAngles
            fild dword ptr [ecx]
            fstp recoilOldAngle
            lea eax, seed
            push eax
            call Q_random
            fmul randomScale
            fadd recoilPitchBias
            fmul kickFloat
            fsubr recoilOldAngle
            call PM_WeaponTruncateST0
            mov ecx, recoilAngles
            mov dword ptr [ecx], eax
            fild dword ptr [ecx+4]
            fstp recoilOldAngle
            lea eax, seed
            push eax
            call Q_random
            fmul randomScale
            add esp, 8
            fmul kickFloat
            fsubr recoilOldAngle
            call PM_WeaponTruncateST0
            mov ecx, recoilAngles
            mov dword ptr [ecx+4], eax
        }
    }
#else
    pm->pmext->weapRecoilTime = 0;
    randomScale = (float)((double)oldInstability * (double)0.001f);
    kickFloat = (float)kick;
    ++seed;
    seed = 69069u * seed + 1u;
    pm->ps->delta_angles[PITCH] = (int)((double)pm->ps->delta_angles[PITCH] -
        (((double)(seed & 65535u) / 65536.0 * randomScale) + 2.0) * kickFloat);
    seed = 69069u * seed + 1u;
    pm->ps->delta_angles[YAW] = (int)((double)pm->ps->delta_angles[YAW] -
        ((double)(seed & 65535u) / 65536.0 * randomScale) * kickFloat);
#endif
    pm->pmext->tceFreelookRecoil[PITCH] += AngleNormalize180(SHORT2ANGLE(pm->ps->delta_angles[PITCH] - oldRecoilPitch));
    pm->pmext->tceFreelookRecoil[YAW] += AngleNormalize180(SHORT2ANGLE(pm->ps->delta_angles[YAW] - oldRecoilYaw));
}

/* Complete activate-action branch of original PM_Weapon, 30010b6e..30010d16.
 * Timers are predicted on both modules; the server validates the target again
 * when consuming the completion event. The parent weapon controller is partial. */
static void PM_TCEObjectiveAction(void) {
    playerState_t *ps = pm->ps;
    int hint = ps->serverCursorHint;
    qboolean target = hint == HINT_DISARM || hint == HINT_BREAKABLE_DYNAMITE || hint == HINT_ACTIVATE;
    if (!(pm->cmd.buttons & BUTTON_ACTIVATE)) {
        if (ps->pm_flags & PMF_TCE_OBJECTIVE_ACTION) {
            ps->pm_flags &= ~PMF_TCE_OBJECTIVE_ACTION;
            ps->weaponTime = 100;
            PM_AddEvent(EV_TCE_OBJECTIVE_STOP);
        }
        return;
    }
    if (target && ps->weaponTime <= 0 && ps->weaponDelay <= 0 &&
        ps->weaponstate == WEAPON_READY && !(ps->pm_flags & PMF_TCE_OBJECTIVE_ACTION)) {
        ps->pm_flags |= PMF_TCE_OBJECTIVE_ACTION;
        ps->weaponTime = (ps->stats[STAT_TCE_FLAGS] & 2) ? 2000 : 4000;
        if (hint == HINT_DISARM)
            ps->weaponTime = (ps->stats[STAT_TCE_FLAGS] & 2) ? 5000 : 10000;
        if (hint == HINT_ACTIVATE) PM_AddEvent(EV_TCE_OBJECTIVE_START);
    } else if (ps->pm_flags & PMF_TCE_OBJECTIVE_ACTION) {
        if (ps->weaponTime <= 0) {
            if (hint == HINT_DISARM) PM_AddEvent(EV_TCE_DEFUSE);
            else if (hint == HINT_BREAKABLE_DYNAMITE) PM_AddEvent(EV_TCE_PLANT);
            else if (hint == HINT_ACTIVATE) PM_AddEvent(EV_TCE_OBJECTIVE_COMPLETE);
            ps->weaponTime += 500;
            ps->pm_flags &= ~PMF_TCE_OBJECTIVE_ACTION;
        } else if (!target) {
            ps->pm_flags &= ~PMF_TCE_OBJECTIVE_ACTION;
            ps->weaponDelay = 0;
            ps->weaponTime = 100;
            PM_AddEvent(EV_TCE_OBJECTIVE_STOP);
        }
    }
}

#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) void PM_WeaponTruncateST0(void) {
    __asm {
        push ebp
        mov ebp, esp
        sub esp, 12
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

/* Native extraction of the two original mounted-weapon cooling blocks. */
static void PM_WeaponCoolMounted(void) {
    static double (__cdecl *const mountedFloor)(double) = floor;
    static const float mountedCooling = 300.0f;
    static const float mountedReciprocal = 0.0006666666595265269f;
    static const float mountedByteScale = 255.0f;
    int *mountedHeat = &pm->ps->weapHeat[34];
    int *mountedDisplay = &pm->ps->curWeapHeat;
    float *mountedFrame = &pml.frametime;
    __asm {
        mov ecx, mountedHeat
        fild dword ptr [ecx]
        mov ecx, mountedFrame
        fld dword ptr [ecx]
        fmul mountedCooling
        fsubp st(1), st(0)
        call PM_WeaponTruncateST0
        mov ecx, mountedHeat
        mov dword ptr [ecx], eax
        test eax, eax
        jge mounted_heat_nonnegative
        mov dword ptr [ecx], 0
mounted_heat_nonnegative:
        sub esp, 8
        fild dword ptr [ecx]
        fmul mountedReciprocal
        fmul mountedByteScale
        fstp qword ptr [esp]
        call dword ptr [mountedFloor]
        add esp, 8
        call PM_WeaponTruncateST0
        mov ecx, mountedDisplay
        mov dword ptr [ecx], eax
    }
}
#endif

/* Extracted PM_Weapon recoil expressions; preserve the original final store. */
static void PM_WeaponRandomPitch(float *recoilDestination, float recoilScale) {
    int recoilBits = rand() & 0x7fff;
#if defined(_MSC_VER) && defined(_M_IX86)
    static const float recoilRandomScale = 3.0518509447574615e-05f;
    __asm {
        fild recoilBits
        fmul recoilRandomScale
        fmul recoilScale
        mov eax, recoilDestination
        fstp dword ptr [eax]
    }
#else
    *recoilDestination = (float)((double)recoilBits * (double)3.0518509447574615e-05f * (double)recoilScale);
#endif
}

static void PM_WeaponRandomYaw(float *recoilDestination, double recoilScale) {
    int recoilBits = rand() & 0x7fff;
#if defined(_MSC_VER) && defined(_M_IX86)
    static const float recoilRandomScale = 3.0518509447574615e-05f;
    static const double recoilMidpoint = 0.5;
    __asm {
        fild recoilBits
        fmul recoilRandomScale
        fsub recoilMidpoint
        fadd st(0), st(0)
        fmul recoilScale
        mov eax, recoilDestination
        fstp dword ptr [eax]
    }
#else
    *recoilDestination = (float)(((double)recoilBits * (double)3.0518509447574615e-05f - 0.5) * 2.0 * recoilScale);
#endif
}

/* Original inlined class-charge comparison: threshold > elapsed, ordered.
 * The product remains in ST0 until FCOMPP, not rounded to binary32. */
static qboolean PM_WeaponChargePending(int elapsed, int chargeTime, float fraction) {
#if defined(_MSC_VER) && defined(_M_IX86)
    int chargePending;
    __asm {
        fild elapsed
        fild chargeTime
        fmul fraction
        fcompp
        fnstsw ax
        test ah, 41h
        setz al
        movzx eax, al
        mov chargePending, eax
    }
    return chargePending;
#else
    return elapsed < chargeTime * fraction;
#endif
}

/* Inlined PM_Weapon ADD32 sites; preserve wrapping timer arithmetic. */
static void PM_WeaponAddTime(int interval) {
#if defined(_MSC_VER) && defined(_M_IX86)
    int *cycleTimer = &pm->ps->weaponTime;
    __asm {
        mov eax, cycleTimer
        mov ecx, interval
        add dword ptr [eax], ecx
    }
#else
    pm->ps->weaponTime = (int)((unsigned int)pm->ps->weaponTime + (unsigned int)interval);
#endif
}

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
/* PM_Weapon20037fb0 private extraction: 20038a65..20039074.
 * Existing64-definition domain only; not an independent original function. */
static const float actions834K200ac788 = -360.0;
static const float actions834K200ac3ac = 1000.0;
static const float actions834K200ac6b8 = 6.2831854820251465;
static const double actions834K200ac6a8 = 200.0;
static const double actions834K200ac780 = 2000.0;
static const double actions834K200ac6b0 = 400.0;
enum {
    actions834Ps = offsetof(pmove_t,ps),
    actions834Ext = offsetof(pmove_t,pmext),
    actions834Wbuttons = offsetof(pmove_t,cmd)+offsetof(usercmd_t,wbuttons),
    actions834Buttons = offsetof(pmove_t,cmd)+offsetof(usercmd_t,buttons),
    actions834ForwardCmd = offsetof(pmove_t,cmd)+offsetof(usercmd_t,forwardmove),
    actions834RightCmd = offsetof(pmove_t,cmd)+offsetof(usercmd_t,rightmove),
    actions834CmdTime = offsetof(pmove_t,cmd)+offsetof(usercmd_t,serverTime),
    actions834Water = offsetof(pmove_t,waterlevel),
    actions834Flags = offsetof(playerState_t,pm_flags),
    actions834Eflags = offsetof(playerState_t,eFlags),
    actions834Time = offsetof(playerState_t,weaponTime),
    actions834Delay = offsetof(playerState_t,weaponDelay),
    actions834VelocityZ = offsetof(playerState_t,velocity)+2*sizeof(float),
    actions834Ground = offsetof(playerState_t,groundEntityNum),
    actions834Weapon = offsetof(playerState_t,weapon),
    actions834State = offsetof(playerState_t,weaponstate),
    actions834WeaponFlags = offsetof(playerState_t,stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    actions834TcFlags = offsetof(playerState_t,stats)+STAT_TCE_FLAGS*sizeof(int),
    actions834Instability = offsetof(playerState_t,stats)+STAT_TCE_MOVEMENT_INSTABILITY*sizeof(int),
    actions834Phase = offsetof(playerState_t,stats)+STAT_TCE_AIM_PHASE*sizeof(int),
    actions834Seed = offsetof(playerState_t,stats)+STAT_TCE_SHOT_SEED*sizeof(int),
    actions834RecoilX = offsetof(playerState_t,holdable)+5*sizeof(int),
    actions834RecoilY = offsetof(playerState_t,holdable)+6*sizeof(int),
    actions834Hint = offsetof(playerState_t,serverCursorHint),
    actions834ProneTime = offsetof(pmoveExt_t,proneTime),
    actions834Ladder = offsetof(pml_t,ladder),
    actions834NoTac = offsetof(tce_weaponDef_t,noTacMode)
};
typedef char actions834Protocol[(offsetof(pmove_t,ps)==0 && sizeof(tce_weaponDef_t)==460 && offsetof(tce_weaponDef_t,noTacMode)==0x194 && HINT_DISARM==39 && HINT_BREAKABLE_DYNAMITE==10 && HINT_ACTIVATE==3 && PMF_TCE_OBJECTIVE_ACTION==128 && WBUTTON_ZOOM==2 && BUTTON_ACTIVATE==64 && WEAPON_FIRING==7) ? 1 : -1];
static __declspec(naked) qboolean PM_TCEWeaponActions834(qboolean delayedFire) {
    __asm {
        sub esp, 0x38
        push ebx
        push ebp
        push esi
        push edi
        mov ebp, dword ptr [esp+0x4c]
        mov dword ptr [esp+0x18], ebp
        mov esi, dword ptr [pm]
        xor ebx, ebx
        mov edi, 7
        mov dl, byte ptr [esi+actions834Wbuttons]
        and dl, 2
        mov byte ptr [esp + 0x13], dl
        jne actions834_20038a98
        mov ecx, dword ptr [esi]
        mov eax, dword ptr [ecx+actions834WeaponFlags]
        test ah, 0x80
        je actions834_20038a98
        and ah, 0x7f
        mov dword ptr [ecx+actions834WeaponFlags], eax
        mov esi, dword ptr [pm]
actions834_20038a8d:
        mov ebx, dword ptr [esp + 0x18]
        xor ebp, ebp
        jmp actions834_20038d73
actions834_20038a98:
        test dl, dl
        je actions834_20038ac7
        mov ecx, dword ptr [esi]
        mov ebx, dword ptr [ecx+actions834WeaponFlags]
        test bh, 0x80
        jne actions834_20038ac7
        mov eax, dword ptr [ecx+actions834Time]
        test eax, eax
        jg actions834_20038ac7
        test ebp, ebp
        jne actions834_20038ac7
        mov eax, dword ptr [ecx+actions834State]
        cmp eax, edi
        je actions834_20038ac7
        cmp eax, 8
        je actions834_20038ac7
        test eax, eax
        je actions834_20038ae0
actions834_20038ac7:
        mov ecx, dword ptr [esi]
        mov eax, dword ptr [ecx+actions834Time]
        test eax, eax
        jg actions834_20038a8d
        mov ebx, dword ptr [ecx+actions834WeaponFlags]
        test bl, 8
        je actions834_20038a8d
        test bl, 4
        jne actions834_20038a8d
actions834_20038ae0:
        mov eax, dword ptr [ecx+actions834State]
        cmp eax, 3
        je actions834_20038a8d
        cmp eax, 9
        je actions834_20038a8d
        cmp eax, 0xa
        je actions834_20038a8d
        cmp eax, 0xb
        je actions834_20038a8d
        mov edi, dword ptr [ecx+actions834Weapon]
        cmp edi, 1
        je actions834_20038a8d
        cmp edi, 0x15
        je actions834_20038a8d
        cmp edi, 0x13
        je actions834_20038a8d
        cmp edi, 0xc
        je actions834_20038a8d
        cmp edi, 0xf
        je actions834_20038a8d
        test byte ptr [esi+actions834Buttons], 0x20
        je actions834_20038b4c
        mov al, byte ptr [esi+actions834ForwardCmd]
        test al, al
        jne actions834_20038b39
        mov al, byte ptr [esi+actions834RightCmd]
        test al, al
        je actions834_20038b4c
actions834_20038b39:
        test byte ptr [ecx+actions834Flags], 1
        jne actions834_20038b4c
        test dword ptr [ecx+actions834Eflags], 0x80000
        je actions834_20038a8d
actions834_20038b4c:
        mov eax, dword ptr [esi+actions834Water]
        cmp eax, 2
        jg actions834_20038a8d
        test eax, eax
        jle actions834_20038b6c
        cmp dword ptr [ecx+actions834Ground], 0x3ff
        je actions834_20038a8d
actions834_20038b6c:
        mov eax, dword ptr [pml+actions834Ladder]
        test eax, eax
        jne actions834_20038a8d
        fld dword ptr [ecx+actions834VelocityZ]
        fcomp dword ptr [actions834K200ac788]
        fnstsw ax
        test ah, 1
        jne actions834_20038a8d
        test bh, 0x10
        jne actions834_20038a8d
        mov edx, dword ptr [esi+actions834Ext]
        mov eax, dword ptr [esi+actions834CmdTime]
        mov ebp, eax
        mov edx, dword ptr [edx+actions834ProneTime]
        sub ebp, edx
        cmp ebp, 0xc8
        jl actions834_20038a8d
        add edx, eax
        cmp edx, 0xc8
        jl actions834_20038a8d
        test dword ptr [ecx+actions834Eflags], 0x100000
        jne actions834_20038a8d
        xor ebp, ebp
        cmp edi, ebp
        je actions834_20038a8d
        mov al, byte ptr [esp + 0x13]
        test al, al
        je actions834_20038c08
        or bh, 0x80
        mov dword ptr [ecx+actions834WeaponFlags], ebx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov eax, dword ptr [ecx+actions834WeaponFlags]
        test al, 8
        je actions834_20038bfa
        and al, 0xf7
        jmp actions834_20038bfc
actions834_20038bfa:
        or al, 8
actions834_20038bfc:
        mov dword ptr [ecx+actions834WeaponFlags], eax
        mov esi, dword ptr [pm]
actions834_20038c08:
        mov ecx, dword ptr [esi]
        mov eax, dword ptr [ecx+actions834WeaponFlags]
        mov edx, eax
        and edx, 4
        je actions834_20038c1f
        test al, 8
        jne actions834_20038c1f
        and al, 0xfb
        jmp actions834_20038c31
actions834_20038c1f:
        cmp edx, ebp
        jne actions834_20038d6f
        test al, 8
        je actions834_20038d6f
        or al, 4
actions834_20038c31:
        mov dword ptr [ecx+actions834WeaponFlags], eax
        mov ecx, dword ptr [pm]
        mov ebp, 0x3e8
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+actions834Instability], ebp
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        cmp dword ptr [ecx+actions834State], 7
        je actions834_20038d51
        mov edx, ecx
        lea ecx, [esp + 0x1c]
        push ecx
        mov eax, dword ptr [edx+actions834Seed]
        mov dword ptr [esp + 0x20], eax
        call Q_random
        fmul dword ptr [actions834K200ac3ac]
        add esp, 4
        call PM_WeaponTruncateST0
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx]
        mov dword ptr [ecx+actions834Phase], eax
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+actions834Phase]
        cmp ecx, ebp
        jle actions834_20038cae
        add ecx, 0xfffffc18
        mov dword ptr [eax+actions834Phase], ecx
actions834_20038cae:
        mov eax, dword ptr [pm]
        mov edx, dword ptr [eax]
        mov ecx, dword ptr [edx+actions834Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea ecx, [eax + eax*4]
        mov eax, dword ptr [ecx*4 + weaponDef+actions834NoTac]
        test eax, eax
        jne actions834_20038d31
        test byte ptr [edx+actions834WeaponFlags], 4
        je actions834_20038d31
        lea edx, [esp + 0x1c]
        push edx
        call Q_random
        fmul dword ptr [actions834K200ac6b8]
        add esp, 4
        fld st(0)
        fcos 
        fmul qword ptr [actions834K200ac6a8]
        fadd qword ptr [actions834K200ac780]
        call PM_WeaponTruncateST0
        fsin 
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+actions834RecoilX], eax
        fmul qword ptr [actions834K200ac6b0]
        fadd qword ptr [actions834K200ac780]
        call PM_WeaponTruncateST0
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+actions834RecoilY], eax
actions834_20038d31:
        mov eax, dword ptr [esp + 0x1c]
        mov ecx, dword ptr [pm]
        and eax, 0xffff
        mov dword ptr [esp + 0x1c], eax
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+actions834Seed], eax
        mov eax, dword ptr [pm]
actions834_20038d51:
        mov eax, dword ptr [eax]
        push EV_TCE_TOGGLE_AIMING
        add dword ptr [eax+actions834Time], 0x12c
        call PM_AddEvent
        add esp, 4
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        mov eax, 1
        ret 
actions834_20038d6f:
        mov ebx, dword ptr [esp + 0x18]
actions834_20038d73:
        test byte ptr [esi+actions834Wbuttons], 2
        je actions834_20038db3
        mov ecx, dword ptr [esi]
        mov eax, dword ptr [ecx+actions834WeaponFlags]
        test ah, 0x80
        jne actions834_20038db3
        test al, 8
        je actions834_20038db3
        test al, 4
        jne actions834_20038db3
        or ah, 0x80
        mov dword ptr [ecx+actions834WeaponFlags], eax
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        mov ecx, dword ptr [eax+actions834WeaponFlags]
        and ecx, 0xfffffff7
        mov dword ptr [eax+actions834WeaponFlags], ecx
        mov esi, dword ptr [pm]
actions834_20038db3:
        cmp dword ptr [pml+actions834Ladder], ebp
        je actions834_20038de8
        mov eax, dword ptr [esi]
        mov ecx, 0x1f4
        cmp dword ptr [eax+actions834Time], ecx
        jg actions834_20038dd0
        mov dword ptr [eax+actions834Time], ecx
        mov esi, dword ptr [pm]
actions834_20038dd0:
        mov esi, dword ptr [esi]
        pop edi
        mov eax, dword ptr [esi+actions834TcFlags]
        or al, 8
        mov dword ptr [esi+actions834TcFlags], eax
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        mov eax, 1
        ret 
actions834_20038de8:
        test byte ptr [esi+actions834Buttons], 0x20
        je actions834_20038e68
        mov al, byte ptr [esi+actions834ForwardCmd]
        test al, al
        jne actions834_20038dfc
        mov al, byte ptr [esi+actions834RightCmd]
        test al, al
        je actions834_20038e68
actions834_20038dfc:
        mov eax, dword ptr [esi]
        test byte ptr [eax+actions834Flags], 1
        jne actions834_20038e68
        test dword ptr [eax+actions834Eflags], 0x80000
        jne actions834_20038e68
        mov esi, dword ptr [eax+actions834WeaponFlags]
        or esi, 0x4000
        mov dword ptr [eax+actions834WeaponFlags], esi
        mov esi, dword ptr [pm]
        mov eax, dword ptr [esi]
        mov ecx, dword ptr [eax+actions834Weapon]
        cmp ecx, 0x1e
        je actions834_20038e3c
        cmp ecx, 4
        je actions834_20038e3c
        cmp ecx, 9
        jne actions834_20038e45
actions834_20038e3c:
        cmp ebx, ebp
        je actions834_20038e45
        mov dword ptr [eax+actions834Time], ebp
        jmp actions834_20038e92
actions834_20038e45:
        cmp dword ptr [eax+actions834State], ebp
        jne actions834_20038e98
        mov edx, dword ptr [eax+actions834Time]
        mov ecx, 0xfa
        cmp edx, ecx
        jg actions834_exit
        pop edi
        pop esi
        pop ebp
        mov dword ptr [eax+actions834Time], ecx
        pop ebx
        add esp, 0x38
        mov eax, 1
        ret 
actions834_20038e68:
        mov ecx, dword ptr [esi]
        cmp dword ptr [ecx+actions834Time], ebp
        jg actions834_20038e98
        mov eax, dword ptr [ecx+actions834WeaponFlags]
        test ah, 0x40
        je actions834_20038e98
        and ah, 0xbf
        mov dword ptr [ecx+actions834WeaponFlags], eax
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+actions834Time], 0xfa
actions834_20038e92:
        mov esi, dword ptr [pm]
actions834_20038e98:
        test byte ptr [esi+actions834Buttons], 0x40
        je actions834_20039010
        mov eax, dword ptr [esi]
        mov edi, dword ptr [eax+actions834Hint]
        cmp edi, 0x27
        je actions834_20038ebd
        cmp edi, 0xa
        je actions834_20038ebd
        cmp edi, 3
        jne actions834_20038f79
actions834_20038ebd:
        cmp dword ptr [eax+actions834Time], ebp
        jg actions834_20038f79
        cmp dword ptr [eax+actions834Delay], ebp
        jg actions834_20038f79
        mov edx, dword ptr [eax+actions834State]
        cmp edx, 3
        je actions834_20038f79
        cmp edx, 1
        je actions834_20038f79
        cmp edx, 9
        je actions834_20038f79
        cmp edx, 0xa
        je actions834_20038f79
        cmp edx, 0xb
        je actions834_20038f79
        mov ecx, dword ptr [eax+actions834Flags]
        test cl, 0x80
        jne actions834_20038f79
        cmp edx, ebp
        jne actions834_20038f79
        or cl, 0x80
        mov dword ptr [eax+actions834Flags], ecx
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        test byte ptr [eax+actions834TcFlags], 2
        je actions834_20038f29
        mov dword ptr [eax+actions834Time], 0x7d0
        jmp actions834_20038f30
actions834_20038f29:
        mov dword ptr [eax+actions834Time], 0xfa0
actions834_20038f30:
        mov esi, dword ptr [pm]
        mov eax, dword ptr [esi]
        cmp dword ptr [eax+actions834Hint], 0x27
        jne actions834_20038f60
        test byte ptr [eax+actions834TcFlags], 2
        je actions834_20038f53
        mov dword ptr [eax+actions834Time], 0x1388
        jmp actions834_20038f5a
actions834_20038f53:
        mov dword ptr [eax+actions834Time], 0x2710
actions834_20038f5a:
        mov esi, dword ptr [pm]
actions834_20038f60:
        mov ecx, dword ptr [esi]
        cmp dword ptr [ecx+actions834Hint], 3
        jne actions834_20039040
        push EV_TCE_OBJECTIVE_START
        jmp actions834_20039032
actions834_20038f79:
        mov ecx, dword ptr [eax+actions834Flags]
        mov edx, ecx
        and edx, 0x80
        je actions834_20038fdc
        cmp dword ptr [eax+actions834Time], ebp
        jg actions834_20038fdc
        cmp edi, 0x27
        jne actions834_20038f97
        push EV_TCE_DEFUSE
        jmp actions834_20038fad
actions834_20038f97:
        cmp edi, 0xa
        jne actions834_20038fa3
        push EV_TCE_PLANT
        jmp actions834_20038fad
actions834_20038fa3:
        cmp edi, 3
        jne actions834_20038fbb
        push EV_TCE_OBJECTIVE_COMPLETE
actions834_20038fad:
        call PM_AddEvent
        mov esi, dword ptr [pm]
        add esp, 4
actions834_20038fbb:
        mov esi, dword ptr [esi]
        mov edx, dword ptr [esi+actions834Time]
        add edx, 0x1f4
        mov dword ptr [esi+actions834Time], edx
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+actions834Flags]
        and cl, 0x7f
        mov dword ptr [eax+actions834Flags], ecx
        jmp actions834_2003903a
actions834_20038fdc:
        cmp edx, ebp
        je actions834_20039040
        cmp edi, 0x27
        je actions834_20039040
        cmp edi, 0xa
        je actions834_20039040
        cmp edi, 3
        je actions834_20039040
        and cl, 0x7f
        mov dword ptr [eax+actions834Flags], ecx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov dword ptr [ecx+actions834Delay], ebp
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov dword ptr [eax+actions834Time], 0x64
        jmp actions834_2003902d
actions834_20039010:
        mov ecx, dword ptr [esi]
        mov eax, dword ptr [ecx+actions834Flags]
        test al, 0x80
        je actions834_20039040
        and al, 0x7f
        mov dword ptr [ecx+actions834Flags], eax
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+actions834Time], 0x64
actions834_2003902d:
        push EV_TCE_OBJECTIVE_STOP
actions834_20039032:
        call PM_AddEvent
        add esp, 4
actions834_2003903a:
        mov esi, dword ptr [pm]
actions834_20039040:
        mov esi, dword ptr [esi]
        cmp dword ptr [esi+actions834Delay], ebp
        jg actions834_exit
        mov eax, dword ptr [esi+actions834Weapon]
        push eax
        call PM_CheckForReload
        mov ecx, dword ptr [pm]
        add esp, 4
        mov eax, dword ptr [ecx]
        cmp dword ptr [eax+actions834Time], ebp
        jg actions834_exit
        cmp dword ptr [eax+actions834Delay], ebp
        jg actions834_exit
        xor eax, eax
        jmp actions834_done
actions834_exit:
        mov eax, 1
actions834_done:
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        ret
    }
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
/* Private PM_Weapon range2003908c..2003934d: trigger/burst/reload continuation.
 * Native fields, original callback order. No independent original function. */
enum {
    trigger835Buttons = offsetof(pmove_t,cmd)+offsetof(usercmd_t,buttons),
    trigger835Flags = offsetof(playerState_t,pm_flags),
    trigger835Weapon = offsetof(playerState_t,weapon),
    trigger835State = offsetof(playerState_t,weaponstate),
    trigger835Mode = offsetof(playerState_t,persistant)+10*sizeof(int),
    trigger835BurstCount = offsetof(playerState_t,holdable)+11*sizeof(int),
    trigger835TcFlags = offsetof(playerState_t,stats)+STAT_TCE_FLAGS*sizeof(int),
    trigger835Clip = offsetof(playerState_t,ammoclip),
    trigger835Time = offsetof(playerState_t,weaponTime),
    trigger835Delay = offsetof(playerState_t,weaponDelay),
    trigger835Semi = offsetof(tce_weaponDef_t,semiauto),
    trigger835Burst = offsetof(tce_weaponDef_t,burst),
    trigger835Pump = offsetof(tce_weaponDef_t,pump),
    trigger835Bolt = offsetof(tce_weaponDef_t,bolt),
    trigger835Single = offsetof(tce_weaponDef_t,singleReload)
};
typedef char trigger835Protocol[(offsetof(pmove_t,ps)==0 && sizeof(tce_weaponDef_t)==460 && offsetof(tce_weaponDef_t,semiauto)==0x15c && offsetof(tce_weaponDef_t,burst)==0x164 && offsetof(tce_weaponDef_t,pump)==0x168 && offsetof(tce_weaponDef_t,bolt)==0x16c && offsetof(tce_weaponDef_t,singleReload)==0x170 && BUTTON_ATTACK==1 && WEAPON_FIRING==7 && WEAPON_FIRINGALT==8) ? 1 : -1];
static __declspec(naked) qboolean PM_TCEWeaponTrigger835(qboolean delayedFire) {
    __asm {
        sub esp, 0x38
        push ebx
        push ebp
        push esi
        push edi
        mov ebx, dword ptr [esp+0x4c]
        xor ebp, ebp
        mov edi, 7
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        test byte ptr [ecx +trigger835Buttons], 1
        jne trigger835_20039144
        mov edx, dword ptr [eax+trigger835Flags]
        test dh, 4
        je trigger835_20039144
        cmp ebx, ebp
        jne trigger835_20039144
        mov ecx, dword ptr [eax+trigger835Weapon]
        push ecx
        call BG_FiremodeWeapon
        add esp, 4
        test eax, eax
        je trigger835_20039137
        mov ecx, dword ptr [pm]
        mov edi, 1
        mov esi, dword ptr [ecx]
        cmp dword ptr [esi+trigger835Mode], edi
        jne trigger835_20039149
        mov edx, dword ptr [esi+trigger835Weapon]
        mov esi, dword ptr [esi+trigger835BurstCount]
        lea eax, [edx + edx*2]
        shl eax, 3
        sub eax, edx
        lea eax, [eax + eax*4]
        cmp esi, dword ptr [eax*4 + weaponDef+trigger835Burst]
        jge trigger835_20039149
        mov esi, dword ptr [ecx]
        push edx
        call BG_FindClipForWeapon
        mov ecx, dword ptr [esi + eax*4 +trigger835Clip]
        add esp, 4
        cmp ecx, ebp
        je trigger835_2003913c
        test byte ptr [esi+trigger835TcFlags], 8
        jne trigger835_2003913c
        mov esi, dword ptr [esi+trigger835State]
        cmp esi, 7
        je trigger835_20039121
        cmp esi, 8
        jne trigger835_2003913c
trigger835_20039121:
        mov eax, dword ptr [pm]
        mov cl, byte ptr [eax +trigger835Buttons]
        or cl, 1
        mov byte ptr [eax +trigger835Buttons], cl
        mov ecx, dword ptr [pm]
        jmp trigger835_20039149
trigger835_20039137:
        mov edi, 1
trigger835_2003913c:
        mov ecx, dword ptr [pm]
        jmp trigger835_20039149
trigger835_20039144:
        mov edi, 1
trigger835_20039149:
        test byte ptr [ecx +trigger835Buttons], 1
        jne trigger835_20039163
        cmp ebx, ebp
        jne trigger835_20039163
        mov ecx, dword ptr [ecx]
        mov eax, dword ptr [ecx+trigger835Flags]
        and ah, 0xfb
        mov dword ptr [ecx+trigger835Flags], eax
        jmp trigger835_20039325
trigger835_20039163:
        mov edx, dword ptr [ecx]
        mov eax, dword ptr [edx+trigger835Flags]
        test ah, 4
        je trigger835_2003931f
        cmp ebx, ebp
        jne trigger835_2003931f
        mov eax, dword ptr [edx+trigger835Weapon]
        cmp eax, edi
        je trigger835_2003930c
        test dword ptr [edx+trigger835TcFlags], 0x800
        jne trigger835_2003930c
        push eax
        call BG_FiremodeWeapon
        mov edx, dword ptr [pm]
        add esp, 4
        test eax, eax
        jne trigger835_200391e4
        mov esi, dword ptr [edx]
        mov ecx, dword ptr [esi+trigger835Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea eax, [eax + eax*4]
        shl eax, 2
        cmp dword ptr [eax + weaponDef+trigger835Semi], ebp
        jne trigger835_2003928a
        cmp dword ptr [eax + weaponDef+trigger835Pump], ebp
        jne trigger835_2003928a
        cmp dword ptr [eax + weaponDef+trigger835Bolt], ebp
        jne trigger835_2003928a
trigger835_200391e4:
        mov esi, dword ptr [edx]
        mov ecx, esi
        mov edx, dword ptr [ecx+trigger835Weapon]
        push edx
        call BG_FindClipForWeapon
        mov ecx, dword ptr [esi + eax*4 +trigger835Clip]
        add esp, 4
        cmp ecx, ebp
        jne trigger835_2003921b
        mov eax, dword ptr [esi+trigger835Weapon]
        cmp eax, edi
        je trigger835_2003921b
        cmp eax, 4
        je trigger835_2003921b
        cmp eax, 0x1e
        je trigger835_2003921b
        cmp eax, 9
        jne trigger835_2003928a
trigger835_2003921b:
        mov eax, dword ptr [esi+trigger835Weapon]
        push eax
        call BG_FiremodeWeapon
        mov ecx, dword ptr [pm]
        add esp, 4
        test eax, eax
        je trigger835_2003930c
        mov esi, dword ptr [ecx]
        mov eax, dword ptr [esi+trigger835Mode]
        cmp eax, 2
        je trigger835_2003928a
        cmp eax, edi
        jne trigger835_2003926c
        mov edx, dword ptr [esi+trigger835Weapon]
        lea eax, [edx + edx*2]
        shl eax, 3
        sub eax, edx
        lea edx, [eax + eax*4]
        mov eax, dword ptr [edx*4 + weaponDef+trigger835Burst]
        mov edx, dword ptr [esi+trigger835BurstCount]
        dec eax
        cmp edx, eax
        jg trigger835_2003928a
trigger835_2003926c:
        mov edx, dword ptr [esi+trigger835Weapon]
        lea eax, [edx + edx*2]
        shl eax, 3
        sub eax, edx
        lea edx, [eax + eax*4]
        cmp dword ptr [edx*4 + weaponDef+trigger835Single], ebp
        je trigger835_2003930c
trigger835_2003928a:
        mov dword ptr [esi+trigger835Time], ebp
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov dword ptr [ecx+trigger835Delay], ebp
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx]
        mov eax, dword ptr [ecx+trigger835State]
        cmp eax, 7
        je trigger835_200392af
        cmp eax, 8
        jne trigger835_200392f6
trigger835_200392af:
        mov eax, dword ptr [ecx+trigger835Weapon]
        push eax
        call BG_FindClipForWeapon
        mov ecx, dword ptr [pm]
        add esp, 4
        mov edx, dword ptr [ecx]
        cmp dword ptr [edx + eax*4 +trigger835Clip], ebp
        jne trigger835_200392ed
        push edi
        call PM_StartWeaponAnim
        mov eax, dword ptr [pm]
        add esp, 4
        mov ecx, dword ptr [eax]
        pop edi
        pop esi
        mov dword ptr [ecx+trigger835State], ebp
        pop ebp
        pop ebx
        add esp, 0x38
        mov eax, 1
        ret 
trigger835_200392ed:
        push ebp
        call PM_ContinueWeaponAnim
        add esp, 4
trigger835_200392f6:
        mov edx, dword ptr [pm]
        pop edi
        pop esi
        mov eax, dword ptr [edx]
        mov dword ptr [eax+trigger835State], ebp
        pop ebp
        pop ebx
        add esp, 0x38
        mov eax, 1
        ret 
trigger835_2003930c:
        test byte ptr [ecx +trigger835Buttons], 1
        jne trigger835_2003932b
        mov ecx, dword ptr [ecx]
        mov eax, dword ptr [ecx+trigger835Flags]
        and ah, 0xfb
        mov dword ptr [ecx+trigger835Flags], eax
        jmp trigger835_20039325
trigger835_2003931f:
        or ah, 4
        mov dword ptr [edx+trigger835Flags], eax
trigger835_20039325:
        mov ecx, dword ptr [pm]
trigger835_2003932b:
        mov edx, dword ptr [ecx]
        mov eax, dword ptr [edx+trigger835State]
        cmp eax, 9
        je trigger835_20039342
        cmp eax, 0xb
        je trigger835_20039342
        cmp eax, 0xc
        jne trigger835_2003934d
trigger835_20039342:
        call PM_FinishWeaponReload
        mov ecx, dword ptr [pm]
trigger835_2003934d:
        xor eax, eax
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        ret
    }
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
/* PM_Weapon200393ac..2003986e: charge/release/lean/zoom/water eligibility.
 * Server-only: original cgame omits the zoomed FieldOps fire event. */
static const float attack837K200ac2f0 = 0.6600000262260437f;
static const float attack837K200ac180 = 0.5f;
static const float attack837K200ac814 = 0.3499999940395355f;
static const float attack837K200ac2ec = 0.33000001311302185f;
static const float attack837K200ac2e8 = 0.15000000596046448f;
static const float attack837K200ac198 = 0.25f;
static const float attack837K200ac100 = 0.0f;
enum {
    attack837CmdTime = offsetof(pmove_t,cmd)+offsetof(usercmd_t,serverTime),
    attack837Buttons = offsetof(pmove_t,cmd)+offsetof(usercmd_t,buttons),
    attack837Skill = offsetof(pmove_t,skill),
    attack837Character = offsetof(pmove_t,character),
    attack837AnimModel = offsetof(bg_character_t,animModelInfo),
    attack837Soldier = offsetof(pmove_t,soldierChargeTime),
    attack837Engineer = offsetof(pmove_t,engineerChargeTime),
    attack837Medic = offsetof(pmove_t,medicChargeTime),
    attack837Lt = offsetof(pmove_t,ltChargeTime),
    attack837Covert = offsetof(pmove_t,covertopsChargeTime),
    attack837Water = offsetof(pmove_t,waterlevel),
    attack837Weapon = offsetof(playerState_t,weapon),
    attack837State = offsetof(playerState_t,weaponstate),
    attack837ClassTime = offsetof(playerState_t,classWeaponTime),
    attack837Eflags = offsetof(playerState_t,eFlags),
    attack837Lean = offsetof(playerState_t,leanf),
    attack837Time = offsetof(playerState_t,weaponTime),
    attack837Delay = offsetof(playerState_t,weaponDelay),
    attack837WeaponFlags = offsetof(playerState_t,stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    attack837TcFlags = offsetof(playerState_t,stats)+STAT_TCE_FLAGS*sizeof(int),
    attack837Class = offsetof(playerState_t,stats)+STAT_PLAYER_CLASS*sizeof(int),
    attack837BurstCount = offsetof(playerState_t,holdable)+11*sizeof(int),
    attack837Mode = offsetof(playerState_t,persistant)+10*sizeof(int)
};
typedef char attack837Protocol[(offsetof(pmove_t,ps)==0 && SK_HEAVY_WEAPONS==5 && SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS==6 && SK_EXPLOSIVES_AND_CONSTRUCTION==1 && SK_SIGNALS==3 && SK_FIRST_AID==2 && PC_FIELDOPS==3 && WEAPON_READY==0 && BUTTON_ATTACK==1) ? 1 : -1];
static __declspec(naked) qboolean PM_TCEWeaponEligibility837(qboolean delayedFire) {
    __asm {
        sub esp, 0x38
        push ebx
        push ebp
        push esi
        push edi
        mov ebx, dword ptr [esp+0x4c]
        xor ebp, ebp
        mov edi, 1
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov esi, dword ptr [edx+attack837Weapon]
        cmp esi, ebp
        je attack837_exit
        cmp esi, 0x41
        jne attack837_2003940f
        test dword ptr [edx+attack837Eflags], 0x80000
        jne attack837_exit
        mov eax, dword ptr [ecx+attack837Skill]
        cmp dword ptr [eax+5*4], edi
        mov eax, dword ptr [ecx+attack837CmdTime]
        jl attack837_20039400
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Soldier]
        fmul dword ptr [attack837K200ac2f0]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_exit
        jmp attack837_2003940f
attack837_20039400:
        sub eax, dword ptr [edx+attack837ClassTime]
        cmp eax, dword ptr [ecx+attack837Soldier]
        jl attack837_exit
attack837_2003940f:
        cmp esi, 0x37
        je attack837_20039419
        cmp esi, 0x38
        jne attack837_20039440
attack837_20039419:
        mov eax, dword ptr [ecx+attack837CmdTime]
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Engineer]
        fmul dword ptr [attack837K200ac180]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_exit
attack837_20039440:
        cmp esi, 0x3c
        jne attack837_200394ad
        mov eax, dword ptr [ecx+attack837Skill]
        mov esi, dword ptr [eax+5*4]
        mov eax, dword ptr [ecx+attack837CmdTime]
        cmp esi, edi
        mov esi, dword ptr [edx+attack837ClassTime]
        jl attack837_2003947d
        sub eax, esi
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Soldier]
        fmul dword ptr [attack837K200ac814]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_exit
        jmp attack837_2003949d
attack837_2003947d:
        sub eax, esi
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Soldier]
        fmul dword ptr [attack837K200ac180]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_exit
attack837_2003949d:
        cmp ebx, ebp
        jne attack837_200394ad
        mov dword ptr [edx+attack837State], ebp
        mov ecx, dword ptr [pm]
attack837_200394ad:
        mov edx, dword ptr [ecx]
        mov esi, dword ptr [edx+attack837Weapon]
        cmp esi, 0x1b
        jne attack837_200394fe
        mov eax, dword ptr [ecx+attack837Skill]
        cmp dword ptr [eax+6*4], 2
        mov eax, dword ptr [ecx+attack837CmdTime]
        jl attack837_200394ef
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Covert]
        fmul dword ptr [attack837K200ac2f0]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_exit
        jmp attack837_200394fe
attack837_200394ef:
        sub eax, dword ptr [edx+attack837ClassTime]
        cmp eax, dword ptr [ecx+attack837Covert]
        jl attack837_exit
attack837_200394fe:
        cmp esi, 0x1a
        jne attack837_2003955c
        mov eax, dword ptr [ecx+attack837Skill]
        cmp dword ptr [eax+1*4], 2
        mov eax, dword ptr [ecx+attack837CmdTime]
        jl attack837_20039538
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Engineer]
        fmul dword ptr [attack837K200ac2ec]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_exit
        jmp attack837_2003955c
attack837_20039538:
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Engineer]
        fmul dword ptr [attack837K200ac180]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_exit
attack837_2003955c:
        cmp esi, 0xf
        jne attack837_200395a5
        mov eax, dword ptr [ecx+attack837Skill]
        cmp dword ptr [eax+1*4], 3
        mov eax, dword ptr [ecx+attack837CmdTime]
        jl attack837_20039596
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Engineer]
        fmul dword ptr [attack837K200ac2f0]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_exit
        jmp attack837_200395a5
attack837_20039596:
        sub eax, dword ptr [edx+attack837ClassTime]
        cmp eax, dword ptr [ecx+attack837Engineer]
        jl attack837_exit
attack837_200395a5:
        cmp esi, 0xc
        jne attack837_200395fa
        mov eax, dword ptr [ecx+attack837Skill]
        cmp dword ptr [eax+3*4], edi
        mov eax, dword ptr [ecx+attack837CmdTime]
        jl attack837_200395da
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Lt]
        fmul dword ptr [attack837K200ac2e8]
        fcompp 
        fnstsw ax
        test ah, 0x41
        jne attack837_200395fa
        jmp attack837_20039647
attack837_200395da:
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Lt]
        fmul dword ptr [attack837K200ac198]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_20039647
attack837_200395fa:
        cmp esi, 0x13
        jne attack837_2003966d
        mov eax, dword ptr [ecx+attack837Skill]
        cmp dword ptr [eax+2*4], 2
        mov eax, dword ptr [ecx+attack837CmdTime]
        jl attack837_20039627
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Medic]
        fmul dword ptr [attack837K200ac2e8]
        jmp attack837_2003963e
attack837_20039627:
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Medic]
        fmul dword ptr [attack837K200ac198]
attack837_2003963e:
        fcompp 
        fnstsw ax
        test ah, 0x41
        jne attack837_2003966d
attack837_20039647:
        test byte ptr [ecx+attack837Buttons], 1
        je attack837_exit
        mov ecx, dword ptr [ecx+attack837Character]
        push ebp
        push edi
        push ANIM_ET_NOPOWER
        mov eax, dword ptr [ecx+attack837AnimModel]
        push eax
        push edx
        call BG_AnimScriptEvent
        add esp, 0x14
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        mov eax, 1
        ret 
attack837_2003966d:
        cmp esi, 0x16
        jne attack837_200396b6
        mov eax, dword ptr [ecx+attack837Skill]
        cmp dword ptr [eax+3*4], 2
        mov eax, dword ptr [ecx+attack837CmdTime]
        jl attack837_200396a7
        sub eax, dword ptr [edx+attack837ClassTime]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        fild dword ptr [ecx+attack837Lt]
        fmul dword ptr [attack837K200ac2f0]
        fcompp 
        fnstsw ax
        test ah, 0x41
        je attack837_exit
        jmp attack837_200396b6
attack837_200396a7:
        sub eax, dword ptr [edx+attack837ClassTime]
        cmp eax, dword ptr [ecx+attack837Lt]
        jl attack837_exit
attack837_200396b6:
        cmp esi, 0x3d
        jne attack837_200396cd
        mov eax, dword ptr [ecx+attack837CmdTime]
        sub eax, dword ptr [edx+attack837ClassTime]
        cmp eax, dword ptr [ecx+attack837Medic]
        jl attack837_exit
attack837_200396cd:
        test byte ptr [ecx+attack837Buttons], 1
        jne attack837_200396d7
        cmp ebx, ebp
        je attack837_2003972d
attack837_200396d7:
        fld dword ptr [edx+attack837Lean]
        fcomp dword ptr [attack837K200ac100]
        fnstsw ax
        test ah, 0x40
        jne attack837_200396ff
        cmp esi, 4
        je attack837_200396ff
        cmp esi, 9
        je attack837_200396ff
        cmp esi, 0x1e
        je attack837_200396ff
        test byte ptr [edx+attack837WeaponFlags], 4
        je attack837_2003972d
attack837_200396ff:
        mov eax, dword ptr [edx+attack837WeaponFlags]
        test ah, 0x40
        je attack837_200397d4
        cmp ebx, ebp
        je attack837_2003972d
        cmp esi, 4
        je attack837_200397d4
        cmp esi, 9
        je attack837_200397d4
        cmp esi, 0x1e
        je attack837_200397d4
attack837_2003972d:
        mov dword ptr [edx+attack837Time], ebp
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+attack837Delay], ebp
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov eax, dword ptr [ecx+attack837State]
        cmp eax, 7
        je attack837_20039752
        cmp eax, 8
        jne attack837_20039767
attack837_20039752:
        mov ecx, dword ptr [ecx+attack837Weapon]
        push ecx
        call TCE_PM_IdleAnimForWeapon
        push eax
        call PM_ContinueWeaponAnim
        add esp, 8
attack837_20039767:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        cmp dword ptr [eax+attack837BurstCount], ebp
        jle attack837_200397be
        mov eax, dword ptr [eax+attack837Weapon]
        push eax
        call BG_FiremodeWeapon
        add esp, 4
        test eax, eax
        je attack837_200397a1
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        cmp dword ptr [eax+attack837Mode], edi
        jne attack837_200397a1
        mov dword ptr [eax+attack837Time], 0x32
attack837_200397a1:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov dword ptr [eax+attack837BurstCount], ebp
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        and dword ptr [eax+attack837TcFlags], 0xfffffff7
attack837_200397be:
        mov edx, dword ptr [pm]
        pop edi
        pop esi
        mov eax, dword ptr [edx]
        mov dword ptr [eax+attack837State], ebp
        pop ebp
        pop ebx
        add esp, 0x38
        mov eax, 1
        ret 
attack837_200397d4:
        cmp esi, 0x23
        je attack837_exit
        mov ebp, dword ptr [edx+attack837Eflags]
        test ebp, 0x40000
        je attack837_20039813
        cmp dword ptr [edx+attack837Class], 3
        jne attack837_exit
        mov esi, dword ptr [edx+attack837Time]
        push EV_FIRE_WEAPON
        add esi, 0x1f4
        mov dword ptr [edx+attack837Time], esi
        call PM_AddEvent
        add esp, 4
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        mov eax, 1
        ret 
attack837_20039813:
        cmp dword ptr [ecx+attack837Water], 3
        jne attack837_2003986e
        cmp esi, 1
        je attack837_2003986e
        cmp esi, 4
        je attack837_2003986e
        cmp esi, 9
        je attack837_2003986e
        cmp esi, 0xf
        je attack837_2003986e
        cmp esi, 0x1a
        je attack837_2003986e
        cmp esi, 0x1d
        je attack837_2003986e
        cmp esi, 0x1e
        je attack837_2003986e
        push EV_NOFIRE_UNDERWATER
        call PM_AddEvent
        mov ecx, dword ptr [pm]
        add esp, 4
        mov edx, dword ptr [ecx]
        pop edi
        pop esi
        pop ebp
        mov dword ptr [edx+attack837Time], 0x1f4
        mov eax, dword ptr [pm]
        pop ebx
        mov ecx, dword ptr [eax]
        mov dword ptr [ecx+attack837Delay], 0
        add esp, 0x38
        mov eax, 1
        ret 
attack837_2003986e:
        xor eax, eax
        jmp attack837_done
attack837_exit:
        mov eax, 1
attack837_done:
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        ret
    }
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
/* Private PM_Weapon attack-start range2003986e..20039bc2.
 * Dispatch bytes are TC protocol; delay values are the separate static ammo
 * field, not new weaponDef records. Caller restricts definitions to0..63. */
static const unsigned char start838Dispatch[65] = {0, 1, 2, 3, 1, 1, 1, 2, 3, 2, 8, 8, 1, 1, 3, 8, 8, 8, 2, 8, 2, 2, 1, 1, 1, 4, 5, 6, 5, 3, 2, 1, 2, 8, 8, 2, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 8, 8, 8, 1, 1, 1, 1, 1, 1, 1, 1, 2, 7, 8, 2, 8, 8, 1};
static const int start838StaticDelay[64] = {50, 50, 50, 50, 100, 750, 50, 50, 50, 100, 50, 50, 50, 50, 50, 100, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 722, 100, 100, 50, 50, 50, 0, 750, 750, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
enum {
    start838Weapon = offsetof(playerState_t,weapon),
    start838State = offsetof(playerState_t,weaponstate),
    start838Delay = offsetof(playerState_t,weaponDelay),
    start838Grenade = offsetof(playerState_t,grenadeTimeLeft),
    start838Eflags = offsetof(playerState_t,eFlags),
    start838Character = offsetof(pmove_t,character),
    start838AnimModel = offsetof(bg_character_t,animModelInfo),
    start838DefDelay = offsetof(tce_weaponDef_t,fireDelayTime),
    start838DefTimer = offsetof(tce_weaponDef_t,grenadeTimer)
};
typedef char start838Protocol[(offsetof(pmove_t,ps)==0 && sizeof(tce_weaponDef_t)==460 && offsetof(tce_weaponDef_t,fireDelayTime)==0xe8 && offsetof(tce_weaponDef_t,grenadeTimer)==0x198 && ANIM_ET_FIREWEAPON==2 && ANIM_ET_FIREWEAPON2==3 && ANIM_ET_FIREWEAPONPRONE==30 && ANIM_ET_FIREWEAPON2PRONE==31 && WEAPON_FIRING==7) ? 1 : -1];
static __declspec(naked) void PM_TCEAttackStart838(qboolean delayedFire, qboolean akimboFire) {
    __asm {
        sub esp, 0x38
        push ebx
        push ebp
        push esi
        push edi
        mov eax, dword ptr [esp+0x4c]
        mov dword ptr [esp+0x18], eax
        mov eax, dword ptr [esp+0x50]
        mov dword ptr [esp+0x1c], eax
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov esi, dword ptr [edx+start838Weapon]
        mov ebp, dword ptr [edx+start838Eflags]
        lea edi, [esi-1]
        cmp edi, 0x40
        ja start838_20039b63
        xor eax, eax
        mov al, byte ptr [edi+start838Dispatch]
        cmp eax, 0
        je start838_200399ad
        cmp eax, 1
        je start838_200398c6
        cmp eax, 2
        je start838_20039889
        cmp eax, 3
        je start838_200399c6
        cmp eax, 4
        je start838_20039a68
        cmp eax, 5
        je start838_20039ad5
        cmp eax, 6
        je start838_20039b16
        cmp eax, 7
        je start838_20039955
        cmp eax, 8
        je start838_20039b63
        jmp start838_20039b63
start838_20039889:
        mov eax, dword ptr [edx+start838State]
        cmp eax, 7
        je start838_200398b7
        cmp eax, 8
        je start838_200398b7
        lea eax, [esi + esi*2]
        shl eax, 3
        sub eax, esi
        lea eax, [eax + eax*4]
        mov eax, dword ptr [eax*4 + weaponDef+start838DefDelay]
        test eax, eax
        je start838_200398b7
        mov dword ptr [edx+start838Delay], eax
        jmp start838_20039bb0
start838_200398b7:
        push 1
        push 1
        test ebp, 0x80000
        jmp start838_20039b98
start838_200398c6:
        mov eax, dword ptr [edx+start838State]
        cmp eax, 7
        je start838_20039923
        cmp eax, 8
        je start838_20039923
        lea eax, [esi + esi*2]
        shl eax, 3
        sub eax, esi
        lea eax, [eax + eax*4]
        mov edi, dword ptr [eax*4 + weaponDef+start838DefDelay]
        test edi, edi
        je start838_20039923
        cmp esi, 0x41
        jne start838_20039901
        push EV_SPINUP
        call PM_AddEvent
        mov ecx, dword ptr [pm]
        add esp, 4
start838_20039901:
        mov ecx, dword ptr [ecx]
        mov edx, dword ptr [ecx+start838Weapon]
        lea eax, [edx + edx*2]
        shl eax, 3
        sub eax, edx
        lea edx, [eax + eax*4]
        mov eax, dword ptr [edx*4 + weaponDef+start838DefDelay]
        mov dword ptr [ecx+start838Delay], eax
        jmp start838_20039bb0
start838_20039923:
        mov eax, dword ptr [esp + 0x1c]
        test ebp, 0x80000
        je start838_20039942
        test eax, eax
        push 1
        push 0
        je start838_20039b9a
        push 0x1f
        jmp start838_20039ba0
start838_20039942:
        test eax, eax
        push 1
        push 0
        je start838_20039b9e
        push 3
        jmp start838_20039ba0
start838_20039955:
        mov eax, dword ptr [edx+start838State]
        cmp eax, 7
        je start838_20039b8e
        cmp eax, 8
        je start838_20039b8e
        push EV_SPINUP
        call PM_AddEvent
        push 0x3c
        call TCE_PM_AttackAnimForWeapon
        push eax
        call PM_StartWeaponAnim
        mov ecx, dword ptr [pm]
        add esp, 0xc
        mov ecx, dword ptr [ecx]
start838_2003998c:
        mov edx, dword ptr [ecx+start838Weapon]
        mov eax, edx
        nop 
        nop 
        mov eax, dword ptr [eax*4+start838StaticDelay]
        mov dword ptr [ecx+start838Delay], eax
        jmp start838_20039bb0
start838_200399ad:
        mov eax, dword ptr [esp + 0x18]
        test eax, eax
        jne start838_20039bb6
        test ebp, 0x80000
        push 0
        jmp start838_20039b96
start838_200399c6:
        mov eax, dword ptr [esp + 0x18]
        test eax, eax
        jne start838_20039bb6
        push esi
        call PM_WeaponAmmoAvailable
        add esp, 4
        test eax, eax
        je start838_20039a40
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov ecx, dword ptr [edx+start838Weapon]
        cmp ecx, 0xf
        jne start838_200399fb
        mov dword ptr [edx+start838Grenade], 0x32
        jmp start838_20039a23
start838_200399fb:
        lea eax, [ecx + ecx*2]
        push EV_TCE_GRENADE_PRIME
        shl eax, 3
        sub eax, ecx
        lea eax, [eax + eax*4]
        mov ecx, dword ptr [eax*4 + weaponDef+start838DefTimer]
        add ecx, 0x1f4
        mov dword ptr [edx+start838Grenade], ecx
        call PM_AddEvent
        add esp, 4
start838_20039a23:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+start838Weapon]
        push ecx
        call TCE_PM_AttackAnimForWeapon
        push eax
        call PM_StartWeaponAnim
        add esp, 8
start838_20039a40:
        mov edx, dword ptr [pm]
        mov edx, dword ptr [edx]
        mov ecx, dword ptr [edx+start838Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea eax, [eax + eax*4]
        mov ecx, dword ptr [eax*4 + weaponDef+start838DefDelay]
        mov dword ptr [edx+start838Delay], ecx
        jmp start838_20039bb0
start838_20039a68:
        mov eax, dword ptr [esp + 0x18]
        test eax, eax
        jne start838_20039bb6
        push esi
        call PM_WeaponAmmoAvailable
        add esp, 4
        test eax, eax
        je start838_20039aac
        mov ecx, dword ptr [pm]
        push 1
        push 0
        mov eax, dword ptr [ecx]
        test dword ptr [eax+start838Eflags], 0x80000
        je start838_20039a9a
        push 0x1f
        jmp start838_20039a9c
start838_20039a9a:
        push 2
start838_20039a9c:
        mov edx, dword ptr [ecx +start838Character]
        mov ecx, dword ptr [edx +start838AnimModel]
        push ecx
        push eax
        call BG_AnimScriptEvent
        add esp, 0x14
start838_20039aac:
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx]
        mov edx, dword ptr [ecx+start838Weapon]
        mov eax, edx
        nop 
        nop 
        mov edx, dword ptr [eax*4+start838StaticDelay]
        mov dword ptr [ecx+start838Delay], edx
        jmp start838_20039bb0
start838_20039ad5:
        mov eax, dword ptr [esp + 0x18]
        test eax, eax
        jne start838_20039bb6
        push esi
        call PM_WeaponAmmoAvailable
        add esp, 4
        test eax, eax
        je start838_20039b0a
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [ecx+start838Weapon]
        push edx
        call TCE_PM_AttackAnimForWeapon
        push eax
        call PM_StartWeaponAnim
        add esp, 8
start838_20039b0a:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        jmp start838_2003998c
start838_20039b16:
        mov eax, dword ptr [edx+start838State]
        cmp eax, 7
        je start838_20039b8e
        cmp eax, 8
        je start838_20039b8e
        push EV_SPINUP
        call PM_AddEvent
        mov ecx, dword ptr [pm]
        push 0x1c
        mov ecx, dword ptr [ecx]
        mov edx, dword ptr [ecx+start838Weapon]
        mov eax, edx
        nop 
        nop 
        mov eax, dword ptr [eax*4+start838StaticDelay]
        mov dword ptr [ecx+start838Delay], eax
        call TCE_PM_AttackAnimForWeapon
        push eax
        call PM_ContinueWeaponAnim
        add esp, 0xc
        jmp start838_20039bb0
start838_20039b63:
        mov eax, dword ptr [edx+start838State]
        cmp eax, 7
        je start838_20039b8e
        cmp eax, 8
        je start838_20039b8e
        lea eax, [esi + esi*2]
        shl eax, 3
        sub eax, esi
        lea eax, [eax + eax*4]
        mov eax, dword ptr [eax*4 + weaponDef+start838DefDelay]
        test eax, eax
        je start838_20039b8e
        mov dword ptr [edx+start838Delay], eax
        jmp start838_20039bb0
start838_20039b8e:
        test ebp, 0x80000
        push 1
start838_20039b96:
        push 0
start838_20039b98:
        je start838_20039b9e
start838_20039b9a:
        push 0x1e
        jmp start838_20039ba0
start838_20039b9e:
        push 2
start838_20039ba0:
        mov ecx, dword ptr [ecx +start838Character]
        mov eax, dword ptr [ecx +start838AnimModel]
        push eax
        push edx
        call BG_AnimScriptEvent
        add esp, 0x14
start838_20039bb0:
        mov ecx, dword ptr [pm]
start838_20039bb6:
        mov ecx, dword ptr [ecx]
        mov dword ptr [ecx+start838State], 7
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        ret
    }
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
/* Private PM_Weapon20039bc2..20039f0d: ammo feedback, delay, slick kick,
 * and ammo consumption. TC IDs; no new definition65/66. */
static const float ammo839K200ac160 = 2000.0f;
static const float ammo839K200ac810 = 32000.0f;
static const float ammo839K200ac45c = 400.0f;
static const float ammo839K200ac100 = 0.0f;
static const float ammo839K200ac698 = 0.004999999888241291f;
static const float ammo839K200ac2e4 = -1.0f;
static const unsigned char ammo839Dispatch[56] = {0, 2, 2, 2, 2, 0, 2, 2, 2, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1};
enum {
    ammo839Weapon = offsetof(playerState_t,weapon),
    ammo839Ammo = offsetof(playerState_t,ammo),
    ammo839Time = offsetof(playerState_t,weaponTime),
    ammo839Delay = offsetof(playerState_t,weaponDelay),
    ammo839WeaponFlags = offsetof(playerState_t,stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    ammo839Eflags = offsetof(playerState_t,eFlags),
    ammo839VelocityX = offsetof(playerState_t,velocity),
    ammo839VelocityY = offsetof(playerState_t,velocity)+4,
    ammo839VelocityZ = offsetof(playerState_t,velocity)+8,
    ammo839PmTime = offsetof(playerState_t,pm_time),
    ammo839Flags = offsetof(playerState_t,pm_flags),
    ammo839Mounted = offsetof(playerState_t,persistant)+PERS_HWEAPON_USE*sizeof(int),
    ammo839Ext = offsetof(pmove_t,pmext),
    ammo839AutoReload = offsetof(pmoveExt_t,bAutoReload),
    ammo839Wbuttons = offsetof(pmove_t,cmd)+offsetof(usercmd_t,wbuttons),
    ammo839Uses = offsetof(tce_weaponDef_t,uses),
    ammo839Single = offsetof(tce_weaponDef_t,singleReload),
    ammo839GroundFlags = offsetof(pml_t,groundTrace)+offsetof(trace_t,surfaceFlags),
    ammo839ForwardX = offsetof(pml_t,forward),
    ammo839ForwardY = offsetof(pml_t,forward)+4,
    ammo839ForwardZ = offsetof(pml_t,forward)+8
};
typedef char ammo839Protocol[(offsetof(pmove_t,ps)==0 && sizeof(tce_weaponDef_t)==460 && offsetof(tce_weaponDef_t,uses)==0xd4 && offsetof(tce_weaponDef_t,singleReload)==0x170 && WBUTTON_RELOAD==8 && SURF_SLICK==2 && EF_PRONE==0x80000 && EF_MOUNTEDTANK==0x8000 && PMF_TIME_KNOCKBACK==0x40) ? 1 : -1];
static __declspec(naked) qboolean PM_TCEAmmoConsume839(void) {
    __asm {
        sub esp, 0x38
        push ebx
        push ebp
        push esi
        push edi
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+ammo839Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        test ecx, ecx
        lea edx, [eax + eax*4]
        mov esi, dword ptr [edx*4 + weaponDef+ammo839Uses]
        je ammo839_20039dc0
        push ecx
        call PM_WeaponAmmoAvailable
        add esp, 4
        cmp esi, eax
        jle ammo839_20039dc0
        mov eax, dword ptr [pm]
        mov edi, dword ptr [eax]
        mov eax, edi
        mov ecx, dword ptr [eax+ammo839Weapon]
        push ecx
        call BG_FindAmmoForWeapon
        mov ebx, dword ptr [edi + eax*4 +ammo839Ammo]
        mov ecx, dword ptr [pm]
        add esp, 4
        xor edx, edx
        mov eax, dword ptr [ecx+ammo839Ext]
        cmp esi, ebx
        setle dl
        mov esi, edx
        mov edx, dword ptr [eax+ammo839AutoReload]
        test edx, edx
        jne ammo839_20039d05
        mov eax, dword ptr [edi+ammo839Weapon]
        cmp eax, 0x30
        je ammo839_20039cfd
        cmp eax, 0x33
        je ammo839_20039cfd
        cmp eax, 0x2c
        je ammo839_20039cfd
        cmp eax, 0x2d
        je ammo839_20039cfd
        cmp eax, 0x2b
        je ammo839_20039cfd
        cmp eax, 0x29
        je ammo839_20039cfd
        cmp eax, 0x2a
        je ammo839_20039cfd
        cmp eax, 0x2f
        je ammo839_20039cfd
        cmp eax, 0x2e
        je ammo839_20039cfd
        cmp eax, 0x27
        je ammo839_20039cfd
        cmp eax, 0x28
        je ammo839_20039cfd
        cmp eax, 0x25
        je ammo839_20039cfd
        cmp eax, 2
        je ammo839_20039cfd
        cmp eax, 7
        je ammo839_20039cfd
        cmp eax, 3
        je ammo839_20039cfd
        cmp eax, 8
        je ammo839_20039cfd
        cmp eax, 0xa
        je ammo839_20039cfd
        cmp eax, 0x31
        je ammo839_20039cfd
        cmp eax, 0x32
        je ammo839_20039cfd
        cmp eax, 0x17
        je ammo839_20039cfd
        cmp eax, 0x18
        je ammo839_20039cfd
        cmp eax, 0x39
        je ammo839_20039cfd
        cmp eax, 0x21
        je ammo839_20039cfd
        cmp eax, 0x20
        je ammo839_20039cfd
        cmp eax, 0x1f
        je ammo839_20039cfd
        cmp eax, 0x34
        je ammo839_20039cfd
        cmp eax, 0xe
        je ammo839_20039cfd
        cmp eax, 5
        je ammo839_20039cfd
        cmp eax, 6
        je ammo839_20039cfd
        cmp eax, 0xd
        je ammo839_20039cfd
        cmp eax, 0x19
        je ammo839_20039cfd
        cmp eax, 0x3a
        je ammo839_20039cfd
        cmp eax, 0x3b
        jne ammo839_20039d05
ammo839_20039cfd:
        test byte ptr [ecx+ammo839Wbuttons], WBUTTON_RELOAD
        jne ammo839_20039d05
        xor esi, esi
ammo839_20039d05:
        mov eax, dword ptr [edi+ammo839Weapon]
        add eax, -4
        cmp eax, 0x37
        ja ammo839_20039d22
        xor ecx, ecx
        mov cl, byte ptr [eax + ammo839Dispatch]
        cmp ecx, 0
        je ammo839_20039d30
        cmp ecx, 1
        je ammo839_20039d48
        cmp ecx, 2
        je ammo839_20039d22
        jmp ammo839_20039d22
ammo839_20039d22:
        test esi, esi
        je ammo839_20039d48
        push EV_EMPTYCLIP
        call PM_AddEvent
        add esp, 4
ammo839_20039d30:
        test esi, esi
        je ammo839_20039d52
        push 1
        call PM_ContinueWeaponAnim
        mov edx, dword ptr [pm]
        add esp, 4
        mov eax, dword ptr [edx]
        jmp ammo839_20039d75
ammo839_20039d48:
        push EV_NOAMMO
        call PM_AddEvent
        add esp, 4
ammo839_20039d52:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [ecx+ammo839Weapon]
        push edx
        call TCE_PM_IdleAnimForWeapon
        push eax
        call PM_ContinueWeaponAnim
        mov eax, dword ptr [pm]
        add esp, 8
        mov eax, dword ptr [eax]
ammo839_20039d75:
        mov ecx, dword ptr [eax+ammo839Time]
        add ecx, 0x1f4
        mov dword ptr [eax+ammo839Time], ecx
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov ecx, dword ptr [edx+ammo839Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea eax, [eax + eax*4]
        mov ecx, dword ptr [eax*4 + weaponDef+ammo839Single]
        test ecx, ecx
        je ammo839_exit
        mov eax, dword ptr [edx+ammo839WeaponFlags]
        pop edi
        or ah, 8
        pop esi
        pop ebp
        mov dword ptr [edx+ammo839WeaponFlags], eax
        pop ebx
        add esp, 0x38
        mov eax, 1
        ret 
ammo839_20039dc0:
        mov ecx, dword ptr [pm]
        mov ecx, dword ptr [ecx]
        mov eax, dword ptr [ecx+ammo839Delay]
        test eax, eax
        jg ammo839_exit
        test dword ptr [ecx+ammo839Eflags], 0x80000
        jne ammo839_20039ec4
        test byte ptr [pml+ammo839GroundFlags], 2
        je ammo839_20039ec4
        mov eax, dword ptr [ecx+ammo839Weapon]
        sub eax, 0x1f
        je ammo839_20039e14
        sub eax, 0x22
        je ammo839_20039e0c
        dec eax
        jne ammo839_20039ec4
        fld dword ptr [ammo839K200ac160]
        jmp ammo839_20039e1a
ammo839_20039e0c:
        fld dword ptr [ammo839K200ac810]
        jmp ammo839_20039e1a
ammo839_20039e14:
        fld dword ptr [ammo839K200ac45c]
ammo839_20039e1a:
        fld dword ptr [pml+ammo839ForwardZ]
        fmul dword ptr [ecx+ammo839VelocityZ]
        fld dword ptr [pml+ammo839ForwardY]
        fmul dword ptr [ecx+ammo839VelocityY]
        faddp st(1), st(0)
        fld dword ptr [pml+ammo839ForwardX]
        fmul dword ptr [ecx+ammo839VelocityX]
        faddp st(1), st(0)
        fcomp dword ptr [ammo839K200ac100]
        fmul dword ptr [ammo839K200ac698]
        fnstsw ax
        fmul dword ptr [ammo839K200ac2e4]
        test ah, 0x41
        fst dword ptr [esp + 0x18]
        fmul dword ptr [pml+ammo839ForwardX]
        fld dword ptr [esp + 0x18]
        fmul dword ptr [pml+ammo839ForwardY]
        fstp dword ptr [esp + 0x40]
        fld dword ptr [esp + 0x18]
        fmul dword ptr [pml+ammo839ForwardZ]
        fstp dword ptr [esp + 0x44]
        fadd dword ptr [ecx+ammo839VelocityX]
        fstp dword ptr [ecx+ammo839VelocityX]
        mov edx, dword ptr [pm]
        fld dword ptr [esp + 0x40]
        mov eax, dword ptr [edx]
        fadd dword ptr [eax+ammo839VelocityY]
        fstp dword ptr [eax+ammo839VelocityY]
        mov eax, dword ptr [pm]
        fld dword ptr [esp + 0x44]
        mov eax, dword ptr [eax]
        fadd dword ptr [eax+ammo839VelocityZ]
        fstp dword ptr [eax+ammo839VelocityZ]
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        mov ecx, dword ptr [eax+ammo839PmTime]
        test ecx, ecx
        jne ammo839_20039ec4
        mov ebx, 0x64
        mov dword ptr [eax+ammo839PmTime], ebx
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        or dword ptr [eax+ammo839Flags], 0x40
        jmp ammo839_20039ec9
ammo839_20039ec4:
        mov ebx, 0x64
ammo839_20039ec9:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [ecx+ammo839Weapon]
        push edx
        call PM_WeaponAmmoAvailable
        add esp, 4
        cmp eax, -1
        je ammo839_20039f0d
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        mov ecx, dword ptr [eax+ammo839Mounted]
        test ecx, ecx
        jne ammo839_20039f0d
        mov ecx, dword ptr [eax+ammo839Eflags]
        test ch, 0x80
        jne ammo839_20039f0d
        mov ecx, dword ptr [eax+ammo839Weapon]
        push esi
        push ecx
        call PM_WeaponUseAmmo
        add esp, 8
ammo839_20039f0d:
        xor eax, eax
        jmp ammo839_done
ammo839_exit:
        mov eax, 1
ammo839_done:
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        ret
    }
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
/* Private contiguous PM_Weapon20039f0d..2003a163. Return the original EDI
 * attack animation live-out used by the subsequent cadence switch. */
static const unsigned char fire840Dispatch[60] = {0, 1, 4, 4, 4, 0, 1, 0, 4, 4, 4, 4, 2, 4, 4, 4, 0, 4, 0, 0, 2, 2, 4, 2, 4, 0, 2, 1, 0, 2, 4, 4, 4, 0, 4, 4, 4, 4, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 4, 4, 4, 3, 4, 0};
enum {
    fire840Weapon = offsetof(playerState_t,weapon),
    fire840Clip28 = offsetof(playerState_t,ammoclip)+28*sizeof(int),
    fire840Ammo27 = offsetof(playerState_t,ammo)+27*sizeof(int),
    fire840Clip27 = offsetof(playerState_t,ammoclip)+27*sizeof(int),
    fire840Ammo35 = offsetof(playerState_t,ammo)+35*sizeof(int),
    fire840Ammo = offsetof(playerState_t,ammo),
    fire840Mode = offsetof(playerState_t,persistant)+10*sizeof(int),
    fire840LastFire = offsetof(playerState_t,lastFireTime),
    fire840Ext = offsetof(pmove_t,pmext),
    fire840Released = offsetof(pmoveExt_t,releasedFire),
    fire840ServerTime = offsetof(pmove_t,cmd)+offsetof(usercmd_t,serverTime),
    fire840Pump = offsetof(tce_weaponDef_t,pump)
};
typedef char fire840Protocol[(offsetof(pmove_t,ps)==0 && sizeof(tce_weaponDef_t)==460 && offsetof(tce_weaponDef_t,pump)==0x168 && WEAP_ATTACK1==2 && WEAP_ATTACK2==3 && EV_NOAMMO==32 && EV_FIRE_WEAPON==40 && EV_FIRE_WEAPONB==41 && EV_FIRE_WEAPON_LASTSHOT==42) ? 1 : -1];
static __declspec(naked) int PM_TCEFireEvents840(qboolean akimboFire) {
    __asm {
        sub esp, 0x38
        push ebx
        push ebp
        push esi
        push edi
        mov eax, dword ptr [esp+0x4c]
        mov dword ptr [esp+0x1c], eax
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+fire840Weapon]
        push ecx
        call BG_IsAkimboWeapon
        add esp, 4
        test eax, eax
        je fire840_20039f35
        mov edi, dword ptr [esp + 0x1c]
        neg edi
        sbb edi, edi
        add edi, 3
        jmp fire840_20039f70
fire840_20039f35:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+fire840Weapon]
        push ecx
        call PM_WeaponClipEmpty
        mov edx, dword ptr [pm]
        add esp, 4
        test eax, eax
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+fire840Weapon]
        push ecx
        je fire840_20039f66
        call TCE_PM_LastAttackAnimForWeapon
        jmp fire840_20039f6b
fire840_20039f66:
        call TCE_PM_AttackAnimForWeapon
fire840_20039f6b:
        add esp, 4
        mov edi, eax
fire840_20039f70:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov eax, dword ptr [eax+fire840Weapon]
        add eax, -3
        cmp eax, 0x3b
        ja fire840_20039fa1
        xor ecx, ecx
        mov cl, byte ptr [eax + fire840Dispatch]
        cmp ecx, 0
        je fire840_20039f99
        cmp ecx, 1
        je fire840_20039f95
        cmp ecx, 2
        je fire840_20039fa1
        cmp ecx, 3
        je fire840_20039faa
        cmp ecx, 4
        je fire840_20039fa1
        jmp fire840_20039fa1
fire840_20039f95:
        push 3
        jmp fire840_20039fa2
fire840_20039f99:
        push edi
        call PM_ContinueWeaponAnim
        jmp fire840_20039fa7
fire840_20039fa1:
        push edi
fire840_20039fa2:
        call PM_StartWeaponAnim
fire840_20039fa7:
        add esp, 4
fire840_20039faa:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov eax, dword ptr [eax+fire840Weapon]
        cmp eax, 0x41
        je fire840_20039fcc
        cmp eax, 0x16
        je fire840_20039fcc
        cmp eax, 0x1a
        je fire840_20039fcc
        cmp eax, 0x1b
        jne fire840_20039fd6
fire840_20039fcc:
        push 0x20
        call PM_AddEvent
        add esp, 4
fire840_20039fd6:
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        cmp dword ptr [eax+fire840Weapon], 0x1b
        jne fire840_2003a01c
        mov dword ptr [eax+fire840Clip28], 1
        mov edx, dword ptr [pm]
        xor eax, eax
        mov ecx, dword ptr [edx]
        push eax
        push 0x1c
        push 0x1b
        mov dword ptr [ecx+fire840Ammo27], eax
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx]
        mov dword ptr [ecx+fire840Clip27], eax
        call PM_BeginWeaponChange
        add esp, 0xc
fire840_2003a01c:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov eax, dword ptr [eax+fire840Weapon]
        cmp eax, 0x38
        je fire840_2003a034
        cmp eax, 0x37
        jne fire840_2003a05a
fire840_2003a034:
        push eax
        call BG_FindAmmoForWeapon
        mov ecx, dword ptr [pm]
        add esp, 4
        mov edx, dword ptr [ecx]
        mov ecx, dword ptr [edx + eax*4 +fire840Ammo]
        test ecx, ecx
        jne fire840_2003a05a
        push 0x20
        call PM_AddEvent
        add esp, 4
fire840_2003a05a:
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        cmp dword ptr [eax+fire840Weapon], 0x3c
        jne fire840_2003a07e
        mov ecx, dword ptr [eax+fire840Ammo35]
        test ecx, ecx
        jne fire840_2003a07e
        push 0x20
        call PM_AddEvent
        add esp, 4
fire840_2003a07e:
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov eax, dword ptr [edx+fire840Weapon]
        push eax
        call BG_IsAkimboWeapon
        add esp, 4
        test eax, eax
        je fire840_2003a0ac
        mov eax, dword ptr [esp + 0x1c]
        test eax, eax
        jne fire840_2003a13b
        push 0x29
        jmp fire840_2003a13d
fire840_2003a0ac:
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov ecx, dword ptr [edx+fire840Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea eax, [eax + eax*4]
        mov esi, dword ptr [eax*4 + weaponDef+fire840Pump]
        test esi, esi
        je fire840_2003a11c
        mov eax, dword ptr [edx+fire840Mode]
        test eax, eax
        jle fire840_2003a11c
        push ecx
        call BG_FiremodeWeapon
        add esp, 4
        test eax, eax
        je fire840_2003a11c
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov eax, dword ptr [edx+fire840Weapon]
        push eax
        call PM_WeaponClipEmpty
        add esp, 4
        test eax, eax
        push 1
        je fire840_2003a110
        push 0x2a
        call PM_AddEventExt
        add esp, 8
        jmp fire840_2003a145
fire840_2003a110:
        push 0x28
        call PM_AddEventExt
        add esp, 8
        jmp fire840_2003a145
fire840_2003a11c:
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov eax, dword ptr [edx+fire840Weapon]
        push eax
        call PM_WeaponClipEmpty
        add esp, 4
        test eax, eax
        je fire840_2003a13b
        push 0x2a
        jmp fire840_2003a13d
fire840_2003a13b:
        push 0x28
fire840_2003a13d:
        call PM_AddEvent
        add esp, 4
fire840_2003a145:
        mov ecx, dword ptr [pm]
        xor esi, esi
        mov edx, dword ptr [ecx+fire840Ext]
        mov dword ptr [edx+fire840Released], esi
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [eax+fire840ServerTime]
        mov dword ptr [ecx+fire840LastFire], edx
        mov eax, edi
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        ret
    }
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
/* Private PM_Weapon2003a163..2003a73d: seed, cadence and legacy recoil.
 * CRT rand remains a dependency boundary; no new slots65/66 are defined. */
static int (__cdecl *const cad841Rand)(void) = rand;
static const float cad841K200ac114 = 3.0518509447574615e-05f;
static const double cad841K200ac128 = 0.5;
static const double cad841K200ac170 = 0.25;
static const float cad841K200ac180 = 0.5f;
static const float cad841K200ac804 = 0.03750000149011612f;
static const float cad841K200ac808 = 0.06750000268220901f;
static const float cad841K200ac80c = 0.15000000596046448f;
static const unsigned char cad841Dispatch[66] = {0, 1, 2, 0, 1, 1, 3, 4, 0, 2, 5, 5, 1, 1, 0, 13, 13, 13, 6, 13, 7, 6, 1, 1, 1, 0, 13, 13, 0, 0, 8, 1, 1, 13, 13, 5, 9, 10, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 9, 10, 0, 0, 11, 11, 12, 0, 5, 8, 13, 13, 0, 0};
static const unsigned char cad841RecoilDispatch[58] = {0, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 4, 4, 4, 4, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 4, 2, 2, 3};
static const int cad841Legacy[64] = {0, 500, 150, 50, 1600, 2000, 50, 400, 75, 1600, 50, 1000, 1000, 1000, 400, 1600, 0, 0, 0, 0, 0, 0, 0, 400, 400, 400, 100, 2000, 2000, 2000, 1600, 50, 400, 75, 0, 1600, 1600, 100, 100, 150, 350, 75, 100, 100, 80, 75, 500, 700, 200, 350, 120, 700, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
enum {
    cad841Weapon = offsetof(playerState_t,weapon),
    cad841Seed = offsetof(playerState_t,stats)+STAT_TCE_SHOT_SEED*4,
    cad841WeaponFlags = offsetof(playerState_t,stats)+STAT_TCE_WEAPON_FLAGS*4,
    cad841Clips = offsetof(playerState_t,ammoclip),
    cad841Ext = offsetof(pmove_t,pmext),
    cad841ServerTime = offsetof(pmove_t,cmd)+offsetof(usercmd_t,serverTime),
    cad841Skill = offsetof(pmove_t,skill),
    cad841Pmflags = offsetof(playerState_t,pm_flags),
    cad841Eflags = offsetof(playerState_t,eFlags),
    cad841Delta = offsetof(pmoveExt_t,lastRecoilDeltaTime),
    cad841Time = offsetof(pmoveExt_t,weapRecoilTime),
    cad841Duration = offsetof(pmoveExt_t,weapRecoilDuration),
    cad841Yaw = offsetof(pmoveExt_t,weapRecoilYaw),
    cad841Pitch = offsetof(pmoveExt_t,weapRecoilPitch),
    cad841NextShot = offsetof(tce_weaponDef_t,nextShotTime),
    cad841Pump = offsetof(tce_weaponDef_t,pump)
};
typedef char cad841Protocol[(offsetof(pmove_t,ps)==0 && sizeof(tce_weaponDef_t)==460 && offsetof(tce_weaponDef_t,nextShotTime)==0xec && offsetof(tce_weaponDef_t,pump)==0x168 && SK_LIGHT_WEAPONS==4 && SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS==6) ? 1 : -1];
/* result[0]=masked seed; result[1]=cadence; result[2]=spread addition. */
static __declspec(naked) void PM_TCECadenceRecoil841(qboolean akimboFire, int attackAnimation, int *result) {
    __asm {
        sub esp, 0x38
        push ebx
        push ebp
        push esi
        push edi
        mov eax, dword ptr [esp+0x4c]
        mov dword ptr [esp+0x1c], eax
        mov edi, dword ptr [esp+0x50]
        xor esi, esi
        mov dword ptr [esp+0x14], esi
        mov ebx, 100
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        lea eax, [esp + 0x20]
        push eax
        mov edx, dword ptr [ecx+cad841Seed]
        mov dword ptr [esp + 0x24], edx
        call Q_rand
        mov eax, dword ptr [esp + 0x24]
        mov ecx, dword ptr [pm]
        and eax, 0xffff
        mov dword ptr [esp + 0x1c], esi
        mov dword ptr [esp + 0x24], eax
        mov edx, dword ptr [ecx]
        add esp, 4
        mov ebp, 0x3e8
        mov dword ptr [edx+cad841Seed], eax
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov eax, dword ptr [ecx+cad841Weapon]
        lea esi, [eax - 1]
        cmp esi, 0x41
        ja cad841_2003a471
        xor edx, edx
        mov dl, byte ptr [esi + cad841Dispatch]
        cmp edx, 0
        je cad841_2003a1cd
        cmp edx, 1
        je cad841_2003a1e8
        cmp edx, 2
        je cad841_2003a3e6
        cmp edx, 3
        je cad841_2003a23d
        cmp edx, 4
        je cad841_2003a3ce
        cmp edx, 5
        je cad841_2003a44a
        cmp edx, 6
        je cad841_2003a46d
        cmp edx, 7
        je cad841_2003a463
        cmp edx, 8
        je cad841_2003a412
        cmp edx, 9
        je cad841_2003a260
        cmp edx, 10
        je cad841_2003a2f6
        cmp edx, 11
        je cad841_2003a38c
        cmp edx, 12
        je cad841_2003a3af
        cmp edx, 13
        je cad841_2003a471
        jmp cad841_2003a471
cad841_2003a1cd:
        lea ecx, [eax + eax*2]
        shl ecx, 3
        sub ecx, eax
        lea eax, [ecx + ecx*4]
        mov ecx, dword ptr [eax*4 + weaponDef+cad841NextShot]
        mov dword ptr [esp + 0x14], ecx
        jmp cad841_2003a471
cad841_2003a1e8:
        lea edx, [eax + eax*2]
        shl edx, 3
        sub edx, eax
        lea edx, [edx + edx*4]
        mov eax, dword ptr [edx*4 + weaponDef+cad841Pump]
        test eax, eax
        je cad841_2003a20d
        mov eax, dword ptr [ecx+cad841WeaponFlags]
        and ah, 0xf7
        mov dword ptr [ecx+cad841WeaponFlags], eax
cad841_2003a20d:
        mov eax, dword ptr [pm]
        mov dword ptr [esp + 0x18], 0x23
        mov ecx, dword ptr [eax]
        mov ecx, dword ptr [ecx+cad841Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea edx, [eax + eax*4]
        mov eax, dword ptr [edx*4 + weaponDef+cad841NextShot]
        mov dword ptr [esp + 0x14], eax
        jmp cad841_2003a471
cad841_2003a23d:
        lea ecx, [eax + eax*2]
        mov dword ptr [esp + 0x18], 0x14
        shl ecx, 3
        sub ecx, eax
        lea ecx, [ecx + ecx*4]
        mov edx, dword ptr [ecx*4 + weaponDef+cad841NextShot]
        mov dword ptr [esp + 0x14], edx
        jmp cad841_2003a471
cad841_2003a260:
        lea ecx, [eax + eax*2]
        push eax
        shl ecx, 3
        sub ecx, eax
        lea ecx, [ecx + ecx*4]
        mov edx, dword ptr [ecx*4 + weaponDef+cad841NextShot]
        mov ecx, dword ptr [pm]
        mov dword ptr [esp + 0x18], edx
        mov esi, dword ptr [ecx]
        call BG_FindClipForWeapon
        mov ecx, dword ptr [esi + eax*4 +cad841Clips]
        add esp, 4
        test ecx, ecx
        jne cad841_2003a29c
        mov eax, dword ptr [esp + 0x1c]
        test eax, eax
        jne cad841_2003a2e9
        jmp cad841_2003a2cb
cad841_2003a29c:
        mov eax, dword ptr [pm]
        mov esi, dword ptr [eax]
        mov ecx, dword ptr [esi+cad841Weapon]
        push ecx
        call BG_AkimboSidearm
        push eax
        call BG_FindClipForWeapon
        mov ecx, dword ptr [esi + eax*4 +cad841Clips]
        add esp, 8
        test ecx, ecx
        jne cad841_2003a2e9
        mov eax, dword ptr [esp + 0x1c]
        test eax, eax
        je cad841_2003a2e9
cad841_2003a2cb:
        mov ecx, dword ptr [esi+cad841Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea edx, [eax + eax*4]
        mov eax, dword ptr [edx*4 + weaponDef+cad841NextShot]
        shl eax, 1
        mov dword ptr [esp + 0x14], eax
cad841_2003a2e9:
        mov dword ptr [esp + 0x18], 0x14
        jmp cad841_2003a471
cad841_2003a2f6:
        lea ecx, [eax + eax*2]
        push eax
        shl ecx, 3
        sub ecx, eax
        lea ecx, [ecx + ecx*4]
        mov edx, dword ptr [ecx*4 + weaponDef+cad841NextShot]
        mov ecx, dword ptr [pm]
        mov dword ptr [esp + 0x18], edx
        mov esi, dword ptr [ecx]
        call BG_FindClipForWeapon
        mov ecx, dword ptr [esi + eax*4 +cad841Clips]
        add esp, 4
        test ecx, ecx
        jne cad841_2003a332
        mov eax, dword ptr [esp + 0x1c]
        test eax, eax
        jne cad841_2003a37f
        jmp cad841_2003a361
cad841_2003a332:
        mov eax, dword ptr [pm]
        mov esi, dword ptr [eax]
        mov ecx, dword ptr [esi+cad841Weapon]
        push ecx
        call BG_AkimboSidearm
        push eax
        call BG_FindClipForWeapon
        mov ecx, dword ptr [esi + eax*4 +cad841Clips]
        add esp, 8
        test ecx, ecx
        jne cad841_2003a37f
        mov eax, dword ptr [esp + 0x1c]
        test eax, eax
        je cad841_2003a37f
cad841_2003a361:
        mov ecx, dword ptr [esi+cad841Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea edx, [eax + eax*4]
        mov eax, dword ptr [edx*4 + weaponDef+cad841NextShot]
        shl eax, 1
        mov dword ptr [esp + 0x14], eax
cad841_2003a37f:
        mov dword ptr [esp + 0x18], 0x23
        jmp cad841_2003a471
cad841_2003a38c:
        lea ecx, [eax + eax*2]
        mov dword ptr [esp + 0x18], 0xc8
        shl ecx, 3
        sub ecx, eax
        lea eax, [ecx + ecx*4]
        mov ecx, dword ptr [eax*4 + weaponDef+cad841NextShot]
        mov dword ptr [esp + 0x14], ecx
        jmp cad841_2003a471
cad841_2003a3af:
        lea ecx, [eax + eax*2]
        mov dword ptr [esp + 0x18], ebx
        shl ecx, 3
        sub ecx, eax
        lea edx, [ecx + ecx*4]
        mov eax, dword ptr [edx*4 + weaponDef+cad841NextShot]
        mov dword ptr [esp + 0x14], eax
        jmp cad841_2003a471
cad841_2003a3ce:
        lea ecx, [eax + eax*2]
        shl ecx, 3
        sub ecx, eax
        lea ecx, [ecx + ecx*4]
        mov edx, dword ptr [ecx*4 + weaponDef+cad841NextShot]
        mov dword ptr [esp + 0x14], edx
        jmp cad841_2003a3fc
cad841_2003a3e6:
        lea ecx, [eax + eax*2]
        shl ecx, 3
        sub ecx, eax
        lea edx, [ecx + ecx*4]
        mov eax, dword ptr [edx*4 + weaponDef+cad841NextShot]
        mov dword ptr [esp + 0x14], eax
cad841_2003a3fc:
        call dword ptr [cad841Rand]
        cdq 
        mov ecx, 0xa
        idiv ecx
        add edx, 0xf
        mov dword ptr [esp + 0x18], edx
        jmp cad841_2003a471
cad841_2003a412:
        cmp edi, 4
        jne cad841_2003a429
        mov dword ptr [esp + 0x14], 0x7d0
        mov dword ptr [esp + 0x18], 0x14
        jmp cad841_2003a471
cad841_2003a429:
        lea ecx, [eax*8]
        mov dword ptr [esp + 0x18], 0x14
        sub ecx, eax
        lea edx, [ecx + ecx*2]
        mov eax, dword ptr [eax*4+cad841Legacy]
        mov dword ptr [esp + 0x14], eax
        jmp cad841_2003a471
cad841_2003a44a:
        lea ecx, [eax*8]
        sub ecx, eax
        lea ecx, [ecx + ecx*2]
        mov edx, dword ptr [eax*4+cad841Legacy]
        mov dword ptr [esp + 0x14], edx
        jmp cad841_2003a471
cad841_2003a463:
        mov dword ptr [esp + 0x14], 0x32
        jmp cad841_2003a471
cad841_2003a46d:
        mov dword ptr [esp + 0x14], ebp
cad841_2003a471:
        mov eax, dword ptr [pm]
        xor edx, edx
        mov ecx, dword ptr [eax+cad841Ext]
        mov dword ptr [ecx+cad841Delta], edx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        mov ecx, dword ptr [ecx+cad841Weapon]
        add ecx, -2
        cmp ecx, 0x39
        ja cad841_2003a72c
        xor edx, edx
        mov dl, byte ptr [ecx + cad841RecoilDispatch]
        cmp edx, 0
        je cad841_2003a685
        cmp edx, 1
        je cad841_2003a51f
        cmp edx, 2
        je cad841_2003a4a6
        cmp edx, 3
        je cad841_2003a60e
        cmp edx, 4
        je cad841_2003a72a
        jmp cad841_2003a72a
cad841_2003a4a6:
        mov ecx, dword ptr [eax+cad841Ext]
        mov edx, dword ptr [eax+cad841ServerTime]
        mov dword ptr [ecx+cad841Time], edx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+cad841Ext]
        mov dword ptr [ecx+cad841Duration], 0x12c
        call dword ptr [cad841Rand]
        and eax, 0x7fff
        mov edx, dword ptr [pm]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        mov eax, dword ptr [edx+cad841Ext]
        fmul dword ptr [cad841K200ac114]
        fsub qword ptr [cad841K200ac128]
        fadd st(0), st(0)
        fmul qword ptr [cad841K200ac128]
        fstp dword ptr [eax+cad841Yaw]
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+cad841Skill]
        cmp dword ptr [ecx + 0x18], 3
        jl cad841_2003a510
        mov edx, dword ptr [eax+cad841Ext]
        mov dword ptr [edx+cad841Pitch], 0x3e800000
        jmp cad841_2003a73d
cad841_2003a510:
        mov eax, dword ptr [eax+cad841Ext]
        mov dword ptr [eax+cad841Pitch], 0x3f000000
        jmp cad841_2003a73d
cad841_2003a51f:
        mov ecx, dword ptr [eax+cad841Ext]
        mov edx, dword ptr [eax+cad841ServerTime]
        mov dword ptr [ecx+cad841Time], edx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+cad841Ext]
        mov dword ptr [ecx+cad841Duration], 0xc8
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        test byte ptr [eax+cad841Pmflags], 1
        jne cad841_2003a5ae
        test dword ptr [eax+cad841Eflags], 0x80000
        jne cad841_2003a5ae
        call dword ptr [cad841Rand]
        and eax, 0x7fff
        mov dword ptr [esp + 0x38], eax
        mov eax, dword ptr [pm]
        fild dword ptr [esp + 0x38]
        mov ecx, dword ptr [eax+cad841Ext]
        fmul dword ptr [cad841K200ac114]
        fsub qword ptr [cad841K200ac128]
        fadd st(0), st(0)
        fmul qword ptr [cad841K200ac170]
        fstp dword ptr [ecx+cad841Yaw]
        call dword ptr [cad841Rand]
        and eax, 0x7fff
        mov edx, dword ptr [pm]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        mov eax, dword ptr [edx+cad841Ext]
        fmul dword ptr [cad841K200ac114]
        fmul dword ptr [cad841K200ac80c]
        fstp dword ptr [eax+cad841Pitch]
        jmp cad841_2003a73d
cad841_2003a5ae:
        call dword ptr [cad841Rand]
        and eax, 0x7fff
        mov ecx, dword ptr [pm]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        mov edx, dword ptr [ecx+cad841Ext]
        fmul dword ptr [cad841K200ac114]
        fsub qword ptr [cad841K200ac128]
        fadd st(0), st(0)
        fmul qword ptr [cad841K200ac128]
        fstp dword ptr [edx+cad841Yaw]
        call dword ptr [cad841Rand]
        and eax, 0x7fff
        mov dword ptr [esp + 0x38], eax
        mov eax, dword ptr [pm]
        fild dword ptr [esp + 0x38]
        mov ecx, dword ptr [eax+cad841Ext]
        fmul dword ptr [cad841K200ac114]
        fmul dword ptr [cad841K200ac808]
        fstp dword ptr [ecx+cad841Pitch]
        jmp cad841_2003a73d
cad841_2003a60e:
        mov edx, dword ptr [eax+cad841Ext]
        mov eax, dword ptr [eax+cad841ServerTime]
        mov dword ptr [edx+cad841Time], eax
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx+cad841Ext]
        mov dword ptr [edx+cad841Duration], ebx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+cad841Ext]
        mov dword ptr [ecx+cad841Yaw], 0
        call dword ptr [cad841Rand]
        and eax, 0x7fff
        mov edx, dword ptr [pm]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        mov eax, dword ptr [edx+cad841Ext]
        fmul dword ptr [cad841K200ac114]
        fmul dword ptr [cad841K200ac808]
        fstp dword ptr [eax+cad841Pitch]
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+cad841Skill]
        cmp dword ptr [ecx + 0x18], 3
        jl cad841_2003a73d
        mov eax, dword ptr [eax+cad841Ext]
        fld dword ptr [eax+cad841Pitch]
        fmul dword ptr [cad841K200ac180]
        fstp dword ptr [eax+cad841Pitch]
        jmp cad841_2003a73d
cad841_2003a685:
        mov edx, dword ptr [eax+cad841Ext]
        mov eax, dword ptr [eax+cad841ServerTime]
        mov esi, 3
        mov dword ptr [edx+cad841Time], eax
        mov eax, dword ptr [pm]
        xor edx, edx
        mov ecx, dword ptr [eax+cad841Skill]
        mov eax, dword ptr [eax+cad841Ext]
        cmp dword ptr [ecx + 0x10], esi
        setl dl
        dec edx
        and edx, 0xffffffe2
        add edx, ebx
        mov dword ptr [eax+cad841Duration], edx
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx+cad841Ext]
        mov dword ptr [edx+cad841Yaw], 0
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+cad841Skill]
        cmp dword ptr [ecx + 0x10], esi
        jl cad841_2003a6fe
        call dword ptr [cad841Rand]
        and eax, 0x7fff
        mov edx, dword ptr [pm]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        mov eax, dword ptr [edx+cad841Ext]
        fmul dword ptr [cad841K200ac114]
        fmul dword ptr [cad841K200ac804]
        fstp dword ptr [eax+cad841Pitch]
        jmp cad841_2003a73d
cad841_2003a6fe:
        call dword ptr [cad841Rand]
        and eax, 0x7fff
        mov edx, dword ptr [pm]
        mov dword ptr [esp + 0x38], eax
        fild dword ptr [esp + 0x38]
        mov eax, dword ptr [edx+cad841Ext]
        fmul dword ptr [cad841K200ac114]
        fmul dword ptr [cad841K200ac808]
        fstp dword ptr [eax+cad841Pitch]
        jmp cad841_2003a73d
cad841_2003a72a:
        xor edx, edx
cad841_2003a72c:
        mov ecx, dword ptr [eax+cad841Ext]
        mov dword ptr [ecx+cad841Time], edx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+cad841Ext]
        mov dword ptr [ecx+cad841Yaw], edx
cad841_2003a73d:
        mov ecx, dword ptr [esp+0x54]
        mov eax, dword ptr [esp+0x20]
        mov dword ptr [ecx], eax
        mov eax, dword ptr [esp+0x14]
        mov dword ptr [ecx+4], eax
        mov eax, dword ptr [esp+0x18]
        mov dword ptr [ecx+8], eax
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x38
        ret
    }
}
#endif

static void PM_Weapon( void ) {
	int			shotSeed;
	int			addTime = 0; // TTimo: init
	int			ammoNeeded;
	qboolean	delayedFire;	//----(SA)  true if the delay time has just expired and this is the frame to send the fire event
	int			aimSpreadScaleAdd;
	int			weapattackanim;
	qboolean	akimboFire;
#ifdef DO_WEAPON_DBG
	static int weaponstate_last = -1;
#endif

    /* Original PM_Weapon prefix3000fc80: dead/spectator/respawn guards,
     * mounted MG42, AA gun and tank are self-contained early-return paths. */
    if ((pm->ps->pm_flags & PMF_RESPAWNED) ||
        pm->ps->persistant[PERS_TEAM] == TEAM_SPECTATOR || pm->ps->stats[STAT_HEALTH] <= 0)
        return;
    {
        int mounted = pm->ps->persistant[PERS_HWEAPON_USE];
        qboolean tank = mounted != 1 && mounted != 2 && (pm->ps->eFlags & EF_MOUNTEDTANK);
        if (mounted == 1 || mounted == 2 || tank) {
            int rate;
            if (mounted != 2 && pm->ps->weapHeat[34]) {
#if defined(_MSC_VER) && defined(_M_IX86)
                PM_WeaponCoolMounted();
#else
                /* Preserve original reciprocal multiply, not SDK division.
                 * The original x87 chain remains extended until conversion. */
                pm->ps->weapHeat[34] = (int)((double)pm->ps->weapHeat[34] -
                    (double)pml.frametime * (double)300.0f);
                if (pm->ps->weapHeat[34] < 0) pm->ps->weapHeat[34] = 0;
                pm->ps->curWeapHeat = (int)floor((double)pm->ps->weapHeat[34] *
                    (double)0.0006666666595265269f * (double)255.0f);
#endif
            }
            if (pm->ps->weaponTime > 0) {
                pm->ps->weaponTime -= pml.msec;
                if (pm->ps->weaponTime > 0) return;
                if (!(pm->cmd.buttons & BUTTON_ATTACK)) {
                    pm->ps->weaponTime = 0;
                    return;
                }
            }
            if (!(pm->cmd.buttons & BUTTON_ATTACK)) return;
            rate = mounted == 2 ? 100 :
                (mounted == 1 && (PM_GameType == 0 || PM_GameType == 1) ? 100 : 66);
            if (mounted != 2) pm->ps->weapHeat[34] += rate;
            PM_AddEvent(mounted == 1 ? EV_FIRE_WEAPON_MG42 :
                (mounted == 2 ? EV_FIRE_WEAPON_AAGUN : EV_FIRE_WEAPON_MOUNTEDMG42));
            pm->ps->weaponTime += rate;
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, ANIM_ET_FIREWEAPON, qfalse, qtrue);
            if (mounted == 1) pm->ps->viewlocked = 2;
            if (mounted != 2 && pm->ps->weapHeat[34] >= 1500) {
                /* Original writes weaponTime, not the heat slot, before the event. */
                pm->ps->weaponTime = 1500;
                PM_AddEvent(EV_WEAP_OVERHEAT);
                pm->ps->weaponTime = 2000;
            }
            return;
        }
    }

	pm->watertype = 0;

	if( BG_IsAkimboWeapon( pm->ps->weapon ) )
		akimboFire = BG_AkimboFireSequence( pm->ps->weapon, pm->ps->ammoclip[BG_FindClipForWeapon(pm->ps->weapon)], pm->ps->ammoclip[BG_FindClipForWeapon(BG_AkimboSidearm(pm->ps->weapon))] );
	else
		akimboFire = qfalse;

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

	// weapon cool down
	PM_CoolWeapons();

    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS && BG_FiremodeWeapon(pm->ps->weapon)) {
        tce_weaponDef_t *def = &weaponDef[pm->ps->weapon];
        if (pm->ps->persistant[10] == 0 && def->fullauto < 1) {
            if (def->burst > 1) pm->ps->persistant[10] = 1;
        } else if (pm->ps->persistant[10] == 1 && def->burst < 2) pm->ps->persistant[10] = 0;
    }


    /* Original3000ff..30010263: tactical sway, queued recoil, recovery. */
    if ((pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) &&
        pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS &&
        pm->ps->weapon != 4 && pm->ps->weapon != 9 && pm->ps->weapon != 30) {
#if defined(_MSC_VER) && defined(_M_IX86)
        const float swayFrequency1 = 0.00125663704238832f;
        const float swayFrequency2 = 0.0031415927223861217f;
        const float swayFrequency3 = 0.0018849557964131236f;
        const float swayFrequency5 = 0.002199114765971899f;
        const double swayPhase2 = 1.36, swayPhase3 = 0.5;
        const double swayPhase4 = 0.89, swayPhase5 = 4.71;
        const float swaySpeedLimit = 30.0f, swayZero = 0.0f;
        const float swayAmplitudeScale = 0.2f, swayHipScale = 0.125f;
        const float swayInstabilityScale = 5.8823530707741156e-05f;
        const float swayAngleUnits = 182.04444885253906f;
        int *swayPhaseInput = &pm->ps->stats[14];
        const float *swayViewInput = pm->ps->viewangles;
        const float *swayVelocity = pm->ps->velocity;
        const float *swayScoped = &weaponDef[pm->ps->weapon].scoped;
        const int *swayMovement = &pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
        const int *swayShot = &pm->ps->stats[STAT_TCE_SHOT_INSTABILITY];
        const int *swayCommands = pm->cmd.angles;
        int *swayDeltas = pm->ps->delta_angles;
        float *swayViewOutput = pm->ps->viewangles;
        int swayFlags = pm->ps->eFlags, swayMsec = pml.msec;
        int swayAmount;
        float swayWave1, swayWave2, swayWave3, swayWave4, swayWave5;
        float swayAmplitude, swayBias, swayStoredInstability;
        vec3_t swayAngles;
        /* Original3000ff7d..300101c8, including retained pitch under the
         * angle conversion calls. No C/libm sine or intermediate double. */
        __asm {
            mov ecx, swayPhaseInput
            fild dword ptr [ecx]
            mov ecx, swayViewInput
            mov eax, dword ptr [ecx]
            mov dword ptr swayAngles, eax
            mov eax, dword ptr [ecx+4]
            mov dword ptr swayAngles[4], eax
            mov eax, dword ptr [ecx+8]
            mov dword ptr swayAngles[8], eax
            fld st(0)
            fmul swayFrequency1
            test swayFlags, 10h
            fld st(0)
            fsin
            mov dword ptr swayAmplitude, 03d8f5c29h
            fstp swayWave1
            fld st(1)
            fmul swayFrequency2
            fadd swayPhase2
            fsin
            fstp swayWave2
            fld st(1)
            fmul swayFrequency3
            fadd swayPhase3
            fsin
            fstp swayWave3
            fadd swayPhase4
            fsin
            fstp swayWave4
            fmul swayFrequency5
            fadd swayPhase5
            fsin
            fstp swayWave5
            fld swayWave1
            fmul swayWave1
            fmul swayWave1
            fstp swayWave1
            jz sway_duck_done
            push swayVelocity
            call VectorLength
            fcomp swaySpeedLimit
            add esp, 4
            fnstsw ax
            test ah, 1
            jz sway_duck_done
            mov dword ptr swayAmplitude, 03d3851ech
        sway_duck_done:
            test swayFlags, 80000h
            jz sway_prone_done
            push swayVelocity
            call VectorLength
            fcomp swaySpeedLimit
            add esp, 4
            fnstsw ax
            test ah, 1
            jz sway_prone_done
            mov dword ptr swayAmplitude, 03ccccccdh
        sway_prone_done:
            mov dword ptr swayBias, 03b83126fh
            fld swayAmplitude
            fmul swayAmplitudeScale
            mov ecx, swayScoped
            fld dword ptr [ecx]
            fcomp swayZero
            fnstsw ax
            test ah, 41h
            jnz sway_hip_instability
            mov ecx, swayShot
            mov eax, dword ptr [ecx]
            mov ecx, swayMovement
            add eax, dword ptr [ecx]
            mov swayAmount, eax
            fild swayAmount
            jmp sway_instability_ready
        sway_hip_instability:
            mov ecx, swayMovement
            fild dword ptr [ecx]
            fxch st(1)
            fmul swayHipScale
            fxch st(1)
            mov dword ptr swayBias, 039807358h
        sway_instability_ready:
            fmul swayInstabilityScale
            fst swayStoredInstability
            fmul swayWave4
            fxch st(1)
            fmul swayWave1
            faddp st(1), st(0)
            fsubr dword ptr swayAngles
            fst dword ptr swayAngles
            fld swayWave5
            fadd swayWave3
            fadd swayWave2
            fadd swayWave1
            fld swayStoredInstability
            fadd swayBias
            fmulp st(1), st(0)
            fadd dword ptr swayAngles[4]
            fstp dword ptr swayAngles[4]
            xor edi, edi
        sway_convert_loop:
            fld dword ptr swayAngles[edi]
            fmul swayAngleUnits
            call PM_WeaponTruncateST0
            mov ecx, swayCommands
            mov edx, dword ptr [ecx+edi]
            and eax, 0ffffh
            sub eax, edx
            mov ecx, swayDeltas
            mov dword ptr [ecx+edi], eax
            add edi, 4
            cmp edi, 12
            jl sway_convert_loop
            mov ecx, swayViewOutput
            fstp dword ptr [ecx]
            mov eax, dword ptr swayAngles[4]
            mov dword ptr [ecx+4], eax
            mov eax, dword ptr swayAngles[8]
            mov dword ptr [ecx+8], eax
            mov ecx, swayPhaseInput
            mov eax, dword ptr [ecx]
            add eax, swayMsec
            mov dword ptr [ecx], eax
            cmp eax, 10000
            jle sway_native_done
            add eax, -10000
            mov dword ptr [ecx], eax
        sway_native_done:
        }
#else
        double phase = pm->ps->stats[14]; /* Original ps+108, distinct from aim phase13. */
        float wave = (float)sin(phase * (double)0.00125663704238832f);
        float wave2 = (float)sin(phase * (double)0.0031415927223861217f + 1.36);
        float wave3 = (float)sin(phase * (double)0.0018849557964131236f + 0.5);
        float wave4 = (float)sin(phase * (double)0.00125663704238832f + 0.89);
        float wave5 = (float)sin(phase * (double)0.002199114765971899f + 4.71);
        float amplitude = 0.07f, bias = 0.004f, instability;
        double scale, speed;
        vec3_t angles;
        int amount, i;
        wave = (float)((double)wave * wave * wave);
        speed = sqrt((double)pm->ps->velocity[0] * pm->ps->velocity[0] +
                     (double)pm->ps->velocity[1] * pm->ps->velocity[1] +
                     (double)pm->ps->velocity[2] * pm->ps->velocity[2]);
        if ((pm->ps->eFlags & EF_CROUCHING) && speed < 30.0f) amplitude = 0.045f;
        if ((pm->ps->eFlags & EF_PRONE) && speed < 30.0f) amplitude = 0.025f;
        scale = (double)amplitude * (double)0.2f;
        amount = pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
        if (weaponDef[pm->ps->weapon].scoped <= 0.0f) {
            scale *= (double)0.125f;
            bias = 0.000245f;
        } else amount += pm->ps->stats[STAT_TCE_SHOT_INSTABILITY];
        instability = (float)((double)amount * (double)5.8823530707741156e-05f);
        VectorCopy(pm->ps->viewangles, angles);
        angles[PITCH] = (float)((double)angles[PITCH] -
            (scale * wave + (double)amount * (double)5.8823530707741156e-05f * wave4));
        angles[YAW] = (float)((double)angles[YAW] + ((double)instability + bias) *
            ((double)wave5 + wave3 + wave2 + wave));
        for (i = 0; i < 3; ++i)
            pm->ps->delta_angles[i] = ((int)((double)angles[i] * (double)182.04444885253906f) & 65535) - pm->cmd.angles[i];
        VectorCopy(angles, pm->ps->viewangles);
        pm->ps->stats[14] += pml.msec;
        if (pm->ps->stats[14] > 10000) pm->ps->stats[14] -= 10000;
#endif
    }
    if (pm->pmext->weapRecoilTime) {
#if defined(_MSC_VER) && defined(_M_IX86)
        int *queuedAngles = pm->ps->delta_angles;
        const float *queuedPitch = &pm->pmext->weapRecoilPitch;
        const float *queuedYaw = &pm->pmext->weapRecoilYaw;
        __asm {
            mov ecx, queuedAngles
            fild dword ptr [ecx]
            mov edx, queuedPitch
            fsub dword ptr [edx]
            call PM_WeaponTruncateST0
            mov ecx, queuedAngles
            mov dword ptr [ecx], eax
            fild dword ptr [ecx+4]
            mov edx, queuedYaw
            fsub dword ptr [edx]
            call PM_WeaponTruncateST0
            mov ecx, queuedAngles
            mov dword ptr [ecx+4], eax
        }
#else
        pm->ps->delta_angles[PITCH] = (int)((double)pm->ps->delta_angles[PITCH] - pm->pmext->weapRecoilPitch);
        pm->ps->delta_angles[YAW] = (int)((double)pm->ps->delta_angles[YAW] - pm->pmext->weapRecoilYaw);
#endif
        pm->pmext->weapRecoilTime = 0;
    }
    PM_TCERecoverHipRecoil();

	delayedFire = qfalse;

    /* Original grenade hold/release, delay and timer block. The timer gate
     * is weaponDef+198, not a fixed SDK fuse; release cannot bypass it. */
    if ((pm->ps->weapon == 4 || pm->ps->weapon == 9 ||
         pm->ps->weapon == 15 || pm->ps->weapon == 30) &&
        pm->ps->grenadeTimeLeft > 0) {
        qboolean forceThrow = qfalse;
#if defined(_MSC_VER) && defined(_M_IX86)
        int *heldGrenadeTime = &pm->ps->grenadeTimeLeft;
        int heldGrenadeWeapon = pm->ps->weapon;
        int heldGrenadeMsec = pml.msec;
        int heldGrenadeGate;
        __asm {
            mov ecx, heldGrenadeTime
            mov eax, dword ptr [ecx]
            cmp heldGrenadeWeapon, 15
            jne held_grenade_countdown
            add eax, heldGrenadeMsec
            mov dword ptr [ecx], eax
            cmp eax, 5000
            jge held_grenade_updated
            mov dword ptr [ecx], 5000
            jmp held_grenade_updated
        held_grenade_countdown:
            sub eax, heldGrenadeMsec
            mov dword ptr [ecx], eax
            cmp eax, 100
            jg held_grenade_updated
            mov forceThrow, 1
            mov dword ptr [ecx], 100
        held_grenade_updated:
        }
#else
        if (pm->ps->weapon == 15) {
            pm->ps->grenadeTimeLeft += pml.msec;
            if (pm->ps->grenadeTimeLeft < 5000) pm->ps->grenadeTimeLeft = 5000;
        } else {
            pm->ps->grenadeTimeLeft -= pml.msec;
            if (pm->ps->grenadeTimeLeft < 101) {
                forceThrow = qtrue;
                pm->ps->grenadeTimeLeft = 100;
            }
        }
#endif
        if ((pm->cmd.buttons & BUTTON_ATTACK) && !forceThrow) return;
#if defined(_MSC_VER) && defined(_M_IX86)
        heldGrenadeGate = weaponDef[pm->ps->weapon].grenadeTimer;
        __asm {
            mov eax, heldGrenadeGate
            sub eax, 250
            mov heldGrenadeGate, eax
        }
        if (pm->ps->grenadeTimeLeft >= heldGrenadeGate) return;
#else
        if (weaponDef[pm->ps->weapon].grenadeTimer - 250 <= pm->ps->grenadeTimeLeft) return;
#endif
        if (pm->ps->weaponDelay == weaponDef[pm->ps->weapon].fireDelayTime || forceThrow) {
            int event;
            if (pm->ps->eFlags & EF_PRONE)
                event = akimboFire ? ANIM_ET_FIREWEAPON2PRONE : ANIM_ET_FIREWEAPONPRONE;
            else
                event = akimboFire ? ANIM_ET_FIREWEAPON2 : ANIM_ET_FIREWEAPON;
            BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, event, qfalse, qtrue);
        }
        if (pm->ps->weaponDelay == 0) delayedFire = qtrue;
    }
    if (pm->ps->weaponDelay > 0) {
#if defined(_MSC_VER) && defined(_M_IX86)
        int *shotDelay = &pm->ps->weaponDelay;
        int shotDelayMsec = pml.msec;
        __asm {
            mov ecx, shotDelay
            mov eax, dword ptr [ecx]
            sub eax, shotDelayMsec
            mov dword ptr [ecx], eax
        }
#else
        pm->ps->weaponDelay -= pml.msec;
#endif
        if (pm->ps->weaponDelay < 1) {
            pm->ps->weaponDelay = 0;
            delayedFire = qtrue;
        }
    }
    if (pm->ps->weaponstate == WEAPON_RELAXING) {
        pm->ps->weaponstate = WEAPON_READY;
        return;
    }
    if ((pm->ps->eFlags & EF_PRONE_MOVING) && !delayedFire) {
        pm->ps->stats[STAT_TCE_FLAGS] |= 8;
        return;
    }
    if (pm->ps->weaponTime > 0) {
#if defined(_MSC_VER) && defined(_M_IX86)
        int *shotWeaponTime = &pm->ps->weaponTime;
        int shotWeaponMsec = pml.msec;
        __asm {
            mov ecx, shotWeaponTime
            mov eax, dword ptr [ecx]
            sub eax, shotWeaponMsec
            mov dword ptr [ecx], eax
        }
#else
        pm->ps->weaponTime -= pml.msec;
#endif
        if (!(pm->cmd.buttons & BUTTON_ATTACK) && pm->ps->weaponTime < 0)
            pm->ps->weaponTime = 0;
    }

    /* Original PM_Weapon3000fc80: bolt/pump post-shot cycling precedes
     * burst accounting and reload control, in both prediction and qagame. */
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        tce_weaponDef_t *def = &weaponDef[pm->ps->weapon];
        if (def->bolt && !PM_WeaponClipEmpty(pm->ps->weapon) &&
            pm->ps->weaponstate == WEAPON_FIRING && pm->ps->weaponDelay <= 0 &&
            !delayedFire && pm->ps->weaponTime <= 0) {
            PM_ContinueWeaponAnim(8);
            PM_WeaponAddTime(weaponDef[pm->ps->weapon].unknown_1a0);
            pm->ps->weaponstate = WEAPON_TCE_CYCLE;
            PM_AddEvent(EV_TCE_RELOAD_BOLT);
        }
        def = &weaponDef[pm->ps->weapon];
        if (def->pump && pm->ps->weaponstate == WEAPON_FIRING &&
            pm->ps->weaponDelay <= 0 && !delayedFire && pm->ps->weaponTime <= 0) {
            if (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x800) {
                pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x800;
            } else if (!pm->ps->persistant[10] || !def->semiauto) {
                PM_ContinueWeaponAnim(3);
                PM_WeaponAddTime(weaponDef[pm->ps->weapon].unknown_19c);
                pm->ps->weaponstate = WEAPON_TCE_CYCLE;
                PM_AddEvent(EV_TCE_RELOAD_PUMP2);
            } else {
                pm->ps->weaponstate = WEAPON_TCE_CYCLE;
                PM_WeaponAddTime(100);
            }
        }
    }

    if (pm->ps->weaponstate == WEAPON_FIRING &&
        pm->ps->weaponDelay <= 0 && !delayedFire && pm->ps->weaponTime <= 0) {
#if defined(_MSC_VER) && defined(_M_IX86)
        int *cycleBurstCount = &pm->ps->holdable[11];
        __asm {
            mov eax, cycleBurstCount
            mov ecx, dword ptr [eax]
            inc ecx
            mov dword ptr [eax], ecx
        }
#else
        pm->ps->holdable[11] = (int)((unsigned int)pm->ps->holdable[11] + 1u);
#endif
    }

	// check for weapon change
	// can't change if weapon is firing, but can change
	// again if lowering or raising

	if( (pm->ps->weaponTime <= 0 || ( !weaponstateFiring && pm->ps->weaponDelay <= 0 )) && !delayedFire) {
		if ( pm->ps->weapon != pm->cmd.weapon && pm->cmd.weapon < 55 && !(pm->ps->pm_flags & 0x400)) {
			PM_BeginWeaponChange( pm->ps->weapon, pm->cmd.weapon, qfalse );	//----(SA)	modified
		}
		/* TC selection HUD uses command 56 to confirm without firing. */
		if (pm->cmd.weapon == 56 && (pm->cmd.buttons & BUTTON_ATTACK)) {
			PM_WeaponAddTime(50);
			return;
		}
	}

    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS && pm->cmd.weapon == 55 && pm->ps->weaponTime <= 0 && !delayedFire &&
        pm->ps->weaponstate == WEAPON_READY && BG_FiremodeWeapon(pm->ps->weapon)) {
        tce_weaponDef_t *def = &weaponDef[pm->ps->weapon];
        if (pm->ps->persistant[10] == 0) pm->ps->persistant[10] = def->burst < 2 ? 2 : 1;
        else if (pm->ps->persistant[10] == 1) pm->ps->persistant[10] = 2;
        else if (def->fullauto || def->pump) pm->ps->persistant[10] = 0;
        else if (def->burst > 1) pm->ps->persistant[10] = 1;
        PM_WeaponAddTime(750);
        PM_AddEvent(EV_TCE_FIREMODE);
        return;
    }

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        if (PM_TCEWeaponActions834(delayedFire)) return;
    } else
#endif
    {
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS && PM_TCEAimInput(delayedFire)) return;

    /* Original30010a..30010b: ladder blocks all shots; sprint action latch
     * precedes objective handling and permits only pending grenade release. */
    if (pml.ladder) {
        if (pm->ps->weaponTime < 501) pm->ps->weaponTime = 500;
        pm->ps->stats[STAT_TCE_FLAGS] |= 8;
        return;
    }
    if (!(pm->cmd.buttons & BUTTON_SPRINT) || (!pm->cmd.forwardmove && !pm->cmd.rightmove) ||
        (pm->ps->pm_flags & PMF_DUCKED) || (pm->ps->eFlags & EF_PRONE)) {
        if (pm->ps->weaponTime <= 0 && (pm->ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x4000)) {
            pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x4000;
            pm->ps->weaponTime = 250;
        }
    } else {
        pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x4000;
        if ((pm->ps->weapon == 4 || pm->ps->weapon == 9 || pm->ps->weapon == 30) && delayedFire)
            pm->ps->weaponTime = 0;
        else if (pm->ps->weaponstate == WEAPON_READY) {
            if (pm->ps->weaponTime > 250) return;
            pm->ps->weaponTime = 250;
            return;
        }
    }
    PM_TCEObjectiveAction();

	if( pm->ps->weaponDelay > 0 ) {
		return;
	}

	// check for clip change
	PM_CheckForReload( pm->ps->weapon );

	if( pm->ps->weaponTime > 0 || pm->ps->weaponDelay > 0 ) {
		return;
	}

    }

    /* Original change -> trigger latch -> reload completion ordering. */
    if (pm->ps->weaponstate == WEAPON_DROPPING || pm->ps->weaponstate == WEAPON_DROPPING_TORELOAD) {
        PM_FinishWeaponChange();
        pm->ps->pm_flags &= ~0x400;
        return;
    }
#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        if (PM_TCEWeaponTrigger835(delayedFire)) return;
    } else
#endif
    {
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        /* Original short-circuit lookups: do not eagerly cache mode/clip. */
        if (!(pm->cmd.buttons & BUTTON_ATTACK) && (pm->ps->pm_flags & 0x400) && !delayedFire &&
            BG_FiremodeWeapon(pm->ps->weapon) && pm->ps->persistant[10] == 1 &&
            pm->ps->holdable[11] < weaponDef[pm->ps->weapon].burst &&
            pm->ps->ammoclip[BG_FindClipForWeapon(pm->ps->weapon)] &&
            !(pm->ps->stats[STAT_TCE_FLAGS] & 8) && weaponstateFiring)
            pm->cmd.buttons |= BUTTON_ATTACK;
        if ((pm->cmd.buttons & BUTTON_ATTACK) || delayedFire) {
            if (!(pm->ps->pm_flags & 0x400) || delayedFire) pm->ps->pm_flags |= 0x400;
            else {
                if (pm->ps->weapon != 1 && !(pm->ps->stats[STAT_TCE_FLAGS] & 0x800)) {
                    qboolean triggerStop = qfalse;
                    if (!BG_FiremodeWeapon(pm->ps->weapon)) {
                        tce_weaponDef_t *triggerDef = &weaponDef[pm->ps->weapon];
                        if (triggerDef->semiauto || triggerDef->pump || triggerDef->bolt)
                            triggerStop = qtrue;
                    }
                    if (!triggerStop && !pm->ps->ammoclip[BG_FindClipForWeapon(pm->ps->weapon)] &&
                        pm->ps->weapon != 1 && pm->ps->weapon != 4 && pm->ps->weapon != 30 && pm->ps->weapon != 9)
                        triggerStop = qtrue;
                    if (!triggerStop && BG_FiremodeWeapon(pm->ps->weapon)) {
                        if (pm->ps->persistant[10] == 2) triggerStop = qtrue;
                        else {
                            if (pm->ps->persistant[10] == 1) {
                                int triggerBurstLimit = weaponDef[pm->ps->weapon].burst;
#if defined(_MSC_VER) && defined(_M_IX86)
                                __asm {
                                    mov eax, triggerBurstLimit
                                    dec eax
                                    mov triggerBurstLimit, eax
                                }
#else
                                triggerBurstLimit = (int)((unsigned int)triggerBurstLimit - 1u);
#endif
                                if (pm->ps->holdable[11] > triggerBurstLimit) triggerStop = qtrue;
                            }
                            if (!triggerStop && weaponDef[pm->ps->weapon].singleReload) triggerStop = qtrue;
                        }
                    }
                    if (triggerStop) {
                        pm->ps->weaponTime = 0;
                        pm->ps->weaponDelay = 0;
                        if (weaponstateFiring) {
                            if (!pm->ps->ammoclip[BG_FindClipForWeapon(pm->ps->weapon)]) PM_StartWeaponAnim(1);
                            else PM_ContinueWeaponAnim(0);
                        }
                        pm->ps->weaponstate = WEAPON_READY;
                        return;
                    }
                }
                if (!(pm->cmd.buttons & BUTTON_ATTACK)) pm->ps->pm_flags &= ~0x400;
            }
        } else pm->ps->pm_flags &= ~0x400;
    }
    if (pm->ps->weaponstate == WEAPON_RELOADING ||
        pm->ps->weaponstate == WEAPON_TCE_RELOAD_END || pm->ps->weaponstate == WEAPON_TCE_CYCLE)
        PM_FinishWeaponReload();
    }

	if( pm->ps->weaponstate == WEAPON_RAISING ) {
		pm->ps->weaponstate = WEAPON_READY;

//		if( pm->ps->eFlags & EF_PRONE && pm->ps->weapon == WP_MOBILE_MG42 )
//			pm->pmext->proneMG42Zoomed = qtrue;

		PM_StartWeaponAnim(TCE_PM_IdleAnimForWeapon(pm->ps->weapon));
		return;
	} else if( pm->ps->weaponstate == WEAPON_RAISING_TORELOAD ) {
		pm->ps->weaponstate = WEAPON_READY;

		PM_BeginWeaponReload( pm->ps->weapon );

		return;
	}


#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
    if (PM_TCEWeaponEligibility837(delayedFire)) return;
#else
	if(pm->ps->weapon == WP_NONE)	// this is possible since the player starts with nothing
		return;


    /* Original charge eligibility,300110..300113. TC slot IDs are not
     * SDK enum aliases: e.g. Glock39/40 must never enter riflegrenade gates. */
    {
        int w = pm->ps->weapon;
        int elapsed;
#if defined(_MSC_VER) && defined(_M_IX86)
        int chargeNow = pm->cmd.serverTime, chargeStarted = pm->ps->classWeaponTime;
        __asm {
            mov eax, chargeNow
            sub eax, chargeStarted
            mov elapsed, eax
        }
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

#endif

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        PM_TCEAttackStart838(delayedFire, akimboFire);
    } else
#endif
    {
    /* Original300114..30011868 attack-start dispatch. TC ID groups and
     * zero-delay immediate animation are independent of parser flags. */
    {
        int w = pm->ps->weapon;
        int fireDelay = 0;
        int fireAnim = (pm->ps->eFlags & EF_PRONE) ? ANIM_ET_FIREWEAPONPRONE : ANIM_ET_FIREWEAPON;
        /* Grenade delay is read at its original post-animation point. */
        if (w != 4 && w != 9 && w != 15 && w != 30)
            fireDelay = w >= 0 && w < TCE_WEAPON_CAPACITY
                ? weaponDef[w].fireDelayTime : GetAmmoTableData(w)->fireDelayTime;
        switch (w) {
        case 1:
            if (!delayedFire)
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, fireAnim, qfalse, qfalse);
            break;
        case 2: case 5: case 6: case 7: case 13: case 14: case 23: case 24:
        case 25: case 32: case 37: case 38: case 39: case 40: case 46: case 47:
        case 51: case 52: case 53: case 54: case 55: case 56: case 57: case 58:
            if (weaponstateFiring || !fireDelay) {
                if (akimboFire)
                    fireAnim = (pm->ps->eFlags & EF_PRONE) ? ANIM_ET_FIREWEAPON2PRONE : ANIM_ET_FIREWEAPON2;
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, fireAnim, qfalse, qtrue);
            } else pm->ps->weaponDelay = fireDelay;
            break;
        case 3: case 8: case 10: case 19: case 21: case 22: case 31: case 33:
        case 36: case 41: case 42: case 43: case 44: case 45: case 59: case 62:
            if (weaponstateFiring || !fireDelay)
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, fireAnim, qtrue, qtrue);
            else pm->ps->weaponDelay = fireDelay;
            break;
        case 4: case 9: case 15: case 30:
            if (!delayedFire) {
                if (PM_WeaponAmmoAvailable(w)) {
                    if (pm->ps->weapon == 15) pm->ps->grenadeTimeLeft = 50;
                    else {
#if defined(_MSC_VER) && defined(_M_IX86)
                        int primeInterval = weaponDef[pm->ps->weapon].grenadeTimer;
                        int *primeTimer = &pm->ps->grenadeTimeLeft;
                        __asm {
                            mov eax, primeInterval
                            add eax, 500
                            mov ecx, primeTimer
                            mov dword ptr [ecx], eax
                        }
#else
                        pm->ps->grenadeTimeLeft = (int)((unsigned int)weaponDef[pm->ps->weapon].grenadeTimer + 500u);
#endif
                        PM_AddEvent(EV_TCE_GRENADE_PRIME);
                    }
                    PM_StartWeaponAnim(TCE_PM_AttackAnimForWeapon(pm->ps->weapon));
                }
                /* Original reloads this field after prime/event/animation. */
                pm->ps->weaponDelay = weaponDef[pm->ps->weapon].fireDelayTime;
            }
            break;
        case 26:
            if (!delayedFire) {
                if (PM_WeaponAmmoAvailable(w))
                    BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo,
                        (pm->ps->eFlags & EF_PRONE) ? ANIM_ET_FIREWEAPON2PRONE : ANIM_ET_FIREWEAPON,
                        qfalse, qtrue);
                pm->ps->weaponDelay = 50;
            }
            break;
        case 27: case 29:
            if (!delayedFire) {
                if (PM_WeaponAmmoAvailable(w)) PM_StartWeaponAnim(TCE_PM_AttackAnimForWeapon(w));
                pm->ps->weaponDelay = w == 27 ? 50 : 100;
            }
            break;
        case 28:
            if (!weaponstateFiring) {
                PM_AddEvent(EV_SPINUP);
                pm->ps->weaponDelay = 722;
                PM_ContinueWeaponAnim(TCE_PM_AttackAnimForWeapon(w));
            } else BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, fireAnim, qfalse, qtrue);
            break;
        case 60:
            if (!weaponstateFiring) {
                PM_AddEvent(EV_SPINUP);
                PM_StartWeaponAnim(TCE_PM_AttackAnimForWeapon(w));
                pm->ps->weaponDelay = 0;
            } else BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, fireAnim, qfalse, qtrue);
            break;
        default:
            if (weaponstateFiring || !fireDelay)
                BG_AnimScriptEvent(pm->ps, pm->character->animModelInfo, fireAnim, qfalse, qtrue);
            else pm->ps->weaponDelay = fireDelay;
            break;
        }
    }

	pm->ps->weaponstate = WEAPON_FIRING;
    }

	// Gordon: reset player disguise on firing
//	if( pm->ps->weapon != WP_SMOKE_BOMB && pm->ps->weapon != WP_SATCHEL && pm->ps->weapon != WP_SATCHEL_DET ) {	// Arnout: not for these weapons
//		pm->ps->powerups[PW_OPS_DISGUISED] = 0;
//	}

#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        if (PM_TCEAmmoConsume839()) return;
    } else
#endif
    {
    /* Original30011868..30011a4d: TC uses/clip eligibility. The old SDK
     * autoreload macro aliases several TC pistols and scoped weapons. */
    ammoNeeded = pm->ps->weapon >= 0 && pm->ps->weapon < TCE_WEAPON_CAPACITY
        ? weaponDef[pm->ps->weapon].uses : GetAmmoTableData(pm->ps->weapon)->uses;
    if (pm->ps->weapon && ammoNeeded > PM_WeaponAmmoAvailable(pm->ps->weapon)) {
        qboolean reloading = ammoNeeded <= pm->ps->ammo[BG_FindAmmoForWeapon(pm->ps->weapon)];
        qboolean autoReloadWeapon = qfalse;
        qboolean playSound = qtrue;
        switch (pm->ps->weapon) {
        case 2: case 3: case 5: case 6: case 7: case 8: case 10: case 13: case 14:
        case 23: case 24: case 25: case 31: case 32: case 33: case 37: case 39:
        case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
        case 48: case 49: case 50: case 51: case 52: case 57: case 58: case 59:
            autoReloadWeapon = qtrue;
            break;
        default:
            break;
        }
        if (!pm->pmext->bAutoReload && autoReloadWeapon && !(pm->cmd.wbuttons & WBUTTON_RELOAD))
            reloading = qfalse;
        switch (pm->ps->weapon) {
        case 4: case 9: case 15: case 26: case 29: case 30:
            playSound = qfalse;
            break;
        case 57: case 58: case 59:
            reloading = qfalse;
            break;
        default:
            break;
        }
        if (playSound) PM_AddEvent(reloading ? EV_EMPTYCLIP : EV_NOAMMO);
        PM_ContinueWeaponAnim(reloading ? WEAP_IDLE2 : TCE_PM_IdleAnimForWeapon(pm->ps->weapon));
        PM_WeaponAddTime(500);
        if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_WEAPON_CAPACITY &&
            weaponDef[pm->ps->weapon].singleReload)
            pm->ps->stats[STAT_TCE_WEAPON_FLAGS] |= 0x800;
        return;
    }

	if(pm->ps->weaponDelay > 0)
		// if it hits here, the 'fire' has just been hit and the weapon dictated a delay.
		// animations have been started, weaponstate has been set, but no weapon events yet. (except possibly EV_NOAMMO)
		// checks for delayed weapons that have already been fired are return'ed above.
		return;

    /* Original TC slick-surface kick uses31/65/66, not SDK5/6 aliases. */
    if (!(pm->ps->eFlags & EF_PRONE) && (pml.groundTrace.surfaceFlags & SURF_SLICK)) {
        float strength = pm->ps->weapon == 31 ? 400.0f :
            (pm->ps->weapon == 65 ? 32000.0f : (pm->ps->weapon == 66 ? 2000.0f : 0.0f));
        if (strength > 0.0f) {
#if defined(_MSC_VER) && defined(_M_IX86)
            const float slickFactor = 0.005f, slickReverse = -1.0f, slickZero = 0.0f;
            const float *slickForward = pml.forward;
            float *slickVelocity = pm->ps->velocity;
            float slickScale, slickY, slickZ;
            __asm {
                mov ecx, slickVelocity
                mov edx, slickForward
                fld strength
                fld dword ptr [edx+8]
                fmul dword ptr [ecx+8]
                fld dword ptr [edx+4]
                fmul dword ptr [ecx+4]
                faddp st(1), st(0)
                fld dword ptr [edx]
                fmul dword ptr [ecx]
                faddp st(1), st(0)
                fcomp slickZero
                fmul slickFactor
                fnstsw ax
                fmul slickReverse
                test ah, 41h
                fst slickScale
                fmul dword ptr [edx]
                fld slickScale
                fmul dword ptr [edx+4]
                fstp slickY
                fld slickScale
                fmul dword ptr [edx+8]
                fstp slickZ
                fadd dword ptr [ecx]
                fstp dword ptr [ecx]
                fld slickY
                fadd dword ptr [ecx+4]
                fstp dword ptr [ecx+4]
                fld slickZ
                fadd dword ptr [ecx+8]
                fstp dword ptr [ecx+8]
            }
#else
            double scale = (double)strength * (double)0.005f * -1.0;
            int i;
            for (i = 0; i < 3; ++i)
                pm->ps->velocity[i] = (float)((double)pm->ps->velocity[i] + scale * pml.forward[i]);
#endif
            if (!pm->ps->pm_time) {
                pm->ps->pm_time = 100;
                pm->ps->pm_flags |= PMF_TIME_KNOCKBACK;
            }
        }
    }

	// take an ammo away if not infinite
	if(PM_WeaponAmmoAvailable(pm->ps->weapon) != -1 ) {
		// Rafael - check for being mounted on mg42
		if (!(pm->ps->persistant[PERS_HWEAPON_USE]) && !(pm->ps->eFlags & EF_MOUNTEDTANK)) {
			PM_WeaponUseAmmo(pm->ps->weapon, ammoNeeded);
		}
	}
    }



#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        weapattackanim = PM_TCEFireEvents840(akimboFire);
    } else
#endif
    {
    /* PM_Weapon3000fc80, original30011b..30011e: attack animation and
     * shot events use TC IDs, independently of the selected gear parser. */
    if (BG_IsAkimboWeapon(pm->ps->weapon)) {
        weapattackanim = akimboFire ? WEAP_ATTACK1 : WEAP_ATTACK2;
    } else {
        weapattackanim = PM_WeaponClipEmpty(pm->ps->weapon)
            ? TCE_PM_LastAttackAnimForWeapon(pm->ps->weapon)
            : TCE_PM_AttackAnimForWeapon(pm->ps->weapon);
    }
    switch (pm->ps->weapon) {
    case 3: case 8: case 10: case 19: case 21: case 22: case 28:
    case 31: case 36: case 41: case 42: case 43: case 44: case 45: case 62:
        PM_ContinueWeaponAnim(weapattackanim);
        break;
    case 4: case 9: case 30:
        PM_StartWeaponAnim(WEAP_ATTACK2);
        break;
    case 60:
        break;
    default:
        PM_StartWeaponAnim(weapattackanim);
        break;
    }

    if (pm->ps->weapon == 65 || pm->ps->weapon == 22 ||
        pm->ps->weapon == 26 || pm->ps->weapon == 27) {
        PM_AddEvent(EV_NOAMMO);
    }
    if (pm->ps->weapon == 27) {
        pm->ps->ammoclip[28] = 1;
        pm->ps->ammo[27] = 0;
        pm->ps->ammoclip[27] = 0;
        PM_BeginWeaponChange(27, 28, qfalse);
    }
    if ((pm->ps->weapon == 55 || pm->ps->weapon == 56) &&
        !pm->ps->ammo[BG_FindAmmoForWeapon(pm->ps->weapon)]) {
        PM_AddEvent(EV_NOAMMO);
    }
    if (pm->ps->weapon == 60 && !pm->ps->ammo[35]) {
        PM_AddEvent(EV_NOAMMO);
    }

    if (BG_IsAkimboWeapon(pm->ps->weapon)) {
        PM_AddEvent(akimboFire ? EV_FIRE_WEAPON : EV_FIRE_WEAPONB);
    } else {
        /* Original pump/firemode eventParm=1 is consumed by the TC client.
         * Keep the existing 64-slot data boundary explicit for residual65/66. */
        if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_WEAPON_CAPACITY &&
            weaponDef[pm->ps->weapon].pump && pm->ps->persistant[10] > 0 &&
            BG_FiremodeWeapon(pm->ps->weapon)) {
            int fireEvent = PM_WeaponClipEmpty(pm->ps->weapon)
                ? EV_FIRE_WEAPON_LASTSHOT : EV_FIRE_WEAPON;
            PM_AddEventExt(fireEvent, 1);
        } else {
            int fireEvent = PM_WeaponClipEmpty(pm->ps->weapon)
                ? EV_FIRE_WEAPON_LASTSHOT : EV_FIRE_WEAPON;
            PM_AddEvent(fireEvent);
        }
    }

	// RF
// rain - moved releasedFire into pmext instead of ps
	pm->pmext->releasedFire = qfalse;
	pm->ps->lastFireTime = pm->cmd.serverTime;
    }
#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        int cadenceResult841[3];
        PM_TCECadenceRecoil841(akimboFire, weapattackanim, cadenceResult841);
        shotSeed = cadenceResult841[0];
        addTime = cadenceResult841[1];
        aimSpreadScaleAdd = cadenceResult841[2];
    } else
#endif
    {
    /* Original30011e1b: the masked local seed is retained across cadence
     * and passed to recoil, independently of later player-state reads. */
    shotSeed = pm->ps->stats[STAT_TCE_SHOT_SEED];
    Q_rand(&shotSeed);
    shotSeed &= 65535;
    pm->ps->stats[STAT_TCE_SHOT_SEED] = shotSeed;

    /* Original30011e5a..30012130 cadence/spread switch for supported0..63.
     * TC IDs deliberately do not use the SDK weapon enum aliases. */
    aimSpreadScaleAdd = 0;
    switch (pm->ps->weapon) {
    case 1: case 4: case 9: case 15: case 26: case 29: case 30:
    case 55: case 56: case 60:
        addTime = weaponDef[pm->ps->weapon].nextShotTime; break;
    case 2: case 5: case 6: case 13: case 14: case 23: case 24: case 25:
    case 32: case 33: case 39: case 40: case 41: case 42: case 43: case 44:
    case 45: case 46: case 47: case 48: case 49: case 50: case 51:
        if (weaponDef[pm->ps->weapon].pump) pm->ps->stats[STAT_TCE_WEAPON_FLAGS] &= ~0x800;
        aimSpreadScaleAdd = 35;
        addTime = weaponDef[pm->ps->weapon].nextShotTime; break;
    case 3: case 8: case 10:
        addTime = weaponDef[pm->ps->weapon].nextShotTime;
        aimSpreadScaleAdd = 15 + rand()%10; break;
    case 7: case 52:
        aimSpreadScaleAdd = 20;
        addTime = weaponDef[pm->ps->weapon].nextShotTime; break;
    /* Original legacy cadence column300968cc; proven read-only table. */
    case 11: case 12: addTime = 1000; break;
    case 36: addTime = 1600; break;
    case 61: addTime = 0; break;
    case 19: case 22: addTime = 1000; break;
    case 21: addTime = 50; break;
    case 31: case 62:
        addTime = weapattackanim == WEAP_ATTACK_LASTSHOT ? 2000 : (pm->ps->weapon == 31 ? 50 : 0);
        aimSpreadScaleAdd = 20; break;
    case 37: case 53: case 38: case 54: {
        qboolean cadenceDouble = qfalse;
        addTime = weaponDef[pm->ps->weapon].nextShotTime;
        if (!pm->ps->ammoclip[BG_FindClipForWeapon(pm->ps->weapon)]) {
            if (!akimboFire) cadenceDouble = qtrue;
        } else if (!pm->ps->ammoclip[BG_FindClipForWeapon(BG_AkimboSidearm(pm->ps->weapon))]) {
            if (akimboFire) cadenceDouble = qtrue;
        }
        if (cadenceDouble) {
            /* Original reload after clip/sidearm calls, then SHL32. */
            addTime = weaponDef[pm->ps->weapon].nextShotTime;
#if defined(_MSC_VER) && defined(_M_IX86)
            __asm {
                mov eax, addTime
                shl eax, 1
                mov addTime, eax
            }
#else
            addTime = (int)((unsigned int)addTime << 1);
#endif
        }
        aimSpreadScaleAdd = (pm->ps->weapon == 37 || pm->ps->weapon == 53) ? 20 : 35;
        break;
    }
    case 57: case 58:
        aimSpreadScaleAdd = 200; addTime = weaponDef[pm->ps->weapon].nextShotTime; break;
    case 59:
        aimSpreadScaleAdd = 100; addTime = weaponDef[pm->ps->weapon].nextShotTime; break;
    default: break;
    }

	// set weapon recoil
	pm->pmext->lastRecoilDeltaTime = 0;

	switch( pm->ps->weapon ) {
	case 57:
	case 58:
		pm->pmext->weapRecoilTime = pm->cmd.serverTime;
		pm->pmext->weapRecoilDuration = 300;
		PM_WeaponRandomYaw(&pm->pmext->weapRecoilYaw, 0.5);

		if( pm->skill[SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS] >= 3 ) {
			pm->pmext->weapRecoilPitch = .25f;
		} else {
			pm->pmext->weapRecoilPitch = .5f;
		}
		break;
	case 31:
		pm->pmext->weapRecoilTime = pm->cmd.serverTime;
		pm->pmext->weapRecoilDuration = 200;
		if( pm->ps->pm_flags & PMF_DUCKED || pm->ps->eFlags & EF_PRONE ) {
			PM_WeaponRandomYaw(&pm->pmext->weapRecoilYaw, 0.5);
			PM_WeaponRandomPitch(&pm->pmext->weapRecoilPitch, 0.06750000268220901f);
		} else {
			PM_WeaponRandomYaw(&pm->pmext->weapRecoilYaw, 0.25);
			PM_WeaponRandomPitch(&pm->pmext->weapRecoilPitch, 0.15000000596046448f);
		}
		break;
	/*case WP_MOBILE_MG42_SET:
		pm->pmext->weapRecoilTime = 0;
		pm->pmext->weapRecoilYaw = 0.f;
		break;*/
	case 59:
		pm->pmext->weapRecoilTime = pm->cmd.serverTime;
		pm->pmext->weapRecoilDuration = 100;
		pm->pmext->weapRecoilYaw = 0.f;
		PM_WeaponRandomPitch(&pm->pmext->weapRecoilPitch, 0.06750000268220901f);

		if( pm->skill[SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS] >= 3 ) {
			pm->pmext->weapRecoilPitch *= .5f;
		}
		break;
	case 2:
	case 14:
	case 38:
	case 54:
	case 7:
	case 39:
	case 40:
	case 52:
	case 37:
	case 53:
		pm->pmext->weapRecoilTime = pm->cmd.serverTime;
		pm->pmext->weapRecoilDuration = pm->skill[SK_LIGHT_WEAPONS] >= 3 ? 70 : 100;
		pm->pmext->weapRecoilYaw = 0.f;//crandom() * .1f;
		PM_WeaponRandomPitch(&pm->pmext->weapRecoilPitch,
            pm->skill[SK_LIGHT_WEAPONS] >= 3 ? 0.03750000149011612f : 0.06750000268220901f);
		break;
	default:
		pm->pmext->weapRecoilTime = 0;
		pm->pmext->weapRecoilYaw = 0.f;
		break;
	}

    }
#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
    if (pm->ps->weapon < 0 || pm->ps->weapon >= TCE_MAX_WEAPONS)
#endif
    {
    /* Original300124xx: multiplier30092538 is the double constant3.0.
     * No SDK overheat tail or covert-ops spread-halving exists here. */
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        static const double shotSpreadMultiplier = 3.0;
        static const float shotSpreadLimit = 255.0f;
        float *shotSpreadFloat = &pm->ps->aimSpreadScaleFloat;
        int *shotSpreadInteger = &pm->ps->aimSpreadScale;
        unsigned short shotSpreadSavedCW, shotSpreadTruncateCW;
        __int64 shotSpreadConverted;
        __asm {
            fild aimSpreadScaleAdd
            fmul shotSpreadMultiplier
            mov ecx, shotSpreadFloat
            fadd dword ptr [ecx]
            fstp dword ptr [ecx]
            fld dword ptr [ecx]
            fcomp shotSpreadLimit
            fnstsw ax
            test ah, 41h
            jnz shot_spread_unclamped
            mov dword ptr [ecx], 437f0000h
shot_spread_unclamped:
            fld dword ptr [ecx]
            fwait
            fnstcw shotSpreadSavedCW
            fwait
            mov ax, shotSpreadSavedCW
            or ah, 0ch
            mov shotSpreadTruncateCW, ax
            fldcw shotSpreadTruncateCW
            fistp qword ptr shotSpreadConverted
            fldcw shotSpreadSavedCW
            mov eax, dword ptr shotSpreadConverted
            mov ecx, shotSpreadInteger
            mov dword ptr [ecx], eax
        }
    }
#else
    pm->ps->aimSpreadScaleFloat = (float)((double)pm->ps->aimSpreadScaleFloat + 3.0 * aimSpreadScaleAdd);
    if (pm->ps->aimSpreadScaleFloat > 255.0f) pm->ps->aimSpreadScaleFloat = 255.0f;
    pm->ps->aimSpreadScale = (int)pm->ps->aimSpreadScaleFloat;
#endif

    }
    /* Original complete common tail applies to every supported weapon,
     * with tactical definition118 versus hip114 recoil and shared snapshots. */
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
#if !(defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL))
        pm->ps->holdable[0] = pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
        pm->ps->holdable[1] = pm->ps->stats[STAT_TCE_SHOT_INSTABILITY];
#endif
        PM_TCEShotRecoil(addTime, (unsigned int)shotSeed, aimSpreadScaleAdd);
    }
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        int *cadenceTimer = &pm->ps->weaponTime;
        __asm {
            mov eax, cadenceTimer
            mov ecx, addTime
            add dword ptr [eax], ecx
        }
    }
#else
    pm->ps->weaponTime = (int)((unsigned int)pm->ps->weaponTime + (unsigned int)addTime);
#endif

	PM_SwitchIfEmpty();
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

static void PM_TCEUpdateLean(playerState_t *ps, usercmd_t *cmd, pmove_t *tpm) {
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
            if(target<lean)lean=(float)(lean-(double)pml.msec*(double).005f*28.0);
            if(lean<target)lean=target;
        } else {
            if(lean<target)lean=(float)(lean+(double)pml.msec*(double).005f*28.0);
            if(lean>target)lean=target;
        }
    }
    ps->leanf=lean;
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void PM_UpdateLean(playerState_t *ps, usercmd_t *cmd, pmove_t *tpm) {
    __asm {

        MOV ECX,dword ptr [ESP + 08h]

        SUB ESP,0a0h

        MOV AL,byte ptr [ECX + postureCmdButtons]

        PUSH ESI

        PUSH EDI

        XOR EDI,EDI

        TEST AL,030h

        JZ postureAt20032368

        MOV DL,byte ptr [ECX + postureCmdUp]

        TEST DL,DL

        JG postureAt20032368

        TEST AL,010h

        JZ postureAt20032363

        OR EDI,0ffffffffh
    postureAt20032363:
        TEST AL,020h

        JZ postureAt20032368

        INC EDI
    postureAt20032368:
        MOV ESI,dword ptr [ESP + 0ach]

        MOV EAX,dword ptr [ESI + posturePs68]

        TEST AL,020h

        JNZ postureAt2003237d

        TEST EAX,0408000h

        JZ postureAt2003237f
    postureAt2003237d:
        XOR EDI,EDI
    postureAt2003237f:
        MOV ECX,dword ptr [ESI + posturePsf0]

        TEST CH,040h

        JZ postureAt2003238c

        XOR EDI,EDI
    postureAt2003238c:
        MOV ECX,dword ptr [pml + posturePmlLadder]

        TEST ECX,ECX

        JZ postureAt20032398

        XOR EDI,EDI
    postureAt20032398:
        CMP dword ptr [ESI + posturePsd0],01h

        JGE postureAt200323a3

        XOR EDI,EDI
    postureAt200323a3:
        CMP dword ptr [ESI + posturePsa8],07h

        JNZ postureAt200323b7

        CMP dword ptr [ESI + posturePsa4],0fh

        JNZ postureAt200323b7

        XOR EDI,EDI
    postureAt200323b7:
        TEST EAX,080000h

        JNZ postureAt200323c7

        CMP dword ptr [ESI + posturePsa4],03ch

        JNZ postureAt200323c9
    postureAt200323c7:
        XOR EDI,EDI
    postureAt200323c9:
        FLD dword ptr [ESI + posturePs3c]

        TEST EDI,EDI

        FST dword ptr [ESP + 024h]

        JNZ postureAt20032468

        FCOM dword ptr [postureConstant200ac100]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt2003241e

        FILD dword ptr [pml + posturePmlMsec]

        FMUL dword ptr [postureConstant200ac718]

        FMUL dword ptr [postureConstant200ac714]

        FSUBP ST(1),ST(0)

        FCOM dword ptr [postureConstant200ac100]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032704

        FSTP ST(0)

        FLD dword ptr [postureConstant200ac100]

        POP EDI

        FSTP dword ptr [ESI + posturePs3c]

        POP ESI

        ADD ESP,0a0h

        RET
    postureAt2003241e:
        FCOM dword ptr [postureConstant200ac100]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032704

        FILD dword ptr [pml + posturePmlMsec]

        FMUL dword ptr [postureConstant200ac718]

        FMUL dword ptr [postureConstant200ac714]

        FADDP ST(1),ST(0)

        FCOM dword ptr [postureConstant200ac100]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032704

        FSTP ST(0)

        FLD dword ptr [postureConstant200ac100]

        POP EDI

        FSTP dword ptr [ESI + posturePs3c]

        POP ESI

        ADD ESP,0a0h

        RET
    postureAt20032468:
        MOV EAX,dword ptr [ESI + posturePs14]

        MOV ECX,dword ptr [ESI + posturePs18]

        FSTP ST(0)

        FILD dword ptr [ESI + posturePsbc]

        MOV EDX,dword ptr [ESI + posturePsb0]

        MOV dword ptr [ESP + 018h],EAX

        MOV EAX,dword ptr [ESI + posturePsb4]

        MOV dword ptr [ESP + 01ch],ECX

        FADD dword ptr [ESI + posturePs1c]

        MOV ECX,dword ptr [ESI + posturePsb8]

        MOV dword ptr [ESP + 028h],EDX

        TEST EDI,EDI

        MOV dword ptr [ESP + 02ch],EAX

        MOV dword ptr [ESP + 030h],ECX

        FSTP dword ptr [ESP + 020h]

        JLE postureAt20032505

        FLD dword ptr [ESP + 030h]

        FADD dword ptr [postureConstant200ac710]

        LEA EDX,[ESP + 064h]

        LEA EAX,[ESP + 034h]

        PUSH EDX

        LEA ECX,[ESP + 050h]

        PUSH EAX

        LEA EDX,[ESP + 030h]

        FSTP dword ptr [ESP + 038h]

        PUSH ECX

        PUSH EDX

        CALL AngleVectors

        FLD dword ptr [ESP + 044h]

        FMUL dword ptr [postureConstant200ac714]

        ADD ESP,010h

        FADD dword ptr [ESP + 018h]

        FSTP dword ptr [ESP + 0ch]

        FLD dword ptr [ESP + 038h]

        FMUL dword ptr [postureConstant200ac714]

        FADD dword ptr [ESP + 01ch]

        FSTP dword ptr [ESP + 010h]

        FLD dword ptr [ESP + 03ch]

        FMUL dword ptr [postureConstant200ac714]

        FADD dword ptr [ESP + 020h]

        JMP postureAt20032569
    postureAt20032505:
        JGE postureAt20032565

        FLD dword ptr [ESP + 030h]

        FSUB dword ptr [postureConstant200ac710]

        LEA EAX,[ESP + 064h]

        LEA ECX,[ESP + 034h]

        PUSH EAX

        LEA EDX,[ESP + 050h]

        PUSH ECX

        LEA EAX,[ESP + 030h]

        FSTP dword ptr [ESP + 038h]

        PUSH EDX

        PUSH EAX

        CALL AngleVectors

        FLD dword ptr [ESP + 044h]

        FMUL dword ptr [postureConstant200ac714]

        ADD ESP,010h

        FSUBR dword ptr [ESP + 018h]

        FSTP dword ptr [ESP + 0ch]

        FLD dword ptr [ESP + 038h]

        FMUL dword ptr [postureConstant200ac714]

        FSUBR dword ptr [ESP + 01ch]

        FSTP dword ptr [ESP + 010h]

        FLD dword ptr [ESP + 03ch]

        FMUL dword ptr [postureConstant200ac714]

        FSUBR dword ptr [ESP + 020h]

        JMP postureAt20032569
    postureAt20032565:
        FLD dword ptr [ESP + 014h]
    postureAt20032569:
        FSUB dword ptr [postureConstant200ac2e0]

        FLD dword ptr [ESP + 04ch]

        FMUL dword ptr [postureConstant200ac2bc]

        MOV EAX,[pm]

        MOV dword ptr [ESP + 058h],0c1000000h

        TEST EAX,EAX

        FADD dword ptr [ESP + 0ch]

        MOV dword ptr [ESP + 05ch],0c1000000h

        MOV dword ptr [ESP + 060h],0c1000000h

        MOV dword ptr [ESP + 040h],041000000h

        MOV dword ptr [ESP + 044h],041000000h

        MOV dword ptr [ESP + 048h],041000000h

        PUSH 02010001h

        FSTP dword ptr [ESP + 010h]

        FLD dword ptr [ESP + 054h]

        FMUL dword ptr [postureConstant200ac2bc]

        FADD dword ptr [ESP + 014h]

        FSTP dword ptr [ESP + 014h]

        FLD dword ptr [ESP + 058h]

        FMUL dword ptr [postureConstant200ac2bc]

        FADDP ST(1),ST(0)

        FLD dword ptr [ESP + 068h]

        FMUL dword ptr [postureConstant200ac2e0]

        FADD dword ptr [ESP + 010h]

        FSTP dword ptr [ESP + 010h]

        FLD dword ptr [ESP + 06ch]

        FMUL dword ptr [postureConstant200ac2e0]

        FADD dword ptr [ESP + 014h]

        FSTP dword ptr [ESP + 014h]

        FLD dword ptr [ESP + 070h]

        FMUL dword ptr [postureConstant200ac2e0]

        FADD ST(0),ST(1)

        FSTP dword ptr [ESP + 018h]

        FSTP ST(0)

        JZ postureAt2003263e

        MOV ECX,dword ptr [ESI + posturePsa0]

        LEA EDX,[ESP + 010h]

        PUSH ECX

        PUSH EDX

        LEA ECX,[ESP + 04ch]

        LEA EDX,[ESP + 064h]

        PUSH ECX

        PUSH EDX

        LEA ECX,[ESP + 02ch]

        LEA EDX,[ESP + 084h]

        PUSH ECX

        PUSH EDX

        CALL dword ptr [EAX + posturePmTrace]

        JMP postureAt2003266e
    postureAt2003263e:
        MOV EAX,dword ptr [ESI + posturePsa0]

        LEA ECX,[ESP + 010h]

        PUSH EAX

        LEA EDX,[ESP + 048h]

        PUSH ECX

        LEA EAX,[ESP + 064h]

        PUSH EDX

        LEA ECX,[ESP + 028h]

        PUSH EAX

        MOV EAX,dword ptr [ESP + 0c8h]

        LEA EDX,[ESP + 084h]

        PUSH ECX

        PUSH EDX

        CALL dword ptr [EAX + posturePmTrace]
    postureAt2003266e:
        FLD dword ptr [ESP + 08ch + postureTraceFraction]

        FMUL dword ptr [postureConstant200ac714]

        ADD ESP,01ch

        TEST EDI,EDI

        FSTP dword ptr [ESP + 08h]

        JLE postureAt200326c6

        FLD dword ptr [ESP + 024h]

        FCOM dword ptr [ESP + 08h]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt200326a9

        FILD dword ptr [pml + posturePmlMsec]

        FMUL dword ptr [postureConstant200ac698]

        FMUL dword ptr [postureConstant200ac714]

        FADDP ST(1),ST(0)
    postureAt200326a9:
        FCOM dword ptr [ESP + 08h]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt20032704

        FSTP ST(0)

        FLD dword ptr [ESP + 08h]

        POP EDI

        FSTP dword ptr [ESI + posturePs3c]

        POP ESI

        ADD ESP,0a0h

        RET
    postureAt200326c6:
        FLD dword ptr [ESP + 08h]

        FCHS

        FSTP dword ptr [ESP + 08h]

        FLD dword ptr [ESP + 024h]

        FCOM dword ptr [ESP + 08h]

        FNSTSW AX

        TEST AH,041h

        JNZ postureAt200326f3

        FILD dword ptr [pml + posturePmlMsec]

        FMUL dword ptr [postureConstant200ac698]

        FMUL dword ptr [postureConstant200ac714]

        FSUBP ST(1),ST(0)
    postureAt200326f3:
        FCOM dword ptr [ESP + 08h]

        FNSTSW AX

        TEST AH,01h

        JZ postureAt20032704

        FSTP ST(0)

        FLD dword ptr [ESP + 08h]
    postureAt20032704:
        FSTP dword ptr [ESI + posturePs3c]

        POP EDI

        POP ESI

        ADD ESP,0a0h

        RET
    }
}
#else

/* Portable TC behavior; not an exact Linux instruction port. */
void PM_UpdateLean(playerState_t *ps, usercmd_t *cmd, pmove_t *tpm) {
    PM_TCEUpdateLean(ps, cmd, tpm);
}
#endif



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

/* TC:E30013140 / Linux000e7658: free-climb clearance and stamina gate. */
/* TC:E qagame2003b480: original trace/x87 schedule, native field offsets. */
#if defined(_MSC_VER) && defined(_M_IX86)
typedef char climb832TraceLayout[(sizeof(trace_t)==56 && offsetof(trace_t, allsolid)==0 && offsetof(trace_t, fraction)==8)?1:-1];
enum {
    climb832PmExt = offsetof(pmove_t, pmext),
    climb832Field0 = offsetof(pmove_t, ps),
    climb832Field1 = offsetof(playerState_t, stats) + STAT_TCE_FLAGS * sizeof(int),
    climb832Field2 = offsetof(playerState_t, origin) + 0,
    climb832Field3 = offsetof(playerState_t, origin) + 4,
    climb832Field4 = offsetof(playerState_t, origin) + 8,
    climb832Field5 = offsetof(pmove_t, maxs) + 0,
    climb832Field6 = offsetof(pmove_t, maxs) + 4,
    climb832Field7 = offsetof(pmove_t, maxs) + 8,
    climb832Field8 = offsetof(playerState_t, velocity) + 8,
    climb832Field9 = offsetof(pmove_t, tracemask),
    climb832Field10 = offsetof(playerState_t, clientNum),
    climb832Field11 = offsetof(pmove_t, mins),
    climb832Field12 = offsetof(pmove_t, trace),
    climb832Field13 = offsetof(pmoveExt_t, sprintTime)
};
static const unsigned int climb832Const200ac110 = 0x3f800000u;
static const unsigned int climb832Const200ac180 = 0x3f000000u;
static const unsigned int climb832Const200ac1c4 = 0x43800000u;
static const unsigned int climb832Const200ac2bc = 0x40800000u;
static const unsigned int climb832Const200ac2d8 = 0x41900000u;
static const unsigned int climb832Const200ac32c = 0x42c00000u;
static const unsigned int climb832Const200ac7d4 = 0xc3480000u;
static const unsigned int climb832Const200ac85c = 0x42ca0000u;
static const unsigned int climb832Const200ac860 = 0x427c0000u;
static const unsigned int climb832Const200ac864 = 0xc1880000u;
static const unsigned int climb832Const200ac868 = 0xc3800000u;
static __declspec(naked) qboolean PM_ClimbSlideMove(int unused, const vec3_t normal, qboolean wasClimbing) {
    __asm {
        sub esp, 0x74
        mov eax, dword ptr [pm]
        mov dword ptr [esp + 4], 0x3f800000
        mov ecx, dword ptr [eax + climb832Field0]
        mov edx, dword ptr [ecx + climb832Field1]
        test dh, 2
        je climb832_2003b4a5
        mov dword ptr [esp + 4], 0x3fa00000
climb832_2003b4a5:
        mov ecx, dword ptr [ecx + climb832Field2]
        push esi
        mov dword ptr [esp + 0x28], ecx
        mov edx, dword ptr [eax + climb832Field0]
        push edi
        mov ecx, dword ptr [edx + climb832Field3]
        mov dword ptr [esp + 0x30], ecx
        mov edx, dword ptr [eax + climb832Field0]
        mov ecx, dword ptr [edx + climb832Field4]
        mov dword ptr [esp + 0x34], ecx
        mov edx, dword ptr [eax + climb832Field5]
        mov dword ptr [esp + 0x38], edx
        mov ecx, dword ptr [eax + climb832Field6]
        mov dword ptr [esp + 0x3c], ecx
        mov edx, dword ptr [eax + climb832Field7]
        mov dword ptr [esp + 0x40], edx
        mov eax, dword ptr [eax + climb832Field0]
        mov ecx, dword ptr [eax + climb832Field1]
        and ecx, 0xffffffdf
        mov dword ptr [eax + climb832Field1], ecx
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx + climb832Field0]
        fld dword ptr [eax + climb832Field8]
        fcomp dword ptr [climb832Const200ac7d4]
        fnstsw ax
        test ah, 1
        je climb832_2003b50f
        pop edi
        xor eax, eax
        pop esi
        add esp, 0x74
        ret 
climb832_2003b50f:
        fld dword ptr [esp + 0x34]
        mov edx, dword ptr [esp + 0x2c]
        mov eax, dword ptr [esp + 0x30]
        fsub dword ptr [climb832Const200ac1c4]
        mov dword ptr [esp + 0x14], edx
        mov dword ptr [esp + 0x18], eax
        fstp dword ptr [esp + 0x1c]
        mov edx, dword ptr [ecx + climb832Field9]
        mov eax, dword ptr [ecx + climb832Field0]
        push edx
        mov edx, dword ptr [eax + climb832Field10]
        lea eax, [esp + 0x18]
        push edx
        push eax
        lea edx, [esp + 0x44]
        lea eax, [ecx + climb832Field11]
        push edx
        push eax
        lea edx, [esp + 0x40]
        lea eax, [esp + 0x58]
        push edx
        push eax
        call dword ptr [ecx + climb832Field12]
        fld dword ptr [esp + 0x68]
        fmul dword ptr [climb832Const200ac868]
        add esp, 0x1c
        xor edi, edi
        fst dword ptr [esp + 0x10]
        fcomp dword ptr [climb832Const200ac864]
        fnstsw ax
        test ah, 0x41
        jne climb832_2003b580
        mov edi, 1
climb832_2003b580:
        fld dword ptr [esp + 0xc]
        fmul dword ptr [climb832Const200ac32c]
        mov ecx, dword ptr [esp + 0x2c]
        mov edx, dword ptr [esp + 0x30]
        mov eax, dword ptr [pm]
        mov dword ptr [esp + 0x14], ecx
        mov dword ptr [esp + 0x18], edx
        fstp dword ptr [esp + 8]
        fld dword ptr [esp + 0x34]
        fadd dword ptr [esp + 8]
        fstp dword ptr [esp + 0x1c]
        mov ecx, dword ptr [eax + climb832Field9]
        mov edx, dword ptr [eax]
        push ecx
        mov ecx, dword ptr [edx + climb832Field10]
        lea edx, [esp + 0x18]
        push ecx
        push edx
        lea ecx, [esp + 0x44]
        lea edx, [eax + climb832Field11]
        push ecx
        push edx
        lea ecx, [esp + 0x40]
        lea edx, [esp + 0x58]
        push ecx
        push edx
        call dword ptr [eax + climb832Field12]
        fld dword ptr [esp + 0x68]
        fcomp dword ptr [climb832Const200ac110]
        add esp, 0x1c
        fnstsw ax
        test ah, 1
        je climb832_2003b5fd
        fld dword ptr [esp + 0x4c]
        fmul dword ptr [esp + 8]
        fstp dword ptr [esp + 8]
climb832_2003b5fd:
        fld dword ptr [esp + 8]
        fcomp dword ptr [climb832Const200ac2d8]
        fnstsw ax
        test ah, 1
        je climb832_2003b616
        pop edi
        xor eax, eax
        pop esi
        add esp, 0x74
        ret 
climb832_2003b616:
        fld dword ptr [esp + 0x34]
        mov esi, dword ptr [esp + 0x84]
        mov eax, dword ptr [esp + 0x2c]
        fadd dword ptr [esp + 8]
        mov ecx, dword ptr [esp + 0x30]
        lea edx, [esp + 0x20]
        push edx
        mov dword ptr [esp + 0x18], eax
        mov dword ptr [esp + 0x1c], ecx
        mov dword ptr [esp + 0x2c], 0
        fstp dword ptr [esp + 0x20]
        fld dword ptr [esi]
        fchs 
        fstp dword ptr [esp + 0x24]
        fld dword ptr [esi + 4]
        fchs 
        fstp dword ptr [esp + 0x28]
        call VectorNormalize
        fstp st(0)
        fld dword ptr [esp + 0x24]
        fmul dword ptr [climb832Const200ac2bc]
        mov eax, dword ptr [pm]
        fadd dword ptr [esp + 0x18]
        fstp dword ptr [esp + 0x18]
        fld dword ptr [esp + 0x28]
        fmul dword ptr [climb832Const200ac2bc]
        fadd dword ptr [esp + 0x1c]
        fstp dword ptr [esp + 0x1c]
        fld dword ptr [esp + 0x2c]
        fmul dword ptr [climb832Const200ac2bc]
        fadd dword ptr [esp + 0x20]
        fstp dword ptr [esp + 0x20]
        mov ecx, dword ptr [eax + climb832Field9]
        mov edx, dword ptr [eax + climb832Field0]
        push ecx
        mov ecx, dword ptr [edx + climb832Field10]
        lea edx, [esp + 0x1c]
        push ecx
        push edx
        lea ecx, [esp + 0x48]
        lea edx, [eax + climb832Field11]
        push ecx
        push edx
        lea ecx, [esp + 0x2c]
        lea edx, [esp + 0x5c]
        push ecx
        push edx
        call dword ptr [eax + climb832Field12]
        mov eax, dword ptr [esp + 0x64]
        add esp, 0x20
        test eax, eax
        je climb832_2003b7ad
        fld dword ptr [esp + 8]
        fsub dword ptr [climb832Const200ac2d8]
        mov eax, dword ptr [esp + 0x2c]
        mov ecx, dword ptr [esp + 0x30]
        lea edx, [esp + 0x20]
        mov dword ptr [esp + 0x14], eax
        fmul dword ptr [climb832Const200ac180]
        push edx
        mov dword ptr [esp + 0x1c], ecx
        mov dword ptr [esp + 0x2c], 0
        fadd dword ptr [climb832Const200ac2d8]
        fstp dword ptr [esp + 0xc]
        fld dword ptr [esp + 0x38]
        fadd dword ptr [esp + 0xc]
        fstp dword ptr [esp + 0x20]
        fld dword ptr [esi]
        fchs 
        fstp dword ptr [esp + 0x24]
        fld dword ptr [esi + 4]
        fchs 
        fstp dword ptr [esp + 0x28]
        call VectorNormalize
        fstp st(0)
        fld dword ptr [esp + 0x24]
        fmul dword ptr [climb832Const200ac2bc]
        mov eax, dword ptr [pm]
        fadd dword ptr [esp + 0x18]
        fstp dword ptr [esp + 0x18]
        fld dword ptr [esp + 0x28]
        fmul dword ptr [climb832Const200ac2bc]
        fadd dword ptr [esp + 0x1c]
        fstp dword ptr [esp + 0x1c]
        fld dword ptr [esp + 0x2c]
        fmul dword ptr [climb832Const200ac2bc]
        fadd dword ptr [esp + 0x20]
        fstp dword ptr [esp + 0x20]
        mov ecx, dword ptr [eax + climb832Field9]
        mov edx, dword ptr [eax + climb832Field0]
        push ecx
        mov ecx, dword ptr [edx + climb832Field10]
        lea edx, [esp + 0x1c]
        push ecx
        push edx
        lea ecx, [esp + 0x48]
        lea edx, [eax + climb832Field11]
        push ecx
        push edx
        lea ecx, [esp + 0x2c]
        lea edx, [esp + 0x5c]
        push ecx
        push edx
        call dword ptr [eax + climb832Field12]
        mov eax, dword ptr [esp + 0x64]
        add esp, 0x20
        test eax, eax
        je climb832_2003b7ad
        pop edi
        xor eax, eax
        pop esi
        add esp, 0x74
        ret 
climb832_2003b7ad:
        fld dword ptr [esp + 0x34]
        fadd dword ptr [esp + 8]
        mov eax, dword ptr [esp + 0x2c]
        mov ecx, dword ptr [esp + 0x30]
        lea edx, [esp + 0x20]
        mov dword ptr [esp + 0x14], eax
        push edx
        mov dword ptr [esp + 0x1c], ecx
        fstp dword ptr [esp + 0x20]
        fld dword ptr [esi]
        fchs 
        fstp dword ptr [esp + 0x24]
        fld dword ptr [esi + 4]
        fchs 
        fstp dword ptr [esp + 0x28]
        mov dword ptr [esp + 0x2c], 0
        call VectorNormalize
        fstp st(0)
        fld dword ptr [esp + 0x24]
        fmul dword ptr [climb832Const200ac2bc]
        fadd dword ptr [esp + 0x18]
        fst dword ptr [esp + 0x18]
        fld dword ptr [esp + 0x28]
        fmul dword ptr [climb832Const200ac2bc]
        fadd dword ptr [esp + 0x1c]
        fstp dword ptr [esp + 0x1c]
        fld dword ptr [esp + 0x2c]
        fmul dword ptr [climb832Const200ac2bc]
        mov eax, dword ptr [esp + 0x1c]
        mov dword ptr [esp + 0x28], eax
        mov eax, dword ptr [pm]
        fadd dword ptr [esp + 0x20]
        fstp dword ptr [esp + 0x20]
        fstp dword ptr [esp + 0x24]
        fld dword ptr [esp + 0x20]
        fsub dword ptr [climb832Const200ac1c4]
        fstp dword ptr [esp + 0x2c]
        mov ecx, dword ptr [eax + climb832Field9]
        mov edx, dword ptr [eax + climb832Field0]
        push ecx
        mov ecx, dword ptr [edx + climb832Field10]
        lea edx, [esp + 0x28]
        push ecx
        push edx
        lea ecx, [esp + 0x48]
        lea edx, [eax + climb832Field11]
        push ecx
        push edx
        lea ecx, [esp + 0x2c]
        lea edx, [esp + 0x5c]
        push ecx
        push edx
        call dword ptr [eax + climb832Field12]
        fld dword ptr [esp + 0x28]
        fsub dword ptr [esp + 0x30]
        fld dword ptr [esp + 0x6c]
        fmul dword ptr [climb832Const200ac1c4]
        add esp, 0x20
        fsubp st(1), st(0)
        fcom dword ptr [climb832Const200ac860]
        fnstsw ax
        test ah, 1
        je climb832_2003b8b5
        mov eax, dword ptr [pm]
        pop edi
        fstp st(0)
        mov eax, dword ptr [eax + climb832Field0]
        pop esi
        mov ecx, dword ptr [eax + climb832Field1]
        or ecx, 0x40
        mov dword ptr [eax + climb832Field1], ecx
        xor eax, eax
        add esp, 0x74
        ret 
climb832_2003b8b5:
        fld dword ptr [esp + 0xc]
        fmul dword ptr [climb832Const200ac85c]
        fxch st(1)
        fcompp 
        fnstsw ax
        test ah, 0x41
        jne climb832_2003b8d2
        pop edi
        xor eax, eax
        pop esi
        add esp, 0x74
        ret 
climb832_2003b8d2:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + climb832PmExt]
        mov ecx, dword ptr [ecx + climb832Field13]
        cmp ecx, 0x384
        jge climb832_2003b8f8
        mov edx, dword ptr [esp + 0x88]
        test edx, edx
        jne climb832_2003b8f8
        pop edi
        xor eax, eax
        pop esi
        add esp, 0x74
        ret 
climb832_2003b8f8:
        cmp ecx, 0xa
        jge climb832_2003b905
        pop edi
        xor eax, eax
        pop esi
        add esp, 0x74
        ret 
climb832_2003b905:
        mov eax, dword ptr [eax + climb832Field0]
        mov edx, dword ptr [eax + climb832Field1]
        or edx, 0x20
        mov dword ptr [eax + climb832Field1], edx
        xor eax, eax
        test edi, edi
        pop edi
        pop esi
        sete al
        add esp, 0x74
        ret 
    }
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

/* TC qagame20033480: original control flow; undefined freeClimb remains a documented boundary. */
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC qagame832: native offsets and original Windows x87 instruction order. */
static const float move832K200ac110 = 1.0;
static const float move832K200ac720 = -400.0;
static const double move832K200ac378 = 0.001;
static const double move832K200ac130 = 1.0;
static const float move832K200ac100 = 0.0;
static const float move832K200ac778 = 20000.0;
static const float move832K200ac774 = 3500.0;
static const float move832K200ac250 = 3000.0;
static const float move832K200ac6dc = 2500.0;
static const float move832K200ac2ec = 0.33000001311302185;
static const float move832K200ac180 = 0.5;
static const float move832K200ac770 = 1250.0;
static const float move832K200ac700 = 120.0;
enum {
    move832Ps = offsetof(pmove_t,ps),
    move832Ext = offsetof(pmove_t,pmext),
    move832Character = offsetof(pmove_t,character),
    move832Buttons = offsetof(pmove_t,cmd)+offsetof(usercmd_t,buttons),
    move832ForwardCmd = offsetof(pmove_t,cmd)+offsetof(usercmd_t,forwardmove),
    move832RightCmd = offsetof(pmove_t,cmd)+offsetof(usercmd_t,rightmove),
    move832Mask = offsetof(pmove_t,tracemask),
    move832Mins = offsetof(pmove_t,mins),
    move832Maxs = offsetof(pmove_t,maxs),
    move832Trace = offsetof(pmove_t,trace),
    move832Skill = offsetof(pmove_t,skill),
    move832Origin = offsetof(playerState_t,origin),
    move832Velocity = offsetof(playerState_t,velocity),
    move832Flags = offsetof(playerState_t,pm_flags),
    move832Eflags = offsetof(playerState_t,eFlags),
    move832Health = offsetof(playerState_t,stats)+STAT_HEALTH*sizeof(int),
    move832TcFlags = offsetof(playerState_t,stats)+STAT_TCE_FLAGS*sizeof(int),
    move832WeaponFlags = offsetof(playerState_t,stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    move832Ground = offsetof(playerState_t,groundEntityNum),
    move832Client = offsetof(playerState_t,clientNum),
    move832Weapon = offsetof(playerState_t,weapon),
    move832WeaponState = offsetof(playerState_t,weaponstate),
    move832Adrenaline = offsetof(playerState_t,powerups)+PW_ADRENALINE*sizeof(int),
    move832Fatigue = offsetof(playerState_t,powerups)+PW_NOFATIGUE*sizeof(int),
    move832Carried = offsetof(playerState_t,holdable)+9*sizeof(int),
    move832Exert = offsetof(playerState_t,sprintExertTime),
    move832Sprint = offsetof(pmoveExt_t,sprintTime),
    move832Model = offsetof(bg_character_t,animModelInfo),
    move832DefWeight = offsetof(tce_weaponDef_t,loadoutWeight),
    move832Frame = offsetof(pml_t,frametime),
    move832Walking = offsetof(pml_t,walking),
    move832GroundPlane = offsetof(pml_t,groundPlane),
    move832Forward = offsetof(pml_t,forward),
    move832Ladder = offsetof(pml_t,ladder),
    move832Material = offsetof(pml_t,tceLadderSurfaceFlags)
};
#ifdef GAMEDLL
enum { move832Leadership = offsetof(pmove_t,leadership) };
#endif
typedef char move832TraceLayout[(sizeof(trace_t)==56 && offsetof(trace_t,fraction)==8 && offsetof(trace_t,plane)==24 && offsetof(trace_t,surfaceFlags)==44 && offsetof(trace_t,contents)==48 && offsetof(trace_t,entityNum)==52) ? 1 : -1];
typedef char move832Protocol[(offsetof(pmove_t,ps)==0 && SK_BATTLE_SENSE==0 && offsetof(cplane_t,normal)==0 && PMF_LADDER==4 && PMF_DUCKED==1 && EF_PRONE==0x80000 && EF_PRONE_MOVING==0x100000 && EV_FOOTSTEP==1 && ANIM_ET_CLIMB_MOUNT==9 && ANIM_ET_CLIMB_DISMOUNT==10 && sizeof(tce_weaponDef_t)==460) ? 1 : -1];
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void PM_CheckLadderMove(void) {
    __asm {
        sub esp, 0x64
        mov eax, dword ptr [pm]
        push ebx
        xor ebx, ebx
        push esi
        mov eax, dword ptr [eax]
        push edi
        mov dword ptr [esp+0x10], 0 /* Defined native fallback for original uninitialized freeClimb. */
        mov dword ptr [esp + 0xc], 0x3f800000
        mov edx, dword ptr [eax+move832TcFlags]
        and edx, 0xffffffbf
        mov dword ptr [eax+move832TcFlags], edx
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        mov ecx, dword ptr [eax+move832TcFlags]
        and ecx, 0xffffffdf
        mov dword ptr [eax+move832TcFlags], ecx
        mov eax, dword ptr [pml+move832Walking]
        cmp eax, ebx
        jne move832_200334ce
        mov dword ptr [esp + 0xc], 0x42400000
move832_200334ce:
        mov edx, dword ptr [pml+move832Forward]
        mov eax, dword ptr [pml+move832Forward+4]
        lea ecx, [esp + 0x20]
        mov dword ptr [esp + 0x20], edx
        push ecx
        mov dword ptr [esp + 0x28], eax
        mov dword ptr [esp + 0x2c], 0
        call VectorNormalize
        mov eax, dword ptr [pm]
        fstp st(0)
        fld dword ptr [esp + 0x24]
        fmul dword ptr [esp + 0x10]
        mov edx, dword ptr [eax]
        fadd dword ptr [edx+move832Origin]
        fstp dword ptr [esp + 0x18]
        fld dword ptr [esp + 0x28]
        fmul dword ptr [esp + 0x10]
        mov ecx, dword ptr [eax]
        fadd dword ptr [ecx+move832Origin+4]
        fstp dword ptr [esp + 0x1c]
        fld dword ptr [esp + 0x2c]
        fmul dword ptr [esp + 0x10]
        mov edx, dword ptr [eax]
        fadd dword ptr [edx+move832Origin+8]
        fstp dword ptr [esp + 0x20]
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [eax+move832Mask]
        push edx
        mov edx, dword ptr [ecx+move832Client]
        add ecx, move832Origin
        push edx
        lea edx, [esp + 0x20]
        push edx
        lea edx, [eax+move832Maxs]
        push edx
        lea edx, [eax+move832Mins]
        push edx
        push ecx
        lea ecx, [esp + 0x54]
        push ecx
        call dword ptr [eax+move832Trace]
        fld dword ptr [esp + 0x60]
        fcomp dword ptr [move832K200ac110]
        add esp, 0x20
        fnstsw ax
        test ah, 1
        je move832_200335b2
        mov edx, dword ptr [esp + 0x64]
        and edx, 0xff000000
        cmp edx, 0x14000000
        jne move832_200335c8
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        test byte ptr [ecx+move832WeaponFlags], 0x80
        jne move832_200335c8
        push EV_TCE_FENCE_TOUCH
        call PM_AddEvent
        mov edx, dword ptr [pm]
        add esp, 4
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+move832WeaponFlags]
        or cl, 0x80
        jmp move832_200335c2
move832_200335b2:
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        mov ecx, dword ptr [eax+move832WeaponFlags]
        and cl, 0x7f
move832_200335c2:
        mov dword ptr [eax+move832WeaponFlags], ecx
move832_200335c8:
        mov ecx, dword ptr [esp + 0x64]
        mov dword ptr [pml+move832Material], ecx
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        mov edi, dword ptr [eax+move832Flags]
        mov esi, dword ptr [eax+move832TcFlags]
        mov dword ptr [pml+move832Ladder], ebx
        mov eax, dword ptr [ecx]
        shr edi, 2
        mov edx, dword ptr [eax+move832Flags]
        and edi, 1
        and edx, 0xfffffffb
        mov dword ptr [eax+move832Flags], edx
        mov edx, dword ptr [pm]
        shr esi, 7
        mov eax, dword ptr [edx]
        and esi, 1
        mov ecx, dword ptr [eax+move832TcFlags]
        and cl, 0x7f
        mov dword ptr [eax+move832TcFlags], ecx
        mov ecx, dword ptr [pm]
        mov dword ptr [ladderforward], ebx
        mov edx, dword ptr [ecx]
        cmp dword ptr [edx+move832Health], ebx
        jg move832_20033647
        mov dword ptr [edx+move832Ground], 0x3ff
        pop edi
        mov dword ptr [pml+move832GroundPlane], ebx
        mov dword ptr [pml+move832Walking], ebx
        pop esi
        pop ebx
        add esp, 0x64
        ret 
move832_20033647:
        test dword ptr [edx+move832Eflags], 0x180000
        jne move832_20033981
        mov eax, dword ptr [edx+move832WeaponState]
        cmp eax, 9
        je move832_20033981
        cmp eax, 0xa
        je move832_20033981
        cmp eax, 0xb
        je move832_20033981
        fld dword ptr [esp + 0x40]
        fcomp dword ptr [move832K200ac110]
        mov ebx, dword ptr [esp + 0x64]
        fnstsw ax
        test ah, 1
        je move832_200336bd
        test bl, 8
        je move832_200336bd
        mov eax, dword ptr [pml+move832Walking]
        test eax, eax
        jne move832_200336ac
        fld dword ptr [edx+move832Velocity+8]
        fcomp dword ptr [move832K200ac720]
        fnstsw ax
        test ah, 1
        jne move832_2003374b
move832_200336ac:
        mov ebx, 1
        mov eax, ebx
        mov dword ptr [pml+move832Ladder], eax
        jmp move832_2003375d
move832_200336bd:
        fld dword ptr [esp + 0x40]
        fmul dword ptr [esp + 0xc]
        fcomp dword ptr [move832K200ac110]
        fnstsw ax
        test ah, 1
        je move832_2003374b
        mov eax, dword ptr [edx+move832TcFlags]
        test ah, 8
        jne move832_2003374b
        test ebx, 0x40000
        jne move832_2003374b
        test dword ptr [esp + 0x68], 0x6000000
        jne move832_2003374b
        cmp dword ptr [esp + 0x6c], 0x3fe
        jne move832_2003374b
        fld dword ptr [esp + 0x58]
        fabs 
        fcomp qword ptr [move832K200ac378]
        fnstsw ax
        test ah, 1
        je move832_2003374b
        lea eax, [esp + 0x50]
        push esi
        mov ebx, 1
        push eax
        push ebx
        call PM_ClimbSlideMove
        add esp, 0xc
        test eax, eax
        je move832_20033745
        mov ecx, dword ptr [pm]
        mov dword ptr [pml+move832Ladder], ebx
        mov dword ptr [esp + 0x10], ebx
        mov eax, dword ptr [ecx]
        mov ecx, dword ptr [eax+move832TcFlags]
        or cl, 0x80
        mov dword ptr [eax+move832TcFlags], ecx
move832_20033745:
        mov ecx, dword ptr [pm]
move832_2003374b:
        mov eax, dword ptr [pml+move832Ladder]
        mov ebx, 1
        test eax, eax
        je move832_200338af
move832_2003375d:
        mov edx, dword ptr [esp + 0x50]
        mov dword ptr [laddervec], edx
        mov edx, dword ptr [esp + 0x54]
        mov dword ptr [laddervec+4], edx
        mov edx, dword ptr [esp + 0x58]
        test eax, eax
        mov dword ptr [laddervec+8], edx
        je move832_200338af
        mov eax, dword ptr [pml+move832Walking]
        test eax, eax
        jne move832_200338c2
        fld dword ptr [esp + 0x40]
        fmul dword ptr [esp + 0xc]
        fcomp qword ptr [move832K200ac130]
        fnstsw ax
        test ah, 0x41
        jne move832_200338c2
        fld dword ptr [esp + 0xc]
        mov dword ptr [pml+move832Ladder], 0
        mov eax, dword ptr [ecx+move832Mins]
        fchs 
        mov dword ptr [esp + 0x2c], eax
        mov edx, dword ptr [ecx+move832Mins+4]
        fld st(0)
        fmul dword ptr [esp + 0x50]
        mov dword ptr [esp + 0x30], edx
        mov dword ptr [esp + 0x34], 0xbf800000
        mov eax, dword ptr [ecx]
        fadd dword ptr [eax+move832Origin]
        fstp dword ptr [esp + 0x14]
        mov edx, dword ptr [ecx]
        fld st(0)
        fmul dword ptr [esp + 0x54]
        fadd dword ptr [edx+move832Origin+4]
        fstp dword ptr [esp + 0x18]
        mov eax, dword ptr [ecx]
        fmul dword ptr [esp + 0x58]
        fadd dword ptr [eax+move832Origin+8]
        fstp dword ptr [esp + 0x1c]
        mov eax, dword ptr [ecx]
        mov edx, dword ptr [ecx+move832Mask]
        push edx
        mov edx, dword ptr [eax+move832Client]
        add eax, move832Origin
        push edx
        lea edx, [esp + 0x1c]
        push edx
        lea edx, [ecx+move832Maxs]
        push edx
        lea edx, [esp + 0x3c]
        push edx
        push eax
        lea eax, [esp + 0x50]
        push eax
        call dword ptr [ecx+move832Trace]
        fld dword ptr [esp + 0x5c]
        fcomp dword ptr [move832K200ac110]
        add esp, 0x1c
        fnstsw ax
        test ah, 1
        je move832_2003389f
        test byte ptr [esp + 0x64], 8
        je move832_20033862
        mov ecx, dword ptr [pm]
        mov dword ptr [ladderforward], ebx
        mov dword ptr [pml+move832Ladder], ebx
        mov eax, dword ptr [ecx]
        or dword ptr [eax+move832Flags], 4
        jmp move832_200338cc
move832_20033862:
        mov eax, dword ptr [esp + 0x10]
        test eax, eax
        je move832_2003389f
        mov edx, dword ptr [pm]
        mov dword ptr [ladderforward], ebx
        mov dword ptr [pml+move832Ladder], ebx
        mov eax, dword ptr [edx]
        mov edx, dword ptr [eax+move832Flags]
        or edx, 4
        mov dword ptr [eax+move832Flags], edx
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        mov ecx, dword ptr [eax+move832TcFlags]
        or cl, 0x80
        mov dword ptr [eax+move832TcFlags], ecx
        jmp move832_200338cc
move832_2003389f:
        mov dword ptr [pml+move832Ladder], 0
move832_200338a9:
        mov ecx, dword ptr [pm]
move832_200338af:
        test esi, esi
        je move832_200338f7
        mov edx, dword ptr [ecx]
        test byte ptr [edx+move832TcFlags], 0x80
        jne move832_20033913
        push 0x11
        jmp move832_20033904
move832_200338c2:
        mov ecx, dword ptr [ecx]
        mov eax, dword ptr [ecx+move832Flags]
        or al, 4
        mov dword ptr [ecx+move832Flags], eax
move832_200338cc:
        mov eax, dword ptr [pml+move832Ladder]
        test eax, eax
        je move832_200338a9
        mov eax, dword ptr [pml+move832Walking]
        test eax, eax
        je move832_200338a9
        mov ecx, dword ptr [pm]
        mov al, byte ptr [ecx+move832ForwardCmd]
        test al, al
        jg move832_200338af
        mov dword ptr [pml+move832Ladder], 0
        jmp move832_200338af
move832_200338f7:
        mov eax, dword ptr [ecx]
        test byte ptr [eax+move832TcFlags], 0x80
        je move832_20033913
        push 0x10
move832_20033904:
        push ebx
        call PM_AddEventExt
        mov ecx, dword ptr [pm]
        add esp, 8
move832_20033913:
        mov eax, dword ptr [pml+move832Ladder]
        test eax, eax
        jne move832_20033957
        test edi, edi
        je move832_20033981
        mov edx, dword ptr [ecx]
        fld dword ptr [edx+move832Velocity+8]
        fcomp dword ptr [move832K200ac100]
        fnstsw ax
        test ah, 0x41
        jne move832_20033981
        mov ecx, dword ptr [ecx+move832Character]
        push 0
        push 0
        push 0xa
        mov eax, dword ptr [ecx+move832Model]
        push eax
        push edx
        call BG_AnimScriptEvent
        mov eax, dword ptr [pml+move832Ladder]
        add esp, 0x14
        test eax, eax
        je move832_20033981
        mov ecx, dword ptr [pm]
move832_20033957:
        test edi, edi
        jne move832_20033981
        mov edx, dword ptr [ecx]
        fld dword ptr [edx+move832Velocity+8]
        fcomp dword ptr [move832K200ac100]
        fnstsw ax
        test ah, 1
        je move832_20033981
        mov ecx, dword ptr [ecx+move832Character]
        push edi
        push edi
        push 9
        mov eax, dword ptr [ecx+move832Model]
        push eax
        push edx
        call BG_AnimScriptEvent
        add esp, 0x14
move832_20033981:
        pop edi
        pop esi
        pop ebx
        add esp, 0x64
        ret 
    }
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
               !(pm->ps->stats[STAT_TCE_FLAGS] & 0x800) &&
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
#if defined(_MSC_VER) && defined(_M_IX86)
/* Whole TC Windows20033990; original stack/x87 schedule, native structure offsets.
 * PM_Friction remains a separately documented integration boundary. */
static const double ladderK200ac738 = -200.0;
static const double ladderK200ac128 = 0.5;
static const double ladderK200ac730 = 2.5;
static const double ladderK200ac130 = 1.0;
static const double ladderK200ac728 = -1.0;
static const double ladderK200ac450 = 0.9;
static const float ladderK200ac100 = 0.0;
static const float ladderK200ac110 = 1.0;
static const float ladderK200ac2e4 = -1.0;
enum {
    ladderForward826 = offsetof(pml_t, forward),
    ladderRight826 = offsetof(pml_t, right),
    ladderFrame826 = offsetof(pml_t, frametime),
    ladderPlayerState826 = offsetof(pmove_t, ps),
    ladderCmd826 = offsetof(pmove_t, cmd),
    ladderForwardCmd826 = offsetof(pmove_t, cmd) + offsetof(usercmd_t, forwardmove),
    ladderRightCmd826 = offsetof(pmove_t, cmd) + offsetof(usercmd_t, rightmove),
    ladderVelocity826 = offsetof(playerState_t, velocity),
    ladderGravity826 = offsetof(playerState_t, gravity),
    ladderMovementDir826 = offsetof(playerState_t, movementDir)
};
__declspec(naked) void PM_LadderMove(void) {
    __asm {
        mov eax, dword ptr [ladderforward]
        sub esp, 0x3c
        push ebx
        xor ebx, ebx
        cmp eax, ebx
        je ladder826_200339e8
        fld dword ptr [laddervec]
        fmul qword ptr [ladderK200ac738]
        mov eax, dword ptr [pm]
        fst dword ptr [esp + 0x10]
        fld dword ptr [laddervec+4]
        fmul qword ptr [ladderK200ac738]
        fstp dword ptr [esp + 0x14]
        fld dword ptr [laddervec+8]
        fmul qword ptr [ladderK200ac738]
        fstp dword ptr [esp + 0x18]
        mov ecx, dword ptr [eax + ladderPlayerState826]
        fstp dword ptr [ecx + ladderVelocity826]
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [esp + 0x14]
        mov eax, dword ptr [edx + ladderPlayerState826]
        mov dword ptr [eax + ladderVelocity826+4], ecx
ladder826_200339e8:
        fld dword ptr [pml+ladderForward826+8]
        fadd qword ptr [ladderK200ac128]
        fmul qword ptr [ladderK200ac730]
        fst dword ptr [esp + 4]
        fcomp qword ptr [ladderK200ac130]
        fnstsw ax
        test ah, 0x41
        jne ladder826_20033a15
        mov dword ptr [esp + 4], 0x3f800000
        jmp ladder826_20033a2e
ladder826_20033a15:
        fld dword ptr [esp + 4]
        fcomp qword ptr [ladderK200ac728]
        fnstsw ax
        test ah, 1
        je ladder826_20033a2e
        mov dword ptr [esp + 4], 0xbf800000
ladder826_20033a2e:
        push offset pml+ladderForward826
        mov dword ptr [pml+ladderForward826+8], 0
        mov dword ptr [pml+ladderRight826+8], 0
        call VectorNormalize
        fstp st(0)
        push offset pml+ladderRight826
        call VectorNormalize
        mov edx, dword ptr [pm]
        add edx, ladderCmd826
        fstp st(0)
        push edx
        call PM_CmdScale
        mov ecx, dword ptr [pm]
        mov dword ptr [esp + 0x24], 0
        fstp dword ptr [esp + 0x14]
        mov dword ptr [esp + 0x20], 0
        mov dword ptr [esp + 0x1c], 0
        mov al, byte ptr [ecx + ladderForwardCmd826]
        add esp, 0xc
        cmp al, bl
        je ladder826_20033ab2
        movsx eax, al
        mov dword ptr [esp + 0xc], eax
        fild dword ptr [esp + 0xc]
        fmul dword ptr [esp + 8]
        fmul dword ptr [esp + 4]
        fmul qword ptr [ladderK200ac450]
        fstp dword ptr [esp + 0x18]
ladder826_20033ab2:
        cmp byte ptr [ecx + ladderRightCmd826], bl
        je ladder826_20033b91
        lea ecx, [esp + 0x28]
        push ecx
        push offset laddervec
        call vectoangles
        lea edx, [esp + 0x24]
        push ebx
        push edx
        lea eax, [esp + 0x38]
        push ebx
        push eax
        call AngleVectors
        fld dword ptr [pml+ladderForward826+4]
        fmul dword ptr [laddervec+4]
        fld dword ptr [pml+ladderForward826+8]
        fmul dword ptr [laddervec+8]
        add esp, 0x18
        faddp st(1), st(0)
        fld dword ptr [pml+ladderForward826]
        fmul dword ptr [laddervec]
        faddp st(1), st(0)
        fcomp dword ptr [ladderK200ac100]
        fnstsw ax
        test ah, 1
        je ladder826_20033b20
        lea ecx, [esp + 0x1c]
        push ecx
        call VectorInverse
        add esp, 4
ladder826_20033b20:
        mov eax, dword ptr [pm]
        fld dword ptr [esp + 8]
        movsx edx, byte ptr [eax + ladderRightCmd826]
        mov dword ptr [esp + 0xc], edx
        fild dword ptr [esp + 0xc]
        fmulp st(1), st(0)
        fmul qword ptr [ladderK200ac128]
        fmul dword ptr [esp + 0x1c]
        fadd dword ptr [esp + 0x10]
        fstp dword ptr [esp + 0x10]
        movsx ecx, byte ptr [eax + ladderRightCmd826]
        fld dword ptr [esp + 8]
        mov dword ptr [esp + 0xc], ecx
        fild dword ptr [esp + 0xc]
        fmulp st(1), st(0)
        fmul qword ptr [ladderK200ac128]
        fmul dword ptr [esp + 0x20]
        fadd dword ptr [esp + 0x14]
        fstp dword ptr [esp + 0x14]
        movsx edx, byte ptr [eax + ladderRightCmd826]
        fld dword ptr [esp + 8]
        mov dword ptr [esp + 0xc], edx
        fild dword ptr [esp + 0xc]
        fmulp st(1), st(0)
        fmul qword ptr [ladderK200ac128]
        fmul dword ptr [esp + 0x24]
        fadd dword ptr [esp + 0x18]
        fstp dword ptr [esp + 0x18]
ladder826_20033b91:
        call PM_Friction
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + ladderPlayerState826]
        fld dword ptr [ecx + ladderVelocity826]
        fcomp dword ptr [ladderK200ac110]
        fnstsw ax
        test ah, 1
        je ladder826_20033bc0
        fld dword ptr [ecx + ladderVelocity826]
        fcomp dword ptr [ladderK200ac2e4]
        fnstsw ax
        test ah, 0x41
        jne ladder826_20033bc0
        mov dword ptr [ecx + ladderVelocity826], ebx
ladder826_20033bc0:
        mov ecx, dword ptr [pm]
        mov ecx, dword ptr [ecx + ladderPlayerState826]
        fld dword ptr [ecx + ladderVelocity826+4]
        fcomp dword ptr [ladderK200ac110]
        fnstsw ax
        test ah, 1
        je ladder826_20033beb
        fld dword ptr [ecx + ladderVelocity826+4]
        fcomp dword ptr [ladderK200ac2e4]
        fnstsw ax
        test ah, 0x41
        jne ladder826_20033beb
        mov dword ptr [ecx + ladderVelocity826+4], ebx
ladder826_20033beb:
        lea edx, [esp + 0x34]
        lea eax, [esp + 0x10]
        push edx
        push eax
        call VectorNormalize2
        fstp dword ptr [esp + 0x14]
        mov ecx, dword ptr [pm_accelerate]
        mov edx, dword ptr [esp + 0x14]
        push ecx
        lea eax, [esp + 0x40]
        push edx
        push eax
        call PM_Accelerate
        fld dword ptr [esp + 0x2c]
        fcomp dword ptr [ladderK200ac100]
        add esp, 0x14
        fnstsw ax
        test ah, 0x40
        je ladder826_20033c89
        mov ecx, dword ptr [pm]
        mov ecx, dword ptr [ecx + ladderPlayerState826]
        fld dword ptr [ecx + ladderVelocity826+8]
        fcomp dword ptr [ladderK200ac100]
        fild dword ptr [ecx + ladderGravity826]
        fmul dword ptr [pml+ladderFrame826]
        fnstsw ax
        test ah, 0x41
        jne ladder826_20033c69
        fsubr dword ptr [ecx + ladderVelocity826+8]
        fstp dword ptr [ecx + ladderVelocity826+8]
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx + ladderPlayerState826]
        fld dword ptr [ecx + ladderVelocity826+8]
        fcomp dword ptr [ladderK200ac100]
        fnstsw ax
        test ah, 1
        je ladder826_20033c89
        jmp ladder826_20033c86
ladder826_20033c69:
        fadd dword ptr [ecx + ladderVelocity826+8]
        fstp dword ptr [ecx + ladderVelocity826+8]
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + ladderPlayerState826]
        fld dword ptr [ecx + ladderVelocity826+8]
        fcomp dword ptr [ladderK200ac100]
        fnstsw ax
        test ah, 0x41
        jne ladder826_20033c89
ladder826_20033c86:
        mov dword ptr [ecx + ladderVelocity826+8], ebx
ladder826_20033c89:
        push ebx
        call PM_StepSlideMove
        mov ecx, dword ptr [pm]
        add esp, 4
        mov edx, dword ptr [ecx + ladderPlayerState826]
        mov dword ptr [edx + ladderMovementDir826], ebx
        pop ebx
        add esp, 0x3c
        ret 
    }
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
void PM_LadderMove (void) {
	float	wishspeed, scale;
	vec3_t	wishdir, wishvel;
	float	upscale;

	if (ladderforward) {
		// move towards the ladder
		VectorScale( laddervec, -200.0, wishvel );
		pm->ps->velocity[0] = wishvel[0];
		pm->ps->velocity[1] = wishvel[1];
	}

	upscale = (pml.forward[2] + 0.5)*2.5;
	if (upscale > 1.0)
		upscale = 1.0;
	else if (upscale < -1.0)
		upscale = -1.0;

	// forward/right should be horizontal only
	pml.forward[2] = 0;
	pml.right[2] = 0;
	VectorNormalize (pml.forward);
	VectorNormalize (pml.right);

	// move depending on the view, if view is straight forward, then go up
	// if view is down more then X degrees, start going down
	// if they are back pedalling, then go in reverse of above
	scale = PM_CmdScale( &pm->cmd );
	VectorClear( wishvel );

	if( pm->cmd.forwardmove ) {
		wishvel[2] = (float)pm->cmd.forwardmove * scale * upscale * 0.9;
	}
//Com_Printf("wishvel[2] = %i, fwdmove = %i\n", (int)wishvel[2], (int)pm->cmd.forwardmove );

	if( pm->cmd.rightmove ) {
		// strafe, so we can jump off ladder
		vec3_t	ladder_right, ang;
		vectoangles( laddervec, ang );
		AngleVectors( ang, NULL, ladder_right, NULL );

		// if we are looking away from the ladder, reverse the right vector
		if( DotProduct( laddervec, pml.forward ) < 0 )
			VectorInverse( ladder_right );

		//VectorMA( wishvel, 0.5 * scale * (float)pm->cmd.rightmove, pml.right, wishvel );
		VectorMA( wishvel, 0.5 * scale * (float)pm->cmd.rightmove, ladder_right, wishvel );
	}

	// do strafe friction
	PM_Friction();

	if( pm->ps->velocity[0] < 1 && pm->ps->velocity[0] > -1  )
		pm->ps->velocity[0] = 0;
	if( pm->ps->velocity[1] < 1&& pm->ps->velocity[1] > -1 )
		pm->ps->velocity[1] = 0;

	wishspeed = VectorNormalize2( wishvel, wishdir );

	PM_Accelerate(wishdir, wishspeed, pm_accelerate);
	if( !wishvel[2] )
	{
		if (pm->ps->velocity[2] > 0)
		{
			pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
			if (pm->ps->velocity[2] < 0)
				pm->ps->velocity[2]  = 0;
		}
		else
		{
			pm->ps->velocity[2] += pm->ps->gravity * pml.frametime;
			if (pm->ps->velocity[2] > 0)
				pm->ps->velocity[2]  = 0;
		}
	}

//Com_Printf("vel[2] = %i\n", (int)pm->ps->velocity[2] );

	PM_StepSlideMove (qfalse);	// no gravity while going up ladder

	// always point legs forward
	pm->ps->movementDir = 0;
}
#endif


/*
==============
PM_Sprint
==============
*/
/* TC qagame20034210 / Linux0008c7bc: complete Windows server sprint controller. */
#if defined(_MSC_VER) && defined(_M_IX86) && defined(GAMEDLL)
__declspec(naked) void PM_Sprint(void) {
    __asm {
        mov ecx, dword ptr [pm]
        sub esp, 8
        mov al, byte ptr [ecx+move832Buttons]
        push ebx
        xor ebx, ebx
        push esi
        test al, 0x20
        mov esi, 0x80000
        je move832_20034396
        cmp byte ptr [ecx+move832ForwardCmd], bl
        jne move832_2003423b
        cmp byte ptr [ecx+move832RightCmd], bl
        je move832_20034396
move832_2003423b:
        mov edx, dword ptr [ecx]
        test byte ptr [edx+move832Flags], 1
        jne move832_20034396
        test dword ptr [edx+move832Eflags], esi
        jne move832_20034396
        cmp dword ptr [edx+move832Adrenaline], ebx
        je move832_20034267
        mov eax, dword ptr [ecx+move832Ext]
        mov dword ptr [eax+move832Sprint], 0x4e20
        jmp move832_20034361
move832_20034267:
        mov eax, dword ptr [edx+move832Fatigue]
        cmp eax, ebx
        je move832_200342ca
        add eax, -0x32
        mov dword ptr [edx+move832Fatigue], eax
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx+move832Ext]
        mov ecx, dword ptr [eax+move832Sprint]
        add ecx, 0xa
        mov dword ptr [eax+move832Sprint], ecx
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx+move832Ext]
        fild dword ptr [ecx+move832Sprint]
        fcomp dword ptr [move832K200ac778]
        fnstsw ax
        test ah, 0x41
        jne move832_200342ac
        mov dword ptr [ecx+move832Sprint], 0x4e20
move832_200342ac:
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        cmp dword ptr [eax+move832Fatigue], ebx
        jge move832_20034361
        mov dword ptr [eax+move832Fatigue], ebx
        jmp move832_20034361
move832_200342ca:
        mov esi, dword ptr [edx+move832Weapon]
        lea eax, [esi + esi*2]
        shl eax, 3
        sub eax, esi
        lea eax, [eax + eax*4]
        mov eax, dword ptr [eax*4 + weaponDef+move832DefWeight]
        sub eax, 3
        je move832_200342fd
        dec eax
        je move832_200342f5
        dec eax
        jne move832_200342fd
        fld dword ptr [move832K200ac774]
        jmp move832_20034303
move832_200342f5:
        fld dword ptr [move832K200ac250]
        jmp move832_20034303
move832_200342fd:
        fld dword ptr [move832K200ac6dc]
move832_20034303:
        mov edx, dword ptr [edx+move832Carried]
        sub edx, 3
        je move832_20034324
        dec edx
        je move832_2003431c
        dec edx
        jne move832_20034324
        fld dword ptr [move832K200ac774]
        jmp move832_2003432a
move832_2003431c:
        fld dword ptr [move832K200ac250]
        jmp move832_2003432a
move832_20034324:
        fld dword ptr [move832K200ac6dc]
move832_2003432a:
        fcom st(1)
        fnstsw ax
        test ah, 0x41
        jne move832_2003433f
        fsub st(0), st(1)
        fmul dword ptr [move832K200ac2ec]
        faddp st(1), st(0)
        jmp move832_20034341
move832_2003433f:
        fstp st(0)
move832_20034341:
        mov esi, dword ptr [ecx+move832Ext]
        fild dword ptr [esi+move832Sprint]
        fld dword ptr [pml+move832Frame]
        fmul st(0), st(2)
        fmul dword ptr [move832K200ac180]
        fsubp st(1), st(0)
        call PM_MovementTruncate827
        fstp st(0)
        mov dword ptr [esi+move832Sprint], eax
move832_20034361:
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx+move832Ext]
        cmp dword ptr [eax+move832Sprint], ebx
        jge move832_20034372
        mov dword ptr [eax+move832Sprint], ebx
move832_20034372:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
move832_2003437a:
        cmp dword ptr [eax+move832Exert], ebx
        jne move832_200344e9
        pop esi
        mov dword ptr [eax+move832Exert], 1
        pop ebx
        add esp, 8
        ret 
move832_20034396:
        mov edx, dword ptr [ecx]
        test byte ptr [edx+move832TcFlags], 0x80
        je move832_200343d7
        mov esi, dword ptr [ecx+move832Ext]
        fild dword ptr [esi+move832Sprint]
        fld dword ptr [pml+move832Frame]
        fmul dword ptr [move832K200ac770]
        fsubp st(1), st(0)
        call PM_MovementTruncate827
        mov dword ptr [esi+move832Sprint], eax
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax+move832Ext]
        cmp dword ptr [eax+move832Sprint], ebx
        jge move832_200343cd
        mov dword ptr [eax+move832Sprint], ebx
move832_200343cd:
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        jmp move832_2003437a
move832_200343d7:
        cmp dword ptr [edx+move832Adrenaline], ebx
        je move832_200343ee
        mov edx, dword ptr [ecx+move832Ext]
        mov dword ptr [edx+move832Sprint], 0x4e20
        jmp move832_200344bb
move832_200343ee:
        cmp dword ptr [edx+move832Fatigue], ebx
        je move832_20034402
        mov ecx, dword ptr [ecx+move832Ext]
        add dword ptr [ecx+move832Sprint], 0xa
        jmp move832_200344bb
move832_20034402:
        mov eax, dword ptr [ecx+move832Leadership]
        mov dword ptr [esp + 8], 0x2ee
        cmp eax, ebx
        je move832_2003441e
        mov dword ptr [esp + 8], 0x3e8
        jmp move832_20034435
move832_2003441e:
        mov eax, dword ptr [ecx+move832Skill]
        push ebp
        mov ebp, dword ptr [eax]
        cmp ebp, 2
        pop ebp
        jl move832_20034435
        mov dword ptr [esp + 8], 0x4b0
move832_20034435:
        fld dword ptr [edx+move832Velocity+4]
        fld dword ptr [edx+move832Velocity]
        fld st(0)
        fmul st(0), st(1)
        fld st(2)
        fmul st(0), st(3)
        faddp st(1), st(0)
        fsqrt 
        fstp st(2)
        fstp st(0)
        fcomp dword ptr [move832K200ac700]
        fnstsw ax
        test ah, 0x41
        jne move832_20034476
        cmp byte ptr [ecx+move832ForwardCmd], bl
        jne move832_20034462
        cmp byte ptr [ecx+move832RightCmd], bl
        je move832_20034476
move832_20034462:
        test byte ptr [edx+move832Flags], 1
        jne move832_20034476
        test dword ptr [edx+move832Eflags], esi
        jne move832_20034476
        test byte ptr [edx+move832WeaponFlags], 4
        je move832_20034494
move832_20034476:
        fild dword ptr [esp + 8]
        mov esi, dword ptr [ecx+move832Ext]
        fmul dword ptr [pml+move832Frame]
        fiadd dword ptr [esi+move832Sprint]
        call PM_MovementTruncate827
        mov dword ptr [esi+move832Sprint], eax
        mov ecx, dword ptr [pm]
move832_20034494:
        mov esi, dword ptr [ecx+move832Ext]
        mov eax, dword ptr [esi+move832Sprint]
        cmp eax, 0x1388
        mov dword ptr [esp + 0xc], eax
        jle move832_200344c1
        fild dword ptr [esp + 8]
        fmul dword ptr [pml+move832Frame]
        fiadd dword ptr [esp + 0xc]
        call PM_MovementTruncate827
        mov dword ptr [esi+move832Sprint], eax
move832_200344bb:
        mov ecx, dword ptr [pm]
move832_200344c1:
        mov edx, dword ptr [ecx+move832Ext]
        fild dword ptr [edx+move832Sprint]
        fcomp dword ptr [move832K200ac778]
        fnstsw ax
        test ah, 0x41
        jne move832_200344e1
        mov dword ptr [edx+move832Sprint], 0x4e20
        mov ecx, dword ptr [pm]
move832_200344e1:
        mov ecx, dword ptr [ecx]
        mov dword ptr [ecx+move832Exert], ebx
move832_200344e9:
        pop esi
        pop ebx
        add esp, 8
        ret 
    }
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
/* TC qagame200344f0: complete Windows controller, native layout, original x87 order.
 * Direct movement/weapon dependencies retain their separate integration boundaries. */
static const float single833K200ac100 = 0.0;
static const double single833K200ac378 = 0.001;
static const float single833K200ac788 = -360.0;
static const float single833K200ac6ec = 255.0;
static const float single833K200ac3ac = 1000.0;
static const float single833K200ac6b8 = 6.2831854820251465;
static const double single833K200ac6a8 = 200.0;
static const double single833K200ac780 = 2000.0;
static const double single833K200ac6b0 = 400.0;
enum {
    single833Ps = offsetof(pmove_t,ps),
    single833Ext = offsetof(pmove_t,pmext),
    single833Character = offsetof(pmove_t,character),
    single833Numtouch = offsetof(pmove_t,numtouch),
    single833WaterType = offsetof(pmove_t,watertype),
    single833WaterLevel = offsetof(pmove_t,waterlevel),
    single833Mask = offsetof(pmove_t,tracemask),
    single833Trace = offsetof(pmove_t,trace),
    single833Cmd = offsetof(pmove_t,cmd),
    single833CmdTime = offsetof(pmove_t,cmd)+offsetof(usercmd_t,serverTime),
    single833Buttons = offsetof(pmove_t,cmd)+offsetof(usercmd_t,buttons),
    single833Wbuttons = offsetof(pmove_t,cmd)+offsetof(usercmd_t,wbuttons),
    single833CmdWeapon = offsetof(pmove_t,cmd)+offsetof(usercmd_t,weapon),
    single833ForwardCmd = offsetof(pmove_t,cmd)+offsetof(usercmd_t,forwardmove),
    single833RightCmd = offsetof(pmove_t,cmd)+offsetof(usercmd_t,rightmove),
    single833UpCmd = offsetof(pmove_t,cmd)+offsetof(usercmd_t,upmove),
    single833Tap = offsetof(pmove_t,cmd)+offsetof(usercmd_t,doubleTap),
    single833CommandTime = offsetof(playerState_t,commandTime),
    single833Type = offsetof(playerState_t,pm_type),
    single833Flags = offsetof(playerState_t,pm_flags),
    single833Origin = offsetof(playerState_t,origin),
    single833Velocity = offsetof(playerState_t,velocity),
    single833Lean = offsetof(playerState_t,leanf),
    single833Ground = offsetof(playerState_t,groundEntityNum),
    single833Eflags = offsetof(playerState_t,eFlags),
    single833Weapon = offsetof(playerState_t,weapon),
    single833WeaponState = offsetof(playerState_t,weaponstate),
    single833ViewAngles = offsetof(playerState_t,viewangles),
    single833ViewHeight = offsetof(playerState_t,viewheight),
    single833Health = offsetof(playerState_t,stats)+STAT_HEALTH*sizeof(int),
    single833Class = offsetof(playerState_t,stats)+STAT_PLAYER_CLASS*sizeof(int),
    single833Seed = offsetof(playerState_t,stats)+STAT_TCE_SHOT_SEED*sizeof(int),
    single833WeaponFlags = offsetof(playerState_t,stats)+STAT_TCE_WEAPON_FLAGS*sizeof(int),
    single833TcFlags = offsetof(playerState_t,stats)+STAT_TCE_FLAGS*sizeof(int),
    single833Instability = offsetof(playerState_t,stats)+STAT_TCE_MOVEMENT_INSTABILITY*sizeof(int),
    single833Phase = offsetof(playerState_t,stats)+STAT_TCE_AIM_PHASE*sizeof(int),
    single833HeavyUse = offsetof(playerState_t,persistant)+PERS_HWEAPON_USE*sizeof(int),
    single833Persist14 = offsetof(playerState_t,persistant)+14*sizeof(int),
    single833Clip28 = offsetof(playerState_t,ammoclip)+28*sizeof(int),
    single833RecoilX = offsetof(playerState_t,holdable)+5*sizeof(int),
    single833RecoilY = offsetof(playerState_t,holdable)+6*sizeof(int),
    single833Spread = offsetof(playerState_t,aimSpreadScaleFloat),
    single833Silenced = offsetof(pmoveExt_t,silencedSideArm),
    single833ProneTime = offsetof(pmoveExt_t,proneTime),
    single833Model = offsetof(bg_character_t,animModelInfo),
    single833DefNoTac = offsetof(tce_weaponDef_t,noTacMode),
    single833PmlDwords = sizeof(pml_t)/sizeof(int),
    single833forward = offsetof(pml_t,forward),
    single833right = offsetof(pml_t,right),
    single833up = offsetof(pml_t,up),
    single833frametime = offsetof(pml_t,frametime),
    single833msec = offsetof(pml_t,msec),
    single833walking = offsetof(pml_t,walking),
    single833previous_origin = offsetof(pml_t,previous_origin),
    single833previous_velocity = offsetof(pml_t,previous_velocity),
    single833previous_waterlevel = offsetof(pml_t,previous_waterlevel),
    single833ladder = offsetof(pml_t,ladder)
};
typedef char single833Layouts[(sizeof(pml_t)==156 && offsetof(pmove_t,ps)==0 && sizeof(tce_weaponDef_t)==460 && offsetof(tce_weaponDef_t,noTacMode)==0x194) ? 1 : -1];
typedef char single833Protocol[(WEAPON_READY==0 && WEAPON_FIRING==7 && BUTTON_ATTACK==1 && BUTTON_TALK==2 && BUTTON_WALKING==16 && BUTTON_SPRINT==32 && PMF_LIMBO==0x4000 && PMF_TIME_LOCKPLAYER==0x8000 && PM_NORMAL==0 && PM_NOCLIP==1 && PM_SPECTATOR==2 && PM_DEAD==3 && PM_FREEZE==4 && PM_INTERMISSION==5 && PMF_RESPAWNED==0x200 && EF_TALK==0x200 && EF_FIRING==0x80 && EF_ZOOMING==0x40000 && EF_PRONE==0x80000 && EF_PRONE_MOVING==0x100000 && EF_MOUNTEDTANK==0x8000 && ANIM_MT_IDLE==1) ? 1 : -1];
__declspec(naked) void PmoveSingle(pmove_t *pmove) {
    __asm {
        push ebx
        push ebp
        mov ebp, dword ptr [esp + 0xc]
        push esi
        push edi
        push ebp
        call BG_AnimUpdatePlayerStateConditions
        mov edi, dword ptr [c_pmove]
        add esp, 4
        inc edi
        xor eax, eax
        mov dword ptr [pm], ebp
        mov dword ptr [c_pmove], edi
        mov dword ptr [ebp+single833Numtouch], eax
        mov ecx, dword ptr [pm]
        mov dword ptr [ecx+single833WaterType], eax
        mov edx, dword ptr [pm]
        mov dword ptr [edx+single833WaterLevel], eax
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        cmp dword ptr [edx+single833Health], eax
        jg single833_20034566
        mov edx, dword ptr [ecx+single833Mask]
        and edx, 0xfdffffff
        mov dword ptr [ecx+single833Mask], edx
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        mov ecx, dword ptr [eax+single833Eflags]
        and ecx, 0xfffbffff
        mov dword ptr [eax+single833Eflags], ecx
        mov ecx, dword ptr [pm]
single833_20034566:
        movsx eax, byte ptr [ecx+single833ForwardCmd]
        cdq 
        xor eax, edx
        sub eax, edx
        cmp eax, 0x40
        jg single833_20034582
        movsx eax, byte ptr [ecx+single833RightCmd]
        cdq 
        xor eax, edx
        sub eax, edx
        cmp eax, 0x40
        jle single833_20034590
single833_20034582:
        mov al, byte ptr [ecx+single833Buttons]
        and al, 0xef
        mov byte ptr [ecx+single833Buttons], al
        mov ecx, dword ptr [pm]
single833_20034590:
        mov al, byte ptr [ecx+single833Buttons]
        mov ecx, dword ptr [ecx]
        test al, 2
        mov eax, dword ptr [ecx+single833Eflags]
        mov edi, 0x200
        je single833_200345a5
        or eax, edi
        jmp single833_200345a8
single833_200345a5:
        and ah, 0xfd
single833_200345a8:
        mov dword ptr [ecx+single833Eflags], eax
        mov ecx, dword ptr [pm]
        mov ebx, 1
        mov eax, dword ptr [ecx]
        mov edx, dword ptr [eax+single833Eflags]
        and edx, 0xfffbff7f
        mov dword ptr [eax+single833Eflags], edx
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        test dword ptr [eax+single833Flags], edi
        jne single833_2003463f
        cmp dword ptr [eax+single833Type], 5
        je single833_2003463f
        mov edx, dword ptr [eax+single833Weapon]
        push edx
        call PM_WeaponAmmoAvailable
        mov edx, dword ptr [pm]
        add esp, 4
        test eax, eax
        je single833_2003463f
        mov ecx, dword ptr [edx]
        mov esi, dword ptr [ecx+single833Eflags]
        test esi, 0x40000
        jne single833_2003463f
        fld dword ptr [ecx+single833Lean]
        fcomp dword ptr [single833K200ac100]
        fnstsw ax
        test ah, 0x40
        jne single833_20034616
        test byte ptr [ecx+single833WeaponFlags], 4
        je single833_2003463f
single833_20034616:
        mov eax, dword ptr [ecx+single833WeaponState]
        test eax, eax
        je single833_20034625
        cmp eax, 7
        jne single833_2003463f
single833_20034625:
        mov al, byte ptr [edx+single833Buttons]
        test al, 1
        je single833_2003463f
        test al, 2
        jne single833_2003463f
        or esi, 0x80
        mov dword ptr [ecx+single833Eflags], esi
        mov edx, dword ptr [pm]
single833_2003463f:
        mov eax, dword ptr [edx]
        test dword ptr [eax+single833Flags], edi
        je single833_20034660
        cmp dword ptr [eax+single833Class], 4
        jne single833_20034660
        mov edx, dword ptr [edx+single833Ext]
        mov eax, dword ptr [edx+single833Silenced]
        or eax, ebx
        mov dword ptr [edx+single833Silenced], eax
        mov edx, dword ptr [pm]
single833_20034660:
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+single833Health]
        test ecx, ecx
        jle single833_20034681
        test byte ptr [edx+single833Buttons], 1
        jne single833_20034681
        mov ecx, dword ptr [eax+single833Flags]
        and ch, 0xfd
        mov dword ptr [eax+single833Flags], ecx
        mov edx, dword ptr [pm]
single833_20034681:
        test byte ptr [ebp+single833Buttons], 2
        je single833_200346a5
        mov byte ptr [ebp+single833Buttons], 2
        mov byte ptr [ebp+single833Wbuttons], 0
        mov byte ptr [ebp+single833ForwardCmd], 0
        mov byte ptr [ebp+single833RightCmd], 0
        mov byte ptr [ebp+single833UpCmd], 0
        mov byte ptr [ebp+single833Tap], 0
        mov edx, dword ptr [pm]
single833_200346a5:
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+single833HeavyUse]
        test ecx, ecx
        je single833_200346c3
        mov byte ptr [ebp+single833ForwardCmd], 0
        mov byte ptr [ebp+single833RightCmd], 0
        mov byte ptr [ebp+single833UpCmd], 0
        mov edx, dword ptr [pm]
single833_200346c3:
        mov ecx, single833PmlDwords
        xor eax, eax
        mov edi, offset pml
        rep stosd 
        mov ecx, dword ptr [edx]
        mov eax, dword ptr [ebp+single833CmdTime]
        sub eax, dword ptr [ecx+single833CommandTime]
        cmp eax, ebx
        mov dword ptr [pml+single833msec], eax
        jge single833_200346e9
        mov dword ptr [pml+single833msec], ebx
        jmp single833_200346fa
single833_200346e9:
        cmp eax, 0xc8
        jle single833_200346fa
        mov dword ptr [pml+single833msec], 0xc8
single833_200346fa:
        mov edx, dword ptr [edx]
        mov eax, dword ptr [ebp+single833CmdTime]
        mov dword ptr [edx+single833CommandTime], eax
        mov eax, dword ptr [pm]
        fild dword ptr [pml+single833msec]
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [ecx+single833Origin]
        mov dword ptr [pml+single833previous_origin], edx
        mov ecx, dword ptr [eax]
        fmul qword ptr [single833K200ac378]
        mov edx, dword ptr [ecx+single833Origin+4]
        mov dword ptr [pml+single833previous_origin+4], edx
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [ecx+single833Origin+8]
        mov dword ptr [pml+single833previous_origin+8], edx
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [ecx+single833Velocity]
        mov dword ptr [pml+single833previous_velocity], edx
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [ecx+single833Velocity+4]
        mov dword ptr [pml+single833previous_velocity+4], edx
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [ecx+single833Velocity+8]
        fstp dword ptr [pml+single833frametime]
        mov dword ptr [pml+single833previous_velocity+8], edx
        mov ecx, dword ptr [eax]
        cmp dword ptr [ecx+single833Type], 4
        je single833_2003478b
        mov edx, dword ptr [ecx+single833Flags]
        test dh, 0x40
        jne single833_2003478b
        mov edx, dword ptr [eax+single833Mask]
        push edx
        mov edx, dword ptr [eax+single833Trace]
        push edx
        lea edx, [eax+single833Cmd]
        mov eax, dword ptr [eax+single833Ext]
        push edx
        push eax
        push ecx
        call PM_UpdateViewAngles
        mov eax, dword ptr [pm]
        add esp, 0x14
single833_2003478b:
        mov ecx, dword ptr [eax]
        push offset pml+single833up
        push offset pml+single833right
        add ecx, single833ViewAngles
        push offset pml+single833forward
        push ecx
        call AngleVectors
        mov eax, dword ptr [pm]
        add esp, 0x10
        cmp byte ptr [eax+single833UpCmd], 0xa
        jge single833_200347c6
        mov eax, dword ptr [eax]
        mov ecx, dword ptr [eax+single833Flags]
        and ecx, 0xfffffffd
        mov dword ptr [eax+single833Flags], ecx
        mov eax, dword ptr [pm]
single833_200347c6:
        mov cl, byte ptr [eax+single833ForwardCmd]
        mov ebx, 0x10
        test cl, cl
        jge single833_200347db
        mov eax, dword ptr [eax]
        mov ecx, dword ptr [eax+single833Flags]
        or ecx, ebx
        jmp single833_200347f0
single833_200347db:
        jg single833_200347e8
        test cl, cl
        jne single833_200347f8
        mov cl, byte ptr [eax+single833RightCmd]
        test cl, cl
        je single833_200347f8
single833_200347e8:
        mov eax, dword ptr [eax]
        mov ecx, dword ptr [eax+single833Flags]
        and ecx, 0xffffffef
single833_200347f0:
        mov dword ptr [eax+single833Flags], ecx
        mov eax, dword ptr [pm]
single833_200347f8:
        mov ecx, dword ptr [eax]
        cmp dword ptr [ecx+single833Type], 3
        jge single833_20034808
        mov edx, dword ptr [ecx+single833Flags]
        test dh, 0xc0
        je single833_20034824
single833_20034808:
        mov byte ptr [eax+single833ForwardCmd], 0
        mov edx, dword ptr [pm]
        mov byte ptr [edx+single833RightCmd], 0
        mov eax, dword ptr [pm]
        mov byte ptr [eax+single833UpCmd], 0
        mov eax, dword ptr [pm]
single833_20034824:
        mov edx, dword ptr [eax]
        mov ecx, dword ptr [edx+single833Type]
        cmp ecx, 2
        jne single833_20034842
        call PM_CheckDuck
        call PM_FlyMove
        call PM_DropTimers
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret 
single833_20034842:
        cmp ecx, 1
        jne single833_20034856
        call PM_NoclipMove
        call PM_DropTimers
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret 
single833_20034856:
        cmp ecx, 4
        je single833_20034ce8
        test byte ptr [edx+single833TcFlags], bl
        je single833_200348c0
        mov byte ptr [eax+single833ForwardCmd], 0
        mov ecx, dword ptr [pm]
        xor edi, edi
        mov byte ptr [ecx+single833RightCmd], 0
        mov edx, dword ptr [pm]
        mov byte ptr [edx+single833UpCmd], 0
        mov eax, dword ptr [pm]
        mov byte ptr [eax+single833Buttons], 0
        mov ecx, dword ptr [pm]
        mov byte ptr [ecx+single833Wbuttons], 0
        mov eax, dword ptr [pm]
        mov edx, dword ptr [eax]
        mov cl, byte ptr [edx+single833Weapon]
        mov byte ptr [eax+single833CmdWeapon], cl
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov dword ptr [eax+single833Lean], edi
        mov ecx, dword ptr [pm]
        mov byte ptr [ecx+single833Tap], 0
        mov eax, dword ptr [pm]
        jmp single833_200348c2
single833_200348c0:
        xor edi, edi
single833_200348c2:
        mov ecx, dword ptr [eax]
        mov edx, dword ptr [ecx+single833Type]
        cmp edx, 5
        je single833_20034ce8
        mov ebx, dword ptr [ecx+single833Weapon]
        mov esi, 0x3c
        cmp ebx, esi
        jne single833_200348fa
        cmp edx, edi
        jne single833_200348fa
        mov byte ptr [eax+single833ForwardCmd], 0
        mov edx, dword ptr [pm]
        mov byte ptr [edx+single833RightCmd], 0
        mov eax, dword ptr [pm]
        mov byte ptr [eax+single833UpCmd], 0
single833_200348fa:
        call PM_SetWaterLevel
        mov ecx, dword ptr [ebp+single833WaterLevel]
        mov dword ptr [pml+single833previous_waterlevel], ecx
        call PM_CheckProne
        test eax, eax
        jne single833_20034919
        call PM_CheckDuck
single833_20034919:
        call PM_GroundTrace
        mov edx, dword ptr [pm]
        mov ebp, 3
        mov ebx, 0x80000
        mov eax, dword ptr [edx]
        cmp dword ptr [eax+single833Type], ebp
        jne single833_20034955
        call PM_DeadMove
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        cmp dword ptr [eax+single833Weapon], esi
        jne single833_20034997
        mov dword ptr [eax+single833Weapon], 0x23
        jmp single833_20034997
single833_20034955:
        cmp dword ptr [eax+single833Weapon], 0x3e
        jne single833_20034970
        test dword ptr [eax+single833Eflags], ebx
        jne single833_20034970
        push edi
        push 0x1f
        push 0x3e
        call PM_BeginWeaponChange
        add esp, 0xc
single833_20034970:
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        cmp dword ptr [eax+single833Weapon], 0x1c
        jne single833_20034997
        cmp dword ptr [eax+single833Clip28], edi
        jne single833_20034997
        push 1
        push 0x1b
        push 0x1c
        call PM_BeginWeaponChange
        add esp, 0xc
single833_20034997:
        call PM_CheckLadderMove
        call PM_DropTimers
        mov eax, dword ptr [pml+single833ladder]
        mov esi, 0x8000
        cmp eax, edi
        je single833_200349b6
        call PM_LadderMove
        jmp single833_200349fe
single833_200349b6:
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        mov edx, dword ptr [eax+single833Flags]
        test dh, 1
        je single833_200349cd
        call PM_WaterJumpMove
        jmp single833_200349fe
single833_200349cd:
        cmp dword ptr [ecx+single833WaterLevel], 1
        jle single833_200349dd
        call PM_WaterMove
        jmp single833_200349fe
single833_200349dd:
        mov ecx, dword ptr [pml+single833walking]
        cmp ecx, edi
        mov ecx, dword ptr [eax+single833Eflags]
        je single833_200349f5
        test esi, ecx
        jne single833_200349fe
        call PM_WalkMove
        jmp single833_200349fe
single833_200349f5:
        test esi, ecx
        jne single833_200349fe
        call PM_AirMove
single833_200349fe:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        test dword ptr [eax+single833Eflags], esi
        je single833_20034a50
        mov dword ptr [eax+single833Velocity+8], edi
        mov eax, dword ptr [pm]
        push 1
        push 1
        mov ecx, dword ptr [eax]
        mov dword ptr [ecx+single833Velocity+4], edi
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov dword ptr [eax+single833Velocity], edi
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+single833ViewHeight], 0x2a
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax+single833Character]
        mov eax, dword ptr [eax]
        mov edx, dword ptr [ecx+single833Model]
        push edx
        push eax
        call BG_AnimScriptAnimation
        add esp, 0x10
single833_20034a50:
        call PM_Sprint
        call PM_GroundTrace
        call PM_SetWaterLevel
        call PM_Weapon
        mov esi, dword ptr [pm]
        mov ecx, dword ptr [esi]
        mov edx, dword ptr [ecx+single833WeaponFlags]
        test dl, 4
        je single833_20034ccb
        mov eax, dword ptr [ecx+single833WeaponState]
        cmp eax, ebp
        je single833_20034b3a
        cmp eax, 1
        jne single833_20034a97
        test dl, 8
        je single833_20034b3a
single833_20034a97:
        cmp eax, 9
        je single833_20034b3a
        cmp eax, 0xa
        je single833_20034b3a
        cmp eax, 0xb
        je single833_20034b3a
        mov eax, dword ptr [esi+single833WaterLevel]
        cmp eax, 2
        jg single833_20034b3a
        cmp eax, edi
        jle single833_20034aca
        cmp dword ptr [ecx+single833Ground], 0x3ff
        je single833_20034b3a
single833_20034aca:
        fld dword ptr [ecx+single833Velocity+8]
        fcomp dword ptr [single833K200ac788]
        fnstsw ax
        test ah, 1
        jne single833_20034b3a
        test byte ptr [esi+single833Buttons], 0x20
        je single833_20034af9
        mov al, byte ptr [esi+single833ForwardCmd]
        test al, al
        jne single833_20034aee
        mov al, byte ptr [esi+single833RightCmd]
        test al, al
        je single833_20034af9
single833_20034aee:
        test byte ptr [ecx+single833Flags], 1
        jne single833_20034af9
        test dword ptr [ecx+single833Eflags], ebx
        je single833_20034b3a
single833_20034af9:
        cmp dword ptr [pml+single833ladder], edi
        jne single833_20034b3a
        cmp dword ptr [ecx+single833Health], edi
        jle single833_20034b3a
        test dh, 0x10
        jne single833_20034b3a
        mov eax, dword ptr [esi+single833CmdTime]
        mov esi, dword ptr [esi+single833Ext]
        mov ebx, eax
        mov esi, dword ptr [esi+single833ProneTime]
        sub ebx, esi
        cmp ebx, 0xc8
        jl single833_20034b3a
        add esi, eax
        cmp esi, 0xc8
        jl single833_20034b3a
        test dword ptr [ecx+single833Eflags], 0x100000
        je single833_20034ccb
single833_20034b3a:
        and edx, 0xfffffffb
        mov dword ptr [ecx+single833WeaponFlags], edx
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx]
        cmp dword ptr [eax+single833WeaponState], ebp
        jne single833_20034b67
        mov ecx, dword ptr [eax+single833WeaponFlags]
        test cl, 8
        je single833_20034b67
        and ecx, 0xfffffff7
        mov dword ptr [eax+single833WeaponFlags], ecx
single833_20034b67:
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx]
        mov eax, dword ptr [ecx+single833WeaponFlags]
        test al, 8
        je single833_20034b8a
        test byte ptr [ecx+single833Persist14], 8
        jne single833_20034b8a
        and al, 0xf7
        mov dword ptr [ecx+single833WeaponFlags], eax
single833_20034b8a:
        mov eax, dword ptr [pm]
        mov eax, dword ptr [eax]
        fld dword ptr [eax+single833Spread]
        fadd dword ptr [single833K200ac6ec]
        fstp dword ptr [eax+single833Spread]
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+single833Instability], 0x3e8
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        cmp dword ptr [ecx+single833WeaponState], 7
        je single833_20034cb9
        mov edx, ecx
        lea ecx, [esp + 0x14]
        push ecx
        mov eax, dword ptr [edx+single833Seed]
        mov dword ptr [esp + 0x18], eax
        call Q_random
        fmul dword ptr [single833K200ac3ac]
        add esp, 4
        call PM_MovementTruncate827
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx]
        mov dword ptr [ecx+single833Phase], eax
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx]
        mov ecx, dword ptr [eax+single833Phase]
        cmp ecx, 0x3e8
        jle single833_20034c1d
        add ecx, 0xfffffc18
        mov dword ptr [eax+single833Phase], ecx
single833_20034c1d:
        mov eax, dword ptr [pm]
        mov edx, dword ptr [eax]
        mov ecx, dword ptr [edx+single833Weapon]
        lea eax, [ecx + ecx*2]
        shl eax, 3
        sub eax, ecx
        lea ecx, [eax + eax*4]
        cmp dword ptr [ecx*4 + weaponDef+single833DefNoTac], edi
        jne single833_20034c9e
        test byte ptr [edx+single833WeaponFlags], 4
        je single833_20034c9e
        lea edx, [esp + 0x14]
        push edx
        call Q_random
        fmul dword ptr [single833K200ac6b8]
        add esp, 4
        fld st(0)
        fcos 
        fmul qword ptr [single833K200ac6a8]
        fadd qword ptr [single833K200ac780]
        call PM_MovementTruncate827
        fsin 
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+single833RecoilX], eax
        fmul qword ptr [single833K200ac6b0]
        fadd qword ptr [single833K200ac780]
        call PM_MovementTruncate827
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+single833RecoilY], eax
single833_20034c9e:
        mov eax, dword ptr [esp + 0x14]
        mov ecx, dword ptr [pm]
        and eax, 0xffff
        mov dword ptr [esp + 0x14], eax
        mov edx, dword ptr [ecx]
        mov dword ptr [edx+single833Seed], eax
single833_20034cb9:
        push EV_TCE_TOGGLE_AIMING
        call PM_AddEvent
        add esp, 4
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret 
single833_20034ccb:
        call PM_Footsteps
        call PM_WaterEvents
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax]
        add ecx, single833Velocity
        push ecx
        call trap_SnapVector
        add esp, 4
single833_20034ce8:
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret 
    }
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
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC qagame2003b3a0: exact outer timing, wrap arithmetic and call order. */
enum {
    outerPs834 = offsetof(pmove_t,ps),
    outerCommand834 = offsetof(pmove_t,cmd)+offsetof(usercmd_t,serverTime),
    outerUp834 = offsetof(pmove_t,cmd)+offsetof(usercmd_t,upmove),
    outerFixed834 = offsetof(pmove_t,pmove_fixed),
    outerMsec834 = offsetof(pmove_t,pmove_msec),
    outerTime834 = offsetof(playerState_t,commandTime),
    outerFlags834 = offsetof(playerState_t,pm_flags),
    outerFrame834 = offsetof(playerState_t,pmove_framecount),
    outerHeat834 = offsetof(playerState_t,curWeapHeat)
};
__declspec(naked) int Pmove(pmove_t *pmove) {
    __asm {

        PUSH ESI

        MOV ESI,dword ptr [ESP + 0x8]

        PUSH EDI

        MOV ECX,dword ptr [ESI + outerPs834]

        MOV EDI,dword ptr [ESI + outerCommand834]

        MOV EAX,dword ptr [ECX + outerTime834]

        CMP EDI,EAX

        JL outerAt2003b477

        ADD EAX,0x3e8

        CMP EDI,EAX

        JLE outerAt2003b3c6

        LEA EAX,[EDI - 1000]

        MOV dword ptr [ECX + outerTime834],EAX
    outerAt2003b3c6:
        MOV EAX,dword ptr [ESI + outerPs834]

        MOV ECX,dword ptr [EAX + outerFlags834]

        TEST CH,0x20

        JZ outerAt2003b3e0

        MOV EDX,dword ptr [EAX + outerTime834]

        MOV ECX,EDI

        SUB ECX,EDX

        CMP ECX,0x32

        JLE outerAt2003b3e0

        LEA EDX,[EDI + -0x32]

        MOV dword ptr [EAX + outerTime834],EDX
    outerAt2003b3e0:
        MOV EAX,dword ptr [ESI + outerPs834]

        MOV ECX,dword ptr [EAX + outerFrame834]

        INC ECX

        AND ECX,0x3f

        MOV dword ptr [EAX + outerFrame834],ECX

        MOV dword ptr [pm],ESI

        CALL PM_AdjustAimSpreadScale

        MOV EDX,dword ptr [ESI + outerPs834]

        MOV ECX,dword ptr [EDX + outerTime834]

        CMP ECX,EDI

        JZ outerAt2003b44b
    outerAt2003b405:
        MOV EDX,dword ptr [ESI + outerFixed834]

        MOV EAX,EDI

        SUB EAX,ECX

        TEST EDX,EDX

        JZ outerAt2003b421

        MOV EDX,dword ptr [ESI + outerMsec834]

        CMP EAX,EDX

        JLE outerAt2003b42b

        MOV EAX,EDX

        JMP outerAt2003b42b
    outerAt2003b421:
        CMP EAX,0x32

        JLE outerAt2003b42b

        MOV EAX,0x32
    outerAt2003b42b:
        ADD ECX,EAX

        PUSH ESI

        MOV dword ptr [ESI + outerCommand834],ECX

        CALL PmoveSingle

        MOV EAX,dword ptr [ESI + outerPs834]

        ADD ESP,0x4

        TEST byte ptr [EAX + outerFlags834],0x2

        JZ outerAt2003b445

        MOV byte ptr [ESI + outerUp834],0x14
    outerAt2003b445:
        MOV ECX,dword ptr [EAX + outerTime834]

        CMP ECX,EDI

        JNZ outerAt2003b405
    outerAt2003b44b:
        MOV ESI,dword ptr [ESI + outerPs834]

        MOV EAX,dword ptr [ESI + outerHeat834]

        CMP EAX,0xff

        JLE outerAt2003b469

        MOV dword ptr [ESI + outerHeat834],0xff

        POP EDI

        XOR EAX,EAX

        POP ESI

        RET
    outerAt2003b469:
        TEST EAX,EAX

        JGE outerAt2003b477

        MOV dword ptr [ESI + outerHeat834],0x0
    outerAt2003b477:
        POP EDI

        XOR EAX,EAX

        POP ESI

        RET
    }
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
