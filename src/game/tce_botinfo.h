#ifndef TCE_BOTINFO_H
#define TCE_BOTINFO_H
/* Original TC state stride0x1258. Do not substitute SDK bot_state_t.
 * Remaining storage is reserved until the corresponding navigation producers
 * and consumers are ported. Offsets in ClearBotInfo are original ABI offsets. */
typedef struct { int active;short mode,skill;byte storage[0x1250]; } tceBotInfo_t;
extern tceBotInfo_t tceBotInfo[64];
extern int tceNumBots;
void TCE_ClearBotInfo(int clientNum);
void TCE_InitBotInfo(void);
void TCE_UpdateBotInfo(void);
int TCE_ClientNumberFromNameMatch(const char *name,int *matches);
void TCE_SvcmdKickBot(void);
/* Original Botthink owns a frame-local command at TC offset8. Pass it
 * explicitly instead of storing a native pointer in the fixed-width record. */
void TCE_BotParseServerCommand(gentity_t *ent, int *state);
void TCE_BotSendCommand(gentity_t *ent, int *state, usercmd_t *command);
void TCE_BotHandleEvents(gentity_t *ent, gentity_t *eventEnt);
void TCE_BotEvents(gentity_t *ent);
void TCE_BotMove(gentity_t *ent, usercmd_t *command, int direction, unsigned int flags);
int TCE_BotFindFreeDirection(gentity_t *ent);
void TCE_BotFollowPlayer(gentity_t *ent, usercmd_t *command);
void TCE_HandleFollowState(gentity_t *ent, int *state, usercmd_t *command);
void TCE_BotRevivePlayer(gentity_t *ent, usercmd_t *command);
void TCE_HandleReviveState(gentity_t *ent, int *state, usercmd_t *command);
qboolean TCE_BotNeedAmmo(gentity_t *ent);
void TCE_BotCheckAmmoState(gentity_t *ent, int *state);
void TCE_BotGiveItemToPlayer(gentity_t *ent, int *state, usercmd_t *command);
void TCE_HandleGiveItemState(gentity_t *ent, int *state, usercmd_t *command);
void TCE_HandleDeathState(gentity_t *ent, int *state);
void TCE_FaceEnemy(gentity_t *ent);
void TCE_AttackEnemy(gentity_t *ent, usercmd_t *command);
qboolean TCE_MyVisible(gentity_t *ent, gentity_t *target);
int TCE_CanMove(gentity_t *ent, int direction);
void TCE_HandleAttackState(gentity_t *ent, int *state, usercmd_t *command);
double TCE_BotViewProb(gentity_t *ent, gentity_t *target);
qboolean TCE_LocateEnemy(gentity_t *ent);
short TCE_BotFindClosestNode(gentity_t *ent);
qboolean TCE_BotMoveNearerNode(gentity_t *ent, usercmd_t *command);
qboolean TCE_BotSpecialMove(gentity_t *ent, usercmd_t *command);
qboolean TCE_LadderNodeOccupied(gentity_t *ent);
short TCE_BotGetNextNode(gentity_t *ent, int *state);
qboolean TCE_BotDoStealObjective(gentity_t *ent, usercmd_t *command);
qboolean TCE_BotDoFlag(gentity_t *ent, usercmd_t *command);
qboolean TCE_PerformNodeActions(gentity_t *ent, usercmd_t *command);
void TCE_HandleWanderState(gentity_t *ent, int *state, usercmd_t *command);
void TCE_HandleMoveState(gentity_t *ent, int *state, usercmd_t *command);
int TCE_WPCalcPath(gentity_t *ent, int start, int goal, short *path);
void TCE_BotFindLongTermGoal(gentity_t *ent);
void TCE_HandleIdleState(gentity_t *ent, int *state);
void TCE_HandleCarryObjectiveState(gentity_t *ent, int *state, usercmd_t *command);
void TCE_BotStateActions(gentity_t *ent, int *state, usercmd_t *command);
void TCE_Botthink(gentity_t *ent);
#endif
