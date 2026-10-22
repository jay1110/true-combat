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
#include "../game/tce_numeric.h"

#define MAX_ATMOSPHERIC_HEIGHT          MAX_MAP_SIZE    // maximum world height
#define MIN_ATMOSPHERIC_HEIGHT          -MAX_MAP_SIZE   // minimum world height

//int getgroundtime, getskytime, rendertime, checkvisibletime, generatetime;
//int n_getgroundtime, n_getskytime, n_rendertime, n_checkvisibletime, n_generatetime;

//static qboolean CG_LoadTraceMap( void );

#define MAX_ATMOSPHERIC_PARTICLES       4000    // maximum # of particles
#define MAX_ATMOSPHERIC_DISTANCE        1000    // maximum distance from refdef origin that particles are visible
#define MAX_ATMOSPHERIC_EFFECTSHADERS   6       // maximum different effectshaders for an atmospheric effect
#define ATMOSPHERIC_DROPDELAY           1000
#define ATMOSPHERIC_CUTHEIGHT           800

#define ATMOSPHERIC_RAIN_SPEED      (1.1f * DEFAULT_GRAVITY)
#define ATMOSPHERIC_RAIN_HEIGHT     150

#define ATMOSPHERIC_SNOW_SPEED      (0.1f * DEFAULT_GRAVITY)
#define ATMOSPHERIC_SNOW_HEIGHT     3

typedef enum
{
	ATM_NONE,
	ATM_RAIN,
	ATM_SNOW
} atmFXType_t;

#ifndef ATM_NEW
/*
** Atmospheric Particles PolyPool
*/
static polyVert_t atmPolyPool[MAX_ATMOSPHERIC_PARTICLES * 3];
static int        numParticlesInFrame;
static qhandle_t  atmPolyShader;

static void CG_ClearPolyPool(void)
{
	numParticlesInFrame = 0;
	atmPolyShader       = 0;
}

static void CG_RenderPolyPool(void)
{
	if (numParticlesInFrame)
	{
		trap_R_AddPolysToScene(atmPolyShader, 3, atmPolyPool, numParticlesInFrame);
		CG_ClearPolyPool();
	}
}
#endif // ATM_NEW

static void CG_AddPolyToPool(qhandle_t shader, const polyVert_t *verts)
{
#ifndef ATM_NEW
	if (atmPolyShader && atmPolyShader != shader)
	{
		CG_RenderPolyPool();
	}

	if (numParticlesInFrame == MAX_ATMOSPHERIC_PARTICLES)
	{
		CG_RenderPolyPool();
	}

	atmPolyShader = shader;
	memcpy(&atmPolyPool[numParticlesInFrame * 3], verts, 3 * sizeof(polyVert_t));
	numParticlesInFrame++;
#else
	int firstIndex;
	int firstVertex;
	int i;

	polyBuffer_t *pPolyBuffer = CG_PB_FindFreePolyBuffer(shader, 3, 3);
	if (!pPolyBuffer)
	{
		return;
	}

	firstIndex  = pPolyBuffer->numIndicies;
	firstVertex = pPolyBuffer->numVerts;

	for ( i = 0; i < 3; i++ )
	{
		VectorCopy(verts[i].xyz, pPolyBuffer->xyz[firstVertex + i]);

		pPolyBuffer->st[firstVertex + i][0]    = verts[i].st[0];
		pPolyBuffer->st[firstVertex + i][1]    = verts[i].st[1];
		pPolyBuffer->color[firstVertex + i][0] = verts[i].modulate[0];
		pPolyBuffer->color[firstVertex + i][1] = verts[i].modulate[1];
		pPolyBuffer->color[firstVertex + i][2] = verts[i].modulate[2];
		pPolyBuffer->color[firstVertex + i][3] = verts[i].modulate[3];

		pPolyBuffer->indicies[firstIndex + i] = firstVertex + i;

	}

	pPolyBuffer->numIndicies += 3;
	pPolyBuffer->numVerts    += 3;
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

	if (kludgeChecked)
	{
		return(kludgeResult);
	}
	kludgeChecked = qtrue;
	kludgeResult  = qfalse;

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

	return(kludgeResult = qfalse);
}

typedef enum
{
	ACT_NOT,
	ACT_FALLING
} active_t;

typedef struct cg_atmosphericParticle_s
{
	vec3_t pos, delta, deltaNormalized, colour;
	float height, weight;
	active_t active;
	int nextDropTime;
	qhandle_t *effectshader;
} cg_atmosphericParticle_t;

typedef struct cg_atmosphericEffect_s
{
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

	qboolean (*ParticleCheckVisible)(cg_atmosphericParticle_t *particle);
	qboolean (*ParticleGenerate)(cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight);
	void (*ParticleRender)(cg_atmosphericParticle_t *particle);

	int dropsActive, oldDropsActive;
	int dropsRendered, dropsCreated, dropsSkipped;
} cg_atmosphericEffect_t;

static cg_atmosphericEffect_t cg_atmFx;

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC30017500/30017b70: native layouts, original double time scale. */
enum
{
	WeatherTime     = offsetof(cg_t, time),
	WeatherRefdef   = offsetof(cg_t, refdef_current),
	WeatherLastTime = offsetof(cg_atmosphericEffect_t, lastRainTime),
	WeatherActive   = offsetof(cg_atmosphericParticle_t, active),
	WeatherHeight   = offsetof(cg_atmosphericParticle_t, height),
	WeatherPosX     = offsetof(cg_atmosphericParticle_t, pos),
	WeatherPosY     = offsetof(cg_atmosphericParticle_t, pos) + sizeof(float),
	WeatherPosZ     = offsetof(cg_atmosphericParticle_t, pos) + 2 * sizeof(float),
	WeatherDeltaX   = offsetof(cg_atmosphericParticle_t, delta),
	WeatherDeltaY   = offsetof(cg_atmosphericParticle_t, delta) + sizeof(float),
	WeatherDeltaZ   = offsetof(cg_atmosphericParticle_t, delta) + 2 * sizeof(float),
	WeatherViewX    = offsetof(refdef_t, vieworg),
	WeatherViewY    = offsetof(refdef_t, vieworg) + sizeof(float)
};
#endif


static qboolean CG_SetParticleActive(cg_atmosphericParticle_t *particle, active_t active)
{
	particle->active = active;
	return active ? qtrue : qfalse;
}


/*
**	Raindrop management functions
*/

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC weather generators: complete Windows instructions, native object offsets. */
enum
{
	GenHeightOffset = offsetof(cg_atmosphericEffect_t, baseHeightOffset),
	GenOldDrops     = offsetof(cg_atmosphericEffect_t, oldDropsActive),
	GenNumDrops     = offsetof(cg_atmosphericEffect_t, numDrops),
	GenShaders      = offsetof(cg_atmosphericEffect_t, effectshaders),
	GenViewX        = offsetof(refdef_t, vieworg),
	GenViewY        = offsetof(refdef_t, vieworg) + sizeof(float),
	GenViewZ        = offsetof(refdef_t, vieworg) + 2 * sizeof(float),
	GenPosX         = offsetof(cg_atmosphericParticle_t, pos) + 0 * sizeof(float),
	GenPosY         = offsetof(cg_atmosphericParticle_t, pos) + 1 * sizeof(float),
	GenPosZ         = offsetof(cg_atmosphericParticle_t, pos) + 2 * sizeof(float),
	GenDeltaX       = offsetof(cg_atmosphericParticle_t, delta) + 0 * sizeof(float),
	GenDeltaY       = offsetof(cg_atmosphericParticle_t, delta) + 1 * sizeof(float),
	GenDeltaZ       = offsetof(cg_atmosphericParticle_t, delta) + 2 * sizeof(float),
	GenNormalizedX  = offsetof(cg_atmosphericParticle_t, deltaNormalized) + 0 * sizeof(float),
	GenNormalizedY  = offsetof(cg_atmosphericParticle_t, deltaNormalized) + 1 * sizeof(float),
	GenNormalizedZ  = offsetof(cg_atmosphericParticle_t, deltaNormalized) + 2 * sizeof(float),
	GenColourX      = offsetof(cg_atmosphericParticle_t, colour) + 0 * sizeof(float),
	GenColourY      = offsetof(cg_atmosphericParticle_t, colour) + 1 * sizeof(float),
	GenColourZ      = offsetof(cg_atmosphericParticle_t, colour) + 2 * sizeof(float),
	GenHeight       = offsetof(cg_atmosphericParticle_t, height),
	GenWeight       = offsetof(cg_atmosphericParticle_t, weight),
	GenShader       = offsetof(cg_atmosphericParticle_t, effectshader)
};
typedef char WeatherGeneratorLayoutGuard[(sizeof(float) == 4 && offsetof(cg_atmosphericParticle_t, pos) == 0 && sizeof(vec3_t) == 12)?1:-1];
static const float  genConstant3009254c = 0.000030518509447574615f;
static const float  genConstant30092310 = 6.2831854820251465f;
static const double genConstant30092828 = 1000.0;
static const double genConstant30092820 = 20.0;
static const float  genConstant300927f4 = 65536.0f;
static const double genConstant30092318 = 0.001;
static const double genConstant300922e0 = 0.5;
static const double genConstant30092818 = 51.0;
static const double genConstant30092810 = 0.6;
static const double genConstant300922f8 = 100.0;
static const double genConstant30092808 = 150.0;
static const float  genConstant30092804 = 0.75f;
static const double genConstant30092840 = 25.0;
static const float  genConstant300922b4 = 1.0f;
static const float  genConstant300922b8 = 0.5f;
static __declspec(naked) qboolean CG_RainParticleGenerate(cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight)
{
	__asm {
		SUB ESP, 0x8
		PUSH ESI
		CALL rand
		AND EAX, 0x7fff
		MOV dword ptr[ESP + 0x4], EAX
		FILD dword ptr[ESP + 0x4]
		FMUL dword ptr[genConstant3009254c]
		FMUL dword ptr[genConstant30092310]
		FSTP dword ptr[ESP + 0x4]
		CALL rand
		AND EAX, 0x7fff
		MOV ESI, dword ptr[ESP + 0x10]
		MOV dword ptr[ESP + 0x8], EAX
		MOV EAX, [cg + WeatherRefdef]
		FILD dword ptr[ESP + 0x8]
		PUSH ESI
		FMUL dword ptr[genConstant3009254c]
		FSQRT
		FMUL qword ptr[genConstant30092828]
		FADD qword ptr[genConstant30092820]
		FLD dword ptr[ESP + 0x8]
		FSIN
		FLD ST(1)
		FMULP ST(1), ST(0)
		FADD dword ptr[EAX + GenViewX]
		FSTP dword ptr[ESI + GenPosX]
		FLD dword ptr[ESP + 0x8]
		FCOS
		MOV ECX, dword ptr[cg + WeatherRefdef]
		FXCH ST(1)
		FMULP ST(1), ST(0)
		FADD dword ptr[ECX + GenViewY]
		FSTP dword ptr[ESI + GenPosY]
		CALL BG_GetSkyHeightAtPoint
		FST dword ptr[ESP + 0x8]
		FCOMP dword ptr[genConstant300927f4]
		ADD ESP, 0x4
		FNSTSW AX
		TEST AH, 0x40
		JZ gen_300172eb
		XOR EAX, EAX
		POP ESI
		ADD ESP, 0x8
		RET
gen_300172eb:
		PUSH ESI
		CALL BG_GetSkyGroundHeightAtPoint
		FST dword ptr[ESP + 0x14]
		FCOMP dword ptr[ESP + 0x8]
		ADD ESP, 0x4
		FNSTSW AX
		TEST AH, 0x1
		JNZ gen_3001730a
		XOR EAX, EAX
		POP ESI
		ADD ESP, 0x8
		RET
gen_3001730a:
		CALL rand
		AND EAX, 0x7fff
		MOV dword ptr[ESP + 0x8], EAX
		FILD dword ptr[ESP + 0x8]
		FMUL dword ptr[genConstant3009254c]
		FLD dword ptr[ESP + 0x4]
		FSUB dword ptr[ESP + 0x10]
		FMULP ST(1), ST(0)
		FADD dword ptr[ESP + 0x10]
		FST dword ptr[ESP + 0x8]
		FSTP dword ptr[ESI + GenPosZ]
		MOV EAX, [cg_atmFx + GenHeightOffset]
		TEST EAX, EAX
		JLE gen_30017376
		FILD dword ptr[cg_atmFx + GenHeightOffset]
		MOV ECX, dword ptr[cg + WeatherRefdef]
		FLD dword ptr[ESP + 0x8]
		FSUB dword ptr[ECX + GenViewZ]
		FCOMP ST(1)
		FNSTSW AX
		TEST AH, 0x41
		JNZ gen_30017374
		FADD dword ptr[ECX + GenViewZ]
		FST dword ptr[ESI + GenPosZ]
		FCOMP dword ptr[ESP + 0x10]
		FNSTSW AX
		TEST AH, 0x1
		JZ gen_30017376
		XOR EAX, EAX
		POP ESI
		ADD ESP, 0x8
		RET
gen_30017374:
		FSTP ST(0)
		gen_30017376 :
		MOV EAX, [cg + WeatherTime]
		MOV ECX, 0x2710
		CDQ
		FILD dword ptr[cg_atmFx + GenOldDrops]
		IDIV ECX
		MOV EAX, ECX
		SUB EAX, EDX
		MOV dword ptr[ESP + 0x10], EAX
		FILD dword ptr[ESP + 0x10]
		FMUL qword ptr[genConstant30092318]
		FADD qword ptr[genConstant300922e0]
		FIMUL dword ptr[cg_atmFx + GenNumDrops]
		FXCH ST(1)
		FXCH ST(1)
		FCOMPP
		FNSTSW AX
		TEST AH, 0x1
		JZ gen_300173bb
		XOR EAX, EAX
		POP ESI
		ADD ESP, 0x8
		RET
gen_300173bb:
		PUSH 0x1
		PUSH ESI
		CALL CG_SetParticleActive
		CALL rand
		AND EAX, 0x7fff
		MOV dword ptr[ESP + 0x18], EAX
		FILD dword ptr[ESP + 0x18]
		FMUL dword ptr[genConstant3009254c]
		FMUL qword ptr[genConstant30092818]
		FADD qword ptr[genConstant30092810]
		FSTP dword ptr[ESI + GenColourX]
		CALL rand
		AND EAX, 0x7fff
		MOV dword ptr[ESP + 0x18], EAX
		FILD dword ptr[ESP + 0x18]
		FMUL dword ptr[genConstant3009254c]
		FMUL qword ptr[genConstant30092818]
		FADD qword ptr[genConstant30092810]
		FSTP dword ptr[ESI + GenColourY]
		CALL rand
		AND EAX, 0x7fff
		MOV dword ptr[ESP + 0x18], EAX
		MOV EAX, dword ptr[ESP + 0x1c]
		FILD dword ptr[ESP + 0x18]
		FMUL dword ptr[genConstant3009254c]
		FMUL qword ptr[genConstant30092818]
		FADD qword ptr[genConstant30092810]
		FSTP dword ptr[ESI + GenColourZ]
		MOV ECX, dword ptr[EAX]
		MOV dword ptr[ESI + GenDeltaX], ECX
		MOV EDX, dword ptr[EAX + 0x4]
		MOV dword ptr[ESI + GenDeltaY], EDX
		MOV EAX, dword ptr[EAX + 0x8]
		MOV dword ptr[ESI + GenDeltaZ], EAX
		CALL rand
		AND EAX, 0x7fff
		MOV ECX, dword ptr[ESI + GenDeltaX]
		MOV dword ptr[ESP + 0x18], EAX
		MOV EDX, dword ptr[ESI + GenDeltaY]
		FILD dword ptr[ESP + 0x18]
		LEA EAX, [ESI + GenNormalizedX]
		MOV dword ptr[ESI + GenNormalizedY], EDX
		PUSH EAX
		FMUL dword ptr[genConstant3009254c]
		MOV dword ptr[EAX], ECX
		FSUB qword ptr[genConstant300922e0]
		FADD ST(0), ST(0)
		FMUL qword ptr[genConstant300922f8]
		FADD dword ptr[ESI + GenDeltaZ]
		FST dword ptr[ESI + GenDeltaZ]
		FSTP dword ptr[ESI + GenNormalizedZ]
		CALL VectorNormalizeFast
		ADD ESP, 0xc
		CALL rand
		AND EAX, 0x7fff
		PUSH EAX
		LEA EAX, [cg_atmFx + GenShaders]
		MOV dword ptr[ESI + GenShader], EAX
		POP EAX
		MOV dword ptr[ESP + 0x10], EAX
		MOV EAX, 0x1
		FILD dword ptr[ESP + 0x10]
		FMUL dword ptr[genConstant3009254c]
		FSUB qword ptr[genConstant300922e0]
		FADD ST(0), ST(0)
		FMUL qword ptr[genConstant300922f8]
		FADD qword ptr[genConstant30092808]
		FSTP dword ptr[ESI + GenHeight]
		FLD dword ptr[ESP + 0x18]
		FMUL dword ptr[genConstant30092804]
		FSTP dword ptr[ESI + GenWeight]
		POP ESI
		ADD ESP, 0x8
		RET
	}
}
#else
static qboolean CG_RainParticleGenerate(cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight)
{
	// Attempt to 'spot' a raindrop somewhere below a sky texture.

	float angle, distance;
	float groundHeight, skyHeight;
//	int msec = trap_Milliseconds();

//	n_generatetime++;

	angle    = random() * 2 * M_PI;
	distance = 20 + MAX_ATMOSPHERIC_DISTANCE * sqrt(random());

	particle->pos[0] = cg.refdef_current->vieworg[0] + sin(angle) * distance;
	particle->pos[1] = cg.refdef_current->vieworg[1] + cos(angle) * distance;

	// ydnar: choose a spawn point randomly between sky and ground
	skyHeight = BG_GetSkyHeightAtPoint(particle->pos);
	if (skyHeight == MAX_ATMOSPHERIC_HEIGHT)
	{
		return qfalse;
	}
	groundHeight = BG_GetSkyGroundHeightAtPoint(particle->pos);
	if (groundHeight >= skyHeight)
	{
		return qfalse;
	}
	particle->pos[2] = groundHeight + random() * (skyHeight - groundHeight);

	// make sure it doesn't fall from too far cause it then will go over our heads ('lower the ceiling')
	if (cg_atmFx.baseHeightOffset > 0)
	{
		if (particle->pos[2] - cg.refdef_current->vieworg[2] > cg_atmFx.baseHeightOffset)
		{
			particle->pos[2] = cg.refdef_current->vieworg[2] + cg_atmFx.baseHeightOffset;

			if (particle->pos[2] < groundHeight)
			{
				return qfalse;
			}
		}
	}

	// ydnar: rain goes in bursts
	{
		float maxActiveDrops;

		// every 10 seconds allow max raindrops
		maxActiveDrops = 0.50 * cg_atmFx.numDrops + 0.001 * cg_atmFx.numDrops * (10000 - (cg.time % 10000));
		if (cg_atmFx.oldDropsActive > maxActiveDrops)
		{
			return qfalse;
		}
	}

	CG_SetParticleActive(particle, ACT_FALLING);
	particle->colour[0] = 0.6 + 0.2 * random() * 0xFF;
	particle->colour[1] = 0.6 + 0.2 * random() * 0xFF;
	particle->colour[2] = 0.6 + 0.2 * random() * 0xFF;
	VectorCopy(currvec, particle->delta);
	particle->delta[2] += crandom() * 100;
	VectorCopy(particle->delta, particle->deltaNormalized);
	VectorNormalizeFast(particle->deltaNormalized);
	particle->height       = ATMOSPHERIC_RAIN_HEIGHT + crandom() * 100;
	particle->weight       = currweight * 0.75f;
	particle->effectshader = &cg_atmFx.effectshaders[0];
//	particle->effectshader = &cg_atmFx.effectshaders[ (int) (random() * ( cg_atmFx.numEffectShaders - 1 )) ];

//	generatetime += trap_Milliseconds() - msec;
	return(qtrue);
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86)
static qboolean CG_RainParticleCheckVisible(cg_atmosphericParticle_t *particle)
{
	long double elapsed, z, dx, dy;
	float bottom;

	if (!particle || particle->active == ACT_NOT)
	{
		return qfalse;
	}
	elapsed = (long double)TCE_SubInt32(cg.time, cg_atmFx.lastRainTime) * (long double)(double)0.001;
	particle->pos[0] = (float)(elapsed * particle->delta[0] + particle->pos[0]);
	particle->pos[1] = (float)(elapsed * particle->delta[1] + particle->pos[1]);
	z = elapsed * particle->delta[2] + particle->pos[2];
	particle->pos[2] = (float)z;
	/* The ground test uses a separate float store of the held Z plus height. */
	bottom = (float)(z + particle->height);
	if (bottom < BG_GetSkyGroundHeightAtPoint(particle->pos))
	{
		return CG_SetParticleActive(particle, ACT_NOT);
	}

	dx = (long double)particle->pos[0] - cg.refdef_current->vieworg[0];
	dy = (long double)particle->pos[1] - cg.refdef_current->vieworg[1];
	if (dy * dy + dx * dx > 1000000.0L)
	{
		return CG_SetParticleActive(particle, ACT_NOT);
	}
	return qtrue;
}
#elif defined(__linux__) && !defined(Q3_VM)
static qboolean CG_RainParticleCheckVisible(cg_atmosphericParticle_t *particle)
{
	volatile float moved, bottom, distanceX, distanceY;
	long double factor, productY, productZ, positionY, positionZ, ground;

	if (!particle || particle->active == ACT_NOT)
	{
		return qfalse;
	}
	moved = (float)((long double)TCE_SubInt32(cg.time, cg_atmFx.lastRainTime) *
	                (long double)0.001);
	factor = (long double)moved;
	particle->pos[0] = (float)((long double)particle->delta[0] * factor + particle->pos[0]);
	productY = (long double)particle->delta[1] * factor;
	productZ = factor * (long double)particle->delta[2];
	positionY = productY + (long double)particle->pos[1];
	positionZ = productZ + (long double)particle->pos[2];
	particle->pos[1] = (float)positionY;
	particle->pos[2] = (float)positionZ;
	/* The rain bottom uses held Z before its separate float store. */
	bottom = (float)(positionZ + (long double)particle->height);
	ground = (long double)BG_GetSkyGroundHeightAtPoint(particle->pos);
	if (!((long double)bottom >= ground))
	{
		return CG_SetParticleActive(particle, ACT_NOT);
	}
	distanceX = (float)((long double)particle->pos[0] - cg.refdef_current->vieworg[0]);
	distanceY = (float)((long double)particle->pos[1] - cg.refdef_current->vieworg[1]);
	if ((long double)distanceX * distanceX + (long double)distanceY * distanceY > 1000000.0L)
	{
		return CG_SetParticleActive(particle, ACT_NOT);
	}
	return qtrue;
}
#else

static qboolean CG_RainParticleCheckVisible(cg_atmosphericParticle_t *particle)
{
	// Check the raindrop is visible and still going, wrapping if necessary.

	float  moved;
	vec2_t distance;
//	int msec = trap_Milliseconds();

	if (!particle || particle->active == ACT_NOT)
	{
//		checkvisibletime += trap_Milliseconds() - msec;
		return(qfalse);
	}

	moved = (cg.time - cg_atmFx.lastRainTime) * 0.001;  // Units moved since last frame
	VectorMA(particle->pos, moved, particle->delta, particle->pos);
	if (particle->pos[2] + particle->height < BG_GetSkyGroundHeightAtPoint(particle->pos))
	{
//		checkvisibletime += trap_Milliseconds() - msec;
		return CG_SetParticleActive(particle, ACT_NOT);
	}

	distance[0] = particle->pos[0] - cg.refdef_current->vieworg[0];
	distance[1] = particle->pos[1] - cg.refdef_current->vieworg[1];
	if ((distance[0] * distance[0] + distance[1] * distance[1]) > Square(MAX_ATMOSPHERIC_DISTANCE))
	{
		// ydnar: just nuke this particle, let it respawn
		return CG_SetParticleActive(particle, ACT_NOT);

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
	return(qtrue);
}


#endif

static void CG_RainParticleRender(cg_atmosphericParticle_t *particle)
{
	// Draw a raindrop

	vec3_t     forward, right;
	polyVert_t verts[3];
	vec2_t     line;
	float      len, frac, dist;
	vec3_t     start, finish;
	float      groundHeight;
//	int			msec = trap_Milliseconds();

//	n_rendertime++;

	if (particle->active == ACT_NOT)
	{
//		rendertime += trap_Milliseconds() - msec;
		return;
	}

	if (CG_CullPoint(particle->pos))
	{
		return;
	}

	VectorCopy(particle->pos, start);

	dist = DistanceSquared(particle->pos, cg.refdef_current->vieworg);

	// Make sure it doesn't clip through surfaces
	groundHeight = BG_GetSkyGroundHeightAtPoint(start);
	len          = particle->height;
	if (start[2] <= groundHeight)
	{
		// Stop snow going through surfaces.
		len  = particle->height - groundHeight + start[2];
		frac = start[2];
		VectorMA(start, len - particle->height, particle->deltaNormalized, start);
	}

	if (len <= 0)
	{
//		rendertime += trap_Milliseconds() - msec;
		return;
	}

	// fade nearby rain particles
	if (dist < Square(128.f))
	{
		dist = .25f + .75f * (dist / Square(128.f));
	}
	else
	{
		dist = 1.0f;
	}

	VectorCopy(particle->deltaNormalized, forward);
	VectorMA(start, -len, forward, finish);

	line[0] = DotProduct(forward, cg.refdef_current->viewaxis[1]);
	line[1] = DotProduct(forward, cg.refdef_current->viewaxis[2]);

	VectorScale(cg.refdef_current->viewaxis[1], line[1], right);
	VectorMA(right, -line[0], cg.refdef_current->viewaxis[2], right);
	VectorNormalize(right);

	// dist = 1.0;

	VectorCopy(finish, verts[0].xyz);
	verts[0].st[0]       = 0.5f;
	verts[0].st[1]       = 0;
	verts[0].modulate[0] = particle->colour[0];
	verts[0].modulate[1] = particle->colour[1];
	verts[0].modulate[2] = particle->colour[2];
	verts[0].modulate[3] = 100 * dist;

	VectorMA(start, -particle->weight, right, verts[1].xyz);
	verts[1].st[0]       = 0;
	verts[1].st[1]       = 1;
	verts[1].modulate[0] = particle->colour[0];
	verts[1].modulate[1] = particle->colour[1];
	verts[2].modulate[2] = particle->colour[2];
	verts[1].modulate[3] = 200 * dist;

	VectorMA(start, particle->weight, right, verts[2].xyz);
	verts[2].st[0]       = 1;
	verts[2].st[1]       = 1;
	verts[2].modulate[0] = particle->colour[0];
	verts[2].modulate[1] = particle->colour[1];
	verts[2].modulate[2] = particle->colour[2];
	verts[2].modulate[3] = 200 * dist;

	CG_AddPolyToPool(*particle->effectshader, verts);

//	rendertime += trap_Milliseconds() - msec;
}

/*
**	Snow management functions
*/

#if defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) qboolean CG_SnowParticleGenerate(cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight)
{
	__asm {
		SUB ESP, 0x8
		PUSH ESI
		CALL rand
		AND EAX, 0x7fff
		MOV dword ptr[ESP + 0x4], EAX
		FILD dword ptr[ESP + 0x4]
		FMUL dword ptr[genConstant3009254c]
		FMUL dword ptr[genConstant30092310]
		FSTP dword ptr[ESP + 0x4]
		CALL rand
		AND EAX, 0x7fff
		MOV ESI, dword ptr[ESP + 0x10]
		MOV dword ptr[ESP + 0x8], EAX
		MOV EAX, [cg + WeatherRefdef]
		FILD dword ptr[ESP + 0x8]
		PUSH ESI
		FMUL dword ptr[genConstant3009254c]
		FSQRT
		FMUL qword ptr[genConstant30092828]
		FADD qword ptr[genConstant30092820]
		FLD dword ptr[ESP + 0x8]
		FSIN
		FLD ST(1)
		FMULP ST(1), ST(0)
		FADD dword ptr[EAX + GenViewX]
		FSTP dword ptr[ESI + GenPosX]
		FLD dword ptr[ESP + 0x8]
		FCOS
		MOV ECX, dword ptr[cg + WeatherRefdef]
		FXCH ST(1)
		FMULP ST(1), ST(0)
		FADD dword ptr[ECX + GenViewY]
		FSTP dword ptr[ESI + GenPosY]
		CALL BG_GetSkyHeightAtPoint
		FST dword ptr[ESP + 0x8]
		FCOMP dword ptr[genConstant300927f4]
		ADD ESP, 0x4
		FNSTSW AX
		TEST AH, 0x40
		JZ gen_30017a3b
		XOR EAX, EAX
		POP ESI
		ADD ESP, 0x8
		RET
gen_30017a3b:
		PUSH ESI
		CALL BG_GetSkyGroundHeightAtPoint
		FST dword ptr[ESP + 0x14]
		FCOMP dword ptr[ESP + 0x8]
		ADD ESP, 0x4
		FNSTSW AX
		TEST AH, 0x1
		JNZ gen_30017a5a
		XOR EAX, EAX
		POP ESI
		ADD ESP, 0x8
		RET
gen_30017a5a:
		CALL rand
		AND EAX, 0x7fff
		MOV dword ptr[ESP + 0x8], EAX
		FILD dword ptr[ESP + 0x8]
		FMUL dword ptr[genConstant3009254c]
		FLD dword ptr[ESP + 0x4]
		FSUB dword ptr[ESP + 0x10]
		FMULP ST(1), ST(0)
		FADD dword ptr[ESP + 0x10]
		FST dword ptr[ESP + 0x8]
		FSTP dword ptr[ESI + GenPosZ]
		MOV EAX, [cg_atmFx + GenHeightOffset]
		TEST EAX, EAX
		JLE gen_30017ac6
		FILD dword ptr[cg_atmFx + GenHeightOffset]
		MOV ECX, dword ptr[cg + WeatherRefdef]
		FLD dword ptr[ESP + 0x8]
		FSUB dword ptr[ECX + GenViewZ]
		FCOMP ST(1)
		FNSTSW AX
		TEST AH, 0x41
		JNZ gen_30017ac4
		FADD dword ptr[ECX + GenViewZ]
		FST dword ptr[ESI + GenPosZ]
		FCOMP dword ptr[ESP + 0x10]
		FNSTSW AX
		TEST AH, 0x1
		JZ gen_30017ac6
		XOR EAX, EAX
		POP ESI
		ADD ESP, 0x8
		RET
gen_30017ac4:
		FSTP ST(0)
		gen_30017ac6 :
		PUSH 0x1
		PUSH ESI
		CALL CG_SetParticleActive
		MOV EAX, dword ptr[ESP + 0x1c]
		MOV EDX, dword ptr[EAX]
		MOV dword ptr[ESI + GenDeltaX], EDX
		MOV ECX, dword ptr[EAX + 0x4]
		MOV dword ptr[ESI + GenDeltaY], ECX
		MOV EDX, dword ptr[EAX + 0x8]
		MOV dword ptr[ESI + GenDeltaZ], EDX
		CALL rand
		AND EAX, 0x7fff
		MOV ECX, dword ptr[ESI + GenDeltaX]
		MOV dword ptr[ESP + 0x18], EAX
		MOV EDX, dword ptr[ESI + GenDeltaY]
		FILD dword ptr[ESP + 0x18]
		LEA EAX, [ESI + GenNormalizedX]
		MOV dword ptr[ESI + GenNormalizedY], EDX
		PUSH EAX
		FMUL dword ptr[genConstant3009254c]
		MOV dword ptr[EAX], ECX
		FSUB qword ptr[genConstant300922e0]
		FADD ST(0), ST(0)
		FMUL qword ptr[genConstant30092840]
		FADD dword ptr[ESI + GenDeltaZ]
		FST dword ptr[ESI + GenDeltaZ]
		FSTP dword ptr[ESI + GenNormalizedZ]
		CALL VectorNormalizeFast
		ADD ESP, 0xc
		CALL rand
		AND EAX, 0x7fff
		PUSH EAX
		LEA EAX, [cg_atmFx + GenShaders]
		MOV dword ptr[ESI + GenShader], EAX
		POP EAX
		MOV dword ptr[ESP + 0x10], EAX
		MOV EAX, 0x1
		FILD dword ptr[ESP + 0x10]
		FMUL dword ptr[genConstant3009254c]
		FADD ST(0), ST(0)
		FADD dword ptr[genConstant300922b4]
		FST dword ptr[ESI + GenHeight]
		FMUL dword ptr[genConstant300922b8]
		FSTP dword ptr[ESI + GenWeight]
		POP ESI
		ADD ESP, 0x8
		RET
	}
}
#else
static qboolean CG_SnowParticleGenerate(cg_atmosphericParticle_t *particle, vec3_t currvec, float currweight)
{
	// Attempt to 'spot' a snowflake somewhere below a sky texture.

	float angle, distance;
	float groundHeight, skyHeight;
//	int msec = trap_Milliseconds();

//	n_generatetime++;

	angle    = random() * 2 * M_PI;
	distance = 20 + MAX_ATMOSPHERIC_DISTANCE * sqrt(random());

	particle->pos[0] = cg.refdef_current->vieworg[0] + sin(angle) * distance;
	particle->pos[1] = cg.refdef_current->vieworg[1] + cos(angle) * distance;

	// ydnar: choose a spawn point randomly between sky and ground
	skyHeight = BG_GetSkyHeightAtPoint(particle->pos);
	if (skyHeight == MAX_ATMOSPHERIC_HEIGHT)
	{
		return qfalse;
	}
	groundHeight = BG_GetSkyGroundHeightAtPoint(particle->pos);
	if (groundHeight >= skyHeight)
	{
		return qfalse;
	}
	particle->pos[2] = groundHeight + random() * (skyHeight - groundHeight);

	// make sure it doesn't fall from too far cause it then will go over our heads ('lower the ceiling')
	if (cg_atmFx.baseHeightOffset > 0)
	{
		if (particle->pos[2] - cg.refdef_current->vieworg[2] > cg_atmFx.baseHeightOffset)
		{
			particle->pos[2] = cg.refdef_current->vieworg[2] + cg_atmFx.baseHeightOffset;
			if (particle->pos[2] < groundHeight)
			{
				return qfalse;
			}
		}
	}

	CG_SetParticleActive(particle, ACT_FALLING);
	VectorCopy(currvec, particle->delta);
	particle->delta[2] += crandom() * 25;
	VectorCopy(particle->delta, particle->deltaNormalized);
	VectorNormalizeFast(particle->deltaNormalized);
	particle->height       = 1.0f + random() * 2;
	particle->weight       = particle->height * 0.5f;
	particle->effectshader = &cg_atmFx.effectshaders[0];
//	particle->effectshader = &cg_atmFx.effectshaders[ (int) (random() * ( cg_atmFx.numEffectShaders - 1 )) ];

//	generatetime += trap_Milliseconds() - msec;
	return(qtrue);
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86)
static qboolean CG_SnowParticleCheckVisible(cg_atmosphericParticle_t *particle)
{
	long double elapsed, dx, dy;

	if (!particle || particle->active == ACT_NOT)
	{
		return qfalse;
	}
	elapsed = (long double)TCE_SubInt32(cg.time, cg_atmFx.lastRainTime) * (long double)(double)0.001;
	particle->pos[0] = (float)(elapsed * particle->delta[0] + particle->pos[0]);
	particle->pos[1] = (float)(elapsed * particle->delta[1] + particle->pos[1]);
	particle->pos[2] = (float)(elapsed * particle->delta[2] + particle->pos[2]);
	if (particle->pos[2] < BG_GetSkyGroundHeightAtPoint(particle->pos))
	{
		return CG_SetParticleActive(particle, ACT_NOT);
	}

	dx = (long double)particle->pos[0] - cg.refdef_current->vieworg[0];
	dy = (long double)particle->pos[1] - cg.refdef_current->vieworg[1];
	if (dy * dy + dx * dx > 1000000.0L)
	{
		return CG_SetParticleActive(particle, ACT_NOT);
	}
	return qtrue;
}
#elif defined(__linux__) && !defined(Q3_VM)
static qboolean CG_SnowParticleCheckVisible(cg_atmosphericParticle_t *particle)
{
	volatile float moved, distanceX, distanceY;
	long double factor, productY, productZ, positionY, positionZ, ground;

	if (!particle || particle->active == ACT_NOT)
	{
		return qfalse;
	}
	moved = (float)((long double)TCE_SubInt32(cg.time, cg_atmFx.lastRainTime) *
	                (long double)0.001);
	factor = (long double)moved;
	particle->pos[0] = (float)((long double)particle->delta[0] * factor + particle->pos[0]);
	productY = (long double)particle->delta[1] * factor;
	productZ = factor * (long double)particle->delta[2];
	positionY = productY + (long double)particle->pos[1];
	positionZ = productZ + (long double)particle->pos[2];
	particle->pos[1] = (float)positionY;
	particle->pos[2] = (float)positionZ;
	ground = (long double)BG_GetSkyGroundHeightAtPoint(particle->pos);
	if (!((long double)particle->pos[2] >= ground))
	{
		return CG_SetParticleActive(particle, ACT_NOT);
	}
	distanceX = (float)((long double)particle->pos[0] - cg.refdef_current->vieworg[0]);
	distanceY = (float)((long double)particle->pos[1] - cg.refdef_current->vieworg[1]);
	if ((long double)distanceX * distanceX + (long double)distanceY * distanceY > 1000000.0L)
	{
		return CG_SetParticleActive(particle, ACT_NOT);
	}
	return qtrue;
}
#else

static qboolean CG_SnowParticleCheckVisible(cg_atmosphericParticle_t *particle)
{
	// Check the snowflake is visible and still going, wrapping if necessary.

	float  moved;
	vec2_t distance;
//	int msec = trap_Milliseconds();

//	n_checkvisibletime++;

	if (!particle || particle->active == ACT_NOT)
	{
//		checkvisibletime += trap_Milliseconds() - msec;
		return(qfalse);
	}

	moved = (cg.time - cg_atmFx.lastRainTime) * 0.001;  // Units moved since last frame
	VectorMA(particle->pos, moved, particle->delta, particle->pos);
	if (particle->pos[2] < BG_GetSkyGroundHeightAtPoint(particle->pos))
	{
//		checkvisibletime += trap_Milliseconds() - msec;
		return CG_SetParticleActive(particle, ACT_NOT);
	}

	distance[0] = particle->pos[0] - cg.refdef_current->vieworg[0];
	distance[1] = particle->pos[1] - cg.refdef_current->vieworg[1];
	if ((distance[0] * distance[0] + distance[1] * distance[1]) > Square(MAX_ATMOSPHERIC_DISTANCE))
	{
		// ydnar: just nuke this particle, let it respawn
		return CG_SetParticleActive(particle, ACT_NOT);

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
	return(qtrue);
}


#endif

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC30017c30: retain the original intermediate stores and triangle layout. */
typedef char SnowRenderVertexLayoutGuard[(sizeof(polyVert_t) == 24 &&
                                          offsetof(polyVert_t, xyz) == 0 && offsetof(polyVert_t, st) == 12 &&
                                          offsetof(polyVert_t, modulate) == 20)?1:-1];
static void CG_SnowParticleRender(cg_atmosphericParticle_t *particle)
{
	vec3_t start, finish, forward, right;
	polyVert_t verts[3];
	float halfWeight, attenuation, length, negativeX, negativeY;
	long double ground, offset, alongRight, alongUp, phase;
	long double negative[3];
	int i;

	if (particle->active == ACT_NOT || CG_CullPoint(particle->pos))
	{
		return;
	}
	VectorCopy(particle->pos, start);
	halfWeight = (float)((long double)particle->weight * 0.5L);
	attenuation = (float)(1.0L - particle->deltaNormalized[2]);
	phase = (long double)halfWeight * particle->pos[2] * 0.03125L;
	start[0] = (float)(sinl(phase) * attenuation * 24.0L + start[0]);
	phase = ((long double)particle->pos[1] + particle->pos[2]) * halfWeight * 0.03125L;
	start[1] = (float)(cosl(phase) * attenuation * 24.0L + start[1]);

	ground = BG_GetSkyGroundHeightAtPoint(start);
	length = particle->height;
	/* The native unordered comparison follows the clipping path too. */
	if (!((long double)start[2] > ground))
	{
		length = (float)((long double)particle->height - ground + start[2]);
		offset = (long double)length - particle->height;
		for (i = 0; i < 3; ++i)
		{
			start[i] = (float)(offset * particle->deltaNormalized[i] + start[i]);
		}
	}
	if (!(length > 0.0f))
	{
		return;
	}
	(void)DistanceSquared(particle->pos, cg.refdef_current->vieworg);
	VectorCopy(particle->deltaNormalized, forward);
	for (i = 0; i < 3; ++i)
	{
		finish[i] = (float)(-(long double)length * forward[i] + start[i]);
	}

	/* Both native dot products add X, Z, then Y without a float spill. */
	alongRight = ((long double)forward[0] * cg.refdef_current->viewaxis[1][0] +
	              (long double)forward[2] * cg.refdef_current->viewaxis[1][2]) +
	             (long double)forward[1] * cg.refdef_current->viewaxis[1][1];
	alongUp = ((long double)forward[0] * cg.refdef_current->viewaxis[2][0] +
	           (long double)forward[2] * cg.refdef_current->viewaxis[2][2]) +
	          (long double)forward[1] * cg.refdef_current->viewaxis[2][1];
	for (i = 0; i < 3; ++i)
	{
		right[i] = (float)(alongUp * cg.refdef_current->viewaxis[1][i]);
	}
	for (i = 0; i < 3; ++i)
	{
		right[i] = (float)(-alongRight * cg.refdef_current->viewaxis[2][i] + right[i]);
	}
	VectorNormalize(right);

	for (i = 0; i < 3; ++i)
	{
		negative[i] = -(long double)particle->weight * right[i];
		verts[0].xyz[i] = (float)(negative[i] + finish[i]);
		verts[2].xyz[i] = (float)((long double)particle->weight * right[i] + start[i]);
		memset(verts[i].modulate, 255, sizeof(verts[i].modulate));
	}
	/* The second corner reloads rounded X/Y products, while Z stays held. */
	negativeX = (float)negative[0];
	negativeY = (float)negative[1];
	verts[1].xyz[0] = (float)((long double)negativeX + start[0]);
	verts[1].xyz[1] = (float)((long double)negativeY + start[1]);
	verts[1].xyz[2] = (float)(negative[2] + start[2]);
	verts[0].st[0] = 0; verts[0].st[1] = 0;
	verts[1].st[0] = 0; verts[1].st[1] = 1;
	verts[2].st[0] = 1; verts[2].st[1] = 1;
	CG_AddPolyToPool(*particle->effectshader, verts);
}

#else
static void CG_SnowParticleRender(cg_atmosphericParticle_t *particle)
{
	// Draw a snowflake

	vec3_t     forward, right;
	polyVert_t verts[3];
	vec2_t     line;
	float      len, frac, sinTumbling, cosTumbling, particleWidth, dist;
	vec3_t     start, finish;
	float      groundHeight;
#if defined(__linux__) && !defined(Q3_VM)
	long double phase, attenuation, visibleLength;
	volatile float sine, cosine;
#endif
//	int			msec = trap_Milliseconds();

//	n_rendertime++;

	if (particle->active == ACT_NOT)
	{
//		rendertime += trap_Milliseconds() - msec;
		return;
	}

	if (CG_CullPoint(particle->pos))
	{
		return;
	}

	VectorCopy(particle->pos, start);

#if defined(__linux__) && !defined(Q3_VM)
	phase = (long double)particle->pos[2] * (long double)particle->weight;
	phase *= 0.015625L;
	sine = (float)sinl(phase);
	phase = (long double)particle->pos[2] + (long double)particle->pos[1];
	phase = 0.015625L * ((long double)particle->weight * phase);
	cosine = (float)cosl(phase);
	attenuation = 1.0L - (long double)particle->deltaNormalized[2];
	start[0] = (float)((long double)start[0] + ((long double)sine * attenuation) * 24.0L);
	start[1] = (float)(24.0L * ((long double)cosine * attenuation) + (long double)start[1]);
#else
	sinTumbling = sin(particle->pos[2] * 0.03125f * (0.5f * particle->weight));
	cosTumbling = cos((particle->pos[2] + particle->pos[1]) * 0.03125f * (0.5f * particle->weight));
	start[0]   += 24 * (1 - particle->deltaNormalized[2]) * sinTumbling;
	start[1]   += 24 * (1 - particle->deltaNormalized[2]) * cosTumbling;
#endif

	// Make sure it doesn't clip through surfaces
	groundHeight = BG_GetSkyGroundHeightAtPoint(start);
	len          = particle->height;
#if defined(__linux__) && !defined(Q3_VM)
	visibleLength = (long double)len;
	if (!((long double)start[2] > (long double)groundHeight))
	{
		long double clippedLength = (visibleLength - (long double)groundHeight) + start[2];
		long double offset = clippedLength - visibleLength;
		int axis;

		for (axis = 0; axis < 3; ++axis)
		{
			long double product = axis == 2 ? offset * (long double)particle->deltaNormalized[axis] :
			                      (long double)particle->deltaNormalized[axis] * offset;
			start[axis] = (float)(product + (long double)start[axis]);
		}
		visibleLength = clippedLength;
	}
	if (visibleLength <= 0.0L)
	{
		return;
	}
	/* Native length is rounded only after the visibility decision. */
	len = (float)visibleLength;
#else
	if (start[2] <= groundHeight)
	{
		// Stop snow going through surfaces.
		len  = particle->height - groundHeight + start[2];
		frac = start[2];
		VectorMA(start, len - particle->height, particle->deltaNormalized, start);
	}

	if (len <= 0)
	{
//		rendertime += trap_Milliseconds() - msec;
		return;
	}
#endif

	line[0] = particle->pos[0] - cg.refdef_current->vieworg[0];
	line[1] = particle->pos[1] - cg.refdef_current->vieworg[1];

	/* Both TC originals discard the distance result; there is no size inflation. */
	(void)DistanceSquared(particle->pos, cg.refdef_current->vieworg);
	dist = 1.f;

	len *= dist;

	VectorCopy(particle->deltaNormalized, forward);
#if defined(__linux__) && !defined(Q3_VM)
	{
		long double negativeLength = -(long double)len;
		finish[0] = (float)((long double)forward[0] * negativeLength + start[0]);
		finish[1] = (float)((long double)forward[1] * negativeLength + start[1]);
		finish[2] = (float)(negativeLength * (long double)forward[2] + start[2]);
	}
	{
		long double firstProjection, secondProjection, negativeFirst;
		long double initialRight[3], negative[3], positive[3], width;
		volatile float projectionCopy[2];
		volatile float *storedRight = right;
		int axis;

		firstProjection = (long double)forward[1] * cg.refdef_current->viewaxis[1][1] +
		                  (long double)forward[0] * cg.refdef_current->viewaxis[1][0];
		firstProjection += (long double)forward[2] * cg.refdef_current->viewaxis[1][2];
		projectionCopy[0] = (float)firstProjection;
		secondProjection = (long double)forward[1] * cg.refdef_current->viewaxis[2][1] +
		                   (long double)forward[0] * cg.refdef_current->viewaxis[2][0];
		negativeFirst = -firstProjection;
		secondProjection += (long double)forward[2] * cg.refdef_current->viewaxis[2][2];
		initialRight[0] = (long double)cg.refdef_current->viewaxis[1][0] * secondProjection;
		projectionCopy[1] = (float)secondProjection;
		storedRight[0] = (float)initialRight[0];
		initialRight[1] = secondProjection * cg.refdef_current->viewaxis[1][1];
		storedRight[1] = (float)initialRight[1];
		initialRight[2] = (long double)projectionCopy[1] * cg.refdef_current->viewaxis[1][2];
		storedRight[2] = (float)initialRight[2];
		storedRight[0] = (float)(initialRight[0] +
		                        (long double)cg.refdef_current->viewaxis[2][0] * negativeFirst);
		storedRight[1] = (float)(initialRight[1] +
		                        (long double)cg.refdef_current->viewaxis[2][1] * negativeFirst);
		storedRight[2] = (float)(initialRight[2] +
		                        negativeFirst * cg.refdef_current->viewaxis[2][2]);
		VectorNormalize(right);

		width = 1.0L * (long double)particle->weight;
		negative[0] = (long double)right[0] * -width;
		negative[1] = (long double)right[1] * -width;
		negative[2] = -width * (long double)right[2];
		positive[0] = (long double)right[0] * width;
		positive[1] = (long double)right[1] * width;
		positive[2] = width * (long double)right[2];
		for (axis = 0; axis < 3; ++axis)
		{
			verts[0].xyz[axis] = (float)((long double)finish[axis] + negative[axis]);
			memset(verts[axis].modulate, 255, sizeof(verts[axis].modulate));
		}
		for (axis = 0; axis < 3; ++axis)
		{
			verts[1].xyz[axis] = (float)(negative[axis] + (long double)start[axis]);
			verts[2].xyz[axis] = (float)(positive[axis] + (long double)start[axis]);
		}
		verts[0].st[0] = 0; verts[0].st[1] = 0;
		verts[1].st[0] = 0; verts[1].st[1] = 1;
		verts[2].st[0] = 1; verts[2].st[1] = 1;
	}
#else
	VectorMA(start, -(len /** sinTumbling*/), forward, finish);

	line[0] = DotProduct(forward, cg.refdef_current->viewaxis[1]);
	line[1] = DotProduct(forward, cg.refdef_current->viewaxis[2]);

	VectorScale(cg.refdef_current->viewaxis[1], line[1], right);
	VectorMA(right, -line[0], cg.refdef_current->viewaxis[2], right);
	VectorNormalize(right);

	particleWidth = dist * (/*cosTumbling **/ particle->weight);

	VectorMA(finish, -particleWidth, right, verts[0].xyz);
	verts[0].st[0]       = 0;
	verts[0].st[1]       = 0;
	verts[0].modulate[0] = 255;
	verts[0].modulate[1] = 255;
	verts[0].modulate[2] = 255;
	verts[0].modulate[3] = 255;

	VectorMA(start, -particleWidth, right, verts[1].xyz);
	verts[1].st[0]       = 0;
	verts[1].st[1]       = 1;
	verts[1].modulate[0] = 255;
	verts[1].modulate[1] = 255;
	verts[1].modulate[2] = 255;
	verts[1].modulate[3] = 255;

	VectorMA(start, particleWidth, right, verts[2].xyz);
	verts[2].st[0]       = 1;
	verts[2].st[1]       = 1;
	verts[2].modulate[0] = 255;
	verts[2].modulate[1] = 255;
	verts[2].modulate[2] = 255;
	verts[2].modulate[3] = 255;
#endif

	CG_AddPolyToPool(*particle->effectshader, verts);

//	rendertime += trap_Milliseconds() - msec;
}

/*
**	Set up gust parameters.
*/

#endif

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC weather lifecycle: original x87 stores and native field addresses. */
enum
{
	GustField0       = offsetof(cg_atmosphericEffect_t, lastRainTime),
	GustField1       = offsetof(cg_atmosphericEffect_t, numDrops),
	GustField2       = offsetof(cg_atmosphericEffect_t, gustStartTime),
	GustField3       = offsetof(cg_atmosphericEffect_t, gustEndTime),
	GustField4       = offsetof(cg_atmosphericEffect_t, baseStartTime),
	GustField5       = offsetof(cg_atmosphericEffect_t, baseEndTime),
	GustField6       = offsetof(cg_atmosphericEffect_t, gustMinTime),
	GustField7       = offsetof(cg_atmosphericEffect_t, gustMaxTime),
	GustField8       = offsetof(cg_atmosphericEffect_t, changeMinTime),
	GustField9       = offsetof(cg_atmosphericEffect_t, changeMaxTime),
	GustField10      = offsetof(cg_atmosphericEffect_t, baseMinTime),
	GustField11      = offsetof(cg_atmosphericEffect_t, baseMaxTime),
	GustField12      = offsetof(cg_atmosphericEffect_t, baseWeight),
	GustField13      = offsetof(cg_atmosphericEffect_t, gustWeight),
	GustField14      = offsetof(cg_atmosphericEffect_t, baseDrops),
	GustField15      = offsetof(cg_atmosphericEffect_t, gustDrops),
	GustField16      = offsetof(cg_atmosphericEffect_t, baseHeightOffset),
	GustField17      = offsetof(cg_atmosphericEffect_t, numEffectShaders),
	GustField18      = offsetof(cg_atmosphericEffect_t, baseVec),
	GustField19      = offsetof(cg_atmosphericEffect_t, baseVec) + 4,
	GustField20      = offsetof(cg_atmosphericEffect_t, baseVec) + 8,
	GustField21      = offsetof(cg_atmosphericEffect_t, gustVec),
	GustField22      = offsetof(cg_atmosphericEffect_t, gustVec) + 4,
	GustField23      = offsetof(cg_atmosphericEffect_t, gustVec) + 8,
	GustField24      = offsetof(cg_atmosphericEffect_t, viewDir),
	GustField25      = offsetof(cg_atmosphericEffect_t, viewDir) + 4,
	GustField26      = offsetof(cg_atmosphericEffect_t, viewDir) + 8,
	GustField27      = offsetof(cg_atmosphericEffect_t, ParticleCheckVisible),
	GustField28      = offsetof(cg_atmosphericEffect_t, ParticleGenerate),
	GustField29      = offsetof(cg_atmosphericEffect_t, ParticleRender),
	GustField30      = offsetof(cg_atmosphericEffect_t, dropsActive),
	GustField31      = offsetof(cg_atmosphericEffect_t, oldDropsActive),
	GustField32      = offsetof(cg_atmosphericEffect_t, dropsRendered),
	GustField33      = offsetof(cg_atmosphericEffect_t, dropsCreated),
	GustField34      = offsetof(cg_atmosphericEffect_t, dropsSkipped),
	GustCvarValue    = offsetof(vmCvar_t, value),
	GustCvarInteger  = offsetof(vmCvar_t, integer),
	GustViewX        = offsetof(refdef_t, viewaxis),
	GustViewY        = offsetof(refdef_t, viewaxis) + sizeof(float),
	GustNextDrop     = offsetof(cg_atmosphericParticle_t, nextDropTime),
	GustParticleSize = sizeof(cg_atmosphericParticle_t)
};
static const float  gustZero = 0.0f;
/* Keep native 32-bit time wrapping and the field reads around rand(). */
static void CG_EffectGust(void)
{
	volatile cg_atmosphericEffect_t *effect = &cg_atmFx;
	int randomValue, baseMin, baseMax, changeMin, changeMax;
	int gustMin, gustMax, range, offset, baseEnd, gustStart, gustEnd, time;

	randomValue = rand();
	baseMax = effect->baseMaxTime;
	baseMin = effect->baseMinTime;
	range = TCE_SubInt32(baseMax, baseMin);
	changeMax = effect->changeMaxTime;
	/* The original requires a nonzero base interval; this draw is unconditional. */
	offset = randomValue % range;
	time = ((volatile cg_t *)&cg)->time;
	baseEnd = TCE_AddInt32(TCE_AddInt32(offset, baseMin), time);
	changeMin = effect->changeMinTime;
	range = TCE_SubInt32(changeMax, changeMin);
	effect->baseEndTime = baseEnd;

	offset = 0;
	if (range)
	{
		randomValue = rand();
		changeMax = effect->changeMaxTime;
		offset = randomValue % range;
		baseEnd = effect->baseEndTime;
	}
	changeMin = effect->changeMinTime;
	gustMax = effect->gustMaxTime;
	gustStart = TCE_AddInt32(TCE_AddInt32(changeMin, offset), baseEnd);
	gustMin = effect->gustMinTime;
	range = TCE_SubInt32(gustMax, gustMin);
	effect->gustStartTime = gustStart;

	offset = 0;
	if (range)
	{
		randomValue = rand();
		gustStart = effect->gustStartTime;
		changeMax = effect->changeMaxTime;
		offset = randomValue % range;
	}
	gustMin = effect->gustMinTime;
	gustEnd = TCE_AddInt32(TCE_AddInt32(gustMin, offset), gustStart);
	changeMin = effect->changeMinTime;
	range = TCE_SubInt32(changeMax, changeMin);
	effect->gustEndTime = gustEnd;

	offset = 0;
	if (range)
	{
		randomValue = rand();
		offset = randomValue % range;
		gustEnd = effect->gustEndTime;
	}
	baseEnd = TCE_AddInt32(offset, gustEnd);
	changeMin = effect->changeMinTime;
	effect->baseStartTime = TCE_AddInt32(baseEnd, changeMin);
}

static qboolean CG_EffectGustCurrent(vec3_t curr, float *weight, int *num)
{
	vec3_t delta;
	long double fraction, count;
	int time = cg.time;
	int i, start, end, drops;
	qboolean fading = qfalse;

	if (time < cg_atmFx.baseEndTime)
	{
		VectorCopy(cg_atmFx.baseVec, curr);
		*weight = cg_atmFx.baseWeight;
		*num = cg_atmFx.baseDrops;
		return qfalse;
	}

	/* The original rounds all three vector differences before interpolation. */
	for (i = 0; i < 3; ++i)
	{
		delta[i] = (float)((long double)cg_atmFx.gustVec[i] - cg_atmFx.baseVec[i]);
	}
	start = cg_atmFx.baseEndTime;
	end = cg_atmFx.gustStartTime;
	if (time < end)
	{
		fraction = (long double)TCE_SubInt32(time, start) / TCE_SubInt32(end, start);
	}
	else if (time < cg_atmFx.gustEndTime)
	{
		VectorCopy(cg_atmFx.gustVec, curr);
		*weight = cg_atmFx.gustWeight;
		*num = cg_atmFx.gustDrops;
		return qfalse;
	}
	else
	{
		fading = qtrue;
		start = cg_atmFx.gustEndTime;
		end = cg_atmFx.baseStartTime;
		fraction = 1.0L - (long double)TCE_SubInt32(time, start) / TCE_SubInt32(end, start);
	}

	for (i = 0; i < 3; ++i)
	{
		curr[i] = (float)(fraction * delta[i] + cg_atmFx.baseVec[i]);
	}
	*weight = (float)(((long double)cg_atmFx.gustWeight - cg_atmFx.baseWeight) *
	                  fraction + cg_atmFx.baseWeight);
	drops = TCE_SubInt32(cg_atmFx.gustDrops, cg_atmFx.baseDrops);
	count = (long double)drops * fraction + cg_atmFx.baseDrops;
	*num = TCE_TruncateNativeInteger(count);
	return fading && cg.time >= cg_atmFx.baseStartTime ? qtrue : qfalse;
}

#else
static void CG_EffectGust()
{
	// Generate random values for the next gust

	int diff;

	cg_atmFx.baseEndTime   = cg.time                   + cg_atmFx.baseMinTime      + (rand() % (cg_atmFx.baseMaxTime - cg_atmFx.baseMinTime));
	diff                   = cg_atmFx.changeMaxTime    - cg_atmFx.changeMinTime;
	cg_atmFx.gustStartTime = cg_atmFx.baseEndTime      + cg_atmFx.changeMinTime    + (diff ? (rand() % diff) : 0);
	diff                   = cg_atmFx.gustMaxTime      - cg_atmFx.gustMinTime;
	cg_atmFx.gustEndTime   = cg_atmFx.gustStartTime    + cg_atmFx.gustMinTime      + (diff ? (rand() % diff) : 0);
	diff                   = cg_atmFx.changeMaxTime    - cg_atmFx.changeMinTime;
	cg_atmFx.baseStartTime = cg_atmFx.gustEndTime      + cg_atmFx.changeMinTime    + (diff ? (rand() % diff) : 0);
}

static qboolean CG_EffectGustCurrent(vec3_t curr, float *weight, int *num)
{
	// Calculate direction for new drops.

	vec3_t temp;
	float  frac;

	if (cg.time < cg_atmFx.baseEndTime)
	{
		VectorCopy(cg_atmFx.baseVec, curr);
		*weight = cg_atmFx.baseWeight;
		*num    = cg_atmFx.baseDrops;
	}
	else
	{
		VectorSubtract(cg_atmFx.gustVec, cg_atmFx.baseVec, temp);
		if (cg.time < cg_atmFx.gustStartTime)
		{
			frac = ((float)(cg.time - cg_atmFx.baseEndTime)) / ((float)(cg_atmFx.gustStartTime - cg_atmFx.baseEndTime));
			VectorMA(cg_atmFx.baseVec, frac, temp, curr);
			*weight = cg_atmFx.baseWeight + (cg_atmFx.gustWeight - cg_atmFx.baseWeight) * frac;
			*num    = cg_atmFx.baseDrops + ((float)(cg_atmFx.gustDrops - cg_atmFx.baseDrops)) * frac;
		}
		else if (cg.time < cg_atmFx.gustEndTime)
		{
			VectorCopy(cg_atmFx.gustVec, curr);
			*weight = cg_atmFx.gustWeight;
			*num    = cg_atmFx.gustDrops;
		}
		else
		{
			frac = 1.0 - ((float)(cg.time - cg_atmFx.gustEndTime)) / ((float)(cg_atmFx.baseStartTime - cg_atmFx.gustEndTime));
			VectorMA(cg_atmFx.baseVec, frac, temp, curr);
			*weight = cg_atmFx.baseWeight + (cg_atmFx.gustWeight - cg_atmFx.baseWeight) * frac;
			*num    = cg_atmFx.baseDrops + ((float)(cg_atmFx.gustDrops - cg_atmFx.baseDrops)) * frac;
			if (cg.time >= cg_atmFx.baseStartTime)
			{
				return(qtrue);
			}
		}
	}
	return(qfalse);
}

#endif

static void CG_EP_ParseFloats(char *floatstr, float *f1, float *f2)
{
	// Parse the float or floats

	char *middleptr;
	char buff[64];

	Q_strncpyz(buff, floatstr, sizeof(buff));
	for ( middleptr = buff; *middleptr && *middleptr != ' '; middleptr++ )
		;
	if (*middleptr)
	{
		*middleptr++ = 0;
		*f1          = atof(floatstr);
		*f2          = atof(middleptr);
	}
	else
	{
		*f1 = *f2 = atof(floatstr);
	}
}

#if defined(_MSC_VER) && defined(_M_IX86)
/* The original consumes the low 32 bits of signed 64-bit truncation. */
static void CG_EP_ParseInts(char *intstr, int *i1, int *i2)
{
	char *middleptr;
	char buff[64];

	Q_strncpyz(buff, intstr, sizeof(buff));
	for (middleptr = buff; *middleptr && *middleptr != ' '; middleptr++)
	{
	}
	if (*middleptr)
	{
		*middleptr++ = 0;
		*i1 = TCE_TruncateNativeInteger((long double)atof(intstr));
		*i2 = TCE_TruncateNativeInteger((long double)atof(middleptr));
	}
	else
	{
		int value = TCE_TruncateNativeInteger((long double)atof(intstr));
		*i2 = value;
		*i1 = value;
	}
}

#else

static void CG_EP_ParseInts(char *intstr, int *i1, int *i2)
{
	// Parse the int or ints

	char *middleptr;
	char buff[64];

	Q_strncpyz(buff, intstr, sizeof(buff));
	for ( middleptr = buff; *middleptr && *middleptr != ' '; middleptr++ )
		;
	if (*middleptr)
	{
		*middleptr++ = 0;
		*i1          = atof(intstr);
		*i2          = atof(middleptr);
	}
	else
	{
		*i1 = *i2 = atof(intstr);
	}
}

#endif

void CG_EffectParse(const char *effectstr)
{
	// Split the string into it's component parts.

	float       bmin, bmax, cmin, cmax, gmin, gmax, bdrop, gdrop /*, wsplash, lsplash*/;
	int         count, bheight;
	char        *startptr, *eqptr, *endptr;
	char        workbuff[128];
	atmFXType_t atmFXType = ATM_NONE;

	if (CG_AtmosphericKludge())
	{
		return;
	}

	// Set up some default values
	cg_atmFx.baseVec[0] = cg_atmFx.baseVec[1] = 0;
	cg_atmFx.gustVec[0] = cg_atmFx.gustVec[1] = 100;
	bmin                = 5;
	bmax                = 10;
	cmin                = 1;
	cmax                = 1;
	gmin                = 0;
	gmax                = 2;
	bdrop               = gdrop = 300;
	cg_atmFx.baseWeight = 0.7f;
	cg_atmFx.gustWeight = 1.5f;
	bheight             = 0;

	// Parse the parameter string
	Q_strncpyz(workbuff, effectstr, sizeof(workbuff));
	for ( startptr = workbuff; *startptr; )
	{
		for ( eqptr = startptr; *eqptr && *eqptr != '=' && *eqptr != ','; eqptr++ )
			;
		if (!*eqptr)
		{
			break;          // No more string
		}
		if (*eqptr == ',')
		{
			startptr = eqptr + 1;   // Bad argument, continue
			continue;
		}
		*eqptr++ = 0;
		for ( endptr = eqptr; *endptr && *endptr != ','; endptr++ )
			;
		if (*endptr)
		{
			*endptr++ = 0;
		}

		if (atmFXType == ATM_NONE)
		{
			if (Q_stricmp(startptr, "T"))
			{
				cg_atmFx.numDrops = 0;
				CG_Printf("Atmospheric effect must start with a type.\n");
				return;
			}
			if (!Q_stricmp(eqptr, "RAIN"))
			{
				atmFXType                     = ATM_RAIN;
				cg_atmFx.ParticleCheckVisible = &CG_RainParticleCheckVisible;
				cg_atmFx.ParticleGenerate     = &CG_RainParticleGenerate;
				cg_atmFx.ParticleRender       = &CG_RainParticleRender;

				/* TC sets literal -800, not the SDK 1.1*gravity speed. */
				cg_atmFx.baseVec[2] = cg_atmFx.gustVec[2] = -800.0f;
			}
			else if (!Q_stricmp(eqptr, "SNOW"))
			{
				atmFXType                     = ATM_SNOW;
				cg_atmFx.ParticleCheckVisible = &CG_SnowParticleCheckVisible;
				cg_atmFx.ParticleGenerate     = &CG_SnowParticleGenerate;
				cg_atmFx.ParticleRender       = &CG_SnowParticleRender;

				cg_atmFx.baseVec[2] = cg_atmFx.gustVec[2] = -ATMOSPHERIC_SNOW_SPEED;
			}
			else
			{
				cg_atmFx.numDrops = 0;
				CG_Printf("Only effect type 'rain' and 'snow' are supported.\n");
				return;
			}
		}
		else
		{
			if (!Q_stricmp(startptr, "B"))
			{
				CG_EP_ParseFloats(eqptr, &bmin, &bmax);
			}
			else if (!Q_stricmp(startptr, "C"))
			{
				CG_EP_ParseFloats(eqptr, &cmin, &cmax);
			}
			else if (!Q_stricmp(startptr, "G"))
			{
				CG_EP_ParseFloats(eqptr, &gmin, &gmax);
			}
			else if (!Q_stricmp(startptr, "BV"))
			{
				CG_EP_ParseFloats(eqptr, &cg_atmFx.baseVec[0], &cg_atmFx.baseVec[1]);
			}
			else if (!Q_stricmp(startptr, "GV"))
			{
				CG_EP_ParseFloats(eqptr, &cg_atmFx.gustVec[0], &cg_atmFx.gustVec[1]);
			}
			else if (!Q_stricmp(startptr, "W"))
			{
				CG_EP_ParseFloats(eqptr, &cg_atmFx.baseWeight, &cg_atmFx.gustWeight);
			}
			else if (!Q_stricmp(startptr, "D"))
			{
				CG_EP_ParseFloats(eqptr, &bdrop, &gdrop);
			}
			else if (!Q_stricmp(startptr, "H"))
			{
				CG_EP_ParseInts(eqptr, &bheight, &bheight);
			}
			else
			{
				CG_Printf("Unknown effect key '%s'.\n", startptr);
			}
		}
		startptr = endptr;
	}

	if (atmFXType == ATM_NONE || !cg.tceTraceMapLoaded)
	{
		// No effects

		cg_atmFx.numDrops = -1;
		return;
	}

	cg_atmFx.baseHeightOffset = bheight;
	if (cg_atmFx.baseHeightOffset < 0)
	{
		cg_atmFx.baseHeightOffset = 0;
	}
	cg_atmFx.baseMinTime   = 1000 * bmin;
	cg_atmFx.baseMaxTime   = 1000 * bmax;
	cg_atmFx.changeMinTime = 1000 * cmin;
	cg_atmFx.changeMaxTime = 1000 * cmax;
	cg_atmFx.gustMinTime   = 1000 * gmin;
	cg_atmFx.gustMaxTime   = 1000 * gmax;
	cg_atmFx.baseDrops     = bdrop;
	cg_atmFx.gustDrops     = gdrop;

	cg_atmFx.numDrops = (cg_atmFx.baseDrops > cg_atmFx.gustDrops) ? cg_atmFx.baseDrops : cg_atmFx.gustDrops;
	if (cg_atmFx.numDrops > MAX_ATMOSPHERIC_PARTICLES)
	{
		cg_atmFx.numDrops = MAX_ATMOSPHERIC_PARTICLES;
	}
	// Load graphics

	// Rain
	if (atmFXType == ATM_RAIN)
	{
		cg_atmFx.numEffectShaders = 1;
		cg_atmFx.effectshaders[0] = trap_R_RegisterShader("gfx/misc/raindrop");
		if (!(cg_atmFx.effectshaders[0]))
		{
			cg_atmFx.effectshaders[0] = -1;
			cg_atmFx.numEffectShaders = 0;
		}

		// Snow
	}
	else if (atmFXType == ATM_SNOW)
	{
		cg_atmFx.numEffectShaders = 1;
		cg_atmFx.effectshaders[0] = trap_R_RegisterShader("gfx/misc/snow");

		// This really should never happen
	}
	else
	{
		cg_atmFx.numEffectShaders = 0;
	}

	// Initialise atmospheric effect to prevent all particles falling at the start
	for ( count = 0; count < cg_atmFx.numDrops; count++ )
		cg_atmFx.particles[count].nextDropTime = ATMOSPHERIC_DROPDELAY + (rand() % ATMOSPHERIC_DROPDELAY);

	CG_EffectGust();
}

/*
** Main render loop
*/

#if defined(_MSC_VER) && defined(_M_IX86)
void CG_AddAtmosphericEffects(void)
{
	volatile cg_atmosphericEffect_t *effect = &cg_atmFx;
	volatile cg_t *client = &cg;
	volatile refdef_t *view;
	cg_atmosphericParticle_t *particle;
	vec3_t currentVector;
	float currentWeight;
	int currentCount, remaining, oldActive;

	remaining = effect->numDrops;
	if (remaining <= 0 || effect->numEffectShaders == 0)
	{
		return;
	}
	if (!(((volatile vmCvar_t *)&cg_atmosphericEffects)->value > 0.f) &&
	    ((volatile vmCvar_t *)&developer)->integer != 0)
	{
		return;
	}

	if (CG_EffectGustCurrent(currentVector, &currentWeight, &currentCount))
	{
		CG_EffectGust();
	}
	oldActive = effect->dropsActive;
	effect->dropsActive = 0;
	effect->oldDropsActive = oldActive;
	view = client->refdef_current;
	effect->dropsSkipped = 0;
	effect->dropsCreated = 0;
	effect->dropsRendered = 0;
	effect->viewDir[0] = view->viewaxis[0][0];
	effect->viewDir[1] = view->viewaxis[0][1];
	effect->viewDir[2] = 0.f;

	/* The native loop keeps the entry count across every particle callback. */
	particle = cg_atmFx.particles;
	do
	{
		if (!effect->ParticleCheckVisible(particle))
		{
			if (!effect->ParticleGenerate(particle, currentVector, currentWeight))
			{
				particle->nextDropTime = TCE_AddInt32(client->time, 1000);
				++particle;
				--remaining;
				continue;
			}
			effect->dropsCreated = TCE_AddInt32(effect->dropsCreated, 1);
		}
		effect->ParticleRender(particle);
		effect->dropsActive = TCE_AddInt32(effect->dropsActive, 1);
		++particle;
		--remaining;
	} while (remaining);
	effect->lastRainTime = client->time;
}

#else
void CG_AddAtmosphericEffects()
{
	// Add atmospheric effects (e.g. rain, snow etc.) to view

	int                      curr, max, currnum;
	cg_atmosphericParticle_t *particle;
	vec3_t                   currvec;
	float                    currweight;

	max = cg_atmFx.numDrops;
	if (max <= 0 || cg_atmFx.numEffectShaders == 0 || (!(cg_atmosphericEffects.value > 0) && developer.integer != 0))
	{
		return;
	}

#ifndef ATM_NEW
	CG_ClearPolyPool();
#endif // ATM_NEW

	if (CG_EffectGustCurrent(currvec, &currweight, &currnum))
	{
		CG_EffectGust();            // Recalculate gust parameters

	}
	// ydnar: allow parametric management of drop count for swelling/waning precip
	cg_atmFx.oldDropsActive = cg_atmFx.dropsActive;
	cg_atmFx.dropsActive    = 0;

	cg_atmFx.dropsRendered = cg_atmFx.dropsCreated = cg_atmFx.dropsSkipped = 0;

//	getgroundtime = getskytime = rendertime = checkvisibletime = generatetime = 0;
//	n_getgroundtime = n_getskytime = n_rendertime = n_checkvisibletime = n_generatetime = 0;

	VectorSet(cg_atmFx.viewDir, cg.refdef_current->viewaxis[0][0], cg.refdef_current->viewaxis[0][1], 0.f);

	for ( curr = 0; curr < max; curr++ )
	{
		particle = &cg_atmFx.particles[curr];
		//%	if( !CG_SnowParticleCheckVisible( particle ) )
		if (!cg_atmFx.ParticleCheckVisible(particle))
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
			if (!cg_atmFx.ParticleGenerate(particle, currvec, currweight))
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
		cg_atmFx.ParticleRender(particle);
		cg_atmFx.dropsActive++;
	}

//	CG_RenderPolyPool();

	cg_atmFx.lastRainTime = cg.time;

//	CG_Printf( "Active: %d Generated: %d Rendered: %d Skipped: %d\n", cg_atmFx.dropsActive, cg_atmFx.dropsCreated, cg_atmFx.dropsRendered, cg_atmFx.dropsSkipped );
//	CG_Printf( "gg: %i gs: %i rt: %i cv: %i ge: %i\n", getgroundtime, getskytime, rendertime, checkvisibletime, generatetime );
//	CG_Printf( "\\-> %i \\-> %i \\-> %i \\-> %i \\-> %i\n", n_getgroundtime, n_getskytime, n_rendertime, n_checkvisibletime, n_generatetime );
}
#endif
