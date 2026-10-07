

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

float	pm_proneSpeedScale	= 0.21;	// was: 0.18 (too slow) then: 0.24 (too fast)

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
	vec3_t flatforward;
	float angle;
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
/* TC:E Windows3000de50 / Linux000e65c2: momentum/stamina-scaled jump. */
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

static qboolean PM_CheckProne (void)
{
    if(gearDef.parsed)return PM_TCECheckProne();
	//Com_Printf( "%i: PM_CheckProne (%i)\n", pm->cmd.serverTime, pm->pmext->proneGroundTime );

	if( !(pm->ps->eFlags & EF_PRONE) ) {
		// needs to be on the ground
//		if( !pml.walking ) {
//			return qfalse;
//		}

		// can't go prone on ladders
		if( pm->ps->pm_flags & PMF_LADDER ) {
			return qfalse;
		}

		// no prone when using mg42's
		if( pm->ps->persistant[PERS_HWEAPON_USE] || pm->ps->eFlags & EF_MOUNTEDTANK ) {
			return qfalse;
		}

		if( pm->ps->weaponDelay && pm->ps->weapon == (gearDef.parsed ? 65 : WP_PANZERFAUST) ) {
			return qfalse;
		}

		if( pm->ps->weapon == (gearDef.parsed ? 60 : WP_MORTAR_SET) ) {
			return qfalse;
		}

		// can't go prone while swimming
		if( pm->waterlevel > 1 ) {
			return qfalse;
		}

		// can't go prone when fiddling with mg42
		//if( pm->ps->weaponstate == WEAPON_FROMPRONE ) {
		//	return qfalse;
		//}

		if( ((pm->ps->pm_flags & PMF_DUCKED && pm->cmd.doubleTap == DT_FORWARD) ||
			(pm->cmd.wbuttons & WBUTTON_PRONE)) && pm->cmd.serverTime - -pm->pmext->proneTime > 750  ) {
			trace_t trace;

			pm->mins[0] = pm->ps->mins[0];
			pm->mins[1] = pm->ps->mins[1];

			pm->maxs[0] = pm->ps->maxs[0];
			pm->maxs[1] = pm->ps->maxs[1];

			pm->mins[2] = pm->ps->mins[2];
			pm->maxs[2] = pm->ps->crouchMaxZ;

			pm->ps->eFlags |= EF_PRONE;
			PM_TraceAll( &trace, pm->ps->origin, pm->ps->origin );
			pm->ps->eFlags &= ~EF_PRONE;

			if ( !trace.startsolid && !trace.allsolid ) {
				// go prone
				pm->ps->pm_flags |= PMF_DUCKED;	// crouched as well
				pm->ps->eFlags |= EF_PRONE;
				pm->pmext->proneTime = pm->cmd.serverTime;	// timestamp 'go prone'
				pm->pmext->proneGroundTime = pm->cmd.serverTime;
			}
		}
	}

	if( pm->ps->eFlags & EF_PRONE ) {
		if( pm->waterlevel > 1 ||
			pm->ps->pm_type == PM_DEAD ||
			pm->ps->eFlags & EF_MOUNTEDTANK ||
// zinx - what was the reason for this, anyway? removing fixes bug 424
//			pm->cmd.serverTime - pm->pmext->proneGroundTime > 450 ||
			((pm->cmd.doubleTap == DT_BACK || pm->cmd.upmove > 10 || pm->cmd.wbuttons & WBUTTON_PRONE) && pm->cmd.serverTime - pm->pmext->proneTime > 750) ) {
			trace_t trace;

			// see if we have the space to stop prone
			pm->mins[0] = pm->ps->mins[0];
			pm->mins[1] = pm->ps->mins[1];

			pm->maxs[0] = pm->ps->maxs[0];
			pm->maxs[1] = pm->ps->maxs[1];

			pm->mins[2] = pm->ps->mins[2];
			pm->maxs[2] = pm->ps->crouchMaxZ;

			pm->ps->eFlags &= ~EF_PRONE;
			PM_TraceAll( &trace, pm->ps->origin, pm->ps->origin );
			pm->ps->eFlags |= EF_PRONE;
			
			if( !trace.allsolid ) {
				// crouch for a bit
				pm->ps->pm_flags |= PMF_DUCKED;

				// stop prone
				pm->ps->eFlags &= ~EF_PRONE;
				pm->ps->eFlags &= ~EF_PRONE_MOVING;
				pm->pmext->proneTime = -pm->cmd.serverTime;	// timestamp 'stop prone'

				if( pm->ps->weapon == WP_MOBILE_MG42_SET ) {
					PM_BeginWeaponChange( WP_MOBILE_MG42_SET, WP_MOBILE_MG42, qfalse );
				}

				// don't jump for a bit
				pm->pmext->jumpTime = pm->cmd.serverTime - 650;
				pm->ps->jumpTime = pm->cmd.serverTime - 650;
			}
		}
	}

	if( pm->ps->eFlags & EF_PRONE ) {
		//float frac;
		
		// See if we are moving
		float spd = VectorLength( pm->ps->velocity );
		qboolean userinput = abs(pm->cmd.forwardmove) + abs(pm->cmd.rightmove) > 10 ? qtrue : qfalse;

		if( userinput && spd > 40.f && !(pm->ps->eFlags & EF_PRONE_MOVING) ) {
			pm->ps->eFlags |= EF_PRONE_MOVING;

			switch( pm->ps->weapon ) {
				case WP_FG42SCOPE: PM_BeginWeaponChange( WP_FG42SCOPE, WP_FG42, qfalse ); break;
				case WP_GARAND_SCOPE: PM_BeginWeaponChange( WP_GARAND_SCOPE, WP_GARAND, qfalse ); break;
				case WP_K43_SCOPE: PM_BeginWeaponChange( WP_K43_SCOPE, WP_K43, qfalse ); break;
			}
		} else if( !userinput && spd < 20.0f && (pm->ps->eFlags & EF_PRONE_MOVING) ) {
			pm->ps->eFlags &= ~EF_PRONE_MOVING;
		}

		pm->mins[0] = pm->ps->mins[0];
		pm->mins[1] = pm->ps->mins[1];

		pm->maxs[0] = pm->ps->maxs[0];
		pm->maxs[1] = pm->ps->maxs[1];

		pm->mins[2] = pm->ps->mins[2];

		//frac = (pm->cmd.serverTime - pm->pmext->proneTime) / 500.f;
		//if( frac > 1.f )
		//	frac = 1.f;

		//pm->maxs[2] = pm->ps->maxs[2] - (frac * (pm->ps->standViewHeight - PRONE_VIEWHEIGHT));
		//pm->ps->viewheight = DEFAULT_VIEWHEIGHT - (frac * (DEFAULT_VIEWHEIGHT - PRONE_VIEWHEIGHT));	// default - prone to get a positive which is subtracted from default
		pm->maxs[2] = pm->ps->maxs[2] - pm->ps->standViewHeight - PRONE_VIEWHEIGHT;
		pm->ps->viewheight = PRONE_VIEWHEIGHT;

		return( qtrue );
	}

	return( qfalse );
}

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
/* Whole TC3000e9c0: steep landings suppress the small landing sounds. */
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

/*
=============
PM_CorrectAllSolid
=============
*/
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


/*
=============
PM_GroundTraceMissed

The ground trace didn't hit a surface, so we are in freefall
=============
*/
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


/*
=============
PM_GroundTrace
=============
*/
/* Whole TC3000e490. Direct original capsule trace, including content and
 * prone ground-extension paths absent from the SDK ground controller. */
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

/*
=============
PM_SetWaterLevel	FIXME: avoid this twice?  certainly if not moving
=============
*/
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

static void PM_CheckDuck (void)
{
	trace_t	trace;
    if (gearDef.parsed) {
        PM_TCECheckDuck();
        return;
    }

	// Ridah, modified this for configurable bounding boxes
	pm->mins[0] = pm->ps->mins[0];
	pm->mins[1] = pm->ps->mins[1];

	pm->maxs[0] = pm->ps->maxs[0];
	pm->maxs[1] = pm->ps->maxs[1];

	pm->mins[2] = pm->ps->mins[2];

	if( pm->ps->pm_type == PM_DEAD ) {
		pm->maxs[2] = pm->ps->maxs[2];			// NOTE: must set death bounding box in game code
		pm->ps->viewheight = pm->ps->deadViewHeight;
		return;
	}

	if( (pm->cmd.upmove < 0 && !(pm->ps->eFlags & EF_MOUNTEDTANK) && !(pm->ps->pm_flags & PMF_LADDER) ) || pm->ps->weapon == WP_MORTAR_SET )
	{	// duck
		pm->ps->pm_flags |= PMF_DUCKED;
	}
	else
	{	// stand up if possible
		if (pm->ps->pm_flags & PMF_DUCKED)
		{
			// try to stand up
			pm->maxs[2] = pm->ps->maxs[2];
			PM_TraceAll( &trace, pm->ps->origin, pm->ps->origin );
			if (!trace.allsolid)
				pm->ps->pm_flags &= ~PMF_DUCKED;
		}
	}

	if (pm->ps->pm_flags & PMF_DUCKED)
	{
		pm->maxs[2] = pm->ps->crouchMaxZ;
		pm->ps->viewheight = pm->ps->crouchViewHeight;
	}
	else
	{
		pm->maxs[2] = pm->ps->maxs[2];
		pm->ps->viewheight = pm->ps->standViewHeight;
	}
	// done.
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

/* Whole TC:E cgame3000f3c0 / Linux000e1998, shared prediction/server. */
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

/* Whole TC30012e70, including single-round and state12 exclusions. */
static void PM_FinishWeaponReload(void) {
    tce_reloadState_t state;
    tce_reloadEffects_t effects;
    int weapon=pm->ps->weapon;
    if (weapon < 0 || weapon >= TCE_MAX_WEAPONS) return;
    memset(&state,0,sizeof(state));
    state.state=pm->ps->weaponstate; state.idleAnimation=PM_IdleAnimForWeapon(weapon);
    TCE_PM_FinishWeaponReload(weapon,&state,weaponDef,pm->ps->ammo,pm->ps->ammoclip,&effects);
    pm->ps->weaponstate=state.state;
    PM_StartWeaponAnim(effects.weaponAnimation);
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

/* TC:E Windows30009af0 / Linux000db70e. No SDK-ID-indexed ammo table. */
void PM_CoolWeapons(void) {
    int weapon, maxHeat;
    for (weapon = 0; weapon < 64; ++weapon) {
        if (COM_BitCheck(pm->ps->weapons, weapon) && pm->ps->weapHeat[weapon]) {
            float cooling = PM_TCECoolRate(weapon) * pml.frametime;
            if (pm->skill[SK_HEAVY_WEAPONS] >= 2 &&
                pm->ps->stats[STAT_PLAYER_CLASS] == PC_SOLDIER) cooling += cooling;
            pm->ps->weapHeat[weapon] = (int)(pm->ps->weapHeat[weapon] - cooling);
            if (pm->ps->weapHeat[weapon] < 0) pm->ps->weapHeat[weapon] = 0;
        }
    }
    if (!pm->ps->weapon) return;
    if (pm->ps->persistant[PERS_HWEAPON_USE] || (pm->ps->eFlags & EF_MOUNTEDTANK)) {
        pm->ps->curWeapHeat = (int)floor((pm->ps->weapHeat[34] * (1.0f / 1500.0f)) * 255.0f);
    } else {
        maxHeat = PM_TCEMaxHeat(pm->ps->weapon);
        if (!maxHeat) { pm->ps->curWeapHeat = 0; return; }
        pm->ps->curWeapHeat = (int)floor(((float)pm->ps->weapHeat[pm->ps->weapon] / maxHeat) * 255.0f);
    }
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

void PM_AdjustAimSpreadScale( void ) {
//	int		increase, decrease, i;
	int		i;
	float	increase, decrease;		// (SA) was losing lots of precision on slower weapons (scoped)
	float	viewchange, cmdTime, wpnScale;

    if (gearDef.parsed && pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS &&
        weaponDef[pm->ps->weapon].parsed) {
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
        return;
    }

	// all weapons are very inaccurate in zoomed mode
	if(pm->ps->eFlags & EF_ZOOMING) {
		pm->ps->aimSpreadScale = 255;
		pm->ps->aimSpreadScaleFloat = 255;
		return;
	}

	cmdTime = (float)(pm->cmd.serverTime - pm->oldcmd.serverTime) / 1000.0;

	wpnScale = 0.0f;
	switch(pm->ps->weapon) {
	case WP_LUGER:
	case WP_SILENCER:
	case WP_AKIMBO_LUGER:
	case WP_AKIMBO_SILENCEDLUGER:
// rain - luger and akimbo are supposed to be balanced
//		wpnScale = 0.5f;
//		break;
	case WP_COLT:
	case WP_SILENCED_COLT:
	case WP_AKIMBO_COLT:
	case WP_AKIMBO_SILENCEDCOLT:
		wpnScale = 0.4f;		// doesn't fire as fast, but easier to handle than luger
		break;
	case WP_MP40:
		wpnScale = 0.6f;		// 2 handed, but not as long as mauser, so harder to keep aim
		break;
	case WP_GARAND:
		wpnScale = 0.5f;
		break;
	case WP_K43_SCOPE:
	case WP_GARAND_SCOPE:
	case WP_FG42SCOPE:
		if( pm->skill[SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS] >= 3 ) {
			wpnScale = 5.f;
		} else {
			wpnScale = 10.f;
		}
		break;
	case WP_K43:
		wpnScale = 0.5f;
		break;
	case WP_MOBILE_MG42:
	case WP_MOBILE_MG42_SET:
		wpnScale = 0.9f;
		break;
	case WP_FG42:
		wpnScale = 0.6f;
		break;
	case WP_THOMPSON:
		wpnScale = 0.6f;
		break;
	case WP_STEN:
		wpnScale = 0.6f;
		break;
	case WP_KAR98:
	case WP_CARBINE:
		wpnScale = 0.5f;
		break;
	}

	if (wpnScale) {

		// JPW NERVE crouched players recover faster (mostly useful for snipers)
		if( pm->ps->eFlags & EF_CROUCHING || pm->ps->eFlags & EF_PRONE ) {
			wpnScale *= 0.5;
		}

		decrease = (cmdTime * AIMSPREAD_DECREASE_RATE) / wpnScale;

		viewchange = 0;
		// take player movement into account (even if only for the scoped weapons)
		// TODO: also check for jump/crouch and adjust accordingly
		if(BG_IsScopedWeapon(pm->ps->weapon)) {
			for(i = 0; i < 2; i++) {
				viewchange += fabs(pm->ps->velocity[i]);
			}
		} else {
			// take player view rotation into account
			for( i = 0; i < 2; i++ ) {
				viewchange += fabs( SHORT2ANGLE(pm->cmd.angles[i]) - SHORT2ANGLE(pm->oldcmd.angles[i]) );
			}
		}

		viewchange = (float)viewchange / cmdTime;	// convert into this movement for a second
		viewchange -= AIMSPREAD_VIEWRATE_MIN / wpnScale;
		if (viewchange <= 0) {
			viewchange = 0;
		} else if( viewchange > (AIMSPREAD_VIEWRATE_RANGE / wpnScale) ) {
			viewchange = AIMSPREAD_VIEWRATE_RANGE / wpnScale;
		}

		// now give us a scale from 0.0 to 1.0 to apply the spread increase
		viewchange = viewchange / (float)(AIMSPREAD_VIEWRATE_RANGE / wpnScale);

		increase = (int)(cmdTime * viewchange * AIMSPREAD_INCREASE_RATE);
	} else {
		increase = 0;
		decrease = AIMSPREAD_DECREASE_RATE;
	}

	// update the aimSpreadScale
	pm->ps->aimSpreadScaleFloat += (increase - decrease);
	if (pm->ps->aimSpreadScaleFloat < 0) pm->ps->aimSpreadScaleFloat = 0;
	if (pm->ps->aimSpreadScaleFloat > 255) pm->ps->aimSpreadScaleFloat = 255;

	pm->ps->aimSpreadScale = (int)pm->ps->aimSpreadScaleFloat;	// update the int for the client
}

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

static void PM_TCEShotRecoil(int interval, unsigned int seed) {
    const tce_weaponDef_t *w = &weaponDef[pm->ps->weapon];
    int oldInstability = pm->ps->holdable[1];
    int recoil;
    int half, kick, minimum;
    float increment, randomScale, kickFloat;
#if defined(_MSC_VER) && defined(_M_IX86)
    const float instabilityScale = 0.001f, instabilityUnits = 1000.0f;
    int *instabilityState = &pm->ps->stats[STAT_TCE_SHOT_INSTABILITY];
    const int *instabilityMinimum = &w->unknown_0f8[6];
#else
    double growth;
#endif
    float postureScale = 1.0f;
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
        pm->pmext->tceShotAngles[PITCH] += ((float)pm->ps->holdable[5] - 2000.0f) * 0.01f;
        pm->pmext->tceShotAngles[YAW] += ((float)pm->ps->holdable[6] - 2000.0f) * 0.01f;
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

    /* Original change -> trigger latch -> reload completion ordering. */
    if (pm->ps->weaponstate == WEAPON_DROPPING || pm->ps->weaponstate == WEAPON_DROPPING_TORELOAD) {
        PM_FinishWeaponChange();
        pm->ps->pm_flags &= ~0x400;
        return;
    }
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

    /* Original eligibility: unmounted TC mortar and binocular zoom do not
     * fire. The SDK FieldOps artillery side effect is absent here. */
    if (pm->ps->weapon == 35 || (pm->ps->eFlags & EF_ZOOMING)) return;

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

	// Gordon: reset player disguise on firing
//	if( pm->ps->weapon != WP_SMOKE_BOMB && pm->ps->weapon != WP_SATCHEL && pm->ps->weapon != WP_SATCHEL_DET ) {	// Arnout: not for these weapons
//		pm->ps->powerups[PW_OPS_DISGUISED] = 0;
//	}

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
        int fireEvent = PM_WeaponClipEmpty(pm->ps->weapon)
            ? EV_FIRE_WEAPON_LASTSHOT : EV_FIRE_WEAPON;
        /* Original pump/firemode eventParm=1 is consumed by the TC client.
         * Keep the existing 64-slot data boundary explicit for residual65/66. */
        if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_WEAPON_CAPACITY &&
            weaponDef[pm->ps->weapon].pump && pm->ps->persistant[10] > 0 &&
            BG_FiremodeWeapon(pm->ps->weapon)) {
            PM_AddEventExt(fireEvent, 1);
        } else {
            PM_AddEvent(fireEvent);
        }
    }

	// RF
// rain - moved releasedFire into pmext instead of ps
	pm->pmext->releasedFire = qfalse;
	pm->ps->lastFireTime = pm->cmd.serverTime;
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

    /* Original complete common tail applies to every supported weapon,
     * with tactical definition118 versus hip114 recoil and shared snapshots. */
    if (pm->ps->weapon >= 0 && pm->ps->weapon < TCE_MAX_WEAPONS) {
        pm->ps->holdable[0] = pm->ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
        pm->ps->holdable[1] = pm->ps->stats[STAT_TCE_SHOT_INSTABILITY];
        PM_TCEShotRecoil(addTime, (unsigned int)shotSeed);
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

void PM_UpdateLean(playerState_t *ps, usercmd_t *cmd, pmove_t *tpm) {
    if(gearDef.parsed) { PM_TCEUpdateLean(ps,cmd,tpm); return; }
	vec3_t		start, end, tmins, tmaxs, right;
	int			leaning = 0;	// -1 left, 1 right
	float		leanofs = 0;
	vec3_t		viewangles;
	trace_t		trace;

	if( (cmd->wbuttons & (WBUTTON_LEANLEFT|WBUTTON_LEANRIGHT))  && !cmd->forwardmove && cmd->upmove <= 0 ) {
		// if both are pressed, result is no lean
		if(cmd->wbuttons & WBUTTON_LEANLEFT)
			leaning -= 1;
		if(cmd->wbuttons & WBUTTON_LEANRIGHT)
			leaning += 1;
	}

	if(	BG_PlayerMounted(ps->eFlags) ) {
		leaning = 0;	// leaning not allowed on mg42
	}

	if(ps->eFlags & EF_FIRING)
		leaning = 0;	// not allowed to lean while firing

  // ATVI Wolfenstein Misc #479 - initial fix to #270 would crash in g_synchronousClients 1 situation
	if( ps->weaponstate == WEAPON_FIRING && ps->weapon == WP_DYNAMITE )
		leaning = 0; // not allowed while tossing dynamite
	
	if( ps->eFlags & EF_PRONE || ps->weapon == WP_MORTAR_SET )
		leaning = 0;	// not allowed to lean while prone

	leanofs = ps->leanf;


	if(!leaning) {	// go back to center position
		if ( leanofs > 0 ) {		// right
			//FIXME: play lean anim backwards?
			leanofs -= (((float)pml.msec/(float)LEAN_TIME_FR)*LEAN_MAX);
			if ( leanofs < 0 )
				leanofs = 0;
		}
		else if ( leanofs < 0 ) {	// left
			//FIXME: play lean anim backwards?
			leanofs += (((float)pml.msec/(float)LEAN_TIME_FR)*LEAN_MAX);
			if ( leanofs > 0 )
				leanofs = 0;
		}
	}

	if(leaning) {
		if(leaning > 0) {	// right
			if(leanofs < LEAN_MAX)
				leanofs += (((float)pml.msec/(float)LEAN_TIME_TO)*LEAN_MAX);

			if(leanofs > LEAN_MAX)
				leanofs = LEAN_MAX;

		}
		else {				// left
			if(leanofs > -LEAN_MAX)
				leanofs -= (((float)pml.msec/(float)LEAN_TIME_TO)*LEAN_MAX);

			if(leanofs < -LEAN_MAX)
				leanofs = -LEAN_MAX;

		}
	}

	ps->leanf = leanofs;

	if(leaning){
		VectorCopy( ps->origin, start );
		start[2] += ps->viewheight;

		VectorCopy( ps->viewangles, viewangles );
		viewangles[ROLL] += leanofs/2.0f;
		AngleVectors( viewangles, NULL, right, NULL );
		VectorMA( start, leanofs, right, end );		
		
		VectorSet( tmins, -8, -8, -7 ); // ATVI Wolfenstein Misc #472, bumped from -4 to cover gun clipping issue
		VectorSet( tmaxs, 8, 8, 4 );

		if( pm )
			pm->trace (&trace, start, tmins, tmaxs, end, ps->clientNum, MASK_PLAYERSOLID);
		else
			tpm->trace (&trace, start, tmins, tmaxs, end, ps->clientNum, MASK_PLAYERSOLID);

		ps->leanf *= trace.fraction;
	}


	if(ps->leanf)
		cmd->rightmove = 0;		// also disallowed in cl_input ~391

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

void PM_UpdateViewAngles( playerState_t *ps, pmoveExt_t *pmext, usercmd_t *cmd, void (trace)( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask ), int tracemask ) {	//----(SA)	modified
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

/*
================
PM_CheckLadderMove

  Checks to see if we are on a ladder
================
*/
qboolean	ladderforward;
vec3_t		laddervec;

/* TC:E30013140 / Linux000e7658: free-climb clearance and stamina gate. */
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

/* Whole TC:E3000b120 / Linux000dd4fa. */
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
               trace.entityNum == ENTITYNUM_WORLD && fabs(trace.plane.normal[2]) < 0.001f &&
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
/* Whole TC:E3000beb0 / Linux000de040. */
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

/*
================
PmoveSingle

================
*/
void trap_SnapVector( float *v );

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


/*
================
Pmove

Can be called by either the server or the client
================
*/
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
