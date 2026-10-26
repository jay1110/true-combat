#if !defined(_MSC_VER)
#include <math.h>
#define _finite isfinite
#endif
#include "cg_local.h"
#include "tce_lightgrid.h"
#include <float.h>

#define TCE_GRID_BYTES 8000000
static byte mapSamples[TCE_GRID_BYTES];
static float           mapSine[1024];
static tce_lightGrid_t mapGrid;
static qboolean        gridReady;
vec3_t                 tce_lightGridSpacing = { 64, 64, 128 };

#if defined(__linux__) && !defined(Q3_VM) && \
    LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
#define TCE_LINUX_LIGHTGRID_X87
#endif

#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(TCE_LINUX_LIGHTGRID_X87) || (defined(_WIN32) && !defined(__linux__) && !defined(Q3_VM))
/* Preserve platform-specific integral rounding and stored grid boundaries. */
static void TCE_LightGridAxis(float gridMin, float gridMax,
                              float gridStep, float *gridOrigin, float *gridBound, double *guardDimension)
{
#if defined(TCE_LINUX_LIGHTGRID_X87)
	volatile long double reciprocal, quotient, origin, span, scaledSpan, dimension;
	volatile double rounded;
	volatile float high;

	/* ELF rounds the retained quotient, then spills the integer to Double. */
	reciprocal = 1.0L / (long double)gridStep;
	quotient = reciprocal * (long double)gridMin;
	rounded = (double)ceill(quotient);
	origin = (long double)rounded * (long double)gridStep;
	*gridOrigin = (float)origin;

	reciprocal = 1.0L / (long double)gridStep;
	quotient = reciprocal * (long double)gridMax;
	rounded = (double)floorl(quotient);
	high = (float)((long double)rounded * (long double)gridStep);
	reciprocal = 1.0L / (long double)gridStep;
	span = (long double)high - (long double)*gridOrigin;
	scaledSpan = span * reciprocal;
	dimension = scaledSpan + 1.0L;
	*guardDimension = (double)dimension;
	*gridBound = (float)dimension;
#else
	double      quotient;
	long double high, dimension;

	quotient        = (double)((long double)gridMin / (long double)gridStep);
	*gridOrigin     = (float)((long double)ceil(quotient) * (long double)gridStep);
	quotient        = (double)((long double)gridMax / (long double)gridStep);
	high            = (long double)floor(quotient) * (long double)gridStep;
	dimension       = (high - (long double)*gridOrigin) / (long double)gridStep + 1.0L;
	*guardDimension = (double)dimension;
	*gridBound      = (float)dimension;
#endif
}

#if defined(TCE_LINUX_LIGHTGRID_X87)
static long double TCE_LightGridCountValue(const float *gridBounds)
{
	volatile long double count = (long double)gridBounds[1] * (long double)gridBounds[0];
	count = count * (long double)gridBounds[2];
	return count;
}
#else
static long double TCE_LightGridCountValue(const float *gridBounds)
{
	long double count = (long double)gridBounds[2] * (long double)gridBounds[1];

	return count * (long double)gridBounds[0];
}
#endif
#endif

/* CG_Init fills this table before sound/graphics registration. */
void TCE_CG_InitLightSine(void)
{
	int i;
	for (i = 0; i < 1024; ++i)
	{
#if defined(__linux__) && !defined(Q3_VM)
		/* Linux keeps the binary32 step and binary64 radians factor in x87. */
		mapSine[i] = (float)sinl((long double)i * 0.3519061505794525f * 0.017453292519943295);
#else
		mapSine[i] = (float)sin((double)i * 0.3519061505794525f * 3.1415927410125732f * 0.0055555556900799274f);
#endif
	}
}

const tce_lightGrid_t *TCE_CG_MapLightGrid(void)
{
	return gridReady ? &mapGrid : NULL;
}

/* The original advances only forward, including when lump offsets overlap.
 * Batch its byte-at-a-time discard without changing the resulting position. */
static qboolean SkipGridBytes(fileHandle_t file, int length, int *position, int bytes)
{
	byte discard[4096];
	int  n;
	if (bytes <= 0)
	{
		return qtrue;
	}
	if (bytes > length - *position)
	{
		return qfalse;
	}
	while (bytes > 0)
	{
		n = bytes > (int)sizeof(discard)?(int)sizeof(discard):bytes;
		trap_FS_Read(discard, n, file);
		*position += n; bytes -= n;
	}
	return qtrue;
}

qboolean TCE_CG_LoadLightGrid(const char *mapname)
{
	int          header[49], position = 196, length, i, shaders, models, lightOffset;
	int          lightBytes, sampleBytes, sampleCount;
	fileHandle_t file = 0;
	float        block[256];
	double       dimension, count;
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(TCE_LINUX_LIGHTGRID_X87) || (defined(_WIN32) && !defined(__linux__) && !defined(Q3_VM))
	long double retainedCount;
#endif
#if !((defined(_MSC_VER) && defined(_M_IX86)) || defined(TCE_LINUX_LIGHTGRID_X87) || (defined(_WIN32) && !defined(__linux__) && !defined(Q3_VM)))
	double high;
#endif
	tce_lightGrid_t grid;
	gridReady = qfalse;
	memset(&mapGrid, 0, sizeof(mapGrid));
	memset(mapSamples, 0, sizeof(mapSamples));
	length = trap_FS_FOpenFile(mapname, &file, FS_READ);
	if (!file || length < (int)sizeof(header))
	{
		goto invalid;
	}
	trap_FS_Read(header, sizeof(header), file);
	shaders     = header[4]; models = header[16];
	lightOffset = header[32]; lightBytes = header[33];
	/* Native file/buffer guards replace original unchecked stack reads. */
	if (shaders < 0 || models < 0 || lightOffset < 0)
	{
		goto invalid;
	}
	if (!SkipGridBytes(file, length, &position, shaders - 196) || length - position < 1024)
	{
		goto invalid;
	}
	trap_FS_Read(block, 1024, file); position += 1024;
	if (!SkipGridBytes(file, length, &position,
	                   models > shaders && models - shaders > 1024?models - shaders - 1024:0) || length - position < 1024)
	{
		goto invalid;
	}
	trap_FS_Read(block, 1024, file); position += 1024;
	for (i = 0; i < 3; ++i)
	{
		grid.spacing[i] = tce_lightGridSpacing[i];
		if (!_finite(grid.spacing[i]) || grid.spacing[i] <= 0 ||
		    !_finite(block[i]) || !_finite(block[i + 3]))
		{
			goto invalid;
		}
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(TCE_LINUX_LIGHTGRID_X87) || (defined(_WIN32) && !defined(__linux__) && !defined(Q3_VM))
		TCE_LightGridAxis(block[i], block[i + 3], grid.spacing[i],
		                  &grid.origin[i], &grid.bounds[i], &dimension);
#else
		/* Portable fallback: not a retained-x87 precision claim. */
		grid.origin[i] = (float)(ceil((double)block[i] / grid.spacing[i]) * grid.spacing[i]);
		high           = floor((double)block[i + 3] / grid.spacing[i]) * grid.spacing[i];
		dimension      = (high - grid.origin[i]) / grid.spacing[i] + 1.0;
		grid.bounds[i] = (float)dimension;
#endif
		if (!_finite(dimension) || dimension < 1 || dimension > TCE_GRID_BYTES / 8)
		{
			goto invalid;
		}
	}
	/* Each platform keeps its original product order before truncation. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(TCE_LINUX_LIGHTGRID_X87) || (defined(_WIN32) && !defined(__linux__) && !defined(Q3_VM))
	retainedCount = TCE_LightGridCountValue(grid.bounds);
	count = (double)retainedCount;
#else
	count = (double)grid.bounds[2] * grid.bounds[1] * grid.bounds[0];
#endif
	if (!_finite(count) || count < 1 || count > TCE_GRID_BYTES / 8)
	{
		goto invalid;
	}
	/* The capacity guard makes this conversion defined on every target. */
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(TCE_LINUX_LIGHTGRID_X87) || (defined(_WIN32) && !defined(__linux__) && !defined(Q3_VM))
	sampleCount = (int)retainedCount;
#else
	sampleCount = (int)count;
#endif
	if (!SkipGridBytes(file, length, &position,
	                   lightOffset > models && lightOffset - models > 1024?lightOffset - models - 1024:0))
	{
		goto invalid;
	}
	/* Original requests the fixed eight-million-byte sample array. Bound the
	 * native read by remaining file bytes; zero tail corresponds to fresh cg. */
	sampleBytes = length - position;
	if (sampleBytes > TCE_GRID_BYTES)
	{
		sampleBytes = TCE_GRID_BYTES;
	}
	if (sampleBytes > 0)
	{
		trap_FS_Read(mapSamples, sampleBytes, file);
	}
	if (lightBytes != sampleCount * 8)
	{
		CG_Printf("^3WARNING: lightgrid size mismatch\n");
	}
	trap_FS_FCloseFile(file);
	grid.samples = mapSamples; grid.sine = mapSine; mapGrid = grid; gridReady = qtrue;
	return qtrue;
invalid:
	if (file)
	{
		trap_FS_FCloseFile(file);
	}
	CG_Printf("^3WARNING: TCE lightgrid unavailable or invalid: %s\n", mapname);
	return qfalse;
}
