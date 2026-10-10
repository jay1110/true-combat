/*
**
**	cg_atmospheric.c
**
**	Add atmospheric effects (e.g. rain, snow etc.) to view.
**
**	Current supported effects are rain and snow.
**
*/

#define ATM_NEW

#include "cg_local.h"
#include <stddef.h>

#define	MAX_ATMOSPHERIC_HEIGHT			MAX_MAP_SIZE	// maximum world height
#define	MIN_ATMOSPHERIC_HEIGHT			-MAX_MAP_SIZE	// minimum world height

//int getgroundtime, getskytime, rendertime, checkvisibletime, generatetime;
//int n_getgroundtime, n_getskytime, n_rendertime, n_checkvisibletime, n_generatetime;

//static qboolean CG_LoadTraceMap( void );

#define	MAX_ATMOSPHERIC_PARTICLES		4000	// maximum # of particles
#define	MAX_ATMOSPHERIC_DISTANCE		1000	// maximum distance from refdef origin that particles are visible
#define MAX_ATMOSPHERIC_EFFECTSHADERS	6		// maximum different effectshaders for an atmospheric effect
#define	ATMOSPHERIC_DROPDELAY			1000
#define	ATMOSPHERIC_CUTHEIGHT			800

#define	ATMOSPHERIC_RAIN_SPEED		(1.1f * DEFAULT_GRAVITY)
#define	ATMOSPHERIC_RAIN_HEIGHT		150

#define	ATMOSPHERIC_SNOW_SPEED		(0.1f * DEFAULT_GRAVITY)
#define	ATMOSPHERIC_SNOW_HEIGHT		3

typedef enum {
	ATM_NONE,
	ATM_RAIN,
	ATM_SNOW
} atmFXType_t;

#ifndef ATM_NEW
/*
** Atmospheric Particles PolyPool
*/
static polyVert_t atmPolyPool[MAX_ATMOSPHERIC_PARTICLES*3];
static int numParticlesInFrame;
static qhandle_t atmPolyShader;

static void CG_ClearPolyPool( void ) {
	numParticlesInFrame = 0;
	atmPolyShader = 0;
}

static void CG_RenderPolyPool( void ) {
	if( numParticlesInFrame ) {
		trap_R_AddPolysToScene( atmPolyShader, 3, atmPolyPool, numParticlesInFrame );
		CG_ClearPolyPool();
	}
}
#endif // ATM_NEW

static void CG_AddPolyToPool( qhandle_t shader, const polyVert_t *verts ) {
#ifndef ATM_NEW
		if( atmPolyShader && atmPolyShader != shader ) {
			CG_RenderPolyPool();
		}

		if( numParticlesInFrame == MAX_ATMOSPHERIC_PARTICLES ) {
			CG_RenderPolyPool();
		}

		atmPolyShader = shader;
		memcpy( &atmPolyPool[numParticlesInFrame*3], verts, 3 * sizeof( polyVert_t ) );
		numParticlesInFrame++;
#else
		int firstIndex;
		int firstVertex;
		int i;

		polyBuffer_t* pPolyBuffer = CG_PB_FindFreePolyBuffer( shader, 3, 3 );
		if(!pPolyBuffer) {
			return;
		}

		firstIndex = pPolyBuffer->numIndicies;
		firstVertex = pPolyBuffer->numVerts;

		for( i = 0; i < 3; i++ ) {
			VectorCopy( verts[i].xyz, pPolyBuffer->xyz[firstVertex+i] );

			pPolyBuffer->st[firstVertex + i][0] = verts[i].st[0];
			pPolyBuffer->st[firstVertex + i][1] = verts[i].st[1];
			pPolyBuffer->color[firstVertex + i][0] = verts[i].modulate[0];
			pPolyBuffer->color[firstVertex + i][1] = verts[i].modulate[1];
			pPolyBuffer->color[firstVertex + i][2] = verts[i].modulate[2];
			pPolyBuffer->color[firstVertex + i][3] = verts[i].modulate[3];

			pPolyBuffer->indicies[firstIndex + i] = firstVertex + i;

		}
		
		pPolyBuffer->numIndicies += 3;
		pPolyBuffer->numVerts += 3;
#endif // ATM_NEW
}

/*
**	CG_AtmosphericKludge
*/

static qboolean kludgeChecked, kludgeResult;
qboolean CG_AtmosphericKludge()
{
	// Activate rain for specified kludge maps that don't
	// have it specified for them.

	if( kludgeChecked )
		return( kludgeResult );
	kludgeChecked = qtrue;
	kludgeResult = qfalse;

	/*if( !Q_stricmp( cgs.mapname, "maps/trainyard.bsp" ) )
	{
		//CG_EffectParse( "T=RAIN,B=5 10,C=0.5 2,G=0.5 2,BV=30 100,GV=20 80,W=1 2,D=1000 1000" );
		CG_EffectParse( "T=RAIN,B=5 10,C=0.5,G=0.5 2,BV=50 50,GV=200 200,W=1 2,D=1000" );
		return( kludgeResult = qtrue );
	}*/
/*	if( !Q_stricmp( cgs.mapname, "maps/mp_railgun.bsp" ) )
	{
		//CG_EffectParse( "T=RAIN,B=5 10,C=0.5 2,G=0.5 2,BV=30 100,GV=20 80,W=1 2,D=1000 1000" );
//		CG_EffectParse( "T=SNOW,B=5 10,C=0.5,G=0.3 2,BV=50 50,GV=30 80,W=1 2,D=5000" );
		
		// snow storm, quite horizontally
		//CG_EffectParse( "T=SNOW,B=20 30,C=0.8,G=0.5 8,BV=100 100,GV=70 150,W=3 5,D=5000" );

		// mild snow storm, quite vertically - likely go for this
		//CG_EffectParse( "T=SNOW,B=5 10,C=0.5,G=0.3 2,BV=20 30,GV=25 40,W=3 5,D=5000" );
		CG_EffectParse( "T=SNOW,B=5 10,C=0.5,G=0.3 2,BV=20 30,GV=25 40,W=3 5,D=2000" );

		// cpu-cheap press event effect
		//CG_EffectParse( "T=SNOW,B=5 10,C=0.5,G=0.3 2,BV=20 30,GV=25 40,W=3 5,D=500" );
//		CG_EffectParse( "T=SNOW,B=5 10,C=0.5,G=0.3 2,BV=20 30,GV=25 40,W=3 5,D=750" );
		return( kludgeResult = qtrue );
	}*/

	/*if( !Q_stricmp( cgs.mapname, "maps/mp_goliath.bsp" ) ) {
		//CG_EffectParse( "T=SNOW,B=5 7,C=0.2,G=0.1 5,BV=15 25,GV=25 40,W=3 5,D=400" );
		CG_EffectParse( "T=SNOW,B=5 7,C=0.2,G=0.1 5,BV=15 25,GV=25 40,W=3 5,H=512,D=2000" );
		return( kludgeResult = qtrue );
	}*/
	/*if( !Q_stricmp( cgs.rawmapname, "sp_bruck_test006" ) ) {
		//T=SNOW,B=5 10,C=0.5,G=0.3 2,BV=20 30,GV=25 40,W=3 5,H=608,D=2000
		CG_EffectParse( "T=SNOW,B=5 10,C=0.5,G=0.3 2,BV=20 30,GV=25 40,W=3 5,H=512,D=2000 4000" );
		//CG_EffectParse( "T=SNOW,B=5 7,C=0.2,G=0.1 5,BV=15 25,GV=25 40,W=3 5,H=512,D=2000" );
		return( kludgeResult = qtrue );
	}*/

	return( kludgeResult = qfalse );
}

typedef enum {
	ACT_NOT,
	ACT_FALLING
} active_t;

typedef struct cg_atmosphericParticle_s {
	vec3_t pos, delta, deltaNormalized, colour;
	float height, weight;
	active_t active;
	int nextDropTime;
	qhandle_t *effectshader;
} cg_atmosphericParticle_t;

typedef struct cg_atmosphericEffect_s {
	cg_atmosphericParticle_t particles[MAX_ATMOSPHERIC_PARTICLES];
	qhandle_t effectshaders[MAX_ATMOSPHERIC_EFFECTSHADERS];
	int lastRainTime, numDrops;
	int gustStartTime, gustEndTime;
	int baseStartTime, baseEndTime;
	int gustMinTime, gustMaxTime;
	int changeMinTime, changeMaxTime;
	int baseMinTime, baseMaxTime;
	float baseWeight, gustWeight;
	int baseDrops, gustDrops;
	int baseHeightOffset;
	int numEffectShaders;
	vec3_t baseVec, gustVec;

	vec3_t viewDir;

	qboolean (*ParticleCheckVisible)( cg_atmosphericParticle_t *particle );
	qboolean (*ParticleGenerate)( cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight );
	void (*ParticleRender)( cg_atmosphericParticle_t *particle );

	int dropsActive, oldDropsActive;
	int dropsRendered, dropsCreated, dropsSkipped;
} cg_atmosphericEffect_t;

static cg_atmosphericEffect_t cg_atmFx;

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC30017500/30017b70: native layouts, original double time scale. */
enum {
    WeatherTime = offsetof(cg_t, time),
    WeatherRefdef = offsetof(cg_t, refdef_current),
    WeatherLastTime = offsetof(cg_atmosphericEffect_t, lastRainTime),
    WeatherActive = offsetof(cg_atmosphericParticle_t, active),
    WeatherHeight = offsetof(cg_atmosphericParticle_t, height),
    WeatherPosX = offsetof(cg_atmosphericParticle_t, pos),
    WeatherPosY = offsetof(cg_atmosphericParticle_t, pos) + sizeof(float),
    WeatherPosZ = offsetof(cg_atmosphericParticle_t, pos) + 2*sizeof(float),
    WeatherDeltaX = offsetof(cg_atmosphericParticle_t, delta),
    WeatherDeltaY = offsetof(cg_atmosphericParticle_t, delta) + sizeof(float),
    WeatherDeltaZ = offsetof(cg_atmosphericParticle_t, delta) + 2*sizeof(float),
    WeatherViewX = offsetof(refdef_t, vieworg),
    WeatherViewY = offsetof(refdef_t, vieworg) + sizeof(float)
};
static const double weatherSeconds = 0.001;
static const float weatherDistanceSquared = 1000000.0f;
#endif


static qboolean CG_SetParticleActive( cg_atmosphericParticle_t *particle, active_t active )
{
	particle->active = active;
	return active ? qtrue : qfalse;
}


/*
**	Raindrop management functions
*/

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC weather generators: complete Windows instructions, native object offsets. */
enum {
	GenHeightOffset = offsetof(cg_atmosphericEffect_t, baseHeightOffset),
	GenOldDrops = offsetof(cg_atmosphericEffect_t, oldDropsActive),
	GenNumDrops = offsetof(cg_atmosphericEffect_t, numDrops),
	GenShaders = offsetof(cg_atmosphericEffect_t, effectshaders),
	GenViewX = offsetof(refdef_t, vieworg),
	GenViewY = offsetof(refdef_t, vieworg)+sizeof(float),
	GenViewZ = offsetof(refdef_t, vieworg)+2*sizeof(float),
	GenPosX = offsetof(cg_atmosphericParticle_t, pos)+0*sizeof(float),
	GenPosY = offsetof(cg_atmosphericParticle_t, pos)+1*sizeof(float),
	GenPosZ = offsetof(cg_atmosphericParticle_t, pos)+2*sizeof(float),
	GenDeltaX = offsetof(cg_atmosphericParticle_t, delta)+0*sizeof(float),
	GenDeltaY = offsetof(cg_atmosphericParticle_t, delta)+1*sizeof(float),
	GenDeltaZ = offsetof(cg_atmosphericParticle_t, delta)+2*sizeof(float),
	GenNormalizedX = offsetof(cg_atmosphericParticle_t, deltaNormalized)+0*sizeof(float),
	GenNormalizedY = offsetof(cg_atmosphericParticle_t, deltaNormalized)+1*sizeof(float),
	GenNormalizedZ = offsetof(cg_atmosphericParticle_t, deltaNormalized)+2*sizeof(float),
	GenColourX = offsetof(cg_atmosphericParticle_t, colour)+0*sizeof(float),
	GenColourY = offsetof(cg_atmosphericParticle_t, colour)+1*sizeof(float),
	GenColourZ = offsetof(cg_atmosphericParticle_t, colour)+2*sizeof(float),
	GenHeight = offsetof(cg_atmosphericParticle_t, height),
	GenWeight = offsetof(cg_atmosphericParticle_t, weight),
	GenShader = offsetof(cg_atmosphericParticle_t, effectshader)
};
typedef char WeatherGeneratorLayoutGuard[(sizeof(float)==4 && offsetof(cg_atmosphericParticle_t,pos)==0 && sizeof(vec3_t)==12)?1:-1];
static const float genConstant3009254c = 0.000030518509447574615f;
static const float genConstant30092310 = 6.2831854820251465f;
static const double genConstant30092828 = 1000.0;
static const double genConstant30092820 = 20.0;
static const float genConstant300927f4 = 65536.0f;
static const double genConstant30092318 = 0.001;
static const double genConstant300922e0 = 0.5;
static const double genConstant30092818 = 51.0;
static const double genConstant30092810 = 0.6;
static const double genConstant300922f8 = 100.0;
static const double genConstant30092808 = 150.0;
static const float genConstant30092804 = 0.75f;
static const double genConstant30092840 = 25.0;
static const float genConstant300922b4 = 1.0f;
static const float genConstant300922b8 = 0.5f;
static __declspec(naked) qboolean CG_RainParticleGenerate(cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight)
{
	__asm {
		SUB ESP,0x8
		PUSH ESI
		CALL rand
		AND EAX,0x7fff
		MOV dword ptr [ESP + 0x4],EAX
		FILD dword ptr [ESP + 0x4]
		FMUL dword ptr [genConstant3009254c]
		FMUL dword ptr [genConstant30092310]
		FSTP dword ptr [ESP + 0x4]
		CALL rand
		AND EAX,0x7fff
		MOV ESI,dword ptr [ESP + 0x10]
		MOV dword ptr [ESP + 0x8],EAX
		MOV EAX,[cg + WeatherRefdef]
		FILD dword ptr [ESP + 0x8]
		PUSH ESI
		FMUL dword ptr [genConstant3009254c]
		FSQRT
		FMUL qword ptr [genConstant30092828]
		FADD qword ptr [genConstant30092820]
		FLD dword ptr [ESP + 0x8]
		FSIN
		FLD ST(1)
		FMULP ST(1), ST(0)
		FADD dword ptr [EAX + GenViewX]
		FSTP dword ptr [ESI + GenPosX]
		FLD dword ptr [ESP + 0x8]
		FCOS
		MOV ECX,dword ptr [cg + WeatherRefdef]
		FXCH ST(1)
		FMULP ST(1), ST(0)
		FADD dword ptr [ECX + GenViewY]
		FSTP dword ptr [ESI + GenPosY]
		CALL BG_GetSkyHeightAtPoint
		FST dword ptr [ESP + 0x8]
		FCOMP dword ptr [genConstant300927f4]
		ADD ESP,0x4
		FNSTSW AX
		TEST AH,0x40
		JZ gen_300172eb
		XOR EAX,EAX
		POP ESI
		ADD ESP,0x8
		RET
gen_300172eb:
		PUSH ESI
		CALL BG_GetSkyGroundHeightAtPoint
		FST dword ptr [ESP + 0x14]
		FCOMP dword ptr [ESP + 0x8]
		ADD ESP,0x4
		FNSTSW AX
		TEST AH,0x1
		JNZ gen_3001730a
		XOR EAX,EAX
		POP ESI
		ADD ESP,0x8
		RET
gen_3001730a:
		CALL rand
		AND EAX,0x7fff
		MOV dword ptr [ESP + 0x8],EAX
		FILD dword ptr [ESP + 0x8]
		FMUL dword ptr [genConstant3009254c]
		FLD dword ptr [ESP + 0x4]
		FSUB dword ptr [ESP + 0x10]
		FMULP ST(1), ST(0)
		FADD dword ptr [ESP + 0x10]
		FST dword ptr [ESP + 0x8]
		FSTP dword ptr [ESI + GenPosZ]
		MOV EAX,[cg_atmFx + GenHeightOffset]
		TEST EAX,EAX
		JLE gen_30017376
		FILD dword ptr [cg_atmFx + GenHeightOffset]
		MOV ECX,dword ptr [cg + WeatherRefdef]
		FLD dword ptr [ESP + 0x8]
		FSUB dword ptr [ECX + GenViewZ]
		FCOMP ST(1)
		FNSTSW AX
		TEST AH,0x41
		JNZ gen_30017374
		FADD dword ptr [ECX + GenViewZ]
		FST dword ptr [ESI + GenPosZ]
		FCOMP dword ptr [ESP + 0x10]
		FNSTSW AX
		TEST AH,0x1
		JZ gen_30017376
		XOR EAX,EAX
		POP ESI
		ADD ESP,0x8
		RET
gen_30017374:
		FSTP ST(0)
gen_30017376:
		MOV EAX,[cg + WeatherTime]
		MOV ECX,0x2710
		CDQ
		FILD dword ptr [cg_atmFx + GenOldDrops]
		IDIV ECX
		MOV EAX,ECX
		SUB EAX,EDX
		MOV dword ptr [ESP + 0x10],EAX
		FILD dword ptr [ESP + 0x10]
		FMUL qword ptr [genConstant30092318]
		FADD qword ptr [genConstant300922e0]
		FIMUL dword ptr [cg_atmFx + GenNumDrops]
		FXCH ST(1)
		FXCH ST(1)
		FCOMPP
		FNSTSW AX
		TEST AH,0x1
		JZ gen_300173bb
		XOR EAX,EAX
		POP ESI
		ADD ESP,0x8
		RET
gen_300173bb:
		PUSH 0x1
		PUSH ESI
		CALL CG_SetParticleActive
		CALL rand
		AND EAX,0x7fff
		MOV dword ptr [ESP + 0x18],EAX
		FILD dword ptr [ESP + 0x18]
		FMUL dword ptr [genConstant3009254c]
		FMUL qword ptr [genConstant30092818]
		FADD qword ptr [genConstant30092810]
		FSTP dword ptr [ESI + GenColourX]
		CALL rand
		AND EAX,0x7fff
		MOV dword ptr [ESP + 0x18],EAX
		FILD dword ptr [ESP + 0x18]
		FMUL dword ptr [genConstant3009254c]
		FMUL qword ptr [genConstant30092818]
		FADD qword ptr [genConstant30092810]
		FSTP dword ptr [ESI + GenColourY]
		CALL rand
		AND EAX,0x7fff
		MOV dword ptr [ESP + 0x18],EAX
		MOV EAX,dword ptr [ESP + 0x1c]
		FILD dword ptr [ESP + 0x18]
		FMUL dword ptr [genConstant3009254c]
		FMUL qword ptr [genConstant30092818]
		FADD qword ptr [genConstant30092810]
		FSTP dword ptr [ESI + GenColourZ]
		MOV ECX,dword ptr [EAX]
		MOV dword ptr [ESI + GenDeltaX],ECX
		MOV EDX,dword ptr [EAX + 0x4]
		MOV dword ptr [ESI + GenDeltaY],EDX
		MOV EAX,dword ptr [EAX + 0x8]
		MOV dword ptr [ESI + GenDeltaZ],EAX
		CALL rand
		AND EAX,0x7fff
		MOV ECX,dword ptr [ESI + GenDeltaX]
		MOV dword ptr [ESP + 0x18],EAX
		MOV EDX,dword ptr [ESI + GenDeltaY]
		FILD dword ptr [ESP + 0x18]
		LEA EAX,[ESI + GenNormalizedX]
		MOV dword ptr [ESI + GenNormalizedY],EDX
		PUSH EAX
		FMUL dword ptr [genConstant3009254c]
		MOV dword ptr [EAX],ECX
		FSUB qword ptr [genConstant300922e0]
		FADD ST(0),ST(0)
		FMUL qword ptr [genConstant300922f8]
		FADD dword ptr [ESI + GenDeltaZ]
		FST dword ptr [ESI + GenDeltaZ]
		FSTP dword ptr [ESI + GenNormalizedZ]
		CALL VectorNormalizeFast
		ADD ESP,0xc
		CALL rand
		AND EAX,0x7fff
		PUSH EAX
		LEA EAX,[cg_atmFx + GenShaders]
		MOV dword ptr [ESI + GenShader],EAX
		POP EAX
		MOV dword ptr [ESP + 0x10],EAX
		MOV EAX,0x1
		FILD dword ptr [ESP + 0x10]
		FMUL dword ptr [genConstant3009254c]
		FSUB qword ptr [genConstant300922e0]
		FADD ST(0),ST(0)
		FMUL qword ptr [genConstant300922f8]
		FADD qword ptr [genConstant30092808]
		FSTP dword ptr [ESI + GenHeight]
		FLD dword ptr [ESP + 0x18]
		FMUL dword ptr [genConstant30092804]
		FSTP dword ptr [ESI + GenWeight]
		POP ESI
		ADD ESP,0x8
		RET
	}
}
#else
static qboolean CG_RainParticleGenerate( cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight )
{
	// Attempt to 'spot' a raindrop somewhere below a sky texture.

	float angle, distance;
	float groundHeight, skyHeight;
//	int msec = trap_Milliseconds();

//	n_generatetime++;
	
	angle = random() * 2*M_PI;
	distance = 20 + MAX_ATMOSPHERIC_DISTANCE * sqrt(random());

	particle->pos[0] = cg.refdef_current->vieworg[0] + sin(angle) * distance;
	particle->pos[1] = cg.refdef_current->vieworg[1] + cos(angle) * distance;
	
	// ydnar: choose a spawn point randomly between sky and ground
	skyHeight = BG_GetSkyHeightAtPoint( particle->pos );
	if( skyHeight == MAX_ATMOSPHERIC_HEIGHT )
		return qfalse;
	groundHeight = BG_GetSkyGroundHeightAtPoint( particle->pos );
	if( groundHeight >= skyHeight )
		return qfalse;
	particle->pos[2] = groundHeight + random() * (skyHeight - groundHeight);

	// make sure it doesn't fall from too far cause it then will go over our heads ('lower the ceiling')
	if( cg_atmFx.baseHeightOffset > 0 ) {
		if( particle->pos[2] - cg.refdef_current->vieworg[2] > cg_atmFx.baseHeightOffset ) {
			particle->pos[2] = cg.refdef_current->vieworg[2] + cg_atmFx.baseHeightOffset;

			if( particle->pos[2] < groundHeight ) {
				return qfalse;
			}
		}
	}
	
	// ydnar: rain goes in bursts
	{
		float		maxActiveDrops;
		
		// every 10 seconds allow max raindrops
		maxActiveDrops = 0.50 * cg_atmFx.numDrops + 0.001 * cg_atmFx.numDrops * (10000 - (cg.time % 10000));
		if( cg_atmFx.oldDropsActive > maxActiveDrops )
			return qfalse;
	}
	
	CG_SetParticleActive( particle, ACT_FALLING );
	particle->colour[0] = 0.6 + 0.2 * random() * 0xFF;
	particle->colour[1] = 0.6 + 0.2 * random() * 0xFF;
	particle->colour[2] = 0.6 + 0.2 * random() * 0xFF;
	VectorCopy( currvec, particle->delta );
	particle->delta[2] += crandom() * 100;
	VectorCopy( particle->delta, particle->deltaNormalized );
	VectorNormalizeFast( particle->deltaNormalized );
	particle->height = ATMOSPHERIC_RAIN_HEIGHT + crandom() * 100;
	particle->weight = currweight * 0.75f;
	particle->effectshader = &cg_atmFx.effectshaders[0];
//	particle->effectshader = &cg_atmFx.effectshaders[ (int) (random() * ( cg_atmFx.numEffectShaders - 1 )) ];

//	generatetime += trap_Milliseconds() - msec;
	return( qtrue );
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) qboolean CG_RainParticleCheckVisible(cg_atmosphericParticle_t *particle) {
    __asm {
        PUSH ESI
        MOV ESI,dword ptr [ESP + 0x8]
        TEST ESI,ESI
        JZ weather_300175b7
        MOV EAX,dword ptr [ESI + WeatherActive]
        TEST EAX,EAX
        JZ weather_300175b7
        MOV EAX,dword ptr [cg + WeatherTime]
        MOV EDX,dword ptr [cg_atmFx + WeatherLastTime]
        SUB EAX,EDX
        PUSH ESI
        MOV dword ptr [ESP + 0xc],EAX
        FILD dword ptr [ESP + 0xc]
        FMUL qword ptr [weatherSeconds]
        FLD ST(0)
        FMUL dword ptr [ESI + WeatherDeltaX]
        FADD dword ptr [ESI + WeatherPosX]
        FSTP dword ptr [ESI + WeatherPosX]
        FLD ST(0)
        FMUL dword ptr [ESI + WeatherDeltaY]
        FADD dword ptr [ESI + WeatherPosY]
        FSTP dword ptr [ESI + WeatherPosY]
        FMUL dword ptr [ESI + WeatherDeltaZ]
        FADD dword ptr [ESI + WeatherPosZ]
        FST dword ptr [ESI + WeatherPosZ]
        FADD dword ptr [ESI + WeatherHeight]
        FSTP dword ptr [ESP + 0xc]
        CALL BG_GetSkyGroundHeightAtPoint
        FCOMP dword ptr [ESP + 0xc]
        ADD ESP,0x4
        FNSTSW AX
        TEST AH,0x41
        JNZ weather_30017578
        PUSH 0x0
        PUSH ESI
        CALL CG_SetParticleActive
        ADD ESP,0x8
        POP ESI
        RET
weather_30017578:
        MOV EAX,dword ptr [cg + WeatherRefdef]
        FLD dword ptr [ESI + WeatherPosX]
        FSUB dword ptr [EAX + WeatherViewX]
        FLD dword ptr [ESI + WeatherPosY]
        FSUB dword ptr [EAX + WeatherViewY]
        FLD ST(0)
        FMUL ST(0), ST(1)
        FLD ST(2)
        FMUL ST(0), ST(3)
        FADDP ST(1), ST(0)
        FCOMP dword ptr [weatherDistanceSquared]
        FNSTSW AX
        FSTP ST(0)
        TEST AH,0x41
        FSTP ST(0)
        JNZ weather_300175b0
        PUSH 0x0
        PUSH ESI
        CALL CG_SetParticleActive
        ADD ESP,0x8
        POP ESI
        RET
weather_300175b0:
        MOV EAX,0x1
        POP ESI
        RET
weather_300175b7:
        XOR EAX,EAX
        POP ESI
        RET
    }
}
#else

static qboolean CG_RainParticleCheckVisible( cg_atmosphericParticle_t *particle )
{
	// Check the raindrop is visible and still going, wrapping if necessary.

	float moved;
	vec2_t distance;
//	int msec = trap_Milliseconds();

	if( !particle || particle->active == ACT_NOT ) {
//		checkvisibletime += trap_Milliseconds() - msec;
		return( qfalse );
	}

	moved = (cg.time - cg_atmFx.lastRainTime) * 0.001;	// Units moved since last frame
	VectorMA( particle->pos, moved, particle->delta, particle->pos );
 	if( particle->pos[2] + particle->height < BG_GetSkyGroundHeightAtPoint( particle->pos ) ) {
//		checkvisibletime += trap_Milliseconds() - msec;
		return CG_SetParticleActive( particle, ACT_NOT );
	}

	distance[0] = particle->pos[0] - cg.refdef_current->vieworg[0];
	distance[1] = particle->pos[1] - cg.refdef_current->vieworg[1];
 	if( (distance[0] * distance[0] + distance[1] * distance[1]) > Square( MAX_ATMOSPHERIC_DISTANCE ) )
 	{
		// ydnar: just nuke this particle, let it respawn
		return CG_SetParticleActive( particle, ACT_NOT );
		
		/*
		// Attempt to respot the particle at our other side
		particle->pos[0] -= 1.85f * distance[0];
		particle->pos[1] -= 1.85f * distance[1];

		// Valid spot?
		pointHeight = BG_GetSkyHeightAtPoint( particle->pos );
		if( pointHeight == MAX_ATMOSPHERIC_HEIGHT ) {
//			checkvisibletime += trap_Milliseconds() - msec;
			return CG_SetParticleActive( particle, ACT_NOT );
		}

		pointHeight = BG_GetSkyGroundHeightAtPoint( particle->pos );
		if( pointHeight == MAX_ATMOSPHERIC_HEIGHT || pointHeight >= particle->pos[2] ) {
//			checkvisibletime += trap_Milliseconds() - msec;
			return CG_SetParticleActive( particle, ACT_NOT );
		}
		*/
	}

//	checkvisibletime += trap_Milliseconds() - msec;
	return( qtrue );
}


#endif

static void CG_RainParticleRender( cg_atmosphericParticle_t *particle )
{
	// Draw a raindrop

	vec3_t		forward, right;
	polyVert_t	verts[3];
	vec2_t		line;
	float		len, frac, dist;
	vec3_t		start, finish;
	float		groundHeight;
//	int			msec = trap_Milliseconds();

//	n_rendertime++;

	if( particle->active == ACT_NOT ) {
//		rendertime += trap_Milliseconds() - msec;
		return;
	}

	if( CG_CullPoint( particle->pos ) ) {
		return;
	}

	VectorCopy( particle->pos, start );

	dist = DistanceSquared( particle->pos, cg.refdef_current->vieworg );

	// Make sure it doesn't clip through surfaces
	groundHeight = BG_GetSkyGroundHeightAtPoint( start );
	len = particle->height;
	if( start[2] <= groundHeight ) {
		// Stop snow going through surfaces.
		len = particle->height - groundHeight + start[2];
		frac = start[2];
		VectorMA( start, len - particle->height, particle->deltaNormalized, start );
	}

	if( len <= 0 ) {
//		rendertime += trap_Milliseconds() - msec;
		return;
	}

	// fade nearby rain particles
	if( dist < Square( 128.f ) )
		dist = .25f + .75f * ( dist / Square( 128.f ) );
	else
		dist = 1.0f;

	VectorCopy( particle->deltaNormalized, forward );
	VectorMA( start, -len, forward, finish );

	line[0] = DotProduct( forward, cg.refdef_current->viewaxis[1] );
	line[1] = DotProduct( forward, cg.refdef_current->viewaxis[2] );

	VectorScale( cg.refdef_current->viewaxis[1], line[1], right );
	VectorMA( right, -line[0], cg.refdef_current->viewaxis[2], right );
	VectorNormalize( right );
	
	// dist = 1.0;
	
	VectorCopy( finish, verts[0].xyz );	
	verts[0].st[0] = 0.5f;
	verts[0].st[1] = 0;
	verts[0].modulate[0] = particle->colour[0];
	verts[0].modulate[1] = particle->colour[1];
	verts[0].modulate[2] = particle->colour[2];
	verts[0].modulate[3] = 100 * dist;

	VectorMA( start, -particle->weight, right, verts[1].xyz );
	verts[1].st[0] = 0;
	verts[1].st[1] = 1;
	verts[1].modulate[0] = particle->colour[0];
	verts[1].modulate[1] = particle->colour[1];
	verts[2].modulate[2] = particle->colour[2];
	verts[1].modulate[3] = 200 * dist;

	VectorMA( start, particle->weight, right, verts[2].xyz );
	verts[2].st[0] = 1;
	verts[2].st[1] = 1;
	verts[2].modulate[0] = particle->colour[0];
	verts[2].modulate[1] = particle->colour[1];
	verts[2].modulate[2] = particle->colour[2];
	verts[2].modulate[3] = 200 * dist;

	CG_AddPolyToPool( *particle->effectshader, verts );

//	rendertime += trap_Milliseconds() - msec;
}

/*
**	Snow management functions
*/

#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) qboolean CG_SnowParticleGenerate(cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight)
{
	__asm {
		SUB ESP,0x8
		PUSH ESI
		CALL rand
		AND EAX,0x7fff
		MOV dword ptr [ESP + 0x4],EAX
		FILD dword ptr [ESP + 0x4]
		FMUL dword ptr [genConstant3009254c]
		FMUL dword ptr [genConstant30092310]
		FSTP dword ptr [ESP + 0x4]
		CALL rand
		AND EAX,0x7fff
		MOV ESI,dword ptr [ESP + 0x10]
		MOV dword ptr [ESP + 0x8],EAX
		MOV EAX,[cg + WeatherRefdef]
		FILD dword ptr [ESP + 0x8]
		PUSH ESI
		FMUL dword ptr [genConstant3009254c]
		FSQRT
		FMUL qword ptr [genConstant30092828]
		FADD qword ptr [genConstant30092820]
		FLD dword ptr [ESP + 0x8]
		FSIN
		FLD ST(1)
		FMULP ST(1), ST(0)
		FADD dword ptr [EAX + GenViewX]
		FSTP dword ptr [ESI + GenPosX]
		FLD dword ptr [ESP + 0x8]
		FCOS
		MOV ECX,dword ptr [cg + WeatherRefdef]
		FXCH ST(1)
		FMULP ST(1), ST(0)
		FADD dword ptr [ECX + GenViewY]
		FSTP dword ptr [ESI + GenPosY]
		CALL BG_GetSkyHeightAtPoint
		FST dword ptr [ESP + 0x8]
		FCOMP dword ptr [genConstant300927f4]
		ADD ESP,0x4
		FNSTSW AX
		TEST AH,0x40
		JZ gen_30017a3b
		XOR EAX,EAX
		POP ESI
		ADD ESP,0x8
		RET
gen_30017a3b:
		PUSH ESI
		CALL BG_GetSkyGroundHeightAtPoint
		FST dword ptr [ESP + 0x14]
		FCOMP dword ptr [ESP + 0x8]
		ADD ESP,0x4
		FNSTSW AX
		TEST AH,0x1
		JNZ gen_30017a5a
		XOR EAX,EAX
		POP ESI
		ADD ESP,0x8
		RET
gen_30017a5a:
		CALL rand
		AND EAX,0x7fff
		MOV dword ptr [ESP + 0x8],EAX
		FILD dword ptr [ESP + 0x8]
		FMUL dword ptr [genConstant3009254c]
		FLD dword ptr [ESP + 0x4]
		FSUB dword ptr [ESP + 0x10]
		FMULP ST(1), ST(0)
		FADD dword ptr [ESP + 0x10]
		FST dword ptr [ESP + 0x8]
		FSTP dword ptr [ESI + GenPosZ]
		MOV EAX,[cg_atmFx + GenHeightOffset]
		TEST EAX,EAX
		JLE gen_30017ac6
		FILD dword ptr [cg_atmFx + GenHeightOffset]
		MOV ECX,dword ptr [cg + WeatherRefdef]
		FLD dword ptr [ESP + 0x8]
		FSUB dword ptr [ECX + GenViewZ]
		FCOMP ST(1)
		FNSTSW AX
		TEST AH,0x41
		JNZ gen_30017ac4
		FADD dword ptr [ECX + GenViewZ]
		FST dword ptr [ESI + GenPosZ]
		FCOMP dword ptr [ESP + 0x10]
		FNSTSW AX
		TEST AH,0x1
		JZ gen_30017ac6
		XOR EAX,EAX
		POP ESI
		ADD ESP,0x8
		RET
gen_30017ac4:
		FSTP ST(0)
gen_30017ac6:
		PUSH 0x1
		PUSH ESI
		CALL CG_SetParticleActive
		MOV EAX,dword ptr [ESP + 0x1c]
		MOV EDX,dword ptr [EAX]
		MOV dword ptr [ESI + GenDeltaX],EDX
		MOV ECX,dword ptr [EAX + 0x4]
		MOV dword ptr [ESI + GenDeltaY],ECX
		MOV EDX,dword ptr [EAX + 0x8]
		MOV dword ptr [ESI + GenDeltaZ],EDX
		CALL rand
		AND EAX,0x7fff
		MOV ECX,dword ptr [ESI + GenDeltaX]
		MOV dword ptr [ESP + 0x18],EAX
		MOV EDX,dword ptr [ESI + GenDeltaY]
		FILD dword ptr [ESP + 0x18]
		LEA EAX,[ESI + GenNormalizedX]
		MOV dword ptr [ESI + GenNormalizedY],EDX
		PUSH EAX
		FMUL dword ptr [genConstant3009254c]
		MOV dword ptr [EAX],ECX
		FSUB qword ptr [genConstant300922e0]
		FADD ST(0),ST(0)
		FMUL qword ptr [genConstant30092840]
		FADD dword ptr [ESI + GenDeltaZ]
		FST dword ptr [ESI + GenDeltaZ]
		FSTP dword ptr [ESI + GenNormalizedZ]
		CALL VectorNormalizeFast
		ADD ESP,0xc
		CALL rand
		AND EAX,0x7fff
		PUSH EAX
		LEA EAX,[cg_atmFx + GenShaders]
		MOV dword ptr [ESI + GenShader],EAX
		POP EAX
		MOV dword ptr [ESP + 0x10],EAX
		MOV EAX,0x1
		FILD dword ptr [ESP + 0x10]
		FMUL dword ptr [genConstant3009254c]
		FADD ST(0),ST(0)
		FADD dword ptr [genConstant300922b4]
		FST dword ptr [ESI + GenHeight]
		FMUL dword ptr [genConstant300922b8]
		FSTP dword ptr [ESI + GenWeight]
		POP ESI
		ADD ESP,0x8
		RET
	}
}
#else
static qboolean CG_SnowParticleGenerate( cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight )
{
	// Attempt to 'spot' a snowflake somewhere below a sky texture.

	float angle, distance;
	float groundHeight, skyHeight;
//	int msec = trap_Milliseconds();

//	n_generatetime++;

	angle = random() * 2*M_PI;
	distance = 20 + MAX_ATMOSPHERIC_DISTANCE * sqrt(random());

	particle->pos[0] = cg.refdef_current->vieworg[0] + sin(angle) * distance;
	particle->pos[1] = cg.refdef_current->vieworg[1] + cos(angle) * distance;
	
	// ydnar: choose a spawn point randomly between sky and ground
	skyHeight = BG_GetSkyHeightAtPoint( particle->pos );
	if( skyHeight == MAX_ATMOSPHERIC_HEIGHT )
		return qfalse;
	groundHeight = BG_GetSkyGroundHeightAtPoint( particle->pos );
	if( groundHeight >= skyHeight )
		return qfalse;
	particle->pos[2] = groundHeight + random() * (skyHeight - groundHeight);
	
	// make sure it doesn't fall from too far cause it then will go over our heads ('lower the ceiling')
	if( cg_atmFx.baseHeightOffset > 0 )
	{
		if( particle->pos[2] - cg.refdef_current->vieworg[2] > cg_atmFx.baseHeightOffset )
		{
			particle->pos[2] = cg.refdef_current->vieworg[2] + cg_atmFx.baseHeightOffset;
			if( particle->pos[2] < groundHeight )
				return qfalse;
		}
	}

	CG_SetParticleActive( particle, ACT_FALLING );
	VectorCopy( currvec, particle->delta );
	particle->delta[2] += crandom() * 25;
	VectorCopy( particle->delta, particle->deltaNormalized );
	VectorNormalizeFast( particle->deltaNormalized );
	particle->height = 1.0f + random() * 2;
	particle->weight = particle->height * 0.5f;
	particle->effectshader = &cg_atmFx.effectshaders[0];
//	particle->effectshader = &cg_atmFx.effectshaders[ (int) (random() * ( cg_atmFx.numEffectShaders - 1 )) ];

//	generatetime += trap_Milliseconds() - msec;
	return( qtrue );
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) qboolean CG_SnowParticleCheckVisible(cg_atmosphericParticle_t *particle) {
    __asm {
        PUSH ESI
        MOV ESI,dword ptr [ESP + 0x8]
        TEST ESI,ESI
        JZ weather_30017c1f
        MOV EAX,dword ptr [ESI + WeatherActive]
        TEST EAX,EAX
        JZ weather_30017c1f
        MOV EAX,dword ptr [cg + WeatherTime]
        MOV EDX,dword ptr [cg_atmFx + WeatherLastTime]
        SUB EAX,EDX
        PUSH ESI
        MOV dword ptr [ESP + 0xc],EAX
        FILD dword ptr [ESP + 0xc]
        FMUL qword ptr [weatherSeconds]
        FLD ST(0)
        FMUL dword ptr [ESI + WeatherDeltaX]
        FADD dword ptr [ESI + WeatherPosX]
        FSTP dword ptr [ESI + WeatherPosX]
        FLD ST(0)
        FMUL dword ptr [ESI + WeatherDeltaY]
        FADD dword ptr [ESI + WeatherPosY]
        FSTP dword ptr [ESI + WeatherPosY]
        FMUL dword ptr [ESI + WeatherDeltaZ]
        FADD dword ptr [ESI + WeatherPosZ]
        FSTP dword ptr [ESI + WeatherPosZ]
        CALL BG_GetSkyGroundHeightAtPoint
        FCOMP dword ptr [ESI + WeatherPosZ]
        ADD ESP,0x4
        FNSTSW AX
        TEST AH,0x41
        JNZ weather_30017be0
        PUSH 0x0
        PUSH ESI
        CALL CG_SetParticleActive
        ADD ESP,0x8
        POP ESI
        RET
weather_30017be0:
        MOV EAX,dword ptr [cg + WeatherRefdef]
        FLD dword ptr [ESI + WeatherPosX]
        FSUB dword ptr [EAX + WeatherViewX]
        FLD dword ptr [ESI + WeatherPosY]
        FSUB dword ptr [EAX + WeatherViewY]
        FLD ST(0)
        FMUL ST(0), ST(1)
        FLD ST(2)
        FMUL ST(0), ST(3)
        FADDP ST(1), ST(0)
        FCOMP dword ptr [weatherDistanceSquared]
        FNSTSW AX
        FSTP ST(0)
        TEST AH,0x41
        FSTP ST(0)
        JNZ weather_30017c18
        PUSH 0x0
        PUSH ESI
        CALL CG_SetParticleActive
        ADD ESP,0x8
        POP ESI
        RET
weather_30017c18:
        MOV EAX,0x1
        POP ESI
        RET
weather_30017c1f:
        XOR EAX,EAX
        POP ESI
        RET
    }
}
#else

static qboolean CG_SnowParticleCheckVisible( cg_atmosphericParticle_t *particle )
{
	// Check the snowflake is visible and still going, wrapping if necessary.

	float moved;
	vec2_t distance;
//	int msec = trap_Milliseconds();

//	n_checkvisibletime++;

	if( !particle || particle->active == ACT_NOT ) {
//		checkvisibletime += trap_Milliseconds() - msec;
		return( qfalse );
	}

	moved = (cg.time - cg_atmFx.lastRainTime) * 0.001;	// Units moved since last frame
	VectorMA( particle->pos, moved, particle->delta, particle->pos );
	if( particle->pos[2] < BG_GetSkyGroundHeightAtPoint( particle->pos ) ) {
//		checkvisibletime += trap_Milliseconds() - msec;
		return CG_SetParticleActive( particle, ACT_NOT );
	}

	distance[0] = particle->pos[0] - cg.refdef_current->vieworg[0];
	distance[1] = particle->pos[1] - cg.refdef_current->vieworg[1];
 	if( (distance[0] * distance[0] + distance[1] * distance[1]) > Square( MAX_ATMOSPHERIC_DISTANCE ) )
 	{
		// ydnar: just nuke this particle, let it respawn
		return CG_SetParticleActive( particle, ACT_NOT );
		
		/*
		// Attempt to respot the particle at our other side
		particle->pos[0] -= 1.85f * distance[0];
		particle->pos[1] -= 1.85f * distance[1];

		// ydnar: place particle in random position between ground and sky
		groundHeight = BG_GetSkyGroundHeightAtPoint( particle->pos );
		skyHeight = BG_GetSkyHeightAtPoint( particle->pos );
		if( skyHeight == MAX_ATMOSPHERIC_HEIGHT )
			return CG_SetParticleActive( particle, ACT_NOT );
		particle->pos[ 2 ] = groundHeight + random() * (skyHeight - groundHeight);
		
		// ydnar: valid spot?
		if( particle->pos[ 2 ] <= groundHeight || particle->pos[ 2 ] >= skyHeight )
			return CG_SetParticleActive( particle, ACT_NOT );
		*/
	}

//	checkvisibletime += trap_Milliseconds() - msec;
	return( qtrue );
}


#endif

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC30017c30: complete Windows snow renderer; stack vertices are native polyVert_t. */
enum {
 SnowAxis10 = offsetof(refdef_t, viewaxis) + 3*sizeof(float),
 SnowAxis11 = offsetof(refdef_t, viewaxis) + 4*sizeof(float),
 SnowAxis12 = offsetof(refdef_t, viewaxis) + 5*sizeof(float),
 SnowAxis20 = offsetof(refdef_t, viewaxis) + 6*sizeof(float),
 SnowAxis21 = offsetof(refdef_t, viewaxis) + 7*sizeof(float),
 SnowAxis22 = offsetof(refdef_t, viewaxis) + 8*sizeof(float)
};
typedef char SnowRenderVertexLayoutGuard[(sizeof(polyVert_t)==24 &&
 offsetof(polyVert_t,xyz)==0 && offsetof(polyVert_t,st)==12 &&
 offsetof(polyVert_t,modulate)==20)?1:-1];
static const float snowRenderHalf = 0.5f, snowRenderOne = 1.0f;
static const float snowRenderPhase = 0.03125f, snowRenderAmplitude = 24.0f;
static const float snowRenderZero = 0.0f;
static __declspec(naked) void CG_SnowParticleRender(cg_atmosphericParticle_t *particle)
{
 __asm {
		SUB ESP,0x80
		PUSH ESI
		MOV ESI,dword ptr [ESP + 0x88]
		MOV EAX,dword ptr [ESI + WeatherActive]
		TEST EAX,EAX
		JZ snow_render_30017f24
		PUSH ESI
		CALL CG_CullPoint
		ADD ESP,0x4
		TEST EAX,EAX
		JNZ snow_render_30017f24
		FLD dword ptr [ESI + GenPosX]
		FLD dword ptr [ESI + GenWeight]
		FMUL dword ptr [snowRenderHalf]
		MOV EAX,dword ptr [ESI + GenPosY]
		MOV ECX,dword ptr [ESI + GenPosZ]
		MOV dword ptr [ESP + 0x10],EAX
		LEA EDX,[ESP + 0xc]
		PUSH EDX
		MOV dword ptr [ESP + 0x18],ECX
		FSTP dword ptr [ESP + 0x8]
		FLD dword ptr [snowRenderOne]
		FSUB dword ptr [ESI + GenNormalizedZ]
		FSTP dword ptr [ESP + 0xc]
		FLD dword ptr [ESP + 0x8]
		FMUL dword ptr [ESI + GenPosZ]
		FMUL dword ptr [snowRenderPhase]
		FSIN
		FMUL dword ptr [ESP + 0xc]
		FMUL dword ptr [snowRenderAmplitude]
		FADD ST(0),ST(1)
		FSTP dword ptr [ESP + 0x10]
		FSTP ST(0)
		FLD dword ptr [ESI + GenPosY]
		FADD dword ptr [ESI + GenPosZ]
		FMUL dword ptr [ESP + 0x8]
		FMUL dword ptr [snowRenderPhase]
		FCOS
		FMUL dword ptr [ESP + 0xc]
		FMUL dword ptr [snowRenderAmplitude]
		FADD dword ptr [ESP + 0x14]
		FSTP dword ptr [ESP + 0x14]
		CALL BG_GetSkyGroundHeightAtPoint
		FLD dword ptr [ESP + 0x18]
		MOV EAX,dword ptr [ESI + GenHeight]
		ADD ESP,0x4
		FCOMP ST(1)
		MOV dword ptr [ESP + 0x4],EAX
		FNSTSW AX
		TEST AH,0x41
		JZ snow_render_30017d27
		FLD dword ptr [ESI + GenHeight]
		FSUB ST(0),ST(1)
		FADD dword ptr [ESP + 0x14]
		FSTP dword ptr [ESP + 0x4]
		FSTP ST(0)
		FLD dword ptr [ESP + 0x4]
		FSUB dword ptr [ESI + GenHeight]
		FLD ST(0)
		FMUL dword ptr [ESI + GenNormalizedX]
		FADD dword ptr [ESP + 0xc]
		FSTP dword ptr [ESP + 0xc]
		FLD ST(0)
		FMUL dword ptr [ESI + GenNormalizedY]
		FADD dword ptr [ESP + 0x10]
		FSTP dword ptr [ESP + 0x10]
		FMUL dword ptr [ESI + GenNormalizedZ]
		FADD dword ptr [ESP + 0x14]
		FSTP dword ptr [ESP + 0x14]
		JMP snow_render_30017d29
snow_render_30017d27:
		FSTP ST(0)
snow_render_30017d29:
		FLD dword ptr [ESP + 0x4]
		FCOMP dword ptr [snowRenderZero]
		FNSTSW AX
		TEST AH,0x41
		JNZ snow_render_30017f24
		MOV ECX,dword ptr [cg + WeatherRefdef]
		ADD ECX,GenViewX
		PUSH ECX
		PUSH ESI
		CALL DistanceSquared
		MOV EDX,dword ptr [ESI + GenNormalizedX]
		MOV EAX,dword ptr [ESI + GenNormalizedY]
		FSTP ST(0)
		FLD dword ptr [ESP + 0xc]
		FCHS
		FLD ST(0)
		MOV dword ptr [ESP + 0x2c],EDX
		MOV dword ptr [ESP + 0x30],EAX
		FMUL dword ptr [ESP + 0x2c]
		MOV ECX,dword ptr [ESI + GenNormalizedZ]
		MOV EAX,[cg + WeatherRefdef]
		MOV dword ptr [ESP + 0x34],ECX
		LEA EDX,[ESP + 0x20]
		FADD dword ptr [ESP + 0x14]
		PUSH EDX
		FSTP dword ptr [ESP + 0x3c]
		FLD ST(0)
		FMUL dword ptr [ESP + 0x34]
		FADD dword ptr [ESP + 0x1c]
		FSTP dword ptr [ESP + 0x40]
		FMUL dword ptr [ESP + 0x38]
		FADD dword ptr [ESP + 0x20]
		FSTP dword ptr [ESP + 0x44]
		FLD dword ptr [ESP + 0x30]
		FMUL dword ptr [EAX + SnowAxis10]
		FLD dword ptr [ESP + 0x38]
		FMUL dword ptr [EAX + SnowAxis12]
		FADDP ST(1), ST(0)
		FLD dword ptr [ESP + 0x34]
		FMUL dword ptr [EAX + SnowAxis11]
		FADDP ST(1), ST(0)
		FLD dword ptr [ESP + 0x30]
		FMUL dword ptr [EAX + SnowAxis20]
		FLD dword ptr [ESP + 0x38]
		FMUL dword ptr [EAX + SnowAxis22]
		FADDP ST(1), ST(0)
		FLD dword ptr [ESP + 0x34]
		FMUL dword ptr [EAX + SnowAxis21]
		FADDP ST(1), ST(0)
		FLD ST(0)
		FMUL dword ptr [EAX + SnowAxis10]
		FSTP dword ptr [ESP + 0x24]
		FLD ST(0)
		FMUL dword ptr [EAX + SnowAxis11]
		FSTP dword ptr [ESP + 0x28]
		FMUL dword ptr [EAX + SnowAxis12]
		FSTP dword ptr [ESP + 0x2c]
		FCHS
		FLD ST(0)
		FMUL dword ptr [EAX + SnowAxis20]
		FADD dword ptr [ESP + 0x24]
		FSTP dword ptr [ESP + 0x24]
		FLD ST(0)
		FMUL dword ptr [EAX + SnowAxis21]
		FADD dword ptr [ESP + 0x28]
		FSTP dword ptr [ESP + 0x28]
		FMUL dword ptr [EAX + SnowAxis22]
		FADD dword ptr [ESP + 0x2c]
		FSTP dword ptr [ESP + 0x2c]
		CALL VectorNormalize
		FSTP ST(0)
		FLD dword ptr [ESI + GenWeight]
		FLD ST(0)
		FCHS
		FLD ST(0)
		FMUL dword ptr [ESP + 0x24]
		FST dword ptr [ESP + 0x14]
		FADD dword ptr [ESP + 0x3c]
		FSTP dword ptr [ESP + 0x48]
		FLD ST(0)
		FMUL dword ptr [ESP + 0x28]
		FST dword ptr [ESP + 0x10]
		FADD dword ptr [ESP + 0x40]
		FSTP dword ptr [ESP + 0x4c]
		MOV ECX,dword ptr [ESI + GenShader]
		MOV AL,0xff
		FMUL dword ptr [ESP + 0x2c]
		MOV byte ptr [ESP + 0x5c],AL
		MOV byte ptr [ESP + 0x5d],AL
		MOV byte ptr [ESP + 0x5e],AL
		MOV byte ptr [ESP + 0x5f],AL
		FLD ST(0)
		FADD dword ptr [ESP + 0x44]
		MOV byte ptr [ESP + 0x74],AL
		MOV byte ptr [ESP + 0x75],AL
		MOV byte ptr [ESP + 0x76],AL
		MOV byte ptr [ESP + 0x77],AL
		MOV byte ptr [ESP + 0x8c],AL
		MOV byte ptr [ESP + 0x8d],AL
		FSTP dword ptr [ESP + 0x50]
		FLD dword ptr [ESP + 0x14]
		FADD dword ptr [ESP + 0x18]
		MOV byte ptr [ESP + 0x8e],AL
		MOV byte ptr [ESP + 0x8f],AL
		MOV dword ptr [ESP + 0x54],0x0
		MOV dword ptr [ESP + 0x58],0x0
		MOV dword ptr [ESP + 0x6c],0x0
		MOV dword ptr [ESP + 0x70],0x3f800000
		FSTP dword ptr [ESP + 0x60]
		FLD dword ptr [ESP + 0x10]
		FADD dword ptr [ESP + 0x1c]
		MOV dword ptr [ESP + 0x84],0x3f800000
		MOV dword ptr [ESP + 0x88],0x3f800000
		LEA EAX,[ESP + 0x48]
		PUSH EAX
		FSTP dword ptr [ESP + 0x68]
		FADD dword ptr [ESP + 0x24]
		FSTP dword ptr [ESP + 0x6c]
		FLD ST(0)
		FMUL dword ptr [ESP + 0x28]
		FADD dword ptr [ESP + 0x1c]
		FSTP dword ptr [ESP + 0x7c]
		FLD ST(0)
		FMUL dword ptr [ESP + 0x2c]
		FADD dword ptr [ESP + 0x20]
		FSTP dword ptr [ESP + 0x80]
		FMUL dword ptr [ESP + 0x30]
		FADD dword ptr [ESP + 0x24]
		FSTP dword ptr [ESP + 0x84]
		MOV EDX,dword ptr [ECX]
		PUSH EDX
		CALL CG_AddPolyToPool
		ADD ESP,0x14
snow_render_30017f24:
		POP ESI
		ADD ESP,0x80
		RET
 }
}
#else
static void CG_SnowParticleRender( cg_atmosphericParticle_t *particle )
{
	// Draw a snowflake

	vec3_t		forward, right;
	polyVert_t	verts[3];
	vec2_t		line;
	float		len, frac, sinTumbling, cosTumbling, particleWidth, dist;
	vec3_t		start, finish;
	float		groundHeight;
//	int			msec = trap_Milliseconds();

//	n_rendertime++;

	if( particle->active == ACT_NOT ) {
//		rendertime += trap_Milliseconds() - msec;
		return;
	}

	if( CG_CullPoint( particle->pos ) ) {
		return;
	}

	VectorCopy( particle->pos, start );

	sinTumbling = sin( particle->pos[2] * 0.03125f * ( 0.5f * particle->weight ) );
	cosTumbling = cos( ( particle->pos[2] + particle->pos[1] ) * 0.03125f * ( 0.5f * particle->weight ) );
	start[0] += 24 * ( 1 - particle->deltaNormalized[2] ) * sinTumbling;
	start[1] += 24 * ( 1 - particle->deltaNormalized[2] ) * cosTumbling;

	// Make sure it doesn't clip through surfaces
	groundHeight = BG_GetSkyGroundHeightAtPoint( start );
	len = particle->height;
	if( start[2] <= groundHeight ) {
		// Stop snow going through surfaces.
		len = particle->height - groundHeight + start[2];
		frac = start[2];
		VectorMA( start, len - particle->height, particle->deltaNormalized, start );
	}

	if( len <= 0 ) {
//		rendertime += trap_Milliseconds() - msec;
		return;
	}

	line[0] = particle->pos[0] - cg.refdef_current->vieworg[0];
	line[1] = particle->pos[1] - cg.refdef_current->vieworg[1];

	/* Both TC originals discard the distance result; there is no size inflation. */
	(void)DistanceSquared( particle->pos, cg.refdef_current->vieworg );
	dist = 1.f;

	len *= dist;

	VectorCopy( particle->deltaNormalized, forward );
	VectorMA( start, -( len /** sinTumbling*/ ), forward, finish );

	line[0] = DotProduct( forward, cg.refdef_current->viewaxis[1] );
	line[1] = DotProduct( forward, cg.refdef_current->viewaxis[2] );

	VectorScale( cg.refdef_current->viewaxis[1], line[1], right );
	VectorMA( right, -line[0], cg.refdef_current->viewaxis[2], right );
	VectorNormalize( right );

	particleWidth = dist * (/*cosTumbling **/ particle->weight);

	VectorMA( finish, -particleWidth, right, verts[0].xyz );
	verts[0].st[0] = 0;
	verts[0].st[1] = 0;
	verts[0].modulate[0] = 255;
	verts[0].modulate[1] = 255;
	verts[0].modulate[2] = 255;
	verts[0].modulate[3] = 255;

	VectorMA( start, -particleWidth, right, verts[1].xyz );
	verts[1].st[0] = 0;
	verts[1].st[1] = 1;
	verts[1].modulate[0] = 255;
	verts[1].modulate[1] = 255;
	verts[1].modulate[2] = 255;
	verts[1].modulate[3] = 255;

	VectorMA( start, particleWidth, right, verts[2].xyz );
	verts[2].st[0] = 1;
	verts[2].st[1] = 1;
	verts[2].modulate[0] = 255;
	verts[2].modulate[1] = 255;
	verts[2].modulate[2] = 255;
	verts[2].modulate[3] = 255;

	CG_AddPolyToPool( *particle->effectshader, verts );

//	rendertime += trap_Milliseconds() - msec;
}

/*
**	Set up gust parameters.
*/

#endif

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC weather lifecycle: original x87 stores and native field addresses. */
enum {
	GustField0 = offsetof(cg_atmosphericEffect_t, lastRainTime),
	GustField1 = offsetof(cg_atmosphericEffect_t, numDrops),
	GustField2 = offsetof(cg_atmosphericEffect_t, gustStartTime),
	GustField3 = offsetof(cg_atmosphericEffect_t, gustEndTime),
	GustField4 = offsetof(cg_atmosphericEffect_t, baseStartTime),
	GustField5 = offsetof(cg_atmosphericEffect_t, baseEndTime),
	GustField6 = offsetof(cg_atmosphericEffect_t, gustMinTime),
	GustField7 = offsetof(cg_atmosphericEffect_t, gustMaxTime),
	GustField8 = offsetof(cg_atmosphericEffect_t, changeMinTime),
	GustField9 = offsetof(cg_atmosphericEffect_t, changeMaxTime),
	GustField10 = offsetof(cg_atmosphericEffect_t, baseMinTime),
	GustField11 = offsetof(cg_atmosphericEffect_t, baseMaxTime),
	GustField12 = offsetof(cg_atmosphericEffect_t, baseWeight),
	GustField13 = offsetof(cg_atmosphericEffect_t, gustWeight),
	GustField14 = offsetof(cg_atmosphericEffect_t, baseDrops),
	GustField15 = offsetof(cg_atmosphericEffect_t, gustDrops),
	GustField16 = offsetof(cg_atmosphericEffect_t, baseHeightOffset),
	GustField17 = offsetof(cg_atmosphericEffect_t, numEffectShaders),
	GustField18 = offsetof(cg_atmosphericEffect_t, baseVec),
	GustField19 = offsetof(cg_atmosphericEffect_t, baseVec) + 4,
	GustField20 = offsetof(cg_atmosphericEffect_t, baseVec) + 8,
	GustField21 = offsetof(cg_atmosphericEffect_t, gustVec),
	GustField22 = offsetof(cg_atmosphericEffect_t, gustVec) + 4,
	GustField23 = offsetof(cg_atmosphericEffect_t, gustVec) + 8,
	GustField24 = offsetof(cg_atmosphericEffect_t, viewDir),
	GustField25 = offsetof(cg_atmosphericEffect_t, viewDir) + 4,
	GustField26 = offsetof(cg_atmosphericEffect_t, viewDir) + 8,
	GustField27 = offsetof(cg_atmosphericEffect_t, ParticleCheckVisible),
	GustField28 = offsetof(cg_atmosphericEffect_t, ParticleGenerate),
	GustField29 = offsetof(cg_atmosphericEffect_t, ParticleRender),
	GustField30 = offsetof(cg_atmosphericEffect_t, dropsActive),
	GustField31 = offsetof(cg_atmosphericEffect_t, oldDropsActive),
	GustField32 = offsetof(cg_atmosphericEffect_t, dropsRendered),
	GustField33 = offsetof(cg_atmosphericEffect_t, dropsCreated),
	GustField34 = offsetof(cg_atmosphericEffect_t, dropsSkipped),
	GustCvarValue = offsetof(vmCvar_t, value),
	GustCvarInteger = offsetof(vmCvar_t, integer),
	GustViewX = offsetof(refdef_t, viewaxis),
	GustViewY = offsetof(refdef_t, viewaxis) + sizeof(float),
	GustNextDrop = offsetof(cg_atmosphericParticle_t, nextDropTime),
	GustParticleSize = sizeof(cg_atmosphericParticle_t)
};
static const float gustZero = 0.0f;
static const double gustOne = 1.0;
static __declspec(naked) void CG_WeatherTruncateST0(void)
{
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
static __declspec(naked) void CG_EffectGust(void)
{
	__asm {
		PUSH ESI
		PUSH EDI
		CALL rand
		MOV ESI,dword ptr [cg_atmFx + GustField11]
		MOV ECX,dword ptr [cg_atmFx + GustField10]
		CDQ
		SUB ESI,ECX
		MOV EDI,dword ptr [cg_atmFx + GustField9]
		IDIV ESI
		MOV ESI,EDI
		MOV EAX,EDX
		MOV EDX,dword ptr [cg + WeatherTime]
		ADD EAX,ECX
		MOV ECX,dword ptr [cg_atmFx + GustField8]
		ADD EAX,EDX
		SUB ESI,ECX
		MOV [cg_atmFx + GustField5],EAX
		JZ gust_30017f80
		CALL rand
		MOV EDI,dword ptr [cg_atmFx + GustField9]
		CDQ
		IDIV ESI
		MOV EAX,[cg_atmFx + GustField5]
		JMP gust_30017f82
gust_30017f80:
		XOR EDX,EDX
gust_30017f82:
		MOV ECX,dword ptr [cg_atmFx + GustField8]
		MOV ESI,dword ptr [cg_atmFx + GustField7]
		ADD ECX,EDX
		ADD ECX,EAX
		MOV EAX,[cg_atmFx + GustField6]
		SUB ESI,EAX
		MOV dword ptr [cg_atmFx + GustField2],ECX
		JZ gust_30017fb7
		CALL rand
		MOV ECX,dword ptr [cg_atmFx + GustField2]
		MOV EDI,dword ptr [cg_atmFx + GustField9]
		CDQ
		IDIV ESI
		JMP gust_30017fb9
gust_30017fb7:
		XOR EDX,EDX
gust_30017fb9:
		MOV EAX,[cg_atmFx + GustField6]
		ADD EAX,EDX
		ADD EAX,ECX
		MOV ECX,dword ptr [cg_atmFx + GustField8]
		SUB EDI,ECX
		MOV [cg_atmFx + GustField3],EAX
		JZ gust_30017ff0
		CALL rand
		CDQ
		IDIV EDI
		MOV EAX,[cg_atmFx + GustField3]
		POP EDI
		POP ESI
		ADD EDX,EAX
		MOV EAX,[cg_atmFx + GustField8]
		ADD EDX,EAX
		MOV dword ptr [cg_atmFx + GustField4],EDX
		RET
gust_30017ff0:
		XOR EDX,EDX
		POP EDI
		MOV EDX,EAX
		MOV EAX,[cg_atmFx + GustField8]
		ADD EDX,EAX
		POP ESI
		MOV dword ptr [cg_atmFx + GustField4],EDX
		RET
	}
}

static __declspec(naked) qboolean CG_EffectGustCurrent(vec3_t curr, float *weight, int *num)
{
	__asm {
		MOV EAX,[cg + WeatherTime]
		MOV EDX,dword ptr [cg_atmFx + GustField5]
		SUB ESP,0x10
		CMP EAX,EDX
		JGE gust_3001828e
		FLD dword ptr [cg_atmFx + GustField18]
		MOV EAX,dword ptr [ESP + 0x14]
		FSTP dword ptr [EAX]
		MOV ECX,dword ptr [cg_atmFx + GustField19]
		MOV dword ptr [EAX + 0x4],ECX
		MOV EDX,dword ptr [cg_atmFx + GustField20]
		MOV ECX,dword ptr [ESP + 0x1c]
		MOV dword ptr [EAX + 0x8],EDX
		FLD dword ptr [cg_atmFx + GustField12]
		MOV EAX,dword ptr [ESP + 0x18]
		FSTP dword ptr [EAX]
		MOV EDX,dword ptr [cg_atmFx + GustField14]
		MOV dword ptr [ECX],EDX
gust_30018288:
		XOR EAX,EAX
		ADD ESP,0x10
		RET
gust_3001828e:
		FLD dword ptr [cg_atmFx + GustField21]
		FSUB dword ptr [cg_atmFx + GustField18]
		MOV ECX,dword ptr [cg_atmFx + GustField2]
		CMP EAX,ECX
		FSTP dword ptr [ESP + 0x4]
		FLD dword ptr [cg_atmFx + GustField22]
		FSUB dword ptr [cg_atmFx + GustField19]
		FSTP dword ptr [ESP + 0x8]
		FLD dword ptr [cg_atmFx + GustField23]
		FSUB dword ptr [cg_atmFx + GustField20]
		FSTP dword ptr [ESP + 0xc]
		JGE gust_3001835c
		SUB EAX,EDX
		SUB ECX,EDX
		MOV dword ptr [ESP],EAX
		MOV EAX,dword ptr [ESP + 0x14]
		FILD dword ptr [ESP]
		MOV dword ptr [ESP],ECX
		FILD dword ptr [ESP]
		FDIVP ST(1),ST(0)
		FLD ST(0)
		FMUL dword ptr [ESP + 0x4]
		FADD dword ptr [cg_atmFx + GustField18]
		FSTP dword ptr [EAX]
		FLD ST(0)
		FMUL dword ptr [ESP + 0x8]
		FADD dword ptr [cg_atmFx + GustField19]
		FSTP dword ptr [EAX + 0x4]
		FLD ST(0)
		FMUL dword ptr [ESP + 0xc]
		FADD dword ptr [cg_atmFx + GustField20]
		FSTP dword ptr [EAX + 0x8]
		FLD dword ptr [cg_atmFx + GustField13]
		FSUB dword ptr [cg_atmFx + GustField12]
		MOV EAX,dword ptr [ESP + 0x18]
		FMUL ST(0), ST(1)
		FADD dword ptr [cg_atmFx + GustField12]
		FSTP dword ptr [EAX]
		MOV ECX,dword ptr [cg_atmFx + GustField15]
		MOV EAX,[cg_atmFx + GustField14]
		SUB ECX,EAX
		MOV dword ptr [ESP + 0x14],ECX
		FILD dword ptr [ESP + 0x14]
		FMUL ST(0), ST(1)
		FIADD dword ptr [cg_atmFx + GustField14]
		CALL CG_WeatherTruncateST0
		MOV EDX,dword ptr [ESP + 0x1c]
		FSTP ST(0)
		MOV dword ptr [EDX],EAX
		XOR EAX,EAX
		ADD ESP,0x10
		RET
gust_3001835c:
		MOV ECX,dword ptr [cg_atmFx + GustField3]
		CMP EAX,ECX
		JGE gust_300183a2
		FLD dword ptr [cg_atmFx + GustField21]
		MOV EAX,dword ptr [ESP + 0x14]
		FSTP dword ptr [EAX]
		MOV ECX,dword ptr [cg_atmFx + GustField22]
		MOV dword ptr [EAX + 0x4],ECX
		MOV EDX,dword ptr [cg_atmFx + GustField23]
		MOV ECX,dword ptr [ESP + 0x1c]
		MOV dword ptr [EAX + 0x8],EDX
		FLD dword ptr [cg_atmFx + GustField13]
		MOV EAX,dword ptr [ESP + 0x18]
		FSTP dword ptr [EAX]
		MOV EDX,dword ptr [cg_atmFx + GustField15]
		XOR EAX,EAX
		MOV dword ptr [ECX],EDX
		ADD ESP,0x10
		RET
gust_300183a2:
		SUB EAX,ECX
		MOV dword ptr [ESP],EAX
		MOV EAX,[cg_atmFx + GustField4]
		FILD dword ptr [ESP]
		SUB EAX,ECX
		MOV ECX,dword ptr [ESP + 0x18]
		MOV dword ptr [ESP],EAX
		MOV EAX,dword ptr [ESP + 0x14]
		FILD dword ptr [ESP]
		FDIVP ST(1),ST(0)
		FSUBR qword ptr [gustOne]
		FLD ST(0)
		FMUL dword ptr [ESP + 0x4]
		FADD dword ptr [cg_atmFx + GustField18]
		FSTP dword ptr [EAX]
		FLD ST(0)
		FMUL dword ptr [ESP + 0x8]
		FADD dword ptr [cg_atmFx + GustField19]
		FSTP dword ptr [EAX + 0x4]
		FLD ST(0)
		FMUL dword ptr [ESP + 0xc]
		FADD dword ptr [cg_atmFx + GustField20]
		FSTP dword ptr [EAX + 0x8]
		FLD dword ptr [cg_atmFx + GustField13]
		FSUB dword ptr [cg_atmFx + GustField12]
		FMUL ST(0), ST(1)
		FADD dword ptr [cg_atmFx + GustField12]
		FSTP dword ptr [ECX]
		MOV EDX,dword ptr [cg_atmFx + GustField15]
		MOV ECX,dword ptr [cg_atmFx + GustField14]
		SUB EDX,ECX
		MOV dword ptr [ESP + 0x14],EDX
		FILD dword ptr [ESP + 0x14]
		FMUL ST(0), ST(1)
		FIADD dword ptr [cg_atmFx + GustField14]
		CALL CG_WeatherTruncateST0
		MOV ECX,dword ptr [ESP + 0x1c]
		FSTP ST(0)
		MOV dword ptr [ECX],EAX
		MOV EDX,dword ptr [cg + WeatherTime]
		CMP EDX,dword ptr [cg_atmFx + GustField4]
		JL gust_30018288
		MOV EAX,0x1
		ADD ESP,0x10
		RET
	}
}

#else
static void CG_EffectGust()
{
	// Generate random values for the next gust

	int diff;

	cg_atmFx.baseEndTime		= cg.time					+ cg_atmFx.baseMinTime		+ (rand() % (cg_atmFx.baseMaxTime - cg_atmFx.baseMinTime));
	diff						= cg_atmFx.changeMaxTime	- cg_atmFx.changeMinTime;
	cg_atmFx.gustStartTime		= cg_atmFx.baseEndTime		+ cg_atmFx.changeMinTime	+ (diff ? (rand() % diff) : 0);
	diff						= cg_atmFx.gustMaxTime		- cg_atmFx.gustMinTime;
	cg_atmFx.gustEndTime		= cg_atmFx.gustStartTime	+ cg_atmFx.gustMinTime		+ (diff ? (rand() % diff) : 0);
	diff						= cg_atmFx.changeMaxTime	- cg_atmFx.changeMinTime;
	cg_atmFx.baseStartTime		= cg_atmFx.gustEndTime		+ cg_atmFx.changeMinTime	+ (diff ? (rand() % diff) : 0);
}

static qboolean CG_EffectGustCurrent( vec3_t curr, float *weight, int *num )
{
	// Calculate direction for new drops.

	vec3_t temp;
	float frac;

	if( cg.time < cg_atmFx.baseEndTime )
	{
		VectorCopy( cg_atmFx.baseVec, curr );
		*weight = cg_atmFx.baseWeight;
		*num = cg_atmFx.baseDrops;
	}
	else {
		VectorSubtract( cg_atmFx.gustVec, cg_atmFx.baseVec, temp );
		if( cg.time < cg_atmFx.gustStartTime )
		{
			frac = ((float)(cg.time - cg_atmFx.baseEndTime))/((float)(cg_atmFx.gustStartTime - cg_atmFx.baseEndTime));
			VectorMA( cg_atmFx.baseVec, frac, temp, curr );
			*weight = cg_atmFx.baseWeight + (cg_atmFx.gustWeight - cg_atmFx.baseWeight) * frac;
			*num = cg_atmFx.baseDrops + ((float)(cg_atmFx.gustDrops - cg_atmFx.baseDrops)) * frac;
		}
		else if( cg.time < cg_atmFx.gustEndTime )
		{
			VectorCopy( cg_atmFx.gustVec, curr );
			*weight = cg_atmFx.gustWeight;
			*num = cg_atmFx.gustDrops;
		}
		else
		{
			frac = 1.0 - ((float)(cg.time - cg_atmFx.gustEndTime))/((float)(cg_atmFx.baseStartTime - cg_atmFx.gustEndTime));
			VectorMA( cg_atmFx.baseVec, frac, temp, curr );
			*weight = cg_atmFx.baseWeight + (cg_atmFx.gustWeight - cg_atmFx.baseWeight) * frac;
			*num = cg_atmFx.baseDrops + ((float)(cg_atmFx.gustDrops - cg_atmFx.baseDrops)) * frac;
			if( cg.time >= cg_atmFx.baseStartTime )
				return( qtrue );
		}
	}
	return( qfalse );
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86)
/* Whole Windows range parsers 30018010/30018090. Native atof preserves the
 * call ABI, but its CRT implementation remains an explicit dependency. */
static double (__cdecl * const weatherRangeAtof)(const char *) = atof;
static __declspec(naked) void CG_EP_ParseFloats( char *floatstr, float *f1, float *f2 )
{
	__asm {
		SUB ESP,040h
		LEA EAX,[ESP]
		PUSH ESI
		PUSH EDI
		MOV EDI,dword ptr [ESP + 04ch]
		PUSH 040h
		PUSH EDI
		PUSH EAX
		CALL Q_strncpyz
		MOV AL,byte ptr [ESP + 014h]
		ADD ESP,0ch
		TEST AL,AL
		LEA ESI,[ESP + 08h]
		JZ weatherRange30018041
weatherRange30018035:
		CMP AL,020h
		JZ weatherRange30018041
		MOV AL,byte ptr [ESI + 01h]
		INC ESI
		TEST AL,AL
		JNZ weatherRange30018035
weatherRange30018041:
		CMP byte ptr [ESI],00h
		JZ weatherRange3001806b
		PUSH EDI
		MOV byte ptr [ESI],00h
		CALL dword ptr [weatherRangeAtof]
		MOV ECX,dword ptr [ESP + 054h]
		INC ESI
		PUSH ESI
		FSTP dword ptr [ECX]
		CALL dword ptr [weatherRangeAtof]
		MOV EDX,dword ptr [ESP + 05ch]
		ADD ESP,08h
		FSTP dword ptr [EDX]
		POP EDI
		POP ESI
		ADD ESP,040h
		RET
weatherRange3001806b:
		PUSH EDI
		CALL dword ptr [weatherRangeAtof]
		MOV EAX,dword ptr [ESP + 058h]
		MOV ECX,dword ptr [ESP + 054h]
		FLD st(0)
		ADD ESP,04h
		FSTP dword ptr [EAX]
		POP EDI
		POP ESI
		FSTP dword ptr [ECX]
		ADD ESP,040h
		RET
	}
}

static __declspec(naked) void CG_EP_ParseInts( char *intstr, int *i1, int *i2 )
{
	__asm {
		SUB ESP,040h
		LEA EAX,[ESP]
		PUSH ESI
		PUSH EDI
		MOV EDI,dword ptr [ESP + 04ch]
		PUSH 040h
		PUSH EDI
		PUSH EAX
		CALL Q_strncpyz
		MOV AL,byte ptr [ESP + 014h]
		ADD ESP,0ch
		TEST AL,AL
		LEA ESI,[ESP + 08h]
		JZ weatherRange300180c1
weatherRange300180b5:
		CMP AL,020h
		JZ weatherRange300180c1
		MOV AL,byte ptr [ESI + 01h]
		INC ESI
		TEST AL,AL
		JNZ weatherRange300180b5
weatherRange300180c1:
		CMP byte ptr [ESI],00h
		JZ weatherRange300180f5
		PUSH EDI
		MOV byte ptr [ESI],00h
		CALL dword ptr [weatherRangeAtof]
		CALL CG_WeatherTruncateST0
		MOV ECX,dword ptr [ESP + 054h]
		INC ESI
		PUSH ESI
		MOV dword ptr [ECX],EAX
		CALL dword ptr [weatherRangeAtof]
		ADD ESP,08h
		CALL CG_WeatherTruncateST0
		MOV EDX,dword ptr [ESP + 054h]
		POP EDI
		POP ESI
		MOV dword ptr [EDX],EAX
		ADD ESP,040h
		RET
weatherRange300180f5:
		PUSH EDI
		CALL dword ptr [weatherRangeAtof]
		ADD ESP,04h
		CALL CG_WeatherTruncateST0
		MOV ECX,dword ptr [ESP + 054h]
		MOV EDX,dword ptr [ESP + 050h]
		POP EDI
		POP ESI
		MOV dword ptr [ECX],EAX
		MOV dword ptr [EDX],EAX
		ADD ESP,040h
		RET
	}
}
#else
static void CG_EP_ParseFloats( char *floatstr, float *f1, float *f2 )
{
	// Parse the float or floats

	char *middleptr;
	char buff[64];

	Q_strncpyz( buff, floatstr, sizeof(buff) );
	for( middleptr = buff; *middleptr && *middleptr != ' '; middleptr++ );
	if( *middleptr )
	{
		*middleptr++ = 0;
		*f1 = atof( floatstr );
		*f2 = atof( middleptr );
	}
	else {
		*f1 = *f2 = atof( floatstr );
	}
}

static void CG_EP_ParseInts( char *intstr, int *i1, int *i2 )
{
	// Parse the int or ints

	char *middleptr;
	char buff[64];

	Q_strncpyz( buff, intstr, sizeof(buff) );
	for( middleptr = buff; *middleptr && *middleptr != ' '; middleptr++ );
	if( *middleptr )
	{
		*middleptr++ = 0;
		*i1 = atof( intstr );
		*i2 = atof( middleptr );
	}
	else {
		*i1 = *i2 = atof( intstr );
	}
}

#endif

void CG_EffectParse( const char *effectstr )
{
	// Split the string into it's component parts.

	float bmin, bmax, cmin, cmax, gmin, gmax, bdrop, gdrop/*, wsplash, lsplash*/;
	int count, bheight;
	char *startptr, *eqptr, *endptr;
	char workbuff[128];
	atmFXType_t atmFXType = ATM_NONE;

	if( CG_AtmosphericKludge() ) {
		return;
	}

		// Set up some default values
	cg_atmFx.baseVec[0] = cg_atmFx.baseVec[1] = 0;
	cg_atmFx.gustVec[0] = cg_atmFx.gustVec[1] = 100;
	bmin = 5;
	bmax = 10;
	cmin = 1;
	cmax = 1;
	gmin = 0;
	gmax = 2;
	bdrop = gdrop = 300;
	cg_atmFx.baseWeight = 0.7f;
	cg_atmFx.gustWeight = 1.5f;
	bheight = 0;

		// Parse the parameter string
	Q_strncpyz( workbuff, effectstr, sizeof(workbuff) );
	for( startptr = workbuff; *startptr; )
	{
		for( eqptr = startptr; *eqptr && *eqptr != '=' && *eqptr != ','; eqptr++ );
		if( !*eqptr )
			break;			// No more string
		if( *eqptr == ',' )
		{
			startptr = eqptr + 1;	// Bad argument, continue
			continue;
		}
		*eqptr++ = 0;
		for( endptr = eqptr; *endptr && *endptr != ','; endptr++ );
		if( *endptr )
			*endptr++ = 0;

		if( atmFXType == ATM_NONE )
		{
			if( Q_stricmp( startptr, "T" ) ) {
				cg_atmFx.numDrops = 0;
				CG_Printf( "Atmospheric effect must start with a type.\n" );
				return;
			}
			if( !Q_stricmp( eqptr, "RAIN" ) ) {
				atmFXType = ATM_RAIN;
				cg_atmFx.ParticleCheckVisible = &CG_RainParticleCheckVisible;
				cg_atmFx.ParticleGenerate = &CG_RainParticleGenerate;
				cg_atmFx.ParticleRender = &CG_RainParticleRender;

				/* TC sets literal -800, not the SDK 1.1*gravity speed. */
				cg_atmFx.baseVec[2] = cg_atmFx.gustVec[2] = -800.0f;
			} else if( !Q_stricmp( eqptr, "SNOW" ) ) {
				atmFXType = ATM_SNOW;
				cg_atmFx.ParticleCheckVisible = &CG_SnowParticleCheckVisible;
				cg_atmFx.ParticleGenerate = &CG_SnowParticleGenerate;
				cg_atmFx.ParticleRender = &CG_SnowParticleRender;

				cg_atmFx.baseVec[2] = cg_atmFx.gustVec[2] = - ATMOSPHERIC_SNOW_SPEED;
			} else {
				cg_atmFx.numDrops = 0;
				CG_Printf( "Only effect type 'rain' and 'snow' are supported.\n" );
				return;
			}
		}
		else {
			if( !Q_stricmp( startptr, "B" ) )
				CG_EP_ParseFloats( eqptr, &bmin, &bmax );
			else if( !Q_stricmp( startptr, "C" ) )
				CG_EP_ParseFloats( eqptr, &cmin, &cmax );
			else if( !Q_stricmp( startptr, "G" ) )
				CG_EP_ParseFloats( eqptr, &gmin, &gmax );
			else if( !Q_stricmp( startptr, "BV" ) )
				CG_EP_ParseFloats( eqptr, &cg_atmFx.baseVec[0], &cg_atmFx.baseVec[1] );
			else if( !Q_stricmp( startptr, "GV" ) )
				CG_EP_ParseFloats( eqptr, &cg_atmFx.gustVec[0], &cg_atmFx.gustVec[1] );
			else if( !Q_stricmp( startptr, "W" ) )
				CG_EP_ParseFloats( eqptr, &cg_atmFx.baseWeight, &cg_atmFx.gustWeight );
			else if( !Q_stricmp( startptr, "D" ) )
				CG_EP_ParseFloats( eqptr, &bdrop, &gdrop );
			else if( !Q_stricmp( startptr, "H" ) )
				CG_EP_ParseInts( eqptr, &bheight, &bheight );
			else CG_Printf( "Unknown effect key '%s'.\n", startptr );
		}
		startptr = endptr;
	}

	if( atmFXType == ATM_NONE || !cg.tceTraceMapLoaded ) {
		// No effects

		cg_atmFx.numDrops = -1;
		return;
	}

	cg_atmFx.baseHeightOffset = bheight;
	if( cg_atmFx.baseHeightOffset < 0 )
		cg_atmFx.baseHeightOffset = 0;
	cg_atmFx.baseMinTime = 1000 * bmin;
	cg_atmFx.baseMaxTime = 1000 * bmax;
	cg_atmFx.changeMinTime = 1000 * cmin;
	cg_atmFx.changeMaxTime = 1000 * cmax;
	cg_atmFx.gustMinTime = 1000 * gmin;
	cg_atmFx.gustMaxTime = 1000 * gmax;
	cg_atmFx.baseDrops = bdrop;
	cg_atmFx.gustDrops = gdrop;

	cg_atmFx.numDrops = (cg_atmFx.baseDrops > cg_atmFx.gustDrops) ? cg_atmFx.baseDrops : cg_atmFx.gustDrops;
	if( cg_atmFx.numDrops > MAX_ATMOSPHERIC_PARTICLES ) {
		cg_atmFx.numDrops = MAX_ATMOSPHERIC_PARTICLES;
	}	
	// Load graphics

	// Rain
	if( atmFXType == ATM_RAIN ) {
		cg_atmFx.numEffectShaders = 1;
		cg_atmFx.effectshaders[0] = trap_R_RegisterShader( "gfx/misc/raindrop" );
		if( !(cg_atmFx.effectshaders[0]) ) {
			cg_atmFx.effectshaders[0] = -1;
			cg_atmFx.numEffectShaders = 0;
		}

	// Snow
	} else if( atmFXType == ATM_SNOW ) {
		cg_atmFx.numEffectShaders = 1;
		cg_atmFx.effectshaders[0] = trap_R_RegisterShader( "gfx/misc/snow" );

	// This really should never happen
	} else {
		cg_atmFx.numEffectShaders = 0;
	}

		// Initialise atmospheric effect to prevent all particles falling at the start
	for( count = 0; count < cg_atmFx.numDrops; count++ )
		cg_atmFx.particles[count].nextDropTime = ATMOSPHERIC_DROPDELAY + (rand() % ATMOSPHERIC_DROPDELAY);

	CG_EffectGust();
}

/*
** Main render loop
*/

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void CG_AddAtmosphericEffects(void)
{
	__asm {
		SUB ESP,0x14
		PUSH ESI
		PUSH EDI
		MOV EDI,dword ptr [cg_atmFx + GustField1]
		XOR ESI,ESI
		CMP EDI,ESI
		JLE gust_3001822e
		CMP dword ptr [cg_atmFx + GustField17],ESI
		JZ gust_3001822e
		FLD dword ptr [cg_atmosphericEffects + GustCvarValue]
		FCOMP dword ptr [gustZero]
		FNSTSW AX
		TEST AH,0x41
		JZ gust_30018160
		CMP dword ptr [developer + GustCvarInteger],ESI
		JNZ gust_3001822e
gust_30018160:
		LEA EAX,[ESP + 0xc]
		LEA ECX,[ESP + 0x8]
		PUSH EAX
		LEA EDX,[ESP + 0x14]
		PUSH ECX
		PUSH EDX
		CALL CG_EffectGustCurrent
		ADD ESP,0xc
		TEST EAX,EAX
		JZ gust_30018180
		CALL CG_EffectGust
gust_30018180:
		MOV EAX,[cg_atmFx + GustField30]
		MOV dword ptr [cg_atmFx + GustField30],ESI
		MOV [cg_atmFx + GustField31],EAX
		MOV EAX,[cg + WeatherRefdef]
		MOV dword ptr [cg_atmFx + GustField34],ESI
		MOV dword ptr [cg_atmFx + GustField33],ESI
		MOV dword ptr [cg_atmFx + GustField32],ESI
		CMP EDI,ESI
		FLD dword ptr [EAX + GustViewX]
		FSTP dword ptr [cg_atmFx + GustField24]
		FLD dword ptr [EAX + GustViewY]
		FSTP dword ptr [cg_atmFx + GustField25]
		MOV dword ptr [cg_atmFx + GustField26],0x0
		JLE gust_30018222
		LEA ESI,cg_atmFx.particles
gust_300181cc:
		PUSH ESI
		CALL dword ptr [cg_atmFx + GustField27]
		ADD ESP,0x4
		TEST EAX,EAX
		JNZ gust_30018207
		MOV ECX,dword ptr [ESP + 0x8]
		LEA EDX,[ESP + 0x10]
		PUSH ECX
		PUSH EDX
		PUSH ESI
		CALL dword ptr [cg_atmFx + GustField28]
		ADD ESP,0xc
		TEST EAX,EAX
		JNZ gust_30018201
		MOV EAX,[cg + WeatherTime]
		ADD EAX,0x3e8
		MOV dword ptr [ESI + GustNextDrop],EAX
		JMP gust_3001821c
gust_30018201:
		INC dword ptr [cg_atmFx + GustField33]
gust_30018207:
		PUSH ESI
		CALL dword ptr [cg_atmFx + GustField29]
		MOV EAX,[cg_atmFx + GustField30]
		ADD ESP,0x4
		INC EAX
		MOV [cg_atmFx + GustField30],EAX
gust_3001821c:
		ADD ESI,GustParticleSize
		DEC EDI
		JNZ gust_300181cc
gust_30018222:
		MOV ECX,dword ptr [cg + WeatherTime]
		MOV dword ptr [cg_atmFx + GustField0],ECX
gust_3001822e:
		POP EDI
		POP ESI
		ADD ESP,0x14
		RET
	}
}
#else
void CG_AddAtmosphericEffects()
{
	// Add atmospheric effects (e.g. rain, snow etc.) to view

	int curr, max, currnum;
	cg_atmosphericParticle_t *particle;
	vec3_t currvec;
	float currweight;

	max = cg_atmFx.numDrops;
	if( max <= 0 || cg_atmFx.numEffectShaders == 0 || (!(cg_atmosphericEffects.value > 0) && developer.integer != 0) )
		return;

#ifndef ATM_NEW
	CG_ClearPolyPool();
#endif // ATM_NEW

	if( CG_EffectGustCurrent( currvec, &currweight, &currnum ) )
		CG_EffectGust();			// Recalculate gust parameters

	// ydnar: allow parametric management of drop count for swelling/waning precip
	cg_atmFx.oldDropsActive = cg_atmFx.dropsActive;
	cg_atmFx.dropsActive = 0;
	
	cg_atmFx.dropsRendered = cg_atmFx.dropsCreated = cg_atmFx.dropsSkipped = 0;

//	getgroundtime = getskytime = rendertime = checkvisibletime = generatetime = 0;
//	n_getgroundtime = n_getskytime = n_rendertime = n_checkvisibletime = n_generatetime = 0;

	VectorSet( cg_atmFx.viewDir, cg.refdef_current->viewaxis[0][0], cg.refdef_current->viewaxis[0][1], 0.f );

	for( curr = 0; curr < max; curr++ )
	{
		particle = &cg_atmFx.particles[curr];
		//%	if( !CG_SnowParticleCheckVisible( particle ) )
		if( !cg_atmFx.ParticleCheckVisible( particle ) )
		{
			// Effect has terminated / fallen from screen view
			/*
			if( !particle->nextDropTime )
			{
				// Stop rain being synchronized 
				particle->nextDropTime = cg.time + rand() % ATMOSPHERIC_DROPDELAY;
			}
			if( currnum < curr || particle->nextDropTime > cg.time )
			{
				cg_atmFx.dropsRendered++;
				continue;
			} */
			//%	if( !CG_SnowParticleGenerate( particle, currvec, currweight ) )
			if( !cg_atmFx.ParticleGenerate( particle, currvec, currweight ) )
			{
				// Ensure it doesn't attempt to generate every frame, to prevent
				// 'clumping' when there's only a small sky area available.
				particle->nextDropTime = cg.time + ATMOSPHERIC_DROPDELAY;
				continue;
			}
			else
			{
				cg_atmFx.dropsCreated++;
			}
		}
		
		//%	CG_RainParticleRender( particle );
		cg_atmFx.ParticleRender( particle );
		cg_atmFx.dropsActive++;
	}

//	CG_RenderPolyPool();

	cg_atmFx.lastRainTime = cg.time;
	
//	CG_Printf( "Active: %d Generated: %d Rendered: %d Skipped: %d\n", cg_atmFx.dropsActive, cg_atmFx.dropsCreated, cg_atmFx.dropsRendered, cg_atmFx.dropsSkipped );
//	CG_Printf( "gg: %i gs: %i rt: %i cv: %i ge: %i\n", getgroundtime, getskytime, rendertime, checkvisibletime, generatetime );
//	CG_Printf( "\\-> %i \\-> %i \\-> %i \\-> %i \\-> %i\n", n_getgroundtime, n_getskytime, n_rendertime, n_checkvisibletime, n_generatetime );
}
#endif
