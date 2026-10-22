#include "tce_shotgun_prediction.h"
#include "../game/tce_numeric.h"

static void SnapTowards(vec3_t point, const vec3_t toward)
{
	int i;
	for (i = 0; i < 3; ++i)
	{
#ifdef _WIN32
		point[i] = (float)(point[i] < toward[i]?ceil(point[i]):floor(point[i]));
#else
		point[i] = (float)(toward[i] <= point[i]?floor(point[i]):ceil(point[i]));
#endif
	}
}

/* Windows30075ba0. Client mask, exit tolerance, stored vertical deflection
 * and recursive energy differ from the server: do not share its controller. */
void TCE_CG_BulletFireExtended(const tce_shotgunPrediction_t *ctx, int source, int weapon,
                               vec3_t start, vec3_t end, int energy, int walls, int seed)
{
	trace_t                    tr;
	vec3_t                     entry, normal, direction, from, right, up;
	const tce_pierceMaterial_t *material;
	int                        entryMaterial, exitMaterial, i, randomSeed;
	float                      radius, horizontal, vertical, retained, storedEnergy = (float)energy;
#ifdef _WIN32
	double                     fraction, angle, thickness;
#else
	long double                fraction, angle, thickness;
#endif
	ctx->trace(&tr, start, NULL, NULL, end, source, 0x81);
#ifdef _WIN32
	if (!(tr.fraction <= 1.0f) || tr.entityNum < ctx->maxClients || tr.entityNum == ENTITYNUM_NONE)
#else
	if (tr.fraction > 1.0f || tr.entityNum < ctx->maxClients || tr.entityNum == ENTITYNUM_NONE)
#endif
	{
		return;
	}
	entryMaterial = BG_SurfaceFlag2Type(tr.surfaceFlags);
	ctx->bullet(tr.endpos, source, tr.plane.normal, 0, ENTITYNUM_WORLD, ENTITYNUM_WORLD,
	            0, 0, entryMaterial, start, 1);
	material = &tcePierceTable[entryMaterial];
	if (walls <= 0 || (tr.surfaceFlags & 0x80000) || material->resistance >= 9999)
	{
		return;
	}
	if (material->resistance > 999 && Q_stricmp(ctx->weapons[weapon].caliberClass, "338LAPUA") &&
	    Q_stricmp(ctx->weapons[weapon].caliberClass, "50BMG"))
	{
		return;
	}
	VectorSubtract(end, start, direction);
	VectorNormalize(direction);
	VectorCopy(tr.endpos, entry);
	VectorCopy(tr.plane.normal, normal);
	for (i = 0; i < 3; ++i)
	{
#ifdef _WIN32
		from[i] = (entry[i] - normal[i]) + direction[i];
#else
		from[i] = (float)(((long double)entry[i] - normal[i]) + direction[i]);
#endif
	}
	ctx->trace(&tr, from, NULL, NULL, end, source, 0x81);
	if (!(tr.fraction < 1))
	{
		return;
	}
	if (Distance(tr.endpos, entry) < 4)
	{
		for (i = 0; i < 3; ++i)
		{
#ifdef _WIN32
			from[i] = (float)(((double)entry[i] - normal[i]) + direction[i] * 4.0);
#else
			from[i] = (float)(((long double)entry[i] - normal[i]) + (long double)direction[i] * 4.0L);
#endif
		}
		ctx->trace(&tr, from, NULL, NULL, end, source, 0x81);
#ifdef _WIN32
		if (tr.fraction >= 1)
#else
		if (!(tr.fraction < 1))
#endif
		{
			return;
		}
	}
	SnapTowards(tr.endpos, from);
	VectorCopy(tr.endpos, from);
	ctx->trace(&tr, from, NULL, NULL, start, source, 0x81);
	SnapTowards(tr.endpos, from);
	if (!(Distance(entry, start) < (float)(Distance(tr.endpos, start) + 2.0)))
	{
		return;
	}
	exitMaterial = BG_SurfaceFlag2Type(tr.surfaceFlags);
	if (exitMaterial != entryMaterial && tcePierceTable[exitMaterial].resistance >= material->resistance)
	{
		material = &tcePierceTable[exitMaterial];
	}
	thickness = Distance(tr.endpos, entry);
	if (thickness < 0)
	{
		thickness = 0;
	}
#ifdef _WIN32
	fraction = (storedEnergy - (thickness / material->thicknessScale + 1) * material->resistance) / storedEnergy;
#else
	/* The native Linux energy gates use the held fraction, not its float copy. */
	fraction = (storedEnergy - ((1.0L / (float)material->thicknessScale) * thickness + 1.0L) *
	            (float)material->resistance) * (1.0L / storedEnergy);
#endif
	retained = (float)fraction;
#ifdef _WIN32
	if (!(fraction > 0) || (double)retained * storedEnergy < 5)
#else
	if (!(fraction > 0) || !(fraction * storedEnergy >= 5.0L))
#endif
	{
		return;
	}
	ctx->wall(3, 1, tr.endpos, tr.plane.normal, tr.plane.normal, exitMaterial, tr.entityNum == ENTITYNUM_WORLD, 1);
	randomSeed = ((unsigned)seed + (unsigned)walls * 11u) & 65535u;
#ifdef _WIN32
	radius     = (float)sqrt(Q_random(&randomSeed));
	angle      = Q_crandom(&randomSeed) * 3.141;
	horizontal = (float)(cos(angle) * radius * (1.0 - retained) * 2539.9999618530273);
	vertical   = (float)(sin(angle) * radius * (1.0 - retained) * 2539.9999618530273);
#else
	radius = (float)sqrtl((long double)Q_random(&randomSeed));
	angle = (float)((long double)Q_crandom(&randomSeed) * (double)3.141);
	{
		long double deflection = 1.0L - retained;
		long double x = cosl(angle) * radius * deflection * (double)1269.9999809265137;
		long double y = (long double)radius * sinl(angle) * deflection * (double)1269.9999809265137;
		horizontal = (float)(x + x);
		vertical = (float)(y + y);
	}
#endif
	VectorSubtract(end, start, direction);
	VectorNormalize(direction);
	PerpendicularVector(right, direction);
	CrossProduct(direction, right, up);
	for (i = 0; i < 3; ++i)
	{
#ifdef _WIN32
		end[i] = (float)((double)right[i] * horizontal + (double)up[i] * vertical + end[i]);
#else
		end[i] = (float)(((long double)right[i] * horizontal + end[i]) + (long double)up[i] * vertical);
#endif
	}
	/* Original reloads the stored INPUT energy, not the retained product. */
#ifdef _WIN32
	TCE_CG_BulletFireExtended(ctx, source, weapon, tr.endpos, end, (int)storedEnergy, walls - 1, seed);
#else
	TCE_CG_BulletFireExtended(ctx, source, weapon, tr.endpos, end,
	                         TCE_TruncateNativeInteger(storedEnergy), TCE_SubInt32(walls, 1), seed);
#endif
}

/* Windows3007b1c0: nine visual pellets,200 spread,weapon48 energy. Server
 * damage uses115 spread and its selected weapon; that discrepancy is original. */
void TCE_CG_ShotgunPattern(const tce_shotgunPrediction_t *ctx, vec3_t start, vec3_t direction,
                           int seed, int source, int force)
{
	vec3_t forward, right, up, end;
	int    i;
	if (source == ctx->localClient && !force && ctx->predictBullets > 0)
	{
		return;
	}
	VectorNormalize2(direction, forward);
	PerpendicularVector(right, forward);
	CrossProduct(forward, right, up);
	for (i = 0; i < 9; ++i)
	{
#ifndef _WIN32
		/* Linux c91ec spills radius, angle and both offsets to float, then
		 * holds each endpoint sum until the final vector store. */
		float radius = (float)sqrtl((long double)Q_random(&seed));
		float angle = (float)((long double)Q_crandom(&seed) * (double)3.141);
		float horizontal = (float)(cosl((long double)angle) * radius * 200.0L);
		float vertical = (float)((long double)radius * sinl((long double)angle) * 200.0L);
		int axis;
		for (axis = 0; axis < 3; ++axis)
		{
			long double base = (long double)forward[axis] * 8192.0L + start[axis];
			base += (long double)right[axis] * horizontal;
			end[axis] = (float)(base + (long double)up[axis] * vertical);
		}
#else
		float  radius     = (float)sqrt(Q_random(&seed));
		double angle      = Q_crandom(&seed) * 3.141;
		float  horizontal = (float)(cos(angle) * radius * 200.0);
		double vertical   = sin(angle) * radius * 200.0;
		end[0] = (float)((double)up[0] * vertical + (double)right[0] * horizontal + (double)forward[0] * 8192.0 + start[0]);
		end[1] = (float)((double)up[1] * vertical + (float)((double)right[1] * horizontal + (double)forward[1] * 8192.0 + start[1]));
		end[2] = (float)((double)up[2] * vertical + (float)((double)right[2] * horizontal + (double)forward[2] * 8192.0 + start[2]));
#endif
		TCE_CG_BulletFireExtended(ctx, source, 48, start, end, ctx->weapons[48].unknown_134, 1, seed);
	}
}

void TCE_CG_ShotgunFire(const tce_shotgunPrediction_t *ctx, entityState_t *event)
{
	vec3_t direction, unused;
	VectorSubtract(event->origin2, event->pos.trBase, direction);
	VectorNormalize(direction);
	VectorMA(event->pos.trBase, 32, direction, unused);
	TCE_CG_ShotgunPattern(ctx, event->pos.trBase, event->origin2, event->eventParm, event->otherEntityNum, 0);
}
