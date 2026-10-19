#include "tce_video_modes.h"
// Copyright (C) 1999-2000 Id Software, Inc.
//
/**********************************************************************
	UI_ATOMS.C

	User interface building blocks and support functions.
**********************************************************************/
#include "ui_local.h"

uiStatic_t uis;
qboolean   m_entersound;            // after a frame, so caching won't disrupt the sound

// these are here so the functions in q_shared.c can link
#ifndef UI_HARD_LINKED


// JPW NERVE added Com_DPrintf
#define MAXPRINTMSG 4096
void QDECL Com_DPrintf(const char *fmt, ...)
{
	va_list argptr;
	char    msg[MAXPRINTMSG];
	int     developer;

	developer = trap_Cvar_VariableValue("developer");
	if (!developer)
	{
		return;
	}

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Printf("%s", msg);
}
// jpw

void QDECL Com_Error(int level, const char *error, ...)
{
	va_list argptr;
	char    text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	trap_Error(va("%s", text));
}

void QDECL Com_Printf(const char *msg, ...)
{
	va_list argptr;
	char    text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	trap_Print(va("%s", text));
}

#endif

/*
=================
UI_ClampCvar
=================
*/
float UI_ClampCvar(float min, float max, float value)
{
	if (value < min)
	{
		return min;
	}
	if (value > max)
	{
		return max;
	}
	return value;
}

/*
// TTimo: unused
static void NeedCDAction( qboolean result ) {
	if ( !result ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "quit\n" );
	}
}

static void NeedCDKeyAction( qboolean result ) {
	if ( !result ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "quit\n" );
	}
}
*/

char *UI_Argv(int arg)
{
	static char buffer[MAX_STRING_CHARS];

	trap_Argv(arg, buffer, sizeof(buffer));

	return buffer;
}


char *UI_Cvar_VariableString(const char *var_name)
{
	static char buffer[2][MAX_STRING_CHARS];
	static int  toggle;

	toggle ^= 1;        // flip-flop to allow two returns without clash

	trap_Cvar_VariableStringBuffer(var_name, buffer[toggle], sizeof(buffer[0]));

	return buffer[toggle];
}



void UI_LoadBestScores(const char *map, int game)
{
}

/*
===============
UI_ClearScores
===============
*/
void UI_ClearScores()
{
}



static void UI_Cache_f()
{
	Display_CacheAll();
}

/*
=======================
UI_CalcPostGameStats
=======================
*/
static void UI_CalcPostGameStats()
{
}


/* Typed fields in the optional local DMS menu never interpolate arbitrary
 * console text. The console aliases are also used by older OB_DMS dialogs. */
static qboolean UI_OmniNumericProperty(void)
{
	char   property[32], value[80], *end;
	double number, maximum;
	int    i, dots = 0;
	trap_Argv(1, property, sizeof(property));
	maximum = !Q_stricmp(property, "timeout") ? 5000.0 :
	          (!Q_stricmp(property, "radius") ? 999.0 : 9999.0);
	if (trap_Argc() != 2 || (Q_stricmp(property, "timeout") && Q_stricmp(property, "radius") &&
	                         Q_stricmp(property, "mincamptime") && Q_stricmp(property, "maxcamptime")))
	{
		return qfalse;
	}
	trap_Cvar_VariableStringBuffer("ui_omnibotPropertyValue", value, sizeof(value));
	for (i = 0; value[i]; ++i)
	{
		if (value[i] == '.')
		{
			if (++dots > 1)
			{
				break;
			}
		}
		else if (value[i] < '0' || value[i] > '9')
		{
			break;
		}
	}
	if (!i || i > 16 || value[i])
	{
		return qfalse;
	}
	number = strtod(value, &end);
	if (*end || number < 0.0 || number > maximum || (!Q_stricmp(property, "timeout") && number != (int)number))
	{
		return qfalse;
	}
	trap_Cmd_ExecuteText(EXEC_APPEND, va("bot goal_setproperty %s %s\n", property, value));
	return qtrue;
}

/*
=================
UI_ConsoleCommand
=================
*/
qboolean UI_ConsoleCommand(int realTime)
{
	char            *cmd;
	uiClientState_t cstate;

	uiInfo.uiDC.frameTime = realTime - uiInfo.uiDC.realTime;
	uiInfo.uiDC.realTime  = realTime;

	cmd = UI_Argv(0);
	/* The optional editor belongs to a local authoring game, never a remote client. */
	if ((!Q_stricmp(cmd, "omnibot_menu") || !Q_stricmp(cmd, "omnibot_goalname") ||
	     !Q_stricmp(cmd, "omnibot_goalgroup") || !Q_stricmp(cmd, "omnibot_waypointname") ||
	     !Q_stricmp(cmd, "omnibot_goalproperty")) && !trap_Cvar_VariableValue("sv_running"))
	{
		trap_Print("Omni-bot editor is available only in a local authoring game.\n");
		return qtrue;
	}
	if (!Q_stricmp(cmd, "omnibot_goalproperty"))
	{
		if (!UI_OmniNumericProperty())
		{
			trap_Print("Invalid goal property/value. Use radius 0..999, timeout 0..5000 ms, camp time 0..9999 s.\n");
		}
		return qtrue;
	}
	if (!Q_stricmp(cmd, "omnibot_waypointname"))
	{
		char name[80];
		int  i;
		trap_Cvar_VariableStringBuffer("ui_omnibotGoalName", name, sizeof(name));
		for (i = 0; name[i]; ++i)
		{
			if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z') ||
			      (name[i] >= '0' && name[i] <= '9') || name[i] == '_' || name[i] == ' ' || name[i] == '-' || name[i] == '.'))
			{
				break;
			}
		}
		if (i > 64 || name[i])
		{
			trap_Print("Waypoint name: use up to 64 letters, digits, spaces, underscores, dots or hyphens.\n");
		}
		else
		{
			trap_Cmd_ExecuteText(EXEC_APPEND, va("bot waypoint_setname%s%s\n", i?" ":"", name));
		}
		return qtrue;
	}
	if (!Q_stricmp(cmd, "omnibot_goalname") || !Q_stricmp(cmd, "omnibot_goalgroup"))
	{
		char name[128];
		int  i;
		trap_Cvar_VariableStringBuffer("ui_omnibotGoalName", name, sizeof(name));
		for (i = 0; name[i]; ++i)
		{
			if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z') ||
			      (name[i] >= '0' && name[i] <= '9') || name[i] == '_'))
			{
				break;
			}
		}
		if ((!i && !Q_stricmp(cmd, "omnibot_goalname")) || i > 64 || name[i])
		{
			trap_Print("Goal name: use 1-64 letters, digits or underscores.\n");
		}
		else
		{
			trap_Cmd_ExecuteText(EXEC_APPEND, va("bot goal_setproperty %s %s\n",
			                                     !Q_stricmp(cmd, "omnibot_goalgroup") ? "group" : "tagname", name));
		}
		return qtrue;
	}
	if (!Q_stricmp(cmd, "omnibot_menu"))
	{
		trap_GetClientState(&cstate);
		if (cstate.connState != CA_ACTIVE)
		{
			trap_Print("Omni-bot editor requires a running local game.\n");
			return qtrue;
		}
		Menus_CloseAll();
		if (Menus_ActivateByName("tce_omnibot", qtrue))
		{
			trap_Key_SetCatcher(KEYCATCH_UI);
		}
		else
		{
			trap_Print("Omni-bot menu missing: install the matching TCE2 UI assets.\n");
		}
		return qtrue;
	}
	if (!Q_stricmp(cmd, "tce_mode"))
	{
		char arg[32], *end; long mode;
		trap_Argv(1, arg, sizeof(arg)); mode = strtol(arg, &end, 10);
		if (trap_Argc() != 2 || !arg[0] || *end || mode < 3 || mode > 22)
		{
			trap_Print("Usage: tce_mode <3..22> (TC resolution, then vid_restart)\n");
		}
		else
		{
			TCE_UI_ApplyVideoMode((int)mode);
			trap_Cmd_ExecuteText(EXEC_INSERT, "vid_restart\n");
		}
		return qtrue;
	}


	// ensure minimum menu data is available
	//Menu_Cache();

	if (Q_stricmp(cmd, "ui_test") == 0)
	{
		UI_ShowPostGame(qtrue);
	}

	if (Q_stricmp(cmd, "ui_report") == 0)
	{
		UI_Report();
		return qtrue;
	}

	if (Q_stricmp(cmd, "ui_load") == 0)
	{
		UI_Load();
		return qtrue;
	}

	// Arnout: we DEFINATELY do NOT want this here
	/*if ( Q_stricmp (cmd, "remapShader") == 0 ) {
		if (trap_Argc() == 4) {
			char shader1[MAX_QPATH];
			char shader2[MAX_QPATH];
			Q_strncpyz(shader1, UI_Argv(1), sizeof(shader1));
			Q_strncpyz(shader2, UI_Argv(2), sizeof(shader2));
			trap_R_RemapShader(shader1, shader2, UI_Argv(3));
			return qtrue;
		}
	}*/

	if (Q_stricmp(cmd, "postgame") == 0)
	{
		UI_CalcPostGameStats();
		return qtrue;
	}

	if (Q_stricmp(cmd, "ui_cache") == 0)
	{
		UI_Cache_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "ui_teamOrders") == 0)
	{
		//UI_TeamOrdersMenu_f();
		return qtrue;
	}


	if (Q_stricmp(cmd, "ui_cdkey") == 0)
	{
		//UI_CDKeyMenu_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "iamacheater") == 0)
	{
		int i;

		// unlock all available levels and campaigns for SP
		for ( i = 0; i < uiInfo.campaignCount; i++ )
		{
			if (uiInfo.campaignList[i].typeBits & (1 << GT_SINGLE_PLAYER))
			{
				uiInfo.campaignList[i].unlocked = qtrue;
				uiInfo.campaignList[i].progress = uiInfo.campaignList[i].mapCount;
			}
		}
		return qtrue;
	}

	trap_GetClientState(&cstate);
	if (cstate.connState == CA_DISCONNECTED)
	{
		if (Q_stricmp(cmd, "campaign") == 0)
		{
			UI_Campaign_f();
			return qtrue;
		}

		if (Q_stricmp(cmd, "listcampaigns") == 0)
		{
			UI_ListCampaigns_f();
			return qtrue;
		}
	}

	return qfalse;
}

/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown(void)
{
}

/*
================
UI_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
#include "tce_ui_coordinates.h"
void UI_AdjustFrom640(float *x, float *y, float *w, float *h)
{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	int         screenWidth  = uiInfo.uiDC.glconfig.vidWidth;
	int         screenHeight = uiInfo.uiDC.glconfig.vidHeight;
	float       *scaleX      = &uiInfo.uiDC.xscale;
	float       *scaleY      = &uiInfo.uiDC.yscale;
	const float horizontal   = 0.750000059604644775390625f;
	const float aspect       = 0.00117187504656612873077392578125f;
	long double ratio, shifted, value;

	/* The original compares modulo32 products, not signed C overflow. */
	if ((unsigned)screenWidth * 480u != (unsigned)screenHeight * 640u)
	{
		ratio = (long double)screenWidth * 480.0L;
#if defined(__linux__)
		{
			long double reciprocal = 1.0L / (long double)screenHeight;
			ratio = ratio * reciprocal;
		}
		value   = (long double)*scaleX * (long double)*x;
		ratio   = ratio * (long double)aspect;
		shifted = 1.0L / ratio;
		shifted = shifted - 1.0L;
		shifted = shifted * 240.0L;
		value   = value * (long double)horizontal;
		*x      = (float)value;
		shifted = shifted + (long double)*y;
		/* The Float store must not replace the retained value below. */
		*(volatile float *)y = (float)shifted;
		value                = (long double)*scaleY * ratio;
		value                = shifted * value;
		*y                   = (float)value;
		value                = (long double)*scaleX * (long double)*w;
		value                = (long double)horizontal * value;
		*w                   = (float)value;
		value                = ratio * (long double)*scaleY;
		value                = value * (long double)*h;
		*h                   = (float)value;
#else
		ratio                = ratio / (long double)screenHeight;
		ratio                = ratio * (long double)aspect;
		value                = (long double)*scaleX * (long double)*x;
		value                = value * (long double)horizontal;
		*x                   = (float)value;
		shifted              = 1.0L / ratio;
		shifted              = shifted - 1.0L;
		shifted              = shifted * 240.0L;
		shifted              = shifted + (long double)*y;
		*(volatile float *)y = (float)shifted;
		value                = shifted * (long double)*scaleY;
		value                = value * ratio;
		*y                   = (float)value;
		value                = (long double)*scaleX * (long double)*w;
		value                = value * (long double)horizontal;
		*w                   = (float)value;
		value                = (long double)*scaleY * (long double)*h;
		value                = value * ratio;
		*h                   = (float)value;
#endif
	}
	else
	{
		value = (long double)*scaleX * (long double)*x;
		value = value * (long double)horizontal;
		*x    = (float)value;
#if defined(__linux__)
		shifted = 80.0L + (long double)*y;
#else
		shifted = (long double)*y + 80.0L;
#endif
		*(volatile float *)y = (float)shifted;
		value                = shifted * (long double)*scaleY;
		value                = value * (long double)horizontal;
		*y                   = (float)value;
		value                = (long double)*scaleX * (long double)*w;
		value                = value * (long double)horizontal;
		*w                   = (float)value;
		value                = (long double)*scaleY * (long double)*h;
		value                = value * (long double)horizontal;
		*h                   = (float)value;
	}
#else
	TCE_UI_AdjustCoordinates(x, y, w, h, uiInfo.uiDC.glconfig.vidWidth, uiInfo.uiDC.glconfig.vidHeight, uiInfo.uiDC.xscale, uiInfo.uiDC.yscale);
#endif
}

void UI_DrawNamedPic(float x, float y, float width, float height, const char *picname)
{
	qhandle_t hShader;

	hShader = trap_R_RegisterShaderNoMip(picname);
	UI_AdjustFrom640(&x, &y, &width, &height);
	trap_R_DrawStretchPic(x, y, width, height, 0, 0, 1, 1, hShader);
}

/* The picture callback ABI is eight binary32 slots and one 32-bit handle. */
typedef char tceUIHandlePic855ABI[
		(sizeof(float) == 4 && sizeof(qhandle_t) == 4 && sizeof(unsigned int) == 4 &&
		 UINT_MAX == 0xffffffffu && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
		 FLT_MAX_EXP == 128)?1:-1];
#if (defined(_MSC_VER) && defined(_M_IX86)) || \
	(defined(__GNUC__) && defined(__i386__) && defined(__linux__))
typedef struct
{
	float x, y, width, height, s0, t0, s1, t1;
	qhandle_t shader;
} tceUIDrawPicFrame855_t;

/* Windows flips unordered width; Linux leaves that input untouched.
 * Both platforms flip unordered height. Classify bits independently of
 * compiler floating-point comparison assumptions. */
static void TCE_UI_PrepareHandlePic855(tceUIDrawPicFrame855_t *pic)
{
	unsigned int widthBits, heightBits, widthMagnitude, heightMagnitude;
	int          flipWidth, flipHeight;
	memcpy(&widthBits, &pic->width, sizeof(widthBits));
	widthMagnitude = widthBits & 0x7fffffffu;
	flipWidth      = (widthBits >> 31) && widthMagnitude != 0 &&
	                 widthMagnitude <= 0x7f800000u;
#if defined(_WIN32)
	flipWidth = flipWidth || widthMagnitude > 0x7f800000u;
#endif
	if (flipWidth)
	{
#if defined(_WIN32)
		pic->width = (float)(-(long double)pic->width);
		pic->s0    = 1.0f;
		pic->s1    = 0.0f;
#else
		pic->s0    = 1.0f;
		pic->s1    = 0.0f;
		pic->width = (float)(-(long double)pic->width);
#endif
	}
	else
	{
		pic->s0 = 0.0f;
		pic->s1 = 1.0f;
	}
	memcpy(&heightBits, &pic->height, sizeof(heightBits));
	heightMagnitude = heightBits & 0x7fffffffu;
	flipHeight      = ((heightBits >> 31) && heightMagnitude != 0 &&
	                   heightMagnitude <= 0x7f800000u) || heightMagnitude > 0x7f800000u;
	if (flipHeight)
	{
#if defined(_WIN32)
		pic->height = (float)(-(long double)pic->height);
		pic->t0     = 1.0f;
		pic->t1     = 0.0f;
#else
		pic->t1     = 0.0f;
		pic->t0     = 1.0f;
		pic->height = (float)(-(long double)pic->height);
#endif
	}
	else
	{
		pic->t0 = 0.0f;
		pic->t1 = 1.0f;
	}
}

/* Typed renderer boundary. Raw native argument-bit/exception parity still
 * needs compiler/ABI validation; this is not an assembly calling shim. */
static void TCE_UI_SubmitHandlePic855(const tceUIDrawPicFrame855_t *pic)
{
#if defined(__linux__)
	/* Original ELF loads/stores t0 before issuing the syscall wrapper. */
	volatile float t0 = pic->t0;
	trap_R_DrawStretchPic(pic->x, pic->y, pic->width, pic->height,
	                      pic->s0, t0, pic->s1, pic->t1, pic->shader);
#else
	trap_R_DrawStretchPic(pic->x, pic->y, pic->width, pic->height,
	                      pic->s0, pic->t0, pic->s1, pic->t1, pic->shader);
#endif
}
#endif
void UI_DrawHandlePic(float x, float y, float w, float h, qhandle_t hShader)
{
#if (defined(_MSC_VER) && defined(_M_IX86)) || \
	(defined(__GNUC__) && defined(__i386__) && defined(__linux__))
	tceUIDrawPicFrame855_t tcePic;
	memcpy(&tcePic.x, &x, sizeof(float)); memcpy(&tcePic.y, &y, sizeof(float));
	memcpy(&tcePic.width, &w, sizeof(float)); memcpy(&tcePic.height, &h, sizeof(float));
	tcePic.shader = hShader;
	TCE_UI_PrepareHandlePic855(&tcePic);
	UI_AdjustFrom640(&tcePic.x, &tcePic.y, &tcePic.width, &tcePic.height);
	TCE_UI_SubmitHandlePic855(&tcePic);
#else
	/* Defined numerical fallback, without native x87 flags or spill parity.
	 * Raw NaN classification also remains explicit under fast-math options. */
	float        s0, s1, t0, t1;
	unsigned int tceWidthBits, tceHeightBits, tceWidthMagnitude, tceHeightMagnitude;
	qboolean     tceFlipWidth, tceFlipHeight;
	memcpy(&tceWidthBits, &w, sizeof(tceWidthBits));
	memcpy(&tceHeightBits, &h, sizeof(tceHeightBits));
	tceWidthMagnitude  = tceWidthBits & 0x7fffffffu;
	tceHeightMagnitude = tceHeightBits & 0x7fffffffu;
	tceFlipWidth       = (tceWidthBits >> 31) && tceWidthMagnitude != 0 &&
	                     tceWidthMagnitude <= 0x7f800000u;
#if !defined(__linux__)
	tceFlipWidth = tceFlipWidth || tceWidthMagnitude > 0x7f800000u;
#endif
	tceFlipHeight = ((tceHeightBits >> 31) && tceHeightMagnitude != 0 &&
	                 tceHeightMagnitude <= 0x7f800000u) || tceHeightMagnitude > 0x7f800000u;
	if (tceFlipWidth)
	{
		tceWidthBits ^= 0x80000000u;
		memcpy(&w, &tceWidthBits, sizeof(w));
		s0 = 1.f; s1 = 0.f;
	}
	else
	{
		s0 = 0.f; s1 = 1.f;
	}
	if (tceFlipHeight)
	{
		tceHeightBits ^= 0x80000000u;
		memcpy(&h, &tceHeightBits, sizeof(h));
		t0 = 1.f; t1 = 0.f;
	}
	else
	{
		t0 = 0.f; t1 = 1.f;
	}
	UI_AdjustFrom640(&x, &y, &w, &h);
	trap_R_DrawStretchPic(x, y, w, h, s0, t0, s1, t1, hShader);
#endif
}

/*
================
UI_DrawRotatedPic

Coordinates are 640*480 virtual values
=================
*/
void UI_DrawRotatedPic(float x, float y, float width, float height, qhandle_t hShader, float angle)
{

	UI_AdjustFrom640(&x, &y, &width, &height);

	trap_R_DrawRotatedPic(x, y, width, height, 0, 0, 1, 1, hShader, angle);
}

/*
================
UI_FillRect

Coordinates are 640*480 virtual values
=================
*/
void UI_FillRect(float x, float y, float width, float height, const float *color)
{
	trap_R_SetColor(color);

	UI_AdjustFrom640(&x, &y, &width, &height);
	trap_R_DrawStretchPic(x, y, width, height, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);

	trap_R_SetColor(NULL);
}

void UI_DrawSides(float x, float y, float w, float h)
{
	UI_AdjustFrom640(&x, &y, &w, &h);
	trap_R_DrawStretchPic(x, y, 1, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
	trap_R_DrawStretchPic(x + w - 1, y, 1, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

void UI_DrawTopBottom(float x, float y, float w, float h)
{
	UI_AdjustFrom640(&x, &y, &w, &h);
	trap_R_DrawStretchPic(x, y, w, 1, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
	trap_R_DrawStretchPic(x, y + h - 1, w, 1, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}
/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void UI_DrawRect(float x, float y, float width, float height, const float *color)
{
	trap_R_SetColor(color);

	UI_DrawTopBottom(x, y, width, height);
	UI_DrawSides(x, y, width, height);

	trap_R_SetColor(NULL);
}

void UI_SetColor(const float *rgba)
{
	trap_R_SetColor(rgba);
}

void UI_UpdateScreen(void)
{
	trap_UpdateScreen();
}


void UI_DrawTextBox(int x, int y, int width, int lines)
{
	UI_FillRect(x + BIGCHAR_WIDTH / 2, y + BIGCHAR_HEIGHT / 2, (width + 1) * BIGCHAR_WIDTH, (lines + 1) * BIGCHAR_HEIGHT, colorBlack);
	UI_DrawRect(x + BIGCHAR_WIDTH / 2, y + BIGCHAR_HEIGHT / 2, (width + 1) * BIGCHAR_WIDTH, (lines + 1) * BIGCHAR_HEIGHT, colorWhite);
}

qboolean UI_CursorInRect(int x, int y, int width, int height)
{
	if (uiInfo.uiDC.cursorx < x ||
	    uiInfo.uiDC.cursory < y ||
	    uiInfo.uiDC.cursorx > x + width ||
	    uiInfo.uiDC.cursory > y + height)
	{
		return qfalse;
	}

	return qtrue;
}
