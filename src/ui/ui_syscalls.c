#include "ui_local.h"
#include "../game/tce_native_syscall.h"

// this file is only included when building a dll
// syscalls.asm is included instead when building a qvm

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

void trap_Print( const char *string ) {
	TCE_SYSCALL_1( UI_PRINT, string );
}

void trap_Error( const char *string ) {
	TCE_SYSCALL_1( UI_ERROR, string );
}

int trap_Milliseconds( void ) {
	return TCE_SYSCALL_0( UI_MILLISECONDS );
}

void trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags ) {
	TCE_SYSCALL_4( UI_CVAR_REGISTER, cvar, var_name, value, flags );
}

void trap_Cvar_Update( vmCvar_t *cvar ) {
	TCE_SYSCALL_1( UI_CVAR_UPDATE, cvar );
}

void trap_Cvar_Set( const char *var_name, const char *value ) {
	TCE_SYSCALL_2( UI_CVAR_SET, var_name, value );
}

float trap_Cvar_VariableValue( const char *var_name ) {
	int temp;
	temp = TCE_SYSCALL_1( UI_CVAR_VARIABLEVALUE, var_name );
	{ float value; memcpy(&value, &temp, sizeof(value)); return value; }
}

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	TCE_SYSCALL_3( UI_CVAR_VARIABLESTRINGBUFFER, var_name, buffer, bufsize );
}

void trap_Cvar_LatchedVariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	TCE_SYSCALL_3( UI_CVAR_LATCHEDVARIABLESTRINGBUFFER, var_name, buffer, bufsize );
}

void trap_Cvar_SetValue( const char *var_name, float value ) {
	TCE_SYSCALL_2( UI_CVAR_SETVALUE, var_name, PASSFLOAT( value ) );
}

void trap_Cvar_Reset( const char *name ) {
	TCE_SYSCALL_1( UI_CVAR_RESET, name );
}

void trap_Cvar_Create( const char *var_name, const char *var_value, int flags ) {
	TCE_SYSCALL_3( UI_CVAR_CREATE, var_name, var_value, flags );
}

void trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize ) {
	TCE_SYSCALL_3( UI_CVAR_INFOSTRINGBUFFER, bit, buffer, bufsize );
}

int trap_Argc( void ) {
	return TCE_SYSCALL_0( UI_ARGC );
}

void trap_Argv( int n, char *buffer, int bufferLength ) {
	TCE_SYSCALL_3( UI_ARGV, n, buffer, bufferLength );
}

void trap_Cmd_ExecuteText( int exec_when, const char *text ) {
	TCE_SYSCALL_2( UI_CMD_EXECUTETEXT, exec_when, text );
}

void trap_AddCommand( const char *cmdName ) {
	TCE_SYSCALL_1( UI_ADDCOMMAND, cmdName );
}

int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	return TCE_SYSCALL_3( UI_FS_FOPENFILE, qpath, f, mode );
}

void trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	TCE_SYSCALL_3( UI_FS_READ, buffer, len, f );
}

void trap_FS_Write( const void *buffer, int len, fileHandle_t f ) {
	TCE_SYSCALL_3( UI_FS_WRITE, buffer, len, f );
}

void trap_FS_FCloseFile( fileHandle_t f ) {
	TCE_SYSCALL_1( UI_FS_FCLOSEFILE, f );
}

int trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize ) {
	return TCE_SYSCALL_4( UI_FS_GETFILELIST, path, extension, listbuf, bufsize );
}

int trap_FS_Delete(const char *filename) {
	return TCE_SYSCALL_1( UI_FS_DELETEFILE, filename);
}

qhandle_t trap_R_RegisterModel( const char *name ) {
	return TCE_SYSCALL_1( UI_R_REGISTERMODEL, name );
}

qhandle_t trap_R_RegisterSkin( const char *name ) {
	return TCE_SYSCALL_1( UI_R_REGISTERSKIN, name );
}

void trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
	TCE_SYSCALL_3( UI_R_REGISTERFONT, fontName, pointSize, font );
}

qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	return TCE_SYSCALL_1( UI_R_REGISTERSHADERNOMIP, name );
}

void trap_R_ClearScene( void ) {
	TCE_SYSCALL_0( UI_R_CLEARSCENE );
}

void trap_R_AddRefEntityToScene( const refEntity_t *re ) {
	TCE_SYSCALL_1( UI_R_ADDREFENTITYTOSCENE, re );
}

void trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts ) {
	TCE_SYSCALL_3( UI_R_ADDPOLYTOSCENE, hShader, numVerts, verts );
}

// ydnar: new dlight system
//%	void	trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, int overdraw ) {
//%		TCE_SYSCALL_6( UI_R_ADDLIGHTTOSCENE, org, PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b), overdraw );
//%	}
void	trap_R_AddLightToScene( const vec3_t org, float radius, float intensity, float r, float g, float b, qhandle_t hShader, int flags )
{
	TCE_SYSCALL_8( UI_R_ADDLIGHTTOSCENE, org, PASSFLOAT( radius ), PASSFLOAT( intensity ),
		PASSFLOAT( r ), PASSFLOAT( g ), PASSFLOAT( b ), hShader, flags );
}

void trap_R_AddCoronaToScene( const vec3_t org, float r, float g, float b, float scale, int id, qboolean visible ) {
	TCE_SYSCALL_7( UI_R_ADDCORONATOSCENE, org, PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b), PASSFLOAT(scale), id, visible  );
}

void trap_R_RenderScene( const refdef_t *fd ) {
	TCE_SYSCALL_1( UI_R_RENDERSCENE, fd );
}

void trap_R_SetColor( const float *rgba ) {
	TCE_SYSCALL_1( UI_R_SETCOLOR, rgba );
}

void trap_R_Add2dPolys( polyVert_t* verts, int numverts, qhandle_t hShader ) {
	TCE_SYSCALL_3( UI_R_DRAW2DPOLYS, verts, numverts, hShader );
}

void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	TCE_SYSCALL_9( UI_R_DRAWSTRETCHPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2), hShader );
}

void trap_R_DrawRotatedPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, float angle ) {
	TCE_SYSCALL_10( UI_R_DRAWROTATEDPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2), hShader, PASSFLOAT(angle) );
}

void trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) {
	TCE_SYSCALL_3( UI_R_MODELBOUNDS, model, mins, maxs );
}

void trap_UpdateScreen( void ) {
	TCE_SYSCALL_0( UI_UPDATESCREEN );
}

int trap_CM_LerpTag( orientation_t *tag, const refEntity_t *refent, const char *tagName, int startIndex ) {
	return TCE_SYSCALL_4( UI_CM_LERPTAG, tag, refent, tagName, 0 );			// NEFVE - SMF - fixed
}

void trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum ) {
	TCE_SYSCALL_3( UI_S_STARTLOCALSOUND, sfx, channelNum, 127 /* Gordon: default volume always for the moment*/ );
}

sfxHandle_t	trap_S_RegisterSound( const char *sample, qboolean compressed ) {
	int i = TCE_SYSCALL_2( UI_S_REGISTERSOUND, sample, qfalse /* compressed */ );
#ifdef DEBUG
	if(i == 0) {
		Com_Printf("^1Warning: Failed to load sound: %s\n", sample);
	}
#endif
	return i;
}

void	trap_S_FadeBackgroundTrack( float targetvol, int time, int num){	// yes, i know.  fadebackground coming in, fadestreaming going out.  will have to see where functionality leads...
	TCE_SYSCALL_3( UI_S_FADESTREAMINGSOUND, PASSFLOAT(targetvol), time, num);	// 'num' is '0' if it's music, '1' if it's "all streaming sounds"
}

void	trap_S_FadeAllSound( float targetvol, int time, qboolean stopsound ) {
	TCE_SYSCALL_3( UI_S_FADEALLSOUNDS, PASSFLOAT(targetvol), time, stopsound);
}

void trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen ) {
	TCE_SYSCALL_3( UI_KEY_KEYNUMTOSTRINGBUF, keynum, buf, buflen );
}

void trap_Key_GetBindingBuf( int keynum, char *buf, int buflen ) {
	TCE_SYSCALL_3( UI_KEY_GETBINDINGBUF, keynum, buf, buflen );
}

// binding MUST be lower case
void trap_Key_KeysForBinding( const char* binding, int* key1, int* key2 ) {
	TCE_SYSCALL_3( UI_KEY_BINDINGTOKEYS, binding, key1, key2 );
}

void trap_Key_SetBinding( int keynum, const char *binding ) {
	TCE_SYSCALL_2( UI_KEY_SETBINDING, keynum, binding );
}

qboolean trap_Key_IsDown( int keynum ) {
	return TCE_SYSCALL_1( UI_KEY_ISDOWN, keynum );
}

qboolean trap_Key_GetOverstrikeMode( void ) {
	return TCE_SYSCALL_0( UI_KEY_GETOVERSTRIKEMODE );
}

void trap_Key_SetOverstrikeMode( qboolean state ) {
	TCE_SYSCALL_1( UI_KEY_SETOVERSTRIKEMODE, state );
}

void trap_Key_ClearStates( void ) {
	TCE_SYSCALL_0( UI_KEY_CLEARSTATES );
}

int trap_Key_GetCatcher( void ) {
	return TCE_SYSCALL_0( UI_KEY_GETCATCHER );
}

void trap_Key_SetCatcher( int catcher ) {
	TCE_SYSCALL_1( UI_KEY_SETCATCHER, catcher );
}

void trap_GetClipboardData( char *buf, int bufsize ) {
	TCE_SYSCALL_2( UI_GETCLIPBOARDDATA, buf, bufsize );
}

void trap_GetClientState( uiClientState_t *state ) {
	TCE_SYSCALL_1( UI_GETCLIENTSTATE, state );
}

void trap_GetGlconfig( glconfig_t *glconfig ) {
	TCE_SYSCALL_1( UI_GETGLCONFIG, glconfig );
}

int trap_GetConfigString( int index, char* buff, int buffsize ) {
	return TCE_SYSCALL_3( UI_GETCONFIGSTRING, index, buff, buffsize );
}

int	trap_LAN_GetLocalServerCount( void ) {
	return TCE_SYSCALL_0( UI_LAN_GETLOCALSERVERCOUNT );
}

void trap_LAN_GetLocalServerAddressString( int n, char *buf, int buflen ) {
	TCE_SYSCALL_3( UI_LAN_GETLOCALSERVERADDRESSSTRING, n, buf, buflen );
}

int trap_LAN_GetGlobalServerCount( void ) {
	return TCE_SYSCALL_0( UI_LAN_GETGLOBALSERVERCOUNT );
}

void trap_LAN_GetGlobalServerAddressString( int n, char *buf, int buflen ) {
	TCE_SYSCALL_3( UI_LAN_GETGLOBALSERVERADDRESSSTRING, n, buf, buflen );
}

int trap_LAN_GetPingQueueCount( void ) {
	return TCE_SYSCALL_0( UI_LAN_GETPINGQUEUECOUNT );
}

void trap_LAN_ClearPing( int n ) {
	TCE_SYSCALL_1( UI_LAN_CLEARPING, n );
}

void trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime ) {
	TCE_SYSCALL_4( UI_LAN_GETPING, n, buf, buflen, pingtime );
}

void trap_LAN_GetPingInfo( int n, char *buf, int buflen ) {
	TCE_SYSCALL_3( UI_LAN_GETPINGINFO, n, buf, buflen );
}

// NERVE - SMF
qboolean trap_LAN_UpdateVisiblePings( int source ) {
	return TCE_SYSCALL_1( UI_LAN_UPDATEVISIBLEPINGS, source );
}

int	trap_LAN_GetServerCount( int source ) {
	return TCE_SYSCALL_1( UI_LAN_GETSERVERCOUNT, source );
}

int trap_LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 ) {
	return TCE_SYSCALL_5( UI_LAN_COMPARESERVERS, source, sortKey, sortDir, s1, s2 );
}

void trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen ) {
	TCE_SYSCALL_4( UI_LAN_GETSERVERADDRESSSTRING, source, n, buf, buflen );
}

void trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen ) {
	TCE_SYSCALL_4( UI_LAN_GETSERVERINFO, source, n, buf, buflen );
}

int trap_LAN_AddServer(int source, const char *name, const char *addr) {
	return TCE_SYSCALL_3( UI_LAN_ADDSERVER, source, name, addr );
}

void trap_LAN_RemoveServer(int source, const char *addr) {
	TCE_SYSCALL_2( UI_LAN_REMOVESERVER, source, addr );
}

int trap_LAN_GetServerPing( int source, int n ) {
	return TCE_SYSCALL_2( UI_LAN_GETSERVERPING, source, n );
}

int trap_LAN_ServerIsVisible( int source, int n) {
	return TCE_SYSCALL_2( UI_LAN_SERVERISVISIBLE, source, n );
}

int trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen ) {
	return TCE_SYSCALL_3( UI_LAN_SERVERSTATUS, serverAddress, serverStatus, maxLen );
}

qboolean trap_LAN_ServerIsInFavoriteList( int source, int n  ) {
	return TCE_SYSCALL_2( UI_LAN_SERVERISINFAVORITELIST, source, n );
}

void trap_LAN_SaveCachedServers() {
	TCE_SYSCALL_0( UI_LAN_SAVECACHEDSERVERS );
}

void trap_LAN_LoadCachedServers() {
	TCE_SYSCALL_0( UI_LAN_LOADCACHEDSERVERS );
}

void trap_LAN_MarkServerVisible( int source, int n, qboolean visible ) {
	TCE_SYSCALL_3( UI_LAN_MARKSERVERVISIBLE, source, n, visible );
}

// DHM - Nerve :: PunkBuster
void trap_SetPbClStatus( int status ) {
	TCE_SYSCALL_1( UI_SET_PBCLSTATUS, status );
}
// DHM - Nerve

// TTimo: also for Sv
void trap_SetPbSvStatus( int status ) {
	TCE_SYSCALL_1( UI_SET_PBSVSTATUS, status );
}

void trap_LAN_ResetPings(int n) {
	TCE_SYSCALL_1( UI_LAN_RESETPINGS, n );
}
// -NERVE - SMF

int trap_MemoryRemaining( void ) {
	return TCE_SYSCALL_0( UI_MEMORY_REMAINING );
}

void trap_GetCDKey( char *buf, int buflen ) {
	TCE_SYSCALL_2( UI_GET_CDKEY, buf, buflen );
}

void trap_SetCDKey( char *buf ) {
	TCE_SYSCALL_1( UI_SET_CDKEY, buf );
}

int trap_PC_AddGlobalDefine( char *define ) {
	return TCE_SYSCALL_1( UI_PC_ADD_GLOBAL_DEFINE, define );
}

int trap_PC_RemoveAllGlobalDefines( void ) {
	return TCE_SYSCALL_0( UI_PC_REMOVE_ALL_GLOBAL_DEFINES );
}

int trap_PC_LoadSource( const char *filename ) {
	return TCE_SYSCALL_1( UI_PC_LOAD_SOURCE, filename );
}

int trap_PC_FreeSource( int handle ) {
	return TCE_SYSCALL_1( UI_PC_FREE_SOURCE, handle );
}

int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	return TCE_SYSCALL_2( UI_PC_READ_TOKEN, handle, pc_token );
}

int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	return TCE_SYSCALL_3( UI_PC_SOURCE_FILE_AND_LINE, handle, filename, line );
}

int trap_PC_UnReadToken( int handle ) {
	return TCE_SYSCALL_1( UI_PC_UNREAD_TOKEN, handle );
}

void trap_S_StopBackgroundTrack( void ) {
	TCE_SYSCALL_0( UI_S_STOPBACKGROUNDTRACK );
}

void trap_S_StartBackgroundTrack( const char *intro, const char *loop, int fadeupTime) {
	TCE_SYSCALL_3( UI_S_STARTBACKGROUNDTRACK, intro, loop, fadeupTime );
}

int trap_RealTime(qtime_t *qtime) {
	return TCE_SYSCALL_1( UI_REAL_TIME, qtime );
}

// this returns a handle.  arg0 is the name in the format "idlogo.roq", set arg1 to NULL, alteredstates to qfalse (do not alter gamestate)
int trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits) {
  return TCE_SYSCALL_6(UI_CIN_PLAYCINEMATIC, arg0, xpos, ypos, width, height, bits);
}
 
// stops playing the cinematic and ends it.  should always return FMV_EOF
// cinematics must be stopped in reverse order of when they are started
e_status trap_CIN_StopCinematic(int handle) {
  return TCE_SYSCALL_1(UI_CIN_STOPCINEMATIC, handle);
}


// will run a frame of the cinematic but will not draw it.  Will return FMV_EOF if the end of the cinematic has been reached.
e_status trap_CIN_RunCinematic (int handle) {
  return TCE_SYSCALL_1(UI_CIN_RUNCINEMATIC, handle);
}
 

// draws the current frame
void trap_CIN_DrawCinematic (int handle) {
  TCE_SYSCALL_1(UI_CIN_DRAWCINEMATIC, handle);
}
 

// allows you to resize the animation dynamically
void trap_CIN_SetExtents (int handle, int x, int y, int w, int h) {
  TCE_SYSCALL_5(UI_CIN_SETEXTENTS, handle, x, y, w, h);
}


void	trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset ) {
	TCE_SYSCALL_3( UI_R_REMAP_SHADER, oldShader, newShader, timeOffset );
}

qboolean trap_VerifyCDKey( const char *key, const char *chksum) {
	return TCE_SYSCALL_2( UI_VERIFY_CDKEY, key, chksum);
}

// NERVE - SMF
qboolean trap_GetLimboString( int index, char *buf ) {
	return TCE_SYSCALL_2( UI_CL_GETLIMBOSTRING, index, buf );
}

#define	MAX_VA_STRING		32000

char* trap_TranslateString( const char *string ) {
	static char staticbuf[2][MAX_VA_STRING];
	static int bufcount = 0;
	char *buf;

	buf = staticbuf[bufcount++ % 2];

#ifdef LOCALIZATION_SUPPORT
	TCE_SYSCALL_2( UI_CL_TRANSLATE_STRING, string, buf );
#else
	Q_strncpyz( buf, string, MAX_VA_STRING );
#endif // LOCALIZATION_SUPPORT
	return buf;
}
// -NERVE - SMF

// DHM - Nerve
void trap_CheckAutoUpdate( void ) {
	TCE_SYSCALL_0( UI_CHECKAUTOUPDATE );
}

void trap_GetAutoUpdate( void ) {
	TCE_SYSCALL_0( UI_GET_AUTOUPDATE );
}
// DHM - Nerve

void trap_openURL( const char *s ) {
	TCE_SYSCALL_1( UI_OPENURL, s );
}

void trap_GetHunkData( int* hunkused, int* hunkexpected ) {
	TCE_SYSCALL_2( UI_GETHUNKDATA, hunkused, hunkexpected );
}
