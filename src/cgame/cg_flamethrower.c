
// cg_flamethrower.c - special code for the flamethrower effects
//
//	the flameChunks behave similarly to the trailJunc's, except they are rendered differently, and
//	also interact with the environment
//
// NOTE: some AI's are treated different, mostly for aesthetical reasons.

#include "cg_local.h"

// a flameChunk is a ball or section of fuel which goes from fuel->blue ignition->flame ball
// optimization is necessary, since lots of these will be spawned, but as they grow, they can be
// merged so that less overdraw occurs
typedef struct flameChunk_s
{
	struct flameChunk_s *nextGlobal, *prevGlobal;	// next junction in the global list it is in (free or used)
	struct flameChunk_s *nextFlameChunk;			// next junction in the trail
	struct flameChunk_s *nextHead, *prevHead;		// next head junc in the world

	qboolean		inuse;
	qboolean		dead;	// set when a chunk is effectively inactive, but waiting to be freed
	int				ownerCent;	// cent that spawned us

	int				timeStart, timeEnd;
	float			sizeMax;		// start small, increase if we slow down
	float			sizeRand;
	float			sizeRate;		// rate per ms, variable according to speed (larger if moving slower)
	vec3_t			baseOrg;
	int				baseOrgTime;
	vec3_t			velDir;
	float			velSpeed;		// flame chunks should start with a fast velocity, then slow down if there is nothing behind them pushing them along
	float			rollAngle;
	qboolean		ignitionOnly;
	int				blueLife;
	float			gravity;
	vec3_t			startVelDir;
	float			speedScale;

	// current variables
	vec3_t			org;
	float			size;
	float			lifeFrac;		// 0.0 (baby) -> 1.0 (aged)

	int				lastFriction, lastFrictionTake;
	vec3_t			parentFwd;
} flameChunk_t;

// DHM - Nerve :: lowered this from 2048.  Still allows 6-9 people flaming.
#define	MAX_FLAME_CHUNKS	1024
static flameChunk_t	flameChunks[MAX_FLAME_CHUNKS];
static flameChunk_t	*freeFlameChunks, *activeFlameChunks, *headFlameChunks;

static qboolean initFlameChunks = qfalse;

static int numFlameChunksInuse;

// this structure stores information relevant to each cent in the game, this way we keep
// the flamethrower data seperate to the rest of the code, which helps if we decide against
// using this weapon in the game
typedef struct centFlameInfo_s
{
	int		lastClientFrame;		// client frame that we last fired the flamethrower
	vec3_t	lastAngles;				// angles at last firing
	vec3_t	lastOrigin;				// origin at last firing
	flameChunk_t
			*lastFlameChunk;		// flame chunk we last spawned
	int		lastSoundUpdate;

	qboolean	lastFiring;

	int		lastDmgUpdate;			// time we last told server about this ent's flame damage
	int		lastDmgCheck;			// only check once per 100ms
	int		lastDmgEnemy;			// entity that inflicted the damage
} centFlameInfo_t;

static centFlameInfo_t centFlameInfo[MAX_GENTITIES];

typedef struct
{
//	float fireVolume;	// not needed, since we add individual loop sources for the flame, so it gets spacialized
	float blowVolume;
	float streamVolume;
} flameSoundStatus_t;

static flameSoundStatus_t centFlameStatus[MAX_GENTITIES];

// procedure defs
flameChunk_t *CG_SpawnFlameChunk( flameChunk_t *headFlameChunk );
void CG_FlameCalcOrg( flameChunk_t *f, int time, vec3_t outOrg );
void CG_FlameGetMuzzlePoint( vec3_t org, vec3_t fwd, vec3_t right, vec3_t up, vec3_t outPos );

// these must be globals, since they cannot expand or contract, since that might result in them getting
//	stuck in geometry. therefore when a chunk hits a surface, we should deflect it away from the surface
//	slightly, rather than running along it, so that as the chink grows, the sprites don't sink into the
//	wall too much.
static vec3_t	flameChunkMins = {0, 0, 0};
static vec3_t	flameChunkMaxs = {0, 0, 0};

// these define how the flame looks
#define	FLAME_START_SIZE		1.0
#define	FLAME_START_MAX_SIZE	140.0	// when the flame is spawned, it should endevour to reach this size
#define	FLAME_START_MAX_SIZE_RAND	60.0
#define	FLAME_MAX_SIZE			200.0	// flame sprites cannot be larger than this
#define	FLAME_MIN_MAXSIZE		40.0	// don't ever let the sizeMax go less than this
#define	FLAME_START_SPEED		1200.0//1200.0	// speed of flame as it leaves the nozzle
#define	FLAME_MIN_SPEED			60.0//200.0
#define	FLAME_CHUNK_DIST		8.0		// space in between chunks when fired

#define	FLAME_BLUE_LENGTH		130.0
#define	FLAME_BLUE_MAX_ALPHA	1.0

#define	FLAME_FUEL_LENGTH		48.0
#define	FLAME_FUEL_MAX_ALPHA	0.35
#define	FLAME_FUEL_MIN_WIDTH	1.0

// these are calculated (don't change)
#define	FLAME_LENGTH			(FLAMETHROWER_RANGE + 50.0)	// NOTE: only modify the range, since this should always reflect that range

#define	FLAME_LIFETIME			(int)((FLAME_LENGTH/FLAME_START_SPEED)*1000)	// life duration in milliseconds
#define	FLAME_FRICTION_PER_SEC	(2.0*FLAME_START_SPEED)
#define	FLAME_BLUE_LIFE			(int)((FLAME_BLUE_LENGTH/FLAME_START_SPEED)*1000)
#define	FLAME_FUEL_LIFE			(int)((FLAME_FUEL_LENGTH/FLAME_START_SPEED)*1000)
#define	FLAME_FUEL_FADEIN_TIME	(0.2*FLAME_FUEL_LIFE)

#define	FLAME_BLUE_FADEIN_TIME(x)		(0.2*x)
#define	FLAME_BLUE_FADEOUT_TIME(x)		(0.05*x)
#define	GET_FLAME_BLUE_SIZE_SPEED(x)	(((float)x / FLAME_LIFETIME) / 1.0)	// x is the current sizeMax
#define	GET_FLAME_SIZE_SPEED(x)			(((float)x / FLAME_LIFETIME) / 0.3)	// x is the current sizeMax

//#define	FLAME_MIN_DRAWSIZE		20

// enable this for the fuel stream
//#define FLAME_ENABLE_FUEL_STREAM

// enable this for dynamic lighting around flames
//#define FLAMETHROW_LIGHTS

// disable this to stop rotating flames (this is variable so we can change it at run-time)
int rotatingFlames = qtrue;

/*
===============
CG_FlameLerpVec
===============
*/
void CG_FlameLerpVec( const vec3_t oldV, const vec3_t newV, float backLerp, vec3_t outV )
{
#if defined(_MSC_VER) && defined(_M_IX86)
	static const double one = 1.0;
	__asm {
		fld backLerp
		mov ecx, newV
		mov eax, outV
		fsubr one
		fld dword ptr [ecx]
		fmul st(0), st(1)
		fstp dword ptr [eax]
		fld dword ptr [ecx + 4]
		fmul st(0), st(1)
		fstp dword ptr [eax + 4]
		fld dword ptr [ecx + 8]
		fmul st(0), st(1)
		mov ecx, oldV
		fstp dword ptr [eax + 8]
		fstp st(0)
		fld backLerp
		fmul dword ptr [ecx]
		fadd dword ptr [eax]
		fstp dword ptr [eax]
		fld backLerp
		fmul dword ptr [ecx + 4]
		fadd dword ptr [eax + 4]
		fstp dword ptr [eax + 4]
		fld backLerp
		fmul dword ptr [ecx + 8]
		fadd dword ptr [eax + 8]
		fstp dword ptr [eax + 8]
	}
#else
	VectorScale( newV, (1.0 - backLerp), outV );
	VectorMA( outV, backLerp, oldV, outV );
#endif
}

/*
===============
CG_FlameAdjustSpeed
===============
*/
void CG_FlameAdjustSpeed( flameChunk_t *f, float change )
{
#if defined(_MSC_VER) && defined(_M_IX86)
	float *speed = &f->velSpeed;
	static const float zero = 0.f;
	static const double minimum = 60.0;
	__asm {
		mov ecx, speed
		fld dword ptr [ecx]
		fcomp zero
		fnstsw ax
		test ah, 40h
		jz flame_speed_add
		fld change
		fcomp zero
		fnstsw ax
		test ah, 40h
		jnz flame_speed_done
	flame_speed_add:
		fld change
		fadd dword ptr [ecx]
		fst dword ptr [ecx]
		fcomp minimum
		fnstsw ax
		test ah, 1
		jz flame_speed_done
		mov dword ptr [ecx], 42700000h
	flame_speed_done:
	}
#else
	if (!f->velSpeed && !change) {
		return;
	}

	f->velSpeed += change;
	if (f->velSpeed < FLAME_MIN_SPEED)
		f->velSpeed = FLAME_MIN_SPEED;
#endif
}

/*
===============
CG_FireFlameChunks

  The given entity is firing a flamethrower
===============
*/
void CG_FireFlameChunks( centity_t *cent, vec3_t origin, vec3_t angles, float speedScale, qboolean firing )
{
	centFlameInfo_t	*centInfo;
	flameChunk_t *f, *of;
	vec3_t	lastFwd, thisFwd, fwd;
	vec3_t	lastUp, thisUp, up;
	vec3_t	lastRight, thisRight, right;
	vec3_t	thisOrg, lastOrg, org;
	double	timeInc, backLerp, fracInc;
	int		t, numFrameChunks;
	double	ft;
	trace_t trace;
	vec3_t	parentFwd;
	//float frametime, dot;

	centInfo = &centFlameInfo[cent->currentState.number];

	// for any other character or in 3rd person view, use entity angles for friction
	if( cent->currentState.number != cg.snap->ps.clientNum || cg_thirdPerson.integer ) {
		AngleVectors( cent->currentState.angles, parentFwd, NULL, NULL );
	} else {
		AngleVectors( angles, parentFwd, NULL, NULL );
	}

	AngleVectors( angles, thisFwd, thisRight, thisUp );
	VectorCopy( origin, thisOrg );

	// if this entity was firing last frame, interpolate the angles as we spawn the chunks that
	// fired over the last frame
	if (	(centInfo->lastClientFrame == cent->currentState.frame) &&
			(centInfo->lastFlameChunk && centInfo->lastFiring == firing) ) {
		AngleVectors( centInfo->lastAngles, lastFwd, lastRight, lastUp );
		VectorCopy( centInfo->lastOrigin, lastOrg );
		centInfo->lastFiring = firing;

		of = centInfo->lastFlameChunk;
		timeInc = 1000.0 * (firing ? 1.0 : 0.5) * (FLAME_CHUNK_DIST / (FLAME_START_SPEED * speedScale));
		ft = ((double)of->timeStart + timeInc);
		t = (int)ft;
		fracInc = timeInc / (double)(cg.time - of->timeStart);
		backLerp = 1.0 - fracInc;

		numFrameChunks = 0;			// CHANGE: id

		while (t <= cg.time) {
			// spawn a new chunk
			CG_FlameLerpVec( lastOrg, thisOrg, backLerp, org );

			CG_Trace( &trace, org, flameChunkMins, flameChunkMaxs, org, cent->currentState.number, MASK_SHOT | MASK_WATER ); // JPW NERVE water fixes
			if (trace.startsolid)
				return;		// don't spawn inside a wall

			f = CG_SpawnFlameChunk( of );

			if (!f) {
				//CG_Printf( "Out of flame chunks\n" );
				// CHANGE: id
				// to make sure we do not keep trying to add more and more chunks
				centInfo->lastFlameChunk->timeStart = cg.time;
				// end CHANGE: id
				return;
			}

			CG_FlameLerpVec( lastFwd, thisFwd, backLerp, fwd );
			VectorNormalize( fwd );
			CG_FlameLerpVec( lastRight, thisRight, backLerp, right );
			VectorNormalize( right );
			CG_FlameLerpVec( lastUp, thisUp, backLerp, up );
			VectorNormalize( up );

			f->timeStart = t;
			f->timeEnd = t + FLAME_LIFETIME * (1.0/(0.5 + 0.5*speedScale));
			f->size = FLAME_START_SIZE * speedScale;
			f->sizeMax = speedScale * (FLAME_START_MAX_SIZE + f->sizeRand * (firing ? 1.0 : 0.0));
			f->sizeRand = 0;

			if (f->sizeMax > FLAME_MAX_SIZE)
				f->sizeMax = FLAME_MAX_SIZE;

			f->sizeRate = GET_FLAME_BLUE_SIZE_SPEED( f->sizeMax * speedScale * (1.0+(0.5*(float)!firing)) );
			VectorCopy( org, f->baseOrg );
			f->baseOrgTime = t;
			VectorCopy( fwd, f->velDir );
			VectorCopy( fwd, f->startVelDir );
			f->speedScale = speedScale;

			VectorNormalize( f->velDir );
			f->velSpeed = FLAME_START_SPEED * (0.5+0.5*speedScale) * (firing ? 1.0 : 4.5);
			f->ownerCent = cent->currentState.number;
			f->rollAngle = crandom()*179;
			f->ignitionOnly = !firing;

			if (!firing) {
				f->gravity = -150;
				f->blueLife = FLAME_BLUE_LIFE * 0.1;
			} else {
				f->gravity = 0;
				f->blueLife = FLAME_BLUE_LIFE;
			}
			f->lastFriction = cg.time;
			f->lastFrictionTake = cg.time;
			VectorCopy( parentFwd, f->parentFwd );

			ft += timeInc;
			// always spawn a chunk right on the current time
			if ((int)ft > cg.time && t < cg.time) {
				ft = (double)cg.time;
				backLerp = fracInc;	// so it'll get set to zero a few lines down
			}
			t = (int)ft;
			backLerp -= fracInc;
			centInfo->lastFlameChunk = of = f;
			// CHANGE: id
			// don't spawn too many chunks each frame
			if ( ++numFrameChunks > 50 ) {
				// to make sure we do not keep trying to add more and more chunks
				centInfo->lastFlameChunk->timeStart = cg.time;
				break;
			}
			// end CHANGE: id
		}
	} else {

		centInfo->lastFiring = firing;

		// just fire a single chunk to get us started
		f = CG_SpawnFlameChunk( NULL );

		if (!f) {
			//CG_Printf( "Out of flame chunks\n" );
			return;
		}

		VectorCopy( thisOrg, org );
		VectorCopy( thisFwd, fwd );
		VectorCopy( thisUp, up );
		VectorCopy( thisRight, right );

		f->timeStart = cg.time;
		f->timeEnd = cg.time + FLAME_LIFETIME * (1.0/(0.5 + 0.5*speedScale));
		f->size = FLAME_START_SIZE * speedScale;
		f->sizeMax = FLAME_START_MAX_SIZE * speedScale;
		if (f->sizeMax > FLAME_MAX_SIZE)
			f->sizeMax = FLAME_MAX_SIZE;

		f->sizeRand = 0;
		f->sizeRate = GET_FLAME_BLUE_SIZE_SPEED( f->sizeMax * speedScale );
		VectorCopy( org, f->baseOrg );
		f->baseOrgTime = cg.time;
		VectorCopy( fwd, f->velDir );
		VectorCopy( fwd, f->startVelDir );
		f->velSpeed = FLAME_START_SPEED * (0.5+0.5*speedScale);
		f->ownerCent = cent->currentState.number;
		f->rollAngle = crandom()*179;
		f->ignitionOnly = !firing;
		f->speedScale = speedScale;
		if (!firing) {
			f->gravity = -100;
			f->blueLife = (int)(0.3*(1.0/speedScale)*(float)FLAME_BLUE_LIFE);
		} else {
			f->gravity = 0;
			f->blueLife = FLAME_BLUE_LIFE;
		}
		f->lastFriction = cg.time;
		f->lastFrictionTake = cg.time;
		VectorCopy( parentFwd, f->parentFwd );

		centInfo->lastFlameChunk = f;
	}

	// push them along
	/*
	f = centInfo->lastFlameChunk;
	while (f) {

		if (f->lastFriction < cg.time - 50) {
			frametime = (float)(cg.time - f->lastFriction) / 1000.0;
			f->lastFriction = cg.time;
			dot = DotProduct(parentFwd, f->parentFwd);
			if (dot >= 0.99) {
				dot -= 0.99;
				dot *= (1.0/(1.0-0.99));
				CG_FlameAdjustSpeed( f, 0.5 * frametime * FLAME_FRICTION_PER_SEC * pow(dot,4) );
			}
		}

		f = f->nextFlameChunk;
	}
	*/

	VectorCopy( angles, centInfo->lastAngles );
	VectorCopy( origin, centInfo->lastOrigin );
	centInfo->lastClientFrame = cent->currentState.frame;
}

/*
===============
CG_ClearFlameChunks
===============
*/
void CG_ClearFlameChunks (void)
{
	int		i;

	memset( flameChunks, 0, sizeof(flameChunks) );
	memset( centFlameInfo, 0, sizeof(centFlameInfo) );

	freeFlameChunks = flameChunks;
	activeFlameChunks = NULL;
	headFlameChunks = NULL;

	for (i=0 ;i<MAX_FLAME_CHUNKS ; i++)
	{
		flameChunks[i].nextGlobal = &flameChunks[i+1];

		if (i>0)
			flameChunks[i].prevGlobal = &flameChunks[i-1];
		else
			flameChunks[i].prevGlobal = NULL;

		flameChunks[i].inuse = qfalse;
	}
	flameChunks[MAX_FLAME_CHUNKS-1].nextGlobal = NULL;

	initFlameChunks = qtrue;
	numFlameChunksInuse = 0;
}

/*
===============
CG_SpawnFlameChunk
===============
*/
flameChunk_t *CG_SpawnFlameChunk( flameChunk_t *headFlameChunk )
{
	flameChunk_t	*f;

	if (!freeFlameChunks)
		return NULL;

	if (headFlameChunks && headFlameChunks->dead)
		headFlameChunks = NULL;

	// select the first free trail, and remove it from the list
	f = freeFlameChunks;
	freeFlameChunks = f->nextGlobal;
	if (freeFlameChunks)
		freeFlameChunks->prevGlobal = NULL;

	f->nextGlobal = activeFlameChunks;
	if (activeFlameChunks)
		activeFlameChunks->prevGlobal = f;
	activeFlameChunks = f;
	f->prevGlobal = NULL;
	f->inuse = qtrue;
	f->dead = qfalse;

	// if this owner has a headJunc, add us to the start
	if (headFlameChunk) {
		// remove the headJunc from the list of heads
		if (headFlameChunk == headFlameChunks) {
			headFlameChunks = headFlameChunks->nextHead;
			if (headFlameChunks)
				headFlameChunks->prevHead = NULL;
		} else {
			if (headFlameChunk->nextHead)
				headFlameChunk->nextHead->prevHead = headFlameChunk->prevHead;
			if (headFlameChunk->prevHead)
				headFlameChunk->prevHead->nextHead = headFlameChunk->nextHead;
		}
		headFlameChunk->prevHead = NULL;
		headFlameChunk->nextHead = NULL;
	}
	// make us the headTrail
	if (headFlameChunks)
		headFlameChunks->prevHead = f;
	f->nextHead = headFlameChunks;
	f->prevHead = NULL;
	headFlameChunks = f;

	f->nextFlameChunk = headFlameChunk;	// if headJunc is NULL, then we'll just be the end of the list

	numFlameChunksInuse++;

	return f;
}

/*
===========
CG_FreeFlameChunk
===========
*/
void CG_FreeFlameChunk( flameChunk_t *f )
{
	// kill any juncs after us, so they aren't left hanging
	if (f->nextFlameChunk) {
		CG_FreeFlameChunk( f->nextFlameChunk );
		f->nextFlameChunk = NULL;
	}

	// make it non-active
	f->inuse = qfalse;
	f->dead = qfalse;
	if (f->nextGlobal)
		f->nextGlobal->prevGlobal = f->prevGlobal;
	if (f->prevGlobal)
		f->prevGlobal->nextGlobal = f->nextGlobal;
	if (f == activeFlameChunks)
		activeFlameChunks = f->nextGlobal;

	// if it's a head, remove it
	if (f == headFlameChunks)
		headFlameChunks = f->nextHead;
	if (f->nextHead)
		f->nextHead->prevHead = f->prevHead;
	if (f->prevHead)
		f->prevHead->nextHead = f->nextHead;
	f->nextHead = NULL;
	f->prevHead = NULL;

	// stick it in the free list
	f->prevGlobal = NULL;
	f->nextGlobal = freeFlameChunks;
	if (freeFlameChunks)
		freeFlameChunks->prevGlobal = f;
	freeFlameChunks = f;

	numFlameChunksInuse--;
}

/*
===============
CG_MergeFlameChunks

  Assumes f1 comes before f2
===============
*/
void CG_MergeFlameChunks( flameChunk_t *f1, flameChunk_t *f2 )
{
	if (f1->nextFlameChunk != f2) {
		CG_Error( "CG_MergeFlameChunks: f2 doesn't follow f1, cannot merge\n" );
	}

	f1->nextFlameChunk = f2->nextFlameChunk;
	f2->nextFlameChunk = NULL;

	VectorCopy( f2->velDir, f1->velDir );

	VectorCopy( f2->baseOrg, f1->baseOrg );
	f1->baseOrgTime = f2->baseOrgTime;

	f1->velSpeed = f2->velSpeed;
	f1->sizeMax = f2->sizeMax;
	f1->size = f2->size;
	f1->timeStart = f2->timeStart;
	f1->timeEnd = f2->timeEnd;

	CG_FreeFlameChunk( f2 );
}

/*
===============
CG_FlameCalcOrg
===============
*/
void CG_FlameCalcOrg( flameChunk_t *f, int time, vec3_t outOrg )
{
#if defined(_MSC_VER) && defined(_M_IX86)
	static const float seconds = 0.001f;
	int i;
	for (i = 0; i < 3; ++i) {
		int elapsed = (int)((unsigned)time - (unsigned)f->baseOrgTime);
		float *speed = &f->velSpeed, *direction = &f->velDir[i];
		float *base = &f->baseOrg[i], *resultPosition = &outOrg[i];
		/* Original re-reads the time/speed after each output store. */
		__asm {
			fild elapsed
			fmul seconds
			mov eax, speed
			fmul dword ptr [eax]
			mov eax, direction
			fmul dword ptr [eax]
			mov eax, base
			fadd dword ptr [eax]
			mov eax, resultPosition
			fstp dword ptr [eax]
		}
	}
#else
	VectorMA( f->baseOrg, f->velSpeed * ((float)(time - f->baseOrgTime) / 1000), f->velDir, outOrg );
#endif
}

/*
===============
CG_MoveFlameChunk
===============
*/
void CG_MoveFlameChunk( flameChunk_t *f )
{
#if defined(_MSC_VER) && defined(_M_IX86)
	/* TC 3003d860: retain x87 intermediates through the original comparisons. */
	vec3_t newOrigin, sOrg;
	trace_t trace;
	float flameDelta, flameDot;
	float *flameSpeed = &f->velSpeed, *flameSize = &f->size;
	float *flameMaximum = &f->sizeMax, *flameRate = &f->sizeRate;
	float *flameDirection = f->velDir, *flameNormal = trace.plane.normal;
	float *flameFraction = &trace.fraction;
	float *flameParent = f->parentFwd, *flameBase = f->baseOrg;
	const float flameOne = 1.0f, flameNegTwo = -2.0f, flameNear = 32.0f;
	const float flameGrowth = 0.00047058824566192925f;
	const double flameMillis = 0.001, flameFriction = -2400.0;
	const double flameGrowthScale = 3.3333333333333335;
	const double flameOneD = 1.0, flameHalf = 0.5, flameQuarter = 0.25, flameThreeQuarter = 0.75;
	int flameElapsed, flameGate, flameStep;
	unsigned short flameCW, flameTruncCW;
	__int64 flameInteger;
	const float *flameEye;
	__asm {
		mov ecx, flameSpeed
		fld dword ptr [ecx]
		fcomp flameOne
		fnstsw ax
		test ah, 41h
		setz al
		movzx eax, al
		mov flameGate, eax
	}
	if (flameGate && f->lastFrictionTake < (int)((unsigned int)cg.time - 50u)) {
		flameElapsed = (int)((unsigned int)cg.time - (unsigned int)f->lastFrictionTake);
		__asm {
			fild flameElapsed
			fmul flameMillis
			fmul flameFriction
			fstp flameDelta
		}
		CG_FlameAdjustSpeed(f, flameDelta);
		f->lastFrictionTake = cg.time;
	}
	__asm {
		mov ecx, flameSize
		mov edx, flameMaximum
		fld dword ptr [ecx]
		fcomp dword ptr [edx]
		fnstsw ax
		and ah, 1
		movzx eax, ah
		mov flameGate, eax
	}
	if (flameGate) {
		if ((int)((unsigned int)cg.time - (unsigned int)f->timeStart) < f->blueLife) {
			__asm { mov ecx, flameRate
				mov dword ptr [ecx], 03d86ed54h }
		} else {
			__asm {
				mov ecx, flameMaximum
				mov edx, flameRate
				fld dword ptr [ecx]
				fmul flameGrowth
				fmul flameGrowthScale
				fstp dword ptr [edx]
			}
		}
		flameElapsed = (int)((unsigned int)cg.time - (unsigned int)f->baseOrgTime);
		__asm {
			mov ecx, flameSize
			mov edx, flameRate
			fild flameElapsed
			fmul dword ptr [edx]
			fadd dword ptr [ecx]
			fst dword ptr [ecx]
			mov edx, flameMaximum
			fcomp dword ptr [edx]
			fnstsw ax
			test ah, 41h
			jnz flameSizeDone
			mov eax, dword ptr [edx]
			mov dword ptr [ecx], eax
		flameSizeDone:
		}
	}
	VectorCopy(f->baseOrg, sOrg);
	__asm {
		mov ecx, flameSpeed
		fld dword ptr [ecx]
		fcomp flameOne
		fnstsw ax
		test ah, 41h
		setz al
		movzx eax, al
		mov flameGate, eax
	}
	while (flameGate && f->baseOrgTime != cg.time) {
		CG_FlameCalcOrg(f, cg.time, newOrigin);
		CG_Trace(&trace, sOrg, flameChunkMins, flameChunkMaxs, newOrigin, f->ownerCent, 0x60000b9);
		if (trace.startsolid) {
			f->velSpeed = 0.0f;
			f->dead = 1;
			break;
		}
		if (trace.surfaceFlags & 0x10) break;
		VectorCopy(trace.endpos, f->baseOrg);
		flameElapsed = (int)((unsigned int)cg.time - (unsigned int)f->baseOrgTime);
		__asm {
			fild flameElapsed
			mov ecx, flameFraction
			fmul dword ptr [ecx]
			/* Original __ftol: truncate to signed64, then consume low32. */
			fwait
			fnstcw flameCW
			fwait
			mov ax, flameCW
			or ah, 0ch
			mov flameTruncCW, ax
			fldcw flameTruncCW
			fistp flameInteger
			fldcw flameCW
			mov eax, dword ptr flameInteger
			mov flameStep, eax
			fld dword ptr [ecx]
			fcomp flameOneD
			fnstsw ax
			and ah, 40h
			movzx eax, ah
			mov flameGate, eax
		}
		f->baseOrgTime = (int)((unsigned int)f->baseOrgTime + (unsigned int)flameStep);
		if (flameGate) {
			if (f->ownerCent == cg.snap->ps.clientNum || (cg.snap->ps.eFlags & 1)) break;
			flameEye = cg.snap->ps.origin;
			__asm {
				push flameEye
				lea eax, newOrigin
				push eax
				call Distance
				fcomp flameNear
				add esp, 8
				fnstsw ax
				and ah, 1
				movzx eax, ah
				mov flameGate, eax
			}
			if (!flameGate) break;
			__asm {
				mov ecx, flameDirection
				mov edx, flameNormal
				fld dword ptr [ecx]
				fchs
				fstp dword ptr [edx]
				fld dword ptr [ecx+4]
				fchs
				fstp dword ptr [edx+4]
				fld dword ptr [ecx+8]
				fchs
				fstp dword ptr [edx+8]
			}
		}
		__asm {
			mov ecx, flameDirection
			mov edx, flameNormal
			fld dword ptr [edx+8]
			fmul dword ptr [ecx+8]
			fld dword ptr [edx+4]
			fmul dword ptr [ecx+4]
			faddp st(1), st(0)
			fld dword ptr [edx]
			fmul dword ptr [ecx]
			faddp st(1), st(0)
			fst flameDot
			fmul flameNegTwo
			fld st(0)
			fmul dword ptr [edx]
			fadd dword ptr [ecx]
			fstp dword ptr [ecx]
			fld st(0)
			fmul dword ptr [edx+4]
			fadd dword ptr [ecx+4]
			fstp dword ptr [ecx+4]
			fmul dword ptr [edx+8]
			fadd dword ptr [ecx+8]
			fstp dword ptr [ecx+8]
			push ecx
			call VectorNormalize
			fstp st(0)
			add esp, 4
		}
		__asm {
			fld flameDot
			fadd flameOneD
			mov ecx, flameDirection
			mov edx, flameParent
			mov eax, dword ptr [ecx]
			mov dword ptr [edx], eax
			fmul flameHalf
			mov eax, dword ptr [ecx+4]
			mov dword ptr [edx+4], eax
			mov ecx, flameBase
			mov eax, dword ptr [ecx]
			mov dword ptr sOrg, eax
			fmul flameThreeQuarter
			mov ecx, flameDirection
			mov eax, dword ptr [ecx+8]
			mov dword ptr [edx+8], eax
			mov ecx, flameBase
			mov eax, dword ptr [ecx+4]
			mov dword ptr sOrg[4], eax
			fadd flameQuarter
			mov edx, flameSpeed
			fmul dword ptr [edx]
			mov eax, dword ptr [ecx+8]
			mov dword ptr sOrg[8], eax
			mov ecx, edx
			fmul flameHalf
			fst dword ptr [ecx]
			fcomp flameOne
			fnstsw ax
			test ah, 41h
			setz al
			movzx eax, al
			mov flameGate, eax
		}
	}
	CG_FlameCalcOrg(f, cg.time, f->org);
	f->baseOrgTime = cg.time;
#else

	vec3_t	newOrigin, sOrg;
	trace_t	trace;
	int		jiggleCount;
	float	dot;
  // TTimo: unused
	//static vec3_t	umins = {-1,-1,-1};
	//static vec3_t	umaxs = { 1, 1, 1};

	// subtract friction from speed
	if (f->velSpeed > 1 && f->lastFrictionTake < cg.time - 50) {
		CG_FlameAdjustSpeed( f, -((float)(cg.time - f->lastFrictionTake)/1000.0) * FLAME_FRICTION_PER_SEC );
		f->lastFrictionTake = cg.time;
	}

	// adjust size
	if (f->size < f->sizeMax) {
		if ((cg.time - f->timeStart) < f->blueLife)
			f->sizeRate = GET_FLAME_BLUE_SIZE_SPEED(FLAME_START_MAX_SIZE);	// use a constant so the blue flame doesn't distort
		else
			f->sizeRate = GET_FLAME_SIZE_SPEED(f->sizeMax);

		f->size += f->sizeRate * (float)(cg.time - f->baseOrgTime);
		if (f->size > f->sizeMax) {
			f->size = f->sizeMax;
		}
	}

	jiggleCount = 0;
	VectorCopy( f->baseOrg, sOrg );
	while ( f->velSpeed > 1 && f->baseOrgTime != cg.time ) {
		CG_FlameCalcOrg( f, cg.time, newOrigin );

		// trace a line from previous position to new position
		CG_Trace( &trace, sOrg, flameChunkMins, flameChunkMaxs, newOrigin, f->ownerCent, MASK_SHOT | MASK_WATER ); // JPW NERVE water fixes

		if (trace.startsolid) {
			f->velSpeed = 0;
			f->dead = 1; // JPW NERVE water fixes
			break;
		}

		if ( trace.surfaceFlags & SURF_NOIMPACT ) {
			break;
		}

		// moved some distance
		VectorCopy( trace.endpos, f->baseOrg );
		f->baseOrgTime += (int)((float)(cg.time - f->baseOrgTime) * trace.fraction);

		if (trace.fraction == 1.0) {
			// check for hitting client
			if ((f->ownerCent != cg.snap->ps.clientNum) && !(cg.snap->ps.eFlags & EF_DEAD) && VectorDistance( newOrigin, cg.snap->ps.origin ) < 32) {
				VectorNegate( f->velDir, trace.plane.normal );
			} else {
				break;
			}
		}

		// reflect off surface
		dot = DotProduct( f->velDir, trace.plane.normal );
		VectorMA( f->velDir, -2*dot, trace.plane.normal, f->velDir );
		VectorNormalize( f->velDir );
		// subtract some speed
		f->velSpeed *= 0.5 * (0.25 + 0.75*((dot+1.0)*0.5));
		VectorCopy( f->velDir, f->parentFwd );

		VectorCopy( f->baseOrg, sOrg );
	}

	CG_FlameCalcOrg( f, cg.time, f->org );
	f->baseOrgTime = cg.time;	// incase we skipped the movement
#endif
}

/*
===============
CG_AddFlameSpriteToScene
===============
*/
static vec3_t	vright, vup;
static vec3_t	rright, rup;

#ifdef _DEBUG	// just in case we forget about it, but it should be disabled at all times (only enabled to generate updated shaders)
#ifdef ALLOW_GEN_SHADERS	// secondary security measure

//#define	GEN_FLAME_SHADER

#endif	// ALLOW_GEN_SHADERS
#endif	// _DEBUG

#define	FLAME_BLEND_SRC		"GL_ONE"
#define	FLAME_BLEND_DST		"GL_ONE_MINUS_SRC_COLOR"

#define	NUM_FLAME_SPRITES		45
#define	FLAME_SPRITE_DIR		"twiltb2"

#define	NUM_NOZZLE_SPRITES	8

static qhandle_t flameShaders[NUM_FLAME_SPRITES];
static qhandle_t nozzleShaders[NUM_NOZZLE_SPRITES];
static qboolean initFlameShaders = qtrue;

#define	MAX_CLIPPED_FLAMES	8		// dont draw more than this many per frame
static int numClippedFlames;

void CG_FlameDamage( int owner, vec3_t org, float radius )
{
		return;
}


#if defined(_MSC_VER) && defined(_M_IX86)
/* Original30088628 ABI, consumed only by the sprite producer below. */
__declspec(naked) static int CG_FlameSpriteTruncateST0(void) {
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
#endif

void CG_AddFlameSpriteToScene( flameChunk_t *f, float lifeFrac, float alpha )
{
#if defined(_MSC_VER) && defined(_M_IX86)
	/* Original 3003db40. Keep the z corner accumulator live across all four vertices. */
	vec3_t flameP2, flameProjection, flameVector, flameAngles;
	static vec3_t lastPos;
	float flameRadius, flameVertical, flamePointX, flamePointY, flamePointZ, flameLastScale;
	float *flameOrg = f->org, *flameSize = &f->size, *flameRoll = &f->rollAngle;
	const float *flameView, *flameAxis;
	float *flameXYZ, *flameST;
	const float flameZero = 0.0f, flameSix = 6.0f, flameFrames = 45.0f;
	const float flameProjectionScale = 1024.0f, flameMinusTwo = -2.0f;
	const double flameHalf = 0.5, flameAspect = 0.6752194463200539, flameColorScale = 255.0;
	double (__cdecl *flameFloorCall)(double) = floor;
	int flameGate, flameFrame, flameColor;
	unsigned char flameAlpha;
	polyBuffer_t *pPolyBuffer;
	__asm {
		fld alpha
		fcomp flameZero
		fnstsw ax
		and ah, 1
		movzx eax, ah
		mov flameGate, eax
	}
	if (flameGate) return;
	__asm {
		mov ecx, flameSize
		fld dword ptr [ecx]
		fmul flameHalf
		fst flameRadius
		fcomp flameSix
		fnstsw ax
		test ah, 1
		jz flameRadiusReady
		mov flameRadius, 040c00000h
	flameRadiusReady:
	}
	if (CG_CullPointAndRadius(f->org, flameRadius)) return;
	__asm {
		fld flameRadius
		fmul flameAspect
		fstp flameVertical
		fld alpha
		fmul flameColorScale
		call CG_FlameSpriteTruncateST0
		mov flameColor, eax
		fld lifeFrac
		fmul flameFrames
		sub esp, 8
		fstp qword ptr [esp]
		call dword ptr flameFloorCall
		add esp, 8
		call CG_FlameSpriteTruncateST0
		mov flameFrame, eax
	}
	flameAlpha = (unsigned char)flameColor;
	if (flameFrame < 0) flameFrame = 0;
	else if (flameFrame > 44) flameFrame = 44;
	pPolyBuffer = CG_PB_FindFreePolyBuffer(cg_fxflags & 1 ? getTestShader() : flameShaders[flameFrame], 4, 6);
	pPolyBuffer->color[pPolyBuffer->numVerts][0] = flameAlpha;
	pPolyBuffer->color[pPolyBuffer->numVerts][1] = flameAlpha;
	pPolyBuffer->color[pPolyBuffer->numVerts][2] = flameAlpha;
	pPolyBuffer->color[pPolyBuffer->numVerts][3] = flameAlpha;
	memcpy(pPolyBuffer->color[pPolyBuffer->numVerts+1], pPolyBuffer->color[pPolyBuffer->numVerts], 4);
	memcpy(pPolyBuffer->color[pPolyBuffer->numVerts+2], pPolyBuffer->color[pPolyBuffer->numVerts], 4);
	memcpy(pPolyBuffer->color[pPolyBuffer->numVerts+3], pPolyBuffer->color[pPolyBuffer->numVerts], 4);
	flameView = cg.refdef_current->vieworg;
	flameAxis = cg.refdef_current->viewaxis[0];
	__asm {
		mov ecx, flameAxis
		mov edx, flameView
		fld dword ptr [ecx]
		fmul flameProjectionScale
		fadd dword ptr [edx]
		fstp dword ptr flameP2
		fld dword ptr [ecx+4]
		fmul flameProjectionScale
		fadd dword ptr [edx+4]
		fstp dword ptr flameP2[4]
		fld dword ptr [ecx+8]
		fmul flameProjectionScale
		fadd dword ptr [edx+8]
		fstp dword ptr flameP2[8]
	}
	ProjectPointOntoVector(f->org, cg.refdef_current->vieworg, flameP2, flameProjection);
	flameView = cg.refdef_current->vieworg;
	__asm {
		mov edx, flameView
		fld dword ptr flameProjection
		fsub dword ptr [edx]
		fstp dword ptr flameVector
		fld dword ptr flameProjection[4]
		fsub dword ptr [edx+4]
		fstp dword ptr flameVector[4]
		fld dword ptr flameProjection[8]
		fsub dword ptr [edx+8]
		fstp dword ptr flameVector[8]
		lea eax, flameVector
		push eax
		call VectorNormalize
		fcomp flameZero
		add esp, 4
		fnstsw ax
		and ah, 40h
		movzx eax, ah
		mov flameGate, eax
	}
	if (flameGate) return;
	flameAxis = cg.refdef_current->viewaxis[0];
	__asm {
		mov ecx, flameAxis
		fld dword ptr flameVector[8]
		fmul dword ptr [ecx+8]
		fld dword ptr flameVector[4]
		fmul dword ptr [ecx+4]
		faddp st(1), st(0)
		fld dword ptr flameVector
		fmul dword ptr [ecx]
		faddp st(1), st(0)
		fcomp flameZero
		fnstsw ax
		and ah, 1
		movzx eax, ah
		mov flameGate, eax
	}
	if (flameGate) return;
	if (rotatingFlames && !(cg_fxflags & 1)) {
		vectoangles(cg.refdef_current->viewaxis[0], flameAngles);
		__asm {
			mov ecx, flameRoll
			fld dword ptr flameAngles[8]
			fadd dword ptr [ecx]
			fstp dword ptr flameAngles[8]
		}
		AngleVectors(flameAngles, NULL, rright, rup);
	} else {
		VectorCopy(vright, rright);
		VectorCopy(vup, rup);
	}
	flameXYZ = &pPolyBuffer->xyz[pPolyBuffer->numVerts][0];
	flameST = &pPolyBuffer->st[pPolyBuffer->numVerts][0];
	__asm {
		mov ecx, flameOrg
		mov edx, flameXYZ
		mov eax, flameST
		fld flameVertical
		fchs
		fld st(0)
		fmul dword ptr rup
		fadd dword ptr [ecx]
		fstp flamePointX
		fld st(0)
		fmul dword ptr rup[4]
		fadd dword ptr [ecx+4]
		fstp flamePointY
		fmul dword ptr rup[8]
		fadd dword ptr [ecx+8]
		fld flameRadius
		fchs
		fld st(0)
		fmul dword ptr rright
		fadd flamePointX
		fstp flamePointX
		fld st(0)
		fmul dword ptr rright[4]
		fadd flamePointY
		fstp flamePointY
		fmul dword ptr rright[8]
		faddp st(1), st(0)
		fld flamePointX
		fstp dword ptr [edx]
		fld flamePointY
		fstp dword ptr [edx+4]
		fst dword ptr [edx+8]
		fld flameVertical
		fadd st(0), st(0)
		mov dword ptr [eax], 0
		fld st(0)
		mov dword ptr [eax+4], 0
		fmul dword ptr rup
		fadd flamePointX
		fstp flamePointX
		fld st(0)
		fmul dword ptr rup[4]
		fadd flamePointY
		fstp flamePointY
		fmul dword ptr rup[8]
		faddp st(1), st(0)
		fld flamePointX
		fstp dword ptr [edx+16]
		fld flamePointY
		fstp dword ptr [edx+20]
		fst dword ptr [edx+24]
		fld flameRadius
		fadd st(0), st(0)
		mov dword ptr [eax+8], 0
		fld st(0)
		mov dword ptr [eax+12], 03f800000h
		fmul dword ptr rright
		fadd flamePointX
		fstp flamePointX
		fld st(0)
		fmul dword ptr rright[4]
		fadd flamePointY
		fstp flamePointY
		fmul dword ptr rright[8]
		faddp st(1), st(0)
		fld flamePointX
		fstp dword ptr [edx+32]
		fld flamePointY
		fstp dword ptr [edx+36]
		fst dword ptr [edx+40]
		fld flameVertical
		fmul flameMinusTwo
		mov dword ptr [eax+16], 03f800000h
		mov dword ptr [eax+20], 03f800000h
		fst flameLastScale
		fmul dword ptr rup
		fadd flamePointX
		fld flameLastScale
		fmul dword ptr rup[4]
		fadd flamePointY
		fstp flamePointY
		fld flameLastScale
		fmul dword ptr rup[8]
		fadd st(0), st(2)
		fstp flamePointZ
		fstp dword ptr [edx+48]
		fstp st(0)
		fld flamePointY
		fstp dword ptr [edx+52]
		fld flamePointZ
		fstp dword ptr [edx+56]
		mov dword ptr [eax+24], 03f800000h
		mov dword ptr [eax+28], 0
	}
	pPolyBuffer->indicies[pPolyBuffer->numIndicies] = pPolyBuffer->numVerts;
	pPolyBuffer->indicies[pPolyBuffer->numIndicies+1] = pPolyBuffer->numVerts+1;
	pPolyBuffer->indicies[pPolyBuffer->numIndicies+2] = pPolyBuffer->numVerts+2;
	pPolyBuffer->indicies[pPolyBuffer->numIndicies+3] = pPolyBuffer->numVerts+2;
	pPolyBuffer->indicies[pPolyBuffer->numIndicies+4] = pPolyBuffer->numVerts+3;
	pPolyBuffer->indicies[pPolyBuffer->numIndicies+5] = pPolyBuffer->numVerts;
	pPolyBuffer->numIndicies += 6;
	pPolyBuffer->numVerts += 4;
	VectorCopy(f->org, lastPos);
#else

	vec3_t		point, p2, sProj;
	float		radius, sdist;
	int			frameNum;
	vec3_t		vec, rotate_ang;
	unsigned char alphaChar;
	vec2_t		rST;
	static vec3_t	lastPos;
	polyBuffer_t* pPolyBuffer;

	if (alpha < 0) {
		return;	// we dont want to see this
	}

	radius = (f->size/2.0);
	if (radius < 6) {
		radius = 6;
	}
	
	if(CG_CullPointAndRadius( f->org, radius )) {
		return;
	}

	rST[0] = radius * 1.0;
	rST[1] = radius * 1.0/1.481;
	alphaChar = (unsigned char)(255.0 * alpha);

	frameNum = (int)floor(lifeFrac * NUM_FLAME_SPRITES);
	if (frameNum < 0) {
		frameNum = 0;
	} else if (frameNum > NUM_FLAME_SPRITES - 1) {
		frameNum = NUM_FLAME_SPRITES - 1;
	}

	pPolyBuffer = CG_PB_FindFreePolyBuffer( cg_fxflags & 1 ? getTestShader() : flameShaders[frameNum], 4, 6 );

	pPolyBuffer->color[pPolyBuffer->numVerts + 0][0] = alphaChar;
	pPolyBuffer->color[pPolyBuffer->numVerts + 0][1] = alphaChar;
	pPolyBuffer->color[pPolyBuffer->numVerts + 0][2] = alphaChar;
	pPolyBuffer->color[pPolyBuffer->numVerts + 0][3] = alphaChar;

	memcpy( pPolyBuffer->color[pPolyBuffer->numVerts + 1], pPolyBuffer->color[pPolyBuffer->numVerts + 0], sizeof(pPolyBuffer->color[0]));
	memcpy( pPolyBuffer->color[pPolyBuffer->numVerts + 2], pPolyBuffer->color[pPolyBuffer->numVerts + 0], sizeof(pPolyBuffer->color[0]));
	memcpy( pPolyBuffer->color[pPolyBuffer->numVerts + 3], pPolyBuffer->color[pPolyBuffer->numVerts + 0], sizeof(pPolyBuffer->color[0]));

	// find the projected distance from the eye to the projection of the flame origin
	// onto the view direction vector
	VectorMA( cg.refdef_current->vieworg, 1024, cg.refdef_current->viewaxis[0], p2 );
	ProjectPointOntoVector( f->org, cg.refdef_current->vieworg, p2, sProj );

	// make sure its infront of us
	VectorSubtract( sProj, cg.refdef_current->vieworg, vec );
	sdist = VectorNormalize( vec );
	if (!sdist || DotProduct( vec, cg.refdef_current->viewaxis[0] ) < 0)
		return;


	if ( (rotatingFlames) && (!(cg_fxflags & 1)) ) { // JPW NERVE no rotate for alt flame shaders
		vectoangles( cg.refdef_current->viewaxis[0], rotate_ang );
		rotate_ang[ROLL] += f->rollAngle;
		AngleVectors ( rotate_ang, NULL, rright, rup);
	} else {
		VectorCopy( vright, rright );
		VectorCopy( vup, rup );
	}

	VectorMA (f->org, -rST[1], rup, point);	
	VectorMA (point, -rST[0], rright, point);	
	VectorCopy (point, pPolyBuffer->xyz[pPolyBuffer->numVerts + 0]);
	pPolyBuffer->st[pPolyBuffer->numVerts + 0][0] = 0;
	pPolyBuffer->st[pPolyBuffer->numVerts + 0][1] = 0;

	VectorMA (point, rST[1]*2, rup, point);	
	VectorCopy (point, pPolyBuffer->xyz[pPolyBuffer->numVerts + 1]);
	pPolyBuffer->st[pPolyBuffer->numVerts + 1][0] = 0;
	pPolyBuffer->st[pPolyBuffer->numVerts + 1][1] = 1;

	VectorMA (point, rST[0]*2, rright, point);	
	VectorCopy (point, pPolyBuffer->xyz[pPolyBuffer->numVerts + 2]);
	pPolyBuffer->st[pPolyBuffer->numVerts + 2][0] = 1;
	pPolyBuffer->st[pPolyBuffer->numVerts + 2][1] = 1;

	VectorMA (point, -rST[1]*2, rup, point);	
	VectorCopy (point, pPolyBuffer->xyz[pPolyBuffer->numVerts + 3]);
	pPolyBuffer->st[pPolyBuffer->numVerts + 3][0] = 1;
	pPolyBuffer->st[pPolyBuffer->numVerts + 3][1] = 0;

	pPolyBuffer->indicies[pPolyBuffer->numIndicies + 0] = pPolyBuffer->numVerts + 0;
	pPolyBuffer->indicies[pPolyBuffer->numIndicies + 1] = pPolyBuffer->numVerts + 1;
	pPolyBuffer->indicies[pPolyBuffer->numIndicies + 2] = pPolyBuffer->numVerts + 2;

	pPolyBuffer->indicies[pPolyBuffer->numIndicies + 3] = pPolyBuffer->numVerts + 2;
	pPolyBuffer->indicies[pPolyBuffer->numIndicies + 4] = pPolyBuffer->numVerts + 3;
	pPolyBuffer->indicies[pPolyBuffer->numIndicies + 5] = pPolyBuffer->numVerts + 0;

	pPolyBuffer->numIndicies += 6;
	pPolyBuffer->numVerts += 4;

	VectorCopy( f->org, lastPos );
#endif
}

static int	nextFlameLight = 0;
static int	lastFlameOwner = -1;


#define	FLAME_SOUND_RANGE	1024.0

/*
===============
CG_AddFlameToScene
===============
*/

#if defined(_MSC_VER) && defined(_M_IX86)
/* Whole TC3003e0a0. Original stack/x87 contract; all external objects are native. */
enum {
	FlameSceneF_nextFlameChunk = offsetof(flameChunk_t, nextFlameChunk),
	FlameSceneF_dead = offsetof(flameChunk_t, dead),
	FlameSceneF_ownerCent = offsetof(flameChunk_t, ownerCent),
	FlameSceneF_timeStart = offsetof(flameChunk_t, timeStart),
	FlameSceneF_sizeMax = offsetof(flameChunk_t, sizeMax),
	FlameSceneF_sizeRate = offsetof(flameChunk_t, sizeRate),
	FlameSceneF_velDir = offsetof(flameChunk_t, velDir),
	FlameSceneF_velSpeed = offsetof(flameChunk_t, velSpeed),
	FlameSceneF_ignitionOnly = offsetof(flameChunk_t, ignitionOnly),
	FlameSceneF_blueLife = offsetof(flameChunk_t, blueLife),
	FlameSceneF_startVelDir = offsetof(flameChunk_t, startVelDir),
	FlameSceneF_org = offsetof(flameChunk_t, org),
	FlameSceneF_size = offsetof(flameChunk_t, size),
	FlameSceneF_lifeFrac = offsetof(flameChunk_t, lifeFrac),
	FlameSceneInfoSize = sizeof(centFlameInfo_t),
	FlameSceneStatusSize = sizeof(flameSoundStatus_t),
	FlameSceneStreamVolume = offsetof(flameSoundStatus_t, streamVolume),
	FlameSceneTime = offsetof(cg_t, time),
	FlameSceneClientFrame = offsetof(cg_t, clientFrame),
	FlameSceneRefdef = offsetof(cg_t, refdef_current),
	FlameSceneSnapshot = offsetof(cg_t, snap),
	FlameSceneSnapshotClient = offsetof(snapshot_t, ps) + offsetof(playerState_t, clientNum),
	FlameSceneEntitySize = sizeof(centity_t),
	FlameSceneEntityFlags = offsetof(centity_t, currentState) + offsetof(entityState_t, eFlags),
	FlameSceneViewOrigin = offsetof(refdef_t, vieworg),
	FlameSceneInfoLast = offsetof(centFlameInfo_t, lastFlameChunk),
	FlameSceneStreamShader = offsetof(cgs_t, media) + offsetof(cgMedia_t, flamethrowerFireStream)
};
static const vec3_t flameSceneWhite = {1.0f,1.0f,1.0f};
static const unsigned int flameSceneC300922c8[] = { 0x00000000u, 0x3ff00000u };
static const unsigned int flameSceneC30092d38[] = { 0x00000000u, 0x40900000u };
static const unsigned int flameSceneC30092d30[] = { 0x00000000u, 0x3f500000u };
static const unsigned int flameSceneC30092320[] = { 0x00000000u, 0x407f4000u };
static const unsigned int flameSceneC30092b88[] = { 0x9999999au, 0x3fa99999u };
static const unsigned int flameSceneC300922b4[] = { 0x3f800000u };
static const unsigned int flameSceneC300922e0[] = { 0x00000000u, 0x3fe00000u };
static const unsigned int flameSceneC300922b8[] = { 0x3f000000u };
static const unsigned int flameSceneC300920e0[] = { 0x00000000u };
static const unsigned int flameSceneC30092538[] = { 0x00000000u, 0x40080000u };
static const unsigned int flameSceneC30092bd0[] = { 0x00000000u, 0x40000000u };
static const unsigned int flameSceneC30092d28[] = { 0xd89d89d8u, 0x3fe89d89u };
static const unsigned int flameSceneC30092a30[] = { 0x9999999au, 0x3fc99999u };
static const unsigned int flameSceneC300927e0[] = { 0x00000000u, 0x00000000u };
static const unsigned int flameSceneC30092490[] = { 0x3e800000u };
static const unsigned int flameSceneC30092408[] = { 0xcccccccdu, 0x3fecccccu };
static const unsigned int flameSceneC300925c0[] = { 0x9999999au, 0x3fb99999u };
static const unsigned int flameSceneC30092d20[] = { 0x66666666u, 0x3fd66666u };
static const unsigned int flameSceneC30092a70[] = { 0x00000000u, 0x40440000u };
static const unsigned int flameSceneC300922f8[] = { 0x00000000u, 0x40590000u };
static const unsigned int flameSceneC300925c8[] = { 0x7ae147aeu, 0x3fefae14u };
static const unsigned int flameSceneC30092ce0[] = { 0x00000000u, 0x40a09a00u };
static const unsigned int flameSceneC30092d18[] = { 0x66666666u, 0x3ff66666u };
static const unsigned int flameSceneC30092cb8[] = { 0x39f6b949u };
static const unsigned int flameSceneC30092cf0[] = { 0xaaaaaaabu, 0x400aaaaau };
static const unsigned int flameSceneC300924e0[] = { 0x42a00000u };
static const unsigned int flameSceneC30092a24[] = { 0x43fa0000u };
static const unsigned int flameSceneC30092d10[] = { 0x417d05f4u, 0x3f97d05fu };
static const unsigned int flameSceneC30092d08[] = { 0x47ae147bu, 0x3f947ae1u };
static const unsigned int flameSceneC30092ad0[] = { 0x47ae147bu, 0x3f747ae1u };
__declspec(naked) void CG_AddFlameToScene(flameChunk_t *fHead) {
	__asm {
		SUB ESP,060h
		MOV EAX,dword ptr [ESP + 064h]
		PUSH EBX
		PUSH ESI
		PUSH EDI
		MOV EAX,dword ptr [EAX + FlameSceneF_ownerCent]
		MOV EDI,dword ptr [ESP + 070h]
		XOR EBX,EBX
		MOV ESI,EDI
		IMUL EDX,EAX,FlameSceneInfoSize
		MOV dword ptr [ESP + 030h],EBX
		MOV dword ptr [ESP + 03ch],EBX
		MOV dword ptr [ESP + 02ch],EBX
		XOR ECX,ECX
		MOV EDX,dword ptr [EDX + centFlameInfo + FlameSceneInfoLast]
		CMP EDI,EDX
		SETZ CL
		MOV dword ptr [ESP + 040h],ECX
		IMUL EAX,EAX,FlameSceneEntitySize
		TEST byte ptr [EAX + cg_entities + FlameSceneEntityFlags],080h
		JZ flameScene_3003e102
		CMP EDX,ESI
		JNZ flameScene_3003e102
		MOV ECX,dword ptr [ESI + FlameSceneF_timeStart]
		MOV dword ptr [ESP + 01ch],ECX
		JMP flameScene_3003e10c
flameScene_3003e102:
		MOV EDX,dword ptr [cg + FlameSceneTime]
		MOV dword ptr [ESP + 01ch],EDX
flameScene_3003e10c:
		TEST ESI,ESI
		MOV dword ptr [ESP + 050h],00h
		MOV dword ptr [ESP + 04ch],00h
		MOV dword ptr [ESP + 048h],00h
		MOV dword ptr [ESP + 010h],00h
		MOV dword ptr [ESP + 018h],00h
		MOV dword ptr [ESP + 034h],03f800000h
		JZ flameScene_3003e777
		PUSH EBP
		JMP flameScene_3003e14b
flameScene_3003e147:
		MOV EBX,dword ptr [ESP + 048h]
flameScene_3003e14b:
		MOV EAX,dword ptr [ESI + FlameSceneF_nextFlameChunk]
		TEST EAX,EAX
		JZ flameScene_3003e169
		MOV ECX,dword ptr [EAX + FlameSceneF_dead]
		TEST ECX,ECX
		JZ flameScene_3003e169
		PUSH EAX
		CALL CG_FreeFlameChunk
		ADD ESP,04h
		MOV dword ptr [ESI + FlameSceneF_nextFlameChunk],00h
flameScene_3003e169:
		MOV EAX,dword ptr [ESP + 020h]
		MOV ECX,dword ptr [ESI + FlameSceneF_timeStart]
		SUB EAX,ECX
		MOV ECX,dword ptr [cg + FlameSceneRefdef]
		MOV EDI,dword ptr [ESI + FlameSceneF_nextFlameChunk]
		MOV dword ptr [ESP + 028h],EAX
		FILD dword ptr [ESP + 028h]
		LEA EBP,[ESI + FlameSceneF_org]
		ADD ECX, FlameSceneViewOrigin
		PUSH EBP
		PUSH ECX
		FSTP dword ptr [ESP + 020h]
		CALL Distance
		FSTP dword ptr [ESP + 02ch]
		ADD ESP,08h
		TEST EBX,EBX
		JZ flameScene_3003e248
		MOV ECX,dword ptr [ESI + FlameSceneF_ownerCent]
		FLD dword ptr [ECX*FlameSceneStatusSize + centFlameStatus]
		FCOMP qword ptr [flameSceneC300922c8]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e248
		FLD dword ptr [ESI + FlameSceneF_startVelDir + 8]
		FMUL dword ptr [EBX + FlameSceneF_startVelDir + 8]
		FLD dword ptr [ESI + FlameSceneF_startVelDir + 4]
		FMUL dword ptr [EBX + FlameSceneF_startVelDir + 4]
		FADDP st(1), st(0)
		FLD dword ptr [ESI + FlameSceneF_startVelDir]
		FMUL dword ptr [EBX + FlameSceneF_startVelDir]
		FADDP st(1), st(0)
		FST dword ptr [ESP + 028h]
		FCOMP qword ptr [flameSceneC300922c8]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e248
		FLD dword ptr [ESP + 024h]
		FCOMP qword ptr [flameSceneC30092d38]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e248
		FLD dword ptr [ESP + 024h]
		FMUL qword ptr [flameSceneC30092d30]
		FSUBR qword ptr [flameSceneC300922c8]
		FLD dword ptr [ESP + 028h]
		FSUBR qword ptr [flameSceneC300922c8]
		FMULP st(1), st(0)
		FMUL qword ptr [flameSceneC30092320]
		FADD dword ptr [ECX*FlameSceneStatusSize + centFlameStatus]
		FSTP dword ptr [ECX*FlameSceneStatusSize + centFlameStatus]
		MOV ECX,dword ptr [ESI + FlameSceneF_ownerCent]
		FLD dword ptr [ECX*FlameSceneStatusSize + centFlameStatus]
		FCOMP qword ptr [flameSceneC300922c8]
		FNSTSW AX
		TEST AH,041h
		JNZ flameScene_3003e248
		MOV dword ptr [ECX*FlameSceneStatusSize + centFlameStatus],03f800000h
flameScene_3003e248:
		FLD dword ptr [ESI + FlameSceneF_size]
		FMUL qword ptr [flameSceneC30092b88]
		MOV dword ptr [ESP + 048h],ESI
		MOV dword ptr [ESP + 024h],00h
		FMUL dword ptr [EBP]
		FADD dword ptr [ESP + 04ch]
		FSTP dword ptr [ESP + 04ch]
		FLD dword ptr [ESI + FlameSceneF_size]
		FMUL qword ptr [flameSceneC30092b88]
		FMUL dword ptr [ESI + FlameSceneF_org + 4]
		FADD dword ptr [ESP + 050h]
		FSTP dword ptr [ESP + 050h]
		FLD dword ptr [ESI + FlameSceneF_size]
		FMUL qword ptr [flameSceneC30092b88]
		FMUL dword ptr [ESI + FlameSceneF_org + 8]
		FADD dword ptr [ESP + 054h]
		FSTP dword ptr [ESP + 054h]
		FLD dword ptr [ESP + 014h]
		FADD dword ptr [ESI + FlameSceneF_size]
		MOV EAX,dword ptr [ESI + FlameSceneF_ignitionOnly]
		TEST EAX,EAX
		FSTP dword ptr [ESP + 014h]
		FLD dword ptr [ESI + FlameSceneF_size]
		FMUL qword ptr [flameSceneC30092b88]
		FADD dword ptr [ESP + 01ch]
		FSTP dword ptr [ESP + 01ch]
		JNZ flameScene_3003e2ec
		FLD dword ptr [ESI + FlameSceneF_velSpeed]
		FCOMP dword ptr [flameSceneC300922b4]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e2ec
		MOV EDX,dword ptr [ESI + FlameSceneF_lifeFrac]
		PUSH 03f800000h
		PUSH EDX
		PUSH ESI
		CALL CG_AddFlameSpriteToScene
		ADD ESP,0ch
		JMP flameScene_3003e61e
flameScene_3003e2ec:
		MOV EAX,dword ptr [ESP + 044h]
		TEST EAX,EAX
		JZ flameScene_3003e61e
		FILD dword ptr [ESI + FlameSceneF_blueLife]
		FLD dword ptr [ESP + 018h]
		FMUL qword ptr [flameSceneC300922e0]
		FCOMPP
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e61e
		XOR EBX,EBX
		TEST EDI,EDI
		JZ flameScene_3003e391
		CMP ESI,dword ptr [ESP + 074h]
		JZ flameScene_3003e391
		MOV EAX,dword ptr [ESP + 030h]
		TEST EAX,EAX
		JZ flameScene_3003e391
		FLD dword ptr [EBP]
		FSUB dword ptr [EAX + FlameSceneF_org]
		FSTP dword ptr [ESP + 064h]
		FLD dword ptr [ESI + FlameSceneF_org + 4]
		FSUB dword ptr [EAX + FlameSceneF_org + 4]
		FSTP dword ptr [ESP + 068h]
		FLD dword ptr [ESI + FlameSceneF_org + 8]
		FSUB dword ptr [EAX + FlameSceneF_org + 8]
		LEA EAX,[ESP + 064h]
		PUSH EAX
		FSTP dword ptr [ESP + 070h]
		CALL VectorNormalize
		FLD dword ptr [ESI + FlameSceneF_size]
		FMUL dword ptr [flameSceneC300922b8]
		ADD ESP,04h
		FCOMPP
		FNSTSW AX
		TEST AH,041h
		JZ flameScene_3003e38c
		FLD dword ptr [ESP + 06ch]
		FMUL dword ptr [ESI + FlameSceneF_velDir + 8]
		FLD dword ptr [ESP + 068h]
		FMUL dword ptr [ESI + FlameSceneF_velDir + 4]
		FADDP st(1), st(0)
		FLD dword ptr [ESP + 064h]
		FMUL dword ptr [ESI + FlameSceneF_velDir]
		FADDP st(1), st(0)
		FCOMP dword ptr [flameSceneC300920e0]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e391
flameScene_3003e38c:
		MOV EBX,01h
flameScene_3003e391:
		MOV EAX,dword ptr [ESI + FlameSceneF_ignitionOnly]
		TEST EAX,EAX
		JNZ flameScene_3003e3d1
		MOV EAX,dword ptr [ESI + FlameSceneF_ownerCent]
		FLD dword ptr [EAX*FlameSceneStatusSize + centFlameStatus + FlameSceneStreamVolume]
		FADD qword ptr [flameSceneC30092b88]
		FSTP dword ptr [EAX*FlameSceneStatusSize + centFlameStatus + FlameSceneStreamVolume]
		MOV ECX,dword ptr [ESI + FlameSceneF_ownerCent]
		FLD dword ptr [ECX*FlameSceneStatusSize + centFlameStatus + FlameSceneStreamVolume]
		FCOMP qword ptr [flameSceneC300922c8]
		FNSTSW AX
		TEST AH,041h
		JNZ flameScene_3003e3d1
		MOV dword ptr [ECX*FlameSceneStatusSize + centFlameStatus + FlameSceneStreamVolume],03f800000h
flameScene_3003e3d1:
		TEST EBX,EBX
		JNZ flameScene_3003e61e
		MOV ECX,dword ptr [flameSceneWhite]
		MOV EDX,dword ptr [flameSceneWhite + 4]
		MOV EAX,[flameSceneWhite + 8]
		MOV dword ptr [ESP + 058h],ECX
		MOV dword ptr [ESP + 05ch],EDX
		MOV dword ptr [ESP + 060h],EAX
		FILD dword ptr [ESI + FlameSceneF_blueLife]
		FLD dword ptr [ESP + 018h]
		FMUL qword ptr [flameSceneC30092538]
		MOV EBX,dword ptr [ESI + FlameSceneF_ignitionOnly]
		MOV dword ptr [ESP + 030h],ESI
		FCOMPP
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e4a2
		MOV ECX,dword ptr [cg + FlameSceneTime]
		MOV EAX,051eb851fh
		IMUL ECX
		SAR EDX,04h
		MOV EAX,EDX
		SHR EAX,01fh
		ADD EDX,EAX
		MOV EAX,EDX
		SAR EAX,01h
		ADD EAX,EDX
		AND EAX,080000007h
		JNS flameScene_3003e440
		DEC EAX
		OR EAX,0fffffff8h
		INC EAX
flameScene_3003e440:
		MOV EAX,dword ptr [EAX*04h + nozzleShaders]
		TEST EBX,EBX
		JZ flameScene_3003e453
		FLD qword ptr [flameSceneC30092bd0]
		JMP flameScene_3003e459
flameScene_3003e453:
		FLD qword ptr [flameSceneC300922c8]
flameScene_3003e459:
		FMUL dword ptr [ESI + FlameSceneF_size]
		PUSH 040a00000h
		LEA EDX,[ESP + 05ch]
		PUSH 03f800000h
		PUSH EDX
		LEA EDX,[ESP + 064h]
		PUSH EDX
		PUSH 0ch
		PUSH 043480000h
		PUSH ECX
		FSTP dword ptr [ESP]
		PUSH 03f800000h
		PUSH 03f800000h
		PUSH 01h
		PUSH EBP
		PUSH 00h
		PUSH ECX
		PUSH EAX
		MOV EAX,dword ptr [ESP + 06ch]
		PUSH 00h
		PUSH EAX
		CALL CG_AddTrailJunc
		ADD ESP,040h
		MOV dword ptr [ESP + 034h],EAX
flameScene_3003e4a2:
		MOV EAX,dword ptr [ESI + FlameSceneF_ignitionOnly]
		TEST EAX,EAX
		JNZ flameScene_3003e768
		MOV EAX,dword ptr [ESI + FlameSceneF_nextFlameChunk]
		TEST EAX,EAX
		JNZ flameScene_3003e4be
		MOV dword ptr [ESP + 010h],00h
		JMP flameScene_3003e52d
flameScene_3003e4be:
		FLD dword ptr [ESP + 018h]
		FMUL qword ptr [flameSceneC30092d28]
		FILD dword ptr [ESI + FlameSceneF_blueLife]
		FLD st(0)
		FMUL qword ptr [flameSceneC30092a30]
		FSTP qword ptr [ESP + 028h]
		FLD st(1)
		FCOMP qword ptr [ESP + 028h]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e4f0
		FSTP st(0)
		FDIV qword ptr [ESP + 028h]
		FSTP dword ptr [ESP + 010h]
		JMP flameScene_3003e52d
flameScene_3003e4f0:
		FLD st(0)
		FMUL qword ptr [flameSceneC30092b88]
		FSTP qword ptr [ESP + 028h]
		FSUB qword ptr [ESP + 028h]
		FLD st(1)
		FCOMP st(1)
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e519
		FSTP st(0)
		FSTP st(0)
		MOV dword ptr [ESP + 010h],03f800000h
		JMP flameScene_3003e52d
flameScene_3003e519:
		FXCH st(1)
		FSUB st(0),st(1)
		FDIV qword ptr [ESP + 028h]
		FSUBR qword ptr [flameSceneC300922c8]
		FSTP dword ptr [ESP + 010h]
		FSTP st(0)
flameScene_3003e52d:
		FLD dword ptr [ESP + 010h]
		FCOMP qword ptr [flameSceneC300927e0]
		FNSTSW AX
		TEST AH,041h
		JZ flameScene_3003e55b
		FLD dword ptr [ESP + 038h]
		FCOMP qword ptr [flameSceneC300927e0]
		MOV dword ptr [ESP + 010h],00h
		FNSTSW AX
		TEST AH,041h
		JNZ flameScene_3003e61e
flameScene_3003e55b:
		FLD dword ptr [flameSceneWhite]
		FMUL dword ptr [ESP + 010h]
		MOV ECX,dword ptr [ESP + 010h]
		MOV dword ptr [ESP + 024h],01h
		MOV dword ptr [ESP + 038h],ECX
		FSTP dword ptr [ESP + 058h]
		FLD dword ptr [flameSceneWhite + 4]
		FMUL dword ptr [ESP + 010h]
		FSTP dword ptr [ESP + 05ch]
		FLD dword ptr [flameSceneWhite + 8]
		FMUL dword ptr [ESP + 010h]
		FSTP dword ptr [ESP + 060h]
		FLD dword ptr [ESI + FlameSceneF_size]
		FMUL dword ptr [flameSceneC300922b8]
		FLD dword ptr [ESI + FlameSceneF_sizeMax]
		FMUL dword ptr [flameSceneC30092490]
		FSTP dword ptr [ESP + 028h]
		FCOM dword ptr [ESP + 028h]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e5bf
		FSTP dword ptr [ESP + 03ch]
		JMP flameScene_3003e5c9
flameScene_3003e5bf:
		MOV EDX,dword ptr [ESP + 028h]
		FSTP st(0)
		MOV dword ptr [ESP + 03ch],EDX
flameScene_3003e5c9:
		MOV EDX,dword ptr [ESP + 03ch]
		PUSH 03fc00000h
		LEA EAX,[ESP + 05ch]
		PUSH 03f000000h
		LEA ECX,[ESP + 060h]
		PUSH EAX
		MOV EAX,dword ptr [ESP + 01ch]
		PUSH ECX
		MOV ECX,dword ptr [ESI + FlameSceneF_ignitionOnly]
		PUSH 0eh
		PUSH 043480000h
		PUSH EDX
		MOV EDX,dword ptr [cgs + FlameSceneStreamShader]
		PUSH EAX
		PUSH EAX
		XOR EAX,EAX
		TEST ECX,ECX
		MOV ECX,dword ptr [cg + FlameSceneTime]
		PUSH 01h
		SETZ AL
		PUSH EBP
		PUSH EAX
		MOV EAX,dword ptr [ESP + 070h]
		PUSH ECX
		PUSH EDX
		PUSH 00h
		PUSH EAX
		CALL CG_AddTrailJunc
		ADD ESP,040h
		MOV dword ptr [ESP + 040h],EAX
flameScene_3003e61e:
		MOV EAX,dword ptr [ESI + FlameSceneF_ignitionOnly]
		TEST EAX,EAX
		JNZ flameScene_3003e768
		FILD dword ptr [ESI + FlameSceneF_blueLife]
		FMUL qword ptr [flameSceneC30092a30]
		FCOMP dword ptr [ESP + 018h]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e768
		TEST EDI,EDI
		JZ flameScene_3003e6fb
		MOV EBX,dword ptr [ESP + 024h]
flameScene_3003e64d:
		TEST EBX,EBX
		JNZ flameScene_3003e6fb
		LEA ECX,[EDI + FlameSceneF_org]
		PUSH ECX
		PUSH EBP
		CALL Distance
		FLD dword ptr [ESI + FlameSceneF_lifeFrac]
		FMUL qword ptr [flameSceneC30092408]
		ADD ESP,08h
		FADD qword ptr [flameSceneC300925c0]
		FMUL dword ptr [ESI + FlameSceneF_size]
		FMUL qword ptr [flameSceneC30092d20]
		FCOMPP
		FNSTSW AX
		TEST AH,041h
		JNZ flameScene_3003e6fb
		FLD dword ptr [ESI + FlameSceneF_size]
		FSUB dword ptr [EDI + FlameSceneF_size]
		FABS
		FCOMP qword ptr [flameSceneC30092a70]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e6fb
		MOV EDX,dword ptr [ESI + FlameSceneF_timeStart]
		MOV EAX,dword ptr [EDI + FlameSceneF_timeStart]
		SUB EDX,EAX
		MOV dword ptr [ESP + 028h],EDX
		FILD dword ptr [ESP + 028h]
		FABS
		FCOMP qword ptr [flameSceneC300922f8]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e6fb
		FLD dword ptr [EDI + FlameSceneF_velDir + 8]
		FMUL dword ptr [ESI + FlameSceneF_velDir + 8]
		FLD dword ptr [EDI + FlameSceneF_velDir + 4]
		FMUL dword ptr [ESI + FlameSceneF_velDir + 4]
		FADDP st(1), st(0)
		FLD dword ptr [EDI + FlameSceneF_velDir]
		FMUL dword ptr [ESI + FlameSceneF_velDir]
		FADDP st(1), st(0)
		FCOMP qword ptr [flameSceneC300925c8]
		FNSTSW AX
		TEST AH,041h
		JNZ flameScene_3003e6fb
		PUSH EDI
		PUSH ESI
		CALL CG_MergeFlameChunks
		MOV EDI,dword ptr [ESI + FlameSceneF_nextFlameChunk]
		ADD ESP,08h
		TEST EDI,EDI
		JNZ flameScene_3003e64d
flameScene_3003e6fb:
		FILD dword ptr [ESI + FlameSceneF_blueLife]
		FMUL qword ptr [flameSceneC30092a30]
		FLD dword ptr [ESP + 018h]
		FSUB st(0),st(1)
		FLD qword ptr [flameSceneC30092ce0]
		FSUB st(0),st(2)
		FDIVP st(1), st(0)
		FSTP dword ptr [ESP + 024h]
		FSTP st(0)
		FLD dword ptr [ESP + 024h]
		FSUBR qword ptr [flameSceneC300922c8]
		FMUL qword ptr [flameSceneC30092d18]
		FST dword ptr [ESP + 028h]
		FCOMP qword ptr [flameSceneC300922c8]
		FNSTSW AX
		TEST AH,041h
		JNZ flameScene_3003e743
		MOV dword ptr [ESP + 028h],03f800000h
flameScene_3003e743:
		MOV EAX,dword ptr [ESP + 028h]
		MOV ECX,dword ptr [ESP + 024h]
		PUSH EAX
		PUSH ECX
		PUSH ESI
		CALL CG_AddFlameSpriteToScene
		FLD dword ptr [ESI + FlameSceneF_sizeMax]
		FMUL dword ptr [flameSceneC30092cb8]
		ADD ESP,0ch
		FMUL qword ptr [flameSceneC30092cf0]
		FSTP dword ptr [ESI + FlameSceneF_sizeRate]
flameScene_3003e768:
		TEST EDI,EDI
		MOV ESI,EDI
		JNZ flameScene_3003e147
		MOV ESI,dword ptr [ESP + 074h]
		POP EBP
flameScene_3003e777:
		MOV EDX,dword ptr [lastFlameOwner]
		MOV EAX,dword ptr [ESI + FlameSceneF_ownerCent]
		CMP EDX,EAX
		MOV EAX,[cg + FlameSceneClientFrame]
		JNZ flameScene_3003e795
		CMP dword ptr [nextFlameLight],EAX
		JZ flameScene_3003e8cd
flameScene_3003e795:
		MOV ECX,dword ptr [ESI + FlameSceneF_ignitionOnly]
		TEST ECX,ECX
		JNZ flameScene_3003e7a9
		MOV [nextFlameLight],EAX
		MOV EAX,dword ptr [ESI + FlameSceneF_ownerCent]
		MOV [lastFlameOwner],EAX
flameScene_3003e7a9:
		FLD dword ptr [ESP + 010h]
		FCOMP dword ptr [flameSceneC300924e0]
		FNSTSW AX
		TEST AH,01h
		JZ flameScene_3003e7c4
		MOV dword ptr [ESP + 010h],042a00000h
		JMP flameScene_3003e7dd
flameScene_3003e7c4:
		FLD dword ptr [ESP + 010h]
		FCOMP dword ptr [flameSceneC30092a24]
		FNSTSW AX
		TEST AH,041h
		JNZ flameScene_3003e7dd
		MOV dword ptr [ESP + 010h],043fa0000h
flameScene_3003e7dd:
		FILD dword ptr [cg + FlameSceneTime]
		FLD st(0)
		FMUL qword ptr [flameSceneC30092d10]
		FCOS
		FXCH st(1)
		FMUL qword ptr [flameSceneC30092d08]
		FSIN
		FMULP st(1), st(0)
		FMUL qword ptr [flameSceneC30092a30]
		FADD qword ptr [flameSceneC300922c8]
		FMUL dword ptr [ESP + 010h]
		FMUL qword ptr [flameSceneC30092ad0]
		FST dword ptr [ESP + 0ch]
		FCOMP qword ptr [flameSceneC30092bd0]
		FNSTSW AX
		TEST AH,041h
		JNZ flameScene_3003e828
		MOV dword ptr [ESP + 0ch],040000000h
flameScene_3003e828:
		FLD dword ptr [ESP + 018h]
		FDIVR qword ptr [flameSceneC300922c8]
		MOV EAX,dword ptr [ESI + FlameSceneF_ignitionOnly]
		TEST EAX,EAX
		FLD dword ptr [ESP + 048h]
		FMUL st(0), st(1)
		FSTP dword ptr [ESP + 048h]
		FLD dword ptr [ESP + 04ch]
		FMUL st(0), st(1)
		FSTP dword ptr [ESP + 04ch]
		FLD dword ptr [ESP + 050h]
		FMUL st(0), st(1)
		FSTP dword ptr [ESP + 050h]
		FSTP st(0)
		JZ flameScene_3003e88a
		MOV ECX,dword ptr [ESP + 0ch]
		PUSH 00h
		PUSH 00h
		PUSH 03f000000h
		PUSH 03e570a3dh
		PUSH 03e4ccccdh
		PUSH ECX
		LEA EDX,[ESP + 060h]
		PUSH 042a00000h
		PUSH EDX
		CALL trap_R_AddLightToScene
		ADD ESP,020h
		POP EDI
		POP ESI
		POP EBX
		ADD ESP,060h
		RET
flameScene_3003e88a:
		MOV EAX,dword ptr [ESP + 040h]
		TEST EAX,EAX
		JNZ flameScene_3003e8a3
		MOV ECX,dword ptr [cg + FlameSceneSnapshot]
		MOV EAX,dword ptr [ESI + FlameSceneF_ownerCent]
		CMP EAX, dword ptr [ECX + FlameSceneSnapshotClient]
		JNZ flameScene_3003e8cd
flameScene_3003e8a3:
		MOV EDX,dword ptr [ESP + 0ch]
		PUSH 00h
		PUSH 00h
		PUSH 03e54d4cch
		PUSH 03f1a9aa2h
		PUSH 03f800000h
		PUSH EDX
		LEA EAX,[ESP + 060h]
		PUSH 043a00000h
		PUSH EAX
		CALL trap_R_AddLightToScene
		ADD ESP,020h
flameScene_3003e8cd:
		POP EDI
		POP ESI
		POP EBX
		ADD ESP,060h
		RET
	}
}
#else
void CG_AddFlameToScene( flameChunk_t *fHead ) {
	flameChunk_t *f, *fNext;
	int		blueTrailHead=0, fuelTrailHead=0;
	static	vec3_t whiteColor = {1,1,1};
	vec3_t	c;
	float	alpha;
	float	lived;
	int		headTimeStart;
	float	vdist, bdot;
	flameChunk_t *lastBlowChunk=NULL;
	qboolean isClientFlame, firing;
	int shader;
	flameChunk_t *lastBlueChunk=NULL;
	qboolean skip=qfalse, droppedTrail;
	vec3_t	v;
	vec3_t	lightOrg;				// origin to place light at
	float	lightSize;
	float	lightFlameCount;
	float	lastFuelAlpha;

	isClientFlame = (fHead == centFlameInfo[fHead->ownerCent].lastFlameChunk);

	if ((cg_entities[fHead->ownerCent].currentState.eFlags & EF_FIRING) && (centFlameInfo[fHead->ownerCent].lastFlameChunk == fHead)) {
		headTimeStart = fHead->timeStart;
		firing = qtrue;
	} else {
		headTimeStart = cg.time;
		firing = qfalse;
	}

	VectorClear( lightOrg );
	lightSize = 0;
	lightFlameCount = 0;

	lastFuelAlpha = 1.0;

	f = fHead;
	while (f) {

		if (f->nextFlameChunk && f->nextFlameChunk->dead) {
			// kill it
			CG_FreeFlameChunk(f->nextFlameChunk);
			f->nextFlameChunk = NULL;
		}

		// draw this chunk

		fNext = f->nextFlameChunk;
		lived = (float)(headTimeStart - f->timeStart);

		// update the "blow" sound volume (louder as we sway it)
		vdist = Distance( cg.refdef_current->vieworg, f->org );	// NOTE: this needs to be here or the flameSound code further below won't work
		if (	lastBlowChunk && (centFlameStatus[f->ownerCent].blowVolume < 1.0) &&
				((bdot = DotProduct(lastBlowChunk->startVelDir, f->startVelDir)) < 1.0)) {
			if (vdist < FLAME_SOUND_RANGE) {
				centFlameStatus[f->ownerCent].blowVolume += 500.0 * (1.0 - bdot) * (1.0 - (vdist / FLAME_SOUND_RANGE));
				if (centFlameStatus[f->ownerCent].blowVolume > 1.0)
					centFlameStatus[f->ownerCent].blowVolume = 1.0;
			}
		}
		lastBlowChunk = f;

		VectorMA( lightOrg, f->size/20.0, f->org, lightOrg );
		lightSize += f->size;
		lightFlameCount += f->size/20.0;

		droppedTrail = qfalse;

		// is it a stream chunk? (no special handling)
		if (!f->ignitionOnly && f->velSpeed < 1) {
			CG_AddFlameSpriteToScene (f, f->lifeFrac, 1.0);

		// is it in the blue ignition section of the flame?
		} else if (isClientFlame && f->blueLife > (lived/2.0)) {

			skip = qfalse;

			// if this is backwards from the last chunk, then skip it
			if ( fNext && f != fHead && lastBlueChunk) {
				VectorSubtract( f->org, lastBlueChunk->org, v );
				if (VectorNormalize( v ) < f->size/2 )
					skip = qtrue;
				else if (DotProduct( v, f->velDir ) < 0)
					skip = qtrue;
			}

			// stream sound
			if (!f->ignitionOnly) {
				centFlameStatus[f->ownerCent].streamVolume += 0.05;
				if (centFlameStatus[f->ownerCent].streamVolume > 1.0)
					centFlameStatus[f->ownerCent].streamVolume = 1.0;
			}


			if (!skip) {

				// just call this for damage checking
				//if (!f->ignitionOnly)
					//CG_AddFlameSpriteToScene( f, f->lifeFrac, -1 );

				lastBlueChunk = f;

				alpha = 1.0;	// new nozzle sprite
				VectorScale( whiteColor, alpha, c );

				if (f->blueLife > lived*(f->ignitionOnly ? 3.0 : 3.0)) {

					shader = nozzleShaders[(cg.time/50 + (cg.time/50 >> 1))%NUM_NOZZLE_SPRITES];

					blueTrailHead = CG_AddTrailJunc(	blueTrailHead,
														NULL, // rain - zinx's trail fix
														shader,
														cg.time,
														STYPE_STRETCH,
														f->org,
														1,
														alpha, alpha,
														f->size * (f->ignitionOnly /*&& (cg.snap->ps.clientNum != f->ownerCent || cg_thirdPerson.integer)*/ ? 2.0 : 1.0),
														FLAME_MAX_SIZE,
														TJFL_NOCULL|TJFL_FIXDISTORT,
														c, c, 1.0, 5.0 );
				}

				// fire stream
				if (!f->ignitionOnly) {
					float bscale;
					qboolean fskip=qfalse;

					bscale = 1.0;

					if (!f->nextFlameChunk) {
						alpha = 0;
					} else if (lived/1.3 < bscale*FLAME_BLUE_FADEIN_TIME(f->blueLife)) {
						alpha = FLAME_BLUE_MAX_ALPHA * ((lived/1.3) / (bscale*FLAME_BLUE_FADEIN_TIME(f->blueLife)));
					} else if (lived/1.3 < (f->blueLife - FLAME_BLUE_FADEOUT_TIME(f->blueLife))) {
						alpha = FLAME_BLUE_MAX_ALPHA;
					} else {
						alpha = FLAME_BLUE_MAX_ALPHA * (1.0 - ((lived/1.3 - (f->blueLife - FLAME_BLUE_FADEOUT_TIME(f->blueLife))) / (FLAME_BLUE_FADEOUT_TIME(f->blueLife))));
					}
					if (alpha <= 0.0) {
						alpha = 0.0;
						if (lastFuelAlpha <= 0.0)
							fskip = qtrue;
					}

					if (!fskip) {
						lastFuelAlpha = alpha;

						VectorScale( whiteColor, alpha, c );

						droppedTrail = qtrue;

						fuelTrailHead = CG_AddTrailJunc(	fuelTrailHead,
															NULL, // rain - zinx's trail fix
															cgs.media.flamethrowerFireStream,
															cg.time,
															(f->ignitionOnly ? STYPE_STRETCH : STYPE_REPEAT),
															f->org,
															1,
															alpha, alpha,
															(f->size/2 < f->sizeMax/4 ? f->size/2 : f->sizeMax/4),
															FLAME_MAX_SIZE,
															TJFL_NOCULL|TJFL_FIXDISTORT|TJFL_CROSSOVER,
															c, c, 0.5, 1.5 );
					}
				}
			}
		}

#define	FLAME_SPRITE_START_BLUE_SCALE	0.2

		if (!f->ignitionOnly &&
			((float)(FLAME_SPRITE_START_BLUE_SCALE*f->blueLife) < (float)lived)) {

			float alpha, lifeFrac;
			qboolean skip=qfalse;

			// should we merge it with the next sprite?
			while (fNext && !droppedTrail) {
				if ( (Distance( f->org, fNext->org ) < ( (0.1 + 0.9*f->lifeFrac) * f->size*0.35 ) )
					&&	(fabs(f->size - fNext->size) < (40.0))
					&&	(fabs((double)f->timeStart - fNext->timeStart) < 100)
					&&	(DotProduct( f->velDir, fNext->velDir ) > 0.99)
					) {
					if (!droppedTrail) {
						CG_MergeFlameChunks( f, fNext );
						fNext = f->nextFlameChunk;		// it may have changed
					} else {
						skip = qtrue;
						break;
					}
				} else {
					break;
				}
			}

			lifeFrac = (lived - FLAME_SPRITE_START_BLUE_SCALE*f->blueLife) / (FLAME_LIFETIME - FLAME_SPRITE_START_BLUE_SCALE*f->blueLife);

			alpha = (1.0 - lifeFrac)*1.4;
			if (alpha > 1.0)
				alpha = 1.0;

			if (!skip) {
				// draw the sprite
				CG_AddFlameSpriteToScene (f, lifeFrac, alpha );
			}
			// update the sizeRate
			f->sizeRate = GET_FLAME_SIZE_SPEED( f->sizeMax );
		}

		f = fNext;
	}

	if ( lastFlameOwner == fHead->ownerCent && nextFlameLight == cg.clientFrame )
		return;

	if ( !fHead->ignitionOnly ) {
		nextFlameLight = cg.clientFrame;
		lastFlameOwner = fHead->ownerCent;
	}

	if (lightSize < 80) {
		lightSize = 80;
	}

	if (lightSize > 500) lightSize = 500;
	lightSize *= 1.0 + 0.2*(sin(1.0*cg.time/50.0) * cos(1.0*cg.time/43.0));
	// set the alpha
	//%	alpha = lightSize / 500.0;
	alpha = lightSize * 0.005;	// ydnar
	if (alpha > 2.0) {
		alpha = 2.0;
	}
	VectorScale( lightOrg, 1.0/lightFlameCount, lightOrg );
	// if it's only a nozzle, make it blue
	if (fHead->ignitionOnly) {
		if (lightSize > 80) lightSize = 80;
		//%	trap_R_AddLightToScene( lightOrg, 90 + lightSize, 0, 0, alpha, 0 + isClientFlame * (fHead->ownerCent == cg.snap->ps.clientNum) );
		trap_R_AddLightToScene( lightOrg, 80, alpha, 0.2, 0.21, 0.5, 0, 0 );
	} else if (isClientFlame || (fHead->ownerCent == cg.snap->ps.clientNum)) {
		//%	trap_R_AddLightToScene( lightOrg, 90 + lightSize, 1.000000*alpha, 0.603922*alpha, 0.207843*alpha, 0 );
		trap_R_AddLightToScene( lightOrg, 320, alpha, 1.000000, 0.603922, 0.207843, 0, 0 );
	}
}
#endif


/*
=============
CG_GenerateShaders

  A util to create a bunch of shaders in a unique shader file, which represent an animation
=============
*/
void CG_GenerateShaders( char *filename, char *shaderName, char *dir, int numFrames, char *srcBlend, char *dstBlend, char *extras, qboolean compressedVersionAvailable, qboolean nomipmap )
{
	fileHandle_t f;
	int b, c, d, lastNumber;
	char	str[512];
	int i;

	trap_FS_FOpenFile( filename, &f, FS_WRITE );
	for (i=0; i<numFrames; i++) {
		lastNumber = i;
		b = lastNumber / 100;
		lastNumber -= b*100;
		c = lastNumber / 10;
		lastNumber -= c*10;
		d = lastNumber;

		if (compressedVersionAvailable) {
			Com_sprintf( str, sizeof(str), "%s%i\n{\n\tnofog%s\n\tallowCompress\n\tcull none\n\t{\n\t\tmapcomp sprites/%s_lg/spr%i%i%i.tga\n\t\tmapnocomp sprites/%s/spr%i%i%i.tga\n\t\tblendFunc %s %s\n%s\t}\n}\n", shaderName, i+1, nomipmap ? "\n\tnomipmaps" : "", dir, b, c, d, dir, b, c, d, srcBlend, dstBlend, extras );
		} else {
			Com_sprintf( str, sizeof(str), "%s%i\n{\n\tnofog%s\n\tallowCompress\n\tcull none\n\t{\n\t\tmap sprites/%s/spr%i%i%i.tga\n\t\tblendFunc %s %s\n%s\t}\n}\n", shaderName, i+1, nomipmap ? "\n\tnomipmap" : "", dir, b, c, d, srcBlend, dstBlend, extras );
		}
		trap_FS_Write( str, Q_strlenInt(str), f );
	}
	trap_FS_FCloseFile( f );
}	

/*
===============
CG_InitFlameChunks
===============
*/
void CG_InitFlameChunks(void)
{
	int i;
	char filename[MAX_QPATH];

	CG_ClearFlameChunks();

#ifdef GEN_FLAME_SHADER
	CG_GenerateShaders( "scripts/flamethrower.shader",
						"flamethrowerFire",
						FLAME_SPRITE_DIR,
						NUM_FLAME_SPRITES,
						FLAME_BLEND_SRC,
						FLAME_BLEND_DST,
						"",
						qtrue, qtrue );

	CG_GenerateShaders( "scripts/blacksmokeanim.shader",
						"blacksmokeanim",
						"explode1",
						23,
						"GL_ZERO",
						"GL_ONE_MINUS_SRC_ALPHA",
						"\t\talphaGen const 0.2\n",
						qfalse, qfalse );

	CG_GenerateShaders( "scripts/viewflames.shader",
						"viewFlashFire",
						"clnfire",
						16,
						"GL_ONE",
						"GL_ONE",
						"\t\talphaGen vertex\n\t\trgbGen vertex\n",
						qtrue, qtrue );

	CG_GenerateShaders( "scripts/twiltb.shader",
						"twiltb",
						"twiltb",
						42,
						"GL_SRC_ALPHA",
						"GL_ONE_MINUS_SRC_COLOR",
						"",
						qtrue, qfalse );

	CG_GenerateShaders( "scripts/twiltb2.shader",
						"twiltb2",
						"twiltb2",
						45,
						"GL_ONE",
						"GL_ONE_MINUS_SRC_COLOR",
						"",
						qtrue, qfalse );
/*
	CG_GenerateShaders( "scripts/expblue.shader",
						"expblue",
						"expblue",
						25,
						"GL_ONE",
						"GL_ONE_MINUS_SRC_COLOR",
						"",
						qfalse, qfalse );
*/
	CG_GenerateShaders( "scripts/firest.shader",
						"firest",
						"firest",
						36,
						"GL_ONE",
						"GL_ONE_MINUS_SRC_COLOR",
						"",
						qtrue, qfalse );

	CG_GenerateShaders( "scripts/explode1.shader",
						"explode1",
						"explode1",
						23,
						"GL_ONE",
						"GL_ONE_MINUS_SRC_COLOR",
						"",
						qtrue, qfalse );

	CG_GenerateShaders( "scripts/funnel.shader",
						"funnel",
						"funnel",
						21,
						"GL_ONE",
						"GL_ONE_MINUS_SRC_COLOR",
						"",
						qfalse, qfalse );
#endif

	for (i=0; i<NUM_FLAME_SPRITES; i++) {
		Com_sprintf( filename, MAX_QPATH, "flamethrowerFire%i", i+1 );
		flameShaders[i] = trap_R_RegisterShader( filename );
	}
	for (i=0; i<NUM_NOZZLE_SPRITES; i++) {
		Com_sprintf( filename, MAX_QPATH, "nozzleFlame%i", i+1 );
		nozzleShaders[i] = trap_R_RegisterShader( filename );
	}
	initFlameShaders = qfalse;
}

/*
===============
CG_AddFlameChunks
===============
*/
void CG_AddFlameChunks(void)
{
	flameChunk_t *f, *fNext;

	//AngleVectors( cg.refdef.viewangles, NULL, vright, vup );
	VectorCopy( cg.refdef_current->viewaxis[1], vright );
	VectorCopy( cg.refdef_current->viewaxis[2], vup );

	// clear out the volumes so we can rebuild them
	memset( centFlameStatus, 0, sizeof(centFlameStatus) );

	numClippedFlames = 0;

	// age them
	f = activeFlameChunks;
	while (f) {
		if (!f->dead) {
			if (cg.time > f->timeEnd) {
				f->dead = qtrue;
			} else if (f->ignitionOnly && (f->blueLife < (cg.time - f->timeStart))) {
				f->dead = qtrue;
			} else {
				CG_MoveFlameChunk( f );
				f->lifeFrac = (float)(cg.time - f->timeStart)/(float)(f->timeEnd - f->timeStart);
			}
		}
		f = f->nextGlobal;
	}

	// draw each of the headFlameChunk's
	f = headFlameChunks;
	while (f) {
		fNext = f->nextHead;		// in case it gets removed
		if (f->dead) {
			if (centFlameInfo[f->ownerCent].lastFlameChunk == f) {
				centFlameInfo[f->ownerCent].lastFlameChunk = NULL;
				centFlameInfo[f->ownerCent].lastClientFrame = 0;
			}
			CG_FreeFlameChunk(f);
		} else if (!f->ignitionOnly || (centFlameInfo[f->ownerCent].lastFlameChunk == f)) {	// don't draw the ignition flame after we start firing
			CG_AddFlameToScene( f );
		}
		f = fNext;
	}
}

/*
===============
CG_UpdateFlamethrowerSounds
===============
*/
void CG_UpdateFlamethrowerSounds(void) {
	flameChunk_t *f, *trav; // , *lastSoundFlameChunk=NULL; // TTimo: unused
	#define	MIN_BLOW_VOLUME		30

	// draw each of the headFlameChunk's
	f = headFlameChunks;
	while (f) {
		// update this entity?
		if (centFlameInfo[f->ownerCent].lastSoundUpdate != cg.time) {
			// blow/ignition sound
			if (centFlameStatus[f->ownerCent].blowVolume*255.0 > MIN_BLOW_VOLUME) {
				trap_S_AddLoopingSound( f->org, vec3_origin, cgs.media.flameBlowSound, (int)(255.0*centFlameStatus[f->ownerCent].blowVolume), 0 ); // JPW NERVE
			} else {
				trap_S_AddLoopingSound( f->org, vec3_origin, cgs.media.flameBlowSound, MIN_BLOW_VOLUME, 0 ); // JPW NERVE
			}

			if (centFlameStatus[f->ownerCent].streamVolume) {
					trap_S_AddLoopingSound( f->org, vec3_origin, cgs.media.flameStreamSound, (int)(255.0*centFlameStatus[f->ownerCent].streamVolume), 0 ); // JPW NERVE
			}

			centFlameInfo[f->ownerCent].lastSoundUpdate = cg.time;
		}

		// traverse the chunks, spawning flame sound sources as we go
		for (trav=f; trav; trav=trav->nextFlameChunk) {
			// update the sound volume
			if (trav->blueLife+100 < (cg.time - trav->timeStart)) {
				trap_S_AddLoopingSound( trav->org, vec3_origin, cgs.media.flameSound, (int)(255.0*(0.2 * (trav->size / FLAME_MAX_SIZE))), 0 );
			}
		}

		f = f->nextHead;
	}
}
