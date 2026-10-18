#ifndef TCE_NAV_DOOR_QUERY_H
#define TCE_NAV_DOOR_QUERY_H

/* Read-only observations of the real mover/activation state. This file never
 * invokes use, think, scripts, target chains, or changes an entity pose. */
static int TceNavUseKind(const gentity_t *ent) {
    if(!ent || !ent->inuse || !ent->classname || !ent->r.linked ||
       ent->entstate == STATE_INVISIBLE || ent->entstate == STATE_UNDERCONSTRUCTION) return 0;
    if(!Q_stricmp(ent->classname, "func_door")) return 1;
    if(!Q_stricmp(ent->classname, "func_door_rotating")) return 2;
    if(!Q_stricmp(ent->classname, "func_button")) return 3;
    if(!Q_stricmp(ent->classname, "func_invisible_user")) return 4;
    return 0;
}

static int TceNavDoorSingleState(const gentity_t *ent) {
    const int kind = TceNavUseKind(ent);
    if(kind < 1 || kind > 2 || (kind == 1 && (ent->spawnflags & 1))) return -1;
    switch(ent->moverState) {
        case MOVER_POS1: case MOVER_POS1ROTATE: return 0;
        case MOVER_1TO2: case MOVER_1TO2ROTATE: return 1;
        case MOVER_POS2: case MOVER_POS2ROTATE: return 2;
        case MOVER_2TO1: case MOVER_2TO1ROTATE: return 3;
        default: return -1;
    }
}

static int TceNavDoorGroupStat(const gentity_t *ent, int stat) {
    if(TceNavDoorSingleState(ent) < 0) return -1;
    const gentity_t *master = ent->teammaster ? ent->teammaster : ent;
    int state = 2, team = 3, remaining = -1, count = 0;
    for(const gentity_t *part = master; part; part = part->teamchain) {
        if(++count > MAX_GENTITIES || TceNavDoorSingleState(part) < 0) return -1;
        if(part->key != 0 || (TceNavUseKind(part) == 1 && (part->spawnflags & 1))) return stat == 1 ? 1 : -1;
        const int single = TceNavDoorSingleState(part);
        if(single == 3) state = 3;
        else if(single == 0 && state != 3) state = 0;
        else if(single == 1 && state == 2) state = 1;
        team &= part->allowteams ? (part->allowteams & 3) : 3;
        if(single == 2 && part->nextthink > 0) {
            const int delay = part->nextthink > level.time ? part->nextthink - level.time : 0;
            if(remaining < 0 || delay < remaining) remaining = delay;
        }
    }
    if(stat == 1) return 0;
    if(stat == 2) return team;
    if(stat == 3) return remaining;
    if(stat == 4) return TceNavUseKind(master) == 2 && (master->spawnflags & 32) ? 1 : 0;
    return state;
}

static qboolean TceNavUseReady(gentity_t *actor, gentity_t *target, float fraction) {
    const int kind = TceNavUseKind(target);
    if(!kind || !G_AllowTeamsAllowed(target, actor)) return qfalse;
    /* Cmd_Activate_f handles mountable movers before its door/button cases. */
    if(target->s.eType == ET_MOVER && (target->spawnflags & 128)) return qfalse;
    if(kind == 1 || kind == 2) {
        return fraction <= .4f && !(target->spawnflags & 1) &&
            target->s.apos.trType == TR_STATIONARY && target->s.pos.trType == TR_STATIONARY &&
            !target->active && TceNavDoorGroupStat(target, 1) == 0 ? qtrue : qfalse;
    }
    if(kind == 3) return target->s.apos.trType == TR_STATIONARY &&
        target->s.pos.trType == TR_STATIONARY && !target->active ? qtrue : qfalse;
    return !(target->spawnflags & 1) && !(target->scriptName && *target->scriptName) &&
        target->wait < level.time && target->use ? qtrue : qfalse;
}

static qboolean TceNavOtherActivateStopsFallback(const gentity_t *ent) {
    /* Conservative for unrelated interactive entities: never authorize a
     * button behind an item, gun, checkpoint, or other first-pass interaction. */
    if(!ent || !ent->inuse || !ent->classname) return qfalse;
    return ent->s.eType == ET_ITEM || ent->s.eType == ET_ALARMBOX ||
        (ent->s.eType == ET_MOVER && (ent->spawnflags & 128)) ||
        !Q_stricmp(ent->classname, "team_WOLF_checkpoint") ||
        !Q_stricmp(ent->classname, "props_footlocker") ||
        !Q_stricmp(ent->classname, "misc_mg42") || !Q_stricmp(ent->classname, "misc_aagun") ? qtrue : qfalse;
}

static int TceNavUseIdentity(gentity_t *actor) {
    if(!actor || !actor->inuse || !actor->client || actor->health <= 0 || actor->active ||
       actor->s.weapon == 60 || actor->s.weapon == 62 ||
       actor->client->sess.sessionTeam == TEAM_SPECTATOR ||
       ((actor->client->ps.eFlags & EF_MOUNTEDTANK) && (actor->s.eFlags & EF_MOUNTEDTANK))) return -1;
    vec3_t forward, right, up, eye, end;
    AngleVectors(actor->client->ps.viewangles, forward, right, up);
    VectorCopy(actor->client->ps.origin, eye);
    eye[2] += actor->client->ps.viewheight;
    if(actor->client->ps.leanf) VectorMA(eye, actor->client->ps.leanf, right, eye);
    VectorMA(eye, 96.f, forward, end);
    trace_t trace;
    const int ordinary = CONTENTS_SOLID | CONTENTS_MISSILECLIP | CONTENTS_BODY | CONTENTS_CORPSE;
    trap_Trace(&trace, eye, NULL, NULL, end, actor->s.number, ordinary);
    for(int pass = 0; pass < 2; ++pass) {
        const qboolean blocked = (trace.surfaceFlags & SURF_NOIMPACT) || trace.entityNum == ENTITYNUM_WORLD ? qtrue : qfalse;
        if(!blocked && trace.entityNum >= 0 && trace.entityNum < level.num_entities) {
            gentity_t *target = &g_entities[trace.entityNum];
            if(TceNavUseReady(actor, target, trace.fraction)) return trace.entityNum;
            if(TceNavOtherActivateStopsFallback(target)) return -1;
        }
        if(pass) break;
        trap_Trace(&trace, eye, NULL, NULL, end, actor->s.number, ordinary | CONTENTS_TRIGGER);
    }
    return -1;
}

static int TceNavDoorForwardSpeed(gentity_t *actor, int crouched, int walking) {
    if(!actor || !actor->inuse || !actor->client || actor->health <= 0) return -1;
    const playerState_t *ps = &actor->client->ps;
    tce_moveScale_t scale;
    memset(&scale, 0, sizeof(scale));
    scale.forward = 127; scale.speed = ps->speed;
    scale.runScale = ps->runSpeedScale; scale.sprintScale = ps->sprintSpeedScale;
    scale.crouchScale = ps->crouchSpeedScale;
    scale.ducked = crouched || (ps->pm_flags & PMF_DUCKED);
    scale.commandButtons = walking ? BUTTON_WALKING : 0;
    scale.moveButtons = walking ? BUTTON_WALKING : 0;
    scale.sprintTime = actor->client->pmext.sprintTime;
    scale.gametype = g_gametype.integer; scale.movespeed = g_movespeed.integer;
    scale.tactical = (ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) != 0; scale.lean = ps->leanf;
    scale.weaponWeight = ps->weapon >= 0 && ps->weapon < TCE_MAX_WEAPONS ? weaponDef[ps->weapon].loadoutWeight : 0;
    scale.carriedWeight = ps->holdable[9];
    const float speed = 127.f * TCE_PM_CmdScale(&scale);
    return speed > 0.f && speed < 1000.f ? (int)floorf(speed) : -1;
}

#endif
