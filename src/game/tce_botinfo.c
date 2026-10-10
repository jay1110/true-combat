#include "g_local.h"
#include "tce_botinfo.h"
#include "tce_nodes.h"
#include "tce_node_editor.h"
#include <ctype.h>
#include <stdio.h>

tceBotInfo_t tceBotInfo[64];
int tceNumBots;
static void BotWord(byte *p,int offset,short value){memcpy(p+offset,&value,2);}
static void BotDword(byte *p,int offset,int value){memcpy(p+offset,&value,4);}

/* ClearBotInfo2003e930 / Linux000761b4. Preserve untouched storage, especially
 * 0x438..0x1037 and0x113c..0x123b; original is not memset(sizeof(state)). */
void TCE_ClearBotInfo(int clientNum) {
    byte*p=(byte*)&tceBotInfo[clientNum];int i;
    static const int zeroWords[]={6,0x10,0x1a,0x2e,0x30,0x34,0x123c,0x123e};
    static const int minusWords[]={4,0x12,0x14,0x16,0x18,0x36};
    static const int noneWords[]={0x24,0x2c,0x32};
    static const int zeroDwords[]={0,8,0xc,0x1c,0x20,0x28,0x1038,0x1240,0x1244,0x1248,0x124c,0x1250,0x1254};
    for(i=0;i<sizeof(zeroWords)/sizeof(zeroWords[0]);i++)BotWord(p,zeroWords[i],0);
    for(i=0;i<sizeof(minusWords)/sizeof(minusWords[0]);i++)BotWord(p,minusWords[i],-1);
    for(i=0;i<sizeof(noneWords)/sizeof(noneWords[0]);i++)BotWord(p,noneWords[i],ENTITYNUM_NONE);
    for(i=0;i<sizeof(zeroDwords)/sizeof(zeroDwords[0]);i++)BotDword(p,zeroDwords[i],0);
    memset(p+0x103c,0xff,0x100);memset(p+0x38,0,0x400);
}
/* InitBotInfo2003ea40 / Linux000762f4. Count is supplied separately by InitGame. */
void TCE_InitBotInfo(void){int i;for(i=0;i<64;i++)TCE_ClearBotInfo(i);}
/* UpdateBotInfo2003ea60 / Linux00076590. Clear disconnected active TC slots;
 * never infer TC activity from SVF_BOT (the original does not). */
void TCE_UpdateBotInfo(void){int i;for(i=0;i<64;i++)if(tceBotInfo[i].active&&
    (!g_entities[i].client||g_entities[i].client->pers.connected!=CON_CONNECTED)){
    TCE_ClearBotInfo(i);tceNumBots--;
}}
/* ClientNumberFromNameMatch20054d00 / Linux000ae9cc. Empty queries do not match. */
int TCE_ClientNumberFromNameMatch(const char *name,int *matches){
    char query[32],candidate[32];int i,j,length,count=0;
    Q_strncpyz(query,name,sizeof(query));Q_CleanStr(query);length=Q_strlenInt(query);
    for(i=0;i<level.maxclients;i++)if(g_entities[i].client&&g_entities[i].client->pers.connected==CON_CONNECTED){
        Q_strncpyz(candidate,g_entities[i].client->pers.netname,sizeof(candidate));Q_CleanStr(candidate);
        for(j=0;candidate[j];j++)if(tolower((unsigned char)candidate[j])==tolower((unsigned char)query[0])&&
            !Q_stricmpn(query,candidate+j,length)){matches[count++]=i;break;}
    }
    return count;
}
/* Svcmd_Kickbot_f20089fe0 / Linux000f51d6. Reset is before deferred enginekick. */
void TCE_SvcmdKickBot(void){
    char name[MAX_STRING_CHARS];int matches[64],count,i;
    if(trap_Argc()!=2){G_Printf("Usage:  kickbot <name>\n");return;}
    trap_Argv(1,name,sizeof(name));count=TCE_ClientNumberFromNameMatch(name,matches);
    if(!count){G_Printf("No bots found with that name.\n");return;}
    for(i=0;i<count;i++)if(g_entities[matches[i]].r.svFlags&SVF_BOT){
        TCE_ClearBotInfo(matches[i]);
        trap_SendConsoleCommand(EXEC_APPEND,va("clientkick %d\n",matches[i]));tceNumBots--;
    }
}

/* BotTalk: Windows folded RET2001b750 / Linux0007731e. No voice side
 * effect exists in the shipped TC body. This alias is not a new completion. */
static void TCE_BotTalk(gentity_t *ent, const char *text, int team, int voiceonly) {
    (void)ent; (void)text; (void)team; (void)voiceonly;
}

/* BotParseServerCommand2003eab0 / Linux0007687a.
 * Follow requests update the TC follow target/time and state, not SDK bot AI. */
void TCE_BotParseServerCommand(gentity_t *ent, int *state) {
    char message[1024], voice[1024], *arguments;
    int voiceonly, sender, color;
    vec3_t delta;
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    while (trap_BotGetServerCommand(ent->client->ps.clientNum, message, sizeof(message))) {
        arguments = strchr(message, ' ');
        if (!arguments) continue;
        *arguments++ = 0;
        Q_CleanStr(arguments);
        if (!Q_stricmp(message, "cp") || !Q_stricmp(message, "cs") ||
            !Q_stricmp(message, "print") || !Q_stricmp(message, "chat") ||
            !Q_stricmp(message, "tchat")) continue;
        if (!Q_stricmp(message, "vtchat")) {
            if (ent->client->ps.pm_type == PM_DEAD) return;
            /* Original assumes a well-formed engine voice command and writes
             * an int into a byte local. Keep valid semantics without overflow. */
            if (sscanf(arguments, "%d %d %d %1023s", &voiceonly, &sender, &color, voice) != 4)
                continue;
            if (sender < 0 || sender >= MAX_GENTITIES) continue;
            if (Q_stricmp(voice, "followme") || *state == 2 || *state == 4 ||
                *state == 5 || *state == 6 || g_entities[sender].s.number == ent->s.number)
                continue;
            VectorSubtract(g_entities[sender].r.currentOrigin, ent->r.currentOrigin, delta);
            if (sqrt(((double)delta[0] * delta[0] + (double)delta[1] * delta[1]) +
                     (double)delta[2] * delta[2]) < 600.0) {
                TCE_BotTalk(ent, "affirmative", 1, 1);
                BotWord(info, 0x24, (short)sender);
                BotDword(info, 0x28, level.time);
                *state = 4;
            }
        } else if (Q_stricmp(message, "scores")) {
            Q_stricmp(message, "clientLevelShot");
        }
    }
}

/* BotSendCommand2003ed40 / Linux00076b18. The original copies all28 command
 * bytes into pers.cmd, submits to the engine, then invokes ClientThink_real.
 * The command argument substitutes only Botthink's frame-local pointer slot. */
extern void ClientThink_real(gentity_t *ent);
void TCE_BotSendCommand(gentity_t *ent, int *state, usercmd_t *command) {
    int i;
    BotWord((byte *)&tceBotInfo[ent->s.number], 4, (short)*state);
    command->serverTime = level.time;
    for (i = 0; i < 3; ++i) {
        /* PE200ac480 is the float182.0444488525390625, not exact65536/360. */
        command->angles[i] = (int)((double)ent->s.angles[i] *
                                  (double)182.0444488525390625f) & 65535;
    }
    ent->client->pers.cmd = *command;
    trap_BotUserCommand(ent->client->ps.clientNum, command);
    ClientThink_real(ent);
}

/* BotHandleEvents2003f2c0 / Linux000770e6. Native event enums map the two
 * observed original IDs53(global sound)/70(obituary); TC state stride remains
 * 0x1258, cache starts at0x38 and is indexed by the event entity number. */
void TCE_BotHandleEvents(gentity_t *ent, gentity_t *eventEnt) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int previous, event, number = eventEnt->s.number, actor;
    double roll;
    char sound[1024];
    qboolean ownTeam;
    memcpy(&previous, info + 0x38 + number * 4, 4);
    if (previous == g_entities[number].eventTime) return;
    BotDword(info, 0x38 + number * 4, g_entities[number].eventTime);
    event = eventEnt->s.eType > ET_EVENTS ?
        eventEnt->s.eType - ET_EVENTS : eventEnt->s.event;
    event &= ~EV_EVENT_BITS;
    if (event == EV_GLOBAL_SOUND) {
        trap_GetConfigstring(CS_SOUNDS + eventEnt->s.eventParm, sound, sizeof(sound));
        if (!Q_stricmp(sound, "sound/multiplayer/allies/a-dynamite_planted.wav"))
            ownTeam = ent->client->sess.sessionTeam == TEAM_ALLIES;
        else if (!Q_stricmp(sound, "sound/multiplayer/axis/g-dynamite_planted.wav"))
            ownTeam = ent->client->sess.sessionTeam == TEAM_AXIS;
        else return;
        roll = (double)(rand() & 0x7fff) * (double)3.0518509447574615e-05f;
        if (ownTeam) {
            if (roll >= 0.1) return;
            TCE_BotTalk(ent, "cheer", 0, 1);
        } else {
            if (roll >= 0.05) return;
            TCE_BotTalk(ent, "disarmdynamite", 1, 1);
        }
    } else if (event == EV_OBITUARY) {
        actor = eventEnt->s.otherEntityNum;
        if (eventEnt->s.eventParm != MOD_SUICIDE &&
            eventEnt->s.otherEntityNum2 != actor) return;
        if (actor == ent->s.number || OnSameTeam(ent, &g_entities[actor])) return;
        roll = (double)(rand() & 0x7fff) * (double)3.0518509447574615e-05f;
        if (roll >= 0.1) return;
        TCE_BotTalk(ent, "greatshot", 0, 1);
    }
}

/* BotEvents2003f280 / Linux0007709a. Use live level.num_entities each
 * iteration, matching the original, rather than caching the limit. */
void TCE_BotEvents(gentity_t *ent) {
    int i;
    for (i = 0; i < level.num_entities; ++i)
        TCE_BotHandleEvents(ent, &g_entities[i]);
}

/* Shared TC record access: unaligned short/int fields are preserved exactly. */
static short TCE_BotReadShort(const byte *info, int offset) {
    short value; memcpy(&value, info + offset, sizeof(value)); return value;
}
static unsigned int TCE_BotReadUInt(const byte *info, int offset) {
    unsigned int value; memcpy(&value, info + offset, sizeof(value)); return value;
}

/* BotMove2003f8e0 / Linux000777e8. Commands wrap at byte width as in
 * the original; repeated requests are additive, not clamped or replaced. */
void TCE_BotMove(gentity_t *ent, usercmd_t *command, int direction, unsigned int flags) {
    byte *forward = (byte *)&command->forwardmove;
    byte *right = (byte *)&command->rightmove;
    byte *up = (byte *)&command->upmove;
    switch (direction) {
    case 1: *forward = (byte)(*forward + 127); break;
    case 2: *right = (byte)(*right + 127); break;
    case 3: *forward = (byte)(*forward - 127); break;
    case 4: *right = (byte)(*right - 127); break;
    }
    if ((flags & 1) && ent->waterlevel < 2) *up = (byte)(*up - 127);
    if ((flags & 2) && ent->client->ps.groundEntityNum != ENTITYNUM_NONE)
        *up = (byte)(*up + 127);
    if (flags & 4) command->buttons |= BUTTON_WALKING;
    if (flags & 8) command->buttons |= BUTTON_SPRINT;
}

/* BotFindFreeDirection2003fdf0 / Linux00077da6. Point traces, exact order
 * forward/right/left/back, not a swept player hull or shortest-route search. */
int TCE_BotFindFreeDirection(gentity_t *ent) {
    vec3_t forward, right, start, end;
    trace_t trace;
    int pass, axis;
    static const int result[4] = {1, 2, 4, 3};
    AngleVectors(ent->s.angles, forward, right, NULL);
    VectorCopy(ent->r.currentOrigin, start);
    for (pass = 0; pass < 4; ++pass) {
        for (axis = 0; axis < 3; ++axis) {
            float component = (pass == 0 || pass == 3) ? forward[axis] : right[axis];
            if (pass >= 2) component = -component;
            end[axis] = (float)((double)component * 300.0 + start[axis]);
        }
        trap_Trace(&trace, start, NULL, NULL, end, ent->s.number, 0x2010001);
        if (trace.fraction >= 1.0f) return result[pass];
    }
    return 0;
}

/* BotFollowPlayer2003fd00 / Linux00077caa. Invalid/nonclient targets are
 * cleared as a defined extension of the original valid-target contract. */
void TCE_BotFollowPlayer(gentity_t *ent, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int number = TCE_BotReadShort(info, 0x24);
    gentity_t *target;
    vec3_t delta;
    float distance;
    target = number >= 0 && number < MAX_GENTITIES ? &g_entities[number] : NULL;
    if (!target || !target->client || target->client->ps.pm_type == PM_DEAD ||
        target->client->pers.connected == CON_DISCONNECTED) {
        BotWord(info, 0x24, ENTITYNUM_NONE);
        BotDword(info, 0x28, 0);
        return;
    }
    VectorSubtract(target->r.currentOrigin, ent->r.currentOrigin, delta);
    distance = VectorNormalize(delta);
    vectoangles(delta, ent->s.angles);
    if (distance > 140.0f) TCE_BotMove(ent, command, 1, 0);
}

/* HandleFollowState20042d50 / Linux0007ba7c. Expiry comparison is unsigned
 * and strictly later than timestamp+15000, including its unsigned wrap. */
void TCE_HandleFollowState(gentity_t *ent, int *state, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    if (TCE_BotReadShort(info, 0x24) == ENTITYNUM_NONE) {
        *state = 0;
        return;
    }
    if (TCE_BotReadUInt(info, 0x28) + 15000U < (unsigned int)level.time) {
        BotWord(info, 0x24, ENTITYNUM_NONE);
        BotDword(info, 0x28, 0);
        *state = 0;
        return;
    }
    TCE_BotFollowPlayer(ent, command);
}

/* ChooseWeapon is an original no-op: Windows folded2001b750,
 * Linux000760c8. Do not substitute an SDK weapon chooser. */
static void TCE_BotChooseWeapon(gentity_t *ent, int weapon) {
    (void)ent; (void)weapon;
}

/* BotRevivePlayer2003f120 / Linux00076f38. Aim at target origin+mins[2],
 * not the eye or a guessed chest point; original PS offset0x3e4 is mins[2]. */
void TCE_BotRevivePlayer(gentity_t *ent, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int number = TCE_BotReadShort(info, 0x32);
    gentity_t *target = number >= 0 && number < MAX_GENTITIES ? &g_entities[number] : NULL;
    vec3_t delta;
    float distance;
    if (!target || !target->client || target->client->ps.pm_type != PM_DEAD ||
        target->client->pers.connected == CON_DISCONNECTED) {
        BotWord(info, 0x32, ENTITYNUM_NONE);
        TCE_BotChooseWeapon(ent, 0);
        return;
    }
    VectorSubtract(target->r.currentOrigin, ent->r.currentOrigin, delta);
    delta[2] = (float)(((double)target->r.currentOrigin[2] + target->client->ps.mins[2]) -
                       ent->r.currentOrigin[2]);
    distance = (float)sqrt(((double)delta[0] * delta[0] +
                           (double)delta[1] * delta[1]) + (double)delta[2] * delta[2]);
    vectoangles(delta, ent->s.angles);
    if (distance < 48.0f) {
        TCE_BotMove(ent, command, 0, 1);
        command->buttons |= BUTTON_ATTACK;
    } else if (distance < 100.0f) {
        TCE_BotMove(ent, command, 1, 1);
    } else {
        TCE_BotMove(ent, command, 1, 0);
    }
}

/* HandleReviveState20042ee0 / Linux0007bc4e. This gate does not reset the
 * state when the nested revive controller discovers a disconnected target. */
void TCE_HandleReviveState(gentity_t *ent, int *state, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int number = TCE_BotReadShort(info, 0x32);
    if (number != ENTITYNUM_NONE && number >= 0 && number < MAX_GENTITIES &&
        g_entities[number].client && g_entities[number].client->ps.pm_type == PM_DEAD) {
        TCE_BotRevivePlayer(ent, command);
        return;
    }
    BotWord(info, 0x32, ENTITYNUM_NONE);
    *state = 0;
}

/* BotNeedAmmo2003eee0 / Linux00076d14. The original scans bank3
 * (TC knife bank), including its zero entries. Do not replace it by bank1.
 * Reserve AND clip use BG_FindAmmoForWeapon's same index, as shipped. */
qboolean TCE_BotNeedAmmo(gentity_t *ent) {
    int i, weapon, ammo;
    for (i = 0; i < MAX_WEAPS_IN_BANK_MP; ++i) {
        weapon = weapBanksMultiPlayer[3][i];
        if (COM_BitCheck(ent->client->ps.weapons, weapon)) break;
    }
    if (i == MAX_WEAPS_IN_BANK_MP) return qtrue;
    ammo = BG_FindAmmoForWeapon(weapon);
    if (ent->client->ps.ammo[ammo] < 1 && ent->client->ps.ammoclip[ammo] < 1)
        return qtrue;
    if (ent->client->ps.weapon == weapon) return qfalse;
    ammo = BG_FindAmmoForWeapon(ent->client->sess.sessionTeam == TEAM_AXIS ? 2 : 7);
    return ent->client->ps.ammoclip[ammo] < 1;
}

/* BotCheckAmmoState2003ee70 / Linux00076c76. The original notification
 * and chooser are no-ops, but the item-stage/state reset is real. */
void TCE_BotCheckAmmoState(gentity_t *ent, int *state) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    if (TCE_BotNeedAmmo(ent)) {
        TCE_BotTalk(ent, "needammo", 1, 1);
        return;
    }
    if (TCE_BotReadShort(info, 0x2e) != 0) {
        BotWord(info, 0x2e, 0);
        *state = 0;
        TCE_BotChooseWeapon(ent, 0);
    }
}

/* BotGiveItemToPlayer2003ef90 / Linux00076dd8. Delivery waits three
 * calls after its attack command; it does not invent an ammo-item spawn. */
void TCE_BotGiveItemToPlayer(gentity_t *ent, int *state, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    short stage = TCE_BotReadShort(info, 0x34);
    int number;
    gentity_t *target;
    vec3_t delta;
    float distance;
    if (stage > 0 && stage < 4) {
        BotWord(info, 0x34, stage + 1);
        return;
    }
    if (stage != 4) {
        number = TCE_BotReadShort(info, 0x2c);
        target = number >= 0 && number < MAX_GENTITIES ? &g_entities[number] : NULL;
        if (target && target->client && target->client->ps.pm_type != PM_DEAD &&
            target->client->pers.connected != CON_DISCONNECTED) {
            VectorSubtract(target->r.currentOrigin, ent->r.currentOrigin, delta);
            distance = (float)sqrt(((double)delta[0] * delta[0] +
                (double)delta[1] * delta[1]) + (double)delta[2] * delta[2]);
            vectoangles(delta, ent->s.angles);
            if (distance >= 120.0f) {
                TCE_BotMove(ent, command, 1, 0);
                return;
            }
            command->buttons |= BUTTON_ATTACK;
            BotWord(info, 0x34, 1);
            return;
        }
    }
    BotWord(info, 0x2c, ENTITYNUM_NONE);
    BotWord(info, 0x34, 0);
    *state = 0;
}

/* HandleGiveItemState20042dd0 / Linux0007bb0a. The self-supply path
 * chooses a free facing before issuing three attack ticks, then moves. */
void TCE_HandleGiveItemState(gentity_t *ent, int *state, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int target = TCE_BotReadShort(info, 0x2c);
    short stage;
    int direction;
    if (target == ENTITYNUM_NONE) {
        *state = 0;
        return;
    }
    if (target != ent->s.number) {
        TCE_BotChooseWeapon(ent, 19);
        TCE_BotGiveItemToPlayer(ent, state, command);
        return;
    }
    stage = TCE_BotReadShort(info, 0x2e);
    if (stage != 0) {
        if (stage > 3) {
            TCE_BotMove(ent, command, 1, 0);
            return;
        }
        command->buttons |= BUTTON_ATTACK;
        BotWord(info, 0x2e, (short)(stage + 1));
        return;
    }
    direction = TCE_BotFindFreeDirection(ent);
    if (direction == 2) ent->s.angles[YAW] -= 90.0f;
    else if (direction == 3) ent->s.angles[YAW] -= 180.0f;
    else if (direction == 4) ent->s.angles[YAW] += 90.0f;
    else if (direction == 0) return;
    TCE_BotChooseWeapon(ent, 19);
    BotWord(info, 0x2e, 1);
}

/* HandleDeathState200424b0 / Linux0007b16c. Selective state reset:
 * navigation history, event cache, active flag and command pointer survive. */
void TCE_HandleDeathState(gentity_t *ent, int *state) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    static const int noneWords[] = {0x2c, 0x24, 0x32};
    static const int zeroWords[] = {0x2e, 0x10, 0x30, 0x34, 0x123c, 0x123e};
    static const int zeroDwords[] = {0x28, 0x0c, 0x1038, 0x1240, 0x1244};
    int i;
    ent->client->buttons = 0;
    ent->client->wbuttons = 0;
    *state = -1;
    ent->enemy = NULL;
    ent->client->ps.persistant[PERS_ATTACKER] = -1;
    for (i = 0; i < sizeof(noneWords)/sizeof(noneWords[0]); ++i)
        BotWord(info, noneWords[i], ENTITYNUM_NONE);
    for (i = 0; i < sizeof(zeroWords)/sizeof(zeroWords[0]); ++i)
        BotWord(info, zeroWords[i], 0);
    for (i = 0; i < sizeof(zeroDwords)/sizeof(zeroDwords[0]); ++i)
        BotDword(info, zeroDwords[i], 0);
    ent->s.angles[PITCH] = 0;
    ent->s.angles[YAW] = ent->client->ps.viewangles[YAW];
    ent->s.angles[ROLL] = 0;
    VectorCopy(ent->s.angles, ent->client->ps.viewangles);
}

/* FaceEnemy2003e600 / Linux00075d90. Skill0/1/2 consumes exactly three
 * random draws; other values have no spread. TC aiming flag is stats[8]&4. */
void TCE_FaceEnemy(gentity_t *ent) {
    vec3_t point, delta;
    short skill = tceBotInfo[ent->s.number].skill;
    int axis;
    double spread, jitter;
    if (!ent->enemy || !ent->enemy->client) return;
    VectorCopy(ent->enemy->r.currentOrigin, point);
    if (ent->enemy->client->ps.eFlags & EF_PRONE) point[2] -= 56.0f;
    else if (ent->enemy->client->ps.pm_flags & PMF_DUCKED) point[2] -= 24.0f;
    if (skill >= 0 && skill <= 2) {
        for (axis = 0; axis < 3; ++axis) {
            spread = skill == 2 ? 12.0 : 30.0;
            if (axis == 2) {
                if (skill == 1) spread = 15.0;
                else if (skill == 2) spread = 4.0;
            }
            jitter = (double)(rand() & 0x7fff) * (double)3.0518509447574615e-05f - 0.5;
            point[axis] = (float)((jitter + jitter) * spread + point[axis]);
        }
    }
    VectorSubtract(point, ent->r.currentOrigin, delta);
    vectoangles(delta, ent->s.angles);
    ent->client->ps.stats[STAT_TCE_WEAPON_FLAGS] |= 4;
}

/* AttackEnemy2003e530 / Linux00075cbe. Real ammo/clip mapping, retained
 * original no-op ChooseWeapon, reload and attack are separate button bytes. */
void TCE_AttackEnemy(gentity_t *ent, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int clip;
    if (!ent->enemy) return;
    BotDword(info, 0x0c, 0);
    BotWord(info, 0x10, 0);
    BotWord(info, 0x30, 0);
    TCE_FaceEnemy(ent);
    TCE_BotChooseWeapon(ent, 0);
    clip = BG_FindClipForWeapon(ent->client->ps.weapon);
    if (ent->client->ps.ammoclip[clip] < 1)
        command->wbuttons |= WBUTTON_RELOAD;
    else
        command->buttons |= BUTTON_ATTACK;
}

/* MyVisible20044280 / Linux0007d2ea. Both endpoints use actual eye heights;
 * success is the target entity trace hit, not merely fraction==1. */
qboolean TCE_MyVisible(gentity_t *ent, gentity_t *target) {
    vec3_t start, end;
    trace_t trace;
    if (!ent->client || !target || !target->client) return qfalse;
    VectorCopy(ent->r.currentOrigin, start);
    VectorCopy(target->r.currentOrigin, end);
    start[2] = (float)((double)start[2] + ent->client->ps.viewheight);
    end[2] = (float)((double)end[2] + target->client->ps.viewheight);
    trap_Trace(&trace, start, NULL, NULL, end, ent->s.number, 0x6000081);
    return trace.entityNum >= 0 && trace.entityNum < MAX_GENTITIES &&
        &g_entities[trace.entityNum] == target;
}

/* CanMove2003f590 / Linux000773c8. Player hull's lower Z is explicitly
 * zero; route flag1 substitutes crouchMaxZ. Results0/1/2/3 mean blocked,
 * clear, player obstruction, rotating-door obstruction respectively. */
int TCE_CanMove(gentity_t *ent, int direction) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    vec3_t angles, forward, start, end, mins, maxs, unusedForward;
    trace_t trace;
    gentity_t *obstacle;
    int axis;
    VectorCopy(ent->s.angles, angles);
    if (direction == 4) angles[YAW] += 90.0f;
    else if (direction == 2) angles[YAW] -= 90.0f;
    else if (direction == 3) angles[YAW] -= 180.0f;
    AngleVectors(angles, forward, NULL, NULL);
    VectorCopy(ent->r.currentOrigin, start);
    for (axis = 0; axis < 3; ++axis)
        end[axis] = (float)((double)forward[axis] * 32.0 + start[axis]);
    VectorCopy(ent->client->ps.mins, mins);
    VectorCopy(ent->client->ps.maxs, maxs);
    mins[2] = 0;
    if (TCE_BotReadShort(info, 0x123e) & 1) maxs[2] = ent->client->ps.crouchMaxZ;
    trap_Trace(&trace, start, mins, maxs, end, ent->s.number, 0x2010001);
    if (trace.fraction != 1.0f) {
        if (trace.entityNum >= 0 && trace.entityNum < MAX_CLIENTS) {
            AngleVectors(g_entities[trace.entityNum].s.angles, unusedForward, NULL, NULL);
            BotDword(info, 0x1254, TCE_BotReadUInt(info, 0x1254) + 1U);
            return 2;
        }
        obstacle = trace.entityNum >= 0 && trace.entityNum < MAX_GENTITIES ?
            &g_entities[trace.entityNum] : NULL;
        if (obstacle && obstacle->classname &&
            !Q_stricmp(obstacle->classname, "func_door_rotating")) {
            if (obstacle->moverState == MOVER_POS1 || obstacle->moverState == MOVER_POS1ROTATE)
                G_TryDoor(obstacle, ent, ent);
            BotDword(info, 0x1254, TCE_BotReadUInt(info, 0x1254) + 1U);
            return 3;
        }
        angles[PITCH] = 0;
        AngleVectors(angles, forward, NULL, NULL);
        for (axis = 0; axis < 3; ++axis)
            end[axis] = (float)((double)forward[axis] * 32.0 + start[axis]);
        trap_Trace(&trace, start, mins, maxs, end, ent->s.number, 0x2010001);
        if (trace.fraction != 1.0f) {
            BotDword(info, 0x1254, TCE_BotReadUInt(info, 0x1254) + 1U);
            return 0;
        }
    }
    BotDword(info, 0x1254, 0);
    return 1;
}

/* HandleAttackState20042660 / Linux0007b2e0. Losing sight starts a
 * four-second crouched strafe/search window, not immediate enemy deletion. */
void TCE_HandleAttackState(gentity_t *ent, int *state, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int direction;
    unsigned int began;
    if (!ent->enemy || !ent->enemy->client ||
        ent->enemy->client->ps.stats[STAT_HEALTH] < 1) {
        *state = 0;
        ent->enemy = NULL;
        ent->client->ps.persistant[PERS_ATTACKER] = -1;
        TCE_BotChooseWeapon(ent, 0);
    }
    if (!ent->enemy) return;
    if (TCE_MyVisible(ent, ent->enemy)) {
        TCE_AttackEnemy(ent, command);
        return;
    }
    began = TCE_BotReadUInt(info, 0x0c);
    if (began == 0) {
        BotDword(info, 0x0c, level.time);
        direction = (double)(rand() & 0x7fff) *
            (double)3.0518509447574615e-05f > 0.5 ? 4 : 2;
        BotWord(info, 0x10, (short)direction);
        TCE_FaceEnemy(ent);
        return;
    }
    if ((unsigned int)level.time < began + 4000U) {
        TCE_FaceEnemy(ent);
        direction = TCE_BotReadShort(info, 0x10);
        if (TCE_CanMove(ent, direction) == 1) {
            TCE_BotMove(ent, command, direction, 1);
            return;
        }
        TCE_BotMove(ent, command, 0, 1);
        BotWord(info, 0x10, direction == 4 ? 2 : 4);
        return;
    }
    BotDword(info, 0x0c, 0);
    BotWord(info, 0x10, 0);
    ent->enemy = NULL;
    ent->client->ps.persistant[PERS_ATTACKER] = -1;
    *state = 0;
}

/* G_BotViewProb200441c0 / Linux0007d114. Original x87 return is retained
 * through the caller's probability multiplication, so expose double here. */
double TCE_BotViewProb(gentity_t *ent, gentity_t *target) {
    vec3_t forward, delta;
    double distance, product;
    float dot;
    AngleVectors(ent->s.angles, forward, NULL, NULL);
    VectorSubtract(target->r.currentOrigin, ent->r.currentOrigin, delta);
    distance = sqrt(((double)delta[0] * delta[0] +
                     (double)delta[1] * delta[1]) + (double)delta[2] * delta[2]);
    VectorNormalize(delta);
    /* Windows20044218: Z+Y+X; store dot to float before final multiply. */
    product = ((double)forward[2] * delta[2] +
               (double)forward[1] * delta[1]) + (double)forward[0] * delta[0];
    dot = (float)product;
    if (product < 0.0) dot = 0.0f;
    if (distance < 1024.0) distance = 1024.0;
    return (1024.0 / distance) * dot;
}

/* LocateEnemy2003e280 / Linux000759e8. Retaliation delay and probabilistic
 * visible-nearest selection are distinct paths in the original controller. */
qboolean TCE_LocateEnemy(gentity_t *ent) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int delay, attacker, i;
    short reaction;
    gentity_t *candidate, *best = NULL;
    float closest = -1.0f, probability;
    vec3_t delta;
    double distance, draw;
    switch (tceBotInfo[ent->s.number].skill) {
    case 0: delay = 700; break;
    case 2: delay = 200; break;
    case 3: delay = 50; break;
    default: delay = 450; break;
    }
    attacker = ent->client->ps.persistant[PERS_ATTACKER];
    if (attacker >= 0 && attacker < MAX_GENTITIES &&
        g_entities[attacker].client && attacker != ent->s.number &&
        !OnSameTeam(ent, &g_entities[attacker])) {
        reaction = TCE_BotReadShort(info, 0x30);
        if (reaction * 50 >= delay) {
            ent->enemy = &g_entities[attacker];
            BotWord(info, 0x30, 0);
            return qtrue;
        }
        BotWord(info, 0x30, (short)(reaction + 1));
    }
    for (i = 0; i < level.maxclients; ++i) {
        candidate = &g_entities[i];
        if (!candidate->inuse || !candidate->client || candidate == ent) continue;
        if (candidate->client->sess.sessionTeam == ent->client->sess.sessionTeam ||
            candidate->client->tceDamageSpawnTime + 7000 > level.time ||
            ent->client->tceDamageSpawnTime + 1000 > level.time ||
            candidate->health <= 0 || (candidate->client->ps.pm_flags & PMF_LIMBO) ||
            candidate->client->sess.sessionTeam == TEAM_SPECTATOR) continue;
        if (!TCE_MyVisible(ent, candidate)) continue;
        probability = (float)((double)g_botvar.value *
            TCE_BotViewProb(ent, candidate) * (double)0.1f);
        if (ent->enemy == candidate && probability != 0.0f) probability = 1.1f;
        draw = (double)(rand() & 0x7fff) * (double)3.0518509447574615e-05f;
        if (draw <= probability) {
            VectorSubtract(ent->r.currentOrigin, candidate->r.currentOrigin, delta);
            distance = sqrt(((double)delta[0] * delta[0] +
                             (double)delta[1] * delta[1]) + (double)delta[2] * delta[2]);
            if (distance < closest || closest == -1.0f) {
                closest = (float)distance;
                best = candidate;
            }
        }
    }
    if (!best) return qfalse;
    ent->enemy = best;
    return qtrue;
}

/* BotFindClosestNode20040310 / Linux0007841e. This entry uses a16-unit
 * cube sweep and excludes nodes <=16 units away. Do not substitute the
 * already ported point-based node-editor query, which has different gates. */
short TCE_BotFindClosestNode(gentity_t *ent) {
    const vec3_t mins = {-16.0f, -16.0f, -16.0f};
    const vec3_t maxs = {16.0f, 16.0f, 16.0f};
    vec3_t delta;
    trace_t trace;
    float nearest = -1.0f, rounded;
    double distance;
    short result = -1;
    int i;
    for (i = 0; i < tceNumNodes; ++i) {
        unsigned short flags = (unsigned short)tceNodes[i].flags;
        if (flags == 0xffff ||
            (ent->client->sess.sessionTeam == TEAM_AXIS && (flags & 0x10)) ||
            (ent->client->sess.sessionTeam == TEAM_ALLIES && (flags & 0x20))) continue;
        VectorSubtract(tceNodes[i].origin, ent->r.currentOrigin, delta);
        distance = sqrt(((double)delta[0] * delta[0] +
                         (double)delta[1] * delta[1]) + (double)delta[2] * delta[2]);
        VectorNormalize(delta);
        if (distance <= 16.0) continue;
        trap_Trace(&trace, ent->r.currentOrigin, mins, maxs, tceNodes[i].origin,
                   ent->s.number, 0x10001);
        rounded = (float)distance;
        if (trace.fraction == 1.0f && (rounded < nearest || nearest == -1.0f)) {
            result = (short)i;
            nearest = rounded;
        }
    }
    return result;
}

/* BotMoveNearerNode2003fa10 / Linux00077914. */
qboolean TCE_BotMoveNearerNode(gentity_t *ent, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int node = TCE_BotReadShort(info, 0x16);
    vec3_t delta;
    double distance;
    if (node < 0 || node >= TCE_MAX_NODES || TCE_BotReadUInt(info, 0x1244) == 1) return qfalse;
    VectorSubtract(tceNodes[node].origin, ent->r.currentOrigin, delta);
    delta[2] = 0.0f;
    distance = sqrt((double)delta[0] * delta[0] + (double)delta[1] * delta[1]);
    if (distance > 32.0) return qfalse;
    if (distance < 5.0) {
        BotDword(info, 0x1244, 1);
        return qfalse;
    }
    vectoangles(delta, ent->s.angles);
    TCE_BotMove(ent, command, 1, 1);
    return qtrue;
}

/* BotSpecialMove2003fb00 / Linux00077a28. Flag4 is PMF_LADDER,
 * not PMF_JUMP_HELD. Preserve separate stuck and special-attempt counters. */
qboolean TCE_BotSpecialMove(gentity_t *ent, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    vec3_t forward, start, end, crouchMaxs;
    trace_t trace;
    int i, node;
    AngleVectors(ent->s.angles, forward, NULL, NULL);
    VectorCopy(ent->r.currentOrigin, start);
    for (i = 0; i < 3; ++i) end[i] = (float)((double)forward[i] * 36.0 + start[i]);
    VectorCopy(ent->client->ps.maxs, crouchMaxs);
    crouchMaxs[2] = ent->client->ps.crouchMaxZ;
    if (!(ent->client->ps.pm_flags & PMF_DUCKED)) {
        trap_Trace(&trace, start, ent->client->ps.mins, crouchMaxs, end, ent->s.number, 0x2010001);
        if (trace.fraction == 1.0f) {
            TCE_BotMove(ent, command, 1, 1);
            return qtrue;
        }
    }
    if (ent->client->ps.pm_flags & PMF_LADDER) {
        node = TCE_BotReadShort(info, 0x18);
        if (node < 0 || node >= TCE_MAX_NODES) goto failed;
        trap_Trace(&trace, start, ent->client->ps.mins, ent->client->ps.maxs,
                   tceNodes[node].origin, ent->s.number, 0x2000000);
        if (trace.fraction != 1.0f) {
            BotDword(info, 0x124c, TCE_BotReadUInt(info, 0x124c) + 1);
            goto failed;
        }
    }
    if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
        BotDword(info, 0x124c, TCE_BotReadUInt(info, 0x124c) + 1);
        BotDword(info, 0x1254, TCE_BotReadUInt(info, 0x1254) + 1);
        TCE_BotMove(ent, command, 1, 2);
        return qtrue;
    }
failed:
    BotDword(info, 0x1254, TCE_BotReadUInt(info, 0x1254) + 1);
    return qfalse;
}

/* G_LadderNodeOccupied2003f500 / Linux00077320. Active TC records, not
 * SDK bot navigation state or entity visibility, arbitrate ladder occupancy. */
qboolean TCE_LadderNodeOccupied(gentity_t *ent) {
    int next = TCE_BotReadShort((byte *)&tceBotInfo[ent->s.number], 0x18);
    int i, current;
    const byte *other;
    if (next < 0 || next >= TCE_MAX_NODES || !(tceNodes[next].flags & 0x400)) return qfalse;
    for (i = 0; i < 64; ++i) {
        if (!tceBotInfo[i].active || i == ent->s.number) continue;
        other = (const byte *)&tceBotInfo[i];
        current = TCE_BotReadShort(other, 0x16);
        if ((TCE_BotReadShort(other, 0x18) == next && current >= 0 && current < TCE_MAX_NODES &&
             (tceNodes[current].flags & 0x400)) || current == next) return qtrue;
    }
    return qfalse;
}

/* BotGetNextNode20040480 / Linux00078618. Wander chooses eligible forward
 * links; other states pop the original short path stack backwards. */
short TCE_BotGetNextNode(gentity_t *ent, int *state) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    short candidates[TCE_MAX_NODE_LINKS], count, result;
    int current, target, i, total = 0, selection;
    tceNode_t *node, *next;
    vec3_t delta, forward;
    double dot;
    if (*state == 3) {
        current = TCE_BotReadShort(info, 0x16);
        if (current >= 0 && current < TCE_MAX_NODES) {
            node = &tceNodes[current];
            if (node->flags != -1 && node->number != -1) {
                for (i = 0; i < node->numLinks && i < TCE_MAX_NODE_LINKS; ++i) {
                    target = node->links[i].target;
                    if (target < 0 || target >= TCE_MAX_NODES) continue;
                    next = &tceNodes[target];
                    if (next->flags == -1 || next->number == -1 ||
                        ((next->flags & 0x20) && ent->client->sess.sessionTeam == TEAM_ALLIES) ||
                        ((next->flags & 0x10) && ent->client->sess.sessionTeam == TEAM_AXIS) ||
                        (node->links[i].flags & 0x10) || next->number == TCE_BotReadShort(info, 0x14) ||
                        next->number == TCE_BotReadShort(info, 0x12)) continue;
                    VectorSubtract(next->origin, ent->r.currentOrigin, delta);
                    AngleVectors(ent->s.angles, forward, NULL, NULL);
                    dot = ((double)forward[2] * delta[2] + (double)forward[1] * delta[1]) +
                          (double)forward[0] * delta[0];
                    if (dot > 0.0) candidates[total++] = next->number;
                }
                if (total > 0) {
                    /* Windows20040624..43: reciprocal float, count, then
                     * -0.99f before ftol; negate index, no rand()%count. */
                    selection = -(int)(((double)(rand() & 0x7fff) *
                        (double)3.0518509447574615e-05f) * total * (double)-0.99f);
                    return candidates[selection];
                }
            }
        }
        return TCE_BotFindClosestNode(ent);
    }
    if (TCE_BotReadShort(info, 0x36) == -1) {
        *state = 0;
        return -1;
    }
    count = TCE_BotReadShort(info, 0x123c);
    if (count <= 0 || count > 256) {
        *state = 0;
        BotWord(info, 0x36, -1);
        return -1;
    }
    result = TCE_BotReadShort(info, 0x103a + count * 2);
    BotWord(info, 0x123c, (short)(count - 1));
    return result;
}

/* Node-action callbacks20043b60/20043e80 share the original entity search
 * and facing tail. The cached short at node+0x20 is an entity number. */
static gentity_t *TCE_BotNodeEntity(gentity_t *ent, tceNode_t *node, const char *classname) {
    int list[MAX_GENTITIES], count, i;
    vec3_t mins, maxs;
    gentity_t *target;
    if (node->moverEntity > 64 && node->moverEntity < ENTITYNUM_NONE)
        return &g_entities[node->moverEntity];
    VectorSet(mins, ent->r.currentOrigin[0] - 300.0f,
              ent->r.currentOrigin[1] - 300.0f, ent->r.currentOrigin[2] - 20.0f);
    VectorSet(maxs, ent->r.currentOrigin[0] + 300.0f,
              ent->r.currentOrigin[1] + 300.0f, ent->r.currentOrigin[2] + 20.0f);
    count = trap_EntitiesInBox(mins, maxs, list, MAX_GENTITIES);
    for (i = 0; i < count; ++i) {
        if (list[i] < 0 || list[i] >= MAX_GENTITIES) continue;
        target = &g_entities[list[i]];
        if (target->classname && !Q_stricmp(target->classname, classname)) {
            node->moverEntity = (short)target->s.number;
            return target;
        }
    }
    return NULL;
}

static qboolean TCE_BotApproachNodeEntity(gentity_t *ent, gentity_t *target,
                                         usercmd_t *command, const char *kind) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    vec3_t origin, delta;
    VectorCopy(target->r.currentOrigin, origin);
    if (origin[0] == 0.0f && origin[1] == 0.0f && origin[2] == 0.0f) {
        VectorCopy(target->s.origin, origin);
        if (origin[0] == 0.0f && origin[1] == 0.0f && origin[2] == 0.0f) {
            G_Printf("Error: %s at node %d does not appear to have a valid origin!\n",
                     kind, (int)TCE_BotReadShort(info, 0x18));
            return qtrue;
        }
    }
    VectorSubtract(origin, ent->r.currentOrigin, delta);
    vectoangles(delta, ent->s.angles);
    if (TCE_BotReadShort(info, 0x1a) > 1) TCE_BotMove(ent, command, 1, 0);
    BotDword(info, 0x1c, 1);
    return qfalse;
}

/* BotDoStealObjective20043b60 / Linux0007c9c0. */
qboolean TCE_BotDoStealObjective(gentity_t *ent, usercmd_t *command) {
    int next = TCE_BotReadShort((byte *)&tceBotInfo[ent->s.number], 0x18);
    gentity_t *target;
    if (ent->client->ps.powerups[PW_REDFLAG] || ent->client->ps.powerups[PW_BLUEFLAG]) return qtrue;
    if (next < 0 || next >= TCE_MAX_NODES) return qtrue;
    target = TCE_BotNodeEntity(ent, &tceNodes[next],
        ent->client->sess.sessionTeam == TEAM_AXIS ? "team_CTF_blueflag" : "team_CTF_redflag");
    if (!target) {
        G_Printf("Error: objective not found at node %d\n", next);
        return qtrue;
    }
    return TCE_BotApproachNodeEntity(ent, target, command, "objective");
}

/* BotDoFlag20043e80 / Linux0007cd56. Original entity+0x2e4 here is
 * checkpoint ownership; its real native producer checkpoint_touch uses count. */
qboolean TCE_BotDoFlag(gentity_t *ent, usercmd_t *command) {
    int next = TCE_BotReadShort((byte *)&tceBotInfo[ent->s.number], 0x18);
    int team = ent->client->sess.sessionTeam;
    gentity_t *target;
    if (next < 0 || next >= TCE_MAX_NODES) return qtrue;
    target = TCE_BotNodeEntity(ent, &tceNodes[next], "team_WOLF_checkpoint");
    if (!target) {
        G_Printf("Error: flag not found at node %d\n", next);
        return qtrue;
    }
    if (tceNodes[next].team == team) {
        if (target->count == team) return qtrue;
    } else if (target->count != OtherTeam(team)) return qtrue;
    return TCE_BotApproachNodeEntity(ent, target, command, "flag");
}

/* The Windows folded body200827c0 and Linux BotDoDynamite0007c9b4 really
 * return1. This is not an implemented bomb-planting ability. */
static qboolean TCE_BotDoDynamite(void) { return qtrue; }

/* PerformNodeActions20043af0 / Linux0007c91c. */
qboolean TCE_PerformNodeActions(gentity_t *ent, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int next = TCE_BotReadShort(info, 0x18);
    BotWord(info, 0x1a, (short)(TCE_BotReadShort(info, 0x1a) + 1));
    if (next < 0 || next >= TCE_MAX_NODES) return qtrue;
    if (tceNodes[next].flags & 4) {
        switch (tceNodes[next].moverType) {
        case 0: return TCE_BotDoDynamite();
        case 1: return TCE_BotDoStealObjective(ent, command);
        case 2: return TCE_BotDoFlag(ent, command);
        }
    }
    return qtrue;
}

/* Shared straight-line portions of the two original route controllers. */
static double TCE_BotRouteLength(const vec3_t v) {
    return sqrt(((double)v[0] * v[0] + (double)v[1] * v[1]) + (double)v[2] * v[2]);
}

static void TCE_BotAdvanceRoute(gentity_t *ent, int *state) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    BotWord(info, 0x12, TCE_BotReadShort(info, 0x14));
    BotWord(info, 0x14, TCE_BotReadShort(info, 0x16));
    BotWord(info, 0x16, TCE_BotReadShort(info, 0x18));
    BotWord(info, 0x18, TCE_BotGetNextNode(ent, state));
    BotDword(info, 0x1c, 0);
    BotWord(info, 0x1a, 0);
    BotDword(info, 0x1244, 0);
}

static void TCE_BotTravelLink(gentity_t *ent, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int current = TCE_BotReadShort(info, 0x16), next = TCE_BotReadShort(info, 0x18);
    int i, movement;
    if (next < 0 || next >= TCE_MAX_NODES) return;
    if (current >= 0 && current < TCE_MAX_NODES) {
        /* Original scans all12 slots, not numLinks. */
        for (i = 0; i < TCE_MAX_NODE_LINKS; ++i) {
            if (tceNodes[current].links[i].target == next) {
                BotWord(info, 0x123e, (short)tceNodes[current].links[i].flags);
                break;
            }
        }
        if (!(tceNodes[current].flags & 0x400) && (tceNodes[next].flags & 0x400) &&
            TCE_LadderNodeOccupied(ent)) return;
    }
    movement = TCE_CanMove(ent, 1);
    if (movement < 1 || TCE_BotReadUInt(info, 0x1254) > 19) {
        if (!TCE_BotMoveNearerNode(ent, command) && !TCE_BotSpecialMove(ent, command))
            TCE_BotMove(ent, command, 1, (int)TCE_BotReadShort(info, 0x123e));
        return;
    }
    if (movement >= 1 && movement <= 4) {
        TCE_BotMove(ent, command, movement, (int)TCE_BotReadShort(info, 0x123e));
        BotDword(info, 0x1244, 0);
        if (movement == 1) BotDword(info, 0x1254, 0);
    }
}

static void TCE_BotRouteStall(gentity_t *ent, int *state, qboolean wasMoveState) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    if (TCE_BotReadUInt(info, 0x1c) || TCE_BotRouteLength(ent->client->ps.velocity) >= 32.0)
        BotDword(info, 0x1240, (unsigned int)level.time + 4000u);
    else if (TCE_BotReadUInt(info, 0x1240) && TCE_BotReadUInt(info, 0x1240) <= (unsigned int)level.time)
        BotDword(info, 0x124c, 3);
    if (TCE_BotReadUInt(info, 0x124c) > 2 || TCE_BotReadUInt(info, 0x1254) > 40) {
        if (wasMoveState) BotDword(info, 0x124c, 3);
        BotDword(info, 0x1254, 0);
        BotWord(info, 0x12, TCE_BotReadShort(info, 0x18));
        BotWord(info, 0x18, TCE_BotReadShort(info, 0x16));
        /* Move and wander intentionally restore different current nodes. */
        BotWord(info, 0x16, TCE_BotReadShort(info, wasMoveState ? 0x12 : 0x14));
        BotWord(info, 0x14, TCE_BotReadShort(info, 0x12));
        BotDword(info, 0x1038, (unsigned int)level.time + (wasMoveState ? 5000u : 10000u));
        *state = 3;
        BotDword(info, 0x124c, 0);
    }
}

/* HandleWanderState20042830 / Linux0007b4e2: complete controller. */
void TCE_HandleWanderState(gentity_t *ent, int *state, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int next;
    vec3_t delta;
    float distance;
    if (TCE_BotReadUInt(info, 0x1038) <= (unsigned int)level.time) {
        *state = 0;
        return;
    }
    if (TCE_BotReadShort(info, 0x18) == -1)
        BotWord(info, 0x18, TCE_BotGetNextNode(ent, state));
    next = TCE_BotReadShort(info, 0x18);
    if (next >= 0 && next < TCE_MAX_NODES) {
        VectorSubtract(tceNodes[next].origin, ent->r.currentOrigin, delta);
        distance = (float)TCE_BotRouteLength(delta);
        vectoangles(delta, ent->s.angles);
        if (distance <= 32.0f || TCE_BotReadUInt(info, 0x1c)) {
            if (TCE_PerformNodeActions(ent, command)) TCE_BotAdvanceRoute(ent, state);
        } else TCE_BotTravelLink(ent, command);
    }
    TCE_BotRouteStall(ent, state, qfalse);
}

/* HandleMoveState20042f40 / Linux0007bcbc: path exhaustion, goal arrival,
 * timeout and jammed-route recovery retain their distinct original ordering. */
void TCE_HandleMoveState(gentity_t *ent, int *state, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int next;
    vec3_t delta;
    if (TCE_BotReadUInt(info, 0x1248) && TCE_BotReadUInt(info, 0x1248) < (unsigned int)level.time) {
        BotWord(info, 0x36, -1);
        BotDword(info, 0x1248, 0);
        *state = 0;
        return;
    }
    if (TCE_BotReadShort(info, 0x36) == -1) goto wander;
    next = TCE_BotReadShort(info, 0x18);
    if (next == -1) {
        if (TCE_BotReadShort(info, 0x123c) > 0) {
            BotWord(info, 0x18, TCE_BotGetNextNode(ent, state));
            goto finish;
        }
        BotWord(info, 0x36, -1);
        goto wander;
    }
    if (next < 0 || next >= TCE_MAX_NODES) goto finish;
    VectorSubtract(tceNodes[next].origin, ent->r.currentOrigin, delta);
    vectoangles(delta, ent->s.angles);
    if (TCE_BotRouteLength(delta) > 32.0 && !TCE_BotReadUInt(info, 0x1c)) {
        TCE_BotTravelLink(ent, command);
        goto finish;
    }
    if (!TCE_PerformNodeActions(ent, command)) goto finish;
    TCE_BotAdvanceRoute(ent, state);
    if (TCE_BotReadShort(info, 0x16) != TCE_BotReadShort(info, 0x36)) goto finish;
    BotWord(info, 0x36, -1);
wander:
    *state = 3;
    BotDword(info, 0x1038, (unsigned int)level.time + 15000u);
    BotWord(info, 0x18, -1);
finish:
    TCE_BotRouteStall(ent, state, qtrue);
}

/* WP node scratch at original+0x58..0x64 contains32-bit pointers. Keep native
 * pointers in a sidecar rather than changing the persisted0x6c node layout. */
typedef struct tceWPState_s {
    struct tceWPState_s *parent, *next;
    int cost, estimate;
} tceWPState_t;
static tceWPState_t tceWPStates[TCE_MAX_NODES];
static tceWPState_t *tceWPOpen;
static int tceWPError;

/* WP_AddToOpenList20044340 / Linux0007dc94. Stable insertion on equal cost. */
static void TCE_WPAddToOpenList(tceWPState_t *entry) {
    tceWPState_t *cursor, *previous = NULL;
    int iterations = 0;
    entry->next = NULL;
    if (!tceWPOpen) { tceWPOpen = entry; return; }
    cursor = tceWPOpen;
    for (;;) {
        if (++iterations > tceNumNodes * 2) {
            G_Printf("^3WP_AddToOpenList():- loop fucked up\n");
            tceWPOpen = NULL;
            tceWPError = 1;
            return;
        }
        if ((int)((unsigned int)entry->estimate + entry->cost) <
            (int)((unsigned int)cursor->estimate + cursor->cost)) break;
        previous = cursor;
        cursor = cursor->next;
        if (!cursor) { previous->next = entry; return; }
    }
    if (previous) previous->next = entry;
    else tceWPOpen = entry;
    entry->next = cursor;
}

/* WP_RemoveFromOpenList200443e0 / Linux0007dd5a. */
static void TCE_WPRemoveFromOpenList(void) {
    tceWPState_t *next;
    if (!tceWPOpen) {
        Com_Printf("^3WP_RemoveFromOpenList():- openList == NULL - that aint right\n");
        tceWPError = 1;
        return;
    }
    next = tceWPOpen->next;
    tceWPOpen->next = NULL;
    tceWPOpen = next;
}

/* WP_ClearAll20044420 / Linux0007ddaa clears exactly these four fields. */
static void TCE_WPClearAll(void) {
    int i;
    for (i = 0; i < TCE_MAX_NODES; ++i) {
        tceWPStates[i].parent = tceWPStates[i].next = NULL;
        tceWPStates[i].cost = tceWPStates[i].estimate = 0;
    }
}

/* WP_MakePath20044440 / Linux0007dde4. Caller owns256 shorts. */
static int TCE_WPMakePath(tceWPState_t *entry, short *path) {
    int iterations = 0, count = 0;
    memset(path, 0xff, 256 * sizeof(*path));
    while (entry) {
        if (++iterations > tceNumNodes * 2) {
            Com_Printf("^3WP_MakePath():- loop fucked up\n");
            return count;
        }
        /* Original overflows its256-short output for longer routes. */
        if (count == 256) {
            Com_Printf("WP_MakePath: route exceeds256 nodes\n");
            return -1;
        }
        path[count++] = tceNodes[entry - tceWPStates].number;
        entry = entry->parent;
    }
    return count;
}

/* WP_CalcPath200444a0 / Linux0007de52. This is NOT the separate Linux
 * CreatePathAStar0007d3cc heap implementation. Actual Idle/Carry call this one. */
int TCE_WPCalcPath(gentity_t *ent, int start, int goal, short *path) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    tceWPState_t *current, *last = NULL, *target, *destination;
    tceNode_t *node, *neighbor;
    vec3_t delta;
    int loops = 0, i, next, currentIndex, edge;
    float factor;
    if (start < 0 || goal < 0 || start >= TCE_MAX_NODES || goal >= TCE_MAX_NODES || start == goal) return -1;
    TCE_WPClearAll();
    tceWPOpen = NULL;
    TCE_WPAddToOpenList(&tceWPStates[start]);
    tceWPStates[start].parent = NULL;
    tceWPStates[start].cost = 0;
    VectorSubtract(tceNodes[goal].origin, tceNodes[start].origin, delta);
    tceWPStates[start].estimate = (int)TCE_BotRouteLength(delta);
    destination = &tceWPStates[goal];
    while ((current = tceWPOpen) != NULL) {
        if (++loops > tceNumNodes * 2) {
            G_Printf("WP_CalcPath():- loop fucked up\n");
            tceWPError = 1;
            last = NULL;
            break;
        }
        TCE_WPRemoveFromOpenList();
        if (current == destination) return TCE_WPMakePath(destination, path);
        currentIndex = (int)(current - tceWPStates);
        node = &tceNodes[currentIndex];
        for (i = 0; i < node->numLinks && i < TCE_MAX_NODE_LINKS; ++i) {
            next = node->links[i].target;
            if (next < 0 || next >= TCE_MAX_NODES || (node->links[i].flags & 0x10)) continue;
            neighbor = &tceNodes[next];
            target = &tceWPStates[next];
            if (((neighbor->flags & 0x20) && ent->client->sess.sessionTeam == TEAM_ALLIES) ||
                ((neighbor->flags & 0x10) && ent->client->sess.sessionTeam == TEAM_AXIS) ||
                neighbor->flags == -1 || target->parent || next == start || target == current->parent) continue;
            target->parent = current;
            VectorSubtract(node->origin, neighbor->origin, delta);
            if (!TCE_BotReadUInt(info, 0x124c) &&
                (neighbor->number == TCE_BotReadShort(info, 0x14) ||
                 neighbor->number == TCE_BotReadShort(info, 0x12))) {
                /* Windows2004467b multiplies -10, then SUBTRACTS ftol. */
                edge = (int)(TCE_BotRouteLength(delta) * -10.0);
                target->cost = (int)((unsigned int)current->cost - edge);
            } else {
                factor = (float)((double)(rand() & 0x7fff) * (double)3.0518509447574615e-05f + 0.5);
                edge = (int)(TCE_BotRouteLength(delta) * factor);
                target->cost = (int)((unsigned int)current->cost + edge);
            }
            if (neighbor->flags & 0x400) target->cost = (int)((unsigned int)target->cost + 384u);
            VectorSubtract(tceNodes[goal].origin, neighbor->origin, delta);
            factor = (float)((double)(rand() & 0x7fff) * (double)3.0518509447574615e-05f + 0.5);
            target->estimate = (int)(TCE_BotRouteLength(delta) * factor);
            TCE_WPAddToOpenList(target);
        }
        last = current;
    }
    return last == destination ? TCE_WPMakePath(destination, path) : -1;
}

/* Long-term-goal search uses node-centered boxes and40 results, unlike the
 * action callbacks' bot-centered1024-result search. */
static gentity_t *TCE_BotFindGoalEntity(tceNode_t *node, qboolean checkpoint) {
    int list[40], count, i;
    vec3_t mins, maxs;
    gentity_t *ent;
    VectorSet(mins, node->origin[0] - 300.0f, node->origin[1] - 300.0f, node->origin[2] - 20.0f);
    VectorSet(maxs, node->origin[0] + 300.0f, node->origin[1] + 300.0f, node->origin[2] + 20.0f);
    count = trap_EntitiesInBox(mins, maxs, list, 40);
    for (i = 0; i < count; ++i) {
        if (list[i] < 0 || list[i] >= MAX_GENTITIES) continue;
        ent = &g_entities[list[i]];
        if (!ent->classname) continue;
        if (checkpoint ? !Q_stricmp(ent->classname, "team_WOLF_checkpoint") :
            (!Q_stricmp(ent->classname, "team_CTF_blueflag") || !Q_stricmp(ent->classname, "team_CTF_redflag"))) {
            node->moverEntity = (short)ent->s.number;
            return ent;
        }
    }
    return NULL;
}

static double TCE_BotRandomUnit(void) {
    return (double)(rand() & 0x7fff) * (double)3.0518509447574615e-05f;
}
static int TCE_BotRandomIndex(int count) {
    return -(int)(TCE_BotRandomUnit() * count * (double)-0.99f);
}

/* BotFindLongTermGoal20040ba0 / Linux00079414. Original flag roles, class
 * restriction, gamestate gate, random draws and fallback ordering preserved. */
void TCE_BotFindLongTermGoal(gentity_t *ent) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int objectives[10], choices[64], count = 0, choiceCount, i, team;
    int selected = -1, bestCost = -1, current, distance, cost;
    qboolean carrying;
    tceNode_t *node;
    gentity_t *target;
    vec3_t delta;
    double random;
    team = ent->client->sess.sessionTeam;
    carrying = ent->client->ps.powerups[PW_REDFLAG] || ent->client->ps.powerups[PW_BLUEFLAG];
    for (i = 0; i < tceNumNodes; ++i) {
        node = &tceNodes[i];
        if (node->flags == -1 || ((node->flags & 0x20) && team == TEAM_ALLIES) ||
            ((node->flags & 0x10) && team == TEAM_AXIS) || i == TCE_BotReadShort(info, 0x16)) continue;
        if (carrying) {
            if (((node->flags & 0x80) && team == TEAM_AXIS) ||
                ((node->flags & 0x100) && team == TEAM_ALLIES)) {
                BotWord(info, 0x36, node->number);
                return;
            }
            continue;
        }
        if (!(node->flags & 4)) continue;
        if (node->moverType == 0 && ent->client->ps.stats[STAT_PLAYER_CLASS] != PC_ENGINEER) continue;
        if (node->moverType == 1 && team == node->team &&
            (node->moverEntity == ENTITYNUM_NONE || node->moverEntity < 64)) {
            if (!TCE_BotFindGoalEntity(node, qfalse)) continue;
        }
        if (node->moverType == 2) {
            if (node->moverEntity == ENTITYNUM_NONE || node->moverEntity < 64) {
                if (!TCE_BotFindGoalEntity(node, qtrue)) continue;
            }
            if (node->moverEntity < 0 || node->moverEntity >= MAX_GENTITIES) continue;
            target = &g_entities[node->moverEntity];
            if (node->team == team) {
                if (target->count == team) continue;
            } else if (target->count != OtherTeam(team)) continue;
        }
        objectives[count++] = i;
        if (count == 10) break;
    }
    if (!count && g_gamestate.integer == GS_PLAYING) {
        choiceCount = 0;
        for (i = 0; i < tceNumNodes; ++i)
            if ((tceNodes[i].flags & 0x200) && choiceCount < 64) choices[choiceCount++] = i;
        if (choiceCount) {
            selected = choices[TCE_BotRandomIndex(choiceCount)];
            if (selected != -1 && TCE_BotRandomUnit() < 0.75) {
                BotWord(info, 0x36, (short)selected);
                return;
            }
        }
        if (g_gamestate.integer == GS_PLAYING) {
            choiceCount = 0;
            for (i = 0; i < level.maxclients && i < 64; ++i) {
                target = &g_entities[i];
                if (target->inuse && target->client && target->client->pers.connected == CON_CONNECTED &&
                    target->client->sess.sessionTeam != team && target->health > 0 &&
                    !(target->client->ps.pm_flags & PMF_LIMBO) &&
                    target->client->sess.sessionTeam != TEAM_SPECTATOR) choices[choiceCount++] = i;
            }
            if (choiceCount) {
                target = &g_entities[choices[TCE_BotRandomIndex(choiceCount)]];
                if (target->client) selected = TCE_FindClosestNodeToPoint(ent, target->client->ps.origin);
            }
            if (selected != -1) {
                BotWord(info, 0x36, (short)selected);
                return;
            }
        }
    }
    current = TCE_BotReadShort(info, 0x16);
    if (current == -1) current = TCE_BotFindClosestNode(ent);
    if (current < 0 || current >= TCE_MAX_NODES) return;
    for (i = 0; i < count; ++i) {
        VectorSubtract(tceNodes[current].origin, tceNodes[objectives[i]].origin, delta);
        distance = (int)TCE_BotRouteLength(delta);
        random = TCE_BotRandomUnit() - 0.5;
        cost = (int)(((random + random) * 0.5 + 1.0) * distance);
        if (cost < bestCost || bestCost == -1) {
            selected = objectives[i];
            bestCost = cost;
        }
    }
    BotWord(info, 0x36, (short)selected);
}

/* HandleIdleState20043570 / Linux0007c338. Always ends in move state1,
 * including the no-goal case; HandleMoveState performs the fallback next tick. */
void TCE_HandleIdleState(gentity_t *ent, int *state) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    short start, path[256];
    int count;
    if (TCE_BotReadShort(info, 0x36) == -1) TCE_BotFindLongTermGoal(ent);
    BotWord(info, 0x16, -1);
    BotWord(info, 0x18, -1);
    BotDword(info, 0x1240, 0);
    BotDword(info, 0x1244, 0);
    if (TCE_BotReadShort(info, 0x36) != -1) {
        start = TCE_BotFindClosestNode(ent);
        /* Failed CalcPath leaves the caller's old path bytes untouched. */
        memcpy(path, info + 0x103c, sizeof(path));
        count = TCE_WPCalcPath(ent, start, TCE_BotReadShort(info, 0x36), path);
        memcpy(info + 0x103c, path, sizeof(path));
        BotWord(info, 0x123c, (short)count);
        BotWord(info, 0x18, start);
        BotDword(info, 0x124c, 0);
    }
    *state = 1;
}

/* HandleCarryObjectiveState200436b0 / Linux0007c44a. */
void TCE_HandleCarryObjectiveState(gentity_t *ent, int *state, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int goal, next, current, i, count;
    short start, path[256];
    vec3_t delta;
    float distance;
    if (!ent->client->ps.powerups[PW_REDFLAG] && !ent->client->ps.powerups[PW_BLUEFLAG]) {
        *state = 0;
        BotWord(info, 0x36, -1);
        return;
    }
    goal = TCE_BotReadShort(info, 0x36);
    if (goal < 0 || goal >= TCE_MAX_NODES ||
        !(tceNodes[goal].flags & (ent->client->sess.sessionTeam == TEAM_AXIS ? 0x80 : 0x100))) {
        start = TCE_BotFindClosestNode(ent);
        TCE_BotFindLongTermGoal(ent);
        /* Original clears only0x100 bytes here, not the full512-byte path. */
        memset(info + 0x103c, 0xff, 0x100);
        memcpy(path, info + 0x103c, sizeof(path));
        count = TCE_WPCalcPath(ent, start, TCE_BotReadShort(info, 0x36), path);
        memcpy(info + 0x103c, path, sizeof(path));
        BotWord(info, 0x123c, (short)count);
        BotWord(info, 0x18, start);
        BotDword(info, 0x124c, 0);
    }
    if (TCE_BotReadShort(info, 0x18) == -1)
        BotWord(info, 0x18, TCE_BotGetNextNode(ent, state));
    next = TCE_BotReadShort(info, 0x18);
    if (next < 0 || next >= TCE_MAX_NODES) return;
    VectorSubtract(tceNodes[next].origin, ent->r.currentOrigin, delta);
    distance = (float)TCE_BotRouteLength(delta);
    vectoangles(delta, ent->s.angles);
    if (distance <= 32.0f) {
        BotWord(info, 0x12, TCE_BotReadShort(info, 0x14));
        BotWord(info, 0x14, TCE_BotReadShort(info, 0x16));
        BotWord(info, 0x16, TCE_BotReadShort(info, 0x18));
        BotWord(info, 0x18, TCE_BotGetNextNode(ent, state));
        BotWord(info, 0x123e, -1);
        BotDword(info, 0x1244, 0);
    } else {
        current = TCE_BotReadShort(info, 0x16);
        if (TCE_BotReadShort(info, 0x123e) == -1 && current >= 0 && current < TCE_MAX_NODES) {
            for (i = 0; i < TCE_MAX_NODE_LINKS; ++i) {
                if (tceNodes[current].links[i].target == next) {
                    BotWord(info, 0x123e, (short)tceNodes[current].links[i].flags);
                    break;
                }
            }
        }
        if (TCE_BotRandomUnit() > 0.2) BotWord(info, 0x123e, (short)(TCE_BotReadShort(info, 0x123e) | 8));
        if (TCE_CanMove(ent, 1) != 1) {
            if (!TCE_BotMoveNearerNode(ent, command) && !TCE_BotSpecialMove(ent, command))
                TCE_BotMove(ent, command, 1, (int)TCE_BotReadShort(info, 0x123e));
        } else {
            TCE_BotMove(ent, command, 1, (int)TCE_BotReadShort(info, 0x123e));
            BotDword(info, 0x1244, 0);
        }
    }
    if (TCE_BotRouteLength(ent->client->ps.velocity) >= 32.0)
        BotDword(info, 0x1240, (int)((double)level.time + 6000.0));
    else if (TCE_BotReadUInt(info, 0x1240) && TCE_BotReadUInt(info, 0x1240) <= (unsigned int)level.time)
        Cmd_Kill_f(ent, 0x25); /* Exact original cause; do not infer from function name. */
}

/* TC200420d0 / Linux0007ad94. These are deliberately independent branches:
 * each state handler may hand off to another handler during the same frame. */
void TCE_BotStateActions(gentity_t *ent, int *state, usercmd_t *command) {
    byte *info = (byte *)&tceBotInfo[ent->s.number];
    int i, now = level.time;
    qboolean fresh;
    static const int minusWords[] = {4,0x12,0x14,0x16,0x18,0x36};
    static const int zeroWords[] = {0x1a,0x10,0x2e,0x30,0x34,0x123c,0x123e};
    static const int noneWords[] = {0x24,0x2c,0x32};
    static const int zeroDwords[] = {0x1c,0xc,0x28,0x20,0x1038,0x1240,0x1244,0x1248,0x124c,0x1254};
    if (now < ent->client->respawnTime + 100) {
        /* Not ClearBotInfo: preserve skill, active flag, event cache, path and
         * the caller's local state, which was read before this reset. */
        for (i=0;i<sizeof(minusWords)/sizeof(minusWords[0]);i++) BotWord(info,minusWords[i],-1);
        for (i=0;i<sizeof(zeroWords)/sizeof(zeroWords[0]);i++) BotWord(info,zeroWords[i],0);
        for (i=0;i<sizeof(noneWords)/sizeof(noneWords[0]);i++) BotWord(info,noneWords[i],ENTITYNUM_NONE);
        for (i=0;i<sizeof(zeroDwords)/sizeof(zeroDwords[0]);i++) BotDword(info,zeroDwords[i],0);
    }
    fresh = now < ent->client->respawnTime + 1000;
    if (fresh) ent->client->ps.persistant[PERS_ATTACKER] = -1;
    if (*state == -1 && ent->client->ps.stats[STAT_HEALTH] > 0) *state = 0;
    if (ent->client->ps.pm_type == PM_DEAD) TCE_HandleDeathState(ent,state);
    if (fresh) return;
    if (ent->client->ps.powerups[PW_REDFLAG] || ent->client->ps.powerups[PW_BLUEFLAG]) *state = 7;
    if (*state == 7) TCE_HandleCarryObjectiveState(ent,state,command);
    if (*state == 6) TCE_HandleReviveState(ent,state,command);
    if (*state == 4) TCE_HandleFollowState(ent,state,command);
    if (*state == 5 || TCE_BotReadShort(info,0x2c) == ent->s.number)
        TCE_HandleGiveItemState(ent,state,command);
    if (*state == 2 && TCE_BotReadShort(info,0x2e) == 0) TCE_HandleAttackState(ent,state,command);
    if (*state == 1) TCE_HandleMoveState(ent,state,command);
    if (*state == 3) TCE_HandleWanderState(ent,state,command);
    if (*state != 7 && *state != 6 && *state != 5) {
        TCE_BotCheckAmmoState(ent,state);
        if (TCE_LocateEnemy(ent)) *state = 2;
        else ent->client->ps.stats[STAT_TCE_WEAPON_FLAGS] &= ~4;
    }
    if (*state == 0) TCE_HandleIdleState(ent,state);
}

/* TC2003e870 / Linux000760cc. The original stores a pointer to its seven-word
 * stack command at botInfo+8. Native callers pass that same frame-local command
 * explicitly; no host pointer is written into the original fixed-width state. */
void TCE_Botthink(gentity_t *ent) {
    int state = TCE_BotReadShort((byte *)&tceBotInfo[ent->s.number],4);
    usercmd_t command;
    if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) return;
    memset(&command,0,sizeof(command));
    VectorCopy(ent->client->ps.viewangles,ent->s.angles);
    ent->client->ps.delta_angles[0] = 0;
    ent->client->ps.delta_angles[1] = 0;
    ent->client->ps.delta_angles[2] = 0;
    TCE_BotParseServerCommand(ent,&state);
    TCE_BotStateActions(ent,&state,&command);
    TCE_BotEvents(ent);
    TCE_BotSendCommand(ent,&state,&command);
}
