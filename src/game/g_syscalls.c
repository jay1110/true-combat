// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"
#include "../game/tce_native_syscall.h"

// this file is only included when building a dll
// g_syscalls.asm is included instead when building a qvm

static intptr_t (QDECL *syscall)( intptr_t arg, ... ) = (intptr_t (QDECL *)( intptr_t, ...))-1;

#if defined(__MACOS__)
#ifndef __GNUC__
#pragma export on
#endif
#endif
void dllEntry( intptr_t (QDECL *syscallptr)( intptr_t arg,... ) ) {
	syscall = syscallptr;
}
#if defined(__MACOS__)
#ifndef __GNUC__
#pragma export off
#endif
#endif

static int PASSFLOAT( float x ) {
	int bits;
	memcpy(&bits, &x, sizeof(bits));
	return bits;
}

void	trap_Printf( const char *fmt ) {
	TCE_SYSCALL_1( G_PRINT, fmt );
}

void	trap_Error( const char *fmt ) {
	TCE_SYSCALL_1( G_ERROR, fmt );
}

int		trap_Milliseconds( void ) {
	return TCE_SYSCALL_0( G_MILLISECONDS );
}
int		trap_Argc( void ) {
	return TCE_SYSCALL_0( G_ARGC );
}

void	trap_Argv( int n, char *buffer, int bufferLength ) {
	TCE_SYSCALL_3( G_ARGV, n, buffer, bufferLength );
}

int		trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	return TCE_SYSCALL_3( G_FS_FOPEN_FILE, qpath, f, mode );
}

void	trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	TCE_SYSCALL_3( G_FS_READ, buffer, len, f );
}

int		trap_FS_Write( const void *buffer, int len, fileHandle_t f ) {
	return TCE_SYSCALL_3( G_FS_WRITE, buffer, len, f );
}

int		trap_FS_Rename( const char *from, const char *to ) {
	return TCE_SYSCALL_2( G_FS_RENAME, from, to );
}

void	trap_FS_FCloseFile( fileHandle_t f ) {
	TCE_SYSCALL_1( G_FS_FCLOSE_FILE, f );
}

int trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize ) {
	return TCE_SYSCALL_4( G_FS_GETFILELIST, path, extension, listbuf, bufsize );
}

void	trap_SendConsoleCommand( int exec_when, const char *text ) {
	TCE_SYSCALL_2( G_SEND_CONSOLE_COMMAND, exec_when, text );
}

void	trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags ) {
	TCE_SYSCALL_4( G_CVAR_REGISTER, cvar, var_name, value, flags );
}

void	trap_Cvar_Update( vmCvar_t *cvar ) {
	TCE_SYSCALL_1( G_CVAR_UPDATE, cvar );
}

void trap_Cvar_Set( const char *var_name, const char *value ) {
	TCE_SYSCALL_2( G_CVAR_SET, var_name, value );
}

int trap_Cvar_VariableIntegerValue( const char *var_name ) {
	return TCE_SYSCALL_1( G_CVAR_VARIABLE_INTEGER_VALUE, var_name );
}

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	TCE_SYSCALL_3( G_CVAR_VARIABLE_STRING_BUFFER, var_name, buffer, bufsize );
}

void trap_Cvar_LatchedVariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	TCE_SYSCALL_3( G_CVAR_LATCHEDVARIABLESTRINGBUFFER, var_name, buffer, bufsize );
}

void trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
						 playerState_t *clients, int sizeofGClient ) {
	TCE_SYSCALL_5( G_LOCATE_GAME_DATA, gEnts, numGEntities, sizeofGEntity_t, clients, sizeofGClient );
}

void trap_DropClient( int clientNum, const char *reason, int length ) {
	TCE_SYSCALL_3( G_DROP_CLIENT, clientNum, reason, length );
}

void trap_SendServerCommand( int clientNum, const char *text ) {
	// rain - #433 - commands over 1022 chars will crash the
	// client engine upon receipt, so ignore them
	if( strlen( text ) > 1022 ) {
		G_LogPrintf( "%s: trap_SendServerCommand( %d, ... ) length exceeds 1022.\n", GAMEVERSION, clientNum );
		G_LogPrintf( "%s: text [%s]\n", GAMEVERSION, text );
		return;
	}
	TCE_SYSCALL_2( G_SEND_SERVER_COMMAND, clientNum, text );
}

void trap_SetConfigstring( int num, const char *string ) {
	TCE_SYSCALL_2( G_SET_CONFIGSTRING, num, string );
}

void trap_GetConfigstring( int num, char *buffer, int bufferSize ) {
	TCE_SYSCALL_3( G_GET_CONFIGSTRING, num, buffer, bufferSize );
}

void trap_GetUserinfo( int num, char *buffer, int bufferSize ) {
	TCE_SYSCALL_3( G_GET_USERINFO, num, buffer, bufferSize );
}

void trap_SetUserinfo( int num, const char *buffer ) {
	TCE_SYSCALL_2( G_SET_USERINFO, num, buffer );
}

void trap_GetServerinfo( char *buffer, int bufferSize ) {
	TCE_SYSCALL_2( G_GET_SERVERINFO, buffer, bufferSize );
}

void trap_SetBrushModel( gentity_t *ent, const char *name ) {
	TCE_SYSCALL_2( G_SET_BRUSH_MODEL, ent, name );
}

void trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	TCE_SYSCALL_7( G_TRACE, results, start, mins, maxs, end, passEntityNum, contentmask );
}

void trap_TraceNoEnts( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	TCE_SYSCALL_7( G_TRACE, results, start, mins, maxs, end, -2, contentmask );
}

void trap_TraceCapsule( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	TCE_SYSCALL_7( G_TRACECAPSULE, results, start, mins, maxs, end, passEntityNum, contentmask );
}

void trap_TraceCapsuleNoEnts( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	TCE_SYSCALL_7( G_TRACECAPSULE, results, start, mins, maxs, end, -2, contentmask );
}

int trap_PointContents( const vec3_t point, int passEntityNum ) {
	return TCE_SYSCALL_2( G_POINT_CONTENTS, point, passEntityNum );
}


qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 ) {
	return TCE_SYSCALL_2( G_IN_PVS, p1, p2 );
}

qboolean trap_InPVSIgnorePortals( const vec3_t p1, const vec3_t p2 ) {
	return TCE_SYSCALL_2( G_IN_PVS_IGNORE_PORTALS, p1, p2 );
}

void trap_AdjustAreaPortalState( gentity_t *ent, qboolean open ) {
	TCE_SYSCALL_2( G_ADJUST_AREA_PORTAL_STATE, ent, open );
}

qboolean trap_AreasConnected( int area1, int area2 ) {
	return TCE_SYSCALL_2( G_AREAS_CONNECTED, area1, area2 );
}

void trap_LinkEntity( gentity_t *ent ) {
	TCE_SYSCALL_1( G_LINKENTITY, ent );
}

void trap_UnlinkEntity( gentity_t *ent ) {
	TCE_SYSCALL_1( G_UNLINKENTITY, ent );
}


int trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *list, int maxcount ) {
	return TCE_SYSCALL_4( G_ENTITIES_IN_BOX, mins, maxs, list, maxcount );
}

qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent ) {
	return TCE_SYSCALL_3( G_ENTITY_CONTACT, mins, maxs, ent );
}

qboolean trap_EntityContactCapsule( const vec3_t mins, const vec3_t maxs, const gentity_t *ent ) {
	return TCE_SYSCALL_3( G_ENTITY_CONTACTCAPSULE, mins, maxs, ent );
}

int trap_BotAllocateClient( int clientNum ) {
	return TCE_SYSCALL_1( G_BOT_ALLOCATE_CLIENT, clientNum );
}

void trap_BotFreeClient( int clientNum ) {
	TCE_SYSCALL_1( G_BOT_FREE_CLIENT, clientNum );
}

int trap_GetSoundLength(sfxHandle_t sfxHandle) {
	return TCE_SYSCALL_1( G_GET_SOUND_LENGTH, sfxHandle );
}

sfxHandle_t	trap_RegisterSound( const char *sample, qboolean compressed ) {
	return TCE_SYSCALL_2( G_REGISTERSOUND, sample, compressed );
}

#ifdef DEBUG
//#define FAKELAG
#ifdef FAKELAG
#define	MAX_USERCMD_BACKUP	256
#define	MAX_USERCMD_MASK	(MAX_USERCMD_BACKUP - 1)

static usercmd_t cmds[MAX_CLIENTS][MAX_USERCMD_BACKUP];
static int cmdNumber[MAX_CLIENTS];
#endif // FAKELAG
#endif // DEBUG

void trap_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	TCE_SYSCALL_2( G_GET_USERCMD, clientNum, cmd );

#ifdef FAKELAG
	{
		char s[MAX_STRING_CHARS];
		int fakeLag;

		trap_Cvar_VariableStringBuffer( "g_fakelag", s, sizeof(s) );
		fakeLag = atoi(s);
		if( fakeLag < 0 )
			fakeLag = 0;

		if( fakeLag ) {
			int i;
			int realcmdtime, thiscmdtime;

			// store our newest usercmd
			cmdNumber[clientNum]++;
			memcpy( &cmds[clientNum][cmdNumber[clientNum] & MAX_USERCMD_MASK], cmd, sizeof(usercmd_t) );

			// find a usercmd that is fakeLag msec behind
			i = cmdNumber[clientNum] & MAX_USERCMD_MASK;
			realcmdtime = cmds[clientNum][i].serverTime;
			i--;
			do {
				thiscmdtime = cmds[clientNum][i & MAX_USERCMD_MASK].serverTime;

				if( realcmdtime - thiscmdtime > fakeLag ) {
					// found the right one
                    cmd = &cmds[clientNum][i & MAX_USERCMD_MASK];
					return;
				}

				i--;
			} while ( (i & MAX_USERCMD_MASK) != (cmdNumber[clientNum] & MAX_USERCMD_MASK) );

			// didn't find a proper one, just use the oldest one we have
			cmd = &cmds[clientNum][(cmdNumber[clientNum] - 1) & MAX_USERCMD_MASK];
			return;
		}
	}
#endif // FAKELAG
}

qboolean trap_GetEntityToken( char *buffer, int bufferSize ) {
	return TCE_SYSCALL_2( G_GET_ENTITY_TOKEN, buffer, bufferSize );
}

int trap_DebugPolygonCreate(int color, int numPoints, vec3_t *points) {
	return TCE_SYSCALL_3( G_DEBUG_POLYGON_CREATE, color, numPoints, points );
}

void trap_DebugPolygonDelete(int id) {
	TCE_SYSCALL_1( G_DEBUG_POLYGON_DELETE, id );
}

int trap_RealTime( qtime_t *qtime ) {
	return TCE_SYSCALL_1( G_REAL_TIME, qtime );
}

void trap_SnapVector( float *v ) {
	TCE_SYSCALL_1( G_SNAPVECTOR, v );
	return;
}

qboolean trap_GetTag( int clientNum, int tagFileNumber, char *tagName, orientation_t *or ) {
	return TCE_SYSCALL_4( G_GETTAG, clientNum, tagFileNumber, tagName, or );
}

qboolean trap_LoadTag( const char* filename ) {
	return TCE_SYSCALL_1( G_REGISTERTAG, filename );
}

// BotLib traps start here
int trap_BotLibSetup( void ) {
	return TCE_SYSCALL_0( BOTLIB_SETUP );
}

int trap_BotLibShutdown( void ) {
	return TCE_SYSCALL_0( BOTLIB_SHUTDOWN );
}

int trap_BotLibVarSet(char *var_name, char *value) {
	return TCE_SYSCALL_2( BOTLIB_LIBVAR_SET, var_name, value );
}

int trap_BotLibVarGet(char *var_name, char *value, int size) {
	return TCE_SYSCALL_3( BOTLIB_LIBVAR_GET, var_name, value, size );
}

int trap_BotLibDefine(char *string) {
	return TCE_SYSCALL_1( BOTLIB_PC_ADD_GLOBAL_DEFINE, string );
}

int trap_PC_AddGlobalDefine( char *define ) {
	return TCE_SYSCALL_1( BOTLIB_PC_ADD_GLOBAL_DEFINE, define );
}

int trap_PC_LoadSource( const char *filename ) {
	return TCE_SYSCALL_1( BOTLIB_PC_LOAD_SOURCE, filename );
}

int trap_PC_FreeSource( int handle ) {
	return TCE_SYSCALL_1( BOTLIB_PC_FREE_SOURCE, handle );
}

int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	return TCE_SYSCALL_2( BOTLIB_PC_READ_TOKEN, handle, pc_token );
}

int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	return TCE_SYSCALL_3( BOTLIB_PC_SOURCE_FILE_AND_LINE, handle, filename, line );
}

int trap_PC_UnReadToken( int handle ) {
	return TCE_SYSCALL_1( BOTLIB_PC_UNREAD_TOKEN, handle );
}

int trap_BotLibStartFrame(float time) {
	return TCE_SYSCALL_1( BOTLIB_START_FRAME, PASSFLOAT( time ) );
}

int trap_BotLibLoadMap(const char *mapname) {
	return TCE_SYSCALL_1( BOTLIB_LOAD_MAP, mapname );
}

int trap_BotLibUpdateEntity(int ent, void /* struct bot_updateentity_s */ *bue) {
	return TCE_SYSCALL_2( BOTLIB_UPDATENTITY, ent, bue );
}

int trap_BotLibTest(int parm0, char *parm1, vec3_t parm2, vec3_t parm3) {
	return TCE_SYSCALL_4( BOTLIB_TEST, parm0, parm1, parm2, parm3 );
}

int trap_BotGetSnapshotEntity( int clientNum, int sequence ) {
	return TCE_SYSCALL_2( BOTLIB_GET_SNAPSHOT_ENTITY, clientNum, sequence );
}

int trap_BotGetServerCommand(int clientNum, char *message, int size) {
	return TCE_SYSCALL_3( BOTLIB_GET_CONSOLE_MESSAGE, clientNum, message, size );
}

void trap_BotUserCommand(int clientNum, usercmd_t *ucmd) {
	TCE_SYSCALL_2( BOTLIB_USER_COMMAND, clientNum, ucmd );
}

void trap_AAS_EntityInfo(int entnum, void /* struct aas_entityinfo_s */ *info) {
	TCE_SYSCALL_2( BOTLIB_AAS_ENTITY_INFO, entnum, info );
}

int trap_AAS_Initialized(void) {
	return TCE_SYSCALL_0( BOTLIB_AAS_INITIALIZED );
}

void trap_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs) {
	TCE_SYSCALL_3( BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX, presencetype, mins, maxs );
}

float trap_AAS_Time(void) {
	int temp;
	temp = TCE_SYSCALL_0( BOTLIB_AAS_TIME );
	{ float value; memcpy(&value, &temp, sizeof(value)); return value; }
}

// Ridah, multiple AAS files
void trap_AAS_SetCurrentWorld(int index) {
	// Gordon: stubbed out: we only use one aas
//	TCE_SYSCALL_1( BOTLIB_AAS_SETCURRENTWORLD, index );
}
// done.

int trap_AAS_PointAreaNum(vec3_t point) {
	return TCE_SYSCALL_1( BOTLIB_AAS_POINT_AREA_NUM, point );
}

int trap_AAS_TraceAreas(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas) {
	return TCE_SYSCALL_5( BOTLIB_AAS_TRACE_AREAS, start, end, areas, points, maxareas );
}

int trap_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas) {
	return TCE_SYSCALL_4( BOTLIB_AAS_BBOX_AREAS, absmins, absmaxs, areas, maxareas);
}

void trap_AAS_AreaCenter(int areanum, vec3_t center) {
	TCE_SYSCALL_2( BOTLIB_AAS_AREA_CENTER, areanum, center);
}

qboolean trap_AAS_AreaWaypoint(int areanum, vec3_t center) {
	return TCE_SYSCALL_2( BOTLIB_AAS_AREA_WAYPOINT, areanum, center);
}

int trap_AAS_PointContents(vec3_t point) {
	return TCE_SYSCALL_1( BOTLIB_AAS_POINT_CONTENTS, point );
}

int trap_AAS_NextBSPEntity(int ent) {
	return TCE_SYSCALL_1( BOTLIB_AAS_NEXT_BSP_ENTITY, ent );
}

int trap_AAS_ValueForBSPEpairKey(int ent, char *key, char *value, int size) {
	return TCE_SYSCALL_4( BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY, ent, key, value, size );
}

int trap_AAS_VectorForBSPEpairKey(int ent, char *key, vec3_t v) {
	return TCE_SYSCALL_3( BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY, ent, key, v );
}

int trap_AAS_FloatForBSPEpairKey(int ent, char *key, float *value) {
	return TCE_SYSCALL_3( BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY, ent, key, value );
}

int trap_AAS_IntForBSPEpairKey(int ent, char *key, int *value) {
	return TCE_SYSCALL_3( BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY, ent, key, value );
}

int trap_AAS_AreaReachability(int areanum) {
	return TCE_SYSCALL_1( BOTLIB_AAS_AREA_REACHABILITY, areanum );
}

int trap_AAS_AreaLadder(int areanum) {
	return TCE_SYSCALL_1( BOTLIB_AAS_AREA_LADDER, areanum );
}

int trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags) {
	return TCE_SYSCALL_4( BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA, areanum, origin, goalareanum, travelflags );
}

int trap_AAS_Swimming(vec3_t origin) {
	return TCE_SYSCALL_1( BOTLIB_AAS_SWIMMING, origin );
}

int trap_AAS_PredictClientMovement(void /* struct aas_clientmove_s */ *move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize) {
	return TCE_SYSCALL_13( BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT, move, entnum, origin, presencetype, onground, velocity, cmdmove, cmdframes, maxframes, PASSFLOAT(frametime), stopevent, stopareanum, visualize );
}

// Ridah, route-tables
void trap_AAS_RT_ShowRoute( vec3_t srcpos, int	srcnum, int destnum ) {
	TCE_SYSCALL_3( BOTLIB_AAS_RT_SHOWROUTE, srcpos, srcnum, destnum );
}

//qboolean trap_AAS_RT_GetHidePos( vec3_t srcpos, int srcnum, int srcarea, vec3_t destpos, int destnum, int destarea, vec3_t returnPos ) {
//	return TCE_SYSCALL_7( BOTLIB_AAS_RT_GETHIDEPOS, srcpos, srcnum, srcarea, destpos, destnum, destarea, returnPos );
//}

//int trap_AAS_FindAttackSpotWithinRange(int srcnum, int rangenum, int enemynum, float rangedist, int travelflags, float *outpos) {
//	return TCE_SYSCALL_6( BOTLIB_AAS_FINDATTACKSPOTWITHINRANGE, srcnum, rangenum, enemynum, PASSFLOAT(rangedist), travelflags, outpos );
//}

int trap_AAS_NearestHideArea(int srcnum, vec3_t origin, int areanum, int enemynum, vec3_t enemyorigin, int enemyareanum, int travelflags, float maxdist, vec3_t distpos)
{
	return TCE_SYSCALL_9( BOTLIB_AAS_NEARESTHIDEAREA, srcnum, origin, areanum, enemynum, enemyorigin, enemyareanum, travelflags, PASSFLOAT(maxdist), distpos );
}

int trap_AAS_ListAreasInRange(vec3_t srcpos, int srcarea, float range, int travelflags, float **outareas, int maxareas) {
	return TCE_SYSCALL_6( BOTLIB_AAS_LISTAREASINRANGE, srcpos, srcarea, PASSFLOAT(range), travelflags, outareas, maxareas );
}

int trap_AAS_AvoidDangerArea(vec3_t srcpos, int srcarea, vec3_t dangerpos, int dangerarea, float range, int travelflags) {
	return TCE_SYSCALL_6( BOTLIB_AAS_AVOIDDANGERAREA, srcpos, srcarea, dangerpos, dangerarea, PASSFLOAT(range), travelflags );
}

int trap_AAS_Retreat
(
	// Locations of the danger spots (AAS area numbers)
	int *dangerSpots,
	// The number of danger spots
	int dangerSpotCount,
	vec3_t srcpos, 
	int srcarea, 
	vec3_t dangerpos, 
	int dangerarea, 
	// Min range from startpos
	float range, 
	// Min range from danger
	float dangerRange,
	int travelflags
)
{
	return TCE_SYSCALL_9( BOTLIB_AAS_RETREAT, dangerSpots, dangerSpotCount, srcpos, srcarea, dangerpos, dangerarea, PASSFLOAT(range), PASSFLOAT(dangerRange),travelflags );
}

int trap_AAS_AlternativeRouteGoals(vec3_t start, vec3_t goal, int travelflags,
										 aas_altroutegoal_t *altroutegoals, int maxaltroutegoals,
										 int color) {
	return TCE_SYSCALL_6( BOTLIB_AAS_ALTROUTEGOALS, start, goal, travelflags, altroutegoals, maxaltroutegoals, color );
}

void trap_AAS_SetAASBlockingEntity( vec3_t absmin, vec3_t absmax, int blocking ) {
	TCE_SYSCALL_3( BOTLIB_AAS_SETAASBLOCKINGENTITY, absmin, absmax, blocking );
}

void trap_AAS_RecordTeamDeathArea( vec3_t srcpos, int srcarea, int team, int teamCount, int travelflags ) {
	TCE_SYSCALL_5( BOTLIB_AAS_RECORDTEAMDEATHAREA, srcpos, srcarea, team, teamCount, travelflags );
}
// done.

void trap_EA_Say(int client, char *str) {
	TCE_SYSCALL_2( BOTLIB_EA_SAY, client, str );
}

void trap_EA_SayTeam(int client, char *str) {
	TCE_SYSCALL_2( BOTLIB_EA_SAY_TEAM, client, str );
}

void trap_EA_UseItem(int client, char *it) {
	TCE_SYSCALL_2( BOTLIB_EA_USE_ITEM, client, it );
}

void trap_EA_DropItem(int client, char *it) {
	TCE_SYSCALL_2( BOTLIB_EA_DROP_ITEM, client, it );
}

void trap_EA_UseInv(int client, char *inv) {
	TCE_SYSCALL_2( BOTLIB_EA_USE_INV, client, inv );
}

void trap_EA_DropInv(int client, char *inv) {
	TCE_SYSCALL_2( BOTLIB_EA_DROP_INV, client, inv );
}

void trap_EA_Gesture(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_GESTURE, client );
}

void trap_EA_Command(int client, char *command) {
	TCE_SYSCALL_2( BOTLIB_EA_COMMAND, client, command );
}

void trap_EA_SelectWeapon(int client, int weapon) {
	TCE_SYSCALL_2( BOTLIB_EA_SELECT_WEAPON, client, weapon );
}

void trap_EA_Talk(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_TALK, client );
}

void trap_EA_Attack(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_ATTACK, client );
}

void trap_EA_Reload(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_RELOAD, client );
}

void trap_EA_Activate(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_USE, client );
}

void trap_EA_Respawn(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_RESPAWN, client );
}

void trap_EA_Jump(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_JUMP, client );
}

void trap_EA_DelayedJump(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_DELAYED_JUMP, client );
}

void trap_EA_Crouch(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_CROUCH, client );
}

void trap_EA_Walk(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_WALK, client );
}

void trap_EA_MoveUp(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_MOVE_UP, client );
}

void trap_EA_MoveDown(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_MOVE_DOWN, client );
}

void trap_EA_MoveForward(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_MOVE_FORWARD, client );
}

void trap_EA_MoveBack(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_MOVE_BACK, client );
}

void trap_EA_MoveLeft(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_MOVE_LEFT, client );
}

void trap_EA_MoveRight(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_MOVE_RIGHT, client );
}

void trap_EA_Move(int client, vec3_t dir, float speed) {
	TCE_SYSCALL_3( BOTLIB_EA_MOVE, client, dir, PASSFLOAT(speed) );
}

void trap_EA_View(int client, vec3_t viewangles) {
	TCE_SYSCALL_2( BOTLIB_EA_VIEW, client, viewangles );
}

void trap_EA_EndRegular(int client, float thinktime) {
	TCE_SYSCALL_2( BOTLIB_EA_END_REGULAR, client, PASSFLOAT(thinktime) );
}

void trap_EA_GetInput(int client, float thinktime, void /* struct bot_input_s */ *input) {
	TCE_SYSCALL_3( BOTLIB_EA_GET_INPUT, client, PASSFLOAT(thinktime), input );
}

void trap_EA_ResetInput(int client, void *init) {
	TCE_SYSCALL_2( BOTLIB_EA_RESET_INPUT, client, init );
}

void trap_EA_Prone(int client) {
	TCE_SYSCALL_1( BOTLIB_EA_PRONE, client );
}

int trap_BotLoadCharacter(char *charfile, int skill) {
	return TCE_SYSCALL_2( BOTLIB_AI_LOAD_CHARACTER, charfile, skill);
}

void trap_BotFreeCharacter(int character) {
	TCE_SYSCALL_1( BOTLIB_AI_FREE_CHARACTER, character );
}

float trap_Characteristic_Float(int character, int index) {
	int temp;
	temp = TCE_SYSCALL_2( BOTLIB_AI_CHARACTERISTIC_FLOAT, character, index );
	{ float value; memcpy(&value, &temp, sizeof(value)); return value; }
}

float trap_Characteristic_BFloat(int character, int index, float min, float max) {
	int temp;
	temp = TCE_SYSCALL_4( BOTLIB_AI_CHARACTERISTIC_BFLOAT, character, index, PASSFLOAT(min), PASSFLOAT(max) );
	{ float value; memcpy(&value, &temp, sizeof(value)); return value; }
}

int trap_Characteristic_Integer(int character, int index) {
	return TCE_SYSCALL_2( BOTLIB_AI_CHARACTERISTIC_INTEGER, character, index );
}

int trap_Characteristic_BInteger(int character, int index, int min, int max) {
	return TCE_SYSCALL_4( BOTLIB_AI_CHARACTERISTIC_BINTEGER, character, index, min, max );
}

void trap_Characteristic_String(int character, int index, char *buf, int size) {
	TCE_SYSCALL_4( BOTLIB_AI_CHARACTERISTIC_STRING, character, index, buf, size );
}

int trap_BotAllocChatState(void) {
	return TCE_SYSCALL_0( BOTLIB_AI_ALLOC_CHAT_STATE );
}

void trap_BotFreeChatState(int handle) {
	TCE_SYSCALL_1( BOTLIB_AI_FREE_CHAT_STATE, handle );
}

void trap_BotQueueConsoleMessage(int chatstate, int type, char *message) {
	TCE_SYSCALL_3( BOTLIB_AI_QUEUE_CONSOLE_MESSAGE, chatstate, type, message );
}

void trap_BotRemoveConsoleMessage(int chatstate, int handle) {
	TCE_SYSCALL_2( BOTLIB_AI_REMOVE_CONSOLE_MESSAGE, chatstate, handle );
}

int trap_BotNextConsoleMessage(int chatstate, void /* struct bot_consolemessage_s */ *cm) {
	return TCE_SYSCALL_2( BOTLIB_AI_NEXT_CONSOLE_MESSAGE, chatstate, cm );
}

int trap_BotNumConsoleMessages(int chatstate) {
	return TCE_SYSCALL_1( BOTLIB_AI_NUM_CONSOLE_MESSAGE, chatstate );
}

void trap_BotInitialChat(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 ) {
	TCE_SYSCALL_11( BOTLIB_AI_INITIAL_CHAT, chatstate, type, mcontext, var0, var1, var2, var3, var4, var5, var6, var7 );
}

int	trap_BotNumInitialChats(int chatstate, char *type) {
	return TCE_SYSCALL_2( BOTLIB_AI_NUM_INITIAL_CHATS, chatstate, type );
}

int trap_BotReplyChat(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 ) {
	return TCE_SYSCALL_12( BOTLIB_AI_REPLY_CHAT, chatstate, message, mcontext, vcontext, var0, var1, var2, var3, var4, var5, var6, var7 );
}

int trap_BotChatLength(int chatstate) {
	return TCE_SYSCALL_1( BOTLIB_AI_CHAT_LENGTH, chatstate );
}

void trap_BotEnterChat(int chatstate, int client, int sendto) {
	// RF, disabled
	return;
	TCE_SYSCALL_3( BOTLIB_AI_ENTER_CHAT, chatstate, client, sendto );
}

void trap_BotGetChatMessage(int chatstate, char *buf, int size) {
	TCE_SYSCALL_3( BOTLIB_AI_GET_CHAT_MESSAGE, chatstate, buf, size);
}

int trap_StringContains(char *str1, char *str2, int casesensitive) {
	return TCE_SYSCALL_3( BOTLIB_AI_STRING_CONTAINS, str1, str2, casesensitive );
}

int trap_BotFindMatch(char *str, void /* struct bot_match_s */ *match, unsigned long int context) {
	return TCE_SYSCALL_3( BOTLIB_AI_FIND_MATCH, str, match, context );
}

void trap_BotMatchVariable(void /* struct bot_match_s */ *match, int variable, char *buf, int size) {
	TCE_SYSCALL_4( BOTLIB_AI_MATCH_VARIABLE, match, variable, buf, size );
}

void trap_UnifyWhiteSpaces(char *string) {
	TCE_SYSCALL_1( BOTLIB_AI_UNIFY_WHITE_SPACES, string );
}

void trap_BotReplaceSynonyms(char *string, unsigned long int context) {
	TCE_SYSCALL_2( BOTLIB_AI_REPLACE_SYNONYMS, string, context );
}

int trap_BotLoadChatFile(int chatstate, char *chatfile, char *chatname) {
	return TCE_SYSCALL_3( BOTLIB_AI_LOAD_CHAT_FILE, chatstate, chatfile, chatname );
}

void trap_BotSetChatGender(int chatstate, int gender) {
	TCE_SYSCALL_2( BOTLIB_AI_SET_CHAT_GENDER, chatstate, gender );
}

void trap_BotSetChatName(int chatstate, char *name) {
	TCE_SYSCALL_2( BOTLIB_AI_SET_CHAT_NAME, chatstate, name );
}

void trap_BotResetGoalState(int goalstate) {
	TCE_SYSCALL_1( BOTLIB_AI_RESET_GOAL_STATE, goalstate );
}

void trap_BotResetAvoidGoals(int goalstate) {
	TCE_SYSCALL_1( BOTLIB_AI_RESET_AVOID_GOALS, goalstate );
}

void trap_BotRemoveFromAvoidGoals(int goalstate, int number) {
	TCE_SYSCALL_2( BOTLIB_AI_REMOVE_FROM_AVOID_GOALS, goalstate, number);
}

void trap_BotPushGoal(int goalstate, void /* struct bot_goal_s */ *goal) {
	TCE_SYSCALL_2( BOTLIB_AI_PUSH_GOAL, goalstate, goal );
}

void trap_BotPopGoal(int goalstate) {
	TCE_SYSCALL_1( BOTLIB_AI_POP_GOAL, goalstate );
}

void trap_BotEmptyGoalStack(int goalstate) {
	TCE_SYSCALL_1( BOTLIB_AI_EMPTY_GOAL_STACK, goalstate );
}

void trap_BotDumpAvoidGoals(int goalstate) {
	TCE_SYSCALL_1( BOTLIB_AI_DUMP_AVOID_GOALS, goalstate );
}

void trap_BotDumpGoalStack(int goalstate) {
	TCE_SYSCALL_1( BOTLIB_AI_DUMP_GOAL_STACK, goalstate );
}

void trap_BotGoalName(int number, char *name, int size) {
	TCE_SYSCALL_3( BOTLIB_AI_GOAL_NAME, number, name, size );
}

int trap_BotGetTopGoal(int goalstate, void /* struct bot_goal_s */ *goal) {
	return TCE_SYSCALL_2( BOTLIB_AI_GET_TOP_GOAL, goalstate, goal );
}

int trap_BotGetSecondGoal(int goalstate, void /* struct bot_goal_s */ *goal) {
	return TCE_SYSCALL_2( BOTLIB_AI_GET_SECOND_GOAL, goalstate, goal );
}

int trap_BotChooseLTGItem(int goalstate, vec3_t origin, int *inventory, int travelflags) {
	return TCE_SYSCALL_4( BOTLIB_AI_CHOOSE_LTG_ITEM, goalstate, origin, inventory, travelflags );
}

int trap_BotChooseNBGItem(int goalstate, vec3_t origin, int *inventory, int travelflags, void /* struct bot_goal_s */ *ltg, float maxtime) {
	return TCE_SYSCALL_6( BOTLIB_AI_CHOOSE_NBG_ITEM, goalstate, origin, inventory, travelflags, ltg, PASSFLOAT(maxtime) );
}

int trap_BotTouchingGoal(vec3_t origin, void /* struct bot_goal_s */ *goal) {
	return TCE_SYSCALL_2( BOTLIB_AI_TOUCHING_GOAL, origin, goal );
}

int trap_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void /* struct bot_goal_s */ *goal) {
	return TCE_SYSCALL_4( BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE, viewer, eye, viewangles, goal );
}

int trap_BotGetLevelItemGoal(int index, char *classname, void /* struct bot_goal_s */ *goal) {
	return TCE_SYSCALL_3( BOTLIB_AI_GET_LEVEL_ITEM_GOAL, index, classname, goal );
}

int trap_BotGetNextCampSpotGoal(int num, void /* struct bot_goal_s */ *goal) {
	return TCE_SYSCALL_2( BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL, num, goal );
}

int trap_BotGetMapLocationGoal(char *name, void /* struct bot_goal_s */ *goal) {
	return TCE_SYSCALL_2( BOTLIB_AI_GET_MAP_LOCATION_GOAL, name, goal );
}

float trap_BotAvoidGoalTime(int goalstate, int number) {
	int temp;
	temp = TCE_SYSCALL_2( BOTLIB_AI_AVOID_GOAL_TIME, goalstate, number );
	{ float value; memcpy(&value, &temp, sizeof(value)); return value; }
}

void trap_BotInitLevelItems(void) {
	TCE_SYSCALL_0( BOTLIB_AI_INIT_LEVEL_ITEMS );
}

void trap_BotUpdateEntityItems(void) {
	TCE_SYSCALL_0( BOTLIB_AI_UPDATE_ENTITY_ITEMS );
}

int trap_BotLoadItemWeights(int goalstate, char *filename) {
	return TCE_SYSCALL_2( BOTLIB_AI_LOAD_ITEM_WEIGHTS, goalstate, filename );
}

void trap_BotFreeItemWeights(int goalstate) {
	TCE_SYSCALL_1( BOTLIB_AI_FREE_ITEM_WEIGHTS, goalstate );
}

void trap_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child) {
	TCE_SYSCALL_3( BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC, parent1, parent2, child );
}

void trap_BotSaveGoalFuzzyLogic(int goalstate, char *filename) {
	TCE_SYSCALL_2( BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC, goalstate, filename );
}

void trap_BotMutateGoalFuzzyLogic(int goalstate, float range) {
	TCE_SYSCALL_2( BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC, goalstate, range );
}

int trap_BotAllocGoalState(int state) {
	return TCE_SYSCALL_1( BOTLIB_AI_ALLOC_GOAL_STATE, state );
}

void trap_BotFreeGoalState(int handle) {
	TCE_SYSCALL_1( BOTLIB_AI_FREE_GOAL_STATE, handle );
}

void trap_BotResetMoveState(int movestate) {
	TCE_SYSCALL_1( BOTLIB_AI_RESET_MOVE_STATE, movestate );
}

void trap_BotMoveToGoal(void /* struct bot_moveresult_s */ *result, int movestate, void /* struct bot_goal_s */ *goal, int travelflags) {
	TCE_SYSCALL_4( BOTLIB_AI_MOVE_TO_GOAL, result, movestate, goal, travelflags );
}

int trap_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type) {
	return TCE_SYSCALL_4( BOTLIB_AI_MOVE_IN_DIRECTION, movestate, dir, PASSFLOAT(speed), type );
}

void trap_BotResetAvoidReach(int movestate) {
	TCE_SYSCALL_1( BOTLIB_AI_RESET_AVOID_REACH, movestate );
}

void trap_BotResetLastAvoidReach(int movestate) {
	TCE_SYSCALL_1( BOTLIB_AI_RESET_LAST_AVOID_REACH,movestate  );
}

int trap_BotReachabilityArea(vec3_t origin, int testground) {
	return TCE_SYSCALL_2( BOTLIB_AI_REACHABILITY_AREA, origin, testground );
}

int trap_BotMovementViewTarget(int movestate, void /* struct bot_goal_s */ *goal, int travelflags, float lookahead, vec3_t target) {
	return TCE_SYSCALL_5( BOTLIB_AI_MOVEMENT_VIEW_TARGET, movestate, goal, travelflags, PASSFLOAT(lookahead), target );
}

int trap_BotPredictVisiblePosition(vec3_t origin, int areanum, void /* struct bot_goal_s */ *goal, int travelflags, vec3_t target) {
	return TCE_SYSCALL_5( BOTLIB_AI_PREDICT_VISIBLE_POSITION, origin, areanum, goal, travelflags, target );
}

int trap_BotAllocMoveState(void) {
	return TCE_SYSCALL_0( BOTLIB_AI_ALLOC_MOVE_STATE );
}

void trap_BotFreeMoveState(int handle) {
	TCE_SYSCALL_1( BOTLIB_AI_FREE_MOVE_STATE, handle );
}

void trap_BotInitMoveState(int handle, void /* struct bot_initmove_s */ *initmove) {
	TCE_SYSCALL_2( BOTLIB_AI_INIT_MOVE_STATE, handle, initmove );
}

// Ridah
void trap_BotInitAvoidReach(int handle) {
	TCE_SYSCALL_1( BOTLIB_AI_INIT_AVOID_REACH, handle );
}
// Done.

int trap_BotChooseBestFightWeapon(int weaponstate, int *inventory) {
	return TCE_SYSCALL_2( BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON, weaponstate, inventory );
}

void trap_BotGetWeaponInfo(int weaponstate, int weapon, void /* struct weaponinfo_s */ *weaponinfo) {
	TCE_SYSCALL_3( BOTLIB_AI_GET_WEAPON_INFO, weaponstate, weapon, weaponinfo );
}

int trap_BotLoadWeaponWeights(int weaponstate, char *filename) {
	return TCE_SYSCALL_2( BOTLIB_AI_LOAD_WEAPON_WEIGHTS, weaponstate, filename );
}

int trap_BotAllocWeaponState(void) {
	return TCE_SYSCALL_0( BOTLIB_AI_ALLOC_WEAPON_STATE );
}

void trap_BotFreeWeaponState(int weaponstate) {
	TCE_SYSCALL_1( BOTLIB_AI_FREE_WEAPON_STATE, weaponstate );
}

void trap_BotResetWeaponState(int weaponstate) {
	TCE_SYSCALL_1( BOTLIB_AI_RESET_WEAPON_STATE, weaponstate );
}

int trap_GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child) {
	return TCE_SYSCALL_5( BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION, numranks, ranks, parent1, parent2, child );
}

void trap_PbStat ( int clientNum , char *category , char *values ) {
	TCE_SYSCALL_3( PB_STAT_REPORT , clientNum , category , values ) ;
}

void trap_SendMessage( int clientNum, char *buf, int buflen ) {
	TCE_SYSCALL_3( G_SENDMESSAGE, clientNum, buf, buflen );
}

messageStatus_t trap_MessageStatus( int clientNum ) {
	return TCE_SYSCALL_1( G_MESSAGESTATUS, clientNum );
}
