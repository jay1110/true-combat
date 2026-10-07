// bg_slidemove.c -- part of bg_pmove functionality

#include "q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "tce_bg.h"

/*

input: origin, velocity, bounds, groundPlane, trace function

output: origin, velocity, impacts, stairup boolean

*/

/*
==================
PM_SlideMove

Returns qtrue if the velocity was clipped in some way
==================
*/
#define	MAX_CLIP_PLANES	5
/* TC:E Windows300135f0 / Linux000e7cac: four-bump body solver. */
qboolean	PM_SlideMove( qboolean gravity ) {
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity;
	vec3_t		clipVelocity;
	int			i, j, k;
	trace_t	trace;
	vec3_t		end;
	float		time_left;
	float		into;
	vec3_t		endVelocity;
	vec3_t		endClipVelocity;
	
	numbumps = 4;

	VectorCopy (pm->ps->velocity, primal_velocity);

	if ( gravity || pml.tceGroundExtension ) {
		VectorCopy( pm->ps->velocity, endVelocity );
		endVelocity[2] -= pm->ps->gravity * pml.frametime;
		pm->ps->velocity[2] = ( pm->ps->velocity[2] + endVelocity[2] ) * 0.5;
		primal_velocity[2] = endVelocity[2];
		if ( gravity && pml.groundPlane ) {
			// slide along the ground plane
			PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, 
				pm->ps->velocity, OVERCLIP );
		}
	} else {
		VectorClear( endVelocity );
	}

	time_left = pml.frametime;

	// never turn against the ground plane
	if ( pml.groundPlane && !pml.tceGroundExtension ) {
		numplanes = 1;
		VectorCopy( pml.groundTrace.plane.normal, planes[0] );
	} else {
		numplanes = 0;
	}

	// never turn against original velocity
	VectorNormalize2( pm->ps->velocity, planes[numplanes] );
	numplanes++;

	for ( bumpcount=0 ; bumpcount < numbumps ; bumpcount++ ) {

		// calculate position we are trying to move to
		VectorMA( pm->ps->origin, time_left, pm->ps->velocity, end );

		// see if we can make it there
        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, end,
                  pm->ps->clientNum, pm->tracemask);

		if (trace.allsolid) {
			// entity is completely trapped in another solid
			pm->ps->velocity[2] = 0;	// don't build up falling damage, but allow sideways acceleration
			return qtrue;
		}

		if (trace.fraction > 0) {
			// actually covered some distance
			VectorCopy (trace.endpos, pm->ps->origin);
		}

		if (trace.fraction == 1) {
			 break;		// moved the entire distance
		}

		// save entity for contact
		PM_AddTouchEnt( trace.entityNum );

		time_left -= time_left * trace.fraction;

		if (numplanes >= MAX_CLIP_PLANES) {
			// this shouldn't really happen
			VectorClear( pm->ps->velocity );
			return qtrue;
		}

		//
		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		//
		for ( i = 0 ; i < numplanes ; i++ ) {
			if ( DotProduct( trace.plane.normal, planes[i] ) > 0.99 ) {
                VectorAdd(trace.plane.normal, pm->ps->velocity, pm->ps->velocity);
				break;
			}
		}
		if ( i < numplanes ) {
			continue;
		}
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		//
		// modify velocity so it parallels all of the clip planes
		//

		// find a plane that it enters
		for ( i = 0 ; i < numplanes ; i++ ) {
			into = DotProduct( pm->ps->velocity, planes[i] );
			if ( into >= 0.1 ) {
				continue;		// move doesn't interact with the plane
			}

			// see how hard we are hitting things
			if ( -into > pml.impactSpeed ) {
				pml.impactSpeed = -into;
			}

			// slide along the plane
			PM_ClipVelocity (pm->ps->velocity, planes[i], clipVelocity, OVERCLIP );

			// slide along the plane
			PM_ClipVelocity (endVelocity, planes[i], endClipVelocity, OVERCLIP );

			// see if there is a second plane that the new move enters
			for ( j = 0 ; j < numplanes ; j++ ) {
				if ( j == i ) {
					continue;
				}
				if ( DotProduct( clipVelocity, planes[j] ) >= 0.1 ) {
					continue;		// move doesn't interact with the plane
				}

				// try clipping the move to the plane
				PM_ClipVelocity( clipVelocity, planes[j], clipVelocity, OVERCLIP );
				PM_ClipVelocity( endClipVelocity, planes[j], endClipVelocity, OVERCLIP );

				// see if it goes back into the first clip plane
				if ( DotProduct( clipVelocity, planes[i] ) >= 0 ) {
					continue;
				}

				// slide the original velocity along the crease
				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, pm->ps->velocity );
				VectorScale( dir, d, clipVelocity );

				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, endVelocity );
				VectorScale( dir, d, endClipVelocity );

				// see if there is a third plane the the new move enters
				for ( k = 0 ; k < numplanes ; k++ ) {
					if ( k == i || k == j ) {
						continue;
					}
					if ( DotProduct( clipVelocity, planes[k] ) >= 0.1 ) {
						continue;		// move doesn't interact with the plane
					}

					// stop dead at a tripple plane interaction
					VectorClear( pm->ps->velocity );
					return qtrue;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy( clipVelocity, pm->ps->velocity );
			VectorCopy( endClipVelocity, endVelocity );
			break;
		}
	}

	if ( gravity ) {
		VectorCopy( endVelocity, pm->ps->velocity );
	}

	// don't change velocity if in a timer (FIXME: is this correct?)
	if ( pm->ps->pm_time ) {
		VectorCopy( primal_velocity, pm->ps->velocity );
	}

	return ( bumpcount != 0 );
}

/*
==================
PM_StepSlideMove

==================
*/
/* TC Windows 30013d00: small steps must fit below a low doorway before
 * trying the full 18-unit step. Shared by prediction and the server. */
static void PM_TCEStepSlideMove( qboolean gravity ) {
	vec3_t start, velocity, up, down, forward;
	trace_t trace;
	float height = 9.0f, delta;
	VectorCopy(pm->ps->origin, start);
	VectorCopy(pm->ps->velocity, velocity);
	if (!PM_SlideMove(gravity)) return;
	VectorCopy(start, down);
	down[2] -= 18.0f;
	pm->trace(&trace, start, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
	if (pm->ps->velocity[2] > 0 && (trace.fraction == 1.0f || trace.plane.normal[2] < 0.7)) return;
	VectorCopy(start, up);
	up[2] += height;
	pm->trace(&trace, up, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
	if (trace.allsolid) {
		if (pm->debugLevel) Com_Printf("%i:bend can't step small\n", c_pmove);
		return;
	}
	VectorSet(forward, velocity[0], velocity[1], 0);
	VectorNormalize(forward);
	VectorMA(up, 4.0f, forward, forward);
	pm->trace(&trace, forward, pm->mins, pm->maxs, forward, pm->ps->clientNum, pm->tracemask);
	if (trace.allsolid) {
		height = 18.0f;
		VectorCopy(start, up);
		up[2] += height;
		pm->trace(&trace, up, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
		if (trace.allsolid) {
			if (pm->debugLevel) Com_Printf("%i:bend can't step\n", c_pmove);
			return;
		}
	}
	VectorCopy(up, pm->ps->origin);
	VectorCopy(velocity, pm->ps->velocity);
	PM_SlideMove(gravity);
	VectorCopy(pm->ps->origin, down);
	down[2] -= height;
	pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
	if (!trace.allsolid) VectorCopy(trace.endpos, pm->ps->origin);
	if (trace.fraction < 1.0f) PM_ClipVelocity(pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP);
	delta = pm->ps->origin[2] - start[2];
	if (delta > 2.0f) PM_AddEvent(delta < 7.0f ? EV_STEP_4 : delta < 11.0f ? EV_STEP_8 : delta < 15.0f ? EV_STEP_12 : EV_STEP_16);
	if (pm->debugLevel) Com_Printf("%i:stepped\n", c_pmove);
}

void PM_StepSlideMove( qboolean gravity ) {
	vec3_t		start_o, start_v;
	vec3_t		down_o, down_v;
	trace_t		trace;
//	float		down_dist, up_dist;
//	vec3_t		delta, delta2;
	vec3_t		up, down;
	if (gearDef.parsed && !(pm->ps->eFlags & EF_PRONE)) {
		PM_TCEStepSlideMove(gravity);
		return;
	}

	VectorCopy (pm->ps->origin, start_o);
	VectorCopy (pm->ps->velocity, start_v);

	if ( pm->debugLevel ) {
		qboolean wassolid, slidesucceed;
	
		PM_TraceAll( &trace, pm->ps->origin, pm->ps->origin );
		wassolid = trace.allsolid;
	
		slidesucceed = (PM_SlideMove( gravity ) == 0);
	
		PM_TraceAll( &trace, pm->ps->origin, pm->ps->origin );
		if (trace.allsolid && !wassolid)
			Com_Printf("%i:PM_SlideMove solidified! (%f %f %f) -> (%f %f %f)\n", c_pmove,
				start_o[0],
				start_o[1],
				start_o[2],
				pm->ps->origin[0],
				pm->ps->origin[1],
				pm->ps->origin[2]
			);
	
		if (slidesucceed)
			return;
	} else {
		if ( PM_SlideMove( gravity ) == 0 ) {
			return;		// we got exactly where we wanted to go first try
		}
	}

	if ( pm->debugLevel ) {
		Com_Printf("%i:stepping\n", c_pmove);
	}

	VectorCopy(start_o, down);
	down[2] -= STEPSIZE;

	PM_TraceAll( &trace, start_o, down );
	VectorSet(up, 0, 0, 1);
	// never step up when you still have up velocity
	if ( pm->ps->velocity[2] > 0 && (trace.fraction == 1.0 || DotProduct(trace.plane.normal, up) < 0.7)) {
		return;
	}

	VectorCopy (pm->ps->origin, down_o);
	VectorCopy (pm->ps->velocity, down_v);

	VectorCopy (start_o, up);
	up[2] += STEPSIZE;

	// test the player position if they were a stepheight higher
	PM_TraceAll( &trace, up, up );
	if ( trace.allsolid ) {
		if ( pm->debugLevel ) {
			Com_Printf("%i:bend can't step\n", c_pmove);
		}
		return;		// can't step up
	}

	// try slidemove from this position
	VectorCopy (up, pm->ps->origin);
	VectorCopy (start_v, pm->ps->velocity);

	PM_SlideMove( gravity );

	// push down the final amount
	VectorCopy (pm->ps->origin, down);
	down[2] -= STEPSIZE;

	// check legs separately
	if ( pm->ps->eFlags & EF_PRONE ) {
		PM_TraceLegs( &trace, NULL, pm->ps->origin, down, NULL, pm->ps->viewangles, pm->trace, pm->ps->clientNum, pm->tracemask );
		if ( trace.allsolid ) {
			// legs don't step, just fuzz.
			VectorCopy( down_o, pm->ps->origin );
			VectorCopy( down_v, pm->ps->velocity );
			if ( pm->debugLevel ) {
				Com_Printf("%i:legs unsteppable\n", c_pmove);
			}
			return;
		}
	}

	pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask );
	if ( !trace.allsolid ) {
		VectorCopy (trace.endpos, pm->ps->origin);
	}
	if ( trace.fraction < 1.0 ) {
		PM_ClipVelocity( pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP );
	}

#if 0
	// if the down trace can trace back to the original position directly, don't step
	PM_TraceAll( &trace, pm->ps->origin, start_o );
	if ( trace.fraction == 1.0 ) {
		// use the original move
		VectorCopy (down_o, pm->ps->origin);
		VectorCopy (down_v, pm->ps->velocity);
		if ( pm->debugLevel ) {
			Com_Printf("%i:bend\n", c_pmove);
		}
	} else 
#endif
	{
		// use the step move
		float	delta;

		delta = pm->ps->origin[2] - start_o[2];
		if ( delta > 2 ) {
			if ( delta < 7 ) {
				PM_AddEvent( EV_STEP_4 );
			} else if ( delta < 11 ) {
				PM_AddEvent( EV_STEP_8 );
			} else if ( delta < 15 ) {
				PM_AddEvent( EV_STEP_12 );
			} else {
				PM_AddEvent( EV_STEP_16 );
			}
		}
		if ( pm->debugLevel ) {
			Com_Printf("%i:stepped\n", c_pmove);
		}
	}
}



#if !defined(_MSC_VER) || !defined(_M_IX86)
/* TC:E300141c0: body trace plus independent rear-leg support probe.
 * Original repeats this probe on a fraction==1 move; do not coalesce it. */
static qboolean PM_TCEProneProbe(const vec3_t bodyEnd, qboolean firstProbe,
                                const vec3_t mins, const vec3_t maxs, float scale) {
    vec3_t start, end;
    trace_t trace;
    start[0] = bodyEnd[0] - pml.forward[0] * scale * 32.0f;
    start[1] = bodyEnd[1] - pml.forward[1] * scale * 32.0f;
    start[2] = bodyEnd[2] + 24.0f;
    VectorCopy(start, end); end[2] = start[2] - 21.6f - 24.0f;
    pm->trace(&trace, start, mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    if (trace.startsolid) {
        pm->ps->velocity[2] = 0;
        pm->pmext->proneGroundTime = pm->cmd.serverTime;
        return qfalse;
    }
    if (trace.fraction >= 1.0f) pm->pmext->proneLegsOffset = start[2];
    else {
        VectorCopy(trace.endpos, start); VectorCopy(start, end); end[2] += 21.6f;
        pm->trace(&trace, start, mins, maxs, end, pm->ps->clientNum, pm->tracemask);
        if (trace.allsolid) {
            pm->ps->velocity[2] = 0;
            if (firstProbe) pm->pmext->proneTime = -pm->cmd.serverTime;
            return qfalse;
        }
        pm->pmext->proneLegsOffset = start[2] - bodyEnd[2];
    }
    return qtrue;
}
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC2003c500 full instruction schedule, partial only at the explicitly retained
 * safe no-gravity endVelocity initialization. See proneslide_source_829. */
static const float slideK200b4080 = -13.5;
static const float slideK200b4084 = -13.5;
static const float slideK200b4088 = -24.0;
static const float slideK200b408c = 13.5;
static const float slideK200b4090 = 13.5;
static const float slideK200b4094 = -14.399999618530273;
static const float slideK200ac6e8 = 1.25;
static const double slideK200ac128 = 0.5;
static const float slideK200ac100 = 0.0;
static const float slideK200ac110 = 1.0;
static const float slideK200ac2d4 = 32.0;
static const float slideK200ac290 = 24.0;
static const float slideK200ac71c = 21.600000381469727;
static const double slideK200ac870 = 0.99;
static const double slideK200ac258 = 0.1;
typedef char slideTraceLayout829[(sizeof(trace_t)==56 && offsetof(trace_t,allsolid)==0 &&
    offsetof(trace_t,startsolid)==4 && offsetof(trace_t,fraction)==8 &&
    offsetof(trace_t,endpos)==12 && offsetof(trace_t,plane)+offsetof(cplane_t,normal)==24 &&
    offsetof(trace_t,entityNum)==52) ? 1 : -1];
enum {
    slidePlayerState829=offsetof(pmove_t,ps),
    slideExtension829=offsetof(pmove_t,pmext),
    slideCommandTime829=offsetof(pmove_t,cmd)+offsetof(usercmd_t,serverTime),
    slideMask829=offsetof(pmove_t,tracemask),
    slideMins829=offsetof(pmove_t,mins),
    slideMaxs829=offsetof(pmove_t,maxs),
    slideTrace829=offsetof(pmove_t,trace),
    slideOrigin829=offsetof(playerState_t,origin),
    slideVelocity829=offsetof(playerState_t,velocity),
    slideClient829=offsetof(playerState_t,clientNum),
    slidePmTime829=offsetof(playerState_t,pm_time),
    slideFlags829=offsetof(playerState_t,stats)+STAT_TCE_FLAGS*sizeof(int),
    slideGravity829=offsetof(playerState_t,gravity),
    slideProneTime829=offsetof(pmoveExt_t,proneTime),
    slideGroundTime829=offsetof(pmoveExt_t,proneGroundTime),
    slideLegs829=offsetof(pmoveExt_t,proneLegsOffset),
    slideFrame829=offsetof(pml_t,frametime),
    slideGround829=offsetof(pml_t,groundPlane),
    slideNormal829=offsetof(pml_t,groundTrace)+offsetof(trace_t,plane)+offsetof(cplane_t,normal),
    slideForward829=offsetof(pml_t,forward),
    slideImpact829=offsetof(pml_t,impactSpeed)
};
__declspec(naked) qboolean PM_SlideMoveProne(qboolean gravity) {
    __asm {
        sub esp, 0x148
        /* Safe native definition retained; original leaves no-gravity endVelocity undefined. */
        mov dword ptr [esp+0x20], 0
        mov dword ptr [esp+0x24], 0
        mov dword ptr [esp+0x28], 0
        mov eax, dword ptr [slideK200b4080]
        mov ecx, dword ptr [slideK200b4084]
        mov edx, dword ptr [slideK200b4088]
        mov dword ptr [esp + 0x54], eax
        mov eax, dword ptr [slideK200b408c]
        mov dword ptr [esp + 0x58], ecx
        mov ecx, dword ptr [slideK200b4090]
        mov dword ptr [esp + 0x5c], edx
        mov edx, dword ptr [slideK200b4094]
        mov dword ptr [esp + 0x48], eax
        mov eax, dword ptr [pm]
        mov dword ptr [esp + 0x4c], ecx
        mov dword ptr [esp + 0x50], edx
        mov dword ptr [esp + 0x1c], 0x3f800000
        mov ecx, dword ptr [eax + slidePlayerState829]
        mov edx, dword ptr [ecx + slideFlags829]
        test dh, 2
        je slide829_2003c5b6
        fld dword ptr [esp + 0x54]
        fmul dword ptr [slideK200ac6e8]
        mov dword ptr [esp + 0x1c], 0x3fa00000
        fstp dword ptr [esp + 0x54]
        fld dword ptr [esp + 0x58]
        fmul dword ptr [slideK200ac6e8]
        fstp dword ptr [esp + 0x58]
        fld dword ptr [esp + 0x5c]
        fmul dword ptr [slideK200ac6e8]
        fstp dword ptr [esp + 0x5c]
        fld dword ptr [esp + 0x48]
        fmul dword ptr [slideK200ac6e8]
        fstp dword ptr [esp + 0x48]
        fld dword ptr [esp + 0x4c]
        fmul dword ptr [slideK200ac6e8]
        fstp dword ptr [esp + 0x4c]
        fld dword ptr [esp + 0x50]
        fmul dword ptr [slideK200ac6e8]
        fstp dword ptr [esp + 0x50]
slide829_2003c5b6:
        mov ecx, dword ptr [eax + slidePlayerState829]
        push ebx
        push ebp
        xor ebp, ebp
        mov edx, dword ptr [ecx + slideVelocity829]
        push esi
        mov dword ptr [esp + 0x80], edx
        mov edx, dword ptr [ecx + slideVelocity829+4]
        mov dword ptr [esp + 0x84], edx
        mov edx, dword ptr [ecx + slideVelocity829+8]
        mov dword ptr [esp + 0x88], edx
        mov edx, dword ptr [esp + 0x158]
        cmp edx, ebp
        push edi
        je slide829_2003c65d
        mov ecx, dword ptr [ecx + slideVelocity829]
        mov dword ptr [esp + 0x30], ecx
        mov edx, dword ptr [eax + slidePlayerState829]
        mov ecx, dword ptr [edx + slideVelocity829+4]
        mov dword ptr [esp + 0x34], ecx
        mov edx, dword ptr [eax + slidePlayerState829]
        mov ecx, dword ptr [edx + slideVelocity829+8]
        mov dword ptr [esp + 0x38], ecx
        mov edx, dword ptr [eax + slidePlayerState829]
        fild dword ptr [edx + slideGravity829]
        fmul dword ptr [pml+slideFrame829]
        fsubr dword ptr [esp + 0x38]
        fstp dword ptr [esp + 0x38]
        mov eax, dword ptr [eax + slidePlayerState829]
        fld dword ptr [esp + 0x38]
        fadd dword ptr [eax + slideVelocity829+8]
        fmul qword ptr [slideK200ac128]
        fstp dword ptr [eax + slideVelocity829+8]
        mov eax, dword ptr [esp + 0x38]
        mov dword ptr [esp + 0x8c], eax
        mov eax, dword ptr [pml+slideGround829]
        cmp eax, ebp
        je slide829_2003c658
        mov ecx, dword ptr [pm]
        push 0x3f8020c5
        mov eax, dword ptr [ecx + slidePlayerState829]
        add eax, slideVelocity829
        push eax
        push offset pml+slideNormal829
        push eax
        call PM_ClipVelocity
        add esp, 0x10
slide829_2003c658:
        mov eax, dword ptr [pm]
slide829_2003c65d:
        mov ecx, dword ptr [pml+slideGround829]
        mov edx, dword ptr [pml+slideFrame829]
        cmp ecx, ebp
        mov dword ptr [esp + 0x1c], edx
        je slide829_2003c69f
        mov ecx, dword ptr [pml+slideNormal829]
        mov edx, dword ptr [pml+slideNormal829+4]
        mov dword ptr [esp + 0x11c], ecx
        mov ecx, dword ptr [pml+slideNormal829+8]
        mov edi, 1
        mov dword ptr [esp + 0x120], edx
        mov dword ptr [esp + 0x124], ecx
        jmp slide829_2003c6a1
slide829_2003c69f:
        xor edi, edi
slide829_2003c6a1:
        lea edx, [edi + edi*2]
        lea ecx, [esp + edx*4 + 0x11c]
        mov edx, dword ptr [eax + slidePlayerState829]
        add edx, slideVelocity829
        push ecx
        push edx
        call VectorNormalize2
        mov ecx, dword ptr [pm]
        add esp, 8
        inc edi
        xor ebx, ebx
        fstp st(0)
        lea eax, [edi + edi*2]
        mov dword ptr [esp + 0x74], ebx
        lea esi, [esp + eax*4 + 0x120]
slide829_2003c6d3:
        mov eax, dword ptr [ecx + slidePlayerState829]
        fld dword ptr [esp + 0x1c]
        fmul dword ptr [eax + slideVelocity829]
        fadd dword ptr [eax + slideOrigin829]
        fstp dword ptr [esp + 0x110]
        mov eax, dword ptr [ecx + slidePlayerState829]
        fld dword ptr [esp + 0x1c]
        fmul dword ptr [eax + slideVelocity829+4]
        fadd dword ptr [eax + slideOrigin829+4]
        fstp dword ptr [esp + 0x114]
        mov eax, dword ptr [ecx + slidePlayerState829]
        fld dword ptr [esp + 0x1c]
        fmul dword ptr [eax + slideVelocity829+8]
        fadd dword ptr [eax + slideOrigin829+8]
        fstp dword ptr [esp + 0x118]
        mov eax, dword ptr [ecx + slidePlayerState829]
        mov edx, dword ptr [ecx + slideMask829]
        push edx
        mov edx, dword ptr [eax + slideClient829]
        add eax, slideOrigin829
        push edx
        lea edx, [esp + 0x118]
        push edx
        lea edx, [ecx + slideMaxs829]
        push edx
        lea edx, [ecx + slideMins829]
        push edx
        push eax
        lea eax, [esp + 0xa8]
        push eax
        call dword ptr [ecx + slideTrace829]
        mov eax, dword ptr [esp + 0xac]
        add esp, 0x1c
        cmp eax, ebp
        jne slide829_2003cd23
        fld dword ptr [esp + 0x98]
        fcomp dword ptr [slideK200ac100]
        fnstsw ax
        test ah, 0x41
        jne slide829_2003c953
        fld dword ptr [pml+slideForward829]
        fmul dword ptr [esp + 0x2c]
        mov ecx, dword ptr [pml+slideForward829+4]
        mov eax, dword ptr [pm]
        mov dword ptr [esp + 0x108], ecx
        fmul dword ptr [slideK200ac2d4]
        fsubr dword ptr [esp + 0x9c]
        fst dword ptr [esp + 0x10]
        fld dword ptr [esp + 0x108]
        fmul dword ptr [esp + 0x2c]
        fmul dword ptr [slideK200ac2d4]
        fsubr dword ptr [esp + 0xa0]
        fstp dword ptr [esp + 0x14]
        fld dword ptr [esp + 0xa4]
        fadd dword ptr [slideK200ac290]
        mov edx, dword ptr [esp + 0x14]
        mov dword ptr [esp + 0x4c], edx
        fstp dword ptr [esp + 0x18]
        fstp dword ptr [esp + 0x48]
        fld dword ptr [esp + 0x18]
        fsub dword ptr [slideK200ac71c]
        fsub dword ptr [slideK200ac290]
        fstp dword ptr [esp + 0x50]
        mov ecx, dword ptr [eax + slideMask829]
        mov edx, dword ptr [eax + slidePlayerState829]
        push ecx
        mov ecx, dword ptr [edx + slideClient829]
        lea edx, [esp + 0x4c]
        push ecx
        push edx
        lea ecx, [esp + 0x64]
        lea edx, [esp + 0x70]
        push ecx
        push edx
        lea ecx, [esp + 0x24]
        lea edx, [esp + 0xe0]
        push ecx
        push edx
        call dword ptr [eax + slideTrace829]
        mov eax, dword ptr [esp + 0xec]
        add esp, 0x1c
        cmp eax, ebp
        jne slide829_2003cd4c
        fld dword ptr [esp + 0xd4]
        fcomp dword ptr [slideK200ac110]
        fnstsw ax
        test ah, 1
        je slide829_2003c90d
        fld dword ptr [esp + 0xe0]
        mov eax, dword ptr [esp + 0xd8]
        mov ecx, dword ptr [esp + 0xdc]
        fadd dword ptr [slideK200ac71c]
        mov edx, dword ptr [esp + 0xe0]
        mov dword ptr [esp + 0x10], eax
        mov dword ptr [esp + 0x48], eax
        mov eax, dword ptr [pm]
        fstp dword ptr [esp + 0x50]
        mov dword ptr [esp + 0x14], ecx
        mov dword ptr [esp + 0x18], edx
        mov dword ptr [esp + 0x4c], ecx
        mov edx, dword ptr [eax + slideMask829]
        mov ecx, dword ptr [eax + slidePlayerState829]
        push edx
        mov edx, dword ptr [ecx + slideClient829]
        lea ecx, [esp + 0x4c]
        push edx
        push ecx
        lea edx, [esp + 0x64]
        lea ecx, [esp + 0x70]
        push edx
        push ecx
        lea edx, [esp + 0x24]
        lea ecx, [esp + 0xe0]
        push edx
        push ecx
        call dword ptr [eax + slideTrace829]
        mov eax, dword ptr [esp + 0xe8]
        add esp, 0x1c
        cmp eax, ebp
        jne slide829_2003cd5b
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [esp + 0x9c]
        mov eax, dword ptr [edx + slidePlayerState829]
        mov dword ptr [eax + slideOrigin829], ecx
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [esp + 0xa0]
        mov eax, dword ptr [edx + slidePlayerState829]
        mov dword ptr [eax + slideOrigin829+4], ecx
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [esp + 0xa4]
        mov eax, dword ptr [edx + slidePlayerState829]
        mov dword ptr [eax + slideOrigin829+8], ecx
        mov edx, dword ptr [pm]
        fld dword ptr [esp + 0x18]
        fsub dword ptr [esp + 0xa4]
        mov eax, dword ptr [edx + slideExtension829]
        fstp dword ptr [eax + slideLegs829]
        jmp slide829_2003c953
slide829_2003c90d:
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [esp + 0x9c]
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideOrigin829], eax
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [esp + 0xa0]
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideOrigin829+4], eax
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [esp + 0xa4]
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideOrigin829+8], eax
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [esp + 0x18]
        mov edx, dword ptr [ecx + slideExtension829]
        mov dword ptr [edx + slideLegs829], eax
slide829_2003c953:
        fld dword ptr [esp + 0x98]
        fcomp dword ptr [slideK200ac110]
        fnstsw ax
        test ah, 0x40
        jne slide829_2003cd86
        mov ecx, dword ptr [esp + 0xc4]
        push ecx
        call PM_AddTouchEnt
        fld dword ptr [esp + 0x9c]
        fmul dword ptr [esp + 0x20]
        add esp, 4
        cmp edi, 5
        fsubr dword ptr [esp + 0x1c]
        fstp dword ptr [esp + 0x1c]
        jge slide829_2003cfc4
        xor ecx, ecx
        cmp edi, ebp
        jle slide829_2003ca26
        lea edx, [esp + 0x120]
slide829_2003c9a8:
        fld dword ptr [esp + 0xb0]
        fmul dword ptr [edx + 4]
        fld dword ptr [esp + 0xa8]
        fmul dword ptr [edx - 4]
        faddp st(1), st(0)
        fld dword ptr [esp + 0xac]
        fmul dword ptr [edx]
        faddp st(1), st(0)
        fcomp qword ptr [slideK200ac870]
        fnstsw ax
        test ah, 0x41
        je slide829_2003c9e0
        inc ecx
        add edx, 0xc
        cmp ecx, edi
        jl slide829_2003c9a8
        jmp slide829_2003ca26
slide829_2003c9e0:
        mov edx, dword ptr [pm]
        cmp ecx, edi
        fld dword ptr [esp + 0xa8]
        mov eax, dword ptr [edx + slidePlayerState829]
        fadd dword ptr [eax + slideVelocity829]
        fstp dword ptr [eax + slideVelocity829]
        mov eax, dword ptr [pm]
        fld dword ptr [esp + 0xac]
        mov eax, dword ptr [eax + slidePlayerState829]
        fadd dword ptr [eax + slideVelocity829+4]
        fstp dword ptr [eax + slideVelocity829+4]
        mov edx, dword ptr [pm]
        fld dword ptr [esp + 0xb0]
        mov eax, dword ptr [edx + slidePlayerState829]
        fadd dword ptr [eax + slideVelocity829+8]
        fstp dword ptr [eax + slideVelocity829+8]
        jl slide829_2003cd06
slide829_2003ca26:
        mov eax, dword ptr [esp + 0xa8]
        mov ecx, dword ptr [esp + 0xac]
        mov edx, dword ptr [esp + 0xb0]
        mov dword ptr [esi - 4], eax
        mov dword ptr [esi], ecx
        mov dword ptr [esi + 4], edx
        inc edi
        add esi, 0xc
        cmp edi, ebp
        mov dword ptr [esp + 0xc8], esi
        mov dword ptr [esp + 0x54], ebp
        jle slide829_2003cd06
        mov ecx, dword ptr [pm]
        lea edx, [esp + 0x120]
        mov ebx, dword ptr [ecx + slidePlayerState829]
slide829_2003ca69:
        fld dword ptr [edx - 4]
        fmul dword ptr [ebx + slideVelocity829]
        fld dword ptr [edx + 4]
        fmul dword ptr [ebx + slideVelocity829+8]
        faddp st(1), st(0)
        fld dword ptr [edx]
        fmul dword ptr [ebx + slideVelocity829+4]
        faddp st(1), st(0)
        fcom qword ptr [slideK200ac258]
        fnstsw ax
        test ah, 1
        jne slide829_2003caa0
        inc ebp
        add edx, 0xc
        cmp ebp, edi
        fstp st(0)
        jl slide829_2003ca69
        mov dword ptr [esp + 0x54], ebp
        xor ebp, ebp
        jmp slide829_2003cd0c
slide829_2003caa0:
        fchs 
        fcom dword ptr [pml+slideImpact829]
        mov dword ptr [esp + 0x54], ebp
        fnstsw ax
        test ah, 0x41
        jne slide829_2003cabb
        fstp dword ptr [pml+slideImpact829]
        jmp slide829_2003cabd
slide829_2003cabb:
        fstp st(0)
slide829_2003cabd:
        mov ecx, dword ptr [ecx + slidePlayerState829]
        lea ebp, [ebp + ebp*2]
        shl ebp, 2
        lea eax, [esp + 0x20]
        push 0x3f8020c5
        lea ebx, [esp + ebp + 0x120]
        push eax
        add ecx, slideVelocity829
        push ebx
        push ecx
        call PM_ClipVelocity
        lea edx, [esp + 0x88]
        push 0x3f8020c5
        push edx
        lea eax, [esp + 0x48]
        push ebx
        push eax
        call PM_ClipVelocity
        add esp, 0x20
        xor eax, eax
        mov dword ptr [esp + 0x70], eax
        lea esi, [esp + 0x11c]
slide829_2003cb09:
        cmp eax, dword ptr [esp + 0x54]
        je slide829_2003cc99
        fld dword ptr [esp + 0x28]
        fmul dword ptr [esi + 8]
        fld dword ptr [esp + 0x24]
        fmul dword ptr [esi + 4]
        faddp st(1), st(0)
        fld dword ptr [esp + 0x20]
        fmul dword ptr [esi]
        faddp st(1), st(0)
        fcomp qword ptr [slideK200ac258]
        fnstsw ax
        test ah, 1
        je slide829_2003cc99
        lea ecx, [esp + 0x20]
        push 0x3f8020c5
        push ecx
        lea edx, [esp + 0x28]
        push esi
        push edx
        call PM_ClipVelocity
        lea eax, [esp + 0x88]
        push 0x3f8020c5
        push eax
        lea ecx, [esp + 0x90]
        push esi
        push ecx
        call PM_ClipVelocity
        fld dword ptr [esp + 0x48]
        fmul dword ptr [esp + ebp + 0x144]
        fld dword ptr [esp + 0x44]
        fmul dword ptr [esp + ebp + 0x140]
        add esp, 0x20
        faddp st(1), st(0)
        fld dword ptr [esp + 0x20]
        fmul dword ptr [ebx]
        faddp st(1), st(0)
        fcomp dword ptr [slideK200ac100]
        fnstsw ax
        test ah, 1
        je slide829_2003cc99
        lea edx, [esp + 0x3c]
        push edx
        push esi
        push ebx
        call CrossProduct
        lea eax, [esp + 0x48]
        push eax
        call VectorNormalize
        mov ecx, dword ptr [pm]
        lea edx, [esp + 0x4c]
        fstp st(0)
        mov eax, dword ptr [ecx + slidePlayerState829]
        push edx
        fld dword ptr [esp + 0x58]
        fmul dword ptr [eax + slideVelocity829+8]
        fld dword ptr [esp + 0x54]
        fmul dword ptr [eax + slideVelocity829+4]
        push esi
        push ebx
        faddp st(1), st(0)
        fld dword ptr [esp + 0x58]
        fmul dword ptr [eax + slideVelocity829]
        faddp st(1), st(0)
        fld dword ptr [esp + 0x58]
        fmul st(0), st(1)
        fstp dword ptr [esp + 0x3c]
        fld dword ptr [esp + 0x5c]
        fmul st(0), st(1)
        fstp dword ptr [esp + 0x40]
        fld dword ptr [esp + 0x60]
        fmul st(0), st(1)
        fstp dword ptr [esp + 0x44]
        fstp st(0)
        call CrossProduct
        lea eax, [esp + 0x58]
        push eax
        call VectorNormalize
        fstp st(0)
        fld dword ptr [esp + 0x64]
        fmul dword ptr [esp + 0x58]
        fld dword ptr [esp + 0x60]
        fmul dword ptr [esp + 0x54]
        add esp, 0x20
        xor ecx, ecx
        lea edx, [esp + 0x120]
        faddp st(1), st(0)
        fld dword ptr [esp + 0x3c]
        fmul dword ptr [esp + 0x30]
        faddp st(1), st(0)
        fld dword ptr [esp + 0x3c]
        fmul st(0), st(1)
        fstp dword ptr [esp + 0x78]
        fld dword ptr [esp + 0x40]
        fmul st(0), st(1)
        fstp dword ptr [esp + 0x7c]
        fld dword ptr [esp + 0x44]
        fmul st(0), st(1)
        fstp dword ptr [esp + 0x80]
        fstp st(0)
slide829_2003cc5c:
        cmp ecx, dword ptr [esp + 0x54]
        je slide829_2003cc91
        cmp ecx, dword ptr [esp + 0x70]
        je slide829_2003cc91
        fld dword ptr [esp + 0x28]
        fmul dword ptr [edx + 4]
        fld dword ptr [esp + 0x20]
        fmul dword ptr [edx - 4]
        faddp st(1), st(0)
        fld dword ptr [esp + 0x24]
        fmul dword ptr [edx]
        faddp st(1), st(0)
        fcomp qword ptr [slideK200ac258]
        fnstsw ax
        test ah, 1
        jne slide829_2003d002
slide829_2003cc91:
        inc ecx
        add edx, 0xc
        cmp ecx, edi
        jl slide829_2003cc5c
slide829_2003cc99:
        mov eax, dword ptr [esp + 0x70]
        add esi, 0xc
        inc eax
        cmp eax, edi
        mov dword ptr [esp + 0x70], eax
        jl slide829_2003cb09
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [esp + 0x20]
        mov esi, dword ptr [esp + 0xc8]
        xor ebp, ebp
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideVelocity829], eax
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [esp + 0x24]
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideVelocity829+4], eax
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [esp + 0x28]
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideVelocity829+8], eax
        mov ecx, dword ptr [esp + 0x78]
        mov edx, dword ptr [esp + 0x7c]
        mov eax, dword ptr [esp + 0x80]
        mov dword ptr [esp + 0x30], ecx
        mov ecx, dword ptr [pm]
        mov dword ptr [esp + 0x34], edx
        mov dword ptr [esp + 0x38], eax
        jmp slide829_2003cd0c
slide829_2003cd06:
        mov ecx, dword ptr [pm]
slide829_2003cd0c:
        mov ebx, dword ptr [esp + 0x74]
        inc ebx
        cmp ebx, 4
        mov dword ptr [esp + 0x74], ebx
        jl slide829_2003c6d3
        jmp slide829_2003cf01
slide829_2003cd23:
        mov ecx, dword ptr [pm]
        pop edi
        pop esi
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideVelocity829+8], ebp
        mov eax, dword ptr [pm]
        pop ebp
        pop ebx
        mov ecx, dword ptr [eax + slideExtension829]
        mov edx, dword ptr [eax + slideCommandTime829]
        mov eax, 1
        mov dword ptr [ecx + slideGroundTime829], edx
        add esp, 0x148
        ret 
slide829_2003cd4c:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + slidePlayerState829]
        mov dword ptr [ecx + slideVelocity829+8], ebp
        jmp slide829_2003d024
slide829_2003cd5b:
        mov ecx, dword ptr [pm]
        pop edi
        pop esi
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideVelocity829+8], ebp
        mov eax, dword ptr [pm]
        pop ebp
        pop ebx
        mov ecx, dword ptr [eax + slideCommandTime829]
        mov edx, dword ptr [eax + slideExtension829]
        neg ecx
        mov dword ptr [edx + slideProneTime829], ecx
        mov eax, 1
        add esp, 0x148
        ret 
slide829_2003cd86:
        fld dword ptr [pml+slideForward829]
        fmul dword ptr [esp + 0x2c]
        mov eax, dword ptr [pml+slideForward829+4]
        mov dword ptr [esp + 0x108], eax
        mov eax, dword ptr [pm]
        fmul dword ptr [slideK200ac2d4]
        fsubr dword ptr [esp + 0x9c]
        fst dword ptr [esp + 0x10]
        fld dword ptr [esp + 0x108]
        fmul dword ptr [esp + 0x2c]
        fmul dword ptr [slideK200ac2d4]
        fsubr dword ptr [esp + 0xa0]
        fstp dword ptr [esp + 0x14]
        fld dword ptr [esp + 0xa4]
        fadd dword ptr [slideK200ac290]
        mov ecx, dword ptr [esp + 0x14]
        mov dword ptr [esp + 0x4c], ecx
        fstp dword ptr [esp + 0x18]
        fstp dword ptr [esp + 0x48]
        fld dword ptr [esp + 0x18]
        fsub dword ptr [slideK200ac71c]
        fsub dword ptr [slideK200ac290]
        fstp dword ptr [esp + 0x50]
        mov edx, dword ptr [eax + slideMask829]
        mov ecx, dword ptr [eax + slidePlayerState829]
        push edx
        mov edx, dword ptr [ecx + slideClient829]
        lea ecx, [esp + 0x4c]
        push edx
        push ecx
        lea edx, [esp + 0x64]
        lea ecx, [esp + 0x70]
        push edx
        push ecx
        lea edx, [esp + 0x24]
        lea ecx, [esp + 0xe0]
        push edx
        push ecx
        call dword ptr [eax + slideTrace829]
        mov eax, dword ptr [esp + 0xec]
        add esp, 0x1c
        cmp eax, ebp
        je slide829_2003ce4c
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx + slidePlayerState829]
        mov dword ptr [eax + slideVelocity829+8], ebp
        jmp slide829_2003cfe4
slide829_2003ce4c:
        fld dword ptr [esp + 0xd4]
        fcomp dword ptr [slideK200ac110]
        fnstsw ax
        test ah, 1
        je slide829_2003cfb0
        fld dword ptr [esp + 0xe0]
        mov eax, dword ptr [esp + 0xd8]
        mov ecx, dword ptr [esp + 0xdc]
        fadd dword ptr [slideK200ac71c]
        mov edx, dword ptr [esp + 0xe0]
        mov dword ptr [esp + 0x10], eax
        mov dword ptr [esp + 0x48], eax
        mov eax, dword ptr [pm]
        fstp dword ptr [esp + 0x50]
        mov dword ptr [esp + 0x14], ecx
        mov dword ptr [esp + 0x18], edx
        mov dword ptr [esp + 0x4c], ecx
        mov edx, dword ptr [eax + slideMask829]
        mov ecx, dword ptr [eax + slidePlayerState829]
        push edx
        mov edx, dword ptr [ecx + slideClient829]
        lea ecx, [esp + 0x4c]
        push edx
        push ecx
        lea edx, [esp + 0x64]
        lea ecx, [esp + 0x70]
        push edx
        push ecx
        lea edx, [esp + 0x24]
        lea ecx, [esp + 0xe0]
        push edx
        push ecx
        call dword ptr [eax + slideTrace829]
        mov eax, dword ptr [esp + 0xe8]
        add esp, 0x1c
        cmp eax, ebp
        jne slide829_2003cf95
        fld dword ptr [esp + 0x18]
        mov edx, dword ptr [pm]
        fsub dword ptr [esp + 0xa4]
        mov eax, dword ptr [edx + slideExtension829]
        fstp dword ptr [eax + slideLegs829]
slide829_2003cefb:
        mov ecx, dword ptr [pm]
slide829_2003cf01:
        mov edx, dword ptr [esp + 0x15c]
        cmp edx, ebp
        je slide829_2003cf38
        mov ecx, dword ptr [ecx + slidePlayerState829]
        mov eax, dword ptr [esp + 0x30]
        mov dword ptr [ecx + slideVelocity829], eax
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx + slidePlayerState829]
        mov ecx, dword ptr [esp + 0x34]
        mov dword ptr [eax + slideVelocity829+4], ecx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + slidePlayerState829]
        mov eax, dword ptr [esp + 0x38]
        mov dword ptr [ecx + slideVelocity829+8], eax
        mov ecx, dword ptr [pm]
slide829_2003cf38:
        mov eax, dword ptr [ecx + slidePlayerState829]
        cmp dword ptr [eax + slidePmTime829], ebp
        je slide829_2003cf72
        mov ecx, dword ptr [esp + 0x84]
        mov dword ptr [eax + slideVelocity829], ecx
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + slidePlayerState829]
        mov eax, dword ptr [esp + 0x88]
        mov dword ptr [ecx + slideVelocity829+4], eax
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx + slidePlayerState829]
        mov ecx, dword ptr [esp + 0x8c]
        mov dword ptr [eax + slideVelocity829+8], ecx
        mov ecx, dword ptr [pm]
slide829_2003cf72:
        cmp ebx, ebp
        jne slide829_2003cf83
        cmp edx, ebp
        jne slide829_2003cf83
        mov edx, dword ptr [ecx + slideExtension829]
        mov eax, dword ptr [ecx + slideCommandTime829]
        mov dword ptr [edx + slideGroundTime829], eax
slide829_2003cf83:
        xor eax, eax
        pop edi
        cmp ebx, ebp
        pop esi
        pop ebp
        pop ebx
        setne al
        add esp, 0x148
        ret 
slide829_2003cf95:
        mov ecx, dword ptr [pm]
        pop edi
        pop esi
        mov eax, 1
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideVelocity829+8], ebp
        pop ebp
        pop ebx
        add esp, 0x148
        ret 
slide829_2003cfb0:
        mov eax, dword ptr [pm]
        mov edx, dword ptr [esp + 0x18]
        mov ecx, dword ptr [eax + slideExtension829]
        mov dword ptr [ecx + slideLegs829], edx
        jmp slide829_2003cefb
slide829_2003cfc4:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + slidePlayerState829]
        mov dword ptr [ecx + slideVelocity829+8], ebp
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx + slidePlayerState829]
        mov dword ptr [eax + slideVelocity829+4], ebp
        mov ecx, dword ptr [pm]
        mov edx, dword ptr [ecx + slidePlayerState829]
        mov dword ptr [edx + slideVelocity829], ebp
slide829_2003cfe4:
        mov eax, dword ptr [pm]
        pop edi
        pop esi
        pop ebp
        mov ecx, dword ptr [eax + slideExtension829]
        mov edx, dword ptr [eax + slideCommandTime829]
        mov eax, 1
        pop ebx
        mov dword ptr [ecx + slideGroundTime829], edx
        add esp, 0x148
        ret 
slide829_2003d002:
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + slidePlayerState829]
        xor eax, eax
        mov dword ptr [ecx + slideVelocity829+8], eax
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx + slidePlayerState829]
        mov dword ptr [ecx + slideVelocity829+4], eax
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [edx + slidePlayerState829]
        mov dword ptr [ecx + slideVelocity829], eax
slide829_2003d024:
        mov eax, dword ptr [pm]
        pop edi
        pop esi
        pop ebp
        mov edx, dword ptr [eax + slideExtension829]
        mov eax, dword ptr [eax + slideCommandTime829]
        pop ebx
        mov dword ptr [edx + slideGroundTime829], eax
        mov eax, 1
        add esp, 0x148
        ret 
    }
}
#else
qboolean PM_SlideMoveProne( qboolean gravity ) {
	vec3_t legsMins = {-13.5f, -13.5f, -24.0f};
	vec3_t legsMaxs = {13.5f, 13.5f, -14.4f};
	float legsScale = 1.0f;
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity;
	vec3_t		clipVelocity;
	int			i, j, k;
	trace_t	trace;
	vec3_t		end;
	float		time_left;
	float		into;
	vec3_t		endVelocity;
	vec3_t		endClipVelocity;
	
	/* Original captures these once, before any collision callback. */
	if (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) {
		legsScale = 1.25f;
		VectorScale(legsMins, legsScale, legsMins);
		VectorScale(legsMaxs, legsScale, legsMaxs);
	}
	numbumps = 4;

	VectorCopy (pm->ps->velocity, primal_velocity);

	if ( gravity ) {
		VectorCopy( pm->ps->velocity, endVelocity );
		endVelocity[2] -= pm->ps->gravity * pml.frametime;
		pm->ps->velocity[2] = ( pm->ps->velocity[2] + endVelocity[2] ) * 0.5;
		primal_velocity[2] = endVelocity[2];
		if ( pml.groundPlane ) {
			// slide along the ground plane
			PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, 
				pm->ps->velocity, OVERCLIP );
		}
	} else {
		VectorClear( endVelocity );
	}

	time_left = pml.frametime;

	// never turn against the ground plane
	if ( pml.groundPlane ) {
		numplanes = 1;
		VectorCopy( pml.groundTrace.plane.normal, planes[0] );
	} else {
		numplanes = 0;
	}

	// never turn against original velocity
	VectorNormalize2( pm->ps->velocity, planes[numplanes] );
	numplanes++;

	for ( bumpcount=0 ; bumpcount < numbumps ; bumpcount++ ) {

		// calculate position we are trying to move to
		VectorMA( pm->ps->origin, time_left, pm->ps->velocity, end );

		// see if we can make it there
        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, end,
                  pm->ps->clientNum, pm->tracemask);

		if (trace.allsolid) {
			// entity is completely trapped in another solid
			pm->ps->velocity[2] = 0;	// don't build up falling damage, but allow sideways acceleration
			pm->pmext->proneGroundTime = pm->cmd.serverTime; return qtrue;
		}

        if (trace.fraction > 0) {
            if (!PM_TCEProneProbe(trace.endpos, qtrue, legsMins, legsMaxs, legsScale)) return qtrue;
            VectorCopy(trace.endpos, pm->ps->origin);
        }
        if (trace.fraction == 1) {
            if (!PM_TCEProneProbe(trace.endpos, qfalse, legsMins, legsMaxs, legsScale)) return qtrue;
            break;
        }

		// save entity for contact
		PM_AddTouchEnt( trace.entityNum );

		time_left -= time_left * trace.fraction;

		if (numplanes >= MAX_CLIP_PLANES) {
			// this shouldn't really happen
			VectorClear( pm->ps->velocity );
			pm->pmext->proneGroundTime = pm->cmd.serverTime; return qtrue;
		}

		//
		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		//
		for ( i = 0 ; i < numplanes ; i++ ) {
			if ( DotProduct( trace.plane.normal, planes[i] ) > 0.99 ) {
                VectorAdd(trace.plane.normal, pm->ps->velocity, pm->ps->velocity);
				break;
			}
		}
		if ( i < numplanes ) {
			continue;
		}
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		//
		// modify velocity so it parallels all of the clip planes
		//

		// find a plane that it enters
		for ( i = 0 ; i < numplanes ; i++ ) {
			into = DotProduct( pm->ps->velocity, planes[i] );
			if ( into >= 0.1 ) {
				continue;		// move doesn't interact with the plane
			}

			// see how hard we are hitting things
			if ( -into > pml.impactSpeed ) {
				pml.impactSpeed = -into;
			}

			// slide along the plane
			PM_ClipVelocity (pm->ps->velocity, planes[i], clipVelocity, OVERCLIP );

			// slide along the plane
			PM_ClipVelocity (endVelocity, planes[i], endClipVelocity, OVERCLIP );

			// see if there is a second plane that the new move enters
			for ( j = 0 ; j < numplanes ; j++ ) {
				if ( j == i ) {
					continue;
				}
				if ( DotProduct( clipVelocity, planes[j] ) >= 0.1 ) {
					continue;		// move doesn't interact with the plane
				}

				// try clipping the move to the plane
				PM_ClipVelocity( clipVelocity, planes[j], clipVelocity, OVERCLIP );
				PM_ClipVelocity( endClipVelocity, planes[j], endClipVelocity, OVERCLIP );

				// see if it goes back into the first clip plane
				if ( DotProduct( clipVelocity, planes[i] ) >= 0 ) {
					continue;
				}

				// slide the original velocity along the crease
				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, pm->ps->velocity );
				VectorScale( dir, d, clipVelocity );

				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, endVelocity );
				VectorScale( dir, d, endClipVelocity );

				// see if there is a third plane the the new move enters
				for ( k = 0 ; k < numplanes ; k++ ) {
					if ( k == i || k == j ) {
						continue;
					}
					if ( DotProduct( clipVelocity, planes[k] ) >= 0.1 ) {
						continue;		// move doesn't interact with the plane
					}

					// stop dead at a tripple plane interaction
					VectorClear( pm->ps->velocity );
					pm->pmext->proneGroundTime = pm->cmd.serverTime; return qtrue;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy( clipVelocity, pm->ps->velocity );
			VectorCopy( endClipVelocity, endVelocity );
			break;
		}
	}

	if ( gravity ) {
		VectorCopy( endVelocity, pm->ps->velocity );
	}

	// don't change velocity if in a timer (FIXME: is this correct?)
	if ( pm->ps->pm_time ) {
		VectorCopy( primal_velocity, pm->ps->velocity );
	}

	if (!bumpcount && !gravity) pm->pmext->proneGroundTime = pm->cmd.serverTime;
	return ( bumpcount != 0 );
}

#endif

/* TC:E Windows30014d10 / Linux000e9800: dedicated prone step controller. */
#if defined(_MSC_VER) && defined(_M_IX86)
/* Whole TC Windows2003d050: native fields and original x87/trace callback ABI. */
static const float stepK200ac2d8 = 18.0;
static const float stepK200ac100 = 0.0;
static const double stepK200ac130 = 1.0;
static const double stepK200ac268 = 0.7;
static const float stepK200ac270 = 2.0;
static const float stepK200ac6d0 = 7.0;
static const float stepK200ac87c = 11.0;
static const float stepK200ac878 = 15.0;
static const char stepBlocked828[] = "%i:bend can't step\n";
static const char stepDone828[] = "%i:stepped\n";
/* Original stack trace buffer is56 bytes; reject silent engine ABI changes. */
typedef char stepTraceLayout828[(sizeof(trace_t)==56 && offsetof(trace_t,allsolid)==0 &&
    offsetof(trace_t,fraction)==8 && offsetof(trace_t,endpos)==12 &&
    offsetof(trace_t,plane)+offsetof(cplane_t,normal)==24) ? 1 : -1];
typedef char stepEventIds828[(EV_STEP_4==12 && EV_STEP_8==13 && EV_STEP_12==14 && EV_STEP_16==15) ? 1 : -1];
enum {
    stepPlayerState828=offsetof(pmove_t,ps),
    stepMask828=offsetof(pmove_t,tracemask),
    stepDebug828=offsetof(pmove_t,debugLevel),
    stepMins828=offsetof(pmove_t,mins),
    stepMaxs828=offsetof(pmove_t,maxs),
    stepTrace828=offsetof(pmove_t,trace),
    stepOrigin828=offsetof(playerState_t,origin),
    stepVelocity828=offsetof(playerState_t,velocity),
    stepClient828=offsetof(playerState_t,clientNum)
};
__declspec(naked) void PM_StepSlideMoveProne(qboolean gravity) {
    __asm {
        sub esp, 0x68
        mov eax, dword ptr [pm]
        push esi
        mov esi, dword ptr [esp + 0x70]
        mov ecx, dword ptr [eax + stepPlayerState828]
        push esi
        mov edx, dword ptr [ecx + stepOrigin828]
        mov dword ptr [esp + 0x14], edx
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov edx, dword ptr [ecx + stepOrigin828+4]
        mov dword ptr [esp + 0x18], edx
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov edx, dword ptr [ecx + stepOrigin828+8]
        mov dword ptr [esp + 0x1c], edx
        mov eax, dword ptr [eax + stepPlayerState828]
        mov ecx, dword ptr [eax + stepVelocity828]
        mov edx, dword ptr [eax + stepVelocity828+4]
        mov eax, dword ptr [eax + stepVelocity828+8]
        mov dword ptr [esp + 0x2c], ecx
        mov dword ptr [esp + 0x30], edx
        mov dword ptr [esp + 0x34], eax
        call PM_SlideMoveProne
        add esp, 4
        test eax, eax
        je step828_2003d374
        fld dword ptr [esp + 0x18]
        mov ecx, dword ptr [esp + 0x10]
        mov edx, dword ptr [esp + 0x14]
        mov eax, dword ptr [pm]
        mov dword ptr [esp + 0x1c], ecx
        fsub dword ptr [stepK200ac2d8]
        mov dword ptr [esp + 0x20], edx
        fstp dword ptr [esp + 0x24]
        mov ecx, dword ptr [eax + stepMask828]
        mov edx, dword ptr [eax + stepPlayerState828]
        push ecx
        mov ecx, dword ptr [edx + stepClient828]
        lea edx, [esp + 0x20]
        push ecx
        push edx
        lea ecx, [eax + stepMaxs828]
        lea edx, [eax + stepMins828]
        push ecx
        push edx
        lea ecx, [esp + 0x24]
        lea edx, [esp + 0x48]
        push ecx
        push edx
        call dword ptr [eax + stepTrace828]
        mov ecx, dword ptr [pm]
        mov dword ptr [esp + 0x20], 0
        mov dword ptr [esp + 0x24], 0
        mov dword ptr [esp + 0x28], 0x3f800000
        mov eax, dword ptr [ecx + stepPlayerState828]
        add esp, 0x1c
        fld dword ptr [eax + stepVelocity828+8]
        fcomp dword ptr [stepK200ac100]
        fnstsw ax
        test ah, 0x41
        jne step828_2003d150
        fld dword ptr [esp + 0x3c]
        fcomp qword ptr [stepK200ac130]
        fnstsw ax
        test ah, 0x40
        jne step828_2003d374
        fld dword ptr [esp + 0x54]
        fcomp qword ptr [stepK200ac268]
        fnstsw ax
        test ah, 1
        jne step828_2003d374
step828_2003d150:
        fld dword ptr [esp + 0x18]
        mov edx, dword ptr [esp + 0x10]
        mov eax, dword ptr [esp + 0x14]
        fadd dword ptr [stepK200ac2d8]
        mov dword ptr [esp + 4], edx
        mov dword ptr [esp + 8], eax
        fstp dword ptr [esp + 0xc]
        mov edx, dword ptr [ecx + stepMask828]
        mov eax, dword ptr [ecx + stepPlayerState828]
        push edx
        mov edx, dword ptr [eax + stepClient828]
        lea eax, [esp + 8]
        push edx
        push eax
        lea edx, [ecx + stepMaxs828]
        lea eax, [ecx + stepMins828]
        push edx
        push eax
        lea edx, [esp + 0x18]
        lea eax, [esp + 0x48]
        push edx
        push eax
        call dword ptr [ecx + stepTrace828]
        mov eax, dword ptr [esp + 0x50]
        add esp, 0x1c
        test eax, eax
        je step828_2003d1d3
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx + stepDebug828]
        test eax, eax
        je step828_2003d374
        mov edx, dword ptr [c_pmove]
        push edx
        push offset stepBlocked828
        call Com_Printf
        add esp, 8
        pop esi
        add esp, 0x68
        ret 
step828_2003d1d3:
        mov eax, dword ptr [pm]
        mov edx, dword ptr [esp + 4]
        push esi
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov dword ptr [ecx + stepOrigin828], edx
        mov eax, dword ptr [pm]
        mov edx, dword ptr [esp + 0xc]
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov dword ptr [ecx + stepOrigin828+4], edx
        mov eax, dword ptr [pm]
        mov edx, dword ptr [esp + 0x10]
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov dword ptr [ecx + stepOrigin828+8], edx
        mov eax, dword ptr [pm]
        mov edx, dword ptr [esp + 0x2c]
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov dword ptr [ecx + stepVelocity828], edx
        mov eax, dword ptr [pm]
        mov edx, dword ptr [esp + 0x30]
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov dword ptr [ecx + stepVelocity828+4], edx
        mov eax, dword ptr [pm]
        mov edx, dword ptr [esp + 0x34]
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov dword ptr [ecx + stepVelocity828+8], edx
        call PM_SlideMoveProne
        mov eax, dword ptr [pm]
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov edx, dword ptr [ecx + stepOrigin828]
        mov dword ptr [esp + 0x20], edx
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov edx, dword ptr [ecx + stepOrigin828+4]
        mov dword ptr [esp + 0x24], edx
        mov ecx, dword ptr [eax + stepPlayerState828]
        fld dword ptr [ecx + stepOrigin828+8]
        fsub dword ptr [stepK200ac2d8]
        fstp dword ptr [esp + 0x28]
        mov ecx, dword ptr [eax + stepPlayerState828]
        mov edx, dword ptr [eax + stepMask828]
        push edx
        mov edx, dword ptr [ecx + stepClient828]
        add ecx, stepOrigin828
        push edx
        lea edx, [esp + 0x28]
        push edx
        lea edx, [eax + stepMaxs828]
        push edx
        lea edx, [eax + stepMins828]
        push edx
        push ecx
        lea ecx, [esp + 0x50]
        push ecx
        call dword ptr [eax + stepTrace828]
        mov eax, dword ptr [esp + 0x54]
        add esp, 0x20
        test eax, eax
        jne step828_2003d2ba
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [esp + 0x40]
        mov eax, dword ptr [edx + stepPlayerState828]
        mov dword ptr [eax + stepOrigin828], ecx
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [esp + 0x44]
        mov eax, dword ptr [edx + stepPlayerState828]
        mov dword ptr [eax + stepOrigin828+4], ecx
        mov edx, dword ptr [pm]
        mov ecx, dword ptr [esp + 0x48]
        mov eax, dword ptr [edx + stepPlayerState828]
        mov dword ptr [eax + stepOrigin828+8], ecx
step828_2003d2ba:
        fld dword ptr [esp + 0x3c]
        fcomp qword ptr [stepK200ac130]
        fnstsw ax
        test ah, 1
        je step828_2003d2ea
        mov edx, dword ptr [pm]
        push 0x3f8020c5
        lea ecx, [esp + 0x50]
        mov eax, dword ptr [edx + stepPlayerState828]
        add eax, stepVelocity828
        push eax
        push ecx
        push eax
        call PM_ClipVelocity
        add esp, 0x10
step828_2003d2ea:
        mov edx, dword ptr [pm]
        mov eax, dword ptr [edx + stepPlayerState828]
        fld dword ptr [eax + stepOrigin828+8]
        fsub dword ptr [esp + 0x18]
        fst dword ptr [esp + 0x70]
        fcomp dword ptr [stepK200ac270]
        fnstsw ax
        test ah, 0x41
        jne step828_2003d353
        fld dword ptr [esp + 0x70]
        fcomp dword ptr [stepK200ac6d0]
        fnstsw ax
        test ah, 1
        je step828_2003d31f
        push 0xc
        jmp step828_2003d34b
step828_2003d31f:
        fld dword ptr [esp + 0x70]
        fcomp dword ptr [stepK200ac87c]
        fnstsw ax
        test ah, 1
        je step828_2003d334
        push 0xd
        jmp step828_2003d34b
step828_2003d334:
        fld dword ptr [esp + 0x70]
        fcomp dword ptr [stepK200ac878]
        fnstsw ax
        test ah, 1
        je step828_2003d349
        push 0xe
        jmp step828_2003d34b
step828_2003d349:
        push 0xf
step828_2003d34b:
        call PM_AddEvent
        add esp, 4
step828_2003d353:
        mov ecx, dword ptr [pm]
        mov eax, dword ptr [ecx + stepDebug828]
        test eax, eax
        je step828_2003d374
        mov edx, dword ptr [c_pmove]
        push edx
        push offset stepDone828
        call Com_Printf
        add esp, 8
step828_2003d374:
        pop esi
        add esp, 0x68
        ret 
    }
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
void PM_StepSlideMoveProne(qboolean gravity) {
    vec3_t startOrigin, startVelocity, up, down;
    trace_t trace;
    float delta;
    VectorCopy(pm->ps->origin, startOrigin); VectorCopy(pm->ps->velocity, startVelocity);
    if (!PM_SlideMoveProne(gravity)) return;
    VectorCopy(startOrigin, down); down[2] -= 18.0f;
    pm->trace(&trace, startOrigin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
    if (pm->ps->velocity[2] > 0 && (trace.fraction == 1.0f || trace.plane.normal[2] < 0.7)) return;
    VectorCopy(startOrigin, up); up[2] += 18.0f;
    pm->trace(&trace, up, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
    if (trace.allsolid) {
        if (pm->debugLevel) Com_Printf("%i:bend can't step\n", c_pmove);
        return;
    }
    VectorCopy(up, pm->ps->origin); VectorCopy(startVelocity, pm->ps->velocity);
    PM_SlideMoveProne(gravity);
    VectorCopy(pm->ps->origin, down); down[2] -= 18.0f;
    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
    if (!trace.allsolid) VectorCopy(trace.endpos, pm->ps->origin);
    if (trace.fraction < 1.0f) PM_ClipVelocity(pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP);
    delta = pm->ps->origin[2] - startOrigin[2];
    if (delta > 2.0f) PM_AddEvent(delta < 7.0f ? EV_STEP_4 : delta < 11.0f ? EV_STEP_8 : delta < 15.0f ? EV_STEP_12 : EV_STEP_16);
    if (pm->debugLevel) Com_Printf("%i:stepped\n", c_pmove);
}

#endif
