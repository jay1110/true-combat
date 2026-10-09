/*
 * name:		cg_players.c
 *
 * desc:		handle the media and animation for player entities
 *
*/

#include "cg_local.h"
#include "../game/bg_classes.h"
#include "../game/tce_trajectory.h"
#include "tce_smoke_grenade.h"
#include "tce_weapon_media.h"

#define	SWING_RIGHT	1
#define SWING_LEFT	2

#define JUMP_HEIGHT				56
#define SWINGSPEED				0.3

static int			dp_realtime;
static float		jumpHeight;

animation_t		*lastTorsoAnim;
animation_t		*lastLegsAnim;

extern const char* cg_skillRewards[SK_NUM_SKILLS][NUM_SKILL_LEVELS-1];

/*
================
CG_EntOnFire
================
*/

/*
================
CG_IsCrouchingAnim
================
*/
qboolean CG_IsCrouchingAnim( animModelInfo_t* animModelInfo, int animNum ) {
	animation_t *anim;

	// FIXME: make compatible with new scripting
	animNum &= ~ANIM_TOGGLEBIT;
	//
	anim = BG_GetAnimationForIndex( animModelInfo, animNum );
	//
	if( anim->movetype & ((1<<ANIM_MT_IDLECR)|(1<<ANIM_MT_WALKCR)|(1<<ANIM_MT_WALKCRBK)) )
		return qtrue;
	//
	return qfalse;
}

/*
================
CG_CustomSound
================
*/
sfxHandle_t	CG_CustomSound( int clientNum, const char *soundName ) {
	if ( soundName[0] != '*' ) {
		return trap_S_RegisterSound( soundName, qfalse );
	}

	return 0;
}

/*
=============================================================================

CLIENT INFO

=============================================================================
*/

/*
===================
CG_LoadClientInfo

Load it now, taking the disk hits.
This will usually be deferred to a safe time
===================
*/
static void CG_LoadClientInfo( int clientNum ) {
	int			i;

	// reset any existing players and bodies, because they might be in bad
	// frames for this new model
	for( i = 0 ; i < MAX_GENTITIES ; i++ ) {
		if ( cg_entities[i].currentState.clientNum == clientNum && cg_entities[i].currentState.eType == ET_PLAYER ) {
			CG_ResetPlayerEntity( &cg_entities[i] );
		}
	}
}

void CG_ParseTeamXPs( int n ) {
	int i, j;
	char* cs = (char*)CG_ConfigString( CS_AXIS_MAPS_XP + n );
	const char* token;

	for( i = 0; i < MAX_MAPS_PER_CAMPAIGN; i++ ) {
		for( j = 0; j < SK_NUM_SKILLS; j++ ) {
			token = COM_ParseExt( &cs, qfalse );

			if( !token || !*token ) {
				return;
			}

			if( n == 0 ) {
				cgs.tdbAxisMapsXP[ j ][ i ] = atoi( token );
			} else {
				cgs.tdbAlliedMapsXP[ j ][ i ] = atoi( token );
			}
		}
	}
}

void CG_LimboPanel_SendSetupMsg( qboolean forceteam );

/* TC name pre-pass (Windows3007f290 / Linux Q_EliteCleanStr).
 * Keep this module-local until the shared string API is migrated. */
static char *CG_EliteCleanStr( char *text ) {
	char *read = text;
	char *write = text;

	while( *read ) {
		if( read[0] == '^' && read[1] == '^' ) {
			read += 2;
			continue;
		}
		if( (unsigned char)*read >= 32 && (unsigned char)*read < 127 ) {
			*write++ = *read;
		}
		read++;
	}
	*write = '\0';
	return text;
}

/*
======================
CG_NewClientInfo
======================
*/
void CG_NewClientInfo( int clientNum ) {
	clientInfo_t *ci;
	clientInfo_t newInfo;
	const char	*configstring;
	const char	*v;
	int	oldclass;

	ci = &cgs.clientinfo[clientNum];

	configstring = CG_ConfigString( clientNum + CS_PLAYERS );
	if ( !*configstring ) {
		memset( ci, 0, sizeof( *ci ) );
		return;		// player just left
	}

	// build into a temp buffer so the defer checks can use
	// the old value
	memset( &newInfo, 0, sizeof( newInfo ) );

	// Gordon: grabbing some older stuff, if it's a new client, tinfo will update within one second anyway, otherwise you get the health thing flashing red
	// NOTE: why are we bothering to do all this setting up of a new clientInfo_t anyway? it was all for deffered clients iirc, which we dont have
	newInfo.location[0] =	ci->location[0];
	newInfo.location[1] =	ci->location[1];
	newInfo.health =		ci->health;
	newInfo.fireteamData =	ci->fireteamData;
	newInfo.clientNum =		clientNum;
	newInfo.selected =		ci->selected;
	newInfo.totalWeapAcc =	ci->totalWeapAcc;

	// isolate the player's name
	v = Info_ValueForKey(configstring, "n");
	Q_strncpyz( newInfo.name, v, sizeof( newInfo.name ) );
	CG_EliteCleanStr( newInfo.name );
	Q_CleanStr( newInfo.name );
	Q_strncpyz( newInfo.cleanname, v, sizeof( newInfo.cleanname ) );
	CG_EliteCleanStr( newInfo.cleanname );
	Q_CleanStr( newInfo.cleanname );
	

	// bot skill
	v = Info_ValueForKey( configstring, "skill" );
	newInfo.botSkill = atoi( v );

	// team
	v = Info_ValueForKey( configstring, "t" );
	newInfo.team = atoi( v );

	// class
	v = Info_ValueForKey( configstring, "c" );
	newInfo.cls = atoi( v );

	// rank
	v = Info_ValueForKey( configstring, "r" );
	newInfo.rank = atoi( v );

	v = Info_ValueForKey( configstring, "f" );
	newInfo.fireteam = atoi( v );

	v = Info_ValueForKey( configstring, "m" );
	if( *v ) {
		int i;
		char buf[2];
		buf[1] = '\0';
		for( i = 0; i < SK_NUM_SKILLS; i++ ) {
			buf[0] = *v;
			newInfo.medals[i] = atoi( buf );
			v++;
		}
	}

	v = Info_ValueForKey( configstring, "ch" );
	if( *v ) {
		newInfo.character = cgs.gameCharacters[atoi(v)];
	}

	v = Info_ValueForKey( configstring, "s");
	if( *v ) {
		int i;

		for( i = 0; i < SK_NUM_SKILLS; i++ ) {
			char skill[2];

			skill[0] = v[i];
			skill[1] = '\0';

			newInfo.skill[i] = atoi(skill);
		}
	}

	// diguiseName
	v = Info_ValueForKey( configstring, "dn" );
	Q_strncpyz( newInfo.disguiseName, v, sizeof( newInfo.disguiseName ) );

	// disguiseRank
	v = Info_ValueForKey( configstring, "dr" );
	newInfo.disguiseRank = atoi( v );

	// Gordon: weapon and latchedweapon ( FIXME: make these more secure )
	v = Info_ValueForKey( configstring, "w" );
	newInfo.weapon = atoi( v );

	v = Info_ValueForKey( configstring, "lw" );
	newInfo.latchedweapon = atoi( v );

	v = Info_ValueForKey( configstring, "sw" );
	newInfo.secondaryweapon = atoi( v );

	v = Info_ValueForKey( configstring, "ref" );
	newInfo.refStatus = atoi( v );

	// Gordon: detect rank/skill changes client side
	if( clientNum == cg.clientNum ) {
		int i;

		if( newInfo.team != cgs.clientinfo[ cg.clientNum ].team ) {
			if( cgs.autoFireteamCreateEndTime != cg.time + 20000) { 
				cgs.autoFireteamCreateEndTime = 0;
			}
			if( cgs.autoFireteamJoinEndTime != cg.time + 20000) { 
				cgs.autoFireteamJoinEndTime = 0;
			}
		}

		if( newInfo.rank > cgs.clientinfo[ cg.clientNum ].rank ) {

			CG_SoundPlaySoundScript( cgs.clientinfo[cg.clientNum].team == TEAM_ALLIES ? rankSoundNames_Allies[ newInfo.rank ] : rankSoundNames_Axis[ newInfo.rank ], NULL, -1, qtrue );

			CG_AddPMItemBig( PM_RANK, va("Promoted to rank %s!", cgs.clientinfo[ cg.clientNum ].team == TEAM_AXIS ? rankNames_Axis[newInfo.rank] : rankNames_Allies[newInfo.rank] ), rankicons[ newInfo.rank ][ 0 ].shader );
		}

		for( i = 0; i < SK_NUM_SKILLS; i++ ) {
			if( newInfo.skill[i] > cgs.clientinfo[ cg.clientNum ].skill[i] ) {
				// Gordon: slick hack so that funcs we call use teh new value now
				cgs.clientinfo[ cg.clientNum ].skill[ i ] = newInfo.skill[ i ];

				if( newInfo.skill[i] == 4 && i == SK_HEAVY_WEAPONS ) {
					if( cgs.clientinfo[ cg.clientNum ].skill[SK_LIGHT_WEAPONS] == 4 ) {
						oldclass = cgs.ccSelectedClass;
						cgs.ccSelectedClass = newInfo.cls;
						CG_LimboPanel_SetSelectedWeaponNumForSlot( 1, 2 );
						CG_LimboPanel_SendSetupMsg( qfalse );
						cgs.ccSelectedClass = oldclass;
					} else {
						oldclass = cgs.ccSelectedClass;
						cgs.ccSelectedClass = newInfo.cls;
						CG_LimboPanel_SetSelectedWeaponNumForSlot( 1, 1 );
						CG_LimboPanel_SendSetupMsg( qfalse );
						cgs.ccSelectedClass = oldclass;
					}
				}

				if( newInfo.skill[i] == 4 && i == SK_LIGHT_WEAPONS ) {
					if( cgs.clientinfo[ cg.clientNum ].skill[SK_HEAVY_WEAPONS] == 4 ) {
						if( cgs.ccSelectedWeapon2 == 2 ) {
							oldclass = cgs.ccSelectedClass;
							cgs.ccSelectedClass = newInfo.cls;
							CG_LimboPanel_SetSelectedWeaponNumForSlot( 1, 3 );
							CG_LimboPanel_SendSetupMsg( qfalse );
							cgs.ccSelectedClass = oldclass;
						}
					} else {
						oldclass = cgs.ccSelectedClass;
						cgs.ccSelectedClass = newInfo.cls;
						CG_LimboPanel_SetSelectedWeaponNumForSlot( 1, 1 );
						CG_LimboPanel_SendSetupMsg( qfalse );
						cgs.ccSelectedClass = oldclass;
					}					
				}

				CG_AddPMItemBig( PM_SKILL, va("Increased %s skill to level %i!", skillNames[i], newInfo.skill[i] ), cgs.media.skillPics[ i ] );

				CG_PriorityCenterPrint( va( "You have been rewarded with %s", cg_skillRewards[ i ][ newInfo.skill[i]-1 ]), SCREEN_HEIGHT - (SCREEN_HEIGHT * 0.20), SMALLCHAR_WIDTH, 99999 );
			}
		}

		if( newInfo.team != cgs.clientinfo[ cg.clientNum ].team ) {
			// clear these
			memset( cg.artilleryRequestPos, 0, sizeof(cg.artilleryRequestPos) );
			memset( cg.artilleryRequestTime, 0, sizeof(cg.artilleryRequestTime) );
		}

		trap_Cvar_Set( "authLevel", va( "%i", newInfo.refStatus ) );

		if( newInfo.refStatus != ci->refStatus ) {
			if( newInfo.refStatus <= RL_NONE ) {
				const char *info = CG_ConfigString( CS_SERVERINFO );

				trap_Cvar_Set( "cg_ui_voteFlags", Info_ValueForKey(info, "voteFlags"));
				CG_Printf("[cgnotify]^3*** You have been stripped of your referee status! ***\n");

			} else {
				trap_Cvar_Set( "cg_ui_voteFlags", "0");
				CG_Printf("[cgnotify]^2*** You have been authorized \"%s\" status ***\n", ((newInfo.refStatus == RL_RCON) ? "rcon" : "referee"));
				CG_Printf("Type: ^3ref^7 (by itself) for a list of referee commands.\n");
			}
		}
	}


	// rain - passing the clientNum since that's all we need, and we
	// can't calculate it properly from the clientinfo
	CG_LoadClientInfo( clientNum );

	// replace whatever was there with the new one
	newInfo.infoValid = qtrue;
	*ci = newInfo;

	// make sure we have a character set
	if( !ci->character ) {
		ci->character = BG_GetCharacter( ci->team, ci->cls );
	}

	// Gordon: need to resort the fireteam list, incase ranks etc have changed
	CG_SortClientFireteam();
}

/*
=============================================================================

PLAYER ANIMATION

=============================================================================
*/

bg_playerclass_t* CG_PlayerClassForClientinfo( clientInfo_t *ci, centity_t* cent) {
	int team, cls;

	if( cent && cent->currentState.eType == ET_CORPSE ) {
		return BG_GetPlayerClassInfo( cent->currentState.modelindex, cent->currentState.modelindex2 );
	}

	if( cent && cent->currentState.powerups & (1 << PW_OPS_DISGUISED) ) {
		team = ci->team == TEAM_AXIS ? TEAM_ALLIES : TEAM_AXIS;

		// rain - fixed incorrect class determination (was & 6,
		// should be & 7)
		cls = (cent->currentState.powerups >> PW_OPS_CLASS_1) & 7;

		return BG_GetPlayerClassInfo( team, cls );
	}

	return BG_GetPlayerClassInfo( ci->team, ci->cls );
}

/*
===============
CG_SetLerpFrameAnimation

may include ANIM_TOGGLEBIT
===============
*/
static void CG_SetLerpFrameAnimation( centity_t *cent, clientInfo_t *ci, lerpFrame_t *lf, int newAnimation ) {
	animation_t	*anim;

	bg_character_t* character = CG_CharacterForClientinfo( ci, cent );

	if( !character ) {
		return;
	}

	lf->animationNumber = newAnimation;
	newAnimation &= ~ANIM_TOGGLEBIT;

	if( newAnimation < 0 || newAnimation >= character->animModelInfo->numAnimations ) {
		CG_Error( "CG_SetLerpFrameAnimation: Bad animation number: %i", newAnimation );
	}

	anim = character->animModelInfo->animations[ newAnimation ];

	lf->animation = anim;
	lf->animationTime = lf->frameTime + anim->initialLerp;

	if( cg_debugAnim.integer == 1 ) {
		CG_Printf( "Anim: %i, %s\n", newAnimation, character->animModelInfo->animations[newAnimation]->name );
	}
}

/*
===============
CG_RunLerpFrame

Sets cg.snap, cg.oldFrame, and cg.backlerp
cg.time should be between oldFrameTime and frameTime after exit
===============
*/
void CG_RunLerpFrame( centity_t *cent, clientInfo_t *ci, lerpFrame_t *lf, int newAnimation, float speedScale ) {
	int			f;
	animation_t	*anim;

	// debugging tool to get no animations
	if( cg_animSpeed.integer == 0 ) {
		lf->oldFrame = lf->frame = lf->backlerp = 0;
		return;
	}

	// see if the animation sequence is switching
	if( ci && (newAnimation != lf->animationNumber || !lf->animation) ) {
		CG_SetLerpFrameAnimation( cent, ci, lf, newAnimation );
	}

	// if we have passed the current frame, move it to
	// oldFrame and calculate a new frame
	if( cg.time >= lf->frameTime ) {
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;
		lf->oldFrameModel = lf->frameModel;

		// get the next frame based on the animation
		anim = lf->animation;
		if( !anim->frameLerp ) {
			return;		// shouldn't happen
		}
		if( cg.time < lf->animationTime ) {
			lf->frameTime = lf->animationTime;		// initial lerp
		} else {
			lf->frameTime = lf->oldFrameTime + anim->frameLerp;
		}
		f = ( lf->frameTime - lf->animationTime ) / anim->frameLerp;
		f *= speedScale;		// adjust for haste, etc
		if( f >= anim->numFrames ) {
			f -= anim->numFrames;
			if ( anim->loopFrames ) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = anim->numFrames - 1;
				// the animation is stuck at the end, so it
				// can immediately transition to another sequence
				lf->frameTime = cg.time;
			}
		}
		lf->frame = anim->firstFrame + f;
		lf->frameModel = anim->mdxFile;

		if( cg.time > lf->frameTime ) {
			lf->frameTime = cg.time;
			if( cg_debugAnim.integer ) {
				CG_Printf( "Clamp lf->frameTime\n");
			}
		}
	}

	if( lf->frameTime > cg.time + 200 ) {
		lf->frameTime = cg.time;
	}

	if( lf->oldFrameTime > cg.time ) {
		lf->oldFrameTime = cg.time;
	}
	// calculate current lerp value
	if( lf->frameTime == lf->oldFrameTime ) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = 1.0 - (float)( cg.time - lf->oldFrameTime ) / ( lf->frameTime - lf->oldFrameTime );
	}
}


/*
===============
CG_ClearLerpFrame
===============
*/
static void CG_ClearLerpFrame( centity_t *cent, clientInfo_t *ci, lerpFrame_t *lf, int animationNumber ) {
	lf->frameTime = lf->oldFrameTime = cg.time;
	CG_SetLerpFrameAnimation( cent, ci, lf, animationNumber );
	if( lf->animation ) {
		lf->oldFrame = lf->frame = lf->animation->firstFrame;
		lf->oldFrameModel = lf->frameModel = lf->animation->mdxFile;
	}
}

/*
===============
CG_SetLerpFrameAnimationRate

may include ANIM_TOGGLEBIT
===============
*/
void CG_SetLerpFrameAnimationRate( centity_t *cent, clientInfo_t *ci, lerpFrame_t *lf, int newAnimation ) {
	animation_t		*anim, *oldanim;
	int				transitionMin = -1, oldAnimTime, oldAnimNum;
	qboolean		firstAnim = qfalse;

	bg_character_t *character = CG_CharacterForClientinfo( ci, cent );

	if( !character ) {
		return;
	}

	oldAnimTime	= lf->animationTime;
	oldanim		= lf->animation;
	oldAnimNum	= lf->animationNumber;

	if( !lf->animation ) {
		firstAnim = qtrue;
	}

	lf->animationNumber = newAnimation;
	newAnimation &= ~ANIM_TOGGLEBIT;

	if ( newAnimation < 0 || newAnimation >= character->animModelInfo->numAnimations ) {
		CG_Error( "CG_SetLerpFrameAnimationRate: Bad animation number: %i", newAnimation );
	}

	anim = character->animModelInfo->animations[ newAnimation ];

	lf->animation = anim;
	lf->animationTime = lf->frameTime + anim->initialLerp;

	if( !(anim->flags & ANIMFL_FIRINGANIM) || (lf != &cent->pe.torso) ) {
		if( (lf == &cent->pe.legs) && (CG_IsCrouchingAnim( character->animModelInfo, newAnimation ) != CG_IsCrouchingAnim( character->animModelInfo, oldAnimNum )) ) {
			if( anim->moveSpeed || (anim->movetype & ((1<<ANIM_MT_TURNLEFT)|(1<<ANIM_MT_TURNRIGHT))) )	// if unknown movetype, go there faster
				transitionMin = lf->frameTime + 200;	// slowly raise/drop
			else
				transitionMin = lf->frameTime + 350;	// slowly raise/drop
		} else if( anim->moveSpeed )
			transitionMin = lf->frameTime + 120;	// always do some lerping (?)
		else	// not moving, so take your time
			transitionMin = lf->frameTime + 170;	// always do some lerping (?)

		if( oldanim && oldanim->animBlend ) { //transitionMin < lf->frameTime + oldanim->animBlend) {
			transitionMin = lf->frameTime + oldanim->animBlend;
			lf->animationTime = transitionMin;
		} else {
			// slow down transitions according to speed
			if( anim->moveSpeed && lf->animSpeedScale < 1.0 )
				lf->animationTime += anim->initialLerp;

			if( lf->animationTime < transitionMin )
				lf->animationTime = transitionMin;
		}
	}

	// if first anim, go immediately
	if( firstAnim ) {
		lf->frameTime = cg.time - 1;
		lf->animationTime = cg.time - 1;
		lf->frame = anim->firstFrame;
		lf->frameModel = anim->mdxFile;
	}

	if( cg_debugAnim.integer == 1 ) {			// DHM - Nerve :: extra debug info
		CG_Printf( "Anim: %i, %s\n", newAnimation, character->animModelInfo->animations[newAnimation]->name );
	}
}

/*
===============
CG_RunLerpFrameRate

Sets cg.snap, cg.oldFrame, and cg.backlerp
cg.time should be between oldFrameTime and frameTime after exit
===============
*/
void CG_RunLerpFrameRate( clientInfo_t *ci, lerpFrame_t *lf, int newAnimation, centity_t *cent, int recursion ) {
	int			f;
	animation_t	*anim, *oldAnim;
	animation_t *otherAnim=NULL;
	qboolean	isLadderAnim;

#define	ANIM_SCALEMAX_LOW	1.1
#define	ANIM_SCALEMAX_HIGH	1.6

#define	ANIM_SPEEDMAX_LOW	100
#define	ANIM_SPEEDMAX_HIGH	20

	// debugging tool to get no animations
	if( cg_animSpeed.integer == 0 ) {
		lf->oldFrame = lf->frame = lf->backlerp = 0;
		return;
	}

	isLadderAnim = lf->animation && (lf->animation->flags & ANIMFL_LADDERANIM);

	oldAnim = lf->animation;

	// see if the animation sequence is switching
	if( newAnimation != lf->animationNumber || !lf->animation ) {
		CG_SetLerpFrameAnimationRate( cent, ci, lf, newAnimation );
	}

	// Ridah, make sure the animation speed is updated when possible
	anim = lf->animation;

	// check for forcing last frame
	if( cent->currentState.eFlags & EF_FORCE_END_FRAME 
		// xkan, 12/27/2002 - In SP, corpse also stays at the last frame (of the death animation)
		// so that the death animation can end up in different positions
		// and the body will stay in that position
		|| ( /*CG_IsSinglePlayer() &&*/ cent->currentState.eType == ET_CORPSE ) ) {
		lf->oldFrame = lf->frame = anim->firstFrame + anim->numFrames - 1;
		lf->oldFrameModel = lf->frameModel = anim->mdxFile;
		lf->backlerp = 0;
		return;
	}

	if (anim->moveSpeed && lf->oldFrameSnapshotTime) {
		float	moveSpeed;

		// calculate the speed at which we moved over the last frame
		if( cg.latestSnapshotTime != lf->oldFrameSnapshotTime && cg.nextSnap ) {
			if( cent->currentState.number == cg.snap->ps.clientNum ) {
				if( isLadderAnim ) { // only use Z axis for speed
						lf->oldFramePos[0] = cent->lerpOrigin[0];
						lf->oldFramePos[1] = cent->lerpOrigin[1];
				} else {	// only use x/y axis
					lf->oldFramePos[2] = cent->lerpOrigin[2];
				}
				moveSpeed = Distance( cent->lerpOrigin, lf->oldFramePos ) / ((float)(cg.time - lf->oldFrameTime) / 1000.0);
			} else {
				if( isLadderAnim ) { // only use Z axis for speed
					lf->oldFramePos[0] = cent->currentState.pos.trBase[0];
					lf->oldFramePos[1] = cent->currentState.pos.trBase[1];
				}
				moveSpeed = Distance( cent->lerpOrigin, lf->oldFramePos ) / ((float)(cg.time - lf->oldFrameTime) / 1000.0);
			}
			//
			// convert it to a factor of this animation's movespeed
			lf->animSpeedScale = moveSpeed / (float)anim->moveSpeed;
			lf->oldFrameSnapshotTime = cg.latestSnapshotTime;
		}
	} else {
		// move at normal speed
		lf->animSpeedScale = 1.0;
		lf->oldFrameSnapshotTime = cg.latestSnapshotTime;
	}
	// adjust with manual setting (pain anims)
	lf->animSpeedScale *= cent->pe.animSpeed;

	// if we have passed the current frame, move it to
	// oldFrame and calculate a new frame
	if( cg.time >= lf->frameTime ) {
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;
		lf->oldFrameModel = lf->frameModel;
		VectorCopy( cent->lerpOrigin, lf->oldFramePos );

		// restrict the speed range
		if( lf->animSpeedScale < 0.25 ) {	// if it's too slow, then a really slow spped, combined with a sudden take-off, can leave them playing a really slow frame while they a moving really fast
			if (lf->animSpeedScale < 0.01 && isLadderAnim)
				lf->animSpeedScale = 0.0;
			else
				lf->animSpeedScale = 0.25;
		} else if( lf->animSpeedScale > ANIM_SCALEMAX_LOW ) {

			if( !(anim->flags & ANIMFL_LADDERANIM) ) {
				// allow slower anims to speed up more than faster anims
				if( anim->moveSpeed > ANIM_SPEEDMAX_LOW ) {
					lf->animSpeedScale = ANIM_SCALEMAX_LOW;
				} else if( anim->moveSpeed < ANIM_SPEEDMAX_HIGH ) {
					if( lf->animSpeedScale > ANIM_SCALEMAX_HIGH )
						lf->animSpeedScale = ANIM_SCALEMAX_HIGH;
				} else {
					lf->animSpeedScale = ANIM_SCALEMAX_HIGH - (ANIM_SCALEMAX_HIGH - ANIM_SCALEMAX_LOW) * (float)(anim->moveSpeed - ANIM_SPEEDMAX_HIGH) / (float)(ANIM_SPEEDMAX_LOW - ANIM_SPEEDMAX_HIGH);
				}
			} else if( lf->animSpeedScale > 4.0 ) {
				lf->animSpeedScale = 4.0;
			}

		}

		if( lf == &cent->pe.legs ) {
			otherAnim = cent->pe.torso.animation;
		} else if( lf == &cent->pe.torso ) {
			otherAnim = cent->pe.legs.animation;
		}

		// get the next frame based on the animation
		if( !lf->animSpeedScale ) {
			// stopped on the ladder, so stay on the same frame
			f = lf->frame - anim->firstFrame;
			lf->frameTime += anim->frameLerp;		// don't wait too long before starting to move again
		} else if ( lf->oldAnimationNumber != lf->animationNumber &&
				  (!anim->moveSpeed || lf->oldFrame < anim->firstFrame || lf->oldFrame >= anim->firstFrame + anim->numFrames) ) {	// Ridah, added this so walking frames don't always get reset to 0, which can happen in the middle of a walking anim, which looks wierd
			lf->frameTime = lf->animationTime;		// initial lerp
			if (oldAnim && anim->moveSpeed) {	// keep locomotions going continuously
				f = (lf->frame - oldAnim->firstFrame) + 1;
				while (f < 0) {
					f += anim->numFrames;
				}
			} else {
				f = 0;
			}
		} else if ( (lf == &cent->pe.legs) && otherAnim && !(anim->flags & ANIMFL_FIRINGANIM) && ((lf->animationNumber & ~ANIM_TOGGLEBIT) == (cent->pe.torso.animationNumber & ~ANIM_TOGGLEBIT)) && (!anim->moveSpeed) ) {
			// legs should synch with torso
			f = cent->pe.torso.frame - otherAnim->firstFrame;
			if (f >= anim->numFrames || f < 0) {
				f = 0;	// wait at the start for the legs to catch up (assuming they are still in an old anim)
			}
			lf->frameTime = cent->pe.torso.frameTime;
		} else if ( (lf == &cent->pe.torso) && otherAnim && !(anim->flags & ANIMFL_FIRINGANIM) && ((lf->animationNumber & ~ANIM_TOGGLEBIT) == (cent->pe.legs.animationNumber & ~ANIM_TOGGLEBIT)) && (otherAnim->moveSpeed) ) {
			// torso needs to sync with legs
			f = cent->pe.legs.frame - otherAnim->firstFrame;
			if (f >= anim->numFrames || f < 0) {
				f = 0;	// wait at the start for the legs to catch up (assuming they are still in an old anim)
			}
			lf->frameTime = cent->pe.legs.frameTime;
		} else {
			lf->frameTime = lf->oldFrameTime + (int)((float)anim->frameLerp * (1.0 / lf->animSpeedScale));
			if (lf->frameTime < cg.time) {
				lf->frameTime = cg.time;
			}

			// check for skipping frames (eg. death anims play in slo-mo if low framerate)
			if( anim->flags & ANIMFL_REVERSED ) {
				if ( cg.time > lf->frameTime && !anim->moveSpeed ) {
					f = (anim->numFrames - 1) - ( (lf->frame - anim->firstFrame) - (1 + (cg.time - lf->frameTime) / anim->frameLerp ) );
				} else {
					f = (anim->numFrames - 1) - ( (lf->frame - anim->firstFrame) - 1 );
				}
			} else {
				if ( cg.time > lf->frameTime && !anim->moveSpeed ) {
					f = (lf->frame - anim->firstFrame) + 1 + (cg.time - lf->frameTime) / anim->frameLerp;
				} else {
					f = (lf->frame - anim->firstFrame) + 1;
				}
			}

			if (f < 0)
				f = 0;
		}
		//f = ( lf->frameTime - lf->animationTime ) / anim->frameLerp;
		if ( f >= anim->numFrames ) {
			f -= anim->numFrames;
			if ( anim->loopFrames ) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = anim->numFrames - 1;
				// the animation is stuck at the end, so it
				// can immediately transition to another sequence
				lf->frameTime = cg.time;
			}
		}
		if( anim->flags & ANIMFL_REVERSED ) {
			lf->frame = anim->firstFrame + anim->numFrames - 1 - f;
			lf->frameModel = anim->mdxFile;
		} else {
			lf->frame = anim->firstFrame + f;
			lf->frameModel = anim->mdxFile;
		}

		if( cg.time > lf->frameTime ) {

			// Ridah, run the frame again until we move ahead of the current time, fixes walking speeds for zombie
			if (/*!anim->moveSpeed ||*/ recursion > 4)
				lf->frameTime = cg.time;
			else
				CG_RunLerpFrameRate( ci, lf, newAnimation, cent, recursion + 1 );

			if ( cg_debugAnim.integer > 3 ) {
				CG_Printf( "Clamp lf->frameTime\n");
			}
		}
		lf->oldAnimationNumber = lf->animationNumber;
	}

	// Gordon: BIG hack, occaisionaly (VERY occaisionally), the frametime gets totally wacked
	if( lf->frameTime > cg.time + 5000 ) {
		lf->frameTime = cg.time;
	}

	if( lf->oldFrameTime > cg.time ) {
		lf->oldFrameTime = cg.time;
	}
	// calculate current lerp value
	if( lf->frameTime == lf->oldFrameTime ) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = 1.0 - (float)( cg.time - lf->oldFrameTime ) / ( lf->frameTime - lf->oldFrameTime );
	}
}

/*
===============
CG_ClearLerpFrameRate
===============
*/
void CG_ClearLerpFrameRate( centity_t *cent, clientInfo_t *ci, lerpFrame_t *lf, int animationNumber ) {
	lf->frameTime = lf->oldFrameTime = cg.time;
	CG_SetLerpFrameAnimationRate( cent, ci, lf, animationNumber );
	if( lf->animation ) {
		lf->oldFrame = lf->frame = lf->animation->firstFrame;
		lf->oldFrameModel = lf->frameModel = lf->animation->mdxFile;
	}
}

/*
===============
CG_PlayerAnimation
===============
*/
static void CG_PlayerAnimation( centity_t *cent, refEntity_t *body ) {
	clientInfo_t	*ci;
	int				clientNum;
	int				animIndex, tempIndex;
	bg_character_t	*character;

	clientNum = cent->currentState.clientNum;

	ci = &cgs.clientinfo[ clientNum ];
	character = CG_CharacterForClientinfo( ci, cent );

	if( !character ) {
		return;
	}

	if( cg_noPlayerAnims.integer ) {
		body->frame = body->oldframe = body->torsoFrame = body->oldTorsoFrame = 0;
		body->frameModel = body->oldframeModel = body->torsoFrameModel= body->oldTorsoFrameModel = character->animModelInfo->animations[0]->mdxFile;
		return;
	}

	// default to whatever the legs are currently doing
	animIndex = cent->currentState.legsAnim;

	// do the shuffle turn frames locally
	if( !(cent->currentState.eFlags & EF_DEAD) && cent->pe.legs.yawing &&
		!(cent->currentState.eFlags & 0x00890000) ) {
		int turn;
		/* Original TC disallows shuffle during prone/other locked poses, and
		 * chooses dedicated crouch-turn scripts instead of standing turns. */
		if(cent->currentState.eFlags & EF_CROUCHING)
			turn = cent->pe.legs.yawing == SWING_RIGHT ? ANIM_MT_IDLECRTURNRIGHT : ANIM_MT_IDLECRTURNLEFT;
		else
			turn = cent->pe.legs.yawing == SWING_RIGHT ? ANIM_MT_TURNRIGHT : ANIM_MT_TURNLEFT;
		//CG_Printf("turn: %i\n", cg.time );
		tempIndex = BG_GetAnimScriptAnimation( clientNum, character->animModelInfo, cent->currentState.aiState, turn );
		if (tempIndex > -1) {
			animIndex = tempIndex;
		}
	}
	// run the animation
	CG_RunLerpFrameRate( ci, &cent->pe.legs, animIndex, cent, 0 );

	body->oldframe = cent->pe.legs.oldFrame;
	body->frame = cent->pe.legs.frame;
	body->backlerp = cent->pe.legs.backlerp;
	body->frameModel = cent->pe.legs.frameModel;
	body->oldframeModel = cent->pe.legs.oldFrameModel;

	CG_RunLerpFrameRate( ci, &cent->pe.torso, cent->currentState.torsoAnim, cent, 0 );

	body->oldTorsoFrame = cent->pe.torso.oldFrame;
	body->torsoFrame = cent->pe.torso.frame;
	body->torsoBacklerp = cent->pe.torso.backlerp;
	body->torsoFrameModel = cent->pe.torso.frameModel;
	body->oldTorsoFrameModel = cent->pe.torso.oldFrameModel;
}

/*
=============================================================================

PLAYER ANGLES

=============================================================================
*/

/*
==================
CG_SwingAngles
==================
*/
static void CG_SwingAngles( float destination, float swingTolerance, float clampTolerance,
					float speed, float *angle, qboolean *swinging ) {
	float	swing;
	float	move;
	float	scale;

	if ( !*swinging ) {
		// see if a swing should be started
		swing = AngleSubtract( *angle, destination );
		if ( swing > swingTolerance || swing < -swingTolerance ) {
			*swinging = qtrue;
		}
	}

	if ( !*swinging ) {
		return;
	}
	
	// modify the speed depending on the delta
	// so it doesn't seem so linear
	swing = AngleSubtract( destination, *angle );
	scale = fabs( swing );
	scale *= 0.05;
	if (scale < 0.5)
		scale = 0.5;

	// swing towards the destination angle
	if ( swing >= 0 ) {
		move = cg.frametime * scale * speed;
		if ( move >= swing ) {
			move = swing;
			*swinging = qfalse;
		} else {
			*swinging = SWING_LEFT;		// left
		}
		*angle = AngleMod( *angle + move );
	} else if ( swing < 0 ) {
		move = cg.frametime * scale * -speed;
		if ( move <= swing ) {
			move = swing;
			*swinging = qfalse;
		} else {
			*swinging = SWING_RIGHT;	// right
		}
		*angle = AngleMod( *angle + move );
	}

	// clamp to no more than tolerance
	swing = AngleSubtract( destination, *angle );
	if ( swing > clampTolerance ) {
		*angle = AngleMod( destination - (clampTolerance - 1) );
	} else if ( swing < -clampTolerance ) {
		*angle = AngleMod( destination + (clampTolerance - 1) );
	}
}

/*
=================
CG_AddPainTwitch
=================
*/
static void CG_AddPainTwitch( centity_t *cent, vec3_t torsoAngles ) {
	int		t;
	float	f;
	int		duration;
	float	direction;

	if( !cent->pe.animSpeed ) {
		// we need to inititialize this stuff
		cent->pe.painAnimLegs = -1;
		cent->pe.painAnimTorso = -1;
		cent->pe.animSpeed = 1.0;
	}

	if( cent->currentState.eFlags & EF_DEAD ) {
		cent->pe.painAnimLegs = -1;
		cent->pe.painAnimTorso = -1;
		cent->pe.animSpeed = 1.0;
		return;
	}

	if( cent->pe.painDuration ) {
		duration = cent->pe.painDuration;
	} else {
		duration = PAIN_TWITCH_TIME;
	}
	direction = (float)duration * 0.085;
	if (direction > 30)
		direction = 30;
	if (direction < 10)
		direction = 10;
	direction *= (float)(cent->pe.painDirection*2) - 1;

	t = cg.time - cent->pe.painTime;
	if( t >= duration ) {
		return;
	}

	f = 1.0 - (float)t / duration;
	if( cent->pe.painDirection ) {
		torsoAngles[ROLL] += 20 * f;
	} else {
		torsoAngles[ROLL] -= 20 * f;
	}
}

/*
===============
CG_PlayerAngles

Handles seperate torso motion

  legs pivot based on direction of movement

  head always looks exactly at cent->lerpAngles

  if motion < 20 degrees, show in head only
  if < 45 degrees, also show in torso
===============
*/
static void CG_PlayerAngles( centity_t *cent, vec3_t legs[3], vec3_t torso[3], vec3_t head[3] ) {
	vec3_t			legsAngles, torsoAngles, headAngles;
	float			dest;
	vec3_t			velocity;
	float			speed;
	float			clampTolerance;
	int				legsSet, torsoSet;
	clientInfo_t	*ci;
	bg_character_t	*character;

	ci = &cgs.clientinfo[ cent->currentState.clientNum ];

	character = CG_CharacterForClientinfo( ci, cent );

	if( !character ) {
		return;
	}

	legsSet = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
	torsoSet = cent->currentState.torsoAnim & ~ANIM_TOGGLEBIT;

	VectorCopy( cent->lerpAngles, headAngles );
	headAngles[YAW] = AngleMod( headAngles[YAW] );
	VectorClear( legsAngles );
	VectorClear( torsoAngles );

	// --------- yaw -------------

	// allow yaw to drift a bit, unless these conditions don't allow them
	if( !(BG_GetConditionBitFlag( cent->currentState.clientNum, ANIM_COND_MOVETYPE, ANIM_MT_IDLE ) ||
		  BG_GetConditionBitFlag( cent->currentState.clientNum, ANIM_COND_MOVETYPE, ANIM_MT_IDLECR ) )/*
		||	 (BG_GetConditionValue( cent->currentState.clientNum, ANIM_COND_MOVETYPE, qfalse ) & ((1<<ANIM_MT_STRAFELEFT) | (1<<ANIM_MT_STRAFERIGHT)) )*/) {

		// always point all in the same direction
		cent->pe.torso.yawing = qtrue;	// always center
		cent->pe.torso.pitching = qtrue;	// always center
		cent->pe.legs.yawing = qtrue;	// always center

	// if firing, make sure torso and head are always aligned
	} else if( BG_GetConditionValue( cent->currentState.clientNum, ANIM_COND_FIRING, qtrue ) ) {
		cent->pe.torso.yawing = qtrue;	// always center
		cent->pe.torso.pitching = qtrue;	// always center
	}

	// adjust legs for movement dir
	if( cent->currentState.eFlags & EF_DEAD || cent->currentState.eFlags & EF_MOUNTEDTANK ) {
		// don't let dead bodies twitch
		legsAngles[YAW] = headAngles[YAW];
		torsoAngles[YAW] = headAngles[YAW];
	} else {
		legsAngles[YAW] = headAngles[YAW] + cent->currentState.angles2[YAW];

		/* TC keeps the torso facing the aim direction even while not firing. */
		cent->pe.torso.pitching = qtrue;
		cent->pe.torso.yawing = qtrue;
		torsoAngles[YAW] = headAngles[YAW];
		clampTolerance = 60;

		// torso
		CG_SwingAngles( torsoAngles[YAW], 25, clampTolerance, cg_swingSpeed.value, &cent->pe.torso.yawAngle, &cent->pe.torso.yawing );

		// if the legs are yawing (facing heading direction), allow them to rotate a bit, so we don't keep calling
		// the legs_turn animation while an AI is firing, and therefore his angles will be randomizing according to their accuracy

		clampTolerance = 150;

		if( BG_GetConditionBitFlag( ci->clientNum, ANIM_COND_MOVETYPE, ANIM_MT_IDLE) ) {
			cent->pe.legs.yawing = qfalse; // set it if they really need to swing
			CG_SwingAngles( legsAngles[YAW], 20, clampTolerance, 0.5*cg_swingSpeed.value, &cent->pe.legs.yawAngle, &cent->pe.legs.yawing );
		} else if( strstr( BG_GetAnimString( character->animModelInfo, legsSet ), "strafe" ) ) {
		// FIXME: what is this strstr hack??
		//if	( BG_GetConditionValue( ci->clientNum, ANIM_COND_MOVETYPE, qfalse ) & ((1<<ANIM_MT_STRAFERIGHT)|(1<<ANIM_MT_STRAFELEFT)) )
			cent->pe.legs.yawing = qfalse; // set it if they really need to swing
			legsAngles[YAW] = headAngles[YAW];
			CG_SwingAngles( legsAngles[YAW], 0, clampTolerance, cg_swingSpeed.value, &cent->pe.legs.yawAngle, &cent->pe.legs.yawing );
		} else if(cent->pe.legs.yawing) {
			CG_SwingAngles( legsAngles[YAW], 0, clampTolerance, cg_swingSpeed.value, &cent->pe.legs.yawAngle, &cent->pe.legs.yawing );
		} else {
			CG_SwingAngles( legsAngles[YAW], 40, clampTolerance, cg_swingSpeed.value, &cent->pe.legs.yawAngle, &cent->pe.legs.yawing );
		}

		torsoAngles[YAW] = cent->pe.torso.yawAngle;
		legsAngles[YAW] = cent->pe.legs.yawAngle;
	}

	// --------- pitch -------------

	// only show a fraction of the pitch angle in the torso
	if( headAngles[PITCH] > 180 ) {
		dest = (-360 + headAngles[PITCH]) * 0.75;
	} else {
		dest = headAngles[PITCH] * 0.75;
	}
	//CG_SwingAngles( dest, 15, 30, 0.1, &cent->pe.torso.pitchAngle, &cent->pe.torso.pitching );
	//torsoAngles[PITCH] = cent->pe.torso.pitchAngle;

	if( cent->currentState.eFlags & EF_PRONE ) {
		torsoAngles[PITCH] = legsAngles[PITCH] - 3;
	} else {
		CG_SwingAngles( dest, 15, 30, 0.1, &cent->pe.torso.pitchAngle, &cent->pe.torso.pitching );
		torsoAngles[PITCH] = cent->pe.torso.pitchAngle;
	}


	// --------- roll -------------


	// lean towards the direction of travel
	VectorCopy( cent->currentState.pos.trDelta, velocity );
	speed = VectorNormalize( velocity );
	if( speed ) {
		vec3_t	axis[3];
		float	side;

		speed *= 0.05;

		AnglesToAxis( legsAngles, axis );
		side = speed * DotProduct( velocity, axis[1] );
		legsAngles[ROLL] -= side;

		side = speed * DotProduct( velocity, axis[0] );
		legsAngles[PITCH] += side;
	}

	/* Original g_leanmode serverinfo consumer: left lean has extra roll.
	 * This is entity angles2[ROLL], not local predicted view lean. */
	if(cgs.tceLeanMode > 0 && !(cent->currentState.eFlags & EF_DEAD)) {
		float lean = cent->currentState.angles2[ROLL];
		if(lean < 0.f) lean *= 1.65f;
		torsoAngles[ROLL] += lean;
		headAngles[ROLL] += lean;
	}

	// pain twitch
	CG_AddPainTwitch( cent, torsoAngles );

	// pull the angles back out of the hierarchial chain
	AnglesSubtract( headAngles, torsoAngles, headAngles );
	AnglesSubtract( torsoAngles, legsAngles, torsoAngles );
	AnglesToAxis( legsAngles, legs );
	AnglesToAxis( torsoAngles, torso );
	AnglesToAxis( headAngles, head );
}

/*
==============
CG_BreathPuffs
==============
*/
static void CG_BreathPuffs( centity_t *cent, refEntity_t *head ) {
	clientInfo_t	*ci;
	vec3_t			up, forward;
	int				contents;
	vec3_t			mang, morg, maxis[3];

	ci = &cgs.clientinfo[ cent->currentState.number ];

	if( !cg_enableBreath.integer ) {
		return;
	}

	if( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson ) {
		return;
	}

	if( !(cent->currentState.eFlags & EF_DEAD) ) {
		return;
	}

	// allow cg_enableBreath to force everyone to have breath
	if( !(cent->currentState.eFlags & EF_BREATH) ) {
		return;
	}

	contents = CG_PointContents( head->origin, 0 );
	if( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		return;
	}
	if( ci->breathPuffTime > cg.time ) {
		return;
	}

	CG_GetOriginForTag( cent, head, "tag_mouth", 0, morg, maxis );
	AxisToAngles( maxis, mang );
	AngleVectors( mang, forward, NULL, up );

	//push the origin out a tad so it's not right in the guys face (tad==4)
	VectorMA(morg, 4, forward, morg);

	forward[0] = up[0] * 8 + forward[0] * 5;
	forward[1] = up[1] * 8 + forward[1] * 5;
	forward[2] = up[2] * 8 + forward[2] * 5;

	CG_SmokePuff( morg, forward, 4, 1, 1, 1, 0.5f, 2000, cg.time, cg.time + 400, 0, cgs.media.shotgunSmokePuffShader );

	ci->breathPuffTime = cg.time + 3000 + random()*1000;
}

/*
===============
CG_TrailItem
===============
*/
/*static void CG_TrailItem( centity_t *cent, qhandle_t hModel ) {
	refEntity_t		ent;
	vec3_t			angles;
	qboolean		ducking;

	// DHM - Nerve :: Don't draw icon above your own head
	if ( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson )
		return;

	memset( &ent, 0, sizeof( ent ) );

	VectorCopy( cent->lerpAngles, angles );
	angles[PITCH] = 0;
	angles[ROLL] = 0;
	AnglesToAxis( angles, ent.axis );

	// DHM - Nerve :: adjusted values
	VectorCopy( cent->lerpOrigin, ent.origin );

	// Account for ducking
	if ( cent->currentState.clientNum == cg.snap->ps.clientNum )
		ducking = (cg.snap->ps.pm_flags & PMF_DUCKED);
	else
		ducking = (qboolean)cent->currentState.animMovetype;

	if ( ducking )
		ent.origin[2] += 38;
	else
		ent.origin[2] += 56;

	ent.hModel = hModel;
	trap_R_AddRefEntityToScene( &ent );
}*/

/*
===============
CG_PlayerFloatSprite

Float a sprite over the player's head
DHM - Nerve :: added height parameter
===============
*/
static void CG_PlayerFloatSprite( centity_t *cent, qhandle_t shader, int height ) {
	int				rf;
	refEntity_t		ent;

	if ( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson ) {
		rf = RF_THIRD_PERSON;		// only show in mirrors
	} else {
		rf = 0;
	}

	memset( &ent, 0, sizeof( ent ) );
	VectorCopy( cent->lerpOrigin, ent.origin );
	ent.origin[2] += height;			// DHM - Nerve :: was '48'

	// Account for ducking
	if ( cent->currentState.clientNum == cg.snap->ps.clientNum ) {
		if( cg.snap->ps.pm_flags & PMF_DUCKED )
			ent.origin[2] -= 18;
	} else {
		if( (qboolean)cent->currentState.animMovetype )
			ent.origin[2] -= 18;
	}

	ent.reType = RT_SPRITE;
	ent.customShader = shader;
	ent.radius = 6.66;
	ent.renderfx = rf;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 255;
	trap_R_AddRefEntityToScene( &ent );
}



/*
===============
CG_PlayerSprites

Float sprites over the player's head
===============
*/
/* Whole TC depth-hacked teammate/carrier sprite, Windows30055ba0. */
static void CG_PlayerFloatSpriteDepthHack(centity_t *cent, qhandle_t shader, int height) {
    refEntity_t ent;
    if(cg_drawFriend.integer < 2 || VectorDistance(cent->lerpOrigin,cg.refdef_current->vieworg)>256.f) {
        CG_PlayerFloatSprite(cent,shader,height);
        return;
    }
    memset(&ent,0,sizeof(ent));
    VectorCopy(cent->lerpOrigin,ent.origin);
    ent.origin[2]+=height;
    if(cent->currentState.clientNum==cg.snap->ps.clientNum) {
        if(cg.snap->ps.pm_flags & PMF_DUCKED)ent.origin[2]-=18.f;
    } else if(cent->currentState.animMovetype)ent.origin[2]-=18.f;
    ent.reType=RT_SPRITE;
    ent.customShader=shader;
    ent.radius=6.66f;
    ent.renderfx=(cent->currentState.number==cg.snap->ps.clientNum && !cg.renderingThirdPerson)
        ? RF_THIRD_PERSON : RF_DEPTHHACK;
    memset(ent.shaderRGBA,255,sizeof(ent.shaderRGBA));
    trap_R_AddRefEntityToScene(&ent);
}

static void CG_PlayerSprites(centity_t *cent) {
    entityState_t *es=&cent->currentState;
    fireteamData_t *ft;
    int extra=tceSmokeNewBBox ? 12 : 0;
    int height=extra+((es->effect1Time & 4) ? 12 : 48);
    int team;
    if(es->eFlags & 0x400) {
        CG_PlayerFloatSprite(cent,cgs.media.disconnectIcon,height+4);
        return;
    }
    if((es->eFlags & 0x200) && !(es->eFlags & EF_DEAD)) {
        CG_PlayerFloatSprite(cent,cgs.media.balloonShader,height+4);
        return;
    }
    team=cgs.clientinfo[es->clientNum].team;
    if(cg_drawFriend.integer<1)return;
    ft=CG_IsOnFireteam(es->number);
    if(ft && ft==CG_IsOnFireteam(cg.clientNum) && cgs.clientinfo[es->number].selected)
        CG_PlayerFloatSpriteDepthHack(cent,cgs.media.fireteamicons[ft->ident],extra+56);
    if(cg.snap->ps.persistant[PERS_TEAM]!=team || (es->eFlags & EF_DEAD) || es->number!=es->clientNum)return;
    if(es->effect1Time & 0x10)
        CG_PlayerFloatSpriteDepthHack(cent,cgs.media.tcePlayerCarrierIcons[0],height+4);
    else if(es->effect1Time & 0x20)
        CG_PlayerFloatSpriteDepthHack(cent,cgs.media.tcePlayerCarrierIcons[2],height+4);
    else if(es->effect1Time & 0x40)
        CG_PlayerFloatSpriteDepthHack(cent,cgs.media.tcePlayerCarrierIcons[3],height+4);
    else if(es->powerups & 0xc0)
        CG_PlayerFloatSpriteDepthHack(cent,cgs.media.tcePlayerCarrierIcons[1],height+4);
}

/*
===============
CG_PlayerShadow

Returns the Z component of the surface being shadowed

  should it return a full plane instead of a Z?
===============
*/
#define	SHADOW_DISTANCE		64
#define	ZOFS	6.0
#define	SHADOW_MIN_DIST	250.0
#define	SHADOW_MAX_DIST	512.0

typedef struct {
	char *tagname;
	float size;
	float maxdist;
	float maxalpha;
	qhandle_t shader;
} shadowPart_t;

/* Whole TC tag-shadow renderer, Windows30055d10. */
static qboolean CG_PlayerShadow(centity_t *cent, float *shadowPlane) {
    vec3_t end,origin,angles,axis[3];
    trace_t trace;
    int part,tag;
    shadowPart_t parts[]={
        {"tag_footleft",10,4,1.f,0},
        {"tag_footright",10,4,1.f,0},
        {"tag_torso",18,96,.8f,0},
        {NULL,0,0,0,0}
    };
    parts[0].shader=parts[1].shader=cgs.media.shadowFootShader;
    parts[2].shader=cgs.media.shadowTorsoShader;
    *shadowPlane=0.f;
    if(!cg_shadows.integer && developer.integer)return qfalse;
    VectorCopy(cent->lerpOrigin,end);
    end[2]-=64.f;
    trap_CM_BoxTrace(&trace,cent->lerpOrigin,end,NULL,NULL,0,MASK_PLAYERSOLID);
    if(trace.fraction==1.f || trace.fraction==0.f)return qfalse;
    *shadowPlane=trace.endpos[2]+1.f;
    if(cg_shadows.integer!=1)return qtrue;
    if(cent->currentState.eFlags & EF_DEAD)return qfalse;
    if(VectorDistance(cent->lerpOrigin,cg.refdef_current->vieworg)<1024.f) {
        for(part=0;parts[part].tagname;part++) {
            for(tag=0;(tag=CG_GetOriginForTag(cent,&cent->pe.bodyRefEnt,
                parts[part].tagname,tag,origin,axis))>=0;tag++) {
                float alpha;
                if(origin[2]<*shadowPlane)origin[2]=*shadowPlane;
                alpha=1.f-(origin[2]-(*shadowPlane+6.f))/parts[part].maxdist;
                if(alpha<0.f)continue;
                if(alpha>parts[part].maxalpha)alpha=parts[part].maxalpha;
                origin[2]=*shadowPlane;
                AxisToAngles(axis,angles);
                CG_EliteImpactMark(parts[part].shader,origin,trace.plane.normal,
                    angles[YAW],alpha,alpha,alpha,alpha,qfalse,parts[part].size,qtrue,-1);
            }
        }
    }
    return qtrue;
}

/*
===============
CG_PlayerSplash

Draw a mark at the water surface
===============
*/
static void CG_PlayerSplash( centity_t *cent ) {
	vec3_t		start, end;
	trace_t		trace;
	int			contents;
	polyVert_t	verts[4];

	if ( !cg_shadows.integer ) {
		return;
	}

	VectorCopy( cent->lerpOrigin, end );
	end[2] -= 24;

	// if the feet aren't in liquid, don't make a mark
	// this won't handle moving water brushes, but they wouldn't draw right anyway...
	contents = CG_PointContents( end, 0 );
	if ( !( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) ) {
		return;
	}

	VectorCopy( cent->lerpOrigin, start );
	start[2] += 32;

	// if the head isn't out of liquid, don't make a mark
	contents = CG_PointContents( start, 0 );
	if ( contents & ( CONTENTS_SOLID | CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		return;
	}

	// trace down to find the surface
	trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) );

	if ( trace.fraction == 1.0 ) {
		return;
	}

	// create a mark polygon
	VectorCopy( trace.endpos, verts[0].xyz );
	verts[0].xyz[0] -= 32;
	verts[0].xyz[1] -= 32;
	verts[0].st[0] = 0;
	verts[0].st[1] = 0;
	verts[0].modulate[0] = 255;
	verts[0].modulate[1] = 255;
	verts[0].modulate[2] = 255;
	verts[0].modulate[3] = 255;

	VectorCopy( trace.endpos, verts[1].xyz );
	verts[1].xyz[0] -= 32;
	verts[1].xyz[1] += 32;
	verts[1].st[0] = 0;
	verts[1].st[1] = 1;
	verts[1].modulate[0] = 255;
	verts[1].modulate[1] = 255;
	verts[1].modulate[2] = 255;
	verts[1].modulate[3] = 255;

	VectorCopy( trace.endpos, verts[2].xyz );
	verts[2].xyz[0] += 32;
	verts[2].xyz[1] += 32;
	verts[2].st[0] = 1;
	verts[2].st[1] = 1;
	verts[2].modulate[0] = 255;
	verts[2].modulate[1] = 255;
	verts[2].modulate[2] = 255;
	verts[2].modulate[3] = 255;

	VectorCopy( trace.endpos, verts[3].xyz );
	verts[3].xyz[0] += 32;
	verts[3].xyz[1] -= 32;
	verts[3].st[0] = 1;
	verts[3].st[1] = 0;
	verts[3].modulate[0] = 255;
	verts[3].modulate[1] = 255;
	verts[3].modulate[2] = 255;
	verts[3].modulate[3] = 255;

	trap_R_AddPolyToScene( cgs.media.wakeMarkShader, 4, verts );
}

//==========================================================================

/*
===============
CG_AddRefEntityWithPowerups

Adds a piece with modifications or duplications for powerups
Also called by CG_Missile for quad rockets, but nobody can tell...
===============
*/

char	*vtosf( const vec3_t v ) {
	static	int		index;
	static	char	str[8][64];
	char	*s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1)&7;

	Com_sprintf (s, 64, "(%f %f %f)", v[0], v[1], v[2]);

	return s;
}


/*
===============
CG_AnimPlayerConditions

	predict, or calculate condition for this entity, if it is not the local client
===============
*/
void CG_AnimPlayerConditions( bg_character_t *character, centity_t *cent ) {
	entityState_t	*es;
	int				legsAnim;

	if( !character ) {
		return;
	}
	if( cg.snap && cg.snap->ps.clientNum == cent->currentState.number && !cg.renderingThirdPerson ) {
		return;
	}

	es = &cent->currentState;

	// WEAPON
	if( es->eFlags & EF_ZOOMING ) {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_WEAPON, WP_BINOCULARS, qtrue );
	} else {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_WEAPON, es->weapon, qtrue );
	}

	// MOUNTED
	if( (es->eFlags & EF_MG42_ACTIVE) || (es->eFlags & EF_MOUNTEDTANK) ) {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_MOUNTED, MOUNTED_MG42, qtrue );
	} else if (es->eFlags & EF_AAGUN_ACTIVE) {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_MOUNTED, MOUNTED_AAGUN, qtrue );
	} else {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_MOUNTED, MOUNTED_UNUSED, qtrue );
	}

	// UNDERHAND
	BG_UpdateConditionValue( es->clientNum, ANIM_COND_UNDERHAND, cent->lerpAngles[0] > 0, qtrue );

	if( es->eFlags & EF_CROUCHING ) {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_CROUCHING, qtrue, qtrue );
	} else {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_CROUCHING, qfalse, qtrue );
	}

	if( es->eFlags & EF_FIRING ) {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_FIRING, qtrue, qtrue );
	} else {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_FIRING, qfalse, qtrue );
	}

	// reverse engineer the legs anim -> movetype (if possible)
	legsAnim = es->legsAnim & ~ANIM_TOGGLEBIT;
	if( character->animModelInfo->animations[legsAnim]->movetype ) {
		BG_UpdateConditionValue( es->clientNum, ANIM_COND_MOVETYPE, character->animModelInfo->animations[legsAnim]->movetype, qfalse );
	}

}


/*
===============
CG_Player
===============
*/
/* Windows30034780 / Linux00065e2c. Cache only a positive result: an
 * occluded player is reconsidered every frame, a visible one after 1000ms. */
void CG_CalcEntityVisibility(centity_t *cent) {
    vec3_t direction, point;
    trace_t trace;
    float radius, top, bottom, sample;
    int pose = cent->currentState.otherEntityNum2;
    int corner;
    if (cent->tceVisible &&
        (int)((unsigned)cent->tceVisibilityUntil - (unsigned)cg.time) > 0) return;
    cent->tceVisible = qtrue;
    cent->tceVisibilityUntil = (int)((unsigned)cg.time + 1000u);
    if (cent->currentState.eType != ET_PLAYER ||
        cent->currentState.clientNum == cg.snap->ps.clientNum ||
        (cent->currentState.eFlags & EF_DEAD)) return;

    radius = tceSmokeNewBBox ? 16.0f : 15.0f;
    bottom = tceSmokeNewBBox ? 30.0f : 24.0f;
    if (pose & 4) {
        radius = tceSmokeNewBBox ? 36.0f : 30.0f;
        top = tceSmokeNewBBox ? 6.0f : 4.0f;
    } else if (!(pose & 2)) {
        top = tceSmokeNewBBox ? 60.0f : 48.0f;
    } else if (!(pose & 8)) {
        top = bottom;
    } else {
        top = tceSmokeNewBBox ? 40.0f : 32.0f;
    }
    VectorSubtract(cent->lerpOrigin, cg.refdef.vieworg, direction);
    VectorNormalize2(direction, direction);
    if (DotProduct(cg.refdef.viewaxis[0], direction) > 0.0f) {
        CG_Trace(&trace, cg.refdef.vieworg, vec3_origin, vec3_origin,
            cent->lerpOrigin, cg.snap->ps.clientNum, CONTENTS_SOLID);
        if (trace.fraction >= 1.0f ||
            (trace.surfaceFlags & 0xff000000) == 0x0c000000) return;
        /* Original order: top ++,-+,+-,--; then bottom in the same order. */
        for (corner = 0; corner < 8; ++corner) {
            point[0] = cent->lerpOrigin[0] + ((corner & 1) ? -radius : radius);
            point[1] = cent->lerpOrigin[1] + ((corner & 2) ? -radius : radius);
            point[2] = cent->lerpOrigin[2] + ((corner & 4) ? -bottom : top);
            CG_Trace(&trace, cg.refdef.vieworg, vec3_origin, vec3_origin,
                point, cg.snap->ps.clientNum, CONTENTS_SOLID);
            if (trace.fraction >= 1.0f ||
                (trace.surfaceFlags & 0xff000000) == 0x0c000000) return;
        }
        sample = (rand() & 32767) * (1.0f / 32767.0f) - 0.5f;
        point[0] = (sample + sample) * radius + cent->lerpOrigin[0];
        sample = (rand() & 32767) * (1.0f / 32767.0f) - 0.5f;
        point[1] = (sample + sample) * radius + cent->lerpOrigin[1];
        point[2] = ((bottom + top) * (rand() & 32767) *
            (1.0f / 32767.0f) - bottom) + cent->lerpOrigin[2];
        CG_Trace(&trace, cg.refdef.vieworg, vec3_origin, vec3_origin,
            point, cg.snap->ps.clientNum, CONTENTS_SOLID);
        if (trace.fraction >= 1.0f ||
            (trace.surfaceFlags & 0xff000000) == 0x0c000000) return;
    }
    cent->tceVisible = qfalse;
}

void CG_Player( centity_t *cent )
{
	clientInfo_t	*ci;
	refEntity_t		body;
	refEntity_t		head;
	refEntity_t		acc;
	vec3_t			playerOrigin;
	vec3_t			lightorigin;
	int				clientNum,i;
	int				renderfx;
	qboolean		shadow;
	float			shadowPlane;
//	float			gumsflappin = 0;	// talking amplitude
	qboolean		usingBinocs = qfalse;
	centity_t		*cgsnap;
	bg_character_t	*character;
	float			hilightIntensity = 0.f;

	cgsnap = &cg_entities[cg.snap->ps.clientNum];

	shadow = qfalse;												// gjd added to make sure it was initialized
	shadowPlane = 0.0;												// ditto

	// if set to invisible, skip
	if( cent->currentState.eFlags & EF_NODRAW ) {
		return;
	}

	// the client number is stored in clientNum.  It can't be derived
	// from the entity number, because a single client may have
	// multiple corpses on the level using the same clientinfo
	clientNum = cent->currentState.clientNum;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		CG_Error( "Bad clientNum on player entity");
	}
	ci = &cgs.clientinfo[ clientNum ];

	// it is possible to see corpses from disconnected players that may
	// not have valid clientinfo
	if ( !ci->infoValid ) {
		return;
	}

	/* TC carriers select the special character independently of player class. */
	character = (cent->currentState.effect1Time & 0x60)
		? BG_GetCharacter(ci->team, 6) : CG_CharacterForClientinfo(ci, cent);

	if( cent->currentState.eFlags & EF_MOUNTEDTANK ) {
		VectorCopy( cg_entities[ cg_entities[ cent->currentState.clientNum ].tagParent ].mountedMG42Player.origin, playerOrigin );
	} else if( cent->currentState.eFlags & EF_MG42_ACTIVE || cent->currentState.eFlags & EF_AAGUN_ACTIVE ) { 	// Arnout: see if we're attached to a gun
		centity_t *mg42;
		int num;

		// find the mg42 we're attached to
		for ( num = 0 ; num < cg.snap->numEntities ; num++ ) {
			mg42 = &cg_entities[ cg.snap->entities[ num ].number ];
			if( mg42->currentState.eType == ET_MG42_BARREL &&
				mg42->currentState.otherEntityNum == cent->currentState.number ) {
				// found it, clamp behind gun
				vec3_t	forward, right, up;

				//AngleVectors (mg42->s.apos.trBase, forward, right, up);
				AngleVectors ( cent->lerpAngles, forward, right, up );
				VectorMA ( mg42->currentState.pos.trBase, -36, forward, playerOrigin );
				playerOrigin[2] = cent->lerpOrigin[2];
				break;
			}
		}

		if( num == cg.snap->numEntities ) {
			VectorCopy( cent->lerpOrigin, playerOrigin );
		}
	} else {
		VectorCopy( cent->lerpOrigin, playerOrigin );
	}

	memset( &body, 0, sizeof(body) );
	memset( &head, 0, sizeof(head) );
	memset( &acc, 0, sizeof(acc) );

	// get the rotation information
	CG_PlayerAngles( cent, body.axis, body.torsoAxis, head.axis );
	if (cent->currentState.eFlags & 0x01000000) {
		vec3_t angles, motion;
		vec3_t axis[3];
		float clock, dx, dy, speed, tilt;
		double phase, wave, dz;
		VectorCopy(cent->lerpAngles, angles);
		angles[YAW] = AngleMod(angles[YAW]);
		VectorCopy(cent->tceEntityMotion, motion);
		/* Win30054513 FILDs the original clientInfo pointer. Preserve that
		 * 32-bit numeric phase, not the new module's host/layout address. */
		phase = (double)(0x32580a10 + clientNum * 0x1c0);
		clock = (float)((double)cg.time * (double)0.0062831854447722435f);
		wave = (double)clock * 0.1875;
		dx = (float)(sin(phase * (double)1.492537260055542f + wave + 4.277999925613403) * 1.8600000000000003);
		dy = (float)(sin((double)clock * 0.3625 + 1.3739999532699585 - phase * (double)1.1235954761505127f + 0.4) * 0.82);
		dz = sin(phase * (double)0.9009009003639221f + wave + 7.456999778747559 - 0.8) * 1.3800000000000001;
		playerOrigin[0] += dx;
		playerOrigin[1] += dy;
		playerOrigin[2] = (float)((double)playerOrigin[2] + dz);
		angles[ROLL] += dx;
		angles[PITCH] += dy;
		angles[YAW] = (float)((double)angles[YAW] + dz);
		speed = VectorNormalize(motion);
		if (speed != 0.0f) {
			speed += speed;
			AnglesToAxis(angles, axis);
			tilt = DotProduct(axis[1], motion) * speed;
			if (tilt > 10.0f) tilt = 10.0f;
			else if (tilt < -10.0f) tilt = -10.0f;
			angles[ROLL] -= tilt;
			tilt = DotProduct(axis[0], motion) * speed;
			if (tilt > 10.0f) tilt = 10.0f;
			else if (tilt < -10.0f) tilt = -10.0f;
			angles[PITCH] = tilt + angles[PITCH] + tilt;
		}
		AnglesToAxis(angles, body.axis);
	}

	// FIXME: move this into CG_PlayerAngles
	if(	cgsnap == cent && (cg.snap->ps.pm_flags & PMF_LADDER) )
		memcpy( body.torsoAxis, body.axis, sizeof(body.torsoAxis) );

	// copy the torso rotation to the accessories
	AxisCopy( body.torsoAxis, acc.axis );
	if (tceSmokeNewBBox) {
		for (i = 0; i < 3; ++i) VectorScale(body.axis[i], 1.25f, body.axis[i]);
		body.nonNormalizedAxes = head.nonNormalizedAxes = acc.nonNormalizedAxes = qtrue;
	}

	// calculate client-side conditions
	CG_AnimPlayerConditions( character, cent );

	// get the animation state (after rotation, to allow feet shuffle)
	CG_PlayerAnimation( cent, &body );

	// forcibly set binoc animation
	if( cent->currentState.eFlags & EF_ZOOMING ) {
		usingBinocs = qtrue;
	}

	// add the any sprites hovering above the player
	// rain - corpses don't get icons (fireteam check ran out of bounds)
	if( cent->currentState.eType != ET_CORPSE ) {
		CG_PlayerSprites( cent );
	}

	// add a water splash if partially in and out of water
	CG_PlayerSplash( cent );

	// get the player model information
	renderfx = 0;
	if( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson ) {
		renderfx = RF_THIRD_PERSON;			// only draw in mirrors
	}

	// draw the player in cameras
	if( cg.cameraMode )
		renderfx &= ~RF_THIRD_PERSON;

	if( cg_shadows.integer == 3 && shadow ) {
		renderfx |= RF_SHADOW_PLANE;
	}

	renderfx |= RF_LIGHTING_ORIGIN;			// use the same origin for all

	// set renderfx for accessories
	acc.renderfx = renderfx;

	VectorCopy( playerOrigin, lightorigin );
	lightorigin[2] += 31;

	body.hilightIntensity = hilightIntensity;
	head.hilightIntensity = hilightIntensity;
	acc.hilightIntensity = hilightIntensity;

    /* Original CG_Player30054250 consumes deferred effects even when the
     * visibility controller prevents drawing the model on this frame. */
    if (!cent->tceVisible) {
        cent->tceEjectPending = 0;
        cent->tceFireEffectPending = 0;
        return;
    }

	//
	// add the body
	//
	if( cent->currentState.eType == ET_CORPSE && cent->currentState.time2 == 1 ) {
		body.hModel		= character->undressedCorpseModel;
		body.customSkin	= character->undressedCorpseSkin;
	} else if (cent->currentState.eFlags & 0x01000000) {
		body.hModel = trap_R_RegisterModel("models/players/vc100/vc100.md3");
	} else {
		body.customSkin	= character->skin;
		body.hModel		= character->mesh;
	}

	/* TC damage bits select the eight registered torso skins. */
	{
		int bits = cent->currentState.modelindex2, skin = -1;
		if (bits & 0x40) skin = (bits & 0x10) ? 7 : ((bits & 8) ? 6 : 5);
		else if (bits & 0x20) skin = (bits & 0x10) ? 4 : ((bits & 8) ? 3 : 2);
		else if (bits & 0x10) skin = 1;
		else if (bits & 8) skin = 0;
		if (skin >= 0) body.customSkin = character->bodyDamageSkins[skin];
	}

	VectorCopy( playerOrigin, body.origin );
	VectorCopy( lightorigin, body.lightingOrigin );
	body.shadowPlane = shadowPlane;
	body.renderfx = renderfx;
	VectorCopy( body.origin, body.oldorigin );	// don't positionally lerp at all

	cent->pe.bodyRefEnt = body;

	// if the model failed, allow the default nullmodel to be displayed
	// Gordon: whoever wrote that comment sucks
	if( !body.hModel ) {
		return;
	}

	// (SA) only need to set this once...
	VectorCopy( lightorigin, acc.lightingOrigin );

	CG_AddRefEntityWithPowerups( &body,	cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir );
	if (cent->currentState.eFlags & 0x01000000) {
		if (!(cent->currentState.eFlags & EF_DEAD)) CG_AddPlayerWeapon(&body, NULL, cent);
		return;
	}

	// ydnar debug
	#if 0
	{
		int		y;
		vec3_t	oldOrigin;
		
		VectorCopy( body.origin, oldOrigin );
		body.origin[ 0 ] -= 20;
		//body.origin[ 0 ] -= 20 * 36;
		for( y = 0; y < 40; y++ )
		{
			body.origin[ 0 ] += 1;
			//body.origin[ 0 ] += 36;
			//body.origin[ 2 ] = BG_GetGroundHeightAtPoint( body.origin ) + (oldOrigin[2] - BG_GetGroundHeightAtPoint( oldOrigin ));
			body.frame += (y & 1) ? 1 : -1;
			body.oldframe += (y & 1) ? -1 : 1;
			CG_AddRefEntityWithPowerups( &body,	cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir );
		}
		VectorCopy( oldOrigin, body.origin );
	}
	#endif
	
// DEBUG
	/*{
		int			x, zd, zu;
		vec3_t		bmins, bmaxs;

		x = (cent->currentState.solid & 255);
		zd = ((cent->currentState.solid>>8) & 255);
		zu = ((cent->currentState.solid>>16) & 255) - 32;

		bmins[0] = bmins[1] = -x;
		bmaxs[0] = bmaxs[1] = x;
		bmins[2] = -zd;
		bmaxs[2] = zu;

		VectorAdd( bmins, cent->lerpOrigin, bmins );
		VectorAdd( bmaxs, cent->lerpOrigin, bmaxs );

		CG_RailTrail( NULL, bmins, bmaxs, 1, 0 );
	}*/

	/*{
		orientation_t tag;
		int idx;
		vec3_t start;
		vec3_t ends[3];
		vec3_t axis[3];
		trap_R_LerpTag( &tag, &body, "tag_head", 0 );

		VectorCopy( body.origin, start );

		for( idx = 0; idx < 3; idx++ ) {
			VectorMA( start, tag.origin[idx], body.axis[idx], start );
		}

		MatrixMultiply( tag.axis, body.axis, axis );

		for( idx = 0; idx < 3; idx++ ) {
			VectorMA( start, 32, axis[idx], ends[idx] );
			CG_RailTrail2( NULL, start, ends[idx], 0 );
		}
	}
	{
		vec3_t mins, maxs;
		VectorCopy( cg.predictedPlayerState.mins, mins );
		VectorCopy( cg.predictedPlayerState.maxs, maxs );

		if( cg.predictedPlayerState.eFlags & EF_PRONE ) {
			maxs[2] = maxs[2] - (cg.predictedPlayerState.standViewHeight - PRONE_VIEWHEIGHT + 8);
		} else if( cg.predictedPlayerState.pm_flags & PMF_DUCKED ) {
			maxs[2] = cg.predictedPlayerState.crouchMaxZ;
		}

		VectorAdd( cent->lerpOrigin, mins, mins );
		VectorAdd( cent->lerpOrigin, maxs, maxs );
		CG_RailTrail( NULL, mins, maxs, 1, 0 );

		if( cg.predictedPlayerState.eFlags & EF_PRONE ) {
			vec3_t org, forward;

			// The legs
			VectorCopy( playerlegsProneMins, mins );
			VectorCopy( playerlegsProneMaxs, maxs );

			AngleVectors( cent->lerpAngles, forward, NULL, NULL );
			forward[2] = 0;
			VectorNormalizeFast( forward );

			org[0] = cent->lerpOrigin[0] + forward[0] * -32;
			org[1] = cent->lerpOrigin[1] + forward[1] * -32;
			org[2] = cent->lerpOrigin[2] + cg.pmext.proneLegsOffset;

			VectorAdd( org, mins, mins );
			VectorAdd( org, maxs, maxs );
			CG_RailTrail( NULL, mins, maxs, 1, 0 );

			// And the head
			VectorSet( mins, -6, -6, -22 );
			VectorSet( maxs, 6, 6, -10 );

			org[0] = cent->lerpOrigin[0] + forward[0] * 12;
			org[1] = cent->lerpOrigin[1] + forward[1] * 12;
			org[2] = cent->lerpOrigin[2];

			VectorAdd( org, mins, mins );
			VectorAdd( org, maxs, maxs );
			CG_RailTrail( NULL, mins, maxs, 1, 0 );
		}
	}*/
// DEBUG

	//
	// add the head
	//

	if( !(head.hModel = character->hudhead) ) {
		return;
	}
	head.customSkin = character->hudheadskin;
	if (cent->currentState.modelindex2 & 4) head.customSkin = character->headDamageSkins[1];
	else if (cent->currentState.modelindex2 & 2) head.customSkin = character->headDamageSkins[0];

	VectorCopy( lightorigin, head.lightingOrigin );

	CG_PositionRotatedEntityOnTag( &head, &body, "tag_head" );

	head.shadowPlane = shadowPlane;
	head.renderfx = renderfx;

	if( cent->currentState.eFlags & EF_FIRING ) {
		cent->pe.lastFiredWeaponTime = 0;
		cent->pe.weaponFireTime += cg.frametime;
	} else {
		if( cent->pe.weaponFireTime > 500 && cent->pe.weaponFireTime ) {
			cent->pe.lastFiredWeaponTime = cg.time;
		}

		cent->pe.weaponFireTime = 0;
	}

	if( cent->currentState.eType != ET_CORPSE && !(cent->currentState.eFlags & EF_DEAD) ) {
		hudHeadAnimNumber_t anim;

		if( cent->pe.weaponFireTime > 500 ) {
			anim = HD_ATTACK;
		} else if( cg.time - cent->pe.lastFiredWeaponTime < 500 ) {
			anim = HD_ATTACK_END;
		} else {
			anim = HD_IDLE1;
		}

		CG_HudHeadAnimation( character, &cent->pe.head, &head.oldframe, &head.frame, &head.backlerp, anim );
		/* TC's head.md3 has 401 animated frames, but its head_1.md3 LOD
		 * contains only frame zero. Keep animated heads on the base mesh;
		 * static corpse heads below may still use the inexpensive LOD. */
		if (head.frame != 0 || head.oldframe != 0) {
			head.renderfx |= RF_FORCENOLOD;
		}
	} else {
		head.frame = 0;
		head.oldframe = 0;
		head.backlerp = 0.f;
	}

	// set blinking flag
	CG_AddRefEntityWithPowerups( &head, cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir );

	cent->pe.headRefEnt = head;

	// add the 

	shadow = CG_PlayerShadow( cent, &shadowPlane );

	// set the shadowplane for accessories
	acc.shadowPlane = shadowPlane;

	/* The TC player controller has no SDK breath-puff call here. */

	//
	// add the gun / barrel / flash
	//
	if( !(cent->currentState.eFlags & EF_DEAD) /*&& !usingBinocs*/ ) {			// NERVE - SMF
		CG_AddPlayerWeapon( &body, NULL, cent );
	}

	//
	// add binoculars (if it's not the player)
	//
	if( usingBinocs ) {			// NERVE - SMF
		acc.hModel = cgs.media.thirdPersonBinocModel;
		CG_PositionEntityOnTag( &acc, &body, "tag_weapon", 0, NULL );
		CG_AddRefEntityWithPowerups( &acc, cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir );
	}

	/* TC equipment: effect2Time is the identity-mapped back weapon ID.
	 * Original weapon model address3484a640 is media+0x5c0: view0. */
	if (!(cent->currentState.eFlags & EF_DEAD) &&
		(cent->currentState.effect2Time || (cent->currentState.effect1Time & 0x10) ||
		 (cent->currentState.powerups & 0xc0))) {
		refEntity_t gear;
		int weapon = cent->currentState.effect2Time;
		acc.hModel = cgs.media.tceBackWeaponTagModel;
		CG_PositionEntityOnTag(&acc, &body, "tag_back", 0, NULL);
		CG_AddRefEntityWithPowerups(&acc, cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir);
		memset(&gear, 0, sizeof(gear));
		VectorCopy(lightorigin, gear.lightingOrigin);
		gear.shadowPlane = shadowPlane;
		gear.renderfx = renderfx;
		if (weapon > 0 && weapon < 64) {
			float scale = (weapon == 43 || weapon == 51 || weapon == 47 || weapon == 8 || weapon == 41 || weapon == 44) ? 1.1f : 1.0f;
			gear.hModel = tce_cg_weapons[weapon].weaponModel[0].model;
			gear.customSkin = tce_cg_weapons[weapon].weaponModel[0].skin[0];
			for (i = 0; i < 3; ++i) VectorScale(gear.axis[i], scale, gear.axis[i]);
			gear.nonNormalizedAxes = qtrue;
			CG_PositionEntityOnTag(&gear, &acc, "tag_backweap", 0, NULL);
			CG_AddRefEntityWithPowerups(&gear, cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir);
		}
		if (cent->currentState.effect1Time & 0x10) {
			gear.hModel = cgs.media.tceBackBombModel;
			CG_PositionEntityOnTag(&gear, &acc, "tag_backweap", 0, NULL);
			CG_AddRefEntityWithPowerups(&gear, cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir);
		}
		if (cent->currentState.powerups & 0x40) {
			gear.hModel = cgs.media.tceBackpackRedModel;
			CG_PositionEntityOnTag(&gear, &body, "tag_back", 0, NULL);
			CG_AddRefEntityWithPowerups(&gear, cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir);
		}
		if (cent->currentState.powerups & 0x80) {
			gear.hModel = cgs.media.tceBackpackBlueModel;
			CG_PositionEntityOnTag(&gear, &body, "tag_back", 0, NULL);
			CG_AddRefEntityWithPowerups(&gear, cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir);
		}
	}

	/* TC slots7/8 are leg tags; SDK mouth/rank interpretation is incorrect. */
	for (i = 0; i < ACC_MAX; ++i) {
		static const char *tags[13] = { "tag_bright", "tag_bleft", "tag_ubelt", "tag_back",
			"tag_weapon", "tag_weapon2", "tag_mouth", "tag_legright", "tag_legleft", NULL,
			"tag_chest", "tag_armleft", "tag_armright" };
		if (!character->accModels[i] || !tags[i] ||
			(cent->currentState.eType == ET_CORPSE && cent->currentState.time2 == 1) ||
			(i == 6 && (cent->currentState.eFlags & EF_HEADSHOT))) continue;
		acc.hModel = character->accModels[i];
		acc.customSkin = character->accSkins[i];
		CG_PositionEntityOnTag(&acc, i == 6 ? &head : &body, tags[i], 0, NULL);
		CG_AddRefEntityWithPowerups(&acc, cent->currentState.powerups, ci->team, &cent->currentState, cent->fireRiseDir);
	}
}

//=====================================================================

extern void CG_ClearWeapLerpFrame( clientInfo_t *ci, lerpFrame_t *lf, int animationNumber );

/*
===============
CG_ResetPlayerEntity

A player just came into view or teleported, so reset all animation info
===============
*/
void CG_ResetPlayerEntity( centity_t *cent ) {
	// Gordon: these are unused
//	cent->errorTime = -99999;		// guarantee no error decay added
//	cent->extrapolated = qfalse;	

	if (!(cent->currentState.eFlags & EF_DEAD)) {
		CG_ClearLerpFrameRate( cent, &cgs.clientinfo[ cent->currentState.clientNum ], &cent->pe.legs, cent->currentState.legsAnim );
		CG_ClearLerpFrame( cent, &cgs.clientinfo[ cent->currentState.clientNum ], &cent->pe.torso, cent->currentState.torsoAnim );

		memset( &cent->pe.legs, 0, sizeof( cent->pe.legs ) );
		cent->pe.legs.yawAngle = cent->rawAngles[YAW];
		cent->pe.legs.yawing = qfalse;
		cent->pe.legs.pitchAngle = 0;
		cent->pe.legs.pitching = qfalse;

		memset( &cent->pe.torso, 0, sizeof( cent->pe.legs ) );
		cent->pe.torso.yawAngle = cent->rawAngles[YAW];
		cent->pe.torso.yawing = qfalse;
		cent->pe.torso.pitchAngle = cent->rawAngles[PITCH];
		cent->pe.torso.pitching = qfalse;
	}

	TCE_BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin, qfalse, cent->currentState.effect2Time, 1.0f );
	TCE_BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles, qtrue, cent->currentState.effect2Time, 1.0f );

	VectorCopy( cent->lerpOrigin, cent->rawOrigin );
	VectorCopy( cent->lerpAngles, cent->rawAngles );

	if ( cg_debugPosition.integer ) {
		CG_Printf("%i ResetPlayerEntity yaw=%i\n", cent->currentState.number, cent->pe.torso.yawAngle );
	}

	cent->pe.painAnimLegs = -1;
	cent->pe.painAnimTorso = -1;
	cent->pe.animSpeed = 1.0;

}

void CG_GetBleedOrigin( vec3_t head_origin, vec3_t body_origin, int fleshEntityNum )
{
		clientInfo_t		*ci;
		refEntity_t			body;
		refEntity_t			head;
		centity_t			*cent, backupCent;
		bg_character_t		*character;

		ci = &cgs.clientinfo[ fleshEntityNum ];

		if ( !ci->infoValid ) {
			return;
		}

		character = CG_CharacterForClientinfo( ci, NULL );

		cent = &cg_entities [ fleshEntityNum ];
		backupCent = *cent;

		memset( &body, 0, sizeof(body) );
		memset( &head, 0, sizeof(head) );

		CG_PlayerAngles( cent, body.axis, body.torsoAxis, head.axis );
		CG_PlayerAnimation ( cent, &body );

		body.hModel = character->mesh;
		if( !body.hModel ) {
			return;
		}

		head.hModel = character->hudhead;
		if( !head.hModel ) {
			return;
		}

		VectorCopy( cent->lerpOrigin, body.origin );
		VectorCopy( body.origin, body.oldorigin );

		// Ridah, restore the cent so we don't interfere with animation timings
		*cent = backupCent;

		CG_PositionRotatedEntityOnTag( &head, &body, "tag_head");
	
		VectorCopy( head.origin, head_origin );
		VectorCopy( body.origin, body_origin );
}

/*
===============
CG_GetTag
===============
*/
qboolean CG_GetTag( int clientNum, char *tagname, orientation_t *or ) {
	clientInfo_t	*ci;
	centity_t		*cent;
	refEntity_t		*refent;
	vec3_t			tempAxis[3];
	vec3_t			org;
	int				i;

	ci = &cgs.clientinfo[ clientNum ];

	if( cg.snap && clientNum == cg.snap->ps.clientNum && cg.renderingThirdPerson ) {
		cent = &cg.predictedPlayerEntity;
	} else {
		cent = &cg_entities[ci->clientNum];
		if (!cent->currentValid)
			return qfalse;		// not currently in PVS
	}

	refent = &cent->pe.bodyRefEnt;

	if( trap_R_LerpTag( or, refent, tagname, 0 ) < 0 )
		return qfalse;

	VectorCopy( refent->origin, org );

	for( i = 0 ; i < 3 ; i++ ) {
		VectorMA( org, or->origin[i], refent->axis[i], org );
	}

	VectorCopy( org, or->origin );

	// rotate with entity
	MatrixMultiply( refent->axis, or->axis, tempAxis );
	memcpy( or->axis, tempAxis, sizeof(vec3_t) * 3 );

	return qtrue;
}

/*
===============
CG_GetWeaponTag
===============
*/
qboolean CG_GetWeaponTag( int clientNum, char *tagname, orientation_t *or ) {
	clientInfo_t	*ci;
	centity_t		*cent;
	refEntity_t		*refent;
	vec3_t			tempAxis[3];
	vec3_t			org;
	int				i;

	ci = &cgs.clientinfo[ clientNum ];

	if( cg.snap && clientNum == cg.snap->ps.clientNum && cg.renderingThirdPerson ) {
		cent = &cg.predictedPlayerEntity;
	} else {
		cent = &cg_entities[ci->clientNum];
		if (!cent->currentValid)
			return qfalse;		// not currently in PVS
	}

	if (cent->pe.gunRefEntFrame < cg.clientFrame - 1)
		return qfalse;

	refent = &cent->pe.gunRefEnt;

	if (trap_R_LerpTag( or, refent, tagname, 0 ) < 0)
		return qfalse;

	VectorCopy( refent->origin, org );

	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( org, or->origin[i], refent->axis[i], org );
	}

	VectorCopy( org, or->origin );

	// rotate with entity
	MatrixMultiply( refent->axis, or->axis, tempAxis );
	memcpy( or->axis, tempAxis, sizeof(vec3_t) * 3 );

	return qtrue;
}

// =============
// Menu Versions
// =============

/*
==================
CG_SwingAngles_Limbo
==================
*/
static void CG_SwingAngles_Limbo( float destination, float swingTolerance, float clampTolerance,
					float speed, float *angle, qboolean *swinging ) {
	float	swing;
	float	move;
	float	scale;

	if ( !*swinging ) {
		// see if a swing should be started
		swing = AngleSubtract( *angle, destination );
		if ( swing > swingTolerance || swing < -swingTolerance ) {
			*swinging = qtrue;
		}
	}

	if ( !*swinging ) {
		return;
	}
	
	// modify the speed depending on the delta
	// so it doesn't seem so linear
	swing = AngleSubtract( destination, *angle );
	scale = fabs( swing );
	if ( scale < swingTolerance * 0.5 ) {
		scale = 0.5;
	} else if ( scale < swingTolerance ) {
		scale = 1.0;
	} else {
		scale = 2.0;
	}

	// swing towards the destination angle
	if ( swing >= 0 ) {
		move = cg.frametime * scale * speed;
		if ( move >= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	} else if ( swing < 0 ) {
		move = cg.frametime * scale * -speed;
		if ( move <= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	}

	// clamp to no more than tolerance
	swing = AngleSubtract( destination, *angle );
	if ( swing > clampTolerance ) {
		*angle = AngleMod( destination - (clampTolerance - 1) );
	} else if ( swing < -clampTolerance ) {
		*angle = AngleMod( destination + (clampTolerance - 1) );
	}
}

/*
===============
CG_PlayerAngles_Limbo
===============
*/
void CG_PlayerAngles_Limbo( playerInfo_t *pi, vec3_t legs[3], vec3_t torso[3], vec3_t head[3] ) {
	vec3_t		legsAngles, torsoAngles, headAngles;
	float		dest;

	VectorCopy( pi->viewAngles, headAngles );
	headAngles[YAW] = AngleMod( headAngles[YAW] );
	VectorClear( legsAngles );
	VectorClear( torsoAngles );

	torsoAngles[YAW] = 180;
	legsAngles[YAW] = 180;
	headAngles[YAW] = 180;

	// --------- pitch -------------

	// only show a fraction of the pitch angle in the torso
	if ( headAngles[PITCH] > 180 ) {
		dest = (-360 + headAngles[PITCH]) * 0.75;
	} else {
		dest = headAngles[PITCH] * 0.75;
	}
	CG_SwingAngles_Limbo( dest, 15, 30, 0.1, &pi->torso.pitchAngle, &pi->torso.pitching );
	torsoAngles[PITCH] = pi->torso.pitchAngle;

	// pull the angles back out of the hierarchial chain
	AnglesSubtract( headAngles, torsoAngles, headAngles );
	AnglesSubtract( torsoAngles, legsAngles, torsoAngles );

	AnglesSubtract( legsAngles, pi->moveAngles, legsAngles );		// NERVE - SMF

	AnglesToAxis( legsAngles, legs );
	AnglesToAxis( torsoAngles, torso );
	AnglesToAxis( headAngles, head );
}

animation_t *CG_GetLimboAnimation( playerInfo_t *pi, const char *name ) {
	int				i;
	bg_character_t	*character = BG_GetCharacter( pi->teamNum, pi->classNum );

	if( !character ) {
		return NULL;
	}

	for( i = 0; i < character->animModelInfo->numAnimations; i++ ) {
		if ( !Q_stricmp( character->animModelInfo->animations[i]->name, name ) ) {
			return character->animModelInfo->animations[i];
		}
	}

	return character->animModelInfo->animations[0];	// safe fallback so we never end up without an animation (which will crash the game)
}

int CG_GetSelectedWeapon( void ) {
	return 0;
}

void CG_DrawPlayer_Limbo( float x, float y, float w, float h, playerInfo_t *pi, int time, clientInfo_t *ci, qboolean animatedHead) {
	refdef_t		refdef;
	refEntity_t		body;
	refEntity_t		head;
	refEntity_t		gun;
	refEntity_t		barrel;
	refEntity_t		acc;
	vec3_t			origin;
	int				renderfx;
	vec3_t			mins = {-16, -16, -24};
	vec3_t			maxs = {16, 16, 32};
	float			len;
//	float			xx;
	vec4_t			hcolor = { 1, 0, 0, 0.5 };
	bg_character_t	*character = BG_GetCharacter( pi->teamNum, pi->classNum );
	int				i;

	dp_realtime = time;

	CG_AdjustFrom640( &x, &y, &w, &h );
	y -= jumpHeight;

	memset( &refdef, 0, sizeof( refdef ) );
	memset( &body, 0, sizeof(body) );
	memset( &head, 0, sizeof(head) );
	memset( &acc, 0, sizeof(acc) );

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

/*	refdef.fov_x = (int)((float)refdef.width / 640.0f * 90.0f);
	xx = refdef.width / tan( refdef.fov_x / 360 * M_PI );
	refdef.fov_y = atan2( refdef.height, xx );
	refdef.fov_y *= ( 360 / M_PI );*/

	refdef.fov_x = 35;
	refdef.fov_y = 35;

	// calculate distance so the player nearly fills the box

	// START Mad Doc - TDF
	// for "talking heads", we calc the origin differently
	// FIXME - this is stupid - should all be character driven - NO CODE HACKS FOR SPECIFIC THINGS THAT CAN BE DONE CLEANLY
	if (animatedHead == qfalse)
	{
	// END Mad Doc - TDF	
		len = 0.9 * ( maxs[2] - mins[2] );							// NERVE - SMF - changed from 0.7
		origin[0] = pi->y -70 + (len / tan( DEG2RAD(refdef.fov_x) * 0.5 ));
		origin[1] = 0.5 * ( mins[1] + maxs[1] );
		origin[2] = pi->z -23 + (-0.5 * ( mins[2] + maxs[2] ));
	}
	else
	{
		// for "talking head" animations, we want to center just below the face
		// we precalculated this elsewhere
		VectorCopy(pi->headOrigin, origin);
	}
	// END Mad Doc - TDF

	refdef.time = dp_realtime;

	trap_R_SetColor( hcolor );
	trap_R_ClearScene();
	trap_R_SetColor( NULL );

	// get the rotation information
	CG_PlayerAngles_Limbo( pi, body.axis, body.torsoAxis, head.axis );
	
	renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;

	acc.renderfx = renderfx;
 

	//
	// add the body
	//

	body.hModel = character->mesh;
	body.customSkin = character->skin;

	body.renderfx = renderfx;

	VectorCopy( origin, body.origin );
	VectorCopy( origin, body.lightingOrigin );
	VectorCopy( body.origin, body.oldorigin );

	if( cg.time >= pi->torso.frameTime ) {
		pi->torso.oldFrameTime = pi->torso.frameTime;
		pi->torso.oldFrame = pi->torso.frame;
		pi->torso.oldFrameModel = pi->torso.frameModel = pi->torso.animation->mdxFile;

		while( cg.time >= pi->torso.frameTime ) {
			pi->torso.frameTime += (pi->torso.animation->duration / (float)(pi->torso.animation->numFrames));
			pi->torso.frame++;

			if( pi->torso.frame >= pi->torso.animation->firstFrame + pi->torso.animation->numFrames ) {
				pi->torso.frame = pi->torso.animation->firstFrame;
			}
		}
	}

	if( pi->torso.frameTime == pi->torso.oldFrameTime ) {
		pi->torso.backlerp = 0;
	} else {
		pi->torso.backlerp = 1.0 - (float)( cg.time - pi->torso.oldFrameTime ) / ( pi->torso.frameTime - pi->torso.oldFrameTime );
	}

	if( cg.time >= pi->legs.frameTime ) {
		pi->legs.oldFrameTime = pi->legs.frameTime;
		pi->legs.oldFrame = pi->legs.frame;
		pi->legs.oldFrameModel = pi->legs.frameModel = pi->legs.animation->mdxFile;

		while( cg.time >= pi->legs.frameTime ) {
			pi->legs.frameTime += (pi->legs.animation->duration / (float)(pi->legs.animation->numFrames));
			pi->legs.frame++;

			if( pi->legs.frame >= pi->legs.animation->firstFrame + pi->legs.animation->numFrames ) {
				pi->legs.frame = pi->legs.animation->firstFrame;
			}
		}
	}

	if( pi->legs.frameTime == pi->legs.oldFrameTime ) {
		pi->legs.backlerp = 0;
	} else {
		pi->legs.backlerp = 1.0 - (float)( cg.time - pi->legs.oldFrameTime ) / ( pi->legs.frameTime - pi->legs.oldFrameTime );
	}

	body.oldTorsoFrame = pi->torso.oldFrame;
	body.torsoFrame = pi->torso.frame;
	body.torsoBacklerp = pi->torso.backlerp;
	body.torsoFrameModel = pi->torso.frameModel;
	body.oldTorsoFrameModel = pi->torso.oldFrameModel;

	body.oldframe = pi->legs.oldFrame;
	body.frame = pi->legs.frame;
	body.backlerp = pi->legs.backlerp;
	body.frameModel = pi->legs.frameModel;
	body.oldframeModel = pi->legs.oldFrameModel;

	trap_R_AddRefEntityToScene( &body );

	//
	// add the head
	//

	head.hModel = character->hudhead;
	if( !head.hModel ) {
		return;
	}
	head.customSkin = character->headSkin;

	VectorCopy( origin, head.lightingOrigin );

	CG_PositionRotatedEntityOnTag( &head, &body, "tag_head" );

	head.renderfx = renderfx;

	head.frame = 0;
	head.oldframe = 0;
	head.backlerp = 0.f;

	trap_R_AddRefEntityToScene( &head );

	AxisCopy( body.torsoAxis, acc.axis );
	VectorCopy( origin, acc.lightingOrigin );


	for( i = ACC_BELT_LEFT; i < ACC_MAX; i++ ) {
		if( !(character->accModels[i]) )
			continue;
		acc.hModel = character->accModels[i];
		acc.customSkin = character->accSkins[i];

		switch(i) {
			case ACC_BELT_LEFT:
				CG_PositionEntityOnTag( &acc,	&body,	"tag_bright", 0, NULL);
				break;
			case ACC_BELT_RIGHT:
				CG_PositionEntityOnTag( &acc,	&body,	"tag_bleft", 0, NULL);
				break;

			case ACC_BELT:
				CG_PositionEntityOnTag( &acc,	&body,	"tag_ubelt", 0, NULL);
				break;
			case ACC_BACK:
				CG_PositionEntityOnTag( &acc,	&body,	"tag_back", 0, NULL);
				break;
			
			case ACC_HAT:			// hat
			case ACC_MOUTH2:		// hat2
			case ACC_MOUTH3:		// hat3
				CG_PositionEntityOnTag( &acc,	&head,	"tag_mouth", 0, NULL);
				break;
			
			// weapon and weapon2
			// these are used by characters who have permanent weapons attached to their character in the skin
			case ACC_WEAPON:	// weap
				CG_PositionEntityOnTag( &acc,	&body,	"tag_weapon", 0, NULL);
				break;
			case ACC_WEAPON2:	// weap2
				CG_PositionEntityOnTag( &acc,	&body,	"tag_weapon2", 0, NULL);
				break;

			default:
				continue;
		}

		trap_R_AddRefEntityToScene( &acc );
	}

	//
	// add the gun
	//
	{
		int weap = CG_GetSelectedWeapon();

		memset( &gun, 0, sizeof(gun) );
		memset( &barrel, 0, sizeof(barrel) );

		gun.hModel = cg_weapons[weap].weaponModel[W_TP_MODEL].model;

		VectorCopy( origin, gun.lightingOrigin );
		CG_PositionEntityOnTag( &gun, &body, "tag_weapon", 0, NULL );
		gun.renderfx = renderfx;
		trap_R_AddRefEntityToScene( &gun );
	}

	// Save out the old render info so we don't kill the LOD system here
	trap_R_SaveViewParms();

	//
	// add an accent light
	//
	origin[0] -= 100;	// + = behind, - = in front
	origin[1] += 100;	// + = left, - = right
	origin[2] += 100;	// + = above, - = below
	//%	trap_R_AddLightToScene( origin, 1000, 1.0, 1.0, 1.0, 0 );
	trap_R_AddLightToScene( origin, 1000, 1.0, 1.0, 1.0, 1.0, 0, 0 );

	origin[0] -= 100;
	origin[1] -= 100;
	origin[2] -= 100;
	//%	trap_R_AddLightToScene( origin, 1000, 1.0, 1.0, 1.0, 0 );
	trap_R_AddLightToScene( origin, 1000, 1.0, 1.0, 1.0, 1.0, 0, 0 );

	trap_R_RenderScene( &refdef );

	// Reset the view parameters
	trap_R_RestoreViewParms();
}

/* Original TC table at cgame 300afd68; live weapon names come from gear media. */
weaponType_t weaponTypes[] = {
    { 3, "MICRO UZI", "IMI Micro Uzi 9x19mm", "Machine Pistol" },
    { 8, "MP5/10", "H&K Mp5/10 .40 S&W", "Submachine Gun" },
    { 10, "MAC 10", "Ingrim Mac 10 .45", "Machine Pistol" },
    { 9, "M26 FRAG", NULL, NULL },
    { 4, "XM84 STUN", NULL, NULL },
    { 30, "M83 SMOKE", NULL, NULL },
    { 1, "KNIFE", NULL, NULL },
    { 25, "M1 GARAND", NULL, NULL },
    { 31, "MOBILE MG42", NULL, NULL },
    { 32, "K43", NULL, NULL },
    { 35, "MORTAR", NULL, NULL },
    { 7, "HARDBALLER", "AMT 1911 Hardballer .45", "Automatic Pistol" },
    { 2, "BERETTA", "Beretta 92 9x19mm", "Automatic Pistol" },
    { 37, "AKIMBO", "2x Glock 19 9x19mm", "Dual Automatic Pistol" },
    { 38, "AKIMBO", "2x Beretta 92 9x19mm", "Dual Automatic Pistol" },
    { 52, "COLT", NULL, NULL },
    { 14, "LUGER", NULL, NULL },
    { 53, "AKIMBO COLTS", NULL, NULL },
    { 54, "AKIMBO LUGERS", NULL, NULL },
    { 39, "GLOCK", "Glock 19 9mmx19mm", "Automatic Pistol" },
    { 40, "DESERT EAGLE", "IMI Desert Eagle .50 AE", "Automatic Pistol" },
    { 48, "M3S90", "Benelli M3 Super 90", "Shotgun" },
    { 51, "M76", "Zastava M76 7.92x57mm", "Sniper Rifle" },
    { 44, "M4", "Colt M4 5.56x45mm", "Assault Rifle" },
    { 45, "AKSU-74", "Izhmash AkSU-74 5.45x25mm", "Submachine Gun" },
    { 33, "AKSU-74 SD", "Izhmash AkSU-74 BG 150", "Suppressed Submachine Gun" },
    { 43, "AK-47", "Izhmash Ak-47 7.62x39mm", "Assault Rifle" },
    { 49, "M590C", "Mossberg M590 Compact", "Assault Shotgun" },
    { 50, "G3", "H&K G3A4 7.62x51mm", "Assault Rifle" },
    { 41, "MP5SD", "H&K Mp5 SD 9x19mm", "Suppressed Submachine Gun" },
    { 42, "UMP45", "H&K UMP-45 .45", "Submachine Gun" },
    { 47, "PSG-1", "H&K PSG-1 7.62x51mm", "Sniper Rifle" },
    { 46, "R93", "Blaser R93 .338 Lapua", "Bolt Action Sniper Rifle" },
    { 36, "TECHKIT", "No grenades", "Faster objective ability" },
    { 19, "GRENADES", "M83 Smoke", "Defensive grenade loadout" },
    { 20, "GRENADES", "Mk3A2 Concussion", "Offensive grenade loadout" },
    { 21, "GRENADES", "M83 Smoke, XM84 Stun", "Tactical grenade loadout" },
    { 0, NULL, NULL, NULL },
    { -1, NULL, NULL, NULL },
};

weaponType_t* WM_FindWeaponTypeForWeapon ( weapon_t weapon ) {
	weaponType_t* w = weaponTypes;

	if(!weapon) {
		return NULL;
	}

	while(w->weapindex != -1) {
		if(w->weapindex == weapon) {
			return w;
		}
		w++;
	}
	return NULL;
}

void WM_RegisterWeaponTypeShaders () {
	weaponType_t* w = weaponTypes;

	while(w->weapindex) {	
//		w->shaderHandle = trap_R_RegisterShaderNoMip( w->shader );
		w++;
	}
}

void CG_MenuSetAnimation( playerInfo_t *pi, const char* legsAnim, const char* torsoAnim, qboolean force, qboolean clearpending ) {
	lastLegsAnim = pi->legs.animation = CG_GetLimboAnimation(pi, legsAnim);
	lastTorsoAnim = pi->torso.animation = CG_GetLimboAnimation(pi, torsoAnim);

	if( force ) {
		pi->legs.oldFrame = pi->legs.frame = pi->legs.animation->firstFrame;
		pi->torso.oldFrame = pi->torso.frame = pi->torso.animation->firstFrame;
		
		pi->legs.frameTime = cg.time;
		pi->torso.frameTime = cg.time;

		pi->legs.oldFrameModel = pi->legs.frameModel = pi->legs.animation->mdxFile;
		pi->torso.oldFrameModel = pi->torso.frameModel = pi->torso.animation->mdxFile;

		pi->numPendingAnimations = 0;
	} else {
		pi->legs.oldFrame = pi->legs.frame;
		pi->legs.oldFrameModel = pi->legs.frameModel;
		pi->legs.frame = pi->legs.animation->firstFrame;
		pi->torso.oldFrame = pi->torso.frame;
		pi->torso.oldFrameModel = pi->torso.frameModel;
		pi->torso.frame = pi->torso.animation->firstFrame;

		pi->legs.frameTime += 200; // Give them some time to lerp between animations
		pi->torso.frameTime += 200;
	}

	if( clearpending ) {
		pi->numPendingAnimations = 0;
	}
}

void CG_MenuPendingAnimation( playerInfo_t *pi, const char* legsAnim, const char* torsoAnim, int delay ) {
	if( pi->numPendingAnimations >= 4 ) {
		return;
	}

	if( !pi->numPendingAnimations ) {
		pi->pendingAnimations[pi->numPendingAnimations].pendingAnimationTime =	cg.time + delay;
	} else {
		pi->pendingAnimations[pi->numPendingAnimations].pendingAnimationTime =	pi->pendingAnimations[pi->numPendingAnimations-1].pendingAnimationTime + delay;
	}
	pi->pendingAnimations[pi->numPendingAnimations].pendingLegsAnim =	legsAnim;
	pi->pendingAnimations[pi->numPendingAnimations].pendingTorsoAnim =	torsoAnim;

	lastLegsAnim = CG_GetLimboAnimation(pi, legsAnim);
	lastTorsoAnim = CG_GetLimboAnimation(pi, torsoAnim);
	pi->numPendingAnimations++;
}

void CG_MenuCheckPendingAnimation( playerInfo_t *pi ) {
	int i;

	if(pi->numPendingAnimations <= 0) {
		return;
	}

	if(pi->pendingAnimations[0].pendingAnimationTime && pi->pendingAnimations[0].pendingAnimationTime < cg.time) {
		CG_MenuSetAnimation(pi, pi->pendingAnimations[0].pendingLegsAnim, pi->pendingAnimations[0].pendingTorsoAnim, qfalse, qfalse);

		for(i = 0; i < 3; i++ ) {
			memcpy( &pi->pendingAnimations[i], &pi->pendingAnimations[i+1], sizeof(pendingAnimation_t));
		}

		pi->numPendingAnimations--;
	}
}


void CG_SetHudHeadLerpFrameAnimation( bg_character_t* ch, lerpFrame_t *lf, int newAnimation ) {
	animation_t	*anim;

	lf->animationNumber = newAnimation;
	newAnimation &= ~ANIM_TOGGLEBIT;

	if ( newAnimation < 0 || newAnimation >= MAX_HD_ANIMATIONS ) {
		CG_Error( "Bad animation number (CG_SetHudHeadLerpFrameAnimation): %i", newAnimation );
	}

	anim = &ch->hudheadanimations[ newAnimation ];

	lf->animation		= anim;
	lf->animationTime	= lf->frameTime + anim->initialLerp;
}

void CG_ClearHudHeadLerpFrame( bg_character_t* ch, lerpFrame_t *lf, int animationNumber )
{
	lf->frameTime = lf->oldFrameTime = cg.time;
	CG_SetHudHeadLerpFrameAnimation( ch, lf, animationNumber );
	lf->oldFrame = lf->frame = lf->animation->firstFrame;
	lf->oldFrameModel = lf->frameModel = lf->animation->mdxFile;
}

void CG_RunHudHeadLerpFrame( bg_character_t* ch, lerpFrame_t *lf, int newAnimation, float speedScale ) {
	int			f;
	animation_t	*anim;



	// see if the animation sequence is switching
	if( !lf->animation ) {
		CG_ClearHudHeadLerpFrame( ch, lf, newAnimation );
	} else if( newAnimation != lf->animationNumber ) {
		CG_SetHudHeadLerpFrameAnimation( ch, lf, newAnimation );
	}

	// if we have passed the current frame, move it to
	// oldFrame and calculate a new frame
	if( cg.time >= lf->frameTime ) {
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;
		lf->oldFrameModel = lf->frameModel;

		// get the next frame based on the animation
		anim = lf->animation;
		if ( !anim->frameLerp ) {
			return;		// shouldn't happen
		}
		if ( cg.time < lf->animationTime ) {
			lf->frameTime = lf->animationTime;		// initial lerp
		} else {
			lf->frameTime = lf->oldFrameTime + anim->frameLerp;
		}
		f = ( lf->frameTime - lf->animationTime ) / anim->frameLerp;
		f *= speedScale;		// adjust for haste, etc
		if ( f >= anim->numFrames ) {
			f -= anim->numFrames;
			if ( anim->loopFrames ) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = anim->numFrames - 1;
				// the animation is stuck at the end, so it
				// can immediately transition to another sequence
				lf->frameTime = cg.time;
			}
		}
		lf->frame = anim->firstFrame + f;
		lf->frameModel = anim->mdxFile;
		if ( cg.time > lf->frameTime ) {
			lf->frameTime = cg.time;
		}
	}

	if ( lf->frameTime > cg.time + 200 ) {
		lf->frameTime = cg.time;
	}

	if ( lf->oldFrameTime > cg.time ) {
		lf->oldFrameTime = cg.time;
	}
	// calculate current lerp value
	if ( lf->frameTime == lf->oldFrameTime ) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = 1.0 - (float)( cg.time - lf->oldFrameTime ) / ( lf->frameTime - lf->oldFrameTime );
	}
}

void CG_HudHeadAnimation( bg_character_t* ch, lerpFrame_t* lf, int *oldframe, int *frame, float *backlerp, hudHeadAnimNumber_t animation ) {
//	centity_t *cent = &cg.predictedPlayerEntity;

	CG_RunHudHeadLerpFrame( ch, lf, (int)animation, 1.f );

	*oldframe		= lf->oldFrame;
	*frame			= lf->frame;
	*backlerp		= lf->backlerp;
}
