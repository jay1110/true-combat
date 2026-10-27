#ifndef G_TCE_HIT_DEBUG_H
#define G_TCE_HIT_DEBUG_H
#include "tce_hit_debug.h"
struct gentity_s;
extern vmCvar_t g_debugBulletsClient;
void G_TCEHitDebugFrame(void);
void G_TCEHitDebugShot(struct gentity_s *shooter, int layer);
int G_TCEHitDebugLogAllowed(void);
int G_TCEHitDebugTarget(int clientNum);
void G_TCEHitDebugTrace(const char *stage, int shooter, int target, int region,
    const trace_t *trace, const vec3_t start, const vec3_t end);
#endif
