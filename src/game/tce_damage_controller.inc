/* Recovered TC damage controller. Internal decomposition of Windows200589c0;
 * no original-function completion is implied by the helpers below. */
#include "tce_damage_trace.h"
#include "g_tce_bullet_damage.h"
#include "g_tce_weapon_stats.h"
#include "g_tce_hit_debug.h"
extern void G_Voice(gentity_t *, gentity_t *, int, const char *, qboolean);

static void TCE_DamageDebugBox(gentity_t *part, const vec3_t start, const vec3_t end, float fraction
#if defined(__linux__)
                               , const vec3_t dir, float scale
#endif
                               )
{
	vec3_t mins, maxs, hit; gentity_t *event; int i;
	if (g_debugBullets.integer <= 0 || !(g_debugBullets.integer & 2))
	{
		return;
	}
	VectorAdd(part->r.currentOrigin, part->r.mins, mins); VectorAdd(part->r.currentOrigin, part->r.maxs, maxs);
	event = G_TempEntity(mins, EV_RAILTRAIL); VectorCopy(maxs, event->s.origin2); event->s.dmgFlags = 1;
#if defined(__linux__)
	VectorCopy(end, hit);
	if (fraction != 1.0f && !TCE_FloatIsNaN(fraction))
	{
		long double length = (long double)fraction * scale;
		for (i = 0; i < 3; ++i)
		{
			hit[i] = (float)((long double)dir[i] * length * 64.0f + start[i]);
		}
	}
#else
	for (i = 0; i < 3; ++i)
		hit[i] = start[i] + fraction * (end[i] - start[i]);
#endif
	event = G_TempEntity((float *)start, EV_RAILTRAIL); VectorCopy(hit, event->s.origin2); event->s.dmgFlags = 0;
}

static int TCE_ApplyBodyDamage(gentity_t *targ, gentity_t *attacker, int mod,
                               const vec3_t dir, const vec3_t point, int damage, qboolean regionalBullet
#if defined(__linux__)
                               , int *bodyStatus
#endif
                               )
{
	tceDamagePose_t pose; tceDamageVolume_t volumes[3]; float fractions[3] = { 1, 1, 1 };
	gentity_t       *part; trace_t tr; vec3_t end; int i, j, amounts[3], take; tce_damageScale_t scales[3];
#if defined(__linux__)
	*bodyStatus = 0;
#endif
	/* Original first diagnostic ray reads uninitialized locals.  Keep its
	 * surrounding box/ray events, using the defined incoming ray instead. */
	if (g_debugBullets.integer > 0 && (g_debugBullets.integer & 2))
	{
		for (j = 0; j < 3; ++j)
			end[j] = point[j] + dir[j] * 64;
#if defined(__linux__)
		TCE_DamageDebugBox(targ, point, end, 1, dir, 1);
#else
		TCE_DamageDebugBox(targ, point, end, 1);
#endif
	}
	if (targ->client->sess.sessionTeam != TEAM_AXIS && targ->client->sess.sessionTeam != TEAM_ALLIES)
	{
#if defined(__linux__)
		*bodyStatus = -1;
#endif
		return -1;
	}
	part = G_Spawn(); G_SetOrigin(part, targ->r.currentOrigin);
	TCE_ClientDamagePose(targ, &pose);
	if (g_debugDamage.integer)
	{
		if (pose.prone)
		{
			G_Printf(pose.aiming?"moving proned neck: ":"proned neck: ");
		}
		else if (pose.ducked)
		{
			G_Printf(pose.aiming?"moving ducked neck: ":"ducked neck: ");
		}
		else
		{
			G_Printf(pose.aiming?"moving standing neck: ":"standing neck: ");
			if (pose.weapon == 1)
			{
				G_Printf("knife anim: ");
			}
			else if (pose.weapon == 4 || pose.weapon == 9 || pose.weapon == 30)
			{
				G_Printf("grenade anim: ");
			}
			else if (pose.weapon == 2 || pose.weapon == 39 || pose.weapon == 40 || pose.weapon == 7 || pose.weapon == 37 || pose.weapon == 38 ||
			         pose.weapon == 10 || pose.weapon == 3 || pose.weapon == 52 || pose.weapon == 14 || pose.weapon == 53 || pose.weapon == 54)
			{
				G_Printf("single handed anim: ");
			}
		}
	}
	TCE_DamageVolumes(&pose, volumes);
#if defined(__linux__)
	for (j = 0; j < 3; ++j)
	{
		end[j] = (float)((long double)(pose.enlarged ? 1.25f : 1.0f) * dir[j] * 64.0f + point[j]);
	}
#else
	for (j = 0; j < 3; ++j)
		end[j] = point[j] + dir[j] * (pose.enlarged?80.0f:64.0f);
#endif
	for (i = 0; i < 3; ++i)
	{
		if (i)
		{
			G_SetOrigin(part, targ->r.currentOrigin);
		}
		VectorCopy(volumes[i].origin, part->r.currentOrigin); VectorCopy(volumes[i].origin, part->s.origin);
		VectorCopy(targ->r.currentAngles, part->s.angles);
		VectorCopy(targ->r.currentAngles, part->s.apos.trBase); VectorCopy(targ->r.currentAngles, part->s.apos.trDelta);
		VectorCopy(volumes[i].mins, part->r.mins); VectorCopy(volumes[i].maxs, part->r.maxs);
		part->clipmask = 1; part->r.contents = 1; trap_LinkEntity(part);
		/* The broad player/corpse hulls are candidate selectors, not cover
		 * for another player's narrow volumes. Keep all solid/missile cover. */
		trap_Trace(&tr, point, NULL, NULL, end, targ->s.number,
		           MASK_MISSILESHOT & ~(CONTENTS_BODY | CONTENTS_CORPSE));
		G_TCEHitDebugTrace("regional-narrow", attacker ? attacker->s.number : -1, targ->s.number, i, &tr, point, end);
		if (&g_entities[tr.entityNum] == part)
		{
			fractions[i] = tr.fraction;
			if (g_debugDamage.integer)
			{
				G_Printf(i == 0?"hit head\n":i == 1?"hit body\n":"hit legs\n");
			}
		}
#if defined(__linux__)
		TCE_DamageDebugBox(part, point, end, tr.fraction, dir, pose.enlarged ? 1.25f : 1.0f);
#else
		TCE_DamageDebugBox(part, point, end, tr.fraction);
#endif
	}
	G_FreeEntity(part);
#if defined(__linux__)
	if ((fractions[0] == 1 || TCE_FloatIsNaN(fractions[0])) &&
	    (fractions[1] == 1 || TCE_FloatIsNaN(fractions[1])) &&
	    (fractions[2] == 1 || TCE_FloatIsNaN(fractions[2])))
#else
	if (fractions[0] == 1 && fractions[1] == 1 && fractions[2] == 1)
#endif
	{
#if defined(__linux__)
		*bodyStatus = -2;
#endif
		return -2;
	}
	if (regionalBullet)
	{
		int region = -1;
		long long weighted;
		/* A confirmed bullet contact has the region's nominal damage across
		 * its full volume. Do not attenuate hits a second time by distance
		 * from the volume center. Equal entry fractions choose the lower
		 * coefficient: legs before torso before head. Cover was checked above. */
		for (i = 0; i < 3; ++i)
		{
			amounts[i] = 0; scales[i] = 0;
			if (fractions[i] >= 0 && fractions[i] < 1 &&
			    (region < 0 || fractions[i] <= fractions[region])) region = i;
		}
		if (region < 0)
		{
#if defined(__linux__)
			*bodyStatus = -2;
#endif
			return -2;
		}
		weighted = region == 0 ? (long long)damage * 2 : region == 2 ? damage / 2 : damage;
		take = weighted > INT_MAX ? INT_MAX : (int)weighted;
		if (damage > 0 && take < 1) take = 1;
		amounts[region] = take;
		scales[region] = region == 0 ? 2 : region == 2 ? .5 : 1;
	}
	else
	{
		/* Preserve the historical weighting for radius/non-bullet callers. */
		take = TCE_DamageWeights(volumes, fractions, point, dir, pose.enlarged?1.25f:1.0f, damage, amounts, scales);
	}
	/* A confirmed nearest head volume counts once; reuse the three existing
	 * probes instead of the unrelated SDK head helper and another trace. */
	if (take > 0 && amounts[0] > 0 &&
	    (regionalBullet || (fractions[0] < fractions[1] && fractions[0] < fractions[2])) &&
	    attacker && attacker->client && attacker != targ && !OnSameTeam(targ, attacker))
	{
		G_LogRegionHit(attacker, HR_HEAD);
		G_addStatsHeadShot(attacker, mod);
	}
	for (i = 0; i < 3; ++i)
		if (amounts[i] > 0)
		{
#if defined(__linux__)
			targ->client->ps.holdable[i + 2] = TCE_AddInt32(targ->client->ps.holdable[i + 2], amounts[i]);
#else
			targ->client->ps.holdable[i + 2] += amounts[i];
#endif
		}
	if (g_debugDamage.integer)
	{
		G_Printf("take: %i, scales: headtmp: %f, bodytmp: %f, legstmp: %f\n", take, (double)scales[0], (double)scales[1], (double)scales[2]);
	}
	return take;
}

/* H3 uses the exact immutable anatomical selection, never a second pose or
 * box probe. Ordinary bullets use the cover-confirmed regions above. */
static qboolean TCE_PresentationSelectionValid(const gentity_t *targ,
    const g_tceBulletDamageSelection_t *selection)
{
	return targ && targ->client && targ->health > 0 && selection &&
	    selection->trace.state == G_TCE_POSE_TRACE_HIT &&
	    selection->query.clientNum == targ->s.number &&
	    selection->trace.key.clientNum == targ->s.number &&
	    selection->trace.surface.region >= TCE_POSE_REGION_HEAD &&
	    selection->trace.surface.region <= TCE_POSE_REGION_LEGS &&
	    selection->trace.surface.fraction >= 0.0 &&
	    selection->trace.surface.fraction < selection->maxFraction &&
	    G_TCEPosePresentationTraceMatches(&selection->trace, &selection->query,
	        selection->start, selection->end, selection->maxFraction);
}

static int TCE_ApplyPresentationDamage(gentity_t *targ, gentity_t *attacker,
    int mod, int damage, const g_tceBulletDamageSelection_t *selection)
{
	int region, take;
	long long weighted;
	/* A Lua callback may have changed this life after wrapper prevalidation. */
	if (!TCE_PresentationSelectionValid(targ, selection))
	{
		return -2;
	}
	if (targ->client->sess.sessionTeam != TEAM_AXIS &&
	    targ->client->sess.sessionTeam != TEAM_ALLIES)
	{
		return -1;
	}
	region = selection->trace.surface.region;
	/* Nominal coefficients are HEAD 2, BODY 1, LEGS .5. No radial bridge
	 * weights from an unrelated standing pose contribute to these injuries. */
	weighted = region == TCE_POSE_REGION_HEAD ? (long long)damage * 2 :
	           region == TCE_POSE_REGION_LEGS ? damage / 2 : damage;
	take = weighted > INT_MAX ? INT_MAX : (int)weighted;
	if (damage > 0 && take < 1) take = 1;
	if (take > 0)
	{
		targ->client->ps.holdable[region + 2] =
		    TCE_AddInt32(targ->client->ps.holdable[region + 2], take);
		if (region == TCE_POSE_REGION_HEAD && attacker && attacker->client &&
		    attacker != targ && !OnSameTeam(targ, attacker))
		{
			G_LogRegionHit(attacker, HR_HEAD);
			G_addStatsHeadShot(attacker, mod);
		}
	}
	return take;
}

static tce_damageResult_t TCE_DamageController(gentity_t *targ, gentity_t *inflictor,
    gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod,
    const g_tceBulletDamageSelection_t *selection)
{
	gclient_t *client; int take, wasAlive, i, headBefore; mapEntityData_t *mapEnt;
	if (!targ->takedamage || level.intermissionQueued || g_gamestate.integer != GS_PLAYING)
	{
		return 0;
	}
	if (!inflictor)
	{
		inflictor = &g_entities[ENTITYNUM_WORLD];
	}
	if (!attacker)
	{
		attacker = &g_entities[ENTITYNUM_WORLD];
	}
	if (targ->entstate == STATE_INVISIBLE || targ->entstate == STATE_UNDERCONSTRUCTION)
	{
		return 0;
	}
	wasAlive = targ->health > 0;
	if (targ->client && attacker->client && attacker != targ && !OnSameTeam(attacker, targ))
	{
		targ->client->combatState |= 4; attacker->client->combatState |= 2;
	}
	if (targ->waterlevel > 2 && mod == 19)
	{
		return 0;
	}
	if (targ->s.eType == ET_MOVER && !targ->isProp && !targ->scriptName)
	{
		if (targ->use && targ->moverState == MOVER_POS1)
		{
			G_UseEntity(targ, inflictor, attacker);
		}
		return 0;
	}
	if (targ->s.eType == ET_MOVER && (targ->spawnflags & 4) && !targ->isProp)
	{
		if (!G_WeaponIsExplosive(mod) || G_GetTeamFromEntity(targ) == G_GetTeamFromEntity(inflictor))
		{
			return 0;
		}
	}
	else if (targ->s.eType == ET_EXPLOSIVE)
	{
		if ((targ->parent && G_GetWeaponClassForMOD(mod) == 2) || G_GetTeamFromEntity(targ) == G_GetTeamFromEntity(inflictor) ||
		    G_GetWeaponClassForMOD(mod) < targ->constructibleStats.weaponclass)
		{
			return 0;
		}
	}
	else if (targ->s.eType == ET_MISSILE && targ->methodOfDeath == 45)
	{
		if (targ->s.modelindex2 && G_WeaponIsExplosive(mod))
		{
			for (i = 0; i < 2; ++i)
				if ((mapEnt = G_FindMapEntityData(&mapEntityData[i], targ - g_entities)) != NULL)
				{
					G_FreeMapEntityData(&mapEntityData[i], mapEnt);
				}
			if (attacker->client)
			{
				AddScore(attacker, 1);
			}
			G_ExplodeMissile(targ);
		}
		return 0;
	}
	else if (targ->s.eType == ET_CONSTRUCTIBLE)
	{
		if (G_GetTeamFromEntity(targ) == G_GetTeamFromEntity(inflictor) ||
		    G_GetWeaponClassForMOD(mod) < targ->constructibleStats.weaponclass ||
		    (mod == 26 && !(inflictor->etpro_misc_1 & 1)))
		{
			return 0;
		}
	}
	client = targ->client;
	if (client)
	{
		client->tceLastAppliedDamage = 0; if (client->noclip)
		{
			return 0;
		}
#if defined(__linux__)
		if (attacker->client && !(dflags & DAMAGE_NO_PROTECTION) && targ != attacker &&
		    level.time < TCE_AddInt32(client->tceDamageSpawnTime, 3000))
#else
		if (attacker->client && !(dflags & DAMAGE_NO_PROTECTION) && targ != attacker && level.time < client->tceDamageSpawnTime + 3000)
#endif
		{
			/* CQB keeps GT7 spawn immunity without punishing the shooter. */
			if (g_gametype.integer == 5 && OnSameTeam(targ, attacker))
			{
				G_Damage(attacker, NULL, attacker, NULL, NULL, 1000, DAMAGE_NO_PROTECTION, 65);
			}
			return 0;
		}
	}
	if (targ->flags & FL_GODMODE)
	{
		return 0;
	}
	/* Raw incoming damage, before TC body weighting or health effects. Native
	 * immunity/mover-use/special missile dispatch above retains its ordering. */
	if (TCE_LuaDamage(targ->s.number, attacker->s.number, damage, dflags, mod))
	{
		return 0;
	}
	if (damage < 1)
	{
		damage = 1;
	}
	headBefore = client ? client->ps.holdable[2] : 0;
	take = damage;
	if ((selection || IsHeadShotWeapon(mod)) && client && wasAlive)
	{
#if defined(__linux__)
		int bodyStatus;
		if (selection)
		{
			take = TCE_ApplyPresentationDamage(targ, attacker, mod, damage, selection);
			bodyStatus = take < 0 ? take : 0;
		}
		else
		{
			take = TCE_ApplyBodyDamage(targ, attacker, mod, dir, point, damage, !(dflags & DAMAGE_RADIUS), &bodyStatus);
		}
		if (bodyStatus == -2)
#else
		take = selection ? TCE_ApplyPresentationDamage(targ, attacker, mod, damage, selection) :
		       TCE_ApplyBodyDamage(targ, attacker, mod, dir, point, damage, !(dflags & DAMAGE_RADIUS));
		if (take == -2)
#endif
		{
			return 1;
		}
#if defined(__linux__)
		if (bodyStatus < 0)
#else
		if (take < 0)
#endif
		{
			return 0;
		}
		if (g_gametype.integer == 5 && attacker->client && !OnSameTeam(targ, attacker))
		{
#if defined(__linux__)
			if (TCE_SubInt32(level.time, attacker->client->tceDamageVoiceTime) > 0)
#else
			if ((int)((unsigned)level.time - (unsigned)attacker->client->tceDamageVoiceTime) > 0)
#endif
			{
				G_Voice(attacker, NULL, SAY_TEAM, "Contact", qfalse);
#if defined(__linux__)
				attacker->client->tceDamageVoiceTime = TCE_AddInt32(level.time, 20000);
#else
				attacker->client->tceDamageVoiceTime = level.time + 20000;
#endif
			}
#if defined(__linux__)
			if (TCE_SubInt32(level.time, client->tceDamageVoiceTime) > 0)
#else
			if ((int)((unsigned)level.time - (unsigned)client->tceDamageVoiceTime) > 0)
#endif
			{
				G_Voice(targ, NULL, SAY_TEAM, "TakingFire", qfalse);
#if defined(__linux__)
				client->tceDamageVoiceTime = TCE_AddInt32(level.time, 20000);
#else
				client->tceDamageVoiceTime = level.time + 20000;
#endif
			}
		}
		if (take < 1)
		{
			return 1;
		}
	}
	else if (client && wasAlive)
	{
#if defined(__linux__)
		client->ps.holdable[4] = TCE_AddInt32(client->ps.holdable[4], damage);
#else
		client->ps.holdable[4] += damage;
#endif
		if (mod == 4 || mod == 62 || mod == 18 || mod == 26)
		{
#if defined(__linux__)
			client->ps.holdable[3] = TCE_AddInt32(client->ps.holdable[3], damage);
			client->ps.stats[STAT_DEAD_YAW] = TCE_AddInt32(client->ps.stats[STAT_DEAD_YAW], damage / 2);
#else
			client->ps.holdable[3] += damage; client->ps.stats[STAT_DEAD_YAW] += damage / 2;
#endif
		}
		else
		{
#if defined(__linux__)
			client->ps.holdable[3] = TCE_AddInt32(client->ps.holdable[3], damage / 2);
#else
			client->ps.holdable[3] += damage / 2;
#endif
		}
	}
	if (dir)
	{
		VectorNormalize(dir);
	}
	if (attacker->client && targ != attacker && targ->health > 0)
	{
#if defined(__linux__)
		if (OnSameTeam(targ, attacker))
		{
			attacker->client->ps.persistant[PERS_HITS] = TCE_SubInt32(attacker->client->ps.persistant[PERS_HITS], take);
		}
		else
		{
			attacker->client->ps.persistant[PERS_HITS] = TCE_AddInt32(attacker->client->ps.persistant[PERS_HITS], take);
		}
#else
		attacker->client->ps.persistant[PERS_HITS] += OnSameTeam(targ, attacker)?-take:take;
#endif
	}
	if (g_debugDamage.integer)
	{
		G_Printf("client:%i health:%i damage:%i mod:%s\n", targ->s.number, targ->health, take, modNames[mod]);
	}
	if (client)
	{
#if defined(__linux__)
		client->ps.persistant[PERS_ATTACKER] = attacker->s.number;
		client->damage_blood = TCE_AddInt32(client->damage_blood, take);
#else
		client->ps.persistant[PERS_ATTACKER] = attacker->s.number; client->damage_blood += take;
#endif
		if (wasAlive)
		{
			client->tceLastAppliedDamage = take;
		}
		if (dir)
		{
			VectorCopy(dir, client->damage_from); client->damage_fromWorld = qfalse;
		}
		else
		{
			VectorCopy(targ->r.currentOrigin, client->damage_from); client->damage_fromWorld = qtrue;
		}
		client->lasthurt_client = attacker->s.number; client->lasthurt_mod = mod;
	}
	if (take)
	{
#ifdef FEATURE_OMNIBOT
		if (client && wasAlive && take > 0)
		{
			Bot_Event_TakeDamage(targ->s.number, attacker);
		}
#endif
		if (!client)
		{
#if defined(__linux__)
			targ->health = TCE_SubInt32(targ->health, take);
#else
			targ->health -= take;
#endif
		}
		else
		{
			if (client->ps.holdable[2] < 100 && client->ps.holdable[3] < 100 && client->ps.holdable[4] < 100)
			{
#if defined(__linux__)
				targ->health = TCE_TruncateNativeInteger(
				    (long double)TCE_SubInt32(100, client->ps.holdable[2]) *
				    TCE_SubInt32(100, client->ps.holdable[3]) *
				    TCE_SubInt32(100, client->ps.holdable[4]) * 0.0001f);
#else
				targ->health = (int)((double)(100 - client->ps.holdable[2]) * (100 - client->ps.holdable[3]) * (100 - client->ps.holdable[4]) * 0.00009999999747378752);
#endif
			}
			else if ((mod == 4 || mod == 18) && client->ps.holdable[3] > 100)
			{
#if defined(__linux__)
				targ->health = TCE_SubInt32(100, client->ps.holdable[3]);
#else
				targ->health = 100 - client->ps.holdable[3];
#endif
			}
			else
			{
				targ->health = -1;
			}
			client->ps.stats[STAT_HEALTH] = targ->health;
		}
		G_TCEWeaponStatsDamage(targ, inflictor, attacker, mod, take, wasAlive,
		    client && client->ps.holdable[2] != headBefore);
		if (targ->health < 1 && (targ->r.svFlags & SVF_POW))
		{
			targ->health = -1;
		}
		if (targ->health < -174)
		{
			targ->health = -174;
		}
		if (targ->s.eType == ET_MOVER && !Q_stricmp(targ->classname, "script_mover"))
		{
#if defined(__linux__)
			targ->s.dl_intensity = TCE_TruncateNativeInteger(
			    (1.0L / targ->count) * targ->health * 255.0f);
#else
			targ->s.dl_intensity = (int)((double)targ->health / targ->count * 255);
#endif
		}
		if (targ->health < 1)
		{
			if (client && !wasAlive)
			{
				targ->flags |= FL_NO_KNOCKBACK;
				if (client->ps.pm_type == PM_DEAD)
				{
					G_addStats(targ, attacker, take, mod);
				}
				if (targ->health > -175 && client->ps.pm_time < 1 && g_gametype.integer != 7)
				{
					limbo(targ, qtrue);
				}
				if (!client->deathTime)
				{
					client->deathTime = level.time;
				}
			}
			else
			{
				targ->sound1to2 = 4; targ->sound2to1 = mod; targ->sound2to3 = dflags & DAMAGE_RADIUS;
				if (client && G_GetTeamFromEntity(targ) != G_GetTeamFromEntity(inflictor))
				{
					G_AddKillSkillPoints(attacker, mod, 4, dflags & DAMAGE_RADIUS);
				}
				if (targ->health < -999)
				{
					targ->health = -999;
				}
				targ->enemy = attacker; targ->deathType = mod;
				if (targ->die)
				{
					if (client && attacker->client)
					{
						client->tceLastKillingWeapon = attacker->client->ps.weapon;
					}
					targ->die(targ, inflictor, attacker, take, mod);
				}
				if ((targ->s.eType == ET_MOVER && !Q_stricmp(targ->classname, "script_mover") && (targ->spawnflags & 8)) || !targ->inuse)
				{
					return 0;
				}
				if (targ->health < 1)
				{
					if (targ->r.svFlags & SVF_BOT)
					{
						if (wasAlive && TCE_INTERNAL_BOT(targ))
						{
							Bot_ScriptEvent(targ->s.number, "death", "");
						}
					}
					else if ((targ->s.eType != ET_CONSTRUCTIBLE && targ->s.eType != ET_EXPLOSIVE) || (targ->s.eType == ET_CONSTRUCTIBLE && !targ->desstages))
					{
						G_Script_ScriptEvent(targ, "death", "");
					}
				}
				if (targ->s.number < level.maxclients && (targ->r.svFlags & SVF_BOT) && TCE_INTERNAL_BOT(targ))
				{
					BotRecordDeath(targ->s.number, attacker->s.number);
				}
			}
		}
		else if (targ->pain)
		{
			if (dir)
			{
				VectorCopy(dir, targ->rotate); VectorCopy(point, targ->pos3);
			}
			else
			{
				VectorClear(targ->rotate); VectorClear(targ->pos3);
			}
			targ->pain(targ, attacker, take, point);
		}
		else
		{
			G_addStats(targ, attacker, take, mod);
		}
#if defined(__linux__)
		G_Script_ScriptEvent(targ, "pain", va("%d %d", targ->health, TCE_AddInt32(targ->health, take)));
#else
		G_Script_ScriptEvent(targ, "pain", va("%d %d", targ->health, targ->health + take));
#endif
		if (targ->s.number < MAX_CLIENTS && (targ->r.svFlags & SVF_BOT) && TCE_INTERNAL_BOT(targ))
		{
#if defined(__linux__)
			Bot_ScriptEvent(targ->s.number, "pain", va("%d %d", targ->health, TCE_AddInt32(targ->health, take)));
#else
			Bot_ScriptEvent(targ->s.number, "pain", va("%d %d", targ->health, targ->health + take));
#endif
		}
		if (targ->s.number < level.maxclients && (targ->r.svFlags & SVF_BOT) && TCE_INTERNAL_BOT(targ))
		{
			BotRecordPain(targ->s.number, attacker->s.number, mod);
		}
	}
	if (targ->client)
	{
#if defined(__linux__)
		long double remaining = 0;
		if (damage > 0)
		{
			remaining = 1.0L - (long double)take * (1.0L / damage);
		}
		return remaining < (long double)0.1f ? (long double)0.1f : remaining;
#else
		float remaining = (float)(1.0 - (double)take / damage); return remaining < 0.1f?0.1f:remaining;
#endif
	}
	return 0;
}

/* Preserve the mod-facing ABI; radius/non-bullet weighting is unchanged. */
tce_damageResult_t G_Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
    vec3_t dir, vec3_t point, int damage, int dflags, int mod)
{
	return TCE_DamageController(targ, inflictor, attacker, dir, point, damage,
	                            dflags, mod, NULL);
}

tce_damageResult_t G_TCEPresentationDamage(gentity_t *targ, gentity_t *inflictor,
    gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod,
    const g_tceBulletDamageSelection_t *selection)
{
	/* Reject stale/mismatched selections before combat flags or Lua hooks. */
	if ((dflags & DAMAGE_RADIUS) || !TCE_PresentationSelectionValid(targ, selection))
	{
		return 1;
	}
	return TCE_DamageController(targ, inflictor, attacker, dir, point, damage,
	                            dflags, mod, selection);
}
