/*
 * name:		cg_ents.c
 *
 * desc:		present snapshot entities, happens every single frame
 *
*/


#include "cg_local.h"
#include "../game/tce_trajectory.h"
#include "../game/tce_bg.h"
#include "tce_smoke_grenade.h"
#include "tce_weapon_media.h"
#include "tce_fragment_sound.h"
#include "tce_flash.h"

/*
==========================================================================

FUNCTIONS CALLED EACH FRAME

==========================================================================
*/

/*
======================
CG_SetEntitySoundPosition

Also called by event processing code
======================
*/
void CG_SetEntitySoundPosition( centity_t *cent ) {
	if ( cent->currentState.solid == SOLID_BMODEL ) {
		vec3_t	origin;
		float	*v;

		v = cgs.inlineModelMidpoints[ cent->currentState.modelindex ];
		VectorAdd( cent->lerpOrigin, v, origin );
		trap_S_UpdateEntityPosition( cent->currentState.number, origin );
	} else {
		trap_S_UpdateEntityPosition( cent->currentState.number, cent->lerpOrigin );
	}
}




#define LS_FRAMETIME 100 // (ms)  cycle through lightstyle characters at 10fps


/*
==============
CG_SetDlightIntensity

==============
*/
void CG_AddLightstyle(centity_t *cent)
{
	float lightval;
	int		cl;
	int		r, g, b;
	int		stringlength;
	float	offset;
	int		offsetwhole;
	int		otime;
	int		lastch, nextch;

	if(!cent->dl_stylestring)
		return;

	otime = cg.time - cent->dl_time;
	stringlength = strlen(cent->dl_stylestring);

	// it's been a long time since you were updated, lets assume a reset
	if(otime > 2*LS_FRAMETIME) {
		otime = 0;
		cent->dl_frame = cent->dl_oldframe = 0;
		cent->dl_backlerp = 0;
	}

	cent->dl_time = cg.time;

	offset = ((float)otime)/LS_FRAMETIME;
	offsetwhole = (int)offset;

	cent->dl_backlerp += offset;


	if(cent->dl_backlerp > 1) {						// we're moving on to the next frame
		cent->dl_oldframe	= cent->dl_oldframe + (int)cent->dl_backlerp;
		cent->dl_frame		= cent->dl_oldframe + 1;
		if(cent->dl_oldframe >= stringlength)
		{
			cent->dl_oldframe = (cent->dl_oldframe)%stringlength;
			if(cent->dl_oldframe < 3 && cent->dl_sound)	// < 3 so if an alarm comes back into the pvs it will only start a sound if it's going to be closely synced with the light, otherwise wait till the next cycle
			{
				trap_S_StartSound (NULL, cent->currentState.number, CHAN_AUTO, cgs.gameSounds[cent->dl_sound] );
			}
		}

		if(cent->dl_frame >= stringlength)
			cent->dl_frame = (cent->dl_frame)%stringlength;
			
		cent->dl_backlerp = cent->dl_backlerp - (int)cent->dl_backlerp;
	}


	lastch = cent->dl_stylestring[cent->dl_oldframe] - 'a';
	nextch = cent->dl_stylestring[cent->dl_frame] - 'a';

	lightval = ( lastch * (1.0-cent->dl_backlerp)) + ( nextch * cent->dl_backlerp);
	
	// ydnar: dlight values go from 0-1.5ish
	#if 0
		lightval = ( lightval * (1000.0f/24.0f)) - 200.0f;	// they want 'm' as the "middle" value as 300
		lightval = max(0.0f,	lightval);
		lightval = min(1000.0f,	lightval);
	#else
		lightval *= 0.071429;
		lightval = max( 0.0f,	lightval );
		lightval = min( 20.0f,	lightval );
	#endif
	
	cl = cent->currentState.constantLight;
	r = cl & 255;
	g = ( cl >> 8 ) & 255;
	b = ( cl >> 16 ) & 255;

	//%	trap_R_AddLightToScene( cent->lerpOrigin, lightval, 1.0, (float)r/255.0f, (float)g/255.0f, (float)b/255.0f, 0, 0 );	// overdraw forced to 0 for now
	
	// ydnar: if the dlight has angles, then it is a directional global dlight
	if( cent->currentState.angles[ 0 ] || cent->currentState.angles[ 1 ] || cent->currentState.angles[ 2 ] )
	{
		vec3_t	normal;
		
		
		AngleVectors( cent->currentState.angles, normal, NULL, NULL );
		trap_R_AddLightToScene( normal, 256, lightval,
			(float) r / 255.0f, (float) r / 255.0f, (float) r / 255.0f, 0, REF_DIRECTED_DLIGHT );
	}
	
	// normal global dlight
	else
	{
		trap_R_AddLightToScene( cent->lerpOrigin, 256, lightval,
			(float) r / 255.0f, (float) g / 255.0f, (float) b / 255.0f, 0, 0 );
	}
}


void CG_GetWindVector(vec3_t dir); // JPW NERVE

/*
==================
CG_EntityEffects

Add continuous entity effects, like local entity emission and lighting
==================
*/
static void CG_EntityEffects( centity_t *cent ) {
	static vec3_t dir;
	tce_fragmentSoundContext_t soundContext;
	memset(&soundContext, 0, sizeof(soundContext));
	VectorCopy(cg.refdef_current->vieworg, soundContext.listener);
	soundContext.attenuation = tceFlash.deafness;
	soundContext.distanceVariant = tceSmokeNewBBox;
	soundContext.disabled = cg.tcePortalScopeRendering;
	// update sound origins
	CG_SetEntitySoundPosition( cent );

	// add loop sound
	if ( cent->currentState.loopSound && !cg.tcePortalScopeRendering )
	{
		// ydnar: allow looped sounds to start at trigger time
		if( cent->soundTime == 0 )
			cent->soundTime = trap_S_GetCurrentSoundTime();
		
		if (cent->currentState.eType == ET_SPEAKER) {
			int volume = (int)((1.0 - tceFlash.deafness) * cent->currentState.onFireStart);

			if(cent->currentState.dmgFlags ) {	// range is set
				trap_S_AddRealLoopingSound( cent->lerpOrigin, vec3_origin, cgs.gameSounds[ cent->currentState.loopSound ], cent->currentState.dmgFlags, volume, cent->soundTime );
			} else {
				trap_S_AddRealLoopingSound( cent->lerpOrigin, vec3_origin, cgs.gameSounds[ cent->currentState.loopSound ], 1250, volume, cent->soundTime );
			}
		} else if( cent->currentState.eType == ET_MOVER ) {
			trap_S_AddLoopingSound( cent->lerpOrigin, vec3_origin, cgs.gameSounds[ cent->currentState.loopSound ], TCE_CG_SoundVolume(cent->lerpOrigin, (float)cent->currentState.onFireStart, 1200.f, 0, &soundContext), cent->soundTime );
		} else if ( cent->currentState.solid == SOLID_BMODEL ) {
			vec3_t	origin;
			float	*v;

			v = cgs.inlineModelMidpoints[ cent->currentState.modelindex ];
			VectorAdd( cent->lerpOrigin, v, origin );
			trap_S_AddLoopingSound( origin, vec3_origin, cgs.gameSounds[ cent->currentState.loopSound ], TCE_CG_SoundVolume(origin, (float)cent->currentState.onFireStart, 1200.f, 0, &soundContext), cent->soundTime );
		} else {
			trap_S_AddLoopingSound( cent->lerpOrigin, vec3_origin, cgs.gameSounds[ cent->currentState.loopSound ], TCE_CG_SoundVolume(cent->lerpOrigin, 255.f, 1200.f, 0, &soundContext), cent->soundTime );
		}
	}
	else if( cent->soundTime )
		cent->soundTime = 0;
	
	// constant light glow
	if ( cent->currentState.constantLight ) {
		int		cl;
		int		i, r, g, b;


		if(cent->dl_stylestring[0] != 0)	// it's probably a dlight
		{
			CG_AddLightstyle(cent);
		}
		else
		{
			cl = cent->currentState.constantLight;
			r = cl & 255;
			g = ( cl >> 8 ) & 255;
			b = ( cl >> 16 ) & 255;
			i = ( ( cl >> 24 ) & 255 ) * 4;

			trap_R_AddLightToScene( cent->lerpOrigin, i, 1.0, (float)r/255.0f, (float)g/255.0f, (float)b/255.0f, 0, 0 );
		}
	}

	// Ridah, flaming sounds
	if (CG_EntOnFire(cent) && !cg.tcePortalScopeRendering) {
		// play a flame blow sound when moving
		trap_S_AddLoopingSound( cent->lerpOrigin, vec3_origin, cgs.media.flameBlowSound, (int)(255.0*(1.0-fabs(cent->fireRiseDir[2]))), 0 );
		// play a burning sound when not moving
		trap_S_AddLoopingSound( cent->lerpOrigin, vec3_origin, cgs.media.flameSound, (int)(0.3*255.0*(pow(cent->fireRiseDir[2],2))), 0 );
	}
	/* TC jet entity flag; not the SDK meaning of the same bit. */
	if (cent->currentState.eType == ET_PLAYER &&
		(cent->currentState.eFlags & 0x01000000) && !cg.tcePortalScopeRendering) {
		float motion = VectorLength(cent->tceEntityMotion) * .5f;
		if (motion > 1.f) motion = 1.f;
		else if (motion < 0.f) motion = 0.f;
		trap_S_AddLoopingSound(cent->lerpOrigin, vec3_origin,
			cgs.media.tceJetEngine[0], (int)(motion * 127.5), 0);
		trap_S_AddLoopingSound(cent->lerpOrigin, vec3_origin,
			cgs.media.tceJetEngine[1], (int)((motion * .5 + 1.0) * 170.0), 0);
	}

	// ydnar: overheating is a special effect
	if( (cent->currentState.eFlags & EF_OVERHEATING) == EF_OVERHEATING )
	{
		if( cent->overheatTime < (cg.time - 3000) )
			cent->overheatTime = cg.time;
		if( !(rand() % 3 ) )
		{
			float	alpha;
			vec3_t	muzzle;
			
			if( CG_CalcMuzzlePoint( (cent - cg_entities), muzzle ) )
				muzzle[ 2 ] -= 42.f; /* TC original30092a88, not SDK view height. */
			else
				VectorCopy( cent->lerpOrigin, muzzle );
			alpha = 1.0f - ((float) (cg.time - cent->overheatTime) * (1.f/3000.f));
			alpha *= 0.25f;
			CG_ParticleImpactSmokePuffExtended( cgs.media.smokeParticleShader, muzzle,
				1000, 8, 20, 30, alpha, 8.f );
		}
	}
	
	// DHM - Nerve :: If EF_SMOKING is set, emit smoke
	else if ( cent->currentState.eFlags & EF_SMOKING ) {
		float rnd = random();

		if ( cent->lastTrailTime < cg.time ) {
			int duration;
			float radius, color;
			cent->lastTrailTime = cg.time + 100;

// JPW NERVE -- use wind vector for smoke
			CG_GetWindVector(dir);
			VectorScale(dir,20,dir); // was 75, before that 55
			if (dir[2] < 10)
				dir[2] += 10;
//			dir[0] = crandom() * 10;
//			dir[1] = crandom() * 10;
//			dir[2] = 10 + rnd * 30;
// jpw
			duration = 1500 + rand()%500;
			radius = 15.f + random()*10.f;
			color = .3f + rnd;
			CG_SmokePuff( cent->lerpOrigin, dir, radius,
					color, color, color, 0.4f, duration,
					cg.time, cg.time + 500, 0, cgs.media.smokePuffShader);
		}
	}
	// dhm - end
	
	// JPW NERVE same thing but for smoking barrels instead of nasty server-side effect from single player
	else if( cent->currentState.eFlags & EF_SMOKINGBLACK )
	{
		float rnd = random();

		if ( cent->lastTrailTime < cg.time ) {
			int duration;
			float radius, color;
			cent->lastTrailTime = cg.time + 75;

			CG_GetWindVector(dir);
			VectorScale(dir,50,dir); // was 75, before that 55
			if (dir[2] < 50)
				dir[2] += 50;

			color = rnd*.1f;
			duration = 2800 + rand()%4000;
			radius = 40.f + random()*70.f;
			CG_SmokePuff( cent->lerpOrigin, dir, radius,
					color, color, color, 1, duration,
					cg.time, 0, 0, cgs.media.smokePuffShader);
		}
	}
// jpw



}

void CG_RailTrail2( clientInfo_t *ci, vec3_t start, vec3_t end, int highlighted);

/*
==================
CG_General
==================
*/
static void CG_General( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;
	
	// if set to invisible, skip
	if (!s1->modelindex) {
		return;
	}

	memset (&ent, 0, sizeof(ent));

	// set frame

	ent.frame = s1->frame;
	ent.oldframe = ent.frame;
	ent.backlerp = 0;

	if (ent.frame)
	{
	
		ent.oldframe -= 1;
		ent.backlerp = 1 - cg.frameInterpolation;

		if (cent->currentState.time)
		{
			ent.fadeStartTime = cent->currentState.time;
			ent.fadeEndTime	= cent->currentState.time2;
		}

	}
	
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.hModel = cgs.gameModels[s1->modelindex];

	// player model
	if (s1->number == cg.snap->ps.clientNum) {
		ent.renderfx |= RF_THIRD_PERSON;	// only draw from mirrors
	}

	if( cent->currentState.eType == ET_MG42_BARREL )
	{
		// grab angles from first person user or self if not
		// ATVI Wolfenstein Misc #469 - don't track until viewlocked
		if( cent->currentState.otherEntityNum == cg.snap->ps.clientNum && cg.snap->ps.viewlocked) {
			AnglesToAxis( cg.predictedPlayerState.viewangles, ent.axis );
		} else
			AnglesToAxis( cent->lerpAngles, ent.axis );
	}
	else if( cent->currentState.eType == ET_AAGUN ) {
		// grab angles from first person user or self if not
		if( cent->currentState.otherEntityNum == cg.snap->ps.clientNum && cg.snap->ps.viewlocked) {
			AnglesToAxis( cg.predictedPlayerState.viewangles, ent.axis );
		} else {
			//AnglesToAxis( cg_entities[cent->currentState.otherEntityNum].lerpAngles, ent.axis );
			AnglesToAxis( cent->lerpAngles, ent.axis );
		}
/*		{
			vec3_t v;
			VectorCopy( cent->lerpOrigin, v );
			VectorMA( cent->lerpOrigin, 10, ent.axis[0], v );
			CG_RailTrail2( NULL, cent->lerpOrigin, v, 0 );
			VectorCopy( cent->lerpOrigin, v );
			VectorMA( cent->lerpOrigin, 10, ent.axis[1], v );
			CG_RailTrail2( NULL, cent->lerpOrigin, v, 0 );
			VectorCopy( cent->lerpOrigin, v );
			VectorMA( cent->lerpOrigin, 10, ent.axis[2], v );
			CG_RailTrail2( NULL, cent->lerpOrigin, v, 0 );	
			return;
		}*/
	} else {
		// convert angles to axis
		AnglesToAxis( cent->lerpAngles, ent.axis );
	}

	// scale gamemodels
	if(cent->currentState.eType == ET_GAMEMODEL) {
		VectorScale( ent.axis[0], cent->currentState.angles2[0], ent.axis[0] );
		VectorScale( ent.axis[1], cent->currentState.angles2[1], ent.axis[1] );
		VectorScale( ent.axis[2], cent->currentState.angles2[2], ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;

		if(cent->currentState.apos.trType) {
			ent.reFlags |= REFLAG_ORIENT_LOD;
		}

		if( s1->torsoAnim ) {
			if( cg.time >= cent->lerpFrame.frameTime ) {
				cent->lerpFrame.oldFrameTime = cent->lerpFrame.frameTime;
				cent->lerpFrame.oldFrame = cent->lerpFrame.frame;

				while( cg.time >= cent->lerpFrame.frameTime && 
						// Mad Doc xkan, 11/18/2002 - if teamNum == 1, then we are supposed to stop 
						// the animation when we reach the end of this loop
						// Gordon: 27/11/02: clientNum already does this.
						// xkan, 1/8/2003 - No, it does something a little different.
					   !(s1->teamNum == 1 && 
					     cent->lerpFrame.frame + s1->frame == s1->legsAnim + s1->torsoAnim )) {
					cent->lerpFrame.frameTime += s1->weapon;
					cent->lerpFrame.frame++;

					if( cent->lerpFrame.frame >= s1->legsAnim + s1->torsoAnim ) {
						if( s1->clientNum ) {
							cent->lerpFrame.frame = s1->legsAnim + s1->torsoAnim - 1;
							cent->lerpFrame.oldFrame = s1->legsAnim + s1->torsoAnim -1;
						} else {
							cent->lerpFrame.frame = s1->legsAnim;
						}
					}
				}
			}

			if( cent->lerpFrame.frameTime == cent->lerpFrame.oldFrameTime ) {
				cent->lerpFrame.backlerp = 0;
			} else {
				cent->lerpFrame.backlerp = 1.0 - (float)( cg.time - cent->lerpFrame.oldFrameTime ) / ( cent->lerpFrame.frameTime - cent->lerpFrame.oldFrameTime );
			}

			ent.frame = cent->lerpFrame.frame + s1->frame;	// offset
			if( ent.frame >= s1->legsAnim + s1->torsoAnim ) {
				ent.frame -= s1->torsoAnim;
			}

			ent.oldframe = cent->lerpFrame.oldFrame + s1->frame;	// offset
			if( ent.oldframe >= s1->legsAnim + s1->torsoAnim ) {
				ent.oldframe -= s1->torsoAnim;
			}

			ent.backlerp = cent->lerpFrame.backlerp;

//			CG_Printf( "Gamemodel: oldframe: %i frame: %i lerp: %f\n", ent.oldframe, ent.frame, ent.backlerp );
		}

		// xkan, 11/27/2002 - only advance/change frame if the game model has not 
		// been stopped (teamNum != 1)
		if(cent->trailTime && s1->teamNum != 1) {
			cent->lerpFrame.oldFrame = cent->lerpFrame.frame;
			cent->lerpFrame.frame = s1->legsAnim;

			cent->lerpFrame.oldFrameTime = cent->lerpFrame.frameTime;
			cent->lerpFrame.frameTime = cg.time;

			ent.oldframe = ent.frame;
			ent.frame = s1->legsAnim;
			ent.backlerp = 0;			
		}

		if(cent->nextState.animMovetype != s1->animMovetype) {
			cent->trailTime = 1;
		} else {
			cent->trailTime = 0;
		}

		if( s1->modelindex2 ) {
			ent.customSkin = cgs.gameModelSkins[s1->modelindex2];
		}
	}

	// special shader if under construction
	if( cent->currentState.powerups == STATE_UNDERCONSTRUCTION ) {
		/*if( cent->currentState.solid == SOLID_BMODEL ) {
			ent.customShader = cgs.media.genericConstructionShaderBrush;
		} else {
			ent.customShader = cgs.media.genericConstructionShaderModel;
		}*/
		ent.customShader = cgs.media.genericConstructionShader;
	}

	// add to refresh list
	trap_R_AddRefEntityToScene (&ent);

	memcpy( &cent->refEnt, &ent, sizeof(refEntity_t) );
}

/*
==================
CG_Speaker

Speaker entities can automatically play sounds
==================
*/
static void CG_Speaker( centity_t *cent ) {
	int volume, delayBase, delayRange;
	double randomDelay;
	if (cg.tcePortalScopeRendering) {
		return;
	}
	if ( ! cent->currentState.clientNum ) {	// FIXME: use something other than clientNum...
		return;		// not auto triggering
	}

	if ( cg.time < cent->miscTime ) {
		return;
	}

	volume = (int)((1.0 - tceFlash.deafness) * 127.0);
	trap_S_StartSoundVControl(NULL, cent->currentState.number, CHAN_ITEM,
		cgs.gameSounds[cent->currentState.eventParm], volume);

	//	ent->s.frame = ent->wait * 10;
	//	ent->s.clientNum = ent->random * 10;
	/* Original x87 keeps the random scale product wider than float. */
	delayBase = cg.time + cent->currentState.frame * 100;
	delayRange = cent->currentState.clientNum * 100;
	randomDelay = ((rand() & 32767) * (double)3.0518509447574615e-05f - 0.5) * 2.0;
	cent->miscTime = (int)(delayBase + delayRange * randomDelay);
}

qboolean CG_PlayerSeesItem(playerState_t *ps, entityState_t *item, int atTime, int itemType)
{
	vec3_t	vorigin, eorigin, viewa, dir;
	float	dot, dist, foo;
#if defined(_MSC_VER) && defined(_M_IX86)
    static const float maxDistance = 255.f, coneBase = -0.94f;
    static const float coneScale = 1.f/255.f, coneSlope = 0.057f, one = 1.f;
    int reject;
    float traceFraction;
#endif
	trace_t		tr;

	BG_EvaluateTrajectory( &item->pos, atTime, eorigin, qfalse, item->effect2Time );

	VectorCopy(ps->origin, vorigin);
	vorigin[2] = (float)((double)ps->viewheight + ps->origin[2]);			// get the view loc up to the viewheight
//	eorigin[2] += 8;						// and subtract the item's offset (that is used to place it on the ground)


	VectorSubtract(vorigin, eorigin, dir);	

#if defined(_MSC_VER) && defined(_M_IX86)
	/* Keep the normalize return in ST0 for the original distance gate. */
	__asm {
		lea eax, dir
		push eax
		call VectorNormalize
		add esp, 4
		fst dword ptr [dist]
		fcomp dword ptr [maxDistance]
		fnstsw ax
		and eax, 4100h
		mov reject, eax
	}
	if (!reject) return qfalse;
#else
	dist = VectorNormalize(dir);
	if (dist > 255) return qfalse;
#endif

	// (SA) FIXME: do this without AngleVectors.
	//		It'd be nice if the angle vectors for the player
	//		have already been figured at this point and I can
	//		just pick them up.  (if anybody is storing this somewhere,
	//		for the current frame please let me know so I don't
	//		have to do redundant calcs)
	AngleVectors(ps->viewangles, viewa, 0, 0);
#if defined(_MSC_VER) && defined(_M_IX86)
	/* Original z+y+x dot and unspilled cone threshold, C0 rejection. */
	__asm {
		fld dword ptr [viewa+8]
		fmul dword ptr [dir+8]
		fld dword ptr [viewa+4]
		fmul dword ptr [dir+4]
		faddp st(1), st(0)
		fld dword ptr [viewa]
		fmul dword ptr [dir]
		faddp st(1), st(0)
		fld dword ptr [dist]
		fmul dword ptr [coneScale]
		fmul dword ptr [coneSlope]
		fsubr dword ptr [coneBase]
		fcompp
		fnstsw ax
		and eax, 100h
		mov reject, eax
	}
	if (reject) return qfalse;
#else
	dot = DotProduct(viewa, dir);
	foo = -0.94f - (dist*(1.0f/255.0f)) * 0.057f;
	if (dot > foo) return qfalse;
#endif

	// (SA) okay, everything else is okay, so do a bloody trace. (so coronas on treasure doesn't show through walls) <sigh>
	if(itemType == IT_TREASURE) {
		CG_Trace(&tr, vorigin, NULL, NULL, eorigin, -1, MASK_SOLID);

#if defined(_MSC_VER) && defined(_M_IX86)
		traceFraction = tr.fraction;
		__asm {
			fld dword ptr [traceFraction]
			fcomp dword ptr [one]
			fnstsw ax
			and eax, 4000h
			mov reject, eax
		}
		if (!reject) return qfalse;
#else
		if (tr.fraction != 1) return qfalse;
#endif
	}

	return qtrue;

}


/*
==================
CG_Item
==================
*/
/* Complete TC item renderer; shared as a separate unit for original tests. */
void TCE_CG_Item(centity_t *cent);

//============================================================================

/*
===============
CG_Bomb
===============
*/

static void CG_Bomb( centity_t *cent ) {
	refEntity_t		ent, beam;
	entityState_t	*s1;
	const tce_weaponInfo_t	*weapon;
	vec3_t			end;
	trace_t			trace;

	memset(&ent, 0, sizeof(ent));

	s1 = &cent->currentState;

	/* TC30030780: media base3484a080 +29*0xfd0, not the SDK bank. */
	weapon = &tce_cg_weapons[29];

	VectorCopy( s1->origin2, ent.axis[0] );
	PerpendicularVector( ent.axis[1], ent.axis[0] );
	CrossProduct( ent.axis[0], ent.axis[1], ent.axis[2] );

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.hModel = weapon->missileModel;
	ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

	CG_AddRefEntityWithPowerups( &ent, s1->powerups, TEAM_FREE, s1, vec3_origin );

	
	memset(&beam, 0, sizeof(beam));

	VectorCopy( cent->lerpOrigin, beam.origin );
	VectorMA( cent->lerpOrigin, 4096, s1->origin2, end);

	
	trap_CM_BoxTrace(&trace, cent->lerpOrigin, end, NULL, NULL, 0, MASK_SHOT);

	VectorCopy(trace.endpos, beam.oldorigin);

	beam.reType = RT_RAIL_CORE;
	beam.renderfx = RF_NOSHADOW;
	beam.customShader = cgs.media.railCoreShader;
	beam.shaderRGBA[0] = 255;
	beam.shaderRGBA[1] = 0;
	beam.shaderRGBA[2] = 0;
	beam.shaderRGBA[3] = 255;


	AxisClear( beam.axis );

	trap_R_AddRefEntityToScene( &beam );
}

/*
===============
CG_Smoker
===============
*/
static void CG_Smoker( centity_t *cent ) {
	// this ent has some special setting up
	// time = speed
	// time2 = duration
	// angles2[0] = start_size
	// angles2[1] = end_size
	// angles2[2] = wait
	// dl_intensity = health
	// constantLight = delay
	// origin2 = normal to emit particles along

	if( cg.time - cent->highlightTime > cent->currentState.constantLight ) {
		// FIXME: make this framerate independant?
		cent->highlightTime = cg.time;	// fire a particle this frame
        
		if(cent->currentState.modelindex2) {
			CG_ParticleSmoke (cgs.gameShaders[cent->currentState.modelindex2], cent);
		} else if (cent->currentState.density == 3) { // cannon
			CG_ParticleSmoke (cgs.media.smokePuffShaderdirty, cent);
		} else if (!(cent->currentState.density)) {
			CG_ParticleSmoke (cgs.media.smokePuffShader, cent);
		} else {
			CG_ParticleSmoke (cgs.media.smokePuffShader, cent);
		}
	}
	
	cent->lastTrailTime = cg.time;	// time we were last received at the client
}

/*
===============
CG_Missile
===============
*/


extern void CG_RocketTrail( centity_t *ent, const weaponInfo_t *wi );

/* TC30030a10 is implemented in tce_missile.c using TC media and IDs. */

// DHM - Nerve :: capture and hold flag
static animation_t multi_flagpoleAnims[] = {
	{ 0,	"",	0,		1,		0,		1000/15,	1000/15	},	// (no flags, idle)
	{ 0,	"",	0,		15,		0,		1000/15,	1000/15	},	// (axis flag rising)
	{ 0,	"",	490,	15,		0,		1000/15,	1000/15	},	// (american flag rising)
	{ 0,	"",	20,		211,	211,	1000/15,	1000/15	},	// (axis flag raised)
	{ 0,	"",	255,	211,	211,	1000/15,	1000/15	},	// (american flag raised)
	{ 0,	"",	235,	15,		0,		1000/15,	1000/15	},	// (axis switching to american)
	{ 0,	"",	470,	15,		0,		1000/15,	1000/15	},	// (american switching to axis)
	{ 0,	"",	510,	15,		0,		1000/15,	1000/15	},	// (axis flag falling)
	{ 0,	"",	530,	15,		0,		1000/15,	1000/15	}	// (american flag falling)
};

// dhm - end

extern void CG_RunLerpFrame( centity_t *cent, clientInfo_t *ci, lerpFrame_t *lf, int newAnimation, float speedScale );


/*
==============
CG_TrapSetAnim
==============
*/
static void CG_TrapSetAnim(centity_t *cent, lerpFrame_t *lf, int newAnim) {
	// transition animation
	lf->animationNumber = cent->currentState.frame;

	// DHM - Nerve :: teamNum specifies which set of animations to use (only 1 exists right now)
	switch ( cent->currentState.teamNum ) {
	case 1:
		lf->animation = &multi_flagpoleAnims[ cent->currentState.frame ];
		break;
	default:
		return;
	}

	lf->animationTime = lf->frameTime + lf->animation->initialLerp;
}

/*
==============
CG_Trap
	// TODO: change from 'trap' to something else.  'trap' is a misnomer.  it's actually used for other stuff too
==============
*/
static void CG_Trap(centity_t *cent) {
	refEntity_t			ent;
	entityState_t		*cs;
	lerpFrame_t			*traplf;

	memset (&ent, 0, sizeof(ent));

	cs = &cent->currentState;

	traplf = &cent->lerpFrame;

	// initial setup
	if(!traplf->oldFrameTime) {
		traplf->frameTime		=
		traplf->oldFrameTime	= cg.time;

		CG_TrapSetAnim(cent, traplf, cs->frame);

		traplf->frame			=
		traplf->oldFrame		= traplf->animation->firstFrame;
	}

	// transition to new anim if requested
	if((traplf->animationNumber != cs->frame) || !traplf->animation) {
		CG_TrapSetAnim(cent, traplf, cs->frame);
	}

	CG_RunLerpFrame(cent, NULL, traplf, 0, 1);	// use existing lerp code rather than re-writing

	ent.frame		= traplf->frame;
	ent.oldframe	= traplf->oldFrame;
	ent.backlerp	= traplf->backlerp;

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.hModel = cgs.gameModels[cs->modelindex];

	AnglesToAxis( cent->lerpAngles, ent.axis );

	trap_R_AddRefEntityToScene (&ent);

	memcpy( &cent->refEnt, &ent, sizeof(refEntity_t) );
}
//----(SA)	end


/*
==============
CG_Corona
==============
*/
#include "tce_corona.inc"

/*
==============
CG_Efx
==============
*/
static void CG_SpotlightEfx( centity_t *cent ) {
	vec3_t	targetpos, normalized_direction, direction;
	float	dist, fov = 90;
	vec4_t	color = {1, 1, 1, .1};
	int		splinetarget = 0;
	char	*cs;
	
	
	VectorCopy(cent->currentState.origin2, targetpos);

	splinetarget = cent->overheatTime;

	if(!splinetarget) {
		cs = (char *)CG_ConfigString(CS_SPLINES + cent->currentState.density);
		cent->overheatTime = splinetarget = CG_LoadCamera(va("cameras/%s.camera", cs));
		if(splinetarget != -1) {
			trap_startCamera(splinetarget, cg.time);
		}
	} else {
		vec3_t angles;
		if(splinetarget != -1) {
			if(trap_getCameraInfo(splinetarget, cg.time, &targetpos, &angles, &fov)) {

			} else {	// loop
				trap_startCamera(splinetarget, cg.time);
				trap_getCameraInfo(splinetarget, cg.time, &targetpos, &angles, &fov);
			}
		}
	}


	normalized_direction[0] = direction[0] = targetpos[0] - cent->currentState.origin[0];
	normalized_direction[1] = direction[1] = targetpos[1] - cent->currentState.origin[1];
	normalized_direction[2] = direction[2] = targetpos[2] - cent->currentState.origin[2];

	dist = VectorNormalize( normalized_direction );

	if ( dist == 0 )
		return;

	CG_Spotlight( cent, color, cent->currentState.origin, normalized_direction, 999, 2048, 10, fov, 0);
}


//----(SA) adding func_explosive

/*
===============
CG_Explosive
	This is currently almost exactly the same as CG_Mover
	It's split out so that any changes or experiments are
	unattached to anything else.
===============
*/
static void CG_Explosive( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;


	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
//	VectorCopy( cent->lerpOrigin, ent.oldorigin);
//	VectorCopy( ent.origin, cent->lerpOrigin);
	AnglesToAxis( cent->lerpAngles, ent.axis );

	ent.renderfx = RF_NOSHADOW;

	// get the model, either as a bmodel or a modelindex
	if ( s1->solid == SOLID_BMODEL ) {
		ent.hModel = cgs.inlineDrawModel[s1->modelindex];
	} else {
		ent.hModel = cgs.gameModels[s1->modelindex];
	}

	// add to refresh list
	// trap_R_AddRefEntityToScene(&ent);

	// add the secondary model
	if ( s1->modelindex2 ) {
		ent.skinNum = 0;
		ent.hModel = cgs.gameModels[s1->modelindex2];
		trap_R_AddRefEntityToScene(&ent);
	}
	else
		trap_R_AddRefEntityToScene(&ent);

}

/*
===============
CG_Constructible
	This is currently almost exactly the same as CG_Mover
	It's split out so that any changes or experiments are
	unattached to anything else.
===============
*/
static void CG_Constructible( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);
//	VectorCopy( ent.origin, cent->lerpOrigin);
	AnglesToAxis( cent->lerpAngles, ent.axis );

	ent.renderfx = RF_NOSHADOW;

//	CG_Printf( "Adding constructible: %s\n", CG_ConfigString( CS_OID_NAMES + cent->currentState.otherEntityNum2 ) );

	if( s1->modelindex ) {
	// get the model, either as a bmodel or a modelindex
		//if ( s1->solid == SOLID_BMODEL ) {
		ent.hModel = cgs.inlineDrawModel[s1->modelindex];
		//} else {
		//	ent.hModel = cgs.gameModels[s1->modelindex];
		//}

	// add to refresh list
	// trap_R_AddRefEntityToScene(&ent);

	//	ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = 0xff;
	//	ent.shaderRGBA[3] = s1->density;

		//if( s1->angles2[0] < 255 )
		//if( cent->currentState.powerups == STATE_UNDERCONSTRUCTION )
		//	ent.customShader = cgs.media.genericConstructionShaderBrush;

		trap_R_AddRefEntityToScene(&ent);
	}

	// add the secondary model
	if ( s1->modelindex2 ) {
		if( cent->currentState.powerups == STATE_UNDERCONSTRUCTION ) {
			//ent.customShader = cgs.media.genericConstructionShaderBrush;
			ent.customShader = cgs.media.genericConstructionShader;

			/*switch( cent->currentState.frame ) {
				case 1: trap_S_AddLoopingSound( cent->currentState.origin2, vec3_origin, cgs.media.buildSound[0], 255, 0 ); break;
				case 2: trap_S_AddLoopingSound( cent->currentState.origin2, vec3_origin, cgs.media.buildSound[1], 255, 0 ); break;
				case 3: trap_S_AddLoopingSound( cent->currentState.origin2, vec3_origin, cgs.media.buildSound[2], 255, 0 ); break;
				case 4: trap_S_AddLoopingSound( cent->currentState.origin2, vec3_origin, cgs.media.buildSound[3], 255, 0 ); break;
			}*/
		}

		//if ( s1->solid == SOLID_BMODEL ) {
			ent.hModel = cgs.inlineDrawModel[s1->modelindex2];
		//} else {
		//	ent.hModel = cgs.gameModels[s1->modelindex2];
		//}
		trap_R_AddRefEntityToScene(&ent);
	}
	//else
	//	trap_R_AddRefEntityToScene(&ent);
}

/*
===============
CG_Waypoint
===============
*/
/*
static void CG_Waypoint( centity_t *cent ) {
	refEntity_t ent;

	memset( &ent, 0, sizeof( ent ) );
	ent.reType = RT_SPRITE;
	VectorCopy( cent->lerpOrigin, ent.origin );
	ent.origin[2] += 24;
	VectorCopy( ent.origin, ent.oldorigin );
	ent.radius = 14;

	switch( cent->currentState.frame ) {
		case WAYP_ATTACK: ent.customShader = cgs.media.waypointAttackShader; break;
		case WAYP_DEFEND: ent.customShader = cgs.media.waypointDefendShader; break;
		case WAYP_REGROUP: ent.customShader = cgs.media.waypointRegroupShader; break;
		// TAT 8/29/2002 - Use the bot shader for bots
		case WAYP_BOT: 
			ent.customShader = cgs.media.waypointBotShader;
			break;
		// TAT 1/13/2003 - and the queued shader for queued commands
		case WAYP_BOTQUEUED:
			ent.customShader = cgs.media.waypointBotQueuedShader;
			break;
	}

	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 255;
	trap_R_AddRefEntityToScene(&ent);
}
*/
/*
===============
CG_ConstructibleMarker
===============
*/
/*static void CG_ConstructibleMarker( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	// special shader if under construction
	if( cent->currentState.powerups == STATE_UNDERCONSTRUCTION )
		ent.customShader = cgs.media.genericConstructionShader;

	// add the secondary model
	if ( s1->modelindex2 ) {
		AnglesToAxis( cent->lerpAngles, ent.axis );
		ent.skinNum = 0;
		ent.hModel = cgs.gameModels[s1->modelindex2];
		ent.frame = s1->frame;
		if( s1->effect1Time )
			ent.customSkin = cgs.gameModelSkins[s1->effect1Time];
		trap_R_AddRefEntityToScene(&ent);
		memcpy( &cent->refEnt, &ent, sizeof(refEntity_t) );
	} else {
		AnglesToAxis( vec3_origin, ent.axis );
		trap_R_AddRefEntityToScene(&ent);
	}
}*/

//----(SA) done

// declaration for add bullet particles (might as well stick this one in a .h file I think)
extern void CG_TCEAddLocalBulletSparks( vec3_t origin, vec3_t dir, int speed, int duration, int count, float randScale );

/*
===============
CG_Mover
===============
*/
static void CG_Mover( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	AnglesToAxis( cent->lerpAngles, ent.axis );
	
	ent.renderfx = 0;
	ent.skinNum = 0;

	// get the model, either as a bmodel or a modelindex
	if ( s1->solid == SOLID_BMODEL ) {
		ent.hModel = cgs.inlineDrawModel[s1->modelindex];
	} else {
		ent.hModel = cgs.gameModels[s1->modelindex];
	}
	
	// Rafael 
	//  testing for mike to get movers to scale
	if(cent->currentState.density & 1) {
		VectorScale( ent.axis[0], cent->currentState.angles2[0], ent.axis[0] );
		VectorScale( ent.axis[1], cent->currentState.angles2[1], ent.axis[1] );
		VectorScale( ent.axis[2], cent->currentState.angles2[2], ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
	}


	if(	cent->currentState.eType == ET_ALARMBOX) {
		ent.renderfx |= RF_MINLIGHT;
	}


	// special shader if under construction
	if( cent->currentState.powerups == STATE_UNDERCONSTRUCTION ) {
		/*if( cent->currentState.solid == SOLID_BMODEL ) {
			ent.customShader = cgs.media.genericConstructionShaderBrush;
		} else {
			ent.customShader = cgs.media.genericConstructionShaderModel;
		}*/
		ent.customShader = cgs.media.genericConstructionShader;
	}

	// add the secondary model
	if ( s1->modelindex2 && !(cent->currentState.density & 2)) {
		ent.hModel = cgs.gameModels[s1->modelindex2];
		ent.frame = s1->frame;

		if( s1->torsoAnim ) {
			if( cg.time >= cent->lerpFrame.frameTime ) {
				cent->lerpFrame.oldFrameTime = cent->lerpFrame.frameTime;
				cent->lerpFrame.oldFrame = cent->lerpFrame.frame;

				while( cg.time >= cent->lerpFrame.frameTime ) {
					cent->lerpFrame.frameTime += s1->weapon;
					cent->lerpFrame.frame++;

					if( cent->lerpFrame.frame >= s1->legsAnim + s1->torsoAnim ) {
						cent->lerpFrame.frame = s1->legsAnim;						
					}
				}
			}

			if( cent->lerpFrame.frameTime == cent->lerpFrame.oldFrameTime ) {
				cent->lerpFrame.backlerp = 0;
			} else {
				cent->lerpFrame.backlerp = 1.0 - (float)( cg.time - cent->lerpFrame.oldFrameTime ) / ( cent->lerpFrame.frameTime - cent->lerpFrame.oldFrameTime );
			}

			ent.frame = cent->lerpFrame.frame + s1->frame;	// offset
			if( ent.frame >= s1->legsAnim + s1->torsoAnim )
				ent.frame -= s1->torsoAnim;

			ent.oldframe = cent->lerpFrame.oldFrame + s1->frame;	// offset
			if( ent.oldframe >= s1->legsAnim + s1->torsoAnim )
				ent.oldframe -= s1->torsoAnim;

			ent.backlerp = cent->lerpFrame.backlerp;
		}

		if(cent->trailTime) {
			cent->lerpFrame.oldFrame = cent->lerpFrame.frame;
			cent->lerpFrame.frame = s1->legsAnim;

			cent->lerpFrame.oldFrameTime = cent->lerpFrame.frameTime;
			cent->lerpFrame.frameTime = cg.time;

			ent.oldframe = ent.frame;
			ent.frame = s1->legsAnim;
			ent.backlerp = 0;			
		}

		if(cent->nextState.animMovetype != s1->animMovetype) {
			cent->trailTime = 1;
		} else {
			cent->trailTime = 0;
		}

		trap_R_AddRefEntityToScene(&ent);
		memcpy( &cent->refEnt, &ent, sizeof(refEntity_t) );
	} else {
		trap_R_AddRefEntityToScene(&ent);
	}

	// alarm box spark effects
/*	if(	cent->currentState.eType == ET_ALARMBOX) {
		if(cent->currentState.frame == 2 ) {	// i'm dead
			if(rand()%50 == 1) {
				vec3_t	angNorm;				// normalized angles
				VectorNormalize2(cent->lerpAngles, angNorm);
				//		(origin, dir, speed, duration, count, 'randscale')
				CG_TCEAddLocalBulletSparks( cent->lerpOrigin, angNorm, 2, 800, 4, 16.0f );
				trap_S_StartSound (NULL, cent->currentState.number, CHAN_AUTO, cgs.media.sparkSounds );
			}
		}
	}*/
}

void CG_Mover_PostProcess( centity_t* cent ) {
	refEntity_t mg42base;
	refEntity_t mg42upper;
	refEntity_t mg42gun;
	refEntity_t player;
	refEntity_t flash;
	vec_t* angles;
	int i;

	if( !(cent->currentState.density & 4) ) { // mounted gun
		return;
	}


	if( cg.snap->ps.eFlags & EF_MOUNTEDTANK && cg_entities[cg.snap->ps.clientNum].tagParent == cent->currentState.effect3Time ) {
		i = cg.snap->ps.clientNum;
	} else 
		for( i = 0; i < MAX_CLIENTS; i++ ) {
			// Gordon: is this entity mounted on a tank, and attached to _OUR_ turret entity (which could be us)
			if( cg_entities[i].currentValid && (cg_entities[i].currentState.eFlags & EF_MOUNTEDTANK) ) {
				if( cg_entities[i].tagParent == cent->currentState.effect3Time ) {
					break;
				}
			}
		}

	if(i != MAX_CLIENTS) {
		if( i != cg.snap->ps.clientNum ) {
			angles = cg_entities[i].lerpAngles;
		} else {
			angles = cg.predictedPlayerState.viewangles;
		}
	} else {
		angles = vec3_origin;
	}

	cg_entities[cent->currentState.effect3Time].tankparent = cent-cg_entities;
	CG_AttachBitsToTank( &cg_entities[cent->currentState.effect3Time], &mg42base, &mg42upper, &mg42gun, &player, &flash, angles, "tag_player", cent->currentState.density & 8 ? qtrue : qfalse );

	// Gordon: if we (or someone we're spectating) is on this tank, recalc our view values
	if( cg.snap->ps.eFlags & EF_MOUNTEDTANK ) {
		centity_t* tank = &cg_entities[cg_entities[cg.snap->ps.clientNum].tagParent];
		if( tank == &cg_entities[cent->currentState.effect3Time] ) {
			CG_CalcViewValues();
		}
	}

	VectorCopy( mg42base.origin, mg42base.lightingOrigin );
	VectorCopy( mg42base.origin, mg42base.oldorigin );
	
	VectorCopy( mg42upper.origin, mg42upper.lightingOrigin );
	VectorCopy( mg42upper.origin, mg42upper.oldorigin );

	VectorCopy( mg42gun.origin, mg42gun.lightingOrigin );
	VectorCopy( mg42gun.origin, mg42gun.oldorigin );

	trap_R_AddRefEntityToScene( &mg42base );

	if( i != cg.snap->ps.clientNum || cg.renderingThirdPerson ) {
		trap_R_AddRefEntityToScene( &mg42upper );
		trap_R_AddRefEntityToScene( &mg42gun );
	} 
}



/*
===============
CG_Beam_2

Gordon: new beam entity, for rope like stuff...
===============
*/
void CG_Beam_2( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;
	vec3_t				origin, origin2;

	s1 = &cent->currentState;

	BG_EvaluateTrajectory( &s1->pos, cg.time, origin, qfalse, s1->effect1Time );
	BG_EvaluateTrajectory( &s1->apos, cg.time, origin2, qfalse, s1->effect2Time );

	// create the render entity
	memset (&ent, 0, sizeof(ent));

	VectorCopy( origin, ent.origin );
	VectorCopy( origin2, ent.oldorigin );

//	CG_Printf( "O: %i %i %i OO: %i %i %i\n", (int)origin[0], (int)origin[1], (int)origin[2], (int)origin2[0], (int)origin2[1], (int)origin2[2] );
	AxisClear( ent.axis );
	ent.reType = RT_RAIL_CORE;
	ent.customShader = cgs.gameShaders[s1->modelindex2];
	ent.radius = 8;
	ent.frame = 2;

	VectorScale( cent->currentState.angles2, 255, ent.shaderRGBA );
	ent.shaderRGBA[3] = 255;

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);
}

/*
===============
CG_Beam

Also called as an event
===============
*/
void CG_Beam( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( s1->pos.trBase, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	
	AxisClear( ent.axis );
	ent.reType = RT_RAIL_CORE;

	switch(s1->legsAnim) {
		case 1:
			ent.customShader = cgs.media.ropeShader;
			break;
		default:
			ent.customShader = cgs.media.railCoreShader;
			break;
	}

	ent.shaderRGBA[0] = s1->angles2[0] * 255;
	ent.shaderRGBA[1] = s1->angles2[1] * 255;
	ent.shaderRGBA[2] = s1->angles2[2] * 255;
	ent.shaderRGBA[3] = 255;

	ent.renderfx = RF_NOSHADOW;

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);
}


/*
===============
CG_Portal
===============
*/
static void CG_Portal( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	ByteToDir( s1->eventParm, ent.axis[0] );
	PerpendicularVector( ent.axis[1], ent.axis[0] );

	// negating this tends to get the directions like they want
	// we really should have a camera roll value
	VectorSubtract( vec3_origin, ent.axis[1], ent.axis[1] );

	CrossProduct( ent.axis[0], ent.axis[1], ent.axis[2] );
	ent.reType = RT_PORTALSURFACE;
	ent.frame = s1->frame;		// rotation speed
	ent.skinNum = s1->clientNum/256.0 * 360;	// roll offset

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);
}

/*
===============
CG_Prop
===============
*/
static void CG_Prop( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;
	vec3_t				angles;
	float				scale;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));

	if (cg.renderingThirdPerson)
	{
		VectorCopy( cent->lerpOrigin, ent.origin);
		VectorCopy( cent->lerpOrigin, ent.oldorigin);

		ent.frame = s1->frame;
		ent.oldframe = ent.frame;
		ent.backlerp = 0;			
	}
	else
	{
		VectorCopy (cg.refdef_current->vieworg, ent.origin);
		VectorCopy (cg.refdefViewAngles, angles);

		if ( cg.bobcycle & 1 ) {
			scale = -cg.xyspeed;
		} else {
			scale = cg.xyspeed;
		}

		// modify angles from bobbing
		angles[ROLL] += scale * cg.bobfracsin * 0.005;
		angles[YAW] += scale * cg.bobfracsin * 0.01;
		angles[PITCH] += cg.xyspeed * cg.bobfracsin * 0.005;

		VectorCopy (angles, cent->lerpAngles);
	
		ent.frame = s1->frame;
		ent.oldframe = ent.frame;
		ent.backlerp = 0;
		
		if (cent->currentState.density)
		{
			ent.frame = s1->frame + cent->currentState.density;
			ent.oldframe = ent.frame - 1;
			ent.backlerp = 1 - cg.frameInterpolation;
			ent.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON;

			//CG_Printf ("frame %d oldframe %d\n", ent.frame, ent.oldframe);
		}
		else if (ent.frame)
		{
			ent.oldframe -= 1;
			ent.backlerp = 1 - cg.frameInterpolation;
		}
		else 
		{
			ent.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON;
		}
	}

	AnglesToAxis( cent->lerpAngles, ent.axis );

	ent.renderfx |= RF_NOSHADOW;

	// flicker between two skins (FIXME?)
	ent.skinNum = ( cg.time >> 6 ) & 1;

	// get the model, either as a bmodel or a modelindex
	if ( s1->solid == SOLID_BMODEL ) {
		ent.hModel = cgs.inlineDrawModel[s1->modelindex];
	} else {
		ent.hModel = cgs.gameModels[s1->modelindex];
	}

	// special shader if under construction
	if( cent->currentState.powerups == STATE_UNDERCONSTRUCTION ) {
		/*if( cent->currentState.solid == SOLID_BMODEL ) {
			ent.customShader = cgs.media.genericConstructionShaderBrush;
		} else {
			ent.customShader = cgs.media.genericConstructionShaderModel;
		}*/
		ent.customShader = cgs.media.genericConstructionShader;
	}

	// add the secondary model
	if ( s1->modelindex2 ) {
		ent.skinNum = 0;
		ent.hModel = cgs.gameModels[s1->modelindex2];
		ent.frame = s1->frame;
		trap_R_AddRefEntityToScene(&ent);
		memcpy( &cent->refEnt, &ent, sizeof(refEntity_t) );
	}
	else
		trap_R_AddRefEntityToScene(&ent);

}

typedef enum cabinetType_e {
	CT_AMMO,
	CT_HEALTH,
	CT_MAX,
} cabinetType_t;

#define MAX_CABINET_TAGS 6
typedef struct cabinetTag_s {
	const char* tagsnames[MAX_CABINET_TAGS];

	const char* itemnames[MAX_CABINET_TAGS];
	qhandle_t	itemmodels[MAX_CABINET_TAGS];

	const char* modelName;
	qhandle_t	model;
} cabinetTag_t;

cabinetTag_t cabinetInfo[CT_MAX] = {
	{
		{
			"tag_ammo01",
			"tag_ammo02",
			"tag_ammo03",
			"tag_ammo04",
			"tag_ammo05",
			"tag_ammo06",
/*			"tag_obj1",
			"tag_obj1",
			"tag_obj1",
			"tag_obj1",
			"tag_obj1",
			"tag_obj1",*/
		},
		{
			"models/multiplayer/supplies/ammobox_wm.md3",
			"models/multiplayer/supplies/ammobox_wm.md3",
			"models/multiplayer/supplies/ammobox_wm.md3",
			"models/multiplayer/supplies/ammobox_wm.md3",
			"models/multiplayer/supplies/ammobox_wm.md3",
			"models/multiplayer/supplies/ammobox_wm.md3",
		},
		{
			0, 0, 0, 0, 0, 0
		},
		"models/mapobjects/supplystands/stand_ammo.md3",
//		"models/mapobjects/blitz_sd/blitzbody.md3",
		0,
	},
	{
		{
			"tag_Medikit_01",
			"tag_Medikit_02",
			"tag_Medikit_03",
			"tag_Medikit_04",
			"tag_Medikit_05",
			"tag_Medikit_06",
		},
		{
			"models/multiplayer/supplies/healthbox_wm.md3",
			"models/multiplayer/supplies/healthbox_wm.md3",
			"models/multiplayer/supplies/healthbox_wm.md3",
			"models/multiplayer/supplies/healthbox_wm.md3",
			"models/multiplayer/supplies/healthbox_wm.md3",
			"models/multiplayer/supplies/healthbox_wm.md3",
		},
		{
			0, 0, 0, 0, 0, 0
		},
		"models/mapobjects/supplystands/stand_health.md3",
		0,
	},
};

/*
=========================
CG_Cabinet
=========================
*/
void CG_Cabinet( centity_t* cent, cabinetType_t type ) {
	refEntity_t cabinet;
	refEntity_t mini_me;
	int i, cnt;
//	int k;

	if(type < 0 || type >= CT_MAX) {
		return;
	}

	memset( &cabinet, 0, sizeof(cabinet) );
	memset( &mini_me, 0, sizeof(mini_me) );
	
	cabinet.hModel =	cabinetInfo[type].model;
//	cabinet.hModel =	cabinetInfo[type].itemmodels[0];	
	cabinet.frame =		0;
	cabinet.oldframe =	0;
	cabinet.backlerp =	0.f;

	VectorCopy( cent->lerpOrigin, cabinet.origin );
	VectorCopy( cabinet.origin, cabinet.oldorigin );
	VectorCopy( cabinet.origin, cabinet.lightingOrigin );
	cabinet.lightingOrigin[2] += 16;
	cabinet.renderfx |= RF_MINLIGHT;
	AnglesToAxis( cent->lerpAngles, cabinet.axis );

	if(cent->currentState.onFireStart == -9999) {
		cnt = MAX_CABINET_TAGS;
	} else {
		cnt = MAX_CABINET_TAGS * (cent->currentState.onFireStart / (float)cent->currentState.onFireEnd);
		if(cnt == 0 && cent->currentState.onFireStart) {
			cnt = 1;
		}
	}

	for( i = 0; i < cnt; i++ ) {
		mini_me.hModel = cabinetInfo[type].itemmodels[i];

		CG_PositionEntityOnTag( &mini_me, &cabinet, cabinetInfo[type].tagsnames[i], 0, NULL );

		VectorCopy( mini_me.origin, mini_me.oldorigin );
		VectorCopy( mini_me.origin, mini_me.lightingOrigin );
		mini_me.renderfx |= RF_MINLIGHT;

		trap_R_AddRefEntityToScene( &mini_me );
	}

/*	for( k = 0; k < 3; k++ ) {
		VectorScale( cabinet.axis[k], 2.f, cabinet.axis[k] );
	}
	cabinet.nonNormalizedAxes = qtrue;*/

	trap_R_AddRefEntityToScene( &cabinet );
}

void CG_SetupCabinets( void ) {
	int i, j;
	for( i = 0; i < CT_MAX; i++ ) {
		cabinetInfo[i].model = trap_R_RegisterModel( cabinetInfo[i].modelName );
		for( j = 0; j < MAX_CABINET_TAGS; j++ ) {
			cabinetInfo[i].itemmodels[j] = trap_R_RegisterModel( cabinetInfo[i].itemnames[j] );
		}
	}
}

/*
=========================
CG_AdjustPositionForMover

Also called by client movement prediction code
=========================
*/
void CG_AdjustPositionForMover( const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t outDeltaAngles ) {
	centity_t	*cent;
	vec3_t	oldOrigin, origin, deltaOrigin;
	vec3_t	oldAngles, angles, deltaAngles;
	vec3_t	transpose[3];
	vec3_t	matrix[3];
	vec3_t	move, org, org2;

	if (outDeltaAngles)
		VectorClear( outDeltaAngles );

	if ( moverNum <= 0 || moverNum >= ENTITYNUM_MAX_NORMAL ) {
		VectorCopy( in, out );
		return;
	}

	cent = &cg_entities[ moverNum ];

	if ( cent->currentState.eType != ET_MOVER) {
		VectorCopy( in, out );
		return;
	}

	if( !(cent->currentState.eFlags & EF_PATH_LINK) ) {
		BG_EvaluateTrajectory( &cent->currentState.pos, fromTime, oldOrigin, qfalse, cent->currentState.effect2Time );
		BG_EvaluateTrajectory( &cent->currentState.apos, fromTime, oldAngles, qtrue, cent->currentState.effect2Time );

		BG_EvaluateTrajectory( &cent->currentState.pos, toTime, origin, qfalse, cent->currentState.effect2Time );
		BG_EvaluateTrajectory( &cent->currentState.apos, toTime, angles, qtrue, cent->currentState.effect2Time );

		VectorSubtract( origin, oldOrigin, deltaOrigin );
		VectorSubtract( angles, oldAngles, deltaAngles );
	} else {
		CG_AddLinkedEntity( cent, qtrue, fromTime );
		
		VectorCopy( cent->lerpOrigin, oldOrigin );
		VectorCopy( cent->lerpAngles, oldAngles );

		CG_AddLinkedEntity( cent, qtrue, toTime );

		VectorSubtract( cent->lerpOrigin, oldOrigin, deltaOrigin );
		VectorSubtract( cent->lerpAngles, oldAngles, deltaAngles );

		CG_AddLinkedEntity( cent, qtrue, cg.time );
	}

	BG_CreateRotationMatrix( deltaAngles, transpose );
	BG_TransposeMatrix( (const vec3_t *)transpose, matrix );	

	VectorSubtract (cg.snap->ps.origin, cent->lerpOrigin, org);

	VectorCopy( org, org2 );
	BG_RotatePoint( org2, (const vec3_t *)matrix );
	VectorSubtract (org2, org, move);
	VectorAdd (deltaOrigin, move, deltaOrigin);

	VectorAdd( in, deltaOrigin, out );
	if (outDeltaAngles)
		VectorCopy( deltaAngles, outDeltaAngles );

	// F I X M E: origin change when on a rotating object
	// Gordon: Added
}


/*
=============================
CG_InterpolateEntityPosition
=============================
*/
/* Original TC trajectory IDs are now used by the script path producers too. */
static void CG_EntityTrajectory(const entityState_t *state, const trajectory_t *tr,
	int atTime, vec3_t result, qboolean isAngle, int splinePath, qboolean delta) {
	(void)state;
	if (delta) TCE_BG_EvaluateTrajectoryDelta(tr, atTime, result, isAngle, splinePath, 1.f);
	else TCE_BG_EvaluateTrajectory(tr, atTime, result, isAngle, splinePath, 1.f);
}

static void CG_InterpolateEntityPosition( centity_t *cent ) {
	vec3_t		current, next;
	float		f;

	// it would be an internal error to find an entity that interpolates without
	// a snapshot ahead of the current one
	if ( cg.nextSnap == NULL ) {
		// DHM - Nerve :: FIXME? There are some cases when in Limbo mode during a map restart
		//					that were tripping this error.
		//CG_Error( "CG_InterpolateEntityPosition: cg.nextSnap == NULL" );
		//CG_Printf("CG_InterpolateEntityPosition: cg.nextSnap == NULL");
		return;
	}

	f = cg.frameInterpolation;

	// this will linearize a sine or parabolic curve, but it is important
	// to not extrapolate player positions if more recent data is available
	CG_EntityTrajectory(&cent->currentState, &cent->currentState.pos, cg.snap->serverTime, current, qfalse, cent->currentState.effect2Time, qfalse);
	CG_EntityTrajectory(&cent->nextState, &cent->nextState.pos, cg.nextSnap->serverTime, next, qfalse, cent->currentState.effect2Time, qfalse);

	cent->lerpOrigin[0] = current[0] + f * ( next[0] - current[0] );
	cent->lerpOrigin[1] = current[1] + f * ( next[1] - current[1] );
	cent->lerpOrigin[2] = current[2] + f * ( next[2] - current[2] );
	VectorSubtract(next, current, cent->tceEntityMotion);

	CG_EntityTrajectory(&cent->currentState, &cent->currentState.apos, cg.snap->serverTime, current, qtrue, cent->currentState.effect2Time, qfalse);
	CG_EntityTrajectory(&cent->nextState, &cent->nextState.apos, cg.nextSnap->serverTime, next, qtrue, cent->currentState.effect2Time, qfalse);

	cent->lerpAngles[0] = LerpAngle( current[0], next[0], f );
	cent->lerpAngles[1] = LerpAngle( current[1], next[1], f );
	cent->lerpAngles[2] = LerpAngle( current[2], next[2], f );
	CG_EntityTrajectory(&cent->currentState, &cent->currentState.pos, cg.snap->serverTime,
		current, qfalse, cent->currentState.effect2Time, qtrue);
	CG_EntityTrajectory(&cent->nextState, &cent->nextState.pos, cg.nextSnap->serverTime,
		next, qfalse, cent->currentState.effect2Time, qtrue);
	cent->tceEntityMotion[0] = current[0] + f * (next[0] - current[0]);
	cent->tceEntityMotion[1] = current[1] + f * (next[1] - current[1]);
	cent->tceEntityMotion[2] = current[2] + f * (next[2] - current[2]);

}

/*
===============
CG_CalcEntityLerpPositions

===============
*/
void CG_CalcEntityLerpPositions( centity_t *cent ) {
	if ( cent->interpolate && cent->currentState.pos.trType == TR_INTERPOLATE ) {
		CG_InterpolateEntityPosition( cent );
		return;
	}

	// NERVE - SMF - fix for jittery clients in multiplayer
	// first see if we can interpolate between two snaps for
	// linear extrapolated clients
	if ( cent->interpolate && cent->currentState.pos.trType == TR_LINEAR_STOP && cent->currentState.number < MAX_CLIENTS) {
		CG_InterpolateEntityPosition( cent );
		return;
	}
	// -NERVE - SMF

	// backup
	VectorCopy( cent->lerpAngles, cent->lastLerpAngles );
	VectorCopy( cent->lerpOrigin, cent->lastLerpOrigin );

	// just use the current frame and evaluate as best we can
	CG_EntityTrajectory(&cent->currentState,&cent->currentState.pos,cg.time,cent->lerpOrigin,qfalse,cent->currentState.effect2Time,qfalse);
	CG_EntityTrajectory(&cent->currentState,&cent->currentState.apos,cg.time,cent->lerpAngles,qtrue,cent->currentState.effect2Time,qfalse);
	VectorSubtract(cent->lerpOrigin, cent->lastLerpOrigin, cent->tceEntityMotion);

	// adjust for riding a mover if it wasn't rolled into the predicted
	// player state
	if ( cent != &cg.predictedPlayerEntity && !cg.showGameView ) {
		CG_AdjustPositionForMover( cent->lerpOrigin, cent->currentState.groundEntityNum, cg.snap->serverTime, cg.time, cent->lerpOrigin, NULL );
	}
}

/*
===============
CG_ProcessEntity
===============
*/
/* Whole TC packed radar position producer, Windows30031e40.
 * Malformed negative contact indices are rejected instead of writing before cg. */
static void CG_RadarPositions(centity_t *cent) {
    entityState_t *es=&cent->currentState;
    int group=es->eFlags,i,j,index[3];
    const float *positions[3];
    if(group!=0 && group!=1)return;
    index[0]=es->groundEntityNum;positions[0]=es->origin2;
    index[1]=es->otherEntityNum;positions[1]=es->origin;
    index[2]=es->otherEntityNum2;positions[2]=es->pos.trBase;
    for(i=0;i<3;i++) if(index[i]>=0 && index[i]<64) {
        VectorCopy(positions[i],cg.tceRadarPositions[group][index[i]]);
        cg.tceRadarTimes[group][index[i]]=cg.time;
    }
    for(i=0;i<64;i++) {
        if((cg.tceRadarTimes[group][i] && cg.time-cg.tceRadarTimes[group][i]>750) ||
           i==cg.snap->ps.clientNum || (cg_entities[i].currentState.eFlags&EF_DEAD)) {
            VectorClear(cg.tceRadarPositions[group][i]);cg.tceRadarTimes[group][i]=0;
        }
        if(group==1 && VectorLength(cg.tceRadarPositions[1][i])!=0) {
            for(j=0;j<cg.snap->numEntities;j++) if(cg.snap->entities[j].number==i) {
                /* Original indexes clientinfo by snapshot index, not contact index. */
                if(cg.snap->entities[j].number==cg.snap->entities[j].clientNum &&
                   j<MAX_CLIENTS && cgs.clientinfo[j].infoValid) {
                    VectorCopy(cg_entities[i].lerpOrigin,cg.tceRadarPositions[1][i]);
                    cg.tceRadarTimes[1][i]=cg.time;
                }
                break;
            }
        }
    }
}

static void CG_ProcessEntity( centity_t *cent ) {
	switch ( cent->currentState.eType ) {
	case 61: CG_RadarPositions(cent); break;
	default:
		// ydnar: test for actual bad entity type
		if( cent->currentState.eType < 0 || cent->currentState.eType >= ET_EVENTS )
			CG_Error( "Bad entity type: %i\n", cent->currentState.eType );
		break;
	case ET_CONCUSSIVE_TRIGGER:
	case ET_CAMERA:
	case ET_INVISIBLE:
	case ET_PUSH_TRIGGER:
	case ET_TELEPORT_TRIGGER:
	case ET_OID_TRIGGER:
	case ET_AI_EFFECT:
	case ET_EXPLOSIVE_INDICATOR:		// NERVE - SMF
	case ET_CONSTRUCTIBLE_INDICATOR:
	case ET_TANK_INDICATOR:
	case ET_TANK_INDICATOR_DEAD:
	case ET_COMMANDMAP_MARKER:	// this one should _never_ reach the client
#ifdef VISIBLE_TRIGGERS
	case ET_TRIGGER_MULTIPLE:
	case ET_TRIGGER_FLAGONLY:
	case ET_TRIGGER_FLAGONLY_MULTIPLE:
#endif // VISIBLE_TRIGGERS
		break;
	case ET_SPEAKER:
		CG_Speaker( cent );
		break;
	case ET_GAMEMODEL:
	case ET_MG42_BARREL:
	case ET_FOOTLOCKER:
	case ET_GENERAL:
	case ET_AAGUN:
		CG_General( cent );
		break;
	case ET_CABINET_H:
		CG_Cabinet( cent, CT_HEALTH );
		break;
	case ET_CABINET_A:
		CG_Cabinet( cent, CT_AMMO );
		break;
	case ET_CORPSE:
	case ET_PLAYER:
		if( cg.showGameView && cg.filtercams ) {
			break;
		}
		CG_CalcEntityVisibility( cent );
		CG_Player( cent );
		break;
	case ET_ITEM:
		TCE_CG_Item( cent );
		break;
	case ET_MISSILE:
	case ET_FLAMEBARREL:
	case ET_FP_PARTS:
	case ET_FIRE_COLUMN:
	case ET_FIRE_COLUMN_SMOKE:
	case ET_EXPLO_PART:
	case ET_RAMJET:
		TCE_CG_Missile( cent );
		break;
	case ET_EF_SPOTLIGHT:
		CG_SpotlightEfx( cent );
		break;
	case ET_EXPLOSIVE:
	case 65: /* Original TC objective brush: same renderer as type14. */
		CG_Explosive( cent );
		break;
	case ET_CONSTRUCTIBLE:
		CG_Constructible( cent );
		break;
/*	case ET_WAYPOINT:
	// TAT - 8/29/2002 - draw the botgoal indicator the same way you draw the waypoint flag
	case ET_BOTGOAL_INDICATOR:
		CG_Waypoint( cent );
		break;*/
/*	case ET_CONSTRUCTIBLE_MARKER:
		CG_ConstructibleMarker( cent );
		break;*/
	case ET_TRAP:
		CG_Trap(cent);
		break;
	case ET_CONSTRUCTIBLE_MARKER:
	case ET_ALARMBOX:
	case ET_MOVER:
		CG_Mover( cent );
		break;
	case ET_PROP:
		CG_Prop (cent);
		break;
	case ET_BEAM:
		CG_Beam( cent );
		break;
	case ET_PORTAL:
		CG_Portal( cent );
		break;
	case ET_CORONA:
		CG_Corona( cent );
		break;
	case ET_BOMB:
		CG_Bomb( cent );
		break;
	case ET_BEAM_2:
		CG_Beam_2( cent );
		break;
	case ET_GAMEMANAGER:
		cgs.gameManager = cent;
		break;
	case ET_SMOKER:
		CG_Smoker( cent );
		break;
	}
}

/*
===============
CG_AddCEntity

===============
*/
void CG_AddCEntity( centity_t *cent ) {
	// event-only entities will have been dealt with already
	if ( cent->currentState.eType >= ET_EVENTS ) {
		return;
	}

	cent->processedFrame = cg.clientFrame;

	// calculate the current origin
	CG_CalcEntityLerpPositions( cent );

	// add automatic effects
	CG_EntityEffects( cent );

	// call the appropriate function which will add this entity to the view accordingly
	CG_ProcessEntity( cent );
}

qboolean CG_AddLinkedEntity( centity_t *cent, qboolean ignoreframe, int atTime ) {
	entityState_t		*s1;
	centity_t			*centParent;
	entityState_t		*sParent;
	vec3_t				v;

	// event-only entities will have been dealt with already
	if ( cent->currentState.eType >= ET_EVENTS ) {
		return qtrue;
	}

	if (!ignoreframe && (cent->processedFrame == cg.clientFrame) &&
		!cg.tcePortalScopeRendering && cg.mvTotalClients < 2) {
		// already processed this frame
		return qtrue;
	}

	s1 = &cent->currentState;
	centParent = &cg_entities[s1->torsoAnim];
	sParent = &(centParent->currentState);
	
	// if parent isn't visible, then don't draw us
	if (!centParent->currentValid) {
		return qfalse;
	}

	// make sure all parents are added first
	if ((centParent->processedFrame != cg.clientFrame) || ignoreframe) {
		if (sParent->eFlags & EF_PATH_LINK) {
			if(!CG_AddLinkedEntity(centParent, ignoreframe, atTime)) {
				return qfalse;
			}
		}
	}

	if(!ignoreframe) {
		cent->processedFrame = cg.clientFrame;
	}

	// Arnout: removed from here
	//VectorCopy( cent->lerpAngles, cent->lastLerpAngles );
	//VectorCopy( cent->lerpOrigin, cent->lastLerpOrigin );

	if (!(sParent->eFlags & EF_PATH_LINK)) {
		if( (int)sParent->pos.trType == 16 ) {
			int pos;
			float frac;

			if(!(cent->backspline = BG_GetSplineData( sParent->effect2Time, &cent->back ))) {
				return qfalse;
			}

			cent->backdelta = sParent->pos.trDuration ? (atTime - sParent->pos.trTime) / ((float)sParent->pos.trDuration) : 0;

			if(cent->backdelta < 0.f) {
				cent->backdelta = 0.f;
			} else if(cent->backdelta > 1.f) {
				cent->backdelta = 1.f;
			}

			if(cent->back) {
				cent->backdelta = 1 - cent->backdelta;
			}

			pos = floor(cent->backdelta * (MAX_SPLINE_SEGMENTS));
			if(pos >= MAX_SPLINE_SEGMENTS) {
				pos = MAX_SPLINE_SEGMENTS - 1;
				frac = cent->backspline->segments[pos].length;
			} else {
				frac = ((cent->backdelta * (MAX_SPLINE_SEGMENTS)) - pos) * cent->backspline->segments[pos].length;
			}

			VectorMA( cent->backspline->segments[pos].start, frac, cent->backspline->segments[pos].v_norm, v );
			if(sParent->apos.trBase[0]) {
				BG_LinearPathOrigin2( sParent->apos.trBase[0], &cent->backspline, &cent->backdelta, v, cent->back );
			}

			VectorCopy( v, cent->lerpOrigin );
			
			if(s1->angles2[0]) {
				BG_LinearPathOrigin2( s1->angles2[0], &cent->backspline, &cent->backdelta, v, cent->back );
			}

			VectorCopy( v, cent->origin2 );

			if(	s1->angles2[0] < 0 ) {
				VectorSubtract( v, cent->lerpOrigin, v );
				vectoangles( v, cent->lerpAngles );
			} else if( s1->angles2[0] > 0 ){
				VectorSubtract( cent->lerpOrigin, v, v );
				vectoangles( v, cent->lerpAngles );
			} else {
				VectorClear( cent->lerpAngles );
			}

			cent->moving = qtrue;
		} else {
			cent->moving = qfalse;
			VectorCopy( s1->pos.trBase,		cent->lerpOrigin );
			VectorCopy( s1->apos.trBase,	cent->lerpAngles );
		}
	} else {
		if(centParent->moving) {
			VectorCopy( centParent->origin2, v );

			cent->back =		centParent->back;
			cent->backdelta =	centParent->backdelta;
			cent->backspline =	centParent->backspline;

			VectorCopy( v, cent->lerpOrigin );
			
			if( s1->angles2[0] && cent->backspline ) {
				BG_LinearPathOrigin2( s1->angles2[0], &cent->backspline, &cent->backdelta, v, cent->back );
			}

			VectorCopy( v, cent->origin2 );

			if(	s1->angles2[0] < 0 ) {
				VectorSubtract( v, cent->lerpOrigin, v );
				vectoangles( v, cent->lerpAngles );
			} else if( s1->angles2[0] > 0 ){
				VectorSubtract( cent->lerpOrigin, v, v );
				vectoangles( v, cent->lerpAngles );
			} else {
				VectorClear( cent->lerpAngles );
			}
			cent->moving = qtrue;

		} else {
			cent->moving = qfalse;
			VectorCopy( s1->pos.trBase,		cent->lerpOrigin );
			VectorCopy( s1->apos.trBase,	cent->lerpAngles );
		}
	}

	if(!ignoreframe) {
		// add automatic effects
		CG_EntityEffects( cent );

		// call the appropriate function which will add this entity to the view accordingly
		CG_ProcessEntity( cent );
	}

	return qtrue;
}

void CG_RailTrail2( clientInfo_t *ci, vec3_t start, vec3_t end, int highlighted);

/*
==================
CG_AddEntityToTag
==================
*/
qboolean CG_AddEntityToTag( centity_t *cent ) {
	centity_t			*centParent;
	entityState_t		*sParent;
	refEntity_t			ent;

	// event-only entities will have been dealt with already
	if ( cent->currentState.eType >= ET_EVENTS ) {
		return qfalse;
	}

	if (cent->processedFrame == cg.clientFrame &&
		!cg.tcePortalScopeRendering && cg.mvTotalClients < 2) {
		// already processed this frame
		return qtrue;
	}

	// calculate the current origin
	CG_CalcEntityLerpPositions( cent );	

	if( cent->tagParent < MAX_CLIENTS) {
		return qfalse;
	}

	centParent =	&cg_entities[cent->tagParent];
	sParent =		&centParent->currentState;

	// if parent isn't visible, then don't draw us
	if (!centParent->currentValid) {
		return qfalse;
	}

	// make sure all parents are added first
	if( centParent->processedFrame != cg.clientFrame ) {
		if( !CG_AddCEntity_Filter( centParent ) ) {
			return qfalse;
		}
	}

	cent->processedFrame = cg.clientFrame;

	// start with default axis
	AnglesToAxis( vec3_origin, ent.axis );

	// get the tag position from parent
	CG_PositionEntityOnTag( &ent, &centParent->refEnt, cent->tagName, 0, NULL );

	VectorCopy( ent.origin, cent->lerpOrigin );
	// we need to add the child's angles to the tag angles
	if(cent->currentState.eType != ET_PLAYER) {
		if (!cent->currentState.density) {	// this entity should rotate with it's parent, but can turn around using it's own angles
			// Gordon: fixed to rotate about the object's axis, not the world
			vec3_t mat[3], mat2[3];
			memcpy( mat2, ent.axis, sizeof(mat2));
			BG_CreateRotationMatrix( cent->lerpAngles, mat );
			MatrixMultiply( mat, mat2, ent.axis );
			AxisToAngles( ent.axis, cent->lerpAngles );
		} else {	// face our angles exactly
			BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles, qtrue, cent->currentState.effect2Time  );
		}
	}

	// add automatic effects
	CG_EntityEffects( cent );

	// call the appropriate function which will add this entity to the view accordingly
	CG_ProcessEntity( cent );

	return qtrue;
}

extern int cg_numSolidEntities;
extern centity_t	*cg_solidEntities[];
/*
===============
CG_AddPacketEntities

===============
*/

qboolean CG_AddCEntity_Filter( centity_t* cent ) {
	if(cent->processedFrame == cg.clientFrame &&
		!cg.tcePortalScopeRendering && cg.mvTotalClients < 2) {
		return qtrue;
	}

	if( cent->currentState.eFlags & EF_PATH_LINK ) {
		return CG_AddLinkedEntity(cent, qfalse, cg.time);
	} 
	
	if( cent->currentState.eFlags & EF_TAGCONNECT ) {
		return CG_AddEntityToTag(cent);
	} 
	
	CG_AddCEntity( cent );
	return qtrue;
}

void CG_AddPacketEntities( void ) {
	int					num;
	playerState_t		*ps;
	//int					clcount;

	// set cg.frameInterpolation
	if ( cg.nextSnap ) {
		int		delta;

		delta = (cg.nextSnap->serverTime - cg.snap->serverTime);
		if ( delta == 0 ) {
			cg.frameInterpolation = 0;
		} else {
			cg.frameInterpolation = (float)( cg.time - cg.snap->serverTime ) / delta;
		}
	} else {
		cg.frameInterpolation = 0;	// actually, it should never be used, because 
									// no entities should be marked as interpolating
	}

	// the auto-rotating items will all have the same axis
	cg.autoAnglesSlow[0] = 0;
	cg.autoAnglesSlow[1] = ( cg.time & 4095 ) * 360 / 4095.0;
	cg.autoAnglesSlow[2] = 0;

	cg.autoAngles[0] = 0;
	cg.autoAngles[1] = ( cg.time & 2047 ) * 360 / 2048.0;
	cg.autoAngles[2] = 0;

	cg.autoAnglesFast[0] = 0;
	cg.autoAnglesFast[1] = ( cg.time & 1023 ) * 360 / 1024.0f;
	cg.autoAnglesFast[2] = 0;

	AnglesToAxis( cg.autoAnglesSlow, cg.autoAxisSlow );
	AnglesToAxis( cg.autoAngles, cg.autoAxis );
	AnglesToAxis( cg.autoAnglesFast, cg.autoAxisFast );

	// generate and add the entity from the playerstate
	ps = &cg.predictedPlayerState;
	BG_PlayerStateToEntityState( ps, &cg.predictedPlayerEntity.currentState, qfalse );
	CG_AddCEntity( &cg.predictedPlayerEntity );

	// lerp the non-predicted value for lightning gun origins
	CG_CalcEntityLerpPositions( &cg_entities[ cg.snap->ps.clientNum ] );

	cg.satchelCharge = NULL;

	// Gordon: changing to a single loop, child will request that their parents are added first anyway
	for ( num = 0; num < cg.snap->numEntities ; num++ ) {
		CG_AddCEntity_Filter( &cg_entities[ cg.snap->entities[ num ].number ] );
	}

	// Gordon: Add tank bits as a second loop, to stop recursion problems with tank bits not on base entity
	for ( num = 0; num < cg.snap->numEntities ; num++ ) {
		if( cg_entities[ cg.snap->entities[ num ].number ].currentState.eType == ET_MOVER ) {
			CG_Mover_PostProcess( &cg_entities[ cg.snap->entities[ num ].number ] );
		}
	}	

	// Ridah, add the flamethrower sounds
	CG_UpdateFlamethrowerSounds();
}

void CGRefEntityToTag( refEntity_t* ent, tag_t* tag ) {
	int i;
	VectorCopy( ent->origin, tag->origin );
	for(i = 0; i < 3; i++) {
		VectorCopy(ent->axis[i], tag->axis[i]);
	}
}

void CGTagToRefEntity( refEntity_t* ent, tag_t* tag ) {
	int i;
	VectorCopy( tag->origin, ent->origin );
	for(i = 0; i < 3; i++) {
		VectorCopy(tag->axis[i], ent->axis[i]);
	}
}

void CG_AttachBitsToTank( centity_t* tank, refEntity_t* mg42base, refEntity_t* mg42upper, refEntity_t* mg42gun, refEntity_t* player, refEntity_t* flash, vec_t* playerangles, const char* tagName, qboolean browning ) {
	refEntity_t	ent;
	vec3_t angles;
	int i;

	memset( mg42base,	0,	sizeof( refEntity_t	));
	memset( mg42gun,	0,	sizeof( refEntity_t	));
	memset( mg42upper,	0,	sizeof( refEntity_t	));
	memset( player,		0,	sizeof( refEntity_t ));
	memset( flash,		0,	sizeof( refEntity_t ));

	mg42base->hModel = cgs.media.hMountedMG42Base;
	mg42upper->hModel = cgs.media.hMountedMG42Nest;
	if( browning ) {
		mg42gun->hModel = cgs.media.hMountedBrowning;
	} else {
		mg42gun->hModel = cgs.media.hMountedMG42;
	}

	if( !CG_AddCEntity_Filter( tank ) ) {
		return;
	}

	if( tank->tankframe != cg.clientFrame ) {
		tank->tankframe = cg.clientFrame;

		memset( &ent,		0,	sizeof( refEntity_t	));

		if ( tank->currentState.solid == SOLID_BMODEL ) {
			ent.hModel = cgs.gameModels[tank->currentState.modelindex2];
		} else {
			ent.hModel = cgs.gameModels[tank->currentState.modelindex];
		}

		ent.frame =		tank->lerpFrame.frame;
		ent.oldframe =	tank->lerpFrame.oldFrame;
		ent.backlerp =	tank->lerpFrame.backlerp;

		AnglesToAxis(	tank->lerpAngles, ent.axis		);
		VectorCopy(		tank->lerpOrigin, ent.origin	);

		AxisClear( mg42base->axis );
		CG_PositionEntityOnTag( mg42base, &ent, tagName, 0, NULL );

		VectorCopy( playerangles, angles );
		angles[PITCH] = 0;

		for( i = 0; i < MAX_CLIENTS; i++ ) {
			// Gordon: is this entity mounted on a tank, and attached to _OUR_ turret entity (which could be us)
			if( cg_entities[i].currentValid && cg_entities[i].currentState.eFlags & EF_MOUNTEDTANK && cg_entities[i].tagParent == tank - cg_entities ) {
				angles[YAW] -= tank->lerpAngles[YAW];
				angles[PITCH] -= tank->lerpAngles[PITCH];
				break;
			}
		}

		AnglesToAxis( angles, mg42upper->axis );
		CG_PositionRotatedEntityOnTag( mg42upper, mg42base, "tag_mg42nest" );

		VectorCopy( playerangles, angles );
		angles[YAW] = 0;
		angles[ROLL] = 0;

		AnglesToAxis( angles, mg42gun->axis );
		CG_PositionRotatedEntityOnTag( mg42gun, mg42upper, "tag_mg42" );

		CG_PositionEntityOnTag( player, mg42upper,	"tag_playerpo", 0, NULL );
		CG_PositionEntityOnTag( flash,	mg42gun,	"tag_flash",	0, NULL );

		CGRefEntityToTag( mg42base,		&tank->mountedMG42Base		);
		CGRefEntityToTag( mg42upper,	&tank->mountedMG42Nest		);
		CGRefEntityToTag( mg42gun,		&tank->mountedMG42			);
		CGRefEntityToTag( player,		&tank->mountedMG42Player	);
		CGRefEntityToTag( flash,		&tank->mountedMG42Flash		);
	}

	CGTagToRefEntity( mg42base,		&tank->mountedMG42Base		);
	CGTagToRefEntity( mg42upper,	&tank->mountedMG42Nest		);
	CGTagToRefEntity( mg42gun,		&tank->mountedMG42			);
	CGTagToRefEntity( player,		&tank->mountedMG42Player	);
	CGTagToRefEntity( flash,		&tank->mountedMG42Flash		);
}
