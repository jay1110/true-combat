#include "cg_local.h"
#include "tce_impact_particles.h"
#include "tce_lightgrid.h"
/* Windows 3002a870. Original mutates the caller's direction by normalization. */


/* Float output boundaries for impact arithmetic; native FPU parity remains open. */
static float impactMultiply(float first, float second)
{
	float result;
#if defined(_MSC_VER) && defined(_M_IX86)
	result = first * second;
#else
	result = first * second;
#endif
	return result;
}
static float impactRandomOutput(int mode)
{
	int          sample = rand() & 32767;
	const float  randomUnit = 1.0f / 32767.0f, randomHalf = .5f, angleCircle = 360.0f;
	const double centeredHalf = .5, chipSpin = 720.0, smokeSpin = 90.0;
	float        result;
#if defined(_MSC_VER) && defined(_M_IX86)
	long double value = (long double)sample * (long double)randomUnit;
	if (mode == 1)
	{
		value += (long double)randomHalf;
	}
	else if (mode == 2)
	{
		value *= (long double)angleCircle;
	}
	else if (mode != 0)
	{
		value -= (long double)centeredHalf;
		value += value;
		value *= mode == 3 ? (long double)chipSpin : (long double)smokeSpin;
	}
	result = (float)value;
#else
	{ double r = sample * (double)randomUnit;
	  result = (float)(mode == 0?r:mode == 1?r + .5:mode == 2?r * 360:(r - .5) * 2 * (mode == 3?720:90)); }
#endif
	return result;
}
static int impactChipLifetime(void)
{
	int         sample = rand() & 32767, result;
	const float randomUnit = 1.0f / 32767.0f, lifeOne = 1.0f, lifeBase = 500.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
	result = (int)(((long double)sample * (long double)randomUnit +
	                (long double)lifeOne) * (long double)lifeBase);
#else
	result = (int)((sample * (double)randomUnit + 1) * 500);
#endif
	return result;
}
static float impactSmokeSpeed(float randomScale, float speedScale)
{
	const float geomRange = 16.0f, geomBase = 26.0f;
	float       result;
#if defined(_MSC_VER) && defined(_M_IX86)
	result = (float)(((long double)randomScale * (long double)geomRange +
	                  (long double)geomBase) * (long double)speedScale);
#else
	result = (float)((randomScale * 16.0 + 26.0) * speedScale);
#endif
	return result;
}
static void impactSmokeGeometry(int index, float spread,
                                const vec3_t origin, const vec3_t direction,
                                vec3_t position, float *factor)
{
	int         factorIndex = index + 2;
	const float geomOne = 1.0f, geomThird = 1.0f / 3.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
	float       indexFloat = (float)((long double)index + (long double)geomOne);
	long double distance   = (long double)spread * (long double)indexFloat;
	int         j;
	for (j = 0; j < 3; ++j)
	{
		position[j] = (float)(distance * (long double)direction[j] + (long double)origin[j]);
	}
	*factor = (float)((long double)factorIndex * (long double)geomThird + (long double)geomOne);
#else
	{ int j;
	  for (j = 0; j < 3; ++j)
		  position[j] = (float)(spread * (index + 1.0) * direction[j] + origin[j]);
	  *factor = (float)((index + 2) * (double)geomThird + 1); }
#endif
}
static float impactSmokeLifetime(float factor, float life, float lifeScale)
{
	int         sample = rand() & 32767;
	const float randomUnit = 1.0f / 32767.0f, lifeJitter = 1.2f;
	float       result;
#if defined(_MSC_VER) && defined(_M_IX86)
	long double value = (long double)sample * (long double)randomUnit;
	value *= (long double)life;
	value *= (long double)lifeJitter;
	value += (long double)factor * (long double)life;
	result = (float)(value * (long double)lifeScale);
#else
	result = (float)((factor * (double)life + sample * (double)randomUnit * life * lifeJitter) * lifeScale);
#endif
	return result;
}
static void impactSmokeTrajectory(int index, float life, float gravity, int *duration, float *scaledGravity)
{
	const float timeOne = 1.0f, timeHalf = .5f;
	float       indexFloat = (float)(index + 1);
#if defined(_MSC_VER) && defined(_M_IX86)
	long double factor = (long double)indexFloat * (long double)timeHalf +
	                     (long double)timeOne;
	/* Only the bounded material lifetimes and indices from the producer enter here. */
	*duration      = (int)(factor * (long double)life);
	*scaledGravity = (float)(factor * (long double)gravity);
#else
	*duration      = (int)((indexFloat * .5 + 1) * life);
	*scaledGravity = (float)((indexFloat * .5 + 1) * gravity);
#endif
}
/* Randomize each component before normalizing the complete direction vector. */
static float impactDirectionComponent(float base, qboolean dust)
{
	int          sample     = rand() & 32767;
	const float  randomUnit = 1.0f / 32767.0f;
	const double randomHalf = 0.5, dustSpread = 0.25;
	float        component;
#if defined(_MSC_VER) && defined(_M_IX86)
	long double value = (long double)sample * (long double)randomUnit;
	value -= (long double)randomHalf;
	value += value;
	if (dust)
	{
		value *= (long double)dustSpread;
	}
	component = (float)(value + (long double)base);
#else
	component = (float)(((sample * (double)randomUnit - randomHalf) * 2.0) *
	                    (dust ? dustSpread : 1.0) + base);
#endif
	return component;
}
static float impactChipSpeed(float component, float scale)
{
	int         sample     = rand() & 32767;
	const float randomUnit = 1.0f / 32767.0f;
	const float speedRange = 24.0f, speedBase = 12.0f, speedFraction = 2.0f / 3.0f;
	float       result;
#if defined(_MSC_VER) && defined(_M_IX86)
	long double value = (long double)sample * (long double)randomUnit;
	value *= (long double)speedRange;
	value += (long double)speedBase;
	value *= (long double)scale;
	value *= (long double)speedFraction;
	result = (float)(value * (long double)component);
#else
	result = (float)((sample * (double)randomUnit * speedRange + speedBase) *
	                 scale * speedFraction * component);
#endif
	return result;
}
void TCE_CG_ParticleTest(vec3_t origin, vec3_t direction, int count, int material, float alpha, const tce_impactParticleMedia_t *media)
{
	vec3_t light, originalDirection, velocity, position; vec4_t color = { 1, 1, 1, 0 }, chipColor = { 1, 1, 1, 1 };
	float  randomScale, life = 375, lifeScale = 1, gravity = 1, speedScale = 1, spread = 8;
	int    puffCount = 2, trajectory = 11, single = 0, i, j; qhandle_t shader = media->smoke3; localEntity_t *le;
	TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(), origin, light);
	randomScale = impactRandomOutput(0); VectorCopy(direction, originalDirection); color[3] = impactMultiply(alpha, .3f);
	switch (material)
	{
	case 2: case 29: VectorSet(color, .6f, .6f, .6f); goto dust;
	case 5: case 23: case 28: case 30: VectorSet(color, .75f, .6f, .45f); goto dust;
	case 22: VectorSet(color, .75f, .48f, .42f); goto dust;
	case 26: VectorSet(color, .6f, .6f, .6f); goto dust;
	case 27: VectorSet(color, .99f, .99f, .99f); goto dust;
	case 10: VectorSet(color, .9f, .75f, .45f); puffCount = 3; spread = 12; speedScale = 2; lifeScale = 7; goto dustCommon;
	case 16:
		VectorSet(color, .5f, 0, 0); color[3] = impactMultiply(alpha, .6f); shader = media->smoke4;
		life                                  = 200; lifeScale = 3; gravity = .3f; trajectory = 15;
		if (count >= 3)
		{
			puffCount = 3; spread = 12; lifeScale = 4; speedScale = 2; gravity = .4f;
		}
		else if (count > 0)
		{
			puffCount = 3; spread = 12; lifeScale = 3.5f; speedScale = 1.5f; gravity = .35f;
		}
		count        = 0; break;
dust:   lifeScale    = 3;
dustCommon: color[3] = impactMultiply(alpha, .6f); count = 0; life = 200; trajectory = 15; gravity = .5f; shader = media->smoke4; break;
	case 37: count   = 0; break;
	case 39: spread  = 24; life = 500; count = 0; break;
	case 40: spread  = 16; life = 450; count = 0; break;
	case 41: spread  = 14; life = 500; count = 0; puffCount = 1; single = 1; break;
	}
	for (j = 0; j < 3; ++j)
	{
		color[j] = impactMultiply(color[j], light[j]); chipColor[j] = impactMultiply(chipColor[j], light[j]);
	}
	VectorNormalize(direction);
	for (i = 0; i < count; ++i)
	{
		float radius; int duration; float scale;
		scale = randomScale + 1.0f;
		for (j = 0; j < 3; ++j)
			velocity[j] = impactDirectionComponent(direction[j], qfalse);
		VectorNormalize(velocity);
		for (j = 0; j < 3; ++j)
			velocity[j] = impactChipSpeed(velocity[j], scale);
		duration   = impactChipLifetime();
		radius     = impactRandomOutput(1);
		le         = CG_SmokePuff(origin, velocity, radius, chipColor[0], chipColor[1], chipColor[2], chipColor[3], duration, cg.time, 0, 0, media->blood);
		le->leType = (leType_t)16; le->tceGravity = impactMultiply(radius, .26666668f); le->fadeInTime = (int)((unsigned)cg.time - 250u + (unsigned)duration);
		if (le->leFlags == 2)
		{
			le->angles.trBase[0] = impactRandomOutput(2); le->angles.trDelta[0] = impactRandomOutput(3);
		}
	}
	if (puffCount)
	{
		float speed = impactSmokeSpeed(randomScale, speedScale);
		for (i = 0; i < puffCount; ++i)
		{
			float factor, size, duration, g; int trDuration;
			for (j = 0; j < 3; ++j)
				velocity[j] = impactDirectionComponent(originalDirection[j], qtrue);
			VectorNormalize(velocity);
			for (j = 0; j < 3; ++j)
				velocity[j] = impactMultiply(speed, velocity[j]);
			impactSmokeGeometry(i, spread, origin, direction, position, &factor);
			duration              = impactSmokeLifetime(factor, life, lifeScale);
			size                  = impactMultiply(factor, spread);
			le                    = CG_SmokePuff(position, velocity, size, color[0], color[1], color[2], color[3], duration, cg.time, 0, 0x42, shader);
			le->angles.trBase[0]  = impactRandomOutput(2);
			le->angles.trDelta[0] = impactRandomOutput(4);
			impactSmokeTrajectory(i, life, gravity, &trDuration, &g);
			le->pos.trType = (trType_t)trajectory; le->pos.trTime = cg.time; le->pos.trDuration = trDuration; le->leType = LE_MOVE_SCALE_FADE; le->tceGravity = g;
			if (single)
			{
				return;
			}
			le             = CG_SmokePuff(position, velocity, impactMultiply(size, 2.0f / 3.0f), color[0], color[1], color[2], color[3], impactMultiply(duration, .5f), cg.time, 0, 0x40, shader);
			le->pos.trType = (trType_t)trajectory; le->pos.trTime = cg.time; le->pos.trDuration = trDuration; le->leType = LE_MOVE_SCALE_FADE; le->tceGravity = g;
		}
	}
}
