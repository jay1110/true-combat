// cg_view.c -- setup all the parameters (position, angle, etc)
// for a 3D rendering
#include "cg_local.h"
#include "../game/tce_bg.h"
#include "tce_lightgrid.h"
#include "tce_flash.h"
#include "../game/tce_numeric.h"
#include "../game/tce_ambient_precision.h"
#include <float.h>

/* Select by the retained arithmetic format, independently of pointer width. */
#if defined(__linux__) && !defined(Q3_VM) && \
    LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
#define TCE_LINUX_ROOM_X87
#endif

extern vmCvar_t cg_portalScopes;
extern vmCvar_t cg_aspectMode;

//========================
extern pmove_t cg_pmove;
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
void CG_TestModel_f(void)
{
	vec3_t angles;
	/* TC30067170 gates the whole command, including state clearing. */
	if (!developer.integer)
	{
		return;
	}

	memset(&cg.testModelEntity, 0, sizeof(cg.testModelEntity));
	if (trap_Argc() < 2)
	{
		return;
	}

	Q_strncpyz(cg.testModelName, CG_Argv(1), MAX_QPATH);
	cg.testModelEntity.hModel = trap_R_RegisterModel(cg.testModelName);

	if (trap_Argc() == 3)
	{
		cg.testModelEntity.backlerp = atof(CG_Argv(2));
		cg.testModelEntity.frame    = 1;
		cg.testModelEntity.oldframe = 0;
	}
	if (!cg.testModelEntity.hModel)
	{
		CG_Printf("Can't register model\n");
		return;
	}

	VectorMA(cg.refdef.vieworg, 100, cg.refdef.viewaxis[0], cg.testModelEntity.origin);

	angles[PITCH] = 0;
	angles[YAW]   = 180 + cg.refdefViewAngles[1];
	angles[ROLL]  = 0;

	AnglesToAxis(angles, cg.testModelEntity.axis);
	cg.testGun = qfalse;
}

/*
=================
CG_TestGun_f

Replaces the current view weapon with the given model
=================
*/
void CG_TestGun_f(void)
{
	/* Do not latch testGun when the nested model command is disabled. */
	if (!developer.integer)
	{
		return;
	}
	CG_TestModel_f();
	cg.testGun                  = qtrue;
	cg.testModelEntity.renderfx = RF_MINLIGHT | RF_DEPTHHACK | RF_FIRST_PERSON;
}


void CG_TestModelNextFrame_f(void)
{
	cg.testModelEntity.frame++;
	CG_Printf("frame %i\n", cg.testModelEntity.frame);
}

void CG_TestModelPrevFrame_f(void)
{
	cg.testModelEntity.frame--;
	if (cg.testModelEntity.frame < 0)
	{
		cg.testModelEntity.frame = 0;
	}
	CG_Printf("frame %i\n", cg.testModelEntity.frame);
}

void CG_TestModelNextSkin_f(void)
{
	cg.testModelEntity.skinNum++;
	CG_Printf("skin %i\n", cg.testModelEntity.skinNum);
}

void CG_TestModelPrevSkin_f(void)
{
	cg.testModelEntity.skinNum--;
	if (cg.testModelEntity.skinNum < 0)
	{
		cg.testModelEntity.skinNum = 0;
	}
	CG_Printf("skin %i\n", cg.testModelEntity.skinNum);
}

static void CG_AddTestModel(void)
{
	int i;
	/* Original3006b9b0: test models never enter a non-developer scene. */
	if (!developer.integer)
	{
		return;
	}

	// re-register the model, because the level may have changed
	cg.testModelEntity.hModel = trap_R_RegisterModel(cg.testModelName);
	if (!cg.testModelEntity.hModel)
	{
		CG_Printf("Can't register model\n");
		return;
	}

	// if testing a gun, set the origin reletive to the view origin
	if (cg.testGun)
	{
		VectorCopy(cg.refdef.vieworg, cg.testModelEntity.origin);
		VectorCopy(cg.refdef.viewaxis[0], cg.testModelEntity.axis[0]);
		VectorCopy(cg.refdef.viewaxis[1], cg.testModelEntity.axis[1]);
		VectorCopy(cg.refdef.viewaxis[2], cg.testModelEntity.axis[2]);

		// allow the position to be adjusted
		for (i = 0 ; i < 3 ; i++)
		{
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[0][i] * cg_gun_x.value;
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[1][i] * cg_gun_y.value;
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[2][i] * cg_gun_z.value;
		}
	}

	trap_R_AddRefEntityToScene(&cg.testModelEntity);
}



//============================================================================


/*
=================
CG_CalcVrect

Sets the coordinates of the rendered window
=================
*/

//static float letterbox_frac = 1.0f;	// used for transitioning to letterbox for cutscenes // TODO: add to cg. // TTimo: unused
void CG_Letterbox(float xsize, float ysize, qboolean center)
{
#if defined(__linux__)
	if (cg_letterbox.integer)
	{
		float lbheight, lbdiff;

		lbheight = ysize * 0.85;
		lbdiff = ysize - lbheight;
		if (!center)
		{
			int offset = (cgs.glconfig.vidHeight * (.5f * lbdiff)) / 100;
			offset &= ~1;
			cg.refdef.y += offset;
		}
		ysize = lbheight;
	}
	cg.refdef.width = cgs.glconfig.vidWidth * xsize / 100;
	cg.refdef.width &= ~1;
	cg.refdef.height = cgs.glconfig.vidHeight * ysize / 100;
	cg.refdef.height &= ~1;
	if (center)
	{
		cg.refdef.x = (cgs.glconfig.vidWidth - cg.refdef.width) / 2;
		cg.refdef.y = (cgs.glconfig.vidHeight - cg.refdef.height) / 2;
	}
#else
	/* Preserve original x87 precision and its float percent constant. */
	double height = ysize;
	if (cg_letterbox.integer)
	{
		height *= 0.85;
		if (!center)
		{
			int offset = (int)((ysize - height) * 0.5 * cgs.glconfig.vidHeight * (double)0.01f);
			cg.refdef.y += offset & ~1;
		}
	}
	cg.refdef.width  = (int)((double)cgs.glconfig.vidWidth * xsize * (double)0.01f) & ~1;
	cg.refdef.height = (int)((double)cgs.glconfig.vidHeight * height * (double)0.01f) & ~1;
	if (center)
	{
		cg.refdef.x = (cgs.glconfig.vidWidth - cg.refdef.width) / 2;
		cg.refdef.y = (cgs.glconfig.vidHeight - cg.refdef.height) / 2;
	}
#endif
}
void CG_CalcVrect(void)
{
	if (cg.showGameView)
	{
		float x, y, w, h;
		x = 0;
		y = 0;
		w = 640;
		h = 480;

		CG_AdjustFrom640(&x, &y, &w, &h);

		cg.refdef.x      = x;
		cg.refdef.y      = y;
		cg.refdef.width  = w;
		cg.refdef.height = h;

		CG_Letterbox(100, 100, qfalse);
		return;
	}

	/* TC3006a110: mode0=16:9, mode1=16:10, mode2=4:3. */
	{
		float height = cg_aspectMode.integer == 1 ? 83.33333587646484f :
		               cg_aspectMode.integer == 2 ? 100.f : 75.f;
#if defined(__linux__)
		long double scaledHeight = 1.0L / ((long double)cgs.glconfig.vidHeight * 640.0L);
		scaledHeight *= (long double)cgs.glconfig.vidWidth * 480.0L;
		scaledHeight *= height;
		if (!(scaledHeight <= 100.0L))
		{
			scaledHeight = 100.0L;
		}
		CG_Letterbox(100, (float)scaledHeight, qtrue);
#else
		height = ((float)cgs.glconfig.vidWidth * 480.f / ((float)cgs.glconfig.vidHeight * 640.f)) * height;
		if (height > 100.f)
		{
			height = 100.f;
		}
		CG_Letterbox(100, height, qtrue);
#endif
	}
}

//==============================================================================

/*
===============
CG_OffsetThirdPersonView

===============
*/
#define FOCUS_DISTANCE  400 //800	//512
void CG_OffsetThirdPersonView(void)
{
	vec3_t        forward, right, up;
	vec3_t        view;
	vec3_t        focusAngles;
	trace_t       trace;
	static vec3_t mins = { -4, -4, -4 };
	static vec3_t maxs = { 4, 4, 4 };
	vec3_t        focusPoint;
	float         focusDist;
	float         forwardScale, sideScale;

	cg.refdef_current->vieworg[2] += cg.predictedPlayerState.viewheight;

	VectorCopy(cg.refdefViewAngles, focusAngles);


	if (focusAngles[PITCH] > 45)
	{
		focusAngles[PITCH] = 45;        // don't go too far overhead
	}
	AngleVectors(focusAngles, forward, NULL, NULL);

	if (cg_thirdPerson.integer == 2)
	{
		VectorCopy(cg.predictedPlayerState.origin, focusPoint);
	}
	else
	{
		VectorMA(cg.refdef_current->vieworg, FOCUS_DISTANCE, forward, focusPoint);
	}

	VectorCopy(cg.refdef_current->vieworg, view);

	view[2]                    += 8;
	cg.refdefViewAngles[PITCH] *= 0.5;

	AngleVectors(cg.refdefViewAngles, forward, right, up);

	forwardScale = cos(cg_thirdPersonAngle.value / 180 * M_PI);
	sideScale    = sin(cg_thirdPersonAngle.value / 180 * M_PI);
	VectorMA(view, -cg_thirdPersonRange.value * forwardScale, forward, view);
	VectorMA(view, -cg_thirdPersonRange.value * sideScale, right, view);
	/* Windows 30067430: shift both camera and focus by the shoulder offset. */
	VectorMA(view, cg_thirdPersonOffset.value, right, view);
	VectorMA(focusPoint, cg_thirdPersonOffset.value, right, focusPoint);

	// trace a ray from the origin to the viewpoint to make sure the view isn't
	// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything

	CG_Trace(&trace, cg.refdef_current->vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, CONTENTS_SOLID);

	if (trace.fraction != 1.0)
	{
		VectorCopy(trace.endpos, view);
		view[2] += (1.0 - trace.fraction) * 32;
		// try another trace to this position, because a tunnel may have the ceiling
		// close enogh that this is poking out

		CG_Trace(&trace, cg.refdef_current->vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, CONTENTS_SOLID);
		VectorCopy(trace.endpos, view);
	}


	VectorCopy(view, cg.refdef_current->vieworg);

	// select pitch to look at focus point from vieword
	VectorSubtract(focusPoint, cg.refdef_current->vieworg, focusPoint);
	focusDist = sqrt(focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1]);
	if (focusDist < 1)
	{
		focusDist = 1;  // should never happen
	}
	cg.refdefViewAngles[PITCH] = -180 / M_PI * atan2(focusPoint[2], focusDist);
	cg.refdefViewAngles[YAW]  -= cg_thirdPersonAngle.value;
}


// this causes a compiler bug on mac MrC compiler
static void CG_StepOffset(void)
{
	int timeDelta;

	// smooth out stair climbing
#if defined(__linux__)
	timeDelta = TCE_SubInt32(cg.time, cg.stepTime);
#else
	timeDelta = cg.time - cg.stepTime;
#endif
	// Ridah
	if (timeDelta < 0)
	{
		cg.stepTime = cg.time;
	}
	if (timeDelta < STEP_TIME)
	{
#if defined(__linux__)
		long double change = (long double)TCE_SubInt32(STEP_TIME, timeDelta) * cg.stepChange * .005f;
		cg.refdef_current->vieworg[2] = (float)((long double)cg.refdef_current->vieworg[2] - change);
#else
		cg.refdef_current->vieworg[2] -= (float)(STEP_TIME - timeDelta) * cg.stepChange * 0.005f;
#endif
	}
}

#if defined(__linux__)
void CG_Watchdog(void)
{
	if (TCE_SubInt32(cg.tceWatchdogTime, cg.time) <= 0)
	{
		cg.tceWatchdogTime = TCE_AddInt32(cg.time, 1000);
	}
}
#endif

/*
================
CG_KickAngles
================
*/
void CG_KickAngles(void)
{
	const vec3_t centerSpeed        = { 2400, 2400, 2400 };
	const float  recoilCenterSpeed  = 200;
	const float  recoilIgnoreCutoff = 15;
	const float  recoilMaxSpeed     = 50;
	const vec3_t maxKickAngles      = { 10, 10, 10 };
	float        idealCenterSpeed, kickChange;
	int          i, frametime, t;
	float        ft;
	#define STEP 20
	char buf[32];               // NERVE - SMF

	// this code is frametime-dependant, so split it up into small chunks
	//cg.kickAngles[PITCH] = 0;
	cg.recoilPitchAngle = 0;
	for (t = cg.frametime; t > 0; t -= STEP)
	{
		if (t > STEP)
		{
			frametime = STEP;
		}
		else
		{
			frametime = t;
		}

		ft = ((float)frametime * 0.001f);

		// kickAngles is spring-centered
		for (i = 0; i < 3; i++)
		{
			if (cg.kickAVel[i] || cg.kickAngles[i])
			{
				// apply centering forces to kickAvel
				if (cg.kickAngles[i] && frametime)
				{
					idealCenterSpeed = -(2.0f * (cg.kickAngles[i] > 0) - 1.0f) * centerSpeed[i];
					if (idealCenterSpeed)
					{
						cg.kickAVel[i] += idealCenterSpeed * ft;
					}
				}
				// add the kickAVel to the kickAngles
				kickChange = cg.kickAVel[i] * ft;
				if (cg.kickAngles[i] && (cg.kickAngles[i] < 0) != (kickChange < 0)) // slower when returning to center
				{
					kickChange *= (float)0.06;
				}
				// check for crossing back over the center point
				if (!cg.kickAngles[i] || ((cg.kickAngles[i] + kickChange) < 0) == (cg.kickAngles[i] < 0))
				{
					cg.kickAngles[i] += kickChange;
					if (!cg.kickAngles[i] && frametime)
					{
						cg.kickAVel[i] = 0;
					}
					else if (fabs(cg.kickAngles[i]) > maxKickAngles[i])
					{
						cg.kickAngles[i] = maxKickAngles[i] * ((2 * (cg.kickAngles[i] > 0)) - 1);
						cg.kickAVel[i]   = 0; // force Avel to return us to center rather than keep going outside range
					}
				}
				else     // about to cross, so just zero it out
				{
					cg.kickAngles[i] = 0;
					cg.kickAVel[i]   = 0;
				}
			}
		}

		// recoil is added to input viewangles per frame
		if (cg.recoilPitch)
		{
			// apply max recoil
			if (fabs(cg.recoilPitch) > recoilMaxSpeed)
			{
				if (cg.recoilPitch > 0)
				{
					cg.recoilPitch = recoilMaxSpeed;
				}
				else
				{
					cg.recoilPitch = -recoilMaxSpeed;
				}
			}
			// apply centering forces to kickAvel
			if (frametime)
			{
				idealCenterSpeed = -(2.0 * (cg.recoilPitch > 0) - 1.0) * recoilCenterSpeed * ft;
				if (idealCenterSpeed)
				{
					if (fabs(idealCenterSpeed) < fabs(cg.recoilPitch))
					{
						cg.recoilPitch += idealCenterSpeed;
					}
					else        // back zero out
					{
						cg.recoilPitch = 0;
					}
				}
			}
		}
		if (fabs(cg.recoilPitch) > recoilIgnoreCutoff)
		{
			cg.recoilPitchAngle += cg.recoilPitch * ft;
		}
	}

	// NERVE - SMF - only change cg_recoilPitch cvar when we need to
	trap_Cvar_VariableStringBuffer("cg_recoilPitch", buf, sizeof(buf));

	if (atof(buf) != cg.recoilPitchAngle)
	{
		// encode the kick angles into a 24bit number, for sending to the client exe
		trap_Cvar_Set("cg_recoilPitch", va("%f", cg.recoilPitchAngle));
	}
}


/*
CG_Concussive
*/
void CG_Concussive(centity_t *cent)
{
	vec3_t vec;
	if (cg.renderingThirdPerson || cent->currentState.density != cg.snap->ps.clientNum)
	{
		return;
	}
	VectorSubtract(cg.snap->ps.origin, cent->currentState.origin, vec);
#if defined(_MSC_VER) && defined(_M_IX86)
	{
		static const float limit = 1024.f, numerator = 32.f, amplitudeScale = 64.f, kickScale = 30.f;
		float              amplitude, roll, pitch;
		float              *kick = cg.kickAVel, *recoilPitch = &cg.recoilPitch;
		{
			long double    distance = TCE_VectorLengthValue(vec);
			long double    value, yawValue, pitchValue, rollValue;
			volatile float amplitudeStore, yawStore, rollStore, pitchStore;
			unsigned int   recoilBits;
			int            sample;
			/* C0/C3 admits less/equal/unordered lengths. No zero guard. */
			if (!(distance > (long double)limit))
			{
				value          = (long double)numerator / distance;
				value          = value * (long double)amplitudeScale;
				amplitudeStore = (float)value;
				yawStore       = (float)value;
				sample         = rand();
				yawValue       = (long double)yawStore;
				if (sample % 100 > 50)
				{
					yawValue = -yawValue;
				}
				rollStore  = (float)-yawValue;
				amplitude  = amplitudeStore;
				pitchValue = -(long double)amplitude * (long double)kickScale;
				/* Preserve raw recoil bits rather than arithmetic subtraction0.
				 * The compiler may elide this same-value memory rewrite. */
				memcpy(&recoilBits, recoilPitch, sizeof(recoilBits));
				memcpy(recoilPitch, &recoilBits, sizeof(recoilBits));
				pitchStore = (float)pitchValue;
				pitch      = pitchStore;
				yawValue   = yawValue * (long double)kickScale;
				roll       = rollStore;
				rollValue  = (long double)roll * (long double)kickScale;
				memcpy(&kick[0], &pitch, sizeof(pitch));
				rollStore = (float)rollValue;
				roll      = rollStore;
				kick[1]   = (float)yawValue;
				memcpy(&kick[2], &roll, sizeof(roll));
			}
		}
	}
#elif defined(__linux__) && !defined(Q3_VM)
	{
		long double distance = TCE_VectorLengthValue(vec);
		/* TC admits unordered distances; the SDK's Linux gate does not. */
		if (!(distance > 1024.f))
		{
			float       amplitude = (float)(((1.L / distance) * 32.f) * 64.f);
			float       yaw = amplitude;
			long double pitch, roll, recoil;
			if (rand() % 100 > 50)
			{
				unsigned bits;
				memcpy(&bits, &yaw, sizeof(bits));
				bits ^= 0x80000000u;
				memcpy(&yaw, &bits, sizeof(yaw));
			}
			pitch = -(long double)amplitude * 30.f;
			roll  = -(long double)yaw * 30.f;
			/* The original subtracts zero in the FPU and publishes this last. */
			recoil = (long double)cg.recoilPitch - 0.f;
			cg.kickAVel[YAW]   = (float)((long double)yaw * 30.f);
			cg.kickAVel[PITCH] = (float)pitch;
			cg.kickAVel[ROLL]  = (float)roll;
			cg.recoilPitch    = (float)recoil;
		}
	}
#else
	{
		float length = VectorLength(vec), amplitude, yaw;
		if (length > 1024.f)
		{
			return;
		}
		amplitude          = (32.f / length) * 64.f;
		yaw                = rand() % 100 > 50 ? -amplitude : amplitude;
		cg.kickAVel[PITCH] = -amplitude * 30.f;
		cg.kickAVel[YAW]   = yaw * 30.f;
		cg.kickAVel[ROLL]  = -yaw * 30.f;
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
static void CG_OffsetFirstPersonView(void)
{
	float  *origin = cg.refdef_current->vieworg, *angles = cg.refdefViewAngles;
#if defined(__linux__)
	long double delta;
	float amplitude, lateral, vertical, lean;
#else
	float  ratio, delta, amplitude, lateral, vertical, lean, scale;
#endif
	vec3_t forward, right, up, point, shotForward;
	int    duration, elapsed, weapon = cg.snap->ps.weapon;
	if (cg.snap->ps.pm_type == PM_INTERMISSION)
	{
		return;
	}
	if (weapon == 62 || weapon == 60)
	{
		float oldZ = origin[2];
		AngleVectors(cg.pmext.mountedWeaponAngles, forward, NULL, NULL);
		VectorMA(origin, 31, forward, point);
		AngleVectors(angles, forward, NULL, NULL);
		VectorMA(point, -32, forward, origin); origin[2] = oldZ;
	}
	if (!(cg.snap->ps.pm_flags & PMF_LIMBO) && cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		angles[ROLL] = 40; angles[PITCH] = -15;
		angles[YAW]  = cg.snap->ps.viewlocked == 7?0:SHORT2ANGLE(cg.snap->ps.stats[STAT_DEAD_YAW]);
		origin[2]   += cg.predictedPlayerState.viewheight; return;
	}
	VectorAdd(angles, cg.kick_angles, angles);
	CG_KickAngles();
#if defined(__linux__)
	{
		long double pitch = (long double)cg.kickAngles[PITCH] + angles[PITCH];
		long double roll = (long double)cg.kickAngles[ROLL] + angles[ROLL];
		angles[PITCH] = (float)pitch;
		angles[YAW] = (float)((long double)cg.kickAngles[YAW] + angles[YAW]);
		angles[ROLL] = (float)roll;
		if (cg.damageTime != 0)
		{
			long double damageElapsed = (long double)cg.time - cg.damageTime;
			if (damageElapsed < 100.0f)
			{
				long double ratio = damageElapsed * .01f;
				angles[PITCH] = (float)(ratio * cg.v_dmg_pitch + pitch);
				angles[ROLL] = (float)(ratio * cg.v_dmg_roll + roll);
			}
			else
			{
				float ratio = (float)(1.0L - (damageElapsed - 100.0f) * .0025f);
				if (ratio > 0)
				{
					angles[PITCH] = (float)((long double)ratio * cg.v_dmg_pitch + angles[PITCH]);
					angles[ROLL] = (float)((long double)ratio * cg.v_dmg_roll + angles[ROLL]);
				}
			}
		}
	}
	{
		long double pitchSpeed = (long double)cg.predictedPlayerState.velocity[0] * cg.refdef_current->viewaxis[0][0] +
		                         (long double)cg.predictedPlayerState.velocity[1] * cg.refdef_current->viewaxis[0][1];
		long double rollSpeed = (long double)cg.predictedPlayerState.velocity[0] * cg.refdef_current->viewaxis[1][0] +
		                        (long double)cg.predictedPlayerState.velocity[1] * cg.refdef_current->viewaxis[1][1];
		pitchSpeed += (long double)cg.predictedPlayerState.velocity[2] * cg.refdef_current->viewaxis[0][2];
		rollSpeed += (long double)cg.predictedPlayerState.velocity[2] * cg.refdef_current->viewaxis[1][2];
		angles[PITCH] = (float)(pitchSpeed * .002f + angles[PITCH]);
		angles[ROLL] = (float)((long double)angles[ROLL] - rollSpeed * .005f);
	}
#else
	VectorAdd(angles, cg.kickAngles, angles);
	if (cg.damageTime)
	{
		ratio = (float)cg.time - cg.damageTime;
		if (ratio < 100)
		{
			ratio *= .01f;
		}
		else
		{
			ratio = 1 - (ratio - 100) * .0025f;
		}
		if ((float)cg.time - cg.damageTime < 100 || ratio > 0)
		{
			angles[PITCH] += cg.v_dmg_pitch * ratio;
			angles[ROLL]  += cg.v_dmg_roll * ratio;
		}
	}
	angles[PITCH] += DotProduct(cg.predictedPlayerState.velocity, cg.refdef_current->viewaxis[0]) * .002f;
	angles[ROLL]  -= DotProduct(cg.predictedPlayerState.velocity, cg.refdef_current->viewaxis[1]) * .005f;
#endif
	AngleVectors(angles, shotForward, right, NULL);
	if (cg.bobfracsin == 0 && cg.lastvalidBobfracsin > 0)
	{
#if defined(__linux__)
		cg.lastvalidBobfracsin = (float)((long double)cg.lastvalidBobfracsin -
		                               (long double)cg.frametime * (long double)0.005);
#else
		cg.lastvalidBobfracsin -= (float)cg.frametime * .005f;
#endif
	}
	origin[2] += cg.predictedPlayerState.viewheight;
#if defined(__linux__)
	elapsed = TCE_SubInt32(cg.time, cg.duckTime);
	duration = ((cg.predictedPlayerState.eFlags & EF_PRONE) ||
	            TCE_AddInt32(cg.tceProneTime, cg.time) < 750) ? 750 : 250;
#else
	elapsed    = cg.time - cg.duckTime;
	duration   = ((cg.predictedPlayerState.eFlags & EF_PRONE) || cg.tceProneTime + cg.time < 750)?750:250;
#endif
	if (elapsed < 0)
	{
#if defined(__linux__)
		cg.duckTime = TCE_SubInt32(cg.time, duration);
#else
		cg.duckTime = cg.time - duration;
#endif
	}
	if (elapsed < duration)
	{
#if defined(__linux__)
		long double change = (long double)TCE_SubInt32(duration, elapsed) * cg.duckChange *
		                     (duration == 750 ? .0013333333190530539f : .004f);
		origin[2] = (float)((long double)origin[2] - change);
#else
		origin[2] -= (float)(duration - elapsed) * cg.duckChange * (duration == 750?.0013333333190530539f:.004f);
#endif
	}
#if defined(__linux__)
	{
		long double stride, strideScale;
		if (cg.snap->ps.stats[STAT_TCE_WEAPON_FLAGS] & 0x4000)
		{
			stride = (long double).06666667014360428f * cg.xyspeed; strideScale = .15f;
		}
		else if (cg.predictedPlayerState.eFlags & EF_PRONE)
		{
			stride = (long double).03333333507180214f * cg.xyspeed; strideScale = .6f;
		}
		else if (cg.predictedPlayerState.pm_flags & PMF_DUCKED)
		{
			stride = (long double).03333333507180214f * cg.xyspeed; strideScale = .4f;
		}
		else
		{
			stride = (long double).0031250000465661287f * cg.xyspeed;
			stride += stride; strideScale = .1f;
		}
		if (stride > 4)
		{
			stride = 4;
		}
		vertical = (float)(1.0L - (long double)cg.bobfracsin * cg.bobfracsin);
		lateral = (float)((cg.bobcycle & 1 ? -1.0L : 1.0L) *
		                  ((long double)cg.bobfracsin * stride * strideScale));
		amplitude = (float)stride;
	}
	right[2] = 0; VectorNormalize(right);
	origin[0] = (float)((long double)right[0] * lateral + origin[0]);
	origin[1] = (float)((long double)right[1] * lateral + origin[1]);
	origin[2] = (float)((long double)lateral * right[2] + origin[2] +
	                    ((long double)vertical - .2f) * amplitude);
	delta = TCE_SubInt32(cg.time, cg.landTime);
#else
	if (cg.snap->ps.stats[STAT_TCE_WEAPON_FLAGS] & 0x4000)
	{
		amplitude = cg.xyspeed * .06666667014360428f; scale = .15f;
	}
	else if (cg.predictedPlayerState.eFlags & EF_PRONE)
	{
		amplitude = cg.xyspeed * .03333333507180214f; scale = .6f;
	}
	else if (cg.predictedPlayerState.pm_flags & PMF_DUCKED)
	{
		amplitude = cg.xyspeed * .03333333507180214f; scale = .4f;
	}
	else
	{
		amplitude = cg.xyspeed * .00625f; scale = .1f;
	}
	if (amplitude > 4)
	{
		amplitude = 4;
	}
	lateral  = cg.bobfracsin * amplitude * scale;
	vertical = 1 - cg.bobfracsin * cg.bobfracsin;
	if (cg.bobcycle & 1)
	{
		lateral = -lateral;
	}
	right[2] = 0; VectorNormalize(right);
	VectorMA(origin, lateral, right, origin);
	origin[2] += (vertical - .2f) * amplitude;
	delta      = (float)(cg.time - cg.landTime);
#endif
	if (delta < 0)
	{
#if defined(__linux__)
		cg.landTime = TCE_SubInt32(cg.time, 450);
#else
		cg.landTime = cg.time - 450;
#endif
	}
	if (delta < 150)
	{
#if defined(__linux__)
		origin[2] = (float)(delta * .006666666828095913f * cg.landChange + origin[2]);
#else
		origin[2] += delta * .006666666828095913f * cg.landChange;
#endif
	}
	else if (delta < 450)
	{
#if defined(__linux__)
		float recovery = (float)(1.0L - (delta - 150.0f) * .0033333334140479565f);
		origin[2] = (float)((long double)recovery * cg.landChange + origin[2]);
#else
		origin[2] += (1 - (delta - 150) * .0033333334140479565f) * cg.landChange;
#endif
	}
	CG_StepOffset();
	lean = cg.predictedPlayerState.leanf;
	if (lean != 0)
	{
#if defined(__linux__)
		float multiplier = 1;
		if (cgs.tceLeanMode < 1)
		{
			angles[ROLL] = (float)((long double).5f * lean + angles[ROLL]);
		}
		else
		{
			long double reciprocal = 1.0L / (lean < 0 ? 3.3f : 1.8f);
			angles[ROLL] = (float)((long double)lean * reciprocal + angles[ROLL]);
			multiplier = (float)reciprocal;
		}
		AngleVectors(angles, NULL, right, NULL);
		origin[0] = (float)((long double)cg.predictedPlayerState.leanf * multiplier * right[0] + origin[0]);
		origin[1] = (float)((long double)cg.predictedPlayerState.leanf * multiplier * right[1] + origin[1]);
		origin[2] = (float)((long double)cg.predictedPlayerState.leanf * multiplier * right[2] + origin[2]);
#else
		scale = lean < 0?3.3f:1.8f;
		if (cgs.tceLeanMode < 1)
		{
			angles[ROLL] += lean * .5f;
		}
		else
		{
			lean /= scale; angles[ROLL] += lean;
		}
		AngleVectors(angles, NULL, right, NULL);
		VectorMA(origin, lean, right, origin);
#endif
	}
	VectorAdd(origin, cg.kick_origin, origin);
	origin[2] -= 8;
	AngleVectors(angles, forward, NULL, up);
#if defined(__linux__)
	{
		int axis;
		for (axis = 0; axis < 3; ++axis)
			origin[axis] = (float)((long double)forward[axis] * 4.0f + origin[axis]);
		for (axis = 0; axis < 3; ++axis)
			origin[axis] = (float)((long double)up[axis] * 8.0f + origin[axis]);
	}
#else
	VectorMA(origin, 4, forward, origin); VectorMA(origin, 8, up, origin);
#endif
	if (cg.time < cg.tceShotHoldUntil && weapon >= 0 && weapon < TCE_MAX_WEAPONS)
	{
#if defined(__linux__)
		long double kick = (long double)TCE_SubInt32(cg.tceShotHoldUntil, cg.time) *
		                   weaponDef[weapon].unknown_0f8[cg.tceAimActive ? 8 : 7] * .01f * .01f;
		int axis;
		for (axis = 0; axis < 3; ++axis)
			cg.refdef.vieworg[axis] = (float)(kick * shotForward[axis] * -.5f + cg.refdef.vieworg[axis]);
#else
		float kick = (cg.tceShotHoldUntil - cg.time) * .01f *
		             weaponDef[weapon].unknown_0f8[cg.tceAimActive?8:7] * .01f * -.5f;
		VectorMA(cg.refdef.vieworg, kick, shotForward, cg.refdef.vieworg);
#endif
	}
}

//======================================================================

//
// Zoom controls
//


// probably move to server variables
float zoomTable[ZOOM_MAX_ZOOMS][2] =
{
// max {out,in}
	{ 0,  0  },

	{ 36, 8  }, //	binoc
	{ 90, 60 },   // TC zoomTable300b35e8: scoped FOV bounds
	{ 60, 20 },   //	snooper
	{ 55, 55 },   //	fg42
	{ 55, 55 }    //	mg42
};

void CG_AdjustZoomVal(float val, int type)
{
	cg.zoomval += val;
	if (cg.zoomval > zoomTable[type][ZOOM_OUT])
	{
		cg.zoomval = zoomTable[type][ZOOM_OUT];
	}
	if (cg.zoomval < zoomTable[type][ZOOM_IN])
	{
		cg.zoomval = zoomTable[type][ZOOM_IN];
	}
}

void CG_ZoomIn_f(void)
{
	// Gordon: fixed being able to "latch" your zoom by weaponcheck + quick zoomin
	// OSP - change for zoom view in demos
	if (cg_entities[cg.snap->ps.clientNum].currentState.weapon == 57)
	{
		CG_AdjustZoomVal(-(cg_zoomStepSniper.value), ZOOM_SNIPER);
	}
	else if (cg_entities[cg.snap->ps.clientNum].currentState.weapon == 58)
	{
		CG_AdjustZoomVal(-(cg_zoomStepSniper.value), ZOOM_SNIPER);
	}
	else if (cg.zoomedBinoc)
	{
		CG_AdjustZoomVal(-(cg_zoomStepSniper.value), ZOOM_SNIPER); // JPW NERVE per atvi request all use same vals to match menu (was zoomStepBinoc, ZOOM_BINOC);
	}
}

void CG_ZoomOut_f(void)
{
	if (cg_entities[cg.snap->ps.clientNum].currentState.weapon == 57)
	{
		CG_AdjustZoomVal(cg_zoomStepSniper.value, ZOOM_SNIPER);
	}
	else if (cg_entities[cg.snap->ps.clientNum].currentState.weapon == 58)
	{
		CG_AdjustZoomVal(cg_zoomStepSniper.value, ZOOM_SNIPER);
	}
	else if (cg.zoomedBinoc)
	{
		CG_AdjustZoomVal(cg_zoomStepSniper.value, ZOOM_SNIPER); // JPW NERVE per atvi request BINOC);
	}
}


/*
==============
CG_Zoom
==============
*/
void CG_Zoom(void)
{
#if defined(__linux__)
	tce_weaponDef_t *def;
	if ((cg.snap->ps.pm_flags & PMF_FOLLOW) || cg.demoPlayback)
	{
		cg.predictedPlayerState.eFlags = cg.snap->ps.eFlags;
		cg.predictedPlayerState.weapon = cg.snap->ps.weapon;
		def = &weaponDef[cg.predictedPlayerState.weapon];
		if (def->scoped > 1.0f && cg.tceAimActive && !cg_portalScopes.integer)
		{
			cg.zoomval = (float)((1.0L / def->scoped) * 90.0f);
		}
		else
		{
			cg.zoomval = def->unknown_1ac > 0 ? (float)def->unknown_1ac : 90.0f;
		}
	}
	if (!cg.tceAimActive)
	{
		cg.zoomval = 0;
	}
	else if (TCE_FloatIsZeroOrNaN(cg.zoomval))
	{
		def = &weaponDef[cg.predictedPlayerState.weapon];
		if (def->scoped > 1.0f && !cg_portalScopes.integer)
		{
			cg.zoomval = (float)((1.0L / def->scoped) * 90.0f);
		}
		else
		{
			cg.zoomval = def->unknown_1ac > 0 ? (float)def->unknown_1ac : 90.0f;
		}
	}
#else
	tce_weaponDef_t *def;
	if ((cg.snap->ps.pm_flags & PMF_FOLLOW) || cg.demoPlayback)
	{
		cg.predictedPlayerState.eFlags = cg.snap->ps.eFlags;
		cg.predictedPlayerState.weapon = cg.snap->ps.weapon;
		def                            = &weaponDef[cg.predictedPlayerState.weapon];
		cg.zoomval                     = def->scoped > 1.0f && cg.tceAimActive && !cg_portalScopes.integer ? 90.0f / def->scoped : (def->unknown_1ac > 0?(float)def->unknown_1ac:90.0f);
	}
	if (!cg.tceAimActive)
	{
		cg.zoomval = 0;
	}
	else if (cg.zoomval == 0)
	{
		def        = &weaponDef[cg.predictedPlayerState.weapon];
		cg.zoomval = def->scoped > 1.0f && !cg_portalScopes.integer ? 90.0f / def->scoped : (def->unknown_1ac > 0?(float)def->unknown_1ac:90.0f);
	}
#endif
}

void CG_ToggleAiming(void)
{
	unsigned int elapsed = (unsigned int)cg.time - (unsigned int)cg.zoomTime;
	unsigned int transitionBits;
	int          transition;

	/* The original clamps the sign bit after a wrapping 32-bit subtraction. */
	if (elapsed & 0x80000000u)
	{
		elapsed = 0;
	}
	cg.tceAimRequested   = !cg.tceAimRequested;
	cg.tceAimActive      = cg.tceAimRequested;
	cg.tceAimComplete    = 0;
	cg.tceAimWeaponLatch = 0;
	transitionBits       = elapsed >= 200u ? (unsigned int)cg.time :
	                       (unsigned int)cg.time - 200u + elapsed;
	/* Preserve the wrapped timestamp without an out-of-range signed cast. */
	memcpy(&transition, &transitionBits, sizeof(transition));
	cg.zoomTime        = transition;
	cg.tceAimSyncTime  = transition;
	cg.tceAimRetryTime = 0;
}

/*
====================
CG_CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/
#define WAVE_AMPLITUDE  1
#define WAVE_FREQUENCY  0.4

static int CG_CalcFov(void)
{
	static float lastfov = 90.0f;
#if defined(__linux__)
	long double  base, fov_x, zoomFov, f;
	float        fov_y, x, v;
#else
	float        base, fov_x, fov_y, zoomFov, f, x, v;
#endif
	int          contents, inwater;
	if (cg.tceAimRetryTime && cg.time >= cg.tceAimRetryTime && cg.tceAimRequested)
	{
		if (cg.tceAimActive)
		{
			cg.tceAimActive = 0;
#if defined(__linux__)
			cg.tceAimRetryTime = TCE_AddInt32(cg.time, 1300);
#else
			cg.tceAimRetryTime = cg.time + 1300;
#endif
		}
		else
		{
			cg.tceAimActive = 1; cg.tceAimRetryTime = 0;
		}
		cg.tceAimComplete = 0; cg.zoomTime = cg.time; cg.tceAimSyncTime = cg.time;
	}
#if defined(__linux__)
	if (TCE_SubInt32(cg.time, cg.tceAimSyncTime) > 750)
#else
	if (cg.time - cg.tceAimSyncTime > 750)
#endif
	{
		if ((cg.snap->ps.stats[STAT_TCE_WEAPON_FLAGS] & 4) && !cg.tceAimRequested)
		{
			CG_Printf("force aimed\n");
			cg.tceAimRequested = cg.tceAimComplete = cg.tceAimActive = 1;
		}
		else if (!(cg.snap->ps.stats[STAT_TCE_WEAPON_FLAGS] & 4) && cg.tceAimRequested)
		{
			cg.tceAimRequested = cg.tceAimComplete = cg.tceAimActive = 0;
			CG_Printf("force unaimed\n");
		}
	}
	CG_Zoom();
	if (cg.predictedPlayerState.stats[STAT_HEALTH] < 1 && !(cg.snap->ps.pm_flags & PMF_FOLLOW))
	{
		cg.zoomedBinoc     = 0; cg.zoomTime = 0; cg.zoomval = 0;
		cg.tceAimRequested = cg.tceAimComplete = cg.tceAimActive = 0;
	}
	if (cg.predictedPlayerState.pm_type == PM_INTERMISSION)
	{
		fov_x = 90;
	}
	else
	{
		base = developer.integer?cg_fov.value:90.0f; fov_x = base;
		if (!cg.renderingThirdPerson || developer.integer)
		{
			zoomFov = lastfov;
			if (cg.zoomval != 0)
			{
				zoomFov = cg.zoomval; if (zoomFov < 1)
				{
					zoomFov = 1;
				}
				if (zoomFov > 160)
				{
					zoomFov = 160;
				}
			}
#if defined(__linux__)
			f = (long double)TCE_SubInt32(cg.time, cg.zoomTime) * 0.005f;
#else
			f = (cg.time - cg.zoomTime) * 0.005f;
#endif
			if (cg.zoomedBinoc)
			{
				fov_x = f <= 1?(zoomFov - base) * f + base:zoomFov; lastfov = fov_x;
			}
			else if (cg.tceAimActive)
			{
				if (f <= 1)
				{
					fov_x = (zoomFov - base) * f + base;
				}
				else
				{
					cg.tceAimComplete = 1; fov_x = zoomFov;
				}
				lastfov = fov_x;
			}
			else
			{
				cg.tceAimComplete = 0; if (f <= 1)
				{
					fov_x = (base - zoomFov) * f + zoomFov;
				}
			}
		}
	}
	cg.refdef_current->rdflags &= ~RDF_SNOOPERVIEW;
	if (cg.snap->ps.persistant[PERS_HWEAPON_USE] || cg.snap->ps.weapon == 62)
	{
		fov_x = 55;
	}
	else if (cg.snap->ps.eFlags & EF_MOUNTEDTANK)
	{
		fov_x = 75;
	}
	if (cg.showGameView)
	{
		fov_x = 60;
	}
	if (cg_aspectFovMode.integer == 1)
	{
		if (cg_aspectMode.integer == 1)
		{
			fov_x *= 1.0466f;
		}
		else if (cg_aspectMode.integer == 0)
		{
			fov_x *= 1.0747f;
		}
	}
	else if (cg_aspectFovMode.integer == 2)
	{
		if (cg_aspectMode.integer == 1)
		{
			fov_x *= 1.0954f;
		}
		else if (cg_aspectMode.integer == 0)
		{
			fov_x *= 1.1547f;
		}
	}
#if defined(__linux__)
	{
		double tangent = (double)tanl(fov_x * (1.0f / 360.0f) * (long double)M_PI);
		double angle;
		cg.tceNameTanHalfFov = (float)tangent;
		x = (float)((1.0L / tangent) * cg.refdef_current->width);
		angle = (double)atan2l(cg.refdef_current->height, x);
		fov_y = (float)((long double)(float)angle * 360.0f * (long double)(1.0 / M_PI));
		fov_x = (float)fov_x;
	}
#else
	cg.tceNameTanHalfFov = (float)tan(fov_x / 360 * M_PI);
	x                    = cg.refdef_current->width / tan(fov_x / 360 * M_PI);
	fov_y                = atan2(cg.refdef_current->height, x) * 360 / M_PI;
#endif
	contents             = CG_PointContents(cg.refdef.vieworg, -1);
	inwater              = (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) != 0;
	if (inwater)
	{
#if defined(__linux__)
		float angle = (float)((long double)cg.time * (long double)0.0012566370614359172 * 2.0L);
		v = (float)sinl(angle);
#else
		v = sin(cg.time / 1000.0 * 0.4 * M_PI * 2);
#endif
		fov_x += v; fov_y -= v; cg.refdef_current->rdflags |= RDF_UNDERWATER;
	}
	else
	{
		cg.refdef_current->rdflags &= ~RDF_UNDERWATER;
	}
	cg.refdef_current->fov_x = fov_x; cg.refdef_current->fov_y = fov_y;
	if (cg.snap->ps.pm_type == PM_FREEZE || cg.snap->ps.pm_type == PM_DEAD || (cg.snap->ps.pm_flags & PMF_TIME_LOCKPLAYER))
	{
		cg.zoomSensitivity = 0;
	}
	else if (cg.zoomedBinoc)
	{
#if defined(__linux__)
		cg.zoomSensitivity = (float)((long double)(1.0 / 75.0) * cg.refdef_current->fov_y);
#else
		cg.zoomSensitivity = cg.refdef_current->fov_y / 75.0f;
#endif
	}
	else if (cg.zoomval == 0)
	{
		cg.zoomSensitivity = 1;
	}
	else
	{
		cg.zoomSensitivity = cg.zoomval * (1.0f / 90.0f); if (weaponDef[cg.predictedPlayerState.weapon].scoped >= 4)
		{
			cg.zoomSensitivity *= 0.5f;
		}
	}
	return inwater;
}

/*
==============
CG_UnderwaterSounds
==============
*/
#define UNDERWATER_BIT 16
static void CG_UnderwaterSounds(void)
{
//	trap_S_AddLoopingSound( cent->lerpOrigin, vec3_origin, cgs.media.underWaterSound, 255, 0 );
	trap_S_AddLoopingSound(cg.snap->ps.origin, vec3_origin, cgs.media.underWaterSound, 255 | (1 << UNDERWATER_BIT), 0);
}


/*
===============
CG_DamageBlendBlob

===============
*/
static void CG_DamageBlendBlob(void)
{
	int i, axis;
	if (((cg.snap->ps.pm_flags & PMF_LIMBO) ||
	     cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) && cg.showGameView)
	{
		return;
	}
	if (cgs.glconfig.hardwareType == GLHW_RAGEPRO)
	{
		return;
	}
	for (i = 0; i < MAX_VIEWDAMAGE; ++i)
	{
		viewDamage_t *vd = &cg.viewDamage[i];
		refEntity_t  ent;
		int          elapsed, duration;
#if !defined(__linux__)
		double       right, up;
#endif
		float        alpha;
		if (!vd->damageValue)
		{
			continue;
		}
#if defined(__linux__)
		elapsed = TCE_SubInt32(cg.time, vd->damageTime);
#else
		elapsed  = cg.time - vd->damageTime;
#endif
		duration = vd->damageDuration;
		if (elapsed <= 0 || elapsed >= duration)
		{
			vd->damageValue = 0;
			continue;
		}
		if (!vd->damageX && !vd->damageY)
		{
			continue;
		}
		memset(&ent, 0, sizeof(ent));
		ent.reType   = RT_SPRITE;
		ent.renderfx = RF_FIRST_PERSON;
#if defined(__linux__)
		for (axis = 0; axis < 3; ++axis)
		{
			long double position = (long double)cg.refdef_current->viewaxis[0][axis] * 8.0f +
			                       cg.refdef_current->vieworg[axis];
			position += (long double)vd->damageX * cg.refdef_current->viewaxis[1][axis] * -8.0f;
			position += (long double)vd->damageY * cg.refdef_current->viewaxis[2][axis] * 8.0f;
			ent.origin[axis] = (float)position;
		}
		{
			long double inverseDuration = 1.0L / duration;
			long double radius = (0.5L * elapsed * inverseDuration + 0.5L) * vd->damageValue;
			radius *= fabsl(sinl((long double)vd->damageTime)) * 0.5L + 0.75L;
			ent.radius = (float)(radius * (long double)0.4);
			ent.customShader = cgs.media.viewBloodAni[(int)floorl((long double)elapsed * inverseDuration * (long double)4.9)];
		}
#else
		right        = (double)vd->damageX * -8.f;
		up           = (double)vd->damageY * 8.f;
		for (axis = 0; axis < 3; ++axis)
		{
			ent.origin[axis] = (float)((double)cg.refdef_current->viewaxis[0][axis] * 8.f +
			                           cg.refdef_current->vieworg[axis]);
			ent.origin[axis] = (float)(right * cg.refdef_current->viewaxis[1][axis] + ent.origin[axis]);
			ent.origin[axis] = (float)(up * cg.refdef_current->viewaxis[2][axis] + ent.origin[axis]);
		}
		ent.radius = (float)(((double)elapsed * .5 / duration + .5) *
		                     (fabs(sin((double)vd->damageTime)) * .5 + .75) *
		                     vd->damageValue * .4);
		ent.customShader  = cgs.media.viewBloodAni[(int)floor((double)elapsed / duration * 4.9)];
#endif
		ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = 255;
		alpha             = cg_bloodDamageBlend.value;
		if (alpha > 1.f)
		{
			alpha = 1.f;
		}
		else if (alpha < 0.f)
		{
			alpha = 0.f;
		}
		ent.shaderRGBA[3] = (byte)(255.0 * alpha);
		trap_R_AddRefEntityToScene(&ent);
	}
}

/*
===============
CG_DrawScreenFade
===============
*/
#if defined(TCE_LINUX_ROOM_X87)
static void CG_TCEEyeRayEndpoint(const refdef_t *view, float side, float up,
                                vec3_t end)
{
	volatile long double sideProduct[3], upProduct[3], forward[3];
	volatile long double direction[3], scaled[3], endpoint[3];
	int axis;

	for (axis = 0; axis < 2; ++axis)
	{
		sideProduct[axis] = (long double)view->viewaxis[1][axis] * (long double)side;
		upProduct[axis] = (long double)view->viewaxis[2][axis] * (long double)up;
	}
	sideProduct[2] = (long double)side * (long double)view->viewaxis[1][2];
	upProduct[2] = (long double)up * (long double)view->viewaxis[2][2];
	for (axis = 0; axis < 3; ++axis)
	{
		forward[axis] = sideProduct[axis] + (long double)view->viewaxis[0][axis];
	}
	direction[0] = forward[0] + upProduct[0];
	direction[1] = upProduct[1] + forward[1];
	direction[2] = upProduct[2] + forward[2];
	scaled[0] = direction[0] * 8192.0L;
	scaled[1] = direction[1] * 8192.0L;
	scaled[2] = 8192.0L * direction[2];
	for (axis = 0; axis < 3; ++axis)
	{
		/* Native nonpop direction stores do not narrow the endpoint inputs. */
		endpoint[axis] = scaled[axis] + (long double)view->vieworg[axis];
		end[axis] = (float)endpoint[axis];
	}
}
#endif

static void CG_SampleEyeLighting(void)
{
	static const float side[9] = { 0, -.25f, .25f, -.25f, .25f, -.5f, .5f, 0, 0 };
	static const float up[9]   = { 0, .15f, .15f, -.15f, -.15f, 0, 0, -.3f, .3f };
	vec3_t             eyeStart, end, point, light;
	trace_t            trace;
	float              strongest;
	int                i;
#if !defined(TCE_LINUX_ROOM_X87)
	int j;
#endif
	cg.tceEyeSkySamples = cg.tceEyeSurfaceSamples = 0;
	/* Each ray keeps the start sampled before the first engine callback.
	 * Its endpoint still uses the current view for that particular ray. */
	VectorCopy(cg.refdef.vieworg, eyeStart);
	for (i = 0; i < 9; i++)
	{
#if defined(TCE_LINUX_ROOM_X87)
		CG_TCEEyeRayEndpoint(&cg.refdef, side[i], up[i], end);
#else
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		if (!TCE_AmbientEyeRayEndpoint(cg.refdef.vieworg, cg.refdef.viewaxis,
		                              side[i], up[i], end))
#endif
		{
			for (j = 0; j < 3; j++)
				end[j] = cg.refdef.vieworg[j] + 8192.0f *
				         (up[i] * cg.refdef.viewaxis[2][j] + side[i] * cg.refdef.viewaxis[1][j] + cg.refdef.viewaxis[0][j]);
		}
#endif
		CG_Trace(&trace, eyeStart, NULL, NULL, end, cg.snap->ps.clientNum, CONTENTS_SOLID);
		if (trace.surfaceFlags & SURF_SKY)
		{
			cg.tceEyeSkySamples += 1;
		}
		else
		{
			VectorMA(trace.endpos, 8.0f, trace.plane.normal, point);
			TCE_CG_LightForParticleDirected(TCE_CG_MapLightGrid(), point, trace.plane.normal, light);
			strongest = light[0];
			if (strongest < light[1])
			{
				strongest = light[1];
			}
			if (strongest < light[2])
			{
				strongest = light[2];
			}
			cg.tceEyeSurfaceSamples += strongest;
		}
	}
}

/* Windows30068650 / Linux CG_EliteSndEffects000b51e8.
 * All 32 room traces, material classification and the environment double buffer. */
static void CG_TCERoomRay(vec3_t direction, const vec3_t signs, int axis)
{
	int j;
	for (j = 0; j < 3; j++)
	{
		int sample = rand() & 32767;
#if defined(TCE_LINUX_ROOM_X87)
		/* Linux keeps the product and centered sample until the Float store. */
		volatile long double product = (long double)sample * (1.0f / 32767.0f);
		volatile long double centered = product - .5L;
		volatile long double doubled = centered + centered;
		direction[j] = (float)doubled;
		continue;
#endif
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		if (TCE_AmbientRoomRandom((unsigned)sample, &direction[j]))
		{
			continue;
		}
#endif
		direction[j] = 2.0f * (sample * (1.0f / 32767.0f) - .5f);
	}
	if (axis < 3)
	{
		direction[axis] += signs[axis];
	}
	else
	{
		direction[2] -= 1.0f;
	}
	VectorNormalize(direction);
}

#if defined(TCE_LINUX_ROOM_X87)
static void CG_TCEReflectRoomRayEndpoint(vec3_t direction, const vec3_t normal,
                                        const vec3_t start, vec3_t end)
{
	volatile long double xProduct = (long double)direction[0] * normal[0];
	volatile long double yProduct = (long double)direction[1] * normal[1];
	volatile long double xy = yProduct + xProduct;
	volatile long double zProduct = (long double)direction[2] * normal[2];
	volatile long double dot = xy + zProduct;
	volatile long double scaled[3], reflected[3], endpoint[3];
	int axis;

	/* Linux multiplies each normal by the retained dot before applying -2. */
	for (axis = 0; axis < 2; ++axis)
	{
		scaled[axis] = (long double)normal[axis] * dot;
	}
	scaled[2] = dot * (long double)normal[2];
	for (axis = 0; axis < 2; ++axis)
	{
		scaled[axis] = scaled[axis] * -2.0L;
	}
	scaled[2] = -2.0L * scaled[2];
	for (axis = 0; axis < 3; ++axis)
	{
		reflected[axis] = (long double)direction[axis] + scaled[axis];
	}
	/* Nonpop Float stores do not narrow the values used by the endpoint. */
	direction[0] = (float)reflected[0];
	endpoint[0] = reflected[0] * 2048.0L;
	direction[1] = (float)reflected[1];
	direction[2] = (float)reflected[2];
	endpoint[1] = reflected[1] * 2048.0L;
	endpoint[2] = reflected[2] * 2048.0L;
	for (axis = 0; axis < 3; ++axis)
	{
		endpoint[axis] = endpoint[axis] + (long double)start[axis];
	}
	for (axis = 0; axis < 3; ++axis)
	{
		end[axis] = (float)endpoint[axis];
	}
}
#else
static void CG_TCEReflectRoomRay(vec3_t direction, const vec3_t normal)
{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	if (TCE_AmbientReflect(direction, normal))
	{
		return;
	}
#endif
#if defined(_WIN32)
	/* Windows adds z+y before x and retains the reflection scale until
	 * the three new direction components have each been stored. */
	long double dot = (long double)normal[2] * direction[2] +
	                  (long double)normal[1] * direction[1];
	long double reflection;
	int         axis;
	dot       += (long double)normal[0] * direction[0];
	reflection = dot * -2.0L;
	for (axis = 0; axis < 3; axis++)
		direction[axis] = (float)((long double)normal[axis] * reflection + direction[axis]);
#else
	/* Linux still uses the existing expression. Its retained component
	 * and endpoint stages remain to be reconstructed. */
	float reflection = -2.0f * DotProduct(normal, direction);
	VectorMA(direction, reflection, normal, direction);
#endif
}
#endif

static qboolean CG_TCERoomLess(float left, float right)
{
#if defined(_WIN32) && !defined(Q3_VM)
	/* The original Windows C0 test includes unordered comparisons. */
	return !(left >= right);
#else
	return left < right;
#endif
}

#if (defined(_WIN32) && !defined(Q3_VM)) || defined(TCE_LINUX_ROOM_X87)
static void CG_TCECollectRoomMaterial(const trace_t *tr, float *hardness, int *sky)
{
	if (!(tr->fraction >= 1.0f))
	{
		unsigned flags = (unsigned)tr->surfaceFlags;
		unsigned material = flags & 0xff000000u;
		if (flags & SURF_SKY)
		{
			++*sky;
		}
		else if (material != 0x05000000u && material != 0x07000000u &&
		         material != 0x0a000000u && material != 0x0d000000u)
		{
			*hardness += 1.0f;
		}
	}
}
#endif

static void CG_TCECollectRoomTrace(const trace_t *tr, const vec3_t corner,
                                   vec3_t low, vec3_t high, float *distance,
                                   float *hardness, int *sky, qboolean secondTrace)
{
#if (defined(_WIN32) && !defined(Q3_VM)) || defined(TCE_LINUX_ROOM_X87)
	vec3_t span;
	int j;

	/* The second hit is classified before its span and length are evaluated. */
	if (secondTrace)
	{
		CG_TCECollectRoomMaterial(tr, hardness, sky);
	}
	VectorSubtract(tr->endpos, corner, span);
	for (j = 0; j < 3; ++j)
	{
		if (!(span[j] >= 0.0f))
		{
			if (!(span[j] >= low[j]))
			{
				memcpy(&low[j], &span[j], sizeof(float));
			}
		}
		else if (span[j] > high[j])
		{
			high[j] = span[j];
		}
	}
	*distance += VectorLength(span);
	if (!secondTrace)
	{
		CG_TCECollectRoomMaterial(tr, hardness, sky);
	}
#else
	vec3_t   span;
	unsigned material = (unsigned)tr->surfaceFlags & 0xff000000;
	int      j;
	(void)secondTrace;
	VectorSubtract(tr->endpos, corner, span);
	for (j = 0; j < 3; j++)
	{
		if (span[j] < 0)
		{
			if (span[j] < low[j])
			{
				low[j] = span[j];
			}
		}
		else if (span[j] > high[j])
		{
			high[j] = span[j];
		}
	}
	*distance += VectorLength(span);
	if (tr->fraction < 1)
	{
		if (tr->surfaceFlags & SURF_SKY)
		{
			++*sky;
		}
		else if (material != 0x05000000 && material != 0x07000000 &&
		         material != 0x0a000000 && material != 0x0d000000)
		{
			*hardness += 1;
		}
	}
#endif
}

#if (defined(_WIN32) && !defined(Q3_VM)) || defined(TCE_LINUX_ROOM_X87)
typedef struct
{
	int sounds[2];
	int effects[2];
	int volume;
} tceRoomZoneSnapshot_t;

static void CG_TCECaptureSoundZone(const entityState_t *es, tceRoomZoneSnapshot_t *value)
{
	value->sounds[0] = es->onFireEnd;
	value->sounds[1] = es->modelindex2;
#if defined(TCE_LINUX_ROOM_X87)
	value->volume = es->onFireStart;
#endif
	value->effects[0] = es->otherEntityNum;
	value->effects[1] = es->effect3Time;
#if !defined(TCE_LINUX_ROOM_X87)
	value->volume = es->onFireStart;
#endif
}

static float CG_TCEEnvironmentFraction(int value)
{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	float result;
	if (TCE_AmbientEnvironmentFraction(value, &result))
	{
		return result;
	}
#elif defined(TCE_LINUX_ROOM_X87)
	/* FILD32 is exact; only the scaled value is stored as Float. */
	volatile long double scaled = (long double)value * (long double)(1.0f / 255.0f);
	return (float)scaled;
#endif
	return value * (1.0f / 255.0f);
}

#if defined(TCE_LINUX_ROOM_X87)
static void CG_TCEPublishGlobalSoundZone(const tceRoomZoneSnapshot_t *value)
{
	int bank = 1 - cg.tceSoundZoneActive;

	/* Linux publishes the cached global zone, then its bank and timestamp. */
	cg.tceSoundZoneSounds[bank][1] = value->sounds[1];
	cg.tceSoundZoneSounds[bank][0] = value->sounds[0];
	cg.tceSoundZoneEffects[bank][0] = value->effects[0];
	cg.tceSoundZoneEffects[bank][1] = value->effects[1];
	cg.tceSoundZoneVolume[bank] = value->volume;
	cg.tceSoundZoneActive = bank;
	cg.tceSoundZoneTransitionTime = cg.time;
}
#else
static void CG_TCEPublishSoundZone(const tceRoomZoneSnapshot_t *value)
{
	int bank = 1 - cg.tceSoundZoneActive;

	cg.tceSoundZoneSounds[bank][0] = value->sounds[0];
	cg.tceSoundZoneSounds[bank][1] = value->sounds[1];
	cg.tceSoundZoneEffects[bank][0] = value->effects[0];
	cg.tceSoundZoneEffects[bank][1] = value->effects[1];
	cg.tceSoundZoneVolume[bank] = value->volume;
	cg.tceSoundZoneTransitionTime = cg.time;
	cg.tceSoundZoneActive = bank;
}
#endif
#endif

static void CG_TCESetSoundZone(const entityState_t *es)
{
#if defined(_WIN32) && !defined(Q3_VM)
	tceRoomZoneSnapshot_t value = { { 0, 0 }, { 0, 0 }, 0 };
	if (es)
	{
		CG_TCECaptureSoundZone(es, &value);
	}
	CG_TCEPublishSoundZone(&value);
#else
	int bank = 1 - cg.tceSoundZoneActive;
	cg.tceSoundZoneSounds[bank][0]  = es?es->onFireEnd:0;
	cg.tceSoundZoneSounds[bank][1]  = es?es->modelindex2:0;
	cg.tceSoundZoneEffects[bank][0] = es?es->otherEntityNum:0;
	cg.tceSoundZoneEffects[bank][1] = es?es->effect3Time:0;
	cg.tceSoundZoneVolume[bank]     = es?es->onFireStart:0;
	cg.tceSoundZoneActive           = bank;
	cg.tceSoundZoneTransitionTime   = cg.time;
#endif
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
static float CG_TCERoomExtent(float previous, float high, float low, int axis)
{
	float result;
	if (TCE_AmbientRoomExtent(previous, high, low, axis, &result))
	{
		return result;
	}
	return previous * .6667f + (high - low) * .3333f;
}

static float CG_TCERoomBlend(float previous, float sample, float scale)
{
	float result;
	if (TCE_AmbientRoomBlend(previous, sample, scale, &result))
	{
		return result;
	}
	return previous * .6667f + sample * scale * .3333f;
}
#endif

#if defined(TCE_LINUX_ROOM_X87)
static void CG_TCEFilterRoom(const vec3_t low, const vec3_t high,
                             float distance, float hardness, int sky)
{
	volatile long double decay = (long double).6667f;
	volatile long double fresh = 1.0L - decay;
	volatile float scaledDistance = .125L * (long double)distance;
	volatile float scaledHardness = .0625L * (long double)hardness;
	volatile long double span, contribution, previous, extent[3];
	volatile long double filteredDistance, filteredHardness, filteredSky;
	volatile long double decayedWeight[4], boosted;
	float weight;
	int axis, kind;

	for (axis = 0; axis < 3; ++axis)
	{
		span = (long double)high[axis] - (long double)low[axis];
		contribution = span * fresh;
		previous = decay * (long double)cg.tceRoomExtent[axis];
		extent[axis] = previous + contribution;
	}
	contribution = (long double)scaledDistance * fresh;
	previous = decay * (long double)cg.tceRoomDistance;
	filteredDistance = contribution + previous;
	for (axis = 0; axis < 3; ++axis)
	{
		cg.tceRoomExtent[axis] = (float)extent[axis];
	}
	cg.tceRoomDistance = (float)filteredDistance;
	previous = decay * (long double)cg.tceRoomHardness;
	contribution = (long double)scaledHardness * fresh;
	filteredHardness = previous + contribution;
	cg.tceRoomHardness = (float)filteredHardness;
	previous = decay * (long double)cg.tceRoomSky;
	contribution = (long double)sky * fresh;
	filteredSky = previous + contribution;
	cg.tceRoomSky = (float)filteredSky;
	for (kind = 0; kind < 4; ++kind)
	{
		decayedWeight[kind] = decay * (long double)cg.tceRoomWeights[kind];
		cg.tceRoomWeights[kind] = (float)decayedWeight[kind];
	}
	/* The first decision uses the held distance and hardness, not their stores. */
	if (!(filteredDistance >= 200.0L) && filteredHardness > (long double).8f)
	{
		boosted = decayedWeight[1] + fresh;
		cg.tceRoomWeights[1] = (float)boosted;
	}
	else
	{
		if (cg.tceRoomSky >= 1.0f && cg.tceRoomDistance > 400.0f)
		{
			kind = 3;
		}
		else if (!(cg.tceRoomExtent[2] >= 150.0f))
		{
			volatile long double horizontal = (long double)cg.tceRoomExtent[1] +
			                                  (long double)cg.tceRoomExtent[0];
			if (horizontal > 600.0L && cg.tceRoomHardness > .8f)
			{
				kind = 2;
			}
			else
			{
				kind = 0;
			}
		}
		else
		{
			kind = 0;
		}
		boosted = fresh + (long double)cg.tceRoomWeights[kind];
		cg.tceRoomWeights[kind] = (float)boosted;
	}
	weight = cg.tceRoomWeights[0];
	cg.tceSoundEnvironment = 0;
	if (cg.tceRoomWeights[1] > weight)
	{
		weight = cg.tceRoomWeights[1];
		cg.tceSoundEnvironment = 1;
	}
	if (cg.tceRoomWeights[2] > weight)
	{
		weight = cg.tceRoomWeights[2];
		cg.tceSoundEnvironment = 0;
	}
	if (cg.tceRoomWeights[3] > weight)
	{
		cg.tceSoundEnvironment = 3;
	}
}
#endif

static void CG_EliteSndEffects(void)
{
	vec3_t              low = { 0, 0, 0 }, high = { 0, 0, 0 }, corner, signs, dir, end, start;
	float               distance = 0, hardness = 0;
#if !defined(TCE_LINUX_ROOM_X87)
	float weight;
	int j, kind;
#endif
#if defined(TCE_LINUX_ROOM_X87)
	volatile float nearest = 1e8f, inRadius = 1e8f;
	volatile long double dist;
#else
	float nearest = 1e8f, inRadius = 1e8f, dist;
#endif
	int                 sky = 0, i, axis, closest = -1, bounded = -1, bank;
	int                 sampleTime = cg.time, timeUntilSample;
	unsigned            sampleBits;
	const entityState_t *global = NULL, *selected;
	trace_t             tr;
#if (defined(_WIN32) && !defined(Q3_VM)) || defined(TCE_LINUX_ROOM_X87)
	tceRoomZoneSnapshot_t globalZone = { { 0, 0 }, { 0, 0 }, 0 };
#endif
	/* The native clock subtraction and addition wrap in 32 bits. */
	sampleBits = (unsigned)cg.tceSoundSampleTime - (unsigned)sampleTime;
	memcpy(&timeUntilSample, &sampleBits, sizeof(timeUntilSample));
	if (timeUntilSample > 0)
	{
		return;
	}
	sampleBits = (unsigned)sampleTime + 100u;
	memcpy(&cg.tceSoundSampleTime, &sampleBits, sizeof(cg.tceSoundSampleTime));
	(void)Distance(cg.tceSoundLastOrigin, cg.snap->ps.origin);
	VectorCopy(cg.snap->ps.origin, cg.tceSoundLastOrigin);
	for (i = 0; i < 4; i++)
	{
		VectorCopy(cg.snap->ps.origin, corner);
		signs[0]   = i < 2?-1:1; signs[1] = (i == 0 || i == 2)?-1:1; signs[2] = 1;
		corner[0] += 15 * signs[0]; corner[1] += 15 * signs[1];
		for (axis = 0; axis < 4; axis++)
		{
			CG_TCERoomRay(dir, signs, axis);
			VectorMA(corner, 2048, dir, end);
			CG_Trace(&tr, corner, NULL, NULL, end, cg.snap->ps.clientNum, CONTENTS_SOLID);
			CG_TCECollectRoomTrace(&tr, corner, low, high, &distance, &hardness, &sky, qfalse);
			if (tr.fraction >= 1 || (tr.surfaceFlags & SURF_SKY))
			{
				VectorCopy(corner, start);
				CG_TCERoomRay(dir, signs, axis);
				VectorMA(start, 2048, dir, end);
			}
			else
			{
				VectorCopy(tr.endpos, start);
#if defined(TCE_LINUX_ROOM_X87)
				CG_TCEReflectRoomRayEndpoint(dir, tr.plane.normal, start, end);
#else
				CG_TCEReflectRoomRay(dir, tr.plane.normal);
				VectorMA(start, 2048, dir, end);
#endif
			}
			CG_Trace(&tr, start, NULL, NULL, end, cg.snap->ps.clientNum, CONTENTS_SOLID);
			CG_TCECollectRoomTrace(&tr, corner, low, high, &distance, &hardness, &sky, qtrue);
		}
	}
#if defined(TCE_LINUX_ROOM_X87)
	CG_TCEFilterRoom(low, high, distance, hardness, sky);
#else
	for (j = 0; j < 3; j++)
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		cg.tceRoomExtent[j] = CG_TCERoomExtent(cg.tceRoomExtent[j], high[j], low[j], j);
	cg.tceRoomDistance = CG_TCERoomBlend(cg.tceRoomDistance, distance, .125f);
	cg.tceRoomHardness = CG_TCERoomBlend(cg.tceRoomHardness, hardness, .0625f);
#else
		cg.tceRoomExtent[j] = cg.tceRoomExtent[j] * .6667f + (high[j] - low[j]) * .3333f;
	cg.tceRoomDistance = cg.tceRoomDistance * .6667f + distance * .125f * .3333f;
	cg.tceRoomHardness = cg.tceRoomHardness * .6667f + hardness * .0625f * .3333f;
#endif
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	if (!TCE_AmbientRoomClassify(cg.tceRoomDistance, cg.tceRoomHardness,
	                            cg.tceRoomExtent, sky, &cg.tceRoomSky,
	                            cg.tceRoomWeights, &cg.tceSoundEnvironment))
#endif
	{
		cg.tceRoomSky      = cg.tceRoomSky * .6667f + sky * .3333f;
		for (j = 0; j < 4; j++)
			cg.tceRoomWeights[j] *= .6667f;
		if (CG_TCERoomLess(cg.tceRoomDistance, 200.0f) && cg.tceRoomHardness > .8f)
		{
			kind = 1;
		}
		else if (cg.tceRoomSky >= 1 && cg.tceRoomDistance > 400)
		{
			kind = 3;
		}
		else if (CG_TCERoomLess(cg.tceRoomExtent[2], 150.0f) && cg.tceRoomExtent[0] + cg.tceRoomExtent[1] > 600 && cg.tceRoomHardness > .8f)
		{
			kind = 2;
		}
		else
		{
			kind = 0;
		}
		cg.tceRoomWeights[kind] += .3333f;
		weight                   = cg.tceRoomWeights[0]; cg.tceSoundEnvironment = 0;
		if (weight < cg.tceRoomWeights[1])
		{
			weight = cg.tceRoomWeights[1]; cg.tceSoundEnvironment = 1;
		}
		if (weight < cg.tceRoomWeights[2])
		{
			weight = cg.tceRoomWeights[2]; cg.tceSoundEnvironment = 0;
		}
		if (weight < cg.tceRoomWeights[3])
		{
			cg.tceSoundEnvironment = 3;
		}
	}
#endif
	if (trap_CM_PointContents(cg.snap->ps.origin, 0) & 0x2000)
	{
		cg.tceSoundEnvironment = 0;
	}
	else if (trap_CM_PointContents(cg.snap->ps.origin, 0) & 0x200)
	{
		cg.tceSoundEnvironment = 3;
	}
	cg.tceEnvironmentOutdoor = cg.tceEnvironmentDefault = 0;
	for (i = 0; i < MAX_GENTITIES; i++)
	{
		const entityState_t *es = &cg_entities[i].currentState;
		if (es->eType != ET_ENVIRONMENT)
		{
			continue;
		}
		dist = Distance(cg.snap->ps.origin, cg_entities[i].lerpOrigin);
		if (es->otherEntityNum2 == 1)
		{
			global                   = es;
#if (defined(_WIN32) && !defined(Q3_VM)) || defined(TCE_LINUX_ROOM_X87)
			CG_TCECaptureSoundZone(es, &globalZone);
#endif
#if (defined(_WIN32) && !defined(Q3_VM)) || defined(TCE_LINUX_ROOM_X87)
			cg.tceEnvironmentOutdoor = CG_TCEEnvironmentFraction(es->frame);
			cg.tceEnvironmentDefault = CG_TCEEnvironmentFraction(es->effect2Time);
			cg.tceEnvironmentSun     = CG_TCEEnvironmentFraction(es->nextWeapon);
#else
			cg.tceEnvironmentOutdoor = es->frame * (1.0f / 255.0f);
			cg.tceEnvironmentDefault = es->effect2Time * (1.0f / 255.0f);
			cg.tceEnvironmentSun     = es->nextWeapon * (1.0f / 255.0f);
#endif
		}
		else
		{
#if defined(TCE_LINUX_ROOM_X87)
			/* The candidate stays held; only an accepted best distance is stored. */
			if (!(dist >= (long double)nearest) && !es->density)
#else
			if (CG_TCERoomLess(dist, nearest) && !es->density)
#endif
			{
				nearest = dist; closest = i;
			}
#if defined(TCE_LINUX_ROOM_X87)
			if (!(dist >= (long double)inRadius) && dist <= (long double)es->dmgFlags)
#else
			if (CG_TCERoomLess(dist, inRadius) && (long double)dist <= (long double)es->dmgFlags)
#endif
			{
				inRadius = dist; bounded = i;
			}
		}
	}
	if (bounded >= 0)
	{
		closest = bounded;
	}
	bank = cg.tceSoundZoneActive;
#if (defined(_WIN32) && !defined(Q3_VM)) || defined(TCE_LINUX_ROOM_X87)
	if (cg.tceSoundEnvironment == 3 && global && globalZone.sounds[0] >= 0 && bounded == -1)
	{
		if (cg.tceSoundZoneSounds[bank][0] != globalZone.sounds[0] || cg.tceSoundZoneVolume[bank] != globalZone.volume)
		{
#if defined(TCE_LINUX_ROOM_X87)
			CG_TCEPublishGlobalSoundZone(&globalZone);
#else
			CG_TCEPublishSoundZone(&globalZone);
#endif
		}
	}
#else
	if (cg.tceSoundEnvironment == 3 && global && global->onFireEnd >= 0 && bounded == -1)
	{
		if (cg.tceSoundZoneSounds[bank][0] != global->onFireEnd || cg.tceSoundZoneVolume[bank] != global->onFireStart)
		{
			CG_TCESetSoundZone(global);
		}
	}
#endif
	else if (closest < 0)
	{
		/* The original compares with the global sound index (or -1), not zero. */
#if (defined(_WIN32) && !defined(Q3_VM)) || defined(TCE_LINUX_ROOM_X87)
		if (cg.tceSoundZoneSounds[bank][0] == (global ? globalZone.sounds[0] : -1))
#else
		if (cg.tceSoundZoneSounds[bank][0] == (global?global->onFireEnd:-1))
#endif
		{
			CG_TCESetSoundZone(NULL);
		}
	}
	else
	{
		selected = &cg_entities[closest].currentState;
		if (cg.tceSoundZoneSounds[bank][0] != selected->onFireEnd || cg.tceSoundZoneVolume[bank] != selected->onFireStart)
		{
			CG_TCESetSoundZone(selected);
		}
	}
	if (closest >= 0 && cg_entities[closest].currentState.effect2Time)
	{
		cg.tceEnvironmentDefault = 1;
	}
	if (cg_dynamicEye.value > 0 && cg.tceEyeScale > 0)
	{
		cg.tceEyeProbeReset[0] = cg.tceEyeProbeReset[1] = 0;
		CG_SampleEyeLighting();
	}
}

/* A loop is positioned independently for each ear. The engine callback may
 * change the current view or sound zone before the other ear is submitted. */
static void CG_AmbientLoopOrigin(vec3_t point, qboolean rightEar)
{
	const refdef_t *view = cg.refdef_current;
	int            axis;

	for (axis = 0; axis < 3; ++axis)
	{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		if (TCE_AmbientOrigin(view->vieworg[axis], view->viewaxis[1][axis],
		                      rightEar, &point[axis]))
		{
			continue;
		}
#endif
		{
#if defined(_WIN32)
			long double offset = (long double)view->viewaxis[1][axis] * 64.0f;
			point[axis] = rightEar ? (float)((long double)view->vieworg[axis] - offset)
			                       : (float)(offset + view->vieworg[axis]);
#else
			long double offset = (long double)view->viewaxis[1][axis] * (rightEar ? -64.0f : 64.0f);
			point[axis] = (float)(offset + view->vieworg[axis]);
#endif
		}
	}
}

static int CG_AmbientLoopVolume(int bank, long double gain)
{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	int amount = cg.tceSoundZoneVolume[bank];
	float deafness = tceFlash.deafness;
	int result;

	if (TCE_AmbientVolume(amount, deafness, (float)gain, &result))
	{
		return result;
	}
	return TCE_TruncateNativeInteger(((long double)amount *
	                                 (1.0L - (long double)deafness)) * gain);
#else
	long double amount  = (long double)cg.tceSoundZoneVolume[bank];
	long double hearing = 1.0L - tceFlash.deafness;
	return TCE_TruncateNativeInteger((amount * hearing) * gain);
#endif
}

static void CG_EliteSndEnvironment(void)
{
	unsigned    elapsedBits;
	int         elapsed, bank, sound, volume;
	long double blend = 1.0L;
	long double previousGain;
	float       previousRightGain;
	vec3_t      point;

	if (!cg.tceSoundZoneSounds[0][0] && !cg.tceSoundZoneSounds[1][0])
	{
		return;
	}

	/* ET's millisecond clock and the original subtraction wrap at 32 bits. */
	elapsedBits = (unsigned)cg.time - (unsigned)cg.tceSoundZoneTransitionTime;
	memcpy(&elapsed, &elapsedBits, sizeof(elapsed));
	if (elapsed < 1000)
	{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		float storedBlend;
		if (TCE_AmbientBlend(elapsed, &storedBlend))
		{
			blend = storedBlend;
		}
		else
#endif
		{
			blend = (long double)elapsed * 0.001f;
#if defined(_WIN32)
			blend = (float)blend;
#endif
		}
	}

	bank  = cg.tceSoundZoneActive;
	sound = cg.tceSoundZoneSounds[bank][0];
	if (sound)
	{
		CG_AmbientLoopOrigin(point, qfalse);
		volume = CG_AmbientLoopVolume(bank, blend);
#if !defined(_WIN32)
		/* Linux spills the fade before the first sound callback; its later
		 * ear and previous-zone decisions use that binary32 value. */
		blend = (float)blend;
#endif
		trap_S_AddLoopingSound(point, vec3_origin, cgs.gameSounds[sound], volume, 0);

		bank = cg.tceSoundZoneActive;
		CG_AmbientLoopOrigin(point, qtrue);
		volume = CG_AmbientLoopVolume(bank, blend);
		sound  = cg.tceSoundZoneSounds[bank][1];
		trap_S_AddLoopingSound(point, vec3_origin, cgs.gameSounds[sound], volume, 0);
	}

	bank  = 1 - cg.tceSoundZoneActive;
	sound = cg.tceSoundZoneSounds[bank][0];
	if (!sound || blend >= 1.0L)
	{
		return;
	}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	{
		float storedGain;
		if (TCE_FiniteDifferenceStore(1.0f, (float)blend, &storedGain))
		{
			previousGain = storedGain;
		}
		else
		{
			previousGain = (float)(1.0L - blend);
		}
	}
#else
	previousGain = 1.0L - blend;
#if defined(_WIN32)
	previousGain = (float)previousGain;
#endif
#endif
	previousRightGain = (float)previousGain;
	CG_AmbientLoopOrigin(point, qfalse);
	volume = CG_AmbientLoopVolume(bank, previousGain);
	trap_S_AddLoopingSound(point, vec3_origin, cgs.gameSounds[sound], volume, 0);

	bank = 1 - cg.tceSoundZoneActive;
	CG_AmbientLoopOrigin(point, qtrue);
	volume = CG_AmbientLoopVolume(bank, previousRightGain);
	sound  = cg.tceSoundZoneSounds[bank][1];
	trap_S_AddLoopingSound(point, vec3_origin, cgs.gameSounds[sound], volume, 0);
}

/* Complete Windows 30069520 / Linux CG_DrawScreenFade 000b632a.
 * The flare brightness input belongs to the still-open flare controller. */
void CG_DrawScreenFade(void)
{
	float       elapsed, target, surface, decay, strength;
	refEntity_t ent;
	int         oldTime = cg.tceEyeTime;
#if defined(__linux__)
	if (!(cg_dynamicEye.value > 0) || !(cg.tceEyeScale > 0))
#else
	if (cg_dynamicEye.value <= 0 || cg.tceEyeScale <= 0)
#endif
	{
		cg.tceScopeLightBoost = 0;
		return;
	}
	cg.tceEyeTime = cg.time;
#if defined(__linux__)
	elapsed = (float)TCE_SubInt32(cg.time, oldTime);
#else
	elapsed       = (float)(cg.time - oldTime);
#endif
	surface       = (cg.tceEyeSurfaceSamples * .1111111119389534f - .15f) * 1.1764700412750244f;
	if (surface < 0)
	{
		surface = 0;
	}
	target = cg.tceEyeFlare * .2f + (1 - cg.tceEyeSky) * cg.tceEyeSkySamples * .1111111119389534f + surface;
	if (target > 1)
	{
		target = 1;
	}
	if (target <= cg.tceEyeLinear)
	{
		cg.tceEyeLinear -= elapsed * .000125f;
		if (cg.tceEyeLinear < target)
		{
			cg.tceEyeLinear = target;
		}
	}
	else
	{
		cg.tceEyeLinear += elapsed * .0005f;
		if (cg.tceEyeLinear > target)
		{
			cg.tceEyeLinear = target;
		}
	}
	decay = elapsed * .001f;
	decay = (1 - decay) + decay * decay * .5f;
	if (decay < 0)
	{
		decay = 0;
	}
	cg.tceEyeSmooth = cg.tceEyeSmooth * decay + (1 - decay) * target;
	if (cg.tceEyeSmooth < 0)
	{
		cg.tceEyeSmooth = 0;
	}
	if (cg.tceEyeLinear < 0)
	{
		cg.tceEyeLinear = 0;
	}
	strength = cg.tceEyeScale * cg_dynamicEye.value;
	if (strength > 1)
	{
		strength = 1;
	}
	cg.tceScopeLightBoost = strength * (1 - (cg.tceEyeSmooth + cg.tceEyeLinear) * .5f);
#if defined(__linux__)
	if (!(cg.tceScopeLightBoost > 0))
#else
	if (cg.tceScopeLightBoost <= 0)
#endif
	{
		return;
	}
	memset(&ent, 0, sizeof(ent));
	ent.reType   = RT_SPRITE;
	ent.renderfx = 12;
	VectorMA(cg.refdef_current->vieworg, 4, cg.refdef_current->viewaxis[0], ent.origin);
	VectorMA(ent.origin, 7, cg.refdef_current->viewaxis[1], ent.origin);
	ent.radius        = 40;
	ent.customShader  = cgs.media.tceEyeAdaptationShader;
	ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = (byte)(int)(cg.tceScopeLightBoost * 255.0);
	trap_R_AddRefEntityToScene(&ent);
}

/*
===============
CG_CalcViewValues

Sets cg.refdef view values
===============
*/
int CG_CalcViewValues(void)
{
	playerState_t *ps;
	cg.tceWeaponViewValid = qfalse;

	memset(cg.refdef_current, 0, sizeof(cg.refdef));

	// strings for in game rendering
	// Q_strncpyz( cg.refdef.text[0], "Park Ranger", sizeof(cg.refdef_current->text[0]) );
	// Q_strncpyz( cg.refdef.text[1], "19", sizeof(cg.refdef_current->text[1]) );

	// calculate size of 3D view
	CG_CalcVrect();

	ps = &cg.predictedPlayerState;

	if (cg.cameraMode)
	{
		vec3_t origin, angles;
		float  fov = 90;
		float  x;

		if (trap_getCameraInfo(CAM_PRIMARY, cg.time, &origin, &angles, &fov))
		{
			VectorCopy(origin, cg.refdef_current->vieworg);
			angles[ROLL]  = 0;
			angles[PITCH] = -angles[PITCH];     // (SA) compensate for reversed pitch (this makes the game match the editor, however I'm guessing the real fix is to be done there)
			VectorCopy(angles, cg.refdefViewAngles);
			AnglesToAxis(cg.refdefViewAngles, cg.refdef_current->viewaxis);

			x                        = cg.refdef.width / tan(fov / 360 * M_PI);
			cg.refdef_current->fov_y = atan2(cg.refdef_current->height, x);
			cg.refdef_current->fov_y = cg.refdef_current->fov_y * 360 / M_PI;
			cg.refdef_current->fov_x = fov;

			// FIXME: this is really really bad
			trap_SendClientCommand(va("setCameraOrigin %f %f %f", origin[0], origin[1], origin[2]));
			return 0;

		}
		else
		{
			cg.cameraMode = qfalse;
			trap_Cvar_Set("cg_letterbox", "0");
			trap_SendClientCommand("stopCamera");
			trap_stopCamera(CAM_PRIMARY);               // camera off in client

			CG_Fade(0, 0, 0, 255, 0, 0);                // go black
			CG_Fade(0, 0, 0, 0, cg.time + 200, 1500);   // then fadeup
		}
	}

	// intermission view
	if (ps->pm_type == PM_INTERMISSION)
	{
		VectorCopy(ps->origin, cg.refdef_current->vieworg);
		VectorCopy(ps->viewangles, cg.refdefViewAngles);
		AnglesToAxis(cg.refdefViewAngles, cg.refdef_current->viewaxis);
		return CG_CalcFov();
	}

	if (cg.bobfracsin > 0 && !ps->bobCycle)
	{
		cg.lastvalidBobcycle   = cg.bobcycle;
		cg.lastvalidBobfracsin = cg.bobfracsin;
	}

	cg.bobcycle   = (ps->bobCycle & 128) >> 7;
	cg.bobfracsin = fabs(sin((ps->bobCycle & 127) / 127.0 * M_PI));
	cg.xyspeed    = sqrt(ps->velocity[0] * ps->velocity[0] + ps->velocity[1] * ps->velocity[1]);


	/* Original30069800: the camera game-view flag is distinct from the
	 * TC limbo panel overlay. Its position is produced by portalcam commands. */
	if (cg.showGameView)
	{
		VectorCopy(cgs.ccPortalPos, cg.refdef_current->vieworg);
		if (cgs.ccPortalEnt == -1)
		{
			VectorCopy(cgs.ccPortalAngles, cg.refdefViewAngles);
		}
		else
		{
			vec3_t direction;
			VectorSubtract(cg_entities[cgs.ccPortalEnt].lerpOrigin,
			               cg.refdef_current->vieworg, direction);
			vectoangles(direction, cg.refdefViewAngles);
		}
	}
	else if (cg.renderingThirdPerson && (ps->eFlags & EF_MG42_ACTIVE || ps->eFlags & EF_AAGUN_ACTIVE))      // Arnout: see if we're attached to a gun
	{
		centity_t *mg42 = &cg_entities[ps->viewlocked_entNum];
		vec3_t    forward;

		AngleVectors(ps->viewangles, forward, NULL, NULL);
		VectorMA(mg42->currentState.pos.trBase, -36, forward, cg.refdef_current->vieworg);
		cg.refdef_current->vieworg[2] = ps->origin[2];
		VectorCopy(ps->viewangles, cg.refdefViewAngles);
	}
	else if (ps->eFlags & EF_MOUNTEDTANK)
	{
		centity_t *tank = &cg_entities[cg_entities[cg.snap->ps.clientNum].tagParent];

		VectorCopy(tank->mountedMG42Player.origin, cg.refdef_current->vieworg);
		VectorCopy(ps->viewangles, cg.refdefViewAngles);
	}
	else if (!(ps->eFlags & 0x01000000))
	{
		VectorCopy(ps->origin, cg.refdef_current->vieworg);
		VectorCopy(ps->viewangles, cg.refdefViewAngles);
	}

	if (!cg.showGameView)
	{
		// add error decay
		if (cg_errorDecay.value > 0)
		{
			int   t;
			float f;

			t = cg.time - cg.predictedErrorTime;
			f = (cg_errorDecay.value - t) / cg_errorDecay.value;
			if (f > 0 && f < 1)
			{
				VectorMA(cg.refdef_current->vieworg, f, cg.predictedError, cg.refdef_current->vieworg);
			}
			else
			{
				cg.predictedErrorTime = 0;
			}
		}

		// Ridah, lock the viewangles if the game has told us to
		if (ps->viewlocked)
		{

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
			if (ps->viewlocked != 7 && ps->viewlocked != 3 && ps->viewlocked != 2)
			{
				BG_EvaluateTrajectory(&cg_entities[ps->viewlocked_entNum].currentState.apos, cg.time, cg.refdefViewAngles, qtrue, cg_entities[ps->viewlocked_entNum].currentState.effect2Time);
			}

			if (ps->viewlocked == 2)
			{
				cg.refdefViewAngles[0] += crandom();
				cg.refdefViewAngles[1] += crandom();
			}
		}

		if (cg.renderingThirdPerson)
		{
			// back away from character
			CG_OffsetThirdPersonView();
		}
		else
		{

			// offset for local bobbing and kicks
			CG_OffsetFirstPersonView();

			if (cg.editingSpeakers)
			{
				CG_SetViewanglesForSpeakerEditor();
			}
		}

		// Ridah, lock the viewangles if the game has told us to
		if (ps->viewlocked == 7)
		{
			centity_t *tent;
			vec3_t    vec;

			tent = &cg_entities[ps->viewlocked_entNum];
			VectorCopy(tent->lerpOrigin, vec);
			VectorSubtract(vec, cg.refdef_current->vieworg, vec);
			vectoangles(vec, cg.refdefViewAngles);
		}
		else if (ps->viewlocked == 4)
		{
			vec3_t fwd;
			AngleVectors(cg.refdefViewAngles, fwd, NULL, NULL);
			VectorMA(cg_entities[ps->viewlocked_entNum].lerpOrigin, 16, fwd, cg.refdef_current->vieworg);
		}
		else if (ps->viewlocked)
		{
			vec3_t fwd;
			float  oldZ;
			// set our position to be behind it
			oldZ = cg.refdef_current->vieworg[2];
			AngleVectors(cg.refdefViewAngles, fwd, NULL, NULL);
			if (cg.predictedPlayerState.eFlags & EF_AAGUN_ACTIVE)
			{
				VectorMA(cg_entities[ps->viewlocked_entNum].lerpOrigin, 0, fwd, cg.refdef_current->vieworg);
			}
			else
			{
				VectorMA(cg_entities[ps->viewlocked_entNum].lerpOrigin, -34, fwd, cg.refdef_current->vieworg);
			}
			cg.refdef_current->vieworg[2] = oldZ;
		}
		// done.
	}

	/* CQB keeps the first-person weapon frame before applying free head look.
	 * Shooting and movement continue to use the shared ps.viewangles. */
	if (!cg.renderingThirdPerson && !cg.showGameView && !ps->viewlocked &&
	    ps->pm_type == PM_NORMAL && ps->stats[STAT_HEALTH] > 0)
	{
		VectorCopy(cg.refdef_current->vieworg, cg.tceWeaponViewOrigin);
		VectorCopy(cg.refdefViewAngles, cg.tceWeaponViewAngles);
		AnglesToAxis(cg.tceWeaponViewAngles, cg.tceWeaponViewAxis);
		cg.tceWeaponViewValid = qtrue;
		if (ps->holdable[TCE_FREELOOK_PITCH] && ps->holdable[TCE_FREELOOK_YAW])
		{
			cg.refdefViewAngles[PITCH] += (ps->holdable[TCE_FREELOOK_PITCH] - TCE_FREELOOK_CENTER) * .01f;
			cg.refdefViewAngles[YAW]   += (ps->holdable[TCE_FREELOOK_YAW] - TCE_FREELOOK_CENTER) * .01f;
		}
	}

	// position eye reletive to origin
	AnglesToAxis(cg.refdefViewAngles, cg.refdef_current->viewaxis);

	if (cg.hyperspace)
	{
		cg.refdef.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE;
	}

	// field of view
	return CG_CalcFov();
}


//=========================================================================

char * CG_MustParse(char **pString, const char *pErrorMsg)
{
	char *token = COM_Parse(pString);
	if (!*token)
	{
		CG_Error(pErrorMsg);
	}
	return token;
}

void CG_ParseSkyBox(void)
{
	int    fogStart, fogEnd;
	char   *cstr, *token;
	vec4_t fogColor;

	cstr = (char *)CG_ConfigString(CS_SKYBOXORG);

	if (!*cstr)
	{
		cg.skyboxEnabled = qfalse;
		return;
	}

	token               = CG_MustParse(&cstr, "CG_ParseSkyBox: error parsing skybox configstring\n");
	cg.skyboxViewOrg[0] = atof(token);

	token               = CG_MustParse(&cstr, "CG_ParseSkyBox: error parsing skybox configstring\n");
	cg.skyboxViewOrg[1] = atof(token);

	token               = CG_MustParse(&cstr, "CG_ParseSkyBox: error parsing skybox configstring\n");
	cg.skyboxViewOrg[2] = atof(token);

	token            = CG_MustParse(&cstr, "CG_ParseSkyBox: error parsing skybox configstring\n");
	cg.skyboxViewFov = atoi(token);

	token             = CG_MustParse(&cstr, "CG_ParseSkyBox: error parsing skybox configstring\n");
	cg.tceSkyboxAngle = atoi(token);
	if (!cg.skyboxViewFov)
	{
		cg.skyboxViewFov = 25;
	}


	// setup fog the first time, ignore this part of the configstring after that
	token = CG_MustParse(&cstr, "CG_ParseSkyBox: error parsing skybox configstring.  No fog state\n");
	if (atoi(token))     // this camera has fog
	{
		token       = CG_MustParse(&cstr, "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog[0]\n");
		fogColor[0] = atof(token);

		token       = CG_MustParse(&cstr, "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog[1]\n");
		fogColor[1] = atof(token);

		token       = CG_MustParse(&cstr, "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog[2]\n");
		fogColor[2] = atof(token);

		token    = COM_ParseExt(&cstr, qfalse);
		fogStart = atoi(token);

		token  = COM_ParseExt(&cstr, qfalse);
		fogEnd = atoi(token);

		trap_R_SetFog(FOG_PORTALVIEW, fogStart, fogEnd, fogColor[0], fogColor[1], fogColor[2], 1.1f);
	}
	else
	{
		trap_R_SetFog(FOG_PORTALVIEW, 0, 0, 0, 0, 0, 0);    // init to null
	}

	cg.skyboxEnabled = qtrue;
}

/*
==============
CG_ParseTagConnects
==============
*/

void CG_ParseTagConnects(void)
{
	int i;

	for ( i = CS_TAGCONNECTS; i < CS_TAGCONNECTS + MAX_TAGCONNECTS; i++ )
	{
		CG_ParseTagConnect(i);
	}
}

void CG_ParseTagConnect(int tagNum)
{
	char *token, *pString = (char *)CG_ConfigString(tagNum);  // Gordon: bleh, i hate that cast away of the const
	int  entNum;

	if (!*pString)
	{
		return;
	}

	token = CG_MustParse(&pString, "Invalid TAGCONNECT configstring\n");

	entNum = atoi(token);
	if (entNum < 0 || entNum >= MAX_GENTITIES)
	{
		CG_Error("Invalid TAGCONNECT entitynum\n");
	}

	token = CG_MustParse(&pString, "Invalid TAGCONNECT configstring\n");

	cg_entities[entNum].tagParent = atoi(token);
	if (cg_entities[entNum].tagParent < 0 || cg_entities[entNum].tagParent >= MAX_GENTITIES)
	{
		CG_Error("Invalid TAGCONNECT tagparent\n");
	}

	token = CG_MustParse(&pString, "Invalid TAGCONNECT configstring\n");
	Q_strncpyz(cg_entities[entNum].tagName, token, MAX_QPATH);
}

/*
==============
CG_DrawSkyBoxPortal
==============
*/
/* Windows3006b2a0. A clean portal refdef avoids inheriting scene fog,
 * areamasks and unrelated flags; camera motion is scaled into the sky model. */
void CG_DrawSkyBoxPortal(qboolean fLocalView)
{
	refdef_t rd;
#if defined(__linux__)
	long double inverseScale;
#else
	double   inverseScale;
#endif
	if (!cg_skybox.integer || !cg.skyboxEnabled || !fLocalView)
	{
		return;
	}
	memset(&rd, 0, sizeof(rd));
	rd.rdflags = RDF_SKYBOXPORTAL;
	AxisCopy(cg.refdef_current->viewaxis, rd.viewaxis);
#if defined(__linux__)
	/* Keep the reciprocal wide until the final view-origin stores. */
	inverseScale = 1.0L / (long double)cg.skyboxViewFov;
#else
	inverseScale = 1.0 / cg.skyboxViewFov;
#endif
	VectorMA(cg.skyboxViewOrg, inverseScale, cg.refdef_current->vieworg, rd.vieworg);
	rd.x     = cg.refdef_current->x; rd.y = cg.refdef_current->y;
	rd.width = cg.refdef_current->width; rd.height = cg.refdef_current->height;
	rd.fov_x = cg.refdef_current->fov_x; rd.fov_y = cg.refdef_current->fov_y;
	rd.time  = cg.refdef_current->time;
	trap_R_RenderScene(&rd);
}

//=========================================================================

/*
**  Frustum code
*/

// some culling bits
typedef struct plane_s
{
	vec3_t normal;
	float dist;
} plane_t;

static plane_t frustum[4];

//
//	CG_SetupFrustum
//
void CG_SetupFrustum(void)
{
#if defined(__linux__)
	int i;
	float xs, xc;
	float ang;

	ang = cg.refdef_current->fov_x / 180 * M_PI * 0.5f;
	xs = sin(ang);
	xc = cos(ang);
	VectorScale(cg.refdef_current->viewaxis[0], xs, frustum[0].normal);
	VectorMA(frustum[0].normal, xc, cg.refdef_current->viewaxis[1], frustum[0].normal);
	VectorScale(cg.refdef_current->viewaxis[0], xs, frustum[1].normal);
	VectorMA(frustum[1].normal, -xc, cg.refdef_current->viewaxis[1], frustum[1].normal);

	ang = cg.refdef.fov_y / 180 * M_PI * 0.5f;
	xs = sin(ang);
	xc = cos(ang);
	VectorScale(cg.refdef_current->viewaxis[0], xs, frustum[2].normal);
	VectorMA(frustum[2].normal, xc, cg.refdef_current->viewaxis[2], frustum[2].normal);
	VectorScale(cg.refdef_current->viewaxis[0], xs, frustum[3].normal);
	VectorMA(frustum[3].normal, -xc, cg.refdef_current->viewaxis[2], frustum[3].normal);
	for (i = 0; i < 4; i++)
	{
		frustum[i].dist = DotProduct(cg.refdef_current->vieworg, frustum[i].normal);
	}
#else
	int    plane, axis;
	float  sine;
	double angle, cosine;
	/* Windows3006b3a0: rounded sine, unspilled cosine, one final
	 * normal-component store. Vertical FOV is the main refdef's FOV. */
	for (plane = 0; plane < 4; plane += 2)
	{
		angle = (double)(plane ? cg.refdef.fov_y : cg.refdef_current->fov_x)
		        * (double)0.008726646192371845245361328125f;
		sine   = (float)sin(angle);
		cosine = cos(angle);
		for (axis = 0; axis < 3; ++axis)
		{
			float  forward = sine * cg.refdef_current->viewaxis[0][axis];
			double side    = cosine * cg.refdef_current->viewaxis[plane ? 2 : 1][axis];
			frustum[plane].normal[axis]     = (float)(side + forward);
			frustum[plane + 1].normal[axis] = (float)(-side + forward);
		}
	}
	for (plane = 0; plane < 4; ++plane)
	{
		frustum[plane].dist = (float)(
			(double)frustum[plane].normal[1] * cg.refdef_current->vieworg[1] +
			(double)frustum[plane].normal[2] * cg.refdef_current->vieworg[2] +
			(double)frustum[plane].normal[0] * cg.refdef_current->vieworg[0]);
	}
#endif
}


//
//	CG_CullPoint - returns true if culled
//
/* Original C0 gates cull both less-than and unordered distances.
 * Explicit platform stages preserve operation order; exact x87/compiler
 * precision and exception parity remain unverified. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__linux__)
qboolean CG_CullPoint(vec3_t pt)
{
	int         i;
	long double distance;
#if defined(__linux__)
	long double x = (long double)pt[0];
	long double y = (long double)pt[1];
	long double z = (long double)pt[2];
#endif
	for (i = 0; i < 4; ++i)
	{
		const plane_t *plane = &frustum[i];
#if defined(__linux__)
		distance = y * (long double)plane->normal[1];
		distance = distance + x * (long double)plane->normal[0];
		distance = distance + z * (long double)plane->normal[2];
#else
		distance = (long double)plane->normal[0] * (long double)pt[0];
		distance = distance + (long double)plane->normal[2] * (long double)pt[2];
		distance = distance + (long double)pt[1] * (long double)plane->normal[1];
#endif
		distance = distance - (long double)plane->dist;
		if (!(distance >= 0.0L))
		{
			return qtrue;
		}
	}
	return qfalse;
}

qboolean CG_CullPointAndRadius(const vec3_t pt, vec_t radius)
{
	int         i;
	long double distance, negativeRadius;
#if defined(__linux__)
	long double x = (long double)pt[0];
	long double y = (long double)pt[1];
	long double z = (long double)pt[2];
	negativeRadius = -(long double)radius;
#else
	/* Windows FCHS stores/reloads Float; it does not modify the caller's pt. */
	volatile float storedRadius = (float)-(long double)radius;
	negativeRadius = (long double)storedRadius;
#endif
	for (i = 0; i < 4; ++i)
	{
		const plane_t *plane = &frustum[i];
#if defined(__linux__)
		distance = y * (long double)plane->normal[1];
		distance = distance + x * (long double)plane->normal[0];
		distance = distance + z * (long double)plane->normal[2];
#else
		distance = (long double)plane->normal[0] * (long double)pt[0];
		distance = distance + (long double)plane->normal[2] * (long double)pt[2];
		distance = distance + (long double)plane->normal[1] * (long double)pt[1];
#endif
		distance = distance - (long double)plane->dist;
		if (!(distance >= negativeRadius))
		{
			return qtrue;
		}
	}
	return qfalse;
}
#else
qboolean CG_CullPoint(vec3_t pt)
{
	int     i;
	plane_t *frust;

	// check against frustum planes
	for (i = 0 ; i < 4 ; i++)
	{
		frust = &frustum[i];

		if ((DotProduct(pt, frust->normal) - frust->dist) < 0)
		{
			return(qtrue);
		}
	}

	return(qfalse);
}

qboolean CG_CullPointAndRadius(const vec3_t pt, vec_t radius)
{
	int     i;
	plane_t *frust;

	// check against frustum planes
	for (i = 0 ; i < 4 ; i++)
	{
		frust = &frustum[i];

		if ((DotProduct(pt, frust->normal) - frust->dist) < -radius)
		{
			return(qtrue);
		}
	}

	return(qfalse);
}

#endif

//=========================================================================

extern void CG_SetupDlightstyles(void);


//#define DEBUGTIME_ENABLED
#ifdef DEBUGTIME_ENABLED
#define DEBUGTIME elapsed = (trap_Milliseconds() - dbgTime); if (dbgCnt++ == 1) { CG_Printf("t%i:%i ", dbgCnt, elapsed = (trap_Milliseconds() - dbgTime)); } dbgTime += elapsed;
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

qboolean CG_CalcMuzzlePoint(int entityNum, vec3_t muzzle);

void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback)
{
	int inwater;

#ifdef DEBUGTIME_ENABLED
	int dbgTime = trap_Milliseconds(), elapsed;
	int dbgCnt = 0;
#endif

	cg.time         = serverTime;
	cgDC.realTime   = cg.time;
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
	if (cg.infoScreenText[0] != 0)
	{
		CG_DrawInformation(qfalse);
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
	if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE))
	{
		CG_DrawInformation(qfalse);
		return;
	}

	// check for server set weapons we might not know about
	// (FIXME: this is a hack for the time being since a scripted "selectweapon" does
	// not hit the first snap, the server weapon set in cg_playerstate.c line 219 doesn't
	// do the trick)
	if (!cg.weaponSelect && cg.snap->ps.weapon)
	{
		cg.weaponSelect     = cg.snap->ps.weapon;
		cg.weaponSelectTime = cg.time;
	}

	/* TC CG_DrawActiveFrame 3006b610: only legacy scope slot59. */
	if (cg.weaponSelect == 59)
	{
		float spd;
		spd = VectorLength(cg.snap->ps.velocity);
		if (spd > 180.0f)
		{
			CG_FinishWeaponChange(59, 33);
		}
	}

	DEBUGTIME

	if (!cg.lightstylesInited)
	{
		CG_SetupDlightstyles();
	}

	DEBUGTIME

	// if we have been told not to render, don't
	if (cg_norender.integer)
	{
		return;
	}

	// this counter will be bumped for every valid scene we generate
#if defined(__linux__)
	cg.clientFrame = TCE_AddInt32(cg.clientFrame, 1);
#else
	cg.clientFrame++;
#endif

	// update cg.predictedPlayerState
	CG_PredictPlayerState();

	DEBUGTIME


	// OSP -- MV handling
	if (cg.mvCurrentMainview != NULL && cg.snap->ps.pm_type != PM_INTERMISSION)
	{
		CG_mvDraw(cg.mvCurrentMainview);
		// FIXME: not valid for demo playback
#if defined(__linux__)
		cg.zoomSensitivity = (float)((1.0L / int_sensitivity.value) * mv_sensitivity.value);
#else
		cg.zoomSensitivity = mv_sensitivity.value / int_sensitivity.value;
#endif
	}
	else
	{
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

		if (inwater)
		{
			CG_UnderwaterSounds();
		}

		DEBUGTIME

		// first person blend blobs, done after AnglesToAxis
		if (!cg.renderingThirdPerson)
		{
			CG_DamageBlendBlob();
		}

		DEBUGTIME

		// build the render lists
		if (!cg.hyperspace)
		{
			CG_AddPacketEntities();         // adter calcViewValues, so predicted player state is correct
			CG_AddMarks();

			DEBUGTIME

			CG_AddScriptSpeakers();

			DEBUGTIME

			// Rafael particles
			CG_AddParticles();
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
		if (!cg.showGameView && !cgs.dbShowing)
		{
			if (!cg.snap->ps.persistant[PERS_HWEAPON_USE])
			{
				CG_AddViewWeapon(&cg.predictedPlayerState);
			}
			else
			{
#if defined(__linux__)
				if (TCE_SubInt32(cg.time, cg.predictedPlayerEntity.overheatTime) < 3000)
#else
				if (cg.time - cg.predictedPlayerEntity.overheatTime < 3000)
#endif
				{
					vec3_t muzzle;

					CG_CalcMuzzlePoint(cg.snap->ps.clientNum, muzzle);

					muzzle[2] -= 32;

					if (!(rand() % 3))
					{
						float alpha;
#if defined(__linux__)
						alpha = (float)((1.0L - (long double)TCE_SubInt32(cg.time, cg.predictedPlayerEntity.overheatTime) *
						                 (1.0f / 3000.0f)) * .25f);
#else
						alpha  = 1.0f - ((float)(cg.time - cg.predictedPlayerEntity.overheatTime) / 3000.0f);
						alpha *= 0.25f;     // .25 max alpha
#endif
						CG_ParticleImpactSmokePuffExtended(cgs.media.smokeParticleShader, muzzle, 1000, 8, 20, 30, alpha, 8.f);
					}
				}
			}
		}

		// NERVE - SMF - play buffered voice chats
		CG_PlayBufferedVoiceChats();

		DEBUGTIME
		// Ridah, trails
		if (!cg.hyperspace)
		{
			CG_AddFlameChunks();
			CG_AddTrails();         // this must come last, so the trails dropped this frame get drawn
		}
		// done.

		DEBUGTIME

		// finish up the rest of the refdef
		if (cg.testModelEntity.hModel)
		{
			CG_AddTestModel();
		}
		cg.refdef.time = cg.time;
		memcpy(cg.refdef.areamask, cg.snap->areamask, sizeof(cg.refdef.areamask));

		DEBUGTIME

		// warning sounds when powerup is wearing off
		//CG_PowerupTimerSounds();

		// make sure the lagometerSample and frame timing isn't done twice when in stereo
		if (stereoView != STEREO_RIGHT)
		{
#if defined(__linux__)
			cg.frametime = TCE_SubInt32(cg.time, cg.oldTime);
#else
			cg.frametime = cg.time - cg.oldTime;
#endif
			if (cg.frametime < 0)
			{
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
		trap_SetClientLerpOrigin(cg.refdef.vieworg[0], cg.refdef.vieworg[1], cg.refdef.vieworg[2]);

		// actually issue the rendering calls
		CG_DrawActive(stereoView);

		DEBUGTIME

		// update audio positions
		trap_S_Respatialize(cg.snap->ps.clientNum, cg.refdef.vieworg, cg.refdef.viewaxis, inwater);
	}

	if (cg_stats.integer)
	{
		CG_Printf("cg.clientFrame:%i\n", cg.clientFrame);
	}

	DEBUGTIME

	// let the client system know what our weapon, holdable item and zoom settings are
	trap_SetUserCmdValue(cg.weaponSelect, cg.showGameView ? 0x01 : 0x00, cg.zoomSensitivity, cg.identifyClientRequest);
}
