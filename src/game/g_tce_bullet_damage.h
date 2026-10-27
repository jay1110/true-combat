/* Private immutable H3 bullet selection; include after g_local.h.
 * No engine or public damage ABI. */
#ifndef G_TCE_BULLET_DAMAGE_H
#define G_TCE_BULLET_DAMAGE_H
#include "g_tce_pose.h"
#include "g_tce_pose_presentation.h"

/* Values belong to one selected ray segment. They are never a pointer into
 * mutable history, and damage cannot evaluate or select a second pose. */
typedef struct
{
	g_tcePresentationQuery_t query;
	g_tcePoseTraceResult_t trace;
	vec3_t start, end;
	double maxFraction;
} g_tceBulletDamageSelection_t;

tce_damageResult_t G_TCEPresentationDamage(struct gentity_s *target,
    struct gentity_s *inflictor, struct gentity_s *attacker,
    vec3_t direction, vec3_t point, int damage, int flags, int mod,
    const g_tceBulletDamageSelection_t *selection);
#endif
