/* Windows cgame:3006f350, 3006f400, 3006f440. */
#include "cg_local.h"
#include "tce_weapon_frames.h"
#include "tce_weapon_media.h"

typedef char tce_frame_animation_offset[offsetof(lerpFrame_t, animation)==0x34?1:-1];
typedef char tce_frame_time_offset[offsetof(lerpFrame_t, animationTime)==0x38?1:-1];
typedef char tce_animation_initial_lerp[offsetof(animation_t, initialLerp)==0x54?1:-1];

/* Windows 30072fe0 / 30072fb0. These fields have SDK equivalents; unlike
 * weapon selection, these transitions do not translate any TC:E weapon IDs. */
void CG_StartWeaponAnim(int anim) {
    if (cg.predictedPlayerState.pm_type >= PM_DEAD ||
        cg.pmext.weapAnimTimer > 0 || cg.predictedPlayerState.weapon == WP_NONE)
        return;
    cg.predictedPlayerState.weapAnim =
        ((~cg.predictedPlayerState.weapAnim) & ANIM_TOGGLEBIT) | anim;
}

void CG_ContinueWeaponAnim(int anim) {
    if (cg.predictedPlayerState.weapon == WP_NONE ||
        (cg.predictedPlayerState.weapAnim & ~ANIM_TOGGLEBIT) == anim ||
        cg.pmext.weapAnimTimer > 0)
        return;
    CG_StartWeaponAnim(anim);
}

/* CG_WeaponAnimation:30073020. The original clientInfo argument passed to
 * Run is unused. Project only ps.weapAnim and the raise-selection context. */
void TCE_CG_WeaponAnimation(int number, animation_t *animations, int *oldFrame,
    int *frame, float *backlerp, int raiseAnimation) {
    lerpFrame_t *lf = &cg.predictedPlayerEntity.pe.weap;
    if (cg_noPlayerAnims.integer) {
        *frame = 0;
        *oldFrame = 0;
        return; /* Original deliberately leaves backlerp and frame state alone. */
    }
    TCE_CG_RunWeapLerpFrame(animations, lf, number, 1.0f, raiseAnimation);
    *oldFrame = lf->oldFrame;
    *frame = lf->frame;
    *backlerp = lf->backlerp;
    if (cg_debugAnim.integer == 3)
        CG_Printf("oldframe: %d   frame: %d   backlerp: %f\n",
            lf->oldFrame, lf->frame, lf->backlerp);
}

void TCE_CG_SetWeapLerpFrameAnimation(animation_t *animations, lerpFrame_t *lf, int number) {
    lf->animationNumber = number;
    number &= ~ANIM_TOGGLEBIT;
    if ((unsigned)number >= 13) {
        CG_Error("Bad animation number (CG_SWLFA): %i", number);
        return; /* Engine errors do not return; avoid unsafe reads if an adapter does. */
    }
    lf->animation = &animations[number];
    lf->animationTime = lf->frameTime + lf->animation->initialLerp;
    if (cg_debugAnim.integer & 2)
        CG_Printf("Weap Anim: %d\n", number);
}

void TCE_CG_ClearWeapLerpFrame(animation_t *animations, lerpFrame_t *lf, int number) {
    lf->frameTime = lf->oldFrameTime = cg.time;
    TCE_CG_SetWeapLerpFrameAnimation(animations, lf, number);
    if ((unsigned)(number & ~ANIM_TOGGLEBIT) >= 13) return;
    lf->oldFrame = lf->frame = lf->animation->firstFrame;
    lf->oldFrameModel = lf->frameModel = lf->animation->mdxFile;
}

qboolean TCE_CG_GetPartFramesFromWeap(const lerpFrame_t *lf, refEntity_t *part,
    const refEntity_t *parent, int partId, const animation_t *animations) {
    const animation_t *anim = lf->animation;
    unsigned bit, hideBit;
    int i, offset = 0;
    if (partId == 7) return qtrue;
    /* x86 masks shift counts. Unsigned shifts also avoid signed bit31 UB. */
    bit = 1u << ((unsigned)partId & 31);
    hideBit = 1u << (((unsigned)partId + 8) & 31);
    if ((unsigned)anim->moveSpeed & hideBit) return qfalse;
    for (i = 0; i < (lf->animationNumber & ~ANIM_TOGGLEBIT); ++i)
        if ((unsigned)animations[i].moveSpeed & bit) offset += animations[i].numFrames;
    if ((unsigned)anim->moveSpeed & bit) {
        part->backlerp = parent->backlerp;
        part->oldframe = parent->oldframe - anim->firstFrame + offset;
        part->frame = parent->frame - anim->firstFrame + offset;
    }
    return qtrue;
}

qboolean CG_GetPartFramesFromWeap(centity_t *cent, refEntity_t *part,
    refEntity_t *parent, int partId, weaponInfo_t *wi) {
    return TCE_CG_GetPartFramesFromWeap(&cent->pe.weap, part, parent, partId, wi->weapAnimations);
}
void CG_ClearWeapLerpFrame(weaponInfo_t *wi, lerpFrame_t *lf, int number) {
    TCE_CG_ClearWeapLerpFrame(wi->weapAnimations, lf, number);
}

/* CG_RunWeapLerpFrame:300730d0. raiseAnimation explicitly projects the
 * PM_RaiseAnimForWeapon result; the clientInfo argument is unused originally.
 * Use double intermediates to preserve original x87 truncation/interpolation.
 */
void TCE_CG_RunWeapLerpFrame( animation_t *animations, lerpFrame_t *lf, int newAnimation, float speedScale, int raiseAnimation ) {
	int			f;
	animation_t	*anim;

	if ( cg_animSpeed.integer == 0 ) {
		lf->oldFrame = lf->frame = lf->backlerp = 0;
		return;
	}

	if(!lf->animation ) {
		TCE_CG_ClearWeapLerpFrame(animations, lf, newAnimation );
	}
	else if ( newAnimation != lf->animationNumber ) {
		if((newAnimation&~ANIM_TOGGLEBIT) == raiseAnimation) {
			TCE_CG_ClearWeapLerpFrame(animations, lf, newAnimation );
		} else {
			TCE_CG_SetWeapLerpFrameAnimation( animations, lf, newAnimation );
		}
	}

	if ((unsigned)(newAnimation & ~ANIM_TOGGLEBIT) >= 13) return;

	if ( cg.time >= lf->frameTime ) {
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;
		lf->oldFrameModel = lf->frameModel;

		anim = lf->animation;
		if ( !anim->frameLerp ) {
			return;
		}
		if ( cg.time < lf->animationTime ) {
			lf->frameTime = lf->animationTime;
		} else {
			lf->frameTime = lf->oldFrameTime + anim->frameLerp;
		}
		f = ( lf->frameTime - lf->animationTime ) / anim->frameLerp;
		f = (int)((double)f * (double)speedScale);
		if ( f >= anim->numFrames ) {
			f -= anim->numFrames;
			if ( anim->loopFrames ) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = anim->numFrames - 1;

				lf->frameTime = cg.time;
			}
		}
		lf->frame = anim->firstFrame + f;
		lf->frameModel = anim->mdxFile;
		if ( cg.time > lf->frameTime ) {
			lf->frameTime = cg.time;
			if ( cg_debugAnim.integer ) {
				CG_Printf( "Clamp lf->frameTime\n");
			}
		}
	}

	if ( lf->frameTime > cg.time + 200 ) {
		lf->frameTime = cg.time;
	}

	if ( lf->oldFrameTime > cg.time ) {
		lf->oldFrameTime = cg.time;
	}

	if ( lf->frameTime == lf->oldFrameTime ) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = (float)(1.0 - (double)( cg.time - lf->oldFrameTime ) / ( lf->frameTime - lf->oldFrameTime ));
	}
}
