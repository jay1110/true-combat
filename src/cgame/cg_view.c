// cg_view.c -- setup all the parameters (position, angle, etc)
// for a 3D rendering
#include "cg_local.h"
#include "../game/tce_bg.h"
#include "tce_lightgrid.h"
#include "tce_flash.h"
extern vmCvar_t cg_portalScopes;
extern vmCvar_t cg_aspectMode;

//========================
extern 	pmove_t		cg_pmove;
//========================

/*
=============================================================================

  MODEL TESTING

The viewthing and gun positioning tools from Q2 have been integrated and
enhanced into a single model testing facility.

Model viewing can begin with either "testmodel <modelname>" or "testgun <modelname>".

The names must be the full pathname after the basedir, like 
"models/weapons/v_launch/tris.md3" or "players/male/tris.md3"

Testmodel will create a fake entity 100 units in front of the current view
position, directly facing the viewer.  It will remain immobile, so you can
move around it to view it from different angles.

Testgun will cause the model to follow the player around and supress the real
view weapon model.  The default frame 0 of most guns is completely off screen,
so you will probably have to cycle a couple frames to see it.

"nextframe", "prevframe", "nextskin", and "prevskin" commands will change the
frame or skin of the testmodel.  These are bound to F5, F6, F7, and F8 in
q3default.cfg.

If a gun is being tested, the "gun_x", "gun_y", and "gun_z" variables will let
you adjust the positioning.

Note that none of the model testing features update while the game is paused, so
it may be convenient to test with deathmatch set to 1 so that bringing down the
console doesn't pause the game.

=============================================================================
*/

/*
=================
CG_TestModel_f

Creates an entity in front of the current position, which
can then be moved around
=================
*/
void CG_TestModel_f (void) {
	vec3_t		angles;
	/* TC30067170 gates the whole command, including state clearing. */
	if (!developer.integer) return;

	memset( &cg.testModelEntity, 0, sizeof(cg.testModelEntity) );
	if ( trap_Argc() < 2 ) {
		return;
	}

	Q_strncpyz (cg.testModelName, CG_Argv( 1 ), MAX_QPATH );
	cg.testModelEntity.hModel = trap_R_RegisterModel( cg.testModelName );

	if ( trap_Argc() == 3 ) {
		cg.testModelEntity.backlerp = atof( CG_Argv( 2 ) );
		cg.testModelEntity.frame = 1;
		cg.testModelEntity.oldframe = 0;
	}
	if (! cg.testModelEntity.hModel ) {
		CG_Printf( "Can't register model\n" );
		return;
	}

	VectorMA( cg.refdef.vieworg, 100, cg.refdef.viewaxis[0], cg.testModelEntity.origin );

	angles[PITCH] = 0;
	angles[YAW] = 180 + cg.refdefViewAngles[1];
	angles[ROLL] = 0;

	AnglesToAxis( angles, cg.testModelEntity.axis );
	cg.testGun = qfalse;
}

/*
=================
CG_TestGun_f

Replaces the current view weapon with the given model
=================
*/
void CG_TestGun_f (void) {
	/* Do not latch testGun when the nested model command is disabled. */
	if (!developer.integer) return;
	CG_TestModel_f();
	cg.testGun = qtrue;
	cg.testModelEntity.renderfx = RF_MINLIGHT | RF_DEPTHHACK | RF_FIRST_PERSON;
}


void CG_TestModelNextFrame_f (void) {
	cg.testModelEntity.frame++;
	CG_Printf( "frame %i\n", cg.testModelEntity.frame );
}

void CG_TestModelPrevFrame_f (void) {
	cg.testModelEntity.frame--;
	if ( cg.testModelEntity.frame < 0 ) {
		cg.testModelEntity.frame = 0;
	}
	CG_Printf( "frame %i\n", cg.testModelEntity.frame );
}

void CG_TestModelNextSkin_f (void) {
	cg.testModelEntity.skinNum++;
	CG_Printf( "skin %i\n", cg.testModelEntity.skinNum );
}

void CG_TestModelPrevSkin_f (void) {
	cg.testModelEntity.skinNum--;
	if ( cg.testModelEntity.skinNum < 0 ) {
		cg.testModelEntity.skinNum = 0;
	}
	CG_Printf( "skin %i\n", cg.testModelEntity.skinNum );
}

static void CG_AddTestModel (void) {
	int		i;
	/* Original3006b9b0: test models never enter a non-developer scene. */
	if (!developer.integer) return;

	// re-register the model, because the level may have changed
	cg.testModelEntity.hModel = trap_R_RegisterModel( cg.testModelName );
	if (! cg.testModelEntity.hModel ) {
		CG_Printf ("Can't register model\n");
		return;
	}

	// if testing a gun, set the origin reletive to the view origin
	if ( cg.testGun ) {
		VectorCopy( cg.refdef.vieworg, cg.testModelEntity.origin );
		VectorCopy( cg.refdef.viewaxis[0], cg.testModelEntity.axis[0] );
		VectorCopy( cg.refdef.viewaxis[1], cg.testModelEntity.axis[1] );
		VectorCopy( cg.refdef.viewaxis[2], cg.testModelEntity.axis[2] );

		// allow the position to be adjusted
		for (i=0 ; i<3 ; i++) {
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[0][i] * cg_gun_x.value;
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[1][i] * cg_gun_y.value;
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[2][i] * cg_gun_z.value;
		}
	}

	trap_R_AddRefEntityToScene( &cg.testModelEntity );
}



//============================================================================


/*
=================
CG_CalcVrect

Sets the coordinates of the rendered window
=================
*/

//static float letterbox_frac = 1.0f;	// used for transitioning to letterbox for cutscenes // TODO: add to cg. // TTimo: unused
void CG_Letterbox(float xsize, float ysize, qboolean center) {
    /* Preserve original x87 precision and its float percent constant. */
    double height = ysize;
    if (cg_letterbox.integer) {
        height *= 0.85;
        if (!center) {
            int offset = (int)((ysize-height)*0.5*cgs.glconfig.vidHeight*(double)0.01f);
            cg.refdef.y += offset & ~1;
        }
    }
    cg.refdef.width = (int)((double)cgs.glconfig.vidWidth*xsize*(double)0.01f) & ~1;
    cg.refdef.height = (int)((double)cgs.glconfig.vidHeight*height*(double)0.01f) & ~1;
    if (center) {
        cg.refdef.x = (cgs.glconfig.vidWidth-cg.refdef.width)/2;
        cg.refdef.y = (cgs.glconfig.vidHeight-cg.refdef.height)/2;
    }
}
void CG_CalcVrect (void) {
	if ( cg.showGameView ) {
 		float x, y, w, h;
		x = 0;
		y = 0;
		w = 640;
		h = 480;

 		CG_AdjustFrom640( &x, &y, &w, &h );

 		cg.refdef.x = x;
 		cg.refdef.y = y;
 		cg.refdef.width = w;
 		cg.refdef.height = h;

		CG_Letterbox( 100, 100, qfalse );
		return;
	}

	/* TC3006a110: mode0=16:9, mode1=16:10, mode2=4:3. */
    {
        float height=cg_aspectMode.integer==1 ? 83.33333587646484f :
                     cg_aspectMode.integer==2 ? 100.f : 75.f;
        height=((float)cgs.glconfig.vidWidth*480.f/((float)cgs.glconfig.vidHeight*640.f))*height;
        if(height>100.f)height=100.f;
        CG_Letterbox(100,height,qtrue);
    }
}

//==============================================================================

/*
===============
CG_OffsetThirdPersonView

===============
*/
#define	FOCUS_DISTANCE	400	//800	//512
void CG_OffsetThirdPersonView( void ) {
	vec3_t		forward, right, up;
	vec3_t		view;
	vec3_t		focusAngles;
	trace_t		trace;
	static vec3_t	mins = { -4, -4, -4 };
	static vec3_t	maxs = { 4, 4, 4 };
	vec3_t		focusPoint;
	float		focusDist;
	float		forwardScale, sideScale;

	cg.refdef_current->vieworg[2] += cg.predictedPlayerState.viewheight;

	VectorCopy( cg.refdefViewAngles, focusAngles );


	if ( focusAngles[PITCH] > 45 ) {
		focusAngles[PITCH] = 45;		// don't go too far overhead
	}
	AngleVectors( focusAngles, forward, NULL, NULL );

	if( cg_thirdPerson.integer == 2 )
		VectorCopy( cg.predictedPlayerState.origin, focusPoint );
	else
		VectorMA( cg.refdef_current->vieworg, FOCUS_DISTANCE, forward, focusPoint );

	VectorCopy( cg.refdef_current->vieworg, view );

	view[2] += 8;
	cg.refdefViewAngles[PITCH] *= 0.5;

	AngleVectors( cg.refdefViewAngles, forward, right, up );

	forwardScale = cos( cg_thirdPersonAngle.value / 180 * M_PI );
	sideScale = sin( cg_thirdPersonAngle.value / 180 * M_PI );
	VectorMA( view, -cg_thirdPersonRange.value * forwardScale, forward, view );
	VectorMA( view, -cg_thirdPersonRange.value * sideScale, right, view );
	/* Windows 30067430: shift both camera and focus by the shoulder offset. */
	VectorMA(view,cg_thirdPersonOffset.value,right,view);
	VectorMA(focusPoint,cg_thirdPersonOffset.value,right,focusPoint);

	// trace a ray from the origin to the viewpoint to make sure the view isn't
	// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything

	CG_Trace( &trace, cg.refdef_current->vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, CONTENTS_SOLID );

	if ( trace.fraction != 1.0 ) {
		VectorCopy( trace.endpos, view );
		view[2] += (1.0 - trace.fraction) * 32;
		// try another trace to this position, because a tunnel may have the ceiling
		// close enogh that this is poking out

		CG_Trace( &trace, cg.refdef_current->vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, CONTENTS_SOLID );
		VectorCopy( trace.endpos, view );
	}


	VectorCopy( view, cg.refdef_current->vieworg );

	// select pitch to look at focus point from vieword
	VectorSubtract( focusPoint, cg.refdef_current->vieworg, focusPoint );
	focusDist = sqrt( focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1] );
	if ( focusDist < 1 ) {
		focusDist = 1;	// should never happen
	}
	cg.refdefViewAngles[PITCH] = -180 / M_PI * atan2( focusPoint[2], focusDist );
	cg.refdefViewAngles[YAW] -= cg_thirdPersonAngle.value;
}


// this causes a compiler bug on mac MrC compiler
static void CG_StepOffset( void ) {
	int		timeDelta;
	
	// smooth out stair climbing
	timeDelta = cg.time - cg.stepTime;
	// Ridah
	if (timeDelta < 0) {
		cg.stepTime = cg.time;
	}
	if ( timeDelta < STEP_TIME ) {
		cg.refdef_current->vieworg[2] -= (float)(STEP_TIME-timeDelta)*cg.stepChange*0.005f;
	}
}

/*
================
CG_KickAngles
================
*/
void CG_KickAngles(void) {
	const vec3_t centerSpeed = {2400, 2400, 2400};
	const float	recoilCenterSpeed = 200;
	const float recoilIgnoreCutoff = 15;
	const float recoilMaxSpeed = 50;
	const vec3_t maxKickAngles = {10,10,10};
	float idealCenterSpeed, kickChange;
	int i, frametime, t;
	float	ft;
	#define STEP 20
	char buf[32];				// NERVE - SMF

	// this code is frametime-dependant, so split it up into small chunks
	//cg.kickAngles[PITCH] = 0;
	cg.recoilPitchAngle = 0;
	for (t=cg.frametime; t>0; t-=STEP) {
		if (t > STEP)
			frametime = STEP;
		else
			frametime = t;

		ft = ((float)frametime * 0.001f);

		// kickAngles is spring-centered
		for (i=0; i<3; i++) {
			if (cg.kickAVel[i] || cg.kickAngles[i]) {
				// apply centering forces to kickAvel
				if (cg.kickAngles[i] && frametime) {
					idealCenterSpeed = -(2.0f*(cg.kickAngles[i] > 0) - 1.0f) * centerSpeed[i];
					if (idealCenterSpeed) {
						cg.kickAVel[i] += idealCenterSpeed * ft;
					}
				}
				// add the kickAVel to the kickAngles
				kickChange = cg.kickAVel[i] * ft;
				if (cg.kickAngles[i] && (cg.kickAngles[i] < 0) != (kickChange < 0))	// slower when returning to center
					kickChange *= (float)0.06;
				// check for crossing back over the center point
				if (!cg.kickAngles[i] || ((cg.kickAngles[i] + kickChange) < 0) == (cg.kickAngles[i] < 0)) {
					cg.kickAngles[i] += kickChange;
					if (!cg.kickAngles[i] && frametime) {
						cg.kickAVel[i] = 0;
					} else if (fabs(cg.kickAngles[i]) > maxKickAngles[i]) {
						cg.kickAngles[i] = maxKickAngles[i] * ((2*(cg.kickAngles[i]>0))-1);
						cg.kickAVel[i] = 0;	// force Avel to return us to center rather than keep going outside range
					}
				} else { // about to cross, so just zero it out
					cg.kickAngles[i] = 0;
					cg.kickAVel[i] = 0;
				}
			}
		}

		// recoil is added to input viewangles per frame
		if (cg.recoilPitch) {
			// apply max recoil
			if (fabs(cg.recoilPitch) > recoilMaxSpeed) {
				if (cg.recoilPitch > 0)
					cg.recoilPitch = recoilMaxSpeed;
				else
					cg.recoilPitch = -recoilMaxSpeed;
			}
			// apply centering forces to kickAvel
			if (frametime) {
				idealCenterSpeed = -(2.0*(cg.recoilPitch > 0) - 1.0) * recoilCenterSpeed * ft;
				if (idealCenterSpeed) {
					if (fabs(idealCenterSpeed) < fabs(cg.recoilPitch)) {
						cg.recoilPitch += idealCenterSpeed;
					} else {	// back zero out
						cg.recoilPitch = 0;
					}
				}
			}
		}
		if (fabs(cg.recoilPitch) > recoilIgnoreCutoff) {
			cg.recoilPitchAngle += cg.recoilPitch*ft;
		}
	}

	// NERVE - SMF - only change cg_recoilPitch cvar when we need to
	trap_Cvar_VariableStringBuffer( "cg_recoilPitch", buf, sizeof( buf ) );

	if ( atof( buf ) != cg.recoilPitchAngle ) {
		// encode the kick angles into a 24bit number, for sending to the client exe
		trap_Cvar_Set( "cg_recoilPitch", va("%f", cg.recoilPitchAngle) );
	}
}


/*
CG_Concussive
*/
void CG_Concussive (centity_t *cent)
{
	vec3_t vec;
	if (cg.renderingThirdPerson || cent->currentState.density != cg.snap->ps.clientNum)
		return;
	VectorSubtract(cg.snap->ps.origin, cent->currentState.origin, vec);
#if defined(_MSC_VER) && defined(_M_IX86)
	{
		static const float limit = 1024.f, numerator = 32.f, amplitudeScale = 64.f, kickScale = 30.f;
		float amplitude, yaw, roll, pitch;
		float *kick = cg.kickAVel, *recoilPitch = &cg.recoilPitch;
		__asm {
			lea eax, vec
			push eax
			call VectorLength
			fcom limit
			add esp, 4
			fnstsw ax
			test ah, 41h
			jz concussive_outside
			fdivr numerator
			fmul amplitudeScale
			fst amplitude
			fstp yaw
			call rand
			cdq
			fld yaw
			mov ecx, 100
			idiv ecx
			cmp edx, 50
			jle concussive_sign_done
			fchs
		concussive_sign_done:
			fld st(0)
			fchs
			fstp roll
			fld amplitude
			fchs
			fmul kickScale
			mov ecx, recoilPitch
			mov edx, dword ptr [ecx]
			mov dword ptr [ecx], edx
			fstp pitch
			fmul kickScale
			fld roll
			fmul kickScale
			mov ecx, kick
			mov edx, pitch
			mov dword ptr [ecx], edx
			fstp roll
			mov eax, roll
			fstp dword ptr [ecx + 4]
			mov dword ptr [ecx + 8], eax
			jmp concussive_done
		concussive_outside:
			fstp st(0)
		concussive_done:
		}
	}
#else
	{
		float length = VectorLength(vec), amplitude, yaw;
		if (length > 1024.f) return;
		amplitude = (32.f / length) * 64.f;
		yaw = rand() % 100 > 50 ? -amplitude : amplitude;
		cg.kickAVel[PITCH] = -amplitude * 30.f;
		cg.kickAVel[YAW] = yaw * 30.f;
		cg.kickAVel[ROLL] = -yaw * 30.f;
	}
#endif
}
/*
===============
CG_OffsetFirstPersonView

===============
*/
/* Whole Windows3006a220 / Linux000b8272. TC positional stride, stance,
 * asymmetric lean and neck pivot replace the SDK angular bob controller. */
static void CG_OffsetFirstPersonView(void) {
    float *origin=cg.refdef_current->vieworg,*angles=cg.refdefViewAngles;
    float ratio,delta,amplitude,lateral,vertical,lean,scale;
    vec3_t forward,right,up,point,shotForward;
    int duration,elapsed,weapon=cg.snap->ps.weapon;
    if(cg.snap->ps.pm_type==PM_INTERMISSION)return;
    if(weapon==62 || weapon==60) {
        float oldZ=origin[2];
        AngleVectors(cg.pmext.mountedWeaponAngles,forward,NULL,NULL);
        VectorMA(origin,31,forward,point);
        AngleVectors(angles,forward,NULL,NULL);
        VectorMA(point,-32,forward,origin);origin[2]=oldZ;
    }
    if(!(cg.snap->ps.pm_flags&PMF_LIMBO) && cg.snap->ps.stats[STAT_HEALTH]<=0) {
        angles[ROLL]=40;angles[PITCH]=-15;
        angles[YAW]=cg.snap->ps.viewlocked==7?0:SHORT2ANGLE(cg.snap->ps.stats[STAT_DEAD_YAW]);
        origin[2]+=cg.predictedPlayerState.viewheight;return;
    }
    VectorAdd(angles,cg.kick_angles,angles);
    CG_KickAngles();VectorAdd(angles,cg.kickAngles,angles);
    if(cg.damageTime) {
        ratio=(float)cg.time-cg.damageTime;
        if(ratio<100)ratio*=.01f;
        else ratio=1-(ratio-100)*.0025f;
        if((float)cg.time-cg.damageTime<100 || ratio>0) {
            angles[PITCH]+=cg.v_dmg_pitch*ratio;
            angles[ROLL]+=cg.v_dmg_roll*ratio;
        }
    }
    angles[PITCH]+=DotProduct(cg.predictedPlayerState.velocity,cg.refdef_current->viewaxis[0])*.002f;
    angles[ROLL]-=DotProduct(cg.predictedPlayerState.velocity,cg.refdef_current->viewaxis[1])*.005f;
    AngleVectors(angles,shotForward,right,NULL);
    if(cg.bobfracsin==0 && cg.lastvalidBobfracsin>0)
        cg.lastvalidBobfracsin-=(float)cg.frametime*.005f;
    origin[2]+=cg.predictedPlayerState.viewheight;
    elapsed=cg.time-cg.duckTime;
    duration=((cg.predictedPlayerState.eFlags&EF_PRONE) || cg.tceProneTime+cg.time<750)?750:250;
    if(elapsed<0)cg.duckTime=cg.time-duration;
    if(elapsed<duration)
        origin[2]-=(float)(duration-elapsed)*cg.duckChange*(duration==750?.0013333333190530539f:.004f);
    if(cg.snap->ps.stats[STAT_TCE_WEAPON_FLAGS]&0x4000) {
        amplitude=cg.xyspeed*.06666667014360428f;scale=.15f;
    } else if(cg.predictedPlayerState.eFlags&EF_PRONE) {
        amplitude=cg.xyspeed*.03333333507180214f;scale=.6f;
    } else if(cg.predictedPlayerState.pm_flags&PMF_DUCKED) {
        amplitude=cg.xyspeed*.03333333507180214f;scale=.4f;
    } else {
        amplitude=cg.xyspeed*.00625f;scale=.1f;
    }
    if(amplitude>4)amplitude=4;
    lateral=cg.bobfracsin*amplitude*scale;
    vertical=1-cg.bobfracsin*cg.bobfracsin;
    if(cg.bobcycle&1)lateral=-lateral;
    right[2]=0;VectorNormalize(right);
    VectorMA(origin,lateral,right,origin);
    origin[2]+=(vertical-.2f)*amplitude;
    delta=(float)(cg.time-cg.landTime);
    if(delta<0)cg.landTime=cg.time-450;
    if(delta<150)origin[2]+=delta*.006666666828095913f*cg.landChange;
    else if(delta<450)origin[2]+=(1-(delta-150)*.0033333334140479565f)*cg.landChange;
    CG_StepOffset();
    lean=cg.predictedPlayerState.leanf;
    if(lean!=0) {
        scale=lean<0?3.3f:1.8f;
        if(cgs.tceLeanMode<1)angles[ROLL]+=lean*.5f;
        else {lean/=scale;angles[ROLL]+=lean;}
        AngleVectors(angles,NULL,right,NULL);
        VectorMA(origin,lean,right,origin);
    }
    VectorAdd(origin,cg.kick_origin,origin);
    origin[2]-=8;
    AngleVectors(angles,forward,NULL,up);
    VectorMA(origin,4,forward,origin);VectorMA(origin,8,up,origin);
    if(cg.time<cg.tceShotHoldUntil && weapon>=0 && weapon<TCE_MAX_WEAPONS) {
        float kick=(cg.tceShotHoldUntil-cg.time)*.01f*
            weaponDef[weapon].unknown_0f8[cg.tceAimActive?8:7]*.01f*-.5f;
        VectorMA(cg.refdef.vieworg,kick,shotForward,cg.refdef.vieworg);
    }
}

//======================================================================

//
// Zoom controls
//


// probably move to server variables
float zoomTable[ZOOM_MAX_ZOOMS][2] = {
// max {out,in}
	{0, 0},

	{36, 8},	//	binoc
	{90, 60},	// TC zoomTable300b35e8: scoped FOV bounds
	{60, 20},	//	snooper
	{55, 55},	//	fg42
	{55, 55}	//	mg42
};

void CG_AdjustZoomVal(float val, int type)
{
	cg.zoomval += val;
	if(cg.zoomval > zoomTable[type][ZOOM_OUT])
		cg.zoomval = zoomTable[type][ZOOM_OUT];
	if(cg.zoomval < zoomTable[type][ZOOM_IN])
		cg.zoomval = zoomTable[type][ZOOM_IN];
}

void CG_ZoomIn_f( void )
{
	// Gordon: fixed being able to "latch" your zoom by weaponcheck + quick zoomin
	// OSP - change for zoom view in demos
	if( cg_entities[cg.snap->ps.clientNum].currentState.weapon == 57 ) {
		CG_AdjustZoomVal(-(cg_zoomStepSniper.value), ZOOM_SNIPER);
	} else if( cg_entities[cg.snap->ps.clientNum].currentState.weapon == 58) {
		CG_AdjustZoomVal(-(cg_zoomStepSniper.value), ZOOM_SNIPER);
	} else if(cg.zoomedBinoc) {
		CG_AdjustZoomVal(-(cg_zoomStepSniper.value), ZOOM_SNIPER); // JPW NERVE per atvi request all use same vals to match menu (was zoomStepBinoc, ZOOM_BINOC);
	}
}

void CG_ZoomOut_f( void )
{
	if( cg_entities[cg.snap->ps.clientNum].currentState.weapon == 57 ) {
		CG_AdjustZoomVal(cg_zoomStepSniper.value, ZOOM_SNIPER);
	} else if( cg_entities[cg.snap->ps.clientNum].currentState.weapon == 58 ) {
		CG_AdjustZoomVal(cg_zoomStepSniper.value, ZOOM_SNIPER);
	} else if(cg.zoomedBinoc) {
		CG_AdjustZoomVal(cg_zoomStepSniper.value, ZOOM_SNIPER); // JPW NERVE per atvi request BINOC);
	}
}


/*
==============
CG_Zoom
==============
*/
void CG_Zoom( void ) {
    tce_weaponDef_t *def;
    if((cg.snap->ps.pm_flags & PMF_FOLLOW) || cg.demoPlayback) {
        cg.predictedPlayerState.eFlags=cg.snap->ps.eFlags;
        cg.predictedPlayerState.weapon=cg.snap->ps.weapon;
        def=&weaponDef[cg.predictedPlayerState.weapon];
        cg.zoomval=def->scoped>1.0f && cg.tceAimActive && !cg_portalScopes.integer ? 90.0f/def->scoped : (def->unknown_1ac>0?(float)def->unknown_1ac:90.0f);
    }
    if(!cg.tceAimActive) cg.zoomval=0;
    else if(cg.zoomval==0) {
        def=&weaponDef[cg.predictedPlayerState.weapon];
        cg.zoomval=def->scoped>1.0f && !cg_portalScopes.integer ? 90.0f/def->scoped : (def->unknown_1ac>0?(float)def->unknown_1ac:90.0f);
    }
}

void CG_ToggleAiming(void) {
    int elapsed=cg.time-cg.zoomTime;
    if(elapsed<0) elapsed=0;
    cg.tceAimRequested=!cg.tceAimRequested;
    cg.tceAimActive=cg.tceAimRequested;
    cg.tceAimComplete=0;
    cg.tceAimWeaponLatch=0;
    cg.zoomTime=elapsed>199?cg.time:cg.time-200+elapsed;
    cg.tceAimSyncTime=cg.zoomTime;
    cg.tceAimRetryTime=0;
}

/*
====================
CG_CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/
#define	WAVE_AMPLITUDE	1
#define	WAVE_FREQUENCY	0.4

static int CG_CalcFov( void ) {
    static float lastfov=90.0f;
    float base,fov_x,fov_y,zoomFov,f,x,v;
    int contents,inwater;
    if(cg.tceAimRetryTime && cg.time>=cg.tceAimRetryTime && cg.tceAimRequested) {
        if(cg.tceAimActive) { cg.tceAimActive=0; cg.tceAimRetryTime=cg.time+1300; }
        else { cg.tceAimActive=1; cg.tceAimRetryTime=0; }
        cg.tceAimComplete=0;cg.zoomTime=cg.time;cg.tceAimSyncTime=cg.time;
    }
    if(cg.time-cg.tceAimSyncTime>750) {
        if((cg.snap->ps.stats[STAT_TCE_WEAPON_FLAGS]&4) && !cg.tceAimRequested) {
            CG_Printf("force aimed\n");
            cg.tceAimRequested=cg.tceAimComplete=cg.tceAimActive=1;
        } else if(!(cg.snap->ps.stats[STAT_TCE_WEAPON_FLAGS]&4) && cg.tceAimRequested) {
            cg.tceAimRequested=cg.tceAimComplete=cg.tceAimActive=0;
            CG_Printf("force unaimed\n");
        }
    }
    CG_Zoom();
    if(cg.predictedPlayerState.stats[STAT_HEALTH]<1 && !(cg.snap->ps.pm_flags&PMF_FOLLOW)) {
        cg.zoomedBinoc=0;cg.zoomTime=0;cg.zoomval=0;
        cg.tceAimRequested=cg.tceAimComplete=cg.tceAimActive=0;
    }
    if(cg.predictedPlayerState.pm_type==PM_INTERMISSION) fov_x=90;
    else {
        base=developer.integer?cg_fov.value:90.0f;fov_x=base;
        if(!cg.renderingThirdPerson || developer.integer) {
            zoomFov=lastfov;
            if(cg.zoomval!=0) { zoomFov=cg.zoomval;if(zoomFov<1)zoomFov=1;if(zoomFov>160)zoomFov=160; }
            f=(cg.time-cg.zoomTime)*0.005f;
            if(cg.zoomedBinoc) { fov_x=f<=1?(zoomFov-base)*f+base:zoomFov;lastfov=fov_x; }
            else if(cg.tceAimActive) {
                if(f<=1) fov_x=(zoomFov-base)*f+base;
                else { cg.tceAimComplete=1; fov_x=zoomFov; }
                lastfov=fov_x;
            } else { cg.tceAimComplete=0;if(f<=1)fov_x=(base-zoomFov)*f+zoomFov; }
        }
    }
    cg.refdef_current->rdflags &= ~RDF_SNOOPERVIEW;
    if(cg.snap->ps.persistant[PERS_HWEAPON_USE] || cg.snap->ps.weapon==62) fov_x=55;
    else if(cg.snap->ps.eFlags&EF_MOUNTEDTANK) fov_x=75;
    if(cg.showGameView) fov_x=60;
    if(cg_aspectFovMode.integer==1) {
        if(cg_aspectMode.integer==1)fov_x*=1.0466f;
        else if(cg_aspectMode.integer==0)fov_x*=1.0747f;
    } else if(cg_aspectFovMode.integer==2) {
        if(cg_aspectMode.integer==1)fov_x*=1.0954f;
        else if(cg_aspectMode.integer==0)fov_x*=1.1547f;
    }
    cg.tceNameTanHalfFov = (float)tan(fov_x/360*M_PI);
    x=cg.refdef_current->width/tan(fov_x/360*M_PI);
    fov_y=atan2(cg.refdef_current->height,x)*360/M_PI;
    contents=CG_PointContents(cg.refdef.vieworg,-1);
    inwater=(contents&(CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA))!=0;
    if(inwater) { v=sin(cg.time/1000.0*0.4*M_PI*2);fov_x+=v;fov_y-=v;cg.refdef_current->rdflags|=RDF_UNDERWATER; }
    else cg.refdef_current->rdflags&=~RDF_UNDERWATER;
    cg.refdef_current->fov_x=fov_x;cg.refdef_current->fov_y=fov_y;
    if(cg.snap->ps.pm_type==PM_FREEZE || cg.snap->ps.pm_type==PM_DEAD || (cg.snap->ps.pm_flags&PMF_TIME_LOCKPLAYER)) cg.zoomSensitivity=0;
    else if(cg.zoomedBinoc) cg.zoomSensitivity=cg.refdef_current->fov_y/75.0f;
    else if(cg.zoomval==0) cg.zoomSensitivity=1;
    else { cg.zoomSensitivity=cg.zoomval*(1.0f/90.0f);if(weaponDef[cg.predictedPlayerState.weapon].scoped>=4)cg.zoomSensitivity*=0.5f; }
    return inwater;
}

/*
==============
CG_UnderwaterSounds
==============
*/
#define UNDERWATER_BIT 16
static void CG_UnderwaterSounds( void ) {
//	trap_S_AddLoopingSound( cent->lerpOrigin, vec3_origin, cgs.media.underWaterSound, 255, 0 );
	trap_S_AddLoopingSound( cg.snap->ps.origin, vec3_origin, cgs.media.underWaterSound, 255 | (1<<UNDERWATER_BIT), 0 );
}


/*
===============
CG_DamageBlendBlob

===============
*/
static void CG_DamageBlendBlob( void ) {
    int i, axis;
    if (((cg.snap->ps.pm_flags & PMF_LIMBO) ||
         cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) && cg.showGameView)
        return;
    if (cgs.glconfig.hardwareType == GLHW_RAGEPRO) return;
    for (i = 0; i < MAX_VIEWDAMAGE; ++i) {
        viewDamage_t *vd = &cg.viewDamage[i];
        refEntity_t ent;
        int elapsed, duration;
        double right, up;
        float alpha;
        if (!vd->damageValue) continue;
        elapsed = cg.time - vd->damageTime;
        duration = vd->damageDuration;
        if (elapsed <= 0 || elapsed >= duration) {
            vd->damageValue = 0;
            continue;
        }
        if (!vd->damageX && !vd->damageY) continue;
        memset(&ent, 0, sizeof(ent));
        ent.reType = RT_SPRITE;
        ent.renderfx = RF_FIRST_PERSON;
        right = (double)vd->damageX * -8.f;
        up = (double)vd->damageY * 8.f;
        for (axis = 0; axis < 3; ++axis) {
            ent.origin[axis] = (float)((double)cg.refdef_current->viewaxis[0][axis] * 8.f +
                cg.refdef_current->vieworg[axis]);
            ent.origin[axis] = (float)(right * cg.refdef_current->viewaxis[1][axis] + ent.origin[axis]);
            ent.origin[axis] = (float)(up * cg.refdef_current->viewaxis[2][axis] + ent.origin[axis]);
        }
        ent.radius = (float)(((double)elapsed * .5 / duration + .5) *
            (fabs(sin((double)vd->damageTime)) * .5 + .75) *
            vd->damageValue * .4);
        ent.customShader = cgs.media.viewBloodAni[(int)floor((double)elapsed / duration * 4.9)];
        ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = 255;
        alpha = cg_bloodDamageBlend.value;
        if (alpha > 1.f) alpha = 1.f;
        else if (alpha < 0.f) alpha = 0.f;
        ent.shaderRGBA[3] = (byte)(255.0 * alpha);
        trap_R_AddRefEntityToScene(&ent);
    }
}

/*
===============
CG_DrawScreenFade
===============
*/
static void CG_SampleEyeLighting(void) {
    static const float side[9] = {0,-.25f,.25f,-.25f,.25f,-.5f,.5f,0,0};
    static const float up[9] = {0,.15f,.15f,-.15f,-.15f,0,0,-.3f,.3f};
    vec3_t end, point, light;
    trace_t trace;
    float strongest;
    int i,j;
    cg.tceEyeSkySamples = cg.tceEyeSurfaceSamples = 0;
    for (i=0;i<9;i++) {
        for (j=0;j<3;j++)
            end[j] = cg.refdef.vieworg[j] + 8192.0f *
                (up[i]*cg.refdef.viewaxis[2][j] + side[i]*cg.refdef.viewaxis[1][j] + cg.refdef.viewaxis[0][j]);
        CG_Trace(&trace,cg.refdef.vieworg,NULL,NULL,end,cg.snap->ps.clientNum,CONTENTS_SOLID);
        if (trace.surfaceFlags & SURF_SKY) cg.tceEyeSkySamples += 1;
        else {
            VectorMA(trace.endpos,8.0f,trace.plane.normal,point);
            TCE_CG_LightForParticleDirected(TCE_CG_MapLightGrid(),point,trace.plane.normal,light);
            strongest=light[0];
            if(strongest<light[1]) strongest=light[1];
            if(strongest<light[2]) strongest=light[2];
            cg.tceEyeSurfaceSamples += strongest;
        }
    }
}

/* Windows30068650 / Linux CG_EliteSndEffects000b51e8.
 * All 32 room traces, material classification and the environment double buffer. */
static void CG_TCERoomRay(vec3_t direction, const vec3_t signs, int axis) {
    int j;
    for(j=0;j<3;j++) direction[j]=2.0f*((rand()&32767)*(1.0f/32767.0f)-.5f);
    if(axis<3)direction[axis]+=signs[axis];else direction[2]-=1.0f;
    VectorNormalize(direction);
}

static void CG_TCECollectRoomTrace(const trace_t *tr, const vec3_t corner,
    vec3_t low, vec3_t high, float *distance, float *hardness, int *sky) {
    vec3_t span;
    unsigned material=(unsigned)tr->surfaceFlags&0xff000000;
    int j;
    VectorSubtract(tr->endpos,corner,span);
    for(j=0;j<3;j++) {
        if(span[j]<0) {if(span[j]<low[j])low[j]=span[j];}
        else if(span[j]>high[j])high[j]=span[j];
    }
    *distance+=VectorLength(span);
    if(tr->fraction<1) {
        if(tr->surfaceFlags&SURF_SKY)++*sky;
        else if(material!=0x05000000 && material!=0x07000000 &&
                material!=0x0a000000 && material!=0x0d000000) *hardness+=1;
    }
}

static void CG_TCESetSoundZone(const entityState_t *es) {
    int bank=1-cg.tceSoundZoneActive;
    cg.tceSoundZoneSounds[bank][0]=es?es->onFireEnd:0;
    cg.tceSoundZoneSounds[bank][1]=es?es->modelindex2:0;
    cg.tceSoundZoneEffects[bank][0]=es?es->otherEntityNum:0;
    cg.tceSoundZoneEffects[bank][1]=es?es->effect3Time:0;
    cg.tceSoundZoneVolume[bank]=es?es->onFireStart:0;
    cg.tceSoundZoneActive=bank;
    cg.tceSoundZoneTransitionTime=cg.time;
}

static void CG_EliteSndEffects(void) {
    vec3_t low={0,0,0},high={0,0,0},corner,signs,dir,end,start;
    float distance=0,hardness=0,weight,nearest=1e8f,inRadius=1e8f,dist;
    int sky=0,i,j,axis,kind,closest=-1,bounded=-1,bank;
    const entityState_t *global=NULL,*selected;
    trace_t tr;
    if(cg.tceSoundSampleTime>cg.time)return;
    cg.tceSoundSampleTime=cg.time+100;
    VectorCopy(cg.snap->ps.origin,cg.tceSoundLastOrigin);
    for(i=0;i<4;i++) {
        VectorCopy(cg.snap->ps.origin,corner);
        signs[0]=i<2?-1:1;signs[1]=(i==0||i==2)?-1:1;signs[2]=1;
        corner[0]+=15*signs[0];corner[1]+=15*signs[1];
        for(axis=0;axis<4;axis++) {
            CG_TCERoomRay(dir,signs,axis);
            VectorMA(corner,2048,dir,end);
            CG_Trace(&tr,corner,NULL,NULL,end,cg.snap->ps.clientNum,CONTENTS_SOLID);
            CG_TCECollectRoomTrace(&tr,corner,low,high,&distance,&hardness,&sky);
            if(tr.fraction>=1 || (tr.surfaceFlags&SURF_SKY)) {
                VectorCopy(corner,start);
                CG_TCERoomRay(dir,signs,axis);
            } else {
                float reflection=-2*DotProduct(tr.plane.normal,dir);
                VectorCopy(tr.endpos,start);
                VectorMA(dir,reflection,tr.plane.normal,dir);
            }
            VectorMA(start,2048,dir,end);
            CG_Trace(&tr,start,NULL,NULL,end,cg.snap->ps.clientNum,CONTENTS_SOLID);
            CG_TCECollectRoomTrace(&tr,corner,low,high,&distance,&hardness,&sky);
        }
    }
    for(j=0;j<3;j++)cg.tceRoomExtent[j]=cg.tceRoomExtent[j]*.6667f+(high[j]-low[j])*.3333f;
    cg.tceRoomDistance=cg.tceRoomDistance*.6667f+distance*.125f*.3333f;
    cg.tceRoomHardness=cg.tceRoomHardness*.6667f+hardness*.0625f*.3333f;
    cg.tceRoomSky=cg.tceRoomSky*.6667f+sky*.3333f;
    for(j=0;j<4;j++)cg.tceRoomWeights[j]*=.6667f;
    if(cg.tceRoomDistance<200 && cg.tceRoomHardness>.8f)kind=1;
    else if(cg.tceRoomSky>=1 && cg.tceRoomDistance>400)kind=3;
    else if(cg.tceRoomExtent[2]<150 && cg.tceRoomExtent[0]+cg.tceRoomExtent[1]>600 && cg.tceRoomHardness>.8f)kind=2;
    else kind=0;
    cg.tceRoomWeights[kind]+=.3333f;
    weight=cg.tceRoomWeights[0];cg.tceSoundEnvironment=0;
    if(weight<cg.tceRoomWeights[1]){weight=cg.tceRoomWeights[1];cg.tceSoundEnvironment=1;}
    if(weight<cg.tceRoomWeights[2]){weight=cg.tceRoomWeights[2];cg.tceSoundEnvironment=0;}
    if(weight<cg.tceRoomWeights[3])cg.tceSoundEnvironment=3;
    if(trap_CM_PointContents(cg.snap->ps.origin,0)&0x2000)cg.tceSoundEnvironment=0;
    else if(trap_CM_PointContents(cg.snap->ps.origin,0)&0x200)cg.tceSoundEnvironment=3;
    cg.tceEnvironmentOutdoor=cg.tceEnvironmentDefault=0;
    for(i=0;i<MAX_GENTITIES;i++) {
        const entityState_t *es=&cg_entities[i].currentState;
        if(es->eType!=ET_ENVIRONMENT)continue;
        dist=Distance(cg.snap->ps.origin,cg_entities[i].lerpOrigin);
        if(es->otherEntityNum2==1) {
            global=es;
            cg.tceEnvironmentOutdoor=es->frame*(1.0f/255.0f);
            cg.tceEnvironmentDefault=es->effect2Time*(1.0f/255.0f);
            cg.tceEnvironmentSun=es->nextWeapon*(1.0f/255.0f);
        } else {
            if(dist<nearest && !es->density){nearest=dist;closest=i;}
            if(dist<inRadius && dist<=es->dmgFlags){inRadius=dist;bounded=i;}
        }
    }
    if(bounded>=0)closest=bounded;
    bank=cg.tceSoundZoneActive;
    if(cg.tceSoundEnvironment==3 && global && bounded==-1) {
        if(cg.tceSoundZoneSounds[bank][0]!=global->onFireEnd || cg.tceSoundZoneVolume[bank]!=global->onFireStart)
            CG_TCESetSoundZone(global);
    } else if(closest<0) {
        /* The original compares with the global sound index (or -1), not zero. */
        if(cg.tceSoundZoneSounds[bank][0]==(global?global->onFireEnd:-1))CG_TCESetSoundZone(NULL);
    } else {
        selected=&cg_entities[closest].currentState;
        if(cg.tceSoundZoneSounds[bank][0]!=selected->onFireEnd || cg.tceSoundZoneVolume[bank]!=selected->onFireStart)
            CG_TCESetSoundZone(selected);
    }
    if(closest>=0 && cg_entities[closest].currentState.effect2Time)cg.tceEnvironmentDefault=1;
    if(cg_dynamicEye.value>0 && cg.tceEyeScale>0) {
        cg.tceEyeProbeReset[0]=cg.tceEyeProbeReset[1]=0;
        CG_SampleEyeLighting();
    }
}

/* Windows30067ed0: one-second stereo ambient crossfade, with flash deafness. */
static void CG_EliteSndEnvironment(void) {
    float blend=1,gain;
    vec3_t point;
    int i,bank,volume;
    if(!cg.tceSoundZoneSounds[0][0]&&!cg.tceSoundZoneSounds[1][0])return;
    if(cg.time-cg.tceSoundZoneTransitionTime<1000)blend=(cg.time-cg.tceSoundZoneTransitionTime)*.001f;
    for(i=0;i<2;i++) {
        bank=i?1-cg.tceSoundZoneActive:cg.tceSoundZoneActive;
        if(!cg.tceSoundZoneSounds[bank][0] || (i && blend>=1))continue;
        gain=i?1-blend:blend;
        volume=(int)(cg.tceSoundZoneVolume[bank]*(1-tceFlash.deafness)*gain);
        VectorMA(cg.refdef_current->vieworg,64,cg.refdef_current->viewaxis[1],point);
        trap_S_AddLoopingSound(point,vec3_origin,cgs.gameSounds[cg.tceSoundZoneSounds[bank][0]],volume,0);
        VectorMA(cg.refdef_current->vieworg,-64,cg.refdef_current->viewaxis[1],point);
        trap_S_AddLoopingSound(point,vec3_origin,cgs.gameSounds[cg.tceSoundZoneSounds[bank][1]],volume,0);
    }
}

/* Complete Windows 30069520 / Linux CG_DrawScreenFade 000b632a.
 * The flare brightness input belongs to the still-open flare controller. */
void CG_DrawScreenFade( void ) {
    float elapsed,target,surface,decay,strength;
    refEntity_t ent;
    int oldTime=cg.tceEyeTime;
    if(cg_dynamicEye.value<=0 || cg.tceEyeScale<=0) {
        cg.tceScopeLightBoost=0;
        return;
    }
    cg.tceEyeTime=cg.time;
    elapsed=(float)(cg.time-oldTime);
    surface=(cg.tceEyeSurfaceSamples*.1111111119389534f-.15f)*1.1764700412750244f;
    if(surface<0) surface=0;
    target=cg.tceEyeFlare*.2f+(1-cg.tceEyeSky)*cg.tceEyeSkySamples*.1111111119389534f+surface;
    if(target>1) target=1;
    if(target<=cg.tceEyeLinear) {
        cg.tceEyeLinear-=elapsed*.000125f;
        if(cg.tceEyeLinear<target) cg.tceEyeLinear=target;
    } else {
        cg.tceEyeLinear+=elapsed*.0005f;
        if(cg.tceEyeLinear>target) cg.tceEyeLinear=target;
    }
    decay=elapsed*.001f;
    decay=(1-decay)+decay*decay*.5f;
    if(decay<0) decay=0;
    cg.tceEyeSmooth=cg.tceEyeSmooth*decay+(1-decay)*target;
    if(cg.tceEyeSmooth<0) cg.tceEyeSmooth=0;
    if(cg.tceEyeLinear<0) cg.tceEyeLinear=0;
    strength=cg.tceEyeScale*cg_dynamicEye.value;
    if(strength>1) strength=1;
    cg.tceScopeLightBoost=strength*(1-(cg.tceEyeSmooth+cg.tceEyeLinear)*.5f);
    if(cg.tceScopeLightBoost<=0) return;
    memset(&ent,0,sizeof(ent));
    ent.reType=RT_SPRITE;
    ent.renderfx=12;
    VectorMA(cg.refdef_current->vieworg,4,cg.refdef_current->viewaxis[0],ent.origin);
    VectorMA(ent.origin,7,cg.refdef_current->viewaxis[1],ent.origin);
    ent.radius=40;
    ent.customShader=cgs.media.tceEyeAdaptationShader;
    ent.shaderRGBA[0]=ent.shaderRGBA[1]=ent.shaderRGBA[2]=(byte)(int)(cg.tceScopeLightBoost*255.0);
    trap_R_AddRefEntityToScene(&ent);
}

/*
===============
CG_CalcViewValues

Sets cg.refdef view values
===============
*/
int CG_CalcViewValues( void ) {
	playerState_t	*ps;
	cg.tceWeaponViewValid = qfalse;

	memset( cg.refdef_current, 0, sizeof( cg.refdef ) );

	// strings for in game rendering
	// Q_strncpyz( cg.refdef.text[0], "Park Ranger", sizeof(cg.refdef_current->text[0]) );
	// Q_strncpyz( cg.refdef.text[1], "19", sizeof(cg.refdef_current->text[1]) );

	// calculate size of 3D view
	CG_CalcVrect();

	ps = &cg.predictedPlayerState;

	if (cg.cameraMode) {
		vec3_t origin, angles;
		float fov = 90;
		float x;

		if (trap_getCameraInfo(CAM_PRIMARY, cg.time, &origin, &angles, &fov)) {
			VectorCopy(origin, cg.refdef_current->vieworg);
			angles[ROLL] = 0;
			angles[PITCH] = -angles[PITCH];		// (SA) compensate for reversed pitch (this makes the game match the editor, however I'm guessing the real fix is to be done there)
			VectorCopy(angles, cg.refdefViewAngles);
			AnglesToAxis( cg.refdefViewAngles, cg.refdef_current->viewaxis );

			x = cg.refdef.width / tan( fov / 360 * M_PI );
			cg.refdef_current->fov_y = atan2( cg.refdef_current->height, x );
			cg.refdef_current->fov_y = cg.refdef_current->fov_y * 360 / M_PI;
			cg.refdef_current->fov_x = fov;

			// FIXME: this is really really bad
			trap_SendClientCommand(va("setCameraOrigin %f %f %f", origin[0], origin[1], origin[2]));
			return 0;

		} else {
			cg.cameraMode = qfalse;
			trap_Cvar_Set( "cg_letterbox", "0" );
			trap_SendClientCommand("stopCamera");
			trap_stopCamera(CAM_PRIMARY);				// camera off in client

			CG_Fade(0, 0, 0, 255, 0, 0);				// go black
			CG_Fade(0, 0, 0, 0, cg.time + 200, 1500);	// then fadeup
		}
	}

	// intermission view
	if ( ps->pm_type == PM_INTERMISSION ) {
		VectorCopy( ps->origin, cg.refdef_current->vieworg );
		VectorCopy( ps->viewangles, cg.refdefViewAngles );
		AnglesToAxis( cg.refdefViewAngles, cg.refdef_current->viewaxis );
		return CG_CalcFov();
	}

	if( cg.bobfracsin > 0 && !ps->bobCycle ) {
		cg.lastvalidBobcycle = cg.bobcycle;
		cg.lastvalidBobfracsin = cg.bobfracsin;
	}

	cg.bobcycle = ( ps->bobCycle & 128 ) >> 7;
	cg.bobfracsin = fabs( sin( ( ps->bobCycle & 127 ) / 127.0 * M_PI ) );
	cg.xyspeed = sqrt( ps->velocity[0] * ps->velocity[0] + ps->velocity[1] * ps->velocity[1] );


	/* Original30069800: the camera game-view flag is distinct from the
	 * TC limbo panel overlay. Its position is produced by portalcam commands. */
	if (cg.showGameView) {
		VectorCopy(cgs.ccPortalPos, cg.refdef_current->vieworg);
		if (cgs.ccPortalEnt == -1) {
			VectorCopy(cgs.ccPortalAngles, cg.refdefViewAngles);
		} else {
			vec3_t direction;
			VectorSubtract(cg_entities[cgs.ccPortalEnt].lerpOrigin,
				cg.refdef_current->vieworg, direction);
			vectoangles(direction, cg.refdefViewAngles);
		}
	} else if( cg.renderingThirdPerson && (ps->eFlags & EF_MG42_ACTIVE || ps->eFlags & EF_AAGUN_ACTIVE )) { // Arnout: see if we're attached to a gun
		centity_t *mg42 = &cg_entities[ps->viewlocked_entNum];
		vec3_t	forward;

		AngleVectors ( ps->viewangles, forward, NULL, NULL );
		VectorMA ( mg42->currentState.pos.trBase, -36, forward, cg.refdef_current->vieworg );
		cg.refdef_current->vieworg[2] = ps->origin[2];
		VectorCopy( ps->viewangles, cg.refdefViewAngles );
	} else if( ps->eFlags & EF_MOUNTEDTANK ) {
		centity_t *tank = &cg_entities[cg_entities[cg.snap->ps.clientNum].tagParent];

		VectorCopy( tank->mountedMG42Player.origin, cg.refdef_current->vieworg );
		VectorCopy( ps->viewangles, cg.refdefViewAngles );
	} else if (!(ps->eFlags & 0x01000000)) {
		VectorCopy( ps->origin, cg.refdef_current->vieworg );
		VectorCopy( ps->viewangles, cg.refdefViewAngles );
	}

	if( !cg.showGameView ) {
		// add error decay
		if( cg_errorDecay.value > 0 ) {
			int		t;
			float	f;

			t = cg.time - cg.predictedErrorTime;
			f = ( cg_errorDecay.value - t ) / cg_errorDecay.value;
			if ( f > 0 && f < 1 ) {
				VectorMA( cg.refdef_current->vieworg, f, cg.predictedError, cg.refdef_current->vieworg );
			} else {
				cg.predictedErrorTime = 0;
			}
		}

		// Ridah, lock the viewangles if the game has told us to
		if( ps->viewlocked ) {
			
			/*
			if (ps->viewlocked == 4)
			{
				centity_t *tent;
				tent = &cg_entities[ps->viewlocked_entNum];
				VectorCopy (tent->currentState.apos.trBase, cg.refdefViewAngles);
			}
			else
			*/
			// DHM - Nerve :: don't bother evaluating if set to 7 (look at medic)
			if( ps->viewlocked != 7 && ps->viewlocked != 3 && ps->viewlocked != 2 ) {
				BG_EvaluateTrajectory( &cg_entities[ps->viewlocked_entNum].currentState.apos, cg.time, cg.refdefViewAngles, qtrue, cg_entities[ps->viewlocked_entNum].currentState.effect2Time );
			}

			if( ps->viewlocked == 2 ) {
				cg.refdefViewAngles[0] += crandom();
				cg.refdefViewAngles[1] += crandom();
			}
		}

		if ( cg.renderingThirdPerson ) {
			// back away from character
			CG_OffsetThirdPersonView();
		} else {

			// offset for local bobbing and kicks
			CG_OffsetFirstPersonView();

			if( cg.editingSpeakers ) {
				CG_SetViewanglesForSpeakerEditor();
			}
		}

		// Ridah, lock the viewangles if the game has told us to
		if (ps->viewlocked == 7)
		{
			centity_t	*tent;
			vec3_t		vec;

			tent = &cg_entities[ps->viewlocked_entNum];
			VectorCopy( tent->lerpOrigin, vec );
			VectorSubtract( vec, cg.refdef_current->vieworg, vec );
			vectoangles( vec, cg.refdefViewAngles );
		}
		else if (ps->viewlocked == 4)
		{
			vec3_t fwd;
			AngleVectors( cg.refdefViewAngles, fwd, NULL, NULL );
			VectorMA( cg_entities[ps->viewlocked_entNum].lerpOrigin, 16, fwd, cg.refdef_current->vieworg );
		} else if (ps->viewlocked) {
			vec3_t fwd;
			float oldZ;
			// set our position to be behind it
			oldZ = cg.refdef_current->vieworg[2];
			AngleVectors( cg.refdefViewAngles, fwd, NULL, NULL );
			if(cg.predictedPlayerState.eFlags & EF_AAGUN_ACTIVE) {
				VectorMA( cg_entities[ps->viewlocked_entNum].lerpOrigin, 0, fwd, cg.refdef_current->vieworg );
			} else {
				VectorMA( cg_entities[ps->viewlocked_entNum].lerpOrigin, -34, fwd, cg.refdef_current->vieworg );
			}
			cg.refdef_current->vieworg[2] = oldZ;
		}
		// done.
	}

	/* CQB keeps the first-person weapon frame before applying free head look.
	 * Shooting and movement continue to use the shared ps.viewangles. */
	if (!cg.renderingThirdPerson && !cg.showGameView && !ps->viewlocked &&
	    ps->pm_type == PM_NORMAL && ps->stats[STAT_HEALTH] > 0) {
		VectorCopy(cg.refdef_current->vieworg, cg.tceWeaponViewOrigin);
		VectorCopy(cg.refdefViewAngles, cg.tceWeaponViewAngles);
		AnglesToAxis(cg.tceWeaponViewAngles, cg.tceWeaponViewAxis);
		cg.tceWeaponViewValid = qtrue;
		if (ps->holdable[TCE_FREELOOK_PITCH] && ps->holdable[TCE_FREELOOK_YAW]) {
			cg.refdefViewAngles[PITCH] += (ps->holdable[TCE_FREELOOK_PITCH] - TCE_FREELOOK_CENTER) * .01f;
			cg.refdefViewAngles[YAW] += (ps->holdable[TCE_FREELOOK_YAW] - TCE_FREELOOK_CENTER) * .01f;
		}
	}

	// position eye reletive to origin
	AnglesToAxis( cg.refdefViewAngles, cg.refdef_current->viewaxis );

	if ( cg.hyperspace ) {
		cg.refdef.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE;
	}

	// field of view
	return CG_CalcFov();
}


//=========================================================================

char* CG_MustParse( char** pString, const char* pErrorMsg ) {
	char* token = COM_Parse( pString );
	if(!*token) {
		CG_Error( pErrorMsg );
	}
	return token;
}

void CG_ParseSkyBox( void ) {
	int fogStart, fogEnd;
	char *cstr, *token;
	vec4_t fogColor;

	cstr = (char*)CG_ConfigString(CS_SKYBOXORG);

	if (!*cstr) {
		cg.skyboxEnabled = qfalse;
		return;
	}

	token = CG_MustParse( &cstr, "CG_ParseSkyBox: error parsing skybox configstring\n" );
	cg.skyboxViewOrg[0] = atof(token);

	token = CG_MustParse( &cstr, "CG_ParseSkyBox: error parsing skybox configstring\n" );
	cg.skyboxViewOrg[1] = atof(token);

	token = CG_MustParse( &cstr, "CG_ParseSkyBox: error parsing skybox configstring\n" );
	cg.skyboxViewOrg[2] = atof(token);

	token = CG_MustParse( &cstr, "CG_ParseSkyBox: error parsing skybox configstring\n" );
	cg.skyboxViewFov = atoi(token);

    token = CG_MustParse(&cstr,"CG_ParseSkyBox: error parsing skybox configstring\n");
    cg.tceSkyboxAngle=atoi(token);
    if(!cg.skyboxViewFov)cg.skyboxViewFov=25;


	// setup fog the first time, ignore this part of the configstring after that
	token = CG_MustParse( &cstr, "CG_ParseSkyBox: error parsing skybox configstring.  No fog state\n" );
	if(atoi(token)) {	// this camera has fog
		token = CG_MustParse( &cstr, "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog[0]\n" );
		fogColor[0] = atof(token);

		token = CG_MustParse( &cstr, "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog[1]\n" );
		fogColor[1] = atof(token);

		token = CG_MustParse( &cstr, "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog[2]\n" );
		fogColor[2] = atof(token);

		token = COM_ParseExt(&cstr, qfalse);
			fogStart = atoi(token);

		token = COM_ParseExt(&cstr, qfalse);
			fogEnd = atoi(token);

		trap_R_SetFog(FOG_PORTALVIEW, fogStart, fogEnd, fogColor[0], fogColor[1], fogColor[2], 1.1f);
	} else {
		trap_R_SetFog(FOG_PORTALVIEW, 0, 0, 0, 0, 0, 0);	// init to null
	}

	cg.skyboxEnabled = qtrue;
}

/*
==============
CG_ParseTagConnects
==============
*/

void CG_ParseTagConnects( void ) {
	int i;

	for( i = CS_TAGCONNECTS; i < CS_TAGCONNECTS + MAX_TAGCONNECTS; i++ ) {
		CG_ParseTagConnect( i );
	}
}

void CG_ParseTagConnect( int tagNum ) {
	char *token, *pString = (char*)CG_ConfigString( tagNum ); // Gordon: bleh, i hate that cast away of the const
	int entNum;

	if(!*pString) {
		return;
	}
	
	token = CG_MustParse( &pString, "Invalid TAGCONNECT configstring\n" );

	entNum = atoi(token);
	if(entNum < 0 || entNum >= MAX_GENTITIES) {
		CG_Error( "Invalid TAGCONNECT entitynum\n" );
	}

	token = CG_MustParse( &pString, "Invalid TAGCONNECT configstring\n" );

	cg_entities[entNum].tagParent = atoi(token);
	if(cg_entities[entNum].tagParent < 0 || cg_entities[entNum].tagParent >= MAX_GENTITIES) {
		CG_Error( "Invalid TAGCONNECT tagparent\n" );
	}

	token = CG_MustParse( &pString, "Invalid TAGCONNECT configstring\n" );
	Q_strncpyz( cg_entities[entNum].tagName, token, MAX_QPATH );
}

/*
==============
CG_DrawSkyBoxPortal
==============
*/
/* Windows3006b2a0. A clean portal refdef avoids inheriting scene fog,
 * areamasks and unrelated flags; camera motion is scaled into the sky model. */
void CG_DrawSkyBoxPortal(qboolean fLocalView) {
    refdef_t rd;
    double inverseScale;
    if(!cg_skybox.integer || !cg.skyboxEnabled || !fLocalView)return;
    memset(&rd,0,sizeof(rd));
    rd.rdflags=RDF_SKYBOXPORTAL;
    AxisCopy(cg.refdef_current->viewaxis,rd.viewaxis);
    inverseScale=1.0/cg.skyboxViewFov;
    VectorMA(cg.skyboxViewOrg,inverseScale,cg.refdef_current->vieworg,rd.vieworg);
    rd.x=cg.refdef_current->x;rd.y=cg.refdef_current->y;
    rd.width=cg.refdef_current->width;rd.height=cg.refdef_current->height;
    rd.fov_x=cg.refdef_current->fov_x;rd.fov_y=cg.refdef_current->fov_y;
    rd.time=cg.refdef_current->time;
    trap_R_RenderScene(&rd);
}

//=========================================================================

/*
**  Frustum code
*/

// some culling bits
typedef struct plane_s {
	vec3_t	normal;
	float	dist;
} plane_t;

static plane_t frustum[4];

//
//	CG_SetupFrustum
//
void CG_SetupFrustum( void ) {
    int plane, axis;
    float sine;
    double angle, cosine;
    /* Windows3006b3a0: rounded sine, unspilled cosine, one final
     * normal-component store. Vertical FOV is the main refdef's FOV. */
    for (plane = 0; plane < 4; plane += 2) {
        angle = (double)(plane ? cg.refdef.fov_y : cg.refdef_current->fov_x)
            * (double)0.008726646192371845245361328125f;
        sine = (float)sin(angle);
        cosine = cos(angle);
        for (axis = 0; axis < 3; ++axis) {
            float forward = sine * cg.refdef_current->viewaxis[0][axis];
            double side = cosine * cg.refdef_current->viewaxis[plane ? 2 : 1][axis];
            frustum[plane].normal[axis] = (float)(side + forward);
            frustum[plane + 1].normal[axis] = (float)(-side + forward);
        }
    }
    for (plane = 0; plane < 4; ++plane) {
        frustum[plane].dist = (float)(
            (double)frustum[plane].normal[1] * cg.refdef_current->vieworg[1] +
            (double)frustum[plane].normal[2] * cg.refdef_current->vieworg[2] +
            (double)frustum[plane].normal[0] * cg.refdef_current->vieworg[0]);
    }
}


//
//	CG_CullPoint - returns true if culled
//
/* TC Windows keeps the x+z+y plane expression in ST0 until FCOMP.
 * C0 alone is intentional: unordered inputs are culled as in the DLL. */
#if defined(_MSC_VER) && defined(_M_IX86)
static const float cg_cullZero = 0.0f;
__declspec(naked) qboolean CG_CullPoint( vec3_t pt ) {
	__asm {
		mov edx, dword ptr [esp+4]
		lea ecx, frustum
		add ecx, 4
	cull_point_plane:
		fld dword ptr [ecx-4]
		fmul dword ptr [edx]
		fld dword ptr [ecx+4]
		fmul dword ptr [edx+8]
		faddp st(1), st(0)
		fld dword ptr [edx+4]
		fmul dword ptr [ecx]
		faddp st(1), st(0)
		fsub dword ptr [ecx+8]
		fcomp dword ptr [cg_cullZero]
		fnstsw ax
		test ah, 1
		jnz cull_point_yes
		add ecx, 16
		lea eax, frustum
		add eax, 68
		cmp ecx, eax
		jl cull_point_plane
		xor eax, eax
		ret
	cull_point_yes:
		mov eax, 1
		ret
	}
}

__declspec(naked) qboolean CG_CullPointAndRadius( const vec3_t pt, vec_t radius ) {
	__asm {
		fld dword ptr [esp+8]
		mov edx, dword ptr [esp+4]
		lea ecx, frustum
		add ecx, 4
		fchs
		fstp dword ptr [esp+4]
	cull_radius_plane:
		fld dword ptr [ecx-4]
		fmul dword ptr [edx]
		fld dword ptr [ecx+4]
		fmul dword ptr [edx+8]
		faddp st(1), st(0)
		fld dword ptr [ecx]
		fmul dword ptr [edx+4]
		faddp st(1), st(0)
		fsub dword ptr [ecx+8]
		fcomp dword ptr [esp+4]
		fnstsw ax
		test ah, 1
		jnz cull_radius_yes
		add ecx, 16
		lea eax, frustum
		add eax, 68
		cmp ecx, eax
		jl cull_radius_plane
		xor eax, eax
		ret
	cull_radius_yes:
		mov eax, 1
		ret
	}
}
#else
qboolean CG_CullPoint( vec3_t pt ) {
	int		i;
	plane_t	*frust;

	// check against frustum planes
	for (i = 0 ; i < 4 ; i++) {
		frust = &frustum[i];

		if( ( DotProduct( pt, frust->normal) - frust->dist ) < 0 )
			return( qtrue );
	}

	return( qfalse );
}

qboolean CG_CullPointAndRadius( const vec3_t pt, vec_t radius) {
	int		i;
	plane_t	*frust;

	// check against frustum planes
	for (i = 0 ; i < 4 ; i++) {
		frust = &frustum[i];

		if( ( DotProduct( pt, frust->normal) - frust->dist ) < -radius )
			return( qtrue );
	}

	return( qfalse );
}

#endif

//=========================================================================

extern void CG_SetupDlightstyles(void);


//#define DEBUGTIME_ENABLED
#ifdef DEBUGTIME_ENABLED
#define DEBUGTIME elapsed = (trap_Milliseconds()-dbgTime); if(dbgCnt++ == 1) {CG_Printf("t%i:%i ", dbgCnt, elapsed = (trap_Milliseconds()-dbgTime) ); } dbgTime+=elapsed;
#else
#define DEBUGTIME
#endif

#ifdef _DEBUG
//#define FAKELAG
#ifdef FAKELAG
extern int snapshotDelayTime;
#endif // FAKELAG
#endif // _DEBUG

/*
=================
CG_DrawActiveFrame

Generates and draws a game scene and status information at the given time.
=================
*/

//static int lightningtime = 0;
//static int lightningsequencetime = 0;
//static int lightningsequencecounter = 0;

qboolean CG_CalcMuzzlePoint( int entityNum, vec3_t muzzle );

void CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback ) {
	int		inwater;
		
#ifdef DEBUGTIME_ENABLED
	int dbgTime=trap_Milliseconds(),elapsed;
	int dbgCnt=0;
#endif

	cg.time = serverTime;
	cgDC.realTime = cg.time;
	cg.demoPlayback = demoPlayback;

#ifdef FAKELAG
	cg.time -= snapshotDelayTime;
#endif // _DEBUG


#ifdef DEBUGTIME_ENABLED
	CG_Printf("\n");
#endif
	DEBUGTIME

	// update cvars
	CG_UpdateCvars();

	DEBUGTIME

	// if we are only updating the screen as a loading
	// pacifier, don't even try to read snapshots
	if ( cg.infoScreenText[0] != 0 ) {
		CG_DrawInformation( qfalse );
		return;
	}

	CG_PB_ClearPolyBuffers();

	CG_UpdatePMLists();

	// any looped sounds will be respecified as entities
	// are added to the render list
	trap_S_ClearLoopingSounds();

	CG_UpdateBufferedSoundScripts();

	DEBUGTIME

	// set up cg.snap and possibly cg.nextSnap
	CG_ProcessSnapshots();

	DEBUGTIME

	// if we haven't received any snapshots yet, all
	// we can draw is the information screen
	if ( !cg.snap || ( cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE ) ) {
		CG_DrawInformation( qfalse );
		return;
	}

	// check for server set weapons we might not know about
	// (FIXME: this is a hack for the time being since a scripted "selectweapon" does
	// not hit the first snap, the server weapon set in cg_playerstate.c line 219 doesn't
	// do the trick)
	if( !cg.weaponSelect && cg.snap->ps.weapon) {
		cg.weaponSelect = cg.snap->ps.weapon;
		cg.weaponSelectTime = cg.time;
	}

	/* TC CG_DrawActiveFrame 3006b610: only legacy scope slot59. */
	if (cg.weaponSelect == 59) {
		float spd;
		spd = VectorLength(cg.snap->ps.velocity);
		if (spd > 180.0f)
			CG_FinishWeaponChange(59, 33);
	}

	DEBUGTIME

	if(!cg.lightstylesInited)
		CG_SetupDlightstyles();

	DEBUGTIME

	// if we have been told not to render, don't
	if (cg_norender.integer) {
		return;
	}

	// this counter will be bumped for every valid scene we generate
	cg.clientFrame++;

	// update cg.predictedPlayerState
	CG_PredictPlayerState();

	DEBUGTIME


	// OSP -- MV handling
	if(cg.mvCurrentMainview != NULL && cg.snap->ps.pm_type != PM_INTERMISSION) {
		CG_mvDraw(cg.mvCurrentMainview);
		// FIXME: not valid for demo playback
		cg.zoomSensitivity = mv_sensitivity.value / int_sensitivity.value;
	} else {
		// clear all the render lists
		trap_R_ClearScene();

		DEBUGTIME

		// decide on third person view
		cg.renderingThirdPerson = cg_thirdPerson.integer ||
			(cg.snap->ps.stats[STAT_HEALTH] <= 0) || cg.showGameView;

		// build cg.refdef
		inwater = CG_CalcViewValues();
		CG_SetupFrustum();
		CG_EliteSndEffects();
		CG_EliteSndEnvironment();

		DEBUGTIME

		// RF, draw the skyboxportal
		CG_DrawSkyBoxPortal(qtrue);

		DEBUGTIME

		if(inwater)
			CG_UnderwaterSounds();

		DEBUGTIME

		// first person blend blobs, done after AnglesToAxis
		if ( !cg.renderingThirdPerson ) {
			CG_DamageBlendBlob();
		}

		DEBUGTIME

		// build the render lists
		if ( !cg.hyperspace ) {
			CG_AddPacketEntities();			// adter calcViewValues, so predicted player state is correct
			CG_AddMarks();

			DEBUGTIME

			CG_AddScriptSpeakers();

			DEBUGTIME
			
			// Rafael particles
			CG_AddParticles ();
			// done.

			DEBUGTIME

			CG_AddLocalEntities();

			DEBUGTIME

			CG_AddSmokeSprites();

			DEBUGTIME

			CG_AddAtmosphericEffects();
#ifdef FEATURE_OMNIBOT
            OmnibotRenderDebugLines();
#endif
		}
		
		// Rafael mg42
		if( !cg.showGameView && !cgs.dbShowing ) {
			if( !cg.snap->ps.persistant[PERS_HWEAPON_USE] ) {
				CG_AddViewWeapon( &cg.predictedPlayerState );
			} else {
				if( cg.time - cg.predictedPlayerEntity.overheatTime < 3000 ) {
					vec3_t muzzle;

					CG_CalcMuzzlePoint( cg.snap->ps.clientNum, muzzle );

						muzzle[2] -= 32;

					if(!(rand()%3)) {
						float alpha;
						alpha = 1.0f - ((float)( cg.time - cg.predictedPlayerEntity.overheatTime)/3000.0f );
						alpha *= 0.25f;		// .25 max alpha
						CG_ParticleImpactSmokePuffExtended( cgs.media.smokeParticleShader, muzzle, 1000, 8, 20, 30, alpha, 8.f );
					}
				}
			}
		}

		// NERVE - SMF - play buffered voice chats
		CG_PlayBufferedVoiceChats();

		DEBUGTIME
		// Ridah, trails
		if( !cg.hyperspace ) {
			CG_AddFlameChunks ();
			CG_AddTrails ();		// this must come last, so the trails dropped this frame get drawn
		}
		// done.

		DEBUGTIME

		// finish up the rest of the refdef
		if( cg.testModelEntity.hModel ) {
			CG_AddTestModel();
		}
		cg.refdef.time = cg.time;
		memcpy( cg.refdef.areamask, cg.snap->areamask, sizeof( cg.refdef.areamask ) );

		DEBUGTIME

		// warning sounds when powerup is wearing off
		//CG_PowerupTimerSounds();

		// make sure the lagometerSample and frame timing isn't done twice when in stereo
		if ( stereoView != STEREO_RIGHT ) {
			cg.frametime = cg.time - cg.oldTime;
			if ( cg.frametime < 0 ) {
				cg.frametime = 0;
			}
			cg.oldTime = cg.time;
			CG_AddLagometerFrameInfo();
		}

		DEBUGTIME

		/* Room controller above supplies the main-view eye-light samples. */
		CG_DrawScreenFade();

		DEBUGTIME

		// DHM - Nerve :: let client system know our predicted origin
		trap_SetClientLerpOrigin( cg.refdef.vieworg[0], cg.refdef.vieworg[1], cg.refdef.vieworg[2] );

		// actually issue the rendering calls
		CG_DrawActive( stereoView );

		DEBUGTIME

		// update audio positions
		trap_S_Respatialize( cg.snap->ps.clientNum, cg.refdef.vieworg, cg.refdef.viewaxis, inwater );
	}

	if ( cg_stats.integer ) {
		CG_Printf( "cg.clientFrame:%i\n", cg.clientFrame );
	}

	DEBUGTIME

	// let the client system know what our weapon, holdable item and zoom settings are
	trap_SetUserCmdValue( cg.weaponSelect, cg.showGameView ? 0x01 : 0x00, cg.zoomSensitivity, cg.identifyClientRequest );
}

