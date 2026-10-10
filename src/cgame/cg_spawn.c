/*
 * name:		cg_spawn.c
 *
 * desc:		Client sided only map entities
*/

#include "cg_local.h"
#include "tce_lightgrid.h"

qboolean CG_SpawnString( const char *key, const char *defaultString, char **out ) {
	int		i;

	if ( !cg.spawning ) {
		*out = (char *)defaultString;
		CG_Error( "CG_SpawnString() called while not spawning" );
	}

	for ( i = 0 ; i < cg.numSpawnVars ; i++ ) {
		if ( !strcmp( key, cg.spawnVars[i][0] ) ) {
			*out = cg.spawnVars[i][1];
			return qtrue;
		}
	}

	*out = (char *)defaultString;
	return qfalse;
}

qboolean CG_SpawnFloat( const char *key, const char *defaultString, float *out ) {
	char		*s;
	const char *number;
	qboolean	present;

	present = CG_SpawnString( key, defaultString, &s );
	/* The original Windows CRT accepts decimal input only. Modern atof
	   also accepts C99 hexadecimal floats, changing e.g. "0x10" to 16. */
	number = s;
	while( *number == ' ' || (*number >= '\t' && *number <= '\r') ) ++number;
	if( *number == '+' || *number == '-' ) ++number;
	if( number[0] == '0' && (number[1] == 'x' || number[1] == 'X') )
		*out = number > s && number[-1] == '-' ? -0.0f : 0.0f;
	else
		*out = atof( s );
	return present;
}

qboolean CG_SpawnInt( const char *key, const char *defaultString, int *out ) {
	char		*s;
	qboolean	present;

	present = CG_SpawnString( key, defaultString, &s );
	*out = atoi( s );
	return present;
}

/* Keep the original decimal scanf grammar when hosted by a C99 CRT. */
static void CG_ScanSpawnVector( const char *text, float *out, int components ) {
	int i, digits;
	const char *number, *end;
	for( i = 0; i < components; ++i ) {
		while( *text == ' ' || (*text >= '\t' && *text <= '\r') ) ++text;
		number = text;
		if( *number == '+' || *number == '-' ) ++number;
		if( number[0] == '0' && (number[1] == 'x' || number[1] == 'X') ) {
			out[i] = *text == '-' ? -0.0f : 0.0f;
			/* Decimal conversion consumes the zero, then the x stops the
			   next conversion; later components remain untouched. */
			return;
		}
		end = number;
		digits = 0;
		while( *end >= '0' && *end <= '9' ) { ++end; ++digits; }
		if( *end == '.' ) {
			++end;
			while( *end >= '0' && *end <= '9' ) { ++end; ++digits; }
		}
		if( !digits ) return;
		/* The old scanf accepts the mantissa even when an exponent is
		   incomplete; current UCRT scanf rejects the entire conversion. */
		if( *end == 'e' || *end == 'E' ) {
			++end;
			if( *end == '+' || *end == '-' ) ++end;
			while( *end >= '0' && *end <= '9' ) ++end;
		}
		out[i] = (float)atof(text);
		text = end;
	}
}

qboolean CG_SpawnVector( const char *key, const char *defaultString, float *out ) {
	char		*s;
	qboolean	present;

	present = CG_SpawnString( key, defaultString, &s );
	CG_ScanSpawnVector( s, out, 3 );
	return present;
}

qboolean CG_SpawnVector2D( const char *key, const char *defaultString, float *out ) {
	char		*s;
	qboolean	present;

	present = CG_SpawnString( key, defaultString, &s );
	CG_ScanSpawnVector( s, out, 2 );
	return present;
}

/*
=============
VectorToString

This is just a convenience function
for printing vectors
=============
*/
#if defined(_MSC_VER) && defined(_M_IX86)
/* Original __ftol contract used by the map-entity diagnostic formatter. */
static __declspec(naked) void CG_SpawnTruncateST0( void ) {
	__asm {
		push ebp
		mov ebp,esp
		sub esp,12
		fwait
		fnstcw word ptr [ebp-2]
		fwait
		mov ax,word ptr [ebp-2]
		or ah,0ch
		mov word ptr [ebp-4],ax
		fldcw word ptr [ebp-4]
		fistp qword ptr [ebp-12]
		fldcw word ptr [ebp-2]
		mov eax,dword ptr [ebp-12]
		mov edx,dword ptr [ebp-8]
		leave
		ret
	}
}

__declspec(naked) char *vtos( const vec3_t v ) {
	static int spawnVectorIndex;
	static char spawnVectorStrings[8][32];
	static const char spawnVectorFormat[] = "(%i %i %i)";
	__asm {
		mov eax,spawnVectorIndex
		push esi
		mov esi,eax
		push edi
		mov edi,dword ptr [esp+12]
		shl esi,5
		add esi,offset spawnVectorStrings
		inc eax
		and eax,7
		mov spawnVectorIndex,eax
		fld dword ptr [edi+8]
		call CG_SpawnTruncateST0
		fld dword ptr [edi+4]
		push eax
		call CG_SpawnTruncateST0
		fld dword ptr [edi]
		push eax
		call CG_SpawnTruncateST0
		push eax
		push offset spawnVectorFormat
		push 32
		push esi
		call Com_sprintf
		add esp,24
		mov eax,esi
		pop edi
		pop esi
		ret
	}
}
#else
char	*vtos( const vec3_t v ) {
	static	int		index;
	static	char	str[8][32];
	char	*s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1)&7;

	Com_sprintf (s, 32, "(%i %i %i)", (int)v[0], (int)v[1], (int)v[2]);

	return s;
}

#endif

void SP_path_corner_2( void ) {
	char* targetname;
	vec3_t origin;

	CG_SpawnString("targetname", "", &targetname);
	CG_SpawnVector("origin", "0 0 0", origin);

	if ( !*targetname ) {
		CG_Error("path_corner_2 with no targetname at %s\n", vtos(origin));
		return;
	}

	if( numPathCorners >= MAX_PATH_CORNERS ) {
		CG_Error ("Maximum path_corners hit\n");
		return;
	}

	BG_AddPathCorner( targetname, origin );
}

void SP_info_train_spline_main( void ) {
	char* targetname;
	char* target;
	char* control;
	vec3_t origin;
	int i;
	char* end;
	splinePath_t* spline;

	if( !CG_SpawnVector("origin", "0 0 0", origin) ) {
		CG_Error ("info_train_spline_main with no origin\n");
	}

	if ( !CG_SpawnString("targetname", "", &targetname) ) {
		CG_Error ("info_train_spline_main with no targetname at %s\n", vtos(origin));
	}

	CG_SpawnString("target", "", &target);

	spline = BG_AddSplinePath( targetname, target, origin );

	if(CG_SpawnString( "end", "", &end )) {
		spline->isEnd = qtrue;
	} else if(CG_SpawnString( "start", "", &end )) {
		spline->isStart = qtrue;
	}

	for( i = 1;; i++ ) {
		if(!CG_SpawnString( i == 1 ? va( "control" ) : va( "control%i", i ), "", &control )) {
			break;
		}

		BG_AddSplineControl( spline, control );
	}
}

void SP_misc_gamemodel( void ) {
	char* model;
	vec_t angle;
	vec3_t angles;

	vec_t scale;
	vec3_t vScale;

	vec3_t org;

	cg_gamemodel_t* gamemodel;

	int i;

    /* TC30063f50: flag4 selects the static camera-facing sprite path before
       the SDK's dynamic-model rejection. randomscale is parsed but the
       original discards Q_random's result; it does not scale these bounds. */
    CG_SpawnString("spawnflags", "", &model);
    if(atoi(model)==4) {
        cg_clientsprite_t *sprite;
        vec3_t mins,maxs;
        vec2_t randomScale;
        int seed;
        if(cg.numMiscClientSprites>=MAX_STATIC_CLIENTSPRITES)
            CG_Error("^1MAX_STATIC_CLIENTSPRITES(%i) hit",MAX_STATIC_CLIENTSPRITES);
        CG_SpawnString("model","",&model);
        CG_SpawnVector("origin","0 0 0",org);
        if(!CG_SpawnVector("modelscale_vec","1 1 1",vScale)) {
            if(CG_SpawnFloat("modelscale","1",&scale))VectorSet(vScale,scale,scale,scale);
        }
        sprite=&cgs.miscClientSprites[cg.numMiscClientSprites++];
        sprite->shader=trap_R_RegisterShader(model);
        VectorCopy(org,sprite->org);
        trap_R_ModelBounds(trap_R_RegisterModel(model),mins,maxs);
        for(i=0;i<3;i++){mins[i]*=vScale[i];maxs[i]*=vScale[i];}
        CG_SpawnInt("drawdistance","8192",&sprite->drawDistance);
        CG_SpawnInt("fadedistance","8192",&sprite->fadeDistance);
        CG_SpawnString("randomscale","1.0 1.0",&model);
        sscanf(model,"%f %f",&randomScale[0],&randomScale[1]);
        seed=cg.numMiscClientSprites+7;
        Q_random(&seed);
        sprite->halfWidth=maxs[1];sprite->top=maxs[2];sprite->bottom=mins[2];
        return;
    }

	if(CG_SpawnString( "targetname", "", &model ) || CG_SpawnString( "scriptname", "", &model ) || CG_SpawnString( "spawnflags", "", &model )) {
		// Gordon: this model may not be static, so let the server handle it
		return;
	}

	if( cg.numMiscGameModels >= MAX_STATIC_GAMEMODELS ) {
		CG_Error( "^1MAX_STATIC_GAMEMODELS(%i) hit", MAX_STATIC_GAMEMODELS );
	}

	CG_SpawnString( "model", "", &model );

	CG_SpawnVector( "origin", "0 0 0", org );

	if(!CG_SpawnVector( "angles", "0 0 0", angles )) {
		if(CG_SpawnFloat( "angle", "0", &angle )) {
			angles[YAW] = angle;
		}
	}

	if(!CG_SpawnVector( "modelscale_vec", "1 1 1", vScale )) {
		if( CG_SpawnFloat( "modelscale", "1", &scale ) ) {
			VectorSet( vScale, scale, scale, scale );
		}
	}

	gamemodel = &cgs.miscGameModels[cg.numMiscGameModels++];
	gamemodel->model = trap_R_RegisterModel( model );
	AnglesToAxis( angles, gamemodel->axes );
	for( i = 0; i < 3; i++ ) {
		VectorScale( gamemodel->axes[i], vScale[i], gamemodel->axes[i] );
	}
	VectorCopy( org, gamemodel->org );

	if( gamemodel->model ) {
		vec3_t mins, maxs;

		trap_R_ModelBounds( gamemodel->model, mins, maxs );

		for( i = 0; i < 3; i++ ) {
			mins[i] *= vScale[i];
			maxs[i] *= vScale[i];
		}

		gamemodel->radius = RadiusFromBounds( mins, maxs );
	} else {
		gamemodel->radius = 0;
	}
}

void SP_trigger_objective_info( void ) {
	char* temp;

	CG_SpawnString( "infoAllied", "^1No Text Supplied", &temp );
	Q_strncpyz( cg.oidTriggerInfoAllies[cg.numOIDtriggers2], temp, 256 );

	CG_SpawnString( "infoAxis", "^1No Text Supplied", &temp );
	Q_strncpyz( cg.oidTriggerInfoAxis[cg.numOIDtriggers2], temp, 256 );

	cg.numOIDtriggers2++;
}

typedef struct {
	char	*name;
	void	(*spawn)(void);
} spawn_t;

spawn_t	spawns[] = {
	{0, 0},
	{"path_corner_2",				SP_path_corner_2},
	{"info_train_spline_main",		SP_info_train_spline_main},
	{"info_train_spline_control",	SP_path_corner_2},

	{"trigger_objective_info",		SP_trigger_objective_info},
	{"misc_gamemodel",				SP_misc_gamemodel},
};

#define NUMSPAWNS	(sizeof(spawns)/sizeof(spawn_t))

/*
===================
CG_ParseEntityFromSpawnVars

Spawn an entity and fill in all of the level fields from
cg.spawnVars[], then call the class specfic spawn function
===================
*/
void CG_ParseEntityFromSpawnVars( void ) {
	int		i;
	char	*classname;

	// check for "notteam" / "notfree" flags
	CG_SpawnInt( "notteam", "0", &i );
	if ( i ) {
		return;
	}

	if( CG_SpawnString( "classname", "", &classname ) ) {
		for( i = 0; i < NUMSPAWNS; i++ ) {
			if( !Q_stricmp( spawns[i].name, classname ) ) {
				spawns[i].spawn();
				break;
			}
		}
	}

}

/*
====================
CG_AddSpawnVarToken
====================
*/
char *CG_AddSpawnVarToken( const char *string ) {
	int		l;
	char	*dest;

	l = Q_strlenInt( string );
	if ( cg.numSpawnVarChars + l + 1 > MAX_SPAWN_VARS_CHARS ) {
		CG_Error( "CG_AddSpawnVarToken: MAX_SPAWN_VARS" );
	}

	dest = cg.spawnVarChars + cg.numSpawnVarChars;
	memcpy( dest, string, l+1 );

	cg.numSpawnVarChars += l + 1;

	return dest;
}

/*
====================
CG_ParseSpawnVars

Parses a brace bounded set of key / value pairs out of the
level's entity strings into cg.spawnVars[]

This does not actually spawn an entity.
====================
*/
qboolean CG_ParseSpawnVars( void ) {
	char		keyname[MAX_TOKEN_CHARS];
	char		com_token[MAX_TOKEN_CHARS];

	cg.numSpawnVars = 0;
	cg.numSpawnVarChars = 0;

	// parse the opening brace
	if ( !trap_GetEntityToken( com_token, sizeof( com_token ) ) ) {
		// end of spawn string
		return qfalse;
	}
	if ( com_token[0] != '{' ) {
		CG_Error( "CG_ParseSpawnVars: found %s when expecting {",com_token );
	}

	// go through all the key / value pairs
	while ( 1 ) {	
		// parse key
		if ( !trap_GetEntityToken( keyname, sizeof( keyname ) ) ) {
			CG_Error( "CG_ParseSpawnVars: EOF without closing brace" );
		}

		if ( keyname[0] == '}' ) {
			break;
		}
		
		// parse value	
		if ( !trap_GetEntityToken( com_token, sizeof( com_token ) ) ) {
			CG_Error( "CG_ParseSpawnVars: EOF without closing brace" );
		}

		if ( com_token[0] == '}' ) {
			CG_Error( "CG_ParseSpawnVars: closing brace without data" );
		}
		if ( cg.numSpawnVars == MAX_SPAWN_VARS ) {
			CG_Error( "CG_ParseSpawnVars: MAX_SPAWN_VARS" );
		}
		cg.spawnVars[ cg.numSpawnVars ][0] = CG_AddSpawnVarToken( keyname );
		cg.spawnVars[ cg.numSpawnVars ][1] = CG_AddSpawnVarToken( com_token );
		cg.numSpawnVars++;
	}

	return qtrue;
}

void SP_worldspawn( void ) {
	char	*s;
	int		i;

	CG_SpawnString( "classname", "", &s );
	if ( Q_stricmp( s, "worldspawn" ) ) {
		CG_Error( "SP_worldspawn: The first entity isn't 'worldspawn'" );
	}

	cgs.ccLayers = 0;

	if( CG_SpawnVector2D( "mapcoordsmins", "-128 128", cg.mapcoordsMins ) &&	// top left
		CG_SpawnVector2D( "mapcoordsmaxs", "128 -128", cg.mapcoordsMaxs ) ) {	// bottom right
		cg.mapcoordsValid = qtrue;
	} else {
		cg.mapcoordsValid = qfalse;
	}

	CG_ParseSpawns();
	CG_SpawnVector("gridsize", "64 64 128", tce_lightGridSpacing);
	CG_SpawnString("eyeadaptation_sky", "0.0", &s);
	cg.tceEyeSky=atof(s);
	CG_SpawnString("eyeadaptation_scale", "1.0", &s);
	cg.tceEyeScale=atof(s);
	/* TC worldspawn 300646a9: exposure controls impact-spark brightness. */
	CG_SpawnString("exposure", "80.0", &s);
	{
		double intensity = sqrt(20.0 / atof(s));
		cg.tceSparkIntensity = (float)intensity;
		if (intensity > 1.0) cg.tceSparkIntensity = 1.0f;
		else if (!(cg.tceSparkIntensity >= .33f)) cg.tceSparkIntensity = .33f;
	}
	/* TC parses the map value but deliberately forces the renderer baseline. */
	CG_SpawnString("ambientscale", "0.0", &s);
	trap_Cvar_Set("r_ambientscale", "1.3");
	cg.tceTraceMapLoaded = BG_LoadTraceMap(cgs.rawmapname, cg.mapcoordsMins, cg.mapcoordsMaxs) != 0;

	CG_SpawnString( "cclayers", "0", &s );
	cgs.ccLayers = atoi(s);

	for( i = 0; i < cgs.ccLayers; i++ ) {
		CG_SpawnString( va("cclayerceil%i",i), "0", &s );
		cgs.ccLayerCeils[i] = atoi(s);
	}

	cg.mapcoordsScale[0] = 1 / (cg.mapcoordsMaxs[0] - cg.mapcoordsMins[0]);
	cg.mapcoordsScale[1] = 1 / (cg.mapcoordsMaxs[1] - cg.mapcoordsMins[1]);

	BG_InitLocations( cg.mapcoordsMins, cg.mapcoordsMaxs );

	CG_SpawnString( "atmosphere", "", &s );
	CG_EffectParse( s );

	cg.fiveMinuteSound_g[0] = \
		cg.fiveMinuteSound_a[0] = \
		cg.twoMinuteSound_g[0] = \
		cg.twoMinuteSound_a[0] = \
		cg.thirtySecondSound_g[0] = \
		cg.thirtySecondSound_a[0] = '\0';

	CG_SpawnString( "twoMinuteSound_axis", "axis_hq_5minutes", &s );
	Q_strncpyz( cg.fiveMinuteSound_g, s, sizeof(cg.fiveMinuteSound_g) );
	CG_SpawnString( "twoMinuteSound_allied", "allies_hq_5minutes", &s );
	Q_strncpyz( cg.fiveMinuteSound_a, s, sizeof(cg.fiveMinuteSound_a) );

	CG_SpawnString( "twoMinuteSound_axis", "axis_hq_2minutes", &s );
	Q_strncpyz( cg.twoMinuteSound_g, s, sizeof(cg.twoMinuteSound_g) );
	CG_SpawnString( "twoMinuteSound_allied", "allies_hq_2minutes", &s );
	Q_strncpyz( cg.twoMinuteSound_a, s, sizeof(cg.twoMinuteSound_a) );

	CG_SpawnString( "thirtySecondSound_axis", "axis_hq_30seconds", &s );
	Q_strncpyz( cg.thirtySecondSound_g, s, sizeof(cg.thirtySecondSound_g) );
	CG_SpawnString( "thirtySecondSound_allied", "allies_hq_30seconds", &s );
	Q_strncpyz( cg.thirtySecondSound_a, s, sizeof(cg.thirtySecondSound_a) );

	// 5 minute axis
	if( !*cg.fiveMinuteSound_g )
		cgs.media.fiveMinuteSound_g = 0;
    else if( strstr( cg.fiveMinuteSound_g, ".wav" ) )
 		cgs.media.fiveMinuteSound_g = trap_S_RegisterSound( cg.fiveMinuteSound_g, qtrue );
	else
		cgs.media.fiveMinuteSound_g = -1;

	// 5 minute allied
 	if( !*cg.fiveMinuteSound_a )
		cgs.media.fiveMinuteSound_a = 0;
    else if( strstr( cg.fiveMinuteSound_a, ".wav" ) )
  		cgs.media.fiveMinuteSound_a = trap_S_RegisterSound( cg.fiveMinuteSound_a, qtrue );
	else
		cgs.media.fiveMinuteSound_a = -1;

	// 2 minute axis
 	if( !*cg.twoMinuteSound_g )
		cgs.media.twoMinuteSound_g = 0;
    else if( strstr( cg.twoMinuteSound_g, ".wav" ) )
 		cgs.media.twoMinuteSound_g = trap_S_RegisterSound( cg.twoMinuteSound_g, qtrue );
	else
		cgs.media.twoMinuteSound_g = -1;

	// 2 minute allied
 	if( !*cg.twoMinuteSound_a )
		cgs.media.twoMinuteSound_a = 0;
    else if( strstr( cg.twoMinuteSound_a, ".wav" ) )
		cgs.media.twoMinuteSound_a = trap_S_RegisterSound( cg.twoMinuteSound_a, qtrue );
	else
		cgs.media.twoMinuteSound_a = -1;

	// 30 seconds axis
 	if( !*cg.thirtySecondSound_g )
		cgs.media.thirtySecondSound_g = 0;
    else if( strstr( cg.thirtySecondSound_g, ".wav" ) )
 		cgs.media.thirtySecondSound_g = trap_S_RegisterSound( cg.thirtySecondSound_g, qtrue );
	else
		cgs.media.thirtySecondSound_g = -1;

	// 30 seconds allied
 	if( !*cg.thirtySecondSound_a )
		cgs.media.thirtySecondSound_a = 0;
    else if( strstr( cg.thirtySecondSound_a, ".wav" ) )
  		cgs.media.thirtySecondSound_a = trap_S_RegisterSound( cg.thirtySecondSound_a, qtrue );
	else
		cgs.media.thirtySecondSound_a = -1;
}

/*
==============
CG_ParseEntitiesFromString

Parses textual entity definitions out of an entstring and spawns gentities.
==============
*/
void CG_ParseEntitiesFromString( void ) {
	// allow calls to CG_Spawn*()
	cg.spawning = qtrue;
	cg.numSpawnVars = 0;
	cg.numMiscGameModels = 0;
	cg.numMiscClientSprites = 0;

	// the worldspawn is not an actual entity, but it still
	// has a "spawn" function to perform any global setup
	// needed by a level (setting configstrings or cvars, etc)
	if ( !CG_ParseSpawnVars() ) {
		CG_Error( "ParseEntities: no entities" );
	}
	SP_worldspawn();

	// parse ents
	while( CG_ParseSpawnVars() ) {
		CG_ParseEntityFromSpawnVars();
	}	

	cg.spawning = qfalse;			// any future calls to CG_Spawn*() will be errors
}

