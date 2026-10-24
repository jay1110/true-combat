#include "../game/tce_numeric.h"

static void SmokeLight(const tce_lightGrid_t *grid, const vec3_t point,
                       const vec3_t cached, vec3_t color, vec3_t sum, int *count)
{
	int k;
	TCE_CG_LightForParticleSimple(grid, point, color);
	if (!(trap_CM_PointContents(point, 0) & CONTENTS_SOLID))
	{
		++*count;
		for (k = 0; k < 3; ++k)
		{
			sum[k] += color[k];
			color[k] = (float)(((long double)color[k] + cached[k]) * .5f);
		}
	}
	else
	{
		VectorCopy(cached, color);
	}
}

static void SmokeRGBA(refEntity_t *ent, const vec3_t color, float alpha)
{
	int k;
	for (k = 0; k < 3; ++k)
	{
		ent->shaderRGBA[k] = (byte)(int)((long double)color[k] * 204.f);
	}
	ent->shaderRGBA[3] = (byte)(int)alpha;
}

static float SmokeAge(int elapsed, int delay)
{
	float milliseconds = (float)TCE_SubInt32(elapsed, delay);
	return (float)((1.0L / 5000.f) * milliseconds);
}

static float SmokeAlpha(float age)
{
	float alpha = (float)((7.0L - age) * .5f);
	if (alpha > 1.f)
	{
		alpha = 1.f;
	}
	return (float)((long double)alpha * 255.f);
}

static float SmokeRotation(int *seed, float growth, float speed, qboolean roundSpin)
{
	float angle = (float)((long double)Q_crandom(seed) * 180.f);
	long double spin = (long double)Q_crandom(seed) * growth;
	/* Only the second side sprite reloads this product from a float store. */
	if (roundSpin)
	{
		spin = (float)spin;
	}
	return (float)(spin * speed + angle);
}

/* Linux 000667b6: retain the ELF random order and stored intermediate values. */
void TCE_CG_SmokeGrenadeExtended(const tce_lightGrid_t *grid, int now, int start,
                                 qboolean large, const vec3_t origin, const vec3_t forward,
                                 vec3_t cachedColor, const qhandle_t shaders[3])
{
	static const vec3_t lobes[4] = {
		{ 0, 0, .5f }, { -.866f, .5f, .2f },
		{ .866f, .5f, .2f }, { 0, -1, .2f }
	};
	static const vec3_t outer[10] = {
		{ 1, 0, .2f }, { .5f, .866f, .2f }, { -.5f, .866f, .2f },
		{ -1, 0, .2f }, { -.5f, -.866f, .2f }, { .5f, -.866f, .2f },
		{ .866f, .5f, 1.4f }, { -.866f, .5f, 1.4f },
		{ 0, -1, 1.4f }, { 0, 0, 1 }
	};
	float scale = large ? 137.5f : 110.f;
	int elapsed = TCE_SubInt32(now, start);
	int i, j, k, side, seed, delay, count = 0;
	vec3_t direction, center, color, sum = { 0, 0, 0 };
	float age, growth, alpha, distance;
	refEntity_t ent;

	for (i = 0; i < 4; ++i)
	{
		long double spread;
		float storedSpread;
		VectorCopy(lobes[i], direction);
		if (i)
		{
			VectorNormalize(direction);
		}
		growth = (float)sqrtl((1.0L / 5000.f) * (float)elapsed);
		spread = (long double)scale * growth * .5f;
		storedSpread = (float)spread;
		center[0] = (float)(spread * direction[0] + origin[0]);
		center[1] = (float)((long double)storedSpread * direction[1] + origin[1]);
		center[2] = (float)((long double)storedSpread * direction[2] + origin[2]);
		SmokeLight(grid, center, cachedColor, color, sum, &count);

		for (j = 0; j < 2; ++j)
		{
			seed = TCE_Int32FromUnsigned((unsigned)start + i * 327u + j * 754u);
			delay = (int)((long double)Q_random(&seed) * 500.f);
			if (elapsed <= delay || elapsed >= delay + 35000)
			{
				continue;
			}
			age = SmokeAge(elapsed, delay);
			alpha = SmokeAlpha(age);
			growth = (float)sqrtl(age);
			if (!j)
			{
				memset(&ent, 0, sizeof(ent));
				ent.reType = RT_SPRITE;
				ent.customShader = shaders[0];
				ent.radius = (float)((long double)scale * growth * .5f);
				ent.rotation = SmokeRotation(&seed, growth, 150.f, qfalse);
				VectorCopy(center, ent.origin);
				SmokeRGBA(&ent, color, alpha);
				trap_R_AddRefEntityToScene(&ent);
			}
			else
			{
				float fraction = j * .5f - i * .125f;
				double cap = (double)(1.0L - (long double)fraction * fraction);
				double size = (double)((long double)scale * growth);
				distance = (float)((long double)scale * fraction * growth * .85f);
				for (side = 0; side < 2; ++side)
				{
					memset(&ent, 0, sizeof(ent));
					ent.reType = RT_SPRITE;
					ent.customShader = shaders[0];
					ent.radius = (float)(sqrtl(cap) * size * .5);
					ent.rotation = SmokeRotation(&seed, growth, 150.f, side != 0);
					for (k = 0; k < 3; ++k)
					{
						ent.origin[k] = (float)((long double)(side ? -distance : distance)
						                       * forward[k] + center[k]);
					}
					SmokeRGBA(&ent, color, alpha);
					trap_R_AddRefEntityToScene(&ent);
				}
			}
		}
	}

	for (i = 0; i < 10; ++i)
	{
		long double size;
		float storedSize;
		VectorCopy(outer[i], direction);
		VectorNormalize(direction);
		seed = TCE_Int32FromUnsigned((unsigned)start + i * 127u);
		delay = (int)((long double)Q_random(&seed) * 500.f);
		if (elapsed <= delay || elapsed >= delay + 35000)
		{
			continue;
		}
		age = SmokeAge(elapsed, delay);
		alpha = SmokeAlpha(age);
		growth = (float)sqrtl(age);
		size = (long double)scale * growth;
		storedSize = (float)size;
		distance = (float)(size * .8f);
		memset(&ent, 0, sizeof(ent));
		ent.reType = RT_SPRITE;
		ent.customShader = shaders[i < 6 ? 0 : 1];
		ent.radius = (float)((long double)storedSize * .4f);
		if (i < 6)
		{
			ent.rotation = SmokeRotation(&seed, growth, 300.f, qfalse);
		}
		else
		{
			if (Q_random(&seed) > .5f)
			{
				ent.customShader = shaders[2];
			}
			distance = (float)((long double)distance * .7f);
		}
		ent.radius = (float)(((long double)Q_random(&seed) + 1.f)
		                     * ent.radius * (2.f / 3.f));
		for (k = 0; k < 3; ++k)
		{
			ent.origin[k] = (float)((long double)distance * direction[k] + origin[k]);
		}
		SmokeLight(grid, ent.origin, cachedColor, color, sum, &count);
		SmokeRGBA(&ent, color, alpha);
		trap_R_AddRefEntityToScene(&ent);
	}
	if (count)
	{
		long double weight = (1.0L / count) * .1f;
		for (k = 0; k < 3; ++k)
		{
			cachedColor[k] = (float)((long double)sum[k] * weight
			                         + (long double)cachedColor[k] * .9f);
		}
	}
}
