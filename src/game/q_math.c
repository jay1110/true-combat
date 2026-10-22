// Copyright (C) 1999-2000 Id Software, Inc.
//
// q_math.c -- stateless support routines that are included in each code module
#include "q_shared.h"
#include "tce_numeric.h"
#include <float.h>


vec3_t vec3_origin    = { 0, 0, 0 };
vec3_t axisDefault[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };


vec4_t colorBlack    = { 0, 0, 0, 1 };
vec4_t colorRed      = { 1, 0, 0, 1 };
vec4_t colorGreen    = { 0, 1, 0, 1 };
vec4_t colorBlue     = { 0, 0, 1, 1 };
vec4_t colorYellow   = { 1, 1, 0, 1 };
vec4_t colorOrange   = { 1, 0.5, 0, 1 };
vec4_t colorMagenta  = { 1, 0, 1, 1 };
vec4_t colorCyan     = { 0, 1, 1, 1 };
vec4_t colorWhite    = { 1, 1, 1, 1 };
vec4_t colorLtGrey   = { 0.75, 0.75, 0.75, 1 };
vec4_t colorMdGrey   = { 0.5, 0.5, 0.5, 1 };
vec4_t colorDkGrey   = { 0.25, 0.25, 0.25, 1 };
vec4_t colorMdRed    = { 0.5, 0, 0, 1 };
vec4_t colorMdGreen  = { 0, 0.5, 0, 1 };
vec4_t colorDkGreen  = { 0, 0.20, 0, 1 };
vec4_t colorMdCyan   = { 0, 0.5, 0.5, 1 };
vec4_t colorMdYellow = { 0.5, 0.5, 0, 1 };
vec4_t colorMdOrange = { 0.5, 0.25, 0, 1 };
vec4_t colorMdBlue   = { 0, 0, 0.5, 1 };

vec4_t clrBrown         = { 0.68f, 0.68f, 0.56f, 1.f };
vec4_t clrBrownDk       = { 0.58f * 0.75f, 0.58f * 0.75f, 0.46f * 0.75f, 1.f };
vec4_t clrBrownLine     = { 0.0525f, 0.05f, 0.025f, 0.2f };
vec4_t clrBrownLineFull = { 0.0525f, 0.05f, 0.025f, 1.f };

vec4_t clrBrownTextLt2 = { 108 * 1.8 / 255.f, 88 * 1.8 / 255.f, 62 * 1.8 / 255.f, 1.f };
vec4_t clrBrownTextLt  = { 108 * 1.3 / 255.f, 88 * 1.3 / 255.f, 62 * 1.3 / 255.f, 1.f };
vec4_t clrBrownText    = { 108 / 255.f, 88 / 255.f, 62 / 255.f, 1.f };
vec4_t clrBrownTextDk  = { 20 / 255.f, 2 / 255.f, 0 / 255.f, 1.f };
vec4_t clrBrownTextDk2 = { 108 * 0.75 / 255.f, 88 * 0.75 / 255.f, 62 * 0.75 / 255.f, 1.f };

vec4_t g_color_table[32] =
{
	{ 0.0,  0.0,  0.0,  1.0 },          // 0 - black		0
	{ 0.7f, 0.0,  0.0,  1.0 },          // 1 - red			1
	{ 0.0,  0.7f, 0.0,  1.0 },          // 2 - green		2
	{ 1.0,  1.0,  0.0,  1.0 },          // 3 - yellow		3
	{ 0.0,  0.0,  0.7f, 1.0 },          // 4 - blue			4
	{ 0.0,  1.0,  1.0,  1.0 },          // 5 - cyan			5
	{ 1.0,  0.0,  1.0,  1.0 },          // 6 - purple		6
	{ 1.0,  1.0,  1.0,  1.0 },          // 7 - white		7
	{ 1.0,  0.5,  0.0,  1.0 },          // 8 - orange		8
	{ 0.5,  0.5,  0.5,  1.0 },          // 9 - md.grey		9
	{ 0.75, 0.75, 0.75, 1.0 },          // : - lt.grey		10		// lt grey for names
	{ 0.75, 0.75, 0.75, 1.0 },          // ; - lt.grey		11
	{ 0.0,  0.5,  0.0,  1.0 },          // < - md.green		12
	{ 0.5,  0.5,  0.0,  1.0 },          // = - md.yellow	13
	{ 0.0,  0.0,  0.5,  1.0 },          // > - md.blue		14
	{ 0.5,  0.0,  0.0,  1.0 },          // ? - md.red		15
	{ 0.5,  0.25, 0.0,  1.0 },          // @ - md.orange	16
	{ 1.0,  0.6f, 0.1f, 1.0 },          // A - lt.orange	17
	{ 0.0,  0.5,  0.5,  1.0 },          // B - md.cyan		18
	{ 0.5,  0.0,  0.5,  1.0 },          // C - md.purple	19
	{ 0.0,  0.5,  1.0,  1.0 },          // D				20
	{ 0.5,  0.0,  1.0,  1.0 },          // E				21
	{ 0.2f, 0.6f, 0.8f, 1.0 },          // F				22
	{ 0.8f, 1.0,  0.8f, 1.0 },          // G				23
	{ 0.0,  0.4,  0.2f, 1.0 },          // H				24
	{ 1.0,  0.0,  0.2f, 1.0 },          // I				25
	{ 0.7f, 0.1f, 0.1f, 1.0 },          // J				26
	{ 0.6f, 0.2f, 0.0,  1.0 },          // K				27
	{ 0.8f, 0.6f, 0.2f, 1.0 },          // L				28
	{ 0.6f, 0.6f, 0.2f, 1.0 },          // M				29
	{ 1.0,  1.0,  0.75, 1.0 },          // N				30
	{ 1.0,  1.0,  0.5,  1.0 },          // O				31
};



vec3_t bytedirs[NUMVERTEXNORMALS] =
{
	{ -0.525731, 0.000000,  0.850651  }, { -0.442863, 0.238856,  0.864188  },
	{ -0.295242, 0.000000,  0.955423  }, { -0.309017, 0.500000,  0.809017  },
	{ -0.162460, 0.262866,  0.951056  }, { 0.000000,  0.000000,  1.000000  },
	{ 0.000000,  0.850651,  0.525731  }, { -0.147621, 0.716567,  0.681718  },
	{ 0.147621,  0.716567,  0.681718  }, { 0.000000,  0.525731,  0.850651  },
	{ 0.309017,  0.500000,  0.809017  }, { 0.525731,  0.000000,  0.850651  },
	{ 0.295242,  0.000000,  0.955423  }, { 0.442863,  0.238856,  0.864188  },
	{ 0.162460,  0.262866,  0.951056  }, { -0.681718, 0.147621,  0.716567  },
	{ -0.809017, 0.309017,  0.500000  }, { -0.587785, 0.425325,  0.688191  },
	{ -0.850651, 0.525731,  0.000000  }, { -0.864188, 0.442863,  0.238856  },
	{ -0.716567, 0.681718,  0.147621  }, { -0.688191, 0.587785,  0.425325  },
	{ -0.500000, 0.809017,  0.309017  }, { -0.238856, 0.864188,  0.442863  },
	{ -0.425325, 0.688191,  0.587785  }, { -0.716567, 0.681718,  -0.147621 },
	{ -0.500000, 0.809017,  -0.309017 }, { -0.525731, 0.850651,  0.000000  },
	{ 0.000000,  0.850651,  -0.525731 }, { -0.238856, 0.864188,  -0.442863 },
	{ 0.000000,  0.955423,  -0.295242 }, { -0.262866, 0.951056,  -0.162460 },
	{ 0.000000,  1.000000,  0.000000  }, { 0.000000,  0.955423,  0.295242  },
	{ -0.262866, 0.951056,  0.162460  }, { 0.238856,  0.864188,  0.442863  },
	{ 0.262866,  0.951056,  0.162460  }, { 0.500000,  0.809017,  0.309017  },
	{ 0.238856,  0.864188,  -0.442863 }, { 0.262866,  0.951056,  -0.162460 },
	{ 0.500000,  0.809017,  -0.309017 }, { 0.850651,  0.525731,  0.000000  },
	{ 0.716567,  0.681718,  0.147621  }, { 0.716567,  0.681718,  -0.147621 },
	{ 0.525731,  0.850651,  0.000000  }, { 0.425325,  0.688191,  0.587785  },
	{ 0.864188,  0.442863,  0.238856  }, { 0.688191,  0.587785,  0.425325  },
	{ 0.809017,  0.309017,  0.500000  }, { 0.681718,  0.147621,  0.716567  },
	{ 0.587785,  0.425325,  0.688191  }, { 0.955423,  0.295242,  0.000000  },
	{ 1.000000,  0.000000,  0.000000  }, { 0.951056,  0.162460,  0.262866  },
	{ 0.850651,  -0.525731, 0.000000  }, { 0.955423,  -0.295242, 0.000000  },
	{ 0.864188,  -0.442863, 0.238856  }, { 0.951056,  -0.162460, 0.262866  },
	{ 0.809017,  -0.309017, 0.500000  }, { 0.681718,  -0.147621, 0.716567  },
	{ 0.850651,  0.000000,  0.525731  }, { 0.864188,  0.442863,  -0.238856 },
	{ 0.809017,  0.309017,  -0.500000 }, { 0.951056,  0.162460,  -0.262866 },
	{ 0.525731,  0.000000,  -0.850651 }, { 0.681718,  0.147621,  -0.716567 },
	{ 0.681718,  -0.147621, -0.716567 }, { 0.850651,  0.000000,  -0.525731 },
	{ 0.809017,  -0.309017, -0.500000 }, { 0.864188,  -0.442863, -0.238856 },
	{ 0.951056,  -0.162460, -0.262866 }, { 0.147621,  0.716567,  -0.681718 },
	{ 0.309017,  0.500000,  -0.809017 }, { 0.425325,  0.688191,  -0.587785 },
	{ 0.442863,  0.238856,  -0.864188 }, { 0.587785,  0.425325,  -0.688191 },
	{ 0.688191,  0.587785,  -0.425325 }, { -0.147621, 0.716567,  -0.681718 },
	{ -0.309017, 0.500000,  -0.809017 }, { 0.000000,  0.525731,  -0.850651 },
	{ -0.525731, 0.000000,  -0.850651 }, { -0.442863, 0.238856,  -0.864188 },
	{ -0.295242, 0.000000,  -0.955423 }, { -0.162460, 0.262866,  -0.951056 },
	{ 0.000000,  0.000000,  -1.000000 }, { 0.295242,  0.000000,  -0.955423 },
	{ 0.162460,  0.262866,  -0.951056 }, { -0.442863, -0.238856, -0.864188 },
	{ -0.309017, -0.500000, -0.809017 }, { -0.162460, -0.262866, -0.951056 },
	{ 0.000000,  -0.850651, -0.525731 }, { -0.147621, -0.716567, -0.681718 },
	{ 0.147621,  -0.716567, -0.681718 }, { 0.000000,  -0.525731, -0.850651 },
	{ 0.309017,  -0.500000, -0.809017 }, { 0.442863,  -0.238856, -0.864188 },
	{ 0.162460,  -0.262866, -0.951056 }, { 0.238856,  -0.864188, -0.442863 },
	{ 0.500000,  -0.809017, -0.309017 }, { 0.425325,  -0.688191, -0.587785 },
	{ 0.716567,  -0.681718, -0.147621 }, { 0.688191,  -0.587785, -0.425325 },
	{ 0.587785,  -0.425325, -0.688191 }, { 0.000000,  -0.955423, -0.295242 },
	{ 0.000000,  -1.000000, 0.000000  }, { 0.262866,  -0.951056, -0.162460 },
	{ 0.000000,  -0.850651, 0.525731  }, { 0.000000,  -0.955423, 0.295242  },
	{ 0.238856,  -0.864188, 0.442863  }, { 0.262866,  -0.951056, 0.162460  },
	{ 0.500000,  -0.809017, 0.309017  }, { 0.716567,  -0.681718, 0.147621  },
	{ 0.525731,  -0.850651, 0.000000  }, { -0.238856, -0.864188, -0.442863 },
	{ -0.500000, -0.809017, -0.309017 }, { -0.262866, -0.951056, -0.162460 },
	{ -0.850651, -0.525731, 0.000000  }, { -0.716567, -0.681718, -0.147621 },
	{ -0.716567, -0.681718, 0.147621  }, { -0.525731, -0.850651, 0.000000  },
	{ -0.500000, -0.809017, 0.309017  }, { -0.238856, -0.864188, 0.442863  },
	{ -0.262866, -0.951056, 0.162460  }, { -0.864188, -0.442863, 0.238856  },
	{ -0.809017, -0.309017, 0.500000  }, { -0.688191, -0.587785, 0.425325  },
	{ -0.681718, -0.147621, 0.716567  }, { -0.442863, -0.238856, 0.864188  },
	{ -0.587785, -0.425325, 0.688191  }, { -0.309017, -0.500000, 0.809017  },
	{ -0.147621, -0.716567, 0.681718  }, { -0.425325, -0.688191, 0.587785  },
	{ -0.162460, -0.262866, 0.951056  }, { 0.442863,  -0.238856, 0.864188  },
	{ 0.162460,  -0.262866, 0.951056  }, { 0.309017,  -0.500000, 0.809017  },
	{ 0.147621,  -0.716567, 0.681718  }, { 0.000000,  -0.525731, 0.850651  },
	{ 0.425325,  -0.688191, 0.587785  }, { 0.587785,  -0.425325, 0.688191  },
	{ 0.688191,  -0.587785, 0.425325  }, { -0.955423, 0.295242,  0.000000  },
	{ -0.951056, 0.162460,  0.262866  }, { -1.000000, 0.000000,  0.000000  },
	{ -0.850651, 0.000000,  0.525731  }, { -0.955423, -0.295242, 0.000000  },
	{ -0.951056, -0.162460, 0.262866  }, { -0.864188, 0.442863,  -0.238856 },
	{ -0.951056, 0.162460,  -0.262866 }, { -0.809017, 0.309017,  -0.500000 },
	{ -0.864188, -0.442863, -0.238856 }, { -0.951056, -0.162460, -0.262866 },
	{ -0.809017, -0.309017, -0.500000 }, { -0.681718, 0.147621,  -0.716567 },
	{ -0.681718, -0.147621, -0.716567 }, { -0.850651, 0.000000,  -0.525731 },
	{ -0.688191, 0.587785,  -0.425325 }, { -0.587785, 0.425325,  -0.688191 },
	{ -0.425325, 0.688191,  -0.587785 }, { -0.425325, -0.688191, -0.587785 },
	{ -0.587785, -0.425325, -0.688191 }, { -0.688191, -0.587785, -0.425325 }
};

//==============================================================

/* The native seed is a signed32 object with a modulo32 update. Unsigned
 * arithmetic defines the overflow; copying its bits avoids an out-of-range
 * unsigned-to-signed conversion. The returned value is the value just stored. */
typedef char tceRandomSeedLayout_t[(CHAR_BIT == 8 && sizeof(int) == 4 && sizeof(unsigned int) == 4 &&
                                    UINT_MAX == 0xffffffffu && INT_MAX == 2147483647 &&
                                    INT_MIN == (-2147483647 - 1)) ? 1 : -1];

int Q_rand(int *seed)
{
	unsigned int next = (unsigned int)*seed * 69069u + 1u;
	int          result;
	memcpy(&result, &next, sizeof(result));
	*seed = result;
	return result;
}

/* The masked seed has at most 16 significant bits. Both the scale and
 * the centered result are exact binary32 values for every int seed. */
typedef char tceRandomFloatLayout_t[(sizeof(float) == 4 && FLT_RADIX == 2 &&
                                     FLT_MANT_DIG == 24 && FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128) ? 1 : -1];

float Q_random(int *seed)
{
	unsigned int low = (unsigned int)Q_rand(seed) & 0xffffu;
	return (float)low * 0.0000152587890625f;
}

float Q_crandom(int *seed)
{
	long double centered = (long double)Q_random(seed) - 0.5L;
#if defined(__linux__)
	/* The ELF body stores and reloads its final result as float. */
	volatile float result = (float)(centered + centered);
	return result;
#else
	return (float)(centered + centered);
#endif
}


//=======================================================

signed char ClampChar(int i)
{
	if (i < -128)
	{
		return -128;
	}
	if (i > 127)
	{
		return 127;
	}
	return i;
}

signed short ClampShort(int i)
{
	if (i < -32768)
	{
		return -32768;
	}
	if (i > 0x7fff)
	{
		return 0x7fff;
	}
	return i;
}


// this isn't a real cheap function to call!
int DirToByte(vec3_t dir)
{
#if defined(_WIN32) || defined(__linux__)
	int i, best = 0;
#if defined(_WIN32)
	volatile float bestDot = 0.0f;
#else
	long double bestDot = 0.0L;
	long double x, y, z;
#endif
	if (!dir)
	{
		return 0;
	}
#if !defined(_WIN32)
	/* The ELF body loads the input once, before scanning the table. */
	x = dir[0];
	y = dir[1];
	z = dir[2];
#endif
	for (i = 0; i < NUMVERTEXNORMALS; ++i)
	{
		const float *row = bytedirs[i];
		long double dot;
#if defined(_WIN32)
		/* Windows compares the held dot against a float-rounded winner. */
		dot  = (long double)row[0] * dir[0];
		dot += (long double)row[2] * dir[2];
		dot += (long double)dir[1] * row[1];
#else
		dot  = y * row[1];
		dot += x * row[0];
		dot += z * row[2];
#endif
		/* Equal or unordered comparisons do not replace the stored winner. */
		if (dot > bestDot)
		{
#if defined(_WIN32)
			bestDot = (float)dot;
#else
			bestDot = dot;
#endif
			best = i;
		}
	}
	return best;
#else
	int   i, best;
	float d, bestd;

	if (!dir)
	{
		return 0;
	}

	bestd = 0;
	best  = 0;
	for (i = 0 ; i < NUMVERTEXNORMALS ; i++)
	{
		d = DotProduct(dir, bytedirs[i]);
		if (d > bestd)
		{
			bestd = d;
			best  = i;
		}
	}

	return best;
#endif
}

#if defined(__linux__) || (defined(_WIN32) && defined(CGAMEDLL))
/* Copy one float representation at a time. A whole-vector memcpy would
 * change the original load/store order when source and destination overlap. */
static void TCE_CopyDirectionComponent(float *destination, const float *source)
{
	unsigned int representation;
	memcpy(&representation, source, sizeof(representation));
	memcpy(destination, &representation, sizeof(representation));
}

void ByteToDir(int b, vec3_t dir)
{
	const float *source = (b < 0 || b >= NUMVERTEXNORMALS) ? vec3_origin : bytedirs[b];
#if defined(_WIN32) && defined(CGAMEDLL)
	if (b < 0 || b >= NUMVERTEXNORMALS)
	{
		/* Only the invalid Windows-CGame x component takes a float transfer. */
		volatile float x = source[0];
		dir[0] = x;
	}
	else
#endif
	{
		TCE_CopyDirectionComponent(&dir[0], &source[0]);
	}
	TCE_CopyDirectionComponent(&dir[1], &source[1]);
	TCE_CopyDirectionComponent(&dir[2], &source[2]);
}
#else
void ByteToDir(int b, vec3_t dir)
{
	if (b < 0 || b >= NUMVERTEXNORMALS)
	{
		VectorCopy(vec3_origin, dir);
		return;
	}
	VectorCopy(bytedirs[b], dir);
}
#endif

unsigned ColorBytes3(float r, float g, float b)
{
	unsigned i;

	((byte *)&i)[0] = r * 255;
	((byte *)&i)[1] = g * 255;
	((byte *)&i)[2] = b * 255;

	return i;
}

unsigned ColorBytes4(float r, float g, float b, float a)
{
	unsigned i;

	((byte *)&i)[0] = r * 255;
	((byte *)&i)[1] = g * 255;
	((byte *)&i)[2] = b * 255;
	((byte *)&i)[3] = a * 255;

	return i;
}

float NormalizeColor(const vec3_t in, vec3_t out)
{
	float max;

	max = in[0];
	if (in[1] > max)
	{
		max = in[1];
	}
	if (in[2] > max)
	{
		max = in[2];
	}

#if defined(__linux__) && !defined(Q3_VM)
	if (TCE_FloatIsZeroOrNaN(max))
#else
	if (!max)
#endif
	{
		VectorClear(out);
	}
	else
	{
		out[0] = in[0] / max;
		out[1] = in[1] / max;
		out[2] = in[2] / max;
	}
	return max;
}


/*
=====================
PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
qboolean PlaneFromPoints(vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c)
{
	vec3_t d1, d2;

	VectorSubtract(b, a, d1);
	VectorSubtract(c, a, d2);
	CrossProduct(d2, d1, plane);
#if defined(__linux__) && !defined(Q3_VM)
	/* VectorNormalize returns a nonnegative length or NaN. */
	if (!(VectorNormalize(plane) > 0.0f))
#else
	if (VectorNormalize(plane) == 0)
#endif
	{
		return qfalse;
	}

	plane[3] = DotProduct(a, plane);
	return qtrue;
}

/*
===============
RotatePointAroundVector

This is not implemented very well...
===============
*/
#if defined(_WIN32) || defined(__linux__)
/* Native FSIN/FCOS leave finite operands outside their reduction range intact. */
static long double TCE_RotationTrigValue(long double angle, qboolean cosine)
{
	if ((angle >= 9223372036854775808.0L || angle <= -9223372036854775808.0L) &&
	    angle <= LDBL_MAX && angle >= -LDBL_MAX)
	{
		return angle;
	}
	return cosine ? cosl(angle) : sinl(angle);
}

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees)
{
	vec3_t forward, right, up;
	float  basis[3][3], inverse[3][3], around[3][3];
	float  temporary[3][3], rotation[3][3];
	int    row, column;

	/* Snapshot the rotation axis before the perpendicular-vector provider. */
	memcpy(forward, dir, sizeof(forward));
	PerpendicularVector(right, dir);
	CrossProduct(right, forward, up);
#if defined(__linux__) && !defined(_WIN32)
	{
		/* ELF transfers these two perpendicular components through float. */
		volatile float z = right[2];
		volatile float y = right[1];
		right[2] = z;
		right[1] = y;
	}
#endif
	for (row = 0; row < 3; ++row)
	{
		memcpy(&basis[row][0], &right[row], sizeof(float));
		memcpy(&basis[row][1], &up[row], sizeof(float));
		memcpy(&basis[row][2], &forward[row], sizeof(float));
	}
	for (row = 0; row < 3; ++row)
		for (column = 0; column < 3; ++column)
			memcpy(&inverse[row][column], &basis[column][row], sizeof(float));
	memset(around, 0, sizeof(around));
	around[2][2] = 1.0f;
#if defined(_WIN32)
	{
		long double    angle = (long double)degrees * 3.1415927410125732421875f;
		volatile float roundedAngle;
		long double    cosine, sine;
		angle       *= 0.0055555556900799274444580078125f;
		roundedAngle = (float)angle;
		/* The Windows cosine keeps the angle preceding its float store. */
		cosine       = TCE_RotationTrigValue(angle, qtrue);
		around[0][0] = (float)cosine;
		sine         = TCE_RotationTrigValue((long double)roundedAngle, qfalse);
		around[0][1] = (float)sine;
		around[1][0] = (float)-sine;
		around[1][1] = (float)cosine;
	}
#else
	{
		/* ELF multiplies the double conversion factor, then stores float. */
		volatile float roundedAngle =
			(float)((long double)0.017453292519943295 * degrees);
		around[0][0] = (float)TCE_RotationTrigValue((long double)roundedAngle, qtrue);
		around[0][1] = (float)TCE_RotationTrigValue((long double)roundedAngle, qfalse);
		around[1][0] = (float)-TCE_RotationTrigValue((long double)roundedAngle, qfalse);
		around[1][1] = (float)TCE_RotationTrigValue((long double)roundedAngle, qtrue);
	}
#endif
	MatrixMultiply(basis, around, temporary);
	MatrixMultiply(temporary, inverse, rotation);
	for (row = 0; row < 3; ++row)
	{
		long double product;
#if defined(_WIN32)
		product  = (long double)rotation[row][0] * point[0];
		product += (long double)rotation[row][2] * point[2];
		product += (long double)rotation[row][1] * point[1];
#else
		product  = (long double)point[1] * rotation[row][1];
		product += (long double)point[0] * rotation[row][0];
		product += (long double)point[2] * rotation[row][2];
#endif
		/* Do not snapshot point: earlier output may alias later input. */
		dst[row] = (float)product;
	}
}
#else
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point,
                             float degrees)
{
	float  m[3][3];
	float  im[3][3];
	float  zrot[3][3];
	float  tmpmat[3][3];
	float  rot[3][3];
	int    i;
	vec3_t vr, vup, vf;
	float  rad;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector(vr, dir);
	CrossProduct(vr, vf, vup);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy(im, m, sizeof(im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset(zrot, 0, sizeof(zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	rad        = DEG2RAD(degrees);
	zrot[0][0] = cos(rad);
	zrot[0][1] = sin(rad);
	zrot[1][0] = -sin(rad);
	zrot[1][1] = cos(rad);

	MatrixMultiply(m, zrot, tmpmat);
	MatrixMultiply(tmpmat, im, rot);

	for ( i = 0; i < 3; i++ )
	{
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}
#endif

/*
===============
RotatePointArountVertex

Rotate a point around a vertex
===============
*/
void RotatePointAroundVertex(vec3_t pnt, float rot_x, float rot_y, float rot_z, const vec3_t origin)
{
	float tmp[11];
	//float rad_x, rad_y, rad_z;

	/*rad_x = DEG2RAD( rot_x );
	rad_y = DEG2RAD( rot_y );
	rad_z = DEG2RAD( rot_z );*/

	// move pnt to rel{0,0,0}
	VectorSubtract(pnt, origin, pnt);

	// init temp values
	tmp[0]  = sin(rot_x);
	tmp[1]  = cos(rot_x);
	tmp[2]  = sin(rot_y);
	tmp[3]  = cos(rot_y);
	tmp[4]  = sin(rot_z);
	tmp[5]  = cos(rot_z);
	tmp[6]  = pnt[1] * tmp[5];
	tmp[7]  = pnt[0] * tmp[4];
	tmp[8]  = pnt[0] * tmp[5];
	tmp[9]  = pnt[1] * tmp[4];
	tmp[10] = pnt[2] * tmp[3];

	// rotate point
	pnt[0] = (tmp[3] * (tmp[8] - tmp[9]) + pnt[3] * tmp[2]);
	pnt[1] = (tmp[0] * (tmp[2] * tmp[8] - tmp[2] * tmp[9] - tmp[10]) + tmp[1] * (tmp[7] + tmp[6]));
	pnt[2] = (tmp[1] * (-tmp[2] * tmp[8] + tmp[2] * tmp[9] + tmp[10]) + tmp[0] * (tmp[7] + tmp[6]));

	// move pnt back
	VectorAdd(pnt, origin, pnt);
}

/*
===============
RotateAroundDirection
===============
*/
#if defined(_WIN32) || defined(__linux__)
void RotateAroundDirection(vec3_t axis[3], float yaw)
{
	unsigned int magnitude;
	PerpendicularVector(axis[1], axis[0]);
	memcpy(&magnitude, &yaw, sizeof(magnitude));
	magnitude &= 0x7fffffffu;
	/* Both native C3 gates skip zero and unordered yaw, but allow infinity. */
	if (magnitude != 0 && magnitude <= 0x7f800000u)
	{
		vec3_t temporary;
		memcpy(temporary, axis[1], sizeof(temporary));
		RotatePointAroundVector(axis[1], axis[0], temporary, yaw);
	}
	CrossProduct(axis[0], axis[1], axis[2]);
}
#else
void RotateAroundDirection(vec3_t axis[3], float yaw)
{

	// create an arbitrary axis[1]
	PerpendicularVector(axis[1], axis[0]);

	// rotate it around axis[0] by yaw
	if (yaw)
	{
		vec3_t temp;

		VectorCopy(axis[1], temp);
		RotatePointAroundVector(axis[1], axis[0], temp, yaw);
	}

	// cross to get axis[2]
	CrossProduct(axis[0], axis[1], axis[2]);
}
#endif



/* Native direction-angle stages and aliased output order; proof961.
 * atan2l/sqrtl do not establish FPATAN/FSQRT equivalence. */
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
void vectoangles(const vec3_t value1, vec3_t angles)
{
	long double x, y, z, yaw, pitch, squared, root;
#if defined(_WIN32)
	if (TCE_FloatIsZeroOrNaN(value1[1]) && TCE_FloatIsZeroOrNaN(value1[0]))
	{
		pitch         = value1[2] > 0.0f ? 90.0L : 270.0L;
		angles[PITCH] = (float)-pitch;
		angles[ROLL]  = 0.0f;
		angles[YAW]   = 0.0f;
		return;
	}
	if (TCE_FloatIsZeroOrNaN(value1[0]))
	{
		yaw = value1[1] > 0.0f ? 90.0L : 270.0L;
	}
	else
	{
		y    = (long double)value1[1];
		x    = (long double)value1[0];
		yaw  = atan2l(y, x);
		yaw *= (long double)57.29577791868204;
		if (!(yaw >= 0.0L))
		{
			yaw += (long double)360.0f;
		}
	}
	y        = (long double)value1[1];
	x        = (long double)value1[0];
	z        = (long double)value1[2];
	squared  = x * x;
	squared += y * y;
	root     = sqrtl(squared);
	pitch    = atan2l(z, root);
	pitch   *= (long double)57.29577791868204;
	if (!(pitch >= 0.0L))
	{
		pitch += (long double)360.0f;
	}
	angles[PITCH] = (float)-pitch;
	angles[ROLL]  = 0.0f;
	angles[YAW]   = (float)yaw;
#else
	{
		qboolean        axial = qfalse;
		volatile double angleSpill;
		volatile float  degreesSpill, rootSpill;
		y = (long double)value1[1];
		if (y == 0.0L || y != y)
		{
			x     = (long double)value1[0];
			axial = x == 0.0L || x != x;
		}
		else
		{
			x = (long double)value1[0];
		}
		if (axial)
		{
			yaw   = 0.0L;
			pitch = value1[2] > 0.0f ? 90.0L : 270.0L;
		}
		else
		{
			if (x == 0.0L || x != x)
			{
				yaw = y > 0.0L ? 90.0L : 270.0L;
			}
			else
			{
				angleSpill   = (double)atan2l(y, x);
				degreesSpill = (float)((long double)angleSpill * (long double)57.29577951308232);
				yaw          = (long double)degreesSpill;
			}
			if (!(yaw >= 0.0L))
			{
				yaw += (long double)360.0f;
			}
			squared      = y * y;
			squared     += x * x;
			rootSpill    = (float)sqrtl(squared);
			root         = (long double)rootSpill;
			z            = (long double)value1[2];
			angleSpill   = (double)atan2l(z, root);
			degreesSpill = (float)((long double)57.29577951308232 * (long double)angleSpill);
			pitch        = (long double)degreesSpill;
			if (!(pitch >= 0.0L))
			{
				pitch += (long double)360.0f;
			}
		}
		angles[YAW]   = (float)yaw;
		angles[ROLL]  = 0.0f;
		angles[PITCH] = (float)-pitch;
	}
#endif
}
#else
void vectoangles(const vec3_t value1, vec3_t angles)
{
	float forward;
	float yaw, pitch;

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
		{
			pitch = 90;
		}
		else
		{
			pitch = 270;
		}
	}
	else
	{
		if (value1[0])
		{
			yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
		}
		else if (value1[1] > 0)
		{
			yaw = 90;
		}
		else
		{
			yaw = 270;
		}
		if (yaw < 0)
		{
			yaw += 360;
		}

		forward = sqrt(value1[0] * value1[0] + value1[1] * value1[1]);
		pitch   = (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
		{
			pitch += 360;
		}
	}

	angles[PITCH] = -pitch;
	angles[YAW]   = yaw;
	angles[ROLL]  = 0;
}
#endif

/*
=================
AnglesToAxis
=================
*/
void AnglesToAxis(const vec3_t angles, vec3_t axis[3])
{
	vec3_t right;
	AngleVectors(angles, axis[0], right, axis[2]);
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	/* Keep mutable vec3_origin reads after the child and between stores. */
	axis[1][0] = (float)((long double)vec3_origin[0] - (long double)right[0]);
	axis[1][1] = (float)((long double)vec3_origin[1] - (long double)right[1]);
	axis[1][2] = (float)((long double)vec3_origin[2] - (long double)right[2]);
#else
	VectorSubtract(vec3_origin, right, axis[1]);
#endif
}

void AxisClear(vec3_t axis[3])
{
	axis[0][0] = 1;
	axis[0][1] = 0;
	axis[0][2] = 0;
	axis[1][0] = 0;
	axis[1][1] = 1;
	axis[1][2] = 0;
	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
}

void AxisCopy(vec3_t in[3], vec3_t out[3])
{
	typedef char axisCopyWordLayout[(sizeof(float) == 4 && sizeof(unsigned) == 4) ? 1 : -1];
	int row, component;

	/* Each original word is loaded immediately before its output store.
	 * Keep that forward order for overlapping axes, and preserve float bits
	 * without an additional floating conversion. */
	for (row = 0; row < 3; ++row)
	{
		for (component = 0; component < 3; ++component)
		{
			unsigned bits;
			memcpy(&bits, &in[row][component], sizeof(bits));
			memcpy(&out[row][component], &bits, sizeof(bits));
		}
	}
}

#if defined(_WIN32) || defined(__linux__)
void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal)
{
#if defined(_WIN32)
	long double    z = normal[2], y = normal[1], x = normal[0];
	long double    denominator = x * x;
	long double    inverse, dot, scaledZ;
	volatile float distance, scaledX, scaledY;
	denominator += y * y;
	denominator += z * z;
	inverse      = 1.0L / denominator;
	dot          = (long double)p[2] * normal[2];
	dot         += (long double)p[1] * normal[1];
	dot         += (long double)p[0] * normal[0];
	distance     = (float)(dot * inverse);
	scaledX      = (float)(inverse * normal[0]);
	scaledY      = (float)(inverse * normal[1]);
	scaledZ      = inverse * normal[2];
	dst[0]       = (float)((long double)p[0] - (long double)scaledX * distance);
	dst[1]       = (float)((long double)p[1] - (long double)scaledY * distance);
	dst[2]       = (float)((long double)p[2] - scaledZ * distance);
#else
	long double x = normal[0], y = normal[1], z = normal[2];
	long double pointX      = p[0];
	long double denominator = y * y;
	long double squareX     = x * x;
	long double dotX        = x * pointX;
	long double inverse, dotY, dot, scaledX, scaledY, scaledZ;
	long double distance, correctionX, correctionY, correctionZ;
	denominator += squareX;
	denominator += z * z;
	inverse      = 1.0L / denominator;
	dotY         = y * p[1];
	scaledX      = x * inverse;
	scaledY      = y * inverse;
	dot          = dotY + dotX;
	scaledZ      = z * inverse;
	dot         += z * p[2];
	distance     = inverse * dot;
	correctionX  = scaledX * distance;
	correctionY  = scaledY * distance;
	correctionZ  = scaledZ * distance;
	/* ELF snapshots p.x, but reloads p.y/z after the preceding output stores. */
	dst[0] = (float)(pointX - correctionX);
	dst[1] = (float)((long double)p[1] - correctionY);
	dst[2] = (float)((long double)p[2] - correctionZ);
#endif
}
#else
void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal)
{
	float  d;
	vec3_t n;
	float  inv_denom;

	inv_denom = 1.0F / DotProduct(normal, normal);

	d = DotProduct(normal, p) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}
#endif

/*
================
MakeNormalVectors

Given a normalized forward vector, create two
other perpendicular vectors
================
*/
void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
	float d;

	// this rotate and negate guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct(right, forward);
	VectorMA(right, -d, forward, right);
	VectorNormalize(right);
	CrossProduct(right, forward, up);
}


void VectorRotate(vec3_t in, vec3_t matrix[3], vec3_t out)
{
	out[0] = DotProduct(in, matrix[0]);
	out[1] = DotProduct(in, matrix[1]);
	out[2] = DotProduct(in, matrix[2]);
}

//============================================================================

/*
** float q_rsqrt( float number )
*/
#if defined(_WIN32) || defined(__linux__)
/* Shared arithmetic value for native consumers which retain the return value
 * beyond binary32. The public Q_rsqrt API still returns float. */
long double TCE_ReciprocalSquareRootValue(float number)
{
	uint32_t    bits, shifted, estimateBits;
	float       estimate;
	long double correction;
	memcpy(&bits, &number, sizeof(bits));
	/* Native SAR is signed; use unsigned operations to define sign filling. */
	shifted      = (bits >> 1) | (bits & 0x80000000u);
	estimateBits = 0x5f3759dfu - shifted;
	memcpy(&estimate, &estimateBits, sizeof(estimate));
	correction  = (long double)number * 0.5f;
	correction *= (long double)estimate;
#if defined(_WIN32)
	correction *= (long double)estimate;
	correction  = 1.5f - correction;
	return correction * (long double)estimate;
#else
	correction = (long double)estimate * correction;
	correction = 1.5f - correction;
	return (long double)estimate * correction;
#endif
}

float Q_rsqrt(float number)
{
	return (float)TCE_ReciprocalSquareRootValue(number);
}
#else
float Q_rsqrt(float number)
{
	uint32_t    i;
	float       x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	/* float is 32 bits on both ILP32 and LP64; long is not. */
	memcpy(&i, &y, sizeof(i));
	i = 0x5f3759dfu - (i >> 1);
	memcpy(&y, &i, sizeof(y));
	y = y * (threehalfs - (x2 * y * y));        // 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
}
#endif

/* Clear the stored sign bit before returning the Float value. Finite magnitudes
 * need no extra precision; signaling-NaN conversion/exception behavior at the
 * native x87 return boundary remains compiler-dependent (proof933). */
float Q_fabs(float f)
{
	uint32_t bits;
	memcpy(&bits, &f, sizeof(bits));
	bits &= 0x7fffffffu;
	memcpy(&f, &bits, sizeof(f));
	return f;
}

#if id386 && !((defined __linux__ || defined __FreeBSD__ || defined __GNUC__) && (defined __i386__))     // rb010123
long myftol(float f)
{
	static int tmp;
	/* FISTP follows the current rounding direction, rather than truncating.
	 * Float inputs and their rounded integral results need no wider mantissa.
	 * CRT/x87 exception and control-word equivalence remains unverified. */
	float rounded = rintf(f);
	if (!(rounded >= -2147483648.0f && rounded < 2147483648.0f))
	{
		tmp = INT_MIN;
	}
	else
	{
		tmp = (int)rounded;
	}
	return (long)tmp;
}
#endif

//============================================================

/*
===============
LerpAngle

===============
*/
/* Original native interpolation keeps adjusted endpoints and products wide.
 * The lower comparison includes unordered values; the upper one does not.
 * Public Float return/FP exception equivalence remains open; see proof934. */
#if defined(_WIN32) || defined(__linux__)
float LerpAngle(float from, float to, float frac)
{
	long double endpoint = (long double)to;
	long double start    = (long double)from;
#if defined(__linux__) && !defined(_WIN32)
	long double factor = (long double)frac;
#endif
	long double difference = endpoint - start;
	long double product;

	if (difference > 180.0L)
	{
		endpoint -= 360.0L;
#if !defined(_WIN32)
		difference = endpoint - start;
#endif
	}
#if defined(_WIN32)
	/* Windows repeats the subtraction for the second comparison. */
	difference = endpoint - start;
#endif
	if (!(difference >= -180.0L))
	{
		endpoint += 360.0L;
#if !defined(_WIN32)
		difference = endpoint - start;
#endif
	}
#if defined(_WIN32)
	difference = endpoint - start;
#endif
#if defined(_WIN32)
	product = difference * (long double)frac;
#else
	product = difference * factor;
#endif
	return (float)(product + start);
}
#else
float LerpAngle(float from, float to, float frac)
{
	if (to - from > 180)
	{
		to -= 360;
	}
	if (to - from < -180)
	{
		to += 360;
	}
	return from + frac * (to - from);
}
#endif
/*
=================
LerpPosition

=================
*/

/* Platform interpolation stages from the original bodies; see proof913. */
#if defined(_WIN32)
void LerpPosition(vec3_t start, vec3_t end, float frac, vec3_t result)
{
#if defined(__linux__)
	vec3_t dist;
	VectorSubtract(end, start, dist);
	VectorMA(start, frac, dist, result);
#else
	long double    differenceX = (long double)end[0] - start[0];
	volatile float differenceY = (float)((long double)end[1] - start[1]);
	volatile float differenceZ = (float)((long double)end[2] - start[2]);
	long double    product;

	product   = differenceX * (long double)frac;
	result[0] = (float)(product + (long double)start[0]);
	product   = (long double)differenceY * (long double)frac;
	result[1] = (float)(product + (long double)start[1]);
	product   = (long double)differenceZ * (long double)frac;
	result[2] = (float)(product + (long double)start[2]);
#endif
}
#elif defined(__linux__)
void LerpPosition(vec3_t start, vec3_t end, float frac, vec3_t result)
{
	long double factor      = (long double)frac;
	long double startX      = start[0];
	long double endX        = end[0];
	long double startY      = start[1];
	long double differenceY = (long double)end[1] - startY;
	long double differenceX = endX - startX;
	long double startZ      = start[2];
	long double differenceZ = (long double)end[2] - startZ;
	long double productX    = differenceX * factor;
	long double productY    = differenceY * factor;
	long double productZ    = factor * differenceZ;

	/* Only X retains its original starting value across the output stores.
	 * Y and Z reload start so overlapping input/output keeps native order. */
	result[0] = (float)(productX + startX);
	result[1] = (float)(productY + (long double)start[1]);
	result[2] = (float)(productZ + (long double)start[2]);
}
#else
void LerpPosition(vec3_t start, vec3_t end, float frac, vec3_t out)
{
	vec3_t dist;

	VectorSubtract(end, start, dist);
	VectorMA(start, frac, dist, out);
}
#endif

/*
=================
AngleSubtract

Always returns a value from -180 to 180
=================
*/
float AngleSubtract(float a1, float a2)
{
#if defined(_WIN32) || defined(__linux__)
	/* Wide original consumers use TCE_AngleSubtractValue directly. */
	return (float)TCE_AngleSubtractValue(a1, a2);
#else
	float a = a1 - a2;
	while (a > 180)
	{
		a -= 360;
	}
	while (a < -180)
	{
		a += 360;
	}
	return a;
#endif
}


#if defined(_WIN32) || defined(__linux__)
/* Shared value path retains the original wide result for native C consumers.
 * The public scalar entry rounds only at its declared Float return boundary. */
long double TCE_AngleSubtractValue(float a1, float a2)
{
#if defined(_WIN32)
	long double value = (long double)a1 - (long double)a2;
	while (value > 180.0L)
	{
		value -= 360.0L;
	}
	/* The original lower loop repeats for unordered comparisons as well. */
	while (!(value >= -180.0L))
	{
		value += 360.0L;
	}
	return value;
#else
	long double second = (long double)a2;
	long double value  = (long double)a1 - second;
	/* Linux enters the upper loop for an unordered first comparison, then
	 * exits that loop on unordered; the initial lower comparison skips it. */
	if (!(value <= 180.0L))
	{
		do
		{
			value -= 360.0L;
		}
		while (value > 180.0L);
	}
	if (value < -180.0L)
	{
		do
		{
			value += 360.0L;
		}
		while (!(value >= -180.0L));
	}
	return value;
#endif
}
#endif

void AnglesSubtract(vec3_t v1, vec3_t v2, vec3_t v3)
{
#if defined(_WIN32) || defined(__linux__)
	v3[0] = (float)TCE_AngleSubtractValue(v1[0], v2[0]);
	v3[1] = (float)TCE_AngleSubtractValue(v1[1], v2[1]);
	v3[2] = (float)TCE_AngleSubtractValue(v1[2], v2[2]);
#else
	v3[0] = AngleSubtract(v1[0], v2[0]);
	v3[1] = AngleSubtract(v1[1], v2[1]);
	v3[2] = AngleSubtract(v1[2], v2[2]);
#endif
}


float AngleMod(float angle)
{
#if defined(_WIN32) || defined(__linux__)
	/* Match the original double scale and platform integer conversion.
	 * Do not narrow the product to float before truncation. */
	long double scaled     = (long double)angle * (long double)(65536.0 / 360.0);
	unsigned    shortAngle = (unsigned)TCE_TruncateNativeInteger(scaled) & 65535u;
	/* 45 * shortAngle fits in 22 bits, so the final float is exact. */
	return (float)((long double)shortAngle * (long double)(360.0 / 65536.0));
#else
	return (360.0 / 65536) * ((int)(angle * (65536 / 360.0)) & 65535);
#endif
}

/*
=================
AngleNormalize2Pi

returns angle normalized to the range [0 <= angle < 2*M_PI]
=================
*/
float AngleNormalize2Pi(float angle)
{
	return DEG2RAD(AngleNormalize360(RAD2DEG(angle)));
}

/*
=================
AngleNormalize360

returns angle normalized to the range [0 <= angle < 360]
=================
*/
float AngleNormalize360(float angle)
{
#if defined(_WIN32) || defined(__linux__)
	/* The original multiplies by a double constant before truncation:
	 * Windows __ftol consumes signed64, Linux FISTP consumes signed32.
	 * Keep the product wide until the platform conversion, not in float. */
	long double scaled     = (long double)angle * (long double)(65536.0 / 360.0);
	unsigned    shortAngle = (unsigned)TCE_TruncateNativeInteger(scaled) & 65535u;
	/* 45 * shortAngle fits in 22 bits. This dyadic product is already
	 * exactly representable as float, including the native wide return. */
	return (float)((long double)shortAngle * (long double)(360.0 / 65536.0));
#else
	return (360.0 / 65536) * ((int)(angle * (65536 / 360.0)) & 65535);
#endif
}


/*
=================
AngleNormalize180

returns angle normalized to the range [-180 < angle <= 180]
=================
*/
float AngleNormalize180(float angle)
{
	/* Normalize360 returns an exactly representable float for the native
	 * quantized range. Retain wide arithmetic through the comparison. */
	long double normalized = (long double)AngleNormalize360(angle);
#if defined(__linux__)
	/* Linux compares 180 against the result; unordered also subtracts. */
	if (!(normalized <= 180.0L))
	{
		normalized = (long double)(float)(normalized - 360.0L);
	}
#else
	/* Windows skips subtraction for unordered and equal comparisons. */
	if (normalized > 180.0L)
	{
		normalized -= 360.0L;
	}
#endif
	return (float)normalized;
}


/*
=================
AngleDelta

returns the normalized delta from angle1 to angle2
=================
*/
float AngleDelta(float angle1, float angle2)
{
#if defined(_WIN32) || defined(__linux__)
	volatile float difference;
#if defined(__linux__)
	/* The Linux original loads angle2, then subtracts it from angle1. */
	long double second = (long double)angle2;
	difference = (float)((long double)angle1 - second);
#else
	difference = (float)((long double)angle1 - (long double)angle2);
#endif
	/* Both originals store a Float argument before normalization. */
	return AngleNormalize180(difference);
#else
	return AngleNormalize180(angle1 - angle2);
#endif
}


//============================================================


/*
=================
SetPlaneSignbits
=================
*/
void SetPlaneSignbits(cplane_t *out)
{
	int bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j = 0 ; j < 3 ; j++)
	{
		if (out->normal[j] < 0)
		{
			bits |= 1 << j;
		}
	}
	out->signbits = bits;
}


/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2

// this is the slow, general version
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	int		i;
	float	dist1, dist2;
	int		sides;
	vec3_t	corners[2];

	for (i=0 ; i<3 ; i++)
	{
		if (p->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

	return sides;
}

==================
*/
#if !(defined __linux__ && defined __i386__ && !defined C_ONLY)
#if defined __LCC__ || defined C_ONLY || !id386 || __GNUC__
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float dist1, dist2;
	int   sides;

// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
		{
			return 1;
		}
		if (p->dist >= emaxs[p->type])
		{
			return 2;
		}
		return 3;
	}

// general case
	switch (p->signbits)
	{
	case 0:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		break;
	case 1:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		break;
	case 2:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		break;
	case 3:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		break;
	case 4:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		break;
	case 5:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		break;
	case 6:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		break;
	case 7:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		break;
	default:
		dist1 = dist2 = 0;      // shut up compiler
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
	{
		sides = 1;
	}
	if (dist2 < p->dist)
	{
		sides |= 2;
	}

	return sides;
}
#else
#pragma warning( disable: 4035 )

__inline __declspec(naked) int BoxOnPlaneSide_fast(vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	static int bops_initialized;
	static int Ljmptab[8];

	__asm {

		push ebx

		cmp bops_initialized, 1
		je initialized
		mov bops_initialized, 1

		mov Ljmptab[0 * 4], offset Lcase0
		mov Ljmptab[1 * 4], offset Lcase1
		mov Ljmptab[2 * 4], offset Lcase2
		mov Ljmptab[3 * 4], offset Lcase3
		mov Ljmptab[4 * 4], offset Lcase4
		mov Ljmptab[5 * 4], offset Lcase5
		mov Ljmptab[6 * 4], offset Lcase6
		mov Ljmptab[7 * 4], offset Lcase7

initialized:

		mov edx, dword ptr[4 + 12 + esp]
		mov ecx, dword ptr[4 + 4 + esp]
		xor eax, eax
		mov ebx, dword ptr[4 + 8 + esp]
		mov al, byte ptr[17 + edx]
		cmp al, 8
		jge Lerror
		fld dword ptr[0 + edx]
		fld st(0)
		jmp dword ptr[Ljmptab + eax * 4]
Lcase0:
		fmul dword ptr[ebx]
		fld dword ptr[0 + 4 + edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4 + ebx]
		fld dword ptr[0 + 8 + edx]
		fxch st(2)
		fmul dword ptr[4 + ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8 + ebx]
		fxch st(5)
		faddp st(3), st(0)
		fmul dword ptr[8 + ecx]
		fxch st(1)
		faddp st(3), st(0)
		fxch st(3)
		faddp st(2), st(0)
		jmp LSetSides
Lcase1:
		fmul dword ptr[ecx]
		fld dword ptr[0 + 4 + edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4 + ebx]
		fld dword ptr[0 + 8 + edx]
		fxch st(2)
		fmul dword ptr[4 + ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8 + ebx]
		fxch st(5)
		faddp st(3), st(0)
		fmul dword ptr[8 + ecx]
		fxch st(1)
		faddp st(3), st(0)
		fxch st(3)
		faddp st(2), st(0)
		jmp LSetSides
Lcase2:
		fmul dword ptr[ebx]
		fld dword ptr[0 + 4 + edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4 + ecx]
		fld dword ptr[0 + 8 + edx]
		fxch st(2)
		fmul dword ptr[4 + ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8 + ebx]
		fxch st(5)
		faddp st(3), st(0)
		fmul dword ptr[8 + ecx]
		fxch st(1)
		faddp st(3), st(0)
		fxch st(3)
		faddp st(2), st(0)
		jmp LSetSides
Lcase3:
		fmul dword ptr[ecx]
		fld dword ptr[0 + 4 + edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4 + ecx]
		fld dword ptr[0 + 8 + edx]
		fxch st(2)
		fmul dword ptr[4 + ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8 + ebx]
		fxch st(5)
		faddp st(3), st(0)
		fmul dword ptr[8 + ecx]
		fxch st(1)
		faddp st(3), st(0)
		fxch st(3)
		faddp st(2), st(0)
		jmp LSetSides
Lcase4:
		fmul dword ptr[ebx]
		fld dword ptr[0 + 4 + edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4 + ebx]
		fld dword ptr[0 + 8 + edx]
		fxch st(2)
		fmul dword ptr[4 + ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8 + ecx]
		fxch st(5)
		faddp st(3), st(0)
		fmul dword ptr[8 + ebx]
		fxch st(1)
		faddp st(3), st(0)
		fxch st(3)
		faddp st(2), st(0)
		jmp LSetSides
Lcase5:
		fmul dword ptr[ecx]
		fld dword ptr[0 + 4 + edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4 + ebx]
		fld dword ptr[0 + 8 + edx]
		fxch st(2)
		fmul dword ptr[4 + ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8 + ecx]
		fxch st(5)
		faddp st(3), st(0)
		fmul dword ptr[8 + ebx]
		fxch st(1)
		faddp st(3), st(0)
		fxch st(3)
		faddp st(2), st(0)
		jmp LSetSides
Lcase6:
		fmul dword ptr[ebx]
		fld dword ptr[0 + 4 + edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4 + ecx]
		fld dword ptr[0 + 8 + edx]
		fxch st(2)
		fmul dword ptr[4 + ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8 + ecx]
		fxch st(5)
		faddp st(3), st(0)
		fmul dword ptr[8 + ebx]
		fxch st(1)
		faddp st(3), st(0)
		fxch st(3)
		faddp st(2), st(0)
		jmp LSetSides
Lcase7:
		fmul dword ptr[ecx]
		fld dword ptr[0 + 4 + edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4 + ecx]
		fld dword ptr[0 + 8 + edx]
		fxch st(2)
		fmul dword ptr[4 + ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8 + ecx]
		fxch st(5)
		faddp st(3), st(0)
		fmul dword ptr[8 + ebx]
		fxch st(1)
		faddp st(3), st(0)
		fxch st(3)
		faddp st(2), st(0)
		LSetSides :
		faddp st(2), st(0)
		fcomp dword ptr[12 + edx]
		xor ecx, ecx
		fnstsw ax
		fcomp dword ptr[12 + edx]
		and ah, 1
		xor ah, 1
		add cl, ah
		fnstsw ax
		and ah, 1
		add ah, ah
		add cl, ah
		pop ebx
		mov eax, ecx
		ret
Lerror:
		int 3
	}
}

int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	// fast axial cases

	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
		{
			return 1;
		}
		if (p->dist >= emaxs[p->type])
		{
			return 2;
		}
		return 3;
	}

	return BoxOnPlaneSide_fast(emins, emaxs, p);
}

#pragma warning( default: 4035 )

#endif
#endif

/*
=================
RadiusFromBounds
=================
*/
#if defined(_WIN32) || defined(__linux__)
/* Internal value paths avoid an extra public float-return boundary between
 * reconstructed helpers. Native x87 precision/exception parity remains open.
 */
long double TCE_VectorLengthSquaredValue(const vec3_t v)
{
	long double x, y, z, xSquared, ySquared, sum;

#if defined(_WIN32)
	z = v[2];
	y = v[1];
	x = v[0];
#else
	x = v[0];
	y = v[1];
#endif
	xSquared = x * x;
	ySquared = y * y;
	sum      = xSquared + ySquared;
#if !defined(_WIN32)
	z = v[2];
#endif
	return sum + z * z;
}

long double TCE_VectorLengthValue(const vec3_t v)
{
	long double length = sqrtl(TCE_VectorLengthSquaredValue(v));

#if !defined(_WIN32)
	/* The Linux VectorLength stores and reloads its root as float. */
	volatile float storedLength = (float)length;
	length = storedLength;
#endif
	return length;
}

static void TCE_VectorDifferenceFloat(const vec3_t p1, const vec3_t p2, vec3_t delta)
{
	volatile float *stored = delta;

	/* These three original stores precede the length helper call. */
	stored[0] = (float)((long double)p2[0] - (long double)p1[0]);
	stored[1] = (float)((long double)p2[1] - (long double)p1[1]);
	stored[2] = (float)((long double)p2[2] - (long double)p1[2]);
}
#endif

#if defined(_WIN32) || defined(__linux__)
float RadiusFromBounds(const vec3_t mins, const vec3_t maxs)
{
	vec3_t corner;
	int    i;

	for (i = 0; i < 3; ++i)
	{
		volatile float minAbs = Q_fabs(mins[i]);
		long double    maxAbs = Q_fabs(maxs[i]);

#if defined(_WIN32)
		/* Unordered comparisons keep maxAbs in the Windows original. */
		corner[i] = (float)((long double)minAbs > maxAbs ? (long double)minAbs : maxAbs);
#else
		/* JNC keeps maxAbs only for an ordered greater-or-equal result. */
		corner[i] = (float)(maxAbs >= (long double)minAbs ? maxAbs : (long double)minAbs);
#endif
	}
	return (float)TCE_VectorLengthValue(corner);
}
#else
float RadiusFromBounds(const vec3_t mins, const vec3_t maxs)
{
	int    i;
	vec3_t corner;
	float  a, b;

	for (i = 0 ; i < 3 ; i++)
	{
		a         = Q_fabs(mins[i]);
		b         = Q_fabs(maxs[i]);
		corner[i] = a > b ? a : b;
	}

	return VectorLength(corner);
}
#endif


void ClearBounds(vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

/* Original unordered minimum gate and platform copy/reload stages; proof958. */
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs)
{
	int i;
#if defined(_WIN32)
	unsigned word;
	for (i = 0; i < 3; ++i)
	{
		if (!((long double)v[i] >= (long double)mins[i]))
		{
			memcpy(&word, &v[i], sizeof(word));
			memcpy(&mins[i], &word, sizeof(word));
		}
		if ((long double)v[i] > (long double)maxs[i])
		{
			memcpy(&word, &v[i], sizeof(word));
			memcpy(&maxs[i], &word, sizeof(word));
		}
	}
#else
	long double point;
	for (i = 0; i < 3; ++i)
	{
		point = (long double)v[i];
		if (!(point >= (long double)mins[i]))
		{
			mins[i] = (float)point;
			point   = (long double)v[i];
		}
		if (point > (long double)maxs[i])
		{
			maxs[i] = (float)point;
		}
	}
#endif
}
#else
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs)
{
	if (v[0] < mins[0])
	{
		mins[0] = v[0];
	}
	if (v[0] > maxs[0])
	{
		maxs[0] = v[0];
	}

	if (v[1] < mins[1])
	{
		mins[1] = v[1];
	}
	if (v[1] > maxs[1])
	{
		maxs[1] = v[1];
	}

	if (v[2] < mins[2])
	{
		mins[2] = v[2];
	}
	if (v[2] > maxs[2])
	{
		maxs[2] = v[2];
	}
}
#endif

/* The lower gate rejects unordered; the upper gate rejects only ordered >. */
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
qboolean PointInBounds(const vec3_t v, const vec3_t mins, const vec3_t maxs)
{
	int i;
#if defined(_WIN32)
	for (i = 0; i < 3; ++i)
	{
		if (!((long double)v[i] >= (long double)mins[i]))
		{
			return qfalse;
		}
		if ((long double)v[i] > (long double)maxs[i])
		{
			return qfalse;
		}
	}
#else
	long double point;
	for (i = 0; i < 3; ++i)
	{
		point = (long double)v[i];
		if (!(point >= (long double)mins[i]))
		{
			return qfalse;
		}
		if (point > (long double)maxs[i])
		{
			return qfalse;
		}
	}
#endif
	return qtrue;
}
#else
qboolean PointInBounds(const vec3_t v, const vec3_t mins, const vec3_t maxs)
{
	if (v[0] < mins[0])
	{
		return qfalse;
	}
	if (v[0] > maxs[0])
	{
		return qfalse;
	}

	if (v[1] < mins[1])
	{
		return qfalse;
	}
	if (v[1] > maxs[1])
	{
		return qfalse;
	}

	if (v[2] < mins[2])
	{
		return qfalse;
	}
	if (v[2] > maxs[2])
	{
		return qfalse;
	}

	return qtrue;
}
#endif

/* Original C3-only equality accepts unordered; reject ordered unequal only. */
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
int VectorCompare(const vec3_t v1, const vec3_t v2)
{
	int i;
	for (i = 0; i < 3; ++i)
	{
		long double a = (long double)v1[i];
		long double b = (long double)v2[i];
		if (a < b || a > b)
		{
			return 0;
		}
	}
	return 1;
}
#else
int VectorCompare(const vec3_t v1, const vec3_t v2)
{
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
	{
		return 0;
	}

	return 1;
}
#endif

/* Wide internal return for held-value consumers; public ABI remains Float.
 * Exact x87 precision/exception behavior is still unverified. */
#if defined(_WIN32) || defined(__linux__)
long double TCE_VectorNormalizeValue(vec3_t v)
{
	long double x, y, z, squared, length;
#if defined(_WIN32)
	z        = v[2];
	y        = v[1];
	x        = v[0];
	squared  = x * x;
	squared += y * y;
	squared += z * z;
	length   = sqrtl(squared);
	/* Native C3 skips both zero and unordered lengths without writing v. */
	if (length != 0.0L && length == length)
	{
		long double inverse = 1.0L / length;
		v[0] = (float)(inverse * (long double)v[0]);
		v[1] = (float)(inverse * (long double)v[1]);
		v[2] = (float)(inverse * (long double)v[2]);
	}
#else
	x        = v[0];
	y        = v[1];
	z        = v[2];
	squared  = y * y;
	squared += x * x;
	squared += z * z;
	{
		/* ELF stores/reloads the length before comparing and dividing. */
		volatile float storedLength = (float)sqrtl(squared);
		length = storedLength;
	}
	if (length != 0.0L && length == length)
	{
		long double inverse = 1.0L / length;
		x   *= inverse;
		y   *= inverse;
		z   *= inverse;
		v[0] = (float)x;
		v[1] = (float)y;
		v[2] = (float)z;
	}
#endif
	return length;
}

vec_t VectorNormalize(vec3_t v)
{
	return (float)TCE_VectorNormalizeValue(v);
}
#else
vec_t VectorNormalize(vec3_t v)
{
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt(length);

	if (length)
	{
		ilength = 1 / length;
		v[0]   *= ilength;
		v[1]   *= ilength;
		v[2]   *= ilength;
	}

	return length;
}
#endif

//
// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length
//
#if defined(_WIN32) || defined(__linux__)
void VectorNormalizeFast(vec3_t v)
{
	long double    x, y, z, squared, inverse;
	volatile float argument;
#if defined(_WIN32)
	z = v[2];
	y = v[1];
	x = v[0];
#else
	x = v[0];
	y = v[1];
#endif
	squared  = x * x;
	squared += y * y;
#if !defined(_WIN32)
	z = v[2];
#endif
	squared += z * z;
	argument = (float)squared;
	inverse  = TCE_ReciprocalSquareRootValue(argument);
#if defined(_WIN32)
	v[0] = (float)(inverse * (long double)v[0]);
	v[1] = (float)(inverse * (long double)v[1]);
	v[2] = (float)(inverse * (long double)v[2]);
#else
	v[0] = (float)((long double)v[0] * inverse);
	{
		long double scaledY = (long double)v[1] * inverse;
		long double scaledZ = inverse * (long double)v[2];
		v[1] = (float)scaledY;
		v[2] = (float)scaledZ;
	}
#endif
}
#else
void VectorNormalizeFast(vec3_t v)
{
	float ilength;

	ilength = Q_rsqrt(DotProduct(v, v));

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}
#endif

#if defined(_WIN32) || defined(__linux__)
vec_t VectorNormalize2(const vec3_t v, vec3_t out)
{
	long double x, y, z, squared, length;
#if defined(_WIN32)
	z = v[2];
	y = v[1];
	x = v[0];
#else
	x = v[0];
	y = v[1];
#endif
	squared  = x * x;
	squared += y * y;
#if !defined(_WIN32)
	z = v[2];
#endif
	squared += z * z;
#if defined(_WIN32)
	length = sqrtl(squared);
#else
	{
		volatile float storedLength = (float)sqrtl(squared);
		length = storedLength;
	}
#endif
	if (length != 0.0L && length == length)
	{
		long double inverse = 1.0L / length;
		/* Reload subsequent inputs after the preceding output store. */
#if defined(_WIN32)
		out[0] = (float)(inverse * (long double)v[0]);
		out[1] = (float)(inverse * (long double)v[1]);
#else
		out[0] = (float)(x * inverse);
		out[1] = (float)((long double)v[1] * inverse);
#endif
		out[2] = (float)(inverse * (long double)v[2]);
	}
	else
	{
		out[2] = 0.0f;
		out[1] = 0.0f;
		out[0] = 0.0f;
	}
	return (float)length;
}
#else
vec_t VectorNormalize2(const vec3_t v, vec3_t out)
{
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt(length);

	if (length)
	{
		ilength = 1 / length;
		out[0]  = v[0] * ilength;
		out[1]  = v[1] * ilength;
		out[2]  = v[2] * ilength;
	}
	else
	{
		VectorClear(out);
	}

	return length;

}
#endif

void _VectorMA(const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}


vec_t _DotProduct(const vec3_t v1, const vec3_t v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void _VectorSubtract(const vec3_t veca, const vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] - vecb[0];
	out[1] = veca[1] - vecb[1];
	out[2] = veca[2] - vecb[2];
}

void _VectorAdd(const vec3_t veca, const vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] + vecb[0];
	out[1] = veca[1] + vecb[1];
	out[2] = veca[2] + vecb[2];
}

void _VectorCopy(const vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void _VectorScale(const vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

vec_t VectorLength(const vec3_t v)
{
	return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

vec_t VectorLengthSquared(const vec3_t v)
{
	return (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

vec_t Distance(const vec3_t p1, const vec3_t p2)
{
	vec3_t v;

	VectorSubtract(p2, p1, v);
	return VectorLength(v);
}

#if defined(_WIN32) || defined(__linux__)
/* Unlike VectorDistanceSquared, the originals keep each difference wide.
 * Keep the platform sum order separate; reassociation changes boundary values.
 * A C long double and the public vec_t return do not prove native x87 parity.
 */
long double TCE_DistanceSquaredValue(const vec3_t p1, const vec3_t p2)
{
	long double x, y, z;
	long double xSquared, ySquared, zSquared, sum;

	x = (long double)p2[0] - (long double)p1[0];
	y = (long double)p2[1] - (long double)p1[1];
	z = (long double)p2[2] - (long double)p1[2];
#if defined(_WIN32)
	zSquared = z * z;
	ySquared = y * y;
	sum      = zSquared + ySquared;
	xSquared = x * x;
	return sum + xSquared;
#else
	xSquared = x * x;
	ySquared = y * y;
	zSquared = z * z;
	sum      = ySquared + xSquared;
	return sum + zSquared;
#endif
}

#endif

vec_t DistanceSquared(const vec3_t p1, const vec3_t p2)
{
	vec3_t v;

	VectorSubtract(p2, p1, v);
	return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}


void VectorInverse(vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void Vector4Scale(const vec4_t in, vec_t scale, vec4_t out)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
	out[3] = in[3] * scale;
}


int Q_log2(int val)
{
#if defined(__linux__)
	/* The native counter wraps while negative values keep shifting to -1. */
	uint32_t answer;
#else
	int answer;
#endif

	answer = 0;
	while ((val >>= 1) != 0)
	{
		answer++;
	}
	return answer;
}



/*
=================
PlaneTypeForNormal
=================
*/
/*
int	PlaneTypeForNormal (vec3_t normal) {
	if ( normal[0] == 1.0 )
		return PLANE_X;
	if ( normal[1] == 1.0 )
		return PLANE_Y;
	if ( normal[2] == 1.0 )
		return PLANE_Z;

	return PLANE_NON_AXIAL;
}
*/


/*
================
MatrixMultiply
================
*/
void MatrixMultiply(float in1[3][3], float in2[3][3], float result[3][3])
{
#if defined(_WIN32)
	long double dot;
	/* Each cell has its native sum order and is stored before the next reads. */
	dot          = (long double)in1[0][0] * in2[0][0];
	dot         += (long double)in1[0][1] * in2[1][0];
	dot         += (long double)in2[2][0] * in1[0][2];
	result[0][0] = (float)dot;
	dot          = (long double)in1[0][1] * in2[1][1];
	dot         += (long double)in2[2][1] * in1[0][2];
	dot         += (long double)in2[0][1] * in1[0][0];
	result[0][1] = (float)dot;
	dot          = (long double)in1[0][1] * in2[1][2];
	dot         += (long double)in2[2][2] * in1[0][2];
	dot         += (long double)in2[0][2] * in1[0][0];
	result[0][2] = (float)dot;
	dot          = (long double)in1[1][1] * in2[1][0];
	dot         += (long double)in1[1][0] * in2[0][0];
	dot         += (long double)in1[1][2] * in2[2][0];
	result[1][0] = (float)dot;
	dot          = (long double)in1[1][2] * in2[2][1];
	dot         += (long double)in1[1][1] * in2[1][1];
	dot         += (long double)in1[1][0] * in2[0][1];
	result[1][1] = (float)dot;
	dot          = (long double)in1[1][2] * in2[2][2];
	dot         += (long double)in1[1][1] * in2[1][2];
	dot         += (long double)in1[1][0] * in2[0][2];
	result[1][2] = (float)dot;
	dot          = (long double)in1[2][0] * in2[0][0];
	dot         += (long double)in2[2][0] * in1[2][2];
	dot         += (long double)in1[2][1] * in2[1][0];
	result[2][0] = (float)dot;
	dot          = (long double)in2[2][1] * in1[2][2];
	dot         += (long double)in2[1][1] * in1[2][1];
	dot         += (long double)in2[0][1] * in1[2][0];
	result[2][1] = (float)dot;
	dot          = (long double)in2[2][2] * in1[2][2];
	dot         += (long double)in2[1][2] * in1[2][1];
	dot         += (long double)in2[0][2] * in1[2][0];
	result[2][2] = (float)dot;
#elif defined(__linux__)
	int row, column;
	for (row = 0; row < 3; ++row)
	{
		for (column = 0; column < 3; ++column)
		{
			long double dot = (long double)in2[1][column] * in1[row][1];
			dot                += (long double)in2[0][column] * in1[row][0];
			dot                += (long double)in2[2][column] * in1[row][2];
			result[row][column] = (float)dot;
		}
	}
#else
	/* SDK ordering for other targets; original precision remains unverified. */
	result[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	result[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	result[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	result[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	result[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	result[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	result[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	result[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	result[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
#endif
}


/* Original native trig scratch and platform FP stages; proof957.
 * sinl/cosl do not establish hardware FSIN/FCOS equivalence. */
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
static volatile float tceSinRoll, tceCosRoll, tceSinPitch;
static volatile float tceCosYaw, tceCosPitch, tceSinYaw;

void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	long double phase, a, b;
#if !defined(_WIN32)
	long double    heldCosYaw, heldCosPitch;
	volatile float phaseSpill, cosineSpill;
#endif
#if defined(_WIN32)
	phase = (long double)angles[YAW] * (long double)0.01745329238474369f;
#else
	phase      = (long double)0.017453292519943295 * (long double)angles[YAW];
	phaseSpill = (float)phase;
	phase      = (long double)phaseSpill;
#endif
	tceSinYaw = (float)TCE_RotationTrigValue(phase, qfalse);
#if defined(_WIN32)
	tceCosYaw = (float)TCE_RotationTrigValue(phase, qtrue);
#else
	cosineSpill = (float)TCE_RotationTrigValue(phase, qtrue);
	heldCosYaw  = (long double)cosineSpill;
	tceCosYaw   = (float)heldCosYaw;
#endif
#if defined(_WIN32)
	phase = (long double)angles[PITCH] * (long double)0.01745329238474369f;
#else
	phase      = (long double)0.017453292519943295 * (long double)angles[PITCH];
	phaseSpill = (float)phase;
	phase      = (long double)phaseSpill;
#endif
	tceSinPitch = (float)TCE_RotationTrigValue(phase, qfalse);
#if defined(_WIN32)
	tceCosPitch = (float)TCE_RotationTrigValue(phase, qtrue);
#else
	cosineSpill  = (float)TCE_RotationTrigValue(phase, qtrue);
	heldCosPitch = (long double)cosineSpill;
	tceCosPitch  = (float)heldCosPitch;
#endif
#if defined(_WIN32)
	phase = (long double)angles[ROLL] * (long double)0.01745329238474369f;
#else
	phase      = (long double)0.017453292519943295 * (long double)angles[ROLL];
	phaseSpill = (float)phase;
	phase      = (long double)phaseSpill;
#endif
	tceSinRoll = (float)TCE_RotationTrigValue(phase, qfalse);
	tceCosRoll = (float)TCE_RotationTrigValue(phase, qtrue);
	if (forward)
	{
#if defined(_WIN32)
		forward[0] = (float)((long double)tceCosPitch * (long double)tceCosYaw);
		forward[1] = (float)((long double)tceCosPitch * (long double)tceSinYaw);
		forward[2] = (float)(-(long double)tceSinPitch);
#else
		unsigned bits;
		forward[0] = (float)(heldCosYaw * heldCosPitch);
		forward[1] = (float)((long double)tceSinYaw * (long double)tceCosPitch);
		/* Original Linux XOR changes the sign bit without FP negation. */
		memcpy(&bits, (const void *)&tceSinPitch, sizeof(bits));
		bits ^= 0x80000000u;
		memcpy(&forward[2], &bits, sizeof(bits));
#endif
	}
	if (right)
	{
#if defined(_WIN32)
		a        = (long double)tceCosRoll * (long double)tceSinYaw;
		b        = (long double)tceSinRoll * (long double)tceSinPitch;
		b       *= (long double)tceCosYaw;
		right[0] = (float)(a - b);
		b        = (long double)tceSinRoll * (long double)tceSinPitch;
		b       *= (long double)tceSinYaw;
		a        = (long double)tceCosRoll * (long double)tceCosYaw;
		right[1] = (float)((b + a) * (long double)-1.0f);
		a        = (long double)tceSinRoll * (long double)tceCosPitch;
		right[2] = (float)(a * (long double)-1.0f);
#else
		b        = (long double)tceSinRoll;
		a        = (long double)tceSinYaw * (long double)tceCosRoll;
		b        = -b;
		b       *= (long double)tceSinPitch;
		b       *= (long double)tceCosYaw;
		right[0] = (float)(a + b);
		b        = (long double)tceSinRoll;
		a        = (long double)tceCosRoll * (long double)tceCosYaw;
		b        = -b;
		b       *= (long double)tceSinPitch;
		b       *= (long double)tceSinYaw;
		right[1] = (float)(b - a);
		a        = -(long double)tceSinRoll;
		right[2] = (float)(a * (long double)tceCosPitch);
#endif
	}
	if (up)
	{
#if defined(_WIN32)
		a     = (long double)tceCosRoll * (long double)tceSinPitch;
		a    *= (long double)tceCosYaw;
		b     = (long double)tceSinRoll * (long double)tceSinYaw;
		up[0] = (float)(a + b);
		a     = (long double)tceCosRoll * (long double)tceSinPitch;
		a    *= (long double)tceSinYaw;
		b     = (long double)tceSinRoll * (long double)tceCosYaw;
		up[1] = (float)(a - b);
		up[2] = (float)((long double)tceCosRoll * (long double)tceCosPitch);
#else
		a     = (long double)tceSinPitch * (long double)tceCosRoll;
		b     = (long double)tceSinYaw * (long double)tceSinRoll;
		a    *= (long double)tceCosYaw;
		up[0] = (float)(b + a);
		a     = (long double)tceSinPitch * (long double)tceCosRoll;
		b     = (long double)tceSinRoll * (long double)tceCosYaw;
		a    *= (long double)tceSinYaw;
		up[1] = (float)(a - b);
		up[2] = (float)((long double)tceCosPitch * (long double)tceCosRoll);
#endif
	}
}
#else
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	const float radians = 0.01745329238474369049072265625f;
	double      angle;
	float       sr, sp, sy, cr, cp, cy;
	angle = (double)angles[YAW] * (double)radians;
	sy    = (float)sin(angle); cy = (float)cos(angle);
	angle = (double)angles[PITCH] * (double)radians;
	sp    = (float)sin(angle); cp = (float)cos(angle);
	angle = (double)angles[ROLL] * (double)radians;
	sr    = (float)sin(angle); cr = (float)cos(angle);
	if (forward)
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if (right)
	{
		right[0] = (float)((double)cr * sy - (double)sr * sp * cy);
		right[1] = (float)(-((double)cr * cy + (double)sr * sp * sy));
		right[2] = (float)(-(double)sr * cp);
	}
	if (up)
	{
		up[0] = (float)((double)sr * sy + (double)cr * sp * cy);
		up[1] = (float)((double)cr * sp * sy - (double)sr * cy);
		up[2] = cr * cp;
	}
}
#endif

/*
** assumes "src" is normalized
*/
/* The native smallest-axis gate selects less-than OR unordered, and calls
 * Q_fabs again before storing a new minimum. Keep both provider calls. */
#if defined(_WIN32) || defined(__linux__)
void PerpendicularVector(vec3_t dst, const vec3_t src)
{
	int    axis, smallest = 0;
	float  minimum = 1.0f;
	vec3_t basis;
	for (axis = 0; axis < 3; ++axis)
	{
		float        candidate = Q_fabs(src[axis]);
		unsigned int candidateBits, minimumBits;
		int          unordered;
		memcpy(&candidateBits, &candidate, sizeof(candidateBits));
		memcpy(&minimumBits, &minimum, sizeof(minimumBits));
		unordered = (candidateBits & 0x7fffffffu) > 0x7f800000u ||
		            (minimumBits & 0x7fffffffu) > 0x7f800000u;
		if (unordered || (long double)candidate < (long double)minimum)
		{
			smallest = axis;
			minimum  = Q_fabs(src[axis]);
		}
	}
	memset(basis, 0, sizeof(basis));
	basis[smallest] = 1.0f;
	ProjectPointOnPlane(dst, basis, src);
	(void)VectorNormalize(dst);
}
#else
void PerpendicularVector(vec3_t dst, const vec3_t src)
{
	int    pos;
	int    i;
	float  minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if (Q_fabs(src[i]) < minelem)
		{
			pos     = i;
			minelem = Q_fabs(src[i]);
		}
	}
	tempvec[0]   = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane(dst, tempvec, src);

	/*
	** normalize the result
	*/
	VectorNormalize(dst);
}
#endif

// Ridah
/*
=================
GetPerpendicularViewVector

  Used to find an "up" vector for drawing a sprite so that it always faces the view as best as possible
=================
*/
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
void GetPerpendicularViewVector(const vec3_t point, const vec3_t p1,
                                const vec3_t p2, vec3_t up)
{
	vec3_t v1, v2;
	/* Differences must round to Float before each normalization call. */
	TCE_VectorDifferenceFloat(p1, point, v1);
	(void)VectorNormalize(v1);
	TCE_VectorDifferenceFloat(p2, point, v2);
	(void)VectorNormalize(v2);
	CrossProduct(v1, v2, up);
	(void)VectorNormalize(up);
}
#else
void GetPerpendicularViewVector(const vec3_t point, const vec3_t p1, const vec3_t p2, vec3_t up)
{
	vec3_t v1, v2;

	VectorSubtract(point, p1, v1);
	VectorNormalize(v1);

	VectorSubtract(point, p2, v2);
	VectorNormalize(v2);

	CrossProduct(v1, v2, up);
	VectorNormalize(up);
}
#endif

/*
================
ProjectPointOntoVector
================
*/
#if defined(_WIN32) || defined(__linux__)
void ProjectPointOntoVector(vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj)
{
	vec3_t      pVec, direction;
	long double dot;
#if !defined(_WIN32)
	long double productX, productY, productZ;
#endif

	TCE_VectorDifferenceFloat(vStart, point, pVec);
	TCE_VectorDifferenceFloat(vStart, vEnd, direction);
	(void)VectorNormalize(direction);
#if defined(_WIN32)
	/* The original keeps the dot value across all three output stores. */
	dot      = (long double)direction[2] * (long double)pVec[2];
	dot     += (long double)direction[1] * (long double)pVec[1];
	dot     += (long double)direction[0] * (long double)pVec[0];
	vProj[0] = (float)(dot * (long double)direction[0] + (long double)vStart[0]);
	vProj[1] = (float)(dot * (long double)direction[1] + (long double)vStart[1]);
	vProj[2] = (float)(dot * (long double)direction[2] + (long double)vStart[2]);
#else
	dot  = (long double)pVec[0] * (long double)direction[0];
	dot += (long double)pVec[1] * (long double)direction[1];
	dot += (long double)pVec[2] * (long double)direction[2];
	/* Linux computes the products before writing output. vStart is reloaded
	 * after each preceding store, so overlapping input/output stays meaningful.
	 */
	productX = (long double)direction[0] * dot;
	productY = (long double)direction[1] * dot;
	productZ = (long double)direction[2] * dot;
	vProj[0] = (float)(productX + (long double)vStart[0]);
	vProj[1] = (float)(productY + (long double)vStart[1]);
	vProj[2] = (float)(productZ + (long double)vStart[2]);
#endif
}
#else
void ProjectPointOntoVector(vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj)
{
	vec3_t pVec, vec;

	VectorSubtract(point, vStart, pVec);
	VectorSubtract(vEnd, vStart, vec);
	VectorNormalize(vec);
	// project onto the directional vector for this segment
	VectorMA(vStart, DotProduct(pVec, vec), vec, vProj);
}
#endif

/*
================
ProjectPointOntoVectorBounded
================
*/
void ProjectPointOntoVectorBounded(vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj)
{
	vec3_t pVec, vec;
	int    j;

	VectorSubtract(point, vStart, pVec);
	VectorSubtract(vEnd, vStart, vec);
	VectorNormalize(vec);
	// project onto the directional vector for this segment
	VectorMA(vStart, DotProduct(pVec, vec), vec, vProj);
	// check bounds
	for (j = 0; j < 3; j++)
#if defined(__linux__) && !defined(Q3_VM)
		if ((vProj[j] > vStart[j] && vProj[j] > vEnd[j]) ||
		    (!(vProj[j] >= vStart[j]) && !(vProj[j] >= vEnd[j])))
#else
		if ((vProj[j] > vStart[j] && vProj[j] > vEnd[j]) ||
		    (vProj[j] < vStart[j] && vProj[j] < vEnd[j]))
#endif
		{
			break;
		}
	if (j < 3)
	{
#if defined(__linux__) && !defined(Q3_VM)
		float startDistance = Q_fabs(vProj[j] - vStart[j]);
		if (!(startDistance >= Q_fabs(vProj[j] - vEnd[j])))
#else
		if (Q_fabs(vProj[j] - vStart[j]) < Q_fabs(vProj[j] - vEnd[j]))
#endif
		{
			VectorCopy(vStart, vProj);
		}
		else
		{
			VectorCopy(vEnd, vProj);
		}
	}
}

/*
================
DistanceFromLineSquared
================
*/
#if defined(_WIN32) || defined(__linux__)
long double TCE_DistanceFromLineSquaredValue(vec3_t p, vec3_t lp1, vec3_t lp2)
{
	vec3_t      proj, delta;
	const float *closest = proj;
	int         j;

	ProjectPointOntoVector(p, lp1, lp2, proj);
	for (j = 0; j < 3; ++j)
	{
#if defined(_WIN32)
		/* C0-only lower gates also accept unordered comparisons. */
		if ((proj[j] > lp1[j] && proj[j] > lp2[j]) ||
		    (!(proj[j] >= lp1[j]) && !(proj[j] >= lp2[j])))
		{
			break;
		}
#else
		/* Linux compares the first endpoint against the projection first. */
		if ((!(lp1[j] >= proj[j]) && proj[j] > lp2[j]) ||
		    (lp1[j] > proj[j] && !(proj[j] >= lp2[j])))
		{
			break;
		}
#endif
	}
	if (j < 3)
	{
		volatile float firstDifference  = (float)((long double)proj[j] - (long double)lp1[j]);
		volatile float firstAbs         = Q_fabs(firstDifference);
		volatile float secondDifference = (float)((long double)proj[j] - (long double)lp2[j]);
		long double    secondAbs        = Q_fabs(secondDifference);

#if defined(_WIN32)
		closest = (long double)firstAbs < secondAbs ? lp1 : lp2;
#else
		/* Unordered endpoint-distance selection takes lp1 on Linux. */
		closest = !((long double)firstAbs >= secondAbs) ? lp1 : lp2;
#endif
	}
	TCE_VectorDifferenceFloat(closest, p, delta);
	return TCE_VectorLengthSquaredValue(delta);
}
float DistanceFromLineSquared(vec3_t p, vec3_t lp1, vec3_t lp2)
{
	return (float)TCE_DistanceFromLineSquaredValue(p, lp1, lp2);
}
#else
float DistanceFromLineSquared(vec3_t p, vec3_t lp1, vec3_t lp2)
{
	vec3_t proj, t;
	int    j;

	ProjectPointOntoVector(p, lp1, lp2, proj);
	for (j = 0; j < 3; j++)
		if ((proj[j] > lp1[j] && proj[j] > lp2[j]) ||
		    (proj[j] < lp1[j] && proj[j] < lp2[j]))
		{
			break;
		}
	if (j < 3)
	{
		if (Q_fabs(proj[j] - lp1[j]) < Q_fabs(proj[j] - lp2[j]))
		{
			VectorSubtract(p, lp1, t);
		}
		else
		{
			VectorSubtract(p, lp2, t);
		}
		return VectorLengthSquared(t);
	}
	VectorSubtract(p, proj, t);
	return VectorLengthSquared(t);
}
#endif

/*
================
DistanceFromVectorSquared
================
*/
#if defined(_WIN32) || defined(__linux__)
float DistanceFromVectorSquared(vec3_t p, vec3_t lp1, vec3_t lp2)
{
	vec3_t proj, delta;

	ProjectPointOntoVector(p, lp1, lp2, proj);
	TCE_VectorDifferenceFloat(proj, p, delta);
	return (float)TCE_VectorLengthSquaredValue(delta);
}
#else
float DistanceFromVectorSquared(vec3_t p, vec3_t lp1, vec3_t lp2)
{
	vec3_t proj, t;

	ProjectPointOntoVector(p, lp1, lp2, proj);
	VectorSubtract(p, proj, t);
	return VectorLengthSquared(t);
}
#endif

/* Native scalar stages; FPATAN/libm and external x87 ABI remain unverified. */
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
long double TCE_VectorYawValue(const vec3_t vec)
{
	long double yaw;
#if defined(__linux__)
	float y = vec[1];
	float x = vec[0];
	if (TCE_FloatIsZeroOrNaN(y) && TCE_FloatIsZeroOrNaN(x))
	{
		return 0.0L;
	}
	if (!TCE_FloatIsZeroOrNaN(x))
	{
		volatile double radians = (double)atan2l((long double)y, (long double)x);
		volatile float  degrees = (float)((long double)radians *
		                                  (long double)57.29577951308232);
		yaw = (long double)degrees;
	}
	else
	{
		yaw = y > 0.0f ? 90.0L : 270.0L;
	}
#else
	if (TCE_FloatIsZeroOrNaN(vec[1]) && TCE_FloatIsZeroOrNaN(vec[0]))
	{
		return 0.0L;
	}
	if (!TCE_FloatIsZeroOrNaN(vec[0]))
	{
		long double y = (long double)vec[1];
		long double x = (long double)vec[0];
		yaw = atan2l(y, x) * (long double)57.29577791868204;
	}
	else
	{
		return vec[1] > 0.0f ? 90.0L : 270.0L;
	}
#endif
	/* C0/CF includes unordered on both platforms. No Float spill after add. */
	if (!(yaw >= 0.0L))
	{
		yaw = yaw + 360.0L;
	}
	return yaw;
}

float vectoyaw(const vec3_t vec)
{
	/* Float API consumers round here; wide consumers use the value helper. */
	return (float)TCE_VectorYawValue(vec);
}
#else
float vectoyaw(const vec3_t vec)
{
	float yaw;

	if (vec[YAW] == 0 && vec[PITCH] == 0)
	{
		yaw = 0;
	}
	else
	{
		if (vec[PITCH])
		{
			yaw = (atan2(vec[YAW], vec[PITCH]) * 180 / M_PI);
		}
		else if (vec[YAW] > 0)
		{
			yaw = 90;
		}
		else
		{
			yaw = 270;
		}
		if (yaw < 0)
		{
			yaw += 360;
		}
	}

	return yaw;
}
#endif

/*
=================
AxisToAngles

  Used to convert the MD3 tag axis to MDC tag angles, which are much smaller

  This doesn't have to be fast, since it's only used for conversion in utils, try to avoid
  using this during gameplay
=================
*/
void AxisToAngles(vec3_t axis[3], vec3_t angles)
{
	vec3_t right, roll_angles, tvec;

	// first get the pitch and yaw from the forward vector
	vectoangles(axis[0], angles);

	// now get the roll from the right vector
	VectorCopy(axis[1], right);
	// get the angle difference between the tmpAxis[2] and axis[2] after they have been reverse-rotated
	RotatePointAroundVector(tvec, axisDefault[2], right, -angles[YAW]);
	RotatePointAroundVector(right, axisDefault[1], tvec, -angles[PITCH]);
	// now find the angles, the PITCH is effectively our ROLL
	vectoangles(right, roll_angles);
	roll_angles[PITCH] = AngleNormalize180(roll_angles[PITCH]);
	// if the yaw is more than 90 degrees difference, we should adjust the pitch
	if (DotProduct(right, axisDefault[1]) < 0)
	{
		if (roll_angles[PITCH] < 0)
		{
			roll_angles[PITCH] = -90 + (-90 - roll_angles[PITCH]);
		}
		else
		{
			roll_angles[PITCH] = 90 + (90 - roll_angles[PITCH]);
		}
	}

	angles[ROLL] = -roll_angles[PITCH];
}

float VectorDistance(vec3_t v1, vec3_t v2)
{
	vec3_t dir;

	VectorSubtract(v2, v1, dir);
	return VectorLength(dir);
}


float VectorDistanceSquared(vec3_t v1, vec3_t v2)
{
	vec3_t dir;

	VectorSubtract(v2, v1, dir);
	return VectorLengthSquared(dir);
}
// done.
