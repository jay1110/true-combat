#include "g_local.h"
#include "tce_numeric.h"

/* Windows TC:E records an 80-byte pose, not the SDK's bounds/origin only. */
static void G_CaptureClientMarker(gentity_t *ent, clientMarker_t *marker, int time)
{
	VectorCopy(ent->r.mins, marker->mins);
	VectorCopy(ent->r.maxs, marker->maxs);
	VectorCopy(ent->r.currentOrigin, marker->origin);
	VectorCopy(ent->r.currentAngles, marker->angles);
	VectorCopy(ent->client->ps.viewangles, marker->viewangles);
	marker->time        = time;
	marker->serverTime  = level.time;
	marker->pm_flags    = ent->client->ps.pm_flags;
	marker->weaponFlags = ent->client->ps.stats[STAT_TCE_WEAPON_FLAGS];
	marker->eFlags      = ent->client->ps.eFlags;
}

void G_StoreClientPosition(gentity_t *ent)
{
	int            time    = level.time;
	clientMarker_t *marker = &ent->client->clientMarkers[ent->client->topMarker];
	/* Repeated commands in this frame replace the current pose. Only a new
	 * server frame advances the ring and seals the preceding interval. */
	if (marker->serverTime < level.time)
	{
		marker->time = level.previousTime;
		if (++ent->client->topMarker >= MAX_CLIENT_MARKERS)
		{
			ent->client->topMarker = 0;
		}
		marker = &ent->client->clientMarkers[ent->client->topMarker];
	}
	if (!(ent->r.svFlags & SVF_BOT))
	{
		time = TCE_SubInt32(TCE_AddInt32(trap_Milliseconds(), level.previousTime), level.frameTime);
		if (time > level.time)
		{
			time = level.time;
		}
		else if (time <= level.previousTime)
		{
			time = TCE_AddInt32(level.previousTime, 1);
		}
	}
	G_CaptureClientMarker(ent, marker, time);
}

#if !defined(__linux__)
/* Windows2004eba0 deliberately weights the newer sample by frac. */
static void G_TCELerpPose(float frac, const vec3_t newer, const vec3_t older, vec3_t poseOutput)
{
#if defined(_MSC_VER) && defined(_M_IX86)
	long double inverse = 1.0L - (long double)frac;

	poseOutput[0] = (float)((long double)frac * (long double)newer[0] +
	                        inverse * (long double)older[0]);
	poseOutput[1] = (float)(inverse * (long double)older[1] +
	                        (long double)frac * (long double)newer[1]);
	poseOutput[2] = (float)(inverse * (long double)older[2] +
	                        (long double)frac * (long double)newer[2]);
#else
	float inverse = 1.0f - frac;
	poseOutput[0] = inverse * older[0] + frac * newer[0];
	poseOutput[1] = frac * newer[1] + inverse * older[1];
	poseOutput[2] = frac * newer[2] + inverse * older[2];
#endif
}
#endif

static void G_ApplyClientMarker(gentity_t *ent, const clientMarker_t *marker)
{
	VectorCopy(marker->mins, ent->r.mins);
	VectorCopy(marker->maxs, ent->r.maxs);
	VectorCopy(marker->origin, ent->r.currentOrigin);
	VectorCopy(marker->angles, ent->r.currentAngles);
	VectorCopy(marker->viewangles, ent->client->ps.viewangles);
	ent->client->ps.pm_flags                     = marker->pm_flags;
	ent->client->ps.stats[STAT_TCE_WEAPON_FLAGS] = marker->weaponFlags;
	ent->client->ps.eFlags                       = marker->eFlags;
}

#if defined(__linux__)
/* The Linux original keeps the reciprocal and weights on the x87 stack. */
static void G_TCELerpLinuxPose(long double fraction, const vec3_t newer,
                              const vec3_t older, vec3_t output)
{
	long double inverse = 1.0L - fraction;
	int axis;
	for (axis = 0; axis < 3; ++axis)
	{
		output[axis] = (float)(fraction * (long double)newer[axis] +
		                       inverse * (long double)older[axis]);
	}
}
#endif

/* Windows2004e790: pose rewind, including discrete bounds and pose flags. */
void G_TimeShiftClient(gentity_t *ent, int time)
{
	gclient_t      *client = ent->client;
	clientMarker_t *newer, *older;
	int            i, j;
	if (time > level.time)
	{
		time = level.time;
	}
	i = j = client->topMarker;
	do
	{
		if (client->clientMarkers[i].time <= time)
		{
			break;
		}
		j = i;
		if (--i < 0)
		{
			i = MAX_CLIENT_MARKERS - 1;
		}
	}
	while (i != client->topMarker);
	if (i == j)
	{
		return;
	}
	if (client->backupMarker.serverTime != level.time)
	{
		/* Backup uses offset0x40/serverTime as its restore guard; the
		 * ordinary sample-time field at0x3c is left untouched. */
		G_CaptureClientMarker(ent, &client->backupMarker, client->backupMarker.time);
	}
	newer = &client->clientMarkers[j];
	if (i != client->topMarker)
	{
#if defined(__linux__)
		long double fraction;
		older = &client->clientMarkers[i];
		fraction = (1.0L / (long double)TCE_SubInt32(newer->time, older->time)) *
		           (long double)TCE_SubInt32(newer->time, time);
		G_TCELerpLinuxPose(fraction, newer->origin, older->origin, ent->r.currentOrigin);
		G_TCELerpLinuxPose(fraction, newer->angles, older->angles, ent->r.currentAngles);
		G_TCELerpLinuxPose(fraction, newer->viewangles, older->viewangles, client->ps.viewangles);
#else
		float frac;
		older = &client->clientMarkers[i];
		frac  = (float)((long double)TCE_SubInt32(newer->time, time) /
		                (long double)TCE_SubInt32(newer->time, older->time));
		G_TCELerpPose(frac, newer->origin, older->origin, ent->r.currentOrigin);
		G_TCELerpPose(frac, newer->angles, older->angles, ent->r.currentAngles);
		G_TCELerpPose(frac, newer->viewangles, older->viewangles, client->ps.viewangles);
#endif
		VectorCopy(newer->mins, ent->r.mins);
		VectorCopy(newer->maxs, ent->r.maxs);
		client->ps.pm_flags = newer->pm_flags;
		client->ps.stats[STAT_TCE_WEAPON_FLAGS] = newer->weaponFlags;
		client->ps.eFlags = newer->eFlags;
	}
	else
	{
		G_ApplyClientMarker(ent, newer);
	}
}

/* Windows2004ed40: restoring a pose does not itself link the entity. */
void G_UnTimeShiftClient(gentity_t *ent)
{
	if (ent->client->backupMarker.serverTime == level.time)
	{
		G_ApplyClientMarker(ent, &ent->client->backupMarker);
		ent->client->backupMarker.serverTime = 0;
	}
}

static void G_AdjustSingleClientPosition(gentity_t *ent, int time)
{
	int i, j;

	if (time > level.time)
	{
		time = level.time;
	} // no lerping forward....

	// find a pair of markers which bound the requested time
	i = j = ent->client->tcePositionHistoryIndex;
	do
	{
		if (ent->client->tcePositionHistory[i].time <= time)
		{
			break;
		}

		j = i;
		i--;
		if (i < 0)
		{
			i = MAX_CLIENT_MARKERS - 1;
		}
	}
	while (i != ent->client->tcePositionHistoryIndex);

	if (i == j)    // oops, no valid stored markers
	{
		return;
	}

	// save current position to backup
	if (ent->client->tcePositionBackup.time != level.time)
	{
		VectorCopy(ent->r.currentOrigin, ent->client->tcePositionBackup.origin);
		VectorCopy(ent->r.mins, ent->client->tcePositionBackup.mins);
		VectorCopy(ent->r.maxs, ent->client->tcePositionBackup.maxs);
		ent->client->tcePositionBackup.time = level.time;
	}

	if (i != ent->client->tcePositionHistoryIndex)
	{
		float frac = (float)(time - ent->client->tcePositionHistory[i].time) /
		             (float)(ent->client->tcePositionHistory[j].time - ent->client->tcePositionHistory[i].time);

		LerpPosition(ent->client->tcePositionHistory[i].origin, ent->client->tcePositionHistory[j].origin, frac, ent->r.currentOrigin);
		LerpPosition(ent->client->tcePositionHistory[i].mins, ent->client->tcePositionHistory[j].mins, frac, ent->r.mins);
		LerpPosition(ent->client->tcePositionHistory[i].maxs, ent->client->tcePositionHistory[j].maxs, frac, ent->r.maxs);
	}
	else
	{
		VectorCopy(ent->client->tcePositionHistory[j].origin, ent->r.currentOrigin);
		VectorCopy(ent->client->tcePositionHistory[j].mins, ent->r.mins);
		VectorCopy(ent->client->tcePositionHistory[j].maxs, ent->r.maxs);
	}

	trap_LinkEntity(ent);
}

static void G_ReAdjustSingleClientPosition(gentity_t *ent)
{
	if (!ent || !ent->client)
	{
		return;
	}

	// restore from backup
	if (ent->client->tcePositionBackup.time == level.time)
	{
		VectorCopy(ent->client->tcePositionBackup.origin, ent->r.currentOrigin);
		VectorCopy(ent->client->tcePositionBackup.mins, ent->r.mins);
		VectorCopy(ent->client->tcePositionBackup.maxs, ent->r.maxs);
		ent->client->tcePositionBackup.time = 0;

		trap_LinkEntity(ent);
	}
}

void G_AdjustClientPositions(gentity_t *ent, int time, qboolean forward)
{
	int       i;
	gentity_t *list;

	for ( i = 0; i < level.numConnectedClients; i++, list++ )
	{
		list = g_entities + level.sortedClients[i];
		// Gordon: ok lets test everything under the sun
		if (list->client &&
		    list->inuse &&
		    (list->client->sess.sessionTeam == TEAM_AXIS || list->client->sess.sessionTeam == TEAM_ALLIES) &&
		    (list != ent) &&
		    list->r.linked &&
		    (list->health > 0) &&
		    !(list->client->ps.pm_flags & PMF_LIMBO) &&
		    (list->client->ps.pm_type == PM_NORMAL)
		    )
		{
			if (forward)
			{
				G_AdjustSingleClientPosition(list, time);
			}
			else
			{
				G_ReAdjustSingleClientPosition(list);
			}
		}
	}
}

/* Windows20048370: initialize the independent44-byte frame history.
 * The x87 spacing stays unrounded until each integer timestamp conversion. */
void G_TCEResetFrameMarkers(gentity_t *ent)
{
	char   text[256];
	int    i, time, fps;
#if defined(__linux__)
	long double spacing;
#else
	double spacing;
#endif
	trap_Cvar_VariableStringBuffer("sv_fps", text, 255);
	fps                                  = atoi(text);
#if defined(__linux__)
	spacing = fps ? (1.0L / (long double)fps) * 1000.0L : 50.0L;
#else
	spacing                              = fps ? 1000.0 / fps : 50.0;
#endif
	ent->client->tcePositionHistoryIndex = 9;
	for (i = 9, time = level.time; i >= 0; --i)
	{
		VectorCopy(ent->r.mins, ent->client->tcePositionHistory[i].mins);
		VectorCopy(ent->r.maxs, ent->client->tcePositionHistory[i].maxs);
		VectorCopy(ent->r.currentOrigin, ent->client->tcePositionHistory[i].origin);
		ent->client->tcePositionHistory[i].time = time;
#if defined(__linux__)
		time = TCE_TruncateNativeInteger((long double)time - spacing);
#else
		time = (int)((double)time - spacing);
#endif
	}
}

void G_ResetMarkers(gentity_t *ent)
{
	int i, time;
	ent->client->topMarker = MAX_CLIENT_MARKERS - 1;
	for ( i = MAX_CLIENT_MARKERS - 1, time = level.time; i >= 0; i--, time = TCE_SubInt32(time, 50) )
	{
		G_CaptureClientMarker(ent, &ent->client->clientMarkers[i], time);
		ent->client->clientMarkers[i].serverTime = time;
	}
}

void G_AttachBodyParts(gentity_t *ent)
{
	int       i;
	gentity_t *list;

	for ( i = 0; i < level.numConnectedClients; i++, list++ )
	{
		list = g_entities + level.sortedClients[i];
		// Gordon: ok lets test everything under the sun
		if (list->inuse &&
		    (list->client->sess.sessionTeam == TEAM_AXIS || list->client->sess.sessionTeam == TEAM_ALLIES) &&
		    (list != ent) &&
		    list->r.linked &&
		    (list->health > 0) &&
		    !(list->client->ps.pm_flags & PMF_LIMBO) &&
		    (list->client->ps.pm_type == PM_NORMAL)
		    )
		{
			list->client->tempHead = G_BuildHead(list);
			list->client->tempLeg  = G_BuildLeg(list);
		}
		else
		{
			list->client->tempHead = NULL;
			list->client->tempLeg  = NULL;
		}
	}
}

void G_DettachBodyParts()
{
	int       i;
	gentity_t *list;

	for ( i = 0; i < level.numConnectedClients; i++, list++ )
	{
		list = g_entities + level.sortedClients[i];
		if (list->client->tempHead)
		{
			G_FreeEntity(list->client->tempHead);
		}
		if (list->client->tempLeg)
		{
			G_FreeEntity(list->client->tempLeg);
		}
	}
}

int G_SwitchBodyPartEntity(gentity_t *ent)
{
	if (ent->s.eType == ET_TEMPHEAD)
	{
		return ent->parent - g_entities;
	}
	if (ent->s.eType == ET_TEMPLEGS)
	{
		return ent->parent - g_entities;
	}
	return ent - g_entities;
}

#define POSITION_READJUST                       \
		if (res != results->entityNum) {               \
			VectorSubtract(end, start, dir);          \
			VectorNormalizeFast(dir);             \
                                    \
			VectorMA(results->endpos, -1, dir, results->endpos);  \
			results->entityNum = res;               \
		}

// Run a trace with players in historical positions.
void G_HistoricalTrace(gentity_t *ent, trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask)
{
	int    res;
	vec3_t dir;

	if (!g_antilag.integer || !ent->client)
	{
		G_AttachBodyParts(ent);

		trap_Trace(results, start, mins, maxs, end, passEntityNum, contentmask);

		res = G_SwitchBodyPartEntity(&g_entities[results->entityNum]);
		POSITION_READJUST

		G_DettachBodyParts();
		return;
	}

	G_AdjustClientPositions(ent, ent->client->pers.cmd.serverTime, qtrue);

	G_AttachBodyParts(ent) ;

	trap_Trace(results, start, mins, maxs, end, passEntityNum, contentmask);

	res = G_SwitchBodyPartEntity(&g_entities[results->entityNum]);
	POSITION_READJUST

	G_DettachBodyParts();

	G_AdjustClientPositions(ent, 0, qfalse);
}

/* Original iterates all64 slots and accepts team0 as well as the two teams.
 * Bound scaling belongs only to the historical bullet trace, never movement. */
void G_TimeShiftAllClients(gentity_t *ent, qboolean scaleBounds)
{
	int i, time = ent->client->pers.cmd.serverTime;
	if (time > level.time)
	{
		time = level.time;
	}
	for (i = 0; i < MAX_CLIENTS; ++i)
	{
		gentity_t *target = &g_entities[i];
		if (target->client && target->inuse && (unsigned)target->client->sess.sessionTeam < TEAM_SPECTATOR &&
		    target != ent && target->health > 0)
		{
			G_TimeShiftClient(target, time);
			if (scaleBounds)
			{
				float xy, z;
				if (target->client->ps.eFlags & EF_PRONE)
				{
					target->r.svFlags |= 0x40000;
					xy                 = 2.5f; z = 0.5f;
				}
				else
				{
					target->r.svFlags |= 0x20000;
					xy                 = 1.5f; z = 1.25f;
				}
				target->r.maxs[0] *= xy; target->r.maxs[1] *= xy;
				target->r.maxs[2] *= z;
				target->r.mins[0] *= xy; target->r.mins[1] *= xy;
			}
			trap_LinkEntity(target);
		}
	}
}

void G_UnTimeShiftAllClients(gentity_t *ent)
{
	int i;
	for (i = 0; i < MAX_CLIENTS; ++i)
	{
		gentity_t *target = &g_entities[i];
		if (target->client && target->inuse && (unsigned)target->client->sess.sessionTeam < TEAM_SPECTATOR &&
		    target != ent && target->health > 0)
		{
			if (target->r.svFlags & 0x60000)
			{
				float xy, z;
				if (target->r.svFlags & 0x40000)
				{
					target->r.svFlags &= ~0x40000;
					xy                 = 0.4f; z = 2.0f;
				}
				else
				{
					target->r.svFlags &= ~0x20000;
					xy                 = 2.0f / 3.0f; z = 0.8f;
				}
				target->r.maxs[0] *= xy; target->r.maxs[1] *= xy;
				target->r.maxs[2] *= z;
				target->r.mins[0] *= xy; target->r.mins[1] *= xy;
			}
			G_UnTimeShiftClient(target);
			trap_LinkEntity(target);
		}
	}
}

void G_HistoricalTraceBegin(gentity_t *ent)
{
	G_AdjustClientPositions(ent, ent->client->pers.cmd.serverTime, qtrue);
}

void G_HistoricalTraceEnd(gentity_t *ent)
{
	G_AdjustClientPositions(ent, 0, qfalse);
}

//bani - Run a trace without fixups (historical fixups will be done externally)
void G_Trace(gentity_t *ent, trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask)
{
	int    res;
	vec3_t dir;

	G_AttachBodyParts(ent);

	trap_Trace(results, start, mins, maxs, end, passEntityNum, contentmask);

	res = G_SwitchBodyPartEntity(&g_entities[results->entityNum]);
	POSITION_READJUST

	G_DettachBodyParts();
}
