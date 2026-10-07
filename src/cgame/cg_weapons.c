/*
 * name:		cg_weapons.c
 *
 * desc:		events and effects dealing with weapons
 *
*/

#include "cg_local.h"
#include "tce_lightgrid.h"
#include "tce_weapon_frames.h"
#include "tce_weapon_media.h"
#include "tce_weapon_offset.h"
#include "tce_weapon_position.h"
#include "tce_flash.h"
#include "tce_fragment_sound.h"
#include "tce_smoke_grenade.h"
#include "../game/tce_bg.h"
#include "../game/tce_trajectory.h"
#include "tce_weapon_effects.h"
#include "tce_eject_brass.h"
#include "tce_eject_request.h"
#include "tce_weapon_recoil.h"
#include "tce_weapon_cycle.h"
#include "tce_impact_particles.h"
#include "../game/bg_classes.h"

vec3_t	ejectBrassCasingOrigin;

//----(SA)	
// forward decs
static int getAltWeapon( int weapnum );
int getEquivWeapon( int weapnum );
int	CG_WeaponIndex( int weapnum, int *bank, int *cycle);
static qboolean CG_WeaponHasAmmo( int i );
char cg_fxflags;
extern int weapBanksMultiPlayer[MAX_WEAP_BANKS_MP][MAX_WEAPS_IN_BANK_MP]; // JPW NERVE moved to bg_misc.c so I can get a droplist
// jpw

//----(SA)	end

/* Weapon animation transitions are verified in tce_weapon_frames.c. */

//----(SA)	added

/*
==============
CG_SpearTrail
	simple bubble trail behind a missile
==============
*/
/*void CG_SpearTrail(centity_t *ent, const weaponInfo_t *wi )
{
	int		contents, lastContents;
	vec3_t	origin, lastPos;
	entityState_t	*es;

	es = &ent->currentState;
	BG_EvaluateTrajectory( &es->pos, cg.time, origin );
	contents = CG_PointContents( origin, -1 );

	BG_EvaluateTrajectory( &es->pos, ent->trailTime, lastPos );
	lastContents = CG_PointContents( lastPos, -1 );

	ent->trailTime = cg.time;

	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		if ( contents & lastContents & CONTENTS_WATER ) {
			CG_BubbleTrail( lastPos, origin, 1, 8 );
		}
	}
}*/




// Ridah, new trail effects









/*
==========================
CG_NailgunEjectBrass
==========================
*/
/*
// TTimo: defined but not used
static void CG_NailgunEjectBrass( centity_t *cent ) {
	localEntity_t	*smoke;
	vec3_t			origin;
	vec3_t			v[3];
	vec3_t			offset;
	vec3_t			xoffset;
	vec3_t			up;

	AnglesToAxis( cent->lerpAngles, v );

	offset[0] = 0;
	offset[1] = -12;
	offset[2] = 24;

	xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
	xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
	xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
	VectorAdd( cent->lerpOrigin, xoffset, origin );

	VectorSet( up, 0, 0, 64 );

	smoke = CG_SmokePuff( origin, up, 32, 1, 1, 1, 0.33f, 700, cg.time, 0, 0, cgs.media.smokePuffShader );
	// use the optimized local entity add
	smoke->leType = LE_SCALE_FADE;
}
*/

/*
==========================
CG_RailTrail
	SA: re-inserted this as a debug mechanism for bullets
==========================
*/
void CG_RailTrail2( clientInfo_t *ci, vec3_t start, vec3_t end, int highlighted) {
	localEntity_t	*le;
	refEntity_t		*re;

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	le->leType = LE_FADE_RGB;
	le->startTime = cg.time;
	le->endTime = (int)((double)cg.time + cg_railTrailTime.value);
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	re->shaderTime = (float)((double)(float)cg.time * 0.001f);
	re->reType = RT_RAIL_CORE;
	re->customShader = cgs.media.railCoreShader;

	VectorCopy( start, re->origin );
	VectorCopy( end, re->oldorigin );

    le->color[0] = highlighted ? 0.0f : 1.0f;
    le->color[1] = highlighted ? 1.0f : 0.0f;
    le->color[2] = 0.0f;
	le->color[3] = 1.0f;

	AxisClear( re->axis );
}

//void CG_RailTrailBox( clientInfo_t *ci, vec3_t start, vec3_t end) {
/*
==============
CG_RailTrail
	modified so we could draw boxes for debugging as well
==============
*/
void CG_RailTrail( clientInfo_t *ci, vec3_t start, vec3_t end, int type, int highlighted) {	//----(SA)	added 'type'
	vec3_t	diff, v1, v2, v3, v4, v5, v6;

	if(!type) {	// just a line
		CG_RailTrail2( ci, start, end, highlighted);
		return;
	}

	// type '1' (box)

	VectorSubtract(start, end, diff);

	VectorCopy(start, v1);
	VectorCopy(start, v2);
	VectorCopy(start, v3);
	v1[0] -= diff[0];
	v2[1] -= diff[1];
	v3[2] -= diff[2];
	CG_RailTrail2( ci, start, v1, highlighted);
	CG_RailTrail2( ci, start, v2, highlighted);
	CG_RailTrail2( ci, start, v3, highlighted);

	VectorCopy(end, v4);
	VectorCopy(end, v5);
	VectorCopy(end, v6);
	v4[0] += diff[0];
	v5[1] += diff[1];
	v6[2] += diff[2];
	CG_RailTrail2( ci, end, v4, highlighted);
	CG_RailTrail2( ci, end, v5, highlighted);
	CG_RailTrail2( ci, end, v6, highlighted);

	CG_RailTrail2( ci, v2, v6, highlighted);
	CG_RailTrail2( ci, v6, v1, highlighted);
	CG_RailTrail2( ci, v1, v5, highlighted);

	CG_RailTrail2( ci, v2, v4, highlighted);
	CG_RailTrail2( ci, v4, v3, highlighted);
	CG_RailTrail2( ci, v3, v5, highlighted);

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/*
======================
CG_ParseWeaponConfig
	read information for weapon animations (first/length/fps)
======================
*/
static qboolean	CG_ParseWeaponConfig( const char *filename, weaponInfo_t *wi ) {
	char		*text_p, *prev;
	int			len;
	int			i;
	float		fps;
	char		*token;
	qboolean	newfmt = qfalse;	//----(SA)	
	char		text[20000];
	fileHandle_t	f;

	// load the file
	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		return qfalse;
	}

	if ( len >= sizeof( text ) - 1 ) {
		CG_Printf( "File %s too long\n", filename );
		return qfalse;
	}

	trap_FS_Read( text, len, f );
	text[len] = 0;
	trap_FS_FCloseFile( f );

	// parse the text
	text_p = text;

	// read optional parameters
	while ( 1 ) {
		prev = text_p;	// so we can unget
		token = COM_Parse( &text_p );
		if ( !token ) {						// get the variable
			break;
		}
/*		if ( !Q_stricmp( token, "whatever_variable" ) ) {
			token = COM_Parse( &text_p );	// get the value
			if ( !token ) {
				break;
			}
			continue;
		}*/

		if ( !Q_stricmp( token, "newfmt" ) ) {
			newfmt = qtrue;
			continue;
		}

		// if it is a number, start parsing animations
		if ( token[0] >= '0' && token[0] <= '9' ) {
			text_p = prev;	// unget the token
			break;
		}
		Com_Printf( "unknown token in weapon cfg '%s' is %s\n", token, filename );
	}


	for ( i = 0 ; i < MAX_WP_ANIMATIONS  ; i++ ) {

		token = COM_Parse( &text_p );	// first frame
		if ( !token ) break;
		wi->weapAnimations[i].firstFrame = atoi( token );

		token = COM_Parse( &text_p );	// length
		if ( !token ) break;
		wi->weapAnimations[i].numFrames = atoi( token );

		token = COM_Parse( &text_p );	// fps
		if ( !token ) break;
		fps = atof( token );
		if ( fps == 0 ) {
			fps = 1;
		}

		wi->weapAnimations[i].frameLerp = 1000 / fps;
		wi->weapAnimations[i].initialLerp = 1000 / fps;

		token = COM_Parse( &text_p );	// looping frames
		if ( !token ) break;
		wi->weapAnimations[i].loopFrames = atoi( token );
		if(wi->weapAnimations[i].loopFrames > wi->weapAnimations[i].numFrames)
			wi->weapAnimations[i].loopFrames = wi->weapAnimations[i].numFrames;
		else if(wi->weapAnimations[i].loopFrames < 0)
			wi->weapAnimations[i].loopFrames = 0;


		// store animation/draw bits in '.moveSpeed'

		wi->weapAnimations[i].moveSpeed = 0;

		if(newfmt) {
			token = COM_Parse( &text_p );	// barrel anim bits
			if ( !token ) break;
			wi->weapAnimations[i].moveSpeed = atoi(token);

			token = COM_Parse( &text_p );	// animated weapon
			if ( !token ) break;
			if(atoi(token))
				wi->weapAnimations[i].moveSpeed |= (1<<W_MAX_PARTS);	// set the bit one higher than can be set by the barrel bits

			token = COM_Parse( &text_p );	// barrel hide bits (so objects can be flagged to not be drawn during all sequences (a reloading hand that comes in from off screen for that one animation for example)
			if ( !token ) break;
			wi->weapAnimations[i].moveSpeed |= ((atoi(token))<<8 );	// use 2nd byte for draw bits
		}

	}

	if ( i != MAX_WP_ANIMATIONS ) {
		CG_Printf( "Error parsing weapon animation file: %s", filename );
		return qfalse;
	}


	return qtrue;
}


static qboolean CG_RW_ParseError( int handle, char *format, ... )
{
	int line;
	char filename[128];
	va_list argptr;
	static char string[4096];

	va_start( argptr, format );
	Q_vsnprintf( string, sizeof(string), format, argptr );
	va_end( argptr );

	filename[0] = '\0';
	line = 0;
	trap_PC_SourceFileAndLine( handle, filename, &line );

	Com_Printf( S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string );

	trap_PC_FreeSource( handle );

	return qfalse;
}

static qboolean CG_RW_ParseWeaponLinkPart( int handle, weaponInfo_t *weaponInfo, modelViewType_t viewType )
{
	pc_token_t	token;
	char		filename[MAX_QPATH];
	int			part;
	partModel_t	*partModel;

	if( !PC_Int_Parse( handle, &part ) ) {
		return CG_RW_ParseError( handle, "expected part index" );
	}

	if( part < 0 || part >= W_MAX_PARTS )  {
		return CG_RW_ParseError( handle, "part index out of bounds" );
	}

	partModel = &weaponInfo->partModels[viewType][part];

	memset( partModel, 0, sizeof(*partModel) );

	if( !trap_PC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) ) {
		return CG_RW_ParseError( handle, "expected '{'" );
	}

	while( 1 ) {
		if( !trap_PC_ReadToken( handle, &token ) ) {
			break;
		}

		if( token.string[0] == '}' ) {
			break;
		}

		if( !Q_stricmp( token.string, "tag" ) ) {
			if( !PC_String_ParseNoAlloc( handle, partModel->tagName, sizeof(partModel->tagName) ) )
				return CG_RW_ParseError( handle, "expected tag name" );
		} else if( !Q_stricmp( token.string, "model" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected model filename" );
			} else {
				partModel->model = trap_R_RegisterModel( filename );
			}
		} else if( !Q_stricmp( token.string, "skin" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected skin filename" );
			} else {
				partModel->skin[0] = trap_R_RegisterSkin( filename );
			}
		} else if( !Q_stricmp( token.string, "axisSkin" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected skin filename" );
			} else {
				partModel->skin[TEAM_AXIS] = trap_R_RegisterSkin( filename );
			}
		} else if( !Q_stricmp( token.string, "alliedSkin" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected skin filename" );
			} else {
				partModel->skin[TEAM_ALLIES] = trap_R_RegisterSkin( filename );
			}
		} else {
			return CG_RW_ParseError( handle, "unknown token '%s'", token.string );
		}
	}

	return qtrue;
}

static qboolean CG_RW_ParseWeaponLink( int handle, weaponInfo_t *weaponInfo, modelViewType_t viewType )
{
	pc_token_t	token;

	if( !trap_PC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) ) {
		return CG_RW_ParseError( handle, "expected '{'" );
	}

	while( 1 ) {
		if( !trap_PC_ReadToken( handle, &token ) ) {
			break;
		}

		if( token.string[0] == '}' ) {
			break;
		}

		if( !Q_stricmp( token.string, "part" ) ) {
			if( !CG_RW_ParseWeaponLinkPart( handle, weaponInfo, viewType ) )
				return qfalse;
		} else {
			return CG_RW_ParseError( handle, "unknown token '%s'", token.string );
		}
	}

	return qtrue;
}

static qboolean CG_RW_ParseViewType( int handle, weaponInfo_t *weaponInfo, modelViewType_t viewType )
{
	pc_token_t	token;
	char		filename[MAX_QPATH];

	if( !trap_PC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) ) {
		return CG_RW_ParseError( handle, "expected '{'" );
	}

	while( 1 ) {
		if( !trap_PC_ReadToken( handle, &token ) ) {
			break;
		}

		if( token.string[0] == '}' ) {
			break;
		}

		if( !Q_stricmp( token.string, "model" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected model filename" );
			} else {
				weaponInfo->weaponModel[viewType].model = trap_R_RegisterModel( filename );
			}
		} else if( !Q_stricmp( token.string, "skin" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected skin filename" );
			} else {
				weaponInfo->weaponModel[viewType].skin[0] = trap_R_RegisterSkin( filename );
			}
		} else if( !Q_stricmp( token.string, "axisSkin" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected skin filename" );
			} else {
				weaponInfo->weaponModel[viewType].skin[TEAM_AXIS] = trap_R_RegisterSkin( filename );
			}
		} else if( !Q_stricmp( token.string, "alliedSkin" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected skin filename" );
			} else {
				weaponInfo->weaponModel[viewType].skin[TEAM_ALLIES] = trap_R_RegisterSkin( filename );
			}
		} else if( !Q_stricmp( token.string, "flashModel" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected flashModel filename" );
			} else {
				weaponInfo->flashModel[viewType] = trap_R_RegisterModel( filename );
			}
		} else if( !Q_stricmp( token.string, "weaponLink" ) ) {
			if( !CG_RW_ParseWeaponLink( handle, weaponInfo, viewType ) )
				return qfalse;
		} else {
			return CG_RW_ParseError( handle, "unknown token '%s'", token.string );
		}
	}

	return qtrue;
}

static qboolean CG_RW_ParseModModel( int handle, weaponInfo_t *weaponInfo )
{
	char		filename[MAX_QPATH];
	int			mod;

	if( !PC_Int_Parse( handle, &mod ) ) {
		return CG_RW_ParseError( handle, "expected mod index" );
	}

	if( mod < 0 || mod >= 6 )  {
		return CG_RW_ParseError( handle, "mod index out of bounds" );
	}

	if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
		return CG_RW_ParseError( handle, "expected model filename" );
	} else {
		weaponInfo->modModels[mod] = trap_R_RegisterModel( filename );
		if( !weaponInfo->modModels[mod] )
			// maybe it's a shader
			weaponInfo->modModels[mod] = trap_R_RegisterShader( filename );
	}

	return qtrue;
}

static qboolean CG_RW_ParseClient( int handle, weaponInfo_t *weaponInfo )
{
	pc_token_t	token;
	char		filename[MAX_QPATH];
	int			i;

	if( !trap_PC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) ) {
		return CG_RW_ParseError( handle, "expected '{'" );
	}

	while( 1 ) {
		if( !trap_PC_ReadToken( handle, &token ) ) {
			break;
		}

		if( token.string[0] == '}' ) {
			break;
		}

		if( !Q_stricmp( token.string, "standModel" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected standModel filename" );
			} else {
				weaponInfo->standModel = trap_R_RegisterModel( filename );
			}
		} else if( !Q_stricmp( token.string, "droppedAnglesHack" ) ) {
			weaponInfo->droppedAnglesHack = qtrue;
		} else if( !Q_stricmp( token.string, "pickupModel" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected pickupModel filename" );
			} else {
				weaponInfo->weaponModel[W_PU_MODEL].model = trap_R_RegisterModel( filename );
			}
		} else if( !Q_stricmp( token.string, "pickupSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected pickupSound filename" );
			} else {
				//weaponInfo->pickupSound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "weaponConfig" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected weaponConfig filename" );
			} else {
				if( !CG_ParseWeaponConfig( filename, weaponInfo ) ) {
//					CG_Error( "Couldn't register weapon %i (failed to parse %s)", weaponNum, filename );
				}
			}
		} else if( !Q_stricmp( token.string, "handsModel" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected handsModel filename" );
			} else {
				weaponInfo->handsModel = trap_R_RegisterModel( filename );
			}
		} else if( !Q_stricmp( token.string, "flashDlightColor" ) ) {
			if( !PC_Vec_Parse( handle, &weaponInfo->flashDlightColor ) ) {
				return CG_RW_ParseError( handle, "expected flashDlightColor as r g b" );
			}
		} else if( !Q_stricmp( token.string, "flashSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected flashSound filename" );
			} else {
				for( i = 0; i < 4; i++ ) {
					if( !weaponInfo->flashSound[i] ) {
						weaponInfo->flashSound[i] = trap_S_RegisterSound( filename, qfalse );
						break;
					}
				}
				if( i == 4 )
					CG_Printf( S_COLOR_YELLOW "WARNING: only up to 4 flashSounds supported per weapon\n" );
			}
		} else if( !Q_stricmp( token.string, "flashEchoSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected flashEchoSound filename" );
			} else {
				for( i = 0; i < 4; i++ ) {
					if( !weaponInfo->flashEchoSound[i] ) {
						weaponInfo->flashEchoSound[i] = trap_S_RegisterSound( filename, qfalse );
						break;
					}
				}
				if( i == 4 )
					CG_Printf( S_COLOR_YELLOW "WARNING: only up to 4 flashEchoSounds supported per weapon\n" );
			}
		} else if( !Q_stricmp( token.string, "lastShotSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected lastShotSound filename" );
			} else {
				for( i = 0; i < 4; i++ ) {
					if( !weaponInfo->lastShotSound[i] ) {
						weaponInfo->lastShotSound[i] = trap_S_RegisterSound( filename, qfalse );
						break;
					}
				}
				if( i == 4 )
					CG_Printf( S_COLOR_YELLOW "WARNING: only up to 4 lastShotSound supported per weapon\n" );
			}
		} else if( !Q_stricmp( token.string, "readySound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected readySound filename" );
			} else {
				weaponInfo->readySound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "firingSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected firingSound filename" );
			} else {
				weaponInfo->firingSound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "overheatSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected overheatSound filename" );
			} else {
				weaponInfo->overheatSound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "reloadSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected reloadSound filename" );
			} else {
				weaponInfo->reloadSound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "reloadFastSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected reloadFastSound filename" );
			} else {
				weaponInfo->reloadFastSound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "spinupSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected spinupSound filename" );
			} else {
				weaponInfo->spinupSound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "spindownSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected spindownSound filename" );
			} else {
				weaponInfo->spindownSound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "switchSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected switchSound filename" );
			} else {
				weaponInfo->switchSound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "weaponIcon" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected weaponIcon filename" );
			} else {
				weaponInfo->weaponIcon[0] = trap_R_RegisterShader( filename );
			}
		} else if( !Q_stricmp( token.string, "weaponSelectedIcon" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected weaponSelectedIcon filename" );
			} else {
				weaponInfo->weaponIcon[1] = trap_R_RegisterShader( filename );
			}
		/*} else if( !Q_stricmp( token.string, "ammoIcon" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected ammoIcon filename" );
			} else {
				weaponInfo->ammoIcon = trap_R_RegisterShader( filename );
			}*/
		} else if( !Q_stricmp( token.string, "missileModel" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected missileModel filename" );
			} else {
				weaponInfo->missileModel = trap_R_RegisterModel( filename );
			}
		} else if( !Q_stricmp( token.string, "missileAlliedSkin" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected skin filename" );
			} else {
				weaponInfo->missileAlliedSkin = trap_R_RegisterSkin( filename );
			}
		} else if( !Q_stricmp( token.string, "missileAxisSkin" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected skin filename" );
			} else {
				weaponInfo->missileAxisSkin = trap_R_RegisterSkin( filename );
			}
		} else if( !Q_stricmp( token.string, "missileSound" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected missileSound filename" );
			} else {
				weaponInfo->missileSound = trap_S_RegisterSound( filename, qfalse );
			}
		} else if( !Q_stricmp( token.string, "missileTrailFunc" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected missileTrailFunc" );
			} else {
				if( !Q_stricmp( filename, "GrenadeTrail" ) ) {
					weaponInfo->missileTrailFunc = TCE_CG_GrenadeTrail;
				} else if( !Q_stricmp( filename, "RocketTrail" ) ) {
					weaponInfo->missileTrailFunc = CG_RocketTrail;
				} else if( !Q_stricmp( filename, "PyroSmokeTrail" ) ) {
					weaponInfo->missileTrailFunc = CG_PyroSmokeTrail;
				} else if( !Q_stricmp( filename, "DynamiteTrail" ) ) {
					weaponInfo->missileTrailFunc = TCE_CG_DynamiteTrail;
				}				
			}
		} else if( !Q_stricmp( token.string, "missileDlight" ) ) {
			if( !PC_Float_Parse( handle, &weaponInfo->missileDlight ) ) {
				return CG_RW_ParseError( handle, "expected missileDlight value" );
			}
		} else if( !Q_stricmp( token.string, "missileDlightColor" ) ) {
			if( !PC_Vec_Parse( handle, &weaponInfo->missileDlightColor ) ) {
				return CG_RW_ParseError( handle, "expected missileDlightColor as r g b" );
			}
		} else if( !Q_stricmp( token.string, "ejectBrassFunc" ) ) {
			if( !PC_String_ParseNoAlloc( handle, filename, sizeof(filename) ) ) {
				return CG_RW_ParseError( handle, "expected ejectBrassFunc" );
			} else {
				if( !Q_stricmp( filename, "MachineGunEjectBrass" ) ) {
					weaponInfo->ejectBrassFunc = CG_MachineGunEjectBrass;
				} else if( !Q_stricmp( filename, "PanzerFaustEjectBrass" ) ) {
					weaponInfo->ejectBrassFunc = CG_PanzerFaustEjectBrass;
				}				
			}
		} else if( !Q_stricmp( token.string, "modModel" ) ) {
			if( !CG_RW_ParseModModel( handle, weaponInfo ) )
				return qfalse;
		} else if( !Q_stricmp( token.string, "firstPerson" ) ) {
			if( !CG_RW_ParseViewType( handle, weaponInfo, W_FP_MODEL ) )
				return qfalse;
		} else if( !Q_stricmp( token.string, "thirdPerson" ) ) {
			if( !CG_RW_ParseViewType( handle, weaponInfo, W_TP_MODEL ) )
				return qfalse;
		} else {
			return CG_RW_ParseError( handle, "unknown token '%s'", token.string );
		}
	}

	return qtrue;
}

static qboolean CG_RegisterWeaponFromWeaponFile( const char *filename, weaponInfo_t *weaponInfo )
{
	pc_token_t token;
	int handle;

	handle = trap_PC_LoadSource( filename );

	if( !handle )
		return qfalse;

	if( !trap_PC_ReadToken( handle, &token ) || Q_stricmp( token.string, "weaponDef" ) ) {
		return CG_RW_ParseError( handle, "expected 'weaponDef'" );
	}

	if( !trap_PC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) ) {
		return CG_RW_ParseError( handle, "expected '{'" );
	}

	while( 1 ) {
		if( !trap_PC_ReadToken( handle, &token ) ) {
			break;
		}

		if( token.string[0] == '}' ) {
			break;
		}

		if( !Q_stricmp( token.string, "client" ) ) {
			if( !CG_RW_ParseClient( handle, weaponInfo ) ) {
				return qfalse;
			}
		} else {
			return CG_RW_ParseError( handle, "unknown token '%s'", token.string );
		}
	}

	trap_PC_FreeSource( handle );

	return qtrue;
}

/*
=================
CG_RegisterWeapon
=================
*/
/* SDK media projection: TC files are the single asset source. The original
 * renderer's portal scopes, tactical parts and sleeve override remain separate. */
void CG_RegisterWeapon( int weaponNum, qboolean force ) {
    weaponInfo_t *w; tce_weaponInfo_t *t; int v,p;
    if(weaponNum<=0||weaponNum>=TCE_MAX_WEAPONS)return;
    w=&cg_weapons[weaponNum];if(w->registered&&!force)return;
    TCE_CG_RegisterWeapon(weaponNum,force);t=&tce_cg_weapons[weaponNum];
    memset(w,0,sizeof(*w));w->registered=qtrue;
    memcpy(w->weapAnimations,t->animations,sizeof(t->animations));
    w->handsModel=t->handsModel;w->standModel=t->standModel;w->droppedAnglesHack=t->droppedAnglesHack;
    for(v=0;v<3;++v){
        w->weaponModel[v].model=t->weaponModel[v].model;
        memcpy(w->weaponModel[v].skin,t->weaponModel[v].skin,sizeof(w->weaponModel[v].skin));
        for(p=0;p<7;++p){
            Q_strncpyz(w->partModels[v][p].tagName,t->partModels[v][p].tagName,MAX_QPATH);
            w->partModels[v][p].model=t->partModels[v][p].model;
            memcpy(w->partModels[v][p].skin,t->partModels[v][p].skin,sizeof(w->partModels[v][p].skin));
        }
    }
    memcpy(w->flashModel,t->flashModel,sizeof(w->flashModel));
    memcpy(w->modModels,t->modModels,sizeof(w->modModels));
    VectorCopy(t->flashDlightColor,w->flashDlightColor);
    memcpy(w->flashSound,t->flashSound,sizeof(w->flashSound));
    memcpy(w->flashEchoSound,t->flashEchoSound,sizeof(w->flashEchoSound));
    memcpy(w->lastShotSound,t->lastShotSound,sizeof(w->lastShotSound));
    w->weaponIcon[0]=t->weaponIcon;w->weaponIcon[1]=t->weaponSelectedIcon;
    w->missileModel=t->missileModel;w->missileAlliedSkin=t->missileAlliedSkin;w->missileAxisSkin=t->missileAxisSkin;
    w->missileSound=t->missileSound;w->missileDlight=t->missileDlight;w->missileRenderfx=t->missileRenderfx;
    VectorCopy(t->missileDlightColor,w->missileDlightColor);
    w->ejectBrassFunc=t->ejectBrass==TCE_BRASS_MACHINEGUN?CG_MachineGunEjectBrass:t->ejectBrass==TCE_BRASS_PANZERFAUST?CG_PanzerFaustEjectBrass:NULL;
    w->readySound=t->readySound;w->firingSound=t->firingSound;w->overheatSound=t->overheatSound;
    w->reloadSound=t->reloadSound;w->reloadFastSound=t->reloadFastSound;
    w->spinupSound=t->spinupSound;w->spindownSound=t->spindownSound;w->switchSound=t->switchSound;
}

/*
========================================================================================

VIEW WEAPON

========================================================================================
*/


//
// weapon animations
//

/* SDK weapon-ID adapter; animation behavior is the verified TC:E core. */
static void CG_WeaponAnimation(playerState_t *ps, weaponInfo_t *weapon,
    int *oldFrame, int *frame, float *backlerp) {
    lerpFrame_t *lf = &cg.predictedPlayerEntity.pe.weap;
    int raiseAnimation = -1;
    if (!cg_noPlayerAnims.integer && cg_animSpeed.integer && lf->animation &&
        ps->weapAnim != lf->animationNumber)
        raiseAnimation = PM_RaiseAnimForWeapon(cg.snap->ps.nextWeapon);
    TCE_CG_WeaponAnimation(ps->weapAnim, weapon->weapAnimations,
        oldFrame, frame, backlerp, raiseAnimation);
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


// (SA) it wasn't used anyway


/*
==============
CG_CalculateWeaponPosition
==============
*/
static void CG_CalculateWeaponPosition( vec3_t origin, vec3_t angles ) {
	float	scale;
	int		delta;
	float	fracsin;
	qboolean tc = gearDef.parsed;
	int weapon = cg.predictedPlayerState.weapon;
	qboolean mountedWeapon = tc ? (weapon == 60 || weapon == 62) :
		(weapon == WP_MORTAR_SET || weapon == WP_MOBILE_MG42_SET);
	if (tc && weapon > 0 && weapon < TCE_MAX_WEAPONS) {
		tce_weaponPosition_t s;
		playerState_t *ps=&cg.predictedPlayerState;
		memset(&s,0,sizeof(s));
		s.time=cg.time;s.weapon=weapon;s.eFlags=ps->eFlags;
		s.thirdPerson=cg.renderingThirdPerson;s.weaponState=ps->weaponstate;
		s.mountedPitch=cg.pmext.mountedWeaponAngles[PITCH];
		VectorCopy(cg.refdef_current->vieworg,s.viewOrigin);
		AxisCopy(cg.refdef_current->viewaxis,s.viewAxis);
		VectorCopy(cg.refdefViewAngles,s.viewAngles);
		VectorCopy(tce_cg_weapons[weapon].gunViewAngles,s.gunViewAngles);
		s.proneMovingTime=cg.proneMovingTime;
		s.flags_3407dfb0=ps->stats[STAT_TCE_WEAPON_FLAGS];
		s.stanceTime=cg.tceStanceTime;s.pmFlags=ps->pm_flags;
		s.duckTime=cg.tceWeaponDuckTime;s.lean=ps->leanf;
		s.aiming=cg.tceAimActive;s.leanTime=cg.zoomTime;
		s.shotTime=cg.predictedPlayerEntity.muzzleFlashTime;
		s.count_3407dff8=ps->persistant[PERS_BLEH_2];
		s.scopeEnabled=cg_portalScopes.integer;s.firemodeTime=cg.tceFiremodeAnimationTime;
		s.speed=cg.xyspeed;s.bobSin=cg.bobfracsin;s.bobCycle=cg.bobcycle;
		s.swayTime=cg.tceSwayTime;VectorCopy(cg.tcePriorForward,s.priorForward);
		s.swayHorizontal=cg.tceSwayHorizontal;s.swayVertical=cg.tceSwayVertical;
		VectorCopy(ps->velocity,s.velocity);s.proneTime=cg.tceProneTime;
		s.stepTime=cg.stepTime;s.stepChange=cg.stepChange;
		s.scale_3407dfbc=ps->stats[STAT_TCE_MOVEMENT_INSTABILITY];
		s.scale_3407dfc0=ps->stats[STAT_TCE_SHOT_INSTABILITY];
		s.landTime=cg.landTime;s.landChange=cg.landChange;
		s.developer=developer.integer;
		VectorSet(s.developerAngles,cg_gunPitch.value,cg_gunYaw.value,cg_gunRoll.value);
		s.tacticalScale=cg.tceTacticalScale;
		s.tacticalPitch=ps->holdable[5];s.tacticalYaw=ps->holdable[6];
		VectorCopy(cg.kickAngles,s.kickAngles);
		TCE_CG_CalculateWeaponPosition(&s,origin,angles);
		cg.tceScopeBlocked=s.postureModified;cg.tceSwayTime=s.swayTime;
		VectorCopy(s.priorForward,cg.tcePriorForward);
		cg.tceSwayHorizontal=s.swayHorizontal;cg.tceSwayVertical=s.swayVertical;
		return;
	}

	VectorCopy( cg.refdef_current->vieworg, origin );
	VectorCopy( cg.refdefViewAngles, angles );

	if( cg.predictedPlayerState.eFlags & EF_MOUNTEDTANK ) {
		angles[PITCH] = cg.refdefViewAngles[PITCH] / 1.2;
	}

	if( !cg.renderingThirdPerson &&
		mountedWeapon&&
		cg.predictedPlayerState.weaponstate != WEAPON_RAISING ) {
		angles[PITCH] = cg.pmext.mountedWeaponAngles[PITCH];
	}

	if( cg.predictedPlayerState.eFlags & EF_PRONE_MOVING ) {
		int pronemovingtime = cg.time - cg.proneMovingTime;
		if( pronemovingtime > 0 ) {	// div by 0
			float factor = pronemovingtime > 200 ? 1.f : 1.f / (200.f / (float)pronemovingtime);
			VectorMA( origin, factor * (tc ? -25 : -20), cg.refdef_current->viewaxis[0], origin );
			VectorMA( origin, factor * (tc ? 5 : 3), cg.refdef_current->viewaxis[1], origin );
			if(tc) { VectorMA(origin, factor * -5, cg.refdef_current->viewaxis[2], origin); angles[YAW] += factor * 60; }
		}
	} else {
		int pronenomovingtime = cg.time - -cg.proneMovingTime;

		if( pronenomovingtime < 200 ) {
			float factor = pronenomovingtime == 0 ? 1.f : 1.f - (1.f / (200.f / (float)pronenomovingtime));
			VectorMA( origin, factor * (tc ? -25 : -20), cg.refdef_current->viewaxis[0], origin );
			VectorMA( origin, factor * (tc ? 5 : 3), cg.refdef_current->viewaxis[1], origin );
			if(tc) { VectorMA(origin, factor * -5, cg.refdef_current->viewaxis[2], origin); angles[YAW] += factor * 60; }
		}
	}

	// adjust 'lean' into weapon
	if(cg.predictedPlayerState.leanf != 0) {
		vec3_t	right, up;
		float myfrac = 1.0f;

		if(!tc) switch(cg.predictedPlayerState.weapon) {
			case WP_FLAMETHROWER:
			case WP_KAR98:
			case WP_CARBINE:
			case WP_GPG40:
			case WP_M7:
			case WP_K43:
				myfrac = 2.0f;
				break;
			case WP_GARAND:
				myfrac = 3.0f;
				break;
		}

		// reverse the roll on the weapon so it stays relatively level
		angles[ROLL] -= cg.predictedPlayerState.leanf/(myfrac * 2.0f);
		AngleVectors(angles, NULL, right, up);
		VectorMA(origin, angles[ROLL], right, origin);

		// pitch the gun down a bit to show that firing is not allowed when leaning
		angles[PITCH] += (abs(cg.predictedPlayerState.leanf)/2.0f);

		// this gives you some impression that the weapon stays in relatively the same
		// position while you lean, so you appear to 'peek' over the weapon
		AngleVectors(cg.refdefViewAngles, NULL, right, NULL);
		VectorMA(origin, -cg.predictedPlayerState.leanf/4.0f, right, origin);
	}


	// on odd legs, invert some angles
	if ( cg.bobcycle & 1 ) {
		scale = -cg.xyspeed;
	} else {
		scale = cg.xyspeed;
	}

	// gun angles from bobbing

	angles[ROLL]	+= scale		* cg.bobfracsin * 0.005;
	angles[YAW]		+= scale		* cg.bobfracsin * 0.01;
	angles[PITCH]	+= cg.xyspeed	* cg.bobfracsin * 0.005;

	// drop the weapon when landing
	delta = cg.time - cg.landTime;
	if(tc) {
		/* Windows30073270:150ms deflection,300ms return. */
		if(delta < 150) origin[2] += cg.landChange * delta / 600.0f;
		else if(delta < 450) origin[2] += (450-delta) * cg.landChange / 1200.0f;
	} else if ( delta < LAND_DEFLECT_TIME ) {
		origin[2] += cg.landChange*0.25 * delta / LAND_DEFLECT_TIME;
	} else if ( delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME ) {
		origin[2] += cg.landChange*0.25 *
			(LAND_DEFLECT_TIME + LAND_RETURN_TIME - delta) / LAND_RETURN_TIME;
	}
	if(tc && weapon > 0 && weapon < 64) {
		VectorAdd(angles, tce_cg_weapons[weapon].gunViewAngles, angles);
	}

#if 0
	// drop the weapon when stair climbing
	delta = cg.time - cg.stepTime;
	if ( delta < STEP_TIME/2 ) {
		origin[2] -= cg.stepChange*0.25 * delta / (STEP_TIME/2);
	} else if ( delta < STEP_TIME ) {
		origin[2] -= cg.stepChange*0.25 * (STEP_TIME - delta) / (STEP_TIME/2);
	}
#endif

	// idle drift
	if( (!(cg.predictedPlayerState.eFlags & EF_MOUNTEDTANK)) && !mountedWeapon ) {
	//----(SA) adjustment for MAX KAUFMAN
	//	scale = cg.xyspeed + 40;
		scale = 80;
	//----(SA)	end
		fracsin = sin( cg.time * 0.001 );
		angles[ROLL] += scale * fracsin * 0.01;
		angles[YAW] += scale * fracsin * 0.01;
		angles[PITCH] += scale * fracsin * 0.01;
	}

	// RF, subtract the kickAngles
	VectorMA( angles, -1.0, cg.kickAngles, angles );
}


/* Verified weapon effect dispatch lives in tce_weapon_effects.c. */

/*
=============
CG_AddPlayerWeapon

Used for both the view weapon (ps is valid) and the world modelother character models (ps is NULL)
The main player will have this called for BOTH cases, so effects like light and
sound should only be done on the world model case.
=============
*/
static qboolean debuggingweapon = qfalse;

/* CG_AddPlayerWeapon:3006fa..300701: procedural drop/raise is separate
 * from MD3 frames. The timestamp persists across the old/new weapon pair.
 * This is a recovered branch; the whole renderer remains under reconstruction. */
static void TCE_CG_WeaponSwitchPose(refEntity_t *entity, int animation) {
    float fraction;
    vec3_t angles;
    int i;
    if (animation == 6) {
        fraction = (float)(cg.weaponAnimationTime - cg.time + 400);
        if (fraction <= 0.f) return;
    } else if (animation == 5) {
        fraction = (float)(cg.time - cg.weaponAnimationTime);
        if (fraction > 200.f) {
            cg.weaponAnimationTime = cg.time - 200;
            fraction = 200.f;
        }
    } else {
        cg.weaponAnimationTime = cg.time;
        return;
    }
    fraction *= .005f;
    for (i = 0; i < 3; ++i)
        entity->origin[i] += fraction * -5.f * cg.refdef.viewaxis[0][i]
                           + fraction * -20.f * cg.refdef.viewaxis[2][i];
    VectorCopy(cg.refdefViewAngles, angles);
    angles[PITCH] += fraction * 30.f;
    angles[YAW] += fraction * 10.f;
    AnglesToAxis(angles, entity->axis);
}

void CG_AddPlayerWeapon( refEntity_t *parent, playerState_t *ps, centity_t *cent) {
    refEntity_t tceBrassParent;
    qboolean tceBrassParentReady=qfalse;
    qboolean tceRestoreDualFrames=qfalse;
    int tceDualFrame=0, tceDualOldFrame=0;
    vec3_t tceSecondaryOffset={0,0,0};

	refEntity_t	gun;
	refEntity_t	barrel;
	refEntity_t	flash;
	vec3_t		angles;
	weapon_t	weaponNum;
	weaponInfo_t	*weapon;
	centity_t	*nonPredictedCent;
	qboolean	firing;	// Ridah
	qboolean	akimboFire = qfalse;

//	qboolean	playerScaled;
	qboolean	drawpart;
	int			i;
	qboolean	isPlayer;

	bg_playerclass_t* classInfo;

	classInfo = BG_GetPlayerClassInfo(cgs.clientinfo[cent->currentState.clientNum].team, cgs.clientinfo[cent->currentState.clientNum].cls);

	// (SA) might as well have this check consistant throughout the routine
	isPlayer = (qboolean)(cent->currentState.clientNum == cg.snap->ps.clientNum);

	weaponNum = cent->currentState.weapon;

	if(ps && cg.cameraMode) {
		return;
	}

	// don't draw any weapons when the binocs are up
	if( cent->currentState.eFlags & EF_ZOOMING ) {
		return;
	}

	// don't draw weapon stuff when looking through a scope
	if( weaponNum == 57 || weaponNum == 58 || weaponNum == 59 ) {
		if( isPlayer && !cg.renderingThirdPerson ) {
			return;
		}
	}

	if( weaponNum == 9 || weaponNum == 4 ) {
		if( ps && !ps->ammoclip[ weaponNum ] &&
			(ps->weapAnim & ~ANIM_TOGGLEBIT) != 3 ) {
			return;
		}
	}

	// no weapon when on mg_42
	if( cent->currentState.eFlags & EF_MOUNTEDTANK ) {	
		if( isPlayer && !cg.renderingThirdPerson ) {
			return;
		}

		if( cg.time - cent->muzzleFlashTime < MUZZLE_FLASH_TIME ) {
			memset (&flash, 0, sizeof (flash));
			flash.renderfx = RF_LIGHTING_ORIGIN;
			flash.hModel = cgs.media.mg42muzzleflash;
			
			VectorCopy( cg_entities[cg_entities[ cent->currentState.number ].tagParent].mountedMG42Flash.origin, flash.origin );
			AxisCopy( cg_entities[cg_entities[ cent->currentState.number ].tagParent].mountedMG42Flash.axis, flash.axis );
			
			trap_R_AddRefEntityToScene( &flash );
			
			// ydnar: add dynamic light
			trap_R_AddLightToScene( flash.origin, 320, 1.25 + (rand() & 31) / 128, 1.0, 0.6, 0.23, 0, 0 );
		}
		return;
	}

	if( cent->currentState.eFlags & EF_MG42_ACTIVE || cent->currentState.eFlags & EF_AAGUN_ACTIVE ) {
		// Arnout: MG42 Muzzle Flash
		if ( cg.time - cent->muzzleFlashTime < MUZZLE_FLASH_TIME ) {
			CG_MG42EFX( cent );
		}
		return;
	}

	if( (!ps || cg.renderingThirdPerson) &&
		(cent->currentState.eFlags & 0x00900000) == 0x00100000 ) {
		return;
	}

	weapon = &cg_weapons[weaponNum];

	if( BG_IsAkimboWeapon( weaponNum ) ) {
		if( isPlayer ) {
			akimboFire = BG_AkimboFireSequence( weaponNum, cg.predictedPlayerState.ammoclip[BG_FindClipForWeapon(weaponNum)], cg.predictedPlayerState.ammoclip[BG_FindClipForWeapon(BG_AkimboSidearm(weaponNum))] );
		} else if( ps ) {
			akimboFire = BG_AkimboFireSequence( weaponNum, ps->ammoclip[BG_FindClipForWeapon(weaponNum)], ps->ammoclip[BG_FindClipForWeapon(BG_AkimboSidearm(weaponNum))] );
		}
		// Gordon: FIXME: alternate for other clients, store flip-flop on cent or smuffin
	}


	// add the weapon
	memset( &gun, 0, sizeof( gun ) );
	VectorCopy( parent->lightingOrigin, gun.lightingOrigin );
	gun.shadowPlane = parent->shadowPlane;
	gun.renderfx = parent->renderfx;
	
	if( ps ) {
		team_t team = ps->persistant[PERS_TEAM];
		bg_character_t *character;
		/* Original CG_AddPlayerWeapon3006f4b0: role character precedes disguising the
		 * weapon team. Arms override weapon skins only when registered. */
		if (cent->currentState.effect1Time & 0x60)
			character = BG_GetCharacter(team, 6);
		else
			character = CG_CharacterForClientinfo(&cgs.clientinfo[cent->currentState.clientNum], cent);

		if( weaponNum != 27 && ( cent->currentState.powerups & (1 << PW_OPS_DISGUISED) ) ) {
			team = team == TEAM_AXIS ? TEAM_ALLIES : TEAM_AXIS;
		}

		gun.hModel = weapon->weaponModel[W_FP_MODEL].model;
		if (weaponNum == 1 && character->armsSkins[2])
			gun.customSkin = character->armsSkins[2];
		else if (weaponNum == 9 && character->armsSkins[1])
			gun.customSkin = character->armsSkins[1];
		else if (character->armsSkins[0])
			gun.customSkin = character->armsSkins[0];
		else if( ( team == TEAM_AXIS ) &&
			weapon->weaponModel[W_FP_MODEL].skin[TEAM_AXIS] )
			gun.customSkin = weapon->weaponModel[W_FP_MODEL].skin[TEAM_AXIS];
		else if( ( team == TEAM_ALLIES ) &&
				 weapon->weaponModel[W_FP_MODEL].skin[TEAM_ALLIES] )
			gun.customSkin = weapon->weaponModel[W_FP_MODEL].skin[TEAM_ALLIES];
		else
			gun.customSkin = weapon->weaponModel[W_FP_MODEL].skin[0];	// if not loaded it's 0 so doesn't do any harm
	} else {
		team_t team = cgs.clientinfo[cent->currentState.clientNum].team;

		if( weaponNum != 27 && cent->currentState.powerups & (1 << PW_OPS_DISGUISED) ) {
			team = team == TEAM_AXIS ? TEAM_ALLIES : TEAM_AXIS;
		}

		gun.hModel = weapon->weaponModel[W_TP_MODEL].model;
		if( ( team == TEAM_AXIS ) &&
			weapon->weaponModel[W_TP_MODEL].skin[TEAM_AXIS] )
			gun.customSkin = weapon->weaponModel[W_FP_MODEL].skin[TEAM_AXIS];
		else if( ( team == TEAM_ALLIES ) &&
				 weapon->weaponModel[W_TP_MODEL].skin[TEAM_ALLIES] )
			gun.customSkin = weapon->weaponModel[W_TP_MODEL].skin[TEAM_ALLIES];
		else
			gun.customSkin = weapon->weaponModel[W_TP_MODEL].skin[0];	// if not loaded it's 0 so doesn't do any harm
	}

	if (!gun.hModel) {
		if(debuggingweapon) CG_Printf("returning due to: !gun.hModel\n");
		return;
	}

	if(!ps && cg.snap->ps.pm_flags & PMF_LADDER && isPlayer)		//----(SA) player on ladder
	{
		if(debuggingweapon) CG_Printf("returning due to: !ps && cg.snap->ps.pm_flags & PMF_LADDER\n");
		return;
	}

	if ( !ps ) {
		// add weapon ready sound
		cent->pe.lightningFiring = qfalse;
		if ( ( cent->currentState.eFlags & EF_FIRING ) && weapon->firingSound ) {
			// lightning gun and guantlet make a different sound when fire is held down
			trap_S_AddLoopingSound( cent->lerpOrigin, vec3_origin, weapon->firingSound, 255, 0 );
			cent->pe.lightningFiring = qtrue;
		} else if ( weapon->readySound ) {
			trap_S_AddLoopingSound( cent->lerpOrigin, vec3_origin, weapon->readySound, 255, 0 );
		}
	}

	// Ridah
	firing = ((cent->currentState.eFlags & EF_FIRING) != 0);
	/* Original dual-pistol empty hand: preserve the caller's frames for
	 * the later second-hand parts; the first hand uses frame21. */
	if (ps && !cg.renderingThirdPerson && (weaponNum == 37 || weaponNum == 38) &&
		ps->ammoclip[BG_FindClipForWeapon(weaponNum)] < 1 &&
		(parent->frame == tce_cg_weapons[weaponNum].animations[0].firstFrame ||
		 parent->oldframe == tce_cg_weapons[weaponNum].animations[0].firstFrame ||
		 (ps->weapAnim & ~ANIM_TOGGLEBIT) == 3)) {
		tceDualFrame = parent->frame;
		tceDualOldFrame = parent->oldframe;
		parent->frame = parent->oldframe = 21;
		tceRestoreDualFrames = ps->ammoclip[BG_FindClipForWeapon(BG_AkimboSidearm(weaponNum))] > 0 ||
			(ps->weapAnim & ~ANIM_TOGGLEBIT) == 3;
	}

    /* Original dual pistols move the tag parent; UT models move the root. */
    if (ps && (weaponNum == 37 || weaponNum == 38))
        TCE_CG_WeaponSwitchPose(parent, ps->weapAnim & ~ANIM_TOGGLEBIT);

	if( ps && !cg.renderingThirdPerson && cg.predictedPlayerState.weapon == 60 && cg.predictedPlayerState.weaponstate != WEAPON_RAISING ) {
		vec3_t angles;

		angles[YAW] = angles[ROLL] = 0.f;
		angles[PITCH] = -.4f * AngleNormalize180( cg.pmext.mountedWeaponAngles[PITCH] - ps->viewangles[PITCH] );

		AnglesToAxis( angles, gun.axis );

		CG_PositionRotatedEntityOnTag( &gun, parent, "tag_weapon" );
	} else if( (!ps || cg.renderingThirdPerson) && (weaponNum == 60 || weaponNum == 35 ) ) {
			CG_PositionEntityOnTag( &gun, parent, "tag_weapon2", 0, NULL);
		} else {
			CG_PositionEntityOnTag( &gun, parent, "tag_weapon", 0, NULL);
		}


/*	playerScaled = (qboolean)(cgs.clientinfo[ cent->currentState.clientNum ].playermodelScale[0] != 0);
	if(!ps && playerScaled) {	// don't "un-scale" weap up in 1st person
		for(i=0;i<3;i++) {	// scale weapon back up so it doesn't pick up the adjusted scale of the character models.
							// this will affect any parts attached to the gun as well (barrel/bolt/flash/brass/etc.)
			VectorScale( gun.axis[i], 1.0/(cgs.clientinfo[ cent->currentState.clientNum ].playermodelScale[i]), gun.axis[i]);
		}

	}*/

	if(ps) {
		drawpart = CG_GetPartFramesFromWeap(cent, &gun, parent, W_MAX_PARTS, weapon);	// W_MAX_PARTS specifies this as the primary view model
        /* TC:E CG_AddPlayerWeapon 3006f4b0: UT root uses the full animation.
         * Part 7 deliberately does not alter frames in GetPartFramesFromWeap. */
        if(BG_CheckUTWeapon(weaponNum)) {
            int animation = ps->weapAnim & ~ANIM_TOGGLEBIT;
            const tce_weaponInfo_t *media = &tce_cg_weapons[weaponNum];
            gun.frame=parent->frame;gun.oldframe=parent->oldframe;gun.backlerp=parent->backlerp;
            drawpart = qtrue;
            /* Bolt cycling owns the same timestamp in the original. */
            if (!weaponDef[weaponNum].bolt || animation != 7 ||
                gun.frame < 9 || gun.frame > 23)
                TCE_CG_WeaponSwitchPose(&gun, animation);
            else {
                /* Original3006f4b0: bolt cycle translates along view-up;
                 * it deliberately does not reset the switch/bolt timestamp. */
                double shift = (1.0 - cos((double)(cg.time - cg.weaponAnimationTime)
                    * (double)0.01f * (double)0.4487989544868469f)) * -12.5;
                for (i = 0; i < 3; ++i)
                    gun.origin[i] = (float)(shift * (double)cg.refdef.viewaxis[2][i] + (double)gun.origin[i]);
            }
            /* UT idle animations use the first frame, not a stale interpolated
             * hand frame. Developer cg_gun_frame still has precedence. */
            if (!cg_gun_frame.integer && (animation == 0 || animation == 1)) {
                int idle = media->animations[animation].firstFrame;
                if (gun.frame != idle) gun.frame = gun.oldframe = idle;
            }
            /* Original weaponDef+0x11c is the existing recovered field;
             * retain the original numeric condition without guessing a category. */
            if (!weaponDef[weaponNum].loadoutWeight &&
                ps->ammoclip[BG_FindClipForWeapon(weaponNum)] < 1 &&
                (gun.frame == media->animations[0].firstFrame ||
                 gun.oldframe == media->animations[0].firstFrame))
                gun.frame = gun.oldframe = media->animations[1].firstFrame;
            /* Original UT scoped/bolt/persistent-state frame suppression. */
            if ((weaponDef[weaponNum].bolt &&
                 (cg.predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS] & 4) &&
                 (animation == 2 || animation == 3 || animation == 4)) ||
                (weaponDef[weaponNum].loadoutWeight && cg.predictedPlayerState.persistant[10] > 0 &&
                 weaponDef[weaponNum].unknown_0f8[6] &&
                 (animation == 0 || animation == 2 || animation == 3 || animation == 4)) ||
                (weaponDef[weaponNum].scoped > 1.0f && cg.tceAimActive && cg_portalScopes.integer &&
                 (animation == 0 || animation == 2 || animation == 3 || animation == 4)))
                gun.frame = gun.oldframe = media->animations[0].firstFrame;
            if ((cg.predictedPlayerState.eFlags & 0x00100000) &&
                (weaponNum == 4 || weaponNum == 9 || weaponNum == 30) &&
                (animation == 2 || animation == 3 || animation == 4))
                return;
        }

        /* Windows300702dc..300703ff: offset uses the unshortened UT parent
         * axes; only the rendered UT gun is shortened afterwards. */
        if(!cg.renderingThirdPerson && weaponNum>0 && weaponNum<TCE_MAX_WEAPONS) {
            tce_weaponOffset_t state;vec3_t offset,secondary;float tactical;
            tce_weaponInfo_t *media=&tce_cg_weapons[weaponNum];
            memset(&state,0,sizeof(state));state.time=cg.time;state.aimTime=cg.zoomTime;
            state.aiming=cg.tceAimActive;
            state.secondaryAiming=cg.tceAimWeaponLatch;
            state.gunPosition=cg_gunPosition.integer;
            state.developer=developer.integer;
            state.developerOffset[0]=cg_tacX.value;
            state.developerOffset[1]=cg_tacY.value;
            state.developerOffset[2]=cg_tacZ.value;
            VectorCopy(media->gunViewOffset,state.gunViewOffset);
            VectorCopy(media->gunViewAimOffset,state.gunAimOffset);
            TCE_CG_EliteFPWeaponOffset(weaponNum,&state,parent->axis,offset,secondary,&tactical,NULL);
            VectorCopy(secondary, tceSecondaryOffset);
            cg.tceTacticalScale=tactical;
            if(BG_CheckUTWeapon(weaponNum)) {
                float shorten=media->foreShorten!=0?media->foreShorten:1.f;
                if (cg_gun_foreshorten.value > 0.f) shorten = cg_gun_foreshorten.value;
                VectorScale(gun.axis[0],shorten,gun.axis[0]);gun.nonNormalizedAxes=qtrue;
            } else VectorAdd(parent->origin,offset,parent->origin);
            VectorAdd(gun.origin,offset,gun.origin);
        }
	} else {
		drawpart = qtrue;
	}
	if ((!ps || cg.renderingThirdPerson) && (developer.integer || weaponDef[weaponNum].maxammo > 2)) {
		vec3_t adjustment;
		float scale = weaponDef[weaponNum].maxammo > 2 ? 1.075f : 1.0f;
		VectorSet(adjustment, cg_gun_x.value, cg_gun_y.value,
			cg_gun_z.value + (weaponDef[weaponNum].maxammo > 2 ? -1.0f : 0.0f));
		for (i = 0; i < 3; ++i) {
			VectorMA(gun.origin, adjustment[i], gun.axis[i], gun.origin);
			VectorScale(gun.axis[i], scale, gun.axis[i]);
		}
	}
	/* Non-portal scopes hide the model but still consume the shot's casing
	 * at its original tag before returning. */
	if (ps && !cg.renderingThirdPerson && cg.tceAimActive &&
		weaponDef[weaponNum].scoped > 1.f && !cg_portalScopes.integer) {
		if (cent->tceEjectPending) {
			tce_brassContext_t context;
			localEntity_t *fragment;
			qboolean tagsInMain = tce_cg_weapons[weaponNum].tagsInMain != 0;
			if (tagsInMain) {
				refEntity_t casing;
				memset(&casing, 0, sizeof(casing));
				CG_PositionRotatedEntityOnTag(&casing, &gun, "tag_weapon");
				VectorCopy(casing.origin, ejectBrassCasingOrigin);
			}
			memset(&context, 0, sizeof(context));
			context.weapon = weaponNum;
			VectorCopy(cent->tceEntityMotion, context.entityVelocity);
			context.sizeVariant = tceSmokeNewBBox;
			fragment = TCE_CG_AddEjectBrass(&gun, cent, &context, isPlayer, tagsInMain);
			if (fragment) fragment->tceFragment = qtrue;
			cent->tceEjectPending = 0;
		}
		return;
	}

	if( drawpart ) {
		if( weaponNum == 12 ) {
			if( ps ) {
				if( cgs.clientinfo[ ps->clientNum ].skill[ SK_SIGNALS ] >= 1 ) {
					gun.customShader = weapon->modModels[0];
				}
			} else {
				if( cgs.clientinfo[ cent->currentState.clientNum ].skill[ SK_SIGNALS ] >= 1 ) {
					gun.customShader = weapon->modModels[0];
				}
			}
		}
		if( !ps ) {
			if( weaponNum == 11 ) {
				if( cgs.clientinfo[ cent->currentState.clientNum ].skill[ SK_FIRST_AID ] >= 3 ) {
					gun.customShader = weapon->modModels[ 0 ];
				}
			}
		}
		CG_AddWeaponWithPowerups( &gun, cent->currentState.powerups, ps, cent );
	}
	
	if( (!ps || cg.renderingThirdPerson) &&
		(weaponNum == 37 || weaponNum == 53 || weaponNum == 38 || weaponNum == 54) ) {
		// add to other hand as well
		CG_PositionEntityOnTag( &gun, parent, "tag_weapon2", 0, NULL);
		CG_AddWeaponWithPowerups( &gun, cent->currentState.powerups, ps, cent );
	}
	
	// ydnar: test hack
	//%	if( weaponNum == 1 )
	//%		trap_R_AddLightToScene( gun.origin, 512, 1.5, 1.0, 1.0, 1.0, 0, 0 );

	{
		refEntity_t brass;
		qboolean dual = BG_IsAkimboWeapon(weaponNum);
		memset(&brass, 0, sizeof(brass));
		if (isPlayer && !cg.renderingThirdPerson) {
			CG_PositionRotatedEntityOnTag(&brass, parent, !dual || akimboFire ? "tag_brass" : "tag_brass2");
			if (dual && !akimboFire) VectorAdd(brass.origin, tceSecondaryOffset, brass.origin);
		} else if (!ps || cg.renderingThirdPerson) {
			CG_PositionRotatedEntityOnTag(&brass, parent, !dual || akimboFire ? "tag_weapon" : "tag_weapon2");
		}
		if ((isPlayer && !cg.renderingThirdPerson) || !ps || cg.renderingThirdPerson)
			VectorCopy(brass.origin, ejectBrassCasingOrigin);
		if (cent->tceEjectPending && (!isPlayer || cg.renderingThirdPerson || ps) &&
			(!ps || !BG_CheckUTWeapon(weaponNum) || dual)) {
			if (!BG_CheckUTWeapon(weaponNum) && !dual) {
				if (weapon->ejectBrassFunc) weapon->ejectBrassFunc(cent);
			} else {
				tce_brassContext_t context;
				localEntity_t *fragment;
				memset(&context, 0, sizeof(context));
				context.weapon = weaponNum;
				context.sizeVariant = tceSmokeNewBBox;
				VectorCopy(cent->tceEntityMotion, context.entityVelocity);
				fragment = TCE_CG_AddEjectBrass(&gun, cent, &context, isPlayer, dual);
				if (fragment) fragment->tceFragment = qtrue;
			}
			cent->tceEjectPending = 0;
		}
	}

	memset( &barrel, 0, sizeof( barrel ) );
	VectorCopy( parent->lightingOrigin, barrel.lightingOrigin );
	barrel.shadowPlane = parent->shadowPlane;
	barrel.renderfx = parent->renderfx;

	// add barrels
	// attach generic weapon parts to the first person weapon.
	// if a barrel should be attached for third person, add it in the (!ps) section below
	angles[YAW] = angles[PITCH] = 0;

	if( ps ) {
		qboolean spunpart;

		for( i = W_PART_1; i < W_MAX_PARTS; i++ ) {
			if( weaponNum == 60 && ( i == W_PART_4 || i == W_PART_5 ) ) {
				if( ps && !cg.renderingThirdPerson && cg.predictedPlayerState.weaponstate != WEAPON_RAISING ) {
					continue;
				}
			}

			spunpart = qfalse;
			barrel.hModel = weapon->partModels[W_FP_MODEL][i].model;

			if( weaponNum == 60 ) {
				if( i == W_PART_3 ) {
					if( ps && !cg.renderingThirdPerson && cg.predictedPlayerState.weaponstate != WEAPON_RAISING ) {
						angles[PITCH] = angles[YAW] = 0.f;
						angles[ROLL] = .8f * AngleNormalize180( cg.pmext.mountedWeaponAngles[YAW] - ps->viewangles[YAW] );
						spunpart = qtrue;
					}
				} else if( i == W_PART_1 || i == W_PART_2 ) {
					if( ps && !cg.renderingThirdPerson && cg.predictedPlayerState.weaponstate != WEAPON_RAISING ) {
						angles[YAW] = angles[ROLL] = 0.f;
						angles[PITCH] = -.4f * AngleNormalize180( cg.pmext.mountedWeaponAngles[PITCH] - ps->viewangles[PITCH] );
						spunpart = qtrue;
					}
				}
			} else if( weaponNum == 62 ) {

			}

			if( spunpart ) {
				AnglesToAxis( angles, barrel.axis );
			}

			if( barrel.hModel ) {
				if( spunpart ) {
					CG_PositionRotatedEntityOnTag( &barrel, parent, weapon->partModels[W_FP_MODEL][i].tagName );
				} else {
					if (BG_CheckUTWeapon(weaponNum) && (ps->eFlags & 0x01000000)) {
						int component;
						/* The original mutates gun.origin once per drawable UT
						 * part. Do not hoist this cumulative displacement. */
						for (component = 0; component < 3; ++component) {
							float drift = cg.predictedPlayerState.velocity[component] * 0.0033333334140479565f;
							if (drift > 1.0f) drift = 1.0f;
							else if (drift < -1.0f) drift = -1.0f;
							gun.origin[component] += drift * (component == 2 ? 2.0f : 4.0f);
						}
					}
					if (!BG_CheckUTWeapon(weaponNum) && (weaponNum == 37 || weaponNum == 38) && i > 1 && tceRestoreDualFrames) {
						parent->frame = tceDualFrame;
						parent->oldframe = tceDualOldFrame;
					}
					CG_PositionEntityOnTag( &barrel, BG_CheckUTWeapon(weaponNum)?&gun:parent, weapon->partModels[W_FP_MODEL][i].tagName, 0, NULL );
					if (!BG_CheckUTWeapon(weaponNum) && (weaponNum == 37 || weaponNum == 38) && i > 1)
						VectorAdd(barrel.origin, tceSecondaryOffset, barrel.origin);
				}

				if (BG_CheckUTWeapon(weaponNum) && i == 0 && tce_cg_weapons[weaponNum].tagsInMain) {
					memset(&tceBrassParent, 0, sizeof(tceBrassParent));
					tceBrassParent.hModel = barrel.hModel;
					VectorCopy(barrel.origin, tceBrassParent.origin);
					AxisCopy(barrel.axis, tceBrassParent.axis);
					tceBrassParentReady = qtrue;
				}
				/* Original part modFlags consume the selected gear slot and
				 * ps+0x10c (stats15). Other slots do not filter these flags. */
				{
					int flags = tce_cg_weapons[weaponNum].partModels[W_FP_MODEL][i].modFlags;
					int slot = gearDef.slot[weaponNum];
					int first = slot == 1 ? 2 : 0x20;
					int second = slot == 1 ? 4 : 0x40;
					if (flags && (slot == 1 || slot == 2) &&
						(((flags & 1) && !(ps->stats[15] & first)) ||
						 ((flags & 2) && (ps->stats[15] & first)) ||
						 ((flags & 4) && !(ps->stats[15] & second)) ||
						 ((flags & 8) && (ps->stats[15] & second)))) continue;
				}
				drawpart = BG_CheckUTWeapon(weaponNum) ? qtrue :
					CG_GetPartFramesFromWeap( cent, &barrel, parent, i, weapon );
                

					
				if( weaponNum == 60 && (i == W_PART_1 || i == W_PART_2) ) {
					if( ps && !cg.renderingThirdPerson && cg.predictedPlayerState.weaponstate != WEAPON_RAISING ) {
						VectorMA( barrel.origin, .5f * angles[PITCH], cg.refdef_current->viewaxis[0], barrel.origin );
					}
				}

				if( drawpart ) {
					if( ( ps->persistant[PERS_TEAM] == TEAM_AXIS ||
						( ps->persistant[PERS_TEAM] == TEAM_ALLIES && cent->currentState.powerups & (1 << PW_OPS_DISGUISED) ) ) &&
						weapon->partModels[W_FP_MODEL][i].skin[TEAM_AXIS] )
						barrel.customSkin = weapon->partModels[W_FP_MODEL][i].skin[TEAM_AXIS];
					else if( ( ps->persistant[PERS_TEAM] == TEAM_ALLIES ||
							 ( ps->persistant[PERS_TEAM] == TEAM_AXIS && cent->currentState.powerups & (1 << PW_OPS_DISGUISED) ) ) &&
							 weapon->partModels[W_FP_MODEL][i].skin[TEAM_ALLIES] )
						barrel.customSkin = weapon->partModels[W_FP_MODEL][i].skin[TEAM_ALLIES];
					else
						barrel.customSkin = weapon->partModels[W_FP_MODEL][i].skin[0];	// if not loaded it's 0 so doesn't do any harm
					if ((weaponNum == 37 || weaponNum == 38) && i == 2)
						barrel.customSkin = gun.customSkin;

					if( weaponNum == 11 && i == W_PART_1 ) {
						if( cgs.clientinfo[ ps->clientNum ].skill[ SK_FIRST_AID ] >= 3 ) {
							barrel.customShader = weapon->modModels[ 0 ];
						}
					}

					{
						const tce_partModel_t *part=&tce_cg_weapons[weaponNum].partModels[W_FP_MODEL][i];
						qboolean aimed=cg.tceAimActive && cg.tceAimComplete &&
							!cg.tceScopeBlocked && !cg.renderingThirdPerson;
						if (aimed && cg_portalScopes.integer && weaponDef[weaponNum].scoped>1.f) {
							if (part->portalScope) cg.tcePortalScopeEntity=barrel;
							else if (!part->noPortalScope)
								CG_AddWeaponWithPowerups(&barrel,cent->currentState.powerups,ps,cent);
						} else if (!(part->tacView && !aimed) && !(part->noTacView && aimed)) {
							CG_AddWeaponWithPowerups(&barrel,cent->currentState.powerups,ps,cent);
						}
					}

					if( weaponNum == 28 && i == W_PART_1 ) {
						float rangeSquared;
						qboolean inRange;
						refEntity_t satchelDetPart;

						if( cg.satchelCharge ) {
							rangeSquared = DistanceSquared( cg.satchelCharge->lerpOrigin, cg.predictedPlayerEntity.lerpOrigin );
						} else {
							rangeSquared = Square(2001.f);
						}

						if( rangeSquared <= Square(2000) ) {
							inRange = qtrue;
						} else {
							inRange = qfalse;
						}

						memset( &satchelDetPart, 0, sizeof( satchelDetPart ) );
						VectorCopy( parent->lightingOrigin, satchelDetPart.lightingOrigin );
						satchelDetPart.shadowPlane = parent->shadowPlane;
						satchelDetPart.renderfx = parent->renderfx;

						satchelDetPart.hModel = weapon->modModels[0];
						CG_PositionEntityOnTag( &satchelDetPart, &barrel, "tag_rlight", 0, NULL);
						satchelDetPart.customShader = inRange ? weapon->modModels[2] : weapon->modModels[3];
						CG_AddWeaponWithPowerups( &satchelDetPart, cent->currentState.powerups, ps, cent );

						CG_PositionEntityOnTag( &satchelDetPart, &barrel, "tag_glight", 0, NULL);
						satchelDetPart.customShader = inRange ? weapon->modModels[5] : weapon->modModels[4];
						CG_AddWeaponWithPowerups( &satchelDetPart, cent->currentState.powerups, ps, cent );

						satchelDetPart.hModel = weapon->modModels[1];
						angles[PITCH] = angles[ROLL] = 0.f;
						if( inRange ) {
 							angles[YAW] = -30.f + ( 60.f * ( rangeSquared / Square(2000) ) );
						} else {
							angles[YAW] = 30.f;
						}
						AnglesToAxis( angles, satchelDetPart.axis );
						CG_PositionRotatedEntityOnTag( &satchelDetPart, &barrel, "tag_needle" );
						satchelDetPart.customShader = weapon->modModels[2];
						CG_AddWeaponWithPowerups( &satchelDetPart, cent->currentState.powerups, ps, cent );
					} else if( weaponNum == 60 && i == W_PART_3 ) {
						if( ps && !cg.renderingThirdPerson && cg.predictedPlayerState.weaponstate != WEAPON_RAISING ) {
							refEntity_t bipodLeg;

							memset( &bipodLeg, 0, sizeof( bipodLeg ) );
							VectorCopy( parent->lightingOrigin, bipodLeg.lightingOrigin );
							bipodLeg.shadowPlane = parent->shadowPlane;
							bipodLeg.renderfx = parent->renderfx;

							bipodLeg.hModel = weapon->partModels[W_FP_MODEL][3].model;
							CG_PositionEntityOnTag( &bipodLeg, &barrel, "tag_barrel4", 0, NULL);
							CG_AddWeaponWithPowerups( &bipodLeg, cent->currentState.powerups, ps, cent );

							bipodLeg.hModel = weapon->partModels[W_FP_MODEL][4].model;
							CG_PositionEntityOnTag( &bipodLeg, &barrel, "tag_barrel5", 0, NULL);
							CG_AddWeaponWithPowerups( &bipodLeg, cent->currentState.powerups, ps, cent );
						}
					}
				}
			}
		}
	}

    /* TC custom weapons defer ejection until their animated tag exists.
     * The local third-person entity must not consume the first-person event. */
    if(weaponNum>0 && weaponNum<TCE_MAX_WEAPONS && cent->tceEjectPending && ps) {
        const refEntity_t *ejectParent=&gun;
        if(ps && tce_cg_weapons[weaponNum].tagsInMain)
            ejectParent=tceBrassParentReady?&tceBrassParent:NULL;
        if(ejectParent) {
            tce_brassContext_t context;
            localEntity_t *fragment;
            memset(&context,0,sizeof(context));context.weapon=weaponNum;
            VectorCopy(cent->tceEntityMotion,context.entityVelocity);
            context.sizeVariant=tceSmokeNewBBox;
            fragment=TCE_CG_AddEjectBrass(ejectParent,cent,&context,isPlayer,qfalse);
            if(fragment) {
                fragment->tceFragment=qtrue;
                if(cg_debugEvents.integer==2)
                    CG_Printf("TCE brass entity %d weapon %d frame %d origin %.2f %.2f %.2f\n",
                        cent->currentState.number,weaponNum,ejectParent->frame,
                        fragment->refEntity.origin[0],fragment->refEntity.origin[1],fragment->refEntity.origin[2]);
            }
            cent->tceEjectPending=0;
        }
    }

	/* TC has no SDK rifle-scope/riflegrenade attachment block here. */

	// make sure we aren't looking at cg.predictedPlayerEntity for LG
	nonPredictedCent = &cg_entities[cent->currentState.clientNum]; 

	// if the index of the nonPredictedCent is not the same as the clientNum
	// then this is a fake player (like on the single player podiums), so
	// go ahead and use the cent
	if( ( nonPredictedCent - cg_entities ) != cent->currentState.clientNum ) {
		nonPredictedCent = cent;
	}

	// add the flash
	memset( &flash, 0, sizeof( flash ) );
	VectorCopy( parent->lightingOrigin, flash.lightingOrigin );
	flash.shadowPlane = parent->shadowPlane;
	flash.renderfx = parent->renderfx;

	if(ps)
		flash.hModel = weapon->flashModel[W_FP_MODEL];
	else
		flash.hModel = weapon->flashModel[W_TP_MODEL];

	angles[YAW]		= 0;
	angles[PITCH]	= 0;
	angles[ROLL]	= crandom() * 10;
	AnglesToAxis( angles, flash.axis );

	if( /*isPlayer &&*/ BG_IsAkimboWeapon( weaponNum ) ) {
		if( !ps || cg.renderingThirdPerson ) {
			if( !cent->akimboFire ) {
				CG_PositionRotatedEntityOnTag( &flash, parent, "tag_weapon");
				VectorMA( flash.origin, 10, flash.axis[0], flash.origin );
			} else {
				CG_PositionRotatedEntityOnTag( &flash, &gun, "tag_flash");
			}
		} else {
			if( !cent->akimboFire ) {
				CG_PositionRotatedEntityOnTag( &flash, parent, "tag_flash2");

			} else {
				CG_PositionRotatedEntityOnTag( &flash, parent, "tag_flash");
				VectorAdd(flash.origin, tceSecondaryOffset, flash.origin);
			}
		}
	} else {
		CG_PositionRotatedEntityOnTag( &flash,
			(tce_cg_weapons[weaponNum].tagsInMain && isPlayer && !cg.renderingThirdPerson && tceBrassParentReady)
			? &tceBrassParent : &gun, "tag_flash");
	}

	// store this position for other cgame elements to access
	cent->pe.gunRefEnt = gun;
	cent->pe.gunRefEntFrame = cg.clientFrame;

	if ( ( weaponNum == 66 ) && ( nonPredictedCent->currentState.eFlags & EF_FIRING) )
	{
		// continuous flash

	} else {

		// continuous smoke after firing
#define BARREL_SMOKE_TIME 1000

		if ( ps || cg.renderingThirdPerson || !isPlayer ) {
			if( weaponNum == 10 || weaponNum == 31 || weaponNum == 62 ) {
				// hot smoking gun
				if(cg.time - cent->overheatTime < 3000) {
					if(!(rand()%3)) {
						float alpha;
						alpha = 1.0f - ((float)(cg.time - cent->overheatTime)/3000.0f);
						alpha *= 0.25f;		// .25 max alpha
						CG_ParticleImpactSmokePuffExtended(cgs.media.smokeParticleShader, flash.origin, 1000, 8, 20, 30, alpha, 8.f);
					}
				}

			} else if( weaponNum == 65 || weaponNum == 46 ) {
				if (cg.time - cent->muzzleFlashTime < BARREL_SMOKE_TIME) {
					if(!(rand()%5)) {
						float alpha;
						alpha = 1.0f - ((float)(cg.time - cent->muzzleFlashTime)/(float)BARREL_SMOKE_TIME);	// what fraction of BARREL_SMOKE_TIME are we at
						alpha *= 0.25f;		// .25 max alpha
						CG_ParticleImpactSmokePuffExtended(cgs.media.smokeParticleShader, flash.origin, 1000, 8, 20, 30, alpha, 8.f);
					}
				}
			}
		}

		if( weaponNum == 60 ) {
			if( ps && !cg.renderingThirdPerson && cg.time - cent->muzzleFlashTime < 800 ) {
				CG_ParticleImpactSmokePuffExtended( cgs.media.smokeParticleShader, flash.origin, 700, 16, 20, 30, .12f, 4.f );
			}
		}

		// Original non-continuous flash expires even for a stopped flamer.
		if (cg.time - cent->muzzleFlashTime > 30) return;

	}

	// weapons that don't need to go any further as they have no flash or light
	if(	weaponNum == 4 ||
		weaponNum == 9 ||
		weaponNum == 1 ||
		weaponNum == 15 ||
		weaponNum == 55 ||
		weaponNum == 56 ||
		weaponNum == 26 ||
		weaponNum == 27 ||
		weaponNum == 28 ||
		weaponNum == 29 ||
		weaponNum == 30 ||
		weaponNum == 11 ||
		weaponNum == 61
		)
	{
		return;
	}

	/* Original world exposure modulates the flash, with a longer recent
	 * aim-transition window only for weapon46. */
	if (flash.hModel && weaponNum != 66) {
		int lifetime = ps && !cg.renderingThirdPerson && weaponNum == 46 &&
			cg.time - cg.zoomTime < 600 ? 50 : 30;
		int color = (int)((double)cg.tceSparkIntensity * 255.0);
		flash.shaderRGBA[0] = flash.shaderRGBA[1] = flash.shaderRGBA[2] = (byte)color;
		flash.shaderRGBA[3] = 255;
		if (cg.time - cent->muzzleFlashTime < lifetime) trap_R_AddRefEntityToScene(&flash);
	}

	// Ridah, zombie fires from his head
	//if (CG_MonsterUsingWeapon( cent, AICHAR_ZOMBIE, WP_MONSTER_ATTACK1 )) {
	//	CG_PositionEntityOnTag( &flash, parent, parent->hModel, "tag_head", NULL);
	//}
	
	if ( ps || cg.renderingThirdPerson || !isPlayer )
	{
		// ydnar: no flamethrower flame on prone moving
		// ydnar: or dead players
		if( firing && !(cent->currentState.eFlags & (EF_PRONE_MOVING | EF_DEAD)) )
		{
			// Ridah, Flamethrower effect
			CG_FlamethrowerFlame( cent, flash.origin );

		} else {
			if ( weaponNum == 66 ) {
				vec3_t angles;
				AxisToAngles( flash.axis, angles );
// JPW NERVE
				weaponNum = BG_FindAmmoForWeapon(66);
				if (ps) {
					if (ps->ammoclip[weaponNum])
						CG_FireFlameChunks( cent, flash.origin, angles, 1.0, qfalse );
				}
				else {
					CG_FireFlameChunks( cent, flash.origin, angles, 1.0, qfalse );
				}
// jpw
			}
		}
	}

    /* CG_AddPlayerWeapon3006f4b0: consume the shot's deferred muzzle smoke
     * only in the view which owns this weapon; underwater/intermission shots
     * still consume the request rather than displaying it on a later frame. */
    if (cent->tceFireEffectPending && (ps || cg.renderingThirdPerson ||
        cent->currentState.number != cg.predictedPlayerState.clientNum)) {
        if (cgs.gamestate != 3) {
            vec3_t direction, smokeOrigin;
            tce_impactParticleMedia_t media;
            int type;
            float alpha = 1.0f;
            AngleVectors(cent->lerpAngles, direction, NULL, NULL);
            VectorNormalize(direction);
            VectorMA(flash.origin, 18.0f, direction, smokeOrigin);
            if (!(trap_CM_PointContents(smokeOrigin, 0) & CONTENTS_WATER)) {
                if (!ps || cg.renderingThirdPerson) {
                    type = weaponDef[cent->currentState.weapon].unknown_1b0 == 2 ? 40 : 37;
                    if (type == 37) alpha = 0.6f;
                } else {
                    type = weaponDef[cent->currentState.weapon].unknown_1b0 == 2 ? 39 : 41;
                    VectorCopy(flash.origin, smokeOrigin);
                }
                media.blood = cgs.media.bloodTrailShader;
                media.smoke3 = cgs.media.tceImpactSmokePuff3;
                media.smoke4 = cgs.media.tceImpactSmokePuff4;
                TCE_CG_ParticleTest(smokeOrigin, direction, 0, type, alpha, &media);
            }
        }
        cent->tceFireEffectPending = 0;
    }
}

/*
==============
CG_AddViewWeapon

Add the weapon, and flash for the player's view
==============
*/
void CG_AddViewWeapon( playerState_t *ps ) {
	refEntity_t	hand;
	vec3_t		angles;
	vec3_t		gunoff;
	weaponInfo_t	*weapon;
	/* Portal capture is a native adapter cache; clear before every view pass. */
	cg.tcePortalScopeEntity.hModel=0;

	if ( ps->persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		return;
	}

	if ( ps->pm_type == PM_INTERMISSION ) {
		return;
	}

	// no gun if in third person view
	if ( cg.renderingThirdPerson ) {
		return;
	}

	if( cg.editingSpeakers ) {
		return;
	}

	// allow the gun to be completely removed
	if( !cg_drawGun.integer && developer.integer ) {
		vec3_t		origin;

		//bani - #589
		if ( cg.predictedPlayerState.eFlags & EF_FIRING && !( cg.predictedPlayerState.eFlags & ( EF_MG42_ACTIVE | EF_MOUNTEDTANK ) ) ) {
			// special hack for flamethrower...
			VectorCopy( cg.refdef_current->vieworg, origin );

			VectorMA( origin, 18, cg.refdef_current->viewaxis[0], origin );
			VectorMA( origin, -7, cg.refdef_current->viewaxis[1], origin );
			VectorMA( origin, -4, cg.refdef_current->viewaxis[2], origin );

			// Ridah, Flamethrower effect
			CG_FlamethrowerFlame( &cg.predictedPlayerEntity, origin );
		}

		if( cg.binocZoomTime ) {
			if( cg.binocZoomTime < 0 ) {
				if( -cg.binocZoomTime + 500 + 200 < cg.time ) {
					cg.binocZoomTime = 0;
				}
			} else {
				if( cg.binocZoomTime + 500 < cg.time ) {
					trap_SendConsoleCommand( "+zoom\n" );
					cg.binocZoomTime = 0;
				} else {
				}
			}
		}

		return;
	}

	// don't draw if testing a gun model
	if ( cg.testGun ) {
		return;
	}

	if ( ps->eFlags & EF_MG42_ACTIVE || ps->eFlags & EF_AAGUN_ACTIVE ) {
		return;
	}

	// Gordon: mounted gun drawing
	if( ps->eFlags & EF_MOUNTEDTANK ) {
		// FIXME: Arnout: HACK dummy model to just draw _something_
		refEntity_t		flash;

		memset( &hand, 0, sizeof( hand ) );
		CG_CalculateWeaponPosition( hand.origin, angles );
		AnglesToAxis( angles, hand.axis );
		hand.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON | RF_MINLIGHT;

		if( cg_entities[cg_entities[cg_entities[ ps->clientNum ].tagParent].tankparent].currentState.density & 8 ) { // should we use a browning?
			hand.hModel = cgs.media.hMountedFPBrowning;
		} else {
			hand.hModel = cgs.media.hMountedFPMG42;
		}

		//gunoff[0] = cg_gun_x.value;
		//gunoff[1] = cg_gun_y.value;
		//gunoff[2] = cg_gun_z.value;

		gunoff[0] = 20;
		if ( cg.time - cg.predictedPlayerEntity.muzzleFlashTime < MUZZLE_FLASH_TIME ) {
			gunoff[0] += random() * 2.f;
		}
		{
			int component;
			for (component = 0; component < 3; ++component)
				hand.origin[component] = (float)(((double)gunoff[0] * cg.refdef_current->viewaxis[0][component]
					+ hand.origin[component]) - (double)cg.refdef_current->viewaxis[1][component] * 10.0
					- (double)cg.refdef_current->viewaxis[2][component] * 8.0);
		}

		CG_AddWeaponWithPowerups( &hand, cg.predictedPlayerEntity.currentState.powerups, ps, &cg.predictedPlayerEntity );

		if( cg.time - cg.predictedPlayerEntity.overheatTime < 3000 ) {
			if(!(rand()%3)) {
				float alpha;
				alpha = 1.0f - ((float)(cg.time - cg.predictedPlayerEntity.overheatTime)/3000.0f);
				alpha *= 0.25f;		// .25 max alpha
				CG_ParticleImpactSmokePuffExtended( cgs.media.smokeParticleShader, cg.tankflashorg, 1000, 8, 20, 30, alpha, 8.f);
			}
		}

		{
			memset (&flash, 0, sizeof (flash));
			flash.renderfx = (RF_LIGHTING_ORIGIN|RF_DEPTHHACK);
			flash.hModel = cgs.media.mg42muzzleflash;

			angles[YAW]		= 0;
			angles[PITCH]	= 0;
			angles[ROLL]	= crandom() * 10;
			AnglesToAxis( angles, flash.axis );

			CG_PositionRotatedEntityOnTag( &flash, &hand, "tag_flash");

            VectorMA( flash.origin, 22, flash.axis[0], flash.origin );

			VectorCopy( flash.origin, cg.tankflashorg );

			if ( cg.time - cg.predictedPlayerEntity.muzzleFlashTime < MUZZLE_FLASH_TIME ) { 
				trap_R_AddRefEntityToScene( &flash );
			}
		}
		return;
	}

	if ( ps->weapon > WP_NONE) {
		weapon = &cg_weapons[ ps->weapon ];

		memset (&hand, 0, sizeof(hand));

		// set up gun position
		CG_CalculateWeaponPosition( hand.origin, angles );

		gunoff[0] = cg_gun_x.value + 3.f;
		gunoff[1] = cg_gun_y.value;
		gunoff[2] = cg_gun_z.value - 4.f;

//----(SA)	removed

		AnglesToAxis( angles, hand.axis );
		{
			int component;
			for (component=0;component<3;++component)
				hand.origin[component]=(float)((double)gunoff[2]*hand.axis[2][component]
					+(double)gunoff[1]*hand.axis[1][component]
					+(double)gunoff[0]*hand.axis[0][component]+hand.origin[component]);
		}

		if ( cg_gun_frame.integer ) {
			hand.frame = hand.oldframe = cg_gun_frame.integer;
			hand.backlerp = 0;
		}
		else {	// get the animation state
			if( cg.binocZoomTime ) {
				if( cg.binocZoomTime < 0 ) {
					if( -cg.binocZoomTime + 500 + 200 < cg.time ) {
						cg.binocZoomTime = 0;
					} else {
						if( -cg.binocZoomTime + 200 < cg.time ) {
							CG_ContinueWeaponAnim( WEAP_ALTSWITCHFROM );
						} else {
							CG_ContinueWeaponAnim( WEAP_IDLE2 );
						}
					}
				} else {
					if( cg.binocZoomTime + 500 < cg.time ) {
						trap_SendConsoleCommand( "+zoom\n" );
						cg.binocZoomTime = 0;
						CG_ContinueWeaponAnim( WEAP_IDLE2 );
					} else {
						CG_ContinueWeaponAnim( WEAP_ALTSWITCHTO );
					}
				}
			}
			CG_WeaponAnimation( ps, weapon, &hand.oldframe, &hand.frame, &hand.backlerp);	//----(SA)	changed
		}


        if(!BG_CheckUTWeapon(ps->weapon)) {
            tce_weaponInfo_t *media=&tce_cg_weapons[ps->weapon];
            float shorten=media->foreShorten!=0?media->foreShorten:1.f;
            if(cg_gun_foreshorten.value>0.f)shorten=cg_gun_foreshorten.value;
            VectorScale(hand.axis[0],shorten,hand.axis[0]);hand.nonNormalizedAxes=qtrue;
        }
		hand.hModel = weapon->handsModel;
		hand.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON | RF_MINLIGHT;	//----(SA)	

		// add everything onto the hand
		CG_AddPlayerWeapon( &hand, ps, &cg.predictedPlayerEntity);
		// Ridah

	}
}

/*
==============================================================================

WEAPON SELECTION

==============================================================================
*/

#define WP_ICON_X		38	// new sizes per MK
#define WP_ICON_X_WIDE	72	// new sizes per MK
#define WP_ICON_Y		38
#define WP_ICON_SPACE_Y 10
#define WP_DRAW_X		640 - WP_ICON_X - 4	// 4 is 'selected' border width		
#define WP_DRAW_X_WIDE	640 - WP_ICON_X_WIDE - 4
#define WP_DRAW_Y		4

// secondary fire icons
#define WP_ICON_SEC_X	18	// new sizes per MK
#define WP_ICON_SEC_Y	18

/*
==============
CG_WeaponHasAmmo
	check for ammo
==============
*/
static qboolean CG_WeaponHasAmmo( int i )
{
	// ydnar: certain weapons don't have ammo
	if( i == WP_KNIFE || i == WP_PLIERS )
		return qtrue;
	
	if (!(cg.predictedPlayerState.ammo[BG_FindAmmoForWeapon(i)]) &&
		!(cg.predictedPlayerState.ammoclip[BG_FindClipForWeapon(i)]) ) {
		return qfalse;
	}

	return qtrue;
}

/*
===============
CG_WeaponSelectable
===============
*/
qboolean CG_WeaponSelectable( int i ) {

	// allow the player to unselect all weapons
//	if(i == WP_NONE)
//		return qtrue;

	// if holding a melee weapon (chair/shield/etc.) only allow single-handed weapons
/*	if(cg.snap->ps.eFlags & EF_MELEE_ACTIVE) {
		if(!(WEAPS_ONE_HANDED & (1<<i)))
			return qfalse;
	}*/

	if( BG_PlayerMounted( cg.predictedPlayerState.eFlags ) ) {
		return qfalse;
	}

	// check for weapon
	if (! (COM_BitCheck( cg.predictedPlayerState.weapons, i ) ) ) {
		return qfalse;
	}

	/* Original TC selection depends on ownership, including empty weapons. */

	return qtrue;
}

/*
==============
CG_WeaponIndex
==============
*/
int CG_WeaponIndex( int weapnum, int *bank, int *cycle) {
	static int bnk, cyc;

	if(weapnum <=0 || weapnum >= TCE_MAX_WEAPONS) {
		if(bank)	*bank = 0;
		if(cycle)	*cycle = 0;
		return 0;
	}

	for(bnk = 0; bnk < MAX_WEAP_BANKS_MP; bnk++) {
		for(cyc = 0; cyc < MAX_WEAPS_IN_BANK_MP; cyc++) {

				if(!weapBanksMultiPlayer[bnk][cyc])
					break;

				// found the current weapon
				if(weapBanksMultiPlayer[bnk][cyc] == weapnum) {
					if(bank)	*bank = bnk;
					if(cycle)	*cycle = cyc;
					return 1;
				}
// jpw
		}
	}

	// failed to find the weapon in the table
	// probably an alternate

	return 0;
}

/*
==============
getNextWeapInBank
	Pass in a bank and cycle and this will return the next valid weapon higher in the cycle.
	if the weap passed in is above highest in a cycle (MAX_WEAPS_IN_BANK), this will safely loop around
==============
*/
static int getNextWeapInBank( int bank, int cycle ) {

	cycle++;

	cycle = cycle % MAX_WEAPS_IN_BANK_MP;

		if(weapBanksMultiPlayer[bank][cycle])		// return next weapon in bank if there is one
			return weapBanksMultiPlayer[bank][cycle];
		else								// return first in bank
			return weapBanksMultiPlayer[bank][0];
}

static int getNextWeapInBankBynum( int weapnum ) {
	int bank, cycle;

	if(!CG_WeaponIndex(weapnum, &bank, &cycle))
		return weapnum;

	return getNextWeapInBank(bank, cycle);
}

/*
==============
getPrevWeapInBank
	Pass in a bank and cycle and this will return the next valid weapon lower in the cycle.
	if the weap passed in is the lowest in a cycle (0), this will loop around to the
	top (MAX_WEAPS_IN_BANK-1) and start down from there looking for a valid weapon position
==============
*/
static int getPrevWeapInBank( int bank, int cycle ) {
	cycle--;
	if(cycle < 0)
		cycle = MAX_WEAPS_IN_BANK_MP - 1;

	
		while(!weapBanksMultiPlayer[bank][cycle]) {
			cycle--;

			if(cycle < 0)
			cycle = MAX_WEAPS_IN_BANK_MP - 1;
		}
	return weapBanksMultiPlayer[bank][cycle];
}


static int getPrevWeapInBankBynum( int weapnum ) {
	int bank, cycle;

	if(!CG_WeaponIndex(weapnum, &bank, &cycle))
		return weapnum;

	return getPrevWeapInBank(bank, cycle);
}



/*
==============
getNextBankWeap
	Pass in a bank and cycle and this will return the next valid weapon in a higher bank.
	sameBankPosition: if there's a weapon in the next bank at the same cycle,
	return that	(colt returns thompson for example) rather than the lowest weapon
==============
*/
static int getNextBankWeap( int bank, int cycle, qboolean sameBankPosition ) {
	bank++;

	bank = bank % MAX_WEAP_BANKS_MP;

		if(sameBankPosition && weapBanksMultiPlayer[bank][cycle])
			return weapBanksMultiPlayer[bank][cycle];
		else
			return weapBanksMultiPlayer[bank][0];
}

/*
==============
getPrevBankWeap
	Pass in a bank and cycle and this will return the next valid weapon in a lower bank.
	sameBankPosition: if there's a weapon in the prev bank at the same cycle,
	return that	(thompson returns colt for example) rather than the highest weapon
==============
*/
static int getPrevBankWeap( int bank, int cycle, qboolean sameBankPosition ) {
	int i;

	bank--;

	if(bank < 0)		// don't go below 0, cycle up to top
		bank += MAX_WEAP_BANKS_MP; // JPW NERVE

	bank = bank % MAX_WEAP_BANKS_MP;

		if(sameBankPosition && weapBanksMultiPlayer[bank][cycle]) {
			return weapBanksMultiPlayer[bank][cycle];
		}
		else
		{	// find highest weap in bank
		for(i = MAX_WEAPS_IN_BANK_MP - 1; i >= 0; i--) {
				if(weapBanksMultiPlayer[bank][i])
					return weapBanksMultiPlayer[bank][i];
			}

			// if it gets to here, no valid weaps in this bank, go down another bank
			return getPrevBankWeap(bank, cycle, sameBankPosition);	
		}
}

/*
==============
getAltWeapon
==============
*/
static int getAltWeapon( int weapnum ) {
/*	if(weapnum > MAX_WEAP_ALTS) Gordon: seems unneeded
		return weapnum;*/

	if(weapAlts[weapnum])
		return weapAlts[weapnum];

	return weapnum;
}

/*
==============
getEquivWeapon
	return the id of the opposite team's weapon.
	Passing the weapnum of the mp40 returns the id of the thompson, and likewise
	passing the weapnum of the thompson returns the id of the mp40.
	No equivalent available will return the weapnum passed in.
==============
*/
int getEquivWeapon( int weapnum ) {
    /* TC:E30074300: numeric protocol IDs, not the SDK weapon enum. */
    switch(weapnum) {
    case 2:return 7; case 7:return 2;
    case 3:return 8; case 8:return 3;
    case 4:return 9; case 9:return 4;
    case 14:return 52; case 52:return 14;
    case 23:return 24; case 24:return 23;
    default:return weapnum;
    }
}




/*
==============
CG_SetSniperZoom
==============
*/

void CG_SetSniperZoom(int lastweap, int newweap) {
    if (lastweap == newweap) return;
    if (!(cg.predictedPlayerState.eFlags & EF_ZOOMING)) cg.zoomval = 0;
    cg.zoomedScope = 0;
    if (newweap == 57 || newweap == 58) cg.zoomedScope = 900;
    else if (newweap == 59) cg.zoomedScope = 1;
    else return;
    cg.zoomval = cg_zoomDefaultSniper.value;
    if (cg.zoomval > 90) cg.zoomval = 90;
    if (cg.zoomval < 60) cg.zoomval = 60;
    cg.zoomTime = cg.time;
}

/*
==============
CG_PlaySwitchSound
	Get special switching sounds if they're there
==============
*/
void CG_PlaySwitchSound(int lastweap, int newweap) {
    sfxHandle_t sound;
    if (getAltWeapon(lastweap) != newweap) return;
    sound = cgs.media.selectSound;
    switch (newweap) {
    case 2: case 7: case 14: case 31: case 35: case 52:
    case 55: case 56: case 60: case 62:
        sound = cg_weapons[newweap].switchSound;
        break;
    case 23: case 24:
        if (cg.predictedPlayerState.ammoclip[lastweap])
            sound = cg_weapons[newweap].switchSound;
        break;
    default: return;
    }
    trap_S_StartSound(NULL, cg.snap->ps.clientNum, CHAN_WEAPON, sound);
}

/*
==============
CG_FinishWeaponChange
==============
*/
void CG_FinishWeaponChange(int lastweap, int newweap) {
    int bank;
    if (cg.binocZoomTime) return;
    cg.mortarImpactTime = -2;
    if (lastweap == 20 && (cg.snap->ps.eFlags & EF_ZOOMING))
        trap_SendConsoleCommand("-zoom\n");
    cg.weaponSelectTime = cg.time;
    if (cg.newCrosshairIndex)
        trap_Cvar_Set("cg_drawCrossHair", va("%d", cg.newCrosshairIndex - 1));
    cg.newCrosshairIndex = 0;
    if (CG_WeaponIndex(newweap, &bank, NULL)) cg.lastWeapSelInBank[bank] = newweap;
    if (lastweap == newweap) return;
    CG_PlaySwitchSound(lastweap, newweap);
    CG_SetSniperZoom(lastweap, newweap);
    if ((lastweap == cg.lastFiredWeapon && !(lastweap >= 57 && lastweap <= 59)) ||
        (lastweap != cg.lastFiredWeapon && cg.switchbackWeapon == newweap))
        cg.switchbackWeapon = lastweap;
    cg.weaponSelect = newweap;
}

extern pmove_t cg_pmove;

/*
==============
CG_AltfireWeapon_f
	for example, switching between WP_MAUSER and WP_SNIPERRIFLE
==============
*/
void CG_AltWeapon_f(void)
{
	int original, num;

	if ( !cg.snap ) {
		return;
	}

	// Overload for spec mode when following
	if((cg.snap->ps.pm_flags & PMF_FOLLOW) || cg.mvTotalClients > 0) {
		return;
	}

	// Need ground for this
	if( cg.weaponSelect == WP_MORTAR ) {
		int contents;
		vec3_t point;

		if( cg.predictedPlayerState.groundEntityNum == ENTITYNUM_NONE )
			return;
		if( !cg.predictedPlayerState.ammoclip[WP_MORTAR] )
			return;

		if( cg.predictedPlayerState.eFlags & EF_PRONE ) {
			return;
		}

		if( cg_pmove.waterlevel == 3 ) {
			return;
		}
		
		// ydnar: don't allow set if moving
		if( VectorLengthSquared( cg.snap->ps.velocity ) )
			return;
		
		// eurgh, need it here too else we play sounds :/
		point[0] = cg.snap->ps.origin[0];
		point[1] = cg.snap->ps.origin[1];
		point[2] = cg.snap->ps.origin[2] + cg.snap->ps.crouchViewHeight;
		contents = CG_PointContents( point, cg.snap->ps.clientNum );
		if ( contents & MASK_WATER ) {
			return;
		}
	} else if( cg.weaponSelect == WP_MOBILE_MG42 ) {
		if( !(cg.predictedPlayerState.eFlags & EF_PRONE) ) {
			return;
		}
	}

	if(cg.time - cg.weaponSelectTime < cg_weaponCycleDelay.integer)
		return;	// force pause so holding it down won't go too fast

	// Don't try to switch when in the middle of reloading.
	if ( cg.snap->ps.weaponstate == WEAPON_RELOADING )
		return;

	original = cg.weaponSelect;

	num = getAltWeapon(original);

	if( original == WP_BINOCULARS ) {
		/*if(cg.snap->ps.eFlags & EF_ZOOMING) {
            trap_SendConsoleCommand( "-zoom\n" );
		} else {
			trap_SendConsoleCommand( "+zoom\n" );
		}*/
		if(cg.snap->ps.eFlags & EF_ZOOMING) {
			trap_SendConsoleCommand( "-zoom\n" );
			cg.binocZoomTime = -cg.time;
		} else {
			if( !cg.binocZoomTime )
				cg.binocZoomTime = cg.time;
		}
	}

	// Arnout: don't allow another weapon switch when we're still swapping the gpg40, to prevent animation breaking
	if( ( cg.snap->ps.weaponstate == WEAPON_RAISING || cg.snap->ps.weaponstate == WEAPON_DROPPING ) &&
		( (original == WP_GPG40 || num == WP_GPG40 || original == WP_M7 || num == WP_M7) ||
		  (original == WP_SILENCER || num == WP_SILENCER || original == WP_SILENCED_COLT || num == WP_SILENCED_COLT) ||
		  (original == WP_AKIMBO_SILENCEDCOLT || num == WP_AKIMBO_SILENCEDCOLT || original == WP_AKIMBO_SILENCEDLUGER || num == WP_AKIMBO_SILENCEDLUGER) ||
		  (original == WP_MORTAR_SET || num == WP_MORTAR_SET) ||
		  (original == WP_MOBILE_MG42_SET || num == WP_MOBILE_MG42_SET) ) )
		return;

	if( CG_WeaponSelectable( num ) ) {	// new weapon is valid
		CG_FinishWeaponChange(original, num);
	}
}


/*
==============
CG_NextWeap

  switchBanks - curweap is the last in a bank, 'qtrue' means go to the next available bank, 'qfalse' means loop to the head of the bank
==============
*/
void CG_NextWeap(qboolean switchBanks) {
	int			bank = 0, cycle = 0, newbank = 0, newcycle = 0;
	int			num, curweap;
	qboolean	nextbank = qfalse;	// need to switch to the next bank of weapons?
	int			i, j;

	num = curweap = cg.weaponSelect;

	/* Windows TC controllers30074870/30074c10 use numeric protocol IDs. */
	if( curweap == 60 || curweap == 62 || (cg.snap->ps.eFlags & 0x1000000) )
		return;

	switch(num) {
		case 14:
			curweap = num = 2;
			break;
		case 52:
			curweap = num = 7;
			break;
		case 55:
			curweap = num = 23;
			break;
		case 56:
			curweap = num = 24;
			break;
		case 60:
			curweap = num = 35;
			break;
	}

	CG_WeaponIndex(curweap, &bank, &cycle);		// get bank/cycle of current weapon

	// if you're using an alt mode weapon, try switching back to the parent first
	if(curweap >= 55 && curweap <= 59) {
		num = getAltWeapon(curweap);	// base any further changes on the parent
		if(CG_WeaponSelectable(num)) {	// the parent was selectable, drop back to that
			CG_FinishWeaponChange(curweap, num);
			return;
		}
	}

	if(cg_cycleAllWeaps.integer || !switchBanks) {
		for(i = 0; i < MAX_WEAPS_IN_BANK_MP; i++) {
			num = getNextWeapInBankBynum(num);

			CG_WeaponIndex(num, NULL, &newcycle);		// get cycle of new weapon.  if it's lower than the original, then it cycled around

			if(switchBanks) {
				if(newcycle <= cycle) {
					nextbank = qtrue;
					break;
				}
			} else {	// don't switch banks if you get to the end

				if(num == curweap) {	// back to start, just leave it where it is
					return;
				}
			}

			if( CG_WeaponSelectable( num ) ) {				
				break;
			} else {
				qboolean found = qfalse;
				switch( num ) {
					case 24:
						if ((found = CG_WeaponSelectable( 56 ))) {
							num = 56;
						}
						break;
					case 23:
						if ((found = CG_WeaponSelectable( 55 ))) {
							num = 55;
						}
						break;
				}

				if( found ) {
					break;
				}
			}
		}
	} else {
		nextbank = qtrue;
	}

	if( nextbank ) {
		for(i = 0; i < MAX_WEAP_BANKS_MP; i++) {
			if(cg_cycleAllWeaps.integer)
				num = getNextBankWeap(bank+i, cycle, qfalse);	// cycling all weaps always starts the next bank at the bottom
			else {
				/* The original reads past this array on an exhausted bank scan.
                 * Ignore nonexistent remembered banks instead of reading other cg fields. */
                if(bank+i+1 < MAX_WEAP_BANKS_MP && cg.lastWeapSelInBank[bank+i+1])
					num = cg.lastWeapSelInBank[bank+i+1];
				else
					num = getNextBankWeap(bank+i, cycle, qtrue);
			}

			if(num == 0)
				continue;

//			if(num == WP_BINOCULARS) {
//				continue;
//			}

			if( CG_WeaponSelectable( num ) ) { // first entry in bank was selectable, no need to scan the bank
				break;
			} else {
				qboolean found = qfalse;
				switch( num ) {
					case 24:
						if ((found = CG_WeaponSelectable( 56 ))) {
							num = 56;
						}
						break;
					case 23:
						if ((found = CG_WeaponSelectable( 55 ))) {
							num = 55;
						}
						break;
				}

				if( found ) {
					break;
				}
			}

			CG_WeaponIndex(num, &newbank, &newcycle);	// get the bank of the new weap
	
			for(j = newcycle; j < MAX_WEAPS_IN_BANK_MP; j++) {
				num = getNextWeapInBank(newbank, j);

/*				if(num == WP_BINOCULARS) {
					continue;
				}*/

				if( CG_WeaponSelectable( num ) ) { // found selectable weapon
					break;
				} else {
					qboolean found = qfalse;
					switch( num ) {
						case 24:
							if ((found = CG_WeaponSelectable( 56 ))) {
								num = 56;
							}
							break;
						case 23:
							if ((found = CG_WeaponSelectable( 55 ))) {
								num = 55;
							}
							break;
					}

					if( found ) {
						break;
					}
				}

				num = 0;	
			}

			if( num ) { // a selectable weapon was found in the current bank
				break;
			}
		}
	}

	CG_FinishWeaponChange( curweap, num );//----(SA)	
}

/*
==============
CG_PrevWeap

  switchBanks - curweap is the last in a bank
		'qtrue'  - go to the next available bank
		'qfalse' - loop to the head of the bank
==============
*/
void CG_PrevWeap(qboolean switchBanks) {
	int			bank = 0, cycle = 0, newbank = 0, newcycle = 0;
	int			num, curweap;
	qboolean	prevbank = qfalse;	// need to switch to the next bank of weapons?
	int			i, j;

	num = curweap = cg.weaponSelect;

	/* Windows TC controllers30074870/30074c10 use numeric protocol IDs. */
	if( curweap == 60 || curweap == 62 || (cg.snap->ps.eFlags & 0x1000000) )
		return;

	switch(num) {
		case 14:
			curweap = num = 2;
			break;
		case 52:
			curweap = num = 7;
			break;
		case 55:
			curweap = num = 23;
			break;
		case 56:
			curweap = num = 24;
			break;
		case 60:
			curweap = num = 35;
			break;
	}

	CG_WeaponIndex(curweap, &bank, &cycle);		// get bank/cycle of current weapon

	// if you're using an alt mode weapon, try switching back to the parent first
	if(curweap >= 55 && curweap <= 59) {
		num = getAltWeapon(curweap);	// base any further changes on the parent
		if(CG_WeaponSelectable(num)) {	// the parent was selectable, drop back to that
			CG_FinishWeaponChange(curweap, num);
			return;
		}
	}

	// initially, just try to find a lower weapon in the current bank
	if( cg_cycleAllWeaps.integer || !switchBanks ) {
		for( i = cycle; i >= 0; i-- ) {
			num = getPrevWeapInBankBynum(num);

			CG_WeaponIndex(num, NULL, &newcycle);		// get cycle of new weapon.  if it's greater than the original, then it cycled around

			if(switchBanks) {
				if(newcycle > (cycle-1)) {
					prevbank = qtrue;
					break;
				}
			} else {	// don't switch banks if you get to the end
				if(num == curweap) {	// back to start, just leave it where it is
					return;
				}
			}

//				if(num == WP_BINOCULARS) {
//					continue;
//				}

			if( CG_WeaponSelectable(num) ) {
				break;
			} else {
				qboolean found = qfalse;
				switch( num ) {
					case 24:
						if ((found = CG_WeaponSelectable( 56 ))) {
							num = 56;
						}
						break;
					case 23:
						if ((found = CG_WeaponSelectable( 55 ))) {
							num = 55;
						}
						break;
				}

				if( found ) {
					break;
				}
			}
		}
	} else {
		prevbank = qtrue;
	}

	// cycle to previous bank.
	//	if cycleAllWeaps: find highest weapon in bank
	//		else: try to find weap in bank that matches cycle position
	//			else: use base weap in bank

	if( prevbank ) {
		for( i = 0; i < MAX_WEAP_BANKS_MP; i++ ) {
			if(cg_cycleAllWeaps.integer)
				num = getPrevBankWeap(bank-i, cycle, qfalse);	// cycling all weaps always starts the next bank at the bottom
			else
				num = getPrevBankWeap(bank-i, cycle, qtrue);

			if(num == 0)
				continue;

			if( CG_WeaponSelectable( num ) ) { // first entry in bank was selectable, no need to scan the bank
				break;
			} else {
				qboolean found = qfalse;
				switch( num ) {
					case 24:
						if ((found = CG_WeaponSelectable( 56 ))) {
							num = 56;
						}
						break;
					case 23:
						if ((found = CG_WeaponSelectable( 55 ))) {
							num = 55;
						}
						break;
				}

				if( found ) {
					break;
				}
			}

			CG_WeaponIndex(num, &newbank, &newcycle);	// get the bank of the new weap

			for(j = MAX_WEAPS_IN_BANK_MP; j > 0; j--) {
				num = getPrevWeapInBank(newbank, j);

				if( CG_WeaponSelectable( num ) ) { // found selectable weapon
					break;
				} else {
					qboolean found = qfalse;
					switch( num ) {
						case 24:
							if ((found = CG_WeaponSelectable( 56 ))) {
								num = 56;
							}
							break;
						case 23:
							if ((found = CG_WeaponSelectable( 55 ))) {
								num = 55;
							}
							break;
					}

					if( found ) {
						break;
					}
				}

				num = 0;	
			}

			if( num ) { // a selectable weapon was found in the current bank
				break;
			}
		}
	}

	CG_FinishWeaponChange( curweap, num );//----(SA)	
}


/*
==============
CG_LastWeaponUsed_f
==============
*/
void CG_LastWeaponUsed_f( void ) {
	int lastweap;

	//fretn - #447
	//osp-rtcw & et pause bug
	if (cg.snap->ps.pm_type == PM_FREEZE ) {
		return;
	}

	if(cg.time - cg.weaponSelectTime < cg_weaponCycleDelay.integer)
		return;	// force pause so holding it down won't go too fast

	if( cg.weaponSelect == 60 || cg.weaponSelect == 62 || (cg.snap->ps.eFlags & 0x1000000) ) {
		return;
	}

	cg.weaponSelectTime = cg.time;	// flash the current weapon icon

	// don't switchback if reloading (it nullifies the reload)
	if ( cg.snap->ps.weaponstate == 9 )
		return;

	if(!cg.switchbackWeapon) {
		cg.switchbackWeapon = cg.weaponSelect;
		return;
	}

	if(CG_WeaponSelectable(cg.switchbackWeapon)) {
		lastweap = cg.weaponSelect;
		CG_FinishWeaponChange(cg.weaponSelect, cg.switchbackWeapon);
	} else {	// switchback no longer selectable, reset cycle
		cg.switchbackWeapon = 0;
	}

}

/*
==============
CG_NextWeaponInBank_f 
==============
*/
void CG_NextWeaponInBank_f ( void ) {
    /* Windows TC bank commands do not reinterpret wheel input as zoom. */
    if (cg.snap->ps.pm_type == PM_FREEZE) return;
    if (cg.time - cg.weaponSelectTime < cg_weaponCycleDelay.integer) return;
    cg.weaponSelectTime = cg.time;
    CG_NextWeap(qfalse);
}

/*
==============
CG_PrevWeaponInBank_f 
==============
*/
void CG_PrevWeaponInBank_f ( void ) {
    /* Windows TC bank commands do not reinterpret wheel input as zoom. */
    if (cg.snap->ps.pm_type == PM_FREEZE) return;
    if (cg.time - cg.weaponSelectTime < cg_weaponCycleDelay.integer) return;
    cg.weaponSelectTime = cg.time;
    CG_PrevWeap(qfalse);
}


/*
==============
CG_NextWeapon_f
==============
*/
/* Windows next/previous commands and their HUD share these cg-owned fields.
 * cg is cleared at CG_Init, including reconnect/map changes. */
static void CG_TCECycleRead(tce_weaponCycle_t *s) {
    memset(s,0,sizeof(*s));s->hasSnapshot=cg.snap!=NULL;
    if(cg.snap) {
        s->snapshotFlags=cg.snap->ps.eFlags;s->snapshotPmFlags=cg.snap->ps.pm_flags;
        s->snapshotWeaponState=cg.snap->ps.weaponstate;s->snapshotWeapon=cg.snap->ps.weapon;
    }
    s->time=cg.time;s->weaponSelectTime=cg.weaponSelectTime;
    s->predictedWeaponState=cg.predictedPlayerState.weaponstate;
    s->predictedWeapon=cg.predictedPlayerState.weapon;
    s->cycleTime=cg.tceCycleTime;s->cycleOffset=cg.tceCycleOffset;
    s->cycleSelected=cg.tceCycleSelected;s->weaponSelect=cg.weaponSelect;
    s->cycleDelay=cg_weaponCycleDelay.integer;
}
static void CG_TCECycleWrite(const tce_weaponCycle_t *s) {
    cg.weaponSelectTime=s->weaponSelectTime;cg.weaponSelect=s->weaponSelect;
    cg.tceCycleTime=s->cycleTime;cg.tceCycleOffset=s->cycleOffset;
    cg.tceCycleSelected=s->cycleSelected;
}
static void CG_TCECycleCommand(int direction) {
    tce_weaponCycle_t s;CG_TCECycleRead(&s);
    TCE_CG_CycleWeaponCommand(&s,direction);CG_TCECycleWrite(&s);
}
static int CG_TCECycleWidth(int weapon) {
    return CG_Text_Width_Ext(tce_cg_weapons[weapon].deployMenuShortName,.2f,0,&cgs.media.limboFont1);
}
static void CG_TCECycleBorder(float x,float y,float w,float h,const float *color) {
    CG_DrawRect(x,y,w,h,1,color);
}
static void CG_TCECycleText(float x,float y,const float *color,int weapon) {
    vec4_t ink;memcpy(ink,color,sizeof(ink));
    CG_Text_Paint_Ext(x,y,.2f,.2f,ink,tce_cg_weapons[weapon].deployMenuShortName,0,0,3,&cgs.media.limboFont1);
}
static int CG_TCECycleSelectable(int weapon) { return CG_WeaponSelectable(weapon); }
static int tceCycleFinishOld,tceCycleFinishNew;
static void CG_TCECycleFinish(int oldWeapon,int newWeapon) {
    tceCycleFinishOld=oldWeapon;tceCycleFinishNew=newWeapon;
}
void CG_TCEDrawWeaponCycle(float bottom) {
    tce_weaponCycle_t s;usercmd_t cmd;
    float rect[4]={770,0,60,32};
    tce_weaponCycleServices_t api={CG_TCECycleSelectable,CG_TCECycleWidth,
        CG_TCECycleBorder,CG_TCECycleText,CG_TCECycleFinish};
    if(!gearDef.parsed || !cg.snap)return;
    CG_TCECycleRead(&s);memset(&cmd,0,sizeof(cmd));
    trap_GetUserCmd(trap_GetCurrentCmdNumber(),&cmd);s.buttons=cmd.buttons;
    rect[1]=bottom;tceCycleFinishNew=-1;
    TCE_CG_DrawSelectedWeapon(&s,rect,colorWhite,&api);
    TCE_CG_DrawWeaponCycle(&s,weapBanksMultiPlayer,rect,colorWhite,0,&api);
    CG_TCECycleWrite(&s);
    /* Apply after projection writeback: Finish updates cg selection/time too. */
    if(tceCycleFinishNew>=0)CG_FinishWeaponChange(tceCycleFinishOld,tceCycleFinishNew);
}

void CG_NextWeapon_f( void ) {
    if(gearDef.parsed) { CG_TCECycleCommand(1);return; }

	if ( !cg.snap ) {
		return;
	}

	// Overload for MV clients
	if(cg.mvTotalClients > 0) {
		CG_mvToggleView_f();
		return;
	}

	//fretn - #447
	//osp-rtcw & et pause bug
	if (cg.snap->ps.pm_type == PM_FREEZE ) {
		return;
	}

	if ( cg.snap->ps.pm_flags & PMF_FOLLOW ) {
		return;
	}

	// this cvar is an option that lets the player use his weapon switching keys (probably the mousewheel)
	// for zooming (binocs/snooper/sniper/etc.)
	if(cg.zoomval) {
		if(cg_useWeapsForZoom.integer == 1) {
			CG_ZoomIn_f();
			return;
		} else if(cg_useWeapsForZoom.integer == 2) {
			CG_ZoomOut_f();
			return;
		}
	}

	if(cg.time - cg.weaponSelectTime < cg_weaponCycleDelay.integer)
		return;	// force pause so holding it down won't go too fast

	cg.weaponSelectTime = cg.time;	// flash the current weapon icon

	// Don't try to switch when in the middle of reloading.
	// cheatinfo:	The server actually would let you switch if this check were not
	//				present, but would discard the reload.  So the when you switched
	//				back you'd have to start the reload over.  This seems bad, however
	//				the delay for the current reload is already in effect, so you'd lose
	//				the reload time twice.  (the first pause for the current weapon reload,
	//				and the pause when you have to reload again 'cause you canceled this one)

	if ( cg.snap->ps.weaponstate == WEAPON_RELOADING )
		return;

	CG_NextWeap(qtrue);
}


/*
==============
CG_PrevWeapon_f
==============
*/
void CG_PrevWeapon_f( void ) {
    if(gearDef.parsed) { CG_TCECycleCommand(-1);return; }
	if ( !cg.snap ) {
		return;
	}

	// Overload for MV clients
	if(cg.mvTotalClients > 0) {
		CG_mvSwapViews_f();
		return;
	}

	//fretn - #447
	//osp-rtcw & et pause bug
	if (cg.snap->ps.pm_type == PM_FREEZE ) {
		return;
	}

	if ( cg.snap->ps.pm_flags & PMF_FOLLOW ) {
		return;
	}

	// this cvar is an option that lets the player use his weapon switching keys (probably the mousewheel)
	// for zooming (binocs/snooper/sniper/etc.)
	if(cg.zoomval) {
		if(cg_useWeapsForZoom.integer == 1) {
			CG_ZoomOut_f();
			return;
		} else if(cg_useWeapsForZoom.integer == 2) {
			CG_ZoomIn_f();
			return;
		}
	}

	if(cg.time - cg.weaponSelectTime < cg_weaponCycleDelay.integer)
		return;	// force pause so holding it down won't go too fast
	
	cg.weaponSelectTime = cg.time;	// flash the current weapon icon

	// Don't try to switch when in the middle of reloading.
	if ( cg.snap->ps.weaponstate == WEAPON_RELOADING )
		return;

	CG_PrevWeap(qtrue);
}


/*
==============
CG_WeaponBank_f
	weapon keys are not generally bound directly('bind 1 weapon 1'),
	rather the key is bound to a given bank ('bind 1 weaponbank 1')
==============
*/
void CG_WeaponBank_f(void) {
	int	num, i, curweap;
	int curbank = 0, curcycle = 0, bank = 0, cycle = 0;

	if (!cg.snap)
		return;

	//fretn - #447
	//osp-rtcw & et pause bug
	if (cg.snap->ps.pm_type == PM_FREEZE ) {
		return;
	}

	if ( cg.snap->ps.pm_flags & PMF_FOLLOW )
		return;

	if(cg.time - cg.weaponSelectTime < cg_weaponCycleDelay.integer)
		return;	// force pause so holding it down won't go too fast
	
	if( cg.weaponSelect == (gearDef.parsed ? 60 : WP_MORTAR_SET) || cg.weaponSelect == (gearDef.parsed ? 62 : WP_MOBILE_MG42_SET) ) {
		return;
	}

	if(gearDef.parsed && (cg.snap->ps.eFlags & 0x1000000)) return;

	cg.weaponSelectTime = cg.time;	// flash the current weapon icon

	// Don't try to switch when in the middle of reloading.
	if ( cg.snap->ps.weaponstate == WEAPON_RELOADING )
		return;

	bank = atoi( CG_Argv( 1 ) );

	if ( bank <= 0 || bank >= MAX_WEAP_BANKS_MP ) {
		return;
	}

	curweap = cg.weaponSelect;
	CG_WeaponIndex(curweap, &curbank, &curcycle);		// get bank/cycle of current weapon

	if(!cg.lastWeapSelInBank[bank]) {
		num = weapBanksMultiPlayer[bank][0];
		cycle -= 1;	// cycle up to first weap
	} else {
		num = cg.lastWeapSelInBank[bank];
		CG_WeaponIndex(num, &bank, &cycle);
		if( bank != curbank ) {
			cycle -= 1;
		}
	}

	for(i = 0; i < MAX_WEAPS_IN_BANK_MP; i++) {
		num = getNextWeapInBank(bank, cycle+i);

		if(CG_WeaponSelectable(num)) {
			break;
		} else {
			qboolean found = qfalse;
			switch( num ) {
				case WP_CARBINE:
					if ((found = CG_WeaponSelectable( (gearDef.parsed ? 56 : WP_M7) ))) {
						num = (gearDef.parsed ? 56 : WP_M7);
					}
					break;
				case WP_KAR98:
					if ((found = CG_WeaponSelectable( (gearDef.parsed ? 55 : WP_GPG40) ))) {
						num = (gearDef.parsed ? 55 : WP_GPG40);
					}
					break;
			}

			if( found ) {
				break;
			}
		}
	}

	if(i == MAX_WEAPS_IN_BANK_MP)
		return;

	// Arnout: don't allow another weapon switch when we're still swapping the gpg40, to prevent animation breaking
	if( ( cg.snap->ps.weaponstate == WEAPON_RAISING || cg.snap->ps.weaponstate == WEAPON_DROPPING ) &&
		( (curweap == (gearDef.parsed ? 55 : WP_GPG40) || num == (gearDef.parsed ? 55 : WP_GPG40) || curweap == (gearDef.parsed ? 56 : WP_M7) || num == (gearDef.parsed ? 56 : WP_M7)) ||
		  (curweap == WP_SILENCER || num == WP_SILENCER || curweap == (gearDef.parsed ? 52 : WP_SILENCED_COLT) || num == (gearDef.parsed ? 52 : WP_SILENCED_COLT)) ||
		  (curweap == (gearDef.parsed ? 60 : WP_MORTAR_SET) || num == (gearDef.parsed ? 60 : WP_MORTAR_SET) ) ) )
		return;

	CG_FinishWeaponChange(curweap, num);

}

/*
===============
CG_Weapon_f
===============
*/
void CG_Weapon_f( void ) {
	int	num;
//	int bank = 0, cycle = 0, newbank = 0, newcycle = 0;
//	qboolean banked = qfalse;

	if ( !cg.snap ) {
		return;
	}

	//fretn - #447
	//osp-rtcw & et pause bug
	if (cg.snap->ps.pm_type == PM_FREEZE ) {
		return;
	}

	if ( cg.snap->ps.pm_flags & PMF_FOLLOW ) {
		return;
	}

	if( cg.weaponSelect == WP_MORTAR_SET || cg.weaponSelect == WP_MOBILE_MG42_SET ) {
		return;
	}

	num = atoi( CG_Argv( 1 ) );

// JPW NERVE
// weapon bind should execute weaponbank instead -- for splitting out class weapons, per Id request
	if (num < MAX_WEAP_BANKS_MP) {
			CG_WeaponBank_f();
	}
	return;
// jpw

/*	cg.weaponSelectTime = cg.time;	// flash the current weapon icon

	// Don't try to switch when in the middle of reloading.
	if ( cg.snap->ps.weaponstate == WEAPON_RELOADING )
		return;


	if ( num <= WP_NONE || num > WP_NUM_WEAPONS ) {
		return;
	}

	curweap = cg.weaponSelect;

	CG_WeaponIndex(curweap, &bank, &cycle);		// get bank/cycle of current weapon
	banked = CG_WeaponIndex(num, &newbank, &newcycle);		// get bank/cycle of requested weapon

	// the new weapon was not found in the reglar banks
	// assume the player want's to go directly to it if possible
	if(!banked) {
		if(CG_WeaponSelectable(num)) {
			CG_FinishWeaponChange(curweap, num);
			return;
		}
	}

	if(bank != newbank)
		cycle = newcycle - 1;	//	drop down one from the requested weap's cycle so it will
								//	try to initially cycle up to the requested weapon

	for(i = 0; i < MAX_WEAPS_IN_BANK; i++) {
		num = getNextWeapInBank(newbank, cycle+i);

		if(num == curweap)	// no other weapons in bank
			return;

		if(CG_WeaponSelectable(num)) {
			break;
		}
	}

	if(i == MAX_WEAPS_IN_BANK)
		return;

	CG_FinishWeaponChange(curweap, num);*/
}

/*
===================
CG_OutOfAmmoChange

The current weapon has just run out of ammo
===================
*/
void CG_OutOfAmmoChange( qboolean allowforceswitch ) {
	int		i;
	int		bank = 0, cycle = 0;
	int		equiv = WP_NONE;

	//
	// trivial switching (Windows TC 30075480; retain original bank order)
	//

	if( (cg.snap->ps.eFlags & 0x1000000) ) return;

	if( cg.weaponSelect == WP_PLIERS || ( cg.weaponSelect == WP_SATCHEL_DET && cg.predictedPlayerState.ammo[WP_SATCHEL_DET] ) ) {
		return;
	}

	if( allowforceswitch ) {
		if( cg.weaponSelect == WP_SMOKE_BOMB ) {
			if (CG_WeaponSelectable(WP_LUGER)) {
				cg.weaponSelect = WP_LUGER;
				CG_FinishWeaponChange(cg.predictedPlayerState.weapon, WP_LUGER);
				return;
			} else if(CG_WeaponSelectable(WP_COLT)) {
				cg.weaponSelect = WP_COLT;
				CG_FinishWeaponChange(cg.predictedPlayerState.weapon, WP_COLT);
				return;
			}
		} else if( cg.weaponSelect == WP_LANDMINE ) {
			if (CG_WeaponSelectable(WP_PLIERS)) {
				cg.weaponSelect = WP_PLIERS;
				CG_FinishWeaponChange(cg.predictedPlayerState.weapon, WP_PLIERS);
				return;
			}
		} else if( cg.weaponSelect == WP_SATCHEL ) {
			if( CG_WeaponSelectable( WP_SATCHEL_DET ) ) {
				cg.weaponSelect = WP_SATCHEL_DET;
				return;
			}
		} else if( cg.weaponSelect == 60 ) {
			cg.weaponSelect = 35;
			return;
		} else if( cg.weaponSelect == 62 ) {
			cg.weaponSelect = 31;
			return;
		}

		// JPW NERVE -- early out if we just dropped dynamite, go to pliers
		if (cg.weaponSelect == WP_DYNAMITE)
			if (CG_WeaponSelectable(WP_PLIERS)) {
				cg.weaponSelect = WP_PLIERS;
				CG_FinishWeaponChange(cg.predictedPlayerState.weapon, WP_PLIERS);
				return;
			}

		// JPW NERVE -- early out if we just fired Panzerfaust, go to pistola, then grenades
		if (cg.weaponSelect == 65) {
			for( i = 0; i < MAX_WEAPS_IN_BANK_MP; i++ ) {
				if (CG_WeaponSelectable(weapBanksMultiPlayer[2][i])) { // find a pistol
					cg.weaponSelect = weapBanksMultiPlayer[2][i];
					CG_FinishWeaponChange(cg.predictedPlayerState.weapon, cg.weaponSelect);
					return;
				}
			}
			for( i = 0; i < MAX_WEAPS_IN_BANK_MP; i++ ) {
				if (CG_WeaponSelectable(weapBanksMultiPlayer[4][i])) { // find a grenade
					cg.weaponSelect = weapBanksMultiPlayer[4][i];
					CG_FinishWeaponChange(cg.predictedPlayerState.weapon, cg.weaponSelect);
					return;
				}
			}
		}

		// if you're using an alt mode weapon, try switching back to the parent
		// otherwise, switch to the equivalent if you've got it
		if(cg.weaponSelect >= 55 && cg.weaponSelect <= 59) {
			cg.weaponSelect = equiv = getAltWeapon(cg.weaponSelect);	// base any further changes on the parent
			if(CG_WeaponSelectable(equiv)) {	// the parent was selectable, drop back to that
				CG_FinishWeaponChange(cg.predictedPlayerState.weapon, cg.weaponSelect);//----(SA)	
				return;
			}
		}
		

		// now try the opposite team's equivalent weap
		equiv = getEquivWeapon(cg.weaponSelect);

		if( equiv != cg.weaponSelect && CG_WeaponSelectable( equiv ) ) {
			cg.weaponSelect = equiv;
			CG_FinishWeaponChange(cg.predictedPlayerState.weapon, cg.weaponSelect);//----(SA)	
			return;
		}
	}

	//
	// more complicated selection
	//

	// didn't have available alternative or equivalent, try another weap in the bank
	CG_WeaponIndex(cg.weaponSelect, &bank, &cycle);		// get bank/cycle of current weapon

	// JPW NERVE -- more useful weapon changes -- check if rifle or pistol is still working, and use that if available
	for( i = 0; i < MAX_WEAPS_IN_BANK_MP; i++ ) {
		if( CG_WeaponSelectable(weapBanksMultiPlayer[3][i]) ) { // find a rifle
			cg.weaponSelect = weapBanksMultiPlayer[3][i];
			CG_FinishWeaponChange(cg.predictedPlayerState.weapon, cg.weaponSelect);
			return;
		}
	}
	for( i = 0; i < MAX_WEAPS_IN_BANK_MP; i++ ) {
		if( CG_WeaponSelectable(weapBanksMultiPlayer[2][i]) ) { // find a pistol
			cg.weaponSelect = weapBanksMultiPlayer[2][i];
			CG_FinishWeaponChange(cg.predictedPlayerState.weapon, cg.weaponSelect);
			return;
		}
	}

	// otherwise just do something
	for( i = cycle; i < MAX_WEAPS_IN_BANK_MP; i++ ) {
		equiv = getNextWeapInBank(bank, i);
		if(CG_WeaponSelectable( equiv )) {	// found a reasonable replacement
			cg.weaponSelect = equiv;
			CG_FinishWeaponChange(cg.predictedPlayerState.weapon, cg.weaponSelect);//----(SA)	
			return;
		}
	}
	
	
	// still nothing available, just go to the next
	// available weap using the regular selection scheme
	CG_NextWeap(qtrue);

}

/*
===================================================================================================

WEAPON EVENTS

===================================================================================================
*/

/* CG_MG42EFX is verified in tce_mg42_effect.c. */

// Note to self this is dead code
/*void CG_FLAKEFX (centity_t *cent, int whichgun)
{
	entityState_t *ent;
	vec3_t			forward, right, up;
	vec3_t			point;
	refEntity_t		flash;

	ent = &cent->currentState;

	VectorCopy (cent->currentState.pos.trBase, point);
	AngleVectors (cent->currentState.apos.trBase, forward, right, up);

	// gun 1 and 2 were switched
	if (whichgun == 2)
	{
		VectorMA (point, 136, forward, point);
		VectorMA (point, 31, up, point);
		VectorMA (point, 22, right, point);
	}
	else if (whichgun == 1)
	{
		VectorMA (point, 136, forward, point);
		VectorMA (point, 31, up, point);
		VectorMA (point, -22, right, point);
	}
	else if (whichgun == 3)
	{
		VectorMA (point, 136, forward, point);
		VectorMA (point, 10, up, point);
		VectorMA (point, 22, right, point);
	}
	else if (whichgun == 4)
	{
		VectorMA (point, 136, forward, point);
		VectorMA (point, 10, up, point);
		VectorMA (point, -22, right, point);
	}

	trap_R_AddLightToScene( point, 200 + (rand()&31),1.0, 0.6, 0.23, 0 );

	memset (&flash, 0, sizeof (flash));
	flash.renderfx = RF_LIGHTING_ORIGIN;
	flash.hModel = cgs.media.mg42muzzleflash;

	VectorCopy( point, flash.origin );
	AnglesToAxis (cg.refdefViewAngles, flash.axis);
	
	trap_R_AddRefEntityToScene( &flash );

	trap_S_StartSound( NULL, ent->number , CHAN_WEAPON, hflakWeaponSnd );
}*/


//----(SA)	
/*
==============
CG_MortarEFX
	Right now mostly copied directly from Raf's MG42 FX, but with the optional addtion of smoke
==============
*/
void CG_MortarEFX(centity_t *cent) {
	refEntity_t		flash;

	if(cent->currentState.density & 1) {
		// smoke
		CG_ParticleImpactSmokePuff (cgs.media.smokePuffShader, cent->currentState.origin);
	}

	if(cent->currentState.density & 2) {
		float intensity;
#if defined(_MSC_VER) && defined(_M_IX86)
		int sample = rand() & 31;
		static const double lightNumerator = 8.0, lightOffset = 0.75;
		/* TC3007597c: one final float store, including original sample-zero case. */
		__asm {
			fild sample
			fdivr lightNumerator
			fadd lightOffset
			fstp intensity
		}
#else
		intensity = 0.75 + 8.0 / (rand() & 31);
#endif
		trap_R_AddLightToScene( cent->currentState.origin, 256, intensity, 1.0, 1.0, 1.0, 0, 0 );

		// muzzle flash
		memset (&flash, 0, sizeof (flash));
		flash.renderfx = RF_LIGHTING_ORIGIN;
		flash.hModel = cgs.media.mg42muzzleflash;
		VectorCopy( cent->currentState.origin, flash.origin );
		AnglesToAxis (cg.refdefViewAngles, flash.axis);
		trap_R_AddRefEntityToScene( &flash );
	}
}

//----(SA)	end


// RF
/*
==============
CG_WeaponFireRecoil
==============
*/
void CG_WeaponFireRecoil( int weapon ) {
//	const	vec3_t maxKickAngles = {25, 30, 25};
	float	pitchRecoilAdd, pitchAdd;
	float	yawRandom;
	vec3_t	recoil;
    if(gearDef.parsed) { TCE_CG_WeaponFireRecoil(weapon,cg.kickAVel);return; }
	//
	pitchRecoilAdd = 0;
	pitchAdd = 0;
	yawRandom = 0;
	//
	switch( weapon ) {
	case WP_LUGER:
	case WP_SILENCER:
	case WP_AKIMBO_LUGER:
	case WP_AKIMBO_SILENCEDLUGER:
	case WP_COLT:
	case WP_SILENCED_COLT:
	case WP_AKIMBO_COLT:
	case WP_AKIMBO_SILENCEDCOLT:
		//pitchAdd = 2+rand()%3;
		//yawRandom = 2;
		break;
	case WP_GARAND:
	case WP_KAR98:
	case WP_CARBINE:
	case WP_K43:
		//pitchAdd = 4+rand()%3;
		//yawRandom = 4;
		pitchAdd = 2;	//----(SA)	for DM
		yawRandom = 1;	//----(SA)	for DM
		break;
	case WP_GARAND_SCOPE:
	case WP_K43_SCOPE:
		pitchAdd = 0.3;
		break;
	case WP_FG42SCOPE:
	case WP_FG42:
	case WP_MOBILE_MG42:
	case WP_MOBILE_MG42_SET:
	case WP_MP40:
	case WP_THOMPSON:
	case WP_STEN:
		//pitchRecoilAdd = 1;
		pitchAdd = 1+rand()%3;
		yawRandom = 2;


		pitchAdd *= 0.3;
		yawRandom *= 0.3;
		break;
	case WP_PANZERFAUST:
		//pitchAdd = 12+rand()%3;
		//yawRandom = 6;

		// push the player back instead
		break;
	default:
		return;
	}
	// calc the recoil
	recoil[YAW] = crandom()*yawRandom;
	recoil[ROLL] = -recoil[YAW];	// why not
	recoil[PITCH] = -pitchAdd;
	// scale it up a bit (easier to modify this while tweaking)
	VectorScale( recoil, 30, recoil );

	// set the recoil
	VectorCopy( recoil, cg.kickAVel );
	// set the recoil
	cg.recoilPitch -= pitchRecoilAdd;
}


/*
================
CG_FireWeapon

Caused by an EV_FIRE_WEAPON event

================
*/
/* Original CG_EliteEffectSound30076de0 / Linux000cd6ee. Both sides of
 * the room transition are retained, including its repeated old-side sound. */
void CG_EliteEffectSound(centity_t *cent,int volume) {
    int weapon=cent->currentState.weapon,active=cg.tceSoundZoneActive,side,v;
    float blend=1,scale=(float)cg_snd_reverb.integer*(1.0f/255.0f),distance;
    vec3_t direction,position;
    if(!volume||weapon==1||weapon==4||weapon==9||weapon==30||weapon==12||weapon==21||weapon==11||
       weaponDef[weapon].subsonic||weaponDef[weapon].suppressed)return;
    if(!cg.tceSoundZoneSounds[0][0]&&!cg.tceSoundZoneSounds[1][0])return;
    VectorSubtract(cent->currentState.pos.trBase,cg.refdef_current->vieworg,direction);
    distance=VectorNormalize(direction);if(distance<64)distance=64;
    if(cg.time-cg.tceSoundZoneTransitionTime<1000)blend=(cg.time-cg.tceSoundZoneTransitionTime)*.001f;
    for(side=0;side<2;++side) {
        int bank=side?1-active:active;
        if(!cg.tceSoundZoneSounds[bank][0]||(side&&blend>=1))continue;
        v=(int)((double)volume*scale*(side?1.0f-blend:blend));
        VectorMA(cg.refdef_current->vieworg,distance,cg.refdef_current->viewaxis[1],position);
        trap_S_StartSoundExVControl(position,-1,CHAN_WEAPON,cgs.gameSounds[cg.tceSoundZoneEffects[bank][0]],SND_NOCUT,v);
        VectorMA(cg.refdef_current->vieworg,-distance,cg.refdef_current->viewaxis[1],position);
        trap_S_StartSoundExVControl(position,-1,CHAN_WEAPON,cgs.gameSounds[cg.tceSoundZoneEffects[bank][side?0:1]],SND_NOCUT,v);
    }
}
void CG_FireWeapon(centity_t *cent) {
    entityState_t *ent=&cent->currentState;
    tce_weaponInfo_t *weap;
    int count,index,volume,reverb,weapon=ent->weapon;
    int *sound;
    vec3_t direction,position;
    float distance;
    double angle;
    if(cg.tcePortalScopeRendering)CG_Printf("ELITE PORTAL: CG_FireWeapon\n");
    if(ent->eFlags&0x8000) {
        int tank=cg_entities[cg_entities[cg_entities[ent->number].tagParent].tankparent].currentState.density&8;
        trap_S_StartSound(NULL,ent->number,CHAN_WEAPON,tank?cgs.media.hWeaponSnd_2:cgs.media.hWeaponSnd);
        cent->muzzleFlashTime=cg.time;return;
    }
    if(ent->eFlags&(0x20|0x400000)) {
        if(!(ent->eFlags&0x400000))trap_S_StartSound(NULL,ent->number,CHAN_WEAPON,cgs.media.hWeaponSnd);
        if(cg_brassTime.integer>0)CG_MachineGunEjectBrass(cent);
        cent->muzzleFlashTime=cg.time;return;
    }
    if(!weapon)return;
    if(weapon>=64){CG_Error("CG_FireWeapon: ent->weapon >= WP_NUM_WEAPONS");return;}
    weap=&tce_cg_weapons[weapon];
    if(ent->clientNum==cg.snap->ps.clientNum)cg.lastFiredWeapon=weapon;
    cent->muzzleFlashTime=cg.time;
    if(ent->number==cg.snap->ps.clientNum) {
        CG_WeaponFireRecoil(weapon);cg.tceShotHoldUntil=cg.time+100;
        angle=(rand()&32767)*(double)(1.f/32767.f)*(double)6.2831854820251465f;
        cg.tceSwayVertical=(float)(cg.tceSwayVertical+sin(angle)*((float)cg.predictedPlayerState.stats[STAT_TCE_SHOT_INSTABILITY]*.001f)*1.5);
        cg.tceSwayHorizontal=(float)(cg.tceSwayHorizontal+cos(angle)*((float)cg.predictedPlayerState.stats[STAT_TCE_SHOT_INSTABILITY]*.001f)*1.5);
    }
    if(ent->number==cg.snap->ps.clientNum&&cg_predictBullets.integer>0)CG_PredictedFire(cent);
    if(ent->number==cg.snap->ps.clientNum&&weapon==46&&cg.tceAimRequested) {
        cg.tceAimRequested=0;cg.tceAimActive=0;cg.tceAimRetryTime=0;
        cg.zoomTime=cg.time-300;cg.tceAimSyncTime=cg.zoomTime;
    }
    if(weapon==60&&ent->clientNum==cg.snap->ps.clientNum) {
        cg.mortarImpactTime=-1;cg.mortarFireAngles[PITCH]=cg.predictedPlayerState.viewangles[PITCH];
        cg.mortarFireAngles[YAW]=cg.predictedPlayerState.viewangles[YAW];
    }
    if(weapon==66) {if(cent->pe.lightningFiring)return;}
    else if(weapon==4||weapon==9||weapon==15||weapon==22||weapon==26||weapon==27||weapon==29||weapon==30) {
        if(weapon==9&&cgs.clientinfo[ent->number].infoValid&&cgs.clientinfo[ent->number].team==cg.snap->ps.persistant[PERS_TEAM]) {
            int team=cgs.clientinfo[ent->number].team;
            if(team==1||team==2) {
                const char *name=va("sound/chat/%s_15%c.wav",team==1?"allies":"specops",'a'+rand()%2);
                trap_S_StartSoundVControl(NULL,ent->number,CHAN_WEAPON,trap_S_RegisterSound(name,qfalse),(int)((1.f-tceFlash.deafness)*127.f));
            }
        }
        if(ent->apos.trBase[0]>0)return;
    }
    if(ent->clientNum==cg.snap->ps.clientNum) {
        if(weapon==55)cg.weaponSelect=23;else if(weapon==56)cg.weaponSelect=24;
    }
    sound=weap->flashSound;
    if((ent->event&~EV_EVENT_BITS)==EV_FIRE_WEAPON_LASTSHOT) {
        for(count=0;count<4&&weap->lastShotSound[count];++count){}
        if(count)sound=weap->lastShotSound;
    }
    if(ent->eFlags&0x40000)return;
    for(count=0;count<4&&sound[count];++count){}
    if(count>0) {
        index=rand()%count;
        if(sound[index]&&tceFlash.deafness!=1.f) {
            centity_t *source=&cg_entities[ent->number];
            if(!weaponDef[weapon].suppressed&&!weaponDef[weapon].subsonic)volume=(int)((1.f-tceFlash.deafness)*127.f);
            else {
                tce_fragmentSoundContext_t context;
                memset(&context,0,sizeof(context));VectorCopy(cg.refdef_current->vieworg,context.listener);
                context.distanceVariant=tceSmokeNewBBox;context.attenuation=tceFlash.deafness;context.disabled=cg.tcePortalScopeRendering;
                volume=TCE_CG_SoundVolume(source->currentState.pos.trBase,127,1800,0,&context);
            }
            trap_S_StartSoundExVControl(NULL,ent->number,CHAN_WEAPON,sound[index],SND_NOCUT,volume);
            reverb=weap->flashReverbVolume?(int)((double)volume*weap->flashReverbVolume*(double)(1.f/255.f)):volume;
            CG_EliteEffectSound(source,reverb);
            if(weap->flashEchoSound[index]) {
                VectorSubtract(source->currentState.pos.trBase,cg.refdef_current->vieworg,direction);
                distance=VectorNormalize(direction);
                if(distance>512&&distance<4096) {
                    VectorMA(cg.refdef_current->vieworg,64,direction,position);
                    trap_S_StartSoundExVControl(position,ent->number,CHAN_WEAPON,weap->flashEchoSound[index],SND_NOCUT,(int)(volume*.5f));
                }
            }
        }
    }
    if(BG_CheckUTWeapon(weapon)&&weapon!=4&&weapon!=9&&weapon!=15&&weapon!=22&&weapon!=26&&weapon!=27&&weapon!=1&&weapon!=30)
        cent->tceFireEffectPending=1;
    if(weaponDef[weapon].bolt)return;
    if(weaponDef[weapon].singleReload&&!ent->eventParm)return;
    if(!weap->ejectBrass||cg_brassTime.integer<1)return;
    cent->tceEjectPending=1;
}



// Ridah
/*
=================
CG_AddSparks
=================
*/
void CG_AddSparks( vec3_t origin, vec3_t dir, int speed, int duration, int count, float randScale ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t	velocity;
	int	i;

	for (i=0; i<count; i++) {
		le = CG_AllocLocalEntity();
		re = &le->refEntity;

		VectorSet( velocity, dir[0] + crandom()*randScale, dir[1] + crandom()*randScale, dir[2] + crandom()*randScale );
		VectorScale( velocity, (float)speed, velocity );

		le->leType = LE_SPARK;
		le->startTime = cg.time;
		le->endTime = le->startTime + duration - (int)(0.5 * random() * duration);
		le->lastTrailTime = cg.time;

		VectorCopy( origin, re->origin );
		AxisCopy( axisDefault, re->axis );

		le->pos.trType = TR_GRAVITY_LOW;
		VectorCopy( origin, le->pos.trBase );
		VectorMA( le->pos.trBase, 2 + random()*4, dir, le->pos.trBase );
		VectorCopy( velocity, le->pos.trDelta );
		le->pos.trTime = cg.time;

		le->refEntity.customShader = cgs.media.sparkParticleShader;

		le->bounceFactor = 0.9;

//		le->leBounceSoundType = LEBS_BLOOD;
//		le->leMarkType = LEMT_BLOOD;
	}
}
/*
=================
CG_AddBulletParticles
=================
*/
void CG_TCEAddLocalBulletSparks( vec3_t origin, vec3_t dir, int speed, int duration, int count, float randScale ) {
    int i,j;
    for(i=0;i<count;++i) {
        localEntity_t *le=CG_AllocLocalEntity();
        vec3_t velocity;
        for(j=0;j<3;++j) {
            double randomValue=(double)(rand()&32767)*(1.0f/32767.0f)-0.5;
            velocity[j]=(float)((double)speed*(dir[j]+(randomValue+randomValue)*randScale));
        }
        le->leType=LE_SPARK;
        le->startTime=cg.time;
        le->endTime=cg.time+duration-(int)((double)(rand()&32767)*(1.0f/32767.0f)*duration*0.5);
        le->lastTrailTime=cg.time;
        VectorCopy(origin,le->refEntity.origin);
        AxisCopy(axisDefault,le->refEntity.axis);
        le->pos.trType=(trType_t)7; /* TC low gravity, original30077100. */
        for(j=0;j<3;++j)
            le->pos.trBase[j]=(float)(origin[j]+((double)(rand()&32767)*(1.0f/32767.0f)*4+2)*dir[j]);
        VectorCopy(velocity,le->pos.trDelta);
        le->pos.trTime=cg.time;
        le->refEntity.customShader=cgs.media.bulletParticleTrailShader;
        le->bounceFactor=0.9f;
    }
}

/* Windows30077bb0 / Linux CG_AddBulletParticles000c3e20.  This is the
 * event emitter, distinct from the local-entity wall sparks at30077100. */
void CG_AddBulletParticles(vec3_t origin, vec3_t dir, int speed, int duration, int count, float randScale) {
	int i, j;
	(void)duration; /* Original uses a fresh300..599ms lifetime per particle. */
	for(i = 0; i < count; ++i) {
		vec3_t velocity, position;
		double z;
		for(j = 0; j < 2; ++j) {
			double randomValue = (double)(rand() & 32767) * (double)(1.0f / 32767.0f) - 0.5;
			velocity[j] = (float)(dir[j] + (randomValue + randomValue) * randScale);
		}
		z = (double)(rand() & 32767) * (double)(1.0f / 32767.0f) - 0.5;
		z = dir[2] + (z + z) * randScale;
		/* x/y spill before scaling; z stays on x87 until after scaling. */
		velocity[0] *= (float)speed;
		velocity[1] *= (float)speed;
		velocity[2] = (float)((float)speed * z);
		for(j = 0; j < 3; ++j)
			position[j] = (float)(origin[j] + ((double)(rand() & 32767) * (double)(1.0f / 32767.0f) * 4.0 + 2.0) * dir[j]);
		CG_TCEParticleBulletDebris(position, velocity, rand() % 300 + 300);
	}
}

/* Windows30077330 / Linux CG_EliteAddBulletParticles000c3042. */
void CG_EliteAddBulletParticles(vec3_t origin,vec3_t dir,int speed,int duration,int count,float randScale) {
    int i,j;
    for(i=0;i<count;++i) {
        vec3_t velocity,position;
        double length,randomValue;
        for(j=0;j<3;++j) {
            randomValue=(double)(rand()&32767)*(1.0f/32767.0f)-0.5;
            velocity[j]=(float)(randomValue+randomValue);
        }
        length=sqrt((double)velocity[0]*velocity[0]+(double)velocity[1]*velocity[1]+(double)velocity[2]*velocity[2]);
        if(length) {
            /* Original3007e520 computes one reciprocal, then multiplies all axes. */
            double inverseLength=1.0/length;
            for(j=0;j<3;++j)velocity[j]=(float)(inverseLength*(double)velocity[j]);
        }
        for(j=0;j<3;++j)velocity[j]=(float)((double)velocity[j]*randScale+dir[j]);
        for(j=0;j<3;++j) {
            randomValue=(double)(rand()&32767)*(1.0f/32767.0f);
            velocity[j]=(float)((randomValue+2.0f)*(float)speed*velocity[j]);
        }
        VectorCopy(origin,position);
        randomValue=(double)(rand()&32767)*(1.0f/32767.0f);
        CG_ParticleBulletDebris(position,velocity,(int)((randomValue+1.0f)*(float)duration*0.5f));
    }
}

void CG_AddDirtBulletParticles( vec3_t origin, vec3_t dir, int speed, int duration, int count, float randScale, float width, float height, float alpha, qhandle_t shader) {
    vec3_t velocity, pos, color;
    int i;
    float randomValue;
    TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(),origin,color);
    VectorSet(velocity,0,0,(float)speed);
    VectorCopy(origin,pos);
    CG_ParticleDirtBulletDebris_Core(pos,velocity,duration,width,height,alpha,color,shader);
    for(i=0;i<count;++i) {
        randomValue=(float)(rand()&32767)*(1.0f/32767.0f)-0.5f;
        velocity[0]=(randomValue+randomValue)*dir[0]*randScale*(float)speed;
        randomValue=(float)(rand()&32767)*(1.0f/32767.0f)-0.5f;
        velocity[1]=(randomValue+randomValue)*dir[1]*randScale*(float)speed;
        velocity[2]=(float)(rand()&32767)*(1.0f/32767.0f)*(float)speed*dir[2];
        CG_ParticleDirtBulletDebris_Core(pos,velocity,duration+rand()%(duration>>1),
            width,height,alpha,color,shader);
    }
}

/*
=================
CG_AddDebris
=================
*/
void CG_AddDebris( vec3_t origin, vec3_t dir, int speed, int duration, int count ) {
    int i,j,timeAdd;
    for(i=0;i<count;++i) {
        localEntity_t *le=CG_AllocLocalEntity();
        vec3_t unitvel,velocity;
        double randomValue;
        for(j=0;j<2;++j) {
            randomValue=(double)(rand()&32767)*(1.0f/32767.0f)-0.5;
            unitvel[j]=(float)(dir[j]+(randomValue+randomValue)*0.9);
        }
        randomValue=(double)(rand()&32767)*(1.0f/32767.0f);
        unitvel[2]=(float)(fabs(dir[2])>0.5 ? (0.2+randomValue*0.8)*dir[2] : randomValue*0.6);
        /* The TC binary draws a separate speed multiplier for each axis. */
        for(j=0;j<3;++j) {
            randomValue=(double)(rand()&32767)*(1.0f/32767.0f)-0.5;
            velocity[j]=(float)(((randomValue+randomValue)*0.5+1.0)*(float)speed*unitvel[j]);
        }
        le->leType=LE_DEBRIS;
        le->startTime=cg.time;
        randomValue=(double)(rand()&32767)*(1.0f/32767.0f)-0.5;
        le->endTime=le->startTime-(int)((randomValue+randomValue)*(float)duration*-0.8)+duration;
        le->lastTrailTime=cg.time;
        VectorCopy(origin,le->refEntity.origin);
        AxisCopy(axisDefault,le->refEntity.axis);
        le->pos.trType=(trType_t)7;
        VectorCopy(origin,le->pos.trBase);
        VectorCopy(velocity,le->pos.trDelta);
        le->pos.trTime=cg.time;
        timeAdd=(int)((double)(rand()&32767)*(1.0f/32767.0f)*40.0+10.0);
        TCE_BG_EvaluateTrajectory(&le->pos,cg.time+timeAdd,le->pos.trBase,qfalse,-1,1.0f);
        le->bounceFactor=0.5f;
        le->effectWidth=(float)(((double)(rand()&32767)*(1.0f/32767.0f)+1.0)*5.0f);
        le->effectFlags|=1;
    }
}
// done.


/*
==============
CG_WaterRipple
==============
*/
void CG_WaterRipple(qhandle_t shader, vec3_t loc, vec3_t dir, int size, int lifetime) {
		localEntity_t	*le;
		refEntity_t		*re;

		le = CG_AllocLocalEntity();
		le->leType			= LE_SCALE_FADE;
		le->leFlags			= LEF_PUFF_DONT_SCALE;

		le->startTime		= cg.time;
		le->endTime			= cg.time + lifetime;
		le->lifeRate		= 1.0 / ( le->endTime - le->startTime );

		re = &le->refEntity;
		VectorCopy( loc, re->origin);
		/* FILD keeps integer time exact until the final float store. */
		re->shaderTime		= (float)((double)cg.time * 0.001f);
		re->reType			= RT_SPLASH;
		re->radius			= size;
		re->customShader	= shader;
		re->shaderRGBA[0]	= 0xff;
		re->shaderRGBA[1]	= 0xff;
		re->shaderRGBA[2]	= 0xff;
		re->shaderRGBA[3]	= 0xff;
		le->color[3]		= 1.0;
}

/*
=================
CG_MissileHitWall

Caused by an EV_MISSILE_MISS event, or directly by local bullet tracing

ClientNum is a dummy field used to define what sort of effect to spawn
=================
*/
void CG_MissileHitWall(int weapon,int effect,vec3_t origin,vec3_t dir,int surfaceFlags) {
    CG_TCEMissileHitWall(weapon,effect,origin,dir,dir,(unsigned)surfaceFlags,0,0);
}

/*
==============
CG_MissileHitWallSmall
==============
*/
/* Entire original3007b010; unused weapon/client arguments remain ABI-compatible. */
void CG_MissileHitWallSmall(int weapon,int clientNum,vec3_t origin,vec3_t dir) {
    vec3_t position,velocity;
    vec4_t projection={0,0,-1,80},color={1,1,1,1};
    int sound=trap_S_RegisterSound("sound/weapons/rocket/rocket_expl.wav",qfalse);
    int mark=trap_R_RegisterShaderNoMip("gfx/damage/grenade_mrk");
    (void)weapon;(void)clientNum;
    VectorMA(origin,16,dir,position);VectorScale(dir,64,velocity);
    CG_ParticleExplosion("explode1",position,velocity,600,6,50,qtrue);
    CG_AddDebris(origin,dir,280,1400,7+rand()%2);
    if(sound)trap_S_StartSound(origin,-1,CHAN_AUTO,sound);
    trap_R_ProjectDecal(mark,1,origin,projection,color,cg_markTime.integer,cg_markTime.integer>>4);
}

/*
=================
CG_MissileHitPlayer
=================
*/
/* Entire original3007b150 / Linux000c9116: white blood-light input. */
void CG_MissileHitPlayer(centity_t *cent,int weapon,vec3_t origin,vec3_t dir,int entityNum) {
    vec3_t color={1,1,1};
    (void)cent;
    CG_Bleed(origin,entityNum,color);
    if(weapon==1)CG_TCEMissileHitWall(1,0,origin,dir,dir,16,0,0);
    else if(weapon==4||weapon==65)CG_TCEMissileHitWall(weapon,0,origin,dir,dir,0,0,0);
}

/*
============================================================================

VENOM GUN TRACING

============================================================================
*/

//----(SA)	all changes to venom below should be mine
#define DEFAULT_VENOM_COUNT 10
//#define DEFAULT_VENOM_SPREAD 20
//#define DEFAULT_VENOM_SPREAD 400
#define DEFAULT_VENOM_SPREAD 700

/*
============================================================================

BULLETS

============================================================================
*/

/*
===============
CG_SpawnTracer
===============
*/
void CG_SpawnTracer( int sourceEnt, vec3_t pstart, vec3_t pend ) {
	localEntity_t	*le;
	float	dist;
	vec3_t dir, ofs;
	orientation_t or;
	vec3_t start, end;

	VectorCopy( pstart, start );
	VectorCopy( pend, end );

	// DHM - make MG42 tracers line up
	if( cg_entities[sourceEnt].currentState.eFlags & EF_MG42_ACTIVE ) {
		start[2] -= 42;
	}

	VectorSubtract( end, start, dir );
	dist = VectorNormalize( dir );

	if (dist < 2.0*cg_tracerLength.value)
		return;	// segment isnt long enough, dont bother

	if (sourceEnt < cgs.maxclients) {
		// for visual purposes, find the actual tag_weapon for this client
		// and offset the start and end accordingly
		if(!(cg_entities[sourceEnt].currentState.eFlags & EF_MG42_ACTIVE || cg_entities[sourceEnt].currentState.eFlags & EF_AAGUN_ACTIVE)) {	// not MG42
			if (CG_GetWeaponTag( sourceEnt, "tag_flash", &or )) {
					VectorSubtract( or.origin, start, ofs );
					if (VectorLength( ofs ) < 64) {
						VectorAdd( start, ofs, start );
					}
				}
			}
	}

	// subtract the length of the tracer from the end point, so we dont go through the end point
	VectorMA( end, -cg_tracerLength.value, dir, end );
	dist = VectorDistance( start, end );

	le = CG_AllocLocalEntity();
	le->leType = LE_MOVING_TRACER;
	le->startTime = cg.time - (cg.frametime ? (rand()%cg.frametime)/2 : 0);
	le->endTime = le->startTime + 1000.0 * dist / cg_tracerSpeed.value;

	le->pos.trType = TR_LINEAR;
	le->pos.trTime = le->startTime;
	VectorCopy( start, le->pos.trBase );
	VectorScale( dir, cg_tracerSpeed.value, le->pos.trDelta );
}

/*
===============
CG_DrawTracer
===============
*/
void CG_DrawTracer( vec3_t start, vec3_t finish ) {
	vec3_t		forward, right;
	polyVert_t	verts[4];
	vec3_t		line;

	VectorSubtract( finish, start, forward );

	line[0] = DotProduct( forward, cg.refdef_current->viewaxis[1] );
	line[1] = DotProduct( forward, cg.refdef_current->viewaxis[2] );

	VectorScale( cg.refdef_current->viewaxis[1], line[1], right );
	VectorMA( right, -line[0], cg.refdef_current->viewaxis[2], right );
	VectorNormalize( right );

	VectorMA( finish, cg_tracerWidth.value, right, verts[0].xyz );
	verts[0].st[0] = 1;
	verts[0].st[1] = 1;
	verts[0].modulate[0] = 255;
	verts[0].modulate[1] = 255;
	verts[0].modulate[2] = 255;
	verts[0].modulate[3] = 255;

	VectorMA( finish, -cg_tracerWidth.value, right, verts[1].xyz );
	verts[1].st[0] = 1;
	verts[1].st[1] = 0;
	verts[1].modulate[0] = 255;
	verts[1].modulate[1] = 255;
	verts[1].modulate[2] = 255;
	verts[1].modulate[3] = 255;

	VectorMA( start, -cg_tracerWidth.value, right, verts[2].xyz );
	verts[2].st[0] = 0;
	verts[2].st[1] = 0;
	verts[2].modulate[0] = 255;
	verts[2].modulate[1] = 255;
	verts[2].modulate[2] = 255;
	verts[2].modulate[3] = 255;

	VectorMA( start, cg_tracerWidth.value, right, verts[3].xyz );
	verts[3].st[0] = 0;
	verts[3].st[1] = 1;
	verts[3].modulate[0] = 255;
	verts[3].modulate[1] = 255;
	verts[3].modulate[2] = 255;
	verts[3].modulate[3] = 255;

	trap_R_AddPolyToScene( cgs.media.tracerShader, 4, verts );
}

/*
===============
CG_Tracer
===============
*/
void CG_Tracer( vec3_t source, vec3_t dest, int sparks ) {
	float		len, begin, end;
	vec3_t		start, finish;
	vec3_t		midpoint;
	vec3_t		forward;

	// tracer
	VectorSubtract( dest, source, forward );
	len = VectorNormalize( forward );

	// start at least a little ways from the muzzle
	if ( len < 100 && !sparks) {
		return;
	}
	/* TC Windows3007b5e0 multiplies length before its float RNG reciprocal. */
	begin = (float)(50.0 + ((double)len - 60.0) * (rand() & 0x7fff) * (double)(1.0f / 32767.0f));
	end = begin + cg_tracerLength.value;
	if ( end > len ) {
		end = len;
	}
	VectorMA( source, begin, forward, start );
	VectorMA( source, end, forward, finish );

	CG_DrawTracer( start, finish );

	midpoint[0] = ( start[0] + finish[0] ) * 0.5;
	midpoint[1] = ( start[1] + finish[1] ) * 0.5;
	midpoint[2] = ( start[2] + finish[2] ) * 0.5;
}


/*
======================
CG_CalcMuzzlePoint
======================
*/
/* Windows3007b6f0 uses TC weapon62 and remote standing height42.
 * Named entity types retain the SDK wire translation used by this build. */
qboolean CG_CalcMuzzlePoint( int entityNum, vec3_t muzzle ) {
	vec3_t		forward, right, up;
	centity_t	*cent;
//	int			anim;

	if ( entityNum == cg.snap->ps.clientNum ) {
		// Arnout: see if we're attached to a gun
		if( cg.snap->ps.eFlags & EF_MG42_ACTIVE ) {
			centity_t *mg42 = &cg_entities[cg.snap->ps.viewlocked_entNum];
			vec3_t	forward;

			AngleVectors ( cg.snap->ps.viewangles, forward, NULL, NULL );
			VectorMA ( mg42->currentState.pos.trBase, 40, forward, muzzle );	// wsa -36, made 40 to be in sync with the actual muzzleflash drawing
			muzzle[2] += cg.snap->ps.viewheight;
		} else if( cg.snap->ps.eFlags & EF_AAGUN_ACTIVE ) {
			centity_t *aagun = &cg_entities[cg.snap->ps.viewlocked_entNum];
			vec3_t	forward, right, up;

			AngleVectors ( cg.snap->ps.viewangles, forward, right, up );
			VectorCopy( aagun->lerpOrigin, muzzle );					// Gordon: modelindex2 will already have been incremented on the server, so work out what it WAS then
			BG_AdjustAAGunMuzzleForBarrel( muzzle, forward, right, up, (aagun->currentState.modelindex2 + 3) % 4 );
		} else if( cg.snap->ps.eFlags & EF_MOUNTEDTANK ) {
			if(cg.renderingThirdPerson) {
				centity_t* tank = &cg_entities[cg_entities[cg.snap->ps.clientNum].tagParent];

				VectorCopy( tank->mountedMG42Flash.origin, muzzle );
				AngleVectors( cg.snap->ps.viewangles, forward, NULL, NULL );
				VectorMA( muzzle, 14, forward, muzzle );
			} else {
				//bani - fix firstperson tank muzzle origin if drawgun is off
				if( !cg_drawGun.integer ) {
					VectorCopy( cg.snap->ps.origin, muzzle );
					AngleVectors( cg.snap->ps.viewangles, forward, right, up );
					VectorMA( muzzle, 48, forward, muzzle );
					muzzle[2] += cg.snap->ps.viewheight;
					VectorMA( muzzle, 8, right, muzzle );
				} else {
					VectorCopy( cg.tankflashorg, muzzle );
				}
			}
		} else {
			VectorCopy( cg.snap->ps.origin, muzzle );
			muzzle[2] += cg.snap->ps.viewheight;
			AngleVectors( cg.snap->ps.viewangles, forward, NULL, NULL );
			if( cg.snap->ps.weapon == 62 ) {
				VectorMA( muzzle, 36, forward, muzzle );
			} else {
				VectorMA( muzzle, 14, forward, muzzle );
			}
		}
		return qtrue;
	}

	cent = &cg_entities[entityNum];
//----(SA)	removed check.  is this still necessary?  (this way works for ai's firing mg42)  should I check for mg42?
//	if ( !cent->currentValid ) {	
//		return qfalse;
//	}
//----(SA)	end

	if( cent->currentState.eFlags & EF_MG42_ACTIVE ) {
//		centity_t	*mg42;
//		int			num;
		vec3_t		forward;

		// ydnar: this is silly--the entity is a mg42 barrel, so just use itself
		#if 0
		
			// find the mg42 we're attached to
			for ( num = 0 ; num < cg.snap->numEntities ; num++ )
			{
				mg42 = &cg_entities[ cg.snap->entities[ num ].number ];
				if( mg42->currentState.eType == ET_MG42_BARREL )
				{
					if( mg42->currentState.number == cent->currentState.number )
					{
						// found it

						VectorCopy( mg42->currentState.pos.trBase, muzzle );
						AngleVectors( cent->lerpAngles, forward, NULL, NULL );
						//VectorMA( muzzle, -36, forward, muzzle );
						VectorMA( muzzle, 40, forward, muzzle );
						muzzle[2] += 42.0f;
						
						break;
					}
				}
			}
			
		#else
		
			if( cent->currentState.eType == ET_MG42_BARREL )
			{
				VectorCopy( cent->currentState.pos.trBase, muzzle );
				AngleVectors( cent->lerpAngles, forward, NULL, NULL );
				VectorMA( muzzle, 40, forward, muzzle );
				muzzle[ 2 ] += 42.0f;
			}
		
		#endif
		
	} else if( cent->currentState.eFlags & EF_MOUNTEDTANK ) {
		centity_t* tank = &cg_entities[cent->tagParent];

		VectorCopy( tank->mountedMG42Flash.origin, muzzle );
	} else if( cent->currentState.eFlags & EF_AAGUN_ACTIVE ) {
		centity_t	*aagun;
		int			num;

		// find the mg42 we're attached to
		for ( num = 0; num < cg.snap->numEntities; num++ ) {
			aagun = &cg_entities[ cg.snap->entities[ num ].number ];
			if( aagun->currentState.eType == ET_AAGUN && aagun->currentState.otherEntityNum == cent->currentState.number ) {
				// found it
				vec3_t	forward, right, up;

				AngleVectors ( cg.snap->ps.viewangles, forward, right, up );
				VectorCopy( aagun->lerpOrigin, muzzle );					// Gordon: modelindex2 will already have been incremented on the server, so work out what it WAS then
				BG_AdjustAAGunMuzzleForBarrel( muzzle, forward, right, up, (aagun->currentState.modelindex2 + 3) % 4 );
			}	
		}
	} else {
		VectorCopy( cent->currentState.pos.trBase, muzzle );

		AngleVectors( cent->currentState.apos.trBase, forward, right, up );
		if( cent->currentState.eFlags & EF_PRONE ) {
			muzzle[2] += PRONE_VIEWHEIGHT;
			if( cent->currentState.weapon == 62 )
				VectorMA( muzzle, 36, forward, muzzle );
			else
				VectorMA( muzzle, 14, forward, muzzle );
		} else {
			muzzle[2] += 42.0f;
			VectorMA( muzzle, 14, forward, muzzle );
		}
	}

	return qtrue;

}

void SnapVectorTowards( vec3_t v, vec3_t to ) {
	int		i;

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( to[i] <= v[i] ) {
//			v[i] = (int)v[i];
			v[i] = floor(v[i]);
		} else {
//			v[i] = (int)v[i] + 1;
			v[i] = ceil(v[i]);
		}
	}
}

/*
======================
CG_Bullet

Renders bullet effects.
======================
*/
void CG_Bullet( vec3_t end, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum, int otherEntNum2, float waterfraction, int seed  ) {
	trace_t trace,trace2;
	int sourceContentType, destContentType;
	vec3_t		dir;
	vec3_t		start, trend; // JPW
	vec4_t		projection;
	static int	lastBloodSpat;
	centity_t *cent;
	
	cent = &cg_entities[fleshEntityNum];

	// JPW NERVE -- don't ever shoot if we're binoced in
	if( cg_entities[sourceEntityNum].currentState.eFlags & EF_ZOOMING ) {
		return;
	}

	// Arnout: snap tracers for MG42 to viewangle of client when antilag is enabled
	if( cgs.antilag && otherEntNum2 == cg.snap->ps.clientNum && cg_entities[otherEntNum2].currentState.eFlags & EF_MG42_ACTIVE ) {
		vec3_t muzzle, forward, right, up;
		float r, u;
		trace_t tr;

		AngleVectors( cg.predictedPlayerState.viewangles, forward, right, up );
		VectorCopy( cg_entities[cg.snap->ps.viewlocked_entNum].currentState.pos.trBase, muzzle );
		if( cg_entities[cg.snap->ps.viewlocked_entNum].currentState.onFireStart )
			VectorMA (muzzle, 16, up, muzzle);

		r = Q_crandom(&seed)*MG42_SPREAD_MP;
		u = Q_crandom(&seed)*MG42_SPREAD_MP;

		VectorMA (muzzle, 8192, forward, end);
		VectorMA (end, r, right, end);
		VectorMA (end, u, up, end);

		CG_Trace( &tr, muzzle, NULL, NULL, end, otherEntNum2, MASK_SHOT );

		SnapVectorTowards( tr.endpos, muzzle );
		VectorCopy( tr.endpos, end );
	}	

	// if the shooter is currently valid, calc a source point and possibly
	// do trail effects
	if ( sourceEntityNum >= 0 && cg_tracerChance.value > 0 ) {
		if ( CG_CalcMuzzlePoint( sourceEntityNum, start ) ) {
			sourceContentType = CG_PointContents( start, 0 );
			destContentType = CG_PointContents( end, 0 );

			// do a complete bubble trail if necessary
			if ( ( sourceContentType == destContentType ) && ( sourceContentType & CONTENTS_WATER ) ) {
				CG_BubbleTrail( start, end, .5, 8 );
			} else if ( ( sourceContentType & CONTENTS_WATER ) ) { // bubble trail from water into air
				trap_CM_BoxTrace( &trace, end, start, NULL, NULL, 0, CONTENTS_WATER );
				CG_BubbleTrail( start, trace.endpos, .5, 8 );
			} else if ( ( destContentType & CONTENTS_WATER ) ) { // bubble trail from air into water
				// only add bubbles if effect is close to viewer
				if(Distance(cg.snap->ps.origin, end) < 1024) {
					trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, CONTENTS_WATER );
					CG_BubbleTrail( end, trace.endpos, .5, 8 );
				}
			}

			// if not flesh, then do a moving tracer
			if ( flesh ) {
				// draw a tracer
				if ( random() < cg_tracerChance.value ) {
					CG_Tracer( start, end, 0 );
				}
			} else {	// (not flesh)
				if(otherEntNum2 >=0 && otherEntNum2 != ENTITYNUM_NONE) {
					CG_SpawnTracer( otherEntNum2, start, end );
				} else {
					CG_SpawnTracer( sourceEntityNum, start, end );
				}
			}
		}
	}

	// impact splash and mark
	if ( flesh ) {
		vec3_t origin;
		localEntity_t *le; // JPW NERVE
		float rnd, tmpf; // JPW NERVE
		vec3_t smokedir, tmpv, tmpv2; // JPW NERVE
		int i,headshot; // JPW NERVE

		if( fleshEntityNum < MAX_CLIENTS ) {
			{
                vec3_t color;
                TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(),end,color);
                CG_Bleed(end,fleshEntityNum,color);
            }
		}

		// JPW NERVE smoke puffs (sometimes with some blood)
		VectorSubtract(end,start,smokedir); // get a nice "through the body" vector
		VectorNormalize(smokedir);
		// all this to come up with a decent center-body displacement of bullet impact point
		VectorSubtract(cent->currentState.pos.trBase,end,tmpv);
		tmpv[2] = 0;
		tmpf = VectorLength(tmpv);
		VectorScale(smokedir,tmpf,tmpv);
		VectorAdd(end,tmpv,origin);
		// whee, got a bullet impact point projected to center body
		CG_GetOriginForTag(cent,&cent->pe.headRefEnt, "tag_mouth", 0, tmpv, NULL );
		tmpv[2] += 5;
		VectorSubtract(tmpv, origin, tmpv2);
		headshot = (VectorLength(tmpv2) < 10);

		if (headshot && cg_blood.integer) {
			for (i=0;i<5;i++) {
				rnd = random();
				VectorScale(smokedir,25.0+random()*25,tmpv);
				tmpv[0] += crandom()*25.0f;
				tmpv[1] += crandom()*25.0f;
				tmpv[2] += crandom()*25.0f;
				CG_GetWindVector(tmpv2);
				VectorScale(tmpv2,35,tmpv2); // was 75, before that 55
				tmpv2[2] = 0;
				VectorAdd(tmpv,tmpv2,tmpv);
				le = CG_SmokePuff( origin, tmpv, 5+rnd*10, 1, rnd*0.8, rnd*0.8, 0.5, 500+(rand()%800), cg.time, 0, 0, cgs.media.fleshSmokePuffShader);
			}
		} else {
			// puff out the front (more dust no blood)
			for (i=0;i<10;i++) {
				rnd = random();
				VectorScale(smokedir,-35.0+random()*25,tmpv);
				tmpv[0] += crandom()*25.0f;
				tmpv[1] += crandom()*25.0f;
				tmpv[2] += crandom()*25.0f;
				CG_GetWindVector(tmpv2);
				VectorScale(tmpv2,35,tmpv2); // was 75, before that 55
				tmpv2[2] = 0;
				VectorAdd(tmpv,tmpv2,tmpv);
				le = CG_SmokePuff( origin, tmpv, 5+rnd*10,	rnd*0.3f+0.5f, rnd*0.3f+0.5f, rnd*0.3f+0.5f, 0.125f, 500+(rand()%300), cg.time, 0, 0, cgs.media.smokePuffShader);
			}
		}
// jpw

		// play the bullet hit flesh sound
		// HACK, if this is not us getting hit, make it quieter // JPW NERVE pulled hack, we like loud impact sounds for MP
		if (fleshEntityNum == cg.snap->ps.clientNum) {
			//CG_SoundPlayIndexedScript( cgs.media.bulletHitFleshScript, NULL, fleshEntityNum );
			trap_S_StartSound( NULL, fleshEntityNum, CHAN_BODY, cgs.media.sfx_bullet_stonehit[rand() % (sizeof(cgs.media.sfx_bullet_stonehit) / sizeof(cgs.media.sfx_bullet_stonehit[0]))] );
		} else {
			//CG_SoundPlayIndexedScript( cgs.media.bulletHitFleshScript, cg_entities[fleshEntityNum].currentState.origin, ENTITYNUM_WORLD ); // JPW NERVE changed from ,origin, to this
			trap_S_StartSound( cg_entities[fleshEntityNum].currentState.origin, ENTITYNUM_WORLD, CHAN_BODY, cgs.media.sfx_bullet_stonehit[rand() % (sizeof(cgs.media.sfx_bullet_stonehit) / sizeof(cgs.media.sfx_bullet_stonehit[0]))] );
		}

		// if we haven't dropped a blood spat in a while, check if this is a good scenario
		if (cg_blood.integer && (lastBloodSpat > cg.time || lastBloodSpat < cg.time - 500) )
		{
			vec4_t	color;
			
			
			if ( CG_CalcMuzzlePoint( sourceEntityNum, start ) ) {
				VectorSubtract( end, start, dir );
				VectorNormalize( dir );
				VectorMA( end, 128, dir, trend );
				trap_CM_BoxTrace( &trace, end, trend, NULL, NULL, 0, MASK_SHOT & ~CONTENTS_BODY );

				if( trace.fraction < 1 )
				{
					//%	CG_ImpactMark( cgs.media.bloodDotShaders[rand()%5], trace.endpos, trace.plane.normal, random()*360,
					//%		1,1,1,1, qtrue, 15+random()*20, qfalse, cg_bloodTime.integer * 1000 );
					#if 0
						VectorSubtract( vec3_origin, dir, projection );
						projection[ 3 ] = 64;
						VectorMA( trace.endpos, -8.0f, projection, markOrigin );
						CG_ImpactMark( cgs.media.bloodDotShaders[ rand() % 5 ], markOrigin, projection, 15.0f + random() * 20.0f, 360.0f * random(),
							1.0f, 1.0f, 1.0f, 1.0f, cg_bloodTime.integer * 1000 );
					#else
						VectorSet( projection, 0, 0, -1 );
						projection[ 3 ] = 15.0f + random() * 20.0f;
						Vector4Set( color, 1.0f, 1.0f, 1.0f, 1.0f );
						trap_R_ProjectDecal( cgs.media.bloodDotShaders[ rand() % 5 ], 1, (vec3_t*) origin, projection, color,
							cg_bloodTime.integer * 1000, (cg_bloodTime.integer * 1000) >> 4 );
					#endif
					lastBloodSpat = cg.time;
				}
				else if( lastBloodSpat < cg.time - 1000 )
				{
					// drop one on the ground?
					VectorCopy( end, trend );
					trend[ 2 ] -= 64;
					trap_CM_BoxTrace( &trace, end, trend, NULL, NULL, 0, MASK_SHOT & ~CONTENTS_BODY );

					if (trace.fraction < 1) {
						//%	CG_ImpactMark( cgs.media.bloodDotShaders[rand()%5], trace.endpos, trace.plane.normal, random()*360,
						//%		1,1,1,1, qtrue, 15+random()*10, qfalse, cg_bloodTime.integer * 1000 );
						#if 0
							VectorSubtract( vec3_origin, dir, projection );
							projection[ 3 ] = 64;
							VectorMA( trace.endpos, -8.0f, projection, markOrigin );
							CG_ImpactMark( cgs.media.bloodDotShaders[ rand() % 5 ], markOrigin, projection, 15.0f + random() * 10.0f, 360.0f * random(),
								1.0f, 1.0f, 1.0f, 1.0f, cg_bloodTime.integer * 1000 );
						#else
							VectorSet( projection, 0, 0, -1 );
							projection[ 3 ] = 15.0f + random() * 20.0f;
							Vector4Set( color, 1.0f, 1.0f, 1.0f, 1.0f );
							trap_R_ProjectDecal( cgs.media.bloodDotShaders[ rand() % 5 ], 1, (vec3_t*) origin, projection, color,
								cg_bloodTime.integer * 1000, (cg_bloodTime.integer * 1000) >> 4 );
						#endif
						lastBloodSpat = cg.time;
					}
				}
			}
		}

	} else {	// (not flesh)
		// Gordon: all bullet weapons have the same fx, and this stops pvs issues causing grenade explosions
		int fromweap = WP_MP40; // cg_entities[sourceEntityNum].currentState.weapon;

		if( !fromweap ||
			cg_entities[sourceEntityNum].currentState.eFlags & EF_MG42_ACTIVE ||
			cg_entities[sourceEntityNum].currentState.eFlags & EF_MOUNTEDTANK )	// mounted
			fromweap = WP_MP40;

		if( CG_CalcMuzzlePoint( sourceEntityNum, start ) || cg.snap->ps.persistant[PERS_HWEAPON_USE]) {
			if( waterfraction ) {
				vec3_t dist;
				vec3_t end2;
				vec3_t dir = {0, 0, 1};

				VectorSubtract( end, start, dist );
				VectorMA( start, waterfraction, dist, end2 );

				trap_S_StartSound( end, -1, CHAN_AUTO, cgs.media.sfx_bullet_waterhit[rand()%5]);

				CG_MissileHitWall( fromweap, 2, end2, dir, 0 );
				//CG_MissileHitWall( fromweap, 1, end, normal, trace.surfaceFlags);
				CG_MissileHitWall( fromweap, 1, end, trace.plane.normal, 0 );
			} else {

				// Gordon: um.... WTF? this doenst even make sense...
	/*			vec3_t	start2;
			VectorSubtract( end, start, dir );
				VectorNormalize( dir );*/
	/*			VectorMA( end, -4, dir, start2 );	// back off a little so it doesn't start in solid
				VectorMA( end, 64, dir, dir );*/

				// Arnout: but this does!
				VectorSubtract( end, start, dir );
				VectorNormalizeFast( dir );
				VectorMA( end, 4, dir, end );

				//CG_RailTrail2( NULL, start, end );

				CG_Trace( &trace, start, NULL, NULL, end, 0, MASK_SHOT );
				// JPW NERVE -- water check
				CG_Trace( &trace2, start, NULL, NULL, end, 0, MASK_WATER | MASK_SHOT );
				if (trace.fraction != trace2.fraction) {
					//trap_CM_BoxTrace( &trace2, start, end, NULL, NULL, -1, MASK_WATER );

					trap_S_StartSound( end, -1, CHAN_AUTO, cgs.media.sfx_bullet_waterhit[rand()%5]);

					CG_Trace( &trace2, start, NULL, NULL, end, -1, MASK_WATER );
					CG_MissileHitWall(fromweap, 2, trace2.endpos, trace2.plane.normal, trace2.surfaceFlags);
					return;
				}

				//CG_MissileHitWall( fromweap, 1, end, normal, trace.surfaceFlags);	// smoke puff	//	(SA) modified to send missilehitwall surface parameters
				
				
				//%	CG_MissileHitWall( fromweap, 1, trace.endpos, trace.plane.normal, trace.surfaceFlags);	// smoke puff	//	(SA) modified to send missilehitwall surface parameters
				
				// ydnar: better bullet marks
				VectorSubtract( vec3_origin, dir, dir );
				CG_MissileHitWall( fromweap, 1, trace.endpos, dir, trace.surfaceFlags);
				
				//CG_RailTrail2( NULL, start, trace.endpos );
			}
		}
	}
}
