#include <stddef.h>
#ifdef CGAMEDLL
#include "../cgame/cg_local.h"
#else
#include "g_local.h"
#endif

/*
**  Map tracemap view generation
*/

#define	MAX_WORLD_HEIGHT			MAX_MAP_SIZE	// maximum world height
#define	MIN_WORLD_HEIGHT			-MAX_MAP_SIZE	// minimum world height

//#define TRACEMAP_SIZE				1024
#define TRACEMAP_SIZE				256

typedef struct tracemap_s {
	qboolean loaded;
	float sky[TRACEMAP_SIZE][TRACEMAP_SIZE];
	float skyground[TRACEMAP_SIZE][TRACEMAP_SIZE];
	float ground[TRACEMAP_SIZE][TRACEMAP_SIZE];
	vec2_t world_mins, world_maxs;
	int groundfloor, groundceil;
} tracemap_t;

static tracemap_t tracemap;

static vec2_t one_over_mapgrid_factor;

#if defined(CGAMEDLL) && defined(_MSC_VER) && defined(_M_IX86)
typedef char TraceLoadLayoutGuard[
    (TRACEMAP_SIZE == 256 && sizeof(float) == 4 &&
     sizeof(((tracemap_t*)0)->sky[0]) == 1024 &&
     offsetof(tracemap_t, skyground) == offsetof(tracemap_t, sky) + sizeof(((tracemap_t*)0)->sky) &&
     offsetof(tracemap_t, ground) == offsetof(tracemap_t, skyground) + sizeof(((tracemap_t*)0)->skyground) &&
     offsetof(tracemap_t, world_mins) == offsetof(tracemap_t, ground) + sizeof(((tracemap_t*)0)->ground)) ? 1 : -1];
/* Native fields for TC30016b50/30016be0/30016c60. */
enum {
    TraceQueryLoaded = offsetof(tracemap_t, loaded),
    TraceQueryMinX = offsetof(tracemap_t, world_mins),
    TraceQueryMinY = offsetof(tracemap_t, world_mins) + sizeof(float),
    TraceQueryMaxX = offsetof(tracemap_t, world_maxs),
    TraceQueryMaxY = offsetof(tracemap_t, world_maxs) + sizeof(float),
    TraceQuerySky = offsetof(tracemap_t, sky),
    TraceQuerySkyGround = offsetof(tracemap_t, skyground),
    TraceLoadGround = offsetof(tracemap_t, ground),
    TraceLoadSkyLast = offsetof(tracemap_t, sky) + sizeof(((tracemap_t*)0)->sky) - sizeof(((tracemap_t*)0)->sky[0]),
    TraceLoadSkyGroundLast = offsetof(tracemap_t, skyground) + sizeof(((tracemap_t*)0)->skyground) - sizeof(((tracemap_t*)0)->skyground[0]),
    TraceLoadGroundLast = offsetof(tracemap_t, ground) + sizeof(((tracemap_t*)0)->ground) - sizeof(((tracemap_t*)0)->ground[0]),
    TraceLoadFloor = offsetof(tracemap_t, groundfloor),
    TraceLoadCeil = offsetof(tracemap_t, groundceil)
};
static const float traceQueryMaxHeight = 65536.0f;
static const float traceLoadZero = 0.0f, traceLoadOne = 1.0f;
static const float traceLoadScale = 254.0f, traceLoadInverseSize = 0.00390625f;
static const char traceLoadPath[] = "maps/%s_tracemap.tga";
#endif

void etpro_FinalizeTracemapClamp(int *x, int *y);

#ifdef CGAMEDLL
#if defined(_MSC_VER) && defined(_M_IX86)
static unsigned int CG_TraceMapBits( const float *traceValue ) {
	unsigned int traceBits;
	memcpy( &traceBits, traceValue, sizeof(traceBits) );
	return traceBits;
}

static void CG_TraceMapStoreHeight( float *traceDest, const float *traceSource ) {
	__asm {
		mov ecx, traceSource
		fld dword ptr [ecx]
		mov ecx, traceDest
		fstp dword ptr [ecx]
	}
}

static unsigned int CG_TraceMapCompare( float traceLeft, float traceRight ) {
	unsigned int traceFlags;
	__asm {
		fld traceLeft
		fcomp traceRight
		fnstsw ax
		and eax, 4100h
		mov traceFlags, eax
	}
	return traceFlags;
}

static unsigned int CG_TraceMapCompareInteger( int traceLeft, float traceRight ) {
	unsigned int traceFlags;
	__asm {
		fild traceLeft
		fcomp traceRight
		fnstsw ax
		and eax, 4100h
		mov traceFlags, eax
	}
	return traceFlags;
}

/* Store the new trace origin, then compare the unrounded x87 value. */
static unsigned int CG_TraceMapUpdateZ( float traceValue, float traceStep,
	float *traceStored, float traceBound, int traceSubtract ) {
	unsigned int traceFlags;
	__asm {
		fld traceValue
		cmp traceSubtract, 0
		je traceZAdd
		fsub traceStep
		jmp traceZStore
	traceZAdd:
		fadd traceStep
	traceZStore:
		mov ecx, traceStored
		fst dword ptr [ecx]
		fcomp traceBound
		fnstsw ax
		and eax, 4100h
		mov traceFlags, eax
	}
	return traceFlags;
}

static double CG_TraceMapProgress( int tracePoints ) {
	static const float traceFraction = 0.0000152587890625f, tracePercent = 100.0f;
	double traceResult;
	__asm {
		fild tracePoints
		fmul traceFraction
		fmul tracePercent
		fstp traceResult
	}
	return traceResult;
}

/* Original __ftol returns the low word of a truncating signed 64-bit store. */
static int CG_TraceMapInteger( float traceValue ) {
	unsigned short traceSavedCW, traceTruncateCW;
	__int64 traceInteger;
	__asm {
		fld traceValue
		fstcw traceSavedCW
		fwait
		mov ax, traceSavedCW
		or ah, 0ch
		mov traceTruncateCW, ax
		fldcw traceTruncateCW
		fistp traceInteger
		fldcw traceSavedCW
	}
	return (int)traceInteger;
}

/* The original stores the same retained index*step+minimum result twice. */
static void CG_TraceMapGridCoordinate( int gridIndex, float gridStep, float gridMin,
	float *gridStart, float *gridEnd ) {
	__asm {
		fild gridIndex
		fmul gridStep
		fadd gridMin
		mov eax, gridEnd
		fst dword ptr [eax]
		mov eax, gridStart
		fstp dword ptr [eax]
	}
}

/* Original CG_GenerateTracemap 300160d4..30016288. Keep the scale and
 * integer minimum in x87 registers across the whole plane, not binary32. */
static void CG_TraceMapNormalizePlane( float *plane, int planeMin, int planeMax, int skyPlane ) {
	static const float traceRange = 254.0f, traceOne = 1.0f, traceZero = 0.0f;
	static const double traceOneDouble = 1.0, traceZeroDouble = 0.0, traceMaxDouble = 255.0;
	int traceDelta;
	__asm {
		mov eax, planeMax
		sub eax, planeMin
		mov traceDelta, eax
		cmp skyPlane, 0
		jne traceSkyScale
		fild traceDelta
		fdivr traceRange
		fcom traceZero
		fnstsw ax
		test ah, 40h
		jz traceGroundScaleReady
		fstp st(0)
		fld traceOne
	traceGroundScaleReady:
		fild planeMin
		mov ecx, plane
		mov edx, 65536
	traceGroundPixel:
		fcom dword ptr [ecx]
		fnstsw ax
		test ah, 41h
		jz traceGroundClamp
		fld dword ptr [ecx]
		fsub st(0), st(1)
		fmul st(0), st(2)
		fadd traceOneDouble
		fstp dword ptr [ecx]
	traceGroundClamp:
		fld dword ptr [ecx]
		fcomp traceOneDouble
		fnstsw ax
		test ah, 1
		jz traceGroundUpper
		mov dword ptr [ecx], 3f800000h
		jmp traceGroundNext
	traceGroundUpper:
		fld dword ptr [ecx]
		fcomp traceMaxDouble
		fnstsw ax
		test ah, 41h
		jnz traceGroundNext
		mov dword ptr [ecx], 437f0000h
	traceGroundNext:
		add ecx, 4
		dec edx
		jnz traceGroundPixel
		fstp st(0)
		fstp st(0)
		jmp traceNormalizeDone
	traceSkyScale:
		cmp traceDelta, 0
		jne traceSkyDivide
		fld traceOne
		jmp traceSkyScaleReady
	traceSkyDivide:
		fild traceDelta
		fdivr traceRange
	traceSkyScaleReady:
		mov ecx, plane
		mov edx, 65536
	traceSkyPixel:
		cmp dword ptr [ecx], 47800000h
		jne traceSkyValue
		mov dword ptr [ecx], 0
		jmp traceSkyClamp
	traceSkyValue:
		fild planeMin
		fsubr dword ptr [ecx]
		fmul st(0), st(1)
		fadd traceOne
		fstp dword ptr [ecx]
	traceSkyClamp:
		fld dword ptr [ecx]
		fcomp traceZeroDouble
		fnstsw ax
		test ah, 1
		jz traceSkyUpper
		mov dword ptr [ecx], 0
		jmp traceSkyNext
	traceSkyUpper:
		fld dword ptr [ecx]
		fcomp traceMaxDouble
		fnstsw ax
		test ah, 41h
		jnz traceSkyNext
		mov dword ptr [ecx], 437f0000h
	traceSkyNext:
		add ecx, 4
		dec edx
		jnz traceSkyPixel
		fstp st(0)
	traceNormalizeDone:
	}
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
#define CG_TRACEMAP_PROGRESS(points) CG_TraceMapProgress(points)
#else
#define CG_TRACEMAP_PROGRESS(points) (((points) / (float)(TRACEMAP_SIZE * TRACEMAP_SIZE)) * 100.f)
#endif
void CG_GenerateTracemap( void ) {
	trace_t tr;
	vec3_t start, end;
	int i, j;
	float x_step, y_step;
	int topdownmin, topdownmax;
	int skygroundmin, skygroundmax;
	int min, max;
	float scalefactor;
	fileHandle_t f;
	byte data;
	static int lastDraw = 0;
	static int tracecount = 0;
	int ms;
#if defined(_MSC_VER) && defined(_M_IX86)
	int traceSquare;
	unsigned int traceZFlags;
	const float *traceMins = cg.mapcoordsMins, *traceMaxs = cg.mapcoordsMaxs;
	static const float traceGridScale = 0.00390625f;
	void *traceClear = &tracemap;
	int traceClearWords = sizeof(tracemap) / 4;
#endif

	if( !developer.integer ) {
		CG_Printf( "Can only generate a tracemap in developer mode.\n" );
		return;
	}

	if( !cg.mapcoordsValid ) {
		CG_Printf( "Need valid mapcoords in the worldspawn to be able to generate a tracemap.\n" );
		return;
	}
#if defined(_MSC_VER) && defined(_M_IX86)
	/* 3001594c: compare the two retained differences, including C3 unordered. */
	__asm {
		mov ecx, traceMaxs
		mov edx, traceMins
		fld dword ptr [ecx]
		fsub dword ptr [edx]
		fld dword ptr [edx + 4]
		fsub dword ptr [ecx + 4]
		fcompp
		fnstsw ax
		and eax, 4000h
		mov traceSquare, eax
	}
	if( !traceSquare ) {
#else
	if( (cg.mapcoordsMaxs[0] - cg.mapcoordsMins[0]) != (cg.mapcoordsMins[1] - cg.mapcoordsMaxs[1]) ) {
#endif
		CG_Printf( "Mapcoords need to be square.\n" );
		return;
	}

	// Topdown tracing
	CG_Printf( "Generating level heightmap and level mask...\n" );

#if !defined(_MSC_VER) || !defined(_M_IX86)
	memset( &tracemap, 0, sizeof(tracemap) );
#endif

	topdownmax = MIN_WORLD_HEIGHT;
	topdownmin = MAX_WORLD_HEIGHT;

	// calculate the size of the level
	// ok, i'm lazy. Hijack commandmap extends for now and default to a TRACEMAP_SIZE by TRACEMAP_SIZE datablock
#if defined(_MSC_VER) && defined(_M_IX86)
	__asm {
		mov ecx, traceMaxs
		mov edx, traceMins
		fld dword ptr [ecx]
		fsub dword ptr [edx]
		fmul traceGridScale
		mov edi, traceClear
		mov ecx, traceClearWords
		xor eax, eax
		rep stosd
		fstp x_step
		mov ecx, traceMaxs
		fld dword ptr [ecx + 4]
		fsub dword ptr [edx + 4]
		fmul traceGridScale
		fstp y_step
	}
#else
	x_step = ( cg.mapcoordsMaxs[0] - cg.mapcoordsMins[0] ) / (float)TRACEMAP_SIZE;
	y_step = ( cg.mapcoordsMaxs[1] - cg.mapcoordsMins[1] ) / (float)TRACEMAP_SIZE;
#endif

	for( i = 0; i < TRACEMAP_SIZE; i++ ) {
#if defined(_MSC_VER) && defined(_M_IX86)
		CG_TraceMapGridCoordinate( i, x_step, cg.mapcoordsMins[0], &start[0], &end[0] );
#else
		start[0] = end[0] = cg.mapcoordsMins[0] + i * x_step;
#endif
		for( j = 0; j < TRACEMAP_SIZE; j++ ) {
#if defined(_MSC_VER) && defined(_M_IX86)
		CG_TraceMapGridCoordinate( j, y_step, cg.mapcoordsMins[1], &start[1], &end[1] );
#else
		start[1] = end[1] = cg.mapcoordsMins[1] + j * y_step;
#endif
			start[2] = MAX_WORLD_HEIGHT;
			end[2] = MIN_WORLD_HEIGHT;

			// Find the ceiling
			CG_Trace( &tr, start, NULL, NULL, end, ENTITYNUM_NONE, MASK_SOLID|MASK_WATER );
#if defined(_MSC_VER) && defined(_M_IX86)
			traceZFlags = CG_TraceMapUpdateZ( tr.endpos[2], 1.0f, &start[2], MIN_WORLD_HEIGHT, 1 );
#else
			start[2] = tr.endpos[2] - 1;
#endif
			tracecount = (int)((unsigned int)tracecount + 1u);
            
			// Find ground
			while( 1 )
			{
				// Perform traces up to the sky, repeating at a higher start height if we start
				// inside a solid.

#if defined(_MSC_VER) && defined(_M_IX86)
				if( traceZFlags & 0x4100 ) {
#else
				if( start[2] <= MIN_WORLD_HEIGHT ) {
#endif
					tracemap.ground[j][i] = MIN_WORLD_HEIGHT;
					break;
				}
#if defined(_MSC_VER) && defined(_M_IX86)
				if( CG_TraceMapCompare( end[2], MIN_WORLD_HEIGHT ) & 0x4100 )
#else
				if( end[2] <= MIN_WORLD_HEIGHT )
#endif
					end[2] = MIN_WORLD_HEIGHT + 1;
				CG_Trace( &tr, start, NULL, NULL, end, ENTITYNUM_NONE, (MASK_SOLID|MASK_WATER) );
				tracecount = (int)((unsigned int)tracecount + 1u);
				if( tr.startsolid ) {			// Stuck in something, skip over it.
#if defined(_MSC_VER) && defined(_M_IX86)
					traceZFlags = CG_TraceMapUpdateZ( start[2], 64.0f, &start[2], MIN_WORLD_HEIGHT, 1 );
#else
					start[2] -= 64;
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
				} else if( CG_TraceMapCompare( tr.fraction, 1.0f ) & 0x4000 ) {
#else
				} else if( tr.fraction == 1 ) {
#endif		// Didn't hit anything, we're (probably) outside the world
					tracemap.ground[j][i] = MIN_WORLD_HEIGHT;
					break;
				} else {
#if defined(_MSC_VER) && defined(_M_IX86)
					CG_TraceMapStoreHeight( &tracemap.ground[j][i], &tr.endpos[2] );
#else
					tracemap.ground[j][i] = tr.endpos[2];
#endif
					if( !(tr.surfaceFlags & SURF_NODRAW) ) {
#if defined(_MSC_VER) && defined(_M_IX86)
						if( CG_TraceMapCompareInteger( topdownmax, tracemap.ground[j][i] ) & 0x100 )
#else
						if( tracemap.ground[j][i] > topdownmax )
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
							topdownmax = CG_TraceMapInteger( tracemap.ground[j][i] ); 
#else
							topdownmax = tracemap.ground[j][i]; 
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						if( !(CG_TraceMapCompareInteger( topdownmin, tracemap.ground[j][i] ) & 0x4100) )
#else
						if( tracemap.ground[j][i] < topdownmin )
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
							topdownmin = CG_TraceMapInteger( tracemap.ground[j][i] );
#else
							topdownmin = tracemap.ground[j][i];
#endif
					}
					break;
				}
			}

			// bit hacky, get some console output going
			ms = trap_Milliseconds();
			if( !((lastDraw <= ms) && (lastDraw > (int)((unsigned int)ms - 500u)))) {
				lastDraw = ms;

				CG_Printf( "%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE + j, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS( i * TRACEMAP_SIZE + j ), tracecount );
				trap_UpdateScreen();
			}
		}
	}
	CG_Printf( "%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS( i * TRACEMAP_SIZE ), tracecount );
	trap_UpdateScreen();

	// Sky tracing
	CG_Printf( "Generating sky heightmap and sky mask...\n" );

	max = MIN_WORLD_HEIGHT;
	min = MAX_WORLD_HEIGHT;

	for( i = 0; i < TRACEMAP_SIZE; i++ ) {
#if defined(_MSC_VER) && defined(_M_IX86)
		CG_TraceMapGridCoordinate( i, x_step, cg.mapcoordsMins[0], &start[0], &end[0] );
#else
		start[0] = end[0] = cg.mapcoordsMins[0] + i * x_step;
#endif
		for( j = 0; j < TRACEMAP_SIZE; j++ ) {
#if defined(_MSC_VER) && defined(_M_IX86)
		CG_TraceMapGridCoordinate( j, y_step, cg.mapcoordsMins[1], &start[1], &end[1] );
#else
		start[1] = end[1] = cg.mapcoordsMins[1] + j * y_step;
#endif
			//start[2] = MIN_WORLD_HEIGHT;
			start[2] = tracemap.ground[j][i];
			end[2] = MAX_WORLD_HEIGHT;

#if defined(_MSC_VER) && defined(_M_IX86)
			if( CG_TraceMapCompare( start[2], MIN_WORLD_HEIGHT ) & 0x4000 ) {
#else
			if( start[2] == MIN_WORLD_HEIGHT ) {
#endif
				// we got a hole here, no need to trace
				tracemap.sky[j][i] = MAX_WORLD_HEIGHT;
			} else {
				// Find sky
#if defined(_MSC_VER) && defined(_M_IX86)
				traceZFlags = CG_TraceMapCompare( start[2], MAX_WORLD_HEIGHT );
#endif
				while( 1 )
				{
					// Perform traces up to the sky, repeating at a higher start height if we start
					// inside a solid.

#if defined(_MSC_VER) && defined(_M_IX86)
					if( !(traceZFlags & 0x100) ) {
#else
					if( start[2] >= MAX_WORLD_HEIGHT ) {
#endif
						tracemap.sky[j][i] = MAX_WORLD_HEIGHT;
						break;
					}
#if defined(_MSC_VER) && defined(_M_IX86)
					if( !(CG_TraceMapCompare( end[2], MAX_WORLD_HEIGHT ) & 0x100) )
#else
					if( end[2] >= MAX_WORLD_HEIGHT )
#endif
						end[2] = MAX_WORLD_HEIGHT - 1;
					CG_Trace( &tr, start, NULL, NULL, end, ENTITYNUM_NONE, MASK_SOLID );
					tracecount = (int)((unsigned int)tracecount + 1u);
					if( tr.startsolid ) {			// Stuck in something, skip over it.
						// can happen, tr.endpos still is valid even if we're starting in a solid but trace out of it hitting the next surface
						if( tr.surfaceFlags & SURF_SKY ) {
							// are we in a solid?
							if( !(CG_PointContents( tr.endpos, ENTITYNUM_NONE ) & (MASK_SOLID|MASK_WATER)) ) {
#if defined(_MSC_VER) && defined(_M_IX86)
								CG_TraceMapStoreHeight( &tracemap.sky[j][i], &tr.endpos[2] );
#else
								tracemap.sky[j][i] = tr.endpos[2];
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
								if( CG_TraceMapCompareInteger( max, tracemap.sky[j][i] ) & 0x100 )
#else
								if( tracemap.sky[j][i] > max )
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
									max = CG_TraceMapInteger( tracemap.sky[j][i] ); 
#else
									max = tracemap.sky[j][i]; 
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
								if( !(CG_TraceMapCompareInteger( min, tracemap.sky[j][i] ) & 0x4100) )
#else
								if( tracemap.sky[j][i] < min )
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
									min = CG_TraceMapInteger( tracemap.sky[j][i] );
#else
									min = tracemap.sky[j][i];
#endif
								break;
							} else {
								// skip over it
#if defined(_MSC_VER) && defined(_M_IX86)
								traceZFlags = CG_TraceMapUpdateZ( tr.endpos[2], 1.0f, &start[2], MAX_WORLD_HEIGHT, 0 );
#else
								start[2] = tr.endpos[2] + 1;
#endif
							}
						} else {
#if defined(_MSC_VER) && defined(_M_IX86)
							traceZFlags = CG_TraceMapUpdateZ( tr.endpos[2], 1.0f, &start[2], MAX_WORLD_HEIGHT, 0 );
#else
							start[2] = tr.endpos[2] + 1;
#endif
						}
#if defined(_MSC_VER) && defined(_M_IX86)
					} else if( CG_TraceMapCompare( tr.fraction, 1.0f ) & 0x4000 ) {
#else
					} else if( tr.fraction == 1 ) {
#endif		// Didn't hit anything, we're (probably) outside the world
						tracemap.sky[j][i] = MAX_WORLD_HEIGHT;
						break;
					} else if( tr.surfaceFlags & SURF_SKY ) {	// Hit sky, this is where we start.
#if defined(_MSC_VER) && defined(_M_IX86)
						CG_TraceMapStoreHeight( &tracemap.sky[j][i], &tr.endpos[2] );
#else
						tracemap.sky[j][i] = tr.endpos[2];
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						if( CG_TraceMapCompareInteger( max, tracemap.sky[j][i] ) & 0x100 )
#else
						if( tracemap.sky[j][i] > max )
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
							max = CG_TraceMapInteger( tracemap.sky[j][i] ); 
#else
							max = tracemap.sky[j][i]; 
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						if( !(CG_TraceMapCompareInteger( min, tracemap.sky[j][i] ) & 0x4100) )
#else
						if( tracemap.sky[j][i] < min )
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
							min = CG_TraceMapInteger( tracemap.sky[j][i] );
#else
							min = tracemap.sky[j][i];
#endif
						break;
					} else {
						// hit something else, skip over it
#if defined(_MSC_VER) && defined(_M_IX86)
						traceZFlags = CG_TraceMapUpdateZ( tr.endpos[2], 64.0f, &start[2], MAX_WORLD_HEIGHT, 0 );
#else
						start[2] = tr.endpos[2] + 64;
#endif
					}
				}
			}

			// bit hacky, get some console output going
			ms = trap_Milliseconds();
			if( !((lastDraw <= ms) && (lastDraw > (int)((unsigned int)ms - 500u)))) {
				lastDraw = ms;

				CG_Printf( "%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE + j, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS( i * TRACEMAP_SIZE + j ), tracecount );
				trap_UpdateScreen();
			}
		}
	}
	CG_Printf( "%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS( i * TRACEMAP_SIZE ), tracecount );
	trap_UpdateScreen();

	// More groundtrace, find ceilings for areas where we don't have ground
	CG_Printf( "Generating sky groundmap...\n" );

	skygroundmin = MAX_WORLD_HEIGHT;
	skygroundmax = MIN_WORLD_HEIGHT;

	for( i = 0; i < TRACEMAP_SIZE; i++ ) {
#if defined(_MSC_VER) && defined(_M_IX86)
		CG_TraceMapGridCoordinate( i, x_step, cg.mapcoordsMins[0], &start[0], &end[0] );
#else
		start[0] = end[0] = cg.mapcoordsMins[0] + i * x_step;
#endif
		for( j = 0; j < TRACEMAP_SIZE; j++ ) {
#if defined(_MSC_VER) && defined(_M_IX86)
		CG_TraceMapGridCoordinate( j, y_step, cg.mapcoordsMins[1], &start[1], &end[1] );
#else
		start[1] = end[1] = cg.mapcoordsMins[1] + j * y_step;
#endif
			start[2] = MAX_WORLD_HEIGHT;
			end[2] = MIN_WORLD_HEIGHT;

#if defined(_MSC_VER) && defined(_M_IX86)
			if( CG_TraceMapBits( &tracemap.sky[j][i] ) == 0x47800000u && CG_TraceMapBits( &tracemap.ground[j][i] ) != 0xc7800000u ) {
#else
			if( tracemap.sky[j][i] == MAX_WORLD_HEIGHT && tracemap.ground[j][i] != MIN_WORLD_HEIGHT ) {
#endif
				// Find the ceiling
				CG_Trace( &tr, start, NULL, NULL, end, ENTITYNUM_NONE, MASK_SOLID|MASK_WATER );
				tracecount = (int)((unsigned int)tracecount + 1u);
#if defined(_MSC_VER) && defined(_M_IX86)
				if( CG_TraceMapCompare( tr.fraction, 1.0f ) & 0x4000 ) {
#else
				if( tr.fraction == 1 ) {
#endif		// Didn't hit anything, we're (probably) outside the world
					tracemap.skyground[j][i] = MIN_WORLD_HEIGHT;
				} else {
					
#if defined(_MSC_VER) && defined(_M_IX86)
				memcpy( &tracemap.skyground[j][i], &tr.endpos[2], sizeof(float) );
#else
				tracemap.skyground[j][i] = tr.endpos[2];
#endif
				}
			} else {
				
#if defined(_MSC_VER) && defined(_M_IX86)
				memcpy( &tracemap.skyground[j][i], &tracemap.ground[j][i], sizeof(float) );
#else
				tracemap.skyground[j][i] = tracemap.ground[j][i];
#endif
			}

#if defined(_MSC_VER) && defined(_M_IX86)
			if( CG_TraceMapBits( &tracemap.skyground[j][i] ) != 0xc7800000u ) {
#else
			if( tracemap.skyground[j][i] != MIN_WORLD_HEIGHT ) {
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
				if( CG_TraceMapCompareInteger( skygroundmax, tracemap.skyground[j][i] ) & 0x100 )
#else
				if( tracemap.skyground[j][i] > skygroundmax )
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
					skygroundmax = CG_TraceMapInteger( tracemap.skyground[j][i] ); 
#else
					skygroundmax = tracemap.skyground[j][i]; 
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
				if( !(CG_TraceMapCompareInteger( skygroundmin, tracemap.skyground[j][i] ) & 0x4100) )
#else
				if( tracemap.skyground[j][i] < skygroundmin )
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
					skygroundmin = CG_TraceMapInteger( tracemap.skyground[j][i] );
#else
					skygroundmin = tracemap.skyground[j][i];
#endif
			}

			// bit hacky, get some console output going
			ms = trap_Milliseconds();
			if( !((lastDraw <= ms) && (lastDraw > (int)((unsigned int)ms - 500u)))) {
				lastDraw = ms;

				CG_Printf( "%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE + j, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS( i * TRACEMAP_SIZE + j ), tracecount );
				trap_UpdateScreen();
			}
		}
	}
	CG_Printf( "%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS( i * TRACEMAP_SIZE ), tracecount );
	trap_UpdateScreen();

	// R: topdown mask
	// G: there is sky here yes/no mask
	// B: sky mask
	// A: there is map here yes/no mask

	// scale everything to a colour 256 range

	// min is 0
	// max is 255
	// rain - etmain REALLY expects 1 to 255, so I'm changing this to
	// generate that instead, so that etpro tracemaps can be used with
	// etmain
#if defined(_MSC_VER) && defined(_M_IX86)
	CG_TraceMapNormalizePlane( &tracemap.ground[0][0], topdownmin, topdownmax, 0 );
	CG_TraceMapNormalizePlane( &tracemap.skyground[0][0], skygroundmin, skygroundmax, 0 );
	CG_TraceMapNormalizePlane( &tracemap.sky[0][0], min, max, 1 );
#else
	scalefactor = 254.f / ( topdownmax - topdownmin );
	if( scalefactor == 0.f )
		scalefactor = 1.f;
	for( i = 0; i < TRACEMAP_SIZE; i++ ) {
		for( j = 0; j < TRACEMAP_SIZE; j++ ) {
			if( tracemap.ground[i][j] >= topdownmin )
				tracemap.ground[i][j] = 1.0 + (tracemap.ground[i][j] - topdownmin) * scalefactor;
			
			// rain - hard clamp because *min and *max are rounded :(
			if (tracemap.ground[i][j] < 1.0)
				tracemap.ground[i][j] = 1.0;
			else if (tracemap.ground[i][j] > 255.0)
				tracemap.ground[i][j] = 255.0;
		}
	}

	// min is 0
	// max is 255
	// rain - this is d&l, min=1 max=255
	scalefactor = 254.f / ( skygroundmax - skygroundmin );
	if( scalefactor == 0.f )
		scalefactor = 1.f;
	for( i = 0; i < TRACEMAP_SIZE; i++ ) {
		for( j = 0; j < TRACEMAP_SIZE; j++ ) {
			if( tracemap.skyground[i][j] >= skygroundmin )
				tracemap.skyground[i][j] = 1.0 + (tracemap.skyground[i][j] - skygroundmin) * scalefactor;

			// rain - hard clamp because *min and *max are rounded :(
			if (tracemap.skyground[i][j] < 1.0)
				tracemap.skyground[i][j] = 1.0;
			else if (tracemap.skyground[i][j] > 255.0)
				tracemap.skyground[i][j] = 255.0;
		}
	}

	// no sky is 0
	// min is 1
	// max is 255
	if( max - min == 0 )
		scalefactor = 1.f;
	else {
		scalefactor = 254.f / ( max - min );
	}
	for( i = 0; i < TRACEMAP_SIZE; i++ ) {
		for( j = 0; j < TRACEMAP_SIZE; j++ ) {
			if( tracemap.sky[i][j] == MAX_WORLD_HEIGHT ) {
				tracemap.sky[i][j] = 0.f;
			} else {
				tracemap.sky[i][j] = 1.f + (tracemap.sky[i][j] - min) * scalefactor;
			}

			// rain - hard clamp because *min and *max are rounded :(
			if (tracemap.sky[i][j] < 0.0)
				tracemap.sky[i][j] = 0.0;
			else if (tracemap.sky[i][j] > 255.0)
				tracemap.sky[i][j] = 255.0;
		}
	}
#endif
	// write tga
	trap_FS_FOpenFile( va( "maps/%s_tracemap.tga", Q_strlwr(cgs.rawmapname) ), &f, FS_WRITE );

	// header
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 0
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 1
	data = 2; trap_FS_Write( &data, sizeof(data), f );	// 2 : uncompressed type
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 3
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 4
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 5
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 6
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 7
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 8
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 9
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 10
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 11
	data = TRACEMAP_SIZE & 255; trap_FS_Write( &data, sizeof(data), f );	// 12 : width
	data = TRACEMAP_SIZE >> 8; trap_FS_Write( &data, sizeof(data), f );	// 13 : width
	data = TRACEMAP_SIZE & 255; trap_FS_Write( &data, sizeof(data), f );	// 14 : height
	data = TRACEMAP_SIZE >> 8; trap_FS_Write( &data, sizeof(data), f );	// 15 : height
	data = 32; trap_FS_Write( &data, sizeof(data), f );	// 16 : pixel size
	data = 0; trap_FS_Write( &data, sizeof(data), f );	// 17

	// R: topdown mask
	// G: there is sky here yes/no mask
	// B: sky mask
	// A: there is map here yes/no mask
	for( i = 0; i < TRACEMAP_SIZE; i++ ) {
		for( j = 0; j < TRACEMAP_SIZE; j++ ) {
			if( i == 0 && j < 6 ) {
				// abuse first six pixels for our extended data
				switch( j ) {
					case 0:	trap_FS_Write( &topdownmin, sizeof(topdownmin), f ); break;
					case 1: trap_FS_Write( &topdownmax, sizeof(topdownmax), f ); break;
					case 2:	trap_FS_Write( &skygroundmin, sizeof(skygroundmin), f ); break;
					case 3: trap_FS_Write( &skygroundmax, sizeof(skygroundmax), f ); break;
					case 4: trap_FS_Write( &min, sizeof(min), f ); break;
					case 5: trap_FS_Write( &max, sizeof(max), f ); break;
				}
				continue;
			}
#if defined(_MSC_VER) && defined(_M_IX86)

			data = CG_TraceMapInteger( tracemap.sky[TRACEMAP_SIZE - 1 - i][j] ); trap_FS_Write( &data, sizeof(data), f );	// b
#else

			data = tracemap.sky[TRACEMAP_SIZE - 1 - i][j]; trap_FS_Write( &data, sizeof(data), f );	// b
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
			if( CG_TraceMapBits( &tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] ) == 0xc7800000u ) {
#else
			if( tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] == MIN_WORLD_HEIGHT ) {
#endif
				data = 0; trap_FS_Write( &data, sizeof(data), f );	// g
			} else {
#if defined(_MSC_VER) && defined(_M_IX86)
				data = CG_TraceMapInteger( tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] ); trap_FS_Write( &data, sizeof(data), f );	// g
#else
				data = tracemap.skyground[TRACEMAP_SIZE - 1 - i][j]; trap_FS_Write( &data, sizeof(data), f );	// g
#endif
			}
#if defined(_MSC_VER) && defined(_M_IX86)
			if( CG_TraceMapBits( &tracemap.ground[TRACEMAP_SIZE - 1 - i][j] ) == 0xc7800000u ) {
#else
			if( tracemap.ground[TRACEMAP_SIZE - 1 - i][j] == MIN_WORLD_HEIGHT ) {
#endif
				data = 0; trap_FS_Write( &data, sizeof(data), f );	// r
				data = 0; trap_FS_Write( &data, sizeof(data), f );	// a
			} else {
#if defined(_MSC_VER) && defined(_M_IX86)
				data = CG_TraceMapInteger( tracemap.ground[TRACEMAP_SIZE - 1 - i][j] ); trap_FS_Write( &data, sizeof(data), f );	// r
#else
				data = tracemap.ground[TRACEMAP_SIZE - 1 - i][j]; trap_FS_Write( &data, sizeof(data), f );	// r
#endif
				data = 255; trap_FS_Write( &data, sizeof(data), f );	// a
			}
		}
	}

	// footer
	i = 0; trap_FS_Write( &i, sizeof(i), f );	// extension area offset, 4 bytes
	i = 0; trap_FS_Write( &i, sizeof(i), f );	// developer directory offset, 4 bytes
	trap_FS_Write( "TRUEVISION-XFILE.\0", 18, f );

	trap_FS_FCloseFile( f );
}

#undef CG_TRACEMAP_PROGRESS
#endif // CGAMEDLL

#if defined(CGAMEDLL) && defined(_MSC_VER) && defined(_M_IX86)
/* Complete Windows30016690; all data addresses use the native tracemap. */
__declspec(naked) qboolean BG_LoadTraceMap(char *rawmapname, vec2_t world_mins, vec2_t world_maxs) {
    __asm {
tl_30016690:
        SUB ESP,0x418
tl_30016696:
        MOV ECX,dword ptr [ESP + 0x41c]
tl_3001669d:
        PUSH EBX
tl_3001669e:
        MOV EAX,0xffff0000
tl_300166a3:
        PUSH EBP
tl_300166a4:
        PUSH ESI
tl_300166a5:
        MOV dword ptr [ESP + 0x14],EAX
tl_300166a9:
        MOV dword ptr [ESP + 0x18],EAX
tl_300166ad:
        PUSH EDI
tl_300166ae:
        MOV ESI,0x10000
tl_300166b3:
        LEA EAX,[ESP + 0x20]
tl_300166b7:
        PUSH 0x0
tl_300166b9:
        MOV EDI,ESI
tl_300166bb:
        MOV EBP,ESI
tl_300166bd:
        PUSH EAX
tl_300166be:
        PUSH ECX
tl_300166bf:
        MOV dword ptr [ESP + 0x1c],EDI
tl_300166c3:
        MOV EBX,ESI
tl_300166c5:
        MOV dword ptr [ESP + 0x20],EBP
tl_300166c9:
        CALL Q_strlwr
tl_300166ce:
        PUSH EAX
tl_300166cf:
        PUSH OFFSET traceLoadPath
tl_300166d4:
        CALL va
tl_300166d9:
        ADD ESP,0xc
tl_300166dc:
        PUSH EAX
tl_300166dd:
        CALL trap_FS_FOpenFile
tl_300166e2:
        ADD ESP,0xc
tl_300166e5:
        TEST EAX,EAX
tl_300166e7:
        JL tl_30016b1f
tl_300166ed:
        MOV dword ptr [ESP + 0x10],0x12
tl_300166f5:
        MOV EDX,dword ptr [ESP + 0x20]
tl_300166f9:
        LEA EAX,[ESP + 0x27]
tl_300166fd:
        PUSH EDX
tl_300166fe:
        PUSH 0x1
tl_30016700:
        PUSH EAX
tl_30016701:
        CALL trap_FS_Read
tl_30016706:
        MOV EAX,dword ptr [ESP + 0x1c]
tl_3001670a:
        ADD ESP,0xc
tl_3001670d:
        DEC EAX
tl_3001670e:
        MOV dword ptr [ESP + 0x10],EAX
tl_30016712:
        JNZ tl_300166f5
tl_30016714:
        MOV dword ptr [ESP + 0x10],0x0
tl_3001671c:
        MOV ECX,dword ptr [ESP + 0x20]
tl_30016720:
        LEA EDX,[ESP + 0x28]
tl_30016724:
        PUSH ECX
tl_30016725:
        PUSH 0x400
tl_3001672a:
        PUSH EDX
tl_3001672b:
        CALL trap_FS_Read
tl_30016730:
        ADD ESP,0xc
tl_30016733:
        XOR EDX,EDX
tl_30016735:
        MOV EAX,dword ptr [ESP + 0x10]
tl_30016739:
        TEST EAX,EAX
tl_3001673b:
        JNZ tl_300168da
tl_30016741:
        CMP EDX,0x6
tl_30016744:
        JGE tl_300168da
tl_3001674a:
        CMP EDX,0x5
tl_3001674d:
        JA tl_300168b7
tl_30016753:
        CMP EDX,0
        JE tl_3001675a
        CMP EDX,1
        JE tl_30016799
        CMP EDX,2
        JE tl_300167d8
        CMP EDX,3
        JE tl_30016812
        CMP EDX,4
        JE tl_3001684a
        CMP EDX,5
        JE tl_30016881
        JMP tl_300168b7
tl_3001675a:
        MOV EAX,dword ptr [ESP + 0x2b]
tl_3001675e:
        MOV ECX,dword ptr [ESP + 0x2a]
tl_30016762:
        AND EAX,0xff
tl_30016767:
        AND ECX,0xff
tl_3001676d:
        SHL EAX,0x8
tl_30016770:
        OR EAX,ECX
tl_30016772:
        MOV ECX,dword ptr [ESP + 0x29]
tl_30016776:
        SHL EAX,0x8
tl_30016779:
        AND ECX,0xff
tl_3001677f:
        OR EAX,ECX
tl_30016781:
        MOV ECX,dword ptr [ESP + 0x28]
tl_30016785:
        SHL EAX,0x8
tl_30016788:
        AND ECX,0xff
tl_3001678e:
        OR EAX,ECX
tl_30016790:
        MOV dword ptr [ESP + 0x1c],EAX
tl_30016794:
        JMP tl_300168b7
tl_30016799:
        MOV EAX,dword ptr [ESP + 0x2f]
tl_3001679d:
        MOV ECX,dword ptr [ESP + 0x2e]
tl_300167a1:
        AND EAX,0xff
tl_300167a6:
        AND ECX,0xff
tl_300167ac:
        SHL EAX,0x8
tl_300167af:
        OR EAX,ECX
tl_300167b1:
        MOV ECX,dword ptr [ESP + 0x2d]
tl_300167b5:
        SHL EAX,0x8
tl_300167b8:
        AND ECX,0xff
tl_300167be:
        OR EAX,ECX
tl_300167c0:
        MOV ECX,dword ptr [ESP + 0x2c]
tl_300167c4:
        SHL EAX,0x8
tl_300167c7:
        AND ECX,0xff
tl_300167cd:
        OR EAX,ECX
tl_300167cf:
        MOV dword ptr [ESP + 0x18],EAX
tl_300167d3:
        JMP tl_300168b7
tl_300167d8:
        MOV EDI,dword ptr [ESP + 0x33]
tl_300167dc:
        MOV EAX,dword ptr [ESP + 0x32]
tl_300167e0:
        MOV ECX,dword ptr [ESP + 0x31]
tl_300167e4:
        AND EDI,0xff
tl_300167ea:
        SHL EDI,0x8
tl_300167ed:
        AND EAX,0xff
tl_300167f2:
        AND ECX,0xff
tl_300167f8:
        OR EDI,EAX
tl_300167fa:
        MOV EAX,dword ptr [ESP + 0x30]
tl_300167fe:
        SHL EDI,0x8
tl_30016801:
        OR EDI,ECX
tl_30016803:
        AND EAX,0xff
tl_30016808:
        SHL EDI,0x8
tl_3001680b:
        OR EDI,EAX
tl_3001680d:
        JMP tl_300168b7
tl_30016812:
        MOV ESI,dword ptr [ESP + 0x37]
tl_30016816:
        MOV ECX,dword ptr [ESP + 0x36]
tl_3001681a:
        MOV EAX,dword ptr [ESP + 0x35]
tl_3001681e:
        AND ESI,0xff
tl_30016824:
        SHL ESI,0x8
tl_30016827:
        AND ECX,0xff
tl_3001682d:
        AND EAX,0xff
tl_30016832:
        OR ESI,ECX
tl_30016834:
        MOV ECX,dword ptr [ESP + 0x34]
tl_30016838:
        SHL ESI,0x8
tl_3001683b:
        OR ESI,EAX
tl_3001683d:
        AND ECX,0xff
tl_30016843:
        SHL ESI,0x8
tl_30016846:
        OR ESI,ECX
tl_30016848:
        JMP tl_300168b7
tl_3001684a:
        MOV EBP,dword ptr [ESP + 0x3b]
tl_3001684e:
        MOV EAX,dword ptr [ESP + 0x3a]
tl_30016852:
        MOV ECX,dword ptr [ESP + 0x39]
tl_30016856:
        AND EBP,0xff
tl_3001685c:
        SHL EBP,0x8
tl_3001685f:
        AND EAX,0xff
tl_30016864:
        AND ECX,0xff
tl_3001686a:
        OR EBP,EAX
tl_3001686c:
        MOV EAX,dword ptr [ESP + 0x38]
tl_30016870:
        SHL EBP,0x8
tl_30016873:
        OR EBP,ECX
tl_30016875:
        AND EAX,0xff
tl_3001687a:
        SHL EBP,0x8
tl_3001687d:
        OR EBP,EAX
tl_3001687f:
        JMP tl_300168b7
tl_30016881:
        MOV EBX,dword ptr [ESP + 0x3f]
tl_30016885:
        MOV ECX,dword ptr [ESP + 0x3e]
tl_30016889:
        MOV EAX,dword ptr [ESP + 0x3d]
tl_3001688d:
        AND EBX,0xff
tl_30016893:
        SHL EBX,0x8
tl_30016896:
        AND ECX,0xff
tl_3001689c:
        AND EAX,0xff
tl_300168a1:
        OR EBX,ECX
tl_300168a3:
        MOV ECX,dword ptr [ESP + 0x3c]
tl_300168a7:
        SHL EBX,0x8
tl_300168aa:
        OR EBX,EAX
tl_300168ac:
        AND ECX,0xff
tl_300168b2:
        SHL EBX,0x8
tl_300168b5:
        OR EBX,ECX
tl_300168b7:
        MOV EAX,0x47800000
tl_300168bc:
        MOV dword ptr [EDX*0x4 + tracemap + TraceLoadSkyLast],EAX
tl_300168c3:
        MOV dword ptr [EDX*0x4 + tracemap + TraceLoadSkyGroundLast],EAX
tl_300168ca:
        MOV dword ptr [EDX*0x4 + tracemap + TraceLoadGroundLast],0xc7800000
tl_300168d5:
        JMP tl_30016981
tl_300168da:
        XOR ECX,ECX
tl_300168dc:
        MOV CL,byte ptr [ESP + EDX*0x4 + 0x28]
tl_300168e0:
        MOV dword ptr [ESP + 0x14],ECX
tl_300168e4:
        MOV ECX,EDX
tl_300168e6:
        FILD dword ptr [ESP + 0x14]
tl_300168ea:
        SHL EAX,0x8
tl_300168ed:
        SUB ECX,EAX
tl_300168ef:
        SHL ECX,0x2
tl_300168f2:
        FST dword ptr [ECX + tracemap + TraceLoadSkyLast]
tl_300168f8:
        FCOMP dword ptr [traceLoadZero]
tl_300168fe:
        FNSTSW AX
tl_30016900:
        TEST AH,0x40
tl_30016903:
        JZ tl_3001690f
tl_30016905:
        MOV dword ptr [ECX + tracemap + TraceLoadSkyLast],0x47800000
tl_3001690f:
        XOR EAX,EAX
tl_30016911:
        MOV AL,byte ptr [ESP + EDX*0x4 + 0x29]
tl_30016915:
        MOV dword ptr [ESP + 0x14],EAX
tl_30016919:
        FILD dword ptr [ESP + 0x14]
tl_3001691d:
        FST dword ptr [ECX + tracemap + TraceLoadSkyGroundLast]
tl_30016923:
        FCOMP dword ptr [traceLoadZero]
tl_30016929:
        FNSTSW AX
tl_3001692b:
        TEST AH,0x40
tl_3001692e:
        JZ tl_3001693a
tl_30016930:
        MOV dword ptr [ECX + tracemap + TraceLoadSkyGroundLast],0x47800000
tl_3001693a:
        XOR EAX,EAX
tl_3001693c:
        MOV AL,byte ptr [ESP + EDX*0x4 + 0x2a]
tl_30016940:
        MOV dword ptr [ESP + 0x14],EAX
tl_30016944:
        FILD dword ptr [ESP + 0x14]
tl_30016948:
        FST dword ptr [ECX + tracemap + TraceLoadGroundLast]
tl_3001694e:
        FCOMP dword ptr [traceLoadZero]
tl_30016954:
        FNSTSW AX
tl_30016956:
        TEST AH,0x40
tl_30016959:
        JZ tl_30016965
tl_3001695b:
        MOV dword ptr [ECX + tracemap + TraceLoadGroundLast],0xc7800000
tl_30016965:
        MOV AL,byte ptr [ESP + EDX*0x4 + 0x2b]
tl_30016969:
        TEST AL,AL
tl_3001696b:
        JNZ tl_30016981
tl_3001696d:
        MOV dword ptr [ECX + tracemap + TraceLoadSkyGroundLast],0x47800000
tl_30016977:
        MOV dword ptr [ECX + tracemap + TraceLoadGroundLast],0xc7800000
tl_30016981:
        INC EDX
tl_30016982:
        CMP EDX,0x100
tl_30016988:
        JL tl_30016735
tl_3001698e:
        MOV EAX,dword ptr [ESP + 0x10]
tl_30016992:
        INC EAX
tl_30016993:
        CMP EAX,0x100
tl_30016998:
        MOV dword ptr [ESP + 0x10],EAX
tl_3001699c:
        JL tl_3001671c
tl_300169a2:
        MOV ECX,dword ptr [ESP + 0x20]
tl_300169a6:
        MOV dword ptr [ESP + 0x14],EBP
tl_300169aa:
        PUSH ECX
tl_300169ab:
        MOV dword ptr [ESP + 0x14],EDI
tl_300169af:
        CALL trap_FS_FCloseFile
tl_300169b4:
        MOV EDX,dword ptr [ESP + 0x1c]
tl_300169b8:
        MOV ECX,dword ptr [ESP + 0x20]
tl_300169bc:
        MOV EAX,EDX
tl_300169be:
        ADD ESP,0x4
tl_300169c1:
        SUB EAX,ECX
tl_300169c3:
        MOV dword ptr [ESP + 0x18],EAX
tl_300169c7:
        JNZ tl_300169d1
tl_300169c9:
        FLD dword ptr [traceLoadOne]
tl_300169cf:
        JMP tl_300169db
tl_300169d1:
        FILD dword ptr [ESP + 0x18]
tl_300169d5:
        FDIVR dword ptr [traceLoadScale]
tl_300169db:
        MOV EAX,OFFSET tracemap + TraceLoadGround
tl_300169e0:
        MOV ECX,0x100
tl_300169e5:
        CMP dword ptr [EAX],0xc7800000
tl_300169eb:
        JZ tl_300169f7
tl_300169ed:
        FLD dword ptr [EAX]
tl_300169ef:
        FDIV ST(0),ST(1)
tl_300169f1:
        FIADD dword ptr [ESP + 0x1c]
tl_300169f5:
        FSTP dword ptr [EAX]
tl_300169f7:
        ADD EAX,0x4
tl_300169fa:
        DEC ECX
tl_300169fb:
        JNZ tl_300169e5
tl_300169fd:
        CMP EAX,OFFSET tracemap + TraceQueryMinX
tl_30016a02:
        JL tl_300169e0
tl_30016a04:
        SUB ESI,EDI
tl_30016a06:
        FSTP ST(0)
tl_30016a08:
        MOV dword ptr [ESP + 0x18],ESI
tl_30016a0c:
        JNZ tl_30016a16
tl_30016a0e:
        FLD dword ptr [traceLoadOne]
tl_30016a14:
        JMP tl_30016a20
tl_30016a16:
        FILD dword ptr [ESP + 0x18]
tl_30016a1a:
        FDIVR dword ptr [traceLoadScale]
tl_30016a20:
        MOV EAX,OFFSET tracemap + TraceQuerySkyGround
tl_30016a25:
        MOV ECX,0x100
tl_30016a2a:
        CMP dword ptr [EAX],0x47800000
tl_30016a30:
        JZ tl_30016a3c
tl_30016a32:
        FLD dword ptr [EAX]
tl_30016a34:
        FDIV ST(0),ST(1)
tl_30016a36:
        FIADD dword ptr [ESP + 0x10]
tl_30016a3a:
        FSTP dword ptr [EAX]
tl_30016a3c:
        ADD EAX,0x4
tl_30016a3f:
        DEC ECX
tl_30016a40:
        JNZ tl_30016a2a
tl_30016a42:
        CMP EAX,OFFSET tracemap + TraceLoadGround
tl_30016a47:
        JL tl_30016a25
tl_30016a49:
        SUB EBX,EBP
tl_30016a4b:
        FSTP ST(0)
tl_30016a4d:
        MOV dword ptr [ESP + 0x18],EBX
tl_30016a51:
        JNZ tl_30016a5b
tl_30016a53:
        FLD dword ptr [traceLoadOne]
tl_30016a59:
        JMP tl_30016a65
tl_30016a5b:
        FILD dword ptr [ESP + 0x18]
tl_30016a5f:
        FDIVR dword ptr [traceLoadScale]
tl_30016a65:
        MOV EAX,OFFSET tracemap + TraceQuerySky
tl_30016a6a:
        MOV ECX,0x100
tl_30016a6f:
        CMP dword ptr [EAX],0x47800000
tl_30016a75:
        JZ tl_30016a81
tl_30016a77:
        FLD dword ptr [EAX]
tl_30016a79:
        FDIV ST(0),ST(1)
tl_30016a7b:
        FIADD dword ptr [ESP + 0x14]
tl_30016a7f:
        FSTP dword ptr [EAX]
tl_30016a81:
        ADD EAX,0x4
tl_30016a84:
        DEC ECX
tl_30016a85:
        JNZ tl_30016a6f
tl_30016a87:
        CMP EAX,OFFSET tracemap + TraceQuerySkyGround
tl_30016a8c:
        JL tl_30016a6a
tl_30016a8e:
        MOV EAX,dword ptr [ESP + 0x430]
tl_30016a95:
        POP EDI
tl_30016a96:
        FSTP ST(0)
tl_30016a98:
        MOV ECX,dword ptr [EAX]
tl_30016a9a:
        POP ESI
tl_30016a9b:
        MOV dword ptr [tracemap + TraceQueryMinX],ECX
tl_30016aa1:
        MOV EAX,dword ptr [EAX + 0x4]
tl_30016aa4:
        MOV dword ptr [tracemap + TraceQueryMinY],EAX
tl_30016aa9:
        MOV EAX,dword ptr [ESP + 0x42c]
tl_30016ab0:
        POP EBP
tl_30016ab1:
        POP EBX
tl_30016ab2:
        MOV ECX,dword ptr [EAX]
tl_30016ab4:
        MOV dword ptr [tracemap + TraceQueryMaxX],ECX
tl_30016aba:
        MOV EAX,dword ptr [EAX + 0x4]
tl_30016abd:
        FLD dword ptr [tracemap + TraceQueryMaxX]
tl_30016ac3:
        FSUB dword ptr [tracemap + TraceQueryMinX]
tl_30016ac9:
        MOV dword ptr [tracemap + TraceQueryMaxY],EAX
tl_30016ace:
        MOV ECX,dword ptr [ESP + 0xc]
tl_30016ad2:
        MOV EAX,0x1
tl_30016ad7:
        MOV dword ptr [tracemap + TraceLoadFloor],ECX
tl_30016add:
        FMUL dword ptr [traceLoadInverseSize]
tl_30016ae3:
        MOV dword ptr [tracemap + TraceLoadCeil],EDX
tl_30016ae9:
        MOV dword ptr [tracemap + TraceQueryLoaded],EAX
tl_30016aee:
        FDIVR dword ptr [traceLoadOne]
tl_30016af4:
        FSTP dword ptr [one_over_mapgrid_factor]
tl_30016afa:
        FLD dword ptr [tracemap + TraceQueryMaxY]
tl_30016b00:
        FSUB dword ptr [tracemap + TraceQueryMinY]
tl_30016b06:
        FMUL dword ptr [traceLoadInverseSize]
tl_30016b0c:
        FDIVR dword ptr [traceLoadOne]
tl_30016b12:
        FSTP dword ptr [one_over_mapgrid_factor + 4]
tl_30016b18:
        ADD ESP,0x418
tl_30016b1e:
        RET
tl_30016b1f:
        POP EDI
tl_30016b20:
        POP ESI
tl_30016b21:
        POP EBP
tl_30016b22:
        MOV dword ptr [tracemap + TraceQueryLoaded],0x0
tl_30016b2c:
        XOR EAX,EAX
tl_30016b2e:
        POP EBX
tl_30016b2f:
        ADD ESP,0x418
tl_30016b35:
        RET
    }
}
#else

qboolean BG_LoadTraceMap( char *rawmapname, vec2_t world_mins, vec2_t world_maxs ) {
	int i, j;
	fileHandle_t f;
	byte data, datablock[TRACEMAP_SIZE][4];
	int sky_min, sky_max;
	int ground_min, ground_max;
	int skyground_min, skyground_max;
	float scalefactor;
	//int startTime = trap_Milliseconds();

	ground_min = ground_max = MIN_WORLD_HEIGHT;
	skyground_min = skyground_max = MAX_WORLD_HEIGHT;
	sky_min = sky_max = MAX_WORLD_HEIGHT;

	if( trap_FS_FOpenFile( va( "maps/%s_tracemap.tga", Q_strlwr(rawmapname) ), &f, FS_READ ) >= 0 ) {
		// skip over header
		for( i = 0; i < 18; i++ ) {
			trap_FS_Read( &data, 1, f );
		}

		for( i = 0; i < TRACEMAP_SIZE; i++ ) {
			trap_FS_Read( &datablock, sizeof(datablock), f );	// TRACEMAP_SIZE * { b g r a }

			for( j = 0; j < TRACEMAP_SIZE; j++ ) {
				if( i == 0 && j < 6 ) {
					// abuse first six pixels for our extended data
					switch( j ) {
						case 0:	ground_min = datablock[j][0] | ( datablock[j][1] << 8 ) | ( datablock[j][2] << 16 ) | ( datablock[j][3] << 24 ); break;
						case 1:	ground_max = datablock[j][0] | ( datablock[j][1] << 8 ) | ( datablock[j][2] << 16 ) | ( datablock[j][3] << 24 ); break;
						case 2:	skyground_min = datablock[j][0] | ( datablock[j][1] << 8 ) | ( datablock[j][2] << 16 ) | ( datablock[j][3] << 24 ); break;
						case 3:	skyground_max = datablock[j][0] | ( datablock[j][1] << 8 ) | ( datablock[j][2] << 16 ) | ( datablock[j][3] << 24 ); break;
						case 4:	sky_min = datablock[j][0] | ( datablock[j][1] << 8 ) | ( datablock[j][2] << 16 ) | ( datablock[j][3] << 24 ); break;
						case 5:	sky_max = datablock[j][0] | ( datablock[j][1] << 8 ) | ( datablock[j][2] << 16 ) | ( datablock[j][3] << 24 ); break;
					}
					tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = MIN_WORLD_HEIGHT;
					continue;
				}

				tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[j][0];	// FIXME: swap
				if( tracemap.sky[TRACEMAP_SIZE - 1 - i][j] == 0 )
					tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;

				tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[j][1];	// FIXME: swap
				if( tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] == 0 )
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;

				tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[j][2];	// FIXME: swap
				if( tracemap.ground[TRACEMAP_SIZE - 1 - i][j] == 0 )
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = MIN_WORLD_HEIGHT;

				if( datablock[j][3] == 0 ) {
					// just in case
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = MIN_WORLD_HEIGHT;
				}
			}

			/*for( j = 0; j < TRACEMAP_SIZE; j++ ) {
				if( i == 0 && j < 6 ) {
					// abuse first six pixels for our extended data
					switch( j ) {
						case 0:	trap_FS_Read( &ground_min, sizeof(ground_min), f ); break;
						case 1: trap_FS_Read( &ground_max, sizeof(ground_max), f ); break;
						case 2:	trap_FS_Read( &skyground_min, sizeof(skyground_min), f ); break;
						case 3: trap_FS_Read( &skyground_max, sizeof(skyground_max), f ); break;
						case 4: trap_FS_Read( &sky_min, sizeof(sky_min), f ); break;
						case 5: trap_FS_Read( &sky_max, sizeof(sky_max), f ); break;
					}
					tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = MIN_WORLD_HEIGHT;
					continue;
				}

				trap_FS_Read( &datablock, sizeof(datablock), f );	// b g r a
				tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[0];	// FIXME: swap
				if( tracemap.sky[TRACEMAP_SIZE - 1 - i][j] == 0 )
					tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;

				//trap_FS_Read( &data, 1, f ); // g
				tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[1];	// FIXME: swap
				if( tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] == 0 )
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;

				//trap_FS_Read( &data, sizeof(data), f );	// r
				tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[2];	// FIXME: swap
				if( tracemap.ground[TRACEMAP_SIZE - 1 - i][j] == 0 )
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = MIN_WORLD_HEIGHT;

				//trap_FS_Read( &data, sizeof(data), f ); // a
				if( datablock[3] == 0 ) {
					// just in case
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = MIN_WORLD_HEIGHT;
				}
			}*/
		}

		trap_FS_FCloseFile( f );

		// Ground
		// calculate scalefactor
		if( ground_max - ground_min == 0 )
			scalefactor = 1.f;
		else {
			// rain - scalefactor 254 to compensate for broken etmain behavior
			scalefactor = 254.f / ( ground_max - ground_min );
		}

		// scale properly
		for( i = 0; i < TRACEMAP_SIZE; i++ ) {
			for( j = 0; j < TRACEMAP_SIZE; j++ ) {
				if( tracemap.ground[i][j] != MIN_WORLD_HEIGHT ) {
					tracemap.ground[i][j] = ground_min + ( tracemap.ground[i][j] / scalefactor );
				}
			}
		}

		// SkyGround
		// calculate scalefactor
		if( skyground_max - skyground_min == 0 )
			scalefactor = 1.f;
		else {
			// rain - scalefactor 254 to compensate for broken etmain behavior
			scalefactor = 254.f / ( skyground_max - skyground_min );
		}

		// scale properly
		for( i = 0; i < TRACEMAP_SIZE; i++ ) {
			for( j = 0; j < TRACEMAP_SIZE; j++ ) {
				if( tracemap.skyground[i][j] != MAX_WORLD_HEIGHT ) {
					tracemap.skyground[i][j] = skyground_min + ( tracemap.skyground[i][j] / scalefactor );
				}
			}
		}

		// Sky
		// calculate scalefactor
		if( sky_max - sky_min == 0 )
			scalefactor = 1.f;
		else {
			// rain - scalefactor 254 to compensate for broken etmain behavior
			scalefactor = 254.f / ( sky_max - sky_min );
		}

		// scale properly
		for( i = 0; i < TRACEMAP_SIZE; i++ ) {
			for( j = 0; j < TRACEMAP_SIZE; j++ ) {
				if( tracemap.sky[i][j] != MAX_WORLD_HEIGHT ) {
					tracemap.sky[i][j] = sky_min + ( tracemap.sky[i][j] / scalefactor );
				}
			}
		}
	} else {
		return( tracemap.loaded = qfalse );
	}

	tracemap.world_mins[0] = world_mins[0];
	tracemap.world_mins[1] = world_mins[1];
	tracemap.world_maxs[0] = world_maxs[0];
	tracemap.world_maxs[1] = world_maxs[1];

	one_over_mapgrid_factor[0] = 1.f / (( tracemap.world_maxs[0] - tracemap.world_mins[0] ) / (float)TRACEMAP_SIZE);
	one_over_mapgrid_factor[1] = 1.f / (( tracemap.world_maxs[1] - tracemap.world_mins[1] ) / (float)TRACEMAP_SIZE);

	tracemap.groundfloor = ground_min;
	tracemap.groundceil = ground_max;

	//Com_Printf( "^8Loaded tracemap in %i msec\n", trap_Milliseconds() - startTime );

	return( tracemap.loaded = qtrue );
}


#endif

#if defined(CGAMEDLL) && defined(_MSC_VER) && defined(_M_IX86)
static __declspec(naked) void BG_ClampPointToTracemapExtends(vec3_t point, vec2_t clampedPoint) {
    __asm {
tq_30016be0:
        MOV EDX,dword ptr [ESP + 0x4]
tq_30016be4:
        FLD dword ptr [EDX]
tq_30016be6:
        FCOMP dword ptr [tracemap + TraceQueryMinX]
tq_30016bec:
        FNSTSW AX
tq_30016bee:
        TEST AH,0x1
tq_30016bf1:
        JZ tq_30016c01
tq_30016bf3:
        FLD dword ptr [tracemap + TraceQueryMinX]
tq_30016bf9:
        MOV ECX,dword ptr [ESP + 0x8]
tq_30016bfd:
        FSTP dword ptr [ECX]
tq_30016bff:
        JMP tq_30016c26
tq_30016c01:
        FLD dword ptr [EDX]
tq_30016c03:
        FCOMP dword ptr [tracemap + TraceQueryMaxX]
tq_30016c09:
        FNSTSW AX
tq_30016c0b:
        TEST AH,0x41
tq_30016c0e:
        JNZ tq_30016c1e
tq_30016c10:
        FLD dword ptr [tracemap + TraceQueryMaxX]
tq_30016c16:
        MOV ECX,dword ptr [ESP + 0x8]
tq_30016c1a:
        FSTP dword ptr [ECX]
tq_30016c1c:
        JMP tq_30016c26
tq_30016c1e:
        MOV ECX,dword ptr [ESP + 0x8]
tq_30016c22:
        MOV EAX,dword ptr [EDX]
tq_30016c24:
        MOV dword ptr [ECX],EAX
tq_30016c26:
        FLD dword ptr [EDX + 0x4]
tq_30016c29:
        FCOMP dword ptr [tracemap + TraceQueryMaxY]
tq_30016c2f:
        FNSTSW AX
tq_30016c31:
        TEST AH,0x1
tq_30016c34:
        JZ tq_30016c40
tq_30016c36:
        MOV EDX,dword ptr [tracemap + TraceQueryMaxY]
tq_30016c3c:
        MOV dword ptr [ECX + 0x4],EDX
tq_30016c3f:
        RET
tq_30016c40:
        FLD dword ptr [EDX + 0x4]
tq_30016c43:
        FCOMP dword ptr [tracemap + TraceQueryMinY]
tq_30016c49:
        FNSTSW AX
tq_30016c4b:
        TEST AH,0x41
tq_30016c4e:
        JNZ tq_30016c59
tq_30016c50:
        MOV EAX,dword ptr [tracemap + TraceQueryMinY]
tq_30016c55:
        MOV dword ptr [ECX + 0x4],EAX
tq_30016c58:
        RET
tq_30016c59:
        MOV EDX,dword ptr [EDX + 0x4]
tq_30016c5c:
        MOV dword ptr [ECX + 0x4],EDX
tq_30016c5f:
        RET
    }
}
#else

static void BG_ClampPointToTracemapExtends( vec3_t point, vec2_t out ) {

	if( point[0] < tracemap.world_mins[0] )
		out[0] = tracemap.world_mins[0];
	else if( point[0] > tracemap.world_maxs[0] )
		out[0] = tracemap.world_maxs[0];
	else
		out[0] = point[0];

	if( point[1] < tracemap.world_maxs[1] )
		out[1] = tracemap.world_maxs[1];
	else if( point[1] > tracemap.world_mins[1] )
		out[1] = tracemap.world_mins[1];
	else
		out[1] = point[1];
}


#endif

#if defined(CGAMEDLL) && defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float BG_GetSkyHeightAtPoint(vec3_t pos) {
    __asm {
tq_30016b50:
        MOV EAX,dword ptr [tracemap + TraceQueryLoaded]
tq_30016b55:
        SUB ESP,0xc
tq_30016b58:
        TEST EAX,EAX
tq_30016b5a:
        JNZ tq_30016b66
tq_30016b5c:
        FLD dword ptr [traceQueryMaxHeight]
tq_30016b62:
        ADD ESP,0xc
tq_30016b65:
        RET
tq_30016b66:
        MOV ECX,dword ptr [ESP + 0x10]
tq_30016b6a:
        LEA EAX,[ESP + 0x4]
tq_30016b6e:
        PUSH EAX
tq_30016b6f:
        PUSH ECX
tq_30016b70:
        CALL BG_ClampPointToTracemapExtends
tq_30016b75:
        FLD dword ptr [ESP + 0xc]
tq_30016b79:
        FSUB dword ptr [tracemap + TraceQueryMinX]
tq_30016b7f:
        ADD ESP,0x4
tq_30016b82:
        FMUL dword ptr [one_over_mapgrid_factor]
tq_30016b88:
        FSTP dword ptr [ESP]
tq_30016b8b:
        CALL myftol
tq_30016b90:
        FLD dword ptr [ESP + 0xc]
tq_30016b94:
        FSUB dword ptr [tracemap + TraceQueryMinY]
tq_30016b9a:
        MOV dword ptr [ESP + 0x4],EAX
tq_30016b9e:
        FMUL dword ptr [one_over_mapgrid_factor + 4]
tq_30016ba4:
        FSTP dword ptr [ESP]
tq_30016ba7:
        CALL myftol
tq_30016bac:
        MOV dword ptr [ESP + 0x14],EAX
tq_30016bb0:
        LEA EDX,[ESP + 0x14]
tq_30016bb4:
        LEA EAX,[ESP + 0x4]
tq_30016bb8:
        PUSH EDX
tq_30016bb9:
        PUSH EAX
tq_30016bba:
        CALL etpro_FinalizeTracemapClamp
tq_30016bbf:
        MOV ECX,dword ptr [ESP + 0x1c]
tq_30016bc3:
        MOV EAX,dword ptr [ESP + 0xc]
tq_30016bc7:
        IMUL ECX,TRACEMAP_SIZE
tq_30016bca:
        ADD ESP,0xc
tq_30016bcd:
        ADD ECX,EAX
tq_30016bcf:
        FLD dword ptr [ECX*0x4 + tracemap + TraceQuerySky]
tq_30016bd6:
        ADD ESP,0xc
tq_30016bd9:
        RET
    }
}
#else

float BG_GetSkyHeightAtPoint( vec3_t pos ) {
	int i, j;
	vec2_t point;
//	int msec = trap_Milliseconds();

//	n_getskytime++;

	if( !tracemap.loaded ) {
//		getskytime += trap_Milliseconds() - msec;
		return MAX_WORLD_HEIGHT;
	}

	BG_ClampPointToTracemapExtends( pos, point );

	i = myftol(( point[0] - tracemap.world_mins[0] ) * one_over_mapgrid_factor[0]);
	j = myftol(( point[1] - tracemap.world_mins[1] ) * one_over_mapgrid_factor[1]);

	// rain - re-clamp the points, because a rounding error can cause
	// them to go outside the array
	etpro_FinalizeTracemapClamp(&i, &j);

//	getskytime += trap_Milliseconds() - msec;
	return( tracemap.sky[j][i] );	
}


#endif

#if defined(CGAMEDLL) && defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float BG_GetSkyGroundHeightAtPoint(vec3_t pos) {
    __asm {
tq_30016c60:
        MOV EAX,dword ptr [tracemap + TraceQueryLoaded]
tq_30016c65:
        SUB ESP,0xc
tq_30016c68:
        TEST EAX,EAX
tq_30016c6a:
        JNZ tq_30016c76
tq_30016c6c:
        FLD dword ptr [traceQueryMaxHeight]
tq_30016c72:
        ADD ESP,0xc
tq_30016c75:
        RET
tq_30016c76:
        MOV ECX,dword ptr [ESP + 0x10]
tq_30016c7a:
        LEA EAX,[ESP + 0x4]
tq_30016c7e:
        PUSH EAX
tq_30016c7f:
        PUSH ECX
tq_30016c80:
        CALL BG_ClampPointToTracemapExtends
tq_30016c85:
        FLD dword ptr [ESP + 0xc]
tq_30016c89:
        FSUB dword ptr [tracemap + TraceQueryMinX]
tq_30016c8f:
        ADD ESP,0x4
tq_30016c92:
        FMUL dword ptr [one_over_mapgrid_factor]
tq_30016c98:
        FSTP dword ptr [ESP]
tq_30016c9b:
        CALL myftol
tq_30016ca0:
        FLD dword ptr [ESP + 0xc]
tq_30016ca4:
        FSUB dword ptr [tracemap + TraceQueryMinY]
tq_30016caa:
        MOV dword ptr [ESP + 0x4],EAX
tq_30016cae:
        FMUL dword ptr [one_over_mapgrid_factor + 4]
tq_30016cb4:
        FSTP dword ptr [ESP]
tq_30016cb7:
        CALL myftol
tq_30016cbc:
        MOV dword ptr [ESP + 0x14],EAX
tq_30016cc0:
        LEA EDX,[ESP + 0x14]
tq_30016cc4:
        LEA EAX,[ESP + 0x4]
tq_30016cc8:
        PUSH EDX
tq_30016cc9:
        PUSH EAX
tq_30016cca:
        CALL etpro_FinalizeTracemapClamp
tq_30016ccf:
        MOV ECX,dword ptr [ESP + 0x1c]
tq_30016cd3:
        MOV EAX,dword ptr [ESP + 0xc]
tq_30016cd7:
        IMUL ECX,TRACEMAP_SIZE
tq_30016cda:
        ADD ESP,0xc
tq_30016cdd:
        ADD ECX,EAX
tq_30016cdf:
        FLD dword ptr [ECX*0x4 + tracemap + TraceQuerySkyGround]
tq_30016ce6:
        ADD ESP,0xc
tq_30016ce9:
        RET
    }
}
#else

float BG_GetSkyGroundHeightAtPoint( vec3_t pos ) {
	int i, j;
	vec2_t point;
//	int msec = trap_Milliseconds();

//	n_getgroundtime++;

	if( !tracemap.loaded ) {
//		getgroundtime += trap_Milliseconds() - msec;
		return MAX_WORLD_HEIGHT;
	}

	BG_ClampPointToTracemapExtends( pos, point );

	i = myftol(( point[0] - tracemap.world_mins[0] ) * one_over_mapgrid_factor[0]);
	j = myftol(( point[1] - tracemap.world_mins[1] ) * one_over_mapgrid_factor[1]);

	// rain - re-clamp the points, because a rounding error can cause
	// them to go outside the array
	etpro_FinalizeTracemapClamp(&i, &j);

//	getgroundtime += trap_Milliseconds() - msec;
	return( tracemap.skyground[j][i] );
}


#endif

float BG_GetGroundHeightAtPoint( vec3_t pos ) {
	int i, j;
	vec2_t point;
//	int msec = trap_Milliseconds();

//	n_getgroundtime++;

	if( !tracemap.loaded ) {
//		getgroundtime += trap_Milliseconds() - msec;
		return MIN_WORLD_HEIGHT;
	}

	BG_ClampPointToTracemapExtends( pos, point );

	i = myftol(( point[0] - tracemap.world_mins[0] ) * one_over_mapgrid_factor[0]);
	j = myftol(( point[1] - tracemap.world_mins[1] ) * one_over_mapgrid_factor[1]);

	// rain - re-clamp the points, because a rounding error can cause
	// them to go outside the array
	etpro_FinalizeTracemapClamp(&i, &j);

//	getgroundtime += trap_Milliseconds() - msec;
	return( tracemap.ground[j][i] );
}

int BG_GetTracemapGroundFloor( void ) {
	if( !tracemap.loaded ) {
		return MIN_WORLD_HEIGHT;
	}
	return tracemap.groundfloor;
}

int BG_GetTracemapGroundCeil( void ) {
	if( !tracemap.loaded ) {
		return MAX_WORLD_HEIGHT;
	}
	return tracemap.groundceil;
}

// rain - re-clamp the points, because a rounding error can cause
// them to go outside the array
void etpro_FinalizeTracemapClamp(int *x, int *y)
{
	if (*x < 0)
		*x = 0;
	else if (*x > TRACEMAP_SIZE - 1)
		*x = TRACEMAP_SIZE - 1;

	if (*y < 0)
		*y = 0;
	else if (*y > TRACEMAP_SIZE - 1)
		*y = TRACEMAP_SIZE - 1;
}
