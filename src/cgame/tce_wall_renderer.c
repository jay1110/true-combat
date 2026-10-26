#include "tce_wall_renderer.h"
#include "../game/tce_length_precision.h"
#include "../game/tce_gust_precision.h"
#include "../game/tce_numeric.h"

/* Native SAR32 by four, including negative wrapped mark lifetimes. */
static int TCE_WallMarkFade(int duration)
{
	return duration >= 0 ? duration / 16 : -1 - (int)((unsigned)(-1 - duration) / 16u);
}

static float TCE_WallMarkRotation(unsigned sample)
{
#if defined(__linux__)
	return (float)(20.0L - (long double)sample * 0.0012207403779029846f);
#else
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	tceTriggerSum_t angle, factor, base;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (TCE_FiniteExceptionsMasked(control))
	{
		TCE_TriggerProduct(&angle, (float)sample, 3.0518509447574615e-05f, control);
		TCE_TriggerProduct(&factor, 40.0f, 1.0f, control);
		TCE_GustMultiply(&angle, &factor, control);
		angle.negative ^= 1u;
		TCE_TriggerProduct(&base, 20.0f, 1.0f, control);
		TCE_RotateAdd(&base, &angle, control);
		return TCE_RotateStore(base, control);
	}
#endif
	return (float)(20.0L - (long double)sample * 3.0518509447574615e-05f * 40.0f);
#endif
}

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
/* The Windows normalizer leaves its root on ST0 through both range gates. */
static int TCE_WallEchoGate(const float *delta, int *audible)
{
	tceTriggerSum_t squared, term, root, nearLimit, farLimit;
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&squared, delta[0], delta[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&term, delta[axis], delta[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&squared, &term, control);
	}
	root = TCE_LengthRoot(&squared, control);
	TCE_TriggerProduct(&nearLimit, 1200.0f, 1.0f, control);
	TCE_TriggerProduct(&farLimit, 8000.0f, 1.0f, control);
	*audible = TCE_TriggerCompare(&root, &nearLimit) > 0 &&
	           TCE_TriggerCompare(&root, &farLimit) < 0;
	return 1;
}
#endif

static float TCE_WallEchoCoordinate(float direction, float range, float listener)
{
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	tceTriggerSum_t offset, origin;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (TCE_FiniteExceptionsMasked(control) &&
	    TCE_TriggerProduct(&offset, direction, range, control) &&
	    TCE_TriggerProduct(&origin, listener, 1.0f, control))
	{
		TCE_RotateAdd(&offset, &origin, control);
		return TCE_RotateStore(offset, control);
	}
#endif
	return (float)((long double)direction * range + listener);
}

static void TCE_WallProjection(const float *origin, const float *normal, float radius,
                               float *projection, float *projectOrigin)
{
	int axis;
#if defined(__linux__)
	long double difference[3];

	projection[3] = radius * 32.0f;
	for (axis = 0; axis < 3; ++axis)
	{
		difference[axis] = (long double)vec3_origin[axis] - normal[axis];
		/* Native FST32 does not discard the retained difference. */
		projection[axis] = (float)difference[axis];
	}
	for (axis = 0; axis < 3; ++axis)
	{
		projectOrigin[axis] = (float)(difference[axis] * -16.0f + origin[axis]);
	}
#else
	for (axis = 0; axis < 3; ++axis)
	{
		projection[axis] = vec3_origin[axis] - normal[axis];
	}
	projection[3] = radius * 32.0f;
	for (axis = 0; axis < 3; ++axis)
	{
		projectOrigin[axis] = TCE_WallEchoCoordinate(projection[axis], -16.0f, origin[axis]);
	}
#endif
}

/* Native visibility compares the retained dot product, without a Float store. */
static int TCE_WallFacing(const float *facing, const float *normal)
{
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	tceTriggerSum_t dot, term, zero = { { 0 }, 0 };
	unsigned control;
	int axis, valid = 1;

	__control87_2(0, 0, &control, NULL);
	if (TCE_FiniteExceptionsMasked(control) &&
	    TCE_TriggerProduct(&dot, facing[2], normal[2], control))
	{
		for (axis = 1; axis >= 0; --axis)
		{
			if (!TCE_TriggerProduct(&term, facing[axis], normal[axis], control))
			{
				valid = 0;
				break;
			}
			TCE_RotateAdd(&dot, &term, control);
		}
		if (valid)
		{
			return !dot.negative && TCE_TriggerMagnitude(&dot, &zero) > 0;
		}
	}
#endif
	{
#if defined(__linux__)
		long double dot = (long double)facing[1] * normal[1] + (long double)facing[0] * normal[0];
		dot += (long double)facing[2] * normal[2];
#else
		long double dot = (long double)facing[2] * normal[2] + (long double)facing[1] * normal[1];
		dot += (long double)facing[0] * normal[0];
#endif
		return dot > 0.0L;
	}
}

/* Keep cached media live across effect callbacks. Unbound compatibility
 * slots retain the previous zero-initialized context value. */
static int TCE_WallHandle(const int *handle)
{
	return handle ? *handle : 0;
}

/* Entire Windows 30078260 controller. See wall_full_abi_windows.c.txt and
 * its x87 conversion listing. Dependencies retain their original ABIs. */
void TCE_CG_MissileHitWall(int weapon, int effectType, vec3_t origin, vec3_t normal, vec3_t direction,
                           unsigned surfaceFlags, int forceEffect, int materialIsType, const tce_wallRendererContext_t *ctx)
{
	unsigned randomBits, bits, randomComponent;
	/* direction is retained by the original eight-argument ABI but not read. */
	(void)direction;
	int     value0, value1, value2, value3, particleIndex, impactSound, markShader, farSound, markDuration, soundVolume, materialType, visible, randomValue;
	float   *component, valueFloat0, valueFloat1, valueFloat2, valueFloat3, markAlpha, viewY, viewZ;
#if defined(_WIN32)
	long double distance;
#else
	double distance;
#endif
	vec3_t  position, velocity, particleDir, traceStart, traceEnd, farOrigin, farDir, projectOrigin;
	vec3_t  up = { 0, 0, 1 };
	vec4_t  projection, color;
	trace_t trace;
	float   radius, farRange;
	farRange       = 0.0;
	materialType   = 0;
	particleDir[0] = 0.0;
	particleDir[1] = 0.0;
	particleDir[2] = 0.0;
	visible        = 0;
	if ((((forceEffect != 0) || ((*ctx->forceMarks) != 0)) ||
	     TCE_WallFacing((*ctx->view)->viewaxis[0], normal)) ||
	    !((*ctx->view)->fov_x >= 30.0f))
	{
		visible = 1;
	}
	markShader  = 0;
	radius      = 32.0;
	impactSound = 0;
	farSound    = 0;
	soundVolume = ctx->soundVolume(origin, 127.0, 1800.0, 0);
	if ((*ctx->portal) != 0)
	{
		ctx->print("ELITE PORTAL: CG_MissileHitWall\n");
	}
	markDuration = -1;
	if ((materialIsType != 0) || (weapon == 1))
	{
		materialType = surfaceFlags;
		surfaceFlags = ctx->surfaceType(surfaceFlags);
	}
	bits = surfaceFlags & 0xff000000;
	if ((surfaceFlags & 4U) != 0)
	{
		return;
	}
	switch (weapon)
	{
	case 1:
		if (materialType == 0x10)
		{
			bits = rand();
			bits = bits & 0x80000003;
			if ((int)bits < 0)
			{
				bits = (bits - 1 | 0xfffffffc) + 1;
			}
			impactSound  = TCE_WallHandle(ctx->media.knifeFlesh[bits]);
			markDuration = (*ctx->markTime);
		}
		else
		{
			if (((bits != 0x1e000000) && (bits != 0x1d000000)) &&
			    ((((bits != 0x9000000 && ((bits != 0xa000000 && (bits != 0x7000000)))) &&
			       (bits != 0xd000000)) && ((bits != 0x8000000 && (bits != 0x15000000))))))
			{
				impactSound = TCE_WallHandle(ctx->media.knifeWall);
				markShader  = TCE_WallHandle(ctx->media.knife);
				bits        = rand();
				visible     = 1;
				radius      = (float)(bits & 0x7fff) * 3.0518509447574615e-05f * 0.5f + 1.5f;
			}
			markDuration = (*ctx->markTime);
		}
		break;
	case 2:
	case 3:
	case 7:
	case 8:
	case 10:
	case 0xe:
	case 0x17:
	case 0x18:
	case 0x19:
	case 0x1f:
	case 0x20:
	case 0x21:
	case 0x25:
	case 0x26:
	case 0x27:
	case 0x28:
	case 0x29:
	case 0x2a:
	case 0x2b:
	case 0x2c:
	case 0x2d:
	case 0x2e:
	case 0x2f:
	case 0x33:
	case 0x34:
	case 0x35:
	case 0x36:
	case 0x39:
	case 0x3a:
	case 0x3b:
	case 0x3e:
		randomValue = rand();
		if (effectType == 0)
		{
			valueFloat0 = 0.2;
			value0      = rand();
			ctx->bulletParticles(origin, normal, 0x15e, 200, value0 % 7 + 0xf, valueFloat0);
		}
		else
		{
			if (effectType == 1)
			{
				if ((((((bits == 0x1e000000) || (bits == 0x1d000000)) || (bits == 0xa000000)) ||
				      ((bits == 0x7000000 || (bits == 0xd000000)))) || (bits == 0x8000000)) ||
				    (bits == 0x15000000))
				{
					value0 = 5;
					if (TCE_WallHandle(ctx->weaponMaterialMode[weapon]) != 0)
					{
						value0 = 2;
					}
					if (bits == 0xd000000)
					{
						valueFloat1 = 0.2;
						valueFloat0 = 0.25;
						value1      = TCE_WallHandle(ctx->media.water);
					}
					else
					{
						value1 = TCE_WallHandle(ctx->media.snow);
						if (((bits == 0x8000000) || (value1 = TCE_WallHandle(ctx->media.grass), bits == 0x7000000)) ||
						    (value1 = TCE_WallHandle(ctx->media.dirt), bits != 0x1e000000))
						{
							valueFloat1 = 0.33;
							valueFloat0 = 0.5;
						}
						else
						{
							valueFloat1 = 0.33;
							valueFloat0 = 0.5;
							value1      = TCE_WallHandle(ctx->media.mud);
						}
					}
					ctx->dirtParticles(origin, normal, 0xbe, 900, value0, valueFloat0, 40.0, 8.0, valueFloat1, value1);
				}
				else if ((((((bits == 0x9000000) || (bits == 0x12000000)) || (bits == 0x17000000)) ||
				           ((bits == 0x14000000 || (bits == 0x5000000)))) ||
				          ((bits == 0x6000000 || ((bits == 0xc000000 || (bits == 0x10000000)))))) ||
				         (TCE_WallHandle(ctx->weaponMaterialMode[weapon]) == 0))
				{
					if (((bits == 0x3000000) || (bits == 0x4000000)) || (bits == 0x1f000000))
					{
						randomBits = rand();
						ctx->particle(origin, normal, 0, 0,
						              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
						              0.75f);
						if (materialIsType == 0)
						{
							randomValue = rand();
							value0      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 8 + 10);
							ctx->flatSparks(origin, normal, value0);
							randomBits = rand();
							if ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f <= 0.5f)
							{
								goto LAB_30078e49;
							}
							randomValue = rand();
						}
						else
						{
							randomValue = rand();
						}
						value0 = (int)((((double)(randomValue & 32767) * 0.000030518509447574615) + 1) * 4);
						ctx->sparks(origin, normal, value0);
LAB_30078e49:
						valueFloat0 = 1.0;
						randomBits  = rand();
						randomBits  = randomBits & 0x80000007;
						if ((int)randomBits < 0)
						{
							randomBits = (randomBits - 1 | 0xfffffff8) + 1;
						}
						value0 = randomBits + 0xf;
						value1 = 0x4b;
					}
					else
					{
						if (bits == 0x14000000)
						{
							VectorInverse(normal);
							randomValue = rand();
							value0      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 4 + 6);
							ctx->flatSparks(origin, normal, value0);
							randomBits = rand();
							if (0.75f < (float)(randomBits & 0x7fff) * 3.0518509447574615e-05f)
							{
								randomValue = rand();
								/* 3007872d uses 3, unlike the metal path's shared ftol (4). */
								value0 = (int)(((double)(randomValue & 32767) * 0.000030518509447574615 + 1) * 3);
								ctx->sparks(origin, normal, value0);
							}
							goto LAB_30078e49;
						}
						if (bits == 0xb000000)
						{
							randomBits = rand();
							ctx->particle(origin, normal, 0, 10,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							value0 = ctx->registerShader("models/debris/straw1");
							ctx->explode(origin, normal, 0x28, 7, 0, 1, value0);
							goto LAB_30078cfb;
						}
						if (bits == 0x5000000)
						{
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							randomBits = rand();
LAB_300787e6:
							ctx->particle(origin, normal, 0, 5,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							value0 = ctx->registerShader("models/debris/wood1");
							ctx->explode(origin, normal, 0x19, 8, 0, 1, value0);
							goto LAB_30078cfb;
						}
						if (bits == 0x6000000)
						{
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							randomBits = rand();
							goto LAB_300787e6;
						}
						if (bits == 0x17000000)
						{
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0x10,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							goto LAB_30078cfb;
						}
						if (bits == 0x2000000)
						{
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 2,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							goto LAB_30078cfb;
						}
						if (bits == 0x18000000)
						{
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0x1a,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							goto LAB_30078cfb;
						}
						if (bits == 0x19000000)
						{
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0x1b,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							goto LAB_30078cfb;
						}
						if (bits == 0x1a000000)
						{
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0x1c,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							goto LAB_30078cfb;
						}
						if (bits == 0x1000000)
						{
							valueFloat0 = 1.0;
							value0      = rand();
							ctx->debrisParticles(origin, normal, 0x14, 0x32, value0 % 5, valueFloat0);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0x16,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							goto LAB_30078cfb;
						}
						if (bits == 0x1b000000)
						{
							valueFloat0 = 1.0;
							value0      = rand();
							ctx->debrisParticles(origin, normal, 0x14, 0x32, value0 % 5, valueFloat0);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0x1d,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							goto LAB_30078cfb;
						}
						if (bits == 0x1c000000)
						{
							valueFloat0 = 1.0;
							value0      = rand();
							ctx->debrisParticles(origin, normal, 0x14, 0x32, value0 % 5, valueFloat0);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							randomBits = rand();
							ctx->particle(origin, normal, 0, 0x1e,
							              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
							              0.75f);
							goto LAB_30078cfb;
						}
						randomBits = rand();
						ctx->particle(origin, normal, 0, 0,
						              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) *
						              0.75f);
						if ((((bits == 0xc000000) || (bits == 0x21000000)) ||
						     ((bits == 0xf000000 || ((bits == 0x20000000 || (bits == 0x12000000)))))) ||
						    (bits == 0x10000000))
						{
							goto LAB_30078cfb;
						}
						valueFloat0 = 1.0;
						value0      = rand();
						value0      = value0 % 5;
						value1      = 0x32;
					}
					ctx->debrisParticles(origin, normal, 0x14, value1, value0, valueFloat0);
				}
				else
				{
					randomBits = rand();
					ctx->particle(origin, normal, 0, 0,
					              ((float)(randomBits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f)
					;
					valueFloat0 = 1.0;
					randomBits  = rand();
					randomBits  = randomBits & 0x80000003;
					if ((int)randomBits < 0)
					{
						randomBits = (randomBits - 1 | 0xfffffffc) + 1;
					}
					ctx->debrisParticles(origin, normal, 0x14, 0x15e, randomBits + 4, valueFloat0);
					randomBits = rand();
					if (0.5f < (float)(randomBits & 0x7fff) * 3.0518509447574615e-05f)
					{
						randomValue = rand();
						value0      = (int)((((double)(randomValue & 32767) * 0.000030518509447574615) + 1) * 4);
						ctx->sparks(origin, normal, value0);
					}
					if (((bits == 0x3000000) || (bits == 0x4000000)) || (bits == 0x1f000000))
					{
						randomValue = rand();
						value0      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 8 + 10);
						ctx->flatSparks(origin, normal, value0);
					}
				}
LAB_30078cfb:
				markShader = TCE_WallHandle(ctx->media.bulletDefault);
				randomBits = rand();
				radius     = (float)(randomBits & 0x7fff) * 3.0518509447574615e-05f * 0.5f + 1.0f;
				if (bits == 0x14000000)
				{
					value0       = rand();
					markShader   = 0;
					impactSound  = TCE_WallHandle(ctx->media.fabricHit[value0 % 5]);
					markDuration = (*ctx->markTime);
				}
				else if (((bits == 0x3000000) || (bits == 0x13000000)) || (bits == 0x1f000000))
				{
					value0      = rand();
					impactSound = TCE_WallHandle(ctx->media.metalHit[value0 % 5]);
					if (materialIsType == 0)
					{
LAB_3007924b:
						markShader = TCE_WallHandle(ctx->media.bulletMetal);
						goto LAB_30079255;
					}
					markShader   = TCE_WallHandle(ctx->media.exitMetal);
					markDuration = (*ctx->markTime);
				}
				else if (bits == 0x4000000)
				{
					value0      = rand();
					impactSound = TCE_WallHandle(ctx->media.metal2Hit[value0 % 5]);
					if (materialIsType == 0)
					{
						goto LAB_3007924b;
					}
					markShader   = TCE_WallHandle(ctx->media.exitMetal);
					markDuration = (*ctx->markTime);
				}
				else if ((bits == 0x5000000) || (bits == 0x6000000))
				{
					value0      = rand();
					impactSound = TCE_WallHandle(ctx->media.woodHit[value0 % 5]);
					if (materialIsType == 0)
					{
						radius       = radius + 0.4000000059604645f;
						markShader   = TCE_WallHandle(ctx->media.bulletWood);
						markDuration = (*ctx->markTime);
					}
					else
					{
						radius       = radius + 0.4000000059604645f;
						markShader   = TCE_WallHandle(ctx->media.exitWood);
						markDuration = (*ctx->markTime);
					}
				}
				else if (bits == 0xc000000)
				{
					value0       = rand();
					markShader   = TCE_WallHandle(ctx->media.bulletGlass);
					impactSound  = TCE_WallHandle(ctx->media.glassHit[value0 % 5]);
					markDuration = (*ctx->markTime);
				}
				else if (bits == 0xa000000)
				{
LAB_30079255:
					markDuration = (*ctx->markTime);
				}
				else
				{
					if (bits == 0xb000000)
					{
						value0      = rand();
						impactSound = TCE_WallHandle(ctx->media.strawHit[value0 % 5]);
						markShader  = TCE_WallHandle(ctx->media.bulletWood);
						if (materialIsType != 0)
						{
							radius       = radius + 0.6000000238418579f;
							markShader   = TCE_WallHandle(ctx->media.exitWood);
							markDuration = (*ctx->markTime);
							break;
						}
					}
					else
					{
						if ((((((bits == 0x7000000) || (bits == 0xd000000)) || (bits == 0x10000000)) ||
						      ((bits == 0x8000000 || (bits == 0x12000000)))) ||
						     ((bits == 0xf000000 || ((bits == 0x20000000 || (bits == 0x21000000)))))) ||
						    ((bits == 0x1e000000 ||
						      (((bits == 0x1d000000 || (bits == 0x17000000)) || (bits == 0x16000000))))))
						{
							goto LAB_30079255;
						}
						value0      = rand();
						impactSound = TCE_WallHandle(ctx->media.stoneHit[value0 % 5]);
						markShader  = TCE_WallHandle(ctx->media.bulletStone);
						if (materialIsType != 0)
						{
							markShader = TCE_WallHandle(ctx->media.exitStone);
						}
					}
					radius       = radius + 0.6000000238418579f;
					markDuration = (*ctx->markTime);
				}
				break;
			}
			if (effectType != 2)
			{
				goto LAB_30078cfb;
			}
			value1     = 1000;
			value0     = 0x20;
			markShader = 0;
			component  = up;
			ctx->ripple(TCE_WallHandle(ctx->media.waterRipple), origin, component, value0, value1);
			ctx->dirtParticles(origin, normal, 0xbe, 900, 5, 0.5, 80.0, 16.0, 0.125, TCE_WallHandle(ctx->media.water));
		}
		goto LAB_3007ad98;
	case 4:
		ctx->concussion(origin, 0);
		position[0]  = *normal * 16.0f + *origin;
		impactSound  = TCE_WallHandle(ctx->media.explosive4);
		markShader   = TCE_WallHandle(ctx->media.burn);
		markDuration = 60000;
		radius       = 32.0;
		position[1]  = normal[1] * 16.0f + origin[1];
		position[2]  = normal[2] * 16.0f + origin[2];
		velocity[0]  = *normal * 100.0f;
		velocity[1]  = normal[1] * 100.0f;
		velocity[2]  = normal[2] * 100.0f;
		bits         = ctx->cmPointContents(origin, 0);
		if ((bits & 0x20) == 0)
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 20.0f;
			traceEnd[2]   = origin[2] - 20.0f;
			traceEnd[0]   = traceStart[0];
			traceEnd[1]   = traceStart[1];
			ctx->boxTrace(&trace, traceStart, traceEnd, (float *)0x0, (float *)0x0, 0,
			              0x6000081);
			trace.surfaceFlags = trace.surfaceFlags & 0xff000000;
			value0             = TCE_WallHandle(ctx->media.dirt);
			if (((trace.surfaceFlags == 0xa000000) || (trace.surfaceFlags == 0x1d000000)) ||
			    (((trace.surfaceFlags == 0x15000000 ||
			       ((value0 = TCE_WallHandle(ctx->media.snow), trace.surfaceFlags == 0x8000000 ||
			         (value0 = TCE_WallHandle(ctx->media.grass), trace.surfaceFlags == 0x7000000)))) ||
			      (value0 = TCE_WallHandle(ctx->media.mud), trace.surfaceFlags == 0x1e000000))))
			{
				ctx->dirtParticles(origin, normal, 0xfa, 900, 10, 0.5, 48.0, 24.0, 0.25, value0);
			}
		}
		else
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 10000.0f;
			ctx->boxTrace(&trace, traceStart, origin, (float *)0x0, (float *)0x0, 0, 0x38);
			ctx->ripple(TCE_WallHandle(ctx->media.waterRipple), trace.endpos, normal, 0x96, 1000);
			ctx->dirtParticles(trace.endpos, normal, 0xfa, 900, 0xf, 0.5, 64.0, 32.0, 0.125, TCE_WallHandle(ctx->media.water));
		}
		bits = rand();
		ctx->particle(origin, normal, 0, 0,
		              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
		randomValue = rand();
		value0      = (int)((((double)(randomValue & 32767) * 0.000030518509447574615) + 1) * 8);
		ctx->sparks(origin, normal, value0);
		valueFloat0 = 1.0;
		value0      = rand();
		ctx->debrisParticles(origin, normal, 0x3c, 500, value0 % 0xc + 0x18, valueFloat0);
		ctx->smokePuff(position, vec3_origin, 256.0, 1.0, 1.0, 1.0, 1.0, 250.0, (*ctx->time), 0, 1, TCE_WallHandle(ctx->media.explosionFlash))
		;
		goto LAB_300796c2;
	default:
		goto switchD_3007838b_caseD_5;
	case 9:
	case 0x1a:
	case 0x1b:
	case 0x37:
	case 0x38:
	case 0x3c:
		if (weapon == 0x1b)
		{
			impactSound = ctx->media.dynamite;
			farSound    = ctx->media.dynamiteFar;
		}
		else if (weapon == 0x1a)
		{
			impactSound = ctx->media.satchel;
			farSound    = ctx->media.satchelFar;
		}
		else if (weapon == 0x3c)
		{
			farSound    = 0;
			impactSound = 0;
		}
		else if (((weapon == 4) || (weapon == 9)) || ((weapon == 0x37 || (weapon == 0x38))))
		{
			impactSound = TCE_WallHandle(ctx->media.grenade);
			farSound    = TCE_WallHandle(ctx->media.grenadeFar);
		}
		else
		{
			impactSound = TCE_WallHandle(ctx->media.explosion);
			farSound    = TCE_WallHandle(ctx->media.explosionFar);
		}
		markDuration = TCE_MulInt32(*ctx->markTime, 3);
		farRange     = 400.0;
		markShader   = TCE_WallHandle(ctx->media.burn);
		radius       = 48.0;
		ctx->concussion(origin, 1);
		position[0] = *normal * 16.0f + *origin;
		position[1] = normal[1] * 16.0f + origin[1];
		position[2] = normal[2] * 16.0f + origin[2];
		velocity[0] = *normal * 50.0f;
		velocity[1] = normal[1] * 50.0f;
		velocity[2] = normal[2] * 50.0f;
		bits        = ctx->pointContents(origin, 0);
		if ((bits & 0x20) == 0)
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 20.0f;
			traceEnd[2]   = origin[2] - 20.0f;
			traceEnd[0]   = traceStart[0];
			traceEnd[1]   = traceStart[1];
			ctx->boxTrace(&trace, traceStart, traceEnd, (float *)0x0, (float *)0x0, 0,
			              0x6000081);
			trace.surfaceFlags = trace.surfaceFlags & 0xff000000;
			value0             = TCE_WallHandle(ctx->media.dirt);
			if (((trace.surfaceFlags == 0xa000000) || (trace.surfaceFlags == 0x15000000)) ||
			    ((trace.surfaceFlags == 0x1d000000 ||
			      (((value0 = TCE_WallHandle(ctx->media.snow), trace.surfaceFlags == 0x8000000 ||
			         (value0 = TCE_WallHandle(ctx->media.grass), trace.surfaceFlags == 0x7000000)) ||
			        (value0 = TCE_WallHandle(ctx->media.mud), trace.surfaceFlags == 0x1e000000))))))
			{
				ctx->dirtParticles(origin, normal, 400, 2000, 10, 0.5, 200.0, 75.0, 0.25, value0);
			}
			particleDir[2] = 0.2;
			particleDir[0] = 1.0;
			bits           = rand();
			ctx->particle(origin, particleDir, 0, 0x27,
			              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
			particleDir[0] = -1.0;
			bits           = rand();
			ctx->particle(origin, particleDir, 0, 0x27,
			              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
			particleDir[0] = 0.71;
			particleDir[1] = 0.71;
			bits           = rand();
			ctx->particle(origin, particleDir, 0, 0x27,
			              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
			particleDir[0] = 0.71;
			particleDir[1] = -0.71;
			bits           = rand();
			ctx->particle(origin, particleDir, 0, 0x27,
			              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
			particleDir[0] = -0.71;
			particleDir[1] = 0.71;
			bits           = rand();
			ctx->particle(origin, particleDir, 0, 0x27,
			              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
			particleDir[0] = -0.71;
			particleDir[1] = -0.71;
			bits           = rand();
			ctx->particle(origin, particleDir, 0, 0x27,
			              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
			particleDir[0] = 0.0;
			particleDir[1] = 1.0;
			bits           = rand();
			ctx->particle(origin, particleDir, 0, 0x27,
			              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
			particleDir[1] = -1.0;
			bits           = rand();
			ctx->particle(origin, particleDir, 0, 0x27,
			              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
			randomValue = rand();
			value0      = (int)((((double)(randomValue & 32767) * 0.000030518509447574615) + 1) * 8);
			ctx->sparks(origin, normal, value0);
			valueFloat0 = 1.0;
			value0      = rand();
			ctx->debrisParticles(origin, normal, 0x3c, 500, value0 % 0xc + 0x18, valueFloat0);
			valueFloat0 = 1.0;
			bits        = rand();
			bits        = bits & 0x80000007;
			if ((int)bits < 0)
			{
				bits = (bits - 1 | 0xfffffff8) + 1;
			}
			ctx->debrisParticles(origin, normal, 0x28, 100, bits + 0x10, valueFloat0);
			ctx->smokePuff(position, vec3_origin, 64.0, 1.0, 0.8, 0.1, 1.0, 250.0, (*ctx->time), 0, 1, TCE_WallHandle(ctx->media.explosionFlash)
			               );
			ctx->smokePuff(position, vec3_origin, 32.0, 1.0, 1.0, 0.9, 1.0, 250.0, (*ctx->time), 0, 1, TCE_WallHandle(ctx->media.explosionFlash)
			               );
		}
		else
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 10000.0f;
			impactSound   = TCE_WallHandle(ctx->media.explosionWater);
			ctx->boxTrace(&trace, traceStart, origin, (float *)0x0, (float *)0x0, 0, 0x38);
			ctx->ripple(TCE_WallHandle(ctx->media.waterRipple), trace.endpos, normal, 0x96, 1000);
			ctx->dirtParticles(trace.endpos, normal, 400, 900, 0xf, 0.5, 256.0, 128.0, 0.125, TCE_WallHandle(ctx->media.water));
		}
		goto LAB_300796c2;
	case 0xf:
	case 0x1d:
		impactSound  = TCE_WallHandle(ctx->media.artillery);
		farSound     = TCE_WallHandle(ctx->media.artilleryFar);
		markDuration = TCE_MulInt32(*ctx->markTime, 3);
		farRange     = 400.0;
		markShader   = TCE_WallHandle(ctx->media.burn);
		radius       = 128.0;
		bits         = ctx->pointContents(origin, 0);
		if ((bits & 0x20) == 0)
		{
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 20.0f;
			traceStart[0] = *origin;
			traceEnd[1]   = origin[1];
			traceEnd[2]   = origin[2] - 20.0f;
			traceEnd[0]   = traceStart[0];
			ctx->boxTrace(&trace, traceStart, traceEnd, (float *)0x0, (float *)0x0, 0,
			              0x6000081);
			trace.surfaceFlags = trace.surfaceFlags & 0xff000000;
			if (((trace.surfaceFlags == 0xa000000) || (trace.surfaceFlags == 0x15000000)) || (trace.surfaceFlags == 0x1d000000))
			{
				valueFloat3 = 0.25;
				valueFloat2 = 256.0;
				valueFloat1 = 400.0;
				valueFloat0 = 0.5;
				value2      = 10;
				value1      = 3000;
				value0      = TCE_WallHandle(ctx->media.dirt);
				randomValue = rand();
LAB_3007996e:
				value3 = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 200 + 400);
				ctx->dirtParticles(origin, normal, value3, value1, value2, valueFloat0, valueFloat1, valueFloat2, valueFloat3, value0);
			}
			else
			{
				if (trace.surfaceFlags == 0x8000000)
				{
					valueFloat3 = 0.25;
					valueFloat2 = 256.0;
					valueFloat1 = 400.0;
					valueFloat0 = 0.5;
					value2      = 10;
					value1      = 3000;
					value0      = TCE_WallHandle(ctx->media.snow);
					randomValue = rand();
					goto LAB_3007996e;
				}
				if (trace.surfaceFlags == 0x7000000)
				{
					valueFloat3 = 0.25;
					valueFloat2 = 256.0;
					valueFloat1 = 400.0;
					valueFloat0 = 0.5;
					value2      = 10;
					value1      = 3000;
					value0      = TCE_WallHandle(ctx->media.grass);
					randomValue = rand();
					goto LAB_3007996e;
				}
				if (trace.surfaceFlags == 0x1e000000)
				{
					valueFloat3 = 0.25;
					valueFloat2 = 256.0;
					valueFloat1 = 400.0;
					valueFloat0 = 0.5;
					value2      = 10;
					value1      = 3000;
					value0      = TCE_WallHandle(ctx->media.mud);
					randomValue = rand();
					goto LAB_3007996e;
				}
			}
			particleIndex = 3;
			do
			{
				/* Component-wise position/velocity perturbation. */
				value0 = 0;
				do
				{
					bits                 = rand();
					valueFloat0          = (float)(bits & 0x7fff) * 3.0518509447574615e-05f - (float)0.5;
					position[value0 / 4] =
						(valueFloat0 + valueFloat0) * (float)150.0 +
						origin[value0 / 4];
					bits                 = rand();
					value1               = value0 + 4;
					valueFloat0          = (float)(bits & 0x7fff) * 3.0518509447574615e-05f - (float)0.5;
					velocity[value0 / 4] = (valueFloat0 + valueFloat0) * (float)0.35;
					value0               = value1;
				}
				while (value1 < 0xc);
				value3      = 0;
				velocity[0] = (trace.plane.normal[0] + velocity[0]) * 130.0f;
				velocity[1] = (trace.plane.normal[1] + velocity[1]) * 130.0f;
				velocity[2] = (trace.plane.normal[2] + velocity[2]) * 130.0f;
				randomValue = rand();
				value0      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 200 + 400);
				value2      = 0x28;
				randomValue = rand();
				value1      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 2000 + 6000);
				ctx->particleExplosion
				    ("blacksmokeanim", position, velocity, value1, value2, value0, value3);
				particleIndex = particleIndex + -1;
			}
			while (particleIndex != 0);
			particleIndex = 0;
			do
			{
				/* Component-wise position/velocity perturbation. */
				value0 = 0;
				do
				{
					bits                 = rand();
					valueFloat0          = (float)(bits & 0x7fff) * 3.0518509447574615e-05f - (float)0.5;
					position[value0 / 4] =
						(valueFloat0 + valueFloat0) * (float)100.0 +
						origin[value0 / 4];
					bits                 = rand();
					value1               = value0 + 4;
					valueFloat0          = (float)(bits & 0x7fff) * 3.0518509447574615e-05f - (float)0.5;
					velocity[value0 / 4] = (valueFloat0 + valueFloat0) * (float)0.65;
					value0               = value1;
				}
				while (value1 < 0xc);
				velocity[0] = trace.plane.normal[0] + velocity[0];
				velocity[1] = trace.plane.normal[1] + velocity[1];
				velocity[2] = trace.plane.normal[2] + velocity[2];
				bits        = rand();
				velocity[0] =
					((float)(bits & 0x7fff) * 3.0518509447574615e-05f * 100.0f + 300.0f) *
					velocity[0];
				bits        = rand();
				velocity[1] =
					((float)(bits & 0x7fff) * 3.0518509447574615e-05f * 100.0f + 300.0f) *
					velocity[1];
				bits        = rand();
				randomBits  = (unsigned)(particleIndex == 0);
				velocity[2] =
					((float)(bits & 0x7fff) * 3.0518509447574615e-05f * 100.0f + 300.0f) *
					velocity[2];
				randomValue = rand();
				value0      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 200 + 400);
				value2      = 0x28;
				value1      = rand();
				ctx->particleExplosion
				    ("explode1", position, velocity, value1 % 0x5aa + 1000, value2, value0, randomBits)
				;
				particleIndex = particleIndex + 1;
			}
			while (particleIndex < 4);
			value0      = rand();
			value1      = value0 % 0xc + 0xc;
			value0      = rand();
			value2      = value0 % 2000 + 0x578;
			randomValue = rand();
			value0      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 200 + 400);
			ctx->debris(origin, normal, value0, value2, value1);
		}
		else
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 10000.0f;
			ctx->boxTrace(&trace, traceStart, origin, (float *)0x0, (float *)0x0, 0, 0x38);
			ctx->ripple(TCE_WallHandle(ctx->media.waterRipple), trace.endpos, normal, 300, 2000);
			valueFloat3 = 0.125;
			valueFloat2 = 128.0;
			valueFloat1 = 512.0;
			valueFloat0 = 0.5;
			value3      = 0xf;
			value2      = 900;
			value0      = TCE_WallHandle(ctx->media.water);
			randomValue = rand();
			value1      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 200 + 400);
			ctx->dirtParticles(trace.endpos, normal, value1, value2, value3, valueFloat0, valueFloat1, valueFloat2, valueFloat3, value0);
			valueFloat3 = 0.125;
			valueFloat2 = 512.0;
			valueFloat1 = 128.0;
			valueFloat0 = 0.5;
			value3      = 0xf;
			value2      = 0x578;
			value0      = TCE_WallHandle(ctx->media.water);
			randomValue = rand();
			value1      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 600 + 400);
			ctx->dirtParticles(trace.endpos, normal, value1, value2, value3, valueFloat0, valueFloat1, valueFloat2, valueFloat3, value0);
		}
LAB_300796c2:
		soundVolume = 0xff;
		break;
	case 0x11:
		velocity[0]  = *normal * 16.0f;
		impactSound  = TCE_WallHandle(ctx->media.explosion);
		velocity[1]  = normal[1] * 16.0f;
		markDuration = TCE_MulInt32(*ctx->markTime, 3);
		farSound     = TCE_WallHandle(ctx->media.explosionFar);
		farRange     = 1200.0;
		markShader   = TCE_WallHandle(ctx->media.burn);
		velocity[2]  = normal[2] * 16.0f;
		radius       = 96.0;
		bits         = ctx->pointContents(origin, 0);
		if ((bits & 0x20) == 0)
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 20.0f;
			traceEnd[2]   = origin[2] - 20.0f;
			traceEnd[0]   = traceStart[0];
			traceEnd[1]   = traceStart[1];
			ctx->boxTrace(&trace, traceStart, traceEnd, (float *)0x0, (float *)0x0, 0,
			              0x6000081);
			if (((trace.surfaceFlags & 0xff000000) == 0xa000000) || ((trace.surfaceFlags & 0xff000000) == 0x7000000))
			{
				ctx->dirtParticles(origin, normal, 600, 2000, 10, 0.5, 275.0, 125.0, 0.25, TCE_WallHandle(ctx->media.dirt));
			}
			particleIndex = 5;
			do
			{
				materialType = 3;
				component    = origin;
				do
				{
					randomComponent              = rand();
					randomComponent              = randomComponent & 0x7fff;
					materialType                 = materialType + -1;
					valueFloat0                  = (float)randomComponent * 3.0518509447574615e-05f - (float)0.5;
					position[component - origin] =
						normal[component - origin] * 64.0f + *component +
						(valueFloat0 + valueFloat0) * (float)24.0;
					component = component + 1;
				}
				while (materialType != 0);
				value0      = rand();
				value3      = 0;
				velocity[2] = (float)(value0 % 0x32) + velocity[2];
				value0      = rand();
				value1      = value0 % 0x3c + 0xfa;
				value2      = 10;
				value0      = rand();
				ctx->particleExplosion
				    ("blacksmokeanim", position, velocity, value0 % 0xfa + 0xdac, value2, value1,
				    value3);
				particleIndex = particleIndex + -1;
			}
			while (particleIndex != 0);
			position[0] = *normal * 24.0f + *origin;
			position[1] = normal[1] * 24.0f + origin[1];
			position[2] = normal[2] * 24.0f + origin[2];
			velocity[0] = *normal * 64.0f;
			velocity[1] = normal[1] * 64.0f;
			velocity[2] = normal[2] * 64.0f;
			ctx->particleExplosion("explode1", position, velocity, 1000, 0x14, 300, 1);
		}
		else
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 10000.0f;
			ctx->boxTrace(&trace, traceStart, origin, (float *)0x0, (float *)0x0, 0, 0x38);
			ctx->ripple(TCE_WallHandle(ctx->media.waterRipple), trace.endpos, normal, 0x96, 1000);
			ctx->dirtParticles(trace.endpos, normal, 900, 0x708, 0xf, 0.5, 350.0, 128.0, 0.125, TCE_WallHandle(ctx->media.water));
		}
		break;
	case 0x12:
	case 0x16:
	case 0x3f:
	case 0x41:
		impactSound = TCE_WallHandle(ctx->media.explosion);
		farSound    = TCE_WallHandle(ctx->media.explosionFar);
		if ((weapon == 0x12) || (weapon == 0x3f))
		{
			value0      = rand();
			impactSound = ctx->media.rocket[(value0 % 3)];
			farSound    = ctx->media.rocketFar;
		}
		else if (weapon == 0x16)
		{
			value0      = rand();
			impactSound = ctx->media.mortar[(value0 % 3)];
			farSound    = ctx->media.mortarFar;
		}
		position[0]  = *normal * 24.0f + *origin;
		markDuration = TCE_MulInt32(*ctx->markTime, 3);
		farRange     = 800.0;
		markShader   = TCE_WallHandle(ctx->media.burn);
		radius       = 128.0;
		position[1]  = normal[1] * 24.0f + origin[1];
		position[2]  = normal[2] * 24.0f + origin[2];
		velocity[0]  = *normal * 64.0f;
		velocity[1]  = normal[1] * 64.0f;
		velocity[2]  = normal[2] * 64.0f;
		bits         = ctx->pointContents(origin, 0);
		if ((bits & 0x20) == 0)
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 20.0f;
			traceEnd[2]   = origin[2] - 20.0f;
			traceEnd[0]   = traceStart[0];
			traceEnd[1]   = traceStart[1];
			ctx->boxTrace(&trace, traceStart, traceEnd, (float *)0x0, (float *)0x0, 0,
			              0x6000081);
			trace.surfaceFlags = trace.surfaceFlags & 0xff000000;
			if (((trace.surfaceFlags == 0xa000000) || (trace.surfaceFlags == 0x15000000)) || (trace.surfaceFlags == 0x1d000000))
			{
				valueFloat3 = 0.25;
				valueFloat2 = 256.0;
				valueFloat1 = 400.0;
				valueFloat0 = 0.5;
				value2      = 10;
				value1      = 3000;
				value0      = TCE_WallHandle(ctx->media.dirt);
				randomValue = rand();
LAB_3007abb7:
				value3 = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 200 + 400);
				ctx->dirtParticles(origin, normal, value3, value1, value2, valueFloat0, valueFloat1, valueFloat2, valueFloat3, value0);
			}
			else
			{
				if (trace.surfaceFlags == 0x8000000)
				{
					valueFloat3 = 0.25;
					valueFloat2 = 256.0;
					valueFloat1 = 400.0;
					valueFloat0 = 0.5;
					value2      = 10;
					value1      = 3000;
					value0      = TCE_WallHandle(ctx->media.snow);
					randomValue = rand();
					goto LAB_3007abb7;
				}
				if (trace.surfaceFlags == 0x7000000)
				{
					valueFloat3 = 0.25;
					valueFloat2 = 256.0;
					valueFloat1 = 400.0;
					valueFloat0 = 0.5;
					value2      = 10;
					value1      = 3000;
					value0      = TCE_WallHandle(ctx->media.grass);
					randomValue = rand();
					goto LAB_3007abb7;
				}
				if (trace.surfaceFlags == 0x1e000000)
				{
					valueFloat3 = 0.25;
					valueFloat2 = 256.0;
					valueFloat1 = 400.0;
					valueFloat0 = 0.5;
					value2      = 10;
					value1      = 3000;
					value0      = TCE_WallHandle(ctx->media.mud);
					randomValue = rand();
					goto LAB_3007abb7;
				}
			}
			value1      = 1;
			randomValue = rand();
			value0      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 400 + 200);
			ctx->particleExplosion("explode1", position, velocity, 0x640, 0x14, value0, value1);
			particleIndex = 4;
			do
			{
				/* Component-wise position/velocity perturbation. */
				value0 = 0;
				do
				{
					bits                 = rand();
					valueFloat0          = (float)(bits & 0x7fff) * 3.0518509447574615e-05f - (float)0.5;
					position[value0 / 4] =
						(valueFloat0 + valueFloat0) * (float)50.0 +
						origin[value0 / 4];
					bits                 = rand();
					value1               = value0 + 4;
					valueFloat0          = (float)(bits & 0x7fff) * 3.0518509447574615e-05f - (float)0.5;
					velocity[value0 / 4] = (valueFloat0 + valueFloat0) * (float)0.35;
					value0               = value1;
				}
				while (value1 < 0xc);
				value1      = 0;
				velocity[0] = (trace.plane.normal[0] + velocity[0]) * 300.0f;
				velocity[1] = (trace.plane.normal[1] + velocity[1]) * 300.0f;
				velocity[2] = (trace.plane.normal[2] + velocity[2]) * 300.0f;
				value0      = rand();
				ctx->particleExplosion
				    ("explode1", position, velocity, 0x640, 0x28, value0 % 0x78 + 0x104, value1);
				particleIndex = particleIndex + -1;
			}
			while (particleIndex != 0);
			value0      = rand();
			value1      = value0 % 5 + 5;
			value0      = rand();
			value2      = value0 % 2000 + 1000;
			randomValue = rand();
			value0      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 200 + 400);
			ctx->debris(origin, normal, value0, value2, value1);
			soundVolume = 0x7f;
		}
		else
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 10000.0f;
			ctx->boxTrace(&trace, traceStart, origin, (float *)0x0, (float *)0x0, 0, 0x38);
			ctx->ripple(TCE_WallHandle(ctx->media.waterRipple), trace.endpos, normal, 300, 2000);
			valueFloat3 = 0.125;
			valueFloat2 = 128.0;
			valueFloat1 = 512.0;
			valueFloat0 = 0.5;
			value3      = 0xf;
			value2      = 900;
			value0      = TCE_WallHandle(ctx->media.water);
			randomValue = rand();
			value1      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 200 + 400);
			ctx->dirtParticles(trace.endpos, normal, value1, value2, value3, valueFloat0, valueFloat1, valueFloat2, valueFloat3, value0);
			valueFloat3 = 0.125;
			valueFloat2 = 512.0;
			valueFloat1 = 128.0;
			valueFloat0 = 0.5;
			value3      = 0xf;
			value2      = 0x578;
			value0      = TCE_WallHandle(ctx->media.water);
			randomValue = rand();
			value1      = (int)(((double)(randomValue & 32767) * 0.000030518509447574615) * 600 + 400);
			ctx->dirtParticles(trace.endpos, normal, value1, value2, value3, valueFloat0, valueFloat1, valueFloat2, valueFloat3, value0);
			soundVolume = 0x7f;
		}
		break;
	case 0x1e:
		position[0]  = *normal * 4.0f + *origin;
		impactSound  = TCE_WallHandle(ctx->media.explosive30);
		markShader   = TCE_WallHandle(ctx->media.burn);
		markDuration = 60000;
		radius       = 32.0;
		position[1]  = normal[1] * 4.0f + origin[1];
		position[2]  = normal[2] * 4.0f + origin[2];
		velocity[0]  = *normal * 100.0f;
		velocity[1]  = normal[1] * 100.0f;
		velocity[2]  = normal[2] * 100.0f;
		bits         = ctx->cmPointContents(origin, 0);
		if ((bits & 0x20) == 0)
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 20.0f;
			traceEnd[2]   = origin[2] - 20.0f;
			traceEnd[0]   = traceStart[0];
			traceEnd[1]   = traceStart[1];
			ctx->boxTrace(&trace, traceStart, traceEnd, (float *)0x0, (float *)0x0, 0,
			              0x6000081);
			trace.surfaceFlags = trace.surfaceFlags & 0xff000000;
			value0             = TCE_WallHandle(ctx->media.dirt);
			if (((trace.surfaceFlags == 0xa000000) || (trace.surfaceFlags == 0x1d000000)) ||
			    ((trace.surfaceFlags == 0x15000000 ||
			      (((value0 = TCE_WallHandle(ctx->media.snow), trace.surfaceFlags == 0x8000000 ||
			         (value0 = TCE_WallHandle(ctx->media.grass), trace.surfaceFlags == 0x1e000000)) ||
			        (value0 = TCE_WallHandle(ctx->media.mud), trace.surfaceFlags == 0x7000000))))))
			{
				ctx->dirtParticles(origin, normal, 0xfa, 900, 10, 0.5, 48.0, 24.0, 0.25, value0);
			}
		}
		else
		{
			traceStart[0] = *origin;
			traceStart[1] = origin[1];
			traceStart[2] = origin[2] + 10000.0f;
			ctx->boxTrace(&trace, traceStart, origin, (float *)0x0, (float *)0x0, 0, 0x38);
			ctx->ripple(TCE_WallHandle(ctx->media.waterRipple), trace.endpos, normal, 0x96, 1000);
			ctx->dirtParticles(trace.endpos, normal, 0xfa, 900, 0xf, 0.5, 64.0, 32.0, 0.125, TCE_WallHandle(ctx->media.water));
		}
		bits = rand();
		ctx->particle(origin, normal, 0, 0,
		              ((float)(bits & 0x7fff) * 3.0518509447574615e-05f + 1.0f) * 0.75f);
		randomValue = rand();
		value0      = (int)((((double)(randomValue & 32767) * 0.000030518509447574615) + 1) * 8);
		ctx->sparks(origin, normal, value0);
		valueFloat0 = 1.0;
		value0      = rand();
		ctx->debrisParticles(origin, normal, 0x3c, 500, value0 % 0xc + 0x18, valueFloat0);
		ctx->smokePuff(position, vec3_origin, 32.0, 1.0, 0.8, 0.1, 1.0, 250.0, (*ctx->time), 0, 1, TCE_WallHandle(ctx->media.explosionFlash));
		soundVolume = 0x7f;
	}
	if (impactSound != 0)
	{
		ctx->sound(origin, -1, 0, impactSound, soundVolume);
	}
	value0 = farSound;
	if (farSound != 0)
	{
		int echoAudible;
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		vec3_t delta;
#endif
		farOrigin[0] = *origin;
		farOrigin[1] = origin[1];
		farOrigin[2] = origin[2];
		viewY        = (*ctx->view)->vieworg[1];
		viewZ        = (*ctx->view)->vieworg[2];
		farDir[0]    = farOrigin[0] - (*ctx->view)->vieworg[0];
		farDir[1]    = farOrigin[1] - viewY;
		farDir[2]    = farOrigin[2] - viewZ;
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		VectorCopy(farDir, delta);
#endif
#if defined(_WIN32)
		distance = TCE_VectorNormalizeValue(farDir);
#else
		distance = (double)VectorNormalize(farDir);
#endif
		echoAudible = distance > 1200.0L && distance < 8000.0L;
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
		/* Preserve the held root comparison even when MSVC long double is
		 * binary64. Normalized components still come from the shared provider. */
		(void)TCE_WallEchoGate(delta, &echoAudible);
#endif
		if (echoAudible)
		{
			const float *listener = (*ctx->view)->vieworg;
			int axis;
			for (axis = 0; axis < 3; ++axis)
			{
				farOrigin[axis] = TCE_WallEchoCoordinate(farDir[axis], farRange, listener[axis]);
			}
			ctx->soundEx(farOrigin, -1, 2, value0, 0x10, soundVolume);
		}
	}
LAB_3007ad98:
	if ((markShader == TCE_WallHandle(ctx->media.burn)) && ((*ctx->forceMarks) == 0))
	{
		projection[3] = radius;
		projection[0] = 0.0;
		projection[1] = 0.0;
		projection[2] = -1.0;
		color[0]      = 1.0;
		color[1]      = 1.0;
		color[2]      = 1.0;
		color[3]      = 1.0;
		ctx->projectDecal(markShader, 1, origin, projection, color, markDuration, TCE_WallMarkFade(markDuration));
		return;
	}
	if (markShader != 0)
	{
		if (visible != 0)
		{
			value2      = 0;
			value1      = 1;
			markAlpha   = 1.0;
			valueFloat3 = 1.0;
			valueFloat2 = 1.0;
			valueFloat1 = 1.0;
			valueFloat0 = radius;
			value0      = markDuration;
			bits        = rand();
			ctx->eliteMark(markShader, origin, normal,
			               TCE_WallMarkRotation(bits & 0x7fff),
			               valueFloat1, valueFloat2, valueFloat3, markAlpha, value1, valueFloat0, value2, value0);
			return;
		}
		valueFloat3      = 1.0;
		valueFloat2      = 1.0;
		valueFloat1      = 1.0;
		valueFloat0      = 1.0;
		TCE_WallProjection(origin, normal, radius, projection, projectOrigin);
		value0           = markDuration;
		bits             = rand();
		ctx->impactMark(markShader, projectOrigin, projection, radius,
		                TCE_WallMarkRotation(bits & 0x7fff), valueFloat0,
		                valueFloat1, valueFloat2, valueFloat3, value0);
	}
switchD_3007838b_caseD_5:
	return;
}
