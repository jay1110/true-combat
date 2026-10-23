#include <stddef.h>
#include "tce_player_hud.h"
#include "tce_warmup.h"
#include "tce_centerprint.h"
#include "tce_hud_messages.h"
#include "../game/tce_bg.h"
// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay

#include "cg_local.h"
#include "tce_camera_wave.h"
#if defined(_MSC_VER) && defined(_M_IX86)
#include "../game/tce_x87_sum.h"
#endif
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
#include "../game/tce_numeric.h"
#endif
#include "tce_levelshot_fade.h"
#include "tce_weapon_media.h"
#include "tce_lightgrid.h"
#include "tce_flash.h"

#define STATUSBARHEIGHT 452
char * BindingFromName(const char *cvar);
void Controls_GetConfig(void);
void SetHeadOrigin(clientInfo_t *ci, playerInfo_t *pi);
void CG_DrawOverlays();
int activeFont;

#define TEAM_OVERLAY_TIME 1000

////////////////////////
////////////////////////
////// new hud stuff
///////////////////////
///////////////////////

void CG_Text_SetActiveFont(int font)
{
	activeFont = font;
}

int CG_Text_Width_Ext(const char *text, float scale, int limit, fontInfo_t *font)
{
	int         count, len;
	glyphInfo_t *glyph;
	const char  *s = text;
	float       out, useScale = scale * font->glyphScale;

	out = 0;
	if (text)
	{
		len = Q_strlenInt(text);
		if (limit > 0 && len > limit)
		{
			len = limit;
		}
		count = 0;
		while (s && *s && count < len)
		{
			if (Q_IsColorString(s))
			{
				s += 2;
				continue;
			}
			else
			{
				glyph = &font->glyphs[(unsigned char)*s];
				out  += glyph->xSkip;
				s++;
				count++;
			}
		}
	}

	return out * useScale;
}



int CG_Text_Width(const char *text, float scale, int limit)
{
	fontInfo_t *font = &cgDC.Assets.fonts[activeFont];

	return CG_Text_Width_Ext(text, scale, limit, font);
}

int CG_Text_Height_Ext(const char *text, float scale, int limit, fontInfo_t *font)
{
	int         len, count;
	float       max;
	glyphInfo_t *glyph;
	float       useScale;
	const char  *s = text;

	useScale = scale * font->glyphScale;
	max      = 0;
	if (text)
	{
		len = Q_strlenInt(text);
		if (limit > 0 && len > limit)
		{
			len = limit;
		}
		count = 0;
		while (s && *s && count < len)
		{
			if (Q_IsColorString(s))
			{
				s += 2;
				continue;
			}
			else
			{
				glyph = &font->glyphs[(unsigned char)*s];
				if (max < glyph->height)
				{
					max = glyph->height;
				}
				s++;
				count++;
			}
		}
	}
	return max * useScale;
}



int CG_Text_Height(const char *text, float scale, int limit)
{
	fontInfo_t *font = &cgDC.Assets.fonts[activeFont];

	return CG_Text_Height_Ext(text, scale, limit, font);
}

void CG_Text_PaintChar_Ext(float x, float y, float w, float h, float scalex, float scaley, float s, float t, float s2, float t2, qhandle_t hShader)
{
	w *= scalex;
	h *= scaley;
	CG_AdjustFrom640(&x, &y, &w, &h);
	trap_R_DrawStretchPic(x, y, w, h, s, t, s2, t2, hShader);
}

void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader)
{
	float w, h;
	w = width * scale;
	h = height * scale;
	CG_AdjustFrom640(&x, &y, &w, &h);
	trap_R_DrawStretchPic(x, y, w, h, s, t, s2, t2, hShader);
}

void CG_Text_Paint_Centred_Ext(float x, float y, float scalex, float scaley, vec4_t color, const char *text, float adjust, int limit, int style, fontInfo_t *font)
{
	x -= CG_Text_Width_Ext(text, scalex, limit, font) * 0.5f;


	CG_Text_Paint_Ext(x, y, scalex, scaley, color, text, adjust, limit, style, font);
}

void CG_Text_Paint_Ext(float x, float y, float scalex, float scaley, vec4_t color, const char *text, float adjust, int limit, int style, fontInfo_t *font)
{
	int         len, count;
	vec4_t      newColor;
	glyphInfo_t *glyph;

	scalex *= font->glyphScale;
	scaley *= font->glyphScale;

	if (text)
	{
		const char *s = text;
		trap_R_SetColor(color);
		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
		len = Q_strlenInt(text);
		if (limit > 0 && len > limit)
		{
			len = limit;
		}
		count = 0;
		while (s && *s && count < len)
		{
			glyph = &font->glyphs[(unsigned char)*s];
			if (Q_IsColorString(s))
			{
				if (*(s + 1) == COLOR_NULL)
				{
					memcpy(newColor, color, sizeof(newColor));
				}
				else
				{
					memcpy(newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
					newColor[3] = color[3];
				}
				trap_R_SetColor(newColor);
				s += 2;
				continue;
			}
			else
			{
				float yadj = scaley * glyph->top;
				if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE)
				{
					int ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
					colorBlack[3] = newColor[3];
					trap_R_SetColor(colorBlack);
					CG_Text_PaintChar_Ext(x + (glyph->pitch * scalex) + ofs, y - yadj + ofs, glyph->imageWidth, glyph->imageHeight, scalex, scaley, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->glyph);
					colorBlack[3] = 1.0;
					trap_R_SetColor(newColor);
				}
				CG_Text_PaintChar_Ext(x + (glyph->pitch * scalex), y - yadj, glyph->imageWidth, glyph->imageHeight, scalex, scaley, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->glyph);
				x += (glyph->xSkip * scalex) + adjust;
				s++;
				count++;
			}
		}
		trap_R_SetColor(NULL);
	}
}

void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style)
{
	fontInfo_t *font = &cgDC.Assets.fonts[activeFont];

	CG_Text_Paint_Ext(x, y, scale, scale, color, text, adjust, limit, style, font);
}

// NERVE - SMF - added back in
int CG_DrawFieldWidth(int x, int y, int width, int value, int charWidth, int charHeight)
{
	char num[16], *ptr;
	int  l;
	int  frame;
	int  totalwidth = 0;

	if (width < 1)
	{
		return 0;
	}

	// draw number string
	if (width > 5)
	{
		width = 5;
	}

	switch (width)
	{
	case 1:
		value = value > 9 ? 9 : value;
		value = value < 0 ? 0 : value;
		break;
	case 2:
		value = value > 99 ? 99 : value;
		value = value < -9 ? -9 : value;
		break;
	case 3:
		value = value > 999 ? 999 : value;
		value = value < -99 ? -99 : value;
		break;
	case 4:
		value = value > 9999 ? 9999 : value;
		value = value < -999 ? -999 : value;
		break;
	}

	Com_sprintf(num, sizeof(num), "%i", value);
	l = Q_strlenInt(num);
	if (l > width)
	{
		l = width;
	}

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
		{
			frame = STAT_MINUS;
		}
		else
		{
			frame = *ptr - '0';
		}

		totalwidth += charWidth;
		ptr++;
		l--;
	}

	return totalwidth;
}

int CG_DrawField(int x, int y, int width, int value, int charWidth, int charHeight, qboolean dodrawpic, qboolean leftAlign)
{
	char num[16], *ptr;
	int  l;
	int  frame;
	int  startx;

	if (width < 1)
	{
		return 0;
	}

	// draw number string
	if (width > 5)
	{
		width = 5;
	}

	switch (width)
	{
	case 1:
		value = value > 9 ? 9 : value;
		value = value < 0 ? 0 : value;
		break;
	case 2:
		value = value > 99 ? 99 : value;
		value = value < -9 ? -9 : value;
		break;
	case 3:
		value = value > 999 ? 999 : value;
		value = value < -99 ? -99 : value;
		break;
	case 4:
		value = value > 9999 ? 9999 : value;
		value = value < -999 ? -999 : value;
		break;
	}

	Com_sprintf(num, sizeof(num), "%i", value);
	l = Q_strlenInt(num);
	if (l > width)
	{
		l = width;
	}

	// NERVE - SMF
	if (!leftAlign)
	{
		x -= 2 + charWidth * (l);
	}

	startx = x;

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
		{
			frame = STAT_MINUS;
		}
		else
		{
			frame = *ptr - '0';
		}

		if (dodrawpic)
		{
			CG_DrawPic(x, y, charWidth, charHeight, cgs.media.numberShaders[frame]);
		}
		x += charWidth;
		ptr++;
		l--;
	}

	return startx;
}
// -NERVE - SMF

/*
================
CG_Draw3DModel

================
*/
void CG_Draw3DModel(float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles)
{
	refdef_t    refdef;
	refEntity_t ent;

	CG_AdjustFrom640(&x, &y, &w, &h);

	memset(&refdef, 0, sizeof(refdef));

	memset(&ent, 0, sizeof(ent));
	AnglesToAxis(angles, ent.axis);
	VectorCopy(origin, ent.origin);
	ent.hModel     = model;
	ent.customSkin = skin;
	ent.renderfx   = RF_NOSHADOW;   // no stencil shadows

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear(refdef.viewaxis);

	refdef.fov_x = 30;
	refdef.fov_y = 30;

	refdef.x      = x;
	refdef.y      = y;
	refdef.width  = w;
	refdef.height = h;

	refdef.time = cg.time;

	trap_R_ClearScene();
	trap_R_AddRefEntityToScene(&ent);
	trap_R_RenderScene(&refdef);
}

/*
==============
CG_DrawKeyModel
==============
*/
void CG_DrawKeyModel(int keynum, float x, float y, float w, float h, int fadetime)
{
#if defined(__linux__) && !defined(Q3_VM)
	vec3_t mins, maxs, origin, angles = { 0, 0, 0 };
	float len;
	trap_R_ModelBounds(cg_items[keynum].models[0], mins, maxs);
	origin[2] = (float)(((long double)mins[2] + maxs[2]) * -0.5f);
	origin[1] = (float)(((long double)mins[1] + maxs[1]) * 0.5f);
	len = (float)(((long double)maxs[2] - mins[2]) * 0.75f);
	origin[0] = (float)((long double)len * (double)3.731343283582089);
	angles[YAW] = (float)(sinl((long double)cg.time * (double)0.0005) * 30.0f);
	CG_Draw3DModel(x, y, w, h, cg_items[keynum].models[0], 0, origin, angles);
#else
	qhandle_t cm;
	float     len;
	vec3_t    origin, angles;
	vec3_t    mins, maxs;

	VectorClear(angles);

	cm = cg_items[keynum].models[0];

	// offset the origin y and z to center the model
	trap_R_ModelBounds(cm, mins, maxs);

	origin[2] = -0.5 * (mins[2] + maxs[2]);
	origin[1] = 0.5 * (mins[1] + maxs[1]);

//	len = 0.5 * ( maxs[2] - mins[2] );
	len       = 0.75 * (maxs[2] - mins[2]);
	origin[0] = len / 0.268;    // len / tan( fov/2 )

	angles[YAW] = 30 * sin(cg.time / 2000.0);;

	CG_Draw3DModel(x, y, w, h, cg_items[keynum].models[0], 0, origin, angles);
#endif
}

/*
================
CG_DrawTeamBackground

================
*/
void CG_DrawTeamBackground(int x, int y, int w, int h, float alpha, int team)
{
	vec4_t hcolor;

	hcolor[3] = alpha;
	if (team == TEAM_AXIS)
	{
		hcolor[0] = 1;
		hcolor[1] = 0;
		hcolor[2] = 0;
	}
	else if (team == TEAM_ALLIES)
	{
		hcolor[0] = 0;
		hcolor[1] = 0;
		hcolor[2] = 1;
	}
	else
	{
		return;
	}
	trap_R_SetColor(hcolor);
	CG_DrawPic(x, y, w, h, cgs.media.teamStatusBar);
	trap_R_SetColor(NULL);
}

/*
===========================================================================================

  UPPER RIGHT CORNER

===========================================================================================
*/

#define UPPERRIGHT_X 634
/*
==================
CG_DrawSnapshot
==================
*/
static float CG_DrawSnapshot(float y)
{
	char *s;
	int  w;

	s = va("time:%i snap:%i cmd:%i", cg.snap->serverTime,
	       cg.latestSnapshotNum, cgs.serverCommandSequence);
	w = CG_DrawStrlen(s) * BIGCHAR_WIDTH;

	CG_DrawBigString(840 - w, y + 2, s, 1.0F);

	return y + BIGCHAR_HEIGHT + 4;
}

/*
==================
CG_DrawFPS
==================
*/
#define FPS_FRAMES  4
static void CG_DrawFPSContents(float y)
{
	char       *s;
	int        w;
	static int previousTimes[FPS_FRAMES];
	static int index;
	int        i, total;
	int        fps;
	static int previous;
	int        t, frameTime;
	vec4_t     timerBackground = { 0.16f, 0.2f, 0.17f, 0.8f };
	vec4_t     timerBorder     = { 0.5f, 0.5f, 0.5f, 0.5f };
	vec4_t     tclr            = { 0.75f, 0.75f, 0.75f, 1.0f };

	// don't use serverTime, because that will be drifting to
	// correct for internet lag changes, timescales, timedemos, etc
	t         = trap_Milliseconds();
	frameTime = t - previous;
	previous  = t;

	previousTimes[index % FPS_FRAMES] = frameTime;
	index++;
	if (index > FPS_FRAMES)
	{
		// average multiple frames together to smooth changes out a bit
		total = 0;
		for ( i = 0 ; i < FPS_FRAMES ; i++ )
		{
			total += previousTimes[i];
		}
		if (!total)
		{
			total = 1;
		}
		fps = 1000 * FPS_FRAMES / total;

		s = va("%i FPS", fps);
		w = CG_Text_Width_Ext(s, 0.19f, 0, &cgs.media.limboFont1);


		CG_Text_Paint_Ext(840 - w, y + 11, 0.19f, 0.19f, tclr, s, 0, 0, 3, &cgs.media.limboFont1);
	}


}

static float CG_DrawFPS(float y)
{
	CG_DrawFPSContents(y);
#if defined(__linux__) && !defined(Q3_VM)
	return y + 12 + 4;
#else
	return y + 16.f;
#endif
}


/*
=================
CG_DrawTimer
=================
*/

static float CG_DrawTimer(float y)
{
	char   *s;
	int    w;
	int    mins, seconds, tens;
	int    msec;
	char   *rt;
	vec4_t color           = { 0.75f, 0.75f, 0.75f, 1.0f };
	vec4_t timerBackground = { 0.16f, 0.2f, 0.17f, 0.8f };
	vec4_t timerBorder     = { 0.5f, 0.5f, 0.5f, 0.5f };

	rt = "";

	/* Original30023090 keeps the product/subtraction extended until __ftol. */
#if defined(__linux__) && !defined(Q3_VM)
	/* Linux converts the retained product/difference directly to signed32. */
	msec = TCE_TruncateNativeInteger((long double)cgs.timelimit * 60000.f -
	                                TCE_SubInt32(cg.time, cgs.levelStartTime));
#else
	msec = (int)((double)cgs.timelimit * 60000.f - (cg.time - cgs.levelStartTime));
#endif

	seconds  = msec / 1000;
	mins     = seconds / 60;
	seconds -= mins * 60;
	tens     = seconds / 10;
	seconds -= tens * 10;

	if (cgs.gamestate != GS_PLAYING)
	{
		//%	s = va( "%s^*WARMUP", rt );
		s        = "^*WARMUP"; // ydnar: don't draw reinforcement time in warmup mode
		color[3] = fabs(sin(cg.time * 0.002));
	}
	else if (msec < 0 && cgs.timelimit > 0.0f)
	{
#if defined(__linux__) && !defined(Q3_VM)
		s        = va("^*0:00");
#else
		s        = va("^N0:00");
#endif
		color[3] = fabs(sin(cg.time * 0.002));
	}
	else
	{
		if (cgs.timelimit <= 0.0f)
		{
			s = va("%s", rt);
		}
		else
		{
			s = va("%s^*%i:%i%i", rt, mins, tens, seconds);
		}

		color[3] = 1.f;
	}

	w = CG_Text_Width_Ext(s, 0.19f, 0, &cgs.media.limboFont1);


	CG_Text_Paint_Ext(840 - w, y + 11, 0.19f, 0.19f, color, s, 0, 0, 3, &cgs.media.limboFont1);

	return y + 12 + 4;
}

// START	xkan, 8/29/2002
int CG_BotIsSelected(int clientNum)
{
	int i;

	for (i = 0; i < MAX_NUM_BUDDY; i++)
	{
		if (cg.selectedBotClientNumber[i] == 0)
		{
			return 0;
		}
		else if (cg.selectedBotClientNumber[i] == clientNum)
		{
			return 1;
		}
	}
	return 0;
}
// END		xkan, 8/29/2002

/*
=================
CG_DrawTeamOverlay
=================
*/

int maxCharsBeforeOverlay;

#define TEAM_OVERLAY_MAXNAME_WIDTH  16
#define TEAM_OVERLAY_MAXLOCATION_WIDTH  20

/*
=====================
CG_DrawUpperRight

=====================
*/
static void CG_DrawUpperRight(void)
{
	float           y;
	extern qboolean tce_uiCoordinates;
	qboolean        previous;

	if (!cg_drawFireteamOverlay.integer)
	{
		return;
	}

	y = 20 + 100 + 32;


	if (!(cg.snap->ps.pm_flags & PMF_LIMBO) && (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) &&
	    (cgs.autoMapExpanded || (!cgs.autoMapExpanded && (cg.time - cgs.autoMapExpandTime < 250.f))))
	{
		return;
	}

	previous = tce_uiCoordinates; tce_uiCoordinates = qtrue;
	if (cg_drawRoundTimer.integer)
	{
		y = CG_DrawTimer(y);
	}

	if (cg_drawFPS.integer)
	{
		y = CG_DrawFPS(y);
	}

	if (cg_drawSnapshot.integer)
	{
		y = CG_DrawSnapshot(y);
	}
	tce_uiCoordinates = previous;
}

/*
===========================================================================================

  LOWER RIGHT CORNER

===========================================================================================
*/

#define CHATLOC_X 160
#define CHATLOC_Y 478
#define CHATLOC_TEXT_X (CHATLOC_X + 0.25f * TINYCHAR_WIDTH)

/*
=================
CG_DrawTeamInfo
=================
*/
static void CG_DrawTeamInfo(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean        previous;
	int             w;
	int             i, len;
	vec4_t          hcolor;
	int             chatHeight;
	float           alphapercent;
	float           lineHeight = 9.f;

	int chatWidth = 318;

	if (cg_teamChatHeight.integer < TEAMCHAT_HEIGHT)
	{
		chatHeight = cg_teamChatHeight.integer;
	}
	else
	{
		chatHeight = TEAMCHAT_HEIGHT;
	}

	if (chatHeight <= 0)
	{
		return; // disabled
	}

	if (cgs.teamLastChatPos != cgs.teamChatPos)
	{
		if (cg.time - cgs.teamChatMsgTimes[cgs.teamLastChatPos % chatHeight] > cg_teamChatTime.integer)
		{
			cgs.teamLastChatPos++;
		}

		previous          = tce_uiCoordinates;
		tce_uiCoordinates = qtrue;

		w = 0;

		for ( i = cgs.teamLastChatPos; i < cgs.teamChatPos; i++ )
		{
			len = CG_Text_Width_Ext(cgs.teamChatMsgs[i % chatHeight], 0.2f, 0, &cgs.media.limboFont2);
			if (len > w)
			{
				w = len;
			}
		}
		w *= TINYCHAR_WIDTH;
		w += TINYCHAR_WIDTH * 2;

		for ( i = cgs.teamChatPos - 1; i >= cgs.teamLastChatPos; i-- )
		{
			alphapercent = 1.0f - (cg.time - cgs.teamChatMsgTimes[i % chatHeight]) / (float)(cg_teamChatTime.integer);
			if (alphapercent > 1.0f)
			{
				alphapercent = 1.0f;
			}
			else if (alphapercent < 0.f)
			{
				alphapercent = 0.f;
			}

			hcolor[0] = hcolor[1] = hcolor[2] = 0;
			hcolor[3] = 0.6f * alphapercent;

			trap_R_SetColor(hcolor);
			CG_DrawPic(2, 365 - (cgs.teamChatPos - i) * lineHeight, chatWidth, lineHeight, cgs.media.teamStatusBar);

			hcolor[0] = hcolor[1] = hcolor[2] = 1.0;
			hcolor[3] = alphapercent;
			trap_R_SetColor(hcolor);

			CG_Text_Paint_Ext(4, 365 - (cgs.teamChatPos - i - 1) * lineHeight - 1, 0.2f, 0.2f, hcolor, cgs.teamChatMsgs[i % chatHeight], 0, 0, 0, &cgs.media.limboFont2);
		}
		tce_uiCoordinates = previous;
	}
}

const char * CG_PickupItemText(int item)
{
	if (bg_itemlist[item].giType == IT_HEALTH)
	{
		if (bg_itemlist[item].world_model[2])       // this is a multi-stage item
		// FIXME: print the correct amount for multi-stage
		{
			return va("a %s", bg_itemlist[item].pickup_name);
		}
		else
		{
			/* TC30020ff0: the item name already supplies the pickup wording. */
			return va("%s", bg_itemlist[item].pickup_name);
		}
	}
	else if (bg_itemlist[item].giType == IT_TEAM)
	{
		return "an Objective";
	}
	else
	{
		if (bg_itemlist[item].pickup_name[0] == 'a' ||  bg_itemlist[item].pickup_name[0] == 'A')
		{
			return va("an %s", bg_itemlist[item].pickup_name);
		}
		else
		{
			return va("a %s", bg_itemlist[item].pickup_name);
		}
	}
}

/*
=================
CG_DrawNotify
=================
*/
#define NOTIFYLOC_Y 42 // bottom end
#define NOTIFYLOC_X 0
#define NOTIFYLOC_Y_SP 128

static void CG_DrawNotify(void)
{
	int    w, h;
	int    i, len;
	vec4_t hcolor;
	int    chatHeight;
	float  alphapercent;
	char   var[MAX_TOKEN_CHARS];
	float  notifytime = 1.0f;
	int    yLoc;

	return;

	yLoc = NOTIFYLOC_Y;

	trap_Cvar_VariableStringBuffer("con_notifytime", var, sizeof(var));
	notifytime = atof(var) * 1000;

	if (notifytime <= 100.f)
	{
		notifytime = 100.0f;
	}

	chatHeight = NOTIFY_HEIGHT;

	if (cgs.notifyLastPos != cgs.notifyPos)
	{
		if (cg.time - cgs.notifyMsgTimes[cgs.notifyLastPos % chatHeight] > notifytime)
		{
			cgs.notifyLastPos++;
		}

		h = (cgs.notifyPos - cgs.notifyLastPos) * TINYCHAR_HEIGHT;

		w = 0;

		for (i = cgs.notifyLastPos; i < cgs.notifyPos; i++)
		{
			len = CG_DrawStrlen(cgs.notifyMsgs[i % chatHeight]);
			if (len > w)
			{
				w = len;
			}
		}
		w *= TINYCHAR_WIDTH;
		w += TINYCHAR_WIDTH * 2;

		if (maxCharsBeforeOverlay <= 0)
		{
			maxCharsBeforeOverlay = 80;
		}

		for (i = cgs.notifyPos - 1; i >= cgs.notifyLastPos; i--)
		{
			alphapercent = 1.0f - ((cg.time - cgs.notifyMsgTimes[i % chatHeight]) / notifytime);
			if (alphapercent > 0.5f)
			{
				alphapercent = 1.0f;
			}
			else
			{
				alphapercent *= 2;
			}

			if (alphapercent < 0.f)
			{
				alphapercent = 0.f;
			}

			hcolor[0] = hcolor[1] = hcolor[2] = 1.0;
			hcolor[3] = alphapercent;
			trap_R_SetColor(hcolor);

			CG_DrawStringExt(NOTIFYLOC_X + TINYCHAR_WIDTH,
			                 yLoc - (cgs.notifyPos - i) * TINYCHAR_HEIGHT,
			                 cgs.notifyMsgs[i % chatHeight], hcolor, qfalse, qfalse,
			                 TINYCHAR_WIDTH, TINYCHAR_HEIGHT, maxCharsBeforeOverlay);
		}
	}
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define LAG_SAMPLES     128


typedef struct
{
	int frameSamples[LAG_SAMPLES];
	int frameCount;
	int snapshotFlags[LAG_SAMPLES];
	int snapshotSamples[LAG_SAMPLES];
	int snapshotCount;
} lagometer_t;

lagometer_t lagometer;

/*
==============
CG_AddLagometerFrameInfo

Adds the current interpolate / extrapolate bar for this frame
==============
*/
void CG_AddLagometerFrameInfo(void)
{
	int offset;

	offset                                                           = cg.time - cg.latestSnapshotTime;
	lagometer.frameSamples[lagometer.frameCount & (LAG_SAMPLES - 1)] = offset;
	lagometer.frameCount++;
}

/*
==============
CG_AddLagometerSnapshotInfo

Each time a snapshot is received, log its ping time and
the number of snapshots that were dropped before it.

Pass NULL for a dropped packet.
==============
*/
void CG_AddLagometerSnapshotInfo(snapshot_t *snap)
{
	// dropped packet
	if (!snap)
	{
		lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = -1;
		lagometer.snapshotCount++;
		return;
	}

	// add this snapshot's info
	lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = snap->ping;
	lagometer.snapshotFlags[lagometer.snapshotCount & (LAG_SAMPLES - 1)]   = snap->snapFlags;
	lagometer.snapshotCount++;
}

/*
==============
CG_DrawDisconnect

Should we draw something differnet for long lag vs no packets?
==============
*/
static void CG_DrawDisconnect(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean        previousCoordinates;
	float           x, y;
	int             cmdNum;
	usercmd_t       cmd;
	const char      *s;
	int             w; // bk010215 - FIXME char message[1024];

	// OSP - dont draw if a demo and we're running at a different timescale
	if (cg.demoPlayback && cg_timescale.value != 1.0f)
	{
		return;
	}

	// ydnar: don't draw if the server is respawning
	if (cg.serverRespawning)
	{
		return;
	}

	// draw the phone jack if we are completely past our buffers
	cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
	trap_GetUserCmd(cmdNum, &cmd);
	if (cmd.serverTime <= cg.snap->ps.commandTime
	    || cmd.serverTime > cg.time)    // special check for map_restart // bk 0102165 - FIXME
	{
		return;
	}

	// also add text in center of screen
	/* Original DrawPic/DrawChar use the TC852x480 transform. Preserve this
	 * caller's coordinate domain through both text and the blinking icon. */
	previousCoordinates = tce_uiCoordinates;
	tce_uiCoordinates   = qtrue;
	s                   = CG_TranslateString("Connection Interrupted"); // bk 010215 - FIXME
	w                   = CG_DrawStrlen(s) * BIGCHAR_WIDTH;
	CG_DrawBigString(426 - w / 2, 100, s, 1.0F);

	// blink the icon
	if ((cg.time >> 9) & 1)
	{
		tce_uiCoordinates = previousCoordinates;
		return;
	}

	x = 852 - 48;
	y = 480 - 200;

	CG_DrawPic(x, y, 48, 48, cgs.media.disconnectIcon);
	tce_uiCoordinates = previousCoordinates;
}


#define MAX_LAGOMETER_PING  900
#define MAX_LAGOMETER_RANGE 300

/*
==============
CG_DrawLagometer
==============
*/
static void CG_DrawLagometer(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean        previousCoordinates;
	int             a, x, y, i;
	float           v;
	float           ax, ay, aw, ah, mid, range;
	int             color;
	float           vscale;

	if (!cg_lagometer.integer || cgs.localServer)
	{
//	if(0) {
		CG_DrawDisconnect();
		return;
	}

	//
	// draw the graph
	//
	previousCoordinates = tce_uiCoordinates;
	tce_uiCoordinates   = qtrue;
	x                   = 852 - 48;
	y                   = 480 - 200;

	trap_R_SetColor(NULL);
	CG_DrawPic(x, y, 48, 48, cgs.media.lagometerShader);

	ax = x;
	ay = y;
	aw = 48;
	ah = 48;
	CG_AdjustFrom640(&ax, &ay, &aw, &ah);

	color = -1;
	range = ah * (1.0f / 3.0f);
	mid   = (float)((double)ah * (1.0f / 3.0f) + ay);

	vscale = range * (1.0f / MAX_LAGOMETER_RANGE);

	// draw the frame interpoalte / extrapolate graph
	for ( a = 0 ; a < aw ; a++ )
	{
		i  = (lagometer.frameCount - 1 - a) & (LAG_SAMPLES - 1);
		v  = lagometer.frameSamples[i];
		v *= vscale;
		if (v > 0)
		{
			if (color != 1)
			{
				color = 1;
				trap_R_SetColor(colorYellow);
			}
			if (v > range)
			{
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		}
		else if (v < 0)
		{
			if (color != 2)
			{
				color = 2;
				trap_R_SetColor(colorBlue);
			}
			v = -v;
			if (v > range)
			{
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, mid, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		}
	}

	// draw the snapshot latency / drop graph
	range  = ah / 2;
	vscale = range * (1.0f / MAX_LAGOMETER_PING);

	for ( a = 0 ; a < aw ; a++ )
	{
		i = (lagometer.snapshotCount - 1 - a) & (LAG_SAMPLES - 1);
		v = lagometer.snapshotSamples[i];
		if (v > 0)
		{
			if (lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED)
			{
				if (color != 5)
				{
					color = 5;  // YELLOW for rate delay
					trap_R_SetColor(colorYellow);
				}
			}
			else
			{
				if (color != 3)
				{
					color = 3;
					trap_R_SetColor(colorGreen);
				}
			}
			v = v * vscale;
			if (v > range)
			{
				v = range;
			}
			trap_R_DrawStretchPic((float)((double)ax + aw - a), (float)((double)ah + ay - v), 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		}
		else if (v < 0)
		{
			if (color != 4)
			{
				color = 4;      // RED for dropped snapshots
				trap_R_SetColor(colorRed);
			}
			trap_R_DrawStretchPic((float)((double)ax + aw - a), (float)((double)ah + ay - range), 1, range, 0, 0, 0, 0, cgs.media.whiteShader);
		}
	}

	trap_R_SetColor(NULL);

	if (cg_nopredict.integer
#ifdef ALLOW_GSYNC
	    || cg_synchronousClients.integer
#endif // ALLOW_GSYNC
	    )
	{
		CG_DrawBigString(ax, ay, "snc", 1.0);
	}

	tce_uiCoordinates = previousCoordinates;
	CG_DrawDisconnect();
}


void CG_DrawLivesLeft(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean        previous;
	if (cg_gameType.integer == GT_WOLF_LMS)
	{
		return;
	}

	if (cg.snap->ps.persistant[PERS_RESPAWNS_LEFT] < 0)
	{
		return;
	}

	previous          = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	CG_DrawPic(4, 360, 48, 24, cg.snap->ps.persistant[PERS_TEAM] == TEAM_ALLIES ? cgs.media.hudAlliedHelmet : cgs.media.hudAxisHelmet);

	CG_DrawField(44, 360, 3, cg.snap->ps.persistant[PERS_RESPAWNS_LEFT], 14, 20, qtrue, qtrue);
	tce_uiCoordinates = previous;
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/


/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
#define CP_LINEWIDTH 56         // NERVE - SMF

static void CG_CenterPopup(int type, const char *text, int shader)
{
	CG_AddPMItemBig((popupMessageBigType_t)type, text, shader);
}
void CG_CenterPrint(const char *str, int y, int charWidth)
{
	TCE_CenterPrint(str, cg.time, &cg.centerPrintTime, &cg.centerPrintPriority, cgs.media.pmImages[PM_MESSAGE], CG_CenterPopup);
}
void CG_PriorityCenterPrint(const char *str, int y, int charWidth, int priority)
{
	TCE_PriorityCenterPrint(str, cg.time, priority, &cg.centerPrintTime, &cg.centerPrintPriority, cgs.media.pmImages[PM_MESSAGE], CG_CenterPopup);
}

/*
===================
CG_DrawCenterString
===================
*/
static void CG_DrawCenterString(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean        previous;
	char            *start;
	int             l;
	int             x, y, w;
	float           *color;

	if (!cg.centerPrintTime)
	{
		return;
	}

	color = CG_FadeColor(cg.centerPrintTime, (int)(1000.0 * cg_centertime.value));
	if (!color)
	{
		cg.centerPrintTime     = 0;
		cg.centerPrintPriority = 0;
		return;
	}

	previous          = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	trap_R_SetColor(color);

	start = cg.centerPrint;

	y = cg.centerPrintY - cg.centerPrintLines * BIGCHAR_HEIGHT / 2;

	while (1)
	{
		char linebuffer[1024];

		for ( l = 0; l < CP_LINEWIDTH; l++ )            // NERVE - SMF - added CP_LINEWIDTH
		{
			if (!start[l] || start[l] == '\n')
			{
				break;
			}
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = cg.centerPrintCharWidth * CG_DrawStrlen(linebuffer);

		x = (852 - w) / 2;

		CG_DrawStringExt(x, y, linebuffer, color, qfalse, qtrue, cg.centerPrintCharWidth, (int)(cg.centerPrintCharWidth * 1.5), 0);

		y += cg.centerPrintCharWidth * 1.5;

		while (*start && (*start != '\n'))
		{
			start++;
		}
		if (!*start)
		{
			break;
		}
		start++;
	}

	trap_R_SetColor(NULL);
	tce_uiCoordinates = previous;
}



/*
================================================================================

CROSSHAIRS

================================================================================
*/

/*
==============
CG_DrawWeapReticle
==============
*/
#if defined(__GNUC__) && defined(__i386__) && defined(__linux__)
/* Reticle ELF00042ebe..42f5f has its own held-angle load/stack order.
 * Its center constants are stored before the actual AngleVectors calls. */
static void CG_TCELinuxReticleAngles(float *x, float *y, vec3_t angles)
{
	long double pitch, yaw;

	memcpy(angles, cg.predictedPlayerState.viewangles, sizeof(vec3_t));
	pitch     = (long double)cg.predictedPlayerState.holdable[5] - 2000.0L;
	pitch    *= (long double)0.01f;
	yaw       = (long double)cg.predictedPlayerState.holdable[6] - 2000.0L;
	yaw      *= (long double)0.01f;
	angles[0] = (float)((long double)angles[0] + pitch);
	angles[1] = (float)((long double)angles[1] + yaw);
	*x        = 426.0f;
	*y        = 240.0f;
}

/* TCE scope projection: preserve the double angle and float screen-offset
 * boundaries before updating the caller's center coordinates. */
static void CG_TCELinuxReticleProject(const vec3_t forward, const vec3_t right,
                                      const vec3_t up, float *x, float *y)
{
	const float  degrees = 0.0055555556900799274f;
	const double pi      = 3.141592653589793;
	float        fov     = cg.refdef.fov_x;
	float        dx, dy;
	double       angle;
	long double  rightDot, upDot, reciprocal;

	rightDot = (long double)forward[1] * right[1]
	           + (long double)forward[0] * right[0];
	rightDot  += (long double)forward[2] * right[2];
	angle      = (double)atan2l(rightDot, sqrtl(1.0L - rightDot * rightDot));
	reciprocal = 1.0L / (((long double)fov * degrees) * pi);
	dx         = (float)(((long double)angle * reciprocal) * 852.0L);

	upDot = (long double)forward[1] * up[1]
	        + (long double)forward[0] * up[0];
	upDot      = (long double)forward[2] * up[2] + upDot;
	angle      = (double)atan2l(upDot, sqrtl(1.0L - upDot * upDot));
	*x         = (float)((long double)*x + dx);
	reciprocal = 1.0L / (((long double)degrees * fov) * pi);
	dy         = (float)(((long double)angle * reciprocal) * 852.0L);
	*y         = (float)((long double)*y - dy);
}

/* Native Linux portal-reticle drawing with explicit float/double boundaries. */
static void CG_TCELinuxReticlePortal851(int type, const float *x, const float *y,
                                        const vec4_t black, const vec4_t red)
{
	float centerX, centerY, viewport;
	int   configuredWidth = tce_cg_weapons[0].portalScopeWidth;

	memcpy(&centerX, x, sizeof(centerX));
	memcpy(&centerY, y, sizeof(centerY));
	viewport = (float)((long double)1.2f * 480.0f);
	if (type == 2)
	{
		qhandle_t   shader;
		long double width;
		float       left, top, drawnWidth, drawnHeight;

		width = (long double)configuredWidth * 0.88f;
		if (!(width > 0.0L))
		{
			width = 220.0L;
		}
		width *= (long double)1.2f;
		shader = cgs.media.tceM76ReticleShader;
		if (shader)
		{
			drawnWidth  = (float)width;
			drawnHeight = (float)(width * 0.5L);
			top         = (float)((long double)centerY
			                      - width * (long double)0.03181818127632141f);
			left = (float)((long double)centerX - width * 0.5L);
			CG_DrawPic(left, top, drawnWidth, drawnHeight, shader);
		}
		top  = (float)((long double)centerY - 1.0L);
		left = (float)((long double)centerX - 1.0L);
		CG_FillRect(left, top, 2.0f, 2.0f, red);
	}
	else if (type == 1)
	{
		float       width, halfWidth, left, top, quarterWidth, tickX, tickY;
		double      cachedThreeQuarters;
		long double threeQuarters;

		width     = configuredWidth > 0 ? (float)configuredWidth : 250.0f;
		width     = (float)((long double)1.2f * width);
		halfWidth = (float)((long double)width * 0.5L);
		left      = (float)((long double)centerX - halfWidth);
		CG_FillRect(left, centerY, width, 1.0f, black);

		top = (float)((long double)centerY - halfWidth);
		CG_FillRect(centerX, top, 1.0f, width, black);

		quarterWidth = (float)((long double)width * 0.25L);
		tickY        = (float)((long double)centerY - 1.0L);
		CG_FillRect(left, tickY, quarterWidth, 3.0f, black);
		threeQuarters       = 0.75L * width;
		cachedThreeQuarters = (double)threeQuarters;
		tickX               = (float)(threeQuarters + left);
		CG_FillRect(tickX, tickY, quarterWidth, 3.0f, black);

		tickX = (float)((long double)centerX - 1.0L);
		CG_FillRect(tickX, top, 3.0f, quarterWidth, black);
		tickY = (float)((long double)cachedThreeQuarters + top);
		CG_FillRect(tickX, tickY, 3.0f, quarterWidth, black);
		CG_FillRect(centerX, centerY, 1.0f, 1.0f, red);
	}
	else
	{
		long double width, halfWidth;
		float       drawnWidth, cachedHalf, left, top;

		width      = configuredWidth > 0 ? (long double)configuredWidth : 250.0L;
		halfWidth  = width * 0.5L;
		drawnWidth = (float)width;
		cachedHalf = (float)halfWidth;
		left       = (float)((long double)centerX - halfWidth);
		CG_FillRect(left, centerY, drawnWidth, 1.0f, black);

		top = (float)((long double)centerY - cachedHalf);
		CG_FillRect(centerX, top, 1.0f, drawnWidth, black);
		CG_FillRect(centerX, centerY, 1.0f, 1.0f, red);
	}
	if (cgs.media.tcePortalScopeShader)
	{
		vec3_t      lighting;
		vec4_t      color;
		long double boost, redLight;
		float       left, top, offset;
		qhandle_t   shader;

		TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(), cg.refdef.vieworg, lighting);
		boost    = (long double)cg.tceScopeLightBoost + 1.0L;
		redLight = (long double)lighting[0] * boost;
		color[0] = (float)redLight;
		color[1] = (float)((long double)lighting[1] * boost);
		color[2] = (float)(boost * lighting[2]);
		color[3] = 1.0f;
		if (redLight >= 1.0L)
		{
			color[0] = 1.0f;
		}
		if (color[1] >= 1.0f)
		{
			color[1] = 1.0f;
		}
		if (color[2] >= 1.0f)
		{
			color[2] = 1.0f;
		}
		trap_R_SetColor(color);
		shader = cgs.media.tcePortalScopeShader;
		top    = (float)((long double)centerY - (long double)viewport * 0.5L);
		left   = (float)((long double)centerX - (long double)viewport * 0.5L);
		offset = (float)((long double)viewport * 0.09375f);
		top    = (float)((long double)top + offset);
		CG_DrawPic(left, top, viewport, viewport, shader);
		trap_R_SetColor(NULL);
	}
}
#endif

/* Native TC scope projection used by CG_DrawWeapReticle. */
static void CG_TCEScopeCenter(float *x, float *y, vec3_t angles)
{
	vec3_t forward, right, up;
#if defined(__GNUC__) && defined(__i386__) && defined(__linux__)
	CG_TCELinuxReticleAngles(x, y, angles);
#else
	float radians;
	VectorCopy(cg.predictedPlayerState.viewangles, angles);
	angles[0] += ((float)cg.predictedPlayerState.holdable[5] - 2000.0f) * 0.01f;
	angles[1] += ((float)cg.predictedPlayerState.holdable[6] - 2000.0f) * 0.01f;
#endif
	AngleVectors(angles, forward, NULL, NULL);
	AngleVectors(cg.predictedPlayerState.viewangles, NULL, right, up);
#if defined(__GNUC__) && defined(__i386__) && defined(__linux__)
	CG_TCELinuxReticleProject(forward, right, up, x, y);
#else
	radians = cg.refdef.fov_x * 0.01745329238474369f;
	*x      = (float)((asin(DotProduct(forward, right)) / radians) * 852.0 + 426.0);
	*y      = (float)(240.0 - (asin(DotProduct(forward, up)) / radians) * 852.0);
#endif
}

static void CG_DrawWeapReticle(void)
{
	extern qboolean    tce_uiCoordinates;
	qboolean           previous = tce_uiCoordinates;
	vec4_t             black = { 0, 0, 0, 1 }, red = { 0.5f, 0, 0, 0.7f }, light;
	vec3_t             angles;
	int                weapon, type, i;
	float              x, y, size, half, quarter;
	static const float ticksX[] = { 224, 248, 272, 296, 341, 365, 389, 413 };
	static const float ticksY[] = { 144, 168, 192, 216, 263, 287, 311, 335 };
	weapon = ((cg.snap->ps.pm_flags & PMF_FOLLOW) || cg.demoPlayback)?cg.snap->ps.weapon:cg.predictedPlayerState.weapon;
	if (weapon < 0 || weapon >= TCE_MAX_WEAPONS)
	{
		return;
	}
	type              = weaponDef[weapon].scopeReticleType;
	tce_uiCoordinates = qtrue;
	if (!cg_portalScopes.integer)
	{
		if (type == 2)
		{
			if (cgs.media.tceM76ReticleShader)
			{
				CG_DrawPic(206, 225, 440, 220, cgs.media.tceM76ReticleShader);
			}
			CG_FillRect(425, 239, 2, 2, red);
			CG_FillRect(0, 0, 186, 480, black); CG_FillRect(666, 0, 186, 480, black);
			if (cgs.media.reticleShaderSimple)
			{
				CG_DrawPic(186, 0, 480, 480, cgs.media.reticleShaderSimple);
			}
		}
		else
		{
			if (type == 1)
			{
				CG_FillRect(80, 239, 120, 3, black); CG_FillRect(440, 239, 120, 3, black);
				CG_FillRect(319, 0, 3, 120, black); CG_FillRect(319, 360, 3, 120, black);
				CG_FillRect(200, 240, 240, 1, black); CG_FillRect(320, 120, 1, 240, black);
				for (i = 0; i < 8; i++)
					CG_FillRect(ticksX[i], 239, 3, 3, black);
				for (i = 0; i < 8; i++)
					CG_FillRect(319, ticksY[i], 3, 3, black);
			}
			else
			{
				CG_FillRect(80, 240, 480, 1, black); CG_FillRect(320, 0, 1, 480, black);
			}
			CG_FillRect(320, 240, 1, 1, red);
			CG_FillRect(0, 0, 80, 480, black); CG_FillRect(560, 0, 80, 480, black);
			if (cgs.media.reticleShaderSimple)
			{
				CG_DrawPic(80, 0, 480, 480, cgs.media.reticleShaderSimple);
			}
		}
	}
	else
	{
		CG_TCEScopeCenter(&x, &y, angles);
#if defined(__GNUC__) && defined(__i386__) && defined(__linux__)
		CG_TCELinuxReticlePortal851(type, &x, &y, black, red);
#else
		/* Original reticle reads slot-zero width, unlike the scene viewport. */
		size = (float)tce_cg_weapons[0].portalScopeWidth;
		if (type == 2)
		{
			size *= 0.88f; if (size <= 0)
			{
				size = 220;
			}
			size *= 1.2f;
			if (cgs.media.tceM76ReticleShader)
			{
				CG_DrawPic(x - size * 0.5f, y - size * 0.03181818127632141f, size, size * 0.5f, cgs.media.tceM76ReticleShader);
			}
			CG_FillRect(x - 1, y - 1, 2, 2, red);
		}
		else if (type == 1)
		{
			if (size <= 0)
			{
				size = 250;
			}
			size *= 1.2f; half = size * 0.5f; quarter = size * 0.25f;
			CG_FillRect(x - half, y, size, 1, black); CG_FillRect(x, y - half, 1, size, black);
			CG_FillRect(x - half, y - 1, quarter, 3, black); CG_FillRect(x - half + size * 0.75f, y - 1, quarter, 3, black);
			CG_FillRect(x - 1, y - half, 3, quarter, black); CG_FillRect(x - 1, y - half + size * 0.75f, 3, quarter, black);
			CG_FillRect(x, y, 1, 1, red);
		}
		else
		{
			if (size <= 0)
			{
				size = 250;
			}
			half = size * 0.5f;
			CG_FillRect(x - half, y, size, 1, black); CG_FillRect(x, y - half, 1, size, black); CG_FillRect(x, y, 1, 1, red);
		}
		if (cgs.media.tcePortalScopeShader)
		{
			TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(), cg.refdef.vieworg, light);
			for (i = 0; i < 3; i++)
			{
				light[i] *= 1.0f + cg.tceScopeLightBoost; if (light[i] >= 1)
				{
					light[i] = 1;
				}
			}
			light[3] = 1; trap_R_SetColor(light);
			CG_DrawPic(x - 288, (y - 288) + 54.000003814697266f, 576, 576, cgs.media.tcePortalScopeShader);
			trap_R_SetColor(NULL);
		}
#endif
	}
	tce_uiCoordinates = previous;
}

/*
==============
CG_DrawMortarReticle
==============
*/
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
typedef char mortarTimerLayout_t[(sizeof(int) == 4 && sizeof(unsigned) == 4) ? 1 : -1];

/* Native SUB/LEA time arithmetic wraps before the signed age comparison. */
static int CG_MortarTimeSince(int now, int eventTime, unsigned delay)
{
	unsigned bits = (unsigned)now - (unsigned)eventTime - delay;
	int      elapsed;
	memcpy(&elapsed, &bits, sizeof(elapsed));
	return elapsed;
}

/* Preserve the held product before native FISTP32 and the 16-bit mask. */
static float CG_MortarScaleOffset(float angle, float unitsPerDegree, float degreesPerUnit)
{
	long double scaled = (long double)angle * (long double)unitsPerDegree;
	unsigned    units  = (unsigned)TCE_TruncateNativeInteger(scaled) & 65535u;
	return (float)((long double)units * (long double)degreesPerUnit);
}

/* Windows CRT floor receives a Double argument; Linux rounds held x87. */
static long double CG_MortarFloorValue(long double value)
{
#if defined(_WIN32)
	volatile double argument = (double)value;
	return (long double)floor(argument);
#else
	return floorl(value);
#endif
}

static void CG_MortarHorizontalLabels(float angle, int *minimum, int *major)
{
	volatile float  argument    = (float)((long double)angle - 90.0L);
	long double     normalized  = (long double)AngleNormalize360(argument);
	int             sector      = TCE_TruncateNativeInteger(normalized * (long double)0.06666667014360428f);
	unsigned        minimumBits = (unsigned)sector * 15u;
	volatile double rounded;
	int             degrees, remainder;
	memcpy(minimum, &minimumBits, sizeof(minimumBits));

	normalized = (long double)AngleNormalize360(argument);
	rounded    = (double)CG_MortarFloorValue(normalized);
	degrees    = TCE_TruncateNativeInteger((long double)rounded);
	remainder  = degrees % 15;
	rounded    = (double)CG_MortarFloorValue((long double)remainder);
	*major     = TCE_TruncateNativeInteger((long double)rounded * (long double)0.2);
}

static void CG_MortarVerticalLabels(float angle, int *minimum, int *major)
{
	long double     shifted = (long double)angle + 25.0L;
	volatile double rounded = (double)CG_MortarFloorValue(shifted * (long double)0.10000000149011612f);
	int             tenths, remainder;
#if defined(_WIN32)
	volatile float storedShifted = (float)shifted;
	*minimum = TCE_TruncateNativeInteger((long double)rounded * 10.0L);
	tenths   = TCE_TruncateNativeInteger((long double)storedShifted * 10.0L);
#else
	tenths   = TCE_TruncateNativeInteger(shifted * 10.0L);
	*minimum = TCE_TruncateNativeInteger((long double)rounded * 10.0L);
#endif
	remainder = tenths % 100;
	rounded   = (double)CG_MortarFloorValue((long double)remainder);
	*major    = TCE_TruncateNativeInteger((long double)rounded * (long double)0.04);
}

static float CG_MortarFadeAlpha(int elapsed, float reciprocal)
{
	return (float)(1.0L - (long double)elapsed * (long double)reciprocal);
}

static long double CG_MortarHorizontalOffset(long double difference)
{
#if defined(_WIN32)
	return (difference * (long double)0.20000000298023224f) * 10.0L;
#else
	return difference + difference;
#endif
}

static void CG_MortarPitchMarker(long double difference, qboolean above, const vec4_t color)
{
	volatile float argument;
	long double    offset;
	float          y;
	if (!(difference >= 0.0L))
	{
		difference = 0.0L;
	}
	argument = (float)difference;
	offset   = (long double)AngleNormalize360(argument);
#if defined(_WIN32)
	offset = (offset * (long double)0.40000000596046448f) * 10.0L;
#else
	offset *= 4.0L;
#endif
	if (!(offset >= 100.0L))
	{
		y = (float)(above ? 264.0L - offset : 264.0L + offset);
		CG_FillRect(293, y, 6, 2, color);
		CG_FillRect(342, y, 6, 2, color);
	}
}

/* Native Windows and Linux request-yaw value boundaries differ. */
static long double CG_MortarRequestYaw(const vec3_t position, const vec3_t origin)
{
#if defined(_WIN32)
	volatile float x = (float)((long double)position[0] - (long double)origin[0]);
	long double    y = (long double)position[1] - (long double)origin[1];
	long double    yaw;
	if (!(y > 0.0L || y < 0.0L) && !(x > 0.0f || x < 0.0f))
	{
		return 0.0L;
	}
	if (x > 0.0f || x < 0.0f)
	{
		yaw = atan2l(y, (long double)x) * (long double)57.29577791868204;
	}
	else
	{
		yaw = y > 0.0L ? 90.0L : 270.0L;
	}
	if (!(yaw >= 0.0L))
	{
		yaw += 360.0L;
	}
	return yaw;
#else
	long double    deltaX = (long double)position[0] - (long double)origin[0];
	long double    deltaY = (long double)position[1] - (long double)origin[1];
	volatile float x      = (float)deltaX;
	volatile float y      = (float)deltaY;
	/* The original also computes/stores Z before classifying X and Y. */
	volatile float z = (float)((long double)position[2] - (long double)origin[2]);
	long double    yaw;
	(void)z;

	/* The native C3 zero tests treat unordered as equal. */
	if (!(deltaY > 0.0L || deltaY < 0.0L) &&
	    !(deltaX > 0.0L || deltaX < 0.0L))
	{
		return 0.0L;
	}
	if (x > 0.0f || x < 0.0f)
	{
		volatile double radians = (double)atan2l((long double)y, (long double)x);
		volatile float  degrees = (float)((long double)radians *
		                                  (long double)57.29577951308232);
		yaw = (long double)degrees;
	}
	else
	{
		yaw = y > 0.0f ? 90.0L : 270.0L;
	}
	/* Linux adds a turn for negative or unordered, without a Float store. */
	if (!(yaw >= 0.0L))
	{
		yaw += 360.0L;
	}
	return yaw;
#endif
}
#endif

static void CG_DrawMortarReticle(void)
{
	vec4_t color          = { 1.f, 1.f, 1.f, .5f };
	vec4_t color_back     = { 0.f, 0.f, 0.f, .25f };
	vec4_t color_extends  = { .77f, .73f, .1f, 1.f };
	vec4_t color_lastfire = { .77f, .1f, .1f, 1.f };
	//vec4_t	color_firerequest = { .23f, 1.f, .23f, 1.f };
	vec4_t   color_firerequest = { 1.f, 1.f, 1.f, 1.f };
	float    offset, localOffset;
	int      i, min, majorOffset, val, printval, fadeTime;
	char     *s;
	float    angle, angleMin, angleMax;
	qboolean hasRightTarget, hasLeftTarget;

	// Background
	CG_FillRect(136, 236, 154, 38, color_back);
	CG_FillRect(290, 160, 60, 208, color_back);
	CG_FillRect(350, 236, 154, 38, color_back);

	// Horizontal bar

	// bottom
	CG_FillRect(140, 264, 150, 1, color);   // left
	CG_FillRect(350, 264, 150, 1, color);   // right

	// 10 units - 5 degrees
	// total of 360 units
	// nothing displayed between 150 and 210 units
	// 360 / 10 = 36 bits, means 36 * 5 = 180 degrees
	// that means left is cg.predictedPlayerState.viewangles[YAW] - .5f * 180
	angle = 360 - AngleNormalize360(cg.predictedPlayerState.viewangles[YAW] - 90.f);

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	offset = CG_MortarScaleOffset(angle, 13107.2001953125f, 0.0000762939453125f);
#else
	offset = (5.f / 65536) * ((int)(angle * (65536 / 5.f)) & 65535);
#endif
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	CG_MortarHorizontalLabels(angle, &min, &majorOffset);
#else
	min         = (int)(AngleNormalize360(angle - .5f * 180) / 15.f) * 15;
	majorOffset = (int)(floor((int)floor(AngleNormalize360(angle - .5f * 180)) % 15) / 5.f);
#endif

	for ( val = i = 0; i < 36; i++ )
	{
		localOffset = i * 10.f + (offset * 2.f);

		if (localOffset >= 150 && localOffset <= 210)
		{
			if (i % 3 == majorOffset)
			{
				val++;
			}
			continue;
		}

		if (i % 3 == majorOffset)
		{
			printval = min - val * 15 + 180;

			// rain - old tertiary abuse was nasty and had undefined result
			if (printval < 0)
			{
				printval += 360;
			}
			else if (printval >= 360)
			{
				printval -= 360;
			}

			s = va("%i", printval);
			//CG_Text_Paint_Ext( 140 + localOffset - .5f * CG_Text_Width_Ext( s, .15f, 0, &cgs.media.limboFont1 ), 244, .15f, .15f, color, s, 0, 0, 0, &cgs.media.limboFont1 );
			//CG_FillRect( 140 + localOffset, 248, 1, 16, color);
			CG_Text_Paint_Ext(500 - localOffset - .5f * CG_Text_Width_Ext(s, .15f, 0, &cgs.media.limboFont1), 244, .15f, .15f, color, s, 0, 0, 0, &cgs.media.limboFont1);
			CG_FillRect(500 - localOffset, 248, 1, 16, color);
			val++;
		}
		else
		{
			//CG_FillRect( 140 + localOffset, 256, 1, 8, color);
			CG_FillRect(500 - localOffset, 256, 1, 8, color);
		}
	}

	// the extremes
	// 30 degrees plus a 15 degree border
	angleMin = AngleNormalize360(360 - (cg.pmext.mountedWeaponAngles[YAW] - 90.f) - (30.f + 15.f));
	angleMax = AngleNormalize360(360 - (cg.pmext.mountedWeaponAngles[YAW] - 90.f) + (30.f + 15.f));

	// right
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		long double markerOffset = CG_MortarHorizontalOffset((long double)AngleNormalize360(angle - angleMin));
		CG_FillRect((float)(320.0L - markerOffset), 252, 2, 18, color_extends);
	}
#else
	localOffset = (AngleNormalize360(angle - angleMin) / 5.f) * 10.f;
	//CG_FillRect( 320 + localOffset, 252, 2, 18, color_extends);
	CG_FillRect(320 - localOffset, 252, 2, 18, color_extends);
#endif

	// left
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		long double markerOffset = CG_MortarHorizontalOffset((long double)AngleNormalize360(angleMax - angle));
		CG_FillRect((float)(320.0L + markerOffset), 252, 2, 18, color_extends);
	}
#else
	localOffset = (AngleNormalize360(angleMax - angle) / 5.f) * 10.f;
	//CG_FillRect( 320 - localOffset, 252, 2, 18, color_extends);
	CG_FillRect(320 + localOffset, 252, 2, 18, color_extends);
#endif

	// last fire pos
	fadeTime = 0;
	if (cg.lastFiredWeapon == 60 && cg.mortarImpactTime >= -1)
	{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		fadeTime = CG_MortarTimeSince(cg.time, cg.predictedPlayerEntity.muzzleFlashTime, 5000u);
#else
		fadeTime = cg.time - (cg.predictedPlayerEntity.muzzleFlashTime + 5000);
#endif

		if (fadeTime < 3000)
		{
			float lastfireAngle;

			if (fadeTime > 0)
			{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
				color_lastfire[3] = CG_MortarFadeAlpha(fadeTime, 0.00033333332976326346f);
#else
				color_lastfire[3] = 1.f - (fadeTime / 3000.f);
#endif
			}

			lastfireAngle = AngleNormalize360(360 - (cg.mortarFireAngles[YAW] - 90.f));

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
			{
				long double difference = TCE_AngleSubtractValue(angle, lastfireAngle);
				CG_FillRect((float)(320.0L - CG_MortarHorizontalOffset(difference)),
				            252, 2, 18, color_lastfire);
			}
#else
			localOffset = ((AngleSubtract(angle, lastfireAngle)) / 5.f) * 10.f;
			//CG_FillRect( 320 + localOffset, 252, 2, 18, color_lastfire);
			CG_FillRect(320 - localOffset, 252, 2, 18, color_lastfire);
#endif
		}
	}

	// mortar attack requests
	hasRightTarget = hasLeftTarget = qfalse;
	for ( i = 0; i < MAX_CLIENTS; i++ )
	{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		int requestFadeTime = CG_MortarTimeSince(cg.time, cg.artilleryRequestTime[i], 25000u);
#else
		int requestFadeTime = cg.time - (cg.artilleryRequestTime[i] + 25000);
#endif

		if (requestFadeTime < 5000)
		{
			float attackRequestAngle;
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
			long double yaw = CG_MortarRequestYaw(cg.artilleryRequestPos[i],
			                                      cg.predictedPlayerEntity.lerpOrigin);
#else
			vec3_t dir;
			float  yaw;

			VectorSubtract(cg.artilleryRequestPos[i], cg.predictedPlayerEntity.lerpOrigin, dir);

			// ripped this out of vectoangles
			if (dir[1] == 0 && dir[0] == 0)
			{
				yaw = 0;
			}
			else
			{
				if (dir[0])
				{
					yaw = (atan2(dir[1], dir[0]) * 180 / M_PI);
				}
				else if (dir[1] > 0)
				{
					yaw = 90;
				}
				else
				{
					yaw = 270;
				}
				if (yaw < 0)
				{
					yaw += 360;
				}
			}

#endif

			if (requestFadeTime > 0)
			{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
				color_firerequest[3] = CG_MortarFadeAlpha(requestFadeTime, 0.00019999999494757503f);
#else
				color_firerequest[3] = 1.f - (requestFadeTime / 5000.f);
#endif
			}

			attackRequestAngle = AngleNormalize360(360 - (yaw - 90.f));

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
			yaw = TCE_AngleSubtractValue(attackRequestAngle, angleMin);
			if (!(yaw >= 0.0L))
			{
#else
			yaw = AngleSubtract(attackRequestAngle, angleMin);
			if (yaw < 0)
			{
#endif
				if (!hasLeftTarget)
				{
					//CG_FillRect( 136 + 2, 236 + 38 - 6, 4, 4, color_firerequest );

					trap_R_SetColor(color_firerequest);
					CG_DrawPic(136 + 2, 236 + 38 - 10 + 1, 8, 8, cgs.media.ccMortarTargetArrow);
					trap_R_SetColor(NULL);

					hasLeftTarget = qtrue;
				}
			}
			else if (yaw > 90)
			{
				if (!hasRightTarget)
				{
					//CG_FillRect( 350 + 154 - 6, 236 + 38 - 6, 4, 4, color_firerequest );

					trap_R_SetColor(color_firerequest);
					CG_DrawPic(350 + 154 - 10, 236 + 38 - 10 + 1, -8, 8, cgs.media.ccMortarTargetArrow);
					trap_R_SetColor(NULL);

					hasRightTarget = qtrue;
				}
			}
			else
			{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
				{
					long double    difference = TCE_AngleSubtractValue(angle, attackRequestAngle);
					volatile float doubled    = (float)CG_MortarHorizontalOffset(difference);
					localOffset = doubled;
				}
#else
				localOffset = ((AngleSubtract(angle, attackRequestAngle)) / 5.f) * 10.f;
#endif
				//CG_FillRect( 320 + localOffset - 3, 264 - 3, 6, 6, color_firerequest );

				trap_R_SetColor(color_firerequest);
				//CG_DrawPic( 320 + localOffset - 8, 264 - 8, 16, 16, cgs.media.ccMortarTarget );
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
				CG_DrawPic((float)((320.0L - (long double)localOffset) - 8.0L),
				           264 - 8, 16, 16, cgs.media.ccMortarTarget);
#else
				CG_DrawPic(320 - localOffset - 8, 264 - 8, 16, 16, cgs.media.ccMortarTarget);
#endif
				trap_R_SetColor(NULL);
			}
		}
	}

	/*s = va( "%.2f (%i / %i)",AngleNormalize360(angle - .5f * 180), majorOffset, min );
	CG_Text_Paint( 140, 224, .25f, color, s, 0, 0, 0 );
	s = va( "%.2f",AngleNormalize360(angle) );
	CG_Text_Paint( 320 - .5f * CG_Text_Width( s, .25f, 0), 224, .25f, color, s, 0, 0, 0 );
	s = va( "%.2f", AngleNormalize360(angle + .5f * 180) );
	CG_Text_Paint( 500 - CG_Text_Width( s, .25f, 0 ), 224, .25f, color, s, 0, 0, 0 );*/

	// Vertical bar

	// sides
	CG_FillRect(295, 164, 1, 200, color);   // left
	CG_FillRect(345, 164, 1, 200, color);   // right

	// 10 units - 2.5 degrees
	// total of 200 units
	// 200 / 10 = 20 bits, means 20 * 2.5 = 50 degrees
	// that means left is cg.predictedPlayerState.viewangles[PITCH] - .5f * 50
	angle = AngleNormalize180(360 - (cg.predictedPlayerState.viewangles[PITCH] - 60));

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	offset = CG_MortarScaleOffset(angle, 26214.400390625f, 0.00003814697265625f);
#else
	offset = (2.5f / 65536) * ((int)(angle * (65536 / 2.5f)) & 65535);
#endif
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	CG_MortarVerticalLabels(angle, &min, &majorOffset);
#else
	min         = floor((angle + .5f * 50) / 10.f) * 10;
	majorOffset = (int)(floor((int)((angle + .5f * 50) * 10.f) % 100) / 25.f);
#endif

	for ( val = i = 0; i < 20; i++ )
	{
		localOffset = i * 10.f + (offset * 4.f);

		/*if( localOffset >= 150 && localOffset <= 210 ) {
			if( i % 3 == majorOffset)
				val++;
			continue;
		}*/

		if (i % 4 == majorOffset)
		{
			printval = min - val * 10;

			// rain - old tertiary abuse was nasty and had undefined result
			if (printval <= -180)
			{
				printval += 360;
			}
			else if (printval >= 180)
			{
				printval -= 180;
			}

			s = va("%i", printval);
			CG_Text_Paint_Ext(320 - .5f * CG_Text_Width_Ext(s, .15f, 0, &cgs.media.limboFont1), 164 + localOffset + .5f * CG_Text_Height_Ext(s, .15f, 0, &cgs.media.limboFont1), .15f, .15f, color, s, 0, 0, 0, &cgs.media.limboFont1);
			CG_FillRect(295 + 1, 164 + localOffset, 12, 1, color);
			CG_FillRect(345 - 12, 164 + localOffset, 12, 1, color);
			val++;
		}
		else
		{
			CG_FillRect(295 + 1, 164 + localOffset, 8, 1, color);
			CG_FillRect(345 - 8, 164 + localOffset, 8, 1, color);
		}
	}

	// the extremes
	// 30 degrees up
	// 20 degrees down
	angleMin = AngleNormalize180(360 - (cg.pmext.mountedWeaponAngles[PITCH] - 60)) - 20.f;
	angleMax = AngleNormalize180(360 - (cg.pmext.mountedWeaponAngles[PITCH] - 60)) + 30.f;

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	CG_MortarPitchMarker((long double)angleMax - (long double)angle, qtrue, color_extends);
	CG_MortarPitchMarker((long double)angle - (long double)angleMin, qfalse, color_extends);
	if (cg.lastFiredWeapon == 60 && cg.mortarImpactTime >= -1 && fadeTime < 3000)
	{
		float lastfireAngle = AngleNormalize180(360 - (cg.mortarFireAngles[PITCH] - 60));
		if (lastfireAngle > angle)
		{
			CG_MortarPitchMarker((long double)lastfireAngle - (long double)angle, qtrue, color_lastfire);
		}
		else
		{
			CG_MortarPitchMarker((long double)angle - (long double)lastfireAngle, qfalse, color_lastfire);
		}
	}
#else
	// top
	localOffset = angleMax - angle;
	if (localOffset < 0)
	{
		localOffset = 0;
	}
	localOffset = (AngleNormalize360(localOffset) / 2.5f) * 10.f;
	if (localOffset < 100)
	{
		CG_FillRect(295 - 2, 264 - localOffset, 6, 2, color_extends);
		CG_FillRect(345 - 4 + 1, 264 - localOffset, 6, 2, color_extends);
	}

	// bottom
	localOffset = angle - angleMin;
	if (localOffset < 0)
	{
		localOffset = 0;
	}
	localOffset = (AngleNormalize360(localOffset) / 2.5f) * 10.f;
	if (localOffset < 100)
	{
		CG_FillRect(295 - 2, 264 + localOffset, 6, 2, color_extends);
		CG_FillRect(345 - 4 + 1, 264 + localOffset, 6, 2, color_extends);
	}

	// last fire pos
	if (cg.lastFiredWeapon == 60 && cg.mortarImpactTime >= -1)
	{
		if (fadeTime < 3000)
		{
			float lastfireAngle;

			lastfireAngle = AngleNormalize180(360 - (cg.mortarFireAngles[PITCH] - 60));

			if (lastfireAngle > angle)
			{
				localOffset = lastfireAngle - angle;
				if (localOffset < 0)
				{
					localOffset = 0;
				}
				localOffset = (AngleNormalize360(localOffset) / 2.5f) * 10.f;
				if (localOffset < 100)
				{
					CG_FillRect(295 - 2, 264 - localOffset, 6, 2, color_lastfire);
					CG_FillRect(345 - 4 + 1, 264 - localOffset, 6, 2, color_lastfire);
				}
			}
			else
			{
				localOffset = angle - lastfireAngle;
				if (localOffset < 0)
				{
					localOffset = 0;
				}
				localOffset = (AngleNormalize360(localOffset) / 2.5f) * 10.f;
				if (localOffset < 100)
				{
					CG_FillRect(295 - 2, 264 + localOffset, 6, 2, color_lastfire);
					CG_FillRect(345 - 4 + 1, 264 + localOffset, 6, 2, color_lastfire);
				}
			}
		}
	}

#endif
	/*s = va( "%.2f (%i / %i)", angle + .5f * 50, majorOffset, min );
	CG_Text_Paint( 348, 164, .25f, color, s, 0, 0, 0 );
	s = va( "%.2f",angle );
	CG_Text_Paint( 348, 264, .25f, color, s, 0, 0, 0 );
	s = va( "%.2f", angle - .5f * 50 );
	CG_Text_Paint( 348, 364, .25f, color, s, 0, 0, 0 );*/
}


/*
==============
CG_DrawBinocReticle
==============
*/
static void CG_DrawBinocReticle(void)
{
	// an alternative.  This gives nice sharp lines at the expense of a few extra polys
	vec4_t color;
	color[0] = color[1] = color[2] = 0;
	color[3] = 1;

	if (cgs.media.binocShaderSimple)
	{
		CG_DrawPic(0, 0, 640, 480, cgs.media.binocShaderSimple);
	}

	CG_FillRect(146, 239, 348, 1, color);

	CG_FillRect(188, 234, 1, 13, color);    // ll
	CG_FillRect(234, 226, 1, 29, color);    // l
	CG_FillRect(274, 234, 1, 13, color);    // lr
	CG_FillRect(320, 213, 1, 55, color);    // center
	CG_FillRect(360, 234, 1, 13, color);    // rl
	CG_FillRect(406, 226, 1, 29, color);    // r
	CG_FillRect(452, 234, 1, 13, color);    // rr
}

void CG_FinishWeaponChange(int lastweap, int newweap); // JPW NERVE


/*
=================
CG_DrawCrosshair
=================
*/
static void CG_DrawCrosshair(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean        previous;
	float           w, h;
	qhandle_t       hShader;
	float           f;
	float           x, y;
	int             weapnum;    // DHM - Nerve

	if (cg.renderingThirdPerson)
	{
		return;
	}

	// using binoculars
	if (cg.zoomedBinoc)
	{
		CG_DrawBinocReticle();
		return;
	}

	// DHM - Nerve :: show reticle in limbo and spectator
	if ((cg.snap->ps.pm_flags & PMF_FOLLOW) || cg.demoPlayback)
	{
		weapnum = cg.snap->ps.weapon;
	}
	else
	{
		weapnum = cg.predictedPlayerState.weapon;
	}


	switch (weapnum)
	{

	// weapons that get no reticle
	case WP_NONE:       // no weapon, no crosshair
		if (cg.zoomedBinoc)
		{
			CG_DrawBinocReticle();
		}

		if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR)
		{
			return;
		}
		break;

	// special reticle for weapon
	case 59:
	case 57:
	case 58:
		if (!BG_PlayerMounted(cg.snap->ps.eFlags))
		{
			// JPW NERVE -- don't let players run with rifles -- speed 80 == crouch, 128 == walk, 256 == run
			if (VectorLengthSquared(cg.snap->ps.velocity) > SQR(127))
			{
				if (cg.snap->ps.weapon == 59)
				{
					CG_FinishWeaponChange(59, 33);
				}
				if (cg.snap->ps.weapon == 57)
				{
					CG_FinishWeaponChange(57, 25);
				}
				if (cg.snap->ps.weapon == 58)
				{
					CG_FinishWeaponChange(58, 32);
				}
			}

			// OSP
			if (cg.mvTotalClients < 1 || cg.snap->ps.stats[STAT_HEALTH] > 0)
			{
				CG_DrawWeapReticle();
			}

			return;
		}
		break;
	default:
		if (weapnum > 0 && weapnum < TCE_MAX_WEAPONS &&
		    weaponDef[weapnum].scoped > 1.0f && cg.tceAimActive &&
		    cg.tceAimComplete && !cg.tceScopeBlocked)
		{
			CG_DrawWeapReticle();
			return;
		}
		break;
	}

	if (cg.predictedPlayerState.eFlags & EF_PRONE_MOVING)
	{
		return;
	}

	// FIXME: spectators/chasing?
	if (cg.predictedPlayerState.weapon == 60 && cg.predictedPlayerState.weaponstate != WEAPON_RAISING)
	{
		CG_DrawMortarReticle();
		return;
	}

	if (cg_drawCrosshair.integer < 0 || !developer.integer)     // TC:E keeps ordinary crosshairs developer-only; scope reticles remain above.
	{
		return;
	}

	// no crosshair while leaning
	if (cg.snap->ps.leanf)
	{
		return;
	}

	// TAT 1/10/2003 - Don't draw crosshair if have exit hintcursor
	if (cg.snap->ps.serverCursorHint >= HINT_EXIT && cg.snap->ps.serverCursorHint <= HINT_NOEXIT)
	{
		return;
	}

	// set color based on health
	if (cg_crosshairHealth.integer)
	{
		vec4_t hcolor;

		CG_ColorForHealth(hcolor);
		trap_R_SetColor(hcolor);
	}
	else
	{
		trap_R_SetColor(cg.xhairColor);
	}

	w = h = cg_crosshairSize.value;

	/* TC spread state and per-weapon coefficients, not SDK aimSpreadScale. */
	f = 0;
	if (!(cg.predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS] & 4))
	{
		f = (float)cg.predictedPlayerState.stats[12] * ((float)weaponDef[cg.predictedPlayerState.weapon].unknown_0f8[0] * 0.001f) * 0.001f
		    + (float)cg.predictedPlayerState.stats[11] * ((float)weaponDef[cg.predictedPlayerState.weapon].unknown_0f8[3] * 0.001f) * 0.001f;
	}
	previous = tce_uiCoordinates; tce_uiCoordinates = qtrue;
	w       *= (1 + f * 2.0);
	h       *= (1 + f * 2.0);

	x = cg_crosshairX.integer + 426;
	y = cg_crosshairY.integer + 240;
	CG_AdjustFrom640(&x, &y, &w, &h);

	hShader = cgs.media.crosshairShader[cg_drawCrosshair.integer % NUM_CROSSHAIRS];

	trap_R_DrawStretchPic(x - 0.5f * w, y - 0.5f * h, w, h, 0, 0, 1, 1, hShader);

	if (cg.crosshairShaderAlt[cg_drawCrosshair.integer % NUM_CROSSHAIRS])
	{
		w = h = cg_crosshairSize.value;
		x = cg_crosshairX.integer + 426;
		y = cg_crosshairY.integer + 240;
		CG_AdjustFrom640(&x, &y, &w, &h);

		if (cg_crosshairHealth.integer == 0)
		{
			trap_R_SetColor(cg.xhairColorAlt);
		}

		trap_R_DrawStretchPic(x - 0.5f * w, y - 0.5f * h, w, h, 0, 0, 1, 1, cg.crosshairShaderAlt[cg_drawCrosshair.integer % NUM_CROSSHAIRS]);
	}
	tce_uiCoordinates = previous;
}

static void CG_DrawNoShootIcon(void)
{
	float x, y, w, h;
	float *color;

	/* TC protocol weapon65; SDK WP_PANZERFAUST is weapon5. */
	if (cg.predictedPlayerState.eFlags & EF_PRONE && cg.snap->ps.weapon == 65)
	{
		trap_R_SetColor(colorRed);
	}
	else if (cg.crosshairClientNoShoot
	         // xkan, 1/6/2003 - don't shoot friend or civilian
	         || cg.snap->ps.serverCursorHint == HINT_PLYR_NEUTRAL
	         || cg.snap->ps.serverCursorHint == HINT_PLYR_FRIEND)
	{
		color = CG_FadeColor(cg.crosshairClientTime, 1000);

		if (!color)
		{
			trap_R_SetColor(NULL);
			return;
		}
		else
		{
			trap_R_SetColor(color);
		}
	}
	else
	{
		return;
	}

	w = h = 48.f;

	x = cg_crosshairX.integer + 1;
	y = cg_crosshairY.integer + 1;
	CG_AdjustFrom640(&x, &y, &w, &h);

	// FIXME precache
	trap_R_DrawStretchPic(x + 0.5 * (cg.refdef_current->width - w), y + 0.5 * (cg.refdef_current->height - h), w, h, 0, 0, 1, 1, cgs.media.friendShader);
}

/*
=================
CG_ScanForCrosshairEntity
=================

Returns the distance to the entity

*/
static float CG_ScanForCrosshairEntity(float *zChange, qboolean *hitClient)
{
	trace_t trace;
//	gentity_t	*traceEnt;
	vec3_t    start, end;
	float     dist;
	centity_t *cent;

	// We haven't hit a client yet
	*hitClient = qfalse;

	VectorCopy(cg.refdef.vieworg, start);
	VectorMA(start, 8192, cg.refdef.viewaxis[0], end);      //----(SA)	changed from 8192

	cg.crosshairClientNoShoot = qfalse;

	CG_Trace(&trace, start, NULL, NULL, end, cg.snap->ps.clientNum, CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_ITEM);

	// How far from start to end of trace?
	dist = VectorDistance(start, trace.endpos);

	// How far up or down are we looking?
	*zChange = trace.endpos[2] - start[2];

	if (trace.entityNum >= MAX_CLIENTS)
	{
		if (cg_entities[trace.entityNum].currentState.eFlags & EF_TAGCONNECT)
		{
			trace.entityNum = cg_entities[trace.entityNum].tagParent;
		}

		// is a tank with a healthbar
		// this might have some side-effects, but none right now as the script_mover is the only one that sets effect1Time
		if ((cg_entities[trace.entityNum].currentState.eType == ET_MOVER && cg_entities[trace.entityNum].currentState.effect1Time) ||
		    cg_entities[trace.entityNum].currentState.eType == ET_CONSTRUCTIBLE_MARKER)
		{
			// update the fade timer
			cg.crosshairClientNum    = trace.entityNum;
			cg.crosshairClientTime   = cg.time;
			cg.identifyClientRequest = cg.crosshairClientNum;
		}

		// Default: We're not looking at a client
		cg.crosshairNotLookingAtClient = qtrue;

		return dist;
	}

//	traceEnt = &g_entities[trace.entityNum];

	if (CG_PointContents(trace.endpos, 0) & CONTENTS_FOG)
	{
		cg.crosshairNotLookingAtClient = qtrue;
		return dist;
	}

	// Reset the draw time for the SP crosshair
	cg.crosshairSPClientTime = cg.time;

	// Default: We're not looking at a client
	cg.crosshairNotLookingAtClient = qfalse;

	// We hit a client
	*hitClient = qtrue;

	// Original target-acquisition timer drives the delayed enemy name.
	if (cg.crosshairClientNum != trace.entityNum && trace.entityNum != ENTITYNUM_WORLD)
	{
		cg.tceCrosshairTargetTime = cg.time;
	}
	cg.crosshairClientNum  = trace.entityNum;
	cg.crosshairClientTime = cg.time;
	if (cg.crosshairClientNum != cg.snap->ps.identifyClient && cg.crosshairClientNum != ENTITYNUM_WORLD)
	{
		cg.identifyClientRequest = cg.crosshairClientNum;
	}

	cent = &cg_entities[cg.crosshairClientNum];

	if (cent && cent->currentState.powerups & (1 << PW_OPS_DISGUISED))
	{
		if (cgs.clientinfo[cg.crosshairClientNum].team == cgs.clientinfo[cg.clientNum].team)
		{
			cg.crosshairClientNoShoot = qtrue;
		}
	}

	return dist;
}



#define CH_KNIFE_DIST       48  // from g_weapon.c
#define CH_LADDER_DIST      100
#define CH_WATER_DIST       100
#define CH_BREAKABLE_DIST   64
#define CH_DOOR_DIST        96

#define CH_DIST             100 //128		// use the largest value from above

/*
==============
CG_CheckForCursorHints
	concept in progress...
==============
*/
void CG_CheckForCursorHints(void)
{
	trace_t   trace;
	vec3_t    start, end;
	centity_t *hit;
	if (cg.renderingThirdPerson)
	{
		return;
	}
	if (cg.snap->ps.serverCursorHint)
	{
		cg.cursorHintTime  = cg.time;
		cg.cursorHintFade  = 500;
		cg.cursorHintIcon  = cg.snap->ps.serverCursorHint;
		cg.cursorHintValue = cg.snap->ps.serverCursorHintVal;
		return;
	}
	VectorCopy(cg.refdef_current->vieworg, start);
	VectorMA(start, 100.f, cg.refdef_current->viewaxis[0], end);
	CG_Trace(&trace, start, vec3_origin, vec3_origin, end,
	         cg.snap->ps.clientNum, MASK_PLAYERSOLID);
	if (trace.fraction == 1.f)
	{
		return;
	}
	hit = &cg_entities[trace.entityNum];
	if (trace.entityNum >= MAX_CLIENTS &&
	    (hit->currentState.powerups == STATE_INVISIBLE ||
	     hit->currentState.powerups == STATE_UNDERCONSTRUCTION))
	{
		return;
	}
	/* TC has no client-predicted backstab/knife cursor branch. */
	if (trace.entityNum == ENTITYNUM_WORLD &&
	    (trace.surfaceFlags & SURF_LADDER) && !(cg.snap->ps.pm_flags & PMF_LADDER) &&
	    trace.fraction * 100.f <= 100.f)
	{
		cg.cursorHintIcon  = HINT_LADDER;
		cg.cursorHintTime  = cg.time;
		cg.cursorHintFade  = 500;
		cg.cursorHintValue = 0;
	}
}



/*
=====================
CG_DrawCrosshairNames
=====================
*/
/* Keep crosshair identification for server-supplied teammate information.
 * CQB no longer paints the SDK center-screen crosshair name. */
static void CG_UpdateCrosshairTarget(void)
{
	float    zChange;
	qboolean hitClient;
	if (cg_drawCrosshair.integer >= 0)
	{
		CG_ScanForCrosshairEntity(&zChange, &hitClient);
	}
}

/* CQB CG_Draw2D 0005a5c0 overhead labels, adapted to native TC structures.
 * The optional cvar and independent per-name alpha are TCE2 extensions. */
static void CG_DrawTeamNames(void)
{
	extern qboolean tce_uiCoordinates;
	extern int      tceSmokeNewBBox;
	int             i, team = cg.predictedPlayerState.persistant[PERS_TEAM];
	qboolean        previous;
	if (!cg_drawTeamNames.integer || cg.showScores || cg.showGameView ||
	    team == TEAM_SPECTATOR || cg.renderingThirdPerson)
	{
		return;
	}
	previous          = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	for (i = 0; i < MAX_CLIENTS; ++i)
	{
		centity_t    *cent = &cg_entities[i];
		clientInfo_t *ci   = &cgs.clientinfo[i];
		vec3_t       direction;
		vec4_t       color = { 1, 1, 1, .5f };
		float        distance, forward, side, up, scale, x, y, height, projection;
		if (i == cg.predictedPlayerState.clientNum || !ci->infoValid || ci->team != team ||
		    ci->health <= 0 || cent->currentState.eType != ET_PLAYER ||
		    (cent->currentState.eFlags & (EF_DEAD | EF_NODRAW | EF_CONNECTION)))
		{
			cent->tceTeamNameUntil = 0;
			continue;
		}
		/* CQB also gates on its team-position feed. TC's scanner publishes
		 * only the bomb carrier, so use the real snapshot/visibility state
		 * instead; importing that CQB wire gate would hide all normal names. */
		if (cent->currentValid && cent->tceVisible)
		{
			cent->tceTeamNameUntil = cg.time + 500;
		}
		if (!cent->tceTeamNameUntil || cg.time > cent->tceTeamNameUntil)
		{
			continue;
		}
		if (!cent->currentValid || !cent->tceVisible)
		{
			color[3] *= (cent->tceTeamNameUntil - cg.time) * .002f;
		}
		height = (cent->currentState.effect1Time & 4) ? 12.f : 48.f;
		if (tceSmokeNewBBox)
		{
			height += 12.f;
		}
		if (cent->currentState.animMovetype)
		{
			height -= 18.f;
		}
		VectorSubtract(cent->lerpOrigin, cg.refdef.vieworg, direction);
		direction[2] += height;
		distance      = VectorNormalize(direction);
		forward       = DotProduct(direction, cg.refdef.viewaxis[0]);
		if (forward < .5f || distance > 4096.f || cg.tceNameTanHalfFov <= 0)
		{
			continue;
		}
		side = DotProduct(direction, cg.refdef.viewaxis[1]);
		up   = DotProduct(direction, cg.refdef.viewaxis[2]);
		/* Algebraically the original radial/(cos(asin(radial))*tanFov)
		 * projection, without its 0/0 at the exact center of the screen. */
		projection = 426.f / (forward * cg.tceNameTanHalfFov);
		if (distance < 384.f)
		{
			distance = 384.f;
		}
		scale = .2f * sqrt(384.f / distance);
		if (distance > 3584.f)
		{
			color[3] *= (4096.f - distance) / 512.f;
		}
		x = 426.f - side * projection - CG_Text_Width_Ext(ci->name, scale, 0, &cgs.media.limboFont1) * .5f;
		y = 240.f - up * projection;
		CG_Text_Paint_Ext(x, y, scale, scale, color, ci->name, 0, 0, 3, &cgs.media.limboFont1);
	}
	trap_R_SetColor(NULL);
	tce_uiCoordinates = previous;
}

//==============================================================================

/*
=================
CG_DrawSpectator
=================
*/
static void CG_DrawSpectator(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean        previous = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	CG_Text_Paint_Ext(8, 118, .25f, .25f, colorWhite, CG_TranslateString("Spectating"), 0, 0, 3, &cgs.media.limboFont1);
	tce_uiCoordinates = previous;
}

/*
=================
CG_DrawVote
=================
*/
void CG_DrawVote(void);

/*
=================
CG_DrawIntermission
=================
*/
void CG_DrawIntermission(void)
{
	// End-of-level autoactions
	if (!cg.demoPlayback)
	{
		static int doScreenshot = 0, doDemostop = 0;

		if (!cg.latchAutoActions)
		{
			cg.latchAutoActions = qtrue;

			if (cg_autoAction.integer & AA_SCREENSHOT)
			{
				doScreenshot = cg.time + 1000;
			}

			if (cg_autoAction.integer & AA_STATSDUMP)
			{
				CG_dumpStats_f();
			}

			if ((cg_autoAction.integer & AA_DEMORECORD) &&
			    ((cgs.gametype == GT_WOLF_STOPWATCH && cgs.currentRound == 0) ||
			     cgs.gametype != GT_WOLF_STOPWATCH))
			{
				doDemostop = cg.time + 5000;    // stats should show up within 5 seconds
			}
		}

		if (doScreenshot > 0 && doScreenshot < cg.time)
		{
			CG_autoScreenShot_f();
			doScreenshot = 0;
		}

		if (doDemostop > 0 && doDemostop < cg.time)
		{
			trap_SendConsoleCommand("stoprecord\n");
			doDemostop = 0;
		}
	}

	// Intermission view
	CG_Debriefing_Draw();

/*	cg.scoreFadeTime = cg.time;
	CG_DrawScoreboard();
*/
}

/*
=================
CG_ActivateLimboMenu

NERVE - SMF
=================
*/
static void CG_ActivateLimboMenu(void)
{
/*	static qboolean latch = qfalse;
	qboolean test;

	// should we open the limbo menu (make allowances for MV clients)
	test = ((cg.snap->ps.pm_flags & PMF_LIMBO) ||
			( (cg.mvTotalClients < 1 && (
				(cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) ||
				(cg.warmup))
			  )
			&& cg.snap->ps.pm_type != PM_INTERMISSION ) );


	// auto open/close limbo mode
	if(cg_popupLimboMenu.integer && !cg.demoPlayback) {
		if(test && !latch) {
			CG_LimboMenu_f();
			latch = qtrue;
		} else if(!test && latch && cg.showGameView) {
			CG_EventHandling(CGAME_EVENT_NONE, qfalse);
			latch = qfalse;
		}
	}*/
}

/*
=================
CG_DrawSpectatorMessage
=================
*/
static void CG_DrawSpectatorMessage(void)
{
	const char      *str, *str2;
	extern qboolean tce_uiCoordinates;
	qboolean        previous;
	static int      lastconfigGet = 0;

	if (!cg_descriptiveText.integer)
	{
		return;
	}

	if (!(cg.snap->ps.pm_flags & PMF_LIMBO || cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR))
	{
		return;
	}

	if (cg.time - lastconfigGet > 1000)
	{
		Controls_GetConfig();

		lastconfigGet = cg.time;
	}

	previous          = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;

	str2 = BindingFromName("openlimbomenu");
	if (!Q_stricmp(str2, "(openlimbomenu)"))
	{
		str2 = "ESCAPE";
	}
	str = va(CG_TranslateString("Press %s to open Deploy Menu"), str2);
	CG_Text_Paint_Ext(8, 154, .2f, .2f, colorWhite, str, 0, 0, 3, &cgs.media.limboFont1);

	str2 = BindingFromName("+attack");
	str  = va(CG_TranslateString("Press %s to follow next player"), str2);
	CG_Text_Paint_Ext(8, 168, .2f, .2f, colorWhite, str, 0, 0, 3, &cgs.media.limboFont1);

	tce_uiCoordinates = previous;
}


static int CG_ReinforcementMilliseconds(qboolean menu)
{
	team_t team;
	int    dwDeployTime;

	if (menu)
	{
		if (cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR)
		{
			team = cgs.ccSelectedTeam == 0 ? TEAM_AXIS : TEAM_ALLIES;
		}
		else
		{
			team = cgs.clientinfo[cg.clientNum].team;
		}
	}
	else
	{
		team = cgs.clientinfo[cg.snap->ps.clientNum].team;
	}

	dwDeployTime = (team == TEAM_AXIS) ? cg_redlimbotime.integer : cg_bluelimbotime.integer;
	return dwDeployTime - ((cgs.aReinfOffset[team] + cg.time - cgs.levelStartTime) % dwDeployTime);
}

float CG_CalculateReinfTime_Float(qboolean menu)
{
	return 1.0f + CG_ReinforcementMilliseconds(menu) * 0.001f;
}

int CG_CalculateReinfTime(qboolean menu)
{
	return((int)CG_CalculateReinfTime_Float(menu));
}



/*
=================
CG_DrawLimboMessage
=================
*/

#define INFOTEXT_STARTX 8

/* Original TC:E message functions share the proportional ArialBlack font. */
static const char *CG_MessageTranslate(const char *s)
{
	return CG_TranslateString(s);
}
static const char *CG_MessageBinding(const char *s)
{
	return BindingFromName(s);
}
static int CG_MessageWidth(const char *s, float scale, int limit, void *font)
{
	return CG_Text_Width_Ext(s, scale, limit, (fontInfo_t *)font);
}
static void CG_MessagePaint(float x, float y, float sx, float sy, const float *color, const char *text, float adjust, int limit, int style, void *font)
{
	CG_Text_Paint_Ext(x, y, sx, sy, (float *)color, text, adjust, limit, style, (fontInfo_t *)font);
}
static void CG_MessageSmall(int x, int y, const char *s, const float *color)
{
	CG_DrawSmallStringColor(x, y, s, (float *)color);
}
static int CG_MessageViewing(void)
{
	return CG_ViewingDraw();
}
static int CG_MessageReinf(int menu)
{
	return CG_CalculateReinfTime(menu);
}
static void CG_MessageState(tce_hud_messages_t *s, tce_hud_messages_api_t *a)
{
	playerState_t *ps = &cg.snap->ps;
	memset(s, 0, sizeof(*s)); memset(a, 0, sizeof(*a));
	s->health        = ps->stats[STAT_HEALTH]; s->pmFlags = ps->pm_flags;
	s->localTeam     = cgs.clientinfo[cg.clientNum].team; s->descriptive = cg_descriptiveText.integer;
	s->gameType      = cgs.gametype; s->respawns = ps->persistant[PERS_RESPAWNS_LEFT]; s->penalty = ps->persistant[PERS_RESPAWNS_PENALTY];
	s->client        = ps->clientNum; s->localClient = cg.clientNum; s->followTeam = cgs.clientinfo[ps->clientNum].team;
	s->redTime       = cg_redlimbotime.integer; s->blueTime = cg_bluelimbotime.integer; s->name = cgs.clientinfo[ps->clientNum].name;
	s->objectiveTime = cg.oidPrintTime; s->objectiveLines = cg.oidPrintLines; s->objectiveCharWidth = cg.oidPrintCharWidth; s->objective = cg.oidPrint;
	a->translate     = CG_MessageTranslate; a->binding = CG_MessageBinding; a->width = CG_MessageWidth; a->paint = CG_MessagePaint;
	a->small         = CG_MessageSmall; a->setColor = trap_R_SetColor; a->fade = CG_FadeColor; a->viewing = CG_MessageViewing; a->reinf = CG_MessageReinf; a->font = &cgs.media.limboFont1;
}
static void CG_DrawLimboMessage(void)
{
#if defined(__linux__) && !defined(Q3_VM)
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	playerState_t *ps = &cg.snap->ps;
	vec4_t white = { 1, 1, 1, 1 };
	const char *key, *text, *format;
	if (ps->stats[STAT_HEALTH] > 0 || (ps->pm_flags & PMF_LIMBO) ||
	    cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR ||
	    !cg_descriptiveText.integer || cgs.gametype == 5)
	{
		return;
	}
	previous = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	if (cgs.gametype == 7 || cgs.gametype == 2)
	{
		key = BindingFromName("openlimbomenu");
		if (!Q_stricmp(key, "(openlimbomenu)"))
		{
			key = "ESCAPE";
		}
		format = CG_TranslateString("Press %s to open Deploment Menu");
		text = va(format, key);
		CG_Text_Paint_Ext(8, 154, .2f, .2f, colorWhite, text, 0, 0, 3, &cgs.media.limboFont1);
		key = BindingFromName("+moveup");
		format = CG_TranslateString("Press %s to go into reinforcement queue");
		text = va(format, key);
		CG_Text_Paint_Ext(8, 168, .2f, .2f, colorWhite, text, 0, 0, 3, &cgs.media.limboFont1);
	}
	else
	{
		if (!ps->persistant[PERS_RESPAWNS_LEFT])
		{
			text = CG_TranslateString("No more reinforcements this round.");
		}
		else
		{
			int seconds = CG_CalculateReinfTime(qfalse);
			format = CG_TranslateString("Reinforcements deploy in %d seconds.");
			text = va(format, seconds);
		}
		CG_DrawSmallStringColor(8, 118, text, white);
	}
	trap_R_SetColor(NULL);
	tce_uiCoordinates = previous;
#else
	extern qboolean    tce_uiCoordinates; qboolean previous = tce_uiCoordinates;
	tce_hud_messages_t s; tce_hud_messages_api_t a; CG_MessageState(&s, &a);
	tce_uiCoordinates = qtrue; TCE_DrawLimboMessage(&s, &a); tce_uiCoordinates = previous;
#endif
}
static qboolean CG_DrawFollow(void)
{
#if defined(__linux__) && !defined(Q3_VM)
	extern qboolean tce_uiCoordinates;
	qboolean previous = tce_uiCoordinates, result = qtrue;
	const char *format, *text;
	char buffer[128];
	tce_uiCoordinates = qtrue;
	if (CG_ViewingDraw())
	{
		goto done;
	}
	if (!(cg.snap->ps.pm_flags & PMF_FOLLOW))
	{
		result = qfalse;
		goto done;
	}
	if (!(cg.snap->ps.pm_flags & PMF_LIMBO))
	{
		text = CG_TranslateString("Following");
		CG_Text_Paint_Ext(8, 118, .25f, .25f, colorWhite, text, 0, 0, 3, &cgs.media.limboFont1);
		text = va("%s", cgs.clientinfo[cg.snap->ps.clientNum].name);
		CG_Text_Paint_Ext(84, 118, .25f, .25f, colorWhite, text, 0, 0, 3, &cgs.media.limboFont1);
		goto done;
	}
	if (cgs.gametype != 5)
	{
		if (!cg.snap->ps.persistant[PERS_RESPAWNS_LEFT])
		{
			if (cg.snap->ps.persistant[PERS_RESPAWNS_PENALTY] >= 0)
			{
				int milliseconds = cgs.clientinfo[cg.snap->ps.clientNum].team == TEAM_AXIS
				    ? cg_redlimbotime.integer : cg_bluelimbotime.integer;
				int period = TCE_TruncateNativeInteger((long double)milliseconds * .001f);
				int seconds = CG_CalculateReinfTime(qfalse);
				unsigned bits = (unsigned)cg.snap->ps.persistant[PERS_RESPAWNS_PENALTY]
				                * (unsigned)period + (unsigned)seconds;
				memcpy(&seconds, &bits, sizeof(seconds));
				format = CG_TranslateString("Bonus Life! Deploying in %d seconds");
				sprintf(buffer, format, seconds);
			}
			else
			{
				text = CG_TranslateString("No more deployments this round");
				sprintf(buffer, "%s", text);
			}
		}
		else
		{
			int seconds = CG_CalculateReinfTime(qfalse);
			format = CG_TranslateString("Deploying in %d seconds");
			sprintf(buffer, format, seconds);
		}
		CG_Text_Paint_Ext(8, 118, .25f, .25f, colorWhite, buffer, 0, 0, 3, &cgs.media.limboFont1);
	}
	if (cg.snap->ps.clientNum != cg.clientNum)
	{
		format = CG_TranslateString("Following");
		sprintf(buffer, "(%s %s)", format, cgs.clientinfo[cg.snap->ps.clientNum].name);
		CG_Text_Paint_Ext(8, 136, .25f, .25f, colorWhite, buffer, 0, 0, 3, &cgs.media.limboFont1);
	}
 done:
	tce_uiCoordinates = previous;
	return result;
#else
	extern qboolean    tce_uiCoordinates; qboolean previous = tce_uiCoordinates; int result;
	tce_hud_messages_t s; tce_hud_messages_api_t a; CG_MessageState(&s, &a);
	tce_uiCoordinates = qtrue; result = TCE_DrawFollow(&s, &a); tce_uiCoordinates = previous; return result;
#endif
}

/*
=================
CG_DrawWarmup
=================
*/
static const char *CG_WarmupTranslate(const char *text)
{
	return CG_TranslateString(text);
}
static const char *CG_WarmupBinding(const char *name)
{
	return BindingFromName(name);
}
static int CG_WarmupWidth(const char *text, float scale, int limit, void *font)
{
	return CG_Text_Width_Ext(text, scale, limit, (fontInfo_t *)font);
}
static void CG_WarmupPaint(float x, float y, float sx, float sy, const float *color, const char *text, float adjust, int limit, int style, void *font)
{
	CG_Text_Paint_Ext(x, y, sx, sy, (float *)color, text, adjust, limit, style, (fontInfo_t *)font);
}
#if defined(__linux__) && !defined(Q3_VM)
static void CG_WarmupLineLinux(const char *text, float y)
{
	fontInfo_t *font = &cgs.media.limboFont1;
	int width = CG_Text_Width_Ext(text, .2f, 0, font);
	CG_Text_Paint_Ext((float)(426 - width / 2), y, .2f, .2f,
	                  colorWhite, text, 0, 0, 3, font);
}
#endif

static void CG_DrawWarmup(void)
{
#if defined(__linux__) && !defined(Q3_VM)
	extern qboolean tce_uiCoordinates;
	qboolean previous = tce_uiCoordinates;
	const char *text, *format, *roundText, *first = "", *second = "";
	char key[32];
	int sec, defender;
	tce_uiCoordinates = qtrue;
	if (!cg.warmup)
	{
		if (cgs.gamestate != 2 && cgs.gamestate != 4)
		{
			goto done;
		}
		format = CG_TranslateString("^3WARMUP:^7 Waiting on ^2%i^7 %s");
		text = va(format, cgs.minclients, cgs.minclients == 1 ? "player" : "players");
		CG_WarmupLineLinux(text, 194);
		if (cg.demoPlayback || cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ||
		    (cg.snap->ps.pm_flags & 0x5000) == 0x1000)
		{
			goto done;
		}
		Q_strncpyz(key, BindingFromName("ready"), sizeof(key));
		if (!Q_stricmp(key, "(???" ")"))
		{
			text = "Type ^3\\ready^* in the console to start";
		}
		else
		{
			text = va("Press ^3%s^* to start", key);
		}
		text = CG_TranslateString(text);
		CG_WarmupLineLinux(text, 208);
		goto done;
	}
	sec = TCE_SubInt32(cg.warmup, cg.time) / 1000;
	if (sec < 0)
	{
		sec = 0;
	}
	format = CG_TranslateString("(WARMUP) Match begins in:");
	text = va("%s %i", format, sec + 1);
	CG_WarmupLineLinux(text, 120);
	if (sec != cg.warmupCount)
	{
		cg.warmupCount = sec;
		if (TCE_SubInt32(TCE_SubInt32(cg.warmup, cg.time), sec * 1000) > -100 && sec <= 2)
		{
			trap_S_StartLocalSound(cgs.media.tceCountBeep, 7);
		}
	}
	if (cgs.gametype != 3)
	{
		goto done;
	}
	format = CG_TranslateString("Stopwatch Round");
	roundText = va("%s %i", format, TCE_AddInt32(cgs.currentRound, 1));
	defender = atoi(Info_ValueForKey(CG_ConfigString(CS_MULTI_INFO), "defender"));
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_AXIS)
	{
		first = "You are on the Terrorist team";
		if (cgs.currentRound == 1)
		{
			first = "You have been switched to the Terrorist team";
			second = defender ? "Try to beat the clock!" : "Keep the Specops from beating the clock!";
		}
	}
	else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_ALLIES)
	{
		first = "You are on the Specops team";
		if (cgs.currentRound == 1)
		{
			first = "You have been switched to the Specops team";
			second = defender ? "Keep the Axis from beating the clock!" : "Try to beat the clock!";
		}
	}
	if (*first)
	{
		first = CG_TranslateString(first);
	}
	if (*second)
	{
		second = CG_TranslateString(second);
	}
	/* The original draws all three stopwatch lines at this same height. */
	CG_WarmupLineLinux(roundText, 120);
	CG_WarmupLineLinux(first, 120);
	CG_WarmupLineLinux(second, 120);
done:
	tce_uiCoordinates = previous;
#else
	extern qboolean  tce_uiCoordinates;
	tce_warmup_t     state;
	tce_warmup_api_t api;
	qboolean         previous = tce_uiCoordinates;
	state.warmup      = cg.warmup; state.time = cg.time; state.gameState = cgs.gamestate;
	state.minClients  = cgs.minclients; state.demo = cg.demoPlayback;
	state.team        = cg.snap->ps.persistant[PERS_TEAM]; state.pmFlags = cg.snap->ps.pm_flags;
	state.gameType    = cgs.gametype; state.round = cgs.currentRound;
	state.defender    = cgs.gametype == 3?atoi(Info_ValueForKey(CG_ConfigString(CS_MULTI_INFO), "defender")):0;
	state.count       = cg.warmupCount;
	api.translate     = CG_WarmupTranslate; api.binding = CG_WarmupBinding;
	api.width         = CG_WarmupWidth; api.paint = CG_WarmupPaint; api.sound = trap_S_StartLocalSound;
	api.font          = &cgs.media.limboFont1; api.beep = trap_S_RegisterSound("sound/feedback/count_beep.wav", qfalse);
	tce_uiCoordinates = qtrue; TCE_DrawWarmup(&state, &api); tce_uiCoordinates = previous;
	cg.warmupCount    = state.count;
#endif
}

//==================================================================================

/*
=================
CG_DrawFlashFade
=================
*/
/* Complete Windows30027540 RGB consumer, fed by TC CG_Corona. */
static void CG_DrawCoronaBlend(void)
{
	extern qboolean tce_uiCoordinates;
	vec4_t          color; qboolean prior = tce_uiCoordinates;
	if (!cg.snap || cg.renderingThirdPerson || cg.tceCoronaBlendAlpha == 0)
	{
		return;
	}
	VectorCopy(cg.tceCoronaBlendColor, color); color[3] = cg.tceCoronaBlendAlpha;
	if (color[3] < 0)
	{
		color[3] = 0;
	}
	else if (color[3] > 1)
	{
		color[3] = 1;
	}
	tce_uiCoordinates = qtrue; CG_FillRect(-5, -85, 862, 650, color); tce_uiCoordinates = prior;
}

static void CG_DrawFlashFade(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean        previousCoordinates;
	static int      lastTime;
	int             elapsed, time;
	vec4_t          col;
	qboolean        fBlackout = (!CG_IsSinglePlayer() && int_ui_blackout.integer > 0);

	if (cgs.fadeStartTime + cgs.fadeDuration < cg.time)
	{
		cgs.fadeAlphaCurrent = cgs.fadeAlpha;
	}
	else if (cgs.fadeAlphaCurrent != cgs.fadeAlpha)
	{
		elapsed  = (time = trap_Milliseconds()) - lastTime; // we need to use trap_Milliseconds() here since the cg.time gets modified upon reloading
		lastTime = time;
		if (elapsed < 500 && elapsed > 0)
		{
			if (cgs.fadeAlphaCurrent > cgs.fadeAlpha)
			{
				cgs.fadeAlphaCurrent -= ((float)elapsed / (float)cgs.fadeDuration);
				if (cgs.fadeAlphaCurrent < cgs.fadeAlpha)
				{
					cgs.fadeAlphaCurrent = cgs.fadeAlpha;
				}
			}
			else
			{
				cgs.fadeAlphaCurrent += ((float)elapsed / (float)cgs.fadeDuration);
				if (cgs.fadeAlphaCurrent > cgs.fadeAlpha)
				{
					cgs.fadeAlphaCurrent = cgs.fadeAlpha;
				}
			}
		}
	}

	// OSP - ugh, have to inform the ui that we need to remain blacked out (or not)
	if (int_ui_blackout.integer == 0)
	{
		if (cg.mvTotalClients < 1 && cg.snap->ps.powerups[PW_BLACKOUT] > 0)
		{
			trap_Cvar_Set("ui_blackout", va("%d", cg.snap->ps.powerups[PW_BLACKOUT]));
		}
	}
	else if (cg.snap->ps.powerups[PW_BLACKOUT] == 0 || cg.mvTotalClients > 0)
	{
		trap_Cvar_Set("ui_blackout", "0");
	}

	// now draw the fade
	if (cgs.fadeAlphaCurrent > 0.0 || fBlackout)
	{
		VectorClear(col);
		col[3] = (fBlackout) ? 1.0f : cgs.fadeAlphaCurrent;
//		CG_FillRect( -10, -10, 650, 490, col );
		/* TC Windows300271d0: the blackout covers the full virtual canvas. */
		previousCoordinates = tce_uiCoordinates;
		tce_uiCoordinates   = qtrue;
		CG_FillRect(0, -80, 852, 640, col);


		// OSP - Show who is speclocked
		if (fBlackout)
		{
			int   i, nOffset = 90;
			char  *str, *format = "The %s team is speclocked!";
			char  *teams[TEAM_NUM_TEAMS] = { "??", "TERRORISTS", "SPECOPS", "???" };
			float color[4]               = { 1, 1, 0, 1 };

			for (i = TEAM_AXIS; i <= TEAM_ALLIES; i++)
			{
				if (cg.snap->ps.powerups[PW_BLACKOUT] & i)
				{
					str = va(format, teams[i]);
					CG_DrawStringExt(INFOTEXT_STARTX, nOffset, str, color, qtrue, qfalse, 10, 10, 0);
					nOffset += 12;
				}
			}
		}
		tce_uiCoordinates = previousCoordinates;
	}
}



/*
==============
CG_DrawFlashZoomTransition
	hide the snap transition from regular view to/from zoomed

  FIXME: TODO: use cg_fade?
==============
*/
static void CG_DrawFlashZoomTransition(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean        previous;
	vec4_t          color = { 0, 0, 0, 0 };
	int             elapsed;
	if (!cg.snap)
	{
		return;
	}
	if (BG_PlayerMounted(cg.snap->ps.eFlags))
	{
		cg.zoomTime = cg.time; return;
	}
	if (cg.weaponSelect < 0 || cg.weaponSelect >= TCE_MAX_WEAPONS ||
	    weaponDef[cg.weaponSelect].scoped == 0.0f || cg.renderingThirdPerson)
	{
		return;
	}
	elapsed = cg.time - cg.zoomTime;
	if ((float)elapsed < 400.0f)
	{
		color[3] = 1.0f - (float)elapsed * 0.0025f;
		previous = tce_uiCoordinates; tce_uiCoordinates = qtrue;
		CG_FillRect(-5, -85, 862, 650, color);
		tce_uiCoordinates = previous;
	}
}


/*
=================
CG_DrawFlashDamage
=================
*/
static void CG_DrawFlashDamage(void)
{
	extern qboolean tce_uiCoordinates;
	vec4_t          col = { 0.2f, 0, 0, 0 };
	double          redFlash, blood;
	qboolean        previous;
	if (!cg.snap || !(cg.v_dmg_time > cg.time))
	{
		return;
	}
	/* Windows30027989 keeps the lifetime product extended until final alpha. */
	redFlash = fabs(((double)cg.v_dmg_time - cg.time) * (double)0.002f * cg.v_dmg_pitch);
	if (redFlash > 5.0)
	{
		redFlash = 5.0;
	}
	blood = cg_bloodFlash.value > 1.0 ? 1.0 :
	        cg_bloodFlash.value < 0.0 ? 0.0 : cg_bloodFlash.value;
	col[3]            = (float)(((redFlash * 0.2) * blood) * 0.7);
	previous          = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	CG_FillRect(-5, -85, 862, 650, col);
	tce_uiCoordinates = previous;
}

/*
=================
CG_DrawFlashFire
=================
*/
static void CG_DrawFlashFire(void)
{
	extern qboolean tce_uiCoordinates;
	vec4_t          col = { 1, 1, 1, 1 };
#if defined(__linux__)
	long double     alpha, f;
	float           max;
#else
	float           alpha;
	double          max, f;
#endif
	qboolean        previous;

	if (!cg.snap)
	{
		return;
	}

	if (cg.renderingThirdPerson)
	{
		return;
	}

	if (!cg.snap->ps.onFireStart)
	{
		cg.v_noFireTime = cg.time;
		return;
	}

#if defined(__linux__)
	alpha = (long double)(cg.snap->ps.onFireStart - cg.time + 1000) * 0.001f;
#else
	alpha = (float)(((double)(cg.snap->ps.onFireStart - cg.time + 1000)) * (double)0.001f);
#endif
	if (alpha > 0)
	{
		if (alpha >= 1.0)
		{
			alpha = 1.0;
		}

		// fade in?
#if defined(__linux__)
		f = (long double)(cg.time - cg.v_noFireTime) * 0.001f;
#else
		f = (double)(cg.time - cg.v_noFireTime) * (double)0.001f;
#endif
		if (f >= 0.0 && f < 1.0)
		{
			alpha = f;
		}

#if defined(__linux__)
		max = (float)(0.5L + sinl((long double)((cg.time / 10) % 1000) * (double)0.001) * 0.5L);
#else
		max = (sin((double)((cg.time / 10) % 1000) * 0.001) + 1.0) * 0.5;
#endif
		if (alpha > max)
		{
			alpha = max;
		}
#if defined(__linux__)
		/* Color submission spills the selected alpha before sound-volume use. */
		alpha = (float)alpha;
#endif
		col[0] = alpha;
		col[1] = alpha;
		col[2] = alpha;
		col[3] = alpha;
		trap_R_SetColor(col);
		previous          = tce_uiCoordinates;
		tce_uiCoordinates = qtrue;
		CG_DrawPic(-5, -5, 862, 490, cgs.media.viewFlashFire[(cg.time / 50) % 16]);
		tce_uiCoordinates = previous;
		trap_R_SetColor(NULL);

		trap_S_AddLoopingSound(cg.snap->ps.origin, vec3_origin, cgs.media.flameSound, (int)(255.0 * alpha), 0);
		trap_S_AddLoopingSound(cg.snap->ps.origin, vec3_origin, cgs.media.flameCrackSound, (int)(255.0 * alpha), 0);
	}
	else
	{
		cg.v_noFireTime = cg.time;
	}
}



/*
==============
CG_DrawFlashBlendBehindHUD
	screen flash stuff drawn first (on top of world, behind HUD)
==============
*/
static void CG_DrawFlashBlendBehindHUD(void)
{
	CG_DrawCoronaBlend();
	CG_DrawFlashZoomTransition();
	CG_DrawFlashFade();
}


/*
=================
CG_DrawFlashBlend
	screen flash stuff drawn last (on top of everything)
=================
*/
static void CG_DrawFlashBlend(void)
{
	// Gordon: no flash blends if in limbo or spectator, and in the limbo menu
	if ((cg.snap->ps.pm_flags & PMF_LIMBO || cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) && cg.showGameView)
	{
		TCE_CG_DrawFlashBang();
		return;
	}

	CG_DrawFlashFire();
	CG_DrawFlashDamage();
	TCE_CG_DrawFlashBang();
}

// NERVE - SMF
/*
=================
CG_DrawObjectiveInfo
=================
*/
#define OID_LEFT    10
#define OID_TOP     360

void CG_ObjectivePrint(const char *str, int charWidth)
{
	char     *s;
	int      i, len;                    // NERVE - SMF
	qboolean neednewline = qfalse;      // NERVE - SMF

	if (cg.centerPrintTime)
	{
		return;
	}

	s = CG_TranslateString(str);

	Q_strncpyz(cg.oidPrint, s, sizeof(cg.oidPrint));

	// NERVE - SMF - turn spaces into newlines, if we've run over the linewidth
	len = Q_strlenInt(cg.oidPrint);
	for ( i = 0; i < len; i++ )
	{

		// NOTE: subtract a few chars here so long words still get displayed properly
		if (i % (CP_LINEWIDTH - 20) == 0 && i > 0)
		{
			neednewline = qtrue;
		}
		if (cg.oidPrint[i] == ' ' && neednewline)
		{
			cg.oidPrint[i] = '\n';
			neednewline    = qfalse;
		}
	}
	// -NERVE - SMF

	cg.oidPrintTime      = cg.time;
	cg.oidPrintY         = OID_TOP;
	cg.oidPrintCharWidth = charWidth;

	// count the number of lines for oiding
	cg.oidPrintLines = 1;
	s                = cg.oidPrint;
	while (*s)
	{
		if (*s == '\n')
		{
			cg.oidPrintLines++;
		}
		s++;
	}
}

static void CG_DrawObjectiveInfo(void)
{
#if defined(__linux__) && !defined(Q3_VM)
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	char firstLine[1024], line[1024];
	const char *start;
	float *color;
	int left = 425, right = 427, y, n, width, extent;
	if (!cg.oidPrintTime)
	{
		return;
	}
	previous = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	color = CG_FadeColor(cg.oidPrintTime, 250);
	if (!color)
	{
		cg.oidPrintTime = 0;
		goto done;
	}
	trap_R_SetColor(color);
	start = cg.oidPrint;
	for (;;)
	{
		for (n = 0; n < 56 && start[n] && start[n] != '\n'; ++n)
		{
			firstLine[n] = start[n];
		}
		firstLine[n] = 0;
		extent = (CG_DrawStrlen(firstLine) * cg.oidPrintCharWidth + 10) / 2;
		if (426 - extent < left)
		{
			left = 426 - extent;
			right = 426 + extent;
		}
		while (*start && *start != '\n')
		{
			++start;
		}
		if (!*start)
		{
			break;
		}
		++start;
	}
	right += 4;
	start = cg.oidPrint;
	y = 410 - cg.oidPrintLines * 8;
	for (;;)
	{
		for (n = 0; n < 56 && start[n] && start[n] != '\n'; ++n)
		{
			line[n] = start[n];
		}
		line[n] = 0;
		extent = left + cg.oidPrintCharWidth * CG_DrawStrlen(line);
		if (extent > right)
		{
			right = extent;
		}
		width = CG_Text_Width_Ext(line, .2f, 0, &cgs.media.limboFont1);
		CG_Text_Paint_Ext((float)(426 - width / 2), (float)y, .2f, .2f,
		                   color, line, 0, 0, 3, &cgs.media.limboFont1);
		y = TCE_TruncateNativeInteger((long double)cg.oidPrintCharWidth * 1.5f + y);
		while (*start && *start != '\n')
		{
			++start;
		}
		if (!*start)
		{
			break;
		}
		++start;
	}
	trap_R_SetColor(NULL);
 done:
	tce_uiCoordinates = previous;
#else
	extern qboolean    tce_uiCoordinates; qboolean previous = tce_uiCoordinates;
	tce_hud_messages_t s; tce_hud_messages_api_t a; CG_MessageState(&s, &a);
	tce_uiCoordinates = qtrue; TCE_DrawObjectiveInfo(&s, &a); tce_uiCoordinates = previous;
	cg.oidPrintTime   = s.objectiveTime;
#endif
}

void CG_DrawTimedMenus()
{
	if (cg.voiceTime)
	{
		int t = cg.time - cg.voiceTime;
		if (t > 2500)
		{
			Menus_CloseByName("voiceMenu");
			trap_Cvar_Set("cl_conXOffset", "0");
			cg.voiceTime = 0;
		}
	}
}

/*
=================
CG_Fade
=================
*/
void CG_Fade(int r, int g, int b, int a, int time, int duration)
{
	/* TC Windows30021560/Linux000494c4 only updates the alpha scheme.
	 * RGB parameters belong to the API but are unused by the original body. */
	cgs.fadeAlpha     = (float)a * (1.0f / 255.0f);
	cgs.fadeStartTime = time;
	cgs.fadeDuration  = duration;

	if (cgs.fadeStartTime + cgs.fadeDuration <= cg.time)
	{
		cgs.fadeAlphaCurrent = cgs.fadeAlpha;
	}
}

/*
=================
CG_ScreenFade
=================
*/
static void CG_ScreenFade(void)
{
	int    msec;
	int    i;
	float  t, invt;
	vec4_t color;

	if (!cg.fadeRate)
	{
		return;
	}

	msec = cg.fadeTime - cg.time;
	if (msec <= 0)
	{
		cg.fadeColor1[0] = cg.fadeColor2[0];
		cg.fadeColor1[1] = cg.fadeColor2[1];
		cg.fadeColor1[2] = cg.fadeColor2[2];
		cg.fadeColor1[3] = cg.fadeColor2[3];

		if (!cg.fadeColor1[3])
		{
			cg.fadeRate = 0;
			return;
		}

		CG_FillRect(0, 0, 640, 480, cg.fadeColor1);

	}
	else
	{
		t    = ( float )msec * cg.fadeRate;
		invt = 1.0f - t;

		for ( i = 0; i < 4; i++ )
		{
			color[i] = cg.fadeColor1[i] * t + cg.fadeColor2[i] * invt;
		}

		if (color[3])
		{
			CG_FillRect(0, 0, 640, 480, color);
		}
	}
}

#if 0 // rain - unused
// JPW NERVE
void CG_Draw2D2(void)
{
	qhandle_t weapon;

	trap_R_SetColor(NULL);

	CG_DrawPic(0, 480, 640, -70, cgs.media.hud1Shader);

	if (!BG_PlayerMounted(cg.snap->ps.eFlags))
	{
		switch (cg.snap->ps.weapon)
		{
		case WP_COLT:
		case WP_LUGER:
			weapon = cgs.media.hud2Shader;
			break;
		case WP_KNIFE:
			weapon = cgs.media.hud5Shader;
			break;
		default:
			weapon = cgs.media.hud3Shader;
		}
		CG_DrawPic(220, 410, 200, -200, weapon);
	}
}
#endif

/*
=================
CG_DrawCompassIcon

NERVE - SMF
=================
*/
void CG_DrawCompassIcon(float x, float y, float w, float h, vec3_t origin, vec3_t dest, qhandle_t shader)
{
	float  angle, pi2 = M_PI * 2;
	vec3_t v1, angles;
	float  len;

	VectorCopy(dest, v1);
	VectorSubtract(origin, v1, v1);
	len = VectorLength(v1);
	VectorNormalize(v1);
	vectoangles(v1, angles);

	if (v1[0] == 0 && v1[1] == 0 && v1[2] == 0)
	{
		return;
	}

//	if( cg_drawCompass.integer == 2 )
//		angles[YAW] = AngleSubtract( 90, angles[YAW] );
//	else
	angles[YAW] = AngleSubtract(cg.predictedPlayerState.viewangles[YAW], angles[YAW]);

	angle = ((angles[YAW] + 180.f) / 360.f - (0.50 / 2.f)) * pi2;


//	if (!CG_IsSinglePlayer()) {
	w /= 2;
	h /= 2;

	x += w;
	y += h;


//		if (CG_IsSinglePlayer())
/*		if (0)
		{
			w = 80; // hardcoded, because it has to fit the art
		}
		else*/
	{
		w = sqrt((w * w) + (h * h)) / 3.f * 2.f * 0.9f;
	}

	x = x + (cos(angle) * w);
	y = y + (sin(angle) * w);

	len = 1 - min(1.f, len / 2000.f);


	CG_DrawPic(x - (14 * len + 4) / 2, y - (14 * len + 4) / 2, 14 * len + 8, 14 * len + 8, shader);
#ifdef SQUARE_COMPASS
}
else
{
	int iconWidth, iconHeight;
	// START Mad Doc - TDF
	// talk about fitting a square peg into a round hole...
	// we're now putting the compass icons around the square automap instead of the round compass

	while (angle < 0)
		angle += pi2;

	while (angle >= pi2)
		angle -= pi2;


	x  = x + w / 2;
	y  = y + h / 2;
	w /= 2;    // = sqrt( ( w * w ) + ( h * h ) ) / 3.f * 2.f * 0.9f;

	if ((angle >= 0) && (angle < M_PI / 4.0))
	{
		x += w;
		y += w * tan(angle);

	}
	else if ((angle >= M_PI / 4.0) && (angle < 3.0 * M_PI / 4.0))
	{
		x += w / tan(angle);
		y += w;
	}
	else if ((angle >= 3.0 * M_PI / 4.0) && (angle < 5.0 * M_PI / 4.0))
	{
		x -= w;
		y -= w * tan(angle);
	}
	else if ((angle >= 5.0 * M_PI / 4.0) && (angle < 7.0 * M_PI / 4.0))
	{
		x -= w / tan(angle);
		y -= w;
	}
	else
	{
		x += w;
		y += w * tan(angle);

	}

	len        = 1 - min(1.f, len / 2000.f);
	iconWidth  = 14 * len + 4;    // where did this calc. come from?
	iconHeight = 14 * len + 4;

	// adjust so that icon is always outside of the map
	if ((angle > 5.0 * M_PI / 4.0) && (angle < 2 * M_PI))
	{

		y -= iconHeight;
	}

	if ((angle >= 3.0 * M_PI / 4.0) && (angle <= 5.0 * M_PI / 4.0))
	{
		x -= iconWidth;
	}


	CG_DrawPic(x, y, iconWidth, iconHeight, shader);


	// END Mad Doc - TDF

}
#endif
}

/*
=================
CG_DrawRadar
=================
*/
/* TC radar base; objective/team marker projection remains to be ported. */
/* Originals Windows300215a0 / Linux000496fc; geometry/render fidelity remains open.
 * origin-target is intentional. */
static void CG_DrawRadarIcon(int x, int y, int w, int h, const vec3_t origin, const vec3_t target,
                             qhandle_t shader, qhandle_t above, qhandle_t below)
{
	vec3_t delta, angles;
	float distance, z;
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	long double radians;
#else
	double radians;
#endif
	double fraction, size;
	int radius, px, py;
	VectorSubtract(origin, target, delta); z = delta[2];
	distance                                 = (float)sqrt((double)delta[0] * delta[0] + (double)delta[1] * delta[1] + (double)delta[2] * delta[2]);
	VectorNormalize(delta); vectoangles(delta, angles);
	if (delta[0] == 0 && delta[1] == 0 && delta[2] == 0)
	{
		return;
	}
	/* Original x87 retains these intermediates until the draw arguments. */
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	radians = ((TCE_AngleSubtractValue(cg.predictedPlayerState.viewangles[YAW], angles[YAW]) +
	            180.0L) * (long double)0.0027777778450399637f - 0.25L) *
	          (long double)6.2831854820251465;
#if defined(__linux__)
	{
		volatile float storedRadians = (float)radians;
		radians = (long double)storedRadians;
	}
#endif
#else
	radians = (((double)AngleSubtract(cg.predictedPlayerState.viewangles[YAW], angles[YAW]) + 180.f) * .0027777778450399637f - .25) * 6.2831854820251465;
#endif
	fraction = (double)distance * .0005f; if (fraction > 1)
	{
		fraction = 1;
	}
	radius = (int)((w / 2) * fraction);
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	px = TCE_TruncateNativeInteger((long double)(x + w / 2) + cosl(radians) * (long double)radius);
	py = TCE_TruncateNativeInteger((long double)(y + h / 2) + sinl(radians) * (long double)radius);
#else
	px = (int)(x + w / 2 + cos(radians) * radius); py = (int)(y + h / 2 + sin(radians) * radius);
#endif
	if (z < -72 && above)
	{
		shader = above;
	}
	else if (z > 72 && below)
	{
		shader = below;
	}
	size = ((1 - fraction) + 1) * 8;
	CG_DrawPic(px - size * .5f, py - size * .5f, size, size, shader);
}

/* CQB00053300: fixed-size, heading-aware contacts; sine range compression
 * retains detail near the player and clamps distant contacts to the rim. */
static void CG_DrawCompassContact(float y, const vec3_t target, float heading,
                                  qboolean enemy)
{
	vec3_t delta, angles;
	float distance, fraction, radians, px, py;
	qhandle_t shader;
	VectorSubtract(cg.predictedPlayerState.origin, target, delta);
	distance = VectorLength(delta);
	if (distance <= 0)
	{
		return;
	}
	vectoangles(delta, angles);
	fraction = distance * .0005f;
	if (fraction > 1)
	{
		fraction = 1;
	}
	fraction = (float)sin(fraction * M_PI * .5);
	radians  = (AngleSubtract(cg.predictedPlayerState.viewangles[YAW], angles[YAW]) + 180.f) / 360.f - .25f;
	radians *= 2.f * M_PI;
	px       = 792.f + (float)cos(radians) * 30.f * fraction;
	py       = y + (float)sin(radians) * 30.f * fraction;
	shader   = trap_R_RegisterShaderNoMip(enemy?"gfx/misc/radar_dot.tga":"gfx/misc/radar_arrow.tga");
	CG_DrawRotatedPic(px - 4, py - 4, 8, 8, shader,
	                  AngleSubtract(cg.predictedPlayerState.viewangles[YAW], heading) / 360.f + .125f);
}

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
static void CG_UpdateRadarHeading(float viewYaw, float *lastAngle, float *angleSpeed)
{
	long double phase = ((long double)viewYaw + 180.0L) *
	                    (long double)0.0027777778450399637f - 0.125L;
	volatile float previous = (float)((long double)*lastAngle * 360.0L);
	volatile float current  = (float)(phase * 360.0L);
	long double movement    = TCE_AngleSubtractValue(current, previous) *
	                          (long double)0.0027777778450399637f;
	long double damped = (long double)*angleSpeed * (long double)0.9259259104728699f;
	long double change = movement * (long double)0.009999999776482582f;
	*angleSpeed = (float)(damped + change);
	/* Native carry tests also clear an unordered absolute speed. */
	if (!(Q_fabs(*angleSpeed) >= 0.000009999999747378752f))
	{
		*angleSpeed = 0.0f;
	}
	*lastAngle = (float)((long double)*angleSpeed + (long double)*lastAngle);
}
#endif

static void CG_DrawRadar(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean previous = tce_uiCoordinates;
	snapshot_t *snap  = (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport)?cg.nextSnap:cg.snap;
	char mode[16]; float y;
	static float lastangle, anglespeed;
	if (snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR || cg.mvTotalClients > 0)
	{
		return;
	}
	trap_Cvar_VariableStringBuffer("cg_aspectMode", mode, sizeof(mode));
	y                 = atoi(mode) == 1?23.33f:atoi(mode) == 2?-30.f:50.f;
	tce_uiCoordinates = qtrue; trap_R_SetColor(colorWhite);
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	CG_UpdateRadarHeading(cg.predictedPlayerState.viewangles[YAW], &lastangle, &anglespeed);
#else
	{
		float angle, diff;
		angle      = (cg.predictedPlayerState.viewangles[YAW] + 180.f) / 360.f - .125f;
		diff       = AngleSubtract(angle * 360, lastangle * 360) / 360.f;
		anglespeed = anglespeed * .9259259104728699f + diff * .01f;
		if (Q_fabs(anglespeed) < .00001f)
		{
			anglespeed = 0;
		}
		lastangle += anglespeed;
	}
#endif
	CG_DrawRotatedPic(752, y - 40, 80, 80, trap_R_RegisterShader("gfx/misc/radarcompass2.tga"), lastangle);
	CG_DrawPic(742, y - 50, 100, 100, trap_R_RegisterShader("gfx/misc/radarbg.tga"));
	trap_R_SetColor(NULL);
	{
		int i, kind, enemy, team = cg.snap->ps.persistant[PERS_TEAM];
		qhandle_t shader = 0, above = 0, below = 0;
		centity_t *cent;
		entityState_t *es;
		if (cg.tceRadarViewerTeam == team)
		{
			for (i = 0; i < 64; i++)
			{
				int group;
				if (i == snap->ps.clientNum)
				{
					continue;
				}
				for (group = 0; group < 2; group++)
				{
					if (!cg.tceRadarTimes[group][i] ||
					    cg.time - cg.tceRadarTimes[group][i] > (group == team - 1?cg.tceRadarTeamTimeout:750) ||
					    !cgs.clientinfo[i].infoValid || cgs.clientinfo[i].team != group + 1)
					{
						continue;
					}
					CG_DrawCompassContact(y, cg.tceRadarPositions[group][i],
					                      cg.tceRadarYaw[group][i], group != team - 1);
				}
			}
		}
		if (cg_gameType.integer == 2 || cg_gameType.integer == 5)
		{
			for (i = 0; i < snap->numEntities; i++)
			{
				cent = &cg_entities[snap->entities[i].number]; es = &cent->currentState;
				if (es->eType == 13)
				{
					kind = es->onFireStart;
					if (kind == -1)
					{
						break;
					}
					enemy = (es->teamNum == 0 || (es->teamNum == 1 && team == 2) || (es->teamNum == 2 && team == 1));
					if (kind >= 0 && kind <= 5)
					{
						if (kind == 0)
						{
							enemy = 0;
						}
						shader = cgs.media.tceRadarMarkers[enemy][0][kind];
						above  = cgs.media.tceRadarMarkers[enemy][1][kind];
						below  = cgs.media.tceRadarMarkers[enemy][2][kind];
					}
				}
				else if (es->eType == ET_ITEM && es->otherEntityNum2 == team &&
				         (es->otherEntityNum == 253 || es->otherEntityNum == 254) && es->effect3Time == es->otherEntityNum)
				{
					kind   = es->otherEntityNum - 247;
					shader = cgs.media.tceRadarMarkers[0][0][kind];
					above  = cgs.media.tceRadarMarkers[0][1][kind]; below = cgs.media.tceRadarMarkers[0][2][kind];
				}
				else
				{
					continue;
				}
				CG_DrawRadarIcon(762, (int)(y - 30), 60, 60, cg.predictedPlayerState.origin, cent->lerpOrigin, shader, above, below);
			}
		}
	}
	tce_uiCoordinates = previous;
}

static int CG_PlayerAmmoValue(int *ammo, int *clips, int *akimboammo)
{
	centity_t *cent;
	playerState_t *ps;
	int weap;
	qboolean skipammo = qfalse;

	*ammo = *clips = *akimboammo = -1;

	if (cg.snap->ps.clientNum == cg.clientNum)
	{
		cent = &cg.predictedPlayerEntity;
	}
	else
	{
		cent = &cg_entities[cg.snap->ps.clientNum];
	}
	ps = &cg.snap->ps;

	weap = cent->currentState.weapon;

	if (!weap)
	{
		return weap;
	}

	/* Both originals use TC weapon IDs even when gear loading failed. */
	switch (weap)
	{
	case 1: case 12: case 15: case 19: case 20: case 21: case 22: case 27: case 28:
		return weap;
	case 4: case 9: case 11: case 26: case 30: case 35: case 60: case 61: case 65: case 66:
		skipammo = qtrue;
	}

	if (cg.snap->ps.eFlags & EF_MG42_ACTIVE || cg.snap->ps.eFlags & EF_MOUNTEDTANK)
	{
		return WP_MOBILE_MG42;
	}

	// total ammo in clips
	*clips = cg.snap->ps.ammo[BG_FindAmmoForWeapon(weap)];
	if (weap > 0 && weap < TCE_MAX_WEAPONS &&
	    weaponDef[weap].maxclip > 1 && !weaponDef[weap].singleReload)
	{
		*clips /= weaponDef[weap].maxclip;
	}

	// current clip
	*ammo = ps->ammoclip[BG_FindClipForWeapon(weap)];

	if (BG_IsAkimboWeapon(weap))
	{
		*akimboammo = ps->ammoclip[BG_FindClipForWeapon(BG_AkimboSidearm(weap))];
	}
	else
	{
		*akimboammo = -1;
	}

	if (weap == 26)
	{
		if (!cgs.gameManager)
		{
			*ammo = 0;
		}
		else
		{
			if (cgs.clientinfo[ps->clientNum].team == TEAM_AXIS)
			{
				*ammo = cgs.gameManager->currentState.otherEntityNum;
			}
			else
			{
				*ammo = cgs.gameManager->currentState.otherEntityNum2;
			}
		}
	}
	else if (weap == 35 || weap == 60 || weap == 65)
	{
		*ammo = TCE_AddInt32(*ammo, *clips);
	}

	if (skipammo)
	{
		*clips = -1;
	}

	return weap;
}

#define HEAD_TURNTIME 10000
#define HEAD_TURNANGLE 20
#define HEAD_PITCHANGLE 2.5
static void CG_DrawPlayerStatusHead(void)
{
	hudHeadAnimNumber_t anim;
	rectDef_t headRect = { 44, 480 - 92, 62, 80 };
//	rectDef_t headHintRect =	{ 40, 480 - 22, 20, 20 };
	bg_character_t *character     = CG_CharacterForPlayerstate(&cg.snap->ps);
	bg_character_t *headcharacter = BG_GetCharacter(cgs.clientinfo[cg.snap->ps.clientNum].team, cgs.clientinfo[cg.snap->ps.clientNum].cls);

	qhandle_t painshader = 0;

	anim = cg.idleAnim;

	if (cg.weaponFireTime > 500)
	{
		anim = HD_ATTACK;
	}
	else if (cg.time - cg.lastFiredWeaponTime < 500)
	{
		anim = HD_ATTACK_END;
	}
	else if (cg.time - cg.painTime < (character->hudheadanimations[HD_PAIN].numFrames * character->hudheadanimations[HD_PAIN].frameLerp))
	{
		anim = HD_PAIN;
	}
	else if (cg.time > cg.nextIdleTime)
	{
		cg.nextIdleTime = cg.time + 7000 + rand() % 1000;
		if (cg.snap->ps.stats[STAT_HEALTH] < 40)
		{
			cg.idleAnim = (rand() % (HD_DAMAGED_IDLE3 - HD_DAMAGED_IDLE2 + 1)) + HD_DAMAGED_IDLE2;
		}
		else
		{
			cg.idleAnim = (rand() % (HD_IDLE8 - HD_IDLE2 + 1)) + HD_IDLE2;
		}

		cg.lastIdleTimeEnd = cg.time + character->hudheadanimations[cg.idleAnim].numFrames * character->hudheadanimations[cg.idleAnim].frameLerp;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] < 5)
	{
		painshader = cgs.media.hudDamagedStates[3];
	}
	else if (cg.snap->ps.stats[STAT_HEALTH] < 20)
	{
		painshader = cgs.media.hudDamagedStates[2];
	}
	else if (cg.snap->ps.stats[STAT_HEALTH] < 40)
	{
		painshader = cgs.media.hudDamagedStates[1];
	}
	else if (cg.snap->ps.stats[STAT_HEALTH] < 60)
	{
		painshader = cgs.media.hudDamagedStates[0];
	}

	if (cg.time > cg.lastIdleTimeEnd)
	{
		if (cg.snap->ps.stats[STAT_HEALTH] < 40)
		{
			cg.idleAnim = HD_DAMAGED_IDLE1;
		}
		else
		{
			cg.idleAnim = HD_IDLE1;
		}
	}


	CG_DrawPlayerHead(&headRect, character, headcharacter, 180, 0, cg.snap->ps.eFlags & EF_HEADSHOT ? qfalse : qtrue, anim, painshader, cgs.clientinfo[cg.snap->ps.clientNum].rank, qfalse);

//	CG_DrawKeyHint( &headHintRect, "openlimbomenu" );
}

static void CG_DrawPlayerHealthBar(rectDef_t *rect)
{
	vec4_t bgcolour = { 1.f, 1.f, 1.f, 0.3f };
	vec4_t colour;

	int flags = 1 | 4 | 16 | 64;
	float frac;

	CG_ColorForHealth(colour);
	colour[3] = 0.5f;

	if (cgs.clientinfo[cg.snap->ps.clientNum].cls == PC_MEDIC)
	{
		frac = cg.snap->ps.stats[STAT_HEALTH] / ((float) cg.snap->ps.stats[STAT_MAX_HEALTH] * 1.12f);
	}
	else
	{
		frac = cg.snap->ps.stats[STAT_HEALTH] / (float) cg.snap->ps.stats[STAT_MAX_HEALTH];
	}


	CG_FilledBar(rect->x, rect->y + (rect->h * 0.1f), rect->w, rect->h * 0.84f, colour, NULL, bgcolour, frac, flags);

	trap_R_SetColor(NULL);
	CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.hudSprintBar);
	CG_DrawPic(rect->x, rect->y + rect->h + 4, rect->w, rect->w, cgs.media.hudHealthIcon);
}

static void CG_DrawStaminaBar(rectDef_t *rect)
{
	vec4_t bgcolour  = { 1.f, 1.f, 1.f, 0.3f };
	vec4_t colour    = { 0.1f, 1.0f, 0.1f, 0.5f };
	vec4_t colourlow = { 1.0f, 0.1f, 0.1f, 0.5f };
	vec_t *color     = colour;
	int flags        = 1 | 4 | 16 | 64;
	float frac       = cg.pmext.sprintTime / (float)SPRINTTIME;

	if (cg.snap->ps.powerups[PW_ADRENALINE])
	{
		if (cg.snap->ps.pm_flags & PMF_FOLLOW)
		{
			Vector4Average(colour, colorWhite, sin(cg.time * .005f), colour);
		}
		else
		{
			float msec = cg.snap->ps.powerups[PW_ADRENALINE] - cg.time;

			if (msec < 0)
			{
				msec = 0;
			}
			else
			{
				Vector4Average(colour, colorWhite, .5f + sin(.2f * sqrt(msec) * 2 * M_PI) * .5f, colour);
			}
		}
	}
	else
	{
		if (frac < 0.25)
		{
			color = colourlow;
		}
	}

	CG_FilledBar(rect->x, rect->y + (rect->h * 0.1f), rect->w, rect->h * 0.84f, color, NULL, bgcolour, frac, flags);

	trap_R_SetColor(NULL);
	CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.hudSprintBar);
	CG_DrawPic(rect->x, rect->y + rect->h + 4, rect->w, rect->w, cgs.media.hudSprintIcon);
}

static void CG_DrawWeapRecharge(rectDef_t *rect)
{
	float barFrac, chargeTime;
	int weap, flags;
	qboolean fade = qfalse;

	vec4_t bgcolor = { 1.0f, 1.0f, 1.0f, 0.25f };
	vec4_t color;

	flags = 1 | 4 | 16;

	weap = cg.snap->ps.weapon;

//	if( !(cg.snap->ps.eFlags & EF_ZOOMING) ) {
//		if ( weap != WP_PANZERFAUST && weap != WP_DYNAMITE && weap != WP_MEDKIT && weap != WP_SMOKE_GRENADE && weap != WP_PLIERS && weap != WP_AMMO ) {
//			fade = qtrue;
//		}
//	}

	// Draw power bar
	if (cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_ENGINEER)
	{
		chargeTime = cg.engineerChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
	}
	else if (cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_MEDIC)
	{
		chargeTime = cg.medicChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
	}
	else if (cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_FIELDOPS)
	{
		chargeTime = cg.ltChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
	}
	else if (cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_COVERTOPS)
	{
		chargeTime = cg.covertopsChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
	}
	else
	{
		chargeTime = cg.soldierChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
	}

	barFrac = (float)(cg.time - cg.snap->ps.classWeaponTime) / chargeTime;
	if (barFrac > 1.0)
	{
		barFrac = 1.0;
	}

	color[0] = 1.0f;
	color[1] = color[2] = barFrac;
	color[3] = 0.25 + barFrac * 0.5;

	if (fade)
	{
		bgcolor[3] *= 0.4f;
		color[3]   *= 0.4;
	}

	CG_FilledBar(rect->x, rect->y + (rect->h * 0.1f), rect->w, rect->h * 0.84f, color, NULL, bgcolor, barFrac, flags);

	trap_R_SetColor(NULL);
	CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.hudSprintBar);
	CG_DrawPic(rect->x + (rect->w * 0.25f) - 1, rect->y + rect->h + 4, (rect->w * 0.5f) + 2, rect->w + 2, cgs.media.hudPowerIcon);
}

extern void CG_TCEDrawWeaponCycle(float bottom);
/* Complete carrier-indicator block of CG_DrawPlayerStatus30028530.
 * Carrier state is authoritative snapshot state, not a gametype guess. */
static void CG_TCEDrawCarrierStatus(const playerState_t *ps, float bottom)
{
	if (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x100)
	{
		CG_DrawPic(742, bottom - 42, 36, 36, trap_R_RegisterShader("sprites/bombcarrier.tga"));
	}
	if (ps->stats[9] & 0x100)
	{
		CG_DrawPic(737, bottom - 35, 36, 36, trap_R_RegisterShader("sprites/VIP.tga"));
	}
	if (ps->stats[9] & 0x400)
	{
		CG_DrawPic(737, bottom - 35, 36, 36, trap_R_RegisterShader("sprites/hostage.tga"));
	}
	if (ps->powerups[PW_REDFLAG] || ps->powerups[PW_BLUEFLAG])
	{
		CG_DrawPic(742, bottom - 42, 36, 36, trap_R_RegisterShader("sprites/backpackcarrier.tga"));
	}
}

static void CG_TCEDrawObjectiveProgress(const playerState_t *ps)
{
	float duration, fraction;
	if (!(ps->pm_flags & PMF_TCE_OBJECTIVE_ACTION) ||
	    (ps->serverCursorHint != 39 && ps->serverCursorHint != 10 && ps->serverCursorHint != 3))
	{
		return;
	}
	duration = (ps->stats[STAT_TCE_FLAGS] & 2)?2000.f:4000.f;
	if (ps->serverCursorHint == 39)
	{
		duration = (ps->stats[STAT_TCE_FLAGS] & 2)?6000.f:10000.f;
	}
	trap_R_SetColor(NULL);
	fraction = (duration - ps->weaponTime) / duration;
	if (fraction < 0)
	{
		fraction = 0;
	}
	if (fraction < 1)
	{
		CG_HorizontalPercentBar(376, 310, 100, 10, fraction);
	}
}

static void CG_DrawPlayerStatus(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean previous = tce_uiCoordinates;
	playerState_t *ps = &cg.snap->ps;
	float bottom, rect[4], x, y, frac;
	vec4_t fg = { .7f, .7f, .7f, .7f }, bg = { 0, 0, 0, .3f }, reload = { 1, 1, 1, 1 };
	char mode[16], buffer[32];
	int damage[3], shaders[4], ammo, reserve, akimbo, weap, maxclip, width, i, hasFiremode = 0;
	trap_Cvar_VariableStringBuffer("cg_aspectMode", mode, sizeof(mode));
	bottom            = atoi(mode) == 1?506.67f:atoi(mode) == 2?560.f:480.f;
	tce_uiCoordinates = qtrue;
	CG_TCEDrawWeaponCycle(bottom);
	y = bottom - 10;
	if (cg.time - cg.weaponSelectTime > 500)
	{
		weap    = CG_PlayerAmmoValue(&ammo, &reserve, &akimbo);
		maxclip = weaponDef[weap].maxclip;
		if (akimbo >= 0 || reserve >= 0)
		{
			Com_sprintf(buffer, sizeof(buffer), "/%i", reserve);
			width = CG_Text_Width_Ext(buffer, .25f, 0, &cgs.media.limboFont1);
			CG_Text_Paint_Ext(840 - width, y, .25f, .25f, colorWhite, buffer, 0, 0, 3, &cgs.media.limboFont1);
			for (i = 0; i < (akimbo >= 0?2:1); ++i)
			{
				x = 830 - width - 8 * i; frac = (float)(i?akimbo:ammo) / maxclip;
				CG_FilledBar(x, bottom - 19, 6, 9, fg, NULL, bg, frac, 0x55);
				CG_DrawPic(x, bottom - 19, 6, 9, cgs.media.tceAmmoFrame);
			}
			if (ammo == 0 && akimbo <= 0 && reserve > 0)
			{
				reload[3] = (sin((double)cg.time * .0062831854447722435f) + 1) * .5f;
				if (reload[3] > .7f)
				{
					CG_Text_Paint_Ext(840 - CG_Text_Width_Ext("Reload", .25f, 0, &cgs.media.limboFont1), y - 12, .25f, .25f, reload, "Reload", 0, 0, 3, &cgs.media.limboFont1);
				}
			}
		}
		else if (ammo >= 0)
		{
			Com_sprintf(buffer, sizeof(buffer), "%i", ammo);
			CG_Text_Paint_Ext(840 - CG_Text_Width_Ext(buffer, .25f, 0, &cgs.media.limboFont1), y, .25f, .25f, colorWhite, buffer, 0, 0, 3, &cgs.media.limboFont1);
		}
	}
	weap = cg.predictedPlayerState.weapon;
	if (cg.time - cg.weaponSelectTime > 500)
	{
		hasFiremode = BG_FiremodeWeapon(weap);
	}
	if (cg.time - cg.weaponSelectTime > 500 && hasFiremode)
	{
		const char *label; int shift, firemode = ps->persistant[10];
		if (weaponDef[weap].pump)
		{
			label = firemode > 0?"Semi":"Pump"; shift = firemode > 0?0:4;
		}
		else
		{
			label = firemode == 2?"Single":firemode == 1?"Burst":"Auto"; shift = firemode == 2?0:firemode == 1?2:4;
		}
		CG_Text_Paint_Ext(798 - CG_Text_Width_Ext(label, .15f, 0, &cgs.media.limboFont1), y - shift, .15f, .15f, colorWhite, label, 0, 0, 3, &cgs.media.limboFont1);
	}
	if (cg.time - cg.weaponSelectTime > 500 && !hasFiremode &&
	    (ps->stats[STAT_TCE_WEAPON_FLAGS] & 4) && (cg.predictedPlayerState.weapon == 30 ||
	                                               cg.predictedPlayerState.weapon == 9 || cg.predictedPlayerState.weapon == 4))
	{
		CG_Text_Paint_Ext(798 - CG_Text_Width_Ext("Short throw", .15f, 0, &cgs.media.limboFont1), y,
		                  .15f, .15f, colorWhite, "Short throw", 0, 0, 3, &cgs.media.limboFont1);
	}
	CG_TCEDrawCarrierStatus(ps, bottom);
	for (i = 0; i < 4; ++i)
		shaders[i] = cgs.media.tceHealthMan[i];
	for (i = 0; i < 3; ++i)
		damage[i] = ps->holdable[2 + i];
	rect[0] = 24; rect[1] = bottom - 54; rect[2] = 20; rect[3] = 48;
	TCE_DrawPlayerHealthMan(rect, damage, shaders, trap_R_SetColor, CG_DrawPic);
	rect[0] = 12; rect[1] = bottom - 57; rect[2] = 8; rect[3] = 37;
	TCE_DrawStaminaBar(rect, cg.pmext.sprintTime, cgs.media.hudSprintIcon,
	                   cgs.media.tceStaminaFrame, trap_R_SetColor, CG_DrawPic, CG_FilledBar);
	trap_R_SetColor(NULL);
	CG_DrawPic(47, bottom - 38, 32, 32, cgs.media.tceMoveType[
				   ps->eFlags & EF_PRONE?0:ps->pm_flags & PMF_DUCKED?1:2]);
	CG_TCEDrawObjectiveProgress(ps);
	tce_uiCoordinates = previous;
}

static void CG_DrawSkillBar(float x, float y, float w, float h, int skill)
{
	int i;
	float blockheight = (h - 4) / (float)(NUM_SKILL_LEVELS - 1);
	float draw_y;
	vec4_t colour;
	float x1, y1, w1, h1;

	draw_y = y + h - blockheight;
	for ( i = 0; i < NUM_SKILL_LEVELS - 1; i++ )
	{
		if (i >= skill)
		{
			Vector4Set(colour, 1.f, 1.f, 1.f, .15f);
		}
		else
		{
			Vector4Set(colour, 0.f, 0.f, 0.f, .4f);
		}

		CG_FillRect(x, draw_y, w, blockheight, colour);

		if (i < skill)
		{
			x1 = x;
			y1 = draw_y;
			w1 = w;
			h1 = blockheight;
			CG_AdjustFrom640(&x1, &y1, &w1, &h1);

			trap_R_DrawStretchPic(x1, y1, w1, h1, 0, 0, 1.f, 0.5f, cgs.media.limboStar_roll);
		}

		CG_DrawRect_FixedBorder(x, draw_y, w, blockheight, 1, colorBlack);
//		CG_DrawPic( x, draw_y, w, blockheight, cgs.media.hudBorderVert2 );
		draw_y -= (blockheight + 1);
	}
}

#define SKILL_ICON_SIZE     14

#define SKILLS_X 112
#define SKILLS_Y 20

#define SKILL_BAR_OFFSET    (2 * SKILL_BAR_X_INDENT)
#define SKILL_BAR_X_INDENT  0
#define SKILL_BAR_Y_INDENT  6

#define SKILL_BAR_WIDTH     (SKILL_ICON_SIZE - SKILL_BAR_OFFSET)
#define SKILL_BAR_X         (SKILL_BAR_OFFSET + SKILL_BAR_X_INDENT + SKILLS_X)
#define SKILL_BAR_X_SCALE   (SKILL_ICON_SIZE + 2)
#define SKILL_ICON_X        (SKILL_BAR_OFFSET + SKILLS_X)
#define SKILL_ICON_X_SCALE  (SKILL_ICON_SIZE + 2)
#define SKILL_BAR_Y         (SKILL_BAR_Y_INDENT - SKILL_BAR_OFFSET - SKILLS_Y)
#define SKILL_BAR_Y_SCALE   (SKILL_ICON_SIZE + 2)
#define SKILL_ICON_Y        (-(SKILL_ICON_SIZE + 2) - SKILL_BAR_OFFSET - SKILLS_Y)

skillType_t CG_ClassSkillForPosition(clientInfo_t *ci, int pos)
{
	switch (pos)
	{
	case 0:
		return BG_ClassSkillForClass(ci->cls);
	case 1:
		return SK_BATTLE_SENSE;
	case 2:
		return SK_LIGHT_WEAPONS;
	}

	return SK_BATTLE_SENSE;
}

static void CG_DrawPlayerStats(void)
{
	int value = 0;
	playerState_t *ps;
	clientInfo_t *ci;
	skillType_t skill;
	int i;
	const char *str;
	float w;
	vec_t *clr;

	str = va("%i", cg.snap->ps.stats[STAT_HEALTH]);
	w   = CG_Text_Width_Ext(str, 0.25f, 0, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(SKILLS_X - 28 - w, 480 - 4, 0.25f, 0.25f, colorWhite, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(SKILLS_X - 28 + 2, 480 - 4, 0.2f, 0.2f, colorWhite, "HP", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);

	if (cgs.gametype == GT_WOLF_LMS)
	{
		return;
	}

	ps = &cg.snap->ps;
	ci = &cgs.clientinfo[ps->clientNum];


	for ( i = 0; i < 3; i++ )
	{
		skill = CG_ClassSkillForPosition(ci, i);

		CG_DrawSkillBar(i * SKILL_BAR_X_SCALE + SKILL_BAR_X, 480 - (5 * SKILL_BAR_Y_SCALE) + SKILL_BAR_Y, SKILL_BAR_WIDTH, 4 * SKILL_ICON_SIZE, ci->skill[skill]);
		CG_DrawPic(i * SKILL_ICON_X_SCALE + SKILL_ICON_X, 480 + SKILL_ICON_Y, SKILL_ICON_SIZE, SKILL_ICON_SIZE, cgs.media.skillPics[skill]);
	}

	if (cg.time - cg.xpChangeTime < 1000)
	{
		clr = colorYellow;
	}
	else
	{
		clr = colorWhite;
	}


	str = va("%i", cg.snap->ps.stats[STAT_XP]);
	w   = CG_Text_Width_Ext(str, 0.25f, 0, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(SKILLS_X + 28 - w, 480 - 4, 0.25f, 0.25f, clr, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(SKILLS_X + 28 + 2, 480 - 4, 0.2f, 0.2f, clr, "XP", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);

	// draw treasure icon if we have the flag
	// rain - #274 - use the playerstate instead of the clientinfo
	if (ps->powerups[PW_REDFLAG] || ps->powerups[PW_BLUEFLAG])
	{
		trap_R_SetColor(NULL);
		CG_DrawPic(640 - 40, 480 - 140 - value, 36, 36, cgs.media.objectiveShader);
	}
	else if (ps->powerups[PW_OPS_DISGUISED])       // Disguised?
	{
		CG_DrawPic(640 - 40, 480 - 140 - value, 36, 36, ps->persistant[PERS_TEAM] == TEAM_AXIS ? cgs.media.alliedUniformShader : cgs.media.axisUniformShader);
	}
}

static char statsDebugStrings[6][512];
static int statsDebugTime[6];
static int statsDebugTextWidth[6];
static int statsDebugPos;

void CG_InitStatsDebug(void)
{
	memset(&statsDebugStrings, 0, sizeof(statsDebugStrings));
	memset(&statsDebugTime, 0, sizeof(statsDebugTime));
	statsDebugPos = -1;
}

void CG_StatsDebugAddText(const char *text)
{
	if (cg_debugSkills.integer)
	{
		statsDebugPos++;

		if (statsDebugPos >= 6)
		{
			statsDebugPos = 0;
		}

		Q_strncpyz(statsDebugStrings[statsDebugPos], text, 512);
		statsDebugTime[statsDebugPos]      = cg.time;
		statsDebugTextWidth[statsDebugPos] = CG_Text_Width_Ext(text, .15f, 0, &cgs.media.limboFont2);

		CG_Printf("%s\n", text);
	}
}

static void CG_DrawStatsDebug(void)
{
	int textWidth = 0;
	int i, x, y, w, h;

	if (!cg_debugSkills.integer)
	{
		return;
	}

	for ( i = 0; i < 6; i++ )
	{
		if (statsDebugTime[i] + 9000 > cg.time)
		{
			if (statsDebugTextWidth[i] > textWidth)
			{
				textWidth = statsDebugTextWidth[i];
			}
		}
	}

	w = textWidth + 6;
	h = 9;
	x = 640 - w;
	y = (480 - 5 * (12 + 2) + 6 - 4) - 6 - h;   // don't ask

	i = statsDebugPos;

	do
	{
		vec4_t colour;

		if (statsDebugTime[i] + 9000 <= cg.time)
		{
			break;
		}

		colour[0] = colour[1] = colour[2] = .5f;
		if (cg.time - statsDebugTime[i] > 5000)
		{
#if defined(_MSC_VER) && defined(_M_IX86)
			static const float fadeRate = 0.00025f, baseAlpha = .5f;
			int age = (int)((unsigned int)cg.time - (unsigned int)statsDebugTime[i] - 5000u);
			colour[3] = (float)((long double)baseAlpha - (long double)age * fadeRate * baseAlpha);
#else
			colour[3] = .5f - .5f * ((cg.time - statsDebugTime[i] - 5000) / 4000.f);
#endif
		}
		else
		{
			colour[3] = .5f ;
		}
		CG_FillRect(x, y, w, h, colour);

		colour[0] = colour[1] = colour[2] = 1.f;
		if (cg.time - statsDebugTime[i] > 5000)
		{
#if defined(_MSC_VER) && defined(_M_IX86)
			static const float fadeRate = 0.00025f, baseAlpha = 1.f;
			int age = (int)((unsigned int)cg.time - (unsigned int)statsDebugTime[i] - 5000u);
			colour[3] = (float)((long double)baseAlpha - (long double)age * fadeRate);
#else
			colour[3] = 1.f - ((cg.time - statsDebugTime[i] - 5000) / 4000.f);
#endif
		}
		else
		{
			colour[3] = 1.f ;
		}
		CG_Text_Paint_Ext((float)(637.0 - (double)statsDebugTextWidth[i]), y + h - 2, .15f, .15f, colour, statsDebugStrings[i], 0, 0, ITEM_TEXTSTYLE_NORMAL, &cgs.media.limboFont2);

		y -= h;

		i--;
		if (i < 0)
		{
			i = 6 - 1;
		}
	}
	while (i != statsDebugPos);
}

//bani
void CG_DrawDemoRecording(void)
{
	char status[1024];
	char demostatus[128];
	char wavestatus[128];
	/* Original30021b30 has a separate visibility toggle from the Y position. */
	if (!cg_recording_showstatusline.integer)
	{
		return;
	}

	if (!cl_demorecording.integer && !cl_waverecording.integer)
	{
		return;
	}

	if (!cg_recording_statusline.integer)
	{
		return;
	}

	if (cl_demorecording.integer)
	{
		Com_sprintf(demostatus, sizeof(demostatus), " demo %s: %ik ", cl_demofilename.string, cl_demooffset.integer / 1024);
	}
	else
	{
		strncpy(demostatus, "", sizeof(demostatus));
	}

	if (cl_waverecording.integer)
	{
		Com_sprintf(wavestatus, sizeof(demostatus), " audio %s: %ik ", cl_wavefilename.string, cl_waveoffset.integer / 1024);
	}
	else
	{
		strncpy(wavestatus, "", sizeof(wavestatus));
	}

	Com_sprintf(status, sizeof(status), "RECORDING%s%s", demostatus, wavestatus);

	CG_Text_Paint_Ext(5, cg_recording_statusline.integer, 0.2f, 0.2f, colorWhite, status, 0, 0, 0, &cgs.media.limboFont2);
}

/*
=================
CG_Draw2D
=================
*/
static void CG_DrawLevelshotFade(void)
{
#if defined(__linux__) && !defined(Q3_VM)
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	vec4_t black = { 0, 0, 0, 0 }, white = { 1, 1, 1, 0 };
	long double alpha;
	int elapsed;
	if (!cg.snap)
	{
		return;
	}
	elapsed = TCE_SubInt32(cg.time, cg.levelshotFadeStart);
	if (elapsed > 2000)
	{
		return;
	}
	alpha = 1.0L - (1.0L / 2000) * elapsed;
	black[3] = (float)alpha;
	if (alpha < 0.0L)
	{
		black[3] = 0.0f;
	}
	if (black[3] > 1.0f)
	{
		black[3] = 1.0f;
	}
	white[3] = black[3];
	previous = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	trap_R_SetColor(white);
	CG_DrawPic(0, 58, 854, 364, cgs.media.levelshotShader);
	trap_R_SetColor(NULL);
	CG_FillRect(-5, -85, 862, 143, black);
	CG_FillRect(-5, 422, 862, 143, black);
	tce_uiCoordinates = previous;
#else
	extern qboolean tce_uiCoordinates;
	qboolean previous = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	TCE_DrawLevelshotFade(cg.snap != NULL, cg.time, cg.levelshotFadeStart,
	                      cgs.media.levelshotShader, trap_R_SetColor, CG_DrawPic, CG_FillRect);
	tce_uiCoordinates = previous;
#endif
}

/* Complete TC300217d0 / Linux000499e4. The description is team objective0,
 * not the map briefing and not the SDK current-objective panel. */
extern vmCvar_t cg_aspectMode;
static void CG_DrawObjectiveDesc(void)
{
	extern qboolean tce_uiCoordinates;
	char text[1024];
	const char *description = NULL, *binding, *location;
	vec4_t color = { .75f, .75f, .75f, 1 };
	float y;
	int width;
	qboolean previous = tce_uiCoordinates;
	if ((cg.snap->ps.pm_flags & PMF_LIMBO) ||
	    cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR || cg.mvTotalClients > 0)
	{
		return;
	}
	y                 = cg_aspectMode.integer == 1 ? 84.33f : cg_aspectMode.integer == 2 ? 31.f : 111.f;
	tce_uiCoordinates = qtrue;
	if (cg.snap->ps.holdable[13] && (location = CG_ConfigString(CS_LOCATIONS + cg.snap->ps.holdable[13])) != NULL)
	{
		Com_sprintf(text, sizeof(text), "%s", location);
		width = CG_Text_Width_Ext(text, .19f, 0, &cgs.media.limboFont1);
		CG_Text_Paint_Ext(840 - width, y - 10, .19f, .19f, color, text, 0, 0, 3, &cgs.media.limboFont1);
	}
	if (cgs.gametype != 7 && cg.tceShowObjectiveDesc)
	{
		if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_AXIS)
		{
			description = cg.objDescription_Axis[0];
		}
		else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_ALLIES)
		{
			description = cg.objDescription_Allied[0];
		}
		if (!description || !description[0])
		{
			description = "No Objective Supplied";
		}
		binding = BindingFromName("toggleobjectivedesc");
		if (!Q_stricmp(binding, "(toggleobjectivedesc)"))
		{
			binding = "ESCAPE";
		}
		Com_sprintf(text, sizeof(text), "Press %s to hide", binding);
		width = CG_Text_Width_Ext(text, .19f, 0, &cgs.media.limboFont1);
		CG_Text_Paint_Ext(840 - width, y + 2, .19f, .19f, color, text, 0, 0, 3, &cgs.media.limboFont1);
		Q_strncpyz(text, description, sizeof(text));
		width = CG_Text_Width_Ext(text, .19f, 0, &cgs.media.limboFont1);
		CG_Text_Paint_Ext(840 - width, y + 12, .19f, .19f, color, text, 0, 0, 3, &cgs.media.limboFont1);
	}
	tce_uiCoordinates = previous;
}

/* Original Draw2D30022d1e..30022e5b. Phase is stats14 (ps+108),
 * not the independently maintained aim phase in stats13. */
static void CG_TCEPlayAdsBreath(void)
{
	float phase;
	int bank;
	if (!(cg.predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS] & 4) ||
	    weaponDef[cg.predictedPlayerState.weapon].scoped <= 0)
	{
		return;
	}
	phase = (float)((double)cg.predictedPlayerState.stats[14] * (double).001f);
	if (cg.tceAdsBreathTime + 2000 >= cg.time)
	{
		return;
	}
	if ((phase > .5 && phase < 1) || (phase > 5.5 && phase < 6))
	{
		bank = 0;
	}
	else if ((phase > 3 && phase < 3.5) || (phase > 8 && phase < 8.5))
	{
		bank = 2;
	}
	else
	{
		return;
	}
	cg.tceAdsBreathTime = cg.time;
	trap_S_StartSound(NULL, cg.snap->ps.clientNum, CHAN_LOCAL,
	                  cgs.media.tceBreath[bank + rand() % 2]);
}

/* TC Windows300277e0: aspect-mode matte, independent of cinematic letterbox. */
static void CG_TCEDrawAspectMatte(void)
{
	extern vmCvar_t cg_aspectMode;
	extern qboolean tce_uiCoordinates;
	vec4_t black = { 0, 0, 0, 1 };
	float height, border;
	int width = cgs.glconfig.vidWidth, screenHeight = cgs.glconfig.vidHeight;
	qboolean previous;
	if (!cg.snap)
	{
		return;
	}
	if (cg_aspectMode.integer == 2 &&
	    ((double)width * 3.0) / ((double)screenHeight * 4.0) >= 1.0)
	{
		return;
	}
	if (width * 9 == screenHeight * 16 ||
	    (cg_aspectMode.integer >= 1 && width * 10 == screenHeight * 16))
	{
		return;
	}
	height = cg_aspectMode.integer == 1 ? 533.3333f :
	         cg_aspectMode.integer == 2 ? 640.0f : 480.0f;
	border            = (640.0f - height) * .5f + 35.0f;
	previous          = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	CG_FillRect(-5, -115, 862, border, black);
	CG_FillRect(-5, (height + 480.0f) * .5f, 862, border, black);
	tce_uiCoordinates = previous;
}


static void CG_Draw2D(void)
{
	CG_ScreenFade();

	// Arnout: no 2d when in esc menu
	// FIXME: do allow for quickchat (bleh)
	// Gordon: Removing for now
/*	if( trap_Key_GetCatcher() & KEYCATCH_UI ) {
		return;
	}*/

	if (cg.snap->ps.pm_type == PM_INTERMISSION)
	{
		CG_DrawIntermission();
		return;
	}
	else
	{
		if (cgs.dbShowing)
		{
			CG_Debriefing_Shutdown();
		}
	}

	if (cg.editingSpeakers)
	{
		CG_SpeakerEditorDraw();
		return;
	}

	/* Original30022a40: this switch is effective only for developers. */
	if (!cg_draw2D.integer && developer.integer)
	{
		return;
	}

	if (!cg.cameraMode)
	{
		CG_DrawCrosshair();
		CG_DrawFlashBlendBehindHUD();
		CG_DrawTeamNames();

		if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
		{
			CG_DrawSpectator();
			CG_UpdateCrosshairTarget();

			// NERVE - SMF - we need to do this for spectators as well
			CG_DrawTeamInfo();
		}
		else
		{
			// don't draw any status if dead
			if (cg.snap->ps.stats[STAT_HEALTH] > 0 || (cg.snap->ps.pm_flags & PMF_FOLLOW))
			{

				CG_UpdateCrosshairTarget();

				CG_DrawNoShootIcon();

//				CG_DrawPickupItem();
			}

			CG_DrawTeamInfo();

			if (cg_drawStatus.integer)
			{
				Menu_PaintAll();
				CG_DrawTimedMenus();
			}
		}

		{
			extern qboolean tce_uiCoordinates;
			qboolean previous = tce_uiCoordinates;
			tce_uiCoordinates = qtrue; CG_DrawVote(); tce_uiCoordinates = previous;
		}

		CG_DrawLagometer();
	}

	// don't draw center string if scoreboard is up
	if (!CG_DrawScoreboard())
	{
		if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR)
		{
			rectDef_t rect;

			if (cg.snap->ps.stats[STAT_HEALTH] > 0)
			{

				CG_DrawPlayerStatus();

			}

			CG_DrawLivesLeft();

			// Cursor hint
			rect.w = rect.h = 48;
			rect.x = 402;
			rect.y = 260;
			{ extern qboolean tce_uiCoordinates; qboolean previous = tce_uiCoordinates;
			  tce_uiCoordinates = qtrue; CG_DrawCursorhint(&rect); tce_uiCoordinates = previous; }

			// Stability bar
			rect.x = 50;
			rect.y = 208;
			rect.w = 10;
			rect.h = 64;
			CG_DrawWeapStability(&rect);

			// Stats Debugging
			CG_DrawStatsDebug();
		}

		if (!cg_paused.integer)
		{
			CG_DrawUpperRight();
		}

		CG_DrawCenterString();
		CG_DrawPMItems();
		CG_DrawPMItemsBig();

		CG_DrawFollow();
		CG_DrawWarmup();

		CG_DrawNotify();

		if (cg_drawCompass.integer)
		{
			CG_DrawRadar();
		}

		CG_DrawObjectiveDesc();
		CG_DrawObjectiveInfo();

		CG_DrawSpectatorMessage();

		CG_DrawLimboMessage();
	}
	else
	{
		if (cgs.eventHandling != CGAME_EVENT_NONE)
		{
//			qboolean old = cg.showGameView;

//			cg.showGameView = qfalse;
			// draw cursor
			trap_R_SetColor(NULL);
			CG_DrawPic(cgDC.cursorx - 14, cgDC.cursory - 14, 32, 32, cgs.media.cursorIcon);
//			cg.showGameView = old;
		}
	}


	// Info overlays
	CG_DrawOverlays();

	// OSP - window updates
	CG_windowDraw();

	// Ridah, draw flash blends now
	CG_DrawFlashBlend();

	CG_DrawDemoRecording();
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		CG_DrawLevelshotFade();
	}
	CG_TCEDrawAspectMatte();
	/* Original Draw2D30022a40: injury heartbeat slows during its last six seconds. */
	if (!(cg.predictedPlayerState.pm_flags & PMF_FOLLOW) &&
	    !(cg.predictedPlayerState.pm_type & 2) &&
	    cg.tceHeartbeatUntil > cg.time && cg.snap->ps.stats[STAT_HEALTH] > 0 &&
	    cg.tceHeartbeatNext < cg.time)
	{
		int remaining = cg.tceHeartbeatUntil - cg.time;
		trap_S_StartSound(NULL, cg.snap->ps.clientNum, CHAN_LOCAL, cgs.media.tceHeartbeat);
		cg.tceHeartbeatNext = cg.time + 600;
		if (remaining < 6000)
		{
			cg.tceHeartbeatNext += (6000 - remaining) / 15;
		}
	}
	CG_TCEPlayAdsBreath();
	TCE_CG_UpdateFlashRinging();
}

// NERVE - SMF
/* TC30021c70/30021cd0: retain the Windows float constants and avoid
   rounding the shake displacement before adding it to the camera origin. */
void CG_StartShakeCamera(float p)
{
#if defined(__linux__) && !defined(Q3_VM)
	long double length, phase;

	cg.cameraShakeScale = p;
	length = (long double)p * p * 1000.0L;
	cg.cameraShakeLength = (float)length;
	/* Linux adds the unrounded duration before truncating the deadline. */
	cg.cameraShakeTime = TCE_TruncateNativeInteger((long double)cg.time + length);
	phase = (long double)(rand() & 0x7fff) * 3.0518509447574615e-05f - .5L;
	cg.cameraShakePhase = (float)((phase + phase) * (long double)3.141592653589793);
#else
	double phase;
	double length = (double)p * p * 1000.0;
	cg.cameraShakeScale  = p;
	cg.cameraShakeLength = (float)length;
#if defined(_MSC_VER) && defined(_M_IX86)
	cg.cameraShakeTime = TCE_IntFloatSumLow32(cg.time, cg.cameraShakeLength);
#else
	cg.cameraShakeTime   = (int)((double)cg.time + cg.cameraShakeLength);
#endif
	phase                = (double)(rand() & 0x7fff) * (double)3.0518509447574615e-05f - .5;
	cg.cameraShakePhase  = (float)((phase + phase) * 3.1415927410125732);
#endif
}

void CG_ShakeCamera(void)
{
#if defined(__linux__) && !defined(Q3_VM)
	long double fraction;
	volatile float displacement;

	if (cg.cameraShakeTime < cg.time)
	{
		cg.cameraShakeScale = 0;
		return;
	}
	fraction = (long double)TCE_SubInt32(cg.cameraShakeTime, cg.time) *
	           (1.0L / cg.cameraShakeLength);
	/* Each Linux contribution is stored as float before adding the old origin. */
	displacement = (float)(TCE_CameraShakeWave(fraction * (long double)21.991148575128552 + cg.cameraShakePhase, 0) *
	                       fraction * cg.cameraShakeScale * 4.0L);
	cg.refdef.vieworg[2] = (float)((long double)displacement + cg.refdef.vieworg[2]);
	displacement = (float)(TCE_CameraShakeWave(fraction * (long double)40.840704496667314 + cg.cameraShakePhase, 0) *
	                       fraction * cg.cameraShakeScale * 4.0L);
	cg.refdef.vieworg[1] = (float)((long double)displacement + cg.refdef.vieworg[1]);
	displacement = (float)(TCE_CameraShakeWave(fraction * (long double)53.40707511102649 + cg.cameraShakePhase, 1) *
	                       fraction * cg.cameraShakeScale * 4.0L);
	cg.refdef.vieworg[0] = (float)((long double)displacement + cg.refdef.vieworg[0]);
	AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);
#elif defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	static const float frequency[3] =
	{
		53.40707778930664f, 40.84070587158203f, 21.99114990234375f
	};
	long double fraction, wave, displacement;
	int axis;

	if (cg.cameraShakeTime < cg.time)
	{
		cg.cameraShakeScale = 0;
		return;
	}
	fraction = (long double)TCE_SubInt32(cg.cameraShakeTime, cg.time) /
	           cg.cameraShakeLength;
	for (axis = 2; axis >= 0; --axis)
	{
		wave = TCE_CameraShakeWave(fraction * frequency[axis] + cg.cameraShakePhase, axis == 0);
		displacement = wave * cg.cameraShakeScale;
		displacement = axis == 0 ? fraction * displacement : displacement * fraction;
		displacement *= 4.0;
		cg.refdef.vieworg[axis] = (float)(displacement + cg.refdef.vieworg[axis]);
	}
	AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);
#else
	double fraction, amplitude;
	if (cg.cameraShakeTime < cg.time)
	{
		cg.cameraShakeScale = 0;
		return;
	}
	fraction             = (double)(cg.cameraShakeTime - cg.time) / cg.cameraShakeLength;
	amplitude            = fraction * cg.cameraShakeScale * 4.0;
	cg.refdef.vieworg[2] = (float)(cg.refdef.vieworg[2] + amplitude *
	                               sin(fraction * 21.99114990234375f + cg.cameraShakePhase));
	cg.refdef.vieworg[1] = (float)(cg.refdef.vieworg[1] + amplitude *
	                               sin(fraction * 40.84070587158203f + cg.cameraShakePhase));
	cg.refdef.vieworg[0] = (float)(cg.refdef.vieworg[0] + amplitude *
	                               cos(fraction * 53.40707778930664f + cg.cameraShakePhase));
	AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);
#endif
}
// -NERVE - SMF

/* TC30048990: snap sprite bases to the map and sample original lightgrid. */
void CG_SetupEliteLighting(void)
{
	int i;
	for (i = 0; i < cg.numMiscGameModels; i++)
	{
		cg_gamemodel_t *m = &cgs.miscGameModels[i];
		if (m->billboard)
		{
			TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(), m->org, m->color);
		}
	}
	for (i = 0; i < cg.numMiscClientSprites; i++)
	{
		cg_clientsprite_t *s = &cgs.miscClientSprites[i];
		vec3_t end;
		trace_t trace;
		s->org[2]                       += 8;
		VectorCopy(s->org, end); end[2] -= 128;
		CG_Trace(&trace, s->org, NULL, NULL, end, -1, CONTENTS_SOLID);
		s->org[2] -= trace.fraction * 128 + s->bottom;
		TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(), s->org, s->color);
	}
}

#if defined(__linux__) && !defined(Q3_VM)
/* The original ELF stores the low byte of a truncating signed16 conversion.
 * Masked invalid conversions produce 0x8000, whose low byte is zero. */
static byte CG_TCELinuxSpriteByte(long double value)
{
	if (!(value >= -32768.0L && value < 32768.0L))
	{
		return 0;
	}
	return (byte)(int)value;
}
#endif

/* TC30022020: vertical sprites face the viewer around their world Z axis. */
static void CG_DrawMiscClientsprites(void)
{
	int i, j, k;
	for (i = 0; i < cg.numMiscClientSprites; i++)
	{
		cg_clientsprite_t *s = &cgs.miscClientSprites[i];
		vec3_t forward, right;
		polyVert_t verts[4];
#if defined(__linux__) && !defined(Q3_VM)
		long double distance, facing;
		float alpha = 1;
#else
		float distance, alpha = 1;
#endif
		VectorSubtract(s->org, cg.refdef_current->vieworg, forward);
		distance = VectorNormalize(forward);
#if defined(__linux__) && !defined(Q3_VM)
		facing = (long double)cg.refdef_current->viewaxis[0][1] * forward[1] +
		         (long double)cg.refdef_current->viewaxis[0][0] * forward[0];
		facing += (long double)cg.refdef_current->viewaxis[0][2] * forward[2];
		if (facing < .5L ||
#else
		if (DotProduct(forward, cg.refdef_current->viewaxis[0]) < .5f ||
#endif
		    distance > s->drawDistance || !s->shader)
		{
			continue;
		}
		if (s->drawDistance > s->fadeDistance && distance > s->fadeDistance)
		{
#if defined(__linux__) && !defined(Q3_VM)
			alpha = (float)(1.0L - (1.0L / TCE_SubInt32(s->drawDistance, s->fadeDistance)) *
			                       (distance - s->fadeDistance));
#else
			alpha = (float)(1.0 - ((double)distance - s->fadeDistance) / (s->drawDistance - s->fadeDistance));
#endif
		}
		forward[2] = 0; VectorNormalize(forward);
		VectorSet(right, forward[1], -forward[0], 0);
		for (j = 0; j < 4; j++)
		{
#if defined(__linux__) && !defined(Q3_VM)
			/* ELF4a8b7..4ab0e stores each complete corner, with no
			 * Windows-only reload of a rounded side product. */
			long double side = (j == 0 || j == 3) ? s->halfWidth : -(long double)s->halfWidth;
			verts[j].xyz[0] = (float)((long double)s->org[0] + side * right[0]);
			verts[j].xyz[1] = (float)((long double)s->org[1] + side * right[1]);
			verts[j].xyz[2] = (float)((long double)s->org[2] + ((j < 2) ? s->top : s->bottom));
#else
			/* Original3002217d..3002232b keeps the top-edge products in
			 * x87, but reloads stored X products and bottom-right Y. */
			double side = (j == 0 || j == 3)?s->halfWidth:-s->halfWidth;
			double dx = side * right[0], dy = side * right[1];
			verts[j].xyz[0] = (float)(s->org[0] + (j < 2?dx:(double)(float)dx));
			verts[j].xyz[1] = (float)(s->org[1] + (j == 3?(double)(float)dy:dy));
			verts[j].xyz[2] = s->org[2] + ((j < 2)?s->top:s->bottom);
#endif
			verts[j].st[0]  = (j == 0 || j == 3)?1:0; verts[j].st[1] = j < 2?0:1;
			for (k = 0; k < 3; k++)
#if defined(__linux__) && !defined(Q3_VM)
				verts[j].modulate[k] = CG_TCELinuxSpriteByte((long double)s->color[k] * 255.0L);
			verts[j].modulate[3] = CG_TCELinuxSpriteByte((long double)alpha * 255.0L);
#else
				verts[j].modulate[k] = (byte)(int)((double)s->color[k] * 255);
			verts[j].modulate[3] = (byte)(int)((double)alpha * 255);
#endif
		}
		trap_R_AddPolyToScene(s->shader, 4, verts);
	}
}

/* Complete TC30021da0, including the previously missing billboard branch.
   Like the original, the reused entity is cleared once, not once per model. */
void CG_DrawMiscGamemodels(void)
{
	int i, j;
	refEntity_t ent;
	memset(&ent, 0, sizeof(ent));
	ent.reType = RT_MODEL; ent.nonNormalizedAxes = qtrue; ent.renderfx = RF_NOSHADOW;
	for (i = 0; i < cg.numMiscGameModels; i++)
	{
		cg_gamemodel_t *model = &cgs.miscGameModels[i];
		if (model->radius && CG_CullPointAndRadius(model->org, model->radius))
		{
			continue;
		}
		if (!trap_R_inPVS(cg.refdef_current->vieworg, model->org))
		{
			continue;
		}
		if (!model->billboard)
		{
			AxisCopy(model->axes, ent.axis);
		}
		else
		{
			vec3_t forward, left;
#if defined(__linux__) && !defined(Q3_VM)
			long double distance;
#else
			float distance;
#endif
			VectorSubtract(model->org, cg.refdef_current->vieworg, forward);
			distance = VectorNormalize(forward);
			if (distance > model->drawDistance)
			{
				continue;
			}
			for (j = 0; j < 3; j++)
#if defined(__linux__) && !defined(Q3_VM)
				ent.shaderRGBA[j] = CG_TCELinuxSpriteByte((long double)model->color[j] * 255.0L);
#else
				ent.shaderRGBA[j] = (byte)(int)((double)model->color[j] * 255.0);
#endif
			if (model->drawDistance <= model->fadeDistance || distance <= model->fadeDistance)
			{
				ent.shaderRGBA[3] = 255;
			}
			else
			{
#if defined(__linux__) && !defined(Q3_VM)
				ent.shaderRGBA[3] = CG_TCELinuxSpriteByte(255.0L * (1.0L -
				    (1.0L / TCE_SubInt32(model->drawDistance, model->fadeDistance)) *
				    (distance - model->fadeDistance)));
#else
				ent.shaderRGBA[3] = (byte)(int)(255.0 * (1.0 -
				                                         ((double)distance - model->fadeDistance) / (model->drawDistance - model->fadeDistance)));
#endif
			}
			forward[2] = 0; VectorNormalize(forward);
			VectorSet(left, -forward[1], forward[0], 0);
			VectorScale(forward, model->billboardScale, ent.axis[0]);
			VectorScale(left, model->billboardScale, ent.axis[1]);
			VectorCopy(model->axes[2], ent.axis[2]);
		}
		VectorCopy(model->org, ent.origin);
		VectorCopy(model->org, ent.oldorigin);
		VectorCopy(model->org, ent.lightingOrigin);
		ent.hModel = model->model;
		trap_R_AddRefEntityToScene(&ent);
	}
}

/*
=====================
CG_DrawActive

Perform all drawing needed to completely fill the screen
=====================
*/
/* Original CG_DrawActive 30022370 portal-scoped second scene. The parent
 * still needs a complete differential comparison of both rendering passes. */
/* Windows keeps the low word of a signed64 conversion; Linux converts to int32. */
static int CG_TCEViewportInteger(float viewportValue)
{
#if defined(_WIN32)
	unsigned int viewportBits; int viewportResult;
	if (!((double)viewportValue >= -9223372036854775808.0 &&
	      (double)viewportValue < 9223372036854775808.0))
	{
		return 0;
	}
	viewportBits = (unsigned int)(long long)viewportValue;
	memcpy(&viewportResult, &viewportBits, sizeof(viewportResult));
	return viewportResult;
#else
	if (!((double)viewportValue >= -2147483648.0 &&
	      (double)viewportValue < 2147483648.0))
	{
		return (-2147483647 - 1);
	}
	return (int)viewportValue;
#endif
}

#if defined(__GNUC__) && defined(__i386__) && defined(__linux__)
/* Linux retains the scaled scope width for both window corners. */
static void CG_TCELinuxScopeSize(int width, float *size, float *x, float *y)
{
	long double scaledWidth = width != 0 ? (long double)width : 250.0L;
	long double halfWidth;

	scaledWidth *= (long double)1.2f;
	*size        = (float)scaledWidth;
	halfWidth    = scaledWidth * 0.5L;
	*x           = (float)(426.0L - halfWidth);
	*y           = (float)(240.0L - halfWidth);
}

/* Called after backing up the current view axis. */
static void CG_TCELinuxScopeAngles(vec3_t angles)
{
	long double pitch, yaw;

	memcpy(angles, cg.predictedPlayerState.viewangles, sizeof(vec3_t));
	pitch     = (long double)cg.predictedPlayerState.holdable[5] - 2000.0L;
	pitch    *= (long double)0.01f;
	yaw       = (long double)cg.predictedPlayerState.holdable[6] - 2000.0L;
	yaw      *= (long double)0.01f;
	angles[0] = (float)((long double)angles[0] + pitch);
	angles[1] = (float)((long double)angles[1] + yaw);
}

/* Second-scene projection adds the rounded displacement before the X corner. */
static void CG_TCELinuxScopeProject(const vec3_t forward, const vec3_t right,
                                    const vec3_t up, float *x, float *y)
{
	const float degrees = 0.0055555556900799274f;
	const double pi     = 3.141592653589793;
	float fov           = cg.refdef.fov_x;
	float dx, dy;
	double angle;
	long double rightDot, upDot, reciprocal;

	rightDot = (long double)forward[1] * right[1]
	           + (long double)forward[0] * right[0];
	rightDot  += (long double)forward[2] * right[2];
	angle      = (double)atan2l(rightDot, sqrtl(1.0L - rightDot * rightDot));
	reciprocal = 1.0L / (((long double)fov * degrees) * pi);
	dx         = (float)(((long double)angle * reciprocal) * 852.0L);

	upDot = (long double)forward[1] * up[1]
	        + (long double)forward[0] * up[0];
	upDot      = (long double)forward[2] * up[2] + upDot;
	angle      = (double)atan2l(upDot, sqrtl(1.0L - upDot * upDot));
	*x         = (float)((long double)dx + *x);
	reciprocal = 1.0L / (((long double)degrees * fov) * pi);
	dy         = (float)(((long double)angle * reciprocal) * 852.0L);
	*y         = (float)((long double)*y - dy);
}

#endif

static void CG_DrawTCEScopeScene(void)
{
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	refdef_t modelView;
	vec3_t angles, forward, right, up;
	float x, y, w, h, scopeSize, radians;
	float scopeSavedViewport[4];
	float scopeSavedFov[2];
	vec3_t scopeSavedAxis[3];
	int weapon = cg.predictedPlayerState.weapon;
	if (!cg_portalScopes.integer || cg.renderingThirdPerson || weapon < 0 ||
	    weapon >= TCE_MAX_WEAPONS)
	{
		return;
	}
	if (!(weaponDef[weapon].scoped > 1.0f))
	{
		return;
	}
	if (!cg.tceAimActive || !cg.tceAimComplete || cg.tceScopeBlocked ||
	    (cg.limboEndCinematicTime > cg.time && cg.showGameView))
	{
		return;
	}
#if defined(_MSC_VER) && defined(_M_IX86)
	{
		int width                 = tce_cg_weapons[weapon].portalScopeWidth;
		long double selectedWidth = width != 0 ? (long double)width : 250.0L;
		long double halfWidth, pitch, yaw;

		scopeSize = (float)(selectedWidth * (long double)1.2f);
		halfWidth = (long double)scopeSize * 0.5L;
		x         = (float)(426.0L - halfWidth);
		y         = (float)(240.0L - halfWidth);
		w         = h = scopeSize;
		memcpy(scopeSavedFov, &cg.refdef_current->fov_x, sizeof(scopeSavedFov));
		scopeSavedViewport[0] = (float)cg.refdef_current->x;
		scopeSavedViewport[1] = (float)cg.refdef_current->y;
		scopeSavedViewport[2] = (float)cg.refdef_current->width;
		scopeSavedViewport[3] = (float)cg.refdef_current->height;
		AxisCopy(cg.refdef_current->viewaxis, scopeSavedAxis);
		pitch     = (long double)cg.predictedPlayerState.holdable[5] - 2000.0L;
		pitch    *= (long double)0.01f;
		angles[0] = (float)(pitch + cg.predictedPlayerState.viewangles[0]);
		yaw       = (long double)cg.predictedPlayerState.holdable[6] - 2000.0L;
		yaw      *= (long double)0.01f;
		angles[1] = (float)(yaw + cg.predictedPlayerState.viewangles[1]);
		memcpy(&angles[2], &cg.predictedPlayerState.viewangles[2], sizeof(float));
	}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
	CG_TCELinuxScopeSize(tce_cg_weapons[weapon].portalScopeWidth, &scopeSize, &x, &y);
	w = h = scopeSize;
#else
	scopeSize = (float)tce_cg_weapons[weapon].portalScopeWidth;
	if (scopeSize == 0)
	{
		scopeSize = 250;
	}
	scopeSize *= 1.2f;
	/* Original stores the window corner before adding free-aim displacement;
	 * rounding a center first is not equivalent for non-dyadic scope widths. */
	x = 426.0f - scopeSize * 0.5f; y = 240.0f - scopeSize * 0.5f; w = h = scopeSize;
	VectorCopy(cg.predictedPlayerState.viewangles, angles);
	angles[0] += ((float)cg.predictedPlayerState.holdable[5] - 2000.0f) * 0.01f;
	angles[1] += ((float)cg.predictedPlayerState.holdable[6] - 2000.0f) * 0.01f;
#endif
#if !defined(_MSC_VER) || !defined(_M_IX86)
	{
		int viewport[4];
		viewport[0]           = cg.refdef_current->x; viewport[1] = cg.refdef_current->y;
		viewport[2]           = cg.refdef_current->width; viewport[3] = cg.refdef_current->height;
		scopeSavedViewport[0] = (float)viewport[0];
		scopeSavedViewport[1] = (float)viewport[1];
		scopeSavedViewport[2] = (float)viewport[2];
		scopeSavedViewport[3] = (float)viewport[3];
		memcpy(&scopeSavedFov[0], &cg.refdef_current->fov_x, sizeof(float));
		memcpy(&scopeSavedFov[1], &cg.refdef_current->fov_y, sizeof(float));
		AxisCopy(cg.refdef_current->viewaxis, scopeSavedAxis);
	}
#endif
#if defined(__GNUC__) && defined(__i386__) && defined(__linux__)
	CG_TCELinuxScopeAngles(angles);
#endif
	AngleVectors(angles, forward, NULL, NULL);
	/* Both TC originals project against predicted view angles, not the
	 * camera-shake/bob-adjusted refdefViewAngles. */
	AngleVectors(cg.predictedPlayerState.viewangles, NULL, right, up);
#if defined(_MSC_VER) && defined(_M_IX86)
	{
		long double dot, displacement;
		double angle;

		radians = (float)((long double)cg.refdef.fov_x * 0.01745329238474369f);
		dot     = (long double)right[2] * forward[2]
		          + (long double)right[1] * forward[1];
		dot         += (long double)right[0] * forward[0];
		angle        = asin((double)dot);
		displacement = ((long double)angle / radians) * 852.0L;
		x            = (float)(displacement + x);

		dot = (long double)up[2] * forward[2]
		      + (long double)up[1] * forward[1];
		dot         += (long double)up[0] * forward[0];
		angle        = asin((double)dot);
		displacement = ((long double)angle / radians) * 852.0L;
		y            = (float)((long double)y - displacement);
	}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
	CG_TCELinuxScopeProject(forward, right, up, &x, &y);
#else
	radians = cg.refdef.fov_x * 0.01745329238474369f;
	x       = (float)((asin(DotProduct(forward, right)) / radians) * 852.0 + x);
	y       = (float)(y - (asin(DotProduct(forward, up)) / radians) * 852.0);
#endif
	AnglesToAxis(angles, cg.refdef_current->viewaxis);
	previous                                            = tce_uiCoordinates; tce_uiCoordinates = qtrue;
	CG_AdjustFrom640(&x, &y, &w, &h); tce_uiCoordinates = previous;
	cg.refdef_current->x                                = CG_TCEViewportInteger(x); cg.refdef_current->y = CG_TCEViewportInteger(y);
	cg.refdef_current->width                            = CG_TCEViewportInteger(w); cg.refdef_current->height = CG_TCEViewportInteger(h);
#if defined(_MSC_VER) && defined(_M_IX86)
	cg.refdef_current->fov_x = (float)(((long double)scopeSize *
	                                    (long double)0.10563380271196365f) / weaponDef[weapon].scoped);
	memcpy(&cg.refdef_current->fov_y, &cg.refdef_current->fov_x, sizeof(float));
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
	{
		float product          = (float)((long double)scopeSize * 0.10563380271196365f);
		long double reciprocal = 1.0L / weaponDef[weapon].scoped;

		cg.refdef_current->fov_x = (float)(reciprocal * product);
		memcpy(&cg.refdef_current->fov_y, &cg.refdef_current->fov_x, sizeof(float));
	}
#else
	cg.refdef_current->fov_x = (scopeSize * 0.10563380271196365f) / weaponDef[weapon].scoped;
	cg.refdef_current->fov_y = cg.refdef_current->fov_x;
#endif
	cg.tcePortalScopeRendering = qtrue;
	CG_SetupFrustum(); CG_DrawSkyBoxPortal(qtrue);
	if (!cg.hyperspace)
	{
		CG_AddPacketEntities(); CG_AddMarks(); CG_AddParticles(); CG_AddLocalEntities();
		CG_AddSmokeSprites(); CG_AddAtmosphericEffects(); CG_AddFlameChunks(); CG_AddTrails();
	}
	CG_PB_RenderPolyBuffers(); CG_DrawMiscGamemodels(); CG_DrawMiscClientsprites();
	CG_DrawScreenFade();
	trap_R_RenderScene(cg.refdef_current);
	cg.refdef_current->x      = CG_TCEViewportInteger(scopeSavedViewport[0]);
	cg.refdef_current->y      = CG_TCEViewportInteger(scopeSavedViewport[1]);
	cg.refdef_current->width  = CG_TCEViewportInteger(scopeSavedViewport[2]);
	cg.refdef_current->height = CG_TCEViewportInteger(scopeSavedViewport[3]);
	memcpy(&cg.refdef_current->fov_x, &scopeSavedFov[0], sizeof(float));
	memcpy(&cg.refdef_current->fov_y, &scopeSavedFov[1], sizeof(float));
	AxisCopy(scopeSavedAxis, cg.refdef_current->viewaxis);
	cg.tcePortalScopeRendering = qfalse;
	if (cg.tcePortalScopeEntity.hModel)
	{
		memset(&modelView, 0, sizeof(modelView));
		CG_Printf("ELITE DEBUG: draw model\n");
		modelView.rdflags = RDF_NOWORLDMODEL;
		AxisCopy(cg.refdef_current->viewaxis, modelView.viewaxis);
		/* Original also calls AxisCopy at vieworg: its trailing six words
		 * rewrite the first two axis rows with the same source values. */
		{
			typedef char ScopeAdjacentViewVectors[
					offsetof(refdef_t, viewaxis) == offsetof(refdef_t, vieworg) + sizeof(vec3_t)?1:-1];
			AxisCopy((vec3_t *)((byte *)cg.refdef_current + offsetof(refdef_t, vieworg)),
			         (vec3_t *)((byte *)&modelView + offsetof(refdef_t, vieworg)));
		}
		memcpy(&modelView.fov_x, &cg.refdef_current->fov_x, sizeof(float));
		memcpy(&modelView.fov_y, &cg.refdef_current->fov_y, sizeof(float));
		modelView.x     = cg.refdef_current->x; modelView.y = cg.refdef_current->y;
		modelView.width = cg.refdef_current->width; modelView.height = cg.refdef_current->height;
		modelView.time  = cg.refdef_current->time;

		trap_R_ClearScene(); trap_R_AddRefEntityToScene(&cg.tcePortalScopeEntity);
		trap_R_RenderScene(&modelView);
	}
}

void CG_DrawActive(stereoFrame_t stereoView)
{
	float separation;
	vec3_t baseOrg;


	// optionally draw the info screen instead
	if (!cg.snap)
	{
		CG_DrawInformation(qfalse);
		return;
	}

	// optionally draw the tournement scoreboard instead
	/*if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
		( cg.snap->ps.pm_flags & PMF_SCOREBOARD ) ) {
		CG_DrawTourneyScoreboard();
		return;
	}*/

	switch (stereoView)
	{
	case STEREO_CENTER:
		separation = 0;
		break;
	case STEREO_LEFT:
		separation = -cg_stereoSeparation.value / 2;

		break;
	case STEREO_RIGHT:
		separation = cg_stereoSeparation.value / 2;

		break;
	default:
		separation = 0;
		CG_Error("CG_DrawActive: Undefined stereoView");
	}


	// clear around the rendered view if sized down
	CG_TileClear();

	// offset vieworg appropriately if we're doing stereo separation
	memcpy(baseOrg, cg.refdef_current->vieworg, sizeof(vec3_t));
	if (separation < 0.f || separation > 0.f)
	{
		long double offset = -(long double)separation;
		int axis;
		for (axis = 0; axis < 3; ++axis)
			cg.refdef_current->vieworg[axis] = (float)(
				cg.refdef_current->viewaxis[1][axis] * offset + cg.refdef_current->vieworg[axis]);
	}


	cg.refdef_current->glfog.registered = 0;    // make sure it doesn't use fog from another scene

	CG_ActivateLimboMenu();

//	if( cgs.ccCurrentCamObjective == -1 ) {
//		if( cg.showGameView ) {
//			CG_FillRect( 0, 0, 640, 480, colorBlack );
//			CG_LimboPanel_Draw();
//			return;
//		}
//	}

	if (cg.showGameView)
	{
		float x, y, w, h;
		extern qboolean tce_uiCoordinates;
		qboolean previousCoordinates = tce_uiCoordinates;
		x = 0;
		y = 0;
		w = 640;
		h = 480;

		/* Original 3002249b calls the TC transform, also outside scope mode. */
		tce_uiCoordinates = qtrue;
		CG_AdjustFrom640(&x, &y, &w, &h);
		tce_uiCoordinates = previousCoordinates;

		cg.refdef_current->x      = CG_TCEViewportInteger(x);
		cg.refdef_current->y      = CG_TCEViewportInteger(y);
		cg.refdef_current->width  = CG_TCEViewportInteger(w);
		cg.refdef_current->height = CG_TCEViewportInteger(h);

		CG_Letterbox(100, 100, qfalse);
	}

	CG_ShakeCamera();       // NERVE - SMF

	// Gordon
	CG_PB_RenderPolyBuffers();

	// Gordon
	CG_DrawMiscGamemodels();
	CG_DrawMiscClientsprites();

	if (!(cg.limboEndCinematicTime > cg.time && cg.showGameView))
	{
		trap_R_RenderScene(cg.refdef_current);
	}

	CG_DrawTCEScopeScene();

	// restore original viewpoint if running stereo
	/* Original C3 gate also skips unordered separation. Restore raw words. */
	if (separation < 0.f || separation > 0.f)
	{
		memcpy(cg.refdef_current->vieworg, baseOrg, sizeof(vec3_t));
	}


	if (!cg.showGameView)
	{
		// draw status bar and other floating elements
		CG_Draw2D();
#ifdef FEATURE_OMNIBOT
		OmnibotRenderDebugText();
#endif
	}
	else
	{
		CG_LimboPanel_Draw();
	}
	if (cg.tceShowLimboPanel)
	{
		CG_LimboPanel_Draw();
	}
}
