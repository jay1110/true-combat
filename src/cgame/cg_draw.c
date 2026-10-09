#include <stddef.h>
#include "tce_player_hud.h"
#include "tce_warmup.h"
#include "tce_centerprint.h"
#include "tce_hud_messages.h"
#include "../game/tce_bg.h"
// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay

#include "cg_local.h"
#include "tce_levelshot_fade.h"
#include "tce_weapon_media.h"
#include "tce_lightgrid.h"
#include "tce_flash.h"

#define STATUSBARHEIGHT 452
char* BindingFromName(const char *cvar);
void Controls_GetConfig( void );
void SetHeadOrigin(clientInfo_t *ci, playerInfo_t *pi);
void CG_DrawOverlays();
int activeFont;

#define TEAM_OVERLAY_TIME 1000

////////////////////////
////////////////////////
////// new hud stuff
///////////////////////
///////////////////////

void CG_Text_SetActiveFont( int font ) {
	activeFont = font;
}

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC x87 metric accumulator and scale stay resident through the glyph loop.
 * These compile-time contracts describe native fontInfo_t, not game globals. */
typedef char CG_Text_Width_Ext_layout[(sizeof(glyphInfo_t)==80 && offsetof(fontInfo_t,glyphScale)==0x5000 && offsetof(glyphInfo_t,xSkip)==16 && offsetof(glyphInfo_t,height)==0)?1:-1];
__declspec(naked) int CG_Text_Width_Ext(const char *text, float scale, int limit, fontInfo_t *font) {
    __asm {
        push ebx
        push ebp
        push esi
        push edi
        mov edx, dword ptr [esp+20]
        mov ebp, dword ptr [esp+32]
        fld dword ptr [esp+24]
        fmul dword ptr [ebp+5000h]
        fldz
        test edx, edx
        jz metric_done
        mov edi, edx
        or ecx, -1
        xor eax, eax
        repne scasb
        not ecx
        dec ecx
        mov eax, dword ptr [esp+28]
        test eax, eax
        jle metric_limit
        cmp ecx, eax
        jle metric_limit
        mov ecx, eax
    metric_limit:
        xor esi, esi
    metric_loop:
        mov al, byte ptr [edx]
        test al, al
        jz metric_done
        cmp esi, ecx
        jge metric_done
        cmp al, 5eh
        jne metric_glyph
        mov bl, byte ptr [edx+1]
        test bl, bl
        jz metric_glyph
        cmp bl, al
        je metric_glyph
        add edx, 2
        jmp metric_next
    metric_glyph:
        and eax, 0ffh
        lea eax, [eax+eax*4]
        shl eax, 4
        fiadd dword ptr [eax+ebp+16]
        inc edx
        inc esi
    metric_next:
        test edx, edx
        jnz metric_loop
    metric_done:
        fmul st(0), st(1)
        sub esp, 12
        fstcw word ptr [esp+8]
        fwait
        mov ax, word ptr [esp+8]
        or ax, 0c00h
        mov word ptr [esp+10], ax
        fldcw word ptr [esp+10]
        fistp qword ptr [esp]
        fldcw word ptr [esp+8]
        mov eax, dword ptr [esp]
        mov edx, dword ptr [esp+4]
        add esp, 12
        fstp st(0)
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret
    }
}
#else
int CG_Text_Width_Ext( const char *text, float scale, int limit, fontInfo_t* font ) {
	int count, len;
	glyphInfo_t *glyph;
	const char *s = text;
	float out, useScale = scale * font->glyphScale;
	
	out = 0;
	if( text ) {
		len = strlen( text );
		if (limit > 0 && len > limit) {
			len = limit;
		}
		count = 0;
		while (s && *s && count < len) {
			if ( Q_IsColorString(s) ) {
				s += 2;
				continue;
			} else {
				glyph = &font->glyphs[(unsigned char)*s];
				out += glyph->xSkip;
				s++;
				count++;
			}
		}
	}

	return out * useScale;
}

#endif

int CG_Text_Width( const char *text, float scale, int limit ) {
	fontInfo_t *font = &cgDC.Assets.fonts[activeFont];

	return CG_Text_Width_Ext( text, scale, limit, font );
}

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC x87 metric accumulator and scale stay resident through the glyph loop.
 * These compile-time contracts describe native fontInfo_t, not game globals. */
typedef char CG_Text_Height_Ext_layout[(sizeof(glyphInfo_t)==80 && offsetof(fontInfo_t,glyphScale)==0x5000 && offsetof(glyphInfo_t,xSkip)==16 && offsetof(glyphInfo_t,height)==0)?1:-1];
__declspec(naked) int CG_Text_Height_Ext(const char *text, float scale, int limit, fontInfo_t *font) {
    __asm {
        push ebx
        push ebp
        push esi
        push edi
        mov edx, dword ptr [esp+20]
        mov ebp, dword ptr [esp+32]
        fld dword ptr [esp+24]
        fmul dword ptr [ebp+5000h]
        fldz
        test edx, edx
        jz metric_done
        mov edi, edx
        or ecx, -1
        xor eax, eax
        repne scasb
        not ecx
        dec ecx
        mov eax, dword ptr [esp+28]
        test eax, eax
        jle metric_limit
        cmp ecx, eax
        jle metric_limit
        mov ecx, eax
    metric_limit:
        xor esi, esi
    metric_loop:
        mov al, byte ptr [edx]
        test al, al
        jz metric_done
        cmp esi, ecx
        jge metric_done
        cmp al, 5eh
        jne metric_glyph
        mov bl, byte ptr [edx+1]
        test bl, bl
        jz metric_glyph
        cmp bl, al
        je metric_glyph
        add edx, 2
        jmp metric_next
    metric_glyph:
        and eax, 0ffh
        lea eax, [eax+eax*4]
        shl eax, 4
        push eax
        fild dword ptr [eax+ebp]
        fstp dword ptr [esp]
        fcom dword ptr [esp]
        fnstsw ax
        test ah, 1
        jz metric_keep
        fstp st(0)
        fld dword ptr [esp]
    metric_keep:
        add esp, 4
        inc edx
        inc esi
    metric_next:
        test edx, edx
        jnz metric_loop
    metric_done:
        fmul st(0), st(1)
        sub esp, 12
        fstcw word ptr [esp+8]
        fwait
        mov ax, word ptr [esp+8]
        or ax, 0c00h
        mov word ptr [esp+10], ax
        fldcw word ptr [esp+10]
        fistp qword ptr [esp]
        fldcw word ptr [esp+8]
        mov eax, dword ptr [esp]
        mov edx, dword ptr [esp+4]
        add esp, 12
        fstp st(0)
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret
    }
}
#else
int CG_Text_Height_Ext( const char *text, float scale, int limit, fontInfo_t* font ) {
 int len, count;
	float max;
	glyphInfo_t *glyph;
	float useScale;
	const char *s = text;

	useScale = scale * font->glyphScale;
	max = 0;
	if (text) {
		len = strlen(text);
		if (limit > 0 && len > limit) {
			len = limit;
		}
		count = 0;
		while (s && *s && count < len) {
			if ( Q_IsColorString(s) ) {
				s += 2;
				continue;
			} else {
				glyph = &font->glyphs[(unsigned char)*s];
	      if (max < glyph->height) {
		      max = glyph->height;
			  }
				s++;
				count++;
			}
		}
	}
	return max * useScale;
}

#endif

int CG_Text_Height( const char *text, float scale, int limit ) {
	fontInfo_t *font = &cgDC.Assets.fonts[activeFont];

	return CG_Text_Height_Ext( text, scale, limit, font );
}

void CG_Text_PaintChar_Ext(float x, float y, float w, float h, float scalex, float scaley, float s, float t, float s2, float t2, qhandle_t hShader) {
	w *= scalex;
	h *= scaley;
	CG_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, h, s, t, s2, t2, hShader );
}

void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader) {
	float w, h;
	w = width * scale;
	h = height * scale;
	CG_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, h, s, t, s2, t2, hShader );
}

void CG_Text_Paint_Centred_Ext( float x, float y, float scalex, float scaley, vec4_t color, const char *text, float adjust, int limit, int style, fontInfo_t* font ) {
#if defined(_MSC_VER) && defined(_M_IX86)
	static const float half = 0.5f;
	int width = CG_Text_Width_Ext(text, scalex, limit, font);
	/* The integer metric stays exact until the final coordinate store. */
	__asm {
		fild dword ptr [width]
		fmul dword ptr [half]
		fsubr dword ptr [x]
		fstp dword ptr [x]
	}
#else
	x -= CG_Text_Width_Ext(text, scalex, limit, font) * 0.5f;
#endif

	CG_Text_Paint_Ext( x, y, scalex, scaley, color, text, adjust, limit, style, font );
}

void CG_Text_Paint_Ext( float x, float y, float scalex, float scaley, vec4_t color, const char *text, float adjust, int limit, int style, fontInfo_t* font ) {
	int len, count;
	vec4_t newColor;
	glyphInfo_t *glyph;

	scalex *= font->glyphScale;
	scaley *= font->glyphScale;

	if (text) {
		const char *s = text;
		trap_R_SetColor( color );
		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
		len = strlen(text);
		if (limit > 0 && len > limit) {
			len = limit;
		}
		count = 0;
		while (s && *s && count < len) {
			glyph = &font->glyphs[(unsigned char)*s];
			if ( Q_IsColorString( s ) ) {
				if( *(s+1) == COLOR_NULL ) {
					memcpy( newColor, color, sizeof(newColor) );
				} else {
					memcpy( newColor, g_color_table[ColorIndex(*(s+1))], sizeof( newColor ) );
					newColor[3] = color[3];
				}
				trap_R_SetColor( newColor );
				s += 2;
				continue;
			} else {
				float yadj = scaley * glyph->top;
				if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
					int ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
					colorBlack[3] = newColor[3];
					trap_R_SetColor( colorBlack );
					CG_Text_PaintChar_Ext(x + (glyph->pitch * scalex) + ofs, y - yadj + ofs, glyph->imageWidth, glyph->imageHeight, scalex, scaley, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->glyph);
					colorBlack[3] = 1.0;
					trap_R_SetColor( newColor );
				}
				CG_Text_PaintChar_Ext(x + (glyph->pitch * scalex), y - yadj, glyph->imageWidth, glyph->imageHeight, scalex, scaley, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->glyph);
				x += (glyph->xSkip * scalex) + adjust;
				s++;
				count++;
			}
		}
		trap_R_SetColor( NULL );
	}
}

void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style) {
	fontInfo_t *font = &cgDC.Assets.fonts[activeFont];

	CG_Text_Paint_Ext( x, y, scale, scale, color, text, adjust, limit, style, font );
}

// NERVE - SMF - added back in
int CG_DrawFieldWidth (int x, int y, int width, int value, int charWidth, int charHeight ) {
	char	num[16], *ptr;
	int		l;
	int		frame;
	int		totalwidth = 0;

	if ( width < 1 ) {
		return 0;
	}

	// draw number string
	if ( width > 5 ) {
		width = 5;
	}

	switch ( width ) {
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

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		totalwidth += charWidth;
		ptr++;
		l--;
	}

	return totalwidth;
}

int CG_DrawField (int x, int y, int width, int value, int charWidth, int charHeight, qboolean dodrawpic, qboolean leftAlign ) {
	char	num[16], *ptr;
	int		l;
	int		frame;
	int		startx;

	if ( width < 1 ) {
		return 0;
	}

	// draw number string
	if ( width > 5 ) {
		width = 5;
	}

	switch ( width ) {
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

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;

	// NERVE - SMF
	if ( !leftAlign ) {
		x -= 2 + charWidth*(l);
	}

	startx = x;

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		if ( dodrawpic )
			CG_DrawPic( x,y, charWidth, charHeight, cgs.media.numberShaders[frame] );
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
void CG_Draw3DModel( float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles ) {
	refdef_t		refdef;
	refEntity_t		ent;

	CG_AdjustFrom640( &x, &y, &w, &h );

	memset( &refdef, 0, sizeof( refdef ) );

	memset( &ent, 0, sizeof( ent ) );
	AnglesToAxis( angles, ent.axis );
	VectorCopy( origin, ent.origin );
	ent.hModel = model;
	ent.customSkin = skin;
	ent.renderfx = RF_NOSHADOW;		// no stencil shadows

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	refdef.fov_x = 30;
	refdef.fov_y = 30;

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	refdef.time = cg.time;

	trap_R_ClearScene();
	trap_R_AddRefEntityToScene( &ent );
	trap_R_RenderScene( &refdef );
}

/*
==============
CG_DrawKeyModel
==============
*/
void CG_DrawKeyModel( int keynum, float x, float y, float w, float h, int fadetime) {
	qhandle_t		cm;
	float			len;
	vec3_t			origin, angles;
	vec3_t			mins, maxs;

	VectorClear( angles );

	cm = cg_items[keynum].models[0];

	// offset the origin y and z to center the model
	trap_R_ModelBounds( cm, mins, maxs );

	origin[2] = -0.5 * ( mins[2] + maxs[2] );
	origin[1] = 0.5 * ( mins[1] + maxs[1] );

//	len = 0.5 * ( maxs[2] - mins[2] );		
	len = 0.75 * ( maxs[2] - mins[2] );		
	origin[0] = len / 0.268;	// len / tan( fov/2 )

	angles[YAW] = 30 * sin( cg.time / 2000.0 );;

	CG_Draw3DModel( x, y, w, h, cg_items[keynum].models[0], 0, origin, angles);
}

/*
================
CG_DrawTeamBackground

================
*/
void CG_DrawTeamBackground( int x, int y, int w, int h, float alpha, int team )
{
	vec4_t		hcolor;

	hcolor[3] = alpha;
	if ( team == TEAM_AXIS ) {
		hcolor[0] = 1;
		hcolor[1] = 0;
		hcolor[2] = 0;
	} else if ( team == TEAM_ALLIES ) {
		hcolor[0] = 0;
		hcolor[1] = 0;
		hcolor[2] = 1;
	} else {
		return;
	}
	trap_R_SetColor( hcolor );
	CG_DrawPic( x, y, w, h, cgs.media.teamStatusBar );
	trap_R_SetColor( NULL );
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
static float CG_DrawSnapshot( float y ) {
	char		*s;
	int			w;

	s = va( "time:%i snap:%i cmd:%i", cg.snap->serverTime, 
		cg.latestSnapshotNum, cgs.serverCommandSequence );
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

	CG_DrawBigString( 840 - w, y + 2, s, 1.0F);

	return y + BIGCHAR_HEIGHT + 4;
}

/*
==================
CG_DrawFPS
==================
*/
#define	FPS_FRAMES	4
static void CG_DrawFPSContents( float y ) {
	char		*s;
	int			w;
	static int	previousTimes[FPS_FRAMES];
	static int	index;
	int		i, total;
	int		fps;
	static	int	previous;
	int		t, frameTime;
	vec4_t		timerBackground =	{ 0.16f,	0.2f,	0.17f,	0.8f	};
	vec4_t		timerBorder     =	{ 0.5f,		0.5f,	0.5f,	0.5f	};
	vec4_t		tclr			=	{ 0.75f,	0.75f,	0.75f,	1.0f	};

	// don't use serverTime, because that will be drifting to
	// correct for internet lag changes, timescales, timedemos, etc
	t = trap_Milliseconds();
	frameTime = t - previous;
	previous = t;

	previousTimes[index % FPS_FRAMES] = frameTime;
	index++;
	if ( index > FPS_FRAMES ) {
		// average multiple frames together to smooth changes out a bit
		total = 0;
		for ( i = 0 ; i < FPS_FRAMES ; i++ ) {
			total += previousTimes[i];
		}
		if ( !total ) {
			total = 1;
		}
		fps = 1000 * FPS_FRAMES / total;

		s = va( "%i FPS", fps );
		w = CG_Text_Width_Ext( s, 0.19f, 0, &cgs.media.limboFont1 );


		CG_Text_Paint_Ext( 840 - w, y + 11, 0.19f, 0.19f, tclr, s, 0, 0, 3, &cgs.media.limboFont1 );
	}


}

#if defined(_MSC_VER) && defined(_M_IX86)
static const float cg_fpsLineAdvance = 16.f;
static __declspec(naked) float CG_DrawFPS(float y) {
    __asm {
        push dword ptr [esp+4]
        call CG_DrawFPSContents
        add esp, 4
        fld dword ptr [esp+4]
        fadd dword ptr [cg_fpsLineAdvance]
        ret
    }
}
#else
static float CG_DrawFPS(float y) {
    CG_DrawFPSContents(y);
    return y + 16.f;
}
#endif

/*
=================
CG_DrawTimer
=================
*/

static float CG_DrawTimer( float y ) {
	char		*s;
	int			w;
	int			mins, seconds, tens;
	int			msec;
	char		*rt;
	vec4_t		color =				{ 0.75f,	0.75f,	0.75f,	1.0f	};
	vec4_t		timerBackground =	{ 0.16f,	0.2f,	0.17f,	0.8f	};
	vec4_t		timerBorder     =	{ 0.5f,		0.5f,	0.5f,	0.5f	};

	rt = "";

	/* Original30023090 keeps the product/subtraction extended until __ftol. */
	msec = (int)((double)cgs.timelimit * 60000.f - (cg.time - cgs.levelStartTime));

	seconds = msec / 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	tens = seconds / 10;
	seconds -= tens * 10;

	if(cgs.gamestate != GS_PLAYING) {
		//%	s = va( "%s^*WARMUP", rt );
		s = "^*WARMUP";	// ydnar: don't draw reinforcement time in warmup mode
		color[3] = fabs(sin(cg.time * 0.002));
	} else if ( msec < 0 && cgs.timelimit > 0.0f) {
		s = va( "^N0:00" );
		color[3] = fabs(sin(cg.time * 0.002));
	} else {
		if(cgs.timelimit <= 0.0f) {
			s = va( "%s", rt);
		} else {
			s = va( "%s^*%i:%i%i", rt, mins, tens, seconds);
		}

		color[3] = 1.f;
	}

	w = CG_Text_Width_Ext( s, 0.19f, 0, &cgs.media.limboFont1 );


	CG_Text_Paint_Ext( 840 - w, y + 11, 0.19f, 0.19f, color, s, 0, 0, 3, &cgs.media.limboFont1 );

	return y + 12 + 4;
}

// START	xkan, 8/29/2002
int CG_BotIsSelected(int clientNum)
{
	int i;

	for (i=0; i<MAX_NUM_BUDDY; i++)
	{
		if (cg.selectedBotClientNumber[i] == 0)
			return 0;
		else if (cg.selectedBotClientNumber[i] == clientNum)
			return 1;
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

#define TEAM_OVERLAY_MAXNAME_WIDTH	16
#define TEAM_OVERLAY_MAXLOCATION_WIDTH	20

/*
=====================
CG_DrawUpperRight

=====================
*/
static void CG_DrawUpperRight( void ) {
	float	y;
    extern qboolean tce_uiCoordinates;
    qboolean previous;

	if( !cg_drawFireteamOverlay.integer ) {
		return;
	}

	y = 20 + 100 + 32;


	if( !( cg.snap->ps.pm_flags & PMF_LIMBO ) && ( cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) &&
		( cgs.autoMapExpanded || ( !cgs.autoMapExpanded && ( cg.time - cgs.autoMapExpandTime < 250.f ) ) ) )
		return;

    previous=tce_uiCoordinates;tce_uiCoordinates=qtrue;
	if ( cg_drawRoundTimer.integer ) {
		y = CG_DrawTimer( y );
	}

	if ( cg_drawFPS.integer ) {
		y = CG_DrawFPS( y );
	}

	if ( cg_drawSnapshot.integer ) {
		y = CG_DrawSnapshot( y );
	}
    tce_uiCoordinates=previous;
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
static void CG_DrawTeamInfo( void ) {
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	int w;
	int i, len;
	vec4_t		hcolor;
	int		chatHeight;
	float	alphapercent;
	float	lineHeight = 9.f;

	int chatWidth = 318;
 
	if( cg_teamChatHeight.integer < TEAMCHAT_HEIGHT ) {
		chatHeight = cg_teamChatHeight.integer;
	} else {
		chatHeight = TEAMCHAT_HEIGHT;
	}

	if( chatHeight <= 0 ) {
		return; // disabled
	}

	if( cgs.teamLastChatPos != cgs.teamChatPos ) {
		if( cg.time - cgs.teamChatMsgTimes[cgs.teamLastChatPos % chatHeight] > cg_teamChatTime.integer ) {
			cgs.teamLastChatPos++;
		}

		previous = tce_uiCoordinates;
		tce_uiCoordinates = qtrue;

		w = 0;

		for( i = cgs.teamLastChatPos; i < cgs.teamChatPos; i++ ) {
			len = CG_Text_Width_Ext( cgs.teamChatMsgs[i % chatHeight], 0.2f, 0, &cgs.media.limboFont2 );
			if( len > w ) {
				w = len;
			}
		}
		w *= TINYCHAR_WIDTH;
		w += TINYCHAR_WIDTH * 2;

		for( i = cgs.teamChatPos - 1; i >= cgs.teamLastChatPos; i-- ) {
			alphapercent = 1.0f - (cg.time - cgs.teamChatMsgTimes[i % chatHeight]) / (float)(cg_teamChatTime.integer);
			if( alphapercent > 1.0f ) {
				alphapercent = 1.0f;
			} else if( alphapercent < 0.f ) {
				alphapercent = 0.f;
			}

			hcolor[0] = hcolor[1] = hcolor[2] = 0;
			hcolor[3] = 0.6f * alphapercent;

			trap_R_SetColor( hcolor );
			CG_DrawPic( 2, 365 - (cgs.teamChatPos - i)*lineHeight, chatWidth, lineHeight, cgs.media.teamStatusBar );

			hcolor[0] = hcolor[1] = hcolor[2] = 1.0;
			hcolor[3] = alphapercent;
			trap_R_SetColor( hcolor );

			CG_Text_Paint_Ext( 4, 365 - (cgs.teamChatPos - i - 1) * lineHeight - 1, 0.2f, 0.2f, hcolor, cgs.teamChatMsgs[i % chatHeight], 0, 0, 0, &cgs.media.limboFont2 );
		}
		tce_uiCoordinates = previous;
	}
}

const char* CG_PickupItemText( int item ) {
	if( bg_itemlist[ item ].giType == IT_HEALTH ) {
		if(bg_itemlist[ item ].world_model[2])	{	// this is a multi-stage item
			// FIXME: print the correct amount for multi-stage
			return va( "a %s", bg_itemlist[ item ].pickup_name );
		} else {
			/* TC30020ff0: the item name already supplies the pickup wording. */
			return va( "%s", bg_itemlist[ item ].pickup_name );
		}
	} else if( bg_itemlist[ item ].giType == IT_TEAM ) {
		return "an Objective";
	} else {
		if( bg_itemlist[ item ].pickup_name[0] == 'a' ||  bg_itemlist[ item ].pickup_name[0] == 'A' ) {
			return va( "an %s", bg_itemlist[ item ].pickup_name );
		} else {
			return va( "a %s", bg_itemlist[ item ].pickup_name );
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

static void CG_DrawNotify( void ) {
	int w, h;
	int i, len;
	vec4_t		hcolor;
	int		chatHeight;
	float	alphapercent;
	char	var[MAX_TOKEN_CHARS];
	float	notifytime = 1.0f;
	int		yLoc;

	return;

	yLoc = NOTIFYLOC_Y;

	trap_Cvar_VariableStringBuffer( "con_notifytime", var, sizeof( var ) );
	notifytime = atof( var ) * 1000;

	if ( notifytime <= 100.f )
		notifytime = 100.0f;

	chatHeight = NOTIFY_HEIGHT;

	if (cgs.notifyLastPos != cgs.notifyPos) {
		if (cg.time - cgs.notifyMsgTimes[cgs.notifyLastPos % chatHeight] > notifytime) {
			cgs.notifyLastPos++;
		}

		h = (cgs.notifyPos - cgs.notifyLastPos) * TINYCHAR_HEIGHT;

		w = 0;

		for (i = cgs.notifyLastPos; i < cgs.notifyPos; i++) {
			len = CG_DrawStrlen(cgs.notifyMsgs[i % chatHeight]);
			if (len > w)
				w = len;
		}
		w *= TINYCHAR_WIDTH;
		w += TINYCHAR_WIDTH * 2;

		if ( maxCharsBeforeOverlay <= 0 )
			maxCharsBeforeOverlay = 80;

		for (i = cgs.notifyPos - 1; i >= cgs.notifyLastPos; i--) {
			alphapercent = 1.0f - ((cg.time - cgs.notifyMsgTimes[i % chatHeight]) / notifytime);
			if (alphapercent > 0.5f)
				alphapercent = 1.0f;
			else 
				alphapercent *= 2;
			
			if (alphapercent < 0.f)
				alphapercent = 0.f;

			hcolor[0] = hcolor[1] = hcolor[2] = 1.0;
			hcolor[3] = alphapercent;
			trap_R_SetColor( hcolor );

			CG_DrawStringExt( NOTIFYLOC_X + TINYCHAR_WIDTH, 
				yLoc - (cgs.notifyPos - i)*TINYCHAR_HEIGHT, 
				cgs.notifyMsgs[i % chatHeight], hcolor, qfalse, qfalse,
				TINYCHAR_WIDTH, TINYCHAR_HEIGHT, maxCharsBeforeOverlay );
		}
	}
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define	LAG_SAMPLES		128


typedef struct {
	int		frameSamples[LAG_SAMPLES];
	int		frameCount;
	int		snapshotFlags[LAG_SAMPLES];
	int		snapshotSamples[LAG_SAMPLES];
	int		snapshotCount;
} lagometer_t;

lagometer_t		lagometer;

/*
==============
CG_AddLagometerFrameInfo

Adds the current interpolate / extrapolate bar for this frame
==============
*/
void CG_AddLagometerFrameInfo( void ) {
	int			offset;

	offset = cg.time - cg.latestSnapshotTime;
	lagometer.frameSamples[ lagometer.frameCount & ( LAG_SAMPLES - 1) ] = offset;
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
void CG_AddLagometerSnapshotInfo( snapshot_t *snap ) {
	// dropped packet
	if ( !snap ) {
		lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = -1;
		lagometer.snapshotCount++;
		return;
	}

	// add this snapshot's info
	lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = snap->ping;
	lagometer.snapshotFlags[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = snap->snapFlags;
	lagometer.snapshotCount++;
}

/*
==============
CG_DrawDisconnect

Should we draw something differnet for long lag vs no packets?
==============
*/
static void CG_DrawDisconnect( void ) {
	extern qboolean tce_uiCoordinates;
	qboolean previousCoordinates;
	float		x, y;
	int			cmdNum;
	usercmd_t	cmd;
	const char		*s;
	int			w;  // bk010215 - FIXME char message[1024];

	// OSP - dont draw if a demo and we're running at a different timescale
	if(cg.demoPlayback && cg_timescale.value != 1.0f) return;
	
	// ydnar: don't draw if the server is respawning
	if( cg.serverRespawning )
		return;

	// draw the phone jack if we are completely past our buffers
	cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
	trap_GetUserCmd( cmdNum, &cmd );
	if ( cmd.serverTime <= cg.snap->ps.commandTime
		|| cmd.serverTime > cg.time ) {	// special check for map_restart // bk 0102165 - FIXME
		return;
	}

	// also add text in center of screen
	/* Original DrawPic/DrawChar use the TC852x480 transform. Preserve this
	 * caller's coordinate domain through both text and the blinking icon. */
	previousCoordinates = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	s = CG_TranslateString( "Connection Interrupted" ); // bk 010215 - FIXME
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
	CG_DrawBigString( 426 - w/2, 100, s, 1.0F);

	// blink the icon
	if ( ( cg.time >> 9 ) & 1 ) {
		tce_uiCoordinates = previousCoordinates;
		return;
	}

	x = 852 - 48;
	y = 480 - 200;

	CG_DrawPic( x, y, 48, 48, cgs.media.disconnectIcon );
	tce_uiCoordinates = previousCoordinates;
}


#define	MAX_LAGOMETER_PING	900
#define	MAX_LAGOMETER_RANGE	300

/*
==============
CG_DrawLagometer
==============
*/
static void CG_DrawLagometer( void ) {
	extern qboolean tce_uiCoordinates;
	qboolean previousCoordinates;
	int		a, x, y, i;
	float	v;
	float	ax, ay, aw, ah, mid, range;
	int		color;
	float	vscale;

	if ( !cg_lagometer.integer || cgs.localServer ) {
//	if(0) {
		CG_DrawDisconnect();
		return;
	}

	//
	// draw the graph
	//
	previousCoordinates = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	x = 852 - 48;
	y = 480 - 200;

	trap_R_SetColor( NULL );
	CG_DrawPic( x, y, 48, 48, cgs.media.lagometerShader );

	ax = x;
	ay = y;
	aw = 48;
	ah = 48;
	CG_AdjustFrom640( &ax, &ay, &aw, &ah );

	color = -1;
	range = ah * (1.0f / 3.0f);
	mid = (float)((double)ah * (1.0f / 3.0f) + ay);

	vscale = range * (1.0f / MAX_LAGOMETER_RANGE);

	// draw the frame interpoalte / extrapolate graph
	for ( a = 0 ; a < aw ; a++ ) {
		i = ( lagometer.frameCount - 1 - a ) & (LAG_SAMPLES - 1);
		v = lagometer.frameSamples[i];
		v *= vscale;
		if ( v > 0 ) {
			if ( color != 1 ) {
				color = 1;
				trap_R_SetColor( colorYellow );
			}
			if ( v > range ) {
				v = range;
			}
			trap_R_DrawStretchPic ( ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
		} else if ( v < 0 ) {
			if ( color != 2 ) {
				color = 2;
				trap_R_SetColor( colorBlue );
			}
			v = -v;
			if ( v > range ) {
				v = range;
			}
			trap_R_DrawStretchPic( ax + aw - a, mid, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
		}
	}

	// draw the snapshot latency / drop graph
	range = ah / 2;
	vscale = range * (1.0f / MAX_LAGOMETER_PING);

	for ( a = 0 ; a < aw ; a++ ) {
		i = ( lagometer.snapshotCount - 1 - a ) & (LAG_SAMPLES - 1);
		v = lagometer.snapshotSamples[i];
		if ( v > 0 ) {
			if ( lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED ) {
				if ( color != 5 ) {
					color = 5;	// YELLOW for rate delay
					trap_R_SetColor( colorYellow );
				}
			} else {
				if ( color != 3 ) {
					color = 3;
					trap_R_SetColor( colorGreen );
				}
			}
			v = v * vscale;
			if ( v > range ) {
				v = range;
			}
			trap_R_DrawStretchPic( (float)((double)ax + aw - a), (float)((double)ah + ay - v), 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
		} else if ( v < 0 ) {
			if ( color != 4 ) {
				color = 4;		// RED for dropped snapshots
				trap_R_SetColor( colorRed );
			}
			trap_R_DrawStretchPic( (float)((double)ax + aw - a), (float)((double)ah + ay - range), 1, range, 0, 0, 0, 0, cgs.media.whiteShader );
		}
	}

	trap_R_SetColor( NULL );

	if ( cg_nopredict.integer 
#ifdef ALLOW_GSYNC
		|| cg_synchronousClients.integer 
#endif // ALLOW_GSYNC
		) {
		CG_DrawBigString( ax, ay, "snc", 1.0 );
	}

	tce_uiCoordinates = previousCoordinates;
	CG_DrawDisconnect();
}


void CG_DrawLivesLeft( void ) {
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	if( cg_gameType.integer == GT_WOLF_LMS ) {
		return;
	}

	if( cg.snap->ps.persistant[PERS_RESPAWNS_LEFT] < 0 ) {
		return;
	}

	previous = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	CG_DrawPic( 4, 360, 48, 24, cg.snap->ps.persistant[PERS_TEAM] == TEAM_ALLIES ? cgs.media.hudAlliedHelmet : cgs.media.hudAxisHelmet );

	CG_DrawField( 44, 360, 3, cg.snap->ps.persistant[PERS_RESPAWNS_LEFT], 14, 20, qtrue, qtrue );
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
#define CP_LINEWIDTH 56			// NERVE - SMF

static void CG_CenterPopup(int type,const char *text,int shader) {
 CG_AddPMItemBig((popupMessageBigType_t)type,text,shader);
}
void CG_CenterPrint(const char *str,int y,int charWidth) {
 TCE_CenterPrint(str,cg.time,&cg.centerPrintTime,&cg.centerPrintPriority,cgs.media.pmImages[PM_MESSAGE],CG_CenterPopup);
}
void CG_PriorityCenterPrint(const char *str,int y,int charWidth,int priority) {
 TCE_PriorityCenterPrint(str,cg.time,priority,&cg.centerPrintTime,&cg.centerPrintPriority,cgs.media.pmImages[PM_MESSAGE],CG_CenterPopup);
}

/*
===================
CG_DrawCenterString
===================
*/
static void CG_DrawCenterString( void ) {
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	char	*start;
	int		l;
	int		x, y, w;
	float	*color;

	if ( !cg.centerPrintTime ) {
		return;
	}

	color = CG_FadeColor( cg.centerPrintTime, (int)(1000.0 * cg_centertime.value) );
	if ( !color ) {
		cg.centerPrintTime = 0;
		cg.centerPrintPriority = 0;
		return;
	}

	previous = tce_uiCoordinates;
	tce_uiCoordinates = qtrue;
	trap_R_SetColor( color );

	start = cg.centerPrint;

	y = cg.centerPrintY - cg.centerPrintLines * BIGCHAR_HEIGHT / 2;

	while ( 1 ) {
		char linebuffer[1024];

		for ( l = 0; l < CP_LINEWIDTH; l++ ) {			// NERVE - SMF - added CP_LINEWIDTH
			if ( !start[l] || start[l] == '\n' ) {
				break;
			}
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = cg.centerPrintCharWidth * CG_DrawStrlen( linebuffer );

		x = ( 852 - w ) / 2;

		CG_DrawStringExt( x, y, linebuffer, color, qfalse, qtrue, cg.centerPrintCharWidth, (int)(cg.centerPrintCharWidth * 1.5), 0 );

		y += cg.centerPrintCharWidth * 1.5;

		while ( *start && ( *start != '\n' ) ) {
			start++;
		}
		if ( !*start ) {
			break;
		}
		start++;
	}

	trap_R_SetColor( NULL );
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
/* Shared TC scope projection, CG_DrawActive/CG_DrawWeapReticle. */
static void CG_TCEScopeCenter(float *x, float *y, vec3_t angles) {
    vec3_t forward, right, up;
    float radians;
    VectorCopy(cg.predictedPlayerState.viewangles, angles);
    angles[0] += ((float)cg.predictedPlayerState.holdable[5] - 2000.0f) * 0.01f;
    angles[1] += ((float)cg.predictedPlayerState.holdable[6] - 2000.0f) * 0.01f;
    AngleVectors(angles, forward, NULL, NULL);
    AngleVectors(cg.predictedPlayerState.viewangles, NULL, right, up);
    radians = cg.refdef.fov_x * 0.01745329238474369f;
    *x = (float)((asin(DotProduct(forward,right)) / radians) * 852.0 + 426.0);
    *y = (float)(240.0 - (asin(DotProduct(forward,up)) / radians) * 852.0);
}

static void CG_DrawWeapReticle(void) {
    extern qboolean tce_uiCoordinates;
    qboolean previous = tce_uiCoordinates;
    vec4_t black={0,0,0,1}, red={0.5f,0,0,0.7f}, light;
    vec3_t angles;
    int weapon, type, i;
    float x,y,size,half,quarter;
    static const float ticksX[]={224,248,272,296,341,365,389,413};
    static const float ticksY[]={144,168,192,216,263,287,311,335};
    weapon=((cg.snap->ps.pm_flags&PMF_FOLLOW)||cg.demoPlayback)?cg.snap->ps.weapon:cg.predictedPlayerState.weapon;
    if(weapon<0||weapon>=TCE_MAX_WEAPONS)return;
    type=weaponDef[weapon].scopeReticleType;
    tce_uiCoordinates=qtrue;
    if(!cg_portalScopes.integer) {
        if(type==2) {
            if(cgs.media.tceM76ReticleShader) CG_DrawPic(206,225,440,220,cgs.media.tceM76ReticleShader);
            CG_FillRect(425,239,2,2,red);
            CG_FillRect(0,0,186,480,black);CG_FillRect(666,0,186,480,black);
            if(cgs.media.reticleShaderSimple)CG_DrawPic(186,0,480,480,cgs.media.reticleShaderSimple);
        } else {
            if(type==1) {
                CG_FillRect(80,239,120,3,black);CG_FillRect(440,239,120,3,black);
                CG_FillRect(319,0,3,120,black);CG_FillRect(319,360,3,120,black);
                CG_FillRect(200,240,240,1,black);CG_FillRect(320,120,1,240,black);
                for(i=0;i<8;i++)CG_FillRect(ticksX[i],239,3,3,black);
                for(i=0;i<8;i++)CG_FillRect(319,ticksY[i],3,3,black);
            } else {
                CG_FillRect(80,240,480,1,black);CG_FillRect(320,0,1,480,black);
            }
            CG_FillRect(320,240,1,1,red);
            CG_FillRect(0,0,80,480,black);CG_FillRect(560,0,80,480,black);
            if(cgs.media.reticleShaderSimple)CG_DrawPic(80,0,480,480,cgs.media.reticleShaderSimple);
        }
    } else {
        CG_TCEScopeCenter(&x,&y,angles);
        /* Original reticle reads slot-zero width, unlike the scene viewport. */
        size=(float)tce_cg_weapons[0].portalScopeWidth;
        if(type==2) {
            size*=0.88f;if(size<=0)size=220;size*=1.2f;
            if(cgs.media.tceM76ReticleShader)CG_DrawPic(x-size*0.5f,y-size*0.03181818127632141f,size,size*0.5f,cgs.media.tceM76ReticleShader);
            CG_FillRect(x-1,y-1,2,2,red);
        } else if(type==1) {
            if(size<=0)size=250;size*=1.2f;half=size*0.5f;quarter=size*0.25f;
            CG_FillRect(x-half,y,size,1,black);CG_FillRect(x,y-half,1,size,black);
            CG_FillRect(x-half,y-1,quarter,3,black);CG_FillRect(x-half+size*0.75f,y-1,quarter,3,black);
            CG_FillRect(x-1,y-half,3,quarter,black);CG_FillRect(x-1,y-half+size*0.75f,3,quarter,black);
            CG_FillRect(x,y,1,1,red);
        } else {
            if(size<=0)size=250;half=size*0.5f;
            CG_FillRect(x-half,y,size,1,black);CG_FillRect(x,y-half,1,size,black);CG_FillRect(x,y,1,1,red);
        }
        if(cgs.media.tcePortalScopeShader) {
            TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(),cg.refdef.vieworg,light);
            for(i=0;i<3;i++){light[i]*=1.0f+cg.tceScopeLightBoost;if(light[i]>=1)light[i]=1;}
            light[3]=1;trap_R_SetColor(light);
            CG_DrawPic(x-288,(y-288)+54.000003814697266f,576,576,cgs.media.tcePortalScopeShader);
            trap_R_SetColor(NULL);
        }
    }
    tce_uiCoordinates=previous;
}

/*
==============
CG_DrawMortarReticle
==============
*/
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC300247c0: whole Windows reticle instruction graph; actual native cg fields. */
enum {
	MortarField3407df70 = offsetof(cg_t, predictedPlayerState) + offsetof(playerState_t, viewangles),
	MortarField3407df74 = offsetof(cg_t, predictedPlayerState) + offsetof(playerState_t, viewangles) + 4,
	MortarField34087f28 = offsetof(cg_t, pmext) + offsetof(pmoveExt_t, mountedWeaponAngles),
	MortarField34087f2c = offsetof(cg_t, pmext) + offsetof(pmoveExt_t, mountedWeaponAngles) + 4,
	MortarField34080f0c = offsetof(cg_t, lastFiredWeapon),
	MortarField340a2454 = offsetof(cg_t, mortarImpactTime),
	MortarField3407dea4 = offsetof(cg_t, time),
	MortarField3407e6b4 = offsetof(cg_t, predictedPlayerEntity) + offsetof(centity_t, muzzleFlashTime),
	MortarField340a244c = offsetof(cg_t, mortarFireAngles),
	MortarField340a2450 = offsetof(cg_t, mortarFireAngles) + 4,
	MortarField340a246c = offsetof(cg_t, artilleryRequestPos) + 4,
	MortarField340a2768 = offsetof(cg_t, artilleryRequestTime),
	MortarField340a2868 = offsetof(cg_t, artilleryRequestTime) + sizeof(((cg_t *)0)->artilleryRequestTime),
	MortarField3407eb7c = offsetof(cg_t, predictedPlayerEntity) + offsetof(centity_t, lerpOrigin),
	MortarField3407eb80 = offsetof(cg_t, predictedPlayerEntity) + offsetof(centity_t, lerpOrigin) + 4,
	MortarField32588f8c = offsetof(cgs_t, media) + offsetof(cgMedia_t, limboFont1),
	MortarField32588ea8 = offsetof(cgs_t, media) + offsetof(cgMedia_t, ccMortarTargetArrow),
	MortarField32588ea4 = offsetof(cgs_t, media) + offsetof(cgMedia_t, ccMortarTarget),
    MortarRequestTimeStride = sizeof(((cg_t *)0)->artilleryRequestTime[0]),
    MortarRequestPositionStride = sizeof(((cg_t *)0)->artilleryRequestPos[0])
};
static const unsigned int mortarConst30092a44[] = { 0x42b40000 };
static const unsigned int mortarConst300923e0[] = { 0x43b40000 };
static const unsigned int mortarConst30092a40[] = { 0x464ccccd };
static const unsigned int mortarConst30092a3c[] = { 0x38a00000 };
static const unsigned int mortarConst30092a38[] = { 0x3d888889 };
static const unsigned int mortarConst30092a30[] = { 0x9999999a, 0x3fc99999 };
static const unsigned int mortarConst300924d0[] = { 0x41200000 };
static const unsigned int mortarConst300928b8[] = { 0x43160000 };
static const unsigned int mortarConst30092a28[] = { 0x43520000 };
static const unsigned int mortarConst30092a24[] = { 0x43fa0000 };
static const unsigned int mortarConst300922b8[] = { 0x3f000000 };
static const unsigned int mortarConst300928d4[] = { 0x42340000 };
static const unsigned int mortarConst30092a20[] = { 0x3e4ccccd };
static const unsigned int mortarConst30092a1c[] = { 0x43a00000 };
static const unsigned int mortarConst30092a18[] = { 0x39aec33e };
static const unsigned int mortarConst300922b4[] = { 0x3f800000 };
static const unsigned int mortarConst300920e0[] = { 0x00000000 };
static const unsigned int mortarConst30092a10[] = { 0x0d03cf25, 0x404ca5dc };
static const unsigned int mortarConst300928d0[] = { 0x3951b717 };
static const unsigned int mortarConst300924cc[] = { 0x43870000 };
static const unsigned int mortarConst300922e8[] = { 0x41000000 };
static const unsigned int mortarConst300923d0[] = { 0x42700000 };
static const unsigned int mortarConst30092a0c[] = { 0x46cccccd };
static const unsigned int mortarConst30092a08[] = { 0x38200000 };
static const unsigned int mortarConst30092a04[] = { 0x41c80000 };
static const unsigned int mortarConst30092a00[] = { 0x3dcccccd };
static const unsigned int mortarConst300929f8[] = { 0x00000000, 0x40240000 };
static const unsigned int mortarConst300929f0[] = { 0x47ae147b, 0x3fa47ae1 };
static const unsigned int mortarConst300922bc[] = { 0x40800000 };
static const unsigned int mortarConst300929e8[] = { 0x43240000 };
static const unsigned int mortarConst3009233c[] = { 0x41a00000 };
static const unsigned int mortarConst30092398[] = { 0x41f00000 };
static const unsigned int mortarConst300929e4[] = { 0x3ecccccd };
static const unsigned int mortarConst300923e8[] = { 0x42c80000 };
static const unsigned int mortarConst300929e0[] = { 0x43840000 };
static const char mortarReticleFormat[] = "%i";
/* Preserve the original double argument/ST0 boundary; CRT floor remains external. */
static double (__cdecl *mortarReticleFloor)(double) = floor;
__declspec(naked) static int CG_MortarReticleTruncateST0(void)
{
	__asm {
		push ebp
		mov ebp, esp
		sub esp, 12
		fwait
		fnstcw word ptr [ebp-2]
		fwait
		mov ax, word ptr [ebp-2]
		or ah, 0ch
		mov word ptr [ebp-4], ax
		fldcw word ptr [ebp-4]
		fistp qword ptr [ebp-12]
		fldcw word ptr [ebp-2]
		mov eax, dword ptr [ebp-12]
		mov edx, dword ptr [ebp-8]
		leave
		ret
	}
}
__declspec(naked) static void CG_DrawMortarReticle(void)
{
	__asm {
		SUB ESP,070h
		PUSH EBX
		PUSH EBP
		PUSH ESI
		LEA EAX,[ESP + 06ch]
		PUSH EDI
		PUSH EAX
		PUSH 042180000h
		PUSH 0431a0000h
		PUSH 0436c0000h
		PUSH 043080000h
		MOV dword ptr [ESP + 044h],03f800000h
		MOV dword ptr [ESP + 048h],03f800000h
		MOV dword ptr [ESP + 04ch],03f800000h
		MOV dword ptr [ESP + 050h],03f000000h
		MOV dword ptr [ESP + 084h],00h
		MOV dword ptr [ESP + 088h],00h
		MOV dword ptr [ESP + 08ch],00h
		MOV dword ptr [ESP + 090h],03e800000h
		MOV dword ptr [ESP + 054h],03f451eb8h
		MOV dword ptr [ESP + 058h],03f3ae148h
		MOV dword ptr [ESP + 05ch],03dcccccdh
		MOV dword ptr [ESP + 060h],03f800000h
		MOV dword ptr [ESP + 064h],03f451eb8h
		MOV dword ptr [ESP + 068h],03dcccccdh
		MOV dword ptr [ESP + 06ch],03dcccccdh
		MOV dword ptr [ESP + 070h],03f800000h
		MOV dword ptr [ESP + 074h],03f800000h
		MOV dword ptr [ESP + 078h],03f800000h
		MOV dword ptr [ESP + 07ch],03f800000h
		MOV dword ptr [ESP + 080h],03f800000h
		CALL CG_FillRect
		LEA ECX,[ESP + 084h]
		PUSH ECX
		PUSH 043500000h
		PUSH 042700000h
		PUSH 043200000h
		PUSH 043910000h
		CALL CG_FillRect
		LEA EDX,[ESP + 098h]
		PUSH EDX
		PUSH 042180000h
		PUSH 0431a0000h
		PUSH 0436c0000h
		PUSH 043af0000h
		CALL CG_FillRect
		LEA EAX,[ESP + 06ch]
		PUSH EAX
		PUSH 03f800000h
		PUSH 043160000h
		PUSH 043840000h
		PUSH 0430c0000h
		CALL CG_FillRect
		ADD ESP,050h
		LEA ECX,[ESP + 030h]
		PUSH ECX
		PUSH 03f800000h
		PUSH 043160000h
		PUSH 043840000h
		PUSH 043af0000h
		CALL CG_FillRect
		FLD dword ptr [cg + MortarField3407df74]
		FSUB dword ptr [mortarConst30092a44]
		ADD ESP,010h
		FSTP dword ptr [ESP]
		CALL AngleNormalize360
		FSUBR dword ptr [mortarConst300923e0]
		FST dword ptr [ESP + 018h]
		FMUL dword ptr [mortarConst30092a40]
		CALL CG_MortarReticleTruncateST0
		AND EAX,0ffffh
		MOV dword ptr [ESP + 024h],EAX
		FILD dword ptr [ESP + 024h]
		FMUL dword ptr [mortarConst30092a3c]
		FSTP dword ptr [ESP + 01ch]
		FLD dword ptr [ESP + 018h]
		FSUB dword ptr [mortarConst30092a44]
		FSTP dword ptr [ESP + 024h]
		MOV EDI,dword ptr [ESP + 024h]
		PUSH EDI
		CALL AngleNormalize360
		FMUL dword ptr [mortarConst30092a38]
		CALL CG_MortarReticleTruncateST0
		PUSH EDI
		LEA ESI,[EAX + EAX*02h]
		CALL AngleNormalize360
		FSTP qword ptr [ESP + 04h]
		ADD ESP,04h
		CALL dword ptr [mortarReticleFloor]
		CALL CG_MortarReticleTruncateST0
		CDQ
		MOV ECX,0fh
		IDIV ECX
		MOV dword ptr [ESP + 028h],EDX
		FILD dword ptr [ESP + 028h]
		FSTP qword ptr [ESP]
		CALL dword ptr [mortarReticleFloor]
		FMUL qword ptr [mortarConst30092a30]
		ADD ESP,08h
		CALL CG_MortarReticleTruncateST0
		FLD dword ptr [ESP + 018h]
		FADD st(0),st(0)
		XOR EBP,EBP
		LEA EDI,[ESI + ESI*04h + 0b4h]
		LEA EBX,[ESI + ESI*04h + 0ffffff4ch]
		MOV dword ptr [ESP + 01ch],EAX
		MOV dword ptr [ESP + 010h],EBP
		LEA ESI,[ESI + ESI*04h + 021ch]
		FSTP dword ptr [ESP + 020h]
mortarNative_300249e4:
		FILD dword ptr [ESP + 010h]
		FMUL dword ptr [mortarConst300924d0]
		FADD dword ptr [ESP + 020h]
		FST dword ptr [ESP + 010h]
		FCOMP dword ptr [mortarConst300928b8]
		FNSTSW AX
		TEST AH,01h
		JNZ mortarNative_30024a36
		FLD dword ptr [ESP + 010h]
		FCOMP dword ptr [mortarConst30092a28]
		FNSTSW AX
		TEST AH,041h
		JZ mortarNative_30024a36
		MOV EAX,EBP
		MOV ECX,03h
		CDQ
		IDIV ECX
		CMP EDX,dword ptr [ESP + 01ch]
		JNZ mortarNative_30024b1f
		SUB EDI,0fh
		SUB ESI,0fh
		SUB EBX,0fh
		JMP mortarNative_30024b1f
mortarNative_30024a36:
		MOV EAX,EBP
		MOV ECX,03h
		CDQ
		IDIV ECX
		CMP EDX,dword ptr [ESP + 01ch]
		JNZ mortarNative_30024af5
		TEST EDI,EDI
		MOV EAX,EDI
		JGE mortarNative_30024a54
		MOV EAX,ESI
		JMP mortarNative_30024a5e
mortarNative_30024a54:
		CMP EDI,0168h
		JL mortarNative_30024a5e
		MOV EAX,EBX
mortarNative_30024a5e:
		PUSH EAX
		PUSH OFFSET mortarReticleFormat
		CALL va
		ADD ESP,08h
		LEA EDX,[ESP + 030h]
		FLD dword ptr [mortarConst30092a24]
		FSUB dword ptr [ESP + 010h]
		PUSH OFFSET cgs + MortarField32588f8c
		PUSH 00h
		PUSH 00h
		PUSH 00h
		PUSH EAX
		PUSH EDX
		FSTP dword ptr [ESP + 030h]
		PUSH 03e19999ah
		PUSH 03e19999ah
		PUSH 043740000h
		PUSH OFFSET cgs + MortarField32588f8c
		PUSH 00h
		PUSH 03e19999ah
		PUSH EAX
		CALL CG_Text_Width_Ext
		MOV dword ptr [ESP + 044h],EAX
		ADD ESP,0ch
		FILD dword ptr [ESP + 038h]
		FMUL dword ptr [mortarConst300922b8]
		FSUBR dword ptr [ESP + 040h]
		FSTP dword ptr [ESP]
		CALL CG_Text_Paint_Ext
		MOV ECX,dword ptr [ESP + 040h]
		LEA EAX,[ESP + 058h]
		PUSH EAX
		PUSH 041800000h
		PUSH 03f800000h
		PUSH 043780000h
		PUSH ECX
		CALL CG_FillRect
		ADD ESP,03ch
		SUB EDI,0fh
		SUB ESI,0fh
		SUB EBX,0fh
		JMP mortarNative_30024b1f
mortarNative_30024af5:
		FLD dword ptr [mortarConst30092a24]
		FSUB dword ptr [ESP + 010h]
		LEA EDX,[ESP + 030h]
		PUSH EDX
		PUSH 041000000h
		PUSH 03f800000h
		PUSH 043800000h
		PUSH ECX
		FSTP dword ptr [ESP]
		CALL CG_FillRect
		ADD ESP,014h
mortarNative_30024b1f:
		INC EBP
		CMP EBP,024h
		MOV dword ptr [ESP + 010h],EBP
		JL mortarNative_300249e4
		FLD dword ptr [cg + MortarField34087f2c]
		FSUB dword ptr [mortarConst30092a44]
		PUSH ECX
		FSUBR dword ptr [mortarConst300923e0]
		FSUB dword ptr [mortarConst300928d4]
		FSTP dword ptr [ESP]
		CALL AngleNormalize360
		FSTP dword ptr [ESP + 01ch]
		FLD dword ptr [cg + MortarField34087f2c]
		FSUB dword ptr [mortarConst30092a44]
		FSUBR dword ptr [mortarConst300923e0]
		FADD dword ptr [mortarConst300928d4]
		FSTP dword ptr [ESP]
		CALL AngleNormalize360
		FSTP dword ptr [ESP + 024h]
		FLD dword ptr [ESP + 018h]
		FSUB dword ptr [ESP + 01ch]
		FSTP dword ptr [ESP]
		CALL AngleNormalize360
		FMUL dword ptr [mortarConst30092a20]
		ADD ESP,04h
		LEA EAX,[ESP + 040h]
		FMUL dword ptr [mortarConst300924d0]
		FLD dword ptr [mortarConst30092a1c]
		PUSH EAX
		PUSH 041900000h
		PUSH 040000000h
		PUSH 0437c0000h
		FSUB st(0),st(1)
		PUSH ECX
		FSTP dword ptr [ESP]
		FSTP st(0)
		CALL CG_FillRect
		FLD dword ptr [ESP + 034h]
		FSUB dword ptr [ESP + 028h]
		ADD ESP,010h
		FSTP dword ptr [ESP]
		CALL AngleNormalize360
		FMUL dword ptr [mortarConst30092a20]
		ADD ESP,04h
		LEA ECX,[ESP + 040h]
		FMUL dword ptr [mortarConst300924d0]
		PUSH ECX
		PUSH 041900000h
		PUSH 040000000h
		PUSH 0437c0000h
		FADD dword ptr [mortarConst30092a1c]
		PUSH ECX
		FSTP dword ptr [ESP]
		CALL CG_FillRect
		MOV EAX,[cg + MortarField34080f0c]
		ADD ESP,014h
		XOR EBX,EBX
		CMP EAX,03ch
		MOV dword ptr [ESP + 01ch],EBX
		JNZ mortarNative_30024cc1
		CMP dword ptr [cg + MortarField340a2454],-01h
		JL mortarNative_30024cc1
		MOV EAX,[cg + MortarField3407dea4]
		MOV ESI,dword ptr [cg + MortarField3407e6b4]
		SUB EAX,ESI
		SUB EAX,01388h
		CMP EAX,0bb8h
		MOV dword ptr [ESP + 01ch],EAX
		JGE mortarNative_30024cc1
		CMP EAX,EBX
		JLE mortarNative_30024c5a
		FILD dword ptr [ESP + 01ch]
		FMUL dword ptr [mortarConst30092a18]
		FSUBR dword ptr [mortarConst300922b4]
		FSTP dword ptr [ESP + 05ch]
mortarNative_30024c5a:
		FLD dword ptr [cg + MortarField340a2450]
		FSUB dword ptr [mortarConst30092a44]
		PUSH ECX
		FSUBR dword ptr [mortarConst300923e0]
		FSTP dword ptr [ESP]
		CALL AngleNormalize360
		FSTP dword ptr [ESP + 024h]
		MOV EDX,dword ptr [ESP + 024h]
		MOV EAX,dword ptr [ESP + 018h]
		PUSH EDX
		PUSH EAX
		CALL AngleSubtract
		FMUL dword ptr [mortarConst30092a20]
		ADD ESP,0ch
		LEA ECX,[ESP + 050h]
		FMUL dword ptr [mortarConst300924d0]
		FLD dword ptr [mortarConst30092a1c]
		PUSH ECX
		PUSH 041900000h
		PUSH 040000000h
		PUSH 0437c0000h
		FSUB st(0),st(1)
		PUSH ECX
		FSTP dword ptr [ESP]
		FSTP st(0)
		CALL CG_FillRect
		ADD ESP,014h
mortarNative_30024cc1:
		MOV EAX,OFFSET cg + MortarField340a246c
		XOR EBP,EBP
		MOV dword ptr [ESP + 020h],EAX
		MOV EDI,OFFSET cg + MortarField340a2768
mortarNative_30024cd1:
		MOV ECX,dword ptr [cg + MortarField3407dea4]
		MOV EDX,dword ptr [EDI]
		SUB ECX,EDX
		SUB ECX,061a8h
		CMP ECX,01388h
		MOV dword ptr [ESP + 010h],ECX
		JGE mortarNative_30024ebe
		FLD dword ptr [EAX + -04h]
		FSUB dword ptr [cg + MortarField3407eb7c]
		FSTP dword ptr [ESP + 024h]
		FLD dword ptr [EAX]
		FSUB dword ptr [cg + MortarField3407eb80]
		FCOM dword ptr [mortarConst300920e0]
		FLD dword ptr [ESP + 024h]
		FNSTSW AX
		FCOMP dword ptr [mortarConst300920e0]
		TEST AH,040h
		FNSTSW AX
		JZ mortarNative_30024d2e
		TEST AH,040h
		JZ mortarNative_30024d37
		FSTP st(0)
		FLD dword ptr [mortarConst300920e0]
		JMP mortarNative_30024d56
mortarNative_30024d2e:
		TEST AH,040h
		JNZ mortarNative_30024ded
mortarNative_30024d37:
		FLD dword ptr [ESP + 024h]
		FPATAN
		FMUL qword ptr [mortarConst30092a10]
		FCOM dword ptr [mortarConst300920e0]
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_30024d56
		FADD dword ptr [mortarConst300923e0]
mortarNative_30024d56:
		TEST ECX,ECX
		JLE mortarNative_30024d6e
		FILD dword ptr [ESP + 010h]
		FMUL dword ptr [mortarConst300928d0]
		FSUBR dword ptr [mortarConst300922b4]
		FSTP dword ptr [ESP + 06ch]
mortarNative_30024d6e:
		FSUB dword ptr [mortarConst30092a44]
		PUSH ECX
		FSUBR dword ptr [mortarConst300923e0]
		FSTP dword ptr [ESP]
		CALL AngleNormalize360
		FSTP dword ptr [ESP + 014h]
		MOV EDX,dword ptr [ESP + 01ch]
		MOV ESI,dword ptr [ESP + 014h]
		PUSH EDX
		PUSH ESI
		CALL AngleSubtract
		FCOM dword ptr [mortarConst300920e0]
		ADD ESP,0ch
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_30024e10
		TEST EBP,EBP
		FSTP st(0)
		JNZ mortarNative_30024ebe
		LEA EAX,[ESP + 060h]
		PUSH EAX
		CALL trap_R_SetColor
		MOV ECX,dword ptr [cgs + MortarField32588ea8]
		PUSH ECX
		PUSH 041000000h
		PUSH 041000000h
		PUSH 043848000h
		PUSH 0430a0000h
		CALL CG_DrawPic
		PUSH EBP
		CALL trap_R_SetColor
		ADD ESP,01ch
		MOV EBP,01h
		JMP mortarNative_30024ebe
mortarNative_30024ded:
		FCOMP dword ptr [mortarConst300920e0]
		FNSTSW AX
		TEST AH,041h
		JNZ mortarNative_30024e05
		FLD dword ptr [mortarConst30092a44]
		JMP mortarNative_30024d56
mortarNative_30024e05:
		FLD dword ptr [mortarConst300924cc]
		JMP mortarNative_30024d56
mortarNative_30024e10:
		FCOMP dword ptr [mortarConst30092a44]
		FNSTSW AX
		TEST AH,041h
		JNZ mortarNative_30024e5e
		TEST EBX,EBX
		JNZ mortarNative_30024ebe
		LEA EDX,[ESP + 060h]
		PUSH EDX
		CALL trap_R_SetColor
		MOV EAX,[cgs + MortarField32588ea8]
		PUSH EAX
		PUSH 041000000h
		PUSH 0c1000000h
		PUSH 043848000h
		PUSH 043f70000h
		CALL CG_DrawPic
		PUSH EBX
		CALL trap_R_SetColor
		ADD ESP,01ch
		MOV EBX,01h
		JMP mortarNative_30024ebe
mortarNative_30024e5e:
		MOV ECX,dword ptr [ESP + 014h]
		PUSH ESI
		PUSH ECX
		CALL AngleSubtract
		FMUL dword ptr [mortarConst30092a20]
		LEA EDX,[ESP + 068h]
		PUSH EDX
		FMUL dword ptr [mortarConst300924d0]
		FSTP dword ptr [ESP + 01ch]
		CALL trap_R_SetColor
		FLD dword ptr [mortarConst30092a1c]
		MOV EAX,[cgs + MortarField32588ea4]
		ADD ESP,0ch
		FSUB dword ptr [ESP + 010h]
		PUSH EAX
		PUSH 041800000h
		PUSH 041800000h
		PUSH 043800000h
		FSUB dword ptr [mortarConst300922e8]
		PUSH ECX
		FSTP dword ptr [ESP]
		CALL CG_DrawPic
		PUSH 00h
		CALL trap_R_SetColor
		ADD ESP,018h
mortarNative_30024ebe:
		MOV EAX,dword ptr [ESP + 020h]
		ADD EDI,MortarRequestTimeStride
		ADD EAX,MortarRequestPositionStride
		CMP EDI,OFFSET cg + MortarField340a2868
		MOV dword ptr [ESP + 020h],EAX
		JL mortarNative_30024cd1
		LEA ECX,[ESP + 030h]
		PUSH ECX
		PUSH 043480000h
		PUSH 03f800000h
		PUSH 043240000h
		PUSH 043938000h
		CALL CG_FillRect
		LEA EDX,[ESP + 044h]
		PUSH EDX
		PUSH 043480000h
		PUSH 03f800000h
		PUSH 043240000h
		PUSH 043ac8000h
		CALL CG_FillRect
		FLD dword ptr [cg + MortarField3407df70]
		FSUB dword ptr [mortarConst300923d0]
		ADD ESP,024h
		FSUBR dword ptr [mortarConst300923e0]
		FSTP dword ptr [ESP]
		CALL AngleNormalize180
		FST dword ptr [ESP + 018h]
		FMUL dword ptr [mortarConst30092a0c]
		CALL CG_MortarReticleTruncateST0
		AND EAX,0ffffh
		PUSH ECX
		MOV dword ptr [ESP + 028h],EAX
		FILD dword ptr [ESP + 028h]
		FMUL dword ptr [mortarConst30092a08]
		FSTP dword ptr [ESP + 020h]
		FLD dword ptr [ESP + 01ch]
		FADD dword ptr [mortarConst30092a04]
		FST dword ptr [ESP + 028h]
		FMUL dword ptr [mortarConst30092a00]
		FSTP qword ptr [ESP]
		CALL dword ptr [mortarReticleFloor]
		FMUL qword ptr [mortarConst300929f8]
		CALL CG_MortarReticleTruncateST0
		FLD dword ptr [ESP + 028h]
		FMUL dword ptr [mortarConst300924d0]
		MOV EDI,EAX
		CALL CG_MortarReticleTruncateST0
		CDQ
		MOV ECX,064h
		IDIV ECX
		MOV dword ptr [ESP + 028h],EDX
		FILD dword ptr [ESP + 028h]
		FSTP qword ptr [ESP]
		CALL dword ptr [mortarReticleFloor]
		FMUL qword ptr [mortarConst300929f0]
		ADD ESP,08h
		CALL CG_MortarReticleTruncateST0
		FLD dword ptr [ESP + 018h]
		FMUL dword ptr [mortarConst300922bc]
		XOR EBX,EBX
		MOV EBP,EAX
		MOV dword ptr [ESP + 010h],EBX
		FSTP dword ptr [ESP + 020h]
mortarNative_30024fcc:
		FILD dword ptr [ESP + 010h]
		MOV EDX,EBX
		AND EDX,080000003h
		FMUL dword ptr [mortarConst300924d0]
		FADD dword ptr [ESP + 020h]
		FSTP dword ptr [ESP + 010h]
		JNS mortarNative_30024fed
		DEC EDX
		OR EDX,0fffffffch
		INC EDX
mortarNative_30024fed:
		CMP EDX,EBP
		JNZ mortarNative_300250ef
		CMP EDI,0ffffff4ch
		MOV EAX,EDI
		JG mortarNative_30025007
		LEA EAX,[EDI + 0168h]
		JMP mortarNative_30025015
mortarNative_30025007:
		CMP EDI,0b4h
		JL mortarNative_30025015
		LEA EAX,[EDI + 0ffffff4ch]
mortarNative_30025015:
		PUSH EAX
		PUSH OFFSET mortarReticleFormat
		CALL va
		ADD ESP,08h
		MOV ESI,EAX
		LEA EAX,[ESP + 030h]
		PUSH OFFSET cgs + MortarField32588f8c
		PUSH 00h
		PUSH 00h
		PUSH 00h
		PUSH ESI
		PUSH EAX
		PUSH 03e19999ah
		PUSH 03e19999ah
		PUSH OFFSET cgs + MortarField32588f8c
		PUSH 00h
		PUSH 03e19999ah
		PUSH ESI
		CALL CG_Text_Height_Ext
		MOV dword ptr [ESP + 048h],EAX
		ADD ESP,0ch
		FILD dword ptr [ESP + 03ch]
		FMUL dword ptr [mortarConst300922b8]
		FADD dword ptr [ESP + 034h]
		FADD dword ptr [mortarConst300929e8]
		FSTP dword ptr [ESP]
		PUSH OFFSET cgs + MortarField32588f8c
		PUSH 00h
		PUSH 03e19999ah
		PUSH ESI
		CALL CG_Text_Width_Ext
		MOV dword ptr [ESP + 04ch],EAX
		ADD ESP,0ch
		FILD dword ptr [ESP + 040h]
		FMUL dword ptr [mortarConst300922b8]
		FSUBR dword ptr [mortarConst30092a1c]
		FSTP dword ptr [ESP]
		CALL CG_Text_Paint_Ext
		FLD dword ptr [ESP + 038h]
		FADD dword ptr [mortarConst300929e8]
		LEA ECX,[ESP + 058h]
		PUSH ECX
		PUSH 03f800000h
		PUSH 041400000h
		FSTP dword ptr [ESP + 04ch]
		MOV ESI,dword ptr [ESP + 04ch]
		PUSH ESI
		PUSH 043940000h
		CALL CG_FillRect
		LEA EDX,[ESP + 06ch]
		PUSH EDX
		PUSH 03f800000h
		PUSH 041400000h
		PUSH ESI
		PUSH 043a68000h
		CALL CG_FillRect
		ADD ESP,050h
		SUB EDI,0ah
		JMP mortarNative_30025138
mortarNative_300250ef:
		FLD dword ptr [ESP + 010h]
		FADD dword ptr [mortarConst300929e8]
		LEA EAX,[ESP + 030h]
		PUSH EAX
		PUSH 03f800000h
		PUSH 041000000h
		FSTP dword ptr [ESP + 024h]
		MOV ESI,dword ptr [ESP + 024h]
		PUSH ESI
		PUSH 043940000h
		CALL CG_FillRect
		LEA ECX,[ESP + 044h]
		PUSH ECX
		PUSH 03f800000h
		PUSH 041000000h
		PUSH ESI
		PUSH 043a88000h
		CALL CG_FillRect
		ADD ESP,028h
mortarNative_30025138:
		INC EBX
		CMP EBX,014h
		MOV dword ptr [ESP + 010h],EBX
		JL mortarNative_30024fcc
		FLD dword ptr [cg + MortarField34087f28]
		FSUB dword ptr [mortarConst300923d0]
		PUSH ECX
		FSUBR dword ptr [mortarConst300923e0]
		FSTP dword ptr [ESP]
		CALL AngleNormalize180
		FSUB dword ptr [mortarConst3009233c]
		FSTP dword ptr [ESP + 01ch]
		FLD dword ptr [cg + MortarField34087f28]
		FSUB dword ptr [mortarConst300923d0]
		FSUBR dword ptr [mortarConst300923e0]
		FSTP dword ptr [ESP]
		CALL AngleNormalize180
		FADD dword ptr [mortarConst30092398]
		ADD ESP,04h
		FSUB dword ptr [ESP + 014h]
		FST dword ptr [ESP + 010h]
		FCOMP dword ptr [mortarConst300920e0]
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_300251ab
		MOV dword ptr [ESP + 010h],00h
mortarNative_300251ab:
		MOV EDX,dword ptr [ESP + 010h]
		PUSH EDX
		CALL AngleNormalize360
		FMUL dword ptr [mortarConst300929e4]
		ADD ESP,04h
		FMUL dword ptr [mortarConst300924d0]
		FCOM dword ptr [mortarConst300923e8]
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_3002521c
		FLD dword ptr [mortarConst300929e0]
		FSUB st(0),st(1)
		LEA EAX,[ESP + 040h]
		PUSH EAX
		PUSH 040000000h
		PUSH 040c00000h
		FSTP dword ptr [ESP + 01ch]
		MOV ESI,dword ptr [ESP + 01ch]
		PUSH ESI
		PUSH 043928000h
		FSTP st(0)
		CALL CG_FillRect
		LEA ECX,[ESP + 054h]
		PUSH ECX
		PUSH 040000000h
		PUSH 040c00000h
		PUSH ESI
		PUSH 043ab0000h
		CALL CG_FillRect
		ADD ESP,028h
		JMP mortarNative_3002521e
mortarNative_3002521c:
		FSTP st(0)
mortarNative_3002521e:
		FLD dword ptr [ESP + 014h]
		FSUB dword ptr [ESP + 018h]
		FST dword ptr [ESP + 010h]
		FCOMP dword ptr [mortarConst300920e0]
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_3002523f
		MOV dword ptr [ESP + 010h],00h
mortarNative_3002523f:
		MOV EDX,dword ptr [ESP + 010h]
		PUSH EDX
		CALL AngleNormalize360
		FMUL dword ptr [mortarConst300929e4]
		ADD ESP,04h
		FMUL dword ptr [mortarConst300924d0]
		FCOM dword ptr [mortarConst300923e8]
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_300252ac
		FADD dword ptr [mortarConst300929e0]
		LEA EAX,[ESP + 040h]
		PUSH EAX
		PUSH 040000000h
		PUSH 040c00000h
		FSTP dword ptr [ESP + 024h]
		MOV ESI,dword ptr [ESP + 024h]
		PUSH ESI
		PUSH 043928000h
		CALL CG_FillRect
		LEA ECX,[ESP + 054h]
		PUSH ECX
		PUSH 040000000h
		PUSH 040c00000h
		PUSH ESI
		PUSH 043ab0000h
		CALL CG_FillRect
		ADD ESP,028h
		JMP mortarNative_300252ae
mortarNative_300252ac:
		FSTP st(0)
mortarNative_300252ae:
		CMP dword ptr [cg + MortarField34080f0c],03ch
		JNZ mortarNative_300253fb
		CMP dword ptr [cg + MortarField340a2454],-01h
		JL mortarNative_300253fb
		CMP dword ptr [ESP + 01ch],0bb8h
		JGE mortarNative_300253fb
		FLD dword ptr [cg + MortarField340a244c]
		FSUB dword ptr [mortarConst300923d0]
		PUSH ECX
		FSUBR dword ptr [mortarConst300923e0]
		FSTP dword ptr [ESP]
		CALL AngleNormalize180
		FCOM dword ptr [ESP + 018h]
		ADD ESP,04h
		FNSTSW AX
		TEST AH,041h
		JNZ mortarNative_30025369
		FSUB dword ptr [ESP + 014h]
		FST dword ptr [ESP + 010h]
		FCOMP dword ptr [mortarConst300920e0]
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_3002531c
		MOV dword ptr [ESP + 010h],00h
mortarNative_3002531c:
		MOV EDX,dword ptr [ESP + 010h]
		PUSH EDX
		CALL AngleNormalize360
		FMUL dword ptr [mortarConst300929e4]
		ADD ESP,04h
		FMUL dword ptr [mortarConst300924d0]
		FCOM dword ptr [mortarConst300923e8]
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_300253f9
		FLD dword ptr [mortarConst300929e0]
		FSUB st(0),st(1)
		LEA EAX,[ESP + 050h]
		PUSH EAX
		PUSH 040000000h
		PUSH 040c00000h
		FSTP dword ptr [ESP + 01ch]
		MOV ESI,dword ptr [ESP + 01ch]
		FSTP st(0)
		JMP mortarNative_300253c9
mortarNative_30025369:
		FSUBR dword ptr [ESP + 014h]
		FST dword ptr [ESP + 010h]
		FCOMP dword ptr [mortarConst300920e0]
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_30025386
		MOV dword ptr [ESP + 010h],00h
mortarNative_30025386:
		MOV EDX,dword ptr [ESP + 010h]
		PUSH EDX
		CALL AngleNormalize360
		FMUL dword ptr [mortarConst300929e4]
		ADD ESP,04h
		FMUL dword ptr [mortarConst300924d0]
		FCOM dword ptr [mortarConst300923e8]
		FNSTSW AX
		TEST AH,01h
		JZ mortarNative_300253f9
		FADD dword ptr [mortarConst300929e0]
		LEA EAX,[ESP + 050h]
		PUSH EAX
		PUSH 040000000h
		PUSH 040c00000h
		FSTP dword ptr [ESP + 024h]
		MOV ESI,dword ptr [ESP + 024h]
mortarNative_300253c9:
		PUSH ESI
		PUSH 043928000h
		CALL CG_FillRect
		LEA ECX,[ESP + 064h]
		PUSH ECX
		PUSH 040000000h
		PUSH 040c00000h
		PUSH ESI
		PUSH 043ab0000h
		CALL CG_FillRect
		ADD ESP,028h
		POP EDI
		POP ESI
		POP EBP
		POP EBX
		ADD ESP,070h
		RET
mortarNative_300253f9:
		FSTP st(0)
mortarNative_300253fb:
		POP EDI
		POP ESI
		POP EBP
		POP EBX
		ADD ESP,070h
		RET
	}
}
#else

static void CG_DrawMortarReticle( void ) {
	vec4_t	color = { 1.f, 1.f, 1.f, .5f };
	vec4_t	color_back = { 0.f, 0.f, 0.f, .25f };
	vec4_t	color_extends = { .77f, .73f, .1f, 1.f };
	vec4_t	color_lastfire = { .77f, .1f, .1f, 1.f };
	//vec4_t	color_firerequest = { .23f, 1.f, .23f, 1.f };
	vec4_t	color_firerequest = { 1.f, 1.f, 1.f, 1.f };
	float	offset, localOffset;
	int		i, min, majorOffset, val, printval, fadeTime;
	char	*s;
	float	angle, angleMin, angleMax;
	qboolean hasRightTarget, hasLeftTarget;

	// Background
	CG_FillRect( 136, 236, 154, 38, color_back );
	CG_FillRect( 290, 160, 60, 208, color_back );
	CG_FillRect( 350, 236, 154, 38, color_back );

	// Horizontal bar

	// bottom
	CG_FillRect( 140, 264, 150, 1, color);	// left
	CG_FillRect( 350, 264, 150, 1, color);	// right

	// 10 units - 5 degrees
	// total of 360 units
	// nothing displayed between 150 and 210 units
	// 360 / 10 = 36 bits, means 36 * 5 = 180 degrees
	// that means left is cg.predictedPlayerState.viewangles[YAW] - .5f * 180
	angle = 360 - AngleNormalize360(cg.predictedPlayerState.viewangles[YAW] - 90.f);

	offset = (5.f / 65536) * ((int)(angle * (65536 / 5.f)) & 65535);
	min = (int)(AngleNormalize360(angle - .5f * 180) / 15.f) * 15;
	majorOffset = (int)(floor((int)floor(AngleNormalize360(angle - .5f * 180)) % 15) / 5.f );

	for( val = i = 0; i < 36; i++ ) {
		localOffset = i * 10.f + (offset * 2.f);

		if( localOffset >= 150 && localOffset <= 210 ) {
			if( i % 3 == majorOffset)
				val++;
			continue;
		}

		if( i % 3 == majorOffset) {
			printval = min - val * 15 + 180;
			
			// rain - old tertiary abuse was nasty and had undefined result
			if (printval < 0)
				printval += 360;
			else if (printval >= 360)
				printval -= 360;

			s = va( "%i", printval );
			//CG_Text_Paint_Ext( 140 + localOffset - .5f * CG_Text_Width_Ext( s, .15f, 0, &cgs.media.limboFont1 ), 244, .15f, .15f, color, s, 0, 0, 0, &cgs.media.limboFont1 );
			//CG_FillRect( 140 + localOffset, 248, 1, 16, color);
			CG_Text_Paint_Ext( 500 - localOffset - .5f * CG_Text_Width_Ext( s, .15f, 0, &cgs.media.limboFont1 ), 244, .15f, .15f, color, s, 0, 0, 0, &cgs.media.limboFont1 );
			CG_FillRect( 500 - localOffset, 248, 1, 16, color);
			val++;
		} else {
			//CG_FillRect( 140 + localOffset, 256, 1, 8, color);
			CG_FillRect( 500 - localOffset, 256, 1, 8, color);			
		}
	}

	// the extremes
	// 30 degrees plus a 15 degree border
	angleMin = AngleNormalize360(360 - (cg.pmext.mountedWeaponAngles[YAW] - 90.f) - (30.f + 15.f));
	angleMax = AngleNormalize360(360 - (cg.pmext.mountedWeaponAngles[YAW] - 90.f) + (30.f + 15.f));

	// right
	localOffset = (AngleNormalize360(angle - angleMin) / 5.f ) * 10.f;
	//CG_FillRect( 320 + localOffset, 252, 2, 18, color_extends);
	CG_FillRect( 320 - localOffset, 252, 2, 18, color_extends);

	// left
	localOffset = (AngleNormalize360(angleMax - angle) / 5.f ) * 10.f;
	//CG_FillRect( 320 - localOffset, 252, 2, 18, color_extends);
	CG_FillRect( 320 + localOffset, 252, 2, 18, color_extends);

	// last fire pos
	fadeTime = 0;
	if( cg.lastFiredWeapon == 60 && cg.mortarImpactTime >= -1 ) {
		fadeTime = cg.time - (cg.predictedPlayerEntity.muzzleFlashTime + 5000);

		if( fadeTime < 3000 ) {
			float lastfireAngle;

			if( fadeTime > 0 ) {
				color_lastfire[3] = 1.f - (fadeTime/3000.f);
			}

			lastfireAngle = AngleNormalize360(360 - (cg.mortarFireAngles[YAW] - 90.f));

			localOffset = ( ( AngleSubtract( angle, lastfireAngle ) ) / 5.f ) * 10.f;
			//CG_FillRect( 320 + localOffset, 252, 2, 18, color_lastfire);
			CG_FillRect( 320 - localOffset, 252, 2, 18, color_lastfire);
		}
	}

	// mortar attack requests
	hasRightTarget = hasLeftTarget = qfalse;
	for( i = 0; i < MAX_CLIENTS; i++ ) {
		int requestFadeTime = cg.time - (cg.artilleryRequestTime[i] + 25000);

		if( requestFadeTime < 5000 ) {
			vec3_t dir;
			float yaw;
			float attackRequestAngle;

			VectorSubtract( cg.artilleryRequestPos[i], cg.predictedPlayerEntity.lerpOrigin, dir );

			// ripped this out of vectoangles
			if( dir[1] == 0 && dir[0] == 0 ) {
				yaw = 0;
			} else {
				if( dir[0] ) {
					yaw = ( atan2 ( dir[1], dir[0] ) * 180 / M_PI );
				}
				else if ( dir[1] > 0 ) {
					yaw = 90;
				}
				else {
					yaw = 270;
				}
				if ( yaw < 0 ) {
					yaw += 360;
				}
			}

			if( requestFadeTime > 0 ) {
				color_firerequest[3] = 1.f - (requestFadeTime/5000.f);
			}

			attackRequestAngle = AngleNormalize360(360 - (yaw - 90.f));

			yaw = AngleSubtract( attackRequestAngle, angleMin );

			if( yaw < 0 ) {
				if( !hasLeftTarget ) {
					//CG_FillRect( 136 + 2, 236 + 38 - 6, 4, 4, color_firerequest );

					trap_R_SetColor( color_firerequest );
					CG_DrawPic( 136 + 2, 236 + 38 - 10 + 1, 8, 8, cgs.media.ccMortarTargetArrow );
					trap_R_SetColor( NULL );

					hasLeftTarget = qtrue;
				}
			} else if( yaw > 90 ) {
				if( !hasRightTarget ) {
					//CG_FillRect( 350 + 154 - 6, 236 + 38 - 6, 4, 4, color_firerequest );

					trap_R_SetColor( color_firerequest );
					CG_DrawPic( 350 + 154 - 10, 236 + 38 - 10 + 1, -8, 8, cgs.media.ccMortarTargetArrow );
					trap_R_SetColor( NULL );

					hasRightTarget = qtrue;
				}
			} else {
				localOffset = ( ( AngleSubtract( angle, attackRequestAngle ) ) / 5.f ) * 10.f;
				//CG_FillRect( 320 + localOffset - 3, 264 - 3, 6, 6, color_firerequest );

				trap_R_SetColor( color_firerequest );
 				//CG_DrawPic( 320 + localOffset - 8, 264 - 8, 16, 16, cgs.media.ccMortarTarget );
				CG_DrawPic( 320 - localOffset - 8, 264 - 8, 16, 16, cgs.media.ccMortarTarget );
				trap_R_SetColor( NULL );
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
	CG_FillRect( 295, 164, 1, 200, color);	// left
	CG_FillRect( 345, 164, 1, 200, color);	// right

	// 10 units - 2.5 degrees
	// total of 200 units
	// 200 / 10 = 20 bits, means 20 * 2.5 = 50 degrees
	// that means left is cg.predictedPlayerState.viewangles[PITCH] - .5f * 50
	angle = AngleNormalize180(360 - (cg.predictedPlayerState.viewangles[PITCH] - 60));

	offset = (2.5f / 65536) * ((int)(angle * (65536 / 2.5f)) & 65535);
	min = floor((angle + .5f * 50) / 10.f) * 10;
	majorOffset = (int)(floor((int)((angle + .5f * 50) * 10.f) % 100) / 25.f );

	for( val = i = 0; i < 20; i++ ) {
		localOffset = i * 10.f + (offset * 4.f);

		/*if( localOffset >= 150 && localOffset <= 210 ) {
			if( i % 3 == majorOffset)
				val++;
			continue;
		}*/

		if( i % 4 == majorOffset ) {
			printval = min - val * 10;
			
			// rain - old tertiary abuse was nasty and had undefined result
			if (printval <= -180)
				printval += 360;
			else if (printval >= 180)
				printval -= 180;

			s = va( "%i", printval );
			CG_Text_Paint_Ext( 320 - .5f * CG_Text_Width_Ext( s, .15f, 0, &cgs.media.limboFont1 ), 164 + localOffset + .5f * CG_Text_Height_Ext( s, .15f, 0, &cgs.media.limboFont1 ), .15f, .15f, color, s, 0, 0, 0, &cgs.media.limboFont1 );
			CG_FillRect( 295 + 1, 164 + localOffset, 12, 1, color);
			CG_FillRect( 345 - 12, 164 + localOffset, 12, 1, color);
			val++;
		} else {
			CG_FillRect( 295 + 1, 164 + localOffset, 8, 1, color);
			CG_FillRect( 345 - 8, 164 + localOffset, 8, 1, color);
		}
	}

	// the extremes
	// 30 degrees up
	// 20 degrees down
	angleMin = AngleNormalize180(360 - (cg.pmext.mountedWeaponAngles[PITCH] - 60)) - 20.f;
	angleMax = AngleNormalize180(360 - (cg.pmext.mountedWeaponAngles[PITCH] - 60)) + 30.f;

	// top
	localOffset = angleMax - angle;
	if( localOffset < 0 )
		localOffset = 0;
	localOffset = (AngleNormalize360(localOffset) / 2.5f ) * 10.f;
	if( localOffset < 100 ) {
		CG_FillRect( 295 - 2, 264 - localOffset, 6, 2, color_extends);
		CG_FillRect( 345 - 4 + 1, 264 - localOffset, 6, 2, color_extends);
	}

	// bottom
	localOffset = angle - angleMin;
	if( localOffset < 0 )
		localOffset = 0;
	localOffset = (AngleNormalize360(localOffset) / 2.5f ) * 10.f;
	if( localOffset < 100 ) {
		CG_FillRect( 295 - 2, 264 + localOffset, 6, 2, color_extends);
		CG_FillRect( 345 - 4 + 1, 264 + localOffset, 6, 2, color_extends);
	}

	// last fire pos
	if( cg.lastFiredWeapon == 60 && cg.mortarImpactTime >= -1 ) {
		if( fadeTime < 3000 ) {
			float lastfireAngle;

			lastfireAngle = AngleNormalize180(360 - (cg.mortarFireAngles[PITCH] - 60));

			if( lastfireAngle > angle ) {
				localOffset = lastfireAngle - angle;
				if( localOffset < 0 )
					localOffset = 0;
				localOffset = (AngleNormalize360(localOffset) / 2.5f ) * 10.f;
				if( localOffset < 100 ) {
					CG_FillRect( 295 - 2, 264 - localOffset, 6, 2, color_lastfire);
					CG_FillRect( 345 - 4 + 1, 264 - localOffset, 6, 2, color_lastfire);
				}
			} else {
				localOffset = angle - lastfireAngle;
				if( localOffset < 0 )
					localOffset = 0;
				localOffset = (AngleNormalize360(localOffset) / 2.5f ) * 10.f;
				if( localOffset < 100 ) {
					CG_FillRect( 295 - 2, 264 + localOffset, 6, 2, color_lastfire);
					CG_FillRect( 345 - 4 + 1, 264 + localOffset, 6, 2, color_lastfire);
				}
			}
		}
	}
 
	/*s = va( "%.2f (%i / %i)", angle + .5f * 50, majorOffset, min );
	CG_Text_Paint( 348, 164, .25f, color, s, 0, 0, 0 );
	s = va( "%.2f",angle );
	CG_Text_Paint( 348, 264, .25f, color, s, 0, 0, 0 );
	s = va( "%.2f", angle - .5f * 50 );
	CG_Text_Paint( 348, 364, .25f, color, s, 0, 0, 0 );*/
}

#endif

/*
==============
CG_DrawBinocReticle
==============
*/
static void CG_DrawBinocReticle(void) {
	// an alternative.  This gives nice sharp lines at the expense of a few extra polys
	vec4_t	color;
	color[0] = color[1] = color[2] = 0;
	color[3] = 1;

	if(cgs.media.binocShaderSimple)
		CG_DrawPic( 0, 0, 640, 480, cgs.media.binocShaderSimple );

	CG_FillRect (146, 239, 348, 1, color);

	CG_FillRect (188, 234, 1, 13, color);	// ll
	CG_FillRect (234, 226, 1, 29, color);	// l
	CG_FillRect (274, 234, 1, 13, color);	// lr
	CG_FillRect (320, 213, 1, 55, color);	// center
	CG_FillRect (360, 234, 1, 13, color);	// rl
	CG_FillRect (406, 226, 1, 29, color);	// r
	CG_FillRect (452, 234, 1, 13, color);	// rr
}

void CG_FinishWeaponChange(int lastweap, int newweap); // JPW NERVE


/*
=================
CG_DrawCrosshair
=================
*/
static void CG_DrawCrosshair(void) {
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	float		w, h;
	qhandle_t	hShader;
	float		f;
	float		x, y;
	int			weapnum;		// DHM - Nerve

	if ( cg.renderingThirdPerson ) {
		return;
	}

	// using binoculars
	if(cg.zoomedBinoc) {
		CG_DrawBinocReticle();
		return;
	}

	// DHM - Nerve :: show reticle in limbo and spectator
	if ( (cg.snap->ps.pm_flags & PMF_FOLLOW) || cg.demoPlayback )
		weapnum = cg.snap->ps.weapon;
	else
		weapnum = cg.predictedPlayerState.weapon;


	switch(weapnum) {

		// weapons that get no reticle
		case WP_NONE:	// no weapon, no crosshair
			if(cg.zoomedBinoc)
				CG_DrawBinocReticle();

			if ( cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR )
				return;
			break;

		// special reticle for weapon
		case 59:
		case 57:
		case 58:
			if(!BG_PlayerMounted(cg.snap->ps.eFlags)) {
				// JPW NERVE -- don't let players run with rifles -- speed 80 == crouch, 128 == walk, 256 == run
					if (VectorLengthSquared(cg.snap->ps.velocity) > SQR(127)) {
						if( cg.snap->ps.weapon == 59 ) {
							CG_FinishWeaponChange( 59, 33 );
						}
						if( cg.snap->ps.weapon == 57 ) {
							CG_FinishWeaponChange( 57, 25 );
						}
						if( cg.snap->ps.weapon == 58 ) {
							CG_FinishWeaponChange( 58, 32 );
						}
					}
				
				// OSP
				if(cg.mvTotalClients < 1 || cg.snap->ps.stats[STAT_HEALTH] > 0)
					CG_DrawWeapReticle();

				return;
			}
			break;
		default:
			if(weapnum > 0 && weapnum < TCE_MAX_WEAPONS &&
			   weaponDef[weapnum].scoped > 1.0f && cg.tceAimActive &&
			   cg.tceAimComplete && !cg.tceScopeBlocked) {
				CG_DrawWeapReticle();
				return;
			}
			break;
	}

	if( cg.predictedPlayerState.eFlags & EF_PRONE_MOVING ) {
		return;
	}

	// FIXME: spectators/chasing?
	if( cg.predictedPlayerState.weapon == 60 && cg.predictedPlayerState.weaponstate != WEAPON_RAISING ) {
		CG_DrawMortarReticle();
		return;
	}

	if ( cg_drawCrosshair.integer < 0 || !developer.integer )	// TC:E keeps ordinary crosshairs developer-only; scope reticles remain above.
		return;

	// no crosshair while leaning
	if( cg.snap->ps.leanf ) {
		return;
	}

	// TAT 1/10/2003 - Don't draw crosshair if have exit hintcursor
	if (cg.snap->ps.serverCursorHint >= HINT_EXIT && cg.snap->ps.serverCursorHint <= HINT_NOEXIT )
		return;

	// set color based on health
	if ( cg_crosshairHealth.integer ) {
		vec4_t		hcolor;

		CG_ColorForHealth( hcolor );
		trap_R_SetColor( hcolor );
	} else {
		trap_R_SetColor(cg.xhairColor);
	}

	w = h = cg_crosshairSize.value;

	/* TC spread state and per-weapon coefficients, not SDK aimSpreadScale. */
    f=0;
    if(!(cg.predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS]&4)) {
        f=(float)cg.predictedPlayerState.stats[12] * ((float)weaponDef[cg.predictedPlayerState.weapon].unknown_0f8[0]*0.001f)*0.001f
         +(float)cg.predictedPlayerState.stats[11] * ((float)weaponDef[cg.predictedPlayerState.weapon].unknown_0f8[3]*0.001f)*0.001f;
    }
    previous=tce_uiCoordinates;tce_uiCoordinates=qtrue;
	w *= ( 1 + f*2.0 );
	h *= ( 1 + f*2.0 );
	
	x = cg_crosshairX.integer + 426;
	y = cg_crosshairY.integer + 240;
	CG_AdjustFrom640( &x, &y, &w, &h );

	hShader = cgs.media.crosshairShader[ cg_drawCrosshair.integer % NUM_CROSSHAIRS ];

	trap_R_DrawStretchPic( x - 0.5f*w, y - 0.5f*h, w, h, 0, 0, 1, 1, hShader );

	if ( cg.crosshairShaderAlt[ cg_drawCrosshair.integer % NUM_CROSSHAIRS ] ) {
		w = h = cg_crosshairSize.value;
		x = cg_crosshairX.integer + 426;
		y = cg_crosshairY.integer + 240;
		CG_AdjustFrom640( &x, &y, &w, &h );

		if(cg_crosshairHealth.integer == 0) {
			trap_R_SetColor(cg.xhairColorAlt);
		}

		trap_R_DrawStretchPic( x - 0.5f*w, y - 0.5f*h, w, h, 0, 0, 1, 1, cg.crosshairShaderAlt[ cg_drawCrosshair.integer % NUM_CROSSHAIRS ] );
	}
	tce_uiCoordinates=previous;
}

static void CG_DrawNoShootIcon( void ) {
	float x, y, w, h;
	float *color;

	/* TC protocol weapon65; SDK WP_PANZERFAUST is weapon5. */
	if( cg.predictedPlayerState.eFlags & EF_PRONE && cg.snap->ps.weapon == 65 ) {
		trap_R_SetColor( colorRed );
	} else if ( cg.crosshairClientNoShoot 
				// xkan, 1/6/2003 - don't shoot friend or civilian
				|| cg.snap->ps.serverCursorHint == HINT_PLYR_NEUTRAL
				|| cg.snap->ps.serverCursorHint == HINT_PLYR_FRIEND) {
		color = CG_FadeColor( cg.crosshairClientTime, 1000 );

		if ( !color ) {
			trap_R_SetColor( NULL );
			return;
		} else {
			trap_R_SetColor( color );
		}
	} else {
		return;
	}

	w = h = 48.f;

	x = cg_crosshairX.integer + 1;
	y = cg_crosshairY.integer + 1;
	CG_AdjustFrom640( &x, &y, &w, &h );

	// FIXME precache
	trap_R_DrawStretchPic( x + 0.5 * (cg.refdef_current->width - w), y + 0.5 * (cg.refdef_current->height - h), w, h, 0, 0, 1, 1, cgs.media.friendShader );
}

/*
=================
CG_ScanForCrosshairEntity
=================

Returns the distance to the entity

*/
static float CG_ScanForCrosshairEntity( float * zChange, qboolean * hitClient ) {
	trace_t		trace;
//	gentity_t	*traceEnt;
	vec3_t		start, end;
	float		dist;
	centity_t*	cent;

	// We haven't hit a client yet
	*hitClient = qfalse;

	VectorCopy( cg.refdef.vieworg, start );
	VectorMA( start, 8192, cg.refdef.viewaxis[0], end );	//----(SA)	changed from 8192

	cg.crosshairClientNoShoot = qfalse;

	CG_Trace( &trace, start, NULL, NULL, end, cg.snap->ps.clientNum, CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_ITEM );

	// How far from start to end of trace?
	dist = VectorDistance( start, trace.endpos );

	// How far up or down are we looking?
	*zChange = trace.endpos[2] - start[2];

	if ( trace.entityNum >= MAX_CLIENTS ) {
		if( cg_entities[trace.entityNum].currentState.eFlags & EF_TAGCONNECT ) {
			trace.entityNum = cg_entities[trace.entityNum].tagParent;
		}

		// is a tank with a healthbar
		// this might have some side-effects, but none right now as the script_mover is the only one that sets effect1Time
		if( ( cg_entities[trace.entityNum].currentState.eType == ET_MOVER && cg_entities[trace.entityNum].currentState.effect1Time ) ||
			cg_entities[trace.entityNum].currentState.eType == ET_CONSTRUCTIBLE_MARKER ) {
			// update the fade timer
			cg.crosshairClientNum = trace.entityNum;
			cg.crosshairClientTime = cg.time;
			cg.identifyClientRequest = cg.crosshairClientNum;
		}

		// Default: We're not looking at a client
		cg.crosshairNotLookingAtClient = qtrue;
		
		return dist;
	}

//	traceEnt = &g_entities[trace.entityNum];

	if (CG_PointContents(trace.endpos, 0) & CONTENTS_FOG) {
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
		cg.tceCrosshairTargetTime = cg.time;
	cg.crosshairClientNum = trace.entityNum;
	cg.crosshairClientTime = cg.time;
	if ( cg.crosshairClientNum != cg.snap->ps.identifyClient && cg.crosshairClientNum != ENTITYNUM_WORLD ) {
		cg.identifyClientRequest = cg.crosshairClientNum;
	}

	cent = &cg_entities[cg.crosshairClientNum];

	if( cent && cent->currentState.powerups & (1 << PW_OPS_DISGUISED) ) {
		if(cgs.clientinfo[cg.crosshairClientNum].team == cgs.clientinfo[cg.clientNum].team) {
			cg.crosshairClientNoShoot = qtrue;
		}
	}

	return dist;
}



#define CH_KNIFE_DIST		48	// from g_weapon.c
#define CH_LADDER_DIST		100
#define CH_WATER_DIST		100
#define CH_BREAKABLE_DIST	64
#define CH_DOOR_DIST		96

#define CH_DIST				100 //128		// use the largest value from above

/*
==============
CG_CheckForCursorHints
	concept in progress...
==============
*/
void CG_CheckForCursorHints( void ) {
    trace_t trace;
    vec3_t start, end;
    centity_t *hit;
    if (cg.renderingThirdPerson) return;
    if (cg.snap->ps.serverCursorHint) {
        cg.cursorHintTime = cg.time;
        cg.cursorHintFade = 500;
        cg.cursorHintIcon = cg.snap->ps.serverCursorHint;
        cg.cursorHintValue = cg.snap->ps.serverCursorHintVal;
        return;
    }
    VectorCopy(cg.refdef_current->vieworg, start);
    VectorMA(start, 100.f, cg.refdef_current->viewaxis[0], end);
    CG_Trace(&trace, start, vec3_origin, vec3_origin, end,
             cg.snap->ps.clientNum, MASK_PLAYERSOLID);
    if (trace.fraction == 1.f) return;
    hit = &cg_entities[trace.entityNum];
    if (trace.entityNum >= MAX_CLIENTS &&
        (hit->currentState.powerups == STATE_INVISIBLE ||
         hit->currentState.powerups == STATE_UNDERCONSTRUCTION)) return;
    /* TC has no client-predicted backstab/knife cursor branch. */
    if (trace.entityNum == ENTITYNUM_WORLD &&
        (trace.surfaceFlags & SURF_LADDER) && !(cg.snap->ps.pm_flags & PMF_LADDER) &&
        trace.fraction * 100.f <= 100.f) {
        cg.cursorHintIcon = HINT_LADDER;
        cg.cursorHintTime = cg.time;
        cg.cursorHintFade = 500;
        cg.cursorHintValue = 0;
    }
}



/*
=====================
CG_DrawCrosshairNames
=====================
*/
static void TCE_DrawCrosshairNamesContents(void) {
    float *color, dist, zChange, w;
    qboolean hitClient = qfalse, isTank = qfalse, disguised = qfalse;
    const char *text, *letter, *rank;
    int target, viewer;
    if (cg_drawCrosshair.integer < 0) return;
    dist = CG_ScanForCrosshairEntity(&zChange, &hitClient);
    if (cg.renderingThirdPerson) return;
    color = CG_FadeColor(cg.crosshairClientTime, 1000);
    if (!color) { trap_R_SetColor(NULL); return; }
    target = cg.crosshairClientNum;
    viewer = cg.snap->ps.clientNum;
    if (target > MAX_CLIENTS) {
        if (!cg_drawCrosshairNames.integer || cgs.clientinfo[viewer].team == TEAM_SPECTATOR) return;
        if (cg_entities[target].currentState.eType == ET_MOVER && cg_entities[target].currentState.effect1Time) {
            isTank = qtrue;
            text = Info_ValueForKey(CG_ConfigString(CS_SCRIPT_MOVER_NAMES), va("%i", target));
            if (!*text) return;
            w = CG_DrawStrlen(text) * SMALLCHAR_WIDTH;
            CG_DrawSmallStringColor((int)(426 - w * .5f), 170, text, color);
        } else {
            if (cg_entities[target].currentState.eType != ET_CONSTRUCTIBLE_MARKER) return;
            text = Info_ValueForKey(CG_ConfigString(CS_CONSTRUCTION_NAMES), va("%i", target));
            if (!*text) return;
            w = CG_DrawStrlen(text) * SMALLCHAR_WIDTH;
            CG_DrawSmallStringColor((int)(426 - w * .5f), 170, text, color);
            return;
        }
    } else if (cgs.clientinfo[target].team != cgs.clientinfo[viewer].team) {
        if (!(cg_entities[target].currentState.powerups & (1 << PW_OPS_DISGUISED)) ||
            cgs.clientinfo[viewer].team == TEAM_SPECTATOR) return;
        if (cgs.clientinfo[viewer].skill[SK_SIGNALS] >= 4 && cgs.clientinfo[viewer].cls == PC_FIELDOPS) {
            text = CG_TranslateString("Disguised Enemy!");
            w = CG_DrawStrlen(text) * SMALLCHAR_WIDTH;
            CG_DrawSmallStringColor((int)(426 - w * .5f), 170, text, color);
            return;
        }
        if (dist <= 512 || !cg_drawCrosshairNames.integer) return;
        disguised = qtrue;
        letter = BG_ClassLetterForNumber((cg_entities[target].currentState.powerups >> PW_OPS_CLASS_1) & 6);
        rank = cgs.clientinfo[target].team == TEAM_AXIS ?
            rankNames_Allies[cgs.clientinfo[target].disguiseRank] : rankNames_Axis[cgs.clientinfo[target].disguiseRank];
        text = va("[%s] %s %s", CG_TranslateString(letter), rank, cgs.clientinfo[target].disguiseName);
        w = CG_DrawStrlen(text) * SMALLCHAR_WIDTH;
        CG_DrawSmallStringColor((int)(426 - w * .5f), 170, text, color);
    }
    if (!cg_drawCrosshairNames.integer || isTank) return;
    if (cgs.clientinfo[viewer].team == TEAM_SPECTATOR ||
        cgs.clientinfo[target].team == cgs.clientinfo[viewer].team ||
        cg.time - cg.tceCrosshairTargetTime > 1500) {
        BG_ClassLetterForNumber(cg_entities[target].currentState.teamNum);
        text = va(cgs.clientinfo[target].team == cgs.clientinfo[viewer].team ? "%s" : "Enemy: %s",
                  cgs.clientinfo[target].name);
        color[0] = cgs.clientinfo[target].team == TEAM_ALLIES ? 0 : .7f;
        color[1] = 0;
        color[2] = cgs.clientinfo[target].team == TEAM_ALLIES ? .7f : 0;
        w = CG_Text_Width_Ext(text, .2f, 0, &cgs.media.limboFont1);
        CG_Text_Paint_Ext(426 - (int)w * .5f, 378, .2f, .2f, color, text, 0, 0, 3, &cgs.media.limboFont1);
        trap_R_SetColor(NULL);
    } else if (disguised) {
        trap_R_SetColor(NULL);
    }
}

static void CG_DrawCrosshairNames(void) {
    extern qboolean tce_uiCoordinates;
    qboolean previous = tce_uiCoordinates;
    tce_uiCoordinates = qtrue;
    TCE_DrawCrosshairNamesContents();
    tce_uiCoordinates = previous;
}





//==============================================================================

/*
=================
CG_DrawSpectator
=================
*/
static void CG_DrawSpectator(void) {
    extern qboolean tce_uiCoordinates;
    qboolean previous = tce_uiCoordinates;
    tce_uiCoordinates = qtrue;
    CG_Text_Paint_Ext(8,118,.25f,.25f,colorWhite,CG_TranslateString("Spectating"),0,0,3,&cgs.media.limboFont1);
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
void CG_DrawIntermission( void )
{
	// End-of-level autoactions
	if(!cg.demoPlayback) {
		static int doScreenshot = 0, doDemostop = 0;

		if(!cg.latchAutoActions) {
			cg.latchAutoActions = qtrue;

			if(cg_autoAction.integer & AA_SCREENSHOT) {
				doScreenshot = cg.time + 1000;
			}

			if(cg_autoAction.integer & AA_STATSDUMP) {
				CG_dumpStats_f();
			}

			if((cg_autoAction.integer & AA_DEMORECORD) &&
			  ((cgs.gametype == GT_WOLF_STOPWATCH && cgs.currentRound == 0) ||
			    cgs.gametype != GT_WOLF_STOPWATCH))
			{
				doDemostop = cg.time + 5000;	// stats should show up within 5 seconds
			}
		}

		if(doScreenshot > 0 && doScreenshot < cg.time) {
			CG_autoScreenShot_f();
			doScreenshot = 0;
		}

		if(doDemostop > 0 && doDemostop < cg.time) {
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
static void CG_ActivateLimboMenu(void) {
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
static void CG_DrawSpectatorMessage( void ) {
	const char *str, *str2;
	extern qboolean tce_uiCoordinates;
    qboolean previous;
	static int lastconfigGet = 0;

	if ( !cg_descriptiveText.integer )
		return;

	if ( !( cg.snap->ps.pm_flags & PMF_LIMBO || cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) )
		return;

	if(cg.time - lastconfigGet > 1000) {
		Controls_GetConfig();

		lastconfigGet = cg.time;
	}

    previous = tce_uiCoordinates;
    tce_uiCoordinates = qtrue;

	str2 = BindingFromName( "openlimbomenu" );
	if ( !Q_stricmp( str2, "(openlimbomenu)" ) ) {
		str2 = "ESCAPE";
	}
	str = va( CG_TranslateString( "Press %s to open Deploy Menu" ), str2 );
	CG_Text_Paint_Ext(8,154,.2f,.2f,colorWhite,str,0,0,3,&cgs.media.limboFont1);

	str2 = BindingFromName( "+attack" );
	str = va( CG_TranslateString( "Press %s to follow next player" ), str2 );
	CG_Text_Paint_Ext(8,168,.2f,.2f,colorWhite,str,0,0,3,&cgs.media.limboFont1);

    tce_uiCoordinates = previous;
}


static int CG_ReinforcementMilliseconds( qboolean menu ) {
	team_t team; 
	int dwDeployTime; 

	if( menu ) {
		if(cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) {
			team = cgs.ccSelectedTeam == 0 ? TEAM_AXIS : TEAM_ALLIES;
		} else {
			team = cgs.clientinfo[cg.clientNum].team;
		}
	} else {
		team = cgs.clientinfo[cg.snap->ps.clientNum].team;
	}

	dwDeployTime = (team == TEAM_AXIS) ? cg_redlimbotime.integer : cg_bluelimbotime.integer;
	return dwDeployTime - ((cgs.aReinfOffset[team] + cg.time - cgs.levelStartTime) % dwDeployTime);
}

#if defined(_MSC_VER) && defined(_M_IX86)
static const float cg_reinfMillisecondsScale = 0.001f;
static const float cg_reinfSecondsBias = 1.0f;

/* Original30021390 returns the unspilled x87 value in ST0. */
__declspec(naked) float CG_CalculateReinfTime_Float( qboolean menu ) {
	__asm {
		push dword ptr [esp + 4]
		call CG_ReinforcementMilliseconds
		add esp, 4
		push eax
		fild dword ptr [esp]
		add esp, 4
		fmul dword ptr [cg_reinfMillisecondsScale]
		fadd dword ptr [cg_reinfSecondsBias]
		ret
	}
}

/* Consume ST0 directly, with the truncation mode used by original __ftol. */
__declspec(naked) int CG_CalculateReinfTime( qboolean menu ) {
	__asm {
		push dword ptr [esp + 4]
		call CG_CalculateReinfTime_Float
		add esp, 4
		sub esp, 12
		fstcw word ptr [esp + 8]
		fwait
		mov ax, word ptr [esp + 8]
		or ax, 0c00h
		mov word ptr [esp + 10], ax
		fldcw word ptr [esp + 10]
		fistp qword ptr [esp]
		fldcw word ptr [esp + 8]
		mov eax, dword ptr [esp]
		mov edx, dword ptr [esp + 4]
		add esp, 12
		ret
	}
}
#else
float CG_CalculateReinfTime_Float( qboolean menu ) {
	return 1.0f + CG_ReinforcementMilliseconds( menu ) * 0.001f;
}

int CG_CalculateReinfTime( qboolean menu ) {
	return((int)CG_CalculateReinfTime_Float( menu ));
}
#endif


/*
=================
CG_DrawLimboMessage
=================
*/

#define INFOTEXT_STARTX	8

/* Original TC:E message functions share the proportional ArialBlack font. */
static const char *CG_MessageTranslate(const char *s) { return CG_TranslateString(s); }
static const char *CG_MessageBinding(const char *s) { return BindingFromName(s); }
static int CG_MessageWidth(const char *s,float scale,int limit,void *font) {
 return CG_Text_Width_Ext(s,scale,limit,(fontInfo_t *)font);
}
static void CG_MessagePaint(float x,float y,float sx,float sy,const float *color,const char *text,float adjust,int limit,int style,void *font) {
 CG_Text_Paint_Ext(x,y,sx,sy,(float *)color,text,adjust,limit,style,(fontInfo_t *)font);
}
static void CG_MessageSmall(int x,int y,const char *s,const float *color) { CG_DrawSmallStringColor(x,y,s,(float *)color); }
static int CG_MessageViewing(void) { return CG_ViewingDraw(); }
static int CG_MessageReinf(int menu) { return CG_CalculateReinfTime(menu); }
static void CG_MessageState(tce_hud_messages_t *s,tce_hud_messages_api_t *a) {
 playerState_t *ps=&cg.snap->ps;
 memset(s,0,sizeof(*s));memset(a,0,sizeof(*a));
 s->health=ps->stats[STAT_HEALTH];s->pmFlags=ps->pm_flags;
 s->localTeam=cgs.clientinfo[cg.clientNum].team;s->descriptive=cg_descriptiveText.integer;
 s->gameType=cgs.gametype;s->respawns=ps->persistant[PERS_RESPAWNS_LEFT];s->penalty=ps->persistant[PERS_RESPAWNS_PENALTY];
 s->client=ps->clientNum;s->localClient=cg.clientNum;s->followTeam=cgs.clientinfo[ps->clientNum].team;
 s->redTime=cg_redlimbotime.integer;s->blueTime=cg_bluelimbotime.integer;s->name=cgs.clientinfo[ps->clientNum].name;
 s->objectiveTime=cg.oidPrintTime;s->objectiveLines=cg.oidPrintLines;s->objectiveCharWidth=cg.oidPrintCharWidth;s->objective=cg.oidPrint;
 a->translate=CG_MessageTranslate;a->binding=CG_MessageBinding;a->width=CG_MessageWidth;a->paint=CG_MessagePaint;
 a->small=CG_MessageSmall;a->setColor=trap_R_SetColor;a->fade=CG_FadeColor;a->viewing=CG_MessageViewing;a->reinf=CG_MessageReinf;a->font=&cgs.media.limboFont1;
}
static void CG_DrawLimboMessage(void) {
 extern qboolean tce_uiCoordinates;qboolean previous=tce_uiCoordinates;
 tce_hud_messages_t s;tce_hud_messages_api_t a;CG_MessageState(&s,&a);
 tce_uiCoordinates=qtrue;TCE_DrawLimboMessage(&s,&a);tce_uiCoordinates=previous;
}
static qboolean CG_DrawFollow(void) {
 extern qboolean tce_uiCoordinates;qboolean previous=tce_uiCoordinates;int result;
 tce_hud_messages_t s;tce_hud_messages_api_t a;CG_MessageState(&s,&a);
 tce_uiCoordinates=qtrue;result=TCE_DrawFollow(&s,&a);tce_uiCoordinates=previous;return result;
}

/*
=================
CG_DrawWarmup
=================
*/
static const char *CG_WarmupTranslate(const char *text) { return CG_TranslateString(text); }
static const char *CG_WarmupBinding(const char *name) { return BindingFromName(name); }
static int CG_WarmupWidth(const char *text,float scale,int limit,void *font) {
 return CG_Text_Width_Ext(text,scale,limit,(fontInfo_t *)font);
}
static void CG_WarmupPaint(float x,float y,float sx,float sy,const float *color,const char *text,float adjust,int limit,int style,void *font) {
 CG_Text_Paint_Ext(x,y,sx,sy,(float *)color,text,adjust,limit,style,(fontInfo_t *)font);
}
static void CG_DrawWarmup(void) {
 extern qboolean tce_uiCoordinates;
 tce_warmup_t state;
 tce_warmup_api_t api;
 qboolean previous=tce_uiCoordinates;
 state.warmup=cg.warmup;state.time=cg.time;state.gameState=cgs.gamestate;
 state.minClients=cgs.minclients;state.demo=cg.demoPlayback;
 state.team=cg.snap->ps.persistant[PERS_TEAM];state.pmFlags=cg.snap->ps.pm_flags;
 state.gameType=cgs.gametype;state.round=cgs.currentRound;
 state.defender=cgs.gametype==3?atoi(Info_ValueForKey(CG_ConfigString(CS_MULTI_INFO),"defender")):0;
 state.count=cg.warmupCount;
 api.translate=CG_WarmupTranslate;api.binding=CG_WarmupBinding;
 api.width=CG_WarmupWidth;api.paint=CG_WarmupPaint;api.sound=trap_S_StartLocalSound;
 api.font=&cgs.media.limboFont1;api.beep=trap_S_RegisterSound("sound/feedback/count_beep.wav",qfalse);
 tce_uiCoordinates=qtrue;TCE_DrawWarmup(&state,&api);tce_uiCoordinates=previous;
 cg.warmupCount=state.count;
}

//==================================================================================

/*
=================
CG_DrawFlashFade
=================
*/
/* Complete Windows30027540 RGB consumer, fed by TC CG_Corona. */
static void CG_DrawCoronaBlend(void) {
    extern qboolean tce_uiCoordinates;
    vec4_t color; qboolean prior=tce_uiCoordinates;
    if(!cg.snap || cg.renderingThirdPerson || cg.tceCoronaBlendAlpha==0)return;
    VectorCopy(cg.tceCoronaBlendColor,color);color[3]=cg.tceCoronaBlendAlpha;
    if(color[3]<0)color[3]=0;else if(color[3]>1)color[3]=1;
    tce_uiCoordinates=qtrue;CG_FillRect(-5,-85,862,650,color);tce_uiCoordinates=prior;
}

static void CG_DrawFlashFade( void ) {
	extern qboolean tce_uiCoordinates;
	qboolean previousCoordinates;
	static int lastTime;
	int elapsed, time;
	vec4_t col;
	qboolean fBlackout = (!CG_IsSinglePlayer() && int_ui_blackout.integer > 0);

	if (cgs.fadeStartTime + cgs.fadeDuration < cg.time) {
		cgs.fadeAlphaCurrent = cgs.fadeAlpha;
	} else if (cgs.fadeAlphaCurrent != cgs.fadeAlpha) {
		elapsed = (time = trap_Milliseconds()) - lastTime;	// we need to use trap_Milliseconds() here since the cg.time gets modified upon reloading
		lastTime = time;
		if (elapsed < 500 && elapsed > 0) {
			if (cgs.fadeAlphaCurrent > cgs.fadeAlpha) {
				cgs.fadeAlphaCurrent -= ((float)elapsed/(float)cgs.fadeDuration);
				if (cgs.fadeAlphaCurrent < cgs.fadeAlpha)
					cgs.fadeAlphaCurrent = cgs.fadeAlpha;
			} else {
				cgs.fadeAlphaCurrent += ((float)elapsed/(float)cgs.fadeDuration);
				if (cgs.fadeAlphaCurrent > cgs.fadeAlpha)
					cgs.fadeAlphaCurrent = cgs.fadeAlpha;
			}
		}
	}

	// OSP - ugh, have to inform the ui that we need to remain blacked out (or not)
	if(int_ui_blackout.integer == 0) {
		if(cg.mvTotalClients < 1 && cg.snap->ps.powerups[PW_BLACKOUT] > 0) {
			trap_Cvar_Set("ui_blackout", va("%d", cg.snap->ps.powerups[PW_BLACKOUT]));
		}
	} else if(cg.snap->ps.powerups[PW_BLACKOUT] == 0 || cg.mvTotalClients > 0) {
		trap_Cvar_Set("ui_blackout", "0");
	}

	// now draw the fade
	if(cgs.fadeAlphaCurrent > 0.0 || fBlackout) {
		VectorClear( col );
		col[3] = (fBlackout) ? 1.0f : cgs.fadeAlphaCurrent;
//		CG_FillRect( -10, -10, 650, 490, col );
		/* TC Windows300271d0: the blackout covers the full virtual canvas. */
		previousCoordinates = tce_uiCoordinates;
		tce_uiCoordinates = qtrue;
		CG_FillRect( 0, -80, 852, 640, col );


		// OSP - Show who is speclocked
		if(fBlackout) {
			int i, nOffset = 90;
			char *str, *format = "The %s team is speclocked!";
			char *teams[TEAM_NUM_TEAMS] = { "??", "TERRORISTS", "SPECOPS", "???" };
			float color[4] = { 1, 1, 0, 1 };

			for(i=TEAM_AXIS; i<=TEAM_ALLIES; i++) {
				if(cg.snap->ps.powerups[PW_BLACKOUT] & i) {
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
static void CG_DrawFlashZoomTransition(void) {
    extern qboolean tce_uiCoordinates;
    qboolean previous;
    vec4_t color={0,0,0,0};
    int elapsed;
    if(!cg.snap)return;
    if(BG_PlayerMounted(cg.snap->ps.eFlags)){cg.zoomTime=cg.time;return;}
    if(cg.weaponSelect<0 || cg.weaponSelect>=TCE_MAX_WEAPONS ||
       weaponDef[cg.weaponSelect].scoped==0.0f || cg.renderingThirdPerson)return;
    elapsed=cg.time-cg.zoomTime;
    if((float)elapsed<400.0f) {
        color[3]=1.0f-(float)elapsed*0.0025f;
        previous=tce_uiCoordinates;tce_uiCoordinates=qtrue;
        CG_FillRect(-5,-85,862,650,color);
        tce_uiCoordinates=previous;
    }
}


/*
=================
CG_DrawFlashDamage
=================
*/
static void CG_DrawFlashDamage( void ) {
    extern qboolean tce_uiCoordinates;
    vec4_t col = { 0.2f, 0, 0, 0 };
    double redFlash, blood;
    qboolean previous;
    if (!cg.snap || !(cg.v_dmg_time > cg.time)) return;
    /* Windows30027989 keeps the lifetime product extended until final alpha. */
    redFlash = fabs(((double)cg.v_dmg_time - cg.time) * (double)0.002f * cg.v_dmg_pitch);
    if (redFlash > 5.0) redFlash = 5.0;
    blood = cg_bloodFlash.value > 1.0 ? 1.0 :
        cg_bloodFlash.value < 0.0 ? 0.0 : cg_bloodFlash.value;
    col[3] = (float)(((redFlash * 0.2) * blood) * 0.7);
    previous = tce_uiCoordinates;
    tce_uiCoordinates = qtrue;
    CG_FillRect(-5, -85, 862, 650, col);
    tce_uiCoordinates = previous;
}

/*
=================
CG_DrawFlashFire
=================
*/
static void CG_DrawFlashFire( void ) {
    extern qboolean tce_uiCoordinates;
	vec4_t		col={1,1,1,1};
	float alpha;
	double max, f;
	qboolean previous;

	if (!cg.snap)
		return;

	if ( cg.renderingThirdPerson ) {
		return;
	}

	if (!cg.snap->ps.onFireStart) {
		cg.v_noFireTime = cg.time;
		return;
	}

	alpha = (float)(((double)(cg.snap->ps.onFireStart - cg.time + 1000)) * (double)0.001f);
	if (alpha > 0) {
		if (alpha >= 1.0) {
			alpha = 1.0;
		}

		// fade in?
		f = (double)(cg.time - cg.v_noFireTime) * (double)0.001f;
		if (f >= 0.0 && f < 1.0)
			alpha = f;

		max = (sin((double)((cg.time/10)%1000) * 0.001) + 1.0) * 0.5;
		if (alpha > max)
			alpha = max;
		col[0] = alpha;
		col[1] = alpha;
		col[2] = alpha;
		col[3] = alpha;
		trap_R_SetColor( col );
		previous = tce_uiCoordinates;
		tce_uiCoordinates = qtrue;
		CG_DrawPic( -5, -5, 862, 490, cgs.media.viewFlashFire[(cg.time/50)%16] );
		tce_uiCoordinates = previous;
		trap_R_SetColor( NULL );

		trap_S_AddLoopingSound( cg.snap->ps.origin, vec3_origin, cgs.media.flameSound, (int)(255.0*alpha), 0 );
		trap_S_AddLoopingSound( cg.snap->ps.origin, vec3_origin, cgs.media.flameCrackSound, (int)(255.0*alpha), 0 );
	} else {
		cg.v_noFireTime = cg.time;
	}
}



/*
==============
CG_DrawFlashBlendBehindHUD
	screen flash stuff drawn first (on top of world, behind HUD)
==============
*/
static void CG_DrawFlashBlendBehindHUD(void) {
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
static void CG_DrawFlashBlend( void ) {
	// Gordon: no flash blends if in limbo or spectator, and in the limbo menu
	if( (cg.snap->ps.pm_flags & PMF_LIMBO || cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) && cg.showGameView ) {
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
#define OID_LEFT	10
#define OID_TOP		360

void CG_ObjectivePrint( const char *str, int charWidth ) {
	char	*s;
	int		i, len;						// NERVE - SMF
	qboolean neednewline = qfalse;		// NERVE - SMF

	if( cg.centerPrintTime ) {
		return;
	}

	s = CG_TranslateString( str );

	Q_strncpyz( cg.oidPrint, s, sizeof(cg.oidPrint) );

	// NERVE - SMF - turn spaces into newlines, if we've run over the linewidth
	len = strlen( cg.oidPrint );
	for ( i = 0; i < len; i++ ) {

		// NOTE: subtract a few chars here so long words still get displayed properly
		if ( i % ( CP_LINEWIDTH - 20 ) == 0 && i > 0 )
			neednewline = qtrue;
		if ( cg.oidPrint[i] == ' ' && neednewline ) {
			cg.oidPrint[i] = '\n';
			neednewline = qfalse;
		}
	}
	// -NERVE - SMF

	cg.oidPrintTime = cg.time;
	cg.oidPrintY = OID_TOP;
	cg.oidPrintCharWidth = charWidth;

	// count the number of lines for oiding
	cg.oidPrintLines = 1;
	s = cg.oidPrint;
	while( *s ) {
		if (*s == '\n')
			cg.oidPrintLines++;
		s++;
	}
}

static void CG_DrawObjectiveInfo(void) {
 extern qboolean tce_uiCoordinates;qboolean previous=tce_uiCoordinates;
 tce_hud_messages_t s;tce_hud_messages_api_t a;CG_MessageState(&s,&a);
 tce_uiCoordinates=qtrue;TCE_DrawObjectiveInfo(&s,&a);tce_uiCoordinates=previous;
 cg.oidPrintTime=s.objectiveTime;
}

void CG_DrawTimedMenus() {
	if (cg.voiceTime) {
		int t = cg.time - cg.voiceTime;
		if ( t > 2500 ) {
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
void CG_Fade( int r, int g, int b, int a, int time, int duration ) {
	/* TC Windows30021560/Linux000494c4 only updates the alpha scheme.
	 * RGB parameters belong to the API but are unused by the original body. */
	cgs.fadeAlpha = (float)a * (1.0f / 255.0f);
	cgs.fadeStartTime = time;
	cgs.fadeDuration = duration;

	if (cgs.fadeStartTime + cgs.fadeDuration <= cg.time) {
		cgs.fadeAlphaCurrent = cgs.fadeAlpha;
	}
}

/*
=================
CG_ScreenFade
=================
*/
static void CG_ScreenFade( void ) {
	int		msec;
	int		i;
	float	t, invt;
	vec4_t	color;

	if ( !cg.fadeRate ) {
		return;
	}

	msec = cg.fadeTime - cg.time;
	if ( msec <= 0 ) {
		cg.fadeColor1[ 0 ] = cg.fadeColor2[ 0 ];
		cg.fadeColor1[ 1 ] = cg.fadeColor2[ 1 ];
		cg.fadeColor1[ 2 ] = cg.fadeColor2[ 2 ];
		cg.fadeColor1[ 3 ] = cg.fadeColor2[ 3 ];

		if ( !cg.fadeColor1[ 3 ] ) {
			cg.fadeRate = 0;
			return;
		}

		CG_FillRect( 0, 0, 640, 480, cg.fadeColor1 );

	} else {
		t = ( float )msec * cg.fadeRate;
		invt = 1.0f - t;

		for( i = 0; i < 4; i++ ) {
			color[ i ] = cg.fadeColor1[ i ] * t + cg.fadeColor2[ i ] * invt;
		}

		if ( color[ 3 ] ) {
			CG_FillRect( 0, 0, 640, 480, color );
		}
	}
}

#if 0 // rain - unused
// JPW NERVE
void CG_Draw2D2(void) {
	qhandle_t weapon;

	trap_R_SetColor( NULL );

	CG_DrawPic( 0,480, 640, -70, cgs.media.hud1Shader );

	if(!BG_PlayerMounted(cg.snap->ps.eFlags) ) {
		switch (cg.snap->ps.weapon) {
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
		CG_DrawPic( 220,410, 200,-200,weapon);
	}
}
#endif

/*
=================
CG_DrawCompassIcon

NERVE - SMF
=================
*/
void CG_DrawCompassIcon( float x, float y, float w, float h, vec3_t origin, vec3_t dest, qhandle_t shader ) {
	float angle, pi2 = M_PI * 2;
	vec3_t v1, angles;
	float len;

	VectorCopy( dest, v1 );
	VectorSubtract( origin, v1, v1 );
	len = VectorLength( v1 );
	VectorNormalize( v1 );
	vectoangles( v1, angles );

	if ( v1[0] == 0 && v1[1] == 0 && v1[2] == 0 )
		return;

//	if( cg_drawCompass.integer == 2 )
//		angles[YAW] = AngleSubtract( 90, angles[YAW] );
//	else
		angles[YAW] = AngleSubtract( cg.predictedPlayerState.viewangles[YAW], angles[YAW] );

	angle = ( ( angles[YAW] + 180.f ) / 360.f - ( 0.50 / 2.f ) ) * pi2;


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
			w = sqrt( ( w * w ) + ( h * h ) ) / 3.f * 2.f * 0.9f;
		}

		x = x + ( cos( angle ) * w );
		y = y + ( sin( angle ) * w );

		len = 1 - min( 1.f, len / 2000.f );


		CG_DrawPic( x - (14 * len + 4)/2, y - (14 * len + 4)/2, 14 * len + 8, 14 * len + 8, shader );
#ifdef SQUARE_COMPASS
	} else {
		int iconWidth, iconHeight;
		// START Mad Doc - TDF
		// talk about fitting a square peg into a round hole...
		// we're now putting the compass icons around the square automap instead of the round compass

		while (angle < 0)
			angle += pi2;

		while (angle >= pi2)
			angle -= pi2;


		x = x + w/2;
		y = y + h/2;
		w /= 2;// = sqrt( ( w * w ) + ( h * h ) ) / 3.f * 2.f * 0.9f;

		if ( (angle >= 0) && (angle < M_PI/4.0))
		{
			x += w;
			y += w * tan(angle);

		}
		else if ( (angle >= M_PI/4.0) && (angle < 3.0 * M_PI / 4.0) )
		{
			x += w / tan(angle);
			y += w;
		}
		else if ( (angle >= 3.0 * M_PI / 4.0) && (angle < 5.0 * M_PI / 4.0) )
		{
			x -= w;
			y -= w * tan(angle);
		}
		else if ( (angle >= 5.0 * M_PI / 4.0) && (angle < 7.0 * M_PI / 4.0) )
		{
			x -= w / tan(angle);
			y -= w;
		}
		else
		{
			x += w;
			y += w * tan(angle);

		}

		len = 1 - min( 1.f, len / 2000.f );
		iconWidth = 14 * len + 4; // where did this calc. come from?
		iconHeight = 14 * len + 4; 

		// adjust so that icon is always outside of the map
		if ( (angle > 5.0*M_PI/4.0) && (angle < 2*M_PI) )
		{

			y -= iconHeight;
		} 

		if ((angle >= 3.0*M_PI/4.0) && (angle <= 5.0*M_PI/4.0) )
		{
			x -= iconWidth;
		}
	
		
		CG_DrawPic( x, y, iconWidth, iconHeight, shader );


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
/* Whole Windows300215a0 / Linux000496fc. origin-target is intentional. */
static void CG_DrawRadarIcon(int x,int y,int w,int h,const vec3_t origin,const vec3_t target,
                             qhandle_t shader,qhandle_t above,qhandle_t below) {
    vec3_t delta,angles;
    float distance,z;
    double radians,fraction,size;
    int radius,px,py;
    VectorSubtract(origin,target,delta); z=delta[2];
    distance=(float)sqrt((double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2]);
    VectorNormalize(delta);vectoangles(delta,angles);
    if(delta[0]==0 && delta[1]==0 && delta[2]==0)return;
    /* Original x87 retains these intermediates until the draw arguments. */
    radians=(((double)AngleSubtract(cg.predictedPlayerState.viewangles[YAW],angles[YAW])+180.f)*.0027777778450399637f-.25)*6.2831854820251465;
    fraction=(double)distance*.0005f;if(fraction>1)fraction=1;
    radius=(int)((w/2)*fraction);
    px=(int)(x+w/2+cos(radians)*radius);py=(int)(y+h/2+sin(radians)*radius);
    if(z < -72 && above) shader=above;
    else if(z > 72 && below) shader=below;
    size=((1-fraction)+1)*8;
    CG_DrawPic(px-size*.5f,py-size*.5f,size,size,shader);
}

static void CG_DrawRadar( void ) {
    extern qboolean tce_uiCoordinates;
    qboolean previous=tce_uiCoordinates;
    snapshot_t *snap=(cg.nextSnap&&!cg.nextFrameTeleport&&!cg.thisFrameTeleport)?cg.nextSnap:cg.snap;
    char mode[16];float y,angle,diff;
    static float lastangle,anglespeed;
    if(snap->ps.persistant[PERS_TEAM]==TEAM_SPECTATOR||cg.mvTotalClients>0)return;
    trap_Cvar_VariableStringBuffer("cg_aspectMode",mode,sizeof(mode));
    y=atoi(mode)==1?23.33f:atoi(mode)==2?-30.f:50.f;
    tce_uiCoordinates=qtrue;trap_R_SetColor(colorWhite);
    angle=(cg.predictedPlayerState.viewangles[YAW]+180.f)/360.f-.125f;
    diff=AngleSubtract(angle*360,lastangle*360)/360.f;
    anglespeed=anglespeed*.9259259104728699f+diff*.01f;
    if(Q_fabs(anglespeed)<.00001f)anglespeed=0;
    lastangle+=anglespeed;
    CG_DrawRotatedPic(752,y-40,80,80,trap_R_RegisterShader("gfx/misc/radarcompass2.tga"),lastangle);
    CG_DrawPic(742,y-50,100,100,trap_R_RegisterShader("gfx/misc/radarbg.tga"));
    trap_R_SetColor(NULL);
    {
        int i,kind,enemy,team=cg.snap->ps.persistant[PERS_TEAM];
        qhandle_t shader=0,above=0,below=0;
        centity_t *cent;
        entityState_t *es;
        for(i=0;i<64;i++) {
            if(VectorLength(cg.tceRadarPositions[0][i])!=0 && snap->ps.persistant[PERS_TEAM]==TEAM_AXIS)
                CG_DrawRadarIcon(762,(int)(y-30),60,60,cg.predictedPlayerState.origin,cg.tceRadarPositions[0][i],cgs.media.tceRadarCarrier,0,0);
            /* The Windows and Linux originals do not draw the second array. */
        }
        if(cg_gameType.integer==2 || cg_gameType.integer==5) for(i=0;i<snap->numEntities;i++) {
            cent=&cg_entities[snap->entities[i].number];es=&cent->currentState;
            if(es->eType==13) {
                kind=es->onFireStart;
                if(kind==-1)break;
                enemy=(es->teamNum==0 || (es->teamNum==1 && team==2) || (es->teamNum==2 && team==1));
                if(kind>=0 && kind<=5) {
                    if(kind==0)enemy=0;
                    shader=cgs.media.tceRadarMarkers[enemy][0][kind];
                    above=cgs.media.tceRadarMarkers[enemy][1][kind];
                    below=cgs.media.tceRadarMarkers[enemy][2][kind];
                }
            } else if(es->eType==ET_ITEM && es->otherEntityNum2==team &&
                      (es->otherEntityNum==253 || es->otherEntityNum==254) && es->effect3Time==es->otherEntityNum) {
                kind=es->otherEntityNum-247;
                shader=cgs.media.tceRadarMarkers[0][0][kind];
                above=cgs.media.tceRadarMarkers[0][1][kind];below=cgs.media.tceRadarMarkers[0][2][kind];
            } else continue;
            CG_DrawRadarIcon(762,(int)(y-30),60,60,cg.predictedPlayerState.origin,cent->lerpOrigin,shader,above,below);
        }
    }
    tce_uiCoordinates=previous;
}

static int CG_PlayerAmmoValue( int *ammo, int *clips, int *akimboammo ) {
	centity_t		*cent;
	playerState_t	*ps;
	int				weap;
	qboolean		skipammo = qfalse;

	*ammo = *clips = *akimboammo = -1;

	if( cg.snap->ps.clientNum == cg.clientNum )
		cent = &cg.predictedPlayerEntity;
	else
		cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	weap = cent->currentState.weapon;

	if ( !weap )
		return weap;

    if (gearDef.parsed) {
        switch (weap) {
        case 1: case 12: case 15: case 19: case 20: case 21: case 22: case 27: case 28:
            return weap;
        case 4: case 9: case 11: case 26: case 30: case 35: case 60: case 61: case 65: case 66:
            skipammo = qtrue;
        }
    } else switch(weap) {		// some weapons don't draw ammo count text
		case WP_AMMO:
		case WP_MEDKIT:
		case WP_KNIFE:
		case WP_PLIERS:
		case WP_SMOKE_MARKER:
		case WP_DYNAMITE:
		case WP_SATCHEL:
		case WP_SATCHEL_DET:
		case WP_SMOKE_BOMB:
		case WP_BINOCULARS:
			return weap;

		case WP_LANDMINE:
		case WP_MEDIC_SYRINGE:
		case WP_MEDIC_ADRENALINE:
		case WP_GRENADE_LAUNCHER:
		case WP_GRENADE_PINEAPPLE:
		case WP_FLAMETHROWER:
		case WP_MORTAR:
		case WP_MORTAR_SET:
		case WP_PANZERFAUST:
			skipammo = qtrue;
			break;

		default:
			break;
	}

	if( cg.snap->ps.eFlags & EF_MG42_ACTIVE || cg.snap->ps.eFlags & EF_MOUNTEDTANK ) {
		return WP_MOBILE_MG42;
	}

	// total ammo in clips
	*clips = cg.snap->ps.ammo[BG_FindAmmoForWeapon(weap)];
    if (gearDef.parsed && weap > 0 && weap < TCE_MAX_WEAPONS &&
        weaponDef[weap].maxclip > 1 && !weaponDef[weap].singleReload)
        *clips /= weaponDef[weap].maxclip;

	// current clip
	*ammo = ps->ammoclip[BG_FindClipForWeapon(weap)];

	if( BG_IsAkimboWeapon( weap ) ) {
		*akimboammo = ps->ammoclip[BG_FindClipForWeapon(BG_AkimboSidearm(weap))];
	} else {
		*akimboammo = -1;
	}

	if( weap == (gearDef.parsed ? 26 : WP_LANDMINE) ) {
		if( !cgs.gameManager ) {
			*ammo = 0;
		} else {
			if( cgs.clientinfo[ps->clientNum].team == TEAM_AXIS ) {
				*ammo = cgs.gameManager->currentState.otherEntityNum;
			} else {
				*ammo = cgs.gameManager->currentState.otherEntityNum2;
			}
		}
	} else if( gearDef.parsed ? (weap == 35 || weap == 60 || weap == 65) : (weap == WP_MORTAR || weap == WP_MORTAR_SET || weap == WP_PANZERFAUST) ) {
		*ammo += *clips;
	}
	
	if( skipammo ) {
		*clips = -1;
	}

	return weap;
}

#define HEAD_TURNTIME 10000
#define HEAD_TURNANGLE 20
#define HEAD_PITCHANGLE 2.5
static void CG_DrawPlayerStatusHead( void ) {
	hudHeadAnimNumber_t anim;
	rectDef_t headRect =		{ 44, 480 - 92, 62, 80 };
//	rectDef_t headHintRect =	{ 40, 480 - 22, 20, 20 };
	bg_character_t* character = CG_CharacterForPlayerstate( &cg.snap->ps );
	bg_character_t* headcharacter = BG_GetCharacter( cgs.clientinfo[ cg.snap->ps.clientNum ].team, cgs.clientinfo[ cg.snap->ps.clientNum ].cls );

	qhandle_t painshader = 0;

	anim = cg.idleAnim;

	if( cg.weaponFireTime > 500 ) {
		anim = HD_ATTACK;
	} else if( cg.time - cg.lastFiredWeaponTime < 500 ) {
		anim = HD_ATTACK_END;
	} else if( cg.time - cg.painTime < (character->hudheadanimations[ HD_PAIN ].numFrames * character->hudheadanimations[ HD_PAIN ].frameLerp) ) {
		anim = HD_PAIN;
	} else if( cg.time > cg.nextIdleTime ) { 
		cg.nextIdleTime = cg.time + 7000 + rand() % 1000;
		if( cg.snap->ps.stats[ STAT_HEALTH ] < 40 ) {
			cg.idleAnim = (rand() % (HD_DAMAGED_IDLE3 - HD_DAMAGED_IDLE2 + 1)) + HD_DAMAGED_IDLE2;
		} else {
			cg.idleAnim = (rand() % (HD_IDLE8 - HD_IDLE2 + 1)) + HD_IDLE2;
		}

		cg.lastIdleTimeEnd = cg.time + character->hudheadanimations[ cg.idleAnim ].numFrames * character->hudheadanimations[ cg.idleAnim ].frameLerp;
	}

	if( cg.snap->ps.stats[ STAT_HEALTH ] < 5 ) {
		painshader = cgs.media.hudDamagedStates[3];
	} else if( cg.snap->ps.stats[ STAT_HEALTH ] < 20 ) {
		painshader = cgs.media.hudDamagedStates[2];
	} else if( cg.snap->ps.stats[ STAT_HEALTH ] < 40 ) {
		painshader = cgs.media.hudDamagedStates[1];
	} else if( cg.snap->ps.stats[ STAT_HEALTH ] < 60 ) {
		painshader = cgs.media.hudDamagedStates[0];
	}

	if( cg.time > cg.lastIdleTimeEnd ) {
		if( cg.snap->ps.stats[ STAT_HEALTH ] < 40 ) {
			cg.idleAnim = HD_DAMAGED_IDLE1;
		} else {
			cg.idleAnim = HD_IDLE1;
		}
	}
	

	CG_DrawPlayerHead( &headRect, character, headcharacter, 180, 0, cg.snap->ps.eFlags & EF_HEADSHOT ? qfalse : qtrue, anim, painshader, cgs.clientinfo[ cg.snap->ps.clientNum ].rank, qfalse );

//	CG_DrawKeyHint( &headHintRect, "openlimbomenu" );
}

static void CG_DrawPlayerHealthBar( rectDef_t *rect ) {
	vec4_t bgcolour =	{	1.f,	1.f,	1.f,	0.3f	};
	vec4_t colour;
		
	int flags = 1|4|16|64;
	float frac;

	CG_ColorForHealth( colour );
	colour[3] = 0.5f;

	if( cgs.clientinfo[ cg.snap->ps.clientNum ].cls == PC_MEDIC ) {
		frac = cg.snap->ps.stats[STAT_HEALTH] / ( (float) cg.snap->ps.stats[STAT_MAX_HEALTH] * 1.12f );
	} else {
		frac = cg.snap->ps.stats[STAT_HEALTH] / (float) cg.snap->ps.stats[STAT_MAX_HEALTH];
	}


	CG_FilledBar( rect->x, rect->y + (rect->h * 0.1f), rect->w, rect->h * 0.84f, colour, NULL, bgcolour, frac, flags );

	trap_R_SetColor( NULL );
	CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.hudSprintBar );
	CG_DrawPic( rect->x, rect->y + rect->h + 4, rect->w, rect->w, cgs.media.hudHealthIcon );
}

static void CG_DrawStaminaBar( rectDef_t *rect ) {
	vec4_t bgcolour =	{	1.f,	1.f,	1.f,	0.3f	};
	vec4_t colour =		{	0.1f,	1.0f,	0.1f,	0.5f	};
	vec4_t colourlow =	{	1.0f,	0.1f,	0.1f,	0.5f	};
	vec_t* color = colour;
	int flags = 1|4|16|64;
	float frac = cg.pmext.sprintTime / (float)SPRINTTIME;

	if( cg.snap->ps.powerups[PW_ADRENALINE] ) {
		if ( cg.snap->ps.pm_flags & PMF_FOLLOW ) {
			Vector4Average( colour, colorWhite, sin(cg.time*.005f), colour);
		} else {
			float msec = cg.snap->ps.powerups[PW_ADRENALINE] - cg.time;

			if( msec < 0 ) {
				msec = 0;
			} else {
				Vector4Average( colour, colorWhite, .5f + sin(.2f * sqrt(msec) * 2 * M_PI) * .5f, colour);
			}
		}
	} else {
		if( frac < 0.25 ) {
			color = colourlow;
		}
	}

	CG_FilledBar( rect->x, rect->y + (rect->h * 0.1f), rect->w, rect->h * 0.84f, color, NULL, bgcolour, frac, flags );

	trap_R_SetColor( NULL );
	CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.hudSprintBar );
	CG_DrawPic( rect->x, rect->y + rect->h + 4, rect->w, rect->w, cgs.media.hudSprintIcon);
}

static void CG_DrawWeapRecharge( rectDef_t *rect ) {
	float		barFrac, chargeTime;
	int			weap, flags;
	qboolean	fade = qfalse;

	vec4_t	bgcolor = { 1.0f, 1.0f, 1.0f, 0.25f };
	vec4_t	color;

	flags = 1|4|16;

	weap = cg.snap->ps.weapon;

//	if( !(cg.snap->ps.eFlags & EF_ZOOMING) ) {
//		if ( weap != WP_PANZERFAUST && weap != WP_DYNAMITE && weap != WP_MEDKIT && weap != WP_SMOKE_GRENADE && weap != WP_PLIERS && weap != WP_AMMO ) {
//			fade = qtrue;
//		}
//	}

	// Draw power bar
	if( cg.snap->ps.stats[ STAT_PLAYER_CLASS ] == PC_ENGINEER ) {
		chargeTime = cg.engineerChargeTime[cg.snap->ps.persistant[PERS_TEAM]-1];
	} else if( cg.snap->ps.stats[ STAT_PLAYER_CLASS ] == PC_MEDIC ) {
		chargeTime = cg.medicChargeTime[cg.snap->ps.persistant[PERS_TEAM]-1];
	} else if( cg.snap->ps.stats[ STAT_PLAYER_CLASS ] == PC_FIELDOPS ) {
		chargeTime = cg.ltChargeTime[cg.snap->ps.persistant[PERS_TEAM]-1];
	} else if( cg.snap->ps.stats[ STAT_PLAYER_CLASS ] == PC_COVERTOPS ) {
		chargeTime = cg.covertopsChargeTime[cg.snap->ps.persistant[PERS_TEAM]-1];
	} else {
		chargeTime = cg.soldierChargeTime[cg.snap->ps.persistant[PERS_TEAM]-1];
	}

	barFrac = (float)(cg.time - cg.snap->ps.classWeaponTime) / chargeTime;
	if( barFrac > 1.0 ) {
		barFrac = 1.0;
	}

	color[0] = 1.0f;
	color[1] = color[2] = barFrac;
	color[3] = 0.25 + barFrac*0.5;

	if ( fade ) {
		bgcolor[3] *= 0.4f;
		color[3] *= 0.4;
	}

	CG_FilledBar( rect->x, rect->y + (rect->h * 0.1f), rect->w, rect->h * 0.84f, color, NULL, bgcolor, barFrac, flags );

	trap_R_SetColor( NULL );
	CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.hudSprintBar );
	CG_DrawPic( rect->x + (rect->w * 0.25f) - 1, rect->y + rect->h + 4, (rect->w * 0.5f) + 2, rect->w + 2, cgs.media.hudPowerIcon );
}

extern void CG_TCEDrawWeaponCycle(float bottom);
/* Complete carrier-indicator block of CG_DrawPlayerStatus30028530.
 * Carrier state is authoritative snapshot state, not a gametype guess. */
static void CG_TCEDrawCarrierStatus(const playerState_t *ps,float bottom) {
    if(ps->stats[STAT_TCE_WEAPON_FLAGS]&0x100)
        CG_DrawPic(742,bottom-42,36,36,trap_R_RegisterShader("sprites/bombcarrier.tga"));
    if(ps->stats[9]&0x100)
        CG_DrawPic(737,bottom-35,36,36,trap_R_RegisterShader("sprites/VIP.tga"));
    if(ps->stats[9]&0x400)
        CG_DrawPic(737,bottom-35,36,36,trap_R_RegisterShader("sprites/hostage.tga"));
    if(ps->powerups[PW_REDFLAG] || ps->powerups[PW_BLUEFLAG])
        CG_DrawPic(742,bottom-42,36,36,trap_R_RegisterShader("sprites/backpackcarrier.tga"));
}

static void CG_TCEDrawObjectiveProgress(const playerState_t *ps) {
    float duration, fraction;
    if(!(ps->pm_flags&PMF_TCE_OBJECTIVE_ACTION) ||
       (ps->serverCursorHint!=39 && ps->serverCursorHint!=10 && ps->serverCursorHint!=3))return;
    duration=(ps->stats[STAT_TCE_FLAGS]&2)?2000.f:4000.f;
    if(ps->serverCursorHint==39)duration=(ps->stats[STAT_TCE_FLAGS]&2)?6000.f:10000.f;
    trap_R_SetColor(NULL);
    fraction=(duration-ps->weaponTime)/duration;
    if(fraction<0)fraction=0;
    if(fraction<1)CG_HorizontalPercentBar(376,310,100,10,fraction);
}

static void CG_DrawPlayerStatus( void ) {
    extern qboolean tce_uiCoordinates;
    qboolean previous=tce_uiCoordinates;
    playerState_t *ps=&cg.snap->ps;
    float bottom,rect[4],x,y,frac;
    vec4_t fg={.7f,.7f,.7f,.7f},bg={0,0,0,.3f},reload={1,1,1,1};
    char mode[16],buffer[32];
    int damage[3],shaders[4],ammo,reserve,akimbo,weap,maxclip,width,i,hasFiremode=0;
    trap_Cvar_VariableStringBuffer("cg_aspectMode",mode,sizeof(mode));
    bottom=atoi(mode)==1?506.67f:atoi(mode)==2?560.f:480.f;
    tce_uiCoordinates=qtrue;
    CG_TCEDrawWeaponCycle(bottom);
    y=bottom-10;
    if(cg.time-cg.weaponSelectTime>500) {
        weap=CG_PlayerAmmoValue(&ammo,&reserve,&akimbo);
        maxclip=weaponDef[weap].maxclip;
        if(akimbo>=0 || reserve>=0) {
            Com_sprintf(buffer,sizeof(buffer),"/%i",reserve);
            width=CG_Text_Width_Ext(buffer,.25f,0,&cgs.media.limboFont1);
            CG_Text_Paint_Ext(840-width,y,.25f,.25f,colorWhite,buffer,0,0,3,&cgs.media.limboFont1);
            for(i=0;i<(akimbo>=0?2:1);++i) {
                x=830-width-8*i;frac=(float)(i?akimbo:ammo)/maxclip;
                CG_FilledBar(x,bottom-19,6,9,fg,NULL,bg,frac,0x55);
                CG_DrawPic(x,bottom-19,6,9,cgs.media.tceAmmoFrame);
            }
            if(ammo==0 && akimbo<=0 && reserve>0) {
                reload[3]=(sin((double)cg.time*.0062831854447722435f)+1)*.5f;
                if(reload[3]>.7f) CG_Text_Paint_Ext(840-CG_Text_Width_Ext("Reload",.25f,0,&cgs.media.limboFont1),y-12,.25f,.25f,reload,"Reload",0,0,3,&cgs.media.limboFont1);
            }
        } else if(ammo>=0) {
            Com_sprintf(buffer,sizeof(buffer),"%i",ammo);
            CG_Text_Paint_Ext(840-CG_Text_Width_Ext(buffer,.25f,0,&cgs.media.limboFont1),y,.25f,.25f,colorWhite,buffer,0,0,3,&cgs.media.limboFont1);
        }
    }
    weap=cg.predictedPlayerState.weapon;
    if(cg.time-cg.weaponSelectTime>500)hasFiremode=BG_FiremodeWeapon(weap);
    if(cg.time-cg.weaponSelectTime>500 && hasFiremode) {
        const char *label;int shift,firemode=ps->persistant[10];
        if(weaponDef[weap].pump){label=firemode>0?"Semi":"Pump";shift=firemode>0?0:4;}
        else {label=firemode==2?"Single":firemode==1?"Burst":"Auto";shift=firemode==2?0:firemode==1?2:4;}
        CG_Text_Paint_Ext(798-CG_Text_Width_Ext(label,.15f,0,&cgs.media.limboFont1),y-shift,.15f,.15f,colorWhite,label,0,0,3,&cgs.media.limboFont1);
    }
    if(cg.time-cg.weaponSelectTime>500 && !hasFiremode &&
       (ps->stats[STAT_TCE_WEAPON_FLAGS]&4) && (cg.predictedPlayerState.weapon==30 ||
       cg.predictedPlayerState.weapon==9 || cg.predictedPlayerState.weapon==4)) {
        CG_Text_Paint_Ext(798-CG_Text_Width_Ext("Short throw",.15f,0,&cgs.media.limboFont1),y,
            .15f,.15f,colorWhite,"Short throw",0,0,3,&cgs.media.limboFont1);
    }
    CG_TCEDrawCarrierStatus(ps,bottom);
    for(i=0;i<4;++i)shaders[i]=cgs.media.tceHealthMan[i];
    for(i=0;i<3;++i)damage[i]=ps->holdable[2+i];
    rect[0]=24;rect[1]=bottom-54;rect[2]=20;rect[3]=48;
    TCE_DrawPlayerHealthMan(rect,damage,shaders,trap_R_SetColor,CG_DrawPic);
    rect[0]=12;rect[1]=bottom-57;rect[2]=8;rect[3]=37;
    TCE_DrawStaminaBar(rect,cg.pmext.sprintTime,cgs.media.hudSprintIcon,
        cgs.media.tceStaminaFrame,trap_R_SetColor,CG_DrawPic,CG_FilledBar);
    trap_R_SetColor(NULL);
    CG_DrawPic(47,bottom-38,32,32,cgs.media.tceMoveType[
        ps->eFlags&EF_PRONE?0:ps->pm_flags&PMF_DUCKED?1:2]);
    CG_TCEDrawObjectiveProgress(ps);
    tce_uiCoordinates=previous;
}

static void CG_DrawSkillBar( float x, float y, float w, float h, int skill ) {
	int i;
	float blockheight = ( h - 4 ) / (float)(NUM_SKILL_LEVELS - 1);
	float draw_y;
	vec4_t colour;
	float x1, y1, w1, h1;

	draw_y = y + h - blockheight;
	for( i = 0; i < NUM_SKILL_LEVELS - 1; i++ ) {
		if( i >= skill ) {
			Vector4Set( colour, 1.f, 1.f, 1.f, .15f );
		} else {
			Vector4Set( colour, 0.f, 0.f, 0.f, .4f );
		}

		CG_FillRect( x, draw_y, w, blockheight, colour );

		if( i < skill ) {
			x1 = x;
			y1 = draw_y;
			w1 = w;
			h1 = blockheight;
			CG_AdjustFrom640( &x1, &y1, &w1, &h1 );

			trap_R_DrawStretchPic( x1, y1, w1, h1, 0, 0, 1.f, 0.5f, cgs.media.limboStar_roll );
		}

		CG_DrawRect_FixedBorder( x, draw_y, w, blockheight, 1, colorBlack );
//		CG_DrawPic( x, draw_y, w, blockheight, cgs.media.hudBorderVert2 );
		draw_y -= ( blockheight + 1 );
	}
}

#define SKILL_ICON_SIZE		14

#define SKILLS_X 112
#define SKILLS_Y 20

#define SKILL_BAR_OFFSET	(2*SKILL_BAR_X_INDENT)
#define SKILL_BAR_X_INDENT	0
#define SKILL_BAR_Y_INDENT	6

#define SKILL_BAR_WIDTH		( SKILL_ICON_SIZE - SKILL_BAR_OFFSET )
#define SKILL_BAR_X			( SKILL_BAR_OFFSET + SKILL_BAR_X_INDENT + SKILLS_X )
#define SKILL_BAR_X_SCALE	( SKILL_ICON_SIZE + 2 )
#define SKILL_ICON_X		( SKILL_BAR_OFFSET + SKILLS_X )
#define SKILL_ICON_X_SCALE	( SKILL_ICON_SIZE + 2 )
#define SKILL_BAR_Y			( SKILL_BAR_Y_INDENT - SKILL_BAR_OFFSET - SKILLS_Y )
#define SKILL_BAR_Y_SCALE	( SKILL_ICON_SIZE + 2 )
#define SKILL_ICON_Y		(- ( SKILL_ICON_SIZE + 2 ) - SKILL_BAR_OFFSET - SKILLS_Y )

skillType_t CG_ClassSkillForPosition( clientInfo_t* ci, int pos ) {
	switch( pos ) {
		case 0:
			return BG_ClassSkillForClass(ci->cls);
		case 1:
			return SK_BATTLE_SENSE;
		case 2:
			return SK_LIGHT_WEAPONS;
	}

	return SK_BATTLE_SENSE;
}

static void CG_DrawPlayerStats( void ) {
	int					value = 0;
	playerState_t		*ps;
	clientInfo_t		*ci;
	skillType_t			skill;
	int					i;
	const char*			str;
	float				w;
	vec_t*				clr;

	str = va( "%i", cg.snap->ps.stats[STAT_HEALTH] );
	w = CG_Text_Width_Ext( str, 0.25f, 0, &cgs.media.limboFont1 );
	CG_Text_Paint_Ext( SKILLS_X - 28 - w, 480 - 4, 0.25f, 0.25f, colorWhite, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1 );
	CG_Text_Paint_Ext( SKILLS_X - 28 + 2, 480 - 4, 0.2f, 0.2f, colorWhite, "HP", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1 );

	if( cgs.gametype == GT_WOLF_LMS ) {
		return;
	}

	ps = &cg.snap->ps;
	ci = &cgs.clientinfo[ ps->clientNum ];


	for( i = 0; i < 3; i++ ) {
		skill = CG_ClassSkillForPosition( ci, i );

		CG_DrawSkillBar( i * SKILL_BAR_X_SCALE + SKILL_BAR_X, 480 - (5 * SKILL_BAR_Y_SCALE) + SKILL_BAR_Y, SKILL_BAR_WIDTH, 4 * SKILL_ICON_SIZE, ci->skill[skill] );
		CG_DrawPic( i * SKILL_ICON_X_SCALE + SKILL_ICON_X, 480 + SKILL_ICON_Y, SKILL_ICON_SIZE, SKILL_ICON_SIZE, cgs.media.skillPics[skill] );
	}

	if( cg.time - cg.xpChangeTime < 1000 ) {
		clr = colorYellow;
	} else {
		clr = colorWhite;
	}


	str = va( "%i", cg.snap->ps.stats[STAT_XP] );
	w = CG_Text_Width_Ext( str, 0.25f, 0, &cgs.media.limboFont1 );
	CG_Text_Paint_Ext( SKILLS_X + 28 - w, 480 - 4, 0.25f, 0.25f, clr, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1 );
	CG_Text_Paint_Ext( SKILLS_X + 28 + 2, 480 - 4, 0.2f, 0.2f, clr, "XP", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1 );

	// draw treasure icon if we have the flag
	// rain - #274 - use the playerstate instead of the clientinfo
	if( ps->powerups[PW_REDFLAG] || ps->powerups[PW_BLUEFLAG] ) {
		trap_R_SetColor( NULL );
		CG_DrawPic( 640 - 40, 480 - 140 - value, 36, 36, cgs.media.objectiveShader );
	} else if ( ps->powerups[PW_OPS_DISGUISED] ) { // Disguised?
		CG_DrawPic( 640 - 40, 480 - 140 - value, 36, 36, ps->persistant[PERS_TEAM] == TEAM_AXIS ? cgs.media.alliedUniformShader : cgs.media.axisUniformShader );
	}
}

static char statsDebugStrings[6][512];
static int statsDebugTime[6];
static int statsDebugTextWidth[6];
static int statsDebugPos;

void CG_InitStatsDebug( void )
{
	memset( &statsDebugStrings, 0, sizeof(statsDebugStrings) );
	memset( &statsDebugTime, 0, sizeof(statsDebugTime) );
	statsDebugPos = -1;
}

void CG_StatsDebugAddText( const char *text )
{
	if( cg_debugSkills.integer ) {
		statsDebugPos++;

		if( statsDebugPos >= 6 )
			statsDebugPos = 0;

		Q_strncpyz( statsDebugStrings[statsDebugPos], text, 512 );
		statsDebugTime[statsDebugPos] = cg.time;
		statsDebugTextWidth[statsDebugPos] = CG_Text_Width_Ext( text, .15f, 0, &cgs.media.limboFont2 );

		CG_Printf( "%s\n", text );
	}
}

static void CG_DrawStatsDebug( void )
{
	int textWidth = 0;
	int i, x, y, w, h;

	if( !cg_debugSkills.integer )
		return;

	for( i = 0; i < 6; i++ ) {
		if( statsDebugTime[i] + 9000 > cg.time ) {
			if( statsDebugTextWidth[i] > textWidth )
				textWidth = statsDebugTextWidth[i];
		}
	}

	w = textWidth + 6;
	h = 9;
	x = 640 - w;
	y = (480 - 5 * ( 12 + 2 ) + 6 - 4) - 6 - h;	// don't ask

	i = statsDebugPos;

	do {
		vec4_t colour;

		if( statsDebugTime[i] + 9000 <= cg.time ) {
			break;
		}

        colour[0] = colour[1] = colour[2] = .5f;
		if( cg.time - statsDebugTime[i] > 5000 )
			{
#if defined(_MSC_VER) && defined(_M_IX86)
                static const float fadeRate = 0.00025f, baseAlpha = .5f;
                int age = (int)((unsigned int)cg.time - (unsigned int)statsDebugTime[i] - 5000u);
                float alpha;
                __asm {
                    fild dword ptr [age]
                    fmul dword ptr [fadeRate]
                    fmul dword ptr [baseAlpha]
                    fsubr dword ptr [baseAlpha]
                    fstp dword ptr [alpha]
                }
                colour[3] = alpha;
#else
                colour[3] = .5f - .5f * ( ( cg.time - statsDebugTime[i] - 5000 ) / 4000.f );
#endif
            }
		else
			colour[3] = .5f ;
		CG_FillRect( x, y, w, h, colour );

		colour[0] = colour[1] = colour[2] = 1.f;
		if( cg.time - statsDebugTime[i] > 5000 )
			{
#if defined(_MSC_VER) && defined(_M_IX86)
                static const float fadeRate = 0.00025f, baseAlpha = 1.f;
                int age = (int)((unsigned int)cg.time - (unsigned int)statsDebugTime[i] - 5000u);
                float alpha;
                __asm {
                    fild dword ptr [age]
                    fmul dword ptr [fadeRate]

                    fsubr dword ptr [baseAlpha]
                    fstp dword ptr [alpha]
                }
                colour[3] = alpha;
#else
                colour[3] = 1.f - ( ( cg.time - statsDebugTime[i] - 5000 ) / 4000.f );
#endif
            }
		else
			colour[3] = 1.f ;
		CG_Text_Paint_Ext( (float)(637.0 - (double)statsDebugTextWidth[i]), y + h - 2, .15f, .15f, colour, statsDebugStrings[i], 0, 0, ITEM_TEXTSTYLE_NORMAL, &cgs.media.limboFont2 );

		y -= h;

		i--;
		if( i < 0 )
			i = 6 - 1;
	} while( i != statsDebugPos );
}

//bani
void CG_DrawDemoRecording( void ) {
	char status[1024];
	char demostatus[128];
	char wavestatus[128];
	/* Original30021b30 has a separate visibility toggle from the Y position. */
	if(!cg_recording_showstatusline.integer)return;

	if( !cl_demorecording.integer && !cl_waverecording.integer ) {
		return;
	}

	if( !cg_recording_statusline.integer ) {
		return;
	}

	if( cl_demorecording.integer ) {
		Com_sprintf( demostatus, sizeof( demostatus ), " demo %s: %ik ", cl_demofilename.string, cl_demooffset.integer / 1024 );
	} else {
		strncpy( demostatus, "", sizeof( demostatus ) );
	}

	if( cl_waverecording.integer ) {
		Com_sprintf( wavestatus, sizeof( demostatus ), " audio %s: %ik ", cl_wavefilename.string, cl_waveoffset.integer / 1024 );
	} else {
		strncpy( wavestatus, "", sizeof( wavestatus ) );
	}

	Com_sprintf( status, sizeof( status ), "RECORDING%s%s", demostatus, wavestatus );

	CG_Text_Paint_Ext( 5, cg_recording_statusline.integer, 0.2f, 0.2f, colorWhite, status, 0, 0, 0, &cgs.media.limboFont2 );
}

/*
=================
CG_Draw2D
=================
*/
static void CG_DrawLevelshotFade( void ) {
    extern qboolean tce_uiCoordinates;
    qboolean previous = tce_uiCoordinates;
    tce_uiCoordinates = qtrue;
    TCE_DrawLevelshotFade(cg.snap != NULL, cg.time, cg.levelshotFadeStart,
        cgs.media.levelshotShader, trap_R_SetColor, CG_DrawPic, CG_FillRect);
    tce_uiCoordinates = previous;
}

/* Complete TC300217d0 / Linux000499e4. The description is team objective0,
 * not the map briefing and not the SDK current-objective panel. */
extern vmCvar_t cg_aspectMode;
static void CG_DrawObjectiveDesc(void) {
    extern qboolean tce_uiCoordinates;
    char text[1024];
    const char *description=NULL,*binding,*location;
    vec4_t color={.75f,.75f,.75f,1};
    float y;
    int width;
    qboolean previous=tce_uiCoordinates;
    if((cg.snap->ps.pm_flags & PMF_LIMBO) ||
       cg.snap->ps.persistant[PERS_TEAM]==TEAM_SPECTATOR || cg.mvTotalClients>0) return;
    y=cg_aspectMode.integer==1 ? 84.33f : cg_aspectMode.integer==2 ? 31.f : 111.f;
    tce_uiCoordinates=qtrue;
    if(cg.snap->ps.holdable[13] && (location=CG_ConfigString(CS_LOCATIONS+cg.snap->ps.holdable[13]))!=NULL) {
        Com_sprintf(text,sizeof(text),"%s",location);
        width=CG_Text_Width_Ext(text,.19f,0,&cgs.media.limboFont1);
        CG_Text_Paint_Ext(840-width,y-10,.19f,.19f,color,text,0,0,3,&cgs.media.limboFont1);
    }
    if(cgs.gametype!=7 && cg.tceShowObjectiveDesc) {
        if(cg.snap->ps.persistant[PERS_TEAM]==TEAM_AXIS) description=cg.objDescription_Axis[0];
        else if(cg.snap->ps.persistant[PERS_TEAM]==TEAM_ALLIES) description=cg.objDescription_Allied[0];
        if(!description || !description[0]) description="No Objective Supplied";
        binding=BindingFromName("toggleobjectivedesc");
        if(!Q_stricmp(binding,"(toggleobjectivedesc)")) binding="ESCAPE";
        Com_sprintf(text,sizeof(text),"Press %s to hide",binding);
        width=CG_Text_Width_Ext(text,.19f,0,&cgs.media.limboFont1);
        CG_Text_Paint_Ext(840-width,y+2,.19f,.19f,color,text,0,0,3,&cgs.media.limboFont1);
        Q_strncpyz(text,description,sizeof(text));
        width=CG_Text_Width_Ext(text,.19f,0,&cgs.media.limboFont1);
        CG_Text_Paint_Ext(840-width,y+12,.19f,.19f,color,text,0,0,3,&cgs.media.limboFont1);
    }
    tce_uiCoordinates=previous;
}

/* Original Draw2D30022d1e..30022e5b. Phase is stats14 (ps+108),
 * not the independently maintained aim phase in stats13. */
static void CG_TCEPlayAdsBreath(void) {
    float phase;
    int bank;
    if (!(cg.predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS] & 4) ||
        weaponDef[cg.predictedPlayerState.weapon].scoped <= 0) return;
    phase=(float)((double)cg.predictedPlayerState.stats[14]*(double).001f);
    if (cg.tceAdsBreathTime+2000 >= cg.time) return;
    if ((phase>.5 && phase<1) || (phase>5.5 && phase<6)) bank=0;
    else if ((phase>3 && phase<3.5) || (phase>8 && phase<8.5)) bank=2;
    else return;
    cg.tceAdsBreathTime=cg.time;
    trap_S_StartSound(NULL,cg.snap->ps.clientNum,CHAN_LOCAL,
        cgs.media.tceBreath[bank+rand()%2]);
}

/* TC Windows300277e0: aspect-mode matte, independent of cinematic letterbox. */
static void CG_TCEDrawAspectMatte(void) {
    extern vmCvar_t cg_aspectMode;
    extern qboolean tce_uiCoordinates;
    vec4_t black = {0, 0, 0, 1};
    float height, border;
    int width = cgs.glconfig.vidWidth, screenHeight = cgs.glconfig.vidHeight;
    qboolean previous;
    if (!cg.snap) return;
    if (cg_aspectMode.integer == 2 &&
        ((double)width * 3.0) / ((double)screenHeight * 4.0) >= 1.0) return;
    if (width * 9 == screenHeight * 16 ||
        (cg_aspectMode.integer >= 1 && width * 10 == screenHeight * 16)) return;
    height = cg_aspectMode.integer == 1 ? 533.3333f :
             cg_aspectMode.integer == 2 ? 640.0f : 480.0f;
    border = (640.0f - height) * .5f + 35.0f;
    previous = tce_uiCoordinates;
    tce_uiCoordinates = qtrue;
    CG_FillRect(-5, -115, 862, border, black);
    CG_FillRect(-5, (height + 480.0f) * .5f, 862, border, black);
    tce_uiCoordinates = previous;
}


static void CG_Draw2D( void ) {
	CG_ScreenFade();

	// Arnout: no 2d when in esc menu
	// FIXME: do allow for quickchat (bleh)
	// Gordon: Removing for now
/*	if( trap_Key_GetCatcher() & KEYCATCH_UI ) {
		return;
	}*/

	if( cg.snap->ps.pm_type == PM_INTERMISSION ) {
		CG_DrawIntermission();
		return;
	} else {
		if( cgs.dbShowing ) {
			CG_Debriefing_Shutdown();
		}
	}

	if( cg.editingSpeakers ) {
		CG_SpeakerEditorDraw();
		return;
	}

	/* Original30022a40: this switch is effective only for developers. */
	if (!cg_draw2D.integer && developer.integer) return;

	if( !cg.cameraMode ) {
		CG_DrawCrosshair();
		CG_DrawFlashBlendBehindHUD();

		if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
			CG_DrawSpectator();
			CG_DrawCrosshairNames();

			// NERVE - SMF - we need to do this for spectators as well
			CG_DrawTeamInfo();
		} else {
			// don't draw any status if dead
			if ( cg.snap->ps.stats[STAT_HEALTH] > 0 || (cg.snap->ps.pm_flags & PMF_FOLLOW) ) {

				CG_DrawCrosshairNames();

				CG_DrawNoShootIcon();

//				CG_DrawPickupItem();
			}

			CG_DrawTeamInfo();

			if ( cg_drawStatus.integer ) {
				Menu_PaintAll();
				CG_DrawTimedMenus();
			}
		}

        {
            extern qboolean tce_uiCoordinates;
            qboolean previous=tce_uiCoordinates;
            tce_uiCoordinates=qtrue;CG_DrawVote();tce_uiCoordinates=previous;
        }

		CG_DrawLagometer();
	}

	// don't draw center string if scoreboard is up
	if ( !CG_DrawScoreboard() ) {
		if( cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
			rectDef_t rect;

			if( cg.snap->ps.stats[STAT_HEALTH] > 0 ) {

				CG_DrawPlayerStatus();

			}

			CG_DrawLivesLeft();

			// Cursor hint
			rect.w = rect.h = 48;
			rect.x = 402;
			rect.y = 260;
			{ extern qboolean tce_uiCoordinates; qboolean previous=tce_uiCoordinates;
                tce_uiCoordinates=qtrue; CG_DrawCursorhint( &rect ); tce_uiCoordinates=previous; }

			// Stability bar
			rect.x = 50;
			rect.y = 208;
			rect.w = 10;
			rect.h = 64;
			CG_DrawWeapStability( &rect );

			// Stats Debugging
			CG_DrawStatsDebug();
		}

		if (!cg_paused.integer) {
			CG_DrawUpperRight();
		}

		CG_DrawCenterString();
		CG_DrawPMItems();
		CG_DrawPMItemsBig();

		CG_DrawFollow();
		CG_DrawWarmup();

		CG_DrawNotify();

		if ( cg_drawCompass.integer ) {
			CG_DrawRadar();
		}

		CG_DrawObjectiveDesc();
		CG_DrawObjectiveInfo();

		CG_DrawSpectatorMessage();

		CG_DrawLimboMessage();
	} else {
		if(cgs.eventHandling != CGAME_EVENT_NONE) {
//			qboolean old = cg.showGameView;

//			cg.showGameView = qfalse;
			// draw cursor
			trap_R_SetColor( NULL );
			CG_DrawPic( cgDC.cursorx-14, cgDC.cursory-14, 32, 32, cgs.media.cursorIcon);
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
	if( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		CG_DrawLevelshotFade();
	}
	CG_TCEDrawAspectMatte();
	/* Original Draw2D30022a40: injury heartbeat slows during its last six seconds. */
	if (!(cg.predictedPlayerState.pm_flags & PMF_FOLLOW) &&
	    !(cg.predictedPlayerState.pm_type & 2) &&
	    cg.tceHeartbeatUntil > cg.time && cg.snap->ps.stats[STAT_HEALTH] > 0 &&
	    cg.tceHeartbeatNext < cg.time) {
		int remaining = cg.tceHeartbeatUntil - cg.time;
		trap_S_StartSound(NULL,cg.snap->ps.clientNum,CHAN_LOCAL,cgs.media.tceHeartbeat);
		cg.tceHeartbeatNext = cg.time + 600;
		if (remaining < 6000) cg.tceHeartbeatNext += (6000 - remaining) / 15;
	}
	CG_TCEPlayAdsBreath();
	TCE_CG_UpdateFlashRinging();
}

// NERVE - SMF
/* TC30021c70/30021cd0: retain the Windows float constants and avoid
   rounding the shake displacement before adding it to the camera origin. */
void CG_StartShakeCamera(float p) {
    double phase;
    double length=(double)p*p*1000.0;
    cg.cameraShakeScale=p;
    cg.cameraShakeLength=(float)length;
    cg.cameraShakeTime=(int)((double)cg.time+cg.cameraShakeLength);
    phase=(double)(rand()&0x7fff)*(double)3.0518509447574615e-05f-.5;
    cg.cameraShakePhase=(float)((phase+phase)*3.1415927410125732);
}

void CG_ShakeCamera(void) {
    double fraction,amplitude;
    if(cg.cameraShakeTime<cg.time) {
        cg.cameraShakeScale=0;
        return;
    }
    fraction=(double)(cg.cameraShakeTime-cg.time)/cg.cameraShakeLength;
    amplitude=fraction*cg.cameraShakeScale*4.0;
    cg.refdef.vieworg[2]=(float)(cg.refdef.vieworg[2]+amplitude*
        sin(fraction*21.99114990234375f+cg.cameraShakePhase));
    cg.refdef.vieworg[1]=(float)(cg.refdef.vieworg[1]+amplitude*
        sin(fraction*40.84070587158203f+cg.cameraShakePhase));
    cg.refdef.vieworg[0]=(float)(cg.refdef.vieworg[0]+amplitude*
        cos(fraction*53.40707778930664f+cg.cameraShakePhase));
    AnglesToAxis(cg.refdefViewAngles,cg.refdef.viewaxis);
}
// -NERVE - SMF

/* TC30048990: snap sprite bases to the map and sample original lightgrid. */
void CG_SetupEliteLighting(void) {
    int i;
    for(i=0;i<cg.numMiscGameModels;i++) {
        cg_gamemodel_t *m=&cgs.miscGameModels[i];
        if(m->billboard)TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(),m->org,m->color);
    }
    for(i=0;i<cg.numMiscClientSprites;i++) {
        cg_clientsprite_t *s=&cgs.miscClientSprites[i];
        vec3_t end;
        trace_t trace;
        s->org[2]+=8;
        VectorCopy(s->org,end);end[2]-=128;
        CG_Trace(&trace,s->org,NULL,NULL,end,-1,CONTENTS_SOLID);
        s->org[2]-=trace.fraction*128+s->bottom;
        TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(),s->org,s->color);
    }
}

/* TC30022020: vertical sprites face the viewer around their world Z axis. */
static void CG_DrawMiscClientsprites(void) {
    int i,j,k;
    for(i=0;i<cg.numMiscClientSprites;i++) {
        cg_clientsprite_t *s=&cgs.miscClientSprites[i];
        vec3_t forward,right;
        polyVert_t verts[4];
        float distance,alpha=1;
        VectorSubtract(s->org,cg.refdef_current->vieworg,forward);
        distance=VectorNormalize(forward);
        if(DotProduct(forward,cg.refdef_current->viewaxis[0])<.5f ||
           distance>s->drawDistance || !s->shader)continue;
        if(s->drawDistance>s->fadeDistance && distance>s->fadeDistance)
            alpha=(float)(1.0-((double)distance-s->fadeDistance)/(s->drawDistance-s->fadeDistance));
        forward[2]=0;VectorNormalize(forward);
        VectorSet(right,forward[1],-forward[0],0);
        for(j=0;j<4;j++) {
            /* Original3002217d..3002232b keeps the top-edge products in
             * x87, but reloads stored X products and bottom-right Y. */
            double side=(j==0||j==3)?s->halfWidth:-s->halfWidth;
            double dx=side*right[0],dy=side*right[1];
            verts[j].xyz[0]=(float)(s->org[0]+(j<2?dx:(double)(float)dx));
            verts[j].xyz[1]=(float)(s->org[1]+(j==3?(double)(float)dy:dy));
            verts[j].xyz[2]=s->org[2]+((j<2)?s->top:s->bottom);
            verts[j].st[0]=(j==0||j==3)?1:0;verts[j].st[1]=j<2?0:1;
            for(k=0;k<3;k++)verts[j].modulate[k]=(byte)(int)((double)s->color[k]*255);
            verts[j].modulate[3]=(byte)(int)((double)alpha*255);
        }
        trap_R_AddPolyToScene(s->shader,4,verts);
    }
}

/* Complete TC30021da0, including the previously missing billboard branch.
   Like the original, the reused entity is cleared once, not once per model. */
void CG_DrawMiscGamemodels(void) {
    int i,j;
    refEntity_t ent;
    memset(&ent,0,sizeof(ent));
    ent.reType=RT_MODEL;ent.nonNormalizedAxes=qtrue;ent.renderfx=RF_NOSHADOW;
    for(i=0;i<cg.numMiscGameModels;i++) {
        cg_gamemodel_t *model=&cgs.miscGameModels[i];
        if(model->radius && CG_CullPointAndRadius(model->org,model->radius))continue;
        if(!trap_R_inPVS(cg.refdef_current->vieworg,model->org))continue;
        if(!model->billboard) {
            AxisCopy(model->axes,ent.axis);
        } else {
            vec3_t forward,left;
            float distance;
            VectorSubtract(model->org,cg.refdef_current->vieworg,forward);
            distance=VectorNormalize(forward);
            if(distance>model->drawDistance)continue;
            for(j=0;j<3;j++)ent.shaderRGBA[j]=(byte)(int)((double)model->color[j]*255.0);
            if(model->drawDistance<=model->fadeDistance || distance<=model->fadeDistance)
                ent.shaderRGBA[3]=255;
            else ent.shaderRGBA[3]=(byte)(int)(255.0*(1.0-
                ((double)distance-model->fadeDistance)/(model->drawDistance-model->fadeDistance)));
            forward[2]=0;VectorNormalize(forward);
            VectorSet(left,-forward[1],forward[0],0);
            VectorScale(forward,model->billboardScale,ent.axis[0]);
            VectorScale(left,model->billboardScale,ent.axis[1]);
            VectorCopy(model->axes[2],ent.axis[2]);
        }
        VectorCopy(model->org,ent.origin);
        VectorCopy(model->org,ent.oldorigin);
        VectorCopy(model->org,ent.lightingOrigin);
        ent.hModel=model->model;
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
/* TC viewport stores use __ftol's signed64 conversion and low EAX. */
static int CG_TCEViewportInteger(float viewportValue) {
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short viewportCW, viewportTruncateCW;
    __int64 viewportInteger;
    __asm {
        fld dword ptr viewportValue
        fwait
        fnstcw word ptr viewportCW
        fwait
        mov ax, word ptr viewportCW
        or ah, 0ch
        mov word ptr viewportTruncateCW, ax
        fldcw word ptr viewportTruncateCW
        fistp qword ptr viewportInteger
        fldcw word ptr viewportCW
    }
    return (int)viewportInteger;
#else
    return (int)viewportValue;
#endif
}

static void CG_DrawTCEScopeScene(void) {
    extern qboolean tce_uiCoordinates;
    qboolean previous;
    refdef_t modelView;
    vec3_t angles, forward, right, up;
    float x,y,w,h,scopeSize,radians;
#if defined(_MSC_VER) && defined(_M_IX86)
    float scopeSavedViewport[4];
    float scopeSavedFov[2];
    vec3_t scopeSavedAxis[3];
    int scopeAllowed;
    int scopeWidth;
    float *scopeZoom;
    enum { ScopeAngles = offsetof(cg_t,predictedPlayerState)+offsetof(playerState_t,viewangles),
        ScopeHoldPitch = offsetof(cg_t,predictedPlayerState)+offsetof(playerState_t,holdable)+5*sizeof(int),
        ScopeHoldYaw = offsetof(cg_t,predictedPlayerState)+offsetof(playerState_t,holdable)+6*sizeof(int),
        ScopeCurrentRefdef = offsetof(cg_t,refdef_current),
        ScopeRefX = offsetof(refdef_t,x), ScopeRefY = offsetof(refdef_t,y),
        ScopeRefWidth = offsetof(refdef_t,width), ScopeRefHeight = offsetof(refdef_t,height),
        ScopeRefFovX = offsetof(refdef_t,fov_x), ScopeRefFovY = offsetof(refdef_t,fov_y),
        ScopeRefTime = offsetof(refdef_t,time) };
    static const float scopeDefault = 250.0f, scopeScale = 1.2f, scopeHalf = 0.5f;
    static const float scopeOne = 1.0f;
    static const float scopeCenterX = 426.0f, scopeCenterY = 240.0f;
    static const float scopeBias = 2000.0f, scopeAngleScale = 0.01f, scopeZero = 0.0f;
    static const float scopeFovScale = 0.10563380271196365f;
#else
    refdef_t saved;
#endif
    int weapon=cg.predictedPlayerState.weapon;
    if(!cg_portalScopes.integer || cg.renderingThirdPerson || weapon<0 ||
       weapon>=TCE_MAX_WEAPONS)return;
#if defined(_MSC_VER) && defined(_M_IX86)
    scopeZoom=&weaponDef[weapon].scoped;
    __asm {
        mov eax,scopeZoom
        fld dword ptr [eax]
        fcomp dword ptr [scopeOne]
        fnstsw ax
        xor ecx,ecx
        test ah,41h
        setz cl
        mov scopeAllowed,ecx
    }
    if(!scopeAllowed)return;
#else
    if(!(weaponDef[weapon].scoped>1.0f))return;
#endif
    if(!cg.tceAimActive || !cg.tceAimComplete || cg.tceScopeBlocked ||
       (cg.limboEndCinematicTime>cg.time && cg.showGameView))return;
#if defined(_MSC_VER) && defined(_M_IX86)
    scopeWidth=tce_cg_weapons[weapon].portalScopeWidth;
    __asm {
        fild scopeWidth
        fcom dword ptr [scopeZero]
        fnstsw ax
        test ah,40h
        jz scope_width_present
        fstp st(0)
        fld dword ptr [scopeDefault]
    scope_width_present:
        fmul dword ptr [scopeScale]
        fstp scopeSize
        fld scopeSize
        fmul dword ptr [scopeHalf]
        fld dword ptr [scopeCenterX]
        fsub st(0),st(1)
        fstp x
        fld dword ptr [scopeCenterY]
        fsub st(0),st(1)
        fstp y
        fstp st(0)
    }
    w=h=scopeSize;
    /* Original viewport backup is FILD/FSTP, not a lossless integer copy. */
    __asm {
        mov eax,dword ptr [cg + ScopeCurrentRefdef]
        mov ecx,dword ptr [eax + ScopeRefFovX]
        mov edx,dword ptr [eax + ScopeRefFovY]
        mov dword ptr scopeSavedFov[0],ecx
        mov dword ptr scopeSavedFov[4],edx
        fild dword ptr [eax + ScopeRefX]
        fstp dword ptr scopeSavedViewport[0]
        fild dword ptr [eax + ScopeRefY]
        fstp dword ptr scopeSavedViewport[4]
        fild dword ptr [eax + ScopeRefWidth]
        fstp dword ptr scopeSavedViewport[8]
        fild dword ptr [eax + ScopeRefHeight]
        fstp dword ptr scopeSavedViewport[12]
    }
    AxisCopy(cg.refdef_current->viewaxis,scopeSavedAxis);
    __asm {
        fld dword ptr [cg + ScopeAngles]
        fild dword ptr [cg + ScopeHoldPitch]
        fsub dword ptr [scopeBias]
        fmul dword ptr [scopeAngleScale]
        fadd st(0),st(1)
        fstp dword ptr angles[0]
        fstp st(0)
        fild dword ptr [cg + ScopeHoldYaw]
        fsub dword ptr [scopeBias]
        fmul dword ptr [scopeAngleScale]
        fadd dword ptr [cg + ScopeAngles + 4]
        fstp dword ptr angles[4]
        mov eax,dword ptr [cg + ScopeAngles + 8]
        mov dword ptr angles[8],eax
    }
#else
    scopeSize=(float)tce_cg_weapons[weapon].portalScopeWidth;
    if(scopeSize==0)scopeSize=250;
    scopeSize*=1.2f;
    /* Original stores the window corner before adding free-aim displacement;
     * rounding a center first is not equivalent for non-dyadic scope widths. */
    x=426.0f-scopeSize*0.5f;y=240.0f-scopeSize*0.5f;w=h=scopeSize;
    VectorCopy(cg.predictedPlayerState.viewangles,angles);
    angles[0]+=((float)cg.predictedPlayerState.holdable[5]-2000.0f)*0.01f;
    angles[1]+=((float)cg.predictedPlayerState.holdable[6]-2000.0f)*0.01f;
#endif
    AngleVectors(angles,forward,NULL,NULL);
    AngleVectors(cg.predictedPlayerState.viewangles,NULL,right,up);
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        const float scopeProjectionFov=cg.refdef.fov_x;
        static const float scopeRadiansScale=0.01745329238474369f;
        static const double scopeProjectionWidth=852.0;
        static double (__cdecl * const scopeAsin)(double)=asin;
        /* Original Z/Y/X accumulation and post-asin projection. The cdecl
         * binary64 argument remains an adapter, not the original ST0 ABI. */
        __asm {
            fld dword ptr scopeProjectionFov
            fmul dword ptr scopeRadiansScale
            fstp dword ptr radians
            fld dword ptr right[8]
            fmul dword ptr forward[8]
            fld dword ptr right[4]
            fmul dword ptr forward[4]
            faddp st(1),st(0)
            fld dword ptr right[0]
            fmul dword ptr forward[0]
            faddp st(1),st(0)
            sub esp,8
            fstp qword ptr [esp]
            call dword ptr [scopeAsin]
            add esp,8
            fdiv dword ptr radians
            fmul qword ptr scopeProjectionWidth
            fadd dword ptr x
            fstp dword ptr x
            fld dword ptr up[8]
            fmul dword ptr forward[8]
            fld dword ptr up[4]
            fmul dword ptr forward[4]
            faddp st(1),st(0)
            fld dword ptr up[0]
            fmul dword ptr forward[0]
            faddp st(1),st(0)
            sub esp,8
            fstp qword ptr [esp]
            call dword ptr [scopeAsin]
            add esp,8
            fdiv dword ptr radians
            fmul qword ptr scopeProjectionWidth
            fsubr dword ptr y
            fstp dword ptr y
        }
    }
#else
    saved=*cg.refdef_current;
    radians=cg.refdef.fov_x*0.01745329238474369f;
    x=(float)((asin(DotProduct(forward,right))/radians)*852.0+x);
    y=(float)(y-(asin(DotProduct(forward,up))/radians)*852.0);
#endif
    AnglesToAxis(angles,cg.refdef_current->viewaxis);
    previous=tce_uiCoordinates;tce_uiCoordinates=qtrue;
    CG_AdjustFrom640(&x,&y,&w,&h);tce_uiCoordinates=previous;
    cg.refdef_current->x=CG_TCEViewportInteger(x);cg.refdef_current->y=CG_TCEViewportInteger(y);
    cg.refdef_current->width=CG_TCEViewportInteger(w);cg.refdef_current->height=CG_TCEViewportInteger(h);
#if defined(_MSC_VER) && defined(_M_IX86)
    scopeZoom=&weaponDef[weapon].scoped;
    __asm {
        fld scopeSize
        fmul dword ptr [scopeFovScale]
        mov eax,scopeZoom
        fdiv dword ptr [eax]
        mov edx,dword ptr [cg + ScopeCurrentRefdef]
        fstp dword ptr [edx + ScopeRefFovX]
        mov edx,dword ptr [cg + ScopeCurrentRefdef]
        mov eax,dword ptr [edx + ScopeRefFovX]
        mov dword ptr [edx + ScopeRefFovY],eax
    }
#else
    cg.refdef_current->fov_x=(scopeSize*0.10563380271196365f)/weaponDef[weapon].scoped;
    cg.refdef_current->fov_y=cg.refdef_current->fov_x;
#endif
    cg.tcePortalScopeRendering=qtrue;
    CG_SetupFrustum();CG_DrawSkyBoxPortal(qtrue);
    if(!cg.hyperspace) {
        CG_AddPacketEntities();CG_AddMarks();CG_AddParticles();CG_AddLocalEntities();
        CG_AddSmokeSprites();CG_AddAtmosphericEffects();CG_AddFlameChunks();CG_AddTrails();
    }
    CG_PB_RenderPolyBuffers();CG_DrawMiscGamemodels();CG_DrawMiscClientsprites();
    CG_DrawScreenFade();
    trap_R_RenderScene(cg.refdef_current);
#if defined(_MSC_VER) && defined(_M_IX86)
    cg.refdef_current->x=CG_TCEViewportInteger(scopeSavedViewport[0]);
    cg.refdef_current->y=CG_TCEViewportInteger(scopeSavedViewport[1]);
    cg.refdef_current->width=CG_TCEViewportInteger(scopeSavedViewport[2]);
    cg.refdef_current->height=CG_TCEViewportInteger(scopeSavedViewport[3]);
    __asm {
        fld dword ptr scopeSavedFov[0]
        mov eax,dword ptr [cg + ScopeCurrentRefdef]
        fstp dword ptr [eax + ScopeRefFovX]
        fld dword ptr scopeSavedFov[4]
        mov ecx,dword ptr [cg + ScopeCurrentRefdef]
        fstp dword ptr [ecx + ScopeRefFovY]
    }
    AxisCopy(scopeSavedAxis,cg.refdef_current->viewaxis);
#else
    cg.refdef_current->x=saved.x;cg.refdef_current->y=saved.y;
    cg.refdef_current->width=saved.width;cg.refdef_current->height=saved.height;
    cg.refdef_current->fov_x=saved.fov_x;cg.refdef_current->fov_y=saved.fov_y;
    AxisCopy(saved.viewaxis,cg.refdef_current->viewaxis);
#endif
    cg.tcePortalScopeRendering=qfalse;
    if(cg.tcePortalScopeEntity.hModel) {
        memset(&modelView,0,sizeof(modelView));
        CG_Printf("ELITE DEBUG: draw model\n");
        modelView.rdflags=RDF_NOWORLDMODEL;
        AxisCopy(cg.refdef_current->viewaxis,modelView.viewaxis);
#if defined(_MSC_VER) && defined(_M_IX86)
        /* Original also calls AxisCopy at vieworg: its trailing six words
         * rewrite the first two axis rows with the same source values. */
        {
            typedef char ScopeAdjacentViewVectors[
                offsetof(refdef_t,viewaxis)==offsetof(refdef_t,vieworg)+sizeof(vec3_t)?1:-1];
            AxisCopy((vec3_t *)((byte *)cg.refdef_current+offsetof(refdef_t,vieworg)),
                     (vec3_t *)((byte *)&modelView+offsetof(refdef_t,vieworg)));
        }
#else
        VectorCopy(cg.refdef_current->vieworg,modelView.vieworg);
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
        __asm {
            mov eax,dword ptr [cg + ScopeCurrentRefdef]
            lea edx,modelView
            mov ecx,dword ptr [eax + ScopeRefFovX]
            mov dword ptr [edx + ScopeRefFovX],ecx
            mov ecx,dword ptr [eax + ScopeRefFovY]
            mov dword ptr [edx + ScopeRefFovY],ecx
            mov ecx,dword ptr [eax + ScopeRefX]
            mov dword ptr [edx + ScopeRefX],ecx
            mov ecx,dword ptr [eax + ScopeRefY]
            mov dword ptr [edx + ScopeRefY],ecx
            mov ecx,dword ptr [eax + ScopeRefWidth]
            mov dword ptr [edx + ScopeRefWidth],ecx
            mov ecx,dword ptr [eax + ScopeRefHeight]
            mov dword ptr [edx + ScopeRefHeight],ecx
            mov ecx,dword ptr [eax + ScopeRefTime]
            mov dword ptr [edx + ScopeRefTime],ecx
        }
#else
        modelView.fov_x=cg.refdef_current->fov_x;modelView.fov_y=cg.refdef_current->fov_y;
        modelView.x=cg.refdef_current->x;modelView.y=cg.refdef_current->y;
        modelView.width=cg.refdef_current->width;modelView.height=cg.refdef_current->height;
        modelView.time=cg.refdef_current->time;
#endif
        trap_R_ClearScene();trap_R_AddRefEntityToScene(&cg.tcePortalScopeEntity);
        trap_R_RenderScene(&modelView);
    }
}

void CG_DrawActive( stereoFrame_t stereoView ) {
	float		separation;
	vec3_t		baseOrg;
#if defined(_MSC_VER) && defined(_M_IX86)
	enum { ActiveRefdef = offsetof(cg_t, refdef_current),
		ActiveViewX = offsetof(refdef_t, vieworg),
		ActiveViewY = offsetof(refdef_t, vieworg)+4,
		ActiveViewZ = offsetof(refdef_t, vieworg)+8,
		ActiveAxisX = offsetof(refdef_t, viewaxis)+12,
		ActiveAxisY = offsetof(refdef_t, viewaxis)+16,
		ActiveAxisZ = offsetof(refdef_t, viewaxis)+20,
		ActiveCvarValue = offsetof(vmCvar_t, value) };
	static const float activeHalf = 0.5f, activeNegativeHalf = -0.5f, activeZero = 0.0f;
#endif

	// optionally draw the info screen instead
	if ( !cg.snap ) {
		CG_DrawInformation( qfalse );
		return;
	}

	// optionally draw the tournement scoreboard instead
	/*if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
		( cg.snap->ps.pm_flags & PMF_SCOREBOARD ) ) {
		CG_DrawTourneyScoreboard();
		return;
	}*/

	switch ( stereoView ) {
	case STEREO_CENTER:
		separation = 0;
		break;
	case STEREO_LEFT:
		#if defined(_MSC_VER) && defined(_M_IX86)
		__asm { fld dword ptr [cg_stereoSeparation + ActiveCvarValue]
			fmul dword ptr [activeNegativeHalf]
			fstp separation }
		#else
		separation = -cg_stereoSeparation.value / 2;
		#endif
		break;
	case STEREO_RIGHT:
		#if defined(_MSC_VER) && defined(_M_IX86)
		__asm { fld dword ptr [cg_stereoSeparation + ActiveCvarValue]
			fmul dword ptr [activeHalf]
			fstp separation }
		#else
		separation = cg_stereoSeparation.value / 2;
		#endif
		break;
	default:
		separation = 0;
		CG_Error( "CG_DrawActive: Undefined stereoView" );
	}


	// clear around the rendered view if sized down
	CG_TileClear();

	// offset vieworg appropriately if we're doing stereo separation
#if defined(_MSC_VER) && defined(_M_IX86)
	__asm {
		mov ecx, dword ptr [cg + ActiveRefdef]
		fld separation
		mov eax, dword ptr [ecx + ActiveViewX]
		mov edx, dword ptr [ecx + ActiveViewY]
		fcomp dword ptr [activeZero]
		mov dword ptr baseOrg[0], eax
		mov eax, dword ptr [ecx + ActiveViewZ]
		mov dword ptr baseOrg[8], eax
		mov dword ptr baseOrg[4], edx
		fnstsw ax
		test ah, 40h
		jnz active_stereo_offset_done
		fld separation
		fchs
		fld st(0)
		fmul dword ptr [ecx + ActiveAxisX]
		fadd dword ptr [ecx + ActiveViewX]
		fstp dword ptr [ecx + ActiveViewX]
		mov eax, dword ptr [cg + ActiveRefdef]
		fld st(0)
		fmul dword ptr [eax + ActiveAxisY]
		fadd dword ptr [eax + ActiveViewY]
		fstp dword ptr [eax + ActiveViewY]
		mov eax, dword ptr [cg + ActiveRefdef]
		fmul dword ptr [eax + ActiveAxisZ]
		fadd dword ptr [eax + ActiveViewZ]
		fstp dword ptr [eax + ActiveViewZ]
	active_stereo_offset_done:
	}
#else
	VectorCopy( cg.refdef_current->vieworg, baseOrg );
	if ( separation != 0 ) {
		VectorMA( cg.refdef_current->vieworg, -separation, cg.refdef_current->viewaxis[1], cg.refdef_current->vieworg );
	}
#endif

	cg.refdef_current->glfog.registered = 0;	// make sure it doesn't use fog from another scene

	CG_ActivateLimboMenu();

//	if( cgs.ccCurrentCamObjective == -1 ) {
//		if( cg.showGameView ) {
//			CG_FillRect( 0, 0, 640, 480, colorBlack );
//			CG_LimboPanel_Draw();
//			return;
//		}
//	}

	if ( cg.showGameView ) {
 		float x, y, w, h;
		extern qboolean tce_uiCoordinates;
		qboolean previousCoordinates = tce_uiCoordinates;
		x = 0;
		y = 0;
		w = 640;
		h = 480;

        /* Original 3002249b calls the TC transform, also outside scope mode. */
        tce_uiCoordinates = qtrue;
 		CG_AdjustFrom640( &x, &y, &w, &h );
        tce_uiCoordinates = previousCoordinates;

 		cg.refdef_current->x = CG_TCEViewportInteger(x);
 		cg.refdef_current->y = CG_TCEViewportInteger(y);
 		cg.refdef_current->width = CG_TCEViewportInteger(w);
 		cg.refdef_current->height = CG_TCEViewportInteger(h);

		CG_Letterbox( 100, 100, qfalse );
	}

	CG_ShakeCamera();		// NERVE - SMF

	// Gordon
	CG_PB_RenderPolyBuffers();

	// Gordon
	CG_DrawMiscGamemodels();
	CG_DrawMiscClientsprites();

	if( !(cg.limboEndCinematicTime > cg.time && cg.showGameView) ) {
		trap_R_RenderScene( cg.refdef_current );
	}

	CG_DrawTCEScopeScene();

	// restore original viewpoint if running stereo
#if defined(_MSC_VER) && defined(_M_IX86)
	__asm {
		fld separation
		fcomp dword ptr [activeZero]
		fnstsw ax
		test ah, 40h
		jnz active_stereo_restore_done
		lea edx, baseOrg
		fld dword ptr [edx]
		mov ecx, dword ptr [cg + ActiveRefdef]
		fstp dword ptr [ecx + ActiveViewX]
		fld dword ptr [edx+4]
		mov ecx, dword ptr [cg + ActiveRefdef]
		fstp dword ptr [ecx + ActiveViewY]
		fld dword ptr [edx+8]
		mov ecx, dword ptr [cg + ActiveRefdef]
		fstp dword ptr [ecx + ActiveViewZ]
	active_stereo_restore_done:
	}
#else
	if ( separation != 0 ) {
		VectorCopy( baseOrg, cg.refdef_current->vieworg );
	}
#endif

	if( !cg.showGameView ) {
		// draw status bar and other floating elements
		CG_Draw2D();
#ifdef FEATURE_OMNIBOT
        OmnibotRenderDebugText();
#endif
	} else {
		CG_LimboPanel_Draw();
	}
	if (cg.tceShowLimboPanel) {
		CG_LimboPanel_Draw();
	}
}
