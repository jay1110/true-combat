#include <stddef.h>
#include "tce_numeric.h"
#include "tce_tracemap_precision.h"
#ifdef CGAMEDLL
#include "../cgame/cg_local.h"
#else
#include "g_local.h"
#endif

/*
**  Map tracemap view generation
*/

#define MAX_WORLD_HEIGHT            MAX_MAP_SIZE    // maximum world height
#define MIN_WORLD_HEIGHT            -MAX_MAP_SIZE   // minimum world height

//#define TRACEMAP_SIZE				1024
#define TRACEMAP_SIZE               256

typedef struct tracemap_s
{
	qboolean loaded;
	float sky[TRACEMAP_SIZE][TRACEMAP_SIZE];
	float skyground[TRACEMAP_SIZE][TRACEMAP_SIZE];
	float ground[TRACEMAP_SIZE][TRACEMAP_SIZE];
	vec2_t world_mins, world_maxs;
	int groundfloor, groundceil;
} tracemap_t;

static tracemap_t tracemap;

static vec2_t one_over_mapgrid_factor;

#if (defined(CGAMEDLL) || defined(GAMEDLL)) && defined(_MSC_VER) && defined(_M_IX86)
typedef char TraceLoadLayoutGuard[
		(TRACEMAP_SIZE == 256 && sizeof(float) == 4 &&
		 sizeof(((tracemap_t *) 0)->sky[0]) == 1024 &&
		 offsetof(tracemap_t, skyground) == offsetof(tracemap_t, sky) + sizeof(((tracemap_t *) 0)->sky) &&
		 offsetof(tracemap_t, ground) == offsetof(tracemap_t, skyground) + sizeof(((tracemap_t *) 0)->skyground) &&
		 offsetof(tracemap_t, world_mins) == offsetof(tracemap_t, ground) + sizeof(((tracemap_t *) 0)->ground)) ? 1 : -1];

#endif

void etpro_FinalizeTracemapClamp(int *x, int *y);

#ifdef CGAMEDLL
#if defined(_MSC_VER) && defined(_M_IX86)
static unsigned int CG_TraceMapBits(const float *traceValue)
{
	unsigned int traceBits;
	memcpy(&traceBits, traceValue, sizeof(traceBits));
	return traceBits;
}

static void CG_TraceMapStoreHeight(float *traceDest, const float *traceSource)
{
	*traceDest = *traceSource;
}

static unsigned int CG_TraceMapCompare(float traceLeft, float traceRight)
{
	/* x87 C3:C0: unordered=11, equal=10, less=01, greater=00.
	 * Binary64 also represents every signed 32-bit integer exactly. */
	float left = (float)traceLeft;
	float right = (float)traceRight;

	if (left < right)
	{
		return 0x0100;
	}
	if (left > right)
	{
		return 0;
	}
	if (left == right)
	{
		return 0x4000;
	}
	return 0x4100;
}

static unsigned int CG_TraceMapCompareInteger(int traceLeft, float traceRight)
{
	/* x87 C3:C0: unordered=11, equal=10, less=01, greater=00.
	 * Binary64 also represents every signed 32-bit integer exactly. */
	double left = (double)traceLeft;
	double right = (double)traceRight;

	if (left < right)
	{
		return 0x0100;
	}
	if (left > right)
	{
		return 0;
	}
	if (left == right)
	{
		return 0x4000;
	}
	return 0x4100;
}

/* Store the trace origin before comparing the retained intermediate. */
static unsigned int CG_TraceMapUpdateZ(float traceValue, float traceStep,
                                       float *traceStored, float traceBound, int traceSubtract)
{
	long double value = traceSubtract ? (long double)traceValue - traceStep
	                                 : (long double)traceValue + traceStep;

	/* Compare the retained result, not the binary32 value written to memory. */
	*traceStored = (float)value;
	if (value < (long double)traceBound)
	{
		return 0x0100;
	}
	if (value > (long double)traceBound)
	{
		return 0;
	}
	if (value == (long double)traceBound)
	{
		return 0x4000;
	}
	return 0x4100;
}

static double CG_TraceMapProgress(int tracePoints)
{
	/* Both products are exact in binary64 for every signed 32-bit input. */
	return (double)tracePoints * 0.0000152587890625 * 100.0;
}

/* Original __ftol returns the low word of a truncating signed 64-bit store. */
static int CG_TraceMapInteger(float traceValue)
{
	return TCE_TruncateNativeInteger((long double)traceValue);
}

/* The original stores the same retained index*step+minimum result twice. */
static void CG_TraceMapGridCoordinate(int gridIndex, float gridStep, float gridMin,
                                      float *gridStart, float *gridEnd)
{
	long double coordinate = (long double)gridIndex * gridStep + gridMin;

	*gridEnd = (float)coordinate;
	*gridStart = (float)coordinate;
}

/* Normalize one plane, retaining the scale until each binary32 store. */
static void CG_TraceMapNormalizePlane(float *plane, int planeMin, int planeMax, int skyPlane)
{
	int delta = TCE_SubInt32(planeMax, planeMin);
	int pixel;
	long double scale;
	float lower = skyPlane ? 0.0f : 1.0f;

	if (skyPlane && delta == 0)
	{
		scale = 1.0L;
	}
	else
	{
		scale = 254.0L / (long double)delta;
		if (!skyPlane && !(scale < 0.0L || scale > 0.0L))
		{
			scale = 1.0L;
		}
	}

	for (pixel = 0; pixel < TRACEMAP_SIZE * TRACEMAP_SIZE; ++pixel)
	{
		if (skyPlane && CG_TraceMapBits(&plane[pixel]) == 0x47800000u)
		{
			plane[pixel] = 0.0f;
		}
		else if (skyPlane || !((long double)planeMin > (long double)plane[pixel]))
		{
			plane[pixel] = (float)(((long double)plane[pixel] - planeMin) * scale + 1.0L);
		}

		/* The native lower comparison also clamps unordered values. */
		if (!(plane[pixel] >= lower))
		{
			plane[pixel] = lower;
		}
		else if (plane[pixel] > 255.0f)
		{
			plane[pixel] = 255.0f;
		}
	}
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
#define CG_TRACEMAP_PROGRESS(points) CG_TraceMapProgress(points)
#else
#define CG_TRACEMAP_PROGRESS(points) (((points) / (float)(TRACEMAP_SIZE * TRACEMAP_SIZE)) * 100.f)
#endif
void CG_GenerateTracemap(void)
{
	trace_t tr;
	vec3_t  start, end;
	int     i, j;
	float   x_step, y_step;
	int     topdownmin, topdownmax;
	int     skygroundmin, skygroundmax;
	int     min, max;

#if !defined(_MSC_VER) || !defined(_M_IX86)
	float scalefactor;
#endif
	fileHandle_t f;
	byte         data;
	static int   lastDraw   = 0;
	static int   tracecount = 0;
	int          ms;
#if defined(_MSC_VER) && defined(_M_IX86)
	long double        traceWidth, traceHeight;
	unsigned int       traceZFlags;
	const float        *traceMins = cg.mapcoordsMins, *traceMaxs = cg.mapcoordsMaxs;
	static const float traceGridScale  = 0.00390625f;
#endif

	if (!developer.integer)
	{
		CG_Printf("Can only generate a tracemap in developer mode.\n");
		return;
	}

	if (!cg.mapcoordsValid)
	{
		CG_Printf("Need valid mapcoords in the worldspawn to be able to generate a tracemap.\n");
		return;
	}
#if defined(_MSC_VER) && defined(_M_IX86)
	/* The native C3 test accepts both equality and unordered differences. */
	traceWidth = (long double)traceMaxs[0] - traceMins[0];
	traceHeight = (long double)traceMins[1] - traceMaxs[1];
	if (traceHeight < traceWidth || traceHeight > traceWidth)
	{
#else
	if ((cg.mapcoordsMaxs[0] - cg.mapcoordsMins[0]) != (cg.mapcoordsMins[1] - cg.mapcoordsMaxs[1]))
	{
#endif
		CG_Printf("Mapcoords need to be square.\n");
		return;
	}

	// Topdown tracing
	CG_Printf("Generating level heightmap and level mask...\n");

#if !defined(_MSC_VER) || !defined(_M_IX86)
	memset(&tracemap, 0, sizeof(tracemap));
#endif

	topdownmax = MIN_WORLD_HEIGHT;
	topdownmin = MAX_WORLD_HEIGHT;

	// calculate the size of the level
	// ok, i'm lazy. Hijack commandmap extends for now and default to a TRACEMAP_SIZE by TRACEMAP_SIZE datablock
#if defined(_MSC_VER) && defined(_M_IX86)
	traceWidth = ((long double)traceMaxs[0] - traceMins[0]) * traceGridScale;
	memset(&tracemap, 0, sizeof(tracemap));
	x_step = (float)traceWidth;
	y_step = (float)(((long double)traceMaxs[1] - traceMins[1]) * traceGridScale);
#else
	x_step = (cg.mapcoordsMaxs[0] - cg.mapcoordsMins[0]) / (float)TRACEMAP_SIZE;
	y_step = (cg.mapcoordsMaxs[1] - cg.mapcoordsMins[1]) / (float)TRACEMAP_SIZE;
#endif

	for ( i = 0; i < TRACEMAP_SIZE; i++ )
	{
#if defined(_MSC_VER) && defined(_M_IX86)
		CG_TraceMapGridCoordinate(i, x_step, cg.mapcoordsMins[0], &start[0], &end[0]);
#else
		start[0] = end[0] = cg.mapcoordsMins[0] + i * x_step;
#endif
		for ( j = 0; j < TRACEMAP_SIZE; j++ )
		{
#if defined(_MSC_VER) && defined(_M_IX86)
			CG_TraceMapGridCoordinate(j, y_step, cg.mapcoordsMins[1], &start[1], &end[1]);
#else
			start[1] = end[1] = cg.mapcoordsMins[1] + j * y_step;
#endif
			start[2] = MAX_WORLD_HEIGHT;
			end[2]   = MIN_WORLD_HEIGHT;

			// Find the ceiling
			CG_Trace(&tr, start, NULL, NULL, end, ENTITYNUM_NONE, MASK_SOLID | MASK_WATER);
#if defined(_MSC_VER) && defined(_M_IX86)
			traceZFlags = CG_TraceMapUpdateZ(tr.endpos[2], 1.0f, &start[2], MIN_WORLD_HEIGHT, 1);
#else
			start[2] = tr.endpos[2] - 1;
#endif
			tracecount = (int)((unsigned int)tracecount + 1u);

			// Find ground
			while (1)
			{
				// Perform traces up to the sky, repeating at a higher start height if we start
				// inside a solid.

#if defined(_MSC_VER) && defined(_M_IX86)
				if (traceZFlags & 0x4100)
				{
#else
				if (start[2] <= MIN_WORLD_HEIGHT)
				{
#endif
					tracemap.ground[j][i] = MIN_WORLD_HEIGHT;
					break;
				}
#if defined(_MSC_VER) && defined(_M_IX86)
				if (CG_TraceMapCompare(end[2], MIN_WORLD_HEIGHT) & 0x4100)
#else
				if (end[2] <= MIN_WORLD_HEIGHT)
#endif
				{ end[2] = MIN_WORLD_HEIGHT + 1; }
				CG_Trace(&tr, start, NULL, NULL, end, ENTITYNUM_NONE, (MASK_SOLID | MASK_WATER));
				tracecount = (int)((unsigned int)tracecount + 1u);
				if (tr.startsolid)              // Stuck in something, skip over it.
				{
#if defined(_MSC_VER) && defined(_M_IX86)
					traceZFlags = CG_TraceMapUpdateZ(start[2], 64.0f, &start[2], MIN_WORLD_HEIGHT, 1);
#else
					start[2] -= 64;
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
				}
				else if (CG_TraceMapCompare(tr.fraction, 1.0f) & 0x4000)
				{
#else
				}
				else if (tr.fraction == 1)
				{
#endif      // Didn't hit anything, we're (probably) outside the world
					tracemap.ground[j][i] = MIN_WORLD_HEIGHT;
					break;
				}
				else
				{
#if defined(_MSC_VER) && defined(_M_IX86)
					CG_TraceMapStoreHeight(&tracemap.ground[j][i], &tr.endpos[2]);
#else
					tracemap.ground[j][i] = tr.endpos[2];
#endif
					if (!(tr.surfaceFlags & SURF_NODRAW))
					{
#if defined(_MSC_VER) && defined(_M_IX86)
						if (CG_TraceMapCompareInteger(topdownmax, tracemap.ground[j][i]) & 0x100)
#else
						if (tracemap.ground[j][i] > topdownmax)
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						{ topdownmax = CG_TraceMapInteger(tracemap.ground[j][i]); }
#else
						{ topdownmax = tracemap.ground[j][i]; }
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						if (!(CG_TraceMapCompareInteger(topdownmin, tracemap.ground[j][i]) & 0x4100))
#else
						if (tracemap.ground[j][i] < topdownmin)
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						{ topdownmin = CG_TraceMapInteger(tracemap.ground[j][i]); }
#else
						{ topdownmin = tracemap.ground[j][i]; }
#endif
					}
					break;
				}
			}

			// bit hacky, get some console output going
			ms = trap_Milliseconds();
			if (!((lastDraw <= ms) && (lastDraw > (int)((unsigned int)ms - 500u))))
			{
				lastDraw = ms;

				CG_Printf("%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE + j, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS(i * TRACEMAP_SIZE + j), tracecount);
				trap_UpdateScreen();
			}
		}
	}
	CG_Printf("%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS(i * TRACEMAP_SIZE), tracecount);
	trap_UpdateScreen();

	// Sky tracing
	CG_Printf("Generating sky heightmap and sky mask...\n");

	max = MIN_WORLD_HEIGHT;
	min = MAX_WORLD_HEIGHT;

	for ( i = 0; i < TRACEMAP_SIZE; i++ )
	{
#if defined(_MSC_VER) && defined(_M_IX86)
		CG_TraceMapGridCoordinate(i, x_step, cg.mapcoordsMins[0], &start[0], &end[0]);
#else
		start[0] = end[0] = cg.mapcoordsMins[0] + i * x_step;
#endif
		for ( j = 0; j < TRACEMAP_SIZE; j++ )
		{
#if defined(_MSC_VER) && defined(_M_IX86)
			CG_TraceMapGridCoordinate(j, y_step, cg.mapcoordsMins[1], &start[1], &end[1]);
#else
			start[1] = end[1] = cg.mapcoordsMins[1] + j * y_step;
#endif
			//start[2] = MIN_WORLD_HEIGHT;
			start[2] = tracemap.ground[j][i];
			end[2]   = MAX_WORLD_HEIGHT;

#if defined(_MSC_VER) && defined(_M_IX86)
			if (CG_TraceMapCompare(start[2], MIN_WORLD_HEIGHT) & 0x4000)
			{
#else
			if (start[2] == MIN_WORLD_HEIGHT)
			{
#endif
				// we got a hole here, no need to trace
				tracemap.sky[j][i] = MAX_WORLD_HEIGHT;
			}
			else
			{
				// Find sky
#if defined(_MSC_VER) && defined(_M_IX86)
				traceZFlags = CG_TraceMapCompare(start[2], MAX_WORLD_HEIGHT);
#endif
				while (1)
				{
					// Perform traces up to the sky, repeating at a higher start height if we start
					// inside a solid.

#if defined(_MSC_VER) && defined(_M_IX86)
					if (!(traceZFlags & 0x100))
					{
#else
					if (start[2] >= MAX_WORLD_HEIGHT)
					{
#endif
						tracemap.sky[j][i] = MAX_WORLD_HEIGHT;
						break;
					}
#if defined(_MSC_VER) && defined(_M_IX86)
					if (!(CG_TraceMapCompare(end[2], MAX_WORLD_HEIGHT) & 0x100))
#else
					if (end[2] >= MAX_WORLD_HEIGHT)
#endif
					{ end[2] = MAX_WORLD_HEIGHT - 1; }
					CG_Trace(&tr, start, NULL, NULL, end, ENTITYNUM_NONE, MASK_SOLID);
					tracecount = (int)((unsigned int)tracecount + 1u);
					if (tr.startsolid)              // Stuck in something, skip over it.
					// can happen, tr.endpos still is valid even if we're starting in a solid but trace out of it hitting the next surface
					{
						if (tr.surfaceFlags & SURF_SKY)
						{
							// are we in a solid?
							if (!(CG_PointContents(tr.endpos, ENTITYNUM_NONE) & (MASK_SOLID | MASK_WATER)))
							{
#if defined(_MSC_VER) && defined(_M_IX86)
								CG_TraceMapStoreHeight(&tracemap.sky[j][i], &tr.endpos[2]);
#else
								tracemap.sky[j][i] = tr.endpos[2];
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
								if (CG_TraceMapCompareInteger(max, tracemap.sky[j][i]) & 0x100)
#else
								if (tracemap.sky[j][i] > max)
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
								{ max = CG_TraceMapInteger(tracemap.sky[j][i]); }
#else
								{ max = tracemap.sky[j][i]; }
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
								if (!(CG_TraceMapCompareInteger(min, tracemap.sky[j][i]) & 0x4100))
#else
								if (tracemap.sky[j][i] < min)
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
								{ min = CG_TraceMapInteger(tracemap.sky[j][i]); }
#else
								{ min = tracemap.sky[j][i]; }
#endif
								break;
							}
							else
							{
								// skip over it
#if defined(_MSC_VER) && defined(_M_IX86)
								traceZFlags = CG_TraceMapUpdateZ(tr.endpos[2], 1.0f, &start[2], MAX_WORLD_HEIGHT, 0);
#else
								start[2] = tr.endpos[2] + 1;
#endif
							}
						}
						else
						{
#if defined(_MSC_VER) && defined(_M_IX86)
							traceZFlags = CG_TraceMapUpdateZ(tr.endpos[2], 1.0f, &start[2], MAX_WORLD_HEIGHT, 0);
#else
							start[2] = tr.endpos[2] + 1;
#endif
						}
#if defined(_MSC_VER) && defined(_M_IX86)
					}
					else if (CG_TraceMapCompare(tr.fraction, 1.0f) & 0x4000)
					{
#else
					}
					else if (tr.fraction == 1)
					{
#endif      // Didn't hit anything, we're (probably) outside the world
						tracemap.sky[j][i] = MAX_WORLD_HEIGHT;
						break;
					}
					else if (tr.surfaceFlags & SURF_SKY)        // Hit sky, this is where we start.
					{
#if defined(_MSC_VER) && defined(_M_IX86)
						CG_TraceMapStoreHeight(&tracemap.sky[j][i], &tr.endpos[2]);
#else
						tracemap.sky[j][i] = tr.endpos[2];
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						if (CG_TraceMapCompareInteger(max, tracemap.sky[j][i]) & 0x100)
#else
						if (tracemap.sky[j][i] > max)
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						{ max = CG_TraceMapInteger(tracemap.sky[j][i]); }
#else
						{ max = tracemap.sky[j][i]; }
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						if (!(CG_TraceMapCompareInteger(min, tracemap.sky[j][i]) & 0x4100))
#else
						if (tracemap.sky[j][i] < min)
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
						{ min = CG_TraceMapInteger(tracemap.sky[j][i]); }
#else
						{ min = tracemap.sky[j][i]; }
#endif
						break;
					}
					else
					{
						// hit something else, skip over it
#if defined(_MSC_VER) && defined(_M_IX86)
						traceZFlags = CG_TraceMapUpdateZ(tr.endpos[2], 64.0f, &start[2], MAX_WORLD_HEIGHT, 0);
#else
						start[2] = tr.endpos[2] + 64;
#endif
					}
				}
			}

			// bit hacky, get some console output going
			ms = trap_Milliseconds();
			if (!((lastDraw <= ms) && (lastDraw > (int)((unsigned int)ms - 500u))))
			{
				lastDraw = ms;

				CG_Printf("%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE + j, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS(i * TRACEMAP_SIZE + j), tracecount);
				trap_UpdateScreen();
			}
		}
	}
	CG_Printf("%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS(i * TRACEMAP_SIZE), tracecount);
	trap_UpdateScreen();

	// More groundtrace, find ceilings for areas where we don't have ground
	CG_Printf("Generating sky groundmap...\n");

	skygroundmin = MAX_WORLD_HEIGHT;
	skygroundmax = MIN_WORLD_HEIGHT;

	for ( i = 0; i < TRACEMAP_SIZE; i++ )
	{
#if defined(_MSC_VER) && defined(_M_IX86)
		CG_TraceMapGridCoordinate(i, x_step, cg.mapcoordsMins[0], &start[0], &end[0]);
#else
		start[0] = end[0] = cg.mapcoordsMins[0] + i * x_step;
#endif
		for ( j = 0; j < TRACEMAP_SIZE; j++ )
		{
#if defined(_MSC_VER) && defined(_M_IX86)
			CG_TraceMapGridCoordinate(j, y_step, cg.mapcoordsMins[1], &start[1], &end[1]);
#else
			start[1] = end[1] = cg.mapcoordsMins[1] + j * y_step;
#endif
			start[2] = MAX_WORLD_HEIGHT;
			end[2]   = MIN_WORLD_HEIGHT;

#if defined(_MSC_VER) && defined(_M_IX86)
			if (CG_TraceMapBits(&tracemap.sky[j][i]) == 0x47800000u && CG_TraceMapBits(&tracemap.ground[j][i]) != 0xc7800000u)
			{
#else
			if (tracemap.sky[j][i] == MAX_WORLD_HEIGHT && tracemap.ground[j][i] != MIN_WORLD_HEIGHT)
			{
#endif
				// Find the ceiling
				CG_Trace(&tr, start, NULL, NULL, end, ENTITYNUM_NONE, MASK_SOLID | MASK_WATER);
				tracecount = (int)((unsigned int)tracecount + 1u);
#if defined(_MSC_VER) && defined(_M_IX86)
				if (CG_TraceMapCompare(tr.fraction, 1.0f) & 0x4000)
				{
#else
				if (tr.fraction == 1)
				{
#endif      // Didn't hit anything, we're (probably) outside the world
					tracemap.skyground[j][i] = MIN_WORLD_HEIGHT;
				}
				else
				{

#if defined(_MSC_VER) && defined(_M_IX86)
					memcpy(&tracemap.skyground[j][i], &tr.endpos[2], sizeof(float));
#else
					tracemap.skyground[j][i] = tr.endpos[2];
#endif
				}
			}
			else
			{

#if defined(_MSC_VER) && defined(_M_IX86)
				memcpy(&tracemap.skyground[j][i], &tracemap.ground[j][i], sizeof(float));
#else
				tracemap.skyground[j][i] = tracemap.ground[j][i];
#endif
			}

#if defined(_MSC_VER) && defined(_M_IX86)
			if (CG_TraceMapBits(&tracemap.skyground[j][i]) != 0xc7800000u)
			{
#else
			if (tracemap.skyground[j][i] != MIN_WORLD_HEIGHT)
			{
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
				if (CG_TraceMapCompareInteger(skygroundmax, tracemap.skyground[j][i]) & 0x100)
#else
				if (tracemap.skyground[j][i] > skygroundmax)
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
				{ skygroundmax = CG_TraceMapInteger(tracemap.skyground[j][i]); }
#else
				{ skygroundmax = tracemap.skyground[j][i]; }
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
				if (!(CG_TraceMapCompareInteger(skygroundmin, tracemap.skyground[j][i]) & 0x4100))
#else
				if (tracemap.skyground[j][i] < skygroundmin)
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
				{ skygroundmin = CG_TraceMapInteger(tracemap.skyground[j][i]); }
#else
				{ skygroundmin = tracemap.skyground[j][i]; }
#endif
			}

			// bit hacky, get some console output going
			ms = trap_Milliseconds();
			if (!((lastDraw <= ms) && (lastDraw > (int)((unsigned int)ms - 500u))))
			{
				lastDraw = ms;

				CG_Printf("%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE + j, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS(i * TRACEMAP_SIZE + j), tracecount);
				trap_UpdateScreen();
			}
		}
	}
	CG_Printf("%i of %i gridpoints calculated (%.2f%%), %i total traces\n", i * TRACEMAP_SIZE, TRACEMAP_SIZE * TRACEMAP_SIZE, CG_TRACEMAP_PROGRESS(i * TRACEMAP_SIZE), tracecount);
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
	CG_TraceMapNormalizePlane(&tracemap.ground[0][0], topdownmin, topdownmax, 0);
	CG_TraceMapNormalizePlane(&tracemap.skyground[0][0], skygroundmin, skygroundmax, 0);
	CG_TraceMapNormalizePlane(&tracemap.sky[0][0], min, max, 1);
#else
	scalefactor = 254.f / (topdownmax - topdownmin);
	if (scalefactor == 0.f)
	{
		scalefactor = 1.f;
	}
	for ( i = 0; i < TRACEMAP_SIZE; i++ )
	{
		for ( j = 0; j < TRACEMAP_SIZE; j++ )
		{
			if (tracemap.ground[i][j] >= topdownmin)
			{
				tracemap.ground[i][j] = 1.0 + (tracemap.ground[i][j] - topdownmin) * scalefactor;
			}

			// rain - hard clamp because *min and *max are rounded :(
			if (tracemap.ground[i][j] < 1.0)
			{
				tracemap.ground[i][j] = 1.0;
			}
			else if (tracemap.ground[i][j] > 255.0)
			{
				tracemap.ground[i][j] = 255.0;
			}
		}
	}

	// min is 0
	// max is 255
	// rain - this is d&l, min=1 max=255
	scalefactor = 254.f / (skygroundmax - skygroundmin);
	if (scalefactor == 0.f)
	{
		scalefactor = 1.f;
	}
	for ( i = 0; i < TRACEMAP_SIZE; i++ )
	{
		for ( j = 0; j < TRACEMAP_SIZE; j++ )
		{
			if (tracemap.skyground[i][j] >= skygroundmin)
			{
				tracemap.skyground[i][j] = 1.0 + (tracemap.skyground[i][j] - skygroundmin) * scalefactor;
			}

			// rain - hard clamp because *min and *max are rounded :(
			if (tracemap.skyground[i][j] < 1.0)
			{
				tracemap.skyground[i][j] = 1.0;
			}
			else if (tracemap.skyground[i][j] > 255.0)
			{
				tracemap.skyground[i][j] = 255.0;
			}
		}
	}

	// no sky is 0
	// min is 1
	// max is 255
	if (max - min == 0)
	{
		scalefactor = 1.f;
	}
	else
	{
		scalefactor = 254.f / (max - min);
	}
	for ( i = 0; i < TRACEMAP_SIZE; i++ )
	{
		for ( j = 0; j < TRACEMAP_SIZE; j++ )
		{
			if (tracemap.sky[i][j] == MAX_WORLD_HEIGHT)
			{
				tracemap.sky[i][j] = 0.f;
			}
			else
			{
				tracemap.sky[i][j] = 1.f + (tracemap.sky[i][j] - min) * scalefactor;
			}

			// rain - hard clamp because *min and *max are rounded :(
			if (tracemap.sky[i][j] < 0.0)
			{
				tracemap.sky[i][j] = 0.0;
			}
			else if (tracemap.sky[i][j] > 255.0)
			{
				tracemap.sky[i][j] = 255.0;
			}
		}
	}
#endif
	// write tga
	trap_FS_FOpenFile(va("maps/%s_tracemap.tga", Q_strlwr(cgs.rawmapname)), &f, FS_WRITE);

	// header
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 0
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 1
	data = 2; trap_FS_Write(&data, sizeof(data), f);    // 2 : uncompressed type
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 3
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 4
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 5
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 6
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 7
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 8
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 9
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 10
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 11
	data = TRACEMAP_SIZE & 255; trap_FS_Write(&data, sizeof(data), f);      // 12 : width
	data = TRACEMAP_SIZE >> 8; trap_FS_Write(&data, sizeof(data), f);   // 13 : width
	data = TRACEMAP_SIZE & 255; trap_FS_Write(&data, sizeof(data), f);      // 14 : height
	data = TRACEMAP_SIZE >> 8; trap_FS_Write(&data, sizeof(data), f);   // 15 : height
	data = 32; trap_FS_Write(&data, sizeof(data), f);   // 16 : pixel size
	data = 0; trap_FS_Write(&data, sizeof(data), f);    // 17

	// R: topdown mask
	// G: there is sky here yes/no mask
	// B: sky mask
	// A: there is map here yes/no mask
	for ( i = 0; i < TRACEMAP_SIZE; i++ )
	{
		for ( j = 0; j < TRACEMAP_SIZE; j++ )
		{
			if (i == 0 && j < 6)
			{
				// abuse first six pixels for our extended data
				switch (j)
				{
				case 0: trap_FS_Write(&topdownmin, sizeof(topdownmin), f); break;
				case 1: trap_FS_Write(&topdownmax, sizeof(topdownmax), f); break;
				case 2: trap_FS_Write(&skygroundmin, sizeof(skygroundmin), f); break;
				case 3: trap_FS_Write(&skygroundmax, sizeof(skygroundmax), f); break;
				case 4: trap_FS_Write(&min, sizeof(min), f); break;
				case 5: trap_FS_Write(&max, sizeof(max), f); break;
				}
				continue;
			}
#if defined(_MSC_VER) && defined(_M_IX86)

			data = CG_TraceMapInteger(tracemap.sky[TRACEMAP_SIZE - 1 - i][j]); trap_FS_Write(&data, sizeof(data), f);       // b
#else

			data = tracemap.sky[TRACEMAP_SIZE - 1 - i][j]; trap_FS_Write(&data, sizeof(data), f);   // b
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
			if (CG_TraceMapBits(&tracemap.skyground[TRACEMAP_SIZE - 1 - i][j]) == 0xc7800000u)
			{
#else
			if (tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] == MIN_WORLD_HEIGHT)
			{
#endif
				data = 0; trap_FS_Write(&data, sizeof(data), f);    // g
			}
			else
			{
#if defined(_MSC_VER) && defined(_M_IX86)
				data = CG_TraceMapInteger(tracemap.skyground[TRACEMAP_SIZE - 1 - i][j]); trap_FS_Write(&data, sizeof(data), f);     // g
#else
				data = tracemap.skyground[TRACEMAP_SIZE - 1 - i][j]; trap_FS_Write(&data, sizeof(data), f);     // g
#endif
			}
#if defined(_MSC_VER) && defined(_M_IX86)
			if (CG_TraceMapBits(&tracemap.ground[TRACEMAP_SIZE - 1 - i][j]) == 0xc7800000u)
			{
#else
			if (tracemap.ground[TRACEMAP_SIZE - 1 - i][j] == MIN_WORLD_HEIGHT)
			{
#endif
				data = 0; trap_FS_Write(&data, sizeof(data), f);    // r
				data = 0; trap_FS_Write(&data, sizeof(data), f);    // a
			}
			else
			{
#if defined(_MSC_VER) && defined(_M_IX86)
				data = CG_TraceMapInteger(tracemap.ground[TRACEMAP_SIZE - 1 - i][j]); trap_FS_Write(&data, sizeof(data), f);        // r
#else
				data = tracemap.ground[TRACEMAP_SIZE - 1 - i][j]; trap_FS_Write(&data, sizeof(data), f);    // r
#endif
				data = 255; trap_FS_Write(&data, sizeof(data), f);      // a
			}
		}
	}

	// footer
	i = 0; trap_FS_Write(&i, sizeof(i), f);     // extension area offset, 4 bytes
	i = 0; trap_FS_Write(&i, sizeof(i), f);     // developer directory offset, 4 bytes
	trap_FS_Write("TRUEVISION-XFILE.\0", 18, f);

	trap_FS_FCloseFile(f);
}

#undef CG_TRACEMAP_PROGRESS
#endif // CGAMEDLL

#if (defined(CGAMEDLL) || defined(GAMEDLL)) && ((defined(_MSC_VER) && defined(_M_IX86)) || ((defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)))
/* Extended TGA metadata is signed little-endian, assembled without signed shifts. */
static int BG_TraceMapReadInteger(const byte *data)
{
	unsigned bits = (unsigned)data[0] | ((unsigned)data[1] << 8)
	              | ((unsigned)data[2] << 16) | ((unsigned)data[3] << 24);
	int value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

qboolean BG_LoadTraceMap(char *rawmapname, vec2_t world_mins, vec2_t world_maxs)
{
	int          i, j;
	fileHandle_t f;
	byte         data, datablock[TRACEMAP_SIZE][4];
	int          sky_min, sky_max;
	int          ground_min, ground_max;
	int          skyground_min, skyground_max;
	long double  scalefactor;
	int          delta;
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	tceTraceMapScale_t nativeScale;
	int nativeScaleReady;
#endif

	ground_min    = ground_max = MIN_WORLD_HEIGHT;
	skyground_min = skyground_max = MAX_WORLD_HEIGHT;
	sky_min       = sky_max = MAX_WORLD_HEIGHT;

	if (trap_FS_FOpenFile(va("maps/%s_tracemap.tga", Q_strlwr(rawmapname)), &f, FS_READ) >= 0)
	{
		// skip over header
		for ( i = 0; i < 18; i++ )
		{
			trap_FS_Read(&data, 1, f);
		}

		for ( i = 0; i < TRACEMAP_SIZE; i++ )
		{
			trap_FS_Read(&datablock, sizeof(datablock), f);     // TRACEMAP_SIZE * { b g r a }

			for ( j = 0; j < TRACEMAP_SIZE; j++ )
			{
				if (i == 0 && j < 6)
				{
					// abuse first six pixels for our extended data
					switch (j)
					{
					case 0: ground_min = BG_TraceMapReadInteger(datablock[j]); break;
					case 1: ground_max = BG_TraceMapReadInteger(datablock[j]); break;
					case 2: skyground_min = BG_TraceMapReadInteger(datablock[j]); break;
					case 3: skyground_max = BG_TraceMapReadInteger(datablock[j]); break;
					case 4: sky_min = BG_TraceMapReadInteger(datablock[j]); break;
					case 5: sky_max = BG_TraceMapReadInteger(datablock[j]); break;
					}
					tracemap.sky[TRACEMAP_SIZE - 1 - i][j]       = MAX_WORLD_HEIGHT;
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j]    = MIN_WORLD_HEIGHT;
					continue;
				}

				tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[j][0];    // FIXME: swap
				if (tracemap.sky[TRACEMAP_SIZE - 1 - i][j] == 0)
				{
					tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
				}

				tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[j][1];  // FIXME: swap
				if (tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] == 0)
				{
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
				}

				tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[j][2]; // FIXME: swap
				if (tracemap.ground[TRACEMAP_SIZE - 1 - i][j] == 0)
				{
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = MIN_WORLD_HEIGHT;
				}

				if (datablock[j][3] == 0)
				{
					// just in case
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j]    = MIN_WORLD_HEIGHT;
				}
			}

		}

		trap_FS_FCloseFile(f);

		// Ground
		// calculate scalefactor
		delta = TCE_SubInt32(ground_max, ground_min);
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		nativeScaleReady = TCE_TraceMapPrepareScale(delta, ground_min, &nativeScale);
#endif
		if (delta == 0)
		{
			scalefactor = 1.f;
		}
		else
		{
			// rain - scalefactor 254 to compensate for broken etmain behavior
#if defined(__linux__)
			scalefactor = (1.0L / (long double)delta) * 254.0L;
#else
			scalefactor = 254.0L / (long double)delta;
#endif
		}

		// scale properly
		for ( i = 0; i < TRACEMAP_SIZE; i++ )
		{
			for ( j = 0; j < TRACEMAP_SIZE; j++ )
			{
				if (tracemap.ground[i][j] != MIN_WORLD_HEIGHT)
				{
#if defined(__linux__)
					/* ELF retains the reciprocal before multiplying the sample. */
					tracemap.ground[i][j] = (float)((1.0L / scalefactor) * (long double)tracemap.ground[i][j] + ground_min);
#else
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
					if (nativeScaleReady && TCE_TraceMapFiniteHeight(&nativeScale, tracemap.ground[i][j], &tracemap.ground[i][j]))
					{
						continue;
					}
#endif
					tracemap.ground[i][j] = (float)((long double)tracemap.ground[i][j] / scalefactor + ground_min);
#endif
				}
			}
		}

		// SkyGround
		// calculate scalefactor
		delta = TCE_SubInt32(skyground_max, skyground_min);
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		nativeScaleReady = TCE_TraceMapPrepareScale(delta, skyground_min, &nativeScale);
#endif
		if (delta == 0)
		{
			scalefactor = 1.f;
		}
		else
		{
			// rain - scalefactor 254 to compensate for broken etmain behavior
#if defined(__linux__)
			scalefactor = (1.0L / (long double)delta) * 254.0L;
#else
			scalefactor = 254.0L / (long double)delta;
#endif
		}

		// scale properly
		for ( i = 0; i < TRACEMAP_SIZE; i++ )
		{
			for ( j = 0; j < TRACEMAP_SIZE; j++ )
			{
				if (tracemap.skyground[i][j] != MAX_WORLD_HEIGHT)
				{
#if defined(__linux__)
					/* ELF retains the reciprocal before multiplying the sample. */
					tracemap.skyground[i][j] = (float)((1.0L / scalefactor) * (long double)tracemap.skyground[i][j] + skyground_min);
#else
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
					if (nativeScaleReady && TCE_TraceMapFiniteHeight(&nativeScale, tracemap.skyground[i][j], &tracemap.skyground[i][j]))
					{
						continue;
					}
#endif
					tracemap.skyground[i][j] = (float)((long double)tracemap.skyground[i][j] / scalefactor + skyground_min);
#endif
				}
			}
		}

		// Sky
		// calculate scalefactor
		delta = TCE_SubInt32(sky_max, sky_min);
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		nativeScaleReady = TCE_TraceMapPrepareScale(delta, sky_min, &nativeScale);
#endif
		if (delta == 0)
		{
			scalefactor = 1.f;
		}
		else
		{
			// rain - scalefactor 254 to compensate for broken etmain behavior
#if defined(__linux__)
			scalefactor = (1.0L / (long double)delta) * 254.0L;
#else
			scalefactor = 254.0L / (long double)delta;
#endif
		}

		// scale properly
		for ( i = 0; i < TRACEMAP_SIZE; i++ )
		{
			for ( j = 0; j < TRACEMAP_SIZE; j++ )
			{
				if (tracemap.sky[i][j] != MAX_WORLD_HEIGHT)
				{
#if defined(__linux__)
					/* ELF retains the reciprocal before multiplying the sample. */
					tracemap.sky[i][j] = (float)((1.0L / scalefactor) * (long double)tracemap.sky[i][j] + sky_min);
#else
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
					if (nativeScaleReady && TCE_TraceMapFiniteHeight(&nativeScale, tracemap.sky[i][j], &tracemap.sky[i][j]))
					{
						continue;
					}
#endif
					tracemap.sky[i][j] = (float)((long double)tracemap.sky[i][j] / scalefactor + sky_min);
#endif
				}
			}
		}
	}
	else
	{
		return(tracemap.loaded = qfalse);
	}

	tracemap.world_mins[0] = world_mins[0];
	tracemap.world_mins[1] = world_mins[1];
	tracemap.world_maxs[0] = world_maxs[0];
	tracemap.world_maxs[1] = world_maxs[1];

	tracemap.groundfloor = ground_min;
	tracemap.groundceil = ground_max;
	tracemap.loaded = qtrue;
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	if (!TCE_TraceMapFiniteGrid(tracemap.world_maxs[0], tracemap.world_mins[0], &one_over_mapgrid_factor[0]))
#endif
	one_over_mapgrid_factor[0] = (float)(1.0L / (((long double)tracemap.world_maxs[0] - tracemap.world_mins[0]) * 0.00390625L));
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	if (!TCE_TraceMapFiniteGrid(tracemap.world_maxs[1], tracemap.world_mins[1], &one_over_mapgrid_factor[1]))
#endif
	one_over_mapgrid_factor[1] = (float)(1.0L / (((long double)tracemap.world_maxs[1] - tracemap.world_mins[1]) * 0.00390625L));


	return qtrue;
}

#else

qboolean BG_LoadTraceMap(char *rawmapname, vec2_t world_mins, vec2_t world_maxs)
{
	int          i, j;
	fileHandle_t f;
	byte         data, datablock[TRACEMAP_SIZE][4];
	int          sky_min, sky_max;
	int          ground_min, ground_max;
	int          skyground_min, skyground_max;
	float        scalefactor;
	//int startTime = trap_Milliseconds();

	ground_min    = ground_max = MIN_WORLD_HEIGHT;
	skyground_min = skyground_max = MAX_WORLD_HEIGHT;
	sky_min       = sky_max = MAX_WORLD_HEIGHT;

	if (trap_FS_FOpenFile(va("maps/%s_tracemap.tga", Q_strlwr(rawmapname)), &f, FS_READ) >= 0)
	{
		// skip over header
		for ( i = 0; i < 18; i++ )
		{
			trap_FS_Read(&data, 1, f);
		}

		for ( i = 0; i < TRACEMAP_SIZE; i++ )
		{
			trap_FS_Read(&datablock, sizeof(datablock), f);     // TRACEMAP_SIZE * { b g r a }

			for ( j = 0; j < TRACEMAP_SIZE; j++ )
			{
				if (i == 0 && j < 6)
				{
					// abuse first six pixels for our extended data
					switch (j)
					{
					case 0: ground_min    = datablock[j][0] | (datablock[j][1] << 8) | (datablock[j][2] << 16) | (datablock[j][3] << 24); break;
					case 1: ground_max    = datablock[j][0] | (datablock[j][1] << 8) | (datablock[j][2] << 16) | (datablock[j][3] << 24); break;
					case 2: skyground_min = datablock[j][0] | (datablock[j][1] << 8) | (datablock[j][2] << 16) | (datablock[j][3] << 24); break;
					case 3: skyground_max = datablock[j][0] | (datablock[j][1] << 8) | (datablock[j][2] << 16) | (datablock[j][3] << 24); break;
					case 4: sky_min       = datablock[j][0] | (datablock[j][1] << 8) | (datablock[j][2] << 16) | (datablock[j][3] << 24); break;
					case 5: sky_max       = datablock[j][0] | (datablock[j][1] << 8) | (datablock[j][2] << 16) | (datablock[j][3] << 24); break;
					}
					tracemap.sky[TRACEMAP_SIZE - 1 - i][j]       = MAX_WORLD_HEIGHT;
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j]    = MIN_WORLD_HEIGHT;
					continue;
				}

				tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[j][0];    // FIXME: swap
				if (tracemap.sky[TRACEMAP_SIZE - 1 - i][j] == 0)
				{
					tracemap.sky[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
				}

				tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[j][1];  // FIXME: swap
				if (tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] == 0)
				{
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
				}

				tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = (float)datablock[j][2]; // FIXME: swap
				if (tracemap.ground[TRACEMAP_SIZE - 1 - i][j] == 0)
				{
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j] = MIN_WORLD_HEIGHT;
				}

				if (datablock[j][3] == 0)
				{
					// just in case
					tracemap.skyground[TRACEMAP_SIZE - 1 - i][j] = MAX_WORLD_HEIGHT;
					tracemap.ground[TRACEMAP_SIZE - 1 - i][j]    = MIN_WORLD_HEIGHT;
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

		trap_FS_FCloseFile(f);

		// Ground
		// calculate scalefactor
		if (ground_max - ground_min == 0)
		{
			scalefactor = 1.f;
		}
		else
		{
			// rain - scalefactor 254 to compensate for broken etmain behavior
			scalefactor = 254.f / (ground_max - ground_min);
		}

		// scale properly
		for ( i = 0; i < TRACEMAP_SIZE; i++ )
		{
			for ( j = 0; j < TRACEMAP_SIZE; j++ )
			{
				if (tracemap.ground[i][j] != MIN_WORLD_HEIGHT)
				{
					tracemap.ground[i][j] = ground_min + (tracemap.ground[i][j] / scalefactor);
				}
			}
		}

		// SkyGround
		// calculate scalefactor
		if (skyground_max - skyground_min == 0)
		{
			scalefactor = 1.f;
		}
		else
		{
			// rain - scalefactor 254 to compensate for broken etmain behavior
			scalefactor = 254.f / (skyground_max - skyground_min);
		}

		// scale properly
		for ( i = 0; i < TRACEMAP_SIZE; i++ )
		{
			for ( j = 0; j < TRACEMAP_SIZE; j++ )
			{
				if (tracemap.skyground[i][j] != MAX_WORLD_HEIGHT)
				{
					tracemap.skyground[i][j] = skyground_min + (tracemap.skyground[i][j] / scalefactor);
				}
			}
		}

		// Sky
		// calculate scalefactor
		if (sky_max - sky_min == 0)
		{
			scalefactor = 1.f;
		}
		else
		{
			// rain - scalefactor 254 to compensate for broken etmain behavior
			scalefactor = 254.f / (sky_max - sky_min);
		}

		// scale properly
		for ( i = 0; i < TRACEMAP_SIZE; i++ )
		{
			for ( j = 0; j < TRACEMAP_SIZE; j++ )
			{
				if (tracemap.sky[i][j] != MAX_WORLD_HEIGHT)
				{
					tracemap.sky[i][j] = sky_min + (tracemap.sky[i][j] / scalefactor);
				}
			}
		}
	}
	else
	{
		return(tracemap.loaded = qfalse);
	}

	tracemap.world_mins[0] = world_mins[0];
	tracemap.world_mins[1] = world_mins[1];
	tracemap.world_maxs[0] = world_maxs[0];
	tracemap.world_maxs[1] = world_maxs[1];

	one_over_mapgrid_factor[0] = 1.f / ((tracemap.world_maxs[0] - tracemap.world_mins[0]) / (float)TRACEMAP_SIZE);
	one_over_mapgrid_factor[1] = 1.f / ((tracemap.world_maxs[1] - tracemap.world_mins[1]) / (float)TRACEMAP_SIZE);

	tracemap.groundfloor = ground_min;
	tracemap.groundceil  = ground_max;

	//Com_Printf( "^8Loaded tracemap in %i msec\n", trap_Milliseconds() - startTime );

	return(tracemap.loaded = qtrue);
}


#endif

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
static void BG_ClampPointToTracemapExtends(vec3_t point, vec2_t out)
{
	const float    *selected;
	volatile float stored;
#if !defined(__linux__)
	unsigned int raw;
#endif
#if defined(__linux__)
	/* ELF compares the bound against the point: unordered reaches upper. */
	if ((long double)tracemap.world_mins[0] > (long double)point[0])
	{
		selected = &tracemap.world_mins[0];
	}
	else if (!((long double)tracemap.world_maxs[0] >= (long double)point[0]))
	{
		selected = &tracemap.world_maxs[0];
	}
	else
	{
		selected = &point[0];
	}
	stored = (float)(long double)*selected;
	out[0] = stored;
	if ((long double)tracemap.world_maxs[1] > (long double)point[1])
	{
		selected = &tracemap.world_maxs[1];
	}
	else if (!((long double)tracemap.world_mins[1] >= (long double)point[1]))
	{
		selected = &tracemap.world_mins[1];
	}
	else
	{
		selected = &point[1];
	}
	stored = (float)(long double)*selected;
	out[1] = stored;
#else
	/* PE C0 lower gate accepts unordered; upper gate is ordered greater. */
	if (!((long double)point[0] >= (long double)tracemap.world_mins[0]))
	{
		stored = (float)(long double)tracemap.world_mins[0];
		out[0] = stored;
	}
	else if ((long double)point[0] > (long double)tracemap.world_maxs[0])
	{
		stored = (float)(long double)tracemap.world_maxs[0];
		out[0] = stored;
	}
	else
	{
		memcpy(&raw, &point[0], sizeof(raw));
		memcpy(&out[0], &raw, sizeof(raw));
	}
	/* Original Y outputs are raw copies; read Y only after writing X. */
	if (!((long double)point[1] >= (long double)tracemap.world_maxs[1]))
	{
		selected = &tracemap.world_maxs[1];
	}
	else if ((long double)point[1] > (long double)tracemap.world_mins[1])
	{
		selected = &tracemap.world_mins[1];
	}
	else
	{
		selected = &point[1];
	}
	memcpy(&raw, selected, sizeof(raw));
	memcpy(&out[1], &raw, sizeof(raw));
#endif
}
#else

static void BG_ClampPointToTracemapExtends(vec3_t point, vec2_t out)
{

	if (point[0] < tracemap.world_mins[0])
	{
		out[0] = tracemap.world_mins[0];
	}
	else if (point[0] > tracemap.world_maxs[0])
	{
		out[0] = tracemap.world_maxs[0];
	}
	else
	{
		out[0] = point[0];
	}

	if (point[1] < tracemap.world_maxs[1])
	{
		out[1] = tracemap.world_maxs[1];
	}
	else if (point[1] > tracemap.world_mins[1])
	{
		out[1] = tracemap.world_mins[1];
	}
	else
	{
		out[1] = point[1];
	}
}


#endif

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
static void BG_TracemapQueryIndices(vec3_t pos, int *x, int *y)
{
	vec2_t         point;
	long double    coordinate;
	volatile float stored;
	BG_ClampPointToTracemapExtends(pos, point);
	coordinate = (long double)point[0] - (long double)tracemap.world_mins[0];
	coordinate = coordinate * (long double)one_over_mapgrid_factor[0];
	stored     = (float)coordinate;
	*x         = (int)myftol(stored);
	/* The second conversion follows the first call, including its FP effects. */
	coordinate = (long double)point[1] - (long double)tracemap.world_mins[1];
	coordinate = coordinate * (long double)one_over_mapgrid_factor[1];
	stored     = (float)coordinate;
	*y         = (int)myftol(stored);
	etpro_FinalizeTracemapClamp(x, y);
}
#endif

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
float BG_GetSkyHeightAtPoint(vec3_t pos)
{
	int x, y;
	if (!tracemap.loaded)
	{
		return MAX_WORLD_HEIGHT;
	}
	BG_TracemapQueryIndices(pos, &x, &y);
	return tracemap.sky[y][x];
}
#else

float BG_GetSkyHeightAtPoint(vec3_t pos)
{
	int    i, j;
	vec2_t point;
//	int msec = trap_Milliseconds();

//	n_getskytime++;

	if (!tracemap.loaded)
	{
//		getskytime += trap_Milliseconds() - msec;
		return MAX_WORLD_HEIGHT;
	}

	BG_ClampPointToTracemapExtends(pos, point);

	i = myftol((point[0] - tracemap.world_mins[0]) * one_over_mapgrid_factor[0]);
	j = myftol((point[1] - tracemap.world_mins[1]) * one_over_mapgrid_factor[1]);

	// rain - re-clamp the points, because a rounding error can cause
	// them to go outside the array
	etpro_FinalizeTracemapClamp(&i, &j);

//	getskytime += trap_Milliseconds() - msec;
	return(tracemap.sky[j][i]);
}


#endif

#if ((defined(CGAMEDLL) && defined(_WIN32)) || defined(__linux__)) && !defined(Q3_VM)
float BG_GetSkyGroundHeightAtPoint(vec3_t pos)
{
	int x, y;
	if (!tracemap.loaded)
	{
		return MAX_WORLD_HEIGHT;
	}
	BG_TracemapQueryIndices(pos, &x, &y);
	return tracemap.skyground[y][x];
}
#else

float BG_GetSkyGroundHeightAtPoint(vec3_t pos)
{
	int    i, j;
	vec2_t point;
//	int msec = trap_Milliseconds();

//	n_getgroundtime++;

	if (!tracemap.loaded)
	{
//		getgroundtime += trap_Milliseconds() - msec;
		return MAX_WORLD_HEIGHT;
	}

	BG_ClampPointToTracemapExtends(pos, point);

	i = myftol((point[0] - tracemap.world_mins[0]) * one_over_mapgrid_factor[0]);
	j = myftol((point[1] - tracemap.world_mins[1]) * one_over_mapgrid_factor[1]);

	// rain - re-clamp the points, because a rounding error can cause
	// them to go outside the array
	etpro_FinalizeTracemapClamp(&i, &j);

//	getgroundtime += trap_Milliseconds() - msec;
	return(tracemap.skyground[j][i]);
}


#endif

#if ((defined(GAMEDLL) && defined(_WIN32)) || defined(__linux__)) && !defined(Q3_VM)
float BG_GetGroundHeightAtPoint(vec3_t pos)
{
	int x, y;
	if (!tracemap.loaded)
	{
		return MIN_WORLD_HEIGHT;
	}
	BG_TracemapQueryIndices(pos, &x, &y);
	return tracemap.ground[y][x];
}
#else
float BG_GetGroundHeightAtPoint(vec3_t pos)
{
	int    i, j;
	vec2_t point;
//	int msec = trap_Milliseconds();

//	n_getgroundtime++;

	if (!tracemap.loaded)
	{
//		getgroundtime += trap_Milliseconds() - msec;
		return MIN_WORLD_HEIGHT;
	}

	BG_ClampPointToTracemapExtends(pos, point);

	i = myftol((point[0] - tracemap.world_mins[0]) * one_over_mapgrid_factor[0]);
	j = myftol((point[1] - tracemap.world_mins[1]) * one_over_mapgrid_factor[1]);

	// rain - re-clamp the points, because a rounding error can cause
	// them to go outside the array
	etpro_FinalizeTracemapClamp(&i, &j);

//	getgroundtime += trap_Milliseconds() - msec;
	return(tracemap.ground[j][i]);
}

#endif

int BG_GetTracemapGroundFloor(void)
{
	if (!tracemap.loaded)
	{
		return MIN_WORLD_HEIGHT;
	}
	return tracemap.groundfloor;
}

int BG_GetTracemapGroundCeil(void)
{
	if (!tracemap.loaded)
	{
		return MAX_WORLD_HEIGHT;
	}
	return tracemap.groundceil;
}

// rain - re-clamp the points, because a rounding error can cause
// them to go outside the array
void etpro_FinalizeTracemapClamp(int *x, int *y)
{
	if (*x < 0)
	{
		*x = 0;
	}
	else if (*x > TRACEMAP_SIZE - 1)
	{
		*x = TRACEMAP_SIZE - 1;
	}

	if (*y < 0)
	{
		*y = 0;
	}
	else if (*y > TRACEMAP_SIZE - 1)
	{
		*y = TRACEMAP_SIZE - 1;
	}
}
