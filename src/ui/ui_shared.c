#ifndef CGAMEDLL
#include "tce_video_modes.h"
#endif
// 
// string allocation/managment

#include "ui_shared.h"
#include "ui_local.h"	// For CS settings/retrieval


#define SCROLL_TIME_START					500
#define SCROLL_TIME_ADJUST				150
#define SCROLL_TIME_ADJUSTOFFSET	40
#define SCROLL_TIME_FLOOR					20

typedef struct scrollInfo_s {
	int nextScrollTime;
	int nextAdjustTime;
	int adjustValue;
	int scrollKey;
	float xStart;
	float yStart;
	itemDef_t *item;
	qboolean scrollDir;
} scrollInfo_t;

static scrollInfo_t scrollInfo;

static void (*captureFunc) (void *p) = NULL;
static void *captureData = NULL;
static itemDef_t *itemCapture = NULL;   // item that has the mouse captured ( if any )

displayContextDef_t *DC = NULL;

qboolean g_waitingForKey = qfalse;
qboolean g_editingField = qfalse;

itemDef_t *g_bindItem = NULL;
itemDef_t *g_editItem = NULL;

menuDef_t Menus[MAX_MENUS];      // defined menus
int menuCount = 0;               // how many

// TTimo
// a stack for modal menus only, stores the menus to come back to
// (an item can be NULL, goes back to main menu / no action required)
menuDef_t *modalMenuStack[MAX_MODAL_MENUS];
int modalMenuCount = 0;

static qboolean debugMode = qfalse;

#define DOUBLE_CLICK_DELAY 300
static int lastListBoxClickTime = 0;

void Item_MouseLeave(itemDef_t *item);
void Item_SetMouseOver(itemDef_t *item, qboolean focus);
void Item_Paint(itemDef_t *item);
void Item_RunScript(itemDef_t *item, qboolean *bAbort, const char *s);
void Item_SetupKeywordHash(void);
void Menu_SetupKeywordHash(void);
int BindingIDFromName(const char *name);
qboolean Item_Bind_HandleKey(itemDef_t *item, int key, qboolean down);
itemDef_t *Menu_SetPrevCursorItem(menuDef_t *menu);
itemDef_t *Menu_SetNextCursorItem(menuDef_t *menu);
static qboolean Menu_OverActiveItem(menuDef_t *menu, float x, float y);

#ifdef CGAME
#define MEM_POOL_SIZE  128 * 1024
#else
#define MEM_POOL_SIZE  1536 * 1024	// Arnout: was 1024
#endif

static char		memoryPool[MEM_POOL_SIZE];
static int		allocPoint, outOfMemory;


void Tooltip_Initialize(itemDef_t *item)
{
	item->text = NULL;
	item->font = UI_FONT_COURBD_21;
	item->textalignx = 3;
	item->textaligny = 10;
	item->textscale = .2f;
	item->window.border = WINDOW_BORDER_FULL;
	item->window.borderSize = 1.f;
	item->window.flags &= ~WINDOW_VISIBLE;
	item->window.flags |= (WINDOW_DRAWALWAYSONTOP|WINDOW_AUTOWRAPPED);
	Vector4Set( item->window.backColor, .9f, .9f, .75f, 1.f );
	Vector4Set( item->window.borderColor, 0.f, 0.f, 0.f, 1.f );
	Vector4Set( item->window.foreColor, 0.f, 0.f, 0.f, 1.f );
}

void Tooltip_ComputePosition(itemDef_t *item)
{
	Rectangle *itemRect = &item->window.rectClient;
	Rectangle *tipRect = &item->toolTipData->window.rectClient;

	DC->textFont( item->toolTipData->font );

	// Set positioning based on item location
	// TC 400147bf: multiply by float 1/3 and retain x87 precision until store.
	tipRect->x = (float)((double)itemRect->w * (double)(1.0f / 3.0f) + itemRect->x);
	tipRect->y = (float)((double)itemRect->h + itemRect->y + 8.0);
	//tipRect->h = 14.0f;
	//tipRect->w = DC->textWidth( item->toolTipData->text, item->toolTipData->textscale, 0 ) + 6.0f;
	tipRect->h = DC->multiLineTextHeight( item->toolTipData->text, item->toolTipData->textscale, 0 ) + 9.f;
	tipRect->w = DC->multiLineTextWidth( item->toolTipData->text, item->toolTipData->textscale, 0 ) + 6.f;
	if((double)tipRect->w + tipRect->x > 635.0)
		tipRect->x = (float)((double)tipRect->x - ((double)tipRect->w + tipRect->x - 635.0));

	item->toolTipData->parent = item->parent;
	item->toolTipData->type = ITEM_TYPE_TEXT;
	item->toolTipData->window.style = WINDOW_STYLE_FILLED;
	item->toolTipData->window.flags |= WINDOW_VISIBLE;
}


/*
===============
UI_Alloc
===============
*/				  
void *UI_Alloc( int size ) {
	char	*p; 

	if ( allocPoint + size > MEM_POOL_SIZE ) {
		outOfMemory = qtrue;
		if (DC->Print) {
			DC->Print("UI_Alloc: Failure. Out of memory!\n");
		}
    //DC->trap_Print(S_COLOR_YELLOW"WARNING: UI Out of Memory!\n");
		return NULL;
	}

	p = &memoryPool[allocPoint];

	allocPoint += ( size + 15 ) & ~15;

	return p;
}

/*
===============
UI_InitMemory
===============
*/
void UI_InitMemory( void ) {
	allocPoint = 0;
	outOfMemory = qfalse;
}

qboolean UI_OutOfMemory() {
	return outOfMemory;
}





#define HASH_TABLE_SIZE 2048
/*
================
return a hash value for the string
================
*/
static long hashForString(const char *str) {
	unsigned int i;
	unsigned int hash;
	char	letter;

	hash = 0;
	i = 0;
	while (str[i] != '\0') {
		letter = tolower(str[i]);
		/* TC773: original IMUL/ADD wraps modulo 32 bits before masking. */
		hash += (unsigned int)(int)letter * (i + 119u);
		i++;
	}
	hash &= (HASH_TABLE_SIZE-1);
	return hash;
}

typedef struct stringDef_s {
	struct stringDef_s *next;
	const char *str;
} stringDef_t;

static int strPoolIndex = 0;
static char strPool[STRING_POOL_SIZE];

static int strHandleCount = 0;
static stringDef_t *strHandle[HASH_TABLE_SIZE];


const char *String_Alloc(const char *p) {
	int len;
	long hash;
	stringDef_t *str, *last;
	static const char *staticNULL = "";

	if (p == NULL) {
		return NULL;
	}

	if (*p == 0) {
		return staticNULL;
	}

	hash = hashForString(p);

	str = strHandle[hash];
	while (str) {
		if (strcmp(p, str->str) == 0) {
			return str->str;
		}
		str = str->next;
	}

	len = Q_strlenInt(p);
	if (len + strPoolIndex + 1 < STRING_POOL_SIZE) {
		int ph = strPoolIndex;
		strcpy(&strPool[strPoolIndex], p);
		strPoolIndex += len + 1;

		str = strHandle[hash];
		last = str;
		while (str && str->next) {
			str = str->next;
			last = str;
		}

		str  = UI_Alloc(sizeof(stringDef_t));
		str->next = NULL;
		str->str = &strPool[ph];
		if (last) {
			last->next = str;
		} else {
			strHandle[hash] = str;
		}
		return &strPool[ph];
	}
	return NULL;
}

void String_Report() {
	double f;
	Com_Printf("Memory/String Pool Info\n");
	Com_Printf("----------------\n");
	f = strPoolIndex;
	f *= (double)(1.0f / (STRING_POOL_SIZE));
	f *= 100;
	Com_Printf("String Pool is %.1f%% full, %i bytes out of %i used.\n", f, strPoolIndex, STRING_POOL_SIZE);
	f = allocPoint;
	f *= (double)(1.0f / (MEM_POOL_SIZE));
	f *= 100;
	Com_Printf("Memory Pool is %.1f%% full, %i bytes out of %i used.\n", f, allocPoint, MEM_POOL_SIZE);
}

/*
=================
String_Init
=================
*/
void String_Init() {
	int i;
	for (i = 0; i < HASH_TABLE_SIZE; i++) {
		strHandle[i] = 0;
	}
	strHandleCount = 0;
	strPoolIndex = 0;
	menuCount = 0;
	modalMenuCount = 0;
	UI_InitMemory();
	Item_SetupKeywordHash();
	Menu_SetupKeywordHash();
	if (DC && DC->getBindingBuf) {
		Controls_GetConfig();
	}
}

/*
=================
LerpColor
	lerp and clamp each component of <a> and <b> into <c> by the fraction <t>
=================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static const float tceLerp776Zero = 0.0f;
static const double tceLerp776One = 1.0;
__declspec(naked) void LerpColor(vec4_t a, vec4_t b, vec4_t c, float t) {
	__asm {
		mov eax,[esp+4]
		mov edx,[esp+8]
		mov ecx,[esp+12]
		push esi
		mov esi,eax
		push edi
		sub edx,eax
		sub esi,ecx
		mov edi,4
	tceLerp776Loop:
		lea eax,[esi+ecx]
		fld dword ptr [eax+edx]
		fsub dword ptr [eax]
		fmul dword ptr [esp+24]
		fadd dword ptr [eax]
		fst dword ptr [esp+20]
		fstp dword ptr [ecx]
		fld dword ptr [esp+20]
		fcomp tceLerp776Zero
		fnstsw ax
		test ah,1
		jz tceLerp776Upper
		mov dword ptr [ecx],0
		jmp tceLerp776Next
	tceLerp776Upper:
		fld dword ptr [esp+20]
		fcomp tceLerp776One
		fnstsw ax
		test ah,041h
		jnz tceLerp776Next
		mov dword ptr [ecx],03f800000h
	tceLerp776Next:
		add ecx,4
		dec edi
		jnz tceLerp776Loop
		pop edi
		pop esi
		ret
	}
}
#else
void LerpColor(vec4_t a, vec4_t b, vec4_t c, float t) {
	int i;
	for (i=0; i<4; i++)
	{
		/* TC Windows keeps subtract/multiply/add in x87 precision and rounds
		 * only the completed channel to float before the clamp. */
		c[i] = (float)((double)a[i] + (double)t * ((double)b[i] - (double)a[i]));
		if (c[i] < 0)
			c[i] = 0;
		else if (c[i] > 1.0)
			c[i] = 1.0;
	}
}
#endif

/*
=================
Float_Parse
=================
*/
qboolean Float_Parse(char **p, float *f) {
	char	*token;
	token = COM_ParseExt(p, qfalse);
	if (token && token[0] != 0) {
		*f = atof(token);
		return qtrue;
	} else {
		return qfalse;
	}
}

/*
=================
Color_Parse
=================
*/
qboolean Color_Parse(char **p, vec4_t *c) {
	int i;
	float f=0.0f;

	for (i = 0; i < 4; i++) {
		if (!Float_Parse(p, &f)) {
			return qfalse;
		}
		(*c)[i] = f;
	}
	return qtrue;
}

/*
=================
Int_Parse
=================
*/
qboolean Int_Parse(char **p, int *i) {
	char	*token;
	token = COM_ParseExt(p, qfalse);

	if (token && token[0] != 0) {
		*i = atoi(token);
		return qtrue;
	} else {
		return qfalse;
	}
}

/*
=================
Rect_Parse
=================
*/
qboolean Rect_Parse(char **p, rectDef_t *r) {
	if (Float_Parse(p, &r->x)) {
		if (Float_Parse(p, &r->y)) {
			if (Float_Parse(p, &r->w)) {
				if (Float_Parse(p, &r->h)) {
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
=================
String_Parse
=================
*/
qboolean String_Parse(char **p, const char **out) {
	char *token;

	token = COM_ParseExt(p, qfalse);
	if (token && token[0] != 0) {
		*(out) = String_Alloc(token);
		return qtrue;
	}
	return qfalse;
}

// NERVE - SMF
/*
=================
PC_Char_Parse
=================
*/
qboolean PC_Char_Parse(int handle, char *out) {
	pc_token_t token;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;

	*(out) = token.string[0];
    return qtrue;
}
// -NERVE - SMF

/*
=================
PC_Script_Parse
=================
*/
qboolean PC_Script_Parse(int handle, const char **out) {
	char script[4096];
	pc_token_t token;

	memset(script, 0, sizeof(script));
	// scripts start with { and have ; separated command lists.. commands are command, arg.. 
	// basically we want everything between the { } as it will be interpreted at run time
  
	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (Q_stricmp(token.string, "{") != 0) {
	    return qfalse;
	}

	while ( 1 ) {
		if (!trap_PC_ReadToken(handle, &token))
			return qfalse;

		if (Q_stricmp(token.string, "}") == 0) {
			*out = String_Alloc(script);
			return qtrue;
		}

		if (token.string[1] != '\0') {
			Q_strcat(script, 4096, va("\"%s\"", token.string));
		} else {
			Q_strcat(script, 4096, token.string);
		}
		Q_strcat(script, 4096, " ");
	}
	return qfalse; 	// bk001105 - LCC   missing return value
}

// display, window, menu, item code
// 

/*
==================
Init_Display

Initializes the display with a structure to all the drawing routines
 ==================
*/
void Init_Display(displayContextDef_t *dc) {
	DC = dc;
}



// type and style painting 

void GradientBar_Paint(rectDef_t *rect, vec4_t color) {
	// gradient bar takes two paints
	DC->setColor( color );
	DC->drawHandlePic(rect->x, rect->y, rect->w, rect->h, DC->Assets.gradientBar);
	DC->setColor( NULL );
}


/*
==================
Window_Init

Initializes a window structure ( windowDef_t ) with defaults
 
==================
*/
void Window_Init(Window *w) {
	memset(w, 0, sizeof(windowDef_t));
	w->borderSize = 1;
	w->foreColor[0] = w->foreColor[1] = w->foreColor[2] = w->foreColor[3] = 1.0;
	w->cinematic = -1;
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const double fade787Zero=0.;
enum { fade787Time=offsetof(displayContextDef_t,realTime) };
/* FST intentionally retains the unrounded result for the following comparison. */
__declspec(naked) void Fade(int *flags, float *f, float clamp, int *nextTime, int offsetTime, qboolean bFlags, float fadeAmount) {
 __asm {
 MOV ECX,dword ptr [ESP + 0x4]
 PUSH ESI
 TEST byte ptr [ECX],0x60
 JZ fade787_3007fd85
 MOV EAX,DC
 MOV EDX,dword ptr [ESP + 0x14]
 MOV EAX,dword ptr [EAX + fade787Time]
 MOV ESI,dword ptr [EDX]
 CMP EAX,ESI
 JLE fade787_3007fd85
 MOV ESI,dword ptr [ESP + 0x18]
 ADD EAX,ESI
 MOV dword ptr [EDX],EAX
 MOV AL,byte ptr [ECX]
 TEST AL,0x20
 JZ fade787_3007fd56
 MOV EAX,dword ptr [ESP + 0xc]
 FLD dword ptr [EAX]
 FSUB dword ptr [ESP + 0x20]
 FST dword ptr [EAX]
 MOV EAX,dword ptr [ESP + 0x1c]
 TEST EAX,EAX
 JZ fade787_3007fd83
 FCOMP qword ptr fade787Zero
 FNSTSW AX
 TEST AH,0x41
 JZ fade787_3007fd85
 MOV EAX,dword ptr [ECX]
 POP ESI
 AND AL,0xdb
 MOV dword ptr [ECX],EAX
 RET
fade787_3007fd56:
 MOV EDX,dword ptr [ESP + 0xc]
 FLD dword ptr [ESP + 0x20]
 FADD dword ptr [EDX]
 FST dword ptr [EDX]
 FCOMP dword ptr [ESP + 0x10]
 FNSTSW AX
 TEST AH,0x1
 JNZ fade787_3007fd85
 MOV EAX,dword ptr [ESP + 0x10]
 MOV dword ptr [EDX],EAX
 MOV EAX,dword ptr [ESP + 0x1c]
 TEST EAX,EAX
 JZ fade787_3007fd85
 MOV EAX,dword ptr [ECX]
 POP ESI
 AND AL,0xbf
 MOV dword ptr [ECX],EAX
 RET
fade787_3007fd83:
 FSTP st(0)
fade787_3007fd85:
 POP ESI
 RET
 }
}
#else
void Fade(int *flags, float *f, float clamp, int *nextTime, int offsetTime, qboolean bFlags, float fadeAmount) {
	if (*flags & (WINDOW_FADINGOUT | WINDOW_FADINGIN)) {
		if (DC->realTime > *nextTime) {
			*nextTime = DC->realTime + offsetTime;
			if (*flags & WINDOW_FADINGOUT) {
				*f -= fadeAmount;
				if (bFlags && *f <= 0.0) {
					*flags &= ~(WINDOW_FADINGOUT | WINDOW_VISIBLE);
				}
			} else {
				*f += fadeAmount;
				if (*f >= clamp) {
					*f = clamp;
					if (bFlags) {
						*flags &= ~WINDOW_FADINGIN;
					}
				}
			}
		}
	}
}



#endif

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static int TCE_TextInteger788(void);
static const float wp798Zero = 0.0f, wp798One = 1.0f;
enum {
 wp798W0 = offsetof(windowDef_t, rect.x),
 wp798W4 = offsetof(windowDef_t, rect.y),
 wp798W8 = offsetof(windowDef_t, rect.w),
 wp798W30 = offsetof(windowDef_t, cinematic),
 wp798W34 = offsetof(windowDef_t, style),
 wp798W38 = offsetof(windowDef_t, border),
 wp798W44 = offsetof(windowDef_t, borderSize),
 wp798W48 = offsetof(windowDef_t, flags),
 wp798W70 = offsetof(windowDef_t, nextTime),
 wp798W74 = offsetof(windowDef_t, foreColor),
 wp798W84 = offsetof(windowDef_t, backColor),
 wp798W90 = offsetof(windowDef_t, backColor) + 3 * sizeof(float),
 wp798W94 = offsetof(windowDef_t, borderColor),
 wp798Wc = offsetof(windowDef_t, rect.h),
 wp798W2c = offsetof(windowDef_t, cinematicName),
 wp798Wb4 = offsetof(windowDef_t, background),
 wp798D4 = offsetof(displayContextDef_t, setColor),
 wp798D8 = offsetof(displayContextDef_t, drawHandlePic),
 wp798D40 = offsetof(displayContextDef_t, drawRect),
 wp798D44 = offsetof(displayContextDef_t, drawSides),
 wp798D48 = offsetof(displayContextDef_t, drawTopBottom),
 wp798D3c = offsetof(displayContextDef_t, fillRect),
 wp798D6c = offsetof(displayContextDef_t, getTeamColor),
 wp798Dec = offsetof(displayContextDef_t, playCinematic),
 wp798Df4 = offsetof(displayContextDef_t, drawCinematic),
 wp798Df8 = offsetof(displayContextDef_t, runCinematicFrame),
};
typedef char wp798RectLayout[(offsetof(windowDef_t,rect)==0 && offsetof(rectDef_t,x)==0 && offsetof(rectDef_t,y)==4 && offsetof(rectDef_t,w)==8 && offsetof(rectDef_t,h)==12)?1:-1];
__declspec(naked) void Window_Paint(Window *w, float fadeAmount, float fadeClamp, float fadeCycle) {
 __asm {
 SUB ESP,0x30
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x38]
 MOV EAX,ESI
 MOV ECX,dword ptr [EAX + wp798W0]
 MOV dword ptr [ESP + 0x4],ECX
 MOV EDX,dword ptr [EAX + wp798W4]
 MOV dword ptr [ESP + 0x8],EDX
 MOV ECX,dword ptr [EAX + wp798W8]
 MOV dword ptr [ESP + 0xc],ECX
 MOV EDX,dword ptr [EAX + wp798Wc]
 MOV EAX,[debugMode]
 TEST EAX,EAX
 MOV dword ptr [ESP + 0x10],EDX
 JZ wp798_3007fe03
 MOV ECX,dword ptr [ESI + wp798Wc]
 MOV EDX,dword ptr [ESI + wp798W8]
 LEA EAX,[ESP + 0x14]
 MOV dword ptr [ESP + 0x20],0x3f800000
 PUSH EAX
 MOV EAX,dword ptr [ESI + wp798W4]
 PUSH 0x3f800000
 PUSH ECX
 MOV ECX,dword ptr [ESI + wp798W0]
 PUSH EDX
 MOV EDX,dword ptr [DC]
 PUSH EAX
 PUSH ECX
 MOV dword ptr [ESP + 0x34],0x3f800000
 MOV dword ptr [ESP + 0x30],0x3f800000
 MOV dword ptr [ESP + 0x2c],0x3f800000
 CALL dword ptr [EDX + wp798D40]
 ADD ESP,0x18
wp798_3007fe03:
 TEST ESI,ESI
 JZ wp798_300801bf
 MOV EAX,dword ptr [ESI + wp798W34]
 TEST EAX,EAX
 JNZ wp798_3007fe1d
 MOV ECX,dword ptr [ESI + wp798W38]
 TEST ECX,ECX
 JZ wp798_300801bf
wp798_3007fe1d:
 MOV ECX,dword ptr [ESI + wp798W38]
 TEST ECX,ECX
 JZ wp798_3007fe55
 FLD dword ptr [ESP + 0x4]
 FADD dword ptr [ESI + wp798W44]
 FSTP dword ptr [ESP + 0x4]
 FLD dword ptr [ESP + 0x8]
 FADD dword ptr [ESI + wp798W44]
 FSTP dword ptr [ESP + 0x8]
 FLD dword ptr [ESI + wp798W44]
 FADD ST(0),ST(0)
 FLD dword ptr [ESP + 0xc]
 FSUB ST(0),ST(1)
 FSTP dword ptr [ESP + 0xc]
 FLD dword ptr [ESP + 0x10]
 FSUB ST(0),ST(1)
 FSTP dword ptr [ESP + 0x10]
 FSTP ST(0)
wp798_3007fe55:
 CMP EAX,0x1
 JNZ wp798_3007ff03
 MOV EAX,dword ptr [ESI + wp798Wb4]
 TEST EAX,EAX
 JZ wp798_3007fed7
 MOV EAX,dword ptr [ESP + 0x3c]
 FLD dword ptr [ESP + 0x44]
 PUSH EAX
 PUSH 0x1
 CALL TCE_TextInteger788
 MOV EDX,dword ptr [ESP + 0x48]
 LEA ECX,[ESI + wp798W70]
 PUSH EAX
 PUSH ECX
 LEA EAX,[ESI + wp798W90]
 PUSH EDX
 LEA ECX,[ESI + wp798W48]
 PUSH EAX
 PUSH ECX
 CALL Fade
 MOV EAX,[DC]
 LEA EDX,[ESI + wp798W84]
 PUSH EDX
 CALL dword ptr [EAX + wp798D4]
 MOV ECX,dword ptr [ESI + wp798Wb4]
 MOV EDX,dword ptr [ESP + 0x30]
 MOV EAX,dword ptr [ESP + 0x2c]
 PUSH ECX
 MOV ECX,dword ptr [ESP + 0x2c]
 PUSH EDX
 MOV EDX,dword ptr [ESP + 0x2c]
 PUSH EAX
 MOV EAX,[DC]
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + wp798D8]
 MOV ECX,dword ptr [DC]
 PUSH 0x0
 CALL dword ptr [ECX + wp798D4]
 ADD ESP,0x38
 JMP wp798_3008002c
wp798_3007fed7:
 MOV EAX,dword ptr [ESP + 0x10]
 MOV ECX,dword ptr [ESP + 0xc]
 LEA EDX,[ESI + wp798W84]
 PUSH EDX
 MOV EDX,dword ptr [ESP + 0xc]
 PUSH EAX
 MOV EAX,dword ptr [ESP + 0xc]
 PUSH ECX
 MOV ECX,dword ptr [DC]
 PUSH EDX
 PUSH EAX
 CALL dword ptr [ECX + wp798D3c]
 ADD ESP,0x14
 JMP wp798_3008002c
wp798_3007ff03:
 CMP EAX,0x2
 JNZ wp798_3007ff21
 LEA EDX,[ESI + wp798W84]
 LEA EAX,[ESP + 0x4]
 PUSH EDX
 PUSH EAX
 CALL GradientBar_Paint
 ADD ESP,0x8
 JMP wp798_3008002c
wp798_3007ff21:
 CMP EAX,0x3
 JNZ wp798_3007ff71
 MOV EAX,dword ptr [ESI + wp798W48]
 TEST AH,0x2
 JZ wp798_3007ff3e
 MOV EDX,dword ptr [DC]
 LEA ECX,[ESI + wp798W74]
 PUSH ECX
 CALL dword ptr [EDX + wp798D4]
 ADD ESP,0x4
wp798_3007ff3e:
 MOV EAX,dword ptr [ESI + wp798Wb4]
 MOV ECX,dword ptr [ESP + 0x10]
 MOV EDX,dword ptr [ESP + 0xc]
 PUSH EAX
 MOV EAX,dword ptr [ESP + 0xc]
 PUSH ECX
 MOV ECX,dword ptr [ESP + 0xc]
 PUSH EDX
 MOV EDX,dword ptr [DC]
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + wp798D8]
 MOV EAX,[DC]
 PUSH 0x0
 CALL dword ptr [EAX + wp798D4]
 JMP wp798_30080029
wp798_3007ff71:
 CMP EAX,0x4
 JNZ wp798_3007ffb2
 MOV ECX,dword ptr [DC]
 MOV EAX,dword ptr [ECX + wp798D6c]
 TEST EAX,EAX
 JZ wp798_3008002c
 LEA EDX,[ESP + 0x14]
 PUSH EDX
 CALL EAX
 MOV ECX,dword ptr [ESP + 0x14]
 MOV EDX,dword ptr [ESP + 0x10]
 LEA EAX,[ESP + 0x18]
 PUSH EAX
 MOV EAX,dword ptr [ESP + 0x10]
 PUSH ECX
 MOV ECX,dword ptr [ESP + 0x10]
 PUSH EDX
 MOV EDX,dword ptr [DC]
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + wp798D3c]
 JMP wp798_30080029
wp798_3007ffb2:
 CMP EAX,0x5
 JNZ wp798_3008002c
 CMP dword ptr [ESI + wp798W30],-0x1
 JNZ wp798_3007fff3
 MOV EAX,dword ptr [ESP + 0x10]
 MOV ECX,dword ptr [ESP + 0xc]
 MOV EDX,dword ptr [ESP + 0x8]
 PUSH EAX
 MOV EAX,dword ptr [ESP + 0x8]
 PUSH ECX
 MOV ECX,dword ptr [ESI + wp798W2c]
 PUSH EDX
 MOV EDX,dword ptr [DC]
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + wp798Dec]
 ADD ESP,0x14
 CMP EAX,-0x1
 MOV dword ptr [ESI + wp798W30],EAX
 JNZ wp798_3007fff3
 MOV dword ptr [ESI + wp798W30],0xfffffffe
wp798_3007fff3:
 MOV EAX,dword ptr [ESI + wp798W30]
 TEST EAX,EAX
 JL wp798_3008002c
 PUSH EAX
 MOV EAX,[DC]
 CALL dword ptr [EAX + wp798Df8]
 MOV ECX,dword ptr [ESP + 0x14]
 MOV EDX,dword ptr [ESP + 0x10]
 MOV EAX,dword ptr [ESP + 0xc]
 PUSH ECX
 MOV ECX,dword ptr [ESP + 0xc]
 PUSH EDX
 MOV EDX,dword ptr [ESI + wp798W30]
 PUSH EAX
 MOV EAX,[DC]
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + wp798Df4]
wp798_30080029:
 ADD ESP,0x18
wp798_3008002c:
 MOV EAX,dword ptr [ESI + wp798W38]
 CMP EAX,0x1
 JNZ wp798_300800dd
 CMP dword ptr [ESI + wp798W34],0x4
 JNZ wp798_300800b2
 FLD dword ptr [ESP + 0x14]
 FCOMP dword ptr [wp798Zero]
 FNSTSW AX
 TEST AH,0x41
 JNZ wp798_30080069
 MOV dword ptr [ESP + 0x14],0x3f800000
 MOV dword ptr [ESP + 0x1c],0x3f000000
 MOV dword ptr [ESP + 0x18],0x3f000000
 JMP wp798_30080081
wp798_30080069:
 MOV dword ptr [ESP + 0x1c],0x3f800000
 MOV dword ptr [ESP + 0x18],0x3f000000
 MOV dword ptr [ESP + 0x14],0x3f000000
wp798_30080081:
 MOV EDX,dword ptr [ESI + wp798W44]
 MOV EAX,dword ptr [ESI + wp798Wc]
 LEA ECX,[ESP + 0x14]
 MOV dword ptr [ESP + 0x20],0x3f800000
 PUSH ECX
 MOV ECX,dword ptr [ESI + wp798W8]
 PUSH EDX
 MOV EDX,dword ptr [ESI + wp798W4]
 PUSH EAX
 MOV EAX,dword ptr [ESI + wp798W0]
 PUSH ECX
 MOV ECX,dword ptr [DC]
 PUSH EDX
 PUSH EAX
 CALL dword ptr [ECX + wp798D40]
 ADD ESP,0x18
 POP ESI
 ADD ESP,0x30
 RET
wp798_300800b2:
 MOV EAX,dword ptr [ESI + wp798W44]
 MOV ECX,dword ptr [ESI + wp798Wc]
 LEA EDX,[ESI + wp798W94]
 PUSH EDX
 MOV EDX,dword ptr [ESI + wp798W8]
 PUSH EAX
 MOV EAX,dword ptr [ESI + wp798W4]
 PUSH ECX
 MOV ECX,dword ptr [ESI + wp798W0]
 PUSH EDX
 MOV EDX,dword ptr [DC]
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + wp798D40]
 ADD ESP,0x18
 POP ESI
 ADD ESP,0x30
 RET
wp798_300800dd:
 CMP EAX,0x2
 JNZ wp798_30080121
 MOV ECX,dword ptr [DC]
 LEA EAX,[ESI + wp798W94]
 PUSH EAX
 CALL dword ptr [ECX + wp798D4]
 MOV EDX,dword ptr [ESI + wp798W44]
 MOV EAX,dword ptr [ESI + wp798Wc]
 MOV ECX,dword ptr [ESI + wp798W8]
 PUSH EDX
 MOV EDX,dword ptr [ESI + wp798W4]
 PUSH EAX
 MOV EAX,dword ptr [ESI + wp798W0]
 PUSH ECX
 MOV ECX,dword ptr [DC]
 PUSH EDX
 PUSH EAX
 CALL dword ptr [ECX + wp798D48]
 MOV EDX,dword ptr [DC]
 PUSH 0x0
 CALL dword ptr [EDX + wp798D4]
 ADD ESP,0x1c
 POP ESI
 ADD ESP,0x30
 RET
wp798_30080121:
 CMP EAX,0x3
 JNZ wp798_30080165
 MOV ECX,dword ptr [DC]
 LEA EAX,[ESI + wp798W94]
 PUSH EAX
 CALL dword ptr [ECX + wp798D4]
 MOV EDX,dword ptr [ESI + wp798W44]
 MOV EAX,dword ptr [ESI + wp798Wc]
 MOV ECX,dword ptr [ESI + wp798W8]
 PUSH EDX
 MOV EDX,dword ptr [ESI + wp798W4]
 PUSH EAX
 MOV EAX,dword ptr [ESI + wp798W0]
 PUSH ECX
 MOV ECX,dword ptr [DC]
 PUSH EDX
 PUSH EAX
 CALL dword ptr [ECX + wp798D44]
 MOV EDX,dword ptr [DC]
 PUSH 0x0
 CALL dword ptr [EDX + wp798D4]
 ADD ESP,0x1c
 POP ESI
 ADD ESP,0x30
 RET
wp798_30080165:
 CMP EAX,0x4
 JNZ wp798_300801bf
 MOV EAX,ESI
 PUSH EDI
 LEA EDI,[ESI + wp798W94]
 MOV ECX,dword ptr [EAX + wp798W0]
 PUSH EDI
 MOV dword ptr [ESP + 0x2c],ECX
 MOV EDX,dword ptr [EAX + wp798W4]
 MOV dword ptr [ESP + 0x30],EDX
 MOV ECX,dword ptr [EAX + wp798W8]
 MOV dword ptr [ESP + 0x34],ECX
 LEA ECX,[ESP + 0x2c]
 MOV EDX,dword ptr [EAX + wp798Wc]
 MOV EAX,dword ptr [ESI + wp798W44]
 MOV dword ptr [ESP + 0x38],EDX
 PUSH ECX
 MOV dword ptr [ESP + 0x3c],EAX
 CALL GradientBar_Paint
 FLD dword ptr [ESI + wp798Wc]
 FADD dword ptr [ESI + wp798W4]
 LEA EDX,[ESP + 0x30]
 PUSH EDI
 PUSH EDX
 FSUB dword ptr [wp798One]
 FSTP dword ptr [ESP + 0x3c]
 CALL GradientBar_Paint
 ADD ESP,0x10
 POP EDI
wp798_300801bf:
 POP ESI
 ADD ESP,0x30
 RET
 }
}
#else
void Window_Paint(Window *w, float fadeAmount, float fadeClamp, float fadeCycle) {
	//float bordersize = 0;
	vec4_t color;
	rectDef_t fillRect = w->rect;

	if (debugMode) {
		color[0] = color[1] = color[2] = color[3] = 1;
		DC->drawRect(w->rect.x, w->rect.y, w->rect.w, w->rect.h, 1, color);
	}

	if (w == NULL || (w->style == 0 && w->border == 0)) {
		return;
	}

	// FIXME: do right thing for right border type
	if (w->border != 0) {
		fillRect.x += w->borderSize;
		fillRect.y += w->borderSize;
		fillRect.w -= 2 * w->borderSize;
		fillRect.h -= 2 * w->borderSize;
	}

	if (w->style == WINDOW_STYLE_FILLED) {
	  // box, but possible a shader that needs filled
	  if (w->background) {
		  Fade(&w->flags, &w->backColor[3], fadeClamp, &w->nextTime, fadeCycle, qtrue, fadeAmount);
		  DC->setColor(w->backColor);
		  DC->drawHandlePic(fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->background);
		  DC->setColor(NULL);
	  } else {
		  DC->fillRect(fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->backColor);
	  }
  } else if (w->style == WINDOW_STYLE_GRADIENT) {
	  GradientBar_Paint(&fillRect, w->backColor);
	  // gradient bar
  } else if (w->style == WINDOW_STYLE_SHADER) {
	  if (w->flags & WINDOW_FORECOLORSET) {
		  DC->setColor(w->foreColor);
	  }
	  DC->drawHandlePic(fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->background);
	  DC->setColor(NULL);
  } else if (w->style == WINDOW_STYLE_TEAMCOLOR) {
	  if (DC->getTeamColor) {
		  DC->getTeamColor(&color);
		  DC->fillRect(fillRect.x, fillRect.y, fillRect.w, fillRect.h, color);
	  }
  } else if (w->style == WINDOW_STYLE_CINEMATIC) {
	  if (w->cinematic == -1) {
		  w->cinematic = DC->playCinematic(w->cinematicName, fillRect.x, fillRect.y, fillRect.w, fillRect.h);
		  if (w->cinematic == -1) {
			  w->cinematic = -2;
		  }
	  } 
	  if (w->cinematic >= 0) {
		  DC->runCinematicFrame(w->cinematic);
		  DC->drawCinematic(w->cinematic, fillRect.x, fillRect.y, fillRect.w, fillRect.h);
	  }
  }
  
  if (w->border == WINDOW_BORDER_FULL) {
	  // full
	  // HACK HACK HACK
	  if (w->style == WINDOW_STYLE_TEAMCOLOR) {
		  if (color[0] > 0) { 
			  // red
			  color[0] = 1;
			  color[1] = color[2] = .5;
			  
		  } else {
			  color[2] = 1;
			  color[0] = color[1] = .5;
		  }
		  color[3] = 1;
		  DC->drawRect(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize, color);
	  } else {
		  DC->drawRect(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize, w->borderColor);
	  }
  } else if (w->border == WINDOW_BORDER_HORZ) {
	  // top/bottom
	  DC->setColor(w->borderColor);
	  DC->drawTopBottom(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize);
	  DC->setColor( NULL );
  } else if (w->border == WINDOW_BORDER_VERT) {
	  // left right
	  DC->setColor(w->borderColor);
	  DC->drawSides(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize);
	  DC->setColor( NULL );
  } else if (w->border == WINDOW_BORDER_KCGRADIENT) {
	  // this is just two gradient bars along each horz edge
	  rectDef_t r = w->rect;
	  r.h = w->borderSize;
	  GradientBar_Paint(&r, w->borderColor);
	  r.y = w->rect.y + w->rect.h - 1;
	  GradientBar_Paint(&r, w->borderColor);
  }
  
}



#endif

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const float sc804Limit=635.f,sc804Zero=0.f;
enum {
 sc804I0=offsetof(itemDef_t,window.rect.x),
 sc804I4=offsetof(itemDef_t,window.rect.y),
 sc804I8=offsetof(itemDef_t,window.rect.w),
 sc804I10=offsetof(itemDef_t,window.rectClient.x),
 sc804I14=offsetof(itemDef_t,window.rectClient.y),
 sc804I18=offsetof(itemDef_t,window.rectClient.w),
 sc804I270=offsetof(itemDef_t,toolTipData),
 sc804Ic=offsetof(itemDef_t,window.rect.h),
 sc804I1c=offsetof(itemDef_t,window.rectClient.h),
 sc804Ic0=offsetof(itemDef_t,textRect.w),
 sc804Ic4=offsetof(itemDef_t,textRect.h),
 sc804Iec=offsetof(itemDef_t,parent),
 sc804MenuX=offsetof(menuDef_t,window.rect.x),
 sc804MenuY=offsetof(menuDef_t,window.rect.y)
};
__declspec(naked) void Item_SetScreenCoords(itemDef_t *item,float x,float y) {
 __asm {
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x8]
 TEST ESI,ESI
 JZ sc804_30080261
 FLD dword ptr [ESP + 0xc]
 FADD dword ptr [ESI + sc804I10]
 MOV EAX,dword ptr [ESI + sc804I18]
 MOV ECX,dword ptr [ESI + sc804I1c]
 MOV dword ptr [ESI + sc804I8],EAX
 MOV EAX,dword ptr [ESI + sc804I270]
 TEST EAX,EAX
 FSTP dword ptr [ESI + sc804I0]
 FLD dword ptr [ESP + 0x10]
 FADD dword ptr [ESI + sc804I14]
 MOV dword ptr [ESI + sc804Ic],ECX
 FSTP dword ptr [ESI + sc804I4]
 JZ sc804_3008024d
 MOV EDX,dword ptr [ESP + 0x10]
 MOV ECX,dword ptr [ESP + 0xc]
 PUSH EDX
 PUSH ECX
 PUSH EAX
 CALL Item_SetScreenCoords
 MOV ECX,dword ptr [ESI + sc804I270]
 ADD ESP,0xc
 FLD dword ptr [ECX + sc804I8]
 FADD dword ptr [ECX + sc804I0]
 FSUB dword ptr [sc804Limit]
 FCOM dword ptr [sc804Zero]
 FNSTSW AX
 TEST AH,0x41
 JNZ sc804_3008024b
 FLD dword ptr [ECX + sc804I10]
 FSUB ST(0),ST(1)
 FSTP dword ptr [ECX + sc804I10]
 MOV EAX,dword ptr [ESI + sc804I270]
 FLD dword ptr [EAX + sc804I0]
 FSUB ST(0),ST(1)
 FSTP dword ptr [EAX + sc804I0]
sc804_3008024b:
 FSTP ST(0)
sc804_3008024d:
 MOV dword ptr [ESI + sc804Ic0],0x0
 MOV dword ptr [ESI + sc804Ic4],0x0
sc804_30080261:
 POP ESI
 RET
 }
}
__declspec(naked) void Item_UpdatePosition(itemDef_t *item) {
 __asm {
 MOV ECX,dword ptr [ESP + 0x4]
 TEST ECX,ECX
 JZ sc804_30080298
 MOV EAX,dword ptr [ECX + sc804Iec]
 TEST EAX,EAX
 JZ sc804_30080298
 FLD dword ptr [EAX + sc804MenuY]
 PUSH ECX
 FSTP dword ptr [ESP]
 FLD dword ptr [EAX + sc804MenuX]
 PUSH ECX
 FSTP dword ptr [ESP]
 PUSH ECX
 CALL Item_SetScreenCoords
 ADD ESP,0xc
sc804_30080298:
 RET
 }
}
#else
void Item_SetScreenCoords(itemDef_t *item, float x, float y) {
	
	if(item == NULL) return;

	item->window.rect.x = x + item->window.rectClient.x;
	item->window.rect.y = y + item->window.rectClient.y;
	item->window.rect.w = item->window.rectClient.w;
	item->window.rect.h = item->window.rectClient.h;

	// FIXME: do the proper thing for the right borders here?
	/*if( item->window.border != 0 ) {
		item->window.rect.x += item->window.borderSize;
		item->window.rect.y += item->window.borderSize;
		item->window.rect.w -= 2 * item->window.borderSize;
		item->window.rect.h -= 2 * item->window.borderSize;
	}*/

	// Don't let tooltips draw off the screen.
	if(item->toolTipData) {
		Item_SetScreenCoords(item->toolTipData, x, y);
		{
			float val = (item->toolTipData->window.rect.x + item->toolTipData->window.rect.w) - 635.0f;
			if(val > 0.0f) {
				item->toolTipData->window.rectClient.x -= val;
				item->toolTipData->window.rect.x -= val;
			}
		}
	}

	// force the text rects to recompute
	item->textRect.w = 0;
	item->textRect.h = 0;
}

// FIXME: consolidate this with nearby stuff
void Item_UpdatePosition(itemDef_t *item) {
	float x, y;
	menuDef_t *menu;
	
	if (item == NULL || item->parent == NULL) {
		return;
	}
	
	menu = item->parent;
	
	x = menu->window.rect.x;
	y = menu->window.rect.y;
	
	/*if (menu->window.border != 0) {
		x += menu->window.borderSize;
		y += menu->window.borderSize;
	}*/
	
	Item_SetScreenCoords(item, x, y);
	
}


#endif

// menus
void Menu_UpdatePosition(menuDef_t *menu) {
	int i;
	float x, y;
	
	if (menu == NULL) {
		return;
	}
	
	x = menu->window.rect.x;
	y = menu->window.rect.y;

    /*if (menu->window.border != 0) {
		x += menu->window.borderSize;
		y += menu->window.borderSize;
	}*/
	
	for (i = 0; i < menu->itemCount; i++) {
		Item_SetScreenCoords(menu->items[i], x, y);
	}
}

void Menu_PostParse(menuDef_t *menu) {
	if (menu == NULL) {
		return;
	}
	if (menu->fullScreen) {
		menu->window.rect.x = 0;
		menu->window.rect.y = 0;
		menu->window.rect.w = 640;
		menu->window.rect.h = 480;
	}
	Menu_UpdatePosition(menu);
}

/* TC 40015520: leaveFocus runs for every item, even without HASFOCUS.
 * Keep this ordering; session_focus evidence and whole-DLL test cover it. */
itemDef_t *Menu_ClearFocus(menuDef_t *menu) {
	int i;
	itemDef_t *ret = NULL;
	
	if(menu == NULL) return(NULL);
	
	for(i=0; i<menu->itemCount; i++) {
		if(menu->items[i]->window.flags & WINDOW_HASFOCUS) {
			ret = menu->items[i];
			menu->items[i]->window.flags &= ~WINDOW_HASFOCUS;
		}

		if(menu->items[i]->window.flags & WINDOW_MOUSEOVER) {
			Item_MouseLeave(menu->items[i]);
			Item_SetMouseOver(menu->items[i], qfalse);
		}

		if(menu->items[i]->leaveFocus) {
			Item_RunScript(menu->items[i], NULL, menu->items[i]->leaveFocus);
		}
	}
	
	return(ret);
}

qboolean IsVisible(int flags) {
	return (flags & WINDOW_VISIBLE && !(flags & WINDOW_FADINGOUT));
}

qboolean Rect_ContainsPoint(rectDef_t *rect, float x, float y) {
	if (rect) {
		if (x > rect->x && x < rect->x + rect->w && y > rect->y && y < rect->y + rect->h) {
			return qtrue;
		}
	}
	return qfalse;
}

int Menu_ItemsMatchingGroup(menuDef_t *menu, const char *name) {
	int i;
	int count = 0;
	char *pdest;
	int wildcard = -1;	// if wildcard is set, it's value is the number of characters to compare


	pdest = strstr( name, "*" );	// allow wildcard strings (ex.  "hide nb_*" would translate to "hide nb_pg1; hide nb_extra" etc)
	if(pdest)
		wildcard = pdest - name;

	for (i = 0; i < menu->itemCount; i++) {
		if(wildcard != -1) {
			if (Q_strncmp(menu->items[i]->window.name, name, wildcard) == 0 || (menu->items[i]->window.group && Q_strncmp(menu->items[i]->window.group, name, wildcard) == 0)) {
				count++;
			} 
		} else {
			if (Q_stricmp(menu->items[i]->window.name, name) == 0 || (menu->items[i]->window.group && Q_stricmp(menu->items[i]->window.group, name) == 0)) {
				count++;
			} 
		}
	}

	return count;
}

itemDef_t *Menu_GetMatchingItemByNumber(menuDef_t *menu, int index, const char *name) {
	int i;
	int count = 0;
	char *pdest;
	int wildcard = -1;	// if wildcard is set, it's value is the number of characters to compare

	pdest = strstr( name, "*" );	// allow wildcard strings (ex.  "hide nb_*" would translate to "hide nb_pg1; hide nb_extra" etc)
	if(pdest)
		wildcard = pdest - name;

	for (i = 0; i < menu->itemCount; i++) {
		if(wildcard != -1) {
			if (Q_strncmp(menu->items[i]->window.name, name, wildcard) == 0 || (menu->items[i]->window.group && Q_strncmp(menu->items[i]->window.group, name, wildcard) == 0)) {
				if (count == index)
					return menu->items[i];
				count++;
			}
		} else {
			if (Q_stricmp(menu->items[i]->window.name, name) == 0 || (menu->items[i]->window.group && Q_stricmp(menu->items[i]->window.group, name) == 0)) {
				if (count == index)
					return menu->items[i];
				count++;
			}
		} 
	}
	return NULL;
}

void Script_SetColor(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name = NULL;
	int i;
	float f = 0.0f;
	vec4_t *out;
	// expecting type of color to set and 4 args for the color
	if (String_Parse(args, &name)) {
		out = NULL;
		if (Q_stricmp(name, "backcolor") == 0) {
			out = &item->window.backColor;
			item->window.flags |= WINDOW_BACKCOLORSET;
		} else if (Q_stricmp(name, "forecolor") == 0) {
			out = &item->window.foreColor;
			item->window.flags |= WINDOW_FORECOLORSET;
		} else if (Q_stricmp(name, "bordercolor") == 0) {
			out = &item->window.borderColor;
		}

		if (out) {
			for (i = 0; i < 4; i++) {
				if (!Float_Parse(args, &f)) {
					return;
				}
				(*out)[i] = f;
			}
		}
	}
}

void Script_SetAsset(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name;
	// expecting name to set asset to
	if (String_Parse(args, &name)) {
		// check for a model 
		if (item->type == ITEM_TYPE_MODEL) {
		}
	}
}

void Script_SetBackground(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name = NULL;
	// expecting name to set asset to
	if (String_Parse(args, &name)) {
		item->window.background = DC->registerShaderNoMip(name);
	}
}




itemDef_t *Menu_FindItemByName(menuDef_t *menu, const char *p) {
	int i;

	if (menu == NULL || p == NULL) {
		return NULL;
	}
	
	for (i = 0; i < menu->itemCount; i++) {
		if (Q_stricmp(p, menu->items[i]->window.name) == 0) {
			return menu->items[i];
		}
	}
	
	return NULL;
}

void Script_SetTeamColor(itemDef_t *item, qboolean *bAbort, char **args) {
	if (DC->getTeamColor) {
		int i;
		vec4_t color;
		DC->getTeamColor(&color);
		for (i = 0; i < 4; i++) {
			item->window.backColor[i] = color[i];
		}
	}
}

void Script_SetItemColor(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *itemname = NULL;
	const char *name = NULL;
	vec4_t color;
	int i;
	vec4_t *out;

	// expecting type of color to set and 4 args for the color
	if (String_Parse(args, &itemname) && String_Parse(args, &name)) {
		itemDef_t *item2;
		int j;
		int count = Menu_ItemsMatchingGroup(item->parent, itemname);

		if (!Color_Parse(args, &color)) {
			return;
		}

		for (j = 0; j < count; j++) {
			item2 = Menu_GetMatchingItemByNumber(item->parent, j, itemname);
			if (item2 != NULL) {
				out = NULL;
				if (Q_stricmp(name, "backcolor") == 0) {
					out = &item2->window.backColor;
				} else if (Q_stricmp(name, "forecolor") == 0) {
					out = &item2->window.foreColor;
					item2->window.flags |= WINDOW_FORECOLORSET;
				} else if (Q_stricmp(name, "bordercolor") == 0) {
					out = &item2->window.borderColor;
				}

				if (out) {
					for (i = 0; i < 4; i++) {
						(*out)[i] = color[i];
					}
				}
			}
		}
	}
}

void Script_SetMenuItemColor(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *menuname = NULL;
	const char *itemname = NULL;
	const char *name = NULL;
	vec4_t color;
	int i;
	vec4_t *out;

	// expecting type of color to set and 4 args for the color
	if( String_Parse(args, &menuname) && String_Parse(args, &itemname) && String_Parse(args, &name) ) {
		menuDef_t *menu = Menus_FindByName( menuname );
		itemDef_t *item2;
		int j;
		int count;

		if( !Color_Parse( args, &color ) ) {
			return;
		}
		/* Some original TC menus omit fadebox_black. Still consume the
		 * color arguments so they do not become bogus UI scripts 0/1. */
		if( !menu ) {
			return;
		}
		count = Menu_ItemsMatchingGroup( menu, itemname );

		for (j = 0; j < count; j++) {
			item2 = Menu_GetMatchingItemByNumber( menu, j, itemname );
			if (item2 != NULL) {
				out = NULL;
				if (Q_stricmp(name, "backcolor") == 0) {
					out = &item2->window.backColor;
				} else if (Q_stricmp(name, "forecolor") == 0) {
					out = &item2->window.foreColor;
					item2->window.flags |= WINDOW_FORECOLORSET;
				} else if (Q_stricmp(name, "bordercolor") == 0) {
					out = &item2->window.borderColor;
				}

				if (out) {
					for (i = 0; i < 4; i++) {
						(*out)[i] = color[i];
					}
				}
			}
		}
	}
}


void Menu_ShowItemByName(menuDef_t *menu, const char *p, qboolean bShow) {
	itemDef_t *item;
	int i;
	int count = Menu_ItemsMatchingGroup(menu, p);
	for (i = 0; i < count; i++) {
		item = Menu_GetMatchingItemByNumber(menu, i, p);
		if (item != NULL) {
			if (bShow) {
				item->window.flags |= WINDOW_VISIBLE;
			} else {
				if(item->window.flags & WINDOW_MOUSEOVER) {
					Item_MouseLeave(item);
					Item_SetMouseOver(item, qfalse);
				}

				item->window.flags &= ~WINDOW_VISIBLE;

				// stop cinematics playing in the window
				if (item->window.cinematic >= 0) {
					DC->stopCinematic(item->window.cinematic);
					item->window.cinematic = -1;
				}
			}
		}
	}
}

void Menu_FadeItemByName(menuDef_t *menu, const char *p, qboolean fadeOut) {
  itemDef_t *item;
  int i;
  int count = Menu_ItemsMatchingGroup(menu, p);
  for (i = 0; i < count; i++) {
    item = Menu_GetMatchingItemByNumber(menu, i, p);
    if (item != NULL) {
      if (fadeOut) {
        item->window.flags |= (WINDOW_FADINGOUT | WINDOW_VISIBLE);
        item->window.flags &= ~WINDOW_FADINGIN;
      } else {
        item->window.flags |= (WINDOW_VISIBLE | WINDOW_FADINGIN);
        item->window.flags &= ~WINDOW_FADINGOUT;
      }
    }
  }
}

menuDef_t *Menus_FindByName(const char *p) {
	int i;
	for (i = 0; i < menuCount; i++) {
		if (Q_stricmp(Menus[i].window.name, p) == 0) {
			return &Menus[i];
		} 
	}
	return NULL;
}

void Menus_ShowByName(const char *p) {
	menuDef_t *menu = Menus_FindByName(p);
	if (menu) {
		Menus_Activate(menu);
	}
}

void Menus_OpenByName(const char *p) {
	Menus_ActivateByName(p, qtrue);
}

void Menu_RunCloseScript(menuDef_t *menu) {
	if (menu && menu->window.flags & WINDOW_VISIBLE && menu->onClose) {
		itemDef_t item;
		item.parent = menu;
		Item_RunScript(&item, NULL, menu->onClose);
	}
}

void Menus_CloseByName(const char *p) {
	menuDef_t *menu = Menus_FindByName(p);
	if (menu != NULL) {
		int i;

		// Gordon: make sure no edit fields are left hanging
		for( i = 0; i < menu->itemCount; i++ ) {
			if( g_editItem == menu->items[ i ] ) {
				g_editingField = qfalse;
				g_editItem = NULL;
			}
		}


		menu->cursorItem = -1;
		Menu_ClearFocus(menu);
		Menu_RunCloseScript(menu);
		menu->window.flags &= ~(WINDOW_VISIBLE | WINDOW_HASFOCUS | WINDOW_MOUSEOVER);
		if (menu->window.flags & WINDOW_MODAL)
		{
			if (modalMenuCount <= 0)
				Com_Printf(S_COLOR_YELLOW "WARNING: tried closing a modal window with an empty modal stack!\n");
			else
			{
				modalMenuCount--;
				// if modal doesn't have a parent, the stack item may be NULL .. just go back to the main menu then
				if (modalMenuStack[modalMenuCount])
				{
					Menus_ActivateByName(modalMenuStack[modalMenuCount]->window.name, qfalse); // don't try to push the one we are opening to the stack
				}
			}
		}
	}
}

void Menus_CloseAll() {
	int i;
	for (i = 0; i < menuCount; i++) {
		Menu_RunCloseScript(&Menus[i]);
		Menus[i].window.flags &= ~(WINDOW_HASFOCUS | WINDOW_VISIBLE | WINDOW_MOUSEOVER);
	}
}


void Script_Show(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name=NULL;
	if (String_Parse(args, &name)) {
		Menu_ShowItemByName(item->parent, name, qtrue);
	}
}

void Script_Hide(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name=NULL;
	if (String_Parse(args, &name)) {
		Menu_ShowItemByName(item->parent, name, qfalse);
	}
}

void Script_FadeIn(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name=NULL;
	if (String_Parse(args, &name)) {
		Menu_FadeItemByName(item->parent, name, qfalse);
	}
}

void Script_FadeOut(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name=NULL;
	if (String_Parse(args, &name)) {
		Menu_FadeItemByName(item->parent, name, qtrue);
	}
}

void Script_Open(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name=NULL;
	if (String_Parse(args, &name)) {
		Menus_OpenByName(name);
	}
}

void Menu_FadeMenuByName( const char *p, qboolean *bAbort, qboolean fadeOut ) {
	itemDef_t	*item;
	int		i;
	menuDef_t *menu = Menus_FindByName( p );

	if( menu ) {
		for( i = 0; i < menu->itemCount; i++ ) {
			item = menu->items[i];
			if( fadeOut ) {
				item->window.flags |= (WINDOW_FADINGOUT | WINDOW_VISIBLE);
				item->window.flags &= ~WINDOW_FADINGIN;
			} else {
				item->window.flags |= (WINDOW_VISIBLE | WINDOW_FADINGIN);
				item->window.flags &= ~WINDOW_FADINGOUT;
			}
		}
	}
 }

void Script_FadeInMenu(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name=NULL;
	if( String_Parse( args, &name ) ) {
		Menu_FadeMenuByName( name, bAbort, qfalse );
	}
}

void Script_FadeOutMenu(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name=NULL;
	if( String_Parse( args, &name ) ) {
		Menu_FadeMenuByName( name, bAbort, qtrue );
	}
}

// DHM - Nerve

void Script_ConditionalOpen(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *cvar=NULL;
	const char *name1=NULL;
	const char *name2=NULL;
	float		val;
	char		buff[1024];
	int			testtype; // 0: check val not 0
						  // 1: check cvar not empty

	if ( String_Parse(args, &cvar) && Int_Parse(args, &testtype) && String_Parse(args, &name1) && String_Parse(args, &name2) ) {

		switch( testtype ) {
		default:
		case 0:
			val = DC->getCVarValue( cvar );
			if ( val == 0.f ) {
				Menus_OpenByName(name2);
			} else {
				Menus_OpenByName(name1);
			}
			break;
		case 1:
			DC->getCVarString( cvar, buff, sizeof(buff) );
			if( !buff[0] ) {
				Menus_OpenByName(name2);
			} else {
				Menus_OpenByName(name1);
			}
			break;
		}
	}
}

void Script_ConditionalScript(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *cvar;
	const char *script1;
	const char *script2;
	const char *token;
	float		val;
	char		buff[1024];
	int			testtype; // 0: check val not 0
						  // 1: check cvar not empty
	int			testval;

	if ( String_Parse(args, &cvar) &&
		 Int_Parse(args, &testtype) &&
		 String_Parse(args, &token) && ( token && *token == '(' ) &&
		 String_Parse(args, &script1) &&
		 String_Parse(args, &token) && ( token && *token == ')' ) &&
		 String_Parse(args, &token) && ( token && *token == '(' ) &&
		 String_Parse(args, &script2) &&
		 String_Parse(args, &token) && ( token && *token == ')' ) ) {

		switch( testtype ) {
		default:
		case 0:
			val = DC->getCVarValue( cvar );
			if ( val == 0.f ) {
				Item_RunScript( item, bAbort, script2 );
			} else {
				Item_RunScript( item, bAbort, script1 );
			}
			break;
		case 1:
			DC->getCVarString( cvar, buff, sizeof(buff) );
			if( !buff[0] ) {
				Item_RunScript( item, bAbort, script2 );
			} else {
				Item_RunScript( item, bAbort, script1 );
			}
			break;
		case 3:
			if( Int_Parse( args, &testval ) ) {
				val = DC->getCVarValue( cvar );
				if ( val != testval ) {
					Item_RunScript( item, bAbort, script2 );
				} else {
					Item_RunScript( item, bAbort, script1 );
				}
			}
			break;
		case 2:
			// special tests
			if( !Q_stricmp( cvar, "UIProfileIsActiveProfile" ) ) {
				char ui_profileStr[256];
				char cl_profileStr[256];

				DC->getCVarString( "ui_profile", ui_profileStr, sizeof(ui_profileStr) );
				Q_CleanStr( ui_profileStr );
				Q_CleanDirName( ui_profileStr );

				DC->getCVarString( "cl_profile", cl_profileStr, sizeof(cl_profileStr) );

				if( !Q_stricmp( ui_profileStr, cl_profileStr ) ) {
					Item_RunScript( item, bAbort, script1 );
				} else {
					Item_RunScript( item, bAbort, script2 );
				}
			} else if( !Q_stricmp( cvar, "UIProfileValidName" ) ) {
				char ui_profileStr[256];
				char ui_profileCleanedStr[256];

				DC->getCVarString( "ui_profile", ui_profileStr, sizeof(ui_profileStr) );
				Q_strncpyz( ui_profileCleanedStr, ui_profileStr, sizeof(ui_profileCleanedStr) );
				Q_CleanStr( ui_profileCleanedStr );
				Q_CleanDirName( ui_profileCleanedStr );

				if( *ui_profileStr && *ui_profileCleanedStr ) {
					Item_RunScript( item, bAbort, script1 );
				} else {
					Item_RunScript( item, bAbort, script2 );
				}
				
			} else if( !Q_stricmp( cvar, "UIProfileAlreadyExists" ) ) {
				char ui_profileCleanedStr[256];
				qboolean alreadyExists = qfalse;
				fileHandle_t f;

				DC->getCVarString( "ui_profile", ui_profileCleanedStr, sizeof(ui_profileCleanedStr) );
				Q_CleanStr( ui_profileCleanedStr );
				Q_CleanDirName( ui_profileCleanedStr );

				if( trap_FS_FOpenFile( va( "profiles/%s/profile.dat", ui_profileCleanedStr ), &f, FS_READ ) >= 0 ) {
					alreadyExists = qtrue;
					trap_FS_FCloseFile( f );
				}

				if( alreadyExists ) {
					Item_RunScript( item, bAbort, script1 );
				} else {
					Item_RunScript( item, bAbort, script2 );
				}
			} else if( !Q_stricmp( cvar, "UIProfileAlreadyExists_Rename" ) ) {
				char ui_profileCleanedStr[256];
				qboolean alreadyExists = qfalse;
				fileHandle_t f;

				DC->getCVarString( "ui_profile_renameto", ui_profileCleanedStr, sizeof(ui_profileCleanedStr) );
				Q_CleanStr( ui_profileCleanedStr );
				Q_CleanDirName( ui_profileCleanedStr );

				if( trap_FS_FOpenFile( va( "profiles/%s/profile.dat", ui_profileCleanedStr ), &f, FS_READ ) >= 0 ) {
					alreadyExists = qtrue;
					trap_FS_FCloseFile( f );
				}

				if( alreadyExists ) {
					Item_RunScript( item, bAbort, script1 );
				} else {
					Item_RunScript( item, bAbort, script2 );
				}
			} else if( !Q_stricmp( cvar, "ReadyToCreateProfile" ) ) {
				char ui_profileStr[256], ui_profileCleanedStr[256];
				int ui_rate;
				qboolean alreadyExists = qfalse;
				fileHandle_t f;

				DC->getCVarString( "ui_profile", ui_profileStr, sizeof(ui_profileStr) );

				Q_strncpyz( ui_profileCleanedStr, ui_profileStr, sizeof(ui_profileCleanedStr) );
				Q_CleanStr( ui_profileCleanedStr );
				Q_CleanDirName( ui_profileCleanedStr );

				if( trap_FS_FOpenFile( va( "profiles/%s/profile.dat", ui_profileCleanedStr ), &f, FS_READ ) >= 0 ) {
					alreadyExists = qtrue;
					trap_FS_FCloseFile( f );
				}

				ui_rate = (int)DC->getCVarValue( "ui_rate" );
				
				if( !alreadyExists && *ui_profileStr && ui_rate > 0 ) {
					Item_RunScript( item, bAbort, script1 );
				} else {
					Item_RunScript( item, bAbort, script2 );
				}
			} else if( !Q_stricmp( cvar, "vidrestartIsRequired" ) ) {
				int ui_r_mode = DC->getCVarValue( "ui_r_mode" );
				int ui_r_colorbits = DC->getCVarValue( "ui_r_colorbits" );
				int ui_r_fullscreen = DC->getCVarValue( "ui_r_fullscreen" );
				int ui_r_texturebits = DC->getCVarValue( "ui_r_texturebits" );
				int ui_r_depthbits = DC->getCVarValue( "ui_r_depthbits" );
				int ui_r_ext_compressed_textures = DC->getCVarValue( "ui_r_ext_compressed_textures" );
				int ui_r_allowextensions = DC->getCVarValue( "ui_r_allowextensions" );
				int ui_s_khz = DC->getCVarValue( "ui_s_khz" );
				int ui_r_detailtextures = DC->getCVarValue( "ui_r_detailtextures" );
				int ui_r_subdivisions = DC->getCVarValue( "ui_r_subdivisions" );
				char ui_r_texturemode[MAX_CVAR_VALUE_STRING];

				#ifdef CGAMEDLL
                int r_mode = DC->getCVarValue( "r_mode" );
#else
                int r_mode = TCE_UI_GetVideoMode();
#endif
				int r_colorbits = DC->getCVarValue( "r_colorbits" );
				int r_fullscreen = DC->getCVarValue( "r_fullscreen" );
				int r_texturebits = DC->getCVarValue( "r_texturebits" );
				int r_depthbits = DC->getCVarValue( "r_depthbits" );
				int r_ext_compressed_textures = DC->getCVarValue( "r_ext_compressed_textures" );
				int r_allowextensions = DC->getCVarValue( "r_allowextensions" );
				int s_khz = DC->getCVarValue( "s_khz" );
				int r_detailtextures = DC->getCVarValue( "r_detailtextures" );
				int r_subdivisions = DC->getCVarValue( "r_subdivisions" );
				char r_texturemode[MAX_CVAR_VALUE_STRING];

				/* TC reads these staged audio values but they do not require vid_restart. */
				(void)ui_s_khz;
				(void)s_khz;

				trap_Cvar_VariableStringBuffer( "ui_r_texturemode", ui_r_texturemode, sizeof(ui_r_texturemode) );
				trap_Cvar_VariableStringBuffer( "r_texturemode", r_texturemode, sizeof(r_texturemode) );

				if( ui_r_subdivisions != r_subdivisions ||
					ui_r_mode != r_mode ||
					ui_r_colorbits != r_colorbits ||
					ui_r_fullscreen != r_fullscreen ||
					ui_r_texturebits != r_texturebits ||
					ui_r_depthbits != r_depthbits ||
					ui_r_ext_compressed_textures != r_ext_compressed_textures ||
					ui_r_allowextensions != r_allowextensions ||
					ui_r_detailtextures != r_detailtextures || 
					Q_stricmp( r_texturemode, ui_r_texturemode ) ) {
					Item_RunScript( item, bAbort, script1 );
				} else {
					Item_RunScript( item, bAbort, script2 );
				}
			/*} else if( !Q_stricmpn( cvar, "voteflags", 9 ) ) {
				char info[MAX_INFO_STRING];
				int voteflags = atoi(cvar + 9);

				trap_Cvar_VariableStringBuffer( "cg_ui_voteFlags", info, sizeof(info) );

				if( (atoi(info) & item->voteFlag) != item->voteFlag ) {
					Item_RunScript( item, bAbort, script1 );
				} else {
					Item_RunScript( item, bAbort, script2 );
				}*/
#ifndef CGAMEDLL
			} else if( !Q_stricmpn( cvar, "serversort_", 11 ) ) {
				int sorttype = atoi(cvar + 11);

				if ( sorttype != uiInfo.serverStatus.sortKey ) {
					Item_RunScript( item, bAbort, script2 );
				} else {
					Item_RunScript( item, bAbort, script1 );
				}
			} else if( !Q_stricmp( cvar, "ValidReplaySelected" ) ) {
				if( uiInfo.demoIndex >= 0 && uiInfo.demoIndex < uiInfo.demoCount ) {
					Item_RunScript( item, bAbort, script1 );
				} else {
					Item_RunScript( item, bAbort, script2 );
				}
#endif // !CGAMEDLL
			} else if( !Q_stricmp( cvar, "ROldModeCheck" ) ) {
				char r_oldModeStr[256];
				int r_oldMode;
				#ifdef CGAMEDLL
                int r_mode = DC->getCVarValue( "r_mode" );
#else
                int r_mode = TCE_UI_GetVideoMode();
#endif

				DC->getCVarString( "r_oldMode", r_oldModeStr, sizeof(r_oldModeStr) );
				r_oldMode = atoi(r_oldModeStr);

				if( *r_oldModeStr && r_oldMode != r_mode ) {
					Item_RunScript( item, bAbort, script1 );
				} else {
					if( r_oldMode == r_mode ) {
						trap_Cvar_Set( "r_oldMode", "" );	// clear it
					}
					Item_RunScript( item, bAbort, script2 );
				}
			}
			break;
		}
	}
}

// DHM - Nerve

void Script_Close(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name=NULL;
	if (String_Parse(args, &name)) {
		Menus_CloseByName(name);
	}
}

void Script_CloseAll( itemDef_t *item, qboolean *bAbort, char **args ) {
	Menus_CloseAll();
}

void Script_CloseAllOtherMenus( itemDef_t *item, qboolean *bAbort, char **args ) {
	int i;
	for (i = 0; i < menuCount; i++) {
		if( &Menus[i] == item->parent ) {
			continue;
		}
		Menu_RunCloseScript(&Menus[i]);
		Menus[i].window.flags &= ~(WINDOW_HASFOCUS | WINDOW_VISIBLE | WINDOW_MOUSEOVER);
	}
}

/*
==============
Script_Clipboard
==============
*/
void Script_Clipboard(itemDef_t *item, qboolean *bAbort, char **args) {
}

/*
==============
Script_NotebookShowpage
	hide all notebook pages and show just the active one

	inc == 0	- show current page
	inc == val	- turn inc pages in the notebook (negative numbers are backwards)
	inc == 999	- key number.  +999 is jump to last page, -999 is jump to cover page
==============
*/
void Script_NotebookShowpage(itemDef_t *item, qboolean *bAbort, char **args) {
}



#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
/* TC cgame movement script consumers: preserve x87 conversion and integer copies. */
enum {
 mv805_10 = offsetof(itemDef_t, window) + offsetof(windowDef_t, rectClient.x),
 mv805_14 = offsetof(itemDef_t, window) + offsetof(windowDef_t, rectClient.y),
 mv805_48 = offsetof(itemDef_t, window) + offsetof(windowDef_t, flags),
 mv805_50 = offsetof(itemDef_t, window) + offsetof(windowDef_t, rectEffects.y),
 mv805_60 = offsetof(itemDef_t, window) + offsetof(windowDef_t, rectEffects2.y),
 mv805_64 = offsetof(itemDef_t, window) + offsetof(windowDef_t, rectEffects2.w),
 mv805_68 = offsetof(itemDef_t, window) + offsetof(windowDef_t, rectEffects2.h),
 mv805_4c = offsetof(itemDef_t, window) + offsetof(windowDef_t, rectEffects.x),
 mv805_5c = offsetof(itemDef_t, window) + offsetof(windowDef_t, rectEffects2.x),
 mv805_6c = offsetof(itemDef_t, window) + offsetof(windowDef_t, offsetTime),
};
typedef char tceMovement805RectLayout[(sizeof(rectDef_t) == 16 && offsetof(rectDef_t, x) == 0 && offsetof(rectDef_t, y) == 4 && offsetof(rectDef_t, w) == 8 && offsetof(rectDef_t, h) == 12) ? 1 : -1];
__declspec(naked) void Menu_TransitionItemByName(menuDef_t *menu, const char *p, rectDef_t rectFrom, rectDef_t rectTo, int time, float amt) {
	__asm {
		SUB ESP,0x8
		MOV EAX,dword ptr [ESP + 0x10]
		MOV ECX,dword ptr [ESP + 0xc]
		PUSH EAX
		PUSH ECX
		CALL Menu_ItemsMatchingGroup
		ADD ESP,0x8
		MOV dword ptr [ESP + 0x4],EAX
		TEST EAX,EAX
		MOV dword ptr [ESP],0x0
		JLE mv805_30081a03
		PUSH EBX
		MOV EBX,dword ptr [ESP + 0x30]
		PUSH EBP
		MOV EBP,dword ptr [ESP + 0x30]
		PUSH ESI
		PUSH EDI
		MOV EDI,dword ptr [ESP + 0x44]
mv805_300818f9:
		MOV EDX,dword ptr [ESP + 0x20]
		MOV EAX,dword ptr [ESP + 0x10]
		MOV ECX,dword ptr [ESP + 0x1c]
		PUSH EDX
		PUSH EAX
		PUSH ECX
		CALL Menu_GetMatchingItemByNumber
		MOV ESI,EAX
		ADD ESP,0xc
		TEST ESI,ESI
		JZ mv805_300819ea
		MOV EDX,dword ptr [ESI + mv805_48]
		MOV EAX,dword ptr [ESP + 0x24]
		MOV ECX,dword ptr [ESP + 0x28]
		OR EDX,0x104
		MOV dword ptr [ESI + mv805_48],EDX
		LEA EDX,[ESI + mv805_10]
		FLD dword ptr [ESP + 0x34]
		MOV dword ptr [EDX],EAX
		MOV EAX,dword ptr [ESP + 0x2c]
		FSUB dword ptr [ESP + 0x24]
		MOV dword ptr [EDX + 0x4],ECX
		MOV ECX,dword ptr [ESP + 0x30]
		MOV dword ptr [ESI + mv805_6c],EDI
		MOV dword ptr [EDX + 0x8],EAX
		MOV EAX,dword ptr [ESP + 0x34]
		MOV dword ptr [EDX + 0xc],ECX
		LEA EDX,[ESI + mv805_4c]
		MOV ECX,dword ptr [ESP + 0x40]
		MOV dword ptr [EDX],EAX
		MOV dword ptr [EDX + 0x4],EBP
		MOV dword ptr [EDX + 0x8],EBX
		MOV dword ptr [EDX + 0xc],ECX
		CALL TCE_TextInteger788
		CDQ
		XOR EAX,EDX
		SUB EAX,EDX
		MOV dword ptr [ESP + 0x44],EAX
		FILD dword ptr [ESP + 0x44]
		FDIV dword ptr [ESP + 0x48]
		FSTP dword ptr [ESI + mv805_5c]
		FLD dword ptr [ESP + 0x38]
		FSUB dword ptr [ESP + 0x28]
		CALL TCE_TextInteger788
		CDQ
		XOR EAX,EDX
		SUB EAX,EDX
		MOV dword ptr [ESP + 0x44],EAX
		FILD dword ptr [ESP + 0x44]
		FDIV dword ptr [ESP + 0x48]
		FSTP dword ptr [ESI + mv805_60]
		FLD dword ptr [ESP + 0x3c]
		FSUB dword ptr [ESP + 0x2c]
		CALL TCE_TextInteger788
		CDQ
		XOR EAX,EDX
		SUB EAX,EDX
		MOV dword ptr [ESP + 0x44],EAX
		FILD dword ptr [ESP + 0x44]
		FDIV dword ptr [ESP + 0x48]
		FSTP dword ptr [ESI + mv805_64]
		FLD dword ptr [ESP + 0x40]
		FSUB dword ptr [ESP + 0x30]
		CALL TCE_TextInteger788
		CDQ
		XOR EAX,EDX
		PUSH ESI
		SUB EAX,EDX
		MOV dword ptr [ESP + 0x48],EAX
		FILD dword ptr [ESP + 0x48]
		FDIV dword ptr [ESP + 0x4c]
		FSTP dword ptr [ESI + mv805_68]
		CALL Item_UpdatePosition
		ADD ESP,0x4
mv805_300819ea:
		MOV EAX,dword ptr [ESP + 0x10]
		MOV ECX,dword ptr [ESP + 0x14]
		INC EAX
		CMP EAX,ECX
		MOV dword ptr [ESP + 0x10],EAX
		JL mv805_300818f9
		POP EDI
		POP ESI
		POP EBP
		POP EBX
mv805_30081a03:
		ADD ESP,0x8
		RET
	}
}
__declspec(naked) void Menu_OrbitItemByName(menuDef_t *menu, const char *p, float x, float y, float cx, float cy, int time) {
	__asm {
		PUSH EBX
		MOV EBX,dword ptr [ESP + 0x8]
		PUSH EBP
		PUSH ESI
		PUSH EDI
		MOV EDI,dword ptr [ESP + 0x18]
		PUSH EDI
		PUSH EBX
		CALL Menu_ItemsMatchingGroup
		MOV EBP,EAX
		ADD ESP,0x8
		XOR ESI,ESI
		TEST EBP,EBP
		JLE mv805_30081b6a
mv805_30081b1e:
		PUSH EDI
		PUSH ESI
		PUSH EBX
		CALL Menu_GetMatchingItemByNumber
		ADD ESP,0xc
		TEST EAX,EAX
		JZ mv805_30081b65
		MOV EDX,dword ptr [EAX + mv805_48]
		MOV ECX,dword ptr [ESP + 0x2c]
		OR EDX,0x10004
		MOV dword ptr [EAX + mv805_6c],ECX
		MOV ECX,dword ptr [ESP + 0x28]
		MOV dword ptr [EAX + mv805_48],EDX
		MOV EDX,dword ptr [ESP + 0x24]
		MOV dword ptr [EAX + mv805_50],ECX
		MOV ECX,dword ptr [ESP + 0x20]
		MOV dword ptr [EAX + mv805_4c],EDX
		MOV EDX,dword ptr [ESP + 0x1c]
		PUSH EAX
		MOV dword ptr [EAX + mv805_10],EDX
		MOV dword ptr [EAX + mv805_14],ECX
		CALL Item_UpdatePosition
		ADD ESP,0x4
mv805_30081b65:
		INC ESI
		CMP ESI,EBP
		JL mv805_30081b1e
mv805_30081b6a:
		POP EDI
		POP ESI
		POP EBP
		POP EBX
		RET
	}
}
#else
void Menu_TransitionItemByName(menuDef_t *menu, const char *p, rectDef_t rectFrom, rectDef_t rectTo, int time, float amt) {
	itemDef_t *item;
	int i;
	int count = Menu_ItemsMatchingGroup(menu, p);
	for (i = 0; i < count; i++) {
		item = Menu_GetMatchingItemByNumber(menu, i, p);
		if (item != NULL) {
			item->window.flags |= (WINDOW_INTRANSITION | WINDOW_VISIBLE);
			item->window.offsetTime = time;
			memcpy(&item->window.rectClient, &rectFrom, sizeof(rectDef_t));
			memcpy(&item->window.rectEffects, &rectTo, sizeof(rectDef_t));
			item->window.rectEffects2.x = fabs(rectTo.x - rectFrom.x) / amt;
			item->window.rectEffects2.y = fabs(rectTo.y - rectFrom.y) / amt;
			item->window.rectEffects2.w = fabs(rectTo.w - rectFrom.w) / amt;
			item->window.rectEffects2.h = fabs(rectTo.h - rectFrom.h) / amt;
			Item_UpdatePosition(item);
		}
	}
}

#endif


void Script_Transition(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name=NULL;
	rectDef_t rectFrom, rectTo;
	int time=0;
	float amt=0.0f;
	
	if (String_Parse(args, &name)) {
		if ( Rect_Parse(args, &rectFrom) && Rect_Parse(args, &rectTo) && Int_Parse(args, &time) && Float_Parse(args, &amt)) {
			Menu_TransitionItemByName(item->parent, name, rectFrom, rectTo, time, amt);
		}
	}
}


#if !defined(_MSC_VER) || !defined(_M_IX86) || defined(UIDLL)
void Menu_OrbitItemByName(menuDef_t *menu, const char *p, float x, float y, float cx, float cy, int time) {
  itemDef_t *item;
  int i;
  int count = Menu_ItemsMatchingGroup(menu, p);
  for (i = 0; i < count; i++) {
    item = Menu_GetMatchingItemByNumber(menu, i, p);
    if (item != NULL) {
      item->window.flags |= (WINDOW_ORBITING | WINDOW_VISIBLE);
      item->window.offsetTime = time;
      item->window.rectEffects.x = cx;
      item->window.rectEffects.y = cy;
      item->window.rectClient.x = x;
      item->window.rectClient.y = y;
      Item_UpdatePosition(item);
    }
  }
}

#endif


void Script_Orbit(itemDef_t *item, qboolean *bAbort, char **args) {
  const char *name=NULL;
  float cx=0.0f, cy=0.0f, x=0.0f, y=0.0f;
  int time=0;

  if (String_Parse(args, &name)) {
    if ( Float_Parse(args, &x) && Float_Parse(args, &y) && Float_Parse(args, &cx) && Float_Parse(args, &cy) && Int_Parse(args, &time) ) {
      Menu_OrbitItemByName(item->parent, name, x, y, cx, cy, time);
    }
  }
}



void Script_SetFocus(itemDef_t *item, qboolean *bAbort, char **args) {
  /* TC 40016fc0: decoration/already-focused items do not run callbacks. */
  const char *name=NULL;
  itemDef_t *focusItem;

  if (String_Parse(args, &name)) {
    focusItem = Menu_FindItemByName(item->parent, name);
    if (focusItem && !(focusItem->window.flags & WINDOW_DECORATION) && !(focusItem->window.flags & WINDOW_HASFOCUS)) {
      Menu_ClearFocus(item->parent);
      focusItem->window.flags |= WINDOW_HASFOCUS;
      if (focusItem->onFocus) {
		  Item_RunScript(focusItem, NULL, focusItem->onFocus);
      }
      if (DC->Assets.itemFocusSound) {
        DC->startLocalSound( DC->Assets.itemFocusSound, CHAN_LOCAL_SOUND );
      }
    }
  }
}

void Script_ClearFocus( itemDef_t *item, qboolean *bAbort, char **args )
{
	/* TC 40017060, recovered callback from the UI script dispatch table. */
	Menu_ClearFocus( item->parent );
}

/* TC 40017080/400170c0: selection scripts store the parsed asset name. */
void Script_SetPlayerModel(itemDef_t *item, qboolean *bAbort, char **args) {
  const char *name=NULL;
  if (String_Parse(args, &name)) {
    DC->setCVar("team_model", name);
  }
}

void Script_SetPlayerHead(itemDef_t *item, qboolean *bAbort, char **args) {
  const char *name=NULL;
  if (String_Parse(args, &name)) {
    DC->setCVar("team_headmodel", name);
  }
}

// ATVI Wolfenstein Misc #304
// the parser misreads setCvar "bleh" ""
// you have to use clearCvar "bleh"
void Script_ClearCvar(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *cvar;
	if (String_Parse(args, &cvar)) {
		DC->setCVar(cvar, "");
	}	
}
 
void Script_SetCvar(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *cvar=NULL, *val=NULL;
	if (String_Parse(args, &cvar) && String_Parse(args, &val)) {
		DC->setCVar(cvar, val);
	}
}

void Script_CopyCvar(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *cvar_src=NULL, *cvar_dst=NULL;
	if( String_Parse( args, &cvar_src ) && String_Parse( args, &cvar_dst ) ) {
		char buff[256];

		DC->getCVarString( cvar_src, buff, 256 );
		DC->setCVar( cvar_dst, buff );
	}
}

void Script_Exec(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *val=NULL;
	if (String_Parse(args, &val)) {
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
		/* TC812: retain the DC slot owner across va, then load its callback. */
		displayContextDef_t *actionContext812 = DC;
		const char *commandText812 = va("%s ; ", val);
		actionContext812->executeText(EXEC_APPEND, commandText812);
#else
		DC->executeText(EXEC_APPEND, va("%s ; ", val));
#endif
	}
}

void Script_ExecNOW(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *val=NULL;
	if (String_Parse(args, &val)) {
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
		/* TC812: retain the DC slot owner across va, then load its callback. */
		displayContextDef_t *actionContext812 = DC;
		const char *commandText812 = va("%s ; ", val);
		actionContext812->executeText(EXEC_NOW, commandText812);
#else
		DC->executeText(EXEC_NOW, va("%s ; ", val));
#endif
	}
}

void Script_Play(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *val=NULL;
	if (String_Parse(args, &val)) {
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
		/* TC812: registration may change DC; dispatch through the saved owner. */
		displayContextDef_t *soundContext812 = DC;
		sfxHandle_t soundHandle812 = soundContext812->registerSound(val, qfalse);
		soundContext812->startLocalSound(soundHandle812, CHAN_LOCAL_SOUND);
#else
		DC->startLocalSound(DC->registerSound(val, qfalse), CHAN_LOCAL_SOUND);		// all sounds are not 3d
#endif
	}
}

void Script_playLooped(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *val=NULL;
	if (String_Parse(args, &val)) {
		DC->stopBackgroundTrack();
		DC->startBackgroundTrack(val, val, 0);
	}
}

// NERVE - SMF
void Script_AddListItem(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *itemname=NULL, *val=NULL, *name=NULL;
	itemDef_t *t;

	if ( String_Parse( args, &itemname ) && String_Parse( args, &val ) && String_Parse( args, &name ) ) {
		t = Menu_FindItemByName( item->parent, itemname );
		if ( t && t->special )
			DC->feederAddItem( t->special, name, atoi( val ) );
	}
}
// -NERVE - SMF
// DHM - Nerve
void Script_CheckAutoUpdate(itemDef_t *item, qboolean *bAbort, char **args) {
	DC->checkAutoUpdate();
}

void Script_GetAutoUpdate(itemDef_t *item, qboolean *bAbort, char **args) {
	DC->getAutoUpdate();
}
// DHM - Nerve

void Script_SetMenuFocus(itemDef_t *item, qboolean *bAbort, char **args) {
	const char *name;

	if (String_Parse(args, &name)) {
		menuDef_t *focusMenu = Menus_FindByName( name );
		
		if (focusMenu && !(focusMenu->window.flags & WINDOW_HASFOCUS)) {
			Menu_ClearFocus(item->parent);
			focusMenu->window.flags |= WINDOW_HASFOCUS;
		}
	}
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
/* TC cgame 300820f0: the filename argument slot is also the handle output.
 * Preserve its pointer bits before the provider writes it, and close on both
 * outcomes. This is an ABI port, not a claim of improved profile handling. */
__declspec(naked) qboolean FileExists( char *filename ) {
	__asm {
		mov ecx, dword ptr [esp + 4]
		lea eax, [esp + 4]
		push 0
		push eax
		push ecx
		call trap_FS_FOpenFile
		add esp, 12
		test eax, eax
		jge tceFileExists813Success
		mov edx, dword ptr [esp + 4]
		push edx
		call trap_FS_FCloseFile
		add esp, 4
		xor eax, eax
		ret
	tceFileExists813Success:
		mov eax, dword ptr [esp + 4]
		push eax
		call trap_FS_FCloseFile
		add esp, 4
		mov eax, 1
		ret
	}
}
#else
qboolean FileExists( char *filename ) {
	fileHandle_t    f;

	if ( trap_FS_FOpenFile( filename, &f, FS_READ ) < 0 ) {
		trap_FS_FCloseFile( f );
		return qfalse;
	} else {
		trap_FS_FCloseFile( f );
		return qtrue;
	}
}

#endif

qboolean Script_CheckProfile( char *profile_path ) {
	fileHandle_t    f;
	char f_data[32];
	int f_pid;
	char com_pid[256];
	int pid;

	if( trap_FS_FOpenFile( profile_path, &f, FS_READ ) < 0 ) {
		//no profile found, we're ok
		return qtrue;
	}

	trap_FS_Read( &f_data, sizeof(f_data)-1, f );

	DC->getCVarString( "com_pid", com_pid, sizeof(com_pid) );
	pid = atoi( com_pid );

	f_pid = atoi( f_data );
	if( f_pid != pid ) {
		//pid doesn't match
		trap_FS_FCloseFile( f );
		return qfalse;
	}

	//we're all ok
	trap_FS_FCloseFile( f );
	return qtrue;
}

qboolean Script_WriteProfile( char *profile_path ) {
	fileHandle_t    f;
	char com_pid[256];

	if( FileExists( profile_path ) ) {
		trap_FS_Delete( profile_path );
	}

	if( trap_FS_FOpenFile( profile_path, &f, FS_WRITE ) < 0 ) {
		Com_Printf( "Script_WriteProfile: Can't write %s.\n", profile_path );
		return qfalse;
	}
	if ( f < 0 ) {
		Com_Printf( "Script_WriteProfile: Can't write %s.\n", profile_path );
		return qfalse;
	}

	DC->getCVarString( "com_pid", com_pid, sizeof(com_pid) );

	trap_FS_Write( com_pid, Q_strlenInt( com_pid ), f );

	trap_FS_FCloseFile( f );  

	return qtrue;
}

void Script_ExecWolfConfig(itemDef_t *item, qboolean *bAbort, char **args) {
	char cl_profileStr[256];
	int useprofile = 1;

	if( Int_Parse(args, &useprofile) ) {

		DC->getCVarString( "cl_profile", cl_profileStr, sizeof(cl_profileStr) );

		if( useprofile && cl_profileStr[0] ) {
			if( !Script_CheckProfile( va( "profiles/%s/profile.pid", cl_profileStr ) ) ) {
				Com_Printf( "^3WARNING: profile.pid found for profile '%s' - not executing %s\n", cl_profileStr, CONFIG_NAME);
			} else {
				DC->executeText(EXEC_NOW, va( "exec profiles/%s/%s\n", cl_profileStr, CONFIG_NAME ) );

				if( !Script_WriteProfile( va( "profiles/%s/profile.pid", cl_profileStr ) ) ) {
					Com_Printf( "^3WARNING: couldn't write profiles/%s/profile.pid\n", cl_profileStr );
				}
			}
		} else {
			DC->executeText( EXEC_NOW, va( "exec %s\n", CONFIG_NAME ) );
		}
	}
}

void Script_SetEditFocus( itemDef_t *item, qboolean *bAbort, char **args ) {
	const char *name=NULL;
	itemDef_t *editItem;

	if (String_Parse(args, &name)) {
		editItem = Menu_FindItemByName(item->parent, name);
		if( editItem && (editItem->type == ITEM_TYPE_EDITFIELD || editItem->type == ITEM_TYPE_NUMERICFIELD) ) {
			editFieldDef_t *editPtr = (editFieldDef_t*)editItem->typeData;

			Menu_ClearFocus( item->parent );
			editItem->window.flags |= WINDOW_HASFOCUS;
			if( editItem->onFocus ) {
				Item_RunScript( editItem, NULL, editItem->onFocus );
			}
			if( DC->Assets.itemFocusSound ) {
				DC->startLocalSound( DC->Assets.itemFocusSound, CHAN_LOCAL_SOUND );
			}

			// NERVE - SMF - reset scroll offset so we can see what we're editing
			if ( editPtr )
				editPtr->paintOffset = 0;

			editItem->cursorPos = 0;
			g_editingField = qtrue;
			g_editItem = editItem;

			// the stupidest idea ever, let's just override the console, every ui element, user choice, etc
			// nuking this
			//%	DC->setOverstrikeMode(qtrue);
		}
	}
}

void Script_Abort( itemDef_t *item, qboolean *bAbort, char **args ) {
	*bAbort = qtrue;
}


commandDef_t commandList[] =
{
	{"fadein", &Script_FadeIn},					// group/name
	{"fadeout", &Script_FadeOut},				// group/name
	{"show", &Script_Show},						// group/name
	{"hide", &Script_Hide},						// group/name
	{"setcolor", &Script_SetColor},				// works on this
	{"open", &Script_Open},						// menu
	{"fadeinmenu", &Script_FadeInMenu},			// menu
	{"fadeoutmenu", &Script_FadeOutMenu},		// menu

	{"conditionalopen", &Script_ConditionalOpen},	// DHM - Nerve:: cvar menu menu 
													// opens first menu if cvar is true[non-zero], second if false
	{"conditionalscript", &Script_ConditionalScript},	// as conditonalopen, but then executes scripts

	{"close", &Script_Close},					// menu
	{"closeall", &Script_CloseAll},
	{"closeallothermenus", &Script_CloseAllOtherMenus},

	{"clipboard", &Script_Clipboard},			// show the current clipboard group by name
	{"showpage", &Script_NotebookShowpage},			// 
	{"setasset", &Script_SetAsset},				// works on this
	{"setbackground", &Script_SetBackground},	// works on this
	{"setitemcolor", &Script_SetItemColor},		// group/name
	{"setmenuitemcolor", &Script_SetMenuItemColor},		// group/name
	{"setteamcolor", &Script_SetTeamColor},		// sets this background color to team color
	{"setfocus", &Script_SetFocus},				// sets this background color to team color
	{"clearfocus", &Script_ClearFocus},
	{"setplayermodel", &Script_SetPlayerModel},	// sets this background color to team color
	{"setplayerhead", &Script_SetPlayerHead},	// sets this background color to team color
	{"transition", &Script_Transition},			// group/name
	{"setcvar", &Script_SetCvar},				// group/name
	{"clearcvar", &Script_ClearCvar},
	{"copycvar", &Script_CopyCvar},
	{"exec", &Script_Exec},						// group/name
	{"execnow", &Script_ExecNOW},				// group/name
	{"play", &Script_Play},						// group/name
	{"playlooped", &Script_playLooped},			// group/name
	{"orbit", &Script_Orbit},					// group/name
	{"addlistitem", &Script_AddListItem},		// NERVE - SMF - special command to add text items to list box
	{"checkautoupdate", &Script_CheckAutoUpdate},	// DHM - Nerve
	{"getautoupdate", &Script_GetAutoUpdate},	// DHM - Nerve
	{"setmenufocus", &Script_SetMenuFocus},			// focus menu
	{"execwolfconfig", &Script_ExecWolfConfig},	// executes etconfig.cfg
	{"setEditFocus", &Script_SetEditFocus },
	{"abort", &Script_Abort },
};

int scriptCommandCount = sizeof(commandList) / sizeof(commandDef_t);


void Item_RunScript(itemDef_t *item, qboolean *bAbort, const char *s) {
  char script[4096], *p;
  int i;
  qboolean bRan;
  qboolean b_localAbort = qfalse;
  memset(script, 0, sizeof(script));
  if (item && s && s[0]) {
    Q_strcat(script, 4096, s);
    p = script;
    while (1) {
      const char *command=NULL;
      // expect command then arguments, ; ends command, NULL ends script
      if (!String_Parse(&p, &command)) {
        return;
      }

      if (command[0] == ';' && command[1] == '\0') {
        continue;
      }

      bRan = qfalse;
      for (i = 0; i < scriptCommandCount; i++) {
        if (Q_stricmp(command, commandList[i].name) == 0) {
          (commandList[i].handler(item, &b_localAbort, &p));
          bRan = qtrue;

		  if( b_localAbort ) {
			  if( bAbort ) {
				  *bAbort = b_localAbort;
			  }
			  return;
		  }
          break;
        }
      }
      // not in our auto list, pass to handler
      if (!bRan) {
        DC->runScript(&p);
      }
    }
  }
}


qboolean Item_EnableShowViaCvar(itemDef_t *item, int flag) {
	char script[1024], *p;
	memset(script, 0, sizeof(script));
	if (item && item->enableCvar && *item->enableCvar && item->cvarTest && *item->cvarTest) {
		char buff[1024];
		DC->getCVarString(item->cvarTest, buff, sizeof(buff));
		
		Q_strcat(script, 1024, item->enableCvar);
		p = script;
		while (1) {
			const char *val=NULL;
			// expect value then ; or NULL, NULL ends list
			if (!String_Parse(&p, &val)) {
				return (item->cvarFlags & flag) ? qfalse : qtrue;
			}
			
			if (val[0] == ';' && val[1] == '\0') {
				continue;
			}
			
			// enable it if any of the values are true
			if (item->cvarFlags & flag) {
				if (Q_stricmp(buff, val) == 0) {
					return qtrue;
				}
			} else {
				// disable it if any of the values are true
				if (Q_stricmp(buff, val) == 0) {
					return qfalse;
				}
			}
			
		}
		return (item->cvarFlags & flag) ? qfalse : qtrue;
	}
	return qtrue;
}


// OSP - display if we poll on a server toggle setting
// We want *current* settings, so this is a bit of a perf hit,
// but this is only during UI display

qboolean Item_SettingShow(itemDef_t *item, qboolean fVoteTest)
{
	char info[MAX_INFO_STRING];

	if(fVoteTest) {
		trap_Cvar_VariableStringBuffer("cg_ui_voteFlags", info, sizeof(info));
		return((atoi(info) & item->voteFlag) != item->voteFlag);
	}

	DC->getConfigString( CS_SERVERTOGGLES, info, sizeof( info ) );

	if(item->settingFlags & SVS_ENABLED_SHOW) return(atoi(info) & item->settingTest);
	if(item->settingFlags & SVS_DISABLED_SHOW) return(!(atoi(info) & item->settingTest));

	return(qtrue);
}


// will optionaly set focus to this item 
qboolean Item_SetFocus(itemDef_t *item, float x, float y) {
	int i;
	itemDef_t *oldFocus;
	sfxHandle_t *sfx = &DC->Assets.itemFocusSound;
	qboolean playSound = qfalse;
	menuDef_t *parent; // bk001206: = (menuDef_t*)item->parent;
	// sanity check, non-null, not a decoration and does not already have the focus
	if (item == NULL || item->window.flags & WINDOW_DECORATION || item->window.flags & WINDOW_HASFOCUS || !(item->window.flags & WINDOW_VISIBLE)) {
		return qfalse;
	}

	// bk001206 - this can be NULL.
	parent = (menuDef_t*)item->parent; 
      
	// items can be enabled and disabled based on cvars
	if (item->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(item, CVAR_ENABLE)) {
		return qfalse;
	}

	if (item->cvarFlags & (CVAR_SHOW | CVAR_HIDE) && !Item_EnableShowViaCvar(item, CVAR_SHOW)) {
		return qfalse;
	}

	// OSP
	if((item->settingFlags & (SVS_ENABLED_SHOW | SVS_DISABLED_SHOW)) && !Item_SettingShow(item, qfalse)) {
		return(qfalse);
	}
	if(item->voteFlag != 0 && !Item_SettingShow(item, qtrue)) {
		return(qfalse);
	}

	oldFocus = Menu_ClearFocus(item->parent);

	if (item->type == ITEM_TYPE_TEXT) {
		rectDef_t r;
		r = item->textRect;
		r.y -= r.h;
		if (Rect_ContainsPoint(&r, x, y)) {
			item->window.flags |= WINDOW_HASFOCUS;
			if (item->focusSound) {
				sfx = &item->focusSound;
			}
			playSound = qtrue;
		} else {
			if (oldFocus) {
				oldFocus->window.flags |= WINDOW_HASFOCUS;
				if (oldFocus->onFocus) {
					Item_RunScript(oldFocus, NULL, oldFocus->onFocus);
				}
			}
		}
	} else {
	    item->window.flags |= WINDOW_HASFOCUS;
		if (item->onFocus) {
			Item_RunScript(item, NULL, item->onFocus);
		}
		if (item->focusSound) {
			sfx = &item->focusSound;
		}
		playSound = qtrue;
	}

	if (playSound && sfx) {
		DC->startLocalSound( *sfx, CHAN_LOCAL_SOUND );
	}

	for (i = 0; i < parent->itemCount; i++) {
		if (parent->items[i] == item) {
			parent->cursorItem = i;
			break;
		}
	}

	return qtrue;
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const float lb799Zero = 0.0f, lb799OneFloat = 1.0f;
static const double lb799ThirtyTwo = 32.0, lb799Two = 2.0, lb799Sixteen = 16.0, lb799Seventeen = 17.0, lb799Eight = 8.0, lb799One = 1.0;
enum {
 lb799I0 = offsetof(itemDef_t, window.rect.x),
 lb799I4 = offsetof(itemDef_t, window.rect.y),
 lb799I8 = offsetof(itemDef_t, window.rect.w),
 lb799I48 = offsetof(itemDef_t, window.flags),
 lb799I248 = offsetof(itemDef_t, special),
 lb799I250 = offsetof(itemDef_t, typeData),
 lb799Ic = offsetof(itemDef_t, window.rect.h),
 lb799L0 = offsetof(listBoxDef_t, startPos),
 lb799L10 = offsetof(listBoxDef_t, elementWidth),
 lb799L14 = offsetof(listBoxDef_t, elementHeight),
 lb799D90 = offsetof(displayContextDef_t, feederCount),
 lb799D128 = offsetof(displayContextDef_t, cursorx),
 lb799D12c = offsetof(displayContextDef_t, cursory),
};
__declspec(naked) int Item_ListBox_MaxScroll(itemDef_t *item) {
 __asm {
 MOV ECX,dword ptr [DC]
 PUSH EBX
 PUSH ESI
 PUSH EDI
 MOV EDI,dword ptr [ESP + 0x10]
 MOV EAX,dword ptr [EDI + lb799I248]
 MOV EBX,dword ptr [EDI + lb799I250]
 PUSH EAX
 CALL dword ptr [ECX + lb799D90]
 MOV ESI,EAX
 MOV EAX,dword ptr [EDI + lb799I48]
 ADD ESP,0x4
 TEST AH,0x4
 JZ lb799_300827e5
 FLD dword ptr [EDI + lb799I8]
 FDIV dword ptr [EBX + lb799L10]
 JMP lb799_300827eb
lb799_300827e5:
 FLD dword ptr [EDI + lb799Ic]
 FDIV dword ptr [EBX + lb799L14]
lb799_300827eb:
 CALL TCE_TextInteger788
 SUB ESI,EAX
 XOR EAX,EAX
 TEST ESI,ESI
 SETL AL
 DEC EAX
 POP EDI
 AND EAX,ESI
 POP ESI
 POP EBX
 RET
 }
}

__declspec(naked) int Item_ListBox_ThumbPosition(itemDef_t *item) {
 __asm {
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x8]
 PUSH EDI
 PUSH ESI
 MOV EDI,dword ptr [ESI + lb799I250]
 CALL Item_ListBox_MaxScroll
 MOV dword ptr [ESP + 0x10],EAX
 MOV EAX,dword ptr [ESI + lb799I48]
 FILD dword ptr [ESP + 0x10]
 ADD ESP,0x4
 TEST AH,0x4
 FSTP dword ptr [ESP + 0xc]
 FLD dword ptr [ESP + 0xc]
 FCOMP dword ptr [lb799Zero]
 FNSTSW AX
 JZ lb799_30082869
 TEST AH,0x41
 JNZ lb799_3008285b
 FLD dword ptr [ESI + lb799I8]
 FSUB qword ptr [lb799ThirtyTwo]
 FSUB qword ptr [lb799Two]
 FSUB qword ptr [lb799Sixteen]
 FDIV dword ptr [ESP + 0xc]
 FILD dword ptr [EDI + lb799L0]
 FMUL ST(0),ST(1)
 FLD dword ptr [ESI + lb799I0]
 JMP lb799_30082896
lb799_3008285b:
 FLD dword ptr [lb799Zero]
 FILD dword ptr [EDI + lb799L0]
 FMUL ST(0),ST(1)
 FLD dword ptr [ESI + lb799I0]
 JMP lb799_30082896
lb799_30082869:
 TEST AH,0x41
 JNZ lb799_30082889
 FLD dword ptr [ESI + lb799Ic]
 FSUB qword ptr [lb799ThirtyTwo]
 FSUB qword ptr [lb799Two]
 FSUB qword ptr [lb799Sixteen]
 FDIV dword ptr [ESP + 0xc]
 JMP lb799_3008288f
lb799_30082889:
 FLD dword ptr [lb799Zero]
lb799_3008288f:
 FILD dword ptr [EDI + lb799L0]
 FMUL ST(0),ST(1)
 FLD dword ptr [ESI + lb799I4]
lb799_30082896:
 FADD dword ptr [lb799OneFloat]
 FADDP ST(1),ST(0)
 FADD qword ptr [lb799Sixteen]
 CALL TCE_TextInteger788
 POP EDI
 POP ESI
 FSTP ST(0)
 RET
 }
}

__declspec(naked) int Item_ListBox_ThumbDrawPosition(itemDef_t *item) {
 __asm {
 MOV EAX,[itemCapture]
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x8]
 CMP EAX,ESI
 JNZ lb799_300829ad
 MOV EAX,dword ptr [ESI + lb799I48]
 TEST AH,0x4
 JZ lb799_3008293b
 MOV EAX,[DC]
 FILD dword ptr [EAX + lb799D128]
 FLD dword ptr [ESI + lb799I0]
 FADD qword ptr [lb799Seventeen]
 CALL TCE_TextInteger788
 MOV dword ptr [ESP + 0x8],EAX
 FILD dword ptr [ESP + 0x8]
 FADD qword ptr [lb799Eight]
 FLD ST(1)
 FCOMPP
 FNSTSW AX
 TEST AH,0x1
 JNZ lb799_300829ab
 FLD dword ptr [ESI + lb799I8]
 FADD dword ptr [ESI + lb799I0]
 FSUB qword ptr [lb799ThirtyTwo]
 FSUB qword ptr [lb799One]
 CALL TCE_TextInteger788
 MOV dword ptr [ESP + 0x8],EAX
 FILD dword ptr [ESP + 0x8]
 FADD qword ptr [lb799Eight]
 FLD ST(1)
 FCOMPP
 FNSTSW AX
 TEST AH,0x41
 JZ lb799_300829ab
 FSUB qword ptr [lb799Eight]
 CALL TCE_TextInteger788
 POP ESI
 RET
lb799_3008293b:
 MOV ECX,dword ptr [DC]
 FILD dword ptr [ECX + lb799D12c]
 FLD dword ptr [ESI + lb799I4]
 FADD qword ptr [lb799Seventeen]
 CALL TCE_TextInteger788
 MOV dword ptr [ESP + 0x8],EAX
 FILD dword ptr [ESP + 0x8]
 FADD qword ptr [lb799Eight]
 FLD ST(1)
 FCOMPP
 FNSTSW AX
 TEST AH,0x1
 JNZ lb799_300829ab
 FLD dword ptr [ESI + lb799Ic]
 FADD dword ptr [ESI + lb799I4]
 FSUB qword ptr [lb799ThirtyTwo]
 FSUB qword ptr [lb799One]
 CALL TCE_TextInteger788
 MOV dword ptr [ESP + 0x8],EAX
 FILD dword ptr [ESP + 0x8]
 FADD qword ptr [lb799Eight]
 FLD ST(1)
 FCOMPP
 FNSTSW AX
 TEST AH,0x41
 JZ lb799_300829ab
 FSUB qword ptr [lb799Eight]
 CALL TCE_TextInteger788
 POP ESI
 RET
lb799_300829ab:
 FSTP ST(0)
lb799_300829ad:
 PUSH ESI
 CALL Item_ListBox_ThumbPosition
 ADD ESP,0x4
 POP ESI
 RET
 }
}
#else
int Item_ListBox_MaxScroll(itemDef_t *item) {
	listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;
	int count = DC->feederCount(item->special);
	int max;

	if (item->window.flags & WINDOW_HORIZONTAL) {
		max = count - (int)(item->window.rect.w / listPtr->elementWidth);
	}
	else {
		max = count - (int)(item->window.rect.h / listPtr->elementHeight);
	}
	if (max < 0) {
		return 0;
	}
	return max;
}

int Item_ListBox_ThumbPosition(itemDef_t *item) {
	float max, pos, size;
	listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;

	max = Item_ListBox_MaxScroll(item);
	if (item->window.flags & WINDOW_HORIZONTAL) {
		size = item->window.rect.w - (SCROLLBAR_SIZE * 2) - 2;
		if (max > 0) {
			pos = (size-SCROLLBAR_SIZE) / (float) max;
		} else {
			pos = 0;
		}
		pos *= listPtr->startPos;
		return item->window.rect.x + 1 + SCROLLBAR_SIZE + pos;
	}
	else {
		size = item->window.rect.h - (SCROLLBAR_SIZE * 2) - 2;
		if (max > 0) {
			pos = (size-SCROLLBAR_SIZE) / (float) max;
		} else {
			pos = 0;
		}
		pos *= listPtr->startPos;

		return item->window.rect.y + 1 + SCROLLBAR_SIZE + pos;
	}
}

int Item_ListBox_ThumbDrawPosition(itemDef_t *item) {
	int min, max;

	if (itemCapture == item) {
		if (item->window.flags & WINDOW_HORIZONTAL) {
			min = item->window.rect.x + SCROLLBAR_SIZE + 1;
			max = item->window.rect.x + item->window.rect.w - 2*SCROLLBAR_SIZE - 1;
			if (DC->cursorx >= min + SCROLLBAR_SIZE/2 && DC->cursorx <= max + SCROLLBAR_SIZE/2) {
				return DC->cursorx - SCROLLBAR_SIZE/2;
			}
			else {
				return Item_ListBox_ThumbPosition(item);
			}
		}
		else {
			min = item->window.rect.y + SCROLLBAR_SIZE + 1;
			max = item->window.rect.y + item->window.rect.h - 2*SCROLLBAR_SIZE - 1;
			if (DC->cursory >= min + SCROLLBAR_SIZE/2 && DC->cursory <= max + SCROLLBAR_SIZE/2) {
				return DC->cursory - SCROLLBAR_SIZE/2;
			}
			else {
				return Item_ListBox_ThumbPosition(item);
			}
		}
	}
	else {
		return Item_ListBox_ThumbPosition(item);
	}
}


#endif

#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
static const float tceSlider783Gap = 8.0f;
static const double tceSlider783Width = 96.0;
typedef char tceSlider783Layout[
 (offsetof(editFieldDef_t, minVal) == 0 && offsetof(editFieldDef_t, maxVal) == 4 &&
  offsetof(itemDef_t, window) + offsetof(windowDef_t, rect) + offsetof(rectDef_t, x) == 0) ? 1 : -1];
enum {
 tceSlider783Text = offsetof(itemDef_t,text),
 tceSlider783Data = offsetof(itemDef_t,typeData),
 tceSlider783TextX = offsetof(itemDef_t,textRect) + offsetof(rectDef_t,x),
 tceSlider783TextW = offsetof(itemDef_t,textRect) + offsetof(rectDef_t,w),
 tceSlider783Cvar = offsetof(itemDef_t,cvar),
 tceSlider783GetValue = offsetof(displayContextDef_t,getCVarValue)
};
__declspec(naked) float Item_Slider_ThumbPosition(itemDef_t *item) {
    __asm {
        mov eax,dword ptr [esp + 0x4]
        push esi
        mov ecx,dword ptr [eax + tceSlider783Text]
        mov esi,dword ptr [eax + tceSlider783Data]
        test ecx,ecx
        jz tceSlider783_300829ed
        fld dword ptr [eax + tceSlider783TextW]
        fadd dword ptr [eax + tceSlider783TextX]
        fadd dword ptr tceSlider783Gap
        fstp dword ptr [esp + 0x8]
        jmp tceSlider783_300829f3
    tceSlider783_300829ed:
        mov ecx,dword ptr [eax]
        mov dword ptr [esp + 0x8],ecx
    tceSlider783_300829f3:
        test esi,esi
        jnz tceSlider783_30082a07
        mov ecx,dword ptr [eax + tceSlider783Cvar]
        test ecx,ecx
        jz tceSlider783_30082a07
        fld dword ptr [esp + 0x8]
        pop esi
        ret
    tceSlider783_30082a07:
        mov edx,dword ptr [eax + tceSlider783Cvar]
        mov eax,DC
        push edx
        call dword ptr [eax + tceSlider783GetValue]
        fcom dword ptr [esi]
        add esp,0x4
        fnstsw ax
        test ah,0x1
        jz tceSlider783_30082a28
        fstp st(0)
        fld dword ptr [esi]
        jmp tceSlider783_30082a37
    tceSlider783_30082a28:
        fcom dword ptr [esi + 0x4]
        fnstsw ax
        test ah,0x41
        jnz tceSlider783_30082a37
        fstp st(0)
        fld dword ptr [esi + 0x4]
    tceSlider783_30082a37:
        fsub dword ptr [esi]
        fld dword ptr [esi + 0x4]
        fsub dword ptr [esi]
        pop esi
        fdivp st(1),st(0)
        fmul qword ptr tceSlider783Width
        fadd dword ptr [esp + 0x4]
        ret
    }
}
#else
float Item_Slider_ThumbPosition(itemDef_t *item) {
	float value, range, x;
	editFieldDef_t *editDef = item->typeData;

	if (item->text) {
		x = item->textRect.x + item->textRect.w + 8;
	} else {
		x = item->window.rect.x;
	}

	if (editDef == NULL && item->cvar) {
		return x;
	}

	value = DC->getCVarValue(item->cvar);

	if (value < editDef->minVal) {
		value = editDef->minVal;
	} else if (value > editDef->maxVal) {
		value = editDef->maxVal;
	}

	range = editDef->maxVal - editDef->minVal;
	value -= editDef->minVal;
	value /= range;
	//value /= (editDef->maxVal - editDef->minVal);
	value *= SLIDER_WIDTH;
	x += value;
	// vm fuckage
	//x = x + (((float)value / editDef->maxVal) * SLIDER_WIDTH);
	return x;
}
#endif

int Item_Slider_OverSlider(itemDef_t *item, float x, float y) {
	rectDef_t r;

	r.x = Item_Slider_ThumbPosition(item) - (SLIDER_THUMB_WIDTH / 2);
	//r.y = item->window.rect.y - 2;
	r.y = item->window.rect.y;
	r.w = SLIDER_THUMB_WIDTH;
	r.h = SLIDER_THUMB_HEIGHT;

	if (Rect_ContainsPoint(&r, x, y)) {
		return WINDOW_LB_THUMB;
	}
	return 0;
}

int Item_ListBox_OverLB(itemDef_t *item, float x, float y) {
	rectDef_t r;
	listBoxDef_t *listPtr;
	int thumbstart;
	int count;

	count = DC->feederCount(item->special);
	listPtr = (listBoxDef_t*)item->typeData;
	if (item->window.flags & WINDOW_HORIZONTAL) {
		// check if on left arrow
		r.x = item->window.rect.x;
		r.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
		r.h = r.w = SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_LEFTARROW;
		}
		// check if on right arrow
		r.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_RIGHTARROW;
		}
		// check if on thumb
		//thumbstart = Item_ListBox_ThumbPosition(item);
		thumbstart = Item_ListBox_ThumbDrawPosition( item );
		r.x = thumbstart;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_THUMB;
		}
		r.x = item->window.rect.x + SCROLLBAR_SIZE;
		r.w = thumbstart - r.x;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_PGUP;
		}
		r.x = thumbstart + SCROLLBAR_SIZE;
		r.w = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_PGDN;
		}

		// hack hack
		r.x = item->window.rect.x;
		r.w = item->window.rect.w;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_SOMEWHERE;
		}
	} else {
		r.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE;
		r.y = item->window.rect.y;
		r.h = r.w = SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_LEFTARROW;
		}
		r.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_RIGHTARROW;
		}
		//thumbstart = Item_ListBox_ThumbPosition(item);
		thumbstart = Item_ListBox_ThumbDrawPosition( item );
		r.y = thumbstart;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_THUMB;
		}
		r.y = item->window.rect.y + SCROLLBAR_SIZE;
		r.h = thumbstart - r.y;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_PGUP;
		}
		r.y = thumbstart + SCROLLBAR_SIZE;
		r.h = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_PGDN;
		}

		// hack hack
		r.y = item->window.rect.y;
		r.h = item->window.rect.h;
		if (Rect_ContainsPoint(&r, x, y)) {
			return WINDOW_LB_SOMEWHERE;
		}
	}
	return 0;
}


void Item_ListBox_MouseEnter( itemDef_t *item, float x, float y, qboolean click ) 
{
	rectDef_t r;
	listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;
        
	item->window.flags &= ~(WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB | WINDOW_LB_PGUP | WINDOW_LB_PGDN | WINDOW_LB_SOMEWHERE);
	item->window.flags |= Item_ListBox_OverLB(item, x, y);

	if( click ) {
		if (item->window.flags & WINDOW_HORIZONTAL) {
			if (!(item->window.flags & (WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB | WINDOW_LB_PGUP | WINDOW_LB_PGDN | WINDOW_LB_SOMEWHERE))) {
				// check for selection hit as we have exausted buttons and thumb
				if (listPtr->elementStyle == LISTBOX_IMAGE) {
					r.x = item->window.rect.x;
					r.y = item->window.rect.y;
					r.h = item->window.rect.h - SCROLLBAR_SIZE;
					r.w = item->window.rect.w - listPtr->drawPadding;
					if( Rect_ContainsPoint(&r, x, y) ) {
						listPtr->cursorPos = (int)((x - r.x) / listPtr->elementWidth)  + listPtr->startPos;
						if (listPtr->cursorPos >= listPtr->endPos) {
							listPtr->cursorPos = listPtr->endPos;
						}
					}
				} else {
					// text hit.. 
				}
			}
		} else if (!(item->window.flags & (WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB | WINDOW_LB_PGUP | WINDOW_LB_PGDN | WINDOW_LB_SOMEWHERE))) {
			r.x = item->window.rect.x;
			r.y = item->window.rect.y;
			r.w = item->window.rect.w - SCROLLBAR_SIZE;
			r.h = item->window.rect.h - listPtr->drawPadding;
			if( Rect_ContainsPoint(&r, x, y) ) {
				listPtr->cursorPos = (int)((y - 2 - r.y) / listPtr->elementHeight)  + listPtr->startPos;
				if (listPtr->cursorPos > listPtr->endPos) {
					listPtr->cursorPos = listPtr->endPos;
				}
			}
		}
	}
}

void Item_MouseEnter(itemDef_t *item, float x, float y) {
	rectDef_t r;
	if (item) {
		r = item->textRect;
		r.y -= r.h;
		// in the text rect?

		// items can be enabled and disabled based on cvars
		if (item->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(item, CVAR_ENABLE)) {
			return;
		}

		if (item->cvarFlags & (CVAR_SHOW | CVAR_HIDE) && !Item_EnableShowViaCvar(item, CVAR_SHOW)) {
			return;
		}

		// OSP - server settings too .. (mostly for callvote)
		if((item->settingFlags & (SVS_ENABLED_SHOW | SVS_DISABLED_SHOW)) && !Item_SettingShow(item, qfalse)) {
			return;
		}
		if(item->voteFlag != 0 && !Item_SettingShow(item, qtrue)) {
			return;
		}

		if (Rect_ContainsPoint(&r, x, y)) {
			if (!(item->window.flags & WINDOW_MOUSEOVERTEXT)) {
				Item_RunScript(item, NULL, item->mouseEnterText);
				item->window.flags |= WINDOW_MOUSEOVERTEXT;
			}
			if (!(item->window.flags & WINDOW_MOUSEOVER)) {
				Item_RunScript(item, NULL, item->mouseEnter);
				item->window.flags |= WINDOW_MOUSEOVER;
			}

		} else {
			// not in the text rect
			if (item->window.flags & WINDOW_MOUSEOVERTEXT) {
				// if we were
				Item_RunScript(item, NULL, item->mouseExitText);
				item->window.flags &= ~WINDOW_MOUSEOVERTEXT;
			}
			if (!(item->window.flags & WINDOW_MOUSEOVER)) {
				Item_RunScript(item, NULL, item->mouseEnter);
				item->window.flags |= WINDOW_MOUSEOVER;
			}

			if (item->type == ITEM_TYPE_LISTBOX) {
				Item_ListBox_MouseEnter( item, x, y, qfalse );
			}
		}
	}
}

void Item_MouseLeave(itemDef_t *item) {
	if (item) {
		if (item->window.flags & WINDOW_MOUSEOVERTEXT) {
			Item_RunScript(item, NULL, item->mouseExitText);
			item->window.flags &= ~WINDOW_MOUSEOVERTEXT;
		}
		Item_RunScript(item, NULL, item->mouseExit);
		item->window.flags &= ~(WINDOW_LB_RIGHTARROW | WINDOW_LB_LEFTARROW);
	}
}

itemDef_t *Menu_HitTest(menuDef_t *menu, float x, float y) {
	int i;

	for (i = 0; i < menu->itemCount; i++) {
		if (Rect_ContainsPoint(&menu->items[i]->window.rect, x, y)) {
			return menu->items[i];
		}
	}
	return NULL;
}

void Item_SetMouseOver(itemDef_t *item, qboolean focus) {
	if (item) {
		if (focus) {
			item->window.flags |= WINDOW_MOUSEOVER;
		} else {
			item->window.flags &= ~WINDOW_MOUSEOVER;
		}
	}
}


qboolean Item_OwnerDraw_HandleKey(itemDef_t *item, int key) {
	if (item && DC->ownerDrawHandleKey) {
		return DC->ownerDrawHandleKey(item->window.ownerDraw, item->window.ownerDrawFlags, &item->special, key);
	}
	return qfalse;
}

qboolean Item_ListBox_HandleKey( itemDef_t *item, int key, qboolean down, qboolean force ) {
	listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;
	int count = DC->feederCount(item->special);
	int max, viewmax;

	if( force || (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && item->window.flags & WINDOW_HASFOCUS)) {
		max = Item_ListBox_MaxScroll(item);
		if (item->window.flags & WINDOW_HORIZONTAL) {
			viewmax = (item->window.rect.w / listPtr->elementWidth);
			if ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) 
			{
				if (!listPtr->notselectable) {
					listPtr->cursorPos--;
					if (listPtr->cursorPos < 0) {
						listPtr->cursorPos = 0;
					}
					if (listPtr->cursorPos < listPtr->startPos) {
						listPtr->startPos = listPtr->cursorPos;
					}
					if (listPtr->cursorPos >= listPtr->startPos + viewmax) {
						listPtr->startPos = listPtr->cursorPos - viewmax + 1;
					}
					item->cursorPos = listPtr->cursorPos;
					DC->feederSelection(item->special, item->cursorPos);
				}
				else {
					listPtr->startPos--;
					if (listPtr->startPos < 0)
						listPtr->startPos = 0;
				}
				return qtrue;
			}
			if ( key == K_RIGHTARROW || key == K_KP_RIGHTARROW ) 
			{
				if (!listPtr->notselectable) {
					listPtr->cursorPos++;
					if (listPtr->cursorPos < listPtr->startPos) {
						listPtr->startPos = listPtr->cursorPos;
					}
					if (listPtr->cursorPos >= count) {
						listPtr->cursorPos = count-1;
					}
					if (listPtr->cursorPos >= listPtr->startPos + viewmax) {
						listPtr->startPos = listPtr->cursorPos - viewmax + 1;
					}
					item->cursorPos = listPtr->cursorPos;
					DC->feederSelection(item->special, item->cursorPos);
				}
				else {
					listPtr->startPos++;
					if (listPtr->startPos >= count)
						listPtr->startPos = count-1;
				}
				return qtrue;
			}
		}
		else {
			viewmax = (item->window.rect.h / listPtr->elementHeight);
			if ( key == K_UPARROW || key == K_KP_UPARROW || key == K_MWHEELUP ) 
			{
				if (!listPtr->notselectable) {
					listPtr->cursorPos--;
					if (listPtr->cursorPos < 0) {
						listPtr->cursorPos = 0;
					}
					if (listPtr->cursorPos < listPtr->startPos) {
						listPtr->startPos = listPtr->cursorPos;
					}
					if (listPtr->cursorPos >= listPtr->startPos + viewmax) {
						listPtr->startPos = listPtr->cursorPos - viewmax + 1;
					}
					item->cursorPos = listPtr->cursorPos;
					DC->feederSelection(item->special, item->cursorPos);
				}
				else {
					listPtr->startPos--;
					if (listPtr->startPos < 0)
						listPtr->startPos = 0;
				}
				return qtrue;
			}
			if ( key == K_DOWNARROW || key == K_KP_DOWNARROW || key == K_MWHEELDOWN ) 
			{
				if (!listPtr->notselectable) {
					listPtr->cursorPos++;
					if (listPtr->cursorPos < listPtr->startPos) {
						listPtr->startPos = listPtr->cursorPos;
					}
					if (listPtr->cursorPos >= count) {
						listPtr->cursorPos = count-1;
					}
					if (listPtr->cursorPos >= listPtr->startPos + viewmax) {
						listPtr->startPos = listPtr->cursorPos - viewmax + 1;
					}
					item->cursorPos = listPtr->cursorPos;
					DC->feederSelection(item->special, item->cursorPos);
				}
				else {
					listPtr->startPos++;
					if (listPtr->startPos > max)
						listPtr->startPos = max;
				}
				return qtrue;
			}
		}
		// mouse hit
		if (key == K_MOUSE1 || key == K_MOUSE2) {
			Item_ListBox_MouseEnter( item, DC->cursorx, DC->cursory, qtrue );

			if (item->window.flags & WINDOW_LB_LEFTARROW) {
				listPtr->startPos--;
				if (listPtr->startPos < 0) {
					listPtr->startPos = 0;
				}
			} else if (item->window.flags & WINDOW_LB_RIGHTARROW) {
				// one down
				listPtr->startPos++;
				if (listPtr->startPos > max) {
					listPtr->startPos = max;
				}
			} else if (item->window.flags & WINDOW_LB_PGUP) {
				// page up
				listPtr->startPos -= viewmax;
				if (listPtr->startPos < 0) {
					listPtr->startPos = 0;
				}
			} else if (item->window.flags & WINDOW_LB_PGDN) {
				// page down
				listPtr->startPos += viewmax;
				if (listPtr->startPos > max) {
					listPtr->startPos = max;
				}
			} else if (item->window.flags & WINDOW_LB_THUMB) {
				// Display_SetCaptureItem(item);
			} else if( item->window.flags & WINDOW_LB_SOMEWHERE ) {
				// do nowt
			} else {
				// select an item
				// Arnout: can't select something that doesn't exist
				if (listPtr->cursorPos >= count) {
					listPtr->cursorPos = count-1;
				}

				if( item->cursorPos == listPtr->cursorPos &&
					DC->realTime < lastListBoxClickTime && listPtr->doubleClick ) {
					Item_RunScript(item, NULL, listPtr->doubleClick);
				}
				lastListBoxClickTime = DC->realTime + DOUBLE_CLICK_DELAY;

				if (item->cursorPos != listPtr->cursorPos) {
					item->cursorPos = listPtr->cursorPos;
					DC->feederSelection(item->special, item->cursorPos);
				}

				if( key == K_MOUSE1 ) {
					DC->feederSelectionClick( item );
				}

				if( key == K_MOUSE2 && listPtr->contextMenu ) {
					menuDef_t* menu = Menus_FindByName( listPtr->contextMenu );

					if( menu ) {
						menu->window.rect.x = DC->cursorx;
						menu->window.rect.y = DC->cursory;

						Menu_UpdatePosition( menu );
						Menus_ActivateByName( listPtr->contextMenu, qtrue );
					}
				}
			}
			return qtrue;
		}
		if ( key == K_HOME || key == K_KP_HOME) {
			// home
			listPtr->startPos = 0;
			return qtrue;
		}
		if ( key == K_END || key == K_KP_END) {
			// end
			listPtr->startPos = max;
			return qtrue;
		}
		if (key == K_PGUP || key == K_KP_PGUP ) {
			// page up
			if (!listPtr->notselectable) {
				listPtr->cursorPos -= viewmax;
				if (listPtr->cursorPos < 0) {
					listPtr->cursorPos = 0;
				}
				if (listPtr->cursorPos < listPtr->startPos) {
					listPtr->startPos = listPtr->cursorPos;
				}
				if (listPtr->cursorPos >= listPtr->startPos + viewmax) {
					listPtr->startPos = listPtr->cursorPos - viewmax + 1;
				}
				item->cursorPos = listPtr->cursorPos;
				DC->feederSelection(item->special, item->cursorPos);
			}
			else {
				listPtr->startPos -= viewmax;
				if (listPtr->startPos < 0) {
					listPtr->startPos = 0;
				}
			}
			return qtrue;
		}
		if ( key == K_PGDN || key == K_KP_PGDN ) {
			// page down
			if (!listPtr->notselectable) {
				listPtr->cursorPos += viewmax;
				if (listPtr->cursorPos < listPtr->startPos) {
					listPtr->startPos = listPtr->cursorPos;
				}
				if (listPtr->cursorPos >= count) {
					listPtr->cursorPos = count-1;
				}
				if (listPtr->cursorPos >= listPtr->startPos + viewmax) {
					listPtr->startPos = listPtr->cursorPos - viewmax + 1;
				}
				item->cursorPos = listPtr->cursorPos;
				DC->feederSelection(item->special, item->cursorPos);
			}
			else {
				listPtr->startPos += viewmax;
				if (listPtr->startPos > max) {
					listPtr->startPos = max;
				}
			}
			return qtrue;
		}
	}
	return qfalse;
}

qboolean Item_CheckBox_HandleKey( itemDef_t *item, int key ) {
	if( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) && item->window.flags & WINDOW_HASFOCUS && item->cvar ) {
		if( key == K_MOUSE1 || key == K_ENTER || key == K_MOUSE2 || key == K_MOUSE3 ) {
			// ATVI Wolfenstein Misc #462
			// added the flag to toggle via action script only
			if( !(item->cvarFlags & CVAR_NOTOGGLE) ) {
				if( item->type == ITEM_TYPE_TRICHECKBOX ) {
					int curvalue = DC->getCVarValue( item->cvar ) + 1;
					if( curvalue > 2 )
						curvalue = 0;
					DC->setCVar( item->cvar, va( "%i", curvalue ) );
				} else {
					DC->setCVar( item->cvar, va( "%i", !DC->getCVarValue( item->cvar ) ) );
				}
			}
			return qtrue;
		}
	}
	return qfalse;
}

qboolean Item_YesNo_HandleKey(itemDef_t *item, int key) {
  if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && item->window.flags & WINDOW_HASFOCUS && item->cvar) {
		if (key == K_MOUSE1 || key == K_ENTER || key == K_MOUSE2 || key == K_MOUSE3) {
			// ATVI Wolfenstein Misc #462
			// added the flag to toggle via action script only
			if (!(item->cvarFlags & CVAR_NOTOGGLE))
			{
	    DC->setCVar(item->cvar, va("%i", !DC->getCVarValue(item->cvar)));
			}
		  return qtrue;
		}
  }
  return qfalse;
}

int Item_Multi_CountSettings(itemDef_t *item) {
	multiDef_t *multiPtr = (multiDef_t*)item->typeData;
	if (multiPtr == NULL) {
		return 0;
	}
	return multiPtr->count;
}

int Item_Multi_FindCvarByValue(itemDef_t *item) {
	char buff[1024];
	float value = 0;
	int i;
	multiDef_t *multiPtr = (multiDef_t*)item->typeData;
	if (multiPtr) {
		if (multiPtr->strDef) {
	    DC->getCVarString(item->cvar, buff, sizeof(buff));
		} else {
			value = DC->getCVarValue(item->cvar);
		}
		for (i = 0; i < multiPtr->count; i++) {
			if (multiPtr->strDef) {
				if (Q_stricmp(buff, multiPtr->cvarStr[i]) == 0) {
					return i;
				}
			} else {
 				if (multiPtr->cvarValue[i] == value) {
 					return i;
 				}
 			}
 		}
	}
	return 0;
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const char ms793None[]="None Defined",ms793Custom[]="Custom";
enum { ms793Data=offsetof(itemDef_t,typeData),ms793Cvar=offsetof(itemDef_t,cvar),
ms793Str=offsetof(multiDef_t,strDef),ms793Count=offsetof(multiDef_t,count),
ms793Undefined=offsetof(multiDef_t,undefinedStr),ms793Values=offsetof(multiDef_t,cvarValue),
ms793StringsDelta=offsetof(multiDef_t,cvarStr)-offsetof(multiDef_t,cvarValue),
ms793List=offsetof(multiDef_t,cvarList),ms793GetString=offsetof(displayContextDef_t,getCVarString),
ms793GetValue=offsetof(displayContextDef_t,getCVarValue) };
__declspec(naked) const char *Item_Multi_Setting(itemDef_t *item) {
 __asm {
 SUB ESP,0x404
 MOV EAX,dword ptr [ESP + 0x408]
 PUSH EBX
 PUSH ESI
 PUSH EDI
 MOV ESI,dword ptr [EAX + ms793Data]
 MOV dword ptr [ESP + 0xc],0x0
 TEST ESI,ESI
 JZ ms793_30082b83
 MOV ECX,dword ptr [ESI + ms793Str]
 TEST ECX,ECX
 JZ ms793_30082b1e
 MOV EDX,dword ptr [EAX + ms793Cvar]
 MOV EAX,DC
 LEA ECX,[ESP + 0x10]
 PUSH 0x400
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + ms793GetString]
 ADD ESP,0xc
 JMP ms793_30082b35
ms793_30082b1e:
 MOV ECX,dword ptr [EAX + ms793Cvar]
 MOV EDX,dword ptr DC
 PUSH ECX
 CALL dword ptr [EDX + ms793GetValue]
 FSTP dword ptr [ESP + 0x10]
 ADD ESP,0x4
ms793_30082b35:
 MOV EAX,dword ptr [ESI + ms793Count]
 XOR EDI,EDI
 TEST EAX,EAX
 JLE ms793_30082b83
 LEA EBX,[ESI + ms793Values]
ms793_30082b47:
 MOV EAX,dword ptr [ESI + ms793Str]
 TEST EAX,EAX
 JZ ms793_30082b68
 MOV EAX,dword ptr [EBX + ms793StringsDelta]
 LEA ECX,[ESP + 0x10]
 PUSH EAX
 PUSH ECX
 CALL Q_stricmp
 ADD ESP,0x8
 TEST EAX,EAX
 JZ ms793_30082bab
 JMP ms793_30082b75
ms793_30082b68:
 FLD dword ptr [EBX]
 FCOMP dword ptr [ESP + 0xc]
 FNSTSW AX
 TEST AH,0x40
 JNZ ms793_30082bab
ms793_30082b75:
 MOV EAX,dword ptr [ESI + ms793Count]
 INC EDI
 ADD EBX,0x4
 CMP EDI,EAX
 JL ms793_30082b47
ms793_30082b83:
 MOV EAX,dword ptr [ESI + ms793Undefined]
 TEST EAX,EAX
 JNZ ms793_30082ba1
 MOV EAX,dword ptr [ESI + ms793Count]
 TEST EAX,EAX
 MOV EAX,offset ms793None
 JZ ms793_30082ba1
 MOV EAX,offset ms793Custom
ms793_30082ba1:
 POP EDI
 POP ESI
 POP EBX
 ADD ESP,0x404
 RET
ms793_30082bab:
 MOV EAX,dword ptr [ESI + EDI*4 + ms793List]
 POP EDI
 POP ESI
 POP EBX
 ADD ESP,0x404
 RET
 }
}
#else
const char *Item_Multi_Setting(itemDef_t *item) {
	char buff[1024];
	float value = 0;
	int i;
	multiDef_t *multiPtr = (multiDef_t*)item->typeData;
	if (multiPtr) {
		if (multiPtr->strDef) {
	    DC->getCVarString(item->cvar, buff, sizeof(buff));
		} else {
			value = DC->getCVarValue(item->cvar);
		}
		for (i = 0; i < multiPtr->count; i++) {
			if (multiPtr->strDef) {
				if (Q_stricmp(buff, multiPtr->cvarStr[i]) == 0) {
					return multiPtr->cvarList[i];
				}
			} else {
 				if (multiPtr->cvarValue[i] == value) {
					return multiPtr->cvarList[i];
 				}
 			}
 		}
	}
	if( multiPtr->undefinedStr ) {
		return multiPtr->undefinedStr;
	} else {
		return( ( multiPtr->count == 0 ) ? "None Defined" : "Custom" );
	}
}

#endif
qboolean Item_Multi_HandleKey(itemDef_t *item, int key) {
	multiDef_t *multiPtr = (multiDef_t*)item->typeData;
	if (multiPtr) {
	  if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && item->window.flags & WINDOW_HASFOCUS && item->cvar) {
			if (key == K_MOUSE1 || key == K_ENTER || key == K_MOUSE2 || key == K_MOUSE3) {
				int current = Item_Multi_FindCvarByValue(item);
				int max = Item_Multi_CountSettings(item);

				if( key == K_MOUSE2 ) {
					current--;
				} else {
					current++;
				}

				if ( current < 0 ) {
					current = max - 1;
				} else if( current >= max ) {
					current = 0;
				}
				if (multiPtr->strDef) {
					DC->setCVar(item->cvar, multiPtr->cvarStr[current]);
				} else {
					float value = multiPtr->cvarValue[current];
					if (((float)((int) value)) == value) {
						DC->setCVar(item->cvar, va("%i", (int) value ));
					}
					else {
						DC->setCVar(item->cvar, va("%f", value ));
					}
				}
				return qtrue;
			}
		}
	}
  return qfalse;
}

qboolean Item_TextField_HandleKey(itemDef_t *item, int key) {
	char buff[1024];
	int len;
	itemDef_t *newItem = NULL;
	editFieldDef_t *editPtr = (editFieldDef_t*)item->typeData;

	if (item->cvar) {

		memset(buff, 0, sizeof(buff));
		DC->getCVarString(item->cvar, buff, sizeof(buff));
		len = Q_strlenInt(buff);
		

		if (editPtr->maxChars && len > editPtr->maxChars) {
			len = editPtr->maxChars;
		}

		// Gordon: make sure our cursorpos doesn't go oob, windows doesn't like negative memory copy operations :)
		if( item->cursorPos < 0 || item->cursorPos > len ) {
			item->cursorPos = 0;
		}

		if ( key & K_CHAR_FLAG ) {
			key &= ~K_CHAR_FLAG;


			if (key == 'h' - 'a' + 1 )	{	// ctrl-h is backspace
				if ( item->cursorPos > 0 ) {
					memmove( &buff[item->cursorPos - 1], &buff[item->cursorPos], len + 1 - item->cursorPos);
					item->cursorPos--;
					if (item->cursorPos < editPtr->paintOffset) {
						editPtr->paintOffset--;
					}
					buff[len] = '\0';
				}
				DC->setCVar(item->cvar, buff);
	    		return qtrue;
			}


			//
			// ignore any non printable chars
			//
			if ( key < 32 || !item->cvar) {
			    return qtrue;
		    }

			if (item->type == ITEM_TYPE_NUMERICFIELD) {
				if ((key < '0' || key > '9') && key != '.') {
					return qfalse;
				}
			}

			if (DC->getOverstrikeMode && !DC->getOverstrikeMode()) {
				if (( len == MAX_EDITFIELD - 1 ) || (editPtr->maxChars && len >= editPtr->maxChars)) {
					return qtrue;
				}
				memmove( &buff[item->cursorPos + 1], &buff[item->cursorPos], len + 1 - item->cursorPos );
			} else {
				if (editPtr->maxChars && item->cursorPos >= editPtr->maxChars) {
					return qtrue;
				}
			}

			buff[item->cursorPos] = key;

			DC->setCVar(item->cvar, buff);

			if (item->cursorPos < len + 1) {
				item->cursorPos++;
				if (editPtr->maxPaintChars && item->cursorPos > editPtr->maxPaintChars) {
					editPtr->paintOffset++;
				}
			}

		} else {

			if ( key == K_DEL || key == K_KP_DEL ) {
				if ( item->cursorPos < len ) {
					memmove( buff + item->cursorPos, buff + item->cursorPos + 1, len - item->cursorPos);
					buff[len] = '\0';
					DC->setCVar(item->cvar, buff);
				}
				return qtrue;
			}

			if ( key == K_RIGHTARROW || key == K_KP_RIGHTARROW ) 
			{
				if (editPtr->maxPaintChars && item->cursorPos >= editPtr->paintOffset + editPtr->maxPaintChars && item->cursorPos < len) {
					item->cursorPos++;
					editPtr->paintOffset++;
					return qtrue;
				}
				if (item->cursorPos < len) {
					item->cursorPos++;
				} 
				return qtrue;
			}

			if ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) 
			{
				if ( item->cursorPos > 0 ) {
					item->cursorPos--;
				}
				if (item->cursorPos < editPtr->paintOffset) {
					editPtr->paintOffset--;
				}
				return qtrue;
			}

			if ( key == K_HOME || key == K_KP_HOME) {// || ( tolower(key) == 'a' && trap_Key_IsDown( K_CTRL ) ) ) {
				item->cursorPos = 0;
				editPtr->paintOffset = 0;
				return qtrue;
			}

			if ( key == K_END || key == K_KP_END)  {// ( tolower(key) == 'e' && trap_Key_IsDown( K_CTRL ) ) ) {
				item->cursorPos = len;
				if(item->cursorPos > editPtr->maxPaintChars) {
					editPtr->paintOffset = len - editPtr->maxPaintChars;
				}
				return qtrue;
			}

			if ( key == K_INS || key == K_KP_INS ) {
				DC->setOverstrikeMode(!DC->getOverstrikeMode());
				return qtrue;
			}
		}

		if (key == K_TAB || key == K_DOWNARROW || key == K_KP_DOWNARROW) {
			newItem = Menu_SetNextCursorItem(item->parent);
			if (newItem && (newItem->type == ITEM_TYPE_EDITFIELD || newItem->type == ITEM_TYPE_NUMERICFIELD)) {
				g_editItem = newItem;
			}
		}

		if (key == K_UPARROW || key == K_KP_UPARROW) {
			newItem = Menu_SetPrevCursorItem(item->parent);
			if (newItem && (newItem->type == ITEM_TYPE_EDITFIELD || newItem->type == ITEM_TYPE_NUMERICFIELD)) {
				g_editItem = newItem;
			}
		}

		// NERVE - SMF
		if ( key == K_ENTER || key == K_KP_ENTER ) {
			if ( item->onAccept )
				Item_RunScript(item, NULL, item->onAccept);
		}
		// -NERVE - SMF

		if ( key == K_ENTER || key == K_KP_ENTER || key == K_ESCAPE)  {
			return qfalse;
		}

		return qtrue;
	}
	return qfalse;

}

static void Scroll_ListBox_AutoFunc(void *p) {
	scrollInfo_t *si = (scrollInfo_t*)p;
	if (DC->realTime > si->nextScrollTime) { 
		// need to scroll which is done by simulating a click to the item
		// this is done a bit sideways as the autoscroll "knows" that the item is a listbox
		// so it calls it directly
		Item_ListBox_HandleKey(si->item, si->scrollKey, qtrue, qfalse);
		si->nextScrollTime = DC->realTime + si->adjustValue; 
	}

	if (DC->realTime > si->nextAdjustTime) {
		si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
		if (si->adjustValue > SCROLL_TIME_FLOOR) {
			si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
		}
	}
}

static void Scroll_ListBox_ThumbFunc(void *p) {
	scrollInfo_t *si = (scrollInfo_t*)p;
	rectDef_t r;
	int pos, max;

	listBoxDef_t *listPtr = (listBoxDef_t*)si->item->typeData;
	if (si->item->window.flags & WINDOW_HORIZONTAL) {
		if (DC->cursorx == si->xStart) {
			return;
		}
		r.x = si->item->window.rect.x + SCROLLBAR_SIZE + 1;
		r.y = si->item->window.rect.y + si->item->window.rect.h - SCROLLBAR_SIZE - 1;
		r.h = SCROLLBAR_SIZE;
		r.w = si->item->window.rect.w - (SCROLLBAR_SIZE*2) - 2;
		max = Item_ListBox_MaxScroll(si->item);
		//
		pos = (DC->cursorx - r.x - SCROLLBAR_SIZE/2) * max / (r.w - SCROLLBAR_SIZE);
		if (pos < 0) {
			pos = 0;
		}
		else if (pos > max) {
			pos = max;
		}
		listPtr->startPos = pos;
		si->xStart = DC->cursorx;
	}
	else if (DC->cursory != si->yStart) {

		r.x = si->item->window.rect.x + si->item->window.rect.w - SCROLLBAR_SIZE - 1;
		r.y = si->item->window.rect.y + SCROLLBAR_SIZE + 1;
		r.h = si->item->window.rect.h - (SCROLLBAR_SIZE*2) - 2;
		r.w = SCROLLBAR_SIZE;
		max = Item_ListBox_MaxScroll(si->item);
		//
		pos = (DC->cursory - r.y - SCROLLBAR_SIZE/2) * max / (r.h - SCROLLBAR_SIZE);
		if (pos < 0) {
			pos = 0;
		}
		else if (pos > max) {
			pos = max;
		}
		listPtr->startPos = pos;
		si->yStart = DC->cursory;
	}

	if (DC->realTime > si->nextScrollTime) { 
		// need to scroll which is done by simulating a click to the item
		// this is done a bit sideways as the autoscroll "knows" that the item is a listbox
		// so it calls it directly
		// Arnout: clear doubleclicktime though!
		lastListBoxClickTime = 0;
		Item_ListBox_HandleKey(si->item, si->scrollKey, qtrue, qfalse);
		si->nextScrollTime = DC->realTime + si->adjustValue; 
	}

	if (DC->realTime > si->nextAdjustTime) {
		si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
		if (si->adjustValue > SCROLL_TIME_FLOOR) {
			si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
		}
	}
}

static void Scroll_Slider_ThumbFunc(void *p) {
	float x, value, cursorx;
	scrollInfo_t *si = (scrollInfo_t*)p;
	editFieldDef_t *editDef = si->item->typeData;

	if (si->item->text) {
		x = si->item->textRect.x + si->item->textRect.w + 8;
	} else {
		x = si->item->window.rect.x;
	}

	cursorx = DC->cursorx;

	if (cursorx < x) {
		cursorx = x;
	} else if (cursorx > x + SLIDER_WIDTH) {
		cursorx = x + SLIDER_WIDTH;
	}
	value = cursorx - x;
	value /= SLIDER_WIDTH;
	value *= (editDef->maxVal - editDef->minVal);
	value += editDef->minVal;
	DC->setCVar(si->item->cvar, va("%f", value));
}

void Item_StartCapture(itemDef_t *item, int key) {
	int flags;
	switch (item->type) {
    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:

		case ITEM_TYPE_LISTBOX:
		{
			flags = Item_ListBox_OverLB(item, DC->cursorx, DC->cursory);
			if ( flags & (WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW)) {
				scrollInfo.nextScrollTime = DC->realTime + SCROLL_TIME_START;
				scrollInfo.nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
				scrollInfo.adjustValue = SCROLL_TIME_START;
				scrollInfo.scrollKey = key;
				scrollInfo.scrollDir = (flags & WINDOW_LB_LEFTARROW) ? qtrue : qfalse;
				scrollInfo.item = item;
				captureData = &scrollInfo;
				captureFunc = &Scroll_ListBox_AutoFunc;
				itemCapture = item;
			} else if (flags & WINDOW_LB_THUMB) {
				scrollInfo.scrollKey = key;
				scrollInfo.item = item;
				scrollInfo.xStart = DC->cursorx;
				scrollInfo.yStart = DC->cursory;
				captureData = &scrollInfo;
				captureFunc = &Scroll_ListBox_ThumbFunc;
				itemCapture = item;
			}
			break;
		}
		case ITEM_TYPE_SLIDER:
		{
			flags = Item_Slider_OverSlider(item, DC->cursorx, DC->cursory);
			if (flags & WINDOW_LB_THUMB) {
				scrollInfo.scrollKey = key;
				scrollInfo.item = item;
				scrollInfo.xStart = DC->cursorx;
				scrollInfo.yStart = DC->cursory;
				captureData = &scrollInfo;
				captureFunc = &Scroll_Slider_ThumbFunc;
				itemCapture = item;
			}
			break;
		}
	}
}

void Item_StopCapture(itemDef_t *item) {

}

qboolean Item_Slider_HandleKey(itemDef_t *item, int key, qboolean down) {
	float x, value, width, work;

	//DC->Print("slider handle key\n");
	if (item->window.flags & WINDOW_HASFOCUS && item->cvar && Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory)) {
		if (key == K_MOUSE1 || key == K_ENTER || key == K_MOUSE2 || key == K_MOUSE3) {
			editFieldDef_t *editDef = item->typeData;
			if (editDef) {
				rectDef_t testRect;
				width = SLIDER_WIDTH;
				if (item->text) {
					x = item->textRect.x + item->textRect.w + 8;
				} else {
					x = item->window.rect.x;
				}

				testRect = item->window.rect;
				testRect.x = x;
				value = (float)SLIDER_THUMB_WIDTH / 2;
				testRect.x -= value;
				//DC->Print("slider x: %f\n", testRect.x);
				testRect.w = (SLIDER_WIDTH + (float)SLIDER_THUMB_WIDTH / 2);
				//DC->Print("slider w: %f\n", testRect.w);
				if (Rect_ContainsPoint(&testRect, DC->cursorx, DC->cursory)) {
					work = DC->cursorx - x;
					value = work / width;
					value *= (editDef->maxVal - editDef->minVal);
					// vm fuckage
					// value = (((float)(DC->cursorx - x)/ SLIDER_WIDTH) * (editDef->maxVal - editDef->minVal));
					value += editDef->minVal;
					DC->setCVar(item->cvar, va("%f", value));
					return qtrue;
				}
			}
		}
	}
//	DC->Print("slider handle key exit\n");
	return qfalse;
}


qboolean Item_HandleKey(itemDef_t *item, int key, qboolean down) {
	int realKey;

	realKey = key;
	if( realKey & K_CHAR_FLAG ) {
		realKey &= ~K_CHAR_FLAG;
	}

	if (itemCapture) {
		Item_StopCapture(itemCapture);
		itemCapture = NULL;
		captureFunc = NULL;
		captureData = NULL;
	} else {
	  // bk001206 - parentheses
		if ( down && ( realKey == K_MOUSE1 || realKey == K_MOUSE2 || realKey == K_MOUSE3 ) ) {
			Item_StartCapture(item, key);
		}
	}

	if (!down) {
		return qfalse;
	}

	if( realKey == K_ESCAPE && item->onEsc ) {
		Item_RunScript( item, NULL, item->onEsc );
		return qtrue;
	}

	if( realKey == K_ENTER && item->onEnter ) {
		Item_RunScript( item, NULL, item->onEnter );
		return qtrue;
	}

  switch (item->type) {
    case ITEM_TYPE_BUTTON:
      return qfalse;
      break;
    case ITEM_TYPE_RADIOBUTTON:
      return qfalse;
      break;
    case ITEM_TYPE_CHECKBOX:
	case ITEM_TYPE_TRICHECKBOX:
		return Item_CheckBox_HandleKey( item, key );
		break;
    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:
      //return Item_TextField_HandleKey(item, key);
      return qfalse;
      break;
    case ITEM_TYPE_COMBO:
      return qfalse;
      break;
    case ITEM_TYPE_LISTBOX:
      return Item_ListBox_HandleKey(item, key, down, qfalse);
      break;
    case ITEM_TYPE_YESNO:
      return Item_YesNo_HandleKey(item, key);
      break;
    case ITEM_TYPE_MULTI:
      return Item_Multi_HandleKey(item, key);
      break;
    case ITEM_TYPE_OWNERDRAW:
      return Item_OwnerDraw_HandleKey(item, key);
      break;
	case ITEM_TYPE_BIND:
		return Item_Bind_HandleKey(item, key, down);
		break;
	case ITEM_TYPE_SLIDER:
		return Item_Slider_HandleKey(item, key, down);
		break;
    //case ITEM_TYPE_IMAGE:
    //  Item_Image_Paint(item);
    //  break;
	default:
		return qfalse;
		break;
	}

  //return qfalse;
}

void Item_Action(itemDef_t *item) {
	if( item ) {
		Item_RunScript(item, NULL, item->action);
	}
}

itemDef_t *Menu_SetPrevCursorItem(menuDef_t *menu) {
  qboolean wrapped = qfalse;
	int oldCursor = menu->cursorItem;
  
  if (menu->cursorItem < 0) {
    menu->cursorItem = menu->itemCount-1;
    wrapped = qtrue;
  } 

  while (menu->cursorItem > -1) {
    
    menu->cursorItem--;
    if (menu->cursorItem < 0 && !wrapped) {
      wrapped = qtrue;
      menu->cursorItem = menu->itemCount -1;
    }
		// NERVE - SMF
		if ( menu->cursorItem < 0 ) {
			menu->cursorItem = oldCursor;
			return NULL;
		}
		// -NERVE - SMF

		if (Item_SetFocus(menu->items[menu->cursorItem], DC->cursorx, DC->cursory)) {
			Menu_HandleMouseMove(menu, menu->items[menu->cursorItem]->window.rect.x + 1, menu->items[menu->cursorItem]->window.rect.y + 1);
      return menu->items[menu->cursorItem];
    }
  }
	menu->cursorItem = oldCursor;
	return NULL;

}

itemDef_t *Menu_SetNextCursorItem(menuDef_t *menu) {

	qboolean wrapped = qfalse;
	int oldCursor;
	
	if(!menu) {
		return NULL;
	}

	oldCursor = menu->cursorItem;


	if (menu->cursorItem == -1) {
		menu->cursorItem = 0;
		wrapped = qtrue;
	}

	while (menu->cursorItem < menu->itemCount) {

		menu->cursorItem++;
		if (menu->cursorItem >= menu->itemCount) {	// (SA) had a problem 'tabbing' in dialogs with only one possible button
			if(!wrapped) {
				wrapped = qtrue;
				menu->cursorItem = 0;
			} else {
				return menu->items[oldCursor];
			}
		}

		if (Item_SetFocus(menu->items[menu->cursorItem], DC->cursorx, DC->cursory)) {
			Menu_HandleMouseMove(menu, menu->items[menu->cursorItem]->window.rect.x + 1, menu->items[menu->cursorItem]->window.rect.y + 1);
			return menu->items[menu->cursorItem];
		}
	}

	menu->cursorItem = oldCursor;
	return NULL;
}


static void Window_CloseCinematic(windowDef_t *window) {
	if (window->style == WINDOW_STYLE_CINEMATIC && window->cinematic >= 0) {
		DC->stopCinematic(window->cinematic);
		window->cinematic = -1;
	}
}

static void Menu_CloseCinematics(menuDef_t *menu) {
	if (menu) {
		int i;
		Window_CloseCinematic(&menu->window);
	  for (i = 0; i < menu->itemCount; i++) {
		  Window_CloseCinematic(&menu->items[i]->window);
			if (menu->items[i]->type == ITEM_TYPE_OWNERDRAW) {
				DC->stopCinematic(0-menu->items[i]->window.ownerDraw);
			}
	  }
	}
}

static void Display_CloseCinematics() {
	int i;
	for (i = 0; i < menuCount; i++) {
		Menu_CloseCinematics(&Menus[i]);
	}
}

/*void  Menus_Activate(menuDef_t *menu) {
	menu->window.flags |= (WINDOW_HASFOCUS | WINDOW_VISIBLE);
	if (menu->onOpen) {
		itemDef_t item;
		item.parent = menu;
		Item_RunScript(&item, NULL, menu->onOpen);
	}

	if (menu->soundName && *menu->soundName) {
//		DC->stopBackgroundTrack();					// you don't want to do this since it will reset s_rawend
		DC->startBackgroundTrack(menu->soundName, menu->soundName);
	}

	Display_CloseCinematics();

}*/

void  Menus_Activate(menuDef_t *menu) {
	int i;
	for (i = 0; i < menuCount; i++) {
		Menus[i].window.flags &= ~(WINDOW_HASFOCUS | WINDOW_MOUSEOVER);
	}

	menu->window.flags |= (WINDOW_HASFOCUS | WINDOW_VISIBLE);

	if (menu->onOpen) {
		itemDef_t item;
		item.parent = menu;
		Item_RunScript(&item, NULL, menu->onOpen);
	}
	
	// ydnar: set open time (note dc time may be 0, in which case refresh code sets this)
	menu->openTime = DC->realTime;
	
	if (menu->soundName && *menu->soundName) {
//		DC->stopBackgroundTrack();					// you don't want to do this since it will reset s_rawend
		DC->startBackgroundTrack(menu->soundName, menu->soundName, 0);
	}

	Display_CloseCinematics();

}

qboolean Menus_CaptureFuncActive( void ) {
	if( captureFunc ) {
		return qtrue;
	} else {
		return qfalse;
	}
}

int Display_VisibleMenuCount() {
	int i, count;
	count = 0;
	for (i = 0; i < menuCount; i++) {
		if (Menus[i].window.flags & (WINDOW_FORCED | WINDOW_VISIBLE)) {
			count++;
		}
	}
	return count;
}

void Menus_HandleOOBClick(menuDef_t *menu, int key, qboolean down) {
	if (menu) {
		int i;
		// basically the behaviour we are looking for is if there are windows in the stack.. see if 
		// the cursor is within any of them.. if not close them otherwise activate them and pass the 
		// key on.. force a mouse move to activate focus and script stuff 
		if (down && menu->window.flags & WINDOW_OOB_CLICK) {
			Menu_RunCloseScript(menu);
			menu->window.flags &= ~(WINDOW_HASFOCUS | WINDOW_VISIBLE | WINDOW_MOUSEOVER);
		}

		for (i = 0; i < menuCount; i++) {
			if (Menu_OverActiveItem(&Menus[i], DC->cursorx, DC->cursory)) {
//				Menu_RunCloseScript(menu);			// NERVE - SMF - why do we close the calling menu instead of just removing the focus?
//				menu->window.flags &= ~(WINDOW_HASFOCUS | WINDOW_VISIBLE | WINDOW_MOUSEOVER);
				
				menu->window.flags &= ~(WINDOW_HASFOCUS | WINDOW_MOUSEOVER);
				Menus[i].window.flags |= (WINDOW_HASFOCUS | WINDOW_VISIBLE);

//				Menus_Activate(&Menus[i]);
				Menu_HandleMouseMove(&Menus[i], DC->cursorx, DC->cursory);
				Menu_HandleKey(&Menus[i], key, down);
			}
		}

		if (Display_VisibleMenuCount() == 0) {
			if (DC->Pause) {
				DC->Pause(qfalse);
			}
		}
		Display_CloseCinematics();
	}
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static rectDef_t tceCorrectedRect806;
static const float tceCorrectedZero806 = 0.0f;
enum { cr806Text = offsetof(itemDef_t, textRect) };
typedef char tceCorrectedRectLayout806[(sizeof(rectDef_t) == 16 && offsetof(rectDef_t, x) == 0 && offsetof(rectDef_t, y) == 4 && offsetof(rectDef_t, w) == 8 && offsetof(rectDef_t, h) == 12) ? 1 : -1];
/* TC 30082d30: integer copies, then x87 C3 gate (zero or unordered skips). */
__declspec(naked) static rectDef_t *Item_CorrectedTextRect(itemDef_t *item) {
 __asm {
  XOR EAX,EAX
  MOV dword ptr [tceCorrectedRect806],EAX
  MOV dword ptr [tceCorrectedRect806 + 4],EAX
  MOV dword ptr [tceCorrectedRect806 + 8],EAX
  MOV dword ptr [tceCorrectedRect806 + 12],EAX
  MOV EAX,dword ptr [ESP + 4]
  TEST EAX,EAX
  JZ cr806Done
  ADD EAX,cr806Text
  MOV ECX,dword ptr [EAX]
  MOV dword ptr [tceCorrectedRect806],ECX
  MOV EDX,dword ptr [EAX + 4]
  MOV dword ptr [tceCorrectedRect806 + 4],EDX
  MOV ECX,dword ptr [EAX + 8]
  MOV dword ptr [tceCorrectedRect806 + 8],ECX
  FLD dword ptr [tceCorrectedRect806 + 8]
  FCOMP dword ptr [tceCorrectedZero806]
  MOV EDX,dword ptr [EAX + 12]
  MOV dword ptr [tceCorrectedRect806 + 12],EDX
  FNSTSW AX
  TEST AH,0x40
  JNZ cr806Done
  FLD dword ptr [tceCorrectedRect806 + 4]
  FSUB dword ptr [tceCorrectedRect806 + 12]
  FSTP dword ptr [tceCorrectedRect806 + 4]
 cr806Done:
  MOV EAX,OFFSET tceCorrectedRect806
  RET
 }
}
#else
static rectDef_t *Item_CorrectedTextRect(itemDef_t *item) {
	static rectDef_t rect;
	memset(&rect, 0, sizeof(rectDef_t));
	if (item) {
		rect = item->textRect;
		if (rect.w) {
			rect.y -= rect.h;
		}
	}
	return &rect;
}
#endif

void Menu_HandleKey(menuDef_t *menu, int key, qboolean down) {
	int i;
	itemDef_t *item = NULL;
	qboolean inHandler = qfalse;

	Menu_HandleMouseMove( menu, DC->cursorx, DC->cursory );		// NERVE - SMF - fix for focus not resetting on unhidden buttons

	if (inHandler) {
		return;
	}
	
	// ydnar: enter key handling for the window supercedes item enter handling
	if( down && ((key == K_ENTER || key == K_KP_ENTER) && menu->onEnter) ) {
		itemDef_t it;
		it.parent = menu;
		Item_RunScript( &it, NULL, menu->onEnter );
		return;
	}
	
	inHandler = qtrue;
	if (g_waitingForKey && down) {
		Item_Bind_HandleKey(g_bindItem, key, down);
		inHandler = qfalse;
		return;
	}
	
	if (g_editingField && down) {
		if (!Item_TextField_HandleKey(g_editItem, key)) {
			g_editingField = qfalse;
			g_editItem = NULL;
			inHandler = qfalse;
			return;
		} else if (key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3) {
			g_editingField = qfalse;
			g_editItem = NULL;
			Display_MouseMove(NULL, DC->cursorx, DC->cursory);
		} else if (key == K_TAB || key == K_UPARROW || key == K_DOWNARROW) {
			return;
		}
	}

	if (menu == NULL) {
		inHandler = qfalse;
		return;
	}

		// see if the mouse is within the window bounds and if so is this a mouse click
	if (down && !(menu->window.flags & WINDOW_POPUP) && !Rect_ContainsPoint(&menu->window.rect, DC->cursorx, DC->cursory)) {
		static qboolean inHandleKey = qfalse;
		// bk001206 - parentheses
		if (!inHandleKey && ( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 ) ) {
			inHandleKey = qtrue;
			Menus_HandleOOBClick(menu, key, down);
			inHandleKey = qfalse;
			inHandler = qfalse;
			return;
		}
	}

	// get the item with focus
	for (i = 0; i < menu->itemCount; i++) {
		if (menu->items[i]->window.flags & WINDOW_HASFOCUS) {
			item = menu->items[i];
		}
	}

	if (item != NULL) {
		if (Item_HandleKey(item, key, down)) {
			Item_Action(item);
			inHandler = qfalse;
			return;
		}
	}

	if (!down) {
		inHandler = qfalse;
		return;
	}

	// START - TAT 9/16/2002
	// we need to check and see if we're supposed to loop through the items to find the key press func
	if (!menu->itemHotkeyMode) {
	// END - TAT 9/16/2002

		if ( key > 0 && key <= 255 && menu->onKey[key] ) {
			itemDef_t it;
			it.parent = menu;
			Item_RunScript( &it, NULL, menu->onKey[key] );
			return;
		}

	// START - TAT 9/16/2002
	} else if (key > 0 && key <= 255) {
		itemDef_t* it;

		// we're using the item hotkey mode, so we want to loop through all the items in this menu
	    for (i = 0; i < menu->itemCount; i++)
		{
			it = menu->items[i];

			// is the hotkey for this the same as what was pressed?
			if (it->hotkey == key
				// and is this item visible?
				&& Item_EnableShowViaCvar(it, CVAR_SHOW))
			{
				Item_RunScript(it, NULL, it->onKey);
				return;
			}
		}
	}

	// END - TAT 9/16/2002

	// default handling
	switch ( key ) {

		case K_F11:
			if (DC->getCVarValue("developer")) {
				debugMode ^= 1;
			}
			break;

		case K_F12:
			if (DC->getCVarValue("developer")) {
				DC->executeText(EXEC_APPEND, "screenshot\n");
			}
			break;
		case K_KP_UPARROW:
		case K_UPARROW:
			Menu_SetPrevCursorItem(menu);
			break;
		
		case K_ESCAPE:
			if (!g_waitingForKey && menu->onESC) {
				itemDef_t it;
				it.parent = menu;
				Item_RunScript(&it, NULL, menu->onESC);
			}
			break;
		
		
		case K_ENTER:
		case K_KP_ENTER:
		case K_MOUSE3:
			if( item )
			{
				if( item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_NUMERICFIELD )
				{
					item->cursorPos = 0;
					g_editingField = qtrue;
					g_editItem = item;
				}
				else
					Item_Action( item );
			}
			break;

		case K_TAB:
			if( DC->keyIsDown(K_SHIFT) ) {
				Menu_SetPrevCursorItem(menu);
			} else {
				Menu_SetNextCursorItem(menu);
			}
			break;
		case K_KP_DOWNARROW:
		case K_DOWNARROW:
			Menu_SetNextCursorItem(menu);
			break;

		case K_MOUSE1:
		case K_MOUSE2:
			if (item) {
				if (item->type == ITEM_TYPE_TEXT) {
					if (Rect_ContainsPoint(Item_CorrectedTextRect(item), DC->cursorx, DC->cursory)) {
						Item_Action(item);
					}
				} else if (item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_NUMERICFIELD) {
					if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory)) {
						editFieldDef_t *editPtr = (editFieldDef_t*)item->typeData;
						
						// ydnar: fixme, make it set the insertion point correctly
						
						// NERVE - SMF - reset scroll offset so we can see what we're editing
						if ( editPtr )
							editPtr->paintOffset = 0;
						
						item->cursorPos = 0;
						g_editingField = qtrue;
						g_editItem = item;
						
						// see elsewhere for venomous comment about this particular piece of "functionality"
						//%	DC->setOverstrikeMode(qtrue);
					}
				} else {
					if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory)) {
						Item_Action(item);
					}
				}
			}
			break;

		case K_JOY1:
		case K_JOY2:
		case K_JOY3:
		case K_JOY4:
		case K_AUX1:
		case K_AUX2:
		case K_AUX3:
		case K_AUX4:
		case K_AUX5:
		case K_AUX6:
		case K_AUX7:
		case K_AUX8:
		case K_AUX9:
		case K_AUX10:
		case K_AUX11:
		case K_AUX12:
		case K_AUX13:
		case K_AUX14:
		case K_AUX15:
		case K_AUX16:
			break;
	}
	inHandler = qfalse;
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
enum { tw788X=offsetof(windowDef_t,rect)+offsetof(rectDef_t,x), tw788Y=offsetof(windowDef_t,rect)+offsetof(rectDef_t,y) };
__declspec(naked) void ToWindowCoords(float *x, float *y, windowDef_t *window) {
 __asm {
  mov eax,dword ptr [esp+4]
  mov ecx,dword ptr [esp+12]
  fld dword ptr [eax]
  fadd dword ptr [ecx+tw788X]
  fstp dword ptr [eax]
  mov eax,dword ptr [esp+8]
  fld dword ptr [ecx+tw788Y]
  fadd dword ptr [eax]
  fstp dword ptr [eax]
  ret
 }
}
#else
void ToWindowCoords(float *x, float *y, windowDef_t *window) {
	/*if (window->border != 0) {
		*x += window->borderSize;
		*y += window->borderSize;
	} */
	*x += window->rect.x;
	*y += window->rect.y;
}

#endif

void Rect_ToWindowCoords(rectDef_t *rect, windowDef_t *window) {
	ToWindowCoords(&rect->x, &rect->y, window);
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
typedef char tceText788WindowAtZero[(offsetof(itemDef_t,window)==0)?1:-1];
/* Private ST0 ABI: original __ftol conversion, not a C float argument. */
__declspec(naked) static int TCE_TextInteger788(void) {
 __asm {
  push ebp
  mov ebp,esp
  sub esp,12
  fstcw word ptr [ebp-2]
  wait
  mov ax,word ptr [ebp-2]
  or ah,0ch
  mov word ptr [ebp-4],ax
  fldcw word ptr [ebp-4]
  fistp qword ptr [ebp-12]
  fldcw word ptr [ebp-2]
  mov eax,dword ptr [ebp-12]
  mov edx,dword ptr [ebp-8]
  leave
  ret
 }
}
enum {
 te788Text=offsetof(itemDef_t,text),te788W=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,w),
 te788H=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,h),te788X=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,x),
 te788Y=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,y),te788Type=offsetof(itemDef_t,type),
 te788Align=offsetof(itemDef_t,textalignment),te788Scale=offsetof(itemDef_t,textscale),
 te788Owner=offsetof(itemDef_t,window)+offsetof(windowDef_t,ownerDraw),te788Cvar=offsetof(itemDef_t,cvar),
 te788AlignX=offsetof(itemDef_t,textalignx),te788AlignY=offsetof(itemDef_t,textaligny),
 te788Width=offsetof(displayContextDef_t,textWidth),te788Height=offsetof(displayContextDef_t,textHeight),
 te788OwnerWidth=offsetof(displayContextDef_t,ownerDrawWidth),te788CvarString=offsetof(displayContextDef_t,getCVarString)
};
__declspec(naked) void Item_SetTextExtents(itemDef_t *item, int *width, int *height, const char *text) {
 __asm {
 SUB ESP,0x104
 PUSH EBX
 MOV EBX,dword ptr [ESP + 0x118]
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x110]
 TEST EBX,EBX
 PUSH EDI
 JNZ te788_30082df1
 MOV EBX,dword ptr [ESI + te788Text]
te788_30082df1:
 TEST EBX,EBX
 JZ te788_30082fb0
 FLD dword ptr [ESI + te788W]
 CALL TCE_TextInteger788
 MOV EDI,dword ptr [ESP + 0x118]
 MOV dword ptr [EDI],EAX
 FLD dword ptr [ESI + te788H]
 CALL TCE_TextInteger788
 MOV ECX,dword ptr [ESP + 0x11c]
 MOV dword ptr [ECX],EAX
 MOV EAX,dword ptr [EDI]
 TEST EAX,EAX
 JZ te788_30082e4d
 MOV EAX,dword ptr [ESI + te788Type]
 CMP EAX,0x8
 JNZ te788_30082e3b
 CMP dword ptr [ESI + te788Align],0x1
 JZ te788_30082e4d
te788_30082e3b:
 CMP dword ptr [ESI + te788Align],0x3
 JZ te788_30082e4d
 CMP EAX,0xf
 JNZ te788_30082fb0
te788_30082e4d:
 MOV EDX,dword ptr [ESI + te788Scale]
 MOV EAX,DC
 PUSH EBP
 PUSH 0x0
 PUSH EDX
 PUSH EBX
 CALL dword ptr [EAX + te788Width]
 MOV EBP,EAX
 MOV EAX,dword ptr [ESI + te788Type]
 ADD ESP,0xc
 CMP EAX,0x8
 MOV dword ptr [ESP + 0x10],EBP
 JNZ te788_30082e9f
 MOV ECX,dword ptr [ESI + te788Align]
 CMP ECX,0x1
 JZ te788_30082e84
 CMP ECX,0x2
 JNZ te788_30082e9f
te788_30082e84:
 MOV ECX,dword ptr [ESI + te788Scale]
 MOV EDX,dword ptr [ESI + te788Owner]
 MOV EAX,DC
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + te788OwnerWidth]
 ADD ESP,0x8
 JMP te788_30082f0d
te788_30082e9f:
 CMP EAX,0x4
 JNZ te788_30082ee7
 CMP dword ptr [ESI + te788Align],0x1
 JNZ te788_30082ee7
 MOV EAX,dword ptr [ESI + te788Cvar]
 TEST EAX,EAX
 JZ te788_30082ee7
 MOV EDX,dword ptr DC
 LEA ECX,[ESP + 0x14]
 PUSH 0x100
 PUSH ECX
 PUSH EAX
 CALL dword ptr [EDX + te788CvarString]
 MOV EAX,dword ptr [ESI + te788Scale]
 MOV EDX,dword ptr DC
 PUSH 0x0
 LEA ECX,[ESP + 0x24]
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + te788Width]
 ADD ESP,0x18
 JMP te788_30082f0d
te788_30082ee7:
 CMP dword ptr [ESI + te788Align],0x3
 JNZ te788_30082f13
 MOV EAX,dword ptr [ESI + te788Scale]
 MOV ECX,dword ptr [ESP + 0x124]
 MOV EDX,dword ptr DC
 PUSH 0x0
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + te788Width]
 ADD ESP,0xc
te788_30082f0d:
 ADD EBP,EAX
 MOV dword ptr [ESP + 0x10],EBP
te788_30082f13:
 MOV EAX,dword ptr [ESI + te788Scale]
 MOV ECX,dword ptr DC
 PUSH 0x0
 PUSH EAX
 PUSH EBX
 CALL dword ptr [ECX + te788Width]
 MOV dword ptr [EDI],EAX
 MOV EDX,dword ptr [ESI + te788Scale]
 MOV EAX,DC
 PUSH 0x0
 PUSH EDX
 PUSH EBX
 CALL dword ptr [EAX + te788Height]
 MOV ECX,dword ptr [ESP + 0x138]
 ADD ESP,0x18
 MOV dword ptr [ECX],EAX
 MOV EAX,dword ptr [ESI + te788AlignY]
 FILD dword ptr [EDI]
 MOV EDX,dword ptr [ESI + te788AlignX]
 LEA EDI,[ESI + te788Y]
 FSTP dword ptr [ESI + te788W]
 FILD dword ptr [ECX]
 MOV dword ptr [EDI],EAX
 MOV EAX,dword ptr [ESI + te788Align]
 LEA ECX,[ESI + te788X]
 CMP EAX,0x2
 FSTP dword ptr [ESI + te788H]
 MOV dword ptr [ECX],EDX
 JNZ te788_30082f83
 FILD dword ptr [ESP + 0x10]
 JMP te788_30082f9c
te788_30082f83:
 CMP EAX,0x1
 JZ te788_30082f8d
 CMP EAX,0x3
 JNZ te788_30082fa4
te788_30082f8d:
 MOV EAX,EBP
 CDQ
 SUB EAX,EDX
 SAR EAX,0x1
 MOV dword ptr [ESP + 0x10],EAX
 FILD dword ptr [ESP + 0x10]
te788_30082f9c:
 FSUBR dword ptr [ESI + te788AlignX]
 FSTP dword ptr [ECX]
te788_30082fa4:
 PUSH ESI
 PUSH EDI
 PUSH ECX
 CALL ToWindowCoords
 ADD ESP,0xc
 POP EBP
te788_30082fb0:
 POP EDI
 POP ESI
 POP EBX
 ADD ESP,0x104
 RET
 }
}
#else
void Item_SetTextExtents(itemDef_t *item, int *width, int *height, const char *text) {
	const char *textPtr = (text) ? text : item->text;

	if (textPtr == NULL ) {
		return;
	}

	*width = item->textRect.w;
	*height = item->textRect.h;

	// keeps us from computing the widths and heights more than once
	if( *width == 0 ||
		(item->type == ITEM_TYPE_OWNERDRAW && item->textalignment == ITEM_ALIGN_CENTER) ||
		item->textalignment == ITEM_ALIGN_CENTER2 ||
		item->type == ITEM_TYPE_TIMEOUT_COUNTER )	// ydnar
	{
		//%	int originalWidth = DC->textWidth(item->text, item->textscale, 0);
		int originalWidth = DC->textWidth(textPtr, item->textscale, 0);

		if (item->type == ITEM_TYPE_OWNERDRAW && (item->textalignment == ITEM_ALIGN_CENTER || item->textalignment == ITEM_ALIGN_RIGHT)) {
			originalWidth += DC->ownerDrawWidth(item->window.ownerDraw, item->textscale);
		} else if (item->type == ITEM_TYPE_EDITFIELD && item->textalignment == ITEM_ALIGN_CENTER && item->cvar) {
			char buff[256];
			DC->getCVarString(item->cvar, buff, 256);
			originalWidth += DC->textWidth(buff, item->textscale, 0);
		}
		else if ( item->textalignment == ITEM_ALIGN_CENTER2 ) {
			// NERVE - SMF - default centering case
			originalWidth += DC->textWidth(text, item->textscale, 0);
		}

		*width = DC->textWidth(textPtr, item->textscale, 0);
		*height = DC->textHeight(textPtr, item->textscale, 0);
		item->textRect.w = *width;
		item->textRect.h = *height;
		item->textRect.x = item->textalignx;
		item->textRect.y = item->textaligny;
		if (item->textalignment == ITEM_ALIGN_RIGHT) {
			item->textRect.x = item->textalignx - originalWidth;
		} else if (item->textalignment == ITEM_ALIGN_CENTER || item->textalignment == ITEM_ALIGN_CENTER2) {
			// NERVE - SMF - default centering case
			item->textRect.x = item->textalignx - originalWidth / 2;
		}

		ToWindowCoords(&item->textRect.x, &item->textRect.y, &item->window);
	}
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
/* TC 30082fc0: complete color controller, original pulse FSIN/store boundaries. */
static const double tc787Dim=.8, tc787One=1., tc787Half=.5;
enum {
 tc787Parent=offsetof(itemDef_t,parent), tc787Next=offsetof(itemDef_t,window)+offsetof(windowDef_t,nextTime),
 tc787Flags=offsetof(itemDef_t,window)+offsetof(windowDef_t,flags),
 tc787Fore=offsetof(itemDef_t,window)+offsetof(windowDef_t,foreColor),
 tc787Alpha=offsetof(itemDef_t,window)+offsetof(windowDef_t,foreColor)+3*sizeof(float),
 tc787Style=offsetof(itemDef_t,textStyle),tc787Enable=offsetof(itemDef_t,enableCvar),
 tc787Test=offsetof(itemDef_t,cvarTest),tc787CvarFlags=offsetof(itemDef_t,cvarFlags),
 tc787Amount=offsetof(menuDef_t,fadeAmount),tc787Cycle=offsetof(menuDef_t,fadeCycle),
 tc787Clamp=offsetof(menuDef_t,fadeClamp),tc787Focus=offsetof(menuDef_t,focusColor),
 tc787Disabled=offsetof(menuDef_t,disableColor),tc787Time=offsetof(displayContextDef_t,realTime)
};
__declspec(naked) void Item_TextColor(itemDef_t *item, vec4_t *newColor) {
 __asm {
 SUB ESP,0x10
 PUSH EBX
 PUSH EBP
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x20]
 PUSH EDI
 MOV EDI,dword ptr [ESI + tc787Parent]
 LEA EDX,[ESI + tc787Next]
 LEA EBP,[ESI + tc787Alpha]
 LEA EBX,[ESI + tc787Flags]
 MOV EAX,dword ptr [EDI + tc787Amount]
 MOV ECX,dword ptr [EDI + tc787Cycle]
 PUSH EAX
 MOV EAX,dword ptr [EDI + tc787Clamp]
 PUSH 0x1
 PUSH ECX
 PUSH EDX
 PUSH EAX
 PUSH EBP
 PUSH EBX
 CALL Fade
 MOV EBX,dword ptr [EBX]
 ADD ESP,0x1c
 TEST BL,0x2
 JZ tc787_300830ab
 TEST EBX,0x8000000
 JZ tc787_300830ab
 FLD dword ptr [EDI + tc787Focus]
 FMUL qword ptr tc787Dim
 LEA ECX,[EDI + tc787Focus]
 MOV EDX,dword ptr DC
 MOV EAX,0x1b4e81b5
 FSTP dword ptr [ESP + 0x10]
 FLD dword ptr [EDI + tc787Focus+4]
 FMUL qword ptr tc787Dim
 FSTP dword ptr [ESP + 0x14]
 FLD dword ptr [EDI + tc787Focus+8]
 FMUL qword ptr tc787Dim
 FSTP dword ptr [ESP + 0x18]
 FLD dword ptr [EDI + tc787Focus+12]
 FMUL qword ptr tc787Dim
 FSTP dword ptr [ESP + 0x1c]
 MOV EDX,dword ptr [EDX + tc787Time]
 IMUL EDX
 SAR EDX,0x3
 MOV EAX,EDX
 SHR EAX,0x1f
 ADD EDX,EAX
 MOV dword ptr [ESP + 0x24],EDX
 FILD dword ptr [ESP + 0x24]
tc787_30083081:
 FSIN
 MOV EBX,dword ptr [ESP + 0x28]
 PUSH ECX
 LEA EDX,[ESP + 0x14]
 FADD qword ptr tc787One
 FMUL qword ptr tc787Half
 FSTP dword ptr [ESP]
 PUSH EBX
 PUSH EDX
 PUSH ECX
 CALL LerpColor
 ADD ESP,0x10
 JMP tc787_30083150
tc787_300830ab:
 CMP dword ptr [ESI + tc787Style],0x1
 JNZ tc787_30083131
 MOV EBX,dword ptr DC
 MOV EAX,0x51eb851f
 MOV ECX,dword ptr [EBX + tc787Time]
 IMUL ECX
 SAR EDX,0x6
 MOV EAX,EDX
 SHR EAX,0x1f
 ADD EDX,EAX
 TEST DL,0x1
 JNZ tc787_30083131
 FLD dword ptr [ESI + tc787Fore]
 FMUL qword ptr tc787Dim
 LEA ECX,[ESI + tc787Fore]
 MOV EAX,0x1b4e81b5
 FSTP dword ptr [ESP + 0x10]
 FLD dword ptr [ESI + tc787Fore+4]
 FMUL qword ptr tc787Dim
 FSTP dword ptr [ESP + 0x14]
 FLD dword ptr [ESI + tc787Fore+8]
 FMUL qword ptr tc787Dim
 FSTP dword ptr [ESP + 0x18]
 FLD dword ptr [EBP]
 FMUL qword ptr tc787Dim
 FSTP dword ptr [ESP + 0x1c]
 MOV EDX,dword ptr [EBX + tc787Time]
 IMUL EDX
 SAR EDX,0x3
 MOV EAX,EDX
 SHR EAX,0x1f
 ADD EDX,EAX
 MOV dword ptr [ESP + 0x24],EDX
 FILD dword ptr [ESP + 0x24]
 JMP tc787_30083081
tc787_30083131:
 MOV EBX,dword ptr [ESP + 0x28]
 LEA EAX,[ESI + tc787Fore]
 MOV ECX,EBX
 MOV EDX,dword ptr [EAX]
 MOV dword ptr [ECX],EDX
 MOV EDX,dword ptr [EAX + 0x4]
 MOV dword ptr [ECX + 0x4],EDX
 MOV EDX,dword ptr [EAX + 0x8]
 MOV dword ptr [ECX + 0x8],EDX
 MOV EAX,dword ptr [EAX + 0xc]
 MOV dword ptr [ECX + 0xc],EAX
tc787_30083150:
 MOV EAX,dword ptr [ESI + tc787Enable]
 TEST EAX,EAX
 JZ tc787_300831a2
 CMP byte ptr [EAX],0x0
 JZ tc787_300831a2
 MOV EAX,dword ptr [ESI + tc787Test]
 TEST EAX,EAX
 JZ tc787_300831a2
 CMP byte ptr [EAX],0x0
 JZ tc787_300831a2
 TEST byte ptr [ESI + tc787CvarFlags],0x3
 JZ tc787_300831a2
 PUSH 0x1
 PUSH ESI
 CALL Item_EnableShowViaCvar
 ADD ESP,0x8
 TEST EAX,EAX
 JNZ tc787_300831a2
 ADD EDI,tc787Disabled
 MOV ECX,dword ptr [EDI]
 MOV dword ptr [EBX],ECX
 MOV EDX,dword ptr [EDI + 0x4]
 MOV dword ptr [EBX + 0x4],EDX
 MOV EAX,dword ptr [EDI + 0x8]
 MOV dword ptr [EBX + 0x8],EAX
 MOV ECX,dword ptr [EDI + 0xc]
 MOV dword ptr [EBX + 0xc],ECX
tc787_300831a2:
 POP EDI
 POP ESI
 POP EBP
 POP EBX
 ADD ESP,0x10
 RET
 }
}
#else
void Item_TextColor(itemDef_t *item, vec4_t *newColor) {
	vec4_t lowLight;
	menuDef_t *parent = (menuDef_t*)item->parent;

	Fade(&item->window.flags, &item->window.foreColor[3], parent->fadeClamp, &item->window.nextTime, parent->fadeCycle, qtrue, parent->fadeAmount);

	if (item->window.flags & WINDOW_HASFOCUS && item->window.flags & WINDOW_FOCUSPULSE) {
		lowLight[0] = 0.8 * parent->focusColor[0]; 
		lowLight[1] = 0.8 * parent->focusColor[1]; 
		lowLight[2] = 0.8 * parent->focusColor[2]; 
		lowLight[3] = 0.8 * parent->focusColor[3]; 
		LerpColor(parent->focusColor,lowLight,*newColor,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
	} else if (item->textStyle == ITEM_TEXTSTYLE_BLINK && !((DC->realTime/BLINK_DIVISOR) & 1)) {
		lowLight[0] = 0.8 * item->window.foreColor[0]; 
		lowLight[1] = 0.8 * item->window.foreColor[1]; 
		lowLight[2] = 0.8 * item->window.foreColor[2]; 
		lowLight[3] = 0.8 * item->window.foreColor[3]; 
		LerpColor(item->window.foreColor,lowLight,*newColor,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
	} else {
		memcpy(newColor, &item->window.foreColor, sizeof(vec4_t));
		// items can be enabled and disabled based on cvars
	}

	if (item->enableCvar && *item->enableCvar && item->cvarTest && *item->cvarTest) {
		if (item->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(item, CVAR_ENABLE)) {
			memcpy(newColor, &parent->disableColor, sizeof(vec4_t));
		}
	}
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
enum {
 aw790Text=offsetof(itemDef_t,text),aw790Cvar=offsetof(itemDef_t,cvar),
 aw790AlignY=offsetof(itemDef_t,textaligny),aw790AlignX=offsetof(itemDef_t,textalignx),
 aw790Align=offsetof(itemDef_t,textalignment),aw790Scale=offsetof(itemDef_t,textscale),aw790Style=offsetof(itemDef_t,textStyle),
 aw790RectW=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,w),
 aw790X=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,x),aw790Y=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,y),
 aw790CvarString=offsetof(displayContextDef_t,getCVarString),aw790Width=offsetof(displayContextDef_t,textWidth),aw790Draw=offsetof(displayContextDef_t,drawText)
};
/* TC Windows complete local wrapping controller; preserve FILD comparisons. */
__declspec(naked) void Item_Text_AutoWrapped_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x830
 PUSH EBX
 PUSH EBP
 PUSH ESI
 PUSH EDI
 MOV EDI,dword ptr [ESP + 0x844]
 XOR ESI,ESI
 MOV dword ptr [ESP + 0x1c],ESI
 MOV EBP,dword ptr [EDI + aw790Text]
 CMP EBP,ESI
 JNZ aw790_30083200
 MOV EAX,dword ptr [EDI + aw790Cvar]
 CMP EAX,ESI
 JZ aw790_300833d3
 MOV EDX,dword ptr DC
 LEA ECX,[ESP + 0x440]
 PUSH 0x400
 PUSH ECX
 PUSH EAX
 CALL dword ptr [EDX + aw790CvarString]
 ADD ESP,0xc
 LEA EBP,[ESP + 0x440]
aw790_30083200:
 CMP byte ptr [EBP],0x0
 JZ aw790_300833d3
 LEA EAX,[ESP + 0x30]
 PUSH EAX
 PUSH EDI
 CALL Item_TextColor
 LEA ECX,[ESP + 0x30]
 PUSH EBP
 LEA EDX,[ESP + 0x38]
 PUSH ECX
 PUSH EDX
 PUSH EDI
 CALL Item_SetTextExtents
 MOV EAX,dword ptr [EDI + aw790AlignY]
 ADD ESP,0x18
 XOR EBX,EBX
 MOV dword ptr [ESP + 0x24],EAX
 TEST EBP,EBP
 MOV byte ptr [ESP + 0x40],0x0
 MOV dword ptr [ESP + 0x14],ESI
 MOV dword ptr [ESP + 0x18],EBX
 MOV dword ptr [ESP + 0x20],EBX
 JZ aw790_300833d3
aw790_3008324e:
 MOV ECX,dword ptr [EDI + aw790Scale]
 MOV EAX,DC
 PUSH 0x0
 LEA EDX,[ESP + 0x44]
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + aw790Width]
 MOV CL,byte ptr [EBP]
 ADD ESP,0xc
 MOV EDX,EAX
 CMP CL,0x20
 MOV dword ptr [ESP + 0x10],EDX
 JZ aw790_300832a9
 CMP CL,0x9
 JZ aw790_300832a9
 CMP CL,0xa
 JZ aw790_300832a9
 TEST CL,CL
 JZ aw790_300832a9
 MOV EAX,dword ptr [ESP + 0x20]
 TEST EAX,EAX
 JNZ aw790_300832c2
 FILD dword ptr [ESP + 0x10]
 FCOMP dword ptr [EDI + aw790RectW]
 FNSTSW AX
 TEST AH,0x41
 JNZ aw790_300832c2
 MOV EBX,EDX
 MOV dword ptr [ESP + 0x14],ESI
 MOV dword ptr [ESP + 0x1c],EBP
 MOV dword ptr [ESP + 0x18],EBX
 JMP aw790_300832c2
aw790_300832a9:
 LEA EAX,[EBP + 0x1]
 MOV EBX,EDX
 MOV dword ptr [ESP + 0x14],ESI
 MOV dword ptr [ESP + 0x1c],EAX
 MOV dword ptr [ESP + 0x18],EBX
 MOV dword ptr [ESP + 0x20],0x1
aw790_300832c2:
 MOV EAX,dword ptr [ESP + 0x14]
 TEST EAX,EAX
 JZ aw790_300832d8
 FILD dword ptr [ESP + 0x10]
 FCOMP dword ptr [EDI + aw790RectW]
 FNSTSW AX
 TEST AH,0x41
 JZ aw790_300832fe
aw790_300832d8:
 CMP CL,0xa
 JZ aw790_300832fe
 TEST CL,CL
 JZ aw790_300832fe
 MOV byte ptr [ESP + ESI*0x1 + 0x40],CL
 MOV AL,byte ptr [ESP + ESI*0x1 + 0x40]
 INC ESI
 INC EBP
 CMP AL,0xd
 JNZ aw790_300832f4
 MOV byte ptr [ESP + ESI*0x1 + 0x3f],0x20
aw790_300832f4:
 MOV byte ptr [ESP + ESI*0x1 + 0x40],0x0
 JMP aw790_300833cb
aw790_300832fe:
 TEST ESI,ESI
 JZ aw790_3008339a
 MOV EAX,dword ptr [EDI + aw790Align]
 TEST EAX,EAX
 JNZ aw790_3008331e
 MOV ECX,dword ptr [EDI + aw790AlignX]
 MOV dword ptr [EDI + aw790X],ECX
 JMP aw790_30083349
aw790_3008331e:
 CMP EAX,0x2
 JNZ aw790_30083329
 FILD dword ptr [ESP + 0x18]
 JMP aw790_3008333d
aw790_30083329:
 CMP EAX,0x1
 JNZ aw790_30083349
 MOV EAX,EBX
 CDQ
 SUB EAX,EDX
 SAR EAX,0x1
 MOV dword ptr [ESP + 0x10],EAX
 FILD dword ptr [ESP + 0x10]
aw790_3008333d:
 FSUBR dword ptr [EDI + aw790AlignX]
 FSTP dword ptr [EDI + aw790X]
aw790_30083349:
 MOV EDX,dword ptr [ESP + 0x24]
 LEA ESI,[EDI + aw790Y]
 LEA EBX,[EDI + aw790X]
 PUSH EDI
 PUSH ESI
 PUSH EBX
 MOV dword ptr [ESI],EDX
 CALL ToWindowCoords
 MOV EAX,dword ptr [ESP + 0x20]
 MOV ECX,dword ptr [EDI + aw790Style]
 PUSH ECX
 MOV ECX,dword ptr [EDI + aw790Scale]
 MOV byte ptr [ESP + EAX*0x1 + 0x50],0x0
 PUSH 0x0
 LEA EDX,[ESP + 0x54]
 PUSH 0x0
 LEA EAX,[ESP + 0x48]
 PUSH EDX
 MOV EDX,dword ptr [ESI]
 PUSH EAX
 MOV EAX,dword ptr [EBX]
 PUSH ECX
 MOV ECX,dword ptr DC
 PUSH EDX
 PUSH EAX
 CALL dword ptr [ECX + aw790Draw]
 ADD ESP,0x2c
aw790_3008339a:
 CMP byte ptr [EBP],0x0
 JZ aw790_300833d3
 MOV EDX,dword ptr [ESP + 0x28]
 MOV EBP,dword ptr [ESP + 0x1c]
 ADD EDX,0x5
 XOR ESI,ESI
 MOV dword ptr [ESP + 0x10],EDX
 XOR EBX,EBX
 FILD dword ptr [ESP + 0x10]
 MOV dword ptr [ESP + 0x14],ESI
 MOV dword ptr [ESP + 0x18],EBX
 MOV dword ptr [ESP + 0x20],EBX
 FADD dword ptr [ESP + 0x24]
 FSTP dword ptr [ESP + 0x24]
aw790_300833cb:
 TEST EBP,EBP
 JNZ aw790_3008324e
aw790_300833d3:
 POP EDI
 POP ESI
 POP EBP
 POP EBX
 ADD ESP,0x830
 RET
 }
}
#else
void Item_Text_AutoWrapped_Paint(itemDef_t *item) {
	char text[1024];
	const char *p, *textPtr, *newLinePtr;
	char buff[1024];
	int width, height, len, textWidth, newLine, newLineWidth;
	qboolean hasWhitespace;
	float y;
	vec4_t color;

	textWidth = 0;
	newLinePtr = NULL;

	if (item->text == NULL) {
		if (item->cvar == NULL) {
			return;
		}
		else {
			DC->getCVarString(item->cvar, text, sizeof(text));
			textPtr = text;
		}
	}
	else {
		textPtr = item->text;
	}
	if (*textPtr == '\0') {
		return;
	}
	Item_TextColor(item, &color);
	Item_SetTextExtents(item, &width, &height, textPtr);

	y = item->textaligny;
	len = 0;
	buff[0] = '\0';
	newLine = 0;
	newLineWidth = 0;
	hasWhitespace = qfalse;
	p = textPtr;
	while (p) {
		textWidth = DC->textWidth( buff, item->textscale, 0 );
		if( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\0' ) {
			newLine = len;
			newLinePtr = p+1;
			newLineWidth = textWidth;
			hasWhitespace = qtrue;
		} else if( !hasWhitespace && textWidth > item->window.rect.w ) {
			newLine = len;
			newLinePtr = p;
			newLineWidth = textWidth;
		}
		if ( (newLine && textWidth > item->window.rect.w) || *p == '\n' || *p == '\0') {
			if (len) {
				if (item->textalignment == ITEM_ALIGN_LEFT) {
					item->textRect.x = item->textalignx;
				} else if (item->textalignment == ITEM_ALIGN_RIGHT) {
					item->textRect.x = item->textalignx - newLineWidth;
				} else if (item->textalignment == ITEM_ALIGN_CENTER) {
					item->textRect.x = item->textalignx - newLineWidth / 2;
				}
				item->textRect.y = y;
				ToWindowCoords(&item->textRect.x, &item->textRect.y, &item->window);
				//
				buff[newLine] = '\0';
				DC->drawText(item->textRect.x, item->textRect.y, item->textscale, color, buff, 0, 0, item->textStyle);
			}
			if (*p == '\0') {
				break;
			}
			//
			y += height + 5;
			p = newLinePtr;
			len = 0;
			newLine = 0;
			newLineWidth = 0;
			hasWhitespace = qfalse;
			continue;
		}
		buff[len++] = *p++;

		if(buff[len-1] == 13) {
			buff[len-1] = ' ';
		}

		buff[len] = '\0';
	}
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
enum {
 wr789Text=offsetof(itemDef_t,text),wr789Cvar=offsetof(itemDef_t,cvar),
 wr789X=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,x),wr789Y=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,y),
 wr789Style=offsetof(itemDef_t,textStyle),wr789Scale=offsetof(itemDef_t,textscale),
 wr789CvarString=offsetof(displayContextDef_t,getCVarString),wr789Draw=offsetof(displayContextDef_t,drawText)
};
/* TC Windows whole local body; CRT and font-provider contracts remain separate. */
__declspec(naked) void Item_Text_Wrapped_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x824
 PUSH EBP
 MOV EBP,dword ptr [ESP + 0x82c]
 PUSH EDI
 MOV EDI,dword ptr [EBP + wr789Text]
 TEST EDI,EDI
 JNZ wr789_30083428
 MOV EAX,dword ptr [EBP + wr789Cvar]
 TEST EAX,EAX
 JZ wr789_3008351b
 MOV EDX,dword ptr DC
 LEA ECX,[ESP + 0x42c]
 PUSH 0x400
 PUSH ECX
 PUSH EAX
 CALL dword ptr [EDX + wr789CvarString]
 ADD ESP,0xc
 LEA EDI,[ESP + 0x42c]
wr789_30083428:
 CMP byte ptr [EDI],0x0
 JZ wr789_3008351b
 PUSH EBX
 LEA EAX,[ESP + 0x20]
 PUSH ESI
 PUSH EAX
 PUSH EBP
 CALL Item_TextColor
 LEA ECX,[ESP + 0x24]
 PUSH EDI
 LEA EDX,[ESP + 0x2c]
 PUSH ECX
 PUSH EDX
 PUSH EBP
 CALL Item_SetTextExtents
 MOV EAX,dword ptr [EBP + wr789X]
 MOV ECX,dword ptr [EBP + wr789Y]
 PUSH 0xd
 PUSH EDI
 MOV dword ptr [ESP + 0x34],EAX
 MOV dword ptr [ESP + 0x30],ECX
 CALL strchr
 MOV ESI,EAX
 ADD ESP,0x20
 TEST ESI,ESI
 JZ wr789_300834eb
wr789_30083474:
 CMP byte ptr [ESI],0x0
 JZ wr789_300834eb
 MOV EBX,ESI
 LEA EAX,[ESP + 0x34]
 SUB EBX,EDI
 LEA EDX,[EBX + 0x1]
 PUSH EDX
 PUSH EDI
 PUSH EAX
 CALL strncpy
 MOV ECX,dword ptr [EBP + wr789Style]
 LEA EDX,[ESP + 0x40]
 PUSH ECX
 MOV ECX,dword ptr [EBP + wr789Scale]
 PUSH 0x0
 PUSH 0x0
 LEA EAX,[ESP + 0x3c]
 PUSH EDX
 MOV EDX,dword ptr [ESP + 0x2c]
 PUSH EAX
 MOV EAX,dword ptr [ESP + 0x34]
 PUSH ECX
 MOV ECX,dword ptr DC
 PUSH EDX
 PUSH EAX
 MOV byte ptr [ESP + EBX*0x1 + 0x60],0x0
 CALL dword ptr [ECX + wr789Draw]
 MOV EDX,dword ptr [ESP + 0x48]
 LEA EDI,[ESI + 0x1]
 ADD EDX,0x5
 PUSH 0xd
 MOV dword ptr [ESP + 0x48],EDX
 PUSH EDI
 FILD dword ptr [ESP + 0x4c]
 FADD dword ptr [ESP + 0x44]
 FSTP dword ptr [ESP + 0x44]
 CALL strchr
 MOV ESI,EAX
 ADD ESP,0x34
 TEST ESI,ESI
 JNZ wr789_30083474
wr789_300834eb:
 MOV EAX,dword ptr [EBP + wr789Style]
 MOV EDX,dword ptr [EBP + wr789Scale]
 PUSH EAX
 MOV EAX,dword ptr [ESP + 0x14]
 PUSH 0x0
 PUSH 0x0
 LEA ECX,[ESP + 0x30]
 PUSH EDI
 PUSH ECX
 MOV ECX,dword ptr [ESP + 0x28]
 PUSH EDX
 MOV EDX,dword ptr DC
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + wr789Draw]
 ADD ESP,0x20
 POP ESI
 POP EBX
wr789_3008351b:
 POP EDI
 POP EBP
 ADD ESP,0x824
 RET
 }
}
#else
void Item_Text_Wrapped_Paint(itemDef_t *item) {
	char text[1024];
	const char *p, *start, *textPtr;
	char buff[1024];
	int width, height;
	float x, y;
	vec4_t color;

	// now paint the text and/or any optional images
	// default to left

	if (item->text == NULL) {
		if (item->cvar == NULL) {
			return;
		}
		else {
			DC->getCVarString(item->cvar, text, sizeof(text));
			textPtr = text;
		}
	}
	else {
		textPtr = item->text;
	}
	if (*textPtr == '\0') {
		return;
	}

	Item_TextColor(item, &color);
	Item_SetTextExtents(item, &width, &height, textPtr);

	x = item->textRect.x;
	y = item->textRect.y;
	start = textPtr;
	p = strchr(textPtr, '\r');
	while (p && *p) {
		strncpy(buff, start, p-start+1);
		buff[p-start] = '\0';
		DC->drawText(x, y, item->textscale, color, buff, 0, 0, item->textStyle);
		y += height + 5;
		start += p - start + 1;
		p = strchr(p+1, '\r');
	}
	DC->drawText(x, y, item->textscale, color, start, 0, 0, item->textStyle);
}

#endif

void Item_Text_Paint(itemDef_t *item) {
	char text[1024];
	const char *textPtr;
	int height, width;
	vec4_t color;
	int seconds;
	menuDef_t *menu = (menuDef_t*) item->parent;

	if (item->window.flags & WINDOW_WRAPPED) {
		Item_Text_Wrapped_Paint(item);
		return;
	}
	if (item->window.flags & WINDOW_AUTOWRAPPED) {
		Item_Text_AutoWrapped_Paint(item);
		return;
	}

	if (item->text == NULL) {
		if (item->cvar == NULL) {
			return;
		}
		else {
			DC->getCVarString(item->cvar, text, sizeof(text));
			if( item->window.flags & WINDOW_TEXTASINT ) {
				COM_StripExtension(text, text);
				item->textRect.w = 0;	// force recalculation
			} else if( item->window.flags & WINDOW_TEXTASFLOAT ) {
				char *s = va( "%.2f", atof(text) );
				Q_strncpyz( text, s, sizeof(text) );
				item->textRect.w = 0;	// force recalculation
			}
			textPtr = text;
		}
	}
	else {
		textPtr = item->text;
	}
	
	// ydnar: handle counters
	if( item->type == ITEM_TYPE_TIMEOUT_COUNTER && menu != NULL && menu->openTime > 0 )
	{
		// calc seconds remaining
		/* TC 30083633..30083655: wrap the 32-bit SUB/LEA before
		 * signed division; signed C overflow is not the original contract. */
		{
			unsigned int tceTimeoutBits786 = (unsigned int)menu->timeout
				- (unsigned int)DC->realTime + (unsigned int)menu->openTime + 999u;
			memcpy(&seconds, &tceTimeoutBits786, sizeof(seconds));
			seconds /= 1000;
		}
		
		// build string
		if( seconds <= 2 )
			//Com_sprintf( text, 255, "^1%d", seconds );
			Com_sprintf( text, 255, item->text, va( "^1%d^*", seconds ) );
		else
			//Com_sprintf( text, 255, "%d", seconds );
			Com_sprintf( text, 255, item->text, va( "%d", seconds ) );
		
		// set ptr
		textPtr = text;
	}

	// this needs to go here as it sets extents for cvar types as well
	Item_SetTextExtents(item, &width, &height, textPtr);

	if (*textPtr == '\0') {
		return;
	}


	Item_TextColor(item, &color);

	//FIXME: this is a fucking mess
/*
	adjust = 0;
	if (item->textStyle == ITEM_TEXTSTYLE_OUTLINED || item->textStyle == ITEM_TEXTSTYLE_OUTLINESHADOWED) {
		adjust = 0.5;
	}

	if (item->textStyle == ITEM_TEXTSTYLE_SHADOWED || item->textStyle == ITEM_TEXTSTYLE_OUTLINESHADOWED) {
		Fade(&item->window.flags, &DC->Assets.shadowColor[3], DC->Assets.fadeClamp, &item->window.nextTime, DC->Assets.fadeCycle, qfalse);
		DC->drawText(item->textRect.x + DC->Assets.shadowX, item->textRect.y + DC->Assets.shadowY, item->textscale, DC->Assets.shadowColor, textPtr, adjust);
	}
*/


//	if (item->textStyle == ITEM_TEXTSTYLE_OUTLINED || item->textStyle == ITEM_TEXTSTYLE_OUTLINESHADOWED) {
//		Fade(&item->window.flags, &item->window.outlineColor[3], DC->Assets.fadeClamp, &item->window.nextTime, DC->Assets.fadeCycle, qfalse);
//		/*
//		Text_Paint(item->textRect.x-1, item->textRect.y-1, item->textscale, item->window.foreColor, textPtr, adjust);
//		Text_Paint(item->textRect.x, item->textRect.y-1, item->textscale, item->window.foreColor, textPtr, adjust);
//		Text_Paint(item->textRect.x+1, item->textRect.y-1, item->textscale, item->window.foreColor, textPtr, adjust);
//		Text_Paint(item->textRect.x-1, item->textRect.y, item->textscale, item->window.foreColor, textPtr, adjust);
//		Text_Paint(item->textRect.x+1, item->textRect.y, item->textscale, item->window.foreColor, textPtr, adjust);
//		Text_Paint(item->textRect.x-1, item->textRect.y+1, item->textscale, item->window.foreColor, textPtr, adjust);
//		Text_Paint(item->textRect.x, item->textRect.y+1, item->textscale, item->window.foreColor, textPtr, adjust);
//		Text_Paint(item->textRect.x+1, item->textRect.y+1, item->textscale, item->window.foreColor, textPtr, adjust);
//		*/
//		DC->drawText(item->textRect.x - 1, item->textRect.y + 1, item->textscale * 1.02, item->window.outlineColor, textPtr, adjust);
//	}

	DC->drawText(item->textRect.x, item->textRect.y, item->textscale, color, textPtr, 0, 0, item->textStyle);
}



#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
/* TC cgame 30083710: preserve live x87 editfield width/scroll arithmetic. */
static const double tf791Dim=0.8, tf791One=1.0, tf791Half=0.5;
enum {
 tf791Data=offsetof(itemDef_t,typeData),
 tf791Cvar=offsetof(itemDef_t,cvar),
 tf791Parent=offsetof(itemDef_t,parent),
 tf791Text=offsetof(itemDef_t,text),
 tf791Scale=offsetof(itemDef_t,textscale),
 tf791Style=offsetof(itemDef_t,textStyle),
 tf791Cursor=offsetof(itemDef_t,cursorPos),
 tf791X=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,x),
 tf791Y=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,y),
 tf791W=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,w),
 tf791Flags=offsetof(itemDef_t,window)+offsetof(windowDef_t,flags),
 tf791Color=offsetof(itemDef_t,window)+offsetof(windowDef_t,foreColor),
 tf791RectW=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,w),
 tf791RectX=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,x),
 tf791Paint=offsetof(editFieldDef_t,paintOffset),
 tf791Max=offsetof(editFieldDef_t,maxPaintChars),
 tf791Focus=offsetof(menuDef_t,focusColor),
 tf791Time=offsetof(displayContextDef_t,realTime),
 tf791GetString=offsetof(displayContextDef_t,getCVarString),
 tf791Width=offsetof(displayContextDef_t,textWidth),
 tf791Overstrike=offsetof(displayContextDef_t,getOverstrikeMode),
 tf791CursorDraw=offsetof(displayContextDef_t,drawTextWithCursor),
 tf791Draw=offsetof(displayContextDef_t,drawText)
};
__declspec(naked) void Item_TextField_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x42c
 PUSH EBP
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x438]
 PUSH EDI
 PUSH ESI
 MOV EBP,dword ptr [ESI + tf791Data]
 CALL Item_Text_Paint
 MOV EAX,dword ptr [ESI + tf791Cvar]
 ADD ESP,0x4
 TEST EAX,EAX
 MOV byte ptr [ESP + 0x38],0x0
 JZ tf791_30083755
 MOV EDX,dword ptr DC
 LEA ECX,[ESP + 0x38]
 PUSH 0x400
 PUSH ECX
 PUSH EAX
 CALL dword ptr [EDX + tf791GetString]
 ADD ESP,0xc
tf791_30083755:
 MOV ECX,dword ptr [ESI + tf791Flags]
 MOV EAX,dword ptr [ESI + tf791Parent]
 TEST CL,0x2
 JZ tf791_30083804
 TEST ECX,0x8000000
 JZ tf791_30083804
 FLD dword ptr [EAX + tf791Focus]
 FMUL qword ptr tf791Dim
 LEA ECX,[EAX + tf791Focus]
 PUSH ECX
 FSTP dword ptr [ESP + 0x2c]
 FLD dword ptr [EAX + tf791Focus + 4]
 FMUL qword ptr tf791Dim
 FSTP dword ptr [ESP + 0x30]
 FLD dword ptr [EAX + tf791Focus + 8]
 FMUL qword ptr tf791Dim
 FSTP dword ptr [ESP + 0x34]
 FLD dword ptr [EAX + tf791Focus + 12]
 FMUL qword ptr tf791Dim
 MOV EAX,DC
 FSTP dword ptr [ESP + 0x38]
 MOV EDX,dword ptr [EAX + tf791Time]
 MOV EAX,0x1b4e81b5
 IMUL EDX
 SAR EDX,0x3
 MOV EAX,EDX
 SHR EAX,0x1f
 ADD EDX,EAX
 LEA EAX,[ESP + 0x2c]
 MOV dword ptr [ESP + 0x10],EDX
 LEA EDX,[ESP + 0x1c]
 FILD dword ptr [ESP + 0x10]
 FSIN
 FADD qword ptr tf791One
 FMUL qword ptr tf791Half
 FSTP dword ptr [ESP]
 PUSH EDX
 PUSH EAX
 PUSH ECX
 CALL LerpColor
 ADD ESP,0x10
 JMP tf791_30083823
tf791_30083804:
 LEA ECX,[ESI + tf791Color]
 MOV EDX,dword ptr [ESI + tf791Color]
 MOV dword ptr [ESP + 0x18],EDX
 MOV EAX,dword ptr [ECX + 0x4]
 MOV dword ptr [ESP + 0x1c],EAX
 MOV EDX,dword ptr [ECX + 0x8]
 MOV dword ptr [ESP + 0x20],EDX
 MOV EAX,dword ptr [ECX + 0xc]
 MOV dword ptr [ESP + 0x24],EAX
tf791_30083823:
 MOV EAX,dword ptr [ESI + tf791Text]
 TEST EAX,EAX
 JZ tf791_3008383b
 MOV CL,byte ptr [EAX]
 MOV dword ptr [ESP + 0x14],0x8
 TEST CL,CL
 JNZ tf791_30083843
tf791_3008383b:
 MOV dword ptr [ESP + 0x14],0x0
tf791_30083843:
 MOV EAX,dword ptr [EBP + tf791Paint]
 XOR EDI,EDI
 LEA ECX,[ESP + EAX*0x1 + 0x38]
 TEST ECX,ECX
 JZ tf791_300838d7
 FILD dword ptr [ESP + 0x14]
 FSTP dword ptr [ESP + 0xc]
tf791_3008385c:
 MOV EDX,dword ptr [ESI + tf791Scale]
 MOV ECX,dword ptr DC
 ADD EAX,EDI
 PUSH 0x0
 PUSH EDX
 LEA EAX,[ESP + EAX*0x1 + 0x40]
 PUSH EAX
 CALL dword ptr [ECX + tf791Width]
 MOV dword ptr [ESP + 0x1c],EAX
 ADD ESP,0xc
 FILD dword ptr [ESP + 0x10]
 FADD dword ptr [ESI + tf791X]
 FADD dword ptr [ESI + tf791W]
 FADD dword ptr [ESP + 0xc]
 FLD dword ptr [ESI + tf791RectW]
 FADD dword ptr [ESI + tf791RectX]
 FCOMPP
 FNSTSW AX
 TEST AH,0x1
 JZ tf791_300838ad
 MOV EAX,dword ptr [EBP + tf791Paint]
 INC EDI
 LEA EDX,[EAX + EDI*0x1]
 LEA ECX,[ESP + EDX*0x1 + 0x38]
 TEST ECX,ECX
 JNZ tf791_3008385c
tf791_300838ad:
 TEST EDI,EDI
 JZ tf791_300838d7
 FLD dword ptr [ESI + tf791RectW]
 FADD dword ptr [ESI + tf791RectX]
 FILD dword ptr [ESP + 0x10]
 FADD dword ptr [ESI + tf791X]
 FADD dword ptr [ESI + tf791W]
 FADD dword ptr [ESP + 0xc]
 FSUBP ST(1),ST(0)
 CALL TCE_TextInteger788
 MOV dword ptr [ESP + 0x10],EAX
 JMP tf791_300838df
tf791_300838d7:
 MOV dword ptr [ESP + 0x10],0x0
tf791_300838df:
 TEST byte ptr [ESI + tf791Flags],0x2
 JZ tf791_30083970
 MOV EAX,g_editingField
 TEST EAX,EAX
 JZ tf791_30083970
 MOV EDX,dword ptr DC
 CALL dword ptr [EDX + tf791Overstrike]
 FLD dword ptr [ESI + tf791W]
 FADD dword ptr [ESI + tf791X]
 MOV ECX,dword ptr [ESI + tf791Style]
 MOV EDX,dword ptr [EBP + tf791Max]
 NEG EAX
 FIADD dword ptr [ESP + 0x10]
 SBB AL,AL
 PUSH ECX
 AND AL,0xe3
 PUSH EDX
 MOV EDX,dword ptr [ESI + tf791Cursor]
 ADD AL,0x7c
 MOV byte ptr [ESP + 0x14],AL
 MOV EAX,dword ptr [EBP + tf791Paint]
 MOV ECX,dword ptr [ESP + 0x14]
 SUB EDX,EAX
 ADD EAX,EDI
 SUB EDX,EDI
 FIADD dword ptr [ESP + 0x1c]
 PUSH ECX
 LEA EAX,[ESP + EAX*0x1 + 0x44]
 PUSH EDX
 MOV EDX,dword ptr [ESI + tf791Scale]
 LEA ECX,[ESP + 0x28]
 PUSH EAX
 MOV EAX,dword ptr [ESI + tf791Y]
 PUSH ECX
 PUSH EDX
 PUSH EAX
 PUSH ECX
 MOV ECX,dword ptr DC
 FSTP dword ptr [ESP]
 CALL dword ptr [ECX + tf791CursorDraw]
 ADD ESP,0x24
 POP EDI
 POP ESI
 POP EBP
 ADD ESP,0x42c
 RET
tf791_30083970:
 FLD dword ptr [ESI + tf791W]
 FADD dword ptr [ESI + tf791X]
 MOV ECX,dword ptr [EBP + tf791Paint]
 MOV EDX,dword ptr [ESI + tf791Style]
 MOV EAX,dword ptr [EBP + tf791Max]
 ADD ECX,EDI
 FIADD dword ptr [ESP + 0x10]
 PUSH EDX
 PUSH EAX
 LEA EDX,[ESP + ECX*0x1 + 0x40]
 MOV ECX,dword ptr [ESI + tf791Scale]
 PUSH 0x0
 LEA EAX,[ESP + 0x24]
 PUSH EDX
 MOV EDX,dword ptr [ESI + tf791Y]
 PUSH EAX
 MOV EAX,DC
 PUSH ECX
 PUSH EDX
 FIADD dword ptr [ESP + 0x30]
 PUSH ECX
 FSTP dword ptr [ESP]
 CALL dword ptr [EAX + tf791Draw]
 ADD ESP,0x20
 POP EDI
 POP ESI
 POP EBP
 ADD ESP,0x42c
 RET
 }
}
#else
void Item_TextField_Paint(itemDef_t *item) {
	char buff[1024];
	vec4_t newColor, lowLight;
	int offset;
	int text_len = 0; // screen length of the editfield text that will be printed
	int field_offset; // character offset in the editfield string
	int screen_offset; // offset on screen for precise placement
	menuDef_t *parent = (menuDef_t*)item->parent;
	editFieldDef_t *editPtr = (editFieldDef_t*)item->typeData;

	Item_Text_Paint(item);

	buff[0] = '\0';

	if (item->cvar) {
		DC->getCVarString(item->cvar, buff, sizeof(buff));
	} 

	parent = (menuDef_t*)item->parent;

	if (item->window.flags & WINDOW_HASFOCUS && item->window.flags & WINDOW_FOCUSPULSE) {
		lowLight[0] = 0.8 * parent->focusColor[0]; 
		lowLight[1] = 0.8 * parent->focusColor[1]; 
		lowLight[2] = 0.8 * parent->focusColor[2]; 
		lowLight[3] = 0.8 * parent->focusColor[3]; 
		LerpColor(parent->focusColor,lowLight,newColor,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
	} else {
		memcpy(&newColor, &item->window.foreColor, sizeof(vec4_t));
	}

	// NOTE: offset from the editfield prefix (like "Say: " in limbo menu)
	offset = (item->text && *item->text) ? 8 : 0;
	
	// TTimo
	// text length control
	// if the edit field goes beyond the available width, drop some characters at the beginning of the string and apply some offseting
	// FIXME: we could cache the text length and offseting, but given the low count of edit fields, I abstained for now
	// FIXME: this won't handle going back into the line of the editfield to the hidden area
	// start of text painting: item->textRect.x + item->textRect.w + offset	
	// our window limit: item->window.rect.x + item->window.rect.w
	field_offset = -1;
	do
	{
		field_offset++;
		if( buff[editPtr->paintOffset + field_offset] == '\0' ) {
			break; // keep it safe
		}
		text_len = DC->textWidth( buff + editPtr->paintOffset + field_offset, item->textscale, 0 );
	} while (text_len + item->textRect.x + item->textRect.w + offset > item->window.rect.x + item->window.rect.w);

	if( field_offset ) {
		// we had to take out some chars to make it fit in, there is an additional screen offset to compute
		screen_offset = item->window.rect.x + item->window.rect.w - (text_len + item->textRect.x + item->textRect.w + offset);
	} else {
		screen_offset = 0;
	}
	
	if (item->window.flags & WINDOW_HASFOCUS && g_editingField) {
		char cursor = DC->getOverstrikeMode() ? '_' : '|';
		DC->drawTextWithCursor(item->textRect.x + item->textRect.w + offset + screen_offset, item->textRect.y, item->textscale, newColor, buff + editPtr->paintOffset + field_offset, item->cursorPos - editPtr->paintOffset - field_offset, cursor, editPtr->maxPaintChars, item->textStyle);
	} else {
		DC->drawText(item->textRect.x + item->textRect.w + offset + screen_offset, item->textRect.y, item->textscale, newColor, buff + editPtr->paintOffset + field_offset, 0, editPtr->maxPaintChars, item->textStyle);
	}
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const double cb794Dim=.8,cb794One=1.,cb794Half=.5;
static const float cb794Two=2.f,cb794Zero=0.f,cb794Eight=8.f,cb794Twelve=12.f,cb794Four=4.f;
enum { cb794Cvar=offsetof(itemDef_t,cvar),
cb794Parent=offsetof(itemDef_t,parent),
cb794Data=offsetof(itemDef_t,typeData),
cb794Text=offsetof(itemDef_t,text),
cb794Type=offsetof(itemDef_t,type),
cb794Style=offsetof(itemDef_t,textStyle),
cb794Scale=offsetof(itemDef_t,textscale),
cb794AlignY=offsetof(itemDef_t,textaligny),
cb794X=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,x),
cb794Y=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,y),
cb794W=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,w),
cb794RectX=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,x),
cb794RectY=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,y),
cb794H=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,h),
cb794Flags=offsetof(itemDef_t,window)+offsetof(windowDef_t,flags),
cb794Color=offsetof(itemDef_t,window)+offsetof(windowDef_t,foreColor),
cb794Focus=offsetof(menuDef_t,focusColor),
cb794Count=offsetof(multiDef_t,count),
cb794Time=offsetof(displayContextDef_t,realTime),
cb794Value=offsetof(displayContextDef_t,getCVarValue),
cb794Pic=offsetof(displayContextDef_t,drawHandlePic),
cb794Draw=offsetof(displayContextDef_t,drawText),
cb794Check=offsetof(displayContextDef_t,Assets)+offsetof(cachedAssets_t,checkboxCheck),
cb794CheckNo=offsetof(displayContextDef_t,Assets)+offsetof(cachedAssets_t,checkboxCheckNo),
cb794CheckNot=offsetof(displayContextDef_t,Assets)+offsetof(cachedAssets_t,checkboxCheckNot) };
__declspec(naked) void Item_CheckBox_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x34
 PUSH EBX
 PUSH EBP
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x44]
 XOR EBP,EBP
 PUSH EDI
 MOV EAX,dword ptr [ESI + cb794Cvar]
 MOV EDI,dword ptr [ESI + cb794Parent]
 MOV EBX,dword ptr [ESI + cb794Data]
 TEST EAX,EAX
 JZ cb794_30083a05
 PUSH EAX
 MOV EAX,DC
 CALL dword ptr [EAX + cb794Value]
 FSTP dword ptr [ESP + 0x4c]
 ADD ESP,0x4
 JMP cb794_30083a0d
cb794_30083a05:
 MOV dword ptr [ESP + 0x48],0x0
cb794_30083a0d:
 MOV EAX,dword ptr [ESI + cb794Flags]
 TEST AL,0x2
 JZ cb794_30083ab5
 TEST EAX,0x8000000
 JZ cb794_30083ab5
 FLD dword ptr [EDI + cb794Focus + 0]
 FMUL qword ptr cb794Dim
 LEA ECX,[EDI + cb794Focus + 0]
 MOV EDX,dword ptr DC
 MOV EAX,0x1b4e81b5
 PUSH ECX
 FSTP dword ptr [ESP + 0x18]
 FLD dword ptr [EDI + cb794Focus + 4]
 FMUL qword ptr cb794Dim
 FSTP dword ptr [ESP + 0x1c]
 FLD dword ptr [EDI + cb794Focus + 8]
 FMUL qword ptr cb794Dim
 FSTP dword ptr [ESP + 0x20]
 FLD dword ptr [EDI + cb794Focus + 12]
 FMUL qword ptr cb794Dim
 FSTP dword ptr [ESP + 0x24]
 MOV EDX,dword ptr [EDX + cb794Time]
 IMUL EDX
 SAR EDX,0x3
 MOV EAX,EDX
 SHR EAX,0x1f
 ADD EDX,EAX
 LEA EAX,[ESP + 0x18]
 MOV dword ptr [ESP + 0x14],EDX
 LEA EDX,[ESP + 0x28]
 FILD dword ptr [ESP + 0x14]
 FSIN
 FADD qword ptr cb794One
 FMUL qword ptr cb794Half
 FSTP dword ptr [ESP]
 PUSH EDX
 PUSH EAX
 PUSH ECX
 CALL LerpColor
 ADD ESP,0x10
 JMP cb794_30083ad4
cb794_30083ab5:
 LEA ECX,[ESI + cb794Color]
 MOV EDX,dword ptr [ESI + cb794Color]
 MOV dword ptr [ESP + 0x24],EDX
 MOV EAX,dword ptr [ECX + 0x4]
 MOV dword ptr [ESP + 0x28],EAX
 MOV EDX,dword ptr [ECX + 0x8]
 MOV dword ptr [ESP + 0x2c],EDX
 MOV EAX,dword ptr [ECX + 0xc]
 MOV dword ptr [ESP + 0x30],EAX
cb794_30083ad4:
 TEST EBX,EBX
 JZ cb794_30083ae7
 MOV EAX,dword ptr [EBX + cb794Count]
 TEST EAX,EAX
 JZ cb794_30083ae7
 MOV EBP,0x1
cb794_30083ae7:
 MOV EAX,dword ptr [ESI + cb794Text]
 TEST EAX,EAX
 JZ cb794_30083c00
 PUSH ESI
 CALL Item_Text_Paint
 MOV EAX,dword ptr [ESI + cb794Type]
 ADD ESP,0x4
 CMP EAX,0x10
 JNZ cb794_30083b35
 FLD dword ptr [ESP + 0x48]
 FCOMP dword ptr cb794Two
 FNSTSW AX
 TEST AH,0x40
 JZ cb794_30083b35
 MOV EAX,DC
 MOV ECX,dword ptr [ESI + cb794H]
 MOV dword ptr [ESP + 0x48],ECX
 MOV EDX,dword ptr [EAX + cb794CheckNo]
 PUSH EDX
 PUSH ECX
 PUSH ECX
 MOV ECX,dword ptr [ESI + cb794RectY]
 PUSH ECX
 JMP cb794_30083b77
cb794_30083b35:
 FLD dword ptr [ESP + 0x48]
 FCOMP dword ptr cb794Zero
 FNSTSW AX
 TEST AH,0x40
 JNZ cb794_30083b5a
 MOV EAX,DC
 MOV EDX,dword ptr [ESI + cb794H]
 MOV dword ptr [ESP + 0x48],EDX
 MOV ECX,dword ptr [EAX + cb794Check]
 JMP cb794_30083b6c
cb794_30083b5a:
 MOV EAX,dword ptr [ESI + cb794H]
 MOV dword ptr [ESP + 0x48],EAX
 MOV EAX,DC
 MOV ECX,dword ptr [EAX + cb794CheckNot]
cb794_30083b6c:
 MOV EDX,dword ptr [ESI + cb794RectY]
 PUSH ECX
 MOV ECX,dword ptr [ESP + 0x4c]
 PUSH ECX
 PUSH ECX
 PUSH EDX
cb794_30083b77:
 FLD dword ptr [ESI + cb794W]
 FADD dword ptr [ESI + cb794X]
 PUSH ECX
 FADD dword ptr cb794Eight
 FSTP dword ptr [ESP]
 CALL dword ptr [EAX + cb794Pic]
 ADD ESP,0x14
 TEST EBP,EBP
 JZ cb794_30083cc9
 LEA EAX,[ESP + 0x34]
 PUSH EAX
 PUSH ESI
 CALL Item_TextColor
 MOV EDX,dword ptr [ESI + cb794Style]
 MOV ECX,dword ptr DC
 ADD ESP,0x8
 LEA EDI,[ECX + cb794Draw]
 PUSH EDX
 PUSH 0x0
 PUSH 0x0
 PUSH ESI
 CALL Item_Multi_Setting
 FLD dword ptr [ESI + cb794W]
 FADD dword ptr [ESI + cb794H]
 MOV ECX,dword ptr [ESI + cb794Scale]
 MOV EDX,dword ptr [ESI + cb794Y]
 ADD ESP,0x4
 FADD dword ptr [ESI + cb794X]
 PUSH EAX
 LEA EAX,[ESP + 0x44]
 PUSH EAX
 PUSH ECX
 FADD dword ptr cb794Twelve
 PUSH EDX
 PUSH ECX
 FSTP dword ptr [ESP]
 CALL dword ptr [EDI]
 ADD ESP,0x20
 POP EDI
 POP ESI
 POP EBP
 POP EBX
 ADD ESP,0x34
 RET
cb794_30083c00:
 CMP dword ptr [ESI + cb794Type],0x10
 JNZ cb794_30083c2e
 FLD dword ptr [ESP + 0x48]
 FCOMP dword ptr cb794Two
 FNSTSW AX
 TEST AH,0x40
 JZ cb794_30083c2e
 MOV EAX,dword ptr [ESI + cb794H]
 MOV dword ptr [ESP + 0x48],EAX
 MOV EAX,DC
 MOV ECX,dword ptr [EAX + cb794CheckNo]
 JMP cb794_30083c59
cb794_30083c2e:
 FLD dword ptr [ESP + 0x48]
 FCOMP dword ptr cb794Zero
 MOV EDX,dword ptr [ESI + cb794H]
 MOV dword ptr [ESP + 0x48],EDX
 FNSTSW AX
 TEST AH,0x40
 MOV EAX,DC
 JNZ cb794_30083c53
 MOV ECX,dword ptr [EAX + cb794Check]
 JMP cb794_30083c59
cb794_30083c53:
 MOV ECX,dword ptr [EAX + cb794CheckNot]
cb794_30083c59:
 MOV EDX,dword ptr [ESI + cb794RectY]
 PUSH ECX
 MOV ECX,dword ptr [ESP + 0x4c]
 PUSH ECX
 PUSH ECX
 MOV ECX,dword ptr [ESI + cb794RectX]
 PUSH EDX
 PUSH ECX
 CALL dword ptr [EAX + cb794Pic]
 ADD ESP,0x14
 TEST EBP,EBP
 JZ cb794_30083cc9
 LEA EDX,[ESP + 0x34]
 PUSH EDX
 PUSH ESI
 CALL Item_TextColor
 MOV ECX,dword ptr [ESI + cb794Style]
 MOV EAX,DC
 ADD ESP,0x8
 LEA EDI,[EAX + cb794Draw]
 PUSH ECX
 PUSH 0x0
 PUSH 0x0
 PUSH ESI
 CALL Item_Multi_Setting
 FLD dword ptr [ESI + cb794AlignY]
 FADD dword ptr [ESI + cb794RectY]
 ADD ESP,0x4
 LEA EDX,[ESP + 0x40]
 PUSH EAX
 MOV EAX,dword ptr [ESI + cb794Scale]
 PUSH EDX
 PUSH EAX
 PUSH ECX
 FSTP dword ptr [ESP]
 FLD dword ptr [ESI + cb794H]
 FADD dword ptr [ESI + cb794RectX]
 PUSH ECX
 FADD dword ptr cb794Four
 FSTP dword ptr [ESP]
 CALL dword ptr [EDI]
 ADD ESP,0x20
cb794_30083cc9:
 POP EDI
 POP ESI
 POP EBP
 POP EBX
 ADD ESP,0x34
 RET
 }
}
#else
void Item_CheckBox_Paint( itemDef_t *item ) {
	vec4_t newColor, lowLight;
	float value;
	menuDef_t *parent = (menuDef_t*)item->parent;
	qboolean hasMultiText = qfalse;
	multiDef_t *multiPtr = (multiDef_t*)item->typeData;

	value = (item->cvar) ? DC->getCVarValue(item->cvar) : 0;

	if (item->window.flags & WINDOW_HASFOCUS && item->window.flags & WINDOW_FOCUSPULSE) {
		lowLight[0] = 0.8 * parent->focusColor[0]; 
		lowLight[1] = 0.8 * parent->focusColor[1]; 
		lowLight[2] = 0.8 * parent->focusColor[2]; 
		lowLight[3] = 0.8 * parent->focusColor[3]; 
		LerpColor(parent->focusColor,lowLight,newColor,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
	} else {
		memcpy(&newColor, &item->window.foreColor, sizeof(vec4_t));
	}

	if( multiPtr && multiPtr->count ) {
		hasMultiText = qtrue;
	}

	if (item->text) {
		Item_Text_Paint( item );
		if( item->type == ITEM_TYPE_TRICHECKBOX && value == 2 )
			DC->drawHandlePic( item->textRect.x + item->textRect.w + 8, item->window.rect.y, item->window.rect.h, item->window.rect.h, DC->Assets.checkboxCheckNo );
		else if( value )
			DC->drawHandlePic( item->textRect.x + item->textRect.w + 8, item->window.rect.y, item->window.rect.h, item->window.rect.h, DC->Assets.checkboxCheck );
		else
			DC->drawHandlePic( item->textRect.x + item->textRect.w + 8, item->window.rect.y, item->window.rect.h, item->window.rect.h, DC->Assets.checkboxCheckNot );

		if( hasMultiText ) {
			vec4_t colour;

			Item_TextColor( item, &colour );
			DC->drawText( item->textRect.x + item->textRect.w + 8 + item->window.rect.h + 4, item->textRect.y, item->textscale,
						  colour, Item_Multi_Setting( item ), 0, 0, item->textStyle );
		}
	} else {
		if( item->type == ITEM_TYPE_TRICHECKBOX && value == 2 )
			DC->drawHandlePic( item->window.rect.x, item->window.rect.y, item->window.rect.h, item->window.rect.h, DC->Assets.checkboxCheckNo );
		else if( value )
			DC->drawHandlePic( item->window.rect.x, item->window.rect.y, item->window.rect.h, item->window.rect.h, DC->Assets.checkboxCheck );
		else
			DC->drawHandlePic( item->window.rect.x, item->window.rect.y, item->window.rect.h, item->window.rect.h, DC->Assets.checkboxCheckNot );

		if( hasMultiText ) {
			vec4_t colour;

			Item_TextColor( item, &colour );
			DC->drawText( item->window.rect.x + item->window.rect.h + 4, item->window.rect.y + item->textaligny, item->textscale,
						  colour, Item_Multi_Setting( item ), 0, 0, item->textStyle );
		}
	}
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
/* TC cgame options: original x87 pulse, Cvar store and coordinate order. */
static const double op792Dim=0.8,op792One=1.0,op792Half=0.5;
static const float op792Zero=0.0f,op792Eight=8.0f;
static const char op792Yes[]="Yes",op792No[]="No";
enum { op792Cvar=offsetof(itemDef_t,cvar),
 op792Parent=offsetof(itemDef_t,parent),
 op792Flags=offsetof(itemDef_t,window)+offsetof(windowDef_t,flags),
 op792Color=offsetof(itemDef_t,window)+offsetof(windowDef_t,foreColor),
 op792Text=offsetof(itemDef_t,text),
 op792Style=offsetof(itemDef_t,textStyle),
 op792Scale=offsetof(itemDef_t,textscale),
 op792Focus=offsetof(menuDef_t,focusColor),
 op792X=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,x),
 op792Y=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,y),
 op792W=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,w),
 op792Time=offsetof(displayContextDef_t,realTime),
 op792Value=offsetof(displayContextDef_t,getCVarValue),
 op792Translate=offsetof(displayContextDef_t,translateString),
 op792Draw=offsetof(displayContextDef_t,drawText) };
__declspec(naked) void Item_YesNo_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x24
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x2c]
 PUSH EDI
 MOV EAX,dword ptr [ESI + op792Cvar]
 MOV EDI,dword ptr [ESI + op792Parent]
 TEST EAX,EAX
 JZ op792_30083d0b
 PUSH EAX
 MOV EAX,DC
 CALL dword ptr [EAX + op792Value]
 FSTP dword ptr [ESP + 0x34]
 ADD ESP,0x4
 JMP op792_30083d13
op792_30083d0b:
 MOV dword ptr [ESP + 0x30],0x0
op792_30083d13:
 MOV EAX,dword ptr [ESI + op792Flags]
 TEST AL,0x2
 JZ op792_30083dbb
 TEST EAX,0x8000000
 JZ op792_30083dbb
 FLD dword ptr [EDI + op792Focus + 0]
 FMUL qword ptr op792Dim
 LEA ECX,[EDI + op792Focus + 0]
 MOV EDX,dword ptr DC
 MOV EAX,0x1b4e81b5
 PUSH ECX
 FSTP dword ptr [ESP + 0x20]
 FLD dword ptr [EDI + op792Focus + 4]
 FMUL qword ptr op792Dim
 FSTP dword ptr [ESP + 0x24]
 FLD dword ptr [EDI + op792Focus + 8]
 FMUL qword ptr op792Dim
 FSTP dword ptr [ESP + 0x28]
 FLD dword ptr [EDI + op792Focus + 12]
 FMUL qword ptr op792Dim
 FSTP dword ptr [ESP + 0x2c]
 MOV EDX,dword ptr [EDX + op792Time]
 IMUL EDX
 SAR EDX,0x3
 MOV EAX,EDX
 SHR EAX,0x1f
 ADD EDX,EAX
 LEA EAX,[ESP + 0x20]
 MOV dword ptr [ESP + 0xc],EDX
 LEA EDX,[ESP + 0x10]
 FILD dword ptr [ESP + 0xc]
 FSIN
 FADD qword ptr op792One
 FMUL qword ptr op792Half
 FSTP dword ptr [ESP]
 PUSH EDX
 PUSH EAX
 PUSH ECX
 CALL LerpColor
 ADD ESP,0x10
 JMP op792_30083dda
op792_30083dbb:
 LEA ECX,[ESI + op792Color]
 MOV EDX,dword ptr [ESI + op792Color]
 MOV dword ptr [ESP + 0xc],EDX
 MOV EAX,dword ptr [ECX + 0x4]
 MOV dword ptr [ESP + 0x10],EAX
 MOV EDX,dword ptr [ECX + 0x8]
 MOV dword ptr [ESP + 0x14],EDX
 MOV EAX,dword ptr [ECX + 0xc]
 MOV dword ptr [ESP + 0x18],EAX
op792_30083dda:
 MOV EAX,dword ptr [ESI + op792Text]
 TEST EAX,EAX
 JZ op792_30083e70
 PUSH ESI
 CALL Item_Text_Paint
 FLD dword ptr [ESP + 0x34]
 FCOMP dword ptr op792Zero
 ADD ESP,0x4
 FNSTSW AX
 TEST AH,0x40
 JNZ op792_30083e15
 MOV ECX,dword ptr DC
 PUSH offset op792Yes
 CALL dword ptr [ECX + op792Translate]
 JMP op792_30083e26
op792_30083e15:
 MOV EDX,dword ptr DC
 PUSH offset op792No
 CALL dword ptr [EDX + op792Translate]
op792_30083e26:
 MOV ECX,dword ptr [ESI + op792Style]
 ADD ESP,0x4
 FLD dword ptr [ESI + op792W]
 FADD dword ptr [ESI + op792X]
 PUSH ECX
 MOV ECX,dword ptr [ESI + op792Y]
 PUSH 0x0
 PUSH 0x0
 FADD dword ptr op792Eight
 PUSH EAX
 MOV EAX,dword ptr [ESI + op792Scale]
 LEA EDX,[ESP + 0x1c]
 PUSH EDX
 MOV EDX,dword ptr DC
 PUSH EAX
 PUSH ECX
 PUSH ECX
 FSTP dword ptr [ESP]
 CALL dword ptr [EDX + op792Draw]
 ADD ESP,0x20
 POP EDI
 POP ESI
 ADD ESP,0x24
 RET
op792_30083e70:
 FLD dword ptr [ESP + 0x30]
 FCOMP dword ptr op792Zero
 FNSTSW AX
 TEST AH,0x40
 MOV EAX,offset op792Yes
 JZ op792_30083e8b
 MOV EAX,offset op792No
op792_30083e8b:
 MOV ECX,dword ptr [ESI + op792Style]
 LEA EDX,[ESP + 0xc]
 PUSH ECX
 MOV ECX,dword ptr [ESI + op792Y]
 PUSH 0x0
 PUSH 0x0
 PUSH EAX
 MOV EAX,dword ptr [ESI + op792Scale]
 PUSH EDX
 MOV EDX,dword ptr [ESI + op792X]
 PUSH EAX
 MOV EAX,DC
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + op792Draw]
 ADD ESP,0x20
 POP EDI
 POP ESI
 ADD ESP,0x24
 RET
 }
}
#else
void Item_YesNo_Paint(itemDef_t *item) {
	vec4_t newColor, lowLight;
	float value;
	menuDef_t *parent = (menuDef_t*)item->parent;

	value = (item->cvar) ? DC->getCVarValue(item->cvar) : 0;

	if (item->window.flags & WINDOW_HASFOCUS && item->window.flags & WINDOW_FOCUSPULSE) {
		lowLight[0] = 0.8 * parent->focusColor[0]; 
		lowLight[1] = 0.8 * parent->focusColor[1]; 
		lowLight[2] = 0.8 * parent->focusColor[2]; 
		lowLight[3] = 0.8 * parent->focusColor[3]; 
		LerpColor(parent->focusColor,lowLight,newColor,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
	} else {
		memcpy(&newColor, &item->window.foreColor, sizeof(vec4_t));
	}

	if (item->text) {
		Item_Text_Paint(item);
		DC->drawText(item->textRect.x + item->textRect.w + 8, item->textRect.y, item->textscale, newColor, 
			(value != 0) ? DC->translateString( "Yes" ) : DC->translateString( "No" ), 0, 0, item->textStyle);
	} else {
		DC->drawText(item->textRect.x, item->textRect.y, item->textscale, newColor, (value != 0) ? "Yes" : "No", 0, 0, item->textStyle);
	}
}

#endif
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
__declspec(naked) void Item_Multi_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x20
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x28]
 PUSH EDI
 MOV ECX,dword ptr [ESI + op792Flags]
 MOV EAX,dword ptr [ESI + op792Parent]
 TEST CL,0x2
 JZ op792_30083f88
 TEST ECX,0x8000000
 JZ op792_30083f88
 FLD dword ptr [EAX + op792Focus + 0]
 FMUL qword ptr op792Dim
 LEA ECX,[EAX + op792Focus + 0]
 PUSH ECX
 FSTP dword ptr [ESP + 0x1c]
 FLD dword ptr [EAX + op792Focus + 4]
 FMUL qword ptr op792Dim
 FSTP dword ptr [ESP + 0x20]
 FLD dword ptr [EAX + op792Focus + 8]
 FMUL qword ptr op792Dim
 FSTP dword ptr [ESP + 0x24]
 FLD dword ptr [EAX + op792Focus + 12]
 FMUL qword ptr op792Dim
 MOV EAX,DC
 FSTP dword ptr [ESP + 0x28]
 MOV EDX,dword ptr [EAX + op792Time]
 MOV EAX,0x1b4e81b5
 IMUL EDX
 SAR EDX,0x3
 MOV EAX,EDX
 SHR EAX,0x1f
 ADD EDX,EAX
 LEA EAX,[ESP + 0x1c]
 MOV dword ptr [ESP + 0x30],EDX
 LEA EDX,[ESP + 0xc]
 FILD dword ptr [ESP + 0x30]
 FSIN
 FADD qword ptr op792One
 FMUL qword ptr op792Half
 FSTP dword ptr [ESP]
 PUSH EDX
 PUSH EAX
 PUSH ECX
 CALL LerpColor
 ADD ESP,0x10
 JMP op792_30083fa7
op792_30083f88:
 LEA ECX,[ESI + op792Color]
 MOV EDX,dword ptr [ESI + op792Color]
 MOV dword ptr [ESP + 0x8],EDX
 MOV EAX,dword ptr [ECX + 0x4]
 MOV dword ptr [ESP + 0xc],EAX
 MOV EDX,dword ptr [ECX + 0x8]
 MOV dword ptr [ESP + 0x10],EDX
 MOV EAX,dword ptr [ECX + 0xc]
 MOV dword ptr [ESP + 0x14],EAX
op792_30083fa7:
 PUSH ESI
 CALL Item_Multi_Setting
 MOV EDI,EAX
 MOV EAX,dword ptr [ESI + op792Text]
 ADD ESP,0x4
 TEST EAX,EAX
 JZ op792_3008400c
 PUSH ESI
 CALL Item_Text_Paint
 MOV ECX,dword ptr [ESI + op792Style]
 MOV EAX,dword ptr [ESI + op792Scale]
 FLD dword ptr [ESI + op792W]
 FADD dword ptr [ESI + op792X]
 ADD ESP,0x4
 LEA EDX,[ESP + 0x8]
 FADD dword ptr op792Eight
 PUSH ECX
 MOV ECX,dword ptr [ESI + op792Y]
 PUSH 0x0
 PUSH 0x0
 PUSH EDI
 PUSH EDX
 MOV EDX,dword ptr DC
 PUSH EAX
 PUSH ECX
 PUSH ECX
 FSTP dword ptr [ESP]
 CALL dword ptr [EDX + op792Draw]
 ADD ESP,0x20
 POP EDI
 POP ESI
 ADD ESP,0x20
 RET
op792_3008400c:
 MOV EAX,dword ptr [ESI + op792Style]
 MOV EDX,dword ptr [ESI + op792Scale]
 PUSH EAX
 MOV EAX,dword ptr [ESI + op792Y]
 PUSH 0x0
 PUSH 0x0
 LEA ECX,[ESP + 0x14]
 PUSH EDI
 PUSH ECX
 MOV ECX,dword ptr [ESI + op792X]
 PUSH EDX
 MOV EDX,dword ptr DC
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + op792Draw]
 ADD ESP,0x20
 POP EDI
 POP ESI
 ADD ESP,0x20
 RET
 }
}
#else
void Item_Multi_Paint(itemDef_t *item) {
	vec4_t newColor, lowLight;
	const char *text = "";
	menuDef_t *parent = (menuDef_t*)item->parent;

	if (item->window.flags & WINDOW_HASFOCUS && item->window.flags & WINDOW_FOCUSPULSE) {
		lowLight[0] = 0.8 * parent->focusColor[0]; 
		lowLight[1] = 0.8 * parent->focusColor[1]; 
		lowLight[2] = 0.8 * parent->focusColor[2]; 
		lowLight[3] = 0.8 * parent->focusColor[3]; 
		LerpColor(parent->focusColor,lowLight,newColor,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
	} else {
		memcpy(&newColor, &item->window.foreColor, sizeof(vec4_t));
	}

	text = Item_Multi_Setting(item);

	if (item->text) {
		Item_Text_Paint(item);
		DC->drawText(item->textRect.x + item->textRect.w + 8, item->textRect.y, item->textscale, newColor, text, 0, 0, item->textStyle);
	} else {
		DC->drawText(item->textRect.x, item->textRect.y, item->textscale, newColor, text, 0, 0, item->textStyle);
	}
}


#endif

typedef struct {
	char	*command;
	/* TC retains the SDK id slot. Seven-value initializers below intentionally
	 * leave bind2 zero; do not shift defaults left or remove this ABI member.
	 * Original defaults/apply behavior is covered by tce_controls_defaults_test. */
	int		id;
	int		defaultbind1_right;
	int		defaultbind2_right;
	int		defaultbind1_left;
	int		defaultbind2_left;
	int		bind1;
	int		bind2;
} bind_t;

typedef struct
{
	char*	name;
	float	defaultvalue;
	float	value;	
} configcvar_t;

// Gordon: These MUST be all lowercase now
static bind_t g_bindings[] = {

	{ "+forward",	'w',			-1,	K_UPARROW,		-1,	-1,	-1 },
	{ "+back",		's',			-1,	K_DOWNARROW,	-1,	-1,	-1 },
	{ "+moveleft",	'a',			-1,	K_LEFTARROW,	-1,	-1,	-1 },
	{ "+moveright",	'd',			-1,	K_RIGHTARROW,	-1,	-1,	-1 },
	{ "+moveup",	K_SPACE,		-1,	K_KP_INS,		-1,	-1,	-1 },
	{ "+movedown",	'c',			-1,	K_CTRL,			-1,	-1,	-1 },
	{ "+leanright",	'e',			-1,	K_PGDN,			-1,	-1,	-1 },
	{ "+leanleft",	'q',			-1,	K_DEL,			-1,	-1,	-1 },
	{ "+prone",		'x',			-1,	K_SHIFT,		-1,	-1,	-1 },
	{ "+attack",	K_MOUSE1,		-1,	K_MOUSE1,		-1,	-1,	-1 },
	{ "weapalt",	K_MOUSE2,		-1,	K_MOUSE2,		-1,	-1,	-1 },
	{ "weapprev",	K_MWHEELDOWN,	-1,	K_MWHEELDOWN,	-1,	-1,	-1 },
	{ "weapnext",	K_MWHEELUP,		-1,	K_MWHEELUP,		-1,	-1,	-1 },
	{ "weaponbank 10",	'0',		-1,	'0',			-1,	-1,	-1 },
	{ "weaponbank 1",	'1',		-1,	'1',			-1,	-1,	-1 },
	{ "weaponbank 2",	'2',		-1,	'2',			-1,	-1,	-1 },
	{ "weaponbank 3",	'3',		-1,	'3',			-1,	-1,	-1 },
	{ "weaponbank 4",	'4',		-1,	'4',			-1,	-1,	-1 },
	{ "weaponbank 5",	'5',		-1,	'5',			-1,	-1,	-1 },
	{ "weaponbank 6",	'6',		-1,	'6',			-1,	-1,	-1 },
	{ "weaponbank 7",	'7',		-1,	'7',			-1,	-1,	-1 },
	{ "weaponbank 8",	'8',		-1,	'8',			-1,	-1,	-1 },
	{ "weaponbank 9",	'9',		-1,	'9',			-1,	-1,	-1 },
	{ "+sprint",		K_SHIFT,	-1,	K_MOUSE3,		-1,	-1,	-1 },
	{ "+speed",			K_CAPSLOCK,	-1,	K_CAPSLOCK,		-1,	-1,	-1 },
	{ "+activate",		'f',		-1,	K_ENTER,		-1,	-1,	-1 },
	{ "+zoom",			'b',		-1,	'b',			-1,	-1,	-1 },
	{ "+reload",		'r',		-1,	K_END,			-1,	-1,	-1 },
	{ "+scores",		K_TAB,		-1,	K_TAB,			-1,	-1,	-1 },
	{ "+stats",			K_ALT,		-1,	K_F9,			-1,	-1,	-1 },
	{ "+topshots",		K_CTRL,		-1,	K_F10,			-1,	-1,	-1 },
	{ "toggleconsole",	'`',		'~',	'`',		'~',	-1,	-1 },
	{ "togglemenu",		K_ESCAPE,	-1,	K_ESCAPE,		-1,	-1,	-1 },
	{ "openlimbomenu",	'l',		-1,	'l',			-1,	-1,	-1 },
	{ "mvactivate",		'm',		-1,	'm',			-1,	-1,	-1 },
	{ "mapzoomout",		',',		-1,	'[',			-1,	-1,	-1 },
	{ "mapzoomin",		'.',		-1,	']',			-1,	-1,	-1 },
	{ "zoomin",			'=',		-1,	'-',			-1,	-1,	-1 },
	{ "zoomout",		'-',		-1,	'=',			-1,	-1,	-1 },
	{ "messagemode",	't',		-1,	't',			-1,	-1,	-1 },
	{ "messagemode2",	'y',		-1,	'y',			-1,	-1,	-1 },
	{ "messagemode3",	'u',		-1,	'u',			-1,	-1,	-1 },
	{ "mp_quickmessage",'v',		-1,	'v',			-1,	-1,	-1 },
	{ "mp_fireteammsg",	'z',		-1,	'c',			-1,	-1,	-1 },
	{ "vote yes",		K_F1,		-1,	K_F1,			-1,	-1,	-1 },
	{ "vote no",		K_F2,		-1,	K_F2,			-1,	-1,	-1 },
	{ "ready",			K_F3,		-1,	K_F3,			-1,	-1,	-1 },
	{ "notready",		K_F4,		-1,	K_F4,			-1,	-1,	-1 },
	{ "autoscreenshot",	K_F11,		-1,	K_F11,			-1,	-1,	-1 },
	{ "autoRecord",		K_F12,		-1,	K_F12,			-1,	-1,	-1 },
	{ "mp_fireteamadmin",	K_KP_ENTER,	-1,	K_KP_ENTER,		-1,	-1,	-1 },
	{ "selectbuddy -1",	K_KP_DEL,		-1,	K_KP_PLUS,		-1,	-1,	-1 },
	{ "selectbuddy 0",	K_KP_END,		-1,	K_KP_END,		-1,	-1,	-1 },
	{ "selectbuddy 1",	K_KP_DOWNARROW,	-1,	K_KP_DOWNARROW,	-1,	-1,	-1 },
	{ "selectbuddy 2",	K_KP_PGDN,		-1,	K_KP_PGDN,		-1,	-1,	-1 },
	{ "selectbuddy 3",	K_KP_LEFTARROW,	-1,	K_KP_LEFTARROW,	-1,	-1,	-1 },
	{ "selectbuddy 4",	K_KP_5,			-1,	K_KP_5,			-1,	-1,	-1 },
	{ "selectbuddy 5",	K_KP_RIGHTARROW,-1,	K_KP_RIGHTARROW,-1,	-1,	-1 },
	{ "selectbuddy -2",	K_KP_INS,		-1,	K_KP_MINUS,		-1,	-1,	-1 },
	/* TC controls menu exposes these commands instead of ET map expansion.
	 * Windows binding order40037a08: objective59, firemode60. */
	{ "toggleobjectivedesc", K_F5, -1, K_F5, -1, -1, -1 },
	{ "+firemode", 'g', -1, '#', -1, -1, -1 },
	{ "+freelook", -1, -1, -1, -1, -1, -1 },

/*	{"+scores", 		-1,				-1, -1, -1},
	{"+speed",			K_SHIFT,		-1, -1, -1},
	{"+forward",		K_UPARROW,		-1, -1, -1},
	{"+back",			K_DOWNARROW,	-1, -1, -1},
	{"+moveleft",		',',			-1, -1, -1},
	{"+moveright",		'.',			-1, -1, -1},
	{"+moveup", 		K_SPACE,		-1, -1, -1},
	{"+movedown",		'c',			-1, -1, -1},
	{"+left",			K_LEFTARROW,	-1, -1, -1},
	{"+right",			K_RIGHTARROW,	-1, -1, -1},
	{"+strafe", 		K_ALT,			-1, -1, -1},
	{"+lookup", 		K_PGDN, 		-1, -1, -1},
	{"+lookdown",		K_DEL,			-1, -1, -1},
	{"+mlook",			'/',			-1, -1, -1},
//	{"centerview",		K_END,			-1, -1, -1},	// this is an exploit nowadays
	{"+zoom",			'z', 			-1, -1, -1},
	{"weaponbank 1",	'1',			-1, -1, -1},
	{"weaponbank 2",	'2',			-1, -1, -1},
	{"weaponbank 3",	'3',			-1, -1, -1},
	{"weaponbank 4",	'4',			-1, -1, -1},
	{"weaponbank 5",	'5',			-1, -1, -1},
	{"weaponbank 6",	'6',			-1, -1, -1},
	{"weaponbank 7",	'7',			-1, -1, -1},
	{"weaponbank 8",	'8',			-1, -1, -1},
	{"weaponbank 9",	'9',			-1, -1, -1},
	{"weaponbank 10",	'0',			-1, -1, -1},
	{"+attack", 		K_CTRL, 		-1, -1, -1},
	{"weapprev",		K_MWHEELDOWN,	-1, -1, -1},
	{"weapnext",		K_MWHEELUP,		-1, -1, -1},
	{"weapalt",			-1,				-1, -1, -1},
	{"weaplastused",	-1,				-1, -1, -1},//----(SA)	added
	{"weapnextinbank",	-1,				-1, -1, -1},//----(SA)	added
	{"weapprevinbank",	-1,				-1, -1, -1},//----(SA)	added
	{"+useitem",		K_ENTER,		-1, -1, -1},
	{"+button3",		K_MOUSE3,		-1, -1, -1},

	{"scoresUp",		-1,				-1, -1, -1},
	{"scoresDown",		-1,				-1, -1, -1},
	{"messagemode", 	-1,				-1, -1, -1},
	{"messagemode2",	-1, 			-1, -1, -1},
	{"messagemode3",	-1, 			-1, -1, -1},

	{"+activate",		-1,				-1, -1, -1},
	{"zoomin",			-1,				-1, -1, -1},
	{"zoomout",			-1,				-1, -1, -1},
	{"+kick",			-1,				-1, -1, -1},
	{"+reload", 		-1, 			-1, -1, -1},
	{"+sprint", 		-1, 			-1, -1, -1},
	{"notebook",		K_TAB, 			-1, -1, -1},
	{"help",			K_F1, 			-1, -1, -1},
	{"+leanleft", 		-1, 			-1, -1, -1},
	{"+leanright", 		-1, 			-1, -1, -1},

	{"vote yes",		-1,				-1,	-1,	-1},
	{"vote no",			-1,				-1,	-1,	-1},
	{"openlimbomenu", 	-1, 			-1, -1, -1},
	{"mp_quickmessage",	-1, 			-1, -1, -1},
	{"mp_fireteammsg",	-1, 			-1, -1, -1},
	{"mp_fireteamadmin",-1, 			-1, -1, -1},

	// OSP
	{"+stats",	 		-1, 			-1, -1, -1},
	{"+topshots",	 	-1, 			-1, -1, -1},
	{"+wstats", 		-1, 			-1, -1, -1},
	{"autoScreenshot", 	-1, 			-1, -1, -1},
	{"autoRecord", 		-1, 			-1, -1, -1},
	{"currenttime", 	-1, 			-1, -1, -1},
	{"statsdump",	 	-1, 			-1, -1, -1},
	{"mvallies",	 	-1, 			-1, -1, -1},
	{"mvaxis",		 	-1, 			-1, -1, -1},
	{"mvdel",		 	-1, 			-1, -1, -1},
	{"mvhide",		 	-1, 			-1, -1, -1},
	{"mvnone",		 	-1, 			-1, -1, -1},
	{"mvshow",		 	-1, 			-1, -1, -1},
	{"mvswap",		 	-1, 			-1, -1, -1},
	{"mvtoggle",	 	-1, 			-1, -1, -1},
	{"mvactivate",	 	-1, 			-1, -1, -1},	
	// -OSP

	{"weapon 1",		-1,				-1, -1, -1},
	{"weapon 2",		-1,				-1, -1, -1},
	{"weapon 3",		-1,				-1, -1, -1},
	{"weapon 4",		-1,				-1, -1, -1},
	{"weapon 5",		-1,				-1, -1, -1},
	{"weapon 6",		-1,				-1, -1, -1},
	{"weapon 7",		-1,				-1, -1, -1},
	{"weapon 8",		-1,				-1, -1, -1},
	{"weapon 9",		-1,				-1, -1, -1},
	{"weapon 10",		-1,				-1, -1, -1},
	{"weapon 11",		-1, 			-1, -1, -1},
	{"weapon 12",		-1, 			-1, -1, -1},
	{"weapon 13",		-1, 			-1, -1, -1},
	{"weapon 14",		-1, 			-1, -1, -1},
	{"weapon 15",		-1, 			-1, -1, -1},
	{"weapon 16",		-1, 			-1, -1, -1},
	{"weapon 17",		-1, 			-1, -1, -1},
	{"weapon 18",		-1, 			-1, -1, -1},
	{"weapon 19",		-1, 			-1, -1, -1},
	{"weapon 20",		-1, 			-1, -1, -1},
	{"weapon 21",		-1, 			-1, -1, -1},
	{"weapon 22",		-1, 			-1, -1, -1},
	{"weapon 23",		-1, 			-1, -1, -1},
	{"weapon 24",		-1, 			-1, -1, -1},
	{"weapon 25",		-1, 			-1, -1, -1},
	{"weapon 26",		-1, 			-1, -1, -1},
	{"weapon 27",		-1, 			-1, -1, -1},
	{"weapon 28",		-1, 			-1, -1, -1},
	{"weapon 29",		-1, 			-1, -1, -1},
	{"weapon 30",		-1, 			-1, -1, -1},
	{"weapon 31",		-1, 			-1, -1, -1},
	{"weapon 32",		-1, 			-1, -1, -1},

	{"commandmap",		-1, 			-1, -1, -1},
	{"buddy",			-1, 			-1, -1, -1},
	{"stats",			-1, 			-1, -1, -1},

	{"selectbuddy -1",	-1,				-1, -1, -1},
	{"selectbuddy -2",	-1,				-1, -1, -1},
	{"selectbuddy 0",	-1,				-1, -1, -1},
	{"selectbuddy 1",	-1,				-1, -1, -1},
	{"selectbuddy 2",	-1,				-1, -1, -1},
	{"selectbuddy 3",	-1,				-1, -1, -1},
	{"selectbuddy 4",	-1,				-1, -1, -1},
	{"selectbuddy 5",	-1,				-1, -1, -1},
	{"gotowaypoint",	-1,				-1, -1, -1},
	{"mapzoomin",		-1,				-1, -1, -1},
	{"mapzoomout",		-1,				-1, -1, -1},
	{"+mapexpand",		-1,				-1, -1, -1},

	{"+prone",			-1, 			-1, -1, -1},*/
};


static const int g_bindCount = sizeof(g_bindings) / sizeof(bind_t);

/*
=================
Controls_GetConfig
=================
*/
void Controls_GetConfig( void ) {
	int		i;

	// iterate each command, get its numeric binding
	for (i = 0; i < g_bindCount; i++) {
		DC->getKeysForBinding(g_bindings[i].command, &g_bindings[i].bind1, &g_bindings[i].bind2);
	}
}

/*
=================
Controls_SetConfig
=================
*/
void Controls_SetConfig( qboolean restart ) {
	int		i;

	// iterate each command, get its numeric binding
	for( i = 0; i < g_bindCount; i++) {
		if (g_bindings[i].bind1 != -1) {	
			DC->setBinding( g_bindings[i].bind1, g_bindings[i].command );

			if (g_bindings[i].bind2 != -1) {
				DC->setBinding( g_bindings[i].bind2, g_bindings[i].command );
			}
		}
	}

#if !defined(__MACOS__)
	if( restart ) {
		DC->executeText(EXEC_APPEND, "in_restart\n");
	}
#endif
	//trap_Cmd_ExecuteText( EXEC_APPEND, "in_restart\n" );
}

/*
=================
Controls_SetDefaults
=================
*/
void Controls_SetDefaults( qboolean lefthanded ) {
	int	i;

	// iterate each command, set its default binding
	for( i = 0; i < g_bindCount; i++) {
		g_bindings[i].bind1 = lefthanded ? g_bindings[i].defaultbind1_left : g_bindings[i].defaultbind1_right;
		g_bindings[i].bind2 = lefthanded ? g_bindings[i].defaultbind2_left : g_bindings[i].defaultbind2_right;
	}
}

int BindingIDFromName( const char *name ) {
	int i;
	
	for(i = 0; i < g_bindCount; i++) {
		if(!Q_stricmp(name, g_bindings[i].command)) {
			return i;
		}
	}

	return -1;
}

char g_nameBind1[32];
char g_nameBind2[32];

char* BindingFromName(const char *cvar) {
	int b1, b2;

	DC->getKeysForBinding( cvar, &b1, &b2 );

	if( b1 != -1 ) {
		DC->keynumToStringBuf( b1, g_nameBind1, 32 );
		Q_strupr( g_nameBind1 );

		if(b2 != -1) {
			DC->keynumToStringBuf( b2, g_nameBind2, 32 );
			Q_strupr(g_nameBind2);
			Q_strcat( g_nameBind1, 32, DC->translateString( " or " ) );
			Q_strcat( g_nameBind1, 32, g_nameBind2 );
		}
	} else {
		Q_strncpyz( g_nameBind1, "(?" "?" "?)", 32 );
	}
	return g_nameBind1;			// NERVE - SMF
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
/* TC Windows 30084150: preserve FSIN and the thumb callback's retained ST0. */
static const double sp785Dim = .8, sp785One = 1., sp785Half = .5, sp785Six = 6.;
static const float sp785Gap = 8.f, sp785OneF = 1.f;
enum {
 sp785RectX=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,x),
 sp785RectY=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,y),
 sp785Cvar=offsetof(itemDef_t,cvar), sp785Parent=offsetof(itemDef_t,parent),
 sp785Flags=offsetof(itemDef_t,window)+offsetof(windowDef_t,flags),
 sp785Fore=offsetof(itemDef_t,window)+offsetof(windowDef_t,foreColor),
 sp785Text=offsetof(itemDef_t,text), sp785TextW=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,w),
 sp785TextX=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,x),
 sp785Focus=offsetof(menuDef_t,focusColor), sp785GetValue=offsetof(displayContextDef_t,getCVarValue),
 sp785Time=offsetof(displayContextDef_t,realTime), sp785SetColor=offsetof(displayContextDef_t,setColor),
 sp785Draw=offsetof(displayContextDef_t,drawHandlePic),
 sp785Bar=offsetof(displayContextDef_t,Assets)+offsetof(cachedAssets_t,sliderBar),
 sp785Thumb=offsetof(displayContextDef_t,Assets)+offsetof(cachedAssets_t,sliderThumb)
};
__declspec(naked) void Item_Slider_Paint(itemDef_t *item) {
 __asm {
        SUB ESP,0x24
        PUSH ESI
        MOV ESI,dword ptr [ESP + 0x2c]
        PUSH EDI
        MOV EAX,dword ptr [ESI + sp785Cvar]
        MOV EDI,dword ptr [ESI + sp785Parent]
        TEST EAX,EAX
        JZ sp785_30084177
        PUSH EAX
        MOV EAX,DC
        CALL dword ptr [EAX + sp785GetValue]
        FSTP st(0)
        ADD ESP,0x4
sp785_30084177:
        MOV EAX,dword ptr [ESI + sp785Flags]
        TEST AL,0x2
        JZ sp785_3008421f
        TEST EAX,0x8000000
        JZ sp785_3008421f
        FLD dword ptr [EDI + sp785Focus + 0]
        FMUL qword ptr sp785Dim
        LEA ECX,[EDI + sp785Focus + 0]
        MOV EDX,dword ptr DC
        MOV EAX,0x1b4e81b5
        PUSH ECX
        FSTP dword ptr [ESP + 0x20]
        FLD dword ptr [EDI + sp785Focus + 4]
        FMUL qword ptr sp785Dim
        FSTP dword ptr [ESP + 0x24]
        FLD dword ptr [EDI + sp785Focus + 8]
        FMUL qword ptr sp785Dim
        FSTP dword ptr [ESP + 0x28]
        FLD dword ptr [EDI + sp785Focus + 12]
        FMUL qword ptr sp785Dim
        FSTP dword ptr [ESP + 0x2c]
        MOV EDX,dword ptr [EDX + sp785Time]
        IMUL EDX
        SAR EDX,0x3
        MOV EAX,EDX
        SHR EAX,0x1f
        ADD EDX,EAX
        LEA EAX,[ESP + 0x20]
        MOV dword ptr [ESP + 0x34],EDX
        LEA EDX,[ESP + 0x10]
        FILD dword ptr [ESP + 0x34]
        FSIN
        FADD qword ptr sp785One
        FMUL qword ptr sp785Half
        FSTP dword ptr [ESP]
        PUSH EDX
        PUSH EAX
        PUSH ECX
        CALL LerpColor
        ADD ESP,0x10
        JMP sp785_3008423e
sp785_3008421f:
        LEA ECX,[ESI + sp785Fore]
        MOV EDX,dword ptr [ESI + sp785Fore]
        MOV dword ptr [ESP + 0xc],EDX
        MOV EAX,dword ptr [ECX + 4]
        MOV dword ptr [ESP + 0x10],EAX
        MOV EDX,dword ptr [ECX + 0x8]
        MOV dword ptr [ESP + 0x14],EDX
        MOV EAX,dword ptr [ECX + 0xc]
        MOV dword ptr [ESP + 0x18],EAX
sp785_3008423e:
        MOV EAX,dword ptr [ESI + sp785Text]
        MOV ECX,dword ptr [ESI + sp785RectY]
        TEST EAX,EAX
        MOV dword ptr [ESP + 0x8],ECX
        JZ sp785_30084270
        PUSH ESI
        CALL Item_Text_Paint
        FLD dword ptr [ESI + sp785TextW]
        FADD dword ptr [ESI + sp785TextX]
        ADD ESP,0x4
        FADD dword ptr sp785Gap
        FSTP dword ptr [ESP + 0x30]
        JMP sp785_30084276
sp785_30084270:
        MOV EDX,dword ptr [ESI + sp785RectX]
        MOV dword ptr [ESP + 0x30],EDX
sp785_30084276:
        MOV ECX,dword ptr DC
        LEA EAX,[ESP + 0xc]
        PUSH EAX
        CALL dword ptr [ECX + sp785SetColor]
        MOV EAX,DC
        ADD ESP,0x4
        FLD dword ptr [ESP + 0x8]
        MOV EDX,dword ptr [EAX + sp785Bar]
        FADD dword ptr sp785OneF
        PUSH EDX
        PUSH 0x41200000
        PUSH 0x42c00000
        PUSH ECX
        MOV ECX,dword ptr [ESP + 0x40]
        FSTP dword ptr [ESP]
        PUSH ECX
        CALL dword ptr [EAX + sp785Draw]
        PUSH ESI
        CALL Item_Slider_ThumbPosition
        MOV EAX,DC
        MOV ECX,dword ptr [ESP + 0x20]
        FSUB qword ptr sp785Six
        MOV EDX,dword ptr [EAX + sp785Thumb]
        ADD ESP,0x18
        PUSH EDX
        PUSH 0x41400000
        PUSH 0x41400000
        PUSH ECX
        PUSH ECX
        FSTP dword ptr [ESP]
        CALL dword ptr [EAX + sp785Draw]
        ADD ESP,0x14
        POP EDI
        POP ESI
        ADD ESP,0x24
        RET
 }
}
#else
void Item_Slider_Paint(itemDef_t *item) {
	vec4_t newColor, lowLight;
	float x, y, value;
	menuDef_t *parent = (menuDef_t*)item->parent;

	value = (item->cvar) ? DC->getCVarValue(item->cvar) : 0;

	if (item->window.flags & WINDOW_HASFOCUS && item->window.flags & WINDOW_FOCUSPULSE) {
		lowLight[0] = 0.8 * parent->focusColor[0]; 
		lowLight[1] = 0.8 * parent->focusColor[1]; 
		lowLight[2] = 0.8 * parent->focusColor[2]; 
		lowLight[3] = 0.8 * parent->focusColor[3]; 
		LerpColor(parent->focusColor,lowLight,newColor,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
	} else {
		memcpy(&newColor, &item->window.foreColor, sizeof(vec4_t));
	}

	y = item->window.rect.y;
	if (item->text) {
		Item_Text_Paint(item);
		x = item->textRect.x + item->textRect.w + 8;
	} else {
		x = item->window.rect.x;
	}
	DC->setColor(newColor);
	//DC->drawHandlePic( x, y, SLIDER_WIDTH, SLIDER_HEIGHT, DC->Assets.sliderBar );
	DC->drawHandlePic( x, y + 1, SLIDER_WIDTH, SLIDER_HEIGHT, DC->Assets.sliderBar );

	x = Item_Slider_ThumbPosition(item);
	//DC->drawHandlePic( x - (SLIDER_THUMB_WIDTH / 2), y - 2, SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT, DC->Assets.sliderThumb );
	DC->drawHandlePic( x - (SLIDER_THUMB_WIDTH / 2), y, SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT, DC->Assets.sliderThumb );
}

#endif

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const float bp795Dim=.8f,bp795Eight=8.f;
static const double bp795One=1.,bp795Half=.5;
static const char bp795Fixme[]="FIXME";
enum { bp795Data=offsetof(itemDef_t,typeData),
bp795Parent=offsetof(itemDef_t,parent),
bp795Cvar=offsetof(itemDef_t,cvar),
bp795Text=offsetof(itemDef_t,text),
bp795Style=offsetof(itemDef_t,textStyle),
bp795Scale=offsetof(itemDef_t,textscale),
bp795X=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,x),
bp795Y=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,y),
bp795W=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,w),
bp795Color=offsetof(itemDef_t,window)+offsetof(windowDef_t,foreColor),
bp795Flags=offsetof(itemDef_t,window)+offsetof(windowDef_t,flags),
bp795Focus=offsetof(menuDef_t,focusColor),
bp795Max=offsetof(editFieldDef_t,maxPaintChars),
bp795Value=offsetof(displayContextDef_t,getCVarValue),
bp795Time=offsetof(displayContextDef_t,realTime),
bp795Draw=offsetof(displayContextDef_t,drawText) };
__declspec(naked) void Item_Bind_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x20
 PUSH EBX
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x2c]
 XOR EBX,EBX
 PUSH EDI
 MOV EAX,dword ptr [ESI + bp795Data]
 MOV EDI,dword ptr [ESI + bp795Parent]
 TEST EAX,EAX
 JZ bp795_3008430f
 MOV EBX,dword ptr [EAX + bp795Max]
bp795_3008430f:
 MOV EAX,dword ptr [ESI + bp795Cvar]
 TEST EAX,EAX
 JZ bp795_3008432b
 PUSH EAX
 MOV EAX,DC
 CALL dword ptr [EAX + bp795Value]
 FSTP dword ptr [ESP + 0x34]
 ADD ESP,0x4
 JMP bp795_30084333
bp795_3008432b:
 MOV dword ptr [ESP + 0x30],0x0
bp795_30084333:
 MOV EAX,dword ptr [ESI + bp795Flags]
 TEST AL,0x2
 JZ bp795_30084408
 TEST EAX,0x8000000
 JZ bp795_30084408
 CMP dword ptr g_bindItem,ESI
 JNZ bp795_30084373
 MOV dword ptr [ESP + 0xc],0x3f4ccccd
 MOV dword ptr [ESP + 0x10],0x0
 MOV dword ptr [ESP + 0x14],0x0
 MOV dword ptr [ESP + 0x18],0x3f4ccccd
 JMP bp795_300843b3
bp795_30084373:
 FLD dword ptr [EDI + bp795Focus + 0]
 FMUL dword ptr bp795Dim
 FSTP dword ptr [ESP + 0xc]
 FLD dword ptr [EDI + bp795Focus + 4]
 FMUL dword ptr bp795Dim
 FSTP dword ptr [ESP + 0x10]
 FLD dword ptr [EDI + bp795Focus + 8]
 FMUL dword ptr bp795Dim
 FSTP dword ptr [ESP + 0x14]
 FLD dword ptr [EDI + bp795Focus + 12]
 FMUL dword ptr bp795Dim
 FSTP dword ptr [ESP + 0x18]
bp795_300843b3:
 MOV ECX,dword ptr DC
 MOV EAX,0x1b4e81b5
 ADD EDI,bp795Focus
 MOV ECX,dword ptr [ECX + bp795Time]
 IMUL ECX
 SAR EDX,0x3
 MOV EAX,EDX
 PUSH ECX
 SHR EAX,0x1f
 ADD EDX,EAX
 LEA ECX,[ESP + 0x20]
 MOV dword ptr [ESP + 0x34],EDX
 LEA EDX,[ESP + 0x10]
 FILD dword ptr [ESP + 0x34]
 FSIN
 FADD qword ptr bp795One
 FMUL qword ptr bp795Half
 FSTP dword ptr [ESP]
 PUSH ECX
 PUSH EDX
 PUSH EDI
 CALL LerpColor
 ADD ESP,0x10
 JMP bp795_3008449d
bp795_30084408:
 CMP dword ptr g_bindItem,ESI
 JNZ bp795_3008447e
 MOV EAX,DC
 MOV dword ptr [ESP + 0xc],0x3f4ccccd
 MOV dword ptr [ESP + 0x10],0x0
 MOV dword ptr [ESP + 0x14],0x0
 MOV dword ptr [ESP + 0x18],0x3f4ccccd
 MOV ECX,dword ptr [EAX + bp795Time]
 MOV EAX,0x1b4e81b5
 IMUL ECX
 SAR EDX,0x3
 MOV ECX,EDX
 LEA EAX,[ESP + 0xc]
 SHR ECX,0x1f
 ADD EDX,ECX
 PUSH ECX
 MOV dword ptr [ESP + 0x34],EDX
 LEA EDX,[ESP + 0x20]
 FILD dword ptr [ESP + 0x34]
 LEA ECX,[ESI + bp795Color]
 FSIN
 FADD qword ptr bp795One
 FMUL qword ptr bp795Half
 FSTP dword ptr [ESP]
 PUSH EDX
 PUSH EAX
 PUSH ECX
 CALL LerpColor
 ADD ESP,0x10
 JMP bp795_3008449d
bp795_3008447e:
 LEA EDX,[ESI + bp795Color]
 MOV EAX,dword ptr [ESI + bp795Color]
 MOV dword ptr [ESP + 0x1c],EAX
 MOV ECX,dword ptr [EDX + 0x4]
 MOV dword ptr [ESP + 0x20],ECX
 MOV EAX,dword ptr [EDX + 0x8]
 MOV dword ptr [ESP + 0x24],EAX
 MOV ECX,dword ptr [EDX + 0xc]
 MOV dword ptr [ESP + 0x28],ECX
bp795_3008449d:
 MOV EAX,dword ptr [ESI + bp795Text]
 TEST EAX,EAX
 JZ bp795_30084507
 PUSH ESI
 CALL Item_Text_Paint
 MOV EDX,dword ptr [ESI + bp795Cvar]
 PUSH EDX
 CALL BindingFromName
 MOV EAX,dword ptr [ESI + bp795Style]
 MOV EDX,dword ptr [ESI + bp795Scale]
 FLD dword ptr [ESI + bp795W]
 FADD dword ptr [ESI + bp795X]
 ADD ESP,0x8
 LEA ECX,[ESP + 0x1c]
 FADD dword ptr bp795Eight
 PUSH EAX
 MOV EAX,dword ptr [ESI + bp795Y]
 PUSH EBX
 PUSH 0x0
 PUSH offset g_nameBind1
 PUSH ECX
 PUSH EDX
 PUSH EAX
 PUSH ECX
 MOV ECX,dword ptr DC
 FSTP dword ptr [ESP]
 CALL dword ptr [ECX + bp795Draw]
 ADD ESP,0x20
 POP EDI
 POP ESI
 POP EBX
 ADD ESP,0x20
 RET
bp795_30084507:
 MOV EDX,dword ptr [ESI + bp795Style]
 MOV ECX,dword ptr [ESI + bp795Scale]
 PUSH EDX
 MOV EDX,dword ptr [ESI + bp795Y]
 PUSH EBX
 PUSH 0x0
 LEA EAX,[ESP + 0x28]
 PUSH offset bp795Fixme
 PUSH EAX
 MOV EAX,dword ptr [ESI + bp795X]
 PUSH ECX
 MOV ECX,dword ptr DC
 PUSH EDX
 PUSH EAX
 CALL dword ptr [ECX + bp795Draw]
 ADD ESP,0x20
 POP EDI
 POP ESI
 POP EBX
 ADD ESP,0x20
 RET
 }
}
#else
void Item_Bind_Paint(itemDef_t *item) {
	vec4_t newColor, lowLight;
	float value;
	int maxChars = 0;
	menuDef_t *parent = (menuDef_t*)item->parent;
	editFieldDef_t *editPtr = (editFieldDef_t*)item->typeData;
	if (editPtr) {
		maxChars = editPtr->maxPaintChars;
	}

	value = (item->cvar) ? DC->getCVarValue(item->cvar) : 0;

	if (item->window.flags & WINDOW_HASFOCUS && item->window.flags & WINDOW_FOCUSPULSE) {
		if (g_bindItem == item) {
			lowLight[0] = 0.8f * 1.0f;
			lowLight[1] = 0.8f * 0.0f;
			lowLight[2] = 0.8f * 0.0f;
			lowLight[3] = 0.8f * 1.0f;
		} else {
			lowLight[0] = 0.8f * parent->focusColor[0]; 
			lowLight[1] = 0.8f * parent->focusColor[1]; 
			lowLight[2] = 0.8f * parent->focusColor[2]; 
			lowLight[3] = 0.8f * parent->focusColor[3]; 
		}
		LerpColor(parent->focusColor,lowLight,newColor,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
	} else {
		if( g_bindItem == item ) {
			lowLight[0] = 0.8f * 1.0f;
			lowLight[1] = 0.8f * 0.0f;
			lowLight[2] = 0.8f * 0.0f;
			lowLight[3] = 0.8f * 1.0f;
			LerpColor( item->window.foreColor, lowLight, newColor, 0.5+0.5*sin(DC->realTime / PULSE_DIVISOR) );
		} else {
			memcpy( &newColor, &item->window.foreColor, sizeof(vec4_t) );
		}
	}

	if (item->text) {
		Item_Text_Paint(item);
		BindingFromName(item->cvar);
		DC->drawText(item->textRect.x + item->textRect.w + 8, item->textRect.y, item->textscale, newColor, g_nameBind1, 0, maxChars, item->textStyle);
	} else {
		DC->drawText(item->textRect.x, item->textRect.y, item->textscale, newColor, (value != 0) ? "FIXME" : "FIXME", 0, maxChars, item->textStyle);
	}
}

#endif
qboolean Display_KeyBindPending() {
	return g_waitingForKey;
}

qboolean Item_Bind_HandleKey(itemDef_t *item, int key, qboolean down) {
	int			id;
	int			i;

	if (Rect_ContainsPoint(&item->window.rect, DC->cursorx, DC->cursory) && !g_waitingForKey)
	{
		if (down && (key == K_MOUSE1 || key == K_ENTER)) {
			g_waitingForKey = qtrue;
			g_bindItem = item;
			return qtrue;
		} else {
			return qfalse;
		}
	}
	else
	{
		if (!g_waitingForKey || g_bindItem == NULL) {
			return qfalse;
		}

		if (key & K_CHAR_FLAG) {
			return qtrue;
		}

		switch (key)
		{
			case K_ESCAPE:
				g_waitingForKey = qfalse;
				g_bindItem = NULL;
				return qtrue;
	
			case K_BACKSPACE:
				id = BindingIDFromName(item->cvar);
				if (id != -1) {
					g_bindings[id].bind1 = -1;
					g_bindings[id].bind2 = -1;
				}
				Controls_SetConfig(qtrue);
				g_waitingForKey = qfalse;
				g_bindItem = NULL;
				return qtrue;

			case '`':
				return qtrue;
		}
	}

	if (key != -1)
	{

		for (i=0; i < g_bindCount; i++)
		{

			if (g_bindings[i].bind2 == key) {
				g_bindings[i].bind2 = -1;
			}

			if (g_bindings[i].bind1 == key)
			{
				g_bindings[i].bind1 = g_bindings[i].bind2;
				g_bindings[i].bind2 = -1;
			}
		}
	}


	id = BindingIDFromName(item->cvar);

	if (id != -1) {
		if (key == -1) {
			if( g_bindings[id].bind1 != -1 ) {
				DC->setBinding( g_bindings[id].bind1, "" );
				g_bindings[id].bind1 = -1;
			}
			if( g_bindings[id].bind2 != -1 ) {
				DC->setBinding( g_bindings[id].bind2, "" );
				g_bindings[id].bind2 = -1;
			}
		}
		else if (g_bindings[id].bind1 == -1) {
			g_bindings[id].bind1 = key;
		}
		else if (g_bindings[id].bind1 != key && g_bindings[id].bind2 == -1) {
			g_bindings[id].bind2 = key;
		}
		else {
			DC->setBinding( g_bindings[id].bind1, "" );
			DC->setBinding( g_bindings[id].bind2, "" );
			g_bindings[id].bind1 = key;
			g_bindings[id].bind2 = -1;
		}						
	}

	Controls_SetConfig(qtrue);	
	g_waitingForKey = qfalse;
	g_bindItem = NULL;

	return qtrue;
}



#include "tce_ui_coordinates.h"
#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
static const float tceAdjust780Height = 480.0f;
static const float tceAdjust780Ratio = 0.00117187504656612873077392578125f;
static const float tceAdjust780Horizontal = 0.750000059604644775390625f;
static const float tceAdjust780One = 1.0f;
static const float tceAdjust780Center = 240.0f;
static const float tceAdjust780Shift = 80.0f;
enum {
    tceAdjust780VidWidth = offsetof(displayContextDef_t, glconfig) + offsetof(glconfig_t, vidWidth),
    tceAdjust780VidHeight = offsetof(displayContextDef_t, glconfig) + offsetof(glconfig_t, vidHeight),
    tceAdjust780Xscale = offsetof(displayContextDef_t, xscale),
    tceAdjust780Yscale = offsetof(displayContextDef_t, yscale)
};
__declspec(naked) void AdjustFrom640(float *x,float *y,float *w,float *h) {
    __asm {
        sub esp,0x8
        mov eax,DC
        push esi
        mov ecx,dword ptr [eax + tceAdjust780VidHeight]
        mov edx,dword ptr [eax + tceAdjust780VidWidth]
        mov dword ptr [esp + 0x8],ecx
        mov dword ptr [esp + 0x4],edx
        lea esi,[ecx + ecx*0x4]
        lea ecx,[edx + edx*0x2]
        shl esi,0x7
        lea ecx,[ecx + ecx*0x4]
        shl ecx,0x5
        cmp ecx,esi
        pop esi
        jz tceAdjust780Standard
        fild dword ptr [esp]
        fmul dword ptr tceAdjust780Height
        fidiv dword ptr [esp + 0x4]
        fmul dword ptr tceAdjust780Ratio
        fld dword ptr [eax + tceAdjust780Xscale]
        mov eax,dword ptr [esp + 0xc]
        fmul dword ptr [eax]
        fmul dword ptr tceAdjust780Horizontal
        fstp dword ptr [eax]
        fld dword ptr tceAdjust780One
        fdiv st(0),st(1)
        mov eax,dword ptr [esp + 0x10]
        fsub dword ptr tceAdjust780One
        fmul dword ptr tceAdjust780Center
        fadd dword ptr [eax]
        fst dword ptr [eax]
        mov edx,dword ptr DC
        fmul dword ptr [edx + tceAdjust780Yscale]
        fmul st(0),st(1)
        fstp dword ptr [eax]
        mov eax,DC
        fld dword ptr [eax + tceAdjust780Xscale]
        mov eax,dword ptr [esp + 0x14]
        fmul dword ptr [eax]
        fmul dword ptr tceAdjust780Horizontal
        fstp dword ptr [eax]
        mov ecx,dword ptr DC
        mov eax,dword ptr [esp + 0x18]
        fld dword ptr [ecx + tceAdjust780Yscale]
        fmul dword ptr [eax]
        fmul st(0),st(1)
        fstp dword ptr [eax]
        fstp st(0)
        add esp,0x8
        ret
        tceAdjust780Standard:
        fld dword ptr [eax + tceAdjust780Xscale]
        mov eax,dword ptr [esp + 0xc]
        fmul dword ptr [eax]
        fmul dword ptr tceAdjust780Horizontal
        fstp dword ptr [eax]
        mov eax,dword ptr [esp + 0x10]
        fld dword ptr [eax]
        fadd dword ptr tceAdjust780Shift
        fst dword ptr [eax]
        mov edx,dword ptr DC
        fmul dword ptr [edx + tceAdjust780Yscale]
        fmul dword ptr tceAdjust780Horizontal
        fstp dword ptr [eax]
        mov eax,DC
        fld dword ptr [eax + tceAdjust780Xscale]
        mov eax,dword ptr [esp + 0x14]
        fmul dword ptr [eax]
        fmul dword ptr tceAdjust780Horizontal
        fstp dword ptr [eax]
        mov ecx,dword ptr DC
        mov eax,dword ptr [esp + 0x18]
        fld dword ptr [ecx + tceAdjust780Yscale]
        fmul dword ptr [eax]
        fmul dword ptr tceAdjust780Horizontal
        fstp dword ptr [eax]
        add esp,0x8
        ret
    }
}
#else
void AdjustFrom640(float *x,float *y,float *w,float *h) {
    TCE_UI_AdjustCoordinates(x,y,w,h,DC->glconfig.vidWidth,
        DC->glconfig.vidHeight,DC->xscale,DC->yscale);
}
#endif

#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
/* Original __ftol returns the low word of a truncating signed64 conversion. */
static int TCE_ModelViewportInteger782(float tceModel782Value) {
    unsigned short tceModel782Saved, tceModel782Truncate;
    __int64 tceModel782Result;
    int tceModel782Low;
    __asm {
        fld dword ptr tceModel782Value
        fstcw word ptr tceModel782Saved
        wait
        mov ax,word ptr tceModel782Saved
        or ah,0x0c
        mov word ptr tceModel782Truncate,ax
        fldcw word ptr tceModel782Truncate
        fistp qword ptr tceModel782Result
        fldcw word ptr tceModel782Saved
        mov eax,dword ptr tceModel782Result
        mov tceModel782Low,eax
    }
    return tceModel782Low;
}
static int TCE_ModelFloorInteger782(float tceModel782Value) {
    double (__cdecl *tceModel782Floor)(double) = floor;
    unsigned short tceModel782Saved, tceModel782Truncate;
    __int64 tceModel782Result;
    int tceModel782Low;
    __asm {
        fld dword ptr tceModel782Value
        sub esp,8
        fstp qword ptr [esp]
        call tceModel782Floor
        add esp,8
        fstcw word ptr tceModel782Saved
        wait
        mov ax,word ptr tceModel782Saved
        or ah,0x0c
        mov word ptr tceModel782Truncate,ax
        fldcw word ptr tceModel782Truncate
        fistp qword ptr tceModel782Result
        fldcw word ptr tceModel782Saved
        mov eax,dword ptr tceModel782Result
        mov tceModel782Low,eax
    }
    return tceModel782Low;
}
#endif
void Item_Model_Paint(itemDef_t *item) {
	float x, y, w, h;	//,xx;
	refdef_t refdef;
	qhandle_t		hModel;
	refEntity_t		ent;
	vec3_t			mins, maxs, origin;
	vec3_t			angles;
	modelDef_t *modelPtr = (modelDef_t*)item->typeData;
	int			backLerpWhole;

	if (modelPtr == NULL) {
		return;
	}

	if(!item->asset)
		return;

	hModel = item->asset;

	// setup the refdef
	memset( &refdef, 0, sizeof( refdef ) );
	refdef.rdflags = RDF_NOWORLDMODEL;
	AxisClear( refdef.viewaxis );
	x = item->window.rect.x+1;
	y = item->window.rect.y+1;
	w = item->window.rect.w-2;
	h = item->window.rect.h-2;

	AdjustFrom640( &x, &y, &w, &h );

#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
    refdef.x = TCE_ModelViewportInteger782(x);
    refdef.y = TCE_ModelViewportInteger782(y);
    refdef.width = TCE_ModelViewportInteger782(w);
    refdef.height = TCE_ModelViewportInteger782(h);
#else
	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;
#endif

	DC->modelBounds( hModel, mins, maxs );

#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
    {
        static const double tceModel781NegativeHalf = -0.5;
        static const double tceModel781Half = 0.5;
        static const double tceModel781Reciprocal = 3.731343283582089;
        __asm {
            lea eax,mins
            lea ecx,maxs
            lea edx,origin
            fld dword ptr [ecx + 8]
            fadd dword ptr [eax + 8]
            fmul qword ptr tceModel781NegativeHalf
            fstp dword ptr [edx + 8]
            fld dword ptr [ecx + 4]
            fadd dword ptr [eax + 4]
            fmul qword ptr tceModel781Half
            fstp dword ptr [edx + 4]
            fld dword ptr [ecx + 8]
            fsub dword ptr [eax + 8]
            fmul qword ptr tceModel781Half
            fmul qword ptr tceModel781Reciprocal
            fstp dword ptr [edx]
        }
    }
#else
	origin[2] = -0.5 * ( mins[2] + maxs[2] );
	origin[1] = 0.5 * ( mins[1] + maxs[1] );

	// calculate distance so the model nearly fills the box
	if (qtrue) {
		float len = 0.5 * ( maxs[2] - mins[2] );		
		origin[0] = len / 0.268;	// len / tan( fov/2 )
		//origin[0] = len / tan(w/2);
	} else {
		origin[0] = item->textscale;
	}
#endif

#define NEWWAY
#ifdef NEWWAY
#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
    {
        static const float tceModel782Zero = 0.0f;
        float *tceModel782FovX = &modelPtr->fov_x;
        float *tceModel782FovY = &modelPtr->fov_y;
        float *tceModel782ResultX = &refdef.fov_x;
        float *tceModel782ResultY = &refdef.fov_y;
        __asm {
            mov ecx,tceModel782FovX
            fld dword ptr [ecx]
            fcomp dword ptr tceModel782Zero
            fnstsw ax
            test ah,0x40
            jnz tceModel782FallbackX
            mov edx,dword ptr [ecx]
            jmp tceModel782StoreX
        tceModel782FallbackX:
            mov edx,dword ptr w
        tceModel782StoreX:
            mov ecx,tceModel782ResultX
            mov dword ptr [ecx],edx
            mov ecx,tceModel782FovY
            fld dword ptr [ecx]
            fcomp dword ptr tceModel782Zero
            fnstsw ax
            test ah,0x40
            jnz tceModel782FallbackY
            mov edx,dword ptr [ecx]
            jmp tceModel782StoreY
        tceModel782FallbackY:
            mov edx,dword ptr h
        tceModel782StoreY:
            mov ecx,tceModel782ResultY
            mov dword ptr [ecx],edx
        }
    }
#else
	refdef.fov_x = (modelPtr->fov_x) ? modelPtr->fov_x : w;
	refdef.fov_y = (modelPtr->fov_y) ? modelPtr->fov_y : h;
#endif
#else
	refdef.fov_x = (int)((float)refdef.width / 640.0f * 90.0f);
	xx = refdef.width / tan( refdef.fov_x / 360 * M_PI );
	refdef.fov_y = atan2( refdef.height, xx );
	refdef.fov_y *= ( 360 / M_PI );
#endif
	DC->clearScene();

	refdef.time = DC->realTime;

	// add the model

	memset( &ent, 0, sizeof(ent) );

	//adjust = 5.0 * sin( (float)uis.realtime / 500 );
	//adjust = 360 % (int)((float)uis.realtime / 1000);
	//VectorSet( angles, 0, 0, 1 );

	// use item storage to track
	if (modelPtr->rotationSpeed) {
		if (DC->realTime > item->window.nextTime) {
#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
            unsigned int tceModel782Next = (unsigned int)DC->realTime + (unsigned int)modelPtr->rotationSpeed;
            int *tceModel782Angle = &modelPtr->angle;
            memcpy(&item->window.nextTime, &tceModel782Next, sizeof(tceModel782Next));
            __asm {
                mov ecx,tceModel782Angle
                mov eax,dword ptr [ecx]
                inc eax
                cdq
                push ecx
                mov ecx,360
                idiv ecx
                pop ecx
                mov dword ptr [ecx],edx
            }
#else
			item->window.nextTime = DC->realTime + modelPtr->rotationSpeed;
			modelPtr->angle = (int)(modelPtr->angle + 1) % 360;
#endif
		}
	}
	VectorSet( angles, 0, modelPtr->angle, 0 );
	AnglesToAxis( angles, ent.axis );

	ent.hModel = hModel;


	if(modelPtr->frameTime) {
#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
        static const float tceModel781Millis = 0.0010000000474974513f;
        unsigned int tceModel781Delta = (unsigned int)DC->realTime - (unsigned int)modelPtr->frameTime;
        int tceModel781Fps = modelPtr->fps;
        float *tceModel781Backlerp = &modelPtr->backlerp;
        __asm {
            mov eax,tceModel781Backlerp
            fild dword ptr tceModel781Delta
            fmul dword ptr tceModel781Millis
            fimul dword ptr tceModel781Fps
            fadd dword ptr [eax]
            fstp dword ptr [eax]
        }
#else
		modelPtr->backlerp+=( ((DC->realTime - modelPtr->frameTime)/1000.0f) * (float)modelPtr->fps );
#endif
    }

	if(modelPtr->backlerp > 1) {
#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
        int *tceModel782Frame = &modelPtr->frame;
        int *tceModel782OldFrame = &modelPtr->oldframe;
        int tceModel782Start = modelPtr->startframe;
        int tceModel782Count = modelPtr->numframes;
        float *tceModel782Lerp = &modelPtr->backlerp;
        backLerpWhole = TCE_ModelFloorInteger782(modelPtr->backlerp);
        __asm {
            mov ecx,tceModel782Frame
            mov eax,dword ptr [ecx]
            add eax,backLerpWhole
            mov dword ptr [ecx],eax
            mov edx,eax
            sub edx,tceModel782Start
            cmp edx,tceModel782Count
            jle tceModel782Old
            cdq
            idiv tceModel782Count
            add edx,tceModel782Start
            mov dword ptr [ecx],edx
        tceModel782Old:
            mov ecx,tceModel782OldFrame
            mov eax,dword ptr [ecx]
            add eax,backLerpWhole
            mov dword ptr [ecx],eax
            mov edx,eax
            sub edx,tceModel782Start
            cmp edx,tceModel782Count
            jle tceModel782Remainder
            cdq
            idiv tceModel782Count
            add edx,tceModel782Start
            mov dword ptr [ecx],edx
        tceModel782Remainder:
            mov ecx,tceModel782Lerp
            fild dword ptr backLerpWhole
            fsubr dword ptr [ecx]
            fstp dword ptr [ecx]
        }
#else
		backLerpWhole = floor(modelPtr->backlerp);

		modelPtr->frame+=(backLerpWhole);
		if((modelPtr->frame - modelPtr->startframe) > modelPtr->numframes)
			modelPtr->frame = modelPtr->startframe + modelPtr->frame % modelPtr->numframes;	// todo: ignoring loopframes

		modelPtr->oldframe+=(backLerpWhole);
		if((modelPtr->oldframe - modelPtr->startframe) > modelPtr->numframes)
			modelPtr->oldframe = modelPtr->startframe + modelPtr->oldframe % modelPtr->numframes;	// todo: ignoring loopframes

		modelPtr->backlerp = modelPtr->backlerp - backLerpWhole;
#endif
	}

	modelPtr->frameTime = DC->realTime;

	ent.frame		= modelPtr->frame;
	ent.oldframe	= modelPtr->oldframe;
#if !defined(UIDLL) && defined(_MSC_VER) && defined(_M_IX86)
    {
        static const float tceModel782One = 1.0f;
        float *tceModel782Lerp = &modelPtr->backlerp;
        float *tceModel782EntityLerp = &ent.backlerp;
        __asm {
            mov ecx,tceModel782Lerp
            mov edx,tceModel782EntityLerp
            fld dword ptr tceModel782One
            fsub dword ptr [ecx]
            fstp dword ptr [edx]
        }
    }
    memcpy(ent.origin, origin, sizeof(origin));
    memcpy(ent.lightingOrigin, origin, sizeof(origin));
    ent.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;
    memcpy(ent.oldorigin, origin, sizeof(origin));
#else
	ent.backlerp	= 1.0f - modelPtr->backlerp;

	VectorCopy( origin, ent.origin );
	VectorCopy( origin, ent.lightingOrigin );
	ent.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;
	VectorCopy( ent.origin, ent.oldorigin );
#endif

	DC->addRefEntityToScene( &ent );
	DC->renderScene( &refdef );

}


void Item_Image_Paint(itemDef_t *item) {
	if (item == NULL) {
		return;
	}
	DC->drawHandlePic(item->window.rect.x+1, item->window.rect.y+1, item->window.rect.w-2, item->window.rect.h-2, item->asset);
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const float lp800One=1.f, lp800Four=4.f, lp800Two=2.f;
static const double lp800Sixteen=16., lp800OneDouble=1., lp800Fifteen=15., lp800ThirtyTwo=32., lp800TwoDouble=2.;
enum {
 lp800I0=offsetof(itemDef_t,window.rect.x),
 lp800I4=offsetof(itemDef_t,window.rect.y),
 lp800I8=offsetof(itemDef_t,window.rect.w),
 lp800I44=offsetof(itemDef_t,window.borderSize),
 lp800I48=offsetof(itemDef_t,window.flags),
 lp800I74=offsetof(itemDef_t,window.foreColor),
 lp800I94=offsetof(itemDef_t,window.borderColor),
 lp800I248=offsetof(itemDef_t,special),
 lp800I250=offsetof(itemDef_t,typeData),
 lp800Ic=offsetof(itemDef_t,window.rect.h),
 lp800Ia4=offsetof(itemDef_t,window.outlineColor),
 lp800Id4=offsetof(itemDef_t,textalignx),
 lp800Id8=offsetof(itemDef_t,textaligny),
 lp800Idc=offsetof(itemDef_t,textscale),
 lp800Ie4=offsetof(itemDef_t,textStyle),
 lp800I24c=offsetof(itemDef_t,cursorPos),
 lp800L0=offsetof(listBoxDef_t,startPos),
 lp800L4=offsetof(listBoxDef_t,endPos),
 lp800L8=offsetof(listBoxDef_t,drawPadding),
 lp800L10=offsetof(listBoxDef_t,elementWidth),
 lp800L14=offsetof(listBoxDef_t,elementHeight),
 lp800L18=offsetof(listBoxDef_t,elementStyle),
 lp800L20=offsetof(listBoxDef_t,columnInfo),
 lp800L1c=offsetof(listBoxDef_t,numColumns),
 lp800D8=offsetof(displayContextDef_t,drawHandlePic),
 lp800D10=offsetof(displayContextDef_t,drawText),
 lp800D40=offsetof(displayContextDef_t,drawRect),
 lp800D90=offsetof(displayContextDef_t,feederCount),
 lp800D94=offsetof(displayContextDef_t,feederItemText),
 lp800D3c=offsetof(displayContextDef_t,fillRect),
 lp800D9c=offsetof(displayContextDef_t,feederItemImage),
 lp800D1e2e0=offsetof(displayContextDef_t,Assets.scrollBarArrowUp),
 lp800D1e2e4=offsetof(displayContextDef_t,Assets.scrollBarArrowDown),
 lp800D1e2e8=offsetof(displayContextDef_t,Assets.scrollBarArrowLeft),
 lp800D1e2ec=offsetof(displayContextDef_t,Assets.scrollBarArrowRight),
 lp800D1e2f0=offsetof(displayContextDef_t,Assets.scrollBar),
 lp800D1e2f4=offsetof(displayContextDef_t,Assets.scrollBarThumb),
 lp800C0=offsetof(columnInfo_t,pos),
 lp800C8=offsetof(columnInfo_t,maxChars),
};
typedef char lp800ColumnLayout[(sizeof(columnInfo_t)==12 && offsetof(columnInfo_t,pos)==0 && offsetof(columnInfo_t,maxChars)==8)?1:-1];
__declspec(naked) void Item_ListBox_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x54
 PUSH EBX
 PUSH EBP
 PUSH ESI
 PUSH EDI
 MOV EDI,dword ptr [ESP + 0x68]
 MOV EAX,EDI
 MOV ESI,dword ptr [EDI + lp800I250]
 MOV ECX,dword ptr [EAX + lp800I0]
 MOV dword ptr [ESP + 0x34],ECX
 MOV EDX,dword ptr [EAX + lp800I4]
 MOV dword ptr [ESP + 0x38],EDX
 MOV ECX,dword ptr [EAX + lp800I8]
 MOV dword ptr [ESP + 0x3c],ECX
 MOV ECX,dword ptr [DC]
 MOV EDX,dword ptr [EAX + lp800Ic]
 MOV EAX,dword ptr [EDI + lp800I248]
 PUSH EAX
 MOV dword ptr [ESP + 0x44],EDX
 CALL dword ptr [ECX + lp800D90]
 MOV dword ptr [ESP + 0x6c],EAX
 MOV EAX,dword ptr [EDI + lp800I48]
 FILD dword ptr [ESP + 0x6c]
 ADD ESP,0x4
 TEST AH,0x4
 FSTP dword ptr [ESP + 0x1c]
 JZ lp800_30084c8c
 FLD dword ptr [ESP + 0x34]
 FADD dword ptr [lp800One]
 MOV EAX,[DC]
 MOV EDX,dword ptr [EAX + lp800D1e2e8]
 FST dword ptr [ESP + 0x28]
 FSTP dword ptr [ESP + 0x18]
 FLD dword ptr [ESP + 0x40]
 FADD dword ptr [ESP + 0x38]
 MOV ECX,dword ptr [ESP + 0x18]
 PUSH EDX
 PUSH 0x41800000
 PUSH 0x41800000
 FSUB qword ptr [lp800Sixteen]
 FSUB qword ptr [lp800OneDouble]
 FSTP dword ptr [ESP + 0x74]
 MOV EBX,dword ptr [ESP + 0x74]
 PUSH EBX
 PUSH ECX
 CALL dword ptr [EAX + lp800D8]
 FLD dword ptr [ESP + 0x2c]
 FADD qword ptr [lp800Fifteen]
 MOV EAX,[DC]
 ADD ESP,0x14
 MOV EDX,dword ptr [EAX + lp800D1e2f0]
 FSTP dword ptr [ESP + 0x18]
 FLD dword ptr [ESP + 0x3c]
 FSUB qword ptr [lp800ThirtyTwo]
 PUSH EDX
 PUSH 0x41800000
 PUSH ECX
 MOV ECX,dword ptr [ESP + 0x24]
 FSTP dword ptr [ESP + 0x20]
 FLD dword ptr [ESP + 0x20]
 FADD dword ptr [lp800One]
 FSTP dword ptr [ESP]
 PUSH EBX
 PUSH ECX
 CALL dword ptr [EAX + lp800D8]
 FLD dword ptr [ESP + 0x28]
 FSUB dword ptr [lp800One]
 MOV EAX,[DC]
 FADD dword ptr [ESP + 0x2c]
 MOV EDX,dword ptr [EAX + lp800D1e2ec]
 PUSH EDX
 PUSH 0x41800000
 PUSH 0x41800000
 PUSH EBX
 FSTP dword ptr [ESP + 0x3c]
 MOV ECX,dword ptr [ESP + 0x3c]
 PUSH ECX
 CALL dword ptr [EAX + lp800D8]
 PUSH EDI
 CALL Item_ListBox_ThumbDrawPosition
 MOV dword ptr [ESP + 0x94],EAX
 ADD ESP,0x2c
 FILD dword ptr [ESP + 0x68]
 FSTP dword ptr [ESP + 0x10]
 FLD dword ptr [ESP + 0x18]
 FSUB qword ptr [lp800Sixteen]
 FSUB qword ptr [lp800OneDouble]
 FLD dword ptr [ESP + 0x10]
 FCOMP ST(1)
 FNSTSW AX
 TEST AH,0x41
 JNZ lp800_30084b1c
 FSTP dword ptr [ESP + 0x10]
 JMP lp800_30084b1e
lp800_30084b1c:
 FSTP ST(0)
lp800_30084b1e:
 MOV EAX,[DC]
 MOV ECX,dword ptr [ESP + 0x10]
 MOV EDX,dword ptr [EAX + lp800D1e2f4]
 PUSH EDX
 PUSH 0x41800000
 PUSH 0x41800000
 PUSH EBX
 PUSH ECX
 CALL dword ptr [EAX + lp800D8]
 FLD dword ptr [ESP + 0x50]
 MOV EAX,dword ptr [ESI + lp800L0]
 ADD ESP,0x14
 FSUB dword ptr [lp800Two]
 MOV dword ptr [ESP + 0x24],EAX
 MOV dword ptr [ESI + lp800L4],EAX
 CMP dword ptr [ESI + lp800L18],0x1
 FSTP dword ptr [ESP + 0x14]
 JNZ lp800_300851aa
 FLD dword ptr [ESP + 0x38]
 FADD dword ptr [lp800One]
 MOV EDX,dword ptr [ESP + 0x28]
 MOV dword ptr [ESP + 0x18],EDX
 FSTP dword ptr [ESP + 0x68]
 FILD dword ptr [ESP + 0x24]
 FST dword ptr [ESP + 0x10]
 FCOMP dword ptr [ESP + 0x1c]
 FNSTSW AX
 TEST AH,0x1
 JZ lp800_300851aa
 MOV EBX,dword ptr [ESP + 0x68]
lp800_30084b92:
 FLD dword ptr [ESP + 0x10]
 CALL TCE_TextInteger788
 MOV ECX,dword ptr [DC]
 PUSH EAX
 MOV EAX,dword ptr [EDI + lp800I248]
 PUSH EAX
 CALL dword ptr [ECX + lp800D9c]
 ADD ESP,0x8
 TEST EAX,EAX
 JZ lp800_30084bf9
 FLD dword ptr [ESI + lp800L14]
 FSUB dword ptr [lp800Two]
 PUSH EAX
 PUSH ECX
 MOV EDX,dword ptr [DC]
 FSTP dword ptr [ESP]
 FLD dword ptr [ESI + lp800L10]
 FSUB dword ptr [lp800Two]
 PUSH ECX
 FSTP dword ptr [ESP]
 FLD dword ptr [ESP + 0x74]
 FADD dword ptr [lp800One]
 PUSH ECX
 FSTP dword ptr [ESP]
 FLD dword ptr [ESP + 0x28]
 FADD dword ptr [lp800One]
 PUSH ECX
 FSTP dword ptr [ESP]
 CALL dword ptr [EDX + lp800D8]
 ADD ESP,0x14
lp800_30084bf9:
 FILD dword ptr [EDI + lp800I24c]
 FCOMP dword ptr [ESP + 0x10]
 FNSTSW AX
 TEST AH,0x40
 JZ lp800_30084c40
 FLD dword ptr [ESI + lp800L14]
 MOV ECX,dword ptr [EDI + lp800I44]
 LEA EAX,[EDI + lp800I94]
 FSUB dword ptr [lp800One]
 PUSH EAX
 PUSH ECX
 PUSH ECX
 MOV EDX,dword ptr [ESP + 0x24]
 MOV EAX,[DC]
 FSTP dword ptr [ESP]
 FLD dword ptr [ESI + lp800L10]
 FSUB dword ptr [lp800One]
 PUSH ECX
 FSTP dword ptr [ESP]
 PUSH EBX
 PUSH EDX
 CALL dword ptr [EAX + lp800D40]
 ADD ESP,0x18
lp800_30084c40:
 FLD dword ptr [ESP + 0x14]
 FSUB dword ptr [ESI + lp800L10]
 FST dword ptr [ESP + 0x14]
 FCOMP dword ptr [ESI + lp800L10]
 FNSTSW AX
 TEST AH,0x1
 JNZ lp800_3008519e
 FLD dword ptr [ESP + 0x18]
 FADD dword ptr [ESI + lp800L10]
 INC dword ptr [ESI + lp800L4]
 FSTP dword ptr [ESP + 0x18]
 FLD dword ptr [ESP + 0x10]
 FADD dword ptr [lp800One]
 FST dword ptr [ESP + 0x10]
 FCOMP dword ptr [ESP + 0x1c]
 FNSTSW AX
 TEST AH,0x1
 JNZ lp800_30084b92
 POP EDI
 POP ESI
 POP EBP
 POP EBX
 ADD ESP,0x54
 RET
lp800_30084c8c:
 FLD dword ptr [ESP + 0x3c]
 FADD dword ptr [ESP + 0x34]
 MOV EAX,[DC]
 FSUB qword ptr [lp800Sixteen]
 MOV ECX,dword ptr [EAX + lp800D1e2e0]
 PUSH ECX
 PUSH 0x41800000
 FSUB qword ptr [lp800OneDouble]
 PUSH 0x41800000
 FSTP dword ptr [ESP + 0x24]
 FLD dword ptr [ESP + 0x44]
 FADD dword ptr [lp800One]
 MOV EBX,dword ptr [ESP + 0x24]
 FST dword ptr [ESP + 0x30]
 FSTP dword ptr [ESP + 0x74]
 MOV EDX,dword ptr [ESP + 0x74]
 PUSH EDX
 PUSH EBX
 CALL dword ptr [EAX + lp800D8]
 FLD dword ptr [ESP + 0x7c]
 FADD qword ptr [lp800Fifteen]
 MOV EAX,dword ptr [ESI + lp800L0]
 ADD ESP,0x14
 MOV dword ptr [ESI + lp800L4],EAX
 MOV EAX,[DC]
 FSTP dword ptr [ESP + 0x68]
 FLD dword ptr [ESP + 0x40]
 FSUB qword ptr [lp800ThirtyTwo]
 MOV ECX,dword ptr [EAX + lp800D1e2f0]
 MOV EDX,dword ptr [ESP + 0x68]
 PUSH ECX
 PUSH ECX
 FSTP dword ptr [ESP + 0x1c]
 FLD dword ptr [ESP + 0x1c]
 FADD dword ptr [lp800One]
 FSTP dword ptr [ESP]
 PUSH 0x41800000
 PUSH EDX
 PUSH EBX
 CALL dword ptr [EAX + lp800D8]
 FLD dword ptr [ESP + 0x28]
 FSUB dword ptr [lp800One]
 MOV EAX,[DC]
 FADD dword ptr [ESP + 0x7c]
 MOV ECX,dword ptr [EAX + lp800D1e2e4]
 PUSH ECX
 PUSH 0x41800000
 PUSH 0x41800000
 FSTP dword ptr [ESP + 0x88]
 MOV EDX,dword ptr [ESP + 0x88]
 PUSH EDX
 PUSH EBX
 CALL dword ptr [EAX + lp800D8]
 PUSH EDI
 CALL Item_ListBox_ThumbDrawPosition
 MOV dword ptr [ESP + 0x54],EAX
 ADD ESP,0x2c
 FILD dword ptr [ESP + 0x28]
 FSTP dword ptr [ESP + 0x10]
 FLD dword ptr [ESP + 0x68]
 FSUB qword ptr [lp800Sixteen]
 FSUB qword ptr [lp800OneDouble]
 FLD dword ptr [ESP + 0x10]
 FCOMP ST(1)
 FNSTSW AX
 TEST AH,0x41
 JNZ lp800_30084d94
 FSTP dword ptr [ESP + 0x10]
 JMP lp800_30084d96
lp800_30084d94:
 FSTP ST(0)
lp800_30084d96:
 MOV EAX,[DC]
 MOV EDX,dword ptr [ESP + 0x10]
 MOV ECX,dword ptr [EAX + lp800D1e2f4]
 PUSH ECX
 PUSH 0x41800000
 PUSH 0x41800000
 PUSH EDX
 PUSH EBX
 CALL dword ptr [EAX + lp800D8]
 MOV EAX,dword ptr [ESP + 0x54]
 ADD ESP,0x14
 MOV dword ptr [ESP + 0x14],EAX
 MOV EAX,dword ptr [ESI + lp800L18]
 CMP EAX,0x1
 JNZ lp800_30084f33
 FLD dword ptr [ESP + 0x34]
 FADD dword ptr [lp800One]
 MOV ECX,dword ptr [ESP + 0x24]
 MOV dword ptr [ESP + 0x68],ECX
 FSTP dword ptr [ESP + 0x18]
 FILD dword ptr [ESI + lp800L0]
 FST dword ptr [ESP + 0x10]
 FCOMP dword ptr [ESP + 0x1c]
 FNSTSW AX
 TEST AH,0x1
 JZ lp800_300851aa
 MOV EBP,dword ptr [ESP + 0x18]
lp800_30084dfb:
 FILD dword ptr [EDI + lp800I24c]
 MOV EBX,dword ptr [ESP + 0x68]
 FCOMP dword ptr [ESP + 0x10]
 FNSTSW AX
 TEST AH,0x40
 JZ lp800_30084e3e
 FLD dword ptr [ESI + lp800L14]
 FSUB dword ptr [lp800One]
 LEA EDX,[EDI + lp800Ia4]
 MOV EAX,[DC]
 PUSH EDX
 PUSH ECX
 FSTP dword ptr [ESP]
 FLD dword ptr [ESI + lp800L10]
 FSUB dword ptr [lp800One]
 PUSH ECX
 FSTP dword ptr [ESP]
 PUSH EBX
 PUSH EBP
 CALL dword ptr [EAX + lp800D3c]
 ADD ESP,0x14
lp800_30084e3e:
 FLD dword ptr [ESP + 0x10]
 CALL TCE_TextInteger788
 MOV ECX,dword ptr [EDI + lp800I248]
 MOV EDX,dword ptr [DC]
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + lp800D9c]
 ADD ESP,0x8
 TEST EAX,EAX
 JZ lp800_30084ea4
 FLD dword ptr [ESI + lp800L14]
 FSUB dword ptr [lp800Two]
 PUSH EAX
 PUSH ECX
 MOV EAX,[DC]
 FSTP dword ptr [ESP]
 FLD dword ptr [ESI + lp800L10]
 FSUB dword ptr [lp800Two]
 PUSH ECX
 FSTP dword ptr [ESP]
 FLD dword ptr [ESP + 0x74]
 FADD dword ptr [lp800One]
 PUSH ECX
 FSTP dword ptr [ESP]
 FLD dword ptr [ESP + 0x28]
 FADD dword ptr [lp800One]
 PUSH ECX
 FSTP dword ptr [ESP]
 CALL dword ptr [EAX + lp800D8]
 ADD ESP,0x14
lp800_30084ea4:
 FILD dword ptr [EDI + lp800I24c]
 FCOMP dword ptr [ESP + 0x10]
 FNSTSW AX
 TEST AH,0x40
 JZ lp800_30084ee7
 FLD dword ptr [ESI + lp800L14]
 MOV EDX,dword ptr [EDI + lp800I44]
 LEA ECX,[EDI + lp800I94]
 FSUB dword ptr [lp800One]
 PUSH ECX
 PUSH EDX
 PUSH ECX
 MOV EAX,[DC]
 FSTP dword ptr [ESP]
 FLD dword ptr [ESI + lp800L10]
 FSUB dword ptr [lp800One]
 PUSH ECX
 FSTP dword ptr [ESP]
 PUSH EBX
 PUSH EBP
 CALL dword ptr [EAX + lp800D40]
 ADD ESP,0x18
lp800_30084ee7:
 FLD dword ptr [ESP + 0x14]
 FSUB dword ptr [ESI + lp800L14]
 INC dword ptr [ESI + lp800L4]
 FST dword ptr [ESP + 0x14]
 FCOMP dword ptr [ESI + lp800L14]
 FNSTSW AX
 TEST AH,0x1
 JNZ lp800_3008519e
 FLD dword ptr [ESP + 0x68]
 FADD dword ptr [ESI + lp800L14]
 FSTP dword ptr [ESP + 0x68]
 FLD dword ptr [ESP + 0x10]
 FADD dword ptr [lp800One]
 FST dword ptr [ESP + 0x10]
 FCOMP dword ptr [ESP + 0x1c]
 FNSTSW AX
 TEST AH,0x1
 JNZ lp800_30084dfb
 POP EDI
 POP ESI
 POP EBP
 POP EBX
 ADD ESP,0x54
 RET
lp800_30084f33:
 FILD dword ptr [ESI + lp800L0]
 MOV ECX,dword ptr [ESP + 0x38]
 MOV dword ptr [ESP + 0x68],ECX
 FST dword ptr [ESP + 0x10]
 FCOMP dword ptr [ESP + 0x1c]
 FNSTSW AX
 TEST AH,0x1
 JZ lp800_300851aa
lp800_30084f50:
 MOV EAX,dword ptr [ESI + lp800L1c]
 TEST EAX,EAX
 JLE lp800_30085091
 XOR EBP,EBP
 TEST EAX,EAX
 MOV dword ptr [ESP + 0x28],EBP
 JLE lp800_30085111
 FLD dword ptr [ESP + 0x10]
 CALL TCE_TextInteger788
 MOV dword ptr [ESP + 0x2c],EAX
 LEA EBX,[ESI + lp800L20]
lp800_30084f79:
 MOV ECX,dword ptr [ESP + 0x2c]
 LEA EDX,[ESP + 0x20]
 LEA EAX,[ESP + 0x44]
 PUSH EDX
 MOV EDX,dword ptr [EDI + lp800I248]
 PUSH EAX
 MOV EAX,[DC]
 PUSH EBP
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + lp800D94]
 MOV ECX,dword ptr [ESP + 0x34]
 ADD ESP,0x14
 TEST ECX,ECX
 JLE lp800_30085025
 XOR EBP,EBP
 TEST ECX,ECX
 MOV dword ptr [ESP + 0x24],EBP
 JLE lp800_30085075
 LEA EDX,[ESP + 0x44]
 MOV dword ptr [ESP + 0x18],EDX
lp800_30084fbc:
 MOV EAX,dword ptr [ESP + 0x18]
 MOV EAX,dword ptr [EAX]
 TEST EAX,EAX
 JL lp800_3008500f
 FLD dword ptr [ESI + lp800L14]
 FSUB dword ptr [lp800Two]
 PUSH EAX
 FSTP dword ptr [ESP + 0x34]
 MOV EAX,dword ptr [ESP + 0x34]
 FLD dword ptr [ESP + 0x6c]
 FADD dword ptr [lp800One]
 PUSH EAX
 PUSH EAX
 PUSH ECX
 FSTP dword ptr [ESP]
 FILD dword ptr [ESP + 0x34]
 PUSH ECX
 MOV ECX,dword ptr [DC]
 FMUL dword ptr [ESI + lp800L14]
 FIADD dword ptr [EBX + lp800C0]
 FADD dword ptr [ESP + 0x48]
 FADD dword ptr [lp800One]
 FSTP dword ptr [ESP]
 CALL dword ptr [ECX + lp800D8]
 MOV ECX,dword ptr [ESP + 0x34]
 ADD ESP,0x14
lp800_3008500f:
 MOV EDX,dword ptr [ESP + 0x18]
 INC EBP
 ADD EDX,0x4
 CMP EBP,ECX
 MOV dword ptr [ESP + 0x24],EBP
 MOV dword ptr [ESP + 0x18],EDX
 JL lp800_30084fbc
 JMP lp800_30085075
lp800_30085025:
 TEST EAX,EAX
 JZ lp800_30085075
 MOV EDX,dword ptr [EDI + lp800Ie4]
 MOV ECX,dword ptr [EBX + lp800C8]
 FLD dword ptr [EDI + lp800Id8]
 FADD dword ptr [ESI + lp800L14]
 PUSH EDX
 PUSH ECX
 PUSH 0x0
 PUSH EAX
 MOV EAX,dword ptr [EDI + lp800Idc]
 LEA EDX,[EDI + lp800I74]
 FADD dword ptr [ESP + 0x78]
 PUSH EDX
 PUSH EAX
 PUSH ECX
 FSTP dword ptr [ESP]
 FILD dword ptr [EBX + lp800C0]
 PUSH ECX
 MOV ECX,dword ptr [DC]
 FADD dword ptr [EDI + lp800Id4]
 FADD dword ptr [ESP + 0x54]
 FADD dword ptr [lp800Four]
 FSTP dword ptr [ESP]
 CALL dword ptr [ECX + lp800D10]
 ADD ESP,0x20
lp800_30085075:
 MOV EBP,dword ptr [ESP + 0x28]
 MOV EAX,dword ptr [ESI + lp800L1c]
 INC EBP
 ADD EBX,0xc
 CMP EBP,EAX
 MOV dword ptr [ESP + 0x28],EBP
 JL lp800_30084f79
 JMP lp800_30085111
lp800_30085091:
 FLD dword ptr [ESP + 0x10]
 LEA EDX,[ESP + 0x20]
 LEA EAX,[ESP + 0x44]
 PUSH EDX
 PUSH EAX
 PUSH 0x0
 CALL TCE_TextInteger788
 MOV ECX,dword ptr [EDI + lp800I248]
 MOV EDX,dword ptr [DC]
 PUSH EAX
 PUSH ECX
 CALL dword ptr [EDX + lp800D94]
 MOV ECX,dword ptr [ESP + 0x34]
 ADD ESP,0x14
 TEST ECX,ECX
 JGE lp800_30085111
 TEST EAX,EAX
 JZ lp800_30085111
 MOV ECX,dword ptr [EDI + lp800Ie4]
 LEA EDX,[EDI + lp800I74]
 FLD dword ptr [EDI + lp800Id8]
 FADD dword ptr [ESI + lp800L14]
 PUSH ECX
 PUSH 0x0
 PUSH 0x0
 PUSH EAX
 MOV EAX,dword ptr [EDI + lp800Idc]
 PUSH EDX
 FADD dword ptr [ESP + 0x7c]
 PUSH EAX
 PUSH ECX
 FSTP dword ptr [ESP]
 FLD dword ptr [ESP + 0x50]
 FADD dword ptr [EDI + lp800Id4]
 PUSH ECX
 MOV ECX,dword ptr [DC]
 FADD dword ptr [lp800Four]
 FSTP dword ptr [ESP]
 CALL dword ptr [ECX + lp800D10]
 ADD ESP,0x20
lp800_30085111:
 FILD dword ptr [EDI + lp800I24c]
 FCOMP dword ptr [ESP + 0x10]
 FNSTSW AX
 TEST AH,0x40
 JZ lp800_30085156
 FLD dword ptr [ESP + 0x3c]
 FSUB qword ptr [lp800Sixteen]
 MOV EAX,dword ptr [ESI + lp800L14]
 LEA EDX,[EDI + lp800Ia4]
 PUSH EDX
 MOV EDX,dword ptr [ESP + 0x38]
 FSUB qword ptr [lp800TwoDouble]
 PUSH EAX
 MOV EAX,[DC]
 PUSH ECX
 MOV ECX,dword ptr [ESP + 0x74]
 FSTP dword ptr [ESP]
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + lp800D3c]
 ADD ESP,0x14
lp800_30085156:
 FLD dword ptr [ESP + 0x14]
 FSUB dword ptr [ESI + lp800L14]
 FST dword ptr [ESP + 0x14]
 FCOMP dword ptr [ESI + lp800L14]
 FNSTSW AX
 TEST AH,0x1
 JNZ lp800_3008519e
 FLD dword ptr [ESP + 0x68]
 FADD dword ptr [ESI + lp800L14]
 INC dword ptr [ESI + lp800L4]
 FSTP dword ptr [ESP + 0x68]
 FLD dword ptr [ESP + 0x10]
 FADD dword ptr [lp800One]
 FST dword ptr [ESP + 0x10]
 FCOMP dword ptr [ESP + 0x1c]
 FNSTSW AX
 TEST AH,0x1
 JNZ lp800_30084f50
 POP EDI
 POP ESI
 POP EBP
 POP EBX
 ADD ESP,0x54
 RET
lp800_3008519e:
 FLD dword ptr [ESP + 0x14]
 CALL TCE_TextInteger788
 MOV dword ptr [ESI + lp800L8],EAX
lp800_300851aa:
 POP EDI
 POP ESI
 POP EBP
 POP EBX
 ADD ESP,0x54
 RET
 }
}
#else
void Item_ListBox_Paint(itemDef_t *item) {
	float x, y, size, count, i, thumb;
	qhandle_t image;
	qhandle_t optionalImages[8];
	int numOptionalImages;
	listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;
	rectDef_t fillRect = item->window.rect;

	/*if( item->window.borderSize ) {
		fillRect.x += item->window.borderSize;
		fillRect.y += item->window.borderSize;
		fillRect.w -= 2 * item->window.borderSize;
		fillRect.h -= 2 * item->window.borderSize;
	}*/

	// the listbox is horizontal or vertical and has a fixed size scroll bar going either direction
	// elements are enumerated from the DC and either text or image handles are acquired from the DC as well
	// textscale is used to size the text, textalignx and textaligny are used to size image elements
	// there is no clipping available so only the last completely visible item is painted
	count = DC->feederCount(item->special);
	// default is vertical if horizontal flag is not here
	if (item->window.flags & WINDOW_HORIZONTAL) {
		// draw scrollbar in bottom of the window
		// bar
		x = fillRect.x + 1;
		y = fillRect.y + fillRect.h - SCROLLBAR_SIZE - 1;
		DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowLeft);
		x += SCROLLBAR_SIZE - 1;
		size = fillRect.w - (SCROLLBAR_SIZE * 2);
		DC->drawHandlePic(x, y, size+1, SCROLLBAR_SIZE, DC->Assets.scrollBar);
		x += size - 1;
		DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowRight);
		// thumb
		thumb = Item_ListBox_ThumbDrawPosition(item);//Item_ListBox_ThumbPosition(item);
		if (thumb > x - SCROLLBAR_SIZE - 1) {
			thumb = x - SCROLLBAR_SIZE - 1;
		}
		DC->drawHandlePic(thumb, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarThumb);
		//
		listPtr->endPos = listPtr->startPos;
		size = fillRect.w - 2;
		// items
		// size contains max available space
		if (listPtr->elementStyle == LISTBOX_IMAGE) {
			// fit = 0;
			x = fillRect.x + 1;
			y = fillRect.y + 1;
			for (i = listPtr->startPos; i < count; i++) {
				// always draw at least one
				// which may overdraw the box if it is too small for the element
				image = DC->feederItemImage(item->special, i);
				if (image) {
					DC->drawHandlePic(x+1, y+1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, image);
				}

				if (i == item->cursorPos) {
					DC->drawRect(x, y, listPtr->elementWidth-1, listPtr->elementHeight-1, item->window.borderSize, item->window.borderColor);
				}

				size -= listPtr->elementWidth;
				if (size < listPtr->elementWidth) {
					listPtr->drawPadding = size; //listPtr->elementWidth - size;
					break;
				}
				x += listPtr->elementWidth;
				listPtr->endPos++;
				// fit++;
			}
		} else {
			//
		}
	} else {
		// draw scrollbar to right side of the window
		x = fillRect.x + fillRect.w - SCROLLBAR_SIZE - 1;
		y = fillRect.y + 1;
		DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowUp);
		y += SCROLLBAR_SIZE - 1;

		listPtr->endPos = listPtr->startPos;
		size = fillRect.h - (SCROLLBAR_SIZE * 2);
		DC->drawHandlePic(x, y, SCROLLBAR_SIZE, size+1, DC->Assets.scrollBar);
		y += size - 1;
		DC->drawHandlePic(x, y, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarArrowDown);
		// thumb
		thumb = Item_ListBox_ThumbDrawPosition(item);//Item_ListBox_ThumbPosition(item);
		if (thumb > y - SCROLLBAR_SIZE - 1) {
			thumb = y - SCROLLBAR_SIZE - 1;
		}
		DC->drawHandlePic(x, thumb, SCROLLBAR_SIZE, SCROLLBAR_SIZE, DC->Assets.scrollBarThumb);

		// adjust size for item painting
		size = fillRect.h/* - 2*/;
		if (listPtr->elementStyle == LISTBOX_IMAGE) {
			// fit = 0;
			x = fillRect.x + 1;
			y = fillRect.y + 1;
			for (i = listPtr->startPos; i < count; i++) {
				if (i == item->cursorPos) {
					DC->fillRect(x, y, listPtr->elementWidth - 1, listPtr->elementHeight - 1, item->window.outlineColor);
				}

				// always draw at least one
				// which may overdraw the box if it is too small for the element
				image = DC->feederItemImage(item->special, i);
				if (image) {
					DC->drawHandlePic(x+1, y+1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, image);
				}

				if (i == item->cursorPos) {
					DC->drawRect(x, y, listPtr->elementWidth - 1, listPtr->elementHeight - 1, item->window.borderSize, item->window.borderColor);
				}

				listPtr->endPos++;
				size -= listPtr->elementHeight;
				if (size < listPtr->elementHeight) {
					listPtr->drawPadding = size; //listPtr->elementHeight - size;
					break;
				}
				y += listPtr->elementHeight;
				// fit++;
			}
		} else {
			x = fillRect.x /*+ 1*/;
			y = fillRect.y /*+ 1*/;
			for (i = listPtr->startPos; i < count; i++) {
				const char *text;
				// always draw at least one
				// which may overdraw the box if it is too small for the element

				if (listPtr->numColumns > 0) {
					int j, k;
					for (j = 0; j < listPtr->numColumns; j++) {
						text = DC->feederItemText(item->special, i, j, optionalImages, &numOptionalImages);
						if( numOptionalImages > 0 ) {
							for( k = 0; k < numOptionalImages; k++ ) {
								if( optionalImages[k] >= 0 )
									DC->drawHandlePic( x + listPtr->columnInfo[j].pos + k * listPtr->elementHeight + 1,
														y + 1, listPtr->elementHeight - 2, listPtr->elementHeight - 2, optionalImages[k] );
							}
							//DC->drawHandlePic( x + 4 + listPtr->columnInfo[j].pos, y - 1 + listPtr->elementHeight / 2, listPtr->columnInfo[j].width, listPtr->columnInfo[j].width, optionalImage);
						} else if (text) {
							DC->drawText(x + 4 + listPtr->columnInfo[j].pos + item->textalignx, 
								y + listPtr->elementHeight + item->textaligny, item->textscale, item->window.foreColor, text, 0, listPtr->columnInfo[j].maxChars, item->textStyle);
						}
					}
				} else {
					text = DC->feederItemText(item->special, i, 0, optionalImages, &numOptionalImages);
					if( numOptionalImages >= 0 ) {
						//DC->drawHandlePic(x + 4 + listPtr->elementHeight, y, listPtr->columnInfo[j].width, listPtr->columnInfo[j].width, optionalImage);
					} else if (text) {
						DC->drawText(x + 4 + item->textalignx, y + listPtr->elementHeight + item->textaligny, item->textscale, item->window.foreColor, text, 0, 0, item->textStyle);
					}
				}

				if (i == item->cursorPos) {
					DC->fillRect(x, y, fillRect.w - SCROLLBAR_SIZE - 2, listPtr->elementHeight/* - 1*/, item->window.outlineColor);
				}

				size -= listPtr->elementHeight;
				if (size < listPtr->elementHeight) {
					listPtr->drawPadding = size; //listPtr->elementHeight - size;
					break;
				}
				listPtr->endPos++;
				y += listPtr->elementHeight;
				// fit++;
			}
		}
	}
}



#endif

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const double od796Dim=.8,od796One=1.,od796Half=.5;
static const float od796Eight=8.f;
typedef char od796RangeLayout[(sizeof(colorRangeDef_t)==28 && offsetof(colorRangeDef_t,color)==0 && offsetof(colorRangeDef_t,low)==20 && offsetof(colorRangeDef_t,high)==24)?1:-1];
enum { od796Parent=offsetof(itemDef_t,parent),
od796Count=offsetof(itemDef_t,numColors),
od796RangeType=offsetof(itemDef_t,colorRangeType),
od796Style=offsetof(itemDef_t,textStyle),
od796CvarFlags=offsetof(itemDef_t,cvarFlags),
od796Text=offsetof(itemDef_t,text),
od796Scale=offsetof(itemDef_t,textscale),
od796Special=offsetof(itemDef_t,special),
od796Alignment=offsetof(itemDef_t,alignment),
od796AlignX=offsetof(itemDef_t,textalignx),
od796AlignY=offsetof(itemDef_t,textaligny),
od796Flags=offsetof(itemDef_t,window)+offsetof(windowDef_t,flags),
od796Next=offsetof(itemDef_t,window)+offsetof(windowDef_t,nextTime),
od796Color=offsetof(itemDef_t,window)+offsetof(windowDef_t,foreColor),
od796Owner=offsetof(itemDef_t,window)+offsetof(windowDef_t,ownerDraw),
od796OwnerFlags=offsetof(itemDef_t,window)+offsetof(windowDef_t,ownerDrawFlags),
od796Background=offsetof(itemDef_t,window)+offsetof(windowDef_t,background),
od796Alpha=od796Color+12,
od796Green=od796Color+4,
od796Blue=od796Color+8,
od796Ranges=offsetof(itemDef_t,colorRanges)+offsetof(colorRangeDef_t,color),
od796RangeHigh=offsetof(itemDef_t,colorRanges)+offsetof(colorRangeDef_t,high),
od796RectX=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,x),
od796RectY=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,y),
od796RectW=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,w),
od796RectH=offsetof(itemDef_t,window)+offsetof(windowDef_t,rect)+offsetof(rectDef_t,h),
od796X=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,x),
od796W=offsetof(itemDef_t,textRect)+offsetof(rectDef_t,w),
od796FadeAmount=offsetof(menuDef_t,fadeAmount),
od796FadeCycle=offsetof(menuDef_t,fadeCycle),
od796FadeClamp=offsetof(menuDef_t,fadeClamp),
od796Focus=offsetof(menuDef_t,focusColor),
od796Disabled=offsetof(menuDef_t,disableColor),
od796Draw=offsetof(displayContextDef_t,ownerDrawItem),
od796Value=offsetof(displayContextDef_t,getValue),
od796Time=offsetof(displayContextDef_t,realTime) };
__declspec(naked) void Item_OwnerDraw_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x20
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x28]
 TEST ESI,ESI
 JZ od796_300855a6
 MOV EAX,DC
 MOV ECX,dword ptr [EAX + od796Draw]
 TEST ECX,ECX
 JZ od796_300855a6
 PUSH EBX
 PUSH EBP
 PUSH EDI
 MOV EDI,dword ptr [ESI + od796Parent]
 LEA EAX,[ESI + od796Alpha]
 LEA ECX,[ESI + od796Flags]
 MOV EDX,dword ptr [EDI + od796FadeAmount]
 PUSH EDX
 MOV EDX,dword ptr [EDI + od796FadeCycle]
 PUSH 0x1
 PUSH EDX
 LEA EDX,[ESI + od796Next]
 PUSH EDX
 MOV EDX,dword ptr [EDI + od796FadeClamp]
 PUSH EDX
 PUSH EAX
 PUSH ECX
 CALL Fade
 LEA EBP,[ESI + od796Color]
 ADD ESP,0x1c
 MOV EAX,EBP
 MOV ECX,dword ptr [EAX]
 MOV dword ptr [ESP + 0x10],ECX
 MOV EDX,dword ptr [EAX + 0x4]
 MOV dword ptr [ESP + 0x14],EDX
 MOV ECX,dword ptr [EAX + 0x8]
 MOV dword ptr [ESP + 0x18],ECX
 MOV EDX,dword ptr [EAX + 0xc]
 MOV EAX,dword ptr [ESI + od796Count]
 TEST EAX,EAX
 MOV dword ptr [ESP + 0x1c],EDX
 JLE od796_300852d2
 MOV EAX,DC
 MOV EAX,dword ptr [EAX + od796Value]
 TEST EAX,EAX
 MOV dword ptr [ESP + 0x34],EAX
 JZ od796_300852d2
 MOV ECX,dword ptr [ESI + od796RangeType]
 MOV EDX,dword ptr [ESI + od796Owner]
 PUSH ECX
 PUSH EDX
 CALL dword ptr [ESP + 0x3c]
 MOV EBX,dword ptr [ESI + od796Count]
 ADD ESP,0x8
 FSTP dword ptr [ESP + 0x34]
 XOR ECX,ECX
 TEST EBX,EBX
 JLE od796_300852d2
 LEA EDX,[ESI + od796RangeHigh]
od796_3008527d:
 FLD dword ptr [ESP + 0x34]
 FCOMP dword ptr [EDX + -0x4]
 FNSTSW AX
 TEST AH,0x1
 JNZ od796_30085298
 FLD dword ptr [ESP + 0x34]
 FCOMP dword ptr [EDX]
 FNSTSW AX
 TEST AH,0x41
 JNZ od796_300852a2
od796_30085298:
 INC ECX
 ADD EDX,0x1c
 CMP ECX,EBX
 JL od796_3008527d
 JMP od796_300852d2
od796_300852a2:
 LEA EAX,[ECX*0x8 + 0x0]
 SUB EAX,ECX
 LEA ECX,[ESI + EAX*4 + od796Ranges]
 MOV EDX,dword ptr [ESI + EAX*4 + od796Ranges]
 MOV dword ptr [ESP + 0x10],EDX
 MOV EAX,dword ptr [ECX + 0x4]
 MOV dword ptr [ESP + 0x14],EAX
 MOV EDX,dword ptr [ECX + 0x8]
 MOV dword ptr [ESP + 0x18],EDX
 MOV EAX,dword ptr [ECX + 0xc]
 MOV dword ptr [ESP + 0x1c],EAX
od796_300852d2:
 MOV EAX,dword ptr [ESI + od796Flags]
 TEST AL,0x2
 JZ od796_30085375
 TEST EAX,0x8000000
 JZ od796_30085375
 FLD dword ptr [EDI + od796Focus]
 FMUL qword ptr od796Dim
 LEA ECX,[EDI + od796Focus]
 MOV EDX,dword ptr DC
 MOV EAX,0x1b4e81b5
 PUSH ECX
 FSTP dword ptr [ESP + 0x24]
 FLD dword ptr [EDI + od796Focus + 4]
 FMUL qword ptr od796Dim
 FSTP dword ptr [ESP + 0x28]
 FLD dword ptr [EDI + od796Focus + 8]
 FMUL qword ptr od796Dim
 FSTP dword ptr [ESP + 0x2c]
 FLD dword ptr [EDI + od796Focus + 12]
 FMUL qword ptr od796Dim
 FSTP dword ptr [ESP + 0x30]
 MOV EDX,dword ptr [EDX + od796Time]
 IMUL EDX
 SAR EDX,0x3
 MOV EAX,EDX
 SHR EAX,0x1f
 ADD EDX,EAX
 LEA EAX,[ESP + 0x24]
 MOV dword ptr [ESP + 0x38],EDX
 LEA EDX,[ESP + 0x14]
 FILD dword ptr [ESP + 0x38]
 FSIN
 FADD qword ptr od796One
 FMUL qword ptr od796Half
 FSTP dword ptr [ESP]
 PUSH EDX
 PUSH EAX
 PUSH ECX
 JMP od796_30085417
od796_30085375:
 CMP dword ptr [ESI + od796Style],0x1
 JNZ od796_3008541f
 MOV EBX,dword ptr DC
 MOV EAX,0x51eb851f
 MOV ECX,dword ptr [EBX + od796Time]
 IMUL ECX
 SAR EDX,0x6
 MOV ECX,EDX
 SHR ECX,0x1f
 ADD EDX,ECX
 TEST DL,0x1
 JNZ od796_3008541f
 FLD dword ptr [EBP]
 FMUL qword ptr od796Dim
 MOV EAX,0x1b4e81b5
 FSTP dword ptr [ESP + 0x20]
 FLD dword ptr [ESI + od796Green]
 FMUL qword ptr od796Dim
 FSTP dword ptr [ESP + 0x24]
 FLD dword ptr [ESI + od796Blue]
 FMUL qword ptr od796Dim
 FSTP dword ptr [ESP + 0x28]
 FLD dword ptr [ESI + od796Alpha]
 FMUL qword ptr od796Dim
 FSTP dword ptr [ESP + 0x2c]
 MOV ECX,dword ptr [EBX + od796Time]
 IMUL ECX
 SAR EDX,0x3
 MOV EAX,EDX
 PUSH ECX
 SHR EAX,0x1f
 ADD EDX,EAX
 LEA ECX,[ESP + 0x14]
 MOV dword ptr [ESP + 0x38],EDX
 LEA EDX,[ESP + 0x24]
 FILD dword ptr [ESP + 0x38]
 FSIN
 FADD qword ptr od796One
 FMUL qword ptr od796Half
 FSTP dword ptr [ESP]
 PUSH ECX
 PUSH EDX
 PUSH EBP
od796_30085417:
 CALL LerpColor
 ADD ESP,0x10
od796_3008541f:
 TEST byte ptr [ESI + od796CvarFlags],0x3
 JZ od796_30085458
 PUSH 0x1
 PUSH ESI
 CALL Item_EnableShowViaCvar
 ADD ESP,0x8
 TEST EAX,EAX
 JNZ od796_30085458
 ADD EDI,od796Disabled
 MOV EAX,dword ptr [EDI]
 MOV dword ptr [ESP + 0x10],EAX
 MOV ECX,dword ptr [EDI + 0x4]
 MOV dword ptr [ESP + 0x14],ECX
 MOV EDX,dword ptr [EDI + 0x8]
 MOV dword ptr [ESP + 0x18],EDX
 MOV EAX,dword ptr [EDI + 0xc]
 MOV dword ptr [ESP + 0x1c],EAX
od796_30085458:
 MOV EAX,dword ptr [ESI + od796Text]
 POP EDI
 POP EBP
 POP EBX
 TEST EAX,EAX
 JZ od796_3008554e
 PUSH ESI
 CALL Item_Text_Paint
 MOV ECX,dword ptr [ESI + od796Text]
 ADD ESP,0x4
 CMP byte ptr [ECX],0x0
 JZ od796_300854e9
 MOV EDX,dword ptr [ESI + od796Style]
 MOV EAX,dword ptr [ESI + od796Background]
 FLD dword ptr [ESI + od796W]
 FADD dword ptr [ESI + od796X]
 PUSH EDX
 MOV EDX,dword ptr [ESI + od796Scale]
 LEA ECX,[ESP + 0x8]
 PUSH EAX
 MOV EAX,dword ptr [ESI + od796Special]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796Alignment]
 PUSH EDX
 MOV EDX,dword ptr [ESI + od796OwnerFlags]
 PUSH EAX
 MOV EAX,dword ptr [ESI + od796Owner]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796AlignY]
 PUSH EDX
 MOV EDX,dword ptr [ESI + od796RectH]
 PUSH EAX
 MOV EAX,dword ptr [ESI + od796RectW]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796RectY]
 PUSH 0x0
 FADD dword ptr od796Eight
 PUSH EDX
 MOV EDX,dword ptr DC
 PUSH EAX
 PUSH ECX
 PUSH ECX
 FSTP dword ptr [ESP]
 CALL dword ptr [EDX + od796Draw]
 ADD ESP,0x38
 POP ESI
 ADD ESP,0x20
 RET
od796_300854e9:
 MOV EAX,dword ptr [ESI + od796Style]
 MOV ECX,dword ptr [ESI + od796Background]
 PUSH EAX
 MOV EAX,dword ptr [ESI + od796Scale]
 LEA EDX,[ESP + 0x8]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796Special]
 PUSH EDX
 MOV EDX,dword ptr [ESI + od796Alignment]
 PUSH EAX
 MOV EAX,dword ptr [ESI + od796OwnerFlags]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796Owner]
 PUSH EDX
 MOV EDX,dword ptr [ESI + od796AlignY]
 PUSH EAX
 MOV EAX,dword ptr [ESI + od796RectH]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796RectW]
 PUSH EDX
 FLD dword ptr [ESI + od796W]
 MOV EDX,dword ptr [ESI + od796RectY]
 PUSH 0x0
 FADD dword ptr [ESI + od796X]
 PUSH EAX
 MOV EAX,DC
 PUSH ECX
 PUSH EDX
 PUSH ECX
 FSTP dword ptr [ESP]
 CALL dword ptr [EAX + od796Draw]
 ADD ESP,0x38
 POP ESI
 ADD ESP,0x20
 RET
od796_3008554e:
 MOV ECX,dword ptr [ESI + od796Style]
 MOV EDX,dword ptr [ESI + od796Background]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796Scale]
 LEA EAX,[ESP + 0x8]
 PUSH EDX
 MOV EDX,dword ptr [ESI + od796Special]
 PUSH EAX
 MOV EAX,dword ptr [ESI + od796Alignment]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796OwnerFlags]
 PUSH EDX
 MOV EDX,dword ptr [ESI + od796Owner]
 PUSH EAX
 MOV EAX,dword ptr [ESI + od796AlignY]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796AlignX]
 PUSH EDX
 MOV EDX,dword ptr [ESI + od796RectH]
 PUSH EAX
 MOV EAX,dword ptr [ESI + od796RectW]
 PUSH ECX
 MOV ECX,dword ptr [ESI + od796RectY]
 PUSH EDX
 MOV EDX,dword ptr [ESI + od796RectX]
 PUSH EAX
 MOV EAX,DC
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + od796Draw]
 ADD ESP,0x38
od796_300855a6:
 POP ESI
 ADD ESP,0x20
 RET
 }
}
#else
void Item_OwnerDraw_Paint(itemDef_t *item) {
	menuDef_t *parent;

	if (item == NULL) {
		return;
	}

	parent = (menuDef_t*)item->parent;

	if (DC->ownerDrawItem) {
		vec4_t color, lowLight;
		menuDef_t *parent = (menuDef_t*)item->parent;
		Fade(&item->window.flags, &item->window.foreColor[3], parent->fadeClamp, &item->window.nextTime, parent->fadeCycle, qtrue, parent->fadeAmount);
		memcpy(&color, &item->window.foreColor, sizeof(color));
		if (item->numColors > 0 && DC->getValue) {
			// if the value is within one of the ranges then set color to that, otherwise leave at default
			int i;
			float f = DC->getValue(item->window.ownerDraw, item->colorRangeType);	
			for (i = 0; i < item->numColors; i++) {
				if (f >= item->colorRanges[i].low && f <= item->colorRanges[i].high) {
					memcpy(&color, &item->colorRanges[i].color, sizeof(color));
					break;
				}
			}
		}

		if (item->window.flags & WINDOW_HASFOCUS && item->window.flags & WINDOW_FOCUSPULSE) {
			lowLight[0] = 0.8 * parent->focusColor[0]; 
			lowLight[1] = 0.8 * parent->focusColor[1]; 
			lowLight[2] = 0.8 * parent->focusColor[2]; 
			lowLight[3] = 0.8 * parent->focusColor[3]; 
			LerpColor(parent->focusColor,lowLight,color,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
		} else if (item->textStyle == ITEM_TEXTSTYLE_BLINK && !((DC->realTime/BLINK_DIVISOR) & 1)) {
			lowLight[0] = 0.8 * item->window.foreColor[0]; 
			lowLight[1] = 0.8 * item->window.foreColor[1]; 
			lowLight[2] = 0.8 * item->window.foreColor[2]; 
			lowLight[3] = 0.8 * item->window.foreColor[3]; 
			LerpColor(item->window.foreColor,lowLight,color,0.5+0.5*sin(DC->realTime / PULSE_DIVISOR));
		}

		if (item->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(item, CVAR_ENABLE)) {
			memcpy(color, parent->disableColor, sizeof(vec4_t));
		}
		
		// gah wtf indentation!
		if (item->text)
		{
			Item_Text_Paint(item);
			if (item->text[0])
			{
				// +8 is an offset kludge to properly align owner draw items that have text combined with them
				DC->ownerDrawItem(item->textRect.x + item->textRect.w + 8, item->window.rect.y, item->window.rect.w, item->window.rect.h, 0, item->textaligny, item->window.ownerDraw, item->window.ownerDrawFlags, item->alignment, item->special, item->textscale, color, item->window.background, item->textStyle );
			}
			else
			{
				DC->ownerDrawItem(item->textRect.x + item->textRect.w, item->window.rect.y, item->window.rect.w, item->window.rect.h, 0, item->textaligny, item->window.ownerDraw, item->window.ownerDrawFlags, item->alignment, item->special, item->textscale, color, item->window.background, item->textStyle );
			}
		}
		else
		{
			DC->ownerDrawItem(item->window.rect.x, item->window.rect.y, item->window.rect.w, item->window.rect.h, item->textalignx, item->textaligny, item->window.ownerDraw, item->window.ownerDrawFlags, item->alignment, item->special, item->textscale, color, item->window.background, item->textStyle );
		}
	}
}


#endif
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const float ip797Half=.5f;
static const double ip797Angle=0.05235987901687622;
typedef char ip797Layout[(offsetof(itemDef_t,window)==0 && offsetof(rectDef_t,x)==0 && offsetof(rectDef_t,y)==4 && offsetof(rectDef_t,w)==8 && offsetof(rectDef_t,h)==12)?1:-1];
enum { ip797Parent=offsetof(itemDef_t,parent),
ip797Font=offsetof(itemDef_t,font),
ip797CvarFlags=offsetof(itemDef_t,cvarFlags),
ip797SettingFlags=offsetof(itemDef_t,settingFlags),
ip797VoteFlag=offsetof(itemDef_t,voteFlag),
ip797Type=offsetof(itemDef_t,type),
ip797Flags=offsetof(itemDef_t,window)+offsetof(windowDef_t,flags),
ip797Next=offsetof(itemDef_t,window)+offsetof(windowDef_t,nextTime),
ip797OffsetTime=offsetof(itemDef_t,window)+offsetof(windowDef_t,offsetTime),
ip797OwnerFlags=offsetof(itemDef_t,window)+offsetof(windowDef_t,ownerDrawFlags),
ip797ClientX=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectClient)+offsetof(rectDef_t,x),
ip797ClientY=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectClient)+offsetof(rectDef_t,y),
ip797ClientW=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectClient)+offsetof(rectDef_t,w),
ip797ClientH=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectClient)+offsetof(rectDef_t,h),
ip797EffectX=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectEffects)+offsetof(rectDef_t,x),
ip797EffectY=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectEffects)+offsetof(rectDef_t,y),
ip797EffectW=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectEffects)+offsetof(rectDef_t,w),
ip797EffectH=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectEffects)+offsetof(rectDef_t,h),
ip797StepX=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectEffects2)+offsetof(rectDef_t,x),
ip797StepY=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectEffects2)+offsetof(rectDef_t,y),
ip797StepW=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectEffects2)+offsetof(rectDef_t,w),
ip797StepH=offsetof(itemDef_t,window)+offsetof(windowDef_t,rectEffects2)+offsetof(rectDef_t,h),
ip797FontCall=offsetof(displayContextDef_t,textFont),
ip797Time=offsetof(displayContextDef_t,realTime),
ip797Visible=offsetof(displayContextDef_t,ownerDrawVisible),
ip797DrawRect=offsetof(displayContextDef_t,drawRect),
ip797Cycle=offsetof(menuDef_t,fadeCycle),
ip797Clamp=offsetof(menuDef_t,fadeClamp),
ip797Amount=offsetof(menuDef_t,fadeAmount) };
__declspec(naked) void Item_Paint(itemDef_t *item) {
 __asm {
 SUB ESP,0x20
 PUSH EBP
 PUSH ESI
 MOV ESI,dword ptr [ESP + 0x2c]
 TEST ESI,ESI
 MOV EBP,dword ptr [ESI + ip797Parent]
 JZ ip797_300859bd
 MOV EAX,DC
 MOV EAX,dword ptr [EAX + ip797FontCall]
 TEST EAX,EAX
 JZ ip797_300855df
 MOV ECX,dword ptr [ESI + ip797Font]
 PUSH ECX
 CALL EAX
 ADD ESP,0x4
ip797_300855df:
 TEST dword ptr [ESI + ip797Flags],0x10000
 JZ ip797_30085694
 MOV EDX,dword ptr DC
 MOV ECX,dword ptr [ESI + ip797Next]
 MOV EAX,dword ptr [EDX + ip797Time]
 CMP EAX,ECX
 JLE ip797_30085694
 FLD dword ptr [ESI + ip797ClientW]
 FMUL dword ptr ip797Half
 FLD dword ptr [ESI + ip797ClientH]
 FMUL dword ptr ip797Half
 MOV ECX,dword ptr [ESI + ip797OffsetTime]
 PUSH ESI
 ADD ECX,EAX
 MOV dword ptr [ESI + ip797Next],ECX
 FSTP dword ptr [ESP + 0x18]
 FLD ST(0)
 FADD dword ptr [ESI + ip797ClientX]
 FSUB dword ptr [ESI + ip797EffectX]
 FSTP dword ptr [ESP + 0x14]
 FLD dword ptr [ESP + 0x18]
 FADD dword ptr [ESI + ip797ClientY]
 FSUB dword ptr [ESI + ip797EffectY]
 FSTP dword ptr [ESP + 0xc]
 FLD qword ptr ip797Angle
 FCOS
 FSTP dword ptr [ESP + 0x30]
 FLD qword ptr ip797Angle
 FSIN
 FSTP dword ptr [ESP + 0x10]
 FLD dword ptr [ESP + 0x30]
 FMUL dword ptr [ESP + 0x14]
 FLD dword ptr [ESP + 0x10]
 FMUL dword ptr [ESP + 0xc]
 FSUBP ST(1),ST(0)
 FADD dword ptr [ESI + ip797EffectX]
 FSUB ST(0),ST(1)
 FSTP dword ptr [ESI + ip797ClientX]
 FSTP ST(0)
 FLD dword ptr [ESP + 0x30]
 FMUL dword ptr [ESP + 0xc]
 FLD dword ptr [ESP + 0x10]
 FMUL dword ptr [ESP + 0x14]
 FADDP ST(1),ST(0)
 FADD dword ptr [ESI + ip797EffectY]
 FSUB dword ptr [ESP + 0x18]
 FSTP dword ptr [ESI + ip797ClientY]
 CALL Item_UpdatePosition
 ADD ESP,0x4
ip797_30085694:
 MOV EAX,dword ptr [ESI + ip797Flags]
 TEST AH,0x1
 JZ ip797_3008581a
 MOV EDX,dword ptr DC
 MOV ECX,dword ptr [ESI + ip797Next]
 MOV EAX,dword ptr [EDX + ip797Time]
 CMP EAX,ECX
 JLE ip797_3008581a
 FLD dword ptr [ESI + ip797ClientX]
 FCOMP dword ptr [ESI + ip797EffectX]
 MOV ECX,dword ptr [ESI + ip797OffsetTime]
 PUSH EDI
 ADD ECX,EAX
 XOR EDI,EDI
 MOV dword ptr [ESI + ip797Next],ECX
 FNSTSW AX
 TEST AH,0x40
 JNZ ip797_30085710
 FLD dword ptr [ESI + ip797ClientX]
 FCOMP dword ptr [ESI + ip797EffectX]
 FNSTSW AX
 TEST AH,0x1
 JZ ip797_300856f7
 FLD dword ptr [ESI + ip797StepX]
 FADD dword ptr [ESI + ip797ClientX]
 FST dword ptr [ESI + ip797ClientX]
 FCOMP dword ptr [ESI + ip797EffectX]
 FNSTSW AX
 TEST AH,0x41
 JNZ ip797_30085715
 MOV EDX,dword ptr [ESI + ip797EffectX]
 MOV dword ptr [ESI + ip797ClientX],EDX
 JMP ip797_30085710
ip797_300856f7:
 FLD dword ptr [ESI + ip797ClientX]
 FSUB dword ptr [ESI + ip797StepX]
 FST dword ptr [ESI + ip797ClientX]
 FCOMP dword ptr [ESI + ip797EffectX]
 FNSTSW AX
 TEST AH,0x1
 JZ ip797_30085715
 MOV EAX,dword ptr [ESI + ip797EffectX]
 MOV dword ptr [ESI + ip797ClientX],EAX
ip797_30085710:
 MOV EDI,0x1
ip797_30085715:
 FLD dword ptr [ESI + ip797ClientY]
 FCOMP dword ptr [ESI + ip797EffectY]
 FNSTSW AX
 TEST AH,0x40
 JNZ ip797_30085763
 FLD dword ptr [ESI + ip797ClientY]
 FCOMP dword ptr [ESI + ip797EffectY]
 FNSTSW AX
 TEST AH,0x1
 JZ ip797_3008574a
 FLD dword ptr [ESI + ip797StepY]
 FADD dword ptr [ESI + ip797ClientY]
 FST dword ptr [ESI + ip797ClientY]
 FCOMP dword ptr [ESI + ip797EffectY]
 FNSTSW AX
 TEST AH,0x41
 JNZ ip797_30085764
 MOV ECX,dword ptr [ESI + ip797EffectY]
 MOV dword ptr [ESI + ip797ClientY],ECX
 JMP ip797_30085763
ip797_3008574a:
 FLD dword ptr [ESI + ip797ClientY]
 FSUB dword ptr [ESI + ip797StepY]
 FST dword ptr [ESI + ip797ClientY]
 FCOMP dword ptr [ESI + ip797EffectY]
 FNSTSW AX
 TEST AH,0x1
 JZ ip797_30085764
 MOV EDX,dword ptr [ESI + ip797EffectY]
 MOV dword ptr [ESI + ip797ClientY],EDX
ip797_30085763:
 INC EDI
ip797_30085764:
 FLD dword ptr [ESI + ip797ClientW]
 FCOMP dword ptr [ESI + ip797EffectW]
 FNSTSW AX
 TEST AH,0x40
 JNZ ip797_300857b2
 FLD dword ptr [ESI + ip797ClientW]
 FCOMP dword ptr [ESI + ip797EffectW]
 FNSTSW AX
 TEST AH,0x1
 JZ ip797_30085799
 FLD dword ptr [ESI + ip797StepW]
 FADD dword ptr [ESI + ip797ClientW]
 FST dword ptr [ESI + ip797ClientW]
 FCOMP dword ptr [ESI + ip797EffectW]
 FNSTSW AX
 TEST AH,0x41
 JNZ ip797_300857b3
 MOV EAX,dword ptr [ESI + ip797EffectW]
 MOV dword ptr [ESI + ip797ClientW],EAX
 JMP ip797_300857b2
ip797_30085799:
 FLD dword ptr [ESI + ip797ClientW]
 FSUB dword ptr [ESI + ip797StepW]
 FST dword ptr [ESI + ip797ClientW]
 FCOMP dword ptr [ESI + ip797EffectW]
 FNSTSW AX
 TEST AH,0x1
 JZ ip797_300857b3
 MOV ECX,dword ptr [ESI + ip797EffectW]
 MOV dword ptr [ESI + ip797ClientW],ECX
ip797_300857b2:
 INC EDI
ip797_300857b3:
 FLD dword ptr [ESI + ip797ClientH]
 FCOMP dword ptr [ESI + ip797EffectH]
 FNSTSW AX
 TEST AH,0x40
 JNZ ip797_30085801
 FLD dword ptr [ESI + ip797ClientH]
 FCOMP dword ptr [ESI + ip797EffectH]
 FNSTSW AX
 TEST AH,0x1
 JZ ip797_300857e8
 FLD dword ptr [ESI + ip797StepH]
 FADD dword ptr [ESI + ip797ClientH]
 FST dword ptr [ESI + ip797ClientH]
 FCOMP dword ptr [ESI + ip797EffectH]
 FNSTSW AX
 TEST AH,0x41
 JNZ ip797_30085802
 MOV EDX,dword ptr [ESI + ip797EffectH]
 MOV dword ptr [ESI + ip797ClientH],EDX
 JMP ip797_30085801
ip797_300857e8:
 FLD dword ptr [ESI + ip797ClientH]
 FSUB dword ptr [ESI + ip797StepH]
 FST dword ptr [ESI + ip797ClientH]
 FCOMP dword ptr [ESI + ip797EffectH]
 FNSTSW AX
 TEST AH,0x1
 JZ ip797_30085802
 MOV EAX,dword ptr [ESI + ip797EffectH]
 MOV dword ptr [ESI + ip797ClientH],EAX
ip797_30085801:
 INC EDI
ip797_30085802:
 PUSH ESI
 CALL Item_UpdatePosition
 ADD ESP,0x4
 CMP EDI,0x4
 POP EDI
 JNZ ip797_3008581a
 MOV EAX,dword ptr [ESI + ip797Flags]
 AND AH,0xfe
 MOV dword ptr [ESI + ip797Flags],EAX
ip797_3008581a:
 MOV ECX,dword ptr [ESI + ip797OwnerFlags]
 TEST ECX,ECX
 JZ ip797_30085844
 MOV EDX,dword ptr DC
 MOV EAX,dword ptr [EDX + ip797Visible]
 TEST EAX,EAX
 JZ ip797_30085844
 PUSH ECX
 CALL EAX
 ADD ESP,0x4
 TEST EAX,EAX
 MOV EAX,dword ptr [ESI + ip797Flags]
 JNZ ip797_3008583f
 AND AL,0xfa
 JMP ip797_30085841
ip797_3008583f:
 OR AL,0x4
ip797_30085841:
 MOV dword ptr [ESI + ip797Flags],EAX
ip797_30085844:
 TEST byte ptr [ESI + ip797CvarFlags],0xc
 JZ ip797_30085860
 PUSH 0x4
 PUSH ESI
 CALL Item_EnableShowViaCvar
 ADD ESP,0x8
 TEST EAX,EAX
 JZ ip797_300859bd
ip797_30085860:
 TEST byte ptr [ESI + ip797SettingFlags],0x3
 JZ ip797_3008587c
 PUSH 0x0
 PUSH ESI
 CALL Item_SettingShow
 ADD ESP,0x8
 TEST EAX,EAX
 JZ ip797_300859bd
ip797_3008587c:
 MOV EAX,dword ptr [ESI + ip797VoteFlag]
 TEST EAX,EAX
 JZ ip797_30085899
 PUSH 0x1
 PUSH ESI
 CALL Item_SettingShow
 ADD ESP,0x8
 TEST EAX,EAX
 JZ ip797_300859bd
ip797_30085899:
 TEST byte ptr [ESI + ip797Flags],0x4
 JZ ip797_300859bd
 FILD dword ptr [EBP + ip797Cycle]
 MOV EAX,dword ptr [EBP + ip797Clamp]
 PUSH ECX
 MOV ECX,dword ptr [EBP + ip797Amount]
 FSTP dword ptr [ESP]
 PUSH EAX
 PUSH ECX
 PUSH ESI
 CALL Window_Paint
 MOV EAX,debugMode
 ADD ESP,0x10
 TEST EAX,EAX
 JZ ip797_30085917
 PUSH ESI
 CALL Item_CorrectedTextRect
 LEA EDX,[ESP + 0x1c]
 MOV dword ptr [ESP + 0x28],0x3f800000
 MOV dword ptr [ESP + 0x20],0x3f800000
 MOV dword ptr [ESP + 0x24],0x0
 MOV dword ptr [ESP + 0x1c],0x0
 MOV ECX,dword ptr [EAX + 0xc]
 PUSH EDX
 MOV EDX,dword ptr [EAX + 0x8]
 PUSH 0x3f800000
 PUSH ECX
 MOV ECX,dword ptr [EAX + 0x4]
 PUSH EDX
 MOV EDX,dword ptr [EAX]
 MOV EAX,DC
 PUSH ECX
 PUSH EDX
 CALL dword ptr [EAX + ip797DrawRect]
 ADD ESP,0x1c
ip797_30085917:
 MOV EAX,dword ptr [ESI + ip797Type]
 CMP EAX,0x10
 JA ip797_300859bd
 CMP EAX,0
 JE ip797_3008593c
 CMP EAX,1
 JE ip797_3008593c
 CMP EAX,2
 JE ip797_300859bd
 CMP EAX,3
 JE ip797_3008594b
 CMP EAX,4
 JE ip797_3008595a
 CMP EAX,5
 JE ip797_300859bd
 CMP EAX,6
 JE ip797_30085969
 CMP EAX,7
 JE ip797_30085978
 CMP EAX,8
 JE ip797_3008592d
 CMP EAX,9
 JE ip797_3008595a
 CMP EAX,10
 JE ip797_300859b4
 CMP EAX,11
 JE ip797_30085987
 CMP EAX,12
 JE ip797_30085996
 CMP EAX,13
 JE ip797_300859a5
 CMP EAX,14
 JE ip797_30085978
 CMP EAX,15
 JE ip797_3008593c
 CMP EAX,16
 JE ip797_3008594b
 JMP ip797_300859bd
ip797_3008592d:
 PUSH ESI
 CALL Item_OwnerDraw_Paint
 ADD ESP,0x4
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
ip797_3008593c:
 PUSH ESI
 CALL Item_Text_Paint
 ADD ESP,0x4
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
ip797_3008594b:
 PUSH ESI
 CALL Item_CheckBox_Paint
 ADD ESP,0x4
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
ip797_3008595a:
 PUSH ESI
 CALL Item_TextField_Paint
 ADD ESP,0x4
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
ip797_30085969:
 PUSH ESI
 CALL Item_ListBox_Paint
 ADD ESP,0x4
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
ip797_30085978:
 PUSH ESI
 CALL Item_Model_Paint
 ADD ESP,0x4
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
ip797_30085987:
 PUSH ESI
 CALL Item_YesNo_Paint
 ADD ESP,0x4
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
ip797_30085996:
 PUSH ESI
 CALL Item_Multi_Paint
 ADD ESP,0x4
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
ip797_300859a5:
 PUSH ESI
 CALL Item_Bind_Paint
 ADD ESP,0x4
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
ip797_300859b4:
 PUSH ESI
 CALL Item_Slider_Paint
 ADD ESP,0x4
ip797_300859bd:
 POP ESI
 POP EBP
 ADD ESP,0x20
 RET
 }
}
#else
void Item_Paint(itemDef_t *item) {
  vec4_t red;
  menuDef_t *parent = (menuDef_t*)item->parent;
  red[0] = red[3] = 1;
  red[1] = red[2] = 0;

  if (item == NULL) {
    return;
  }

	if ( DC->textFont ) {
		DC->textFont( item->font );
	}

  if (item->window.flags & WINDOW_ORBITING) {
    if (DC->realTime > item->window.nextTime) {
      float rx, ry, a, c, s, w, h;
      
      item->window.nextTime = DC->realTime + item->window.offsetTime;
      // translate
      w = item->window.rectClient.w / 2;
      h = item->window.rectClient.h / 2;
      rx = item->window.rectClient.x + w - item->window.rectEffects.x;
      ry = item->window.rectClient.y + h - item->window.rectEffects.y;
      a = 3 * M_PI / 180;
  	  c = cos(a);
      s = sin(a);
      item->window.rectClient.x = (rx * c - ry * s) + item->window.rectEffects.x - w;
      item->window.rectClient.y = (rx * s + ry * c) + item->window.rectEffects.y - h;
      Item_UpdatePosition(item);

    }
  }


  if (item->window.flags & WINDOW_INTRANSITION) {
    if (DC->realTime > item->window.nextTime) {
      int done = 0;
      item->window.nextTime = DC->realTime + item->window.offsetTime;
			// transition the x,y
			if (item->window.rectClient.x == item->window.rectEffects.x) {
				done++;
			} else {
				if (item->window.rectClient.x < item->window.rectEffects.x) {
					item->window.rectClient.x += item->window.rectEffects2.x;
					if (item->window.rectClient.x > item->window.rectEffects.x) {
						item->window.rectClient.x = item->window.rectEffects.x;
						done++;
					}
				} else {
					item->window.rectClient.x -= item->window.rectEffects2.x;
					if (item->window.rectClient.x < item->window.rectEffects.x) {
						item->window.rectClient.x = item->window.rectEffects.x;
						done++;
					}
				}
			}
			if (item->window.rectClient.y == item->window.rectEffects.y) {
				done++;
			} else {
				if (item->window.rectClient.y < item->window.rectEffects.y) {
					item->window.rectClient.y += item->window.rectEffects2.y;
					if (item->window.rectClient.y > item->window.rectEffects.y) {
						item->window.rectClient.y = item->window.rectEffects.y;
						done++;
					}
				} else {
					item->window.rectClient.y -= item->window.rectEffects2.y;
					if (item->window.rectClient.y < item->window.rectEffects.y) {
						item->window.rectClient.y = item->window.rectEffects.y;
						done++;
					}
				}
			}
			if (item->window.rectClient.w == item->window.rectEffects.w) {
				done++;
			} else {
				if (item->window.rectClient.w < item->window.rectEffects.w) {
					item->window.rectClient.w += item->window.rectEffects2.w;
					if (item->window.rectClient.w > item->window.rectEffects.w) {
						item->window.rectClient.w = item->window.rectEffects.w;
						done++;
					}
				} else {
					item->window.rectClient.w -= item->window.rectEffects2.w;
					if (item->window.rectClient.w < item->window.rectEffects.w) {
						item->window.rectClient.w = item->window.rectEffects.w;
						done++;
					}
				}
			}
			if (item->window.rectClient.h == item->window.rectEffects.h) {
				done++;
			} else {
				if (item->window.rectClient.h < item->window.rectEffects.h) {
					item->window.rectClient.h += item->window.rectEffects2.h;
					if (item->window.rectClient.h > item->window.rectEffects.h) {
						item->window.rectClient.h = item->window.rectEffects.h;
						done++;
					}
				} else {
					item->window.rectClient.h -= item->window.rectEffects2.h;
					if (item->window.rectClient.h < item->window.rectEffects.h) {
						item->window.rectClient.h = item->window.rectEffects.h;
						done++;
					}
				}
			}

      Item_UpdatePosition(item);

      if (done == 4) {
        item->window.flags &= ~WINDOW_INTRANSITION;
      }

    }
  }

	if (item->window.ownerDrawFlags && DC->ownerDrawVisible) {
		if (!DC->ownerDrawVisible(item->window.ownerDrawFlags)) {
			item->window.flags &= ~(WINDOW_VISIBLE | WINDOW_MOUSEOVER);
		} else {
			item->window.flags |= WINDOW_VISIBLE;
		}
	}

	if (item->cvarFlags & (CVAR_SHOW | CVAR_HIDE)) {
		if (!Item_EnableShowViaCvar(item, CVAR_SHOW)) {
			return;
		}
	}

	// OSP
	if((item->settingFlags & (SVS_ENABLED_SHOW | SVS_DISABLED_SHOW)) && !Item_SettingShow(item, qfalse)) {
		return;
	}
	if(item->voteFlag != 0 && !Item_SettingShow(item, qtrue)) {
		return;
	}

	if (item->window.flags & WINDOW_TIMEDVISIBLE) {
	}

	if (!(item->window.flags & WINDOW_VISIBLE)) {
		return;
	}

	// paint the rect first.. 
	Window_Paint(&item->window, parent->fadeAmount , parent->fadeClamp, parent->fadeCycle);

	if (debugMode) {
		vec4_t color;
		rectDef_t *r = Item_CorrectedTextRect(item);
		color[1] = color[3] = 1;
		color[0] = color[2] = 0;
		DC->drawRect(r->x, r->y, r->w, r->h, 1, color);
	}

	//DC->drawRect(item->window.rect.x, item->window.rect.y, item->window.rect.w, item->window.rect.h, 1, red);

	switch (item->type) {
		case ITEM_TYPE_OWNERDRAW:
			Item_OwnerDraw_Paint(item);
			break;
		case ITEM_TYPE_TEXT:
		case ITEM_TYPE_BUTTON:
		case ITEM_TYPE_TIMEOUT_COUNTER:
			Item_Text_Paint(item);
			break;
		case ITEM_TYPE_RADIOBUTTON:
			break;
		case ITEM_TYPE_CHECKBOX:
		case ITEM_TYPE_TRICHECKBOX:
			Item_CheckBox_Paint(item);
			break;
		case ITEM_TYPE_EDITFIELD:
		case ITEM_TYPE_NUMERICFIELD:
			Item_TextField_Paint(item);
			break;
		case ITEM_TYPE_COMBO:
			break;
		case ITEM_TYPE_LISTBOX:
			Item_ListBox_Paint(item);
			break;
//		case ITEM_TYPE_IMAGE:
//			Item_Image_Paint(item);
//			break;
  		case ITEM_TYPE_MENUMODEL:
			Item_Model_Paint(item);
			break;
		case ITEM_TYPE_MODEL:
			Item_Model_Paint(item);
			break;
		case ITEM_TYPE_YESNO:
			Item_YesNo_Paint(item);
			break;
		case ITEM_TYPE_MULTI:
			Item_Multi_Paint(item);
			break;
		case ITEM_TYPE_BIND:
			Item_Bind_Paint(item);
			break;
		case ITEM_TYPE_SLIDER:
			Item_Slider_Paint(item);
			break;
		default:
			break;
	}
}

#endif
void Menu_Init(menuDef_t *menu) {
	memset(menu, 0, sizeof(menuDef_t));
	menu->cursorItem = -1;
	menu->fadeAmount = DC->Assets.fadeAmount;
	menu->fadeClamp = DC->Assets.fadeClamp;
	menu->fadeCycle = DC->Assets.fadeCycle;
	// START - TAT 9/16/2002
	// by default, do NOT use item hotkey mode
	menu->itemHotkeyMode = qfalse;
	// END - TAT 9/16/2002
	Window_Init(&menu->window);
}

itemDef_t *Menu_GetFocusedItem(menuDef_t *menu) {
  int i;
  if (menu) {
    for (i = 0; i < menu->itemCount; i++) {
      if (menu->items[i]->window.flags & WINDOW_HASFOCUS) {
        return menu->items[i];
      }
    }
  }
  return NULL;
}

menuDef_t *Menu_GetFocused() {
  int i;
  for (i = 0; i < menuCount; i++) {
    if (Menus[i].window.flags & WINDOW_HASFOCUS && Menus[i].window.flags & WINDOW_VISIBLE) {
      return &Menus[i];
    }
  }
  return NULL;
}

void Menu_ScrollFeeder(menuDef_t *menu, int feeder, qboolean down) {
	if (menu) {
		int i;
    for (i = 0; i < menu->itemCount; i++) {
			if (menu->items[i]->special == feeder) {
				Item_ListBox_HandleKey(menu->items[i], (down) ? K_DOWNARROW : K_UPARROW, qtrue, qtrue);
				return;
			}
		}
	}
}



void Menu_SetFeederSelection(menuDef_t *menu, int feeder, int index, const char *name) {
	if (menu == NULL) {
		if (name == NULL) {
			menu = Menu_GetFocused();
		} else {
			menu = Menus_FindByName(name);
		}
	}

	if (menu) {
		int i;
    for (i = 0; i < menu->itemCount; i++) {
			if (menu->items[i]->special == feeder) {
				if (index == 0) {
					listBoxDef_t *listPtr = (listBoxDef_t*)menu->items[i]->typeData;
					listPtr->cursorPos = 0;
					listPtr->startPos = 0;
				}
				menu->items[i]->cursorPos = index;
				DC->feederSelection(menu->items[i]->special, menu->items[i]->cursorPos);
				return;
			}
		}
	}
}

qboolean Menus_AnyFullScreenVisible() {
  int i;
  for (i = 0; i < menuCount; i++) {
    if (Menus[i].window.flags & WINDOW_VISIBLE && Menus[i].fullScreen) {
			return qtrue;
    }
  }
  return qfalse;
}

menuDef_t *Menus_ActivateByName(const char *p, qboolean modalStack) {
	int i;
	menuDef_t *m = NULL;
	menuDef_t *focus = Menu_GetFocused();
	for (i = 0; i < menuCount; i++) {
		if (Q_stricmp(Menus[i].window.name, p) == 0) {
			m = &Menus[i];
			Menus_Activate(m);
			if (modalStack && m->window.flags & WINDOW_MODAL) {
				if (modalMenuCount >= MAX_MODAL_MENUS)
					Com_Error(ERR_DROP, "MAX_MODAL_MENUS exceeded\n");
				modalMenuStack[modalMenuCount++] = focus;
			}
			break;	// Arnout: found it, don't continue searching as we might unfocus the menu we just activated again.
		} else {
			Menus[i].window.flags &= ~(WINDOW_HASFOCUS | WINDOW_MOUSEOVER);
		}
	}
	Display_CloseCinematics();
	return m;
}


void Item_Init(itemDef_t *item) {
	memset(item, 0, sizeof(itemDef_t));
	item->textscale = 0.55f;

	// default hotkey to -1
	item->hotkey = -1;

	Window_Init(&item->window);
}

void Menu_HandleMouseMove(menuDef_t *menu, float x, float y) {
	int i, pass;
	qboolean focusSet = qfalse;

	itemDef_t *overItem;

	if (menu == NULL) {
		return;
	}

	if (!(menu->window.flags & (WINDOW_VISIBLE | WINDOW_FORCED))) {
		return;
	}

	if (itemCapture) {
		if ( itemCapture->type == ITEM_TYPE_LISTBOX ) {
			// NERVE - SMF - lose capture if out of client rect
			if ( !Rect_ContainsPoint( &itemCapture->window.rect, x, y) ) {
				Item_StopCapture(itemCapture);
				itemCapture = NULL;
				captureFunc = NULL;
				captureData = NULL;
			}

		}
		//Item_MouseMove(itemCapture, x, y);
		return;
	}

	if (g_waitingForKey || g_editingField) {
		return;
	}

	// FIXME: this is the whole issue of focus vs. mouse over.. 
	// need a better overall solution as i don't like going through everything twice
	for (pass = 0; pass < 2; pass++) {
		for (i = 0; i < menu->itemCount; i++) {
		  // turn off focus each item
		  // menu->items[i].window.flags &= ~WINDOW_HASFOCUS;

			if (!(menu->items[i]->window.flags & (WINDOW_VISIBLE | WINDOW_FORCED))) {
				continue;
			}

			// items can be enabled and disabled based on cvars
			if (menu->items[i]->cvarFlags & (CVAR_ENABLE | CVAR_DISABLE) && !Item_EnableShowViaCvar(menu->items[i], CVAR_ENABLE)) {
				continue;
			}

			if (menu->items[i]->cvarFlags & (CVAR_SHOW | CVAR_HIDE) && !Item_EnableShowViaCvar(menu->items[i], CVAR_SHOW)) {
				continue;
			}

			// OSP - server settings too
			if((menu->items[i]->settingFlags & (SVS_ENABLED_SHOW | SVS_DISABLED_SHOW)) && !Item_SettingShow(menu->items[i], qfalse)) {
				continue;
			}
			if(menu->items[i]->voteFlag != 0 && !Item_SettingShow(menu->items[i], qtrue)) {
				continue;
			}


			if (Rect_ContainsPoint(&menu->items[i]->window.rect, x, y)) {
				if (pass == 1) {
					overItem = menu->items[i];
					if (overItem->type == ITEM_TYPE_TEXT && overItem->text) {
						if (!Rect_ContainsPoint(Item_CorrectedTextRect(overItem), x, y)) {
							continue;
						}
					}
					// if we are over an item
					if( IsVisible(overItem->window.flags) ) {
						// different one
						Item_MouseEnter(overItem, x, y);
						// Item_SetMouseOver(overItem, qtrue);

						// if item is not a decoration see if it can take focus
						if (!focusSet) {
							focusSet = Item_SetFocus(overItem, x, y);
						}
					}
				}
			} else if (menu->items[i]->window.flags & WINDOW_MOUSEOVER) {
				Item_MouseLeave(menu->items[i]);
				Item_SetMouseOver(menu->items[i], qfalse);
			}
		}
	}
}

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
static const char mp801Tooltips[]="ui_showtooltips";
static const float mp801Zero=0.f;
enum {
 mp801M0=offsetof(menuDef_t,window.rect.x),
 mp801M4=offsetof(menuDef_t,window.rect.y),
 mp801M8=offsetof(menuDef_t,window.rect.w),
 mp801M40=offsetof(menuDef_t,window.ownerDrawFlags),
 mp801M48=offsetof(menuDef_t,window.flags),
 mp801M514=offsetof(menuDef_t,items),
 mp801Mc=offsetof(menuDef_t,window.rect.h),
 mp801Mbc=offsetof(menuDef_t,fullScreen),
 mp801Mb4=offsetof(menuDef_t,window.background),
 mp801Mcc=offsetof(menuDef_t,fadeCycle),
 mp801Md0=offsetof(menuDef_t,fadeClamp),
 mp801Md4=offsetof(menuDef_t,fadeAmount),
 mp801Mc0=offsetof(menuDef_t,itemCount),
 mp801Mec=offsetof(menuDef_t,openTime),
 mp801Me8=offsetof(menuDef_t,timeout),
 mp801Mf0=offsetof(menuDef_t,onTimeout),
 mp801I48=offsetof(itemDef_t,window.flags),
 mp801I270=offsetof(itemDef_t,toolTipData),
 mp801Ie8=offsetof(itemDef_t,text),
 mp801D8=offsetof(displayContextDef_t,drawHandlePic),
 mp801D40=offsetof(displayContextDef_t,drawRect),
 mp801D64=offsetof(displayContextDef_t,ownerDrawVisible),
 mp801D74=offsetof(displayContextDef_t,getCVarValue),
 mp801D120=offsetof(displayContextDef_t,realTime),
 mp801Frame=0x288+sizeof(itemDef_t),
 mp801ArgMenu=0x294+sizeof(itemDef_t),
 mp801ArgForce=0x29c+sizeof(itemDef_t),
 mp801TemporaryParent=0x2c+offsetof(itemDef_t,parent)
};
typedef char mp801WindowBase[(offsetof(menuDef_t,window)==0)?1:-1];
__declspec(naked) void Menu_Paint(menuDef_t *menu,qboolean forcePaint) {
 __asm {
 SUB ESP,mp801Frame
 PUSH EBX
 PUSH ESI
 MOV ESI,dword ptr [ESP + mp801ArgMenu]
 XOR EBX,EBX
 TEST ESI,ESI
 PUSH EDI
 MOV dword ptr [ESP + 0xc],EBX
 JZ mp801_30085d39
 MOV AL,byte ptr [ESI + mp801M48]
 MOV EDI,dword ptr [ESP + mp801ArgForce]
 TEST AL,0x4
 JNZ mp801_30085b94
 TEST EDI,EDI
 JZ mp801_30085d39
mp801_30085b94:
 MOV ECX,dword ptr [ESI + mp801M40]
 TEST ECX,ECX
 JZ mp801_30085bb5
 MOV EAX,[DC]
 MOV EAX,dword ptr [EAX + mp801D64]
 TEST EAX,EAX
 JZ mp801_30085bb5
 PUSH ECX
 CALL EAX
 ADD ESP,0x4
 TEST EAX,EAX
 JZ mp801_30085d39
mp801_30085bb5:
 TEST EDI,EDI
 JZ mp801_30085bc0
 OR dword ptr [ESI + mp801M48],0x100000
mp801_30085bc0:
 MOV EAX,dword ptr [ESI + mp801Mbc]
 TEST EAX,EAX
 JZ mp801_30085beb
 MOV ECX,dword ptr [ESI + mp801Mb4]
 MOV EDX,dword ptr [DC]
 PUSH ECX
 PUSH 0x43f00000
 PUSH 0x44200000
 PUSH 0x0
 PUSH 0x0
 CALL dword ptr [EDX + mp801D8]
 ADD ESP,0x14
mp801_30085beb:
 FILD dword ptr [ESI + mp801Mcc]
 MOV EAX,dword ptr [ESI + mp801Md0]
 PUSH EBP
 PUSH ECX
 MOV ECX,dword ptr [ESI + mp801Md4]
 FSTP dword ptr [ESP]
 PUSH EAX
 PUSH ECX
 PUSH ESI
 CALL Window_Paint
 MOV EAX,dword ptr [ESI + mp801Mc0]
 ADD ESP,0x10
 XOR EBP,EBP
 TEST EAX,EAX
 JLE mp801_30085c49
 LEA EDI,[ESI + mp801M514]
 MOV BL,0x1
mp801_30085c21:
 MOV EDX,dword ptr [EDI]
 PUSH EDX
 CALL Item_Paint
 MOV EAX,dword ptr [EDI]
 ADD ESP,0x4
 TEST byte ptr [EAX + mp801I48],BL
 JZ mp801_30085c37
 MOV dword ptr [ESP + 0x10],EAX
mp801_30085c37:
 MOV EAX,dword ptr [ESI + mp801Mc0]
 INC EBP
 ADD EDI,0x4
 CMP EBP,EAX
 JL mp801_30085c21
 MOV EBX,dword ptr [ESP + 0x10]
mp801_30085c49:
 MOV EAX,[DC]
 PUSH OFFSET mp801Tooltips
 CALL dword ptr [EAX + mp801D74]
 FCOMP dword ptr [mp801Zero]
 ADD ESP,0x4
 POP EBP
 FNSTSW AX
 TEST AH,0x40
 JNZ mp801_30085c8d
 TEST EBX,EBX
 JZ mp801_30085c8d
 MOV EAX,dword ptr [EBX + mp801I270]
 TEST EAX,EAX
 JZ mp801_30085c8d
 MOV ECX,dword ptr [EAX + mp801Ie8]
 TEST ECX,ECX
 JZ mp801_30085c8d
 CMP byte ptr [ECX],0x0
 JZ mp801_30085c8d
 PUSH EAX
 CALL Item_Paint
 ADD ESP,0x4
mp801_30085c8d:
 MOV EDX,dword ptr [ESI + mp801Mec]
 TEST EDX,EDX
 JNZ mp801_30085cab
 MOV ECX,dword ptr [DC]
 MOV EDX,dword ptr [ECX + mp801D120]
 MOV dword ptr [ESI + mp801Mec],EDX
 JMP mp801_30085cec
mp801_30085cab:
 TEST byte ptr [ESI + mp801M48],0x4
 JZ mp801_30085cec
 MOV ECX,dword ptr [ESI + mp801Me8]
 TEST ECX,ECX
 JLE mp801_30085cec
 MOV EAX,dword ptr [ESI + mp801Mf0]
 TEST EAX,EAX
 JZ mp801_30085cec
 ADD ECX,EDX
 MOV EDX,dword ptr [DC]
 CMP ECX,dword ptr [EDX + mp801D120]
 JG mp801_30085cec
 PUSH EAX
 LEA EAX,[ESP + 0x24]
 PUSH 0x0
 PUSH EAX
 MOV dword ptr [ESP + mp801TemporaryParent],ESI
 CALL Item_RunScript
 ADD ESP,0xc
mp801_30085cec:
 MOV EAX,[debugMode]
 TEST EAX,EAX
 JZ mp801_30085d39
 MOV EDX,dword ptr [ESI + mp801Mc]
 MOV EAX,dword ptr [ESI + mp801M8]
 LEA ECX,[ESP + 0x10]
 MOV dword ptr [ESP + 0x1c],0x3f800000
 PUSH ECX
 MOV ECX,dword ptr [ESI + mp801M4]
 PUSH 0x3f800000
 PUSH EDX
 MOV EDX,dword ptr [ESI + mp801M0]
 PUSH EAX
 MOV EAX,[DC]
 PUSH ECX
 PUSH EDX
 MOV dword ptr [ESP + 0x30],0x3f800000
 MOV dword ptr [ESP + 0x28],0x3f800000
 MOV dword ptr [ESP + 0x2c],0x0
 CALL dword ptr [EAX + mp801D40]
 ADD ESP,0x18
mp801_30085d39:
 POP EDI
 POP ESI
 POP EBX
 ADD ESP,mp801Frame
 RET
 }
}
#else
void Menu_Paint(menuDef_t *menu, qboolean forcePaint) {
	int i;
	itemDef_t *item = NULL;


	if (menu == NULL) {
		return;
	}

	if( !(menu->window.flags & WINDOW_VISIBLE) && !forcePaint ) {
		return;
	}

	if (menu->window.ownerDrawFlags && DC->ownerDrawVisible && !DC->ownerDrawVisible(menu->window.ownerDrawFlags)) {
		return;
	}
	
	if (forcePaint) {
		menu->window.flags |= WINDOW_FORCED;
	}

	// draw the background if necessary
	if (menu->fullScreen) {
		// implies a background shader
		// FIXME: make sure we have a default shader if fullscreen is set with no background
		DC->drawHandlePic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, menu->window.background );
	} else if (menu->window.background) {
		// this allows a background shader without being full screen
		//UI_DrawHandlePic(menu->window.rect.x, menu->window.rect.y, menu->window.rect.w, menu->window.rect.h, menu->backgroundShader);
	}

	// paint the background and or border
	Window_Paint(&menu->window, menu->fadeAmount, menu->fadeClamp, menu->fadeCycle );

	for (i = 0; i < menu->itemCount; i++) {
		Item_Paint(menu->items[i]);
		if(menu->items[i]->window.flags & WINDOW_MOUSEOVER) item = menu->items[i];
	}

	// OSP draw tooltip data if we have it
	if( DC->getCVarValue( "ui_showtooltips" ) &&
		item != NULL &&
		item->toolTipData != NULL &&
		item->toolTipData->text != NULL &&
		*item->toolTipData->text ) Item_Paint( item->toolTipData );
	
	// ydnar: handle timeout here
	if( menu->openTime == 0 )
		menu->openTime = DC->realTime;
	else if( menu->window.flags & WINDOW_VISIBLE &&
		menu->timeout > 0 && menu->onTimeout != NULL &&
		menu->openTime + menu->timeout <= DC->realTime )
	{
		itemDef_t it;
		it.parent = menu;
		Item_RunScript( &it, NULL, menu->onTimeout );
	}
	
	if (debugMode) {
		vec4_t color;
		color[0] = color[2] = color[3] = 1;
		color[1] = 0;
		DC->drawRect(menu->window.rect.x, menu->window.rect.y, menu->window.rect.w, menu->window.rect.h, 1, color);
	}
}


#endif

/*
===============
Item_ValidateTypeData
===============
*/
void Item_ValidateTypeData(itemDef_t *item) {
	if (item->typeData) {
		return;
	}

	if (item->type == ITEM_TYPE_LISTBOX) {
		item->typeData = UI_Alloc(sizeof(listBoxDef_t));
		memset(item->typeData, 0, sizeof(listBoxDef_t));
	} else if (item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_NUMERICFIELD || item->type == ITEM_TYPE_YESNO || item->type == ITEM_TYPE_BIND || item->type == ITEM_TYPE_SLIDER || item->type == ITEM_TYPE_TEXT) {
		item->typeData = UI_Alloc(sizeof(editFieldDef_t));
		memset(item->typeData, 0, sizeof(editFieldDef_t));
		if (item->type == ITEM_TYPE_EDITFIELD) {
			if (!((editFieldDef_t *) item->typeData)->maxPaintChars) {
				((editFieldDef_t *) item->typeData)->maxPaintChars = MAX_EDITFIELD;
			}
		}
	} else if ( item->type == ITEM_TYPE_MULTI || item->type == ITEM_TYPE_CHECKBOX || item->type == ITEM_TYPE_TRICHECKBOX ) {
		item->typeData = UI_Alloc(sizeof(multiDef_t));
	} else if (item->type == ITEM_TYPE_MODEL) {
		item->typeData = UI_Alloc(sizeof(modelDef_t));
	} else if (item->type == ITEM_TYPE_MENUMODEL) {
		item->typeData = UI_Alloc(sizeof(modelDef_t));
	}
}

/*
========================
Item_ValidateTooltipData
========================
*/
qboolean Item_ValidateTooltipData(itemDef_t *item)
{
	if(item->toolTipData != NULL) return(qtrue);

	item->toolTipData = UI_Alloc(sizeof(itemDef_t));
	if(item->toolTipData == NULL) return(qfalse);

	Item_Init(item->toolTipData);
	Tooltip_Initialize(item->toolTipData);

	return(qtrue);
}

/*
===============
Keyword Hash
===============
*/

#define KEYWORDHASH_SIZE	512

typedef struct keywordHash_s
{
	char *keyword;
	qboolean (*func)(itemDef_t *item, int handle);
	struct keywordHash_s *next;
} keywordHash_t;

int KeywordHash_Key(char *keyword) {
	unsigned int hash, i;
	int letter;

	hash = 0;
	for (i = 0; keyword[i] != '\0'; i++) {
		/* Original MOVSX and 32-bit IMUL/ADD, including wraparound. */
		letter = (signed char)keyword[i];
		if (letter >= 'A' && letter <= 'Z')
			letter += 'a' - 'A';
		hash += (unsigned int)letter * (119u + i);
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20)) & (KEYWORDHASH_SIZE-1);
	return hash;
}

void KeywordHash_Add(keywordHash_t *table[], keywordHash_t *key) {
	int hash;

	hash = KeywordHash_Key(key->keyword);
/*
	if (table[hash]) {
		int collision = qtrue;
	}
*/
	key->next = table[hash];
	table[hash] = key;
}

keywordHash_t *KeywordHash_Find(keywordHash_t *table[], char *keyword)
{
	keywordHash_t *key;
	int hash;

	hash = KeywordHash_Key(keyword);
	for (key = table[hash]; key; key = key->next) {
		if (!Q_stricmp(key->keyword, keyword))
			return key;
	}
	return NULL;
}

/*
===============
Item Keyword Parse functions
===============
*/

// name <string>
qboolean ItemParse_name( itemDef_t *item, int handle ) {
	if (!PC_String_Parse(handle, &item->window.name)) {
		return qfalse;
	}
	return qtrue;
}

// name <string>
qboolean ItemParse_focusSound( itemDef_t *item, int handle ) {
	const char *temp=NULL;
	if (!PC_String_Parse(handle, &temp)) {
		return qfalse;
	}
	item->focusSound = DC->registerSound(temp, qtrue);
	return qtrue;
}


// text <string>
qboolean ItemParse_text( itemDef_t *item, int handle ) {
	if (!PC_String_Parse(handle, &item->text)) {
		return qfalse;
	}
	return qtrue;
}

//----(SA)	added

// textfile <string>
// read an external textfile into item->text
qboolean ItemParse_textfile( itemDef_t *item, int handle ) {
	const char	*newtext;
	pc_token_t token;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;

	newtext = DC->fileText(token.string);
	item->text = String_Alloc(newtext);

	return qtrue;
}
//----(SA)	

// group <string>
qboolean ItemParse_group( itemDef_t *item, int handle ) {
	if (!PC_String_Parse(handle, &item->window.group)) {
		return qfalse;
	}
	return qtrue;
}


// asset_model <string>
qboolean ItemParse_asset_model( itemDef_t *item, int handle ) {
	const char *temp=NULL;
	modelDef_t *modelPtr;
	Item_ValidateTypeData(item);
	modelPtr = (modelDef_t*)item->typeData;

	if (!PC_String_Parse(handle, &temp)) {
		return qfalse;
	}
	if(!(item->asset)) {
		item->asset = DC->registerModel(temp);
//		modelPtr->angle = rand() % 360;
	}
	return qtrue;
}

// asset_shader <string>
qboolean ItemParse_asset_shader( itemDef_t *item, int handle ) {
	const char *temp=NULL;

	if (!PC_String_Parse(handle, &temp)) {
		return qfalse;
	}
	item->asset = DC->registerShaderNoMip(temp);
	return qtrue;
}

// model_origin <number> <number> <number>
qboolean ItemParse_model_origin( itemDef_t *item, int handle ) {
	modelDef_t *modelPtr;
	Item_ValidateTypeData(item);
	modelPtr = (modelDef_t*)item->typeData;

	if (PC_Float_Parse(handle, &modelPtr->origin[0])) {
		if (PC_Float_Parse(handle, &modelPtr->origin[1])) {
			if (PC_Float_Parse(handle, &modelPtr->origin[2])) {
				return qtrue;
			}
		}
	}
	return qfalse;
}

// model_fovx <number>
qboolean ItemParse_model_fovx( itemDef_t *item, int handle ) {
	modelDef_t *modelPtr;
	Item_ValidateTypeData(item);
	modelPtr = (modelDef_t*)item->typeData;

	if (!PC_Float_Parse(handle, &modelPtr->fov_x)) {
		return qfalse;
	}
	return qtrue;
}

// model_fovy <number>
qboolean ItemParse_model_fovy( itemDef_t *item, int handle ) {
	modelDef_t *modelPtr;
	Item_ValidateTypeData(item);
	modelPtr = (modelDef_t*)item->typeData;

	if (!PC_Float_Parse(handle, &modelPtr->fov_y)) {
		return qfalse;
	}
	return qtrue;
}

// model_rotation <integer>
qboolean ItemParse_model_rotation( itemDef_t *item, int handle ) {
	modelDef_t *modelPtr;
	Item_ValidateTypeData(item);
	modelPtr = (modelDef_t*)item->typeData;

	if (!PC_Int_Parse(handle, &modelPtr->rotationSpeed)) {
		return qfalse;
	}
	return qtrue;
}

// model_angle <integer>
qboolean ItemParse_model_angle( itemDef_t *item, int handle ) {
	modelDef_t *modelPtr;
	Item_ValidateTypeData(item);
	modelPtr = (modelDef_t*)item->typeData;

	if (!PC_Int_Parse(handle, &modelPtr->angle)) {
		return qfalse;
	}
	return qtrue;
}

// model_animplay <int(startframe)> <int(numframes)> <int(loopframes)> <int(fps)>
qboolean ItemParse_model_animplay(itemDef_t *item, int handle ) {
	modelDef_t *modelPtr;
	Item_ValidateTypeData(item);
	modelPtr = (modelDef_t*)item->typeData;

	modelPtr->animated = 1;

	if (!PC_Int_Parse(handle, &modelPtr->startframe))	return qfalse;
	if (!PC_Int_Parse(handle, &modelPtr->numframes))	return qfalse;
	if (!PC_Int_Parse(handle, &modelPtr->loopframes))	return qfalse;
	if (!PC_Int_Parse(handle, &modelPtr->fps))			return qfalse;

	modelPtr->frame		= modelPtr->startframe + 1;
	modelPtr->oldframe	= modelPtr->startframe;
	modelPtr->backlerp	= 0.0f;
	modelPtr->frameTime = DC->realTime;
	return qtrue;
}


// rect <rectangle>
qboolean ItemParse_rect( itemDef_t *item, int handle ) {
	return(PC_Rect_Parse(handle, &item->window.rectClient));
}

// NERVE - SMF
// origin <integer, integer>
qboolean ItemParse_origin( itemDef_t *item, int handle ) {
	int x=0, y=0;

	if (!PC_Int_Parse(handle, &x)) {
		return qfalse;
	}
	if (!PC_Int_Parse(handle, &y)) {
		return qfalse;
	}

	item->window.rectClient.x += x;
	item->window.rectClient.y += y;

	return qtrue;
}
// -NERVE - SMF

// style <integer>
qboolean ItemParse_style( itemDef_t *item, int handle ) {
	if (!PC_Int_Parse(handle, &item->window.style)) {
		return qfalse;
	}
	return qtrue;
}

// decoration
qboolean ItemParse_decoration( itemDef_t *item, int handle ) {
	item->window.flags |= WINDOW_DECORATION;
	return qtrue;
}

// textasint
qboolean ItemParse_textasint( itemDef_t *item, int handle ) {
	item->window.flags |= WINDOW_TEXTASINT;
	return qtrue;
}


// textasfloat
qboolean ItemParse_textasfloat( itemDef_t *item, int handle ) {
	item->window.flags |= WINDOW_TEXTASFLOAT;
	return qtrue;
}


// notselectable
qboolean ItemParse_notselectable( itemDef_t *item, int handle ) {
	listBoxDef_t *listPtr;
	Item_ValidateTypeData(item);
	listPtr = (listBoxDef_t*)item->typeData;
	if (item->type == ITEM_TYPE_LISTBOX && listPtr) {
		listPtr->notselectable = qtrue;
	}
	return qtrue;
}

// manually wrapped
qboolean ItemParse_wrapped( itemDef_t *item, int handle ) {
	item->window.flags |= WINDOW_WRAPPED;
	return qtrue;
}

// auto wrapped
qboolean ItemParse_autowrapped( itemDef_t *item, int handle ) {
	item->window.flags |= WINDOW_AUTOWRAPPED;
	return qtrue;
}


// horizontalscroll
qboolean ItemParse_horizontalscroll( itemDef_t *item, int handle ) {
	item->window.flags |= WINDOW_HORIZONTAL;
	return qtrue;
}

// type <integer>
qboolean ItemParse_type( itemDef_t *item, int handle ) {
	if (!PC_Int_Parse(handle, &item->type)) {
		return qfalse;
	}
	Item_ValidateTypeData(item);
	return qtrue;
}

// elementwidth, used for listbox image elements
// uses textalignx for storage
qboolean ItemParse_elementwidth( itemDef_t *item, int handle ) {
	listBoxDef_t *listPtr;

	Item_ValidateTypeData(item);
	listPtr = (listBoxDef_t*)item->typeData;
	if (!PC_Float_Parse(handle, &listPtr->elementWidth)) {
		return qfalse;
	}
	return qtrue;
}

// elementheight, used for listbox image elements
// uses textaligny for storage
qboolean ItemParse_elementheight( itemDef_t *item, int handle ) {
	listBoxDef_t *listPtr;

	Item_ValidateTypeData(item);
	listPtr = (listBoxDef_t*)item->typeData;
	if (!PC_Float_Parse(handle, &listPtr->elementHeight)) {
		return qfalse;
	}
	return qtrue;
}

// feeder <float>
qboolean ItemParse_feeder( itemDef_t *item, int handle ) {
	if (!PC_Float_Parse(handle, &item->special)) {
		return qfalse;
	}
	return qtrue;
}

// elementtype, used to specify what type of elements a listbox contains
// uses textstyle for storage
qboolean ItemParse_elementtype( itemDef_t *item, int handle ) {
	listBoxDef_t *listPtr;

	Item_ValidateTypeData(item);
	if (!item->typeData)
		return qfalse;
	listPtr = (listBoxDef_t*)item->typeData;
	if (!PC_Int_Parse(handle, &listPtr->elementStyle)) {
		return qfalse;
	}
	return qtrue;
}

// columns sets a number of columns and an x pos and width per.. 
qboolean ItemParse_columns( itemDef_t *item, int handle ) {
	int num=0, i;
	listBoxDef_t *listPtr;

	Item_ValidateTypeData(item);
	if (!item->typeData)
		return qfalse;
	listPtr = (listBoxDef_t*)item->typeData;
	if (PC_Int_Parse(handle, &num)) {
		if (num > MAX_LB_COLUMNS) {
			num = MAX_LB_COLUMNS;
		}
		listPtr->numColumns = num;
		for (i = 0; i < num; i++) {
			int pos=0, width=0, maxChars=0;

			if (PC_Int_Parse(handle, &pos) && PC_Int_Parse(handle, &width) && PC_Int_Parse(handle, &maxChars)) {
				listPtr->columnInfo[i].pos = pos;
				listPtr->columnInfo[i].width = width;
				listPtr->columnInfo[i].maxChars = maxChars;
			} else {
				return qfalse;
			}
		}
	} else {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_border( itemDef_t *item, int handle ) {
	if (!PC_Int_Parse(handle, &item->window.border)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_bordersize( itemDef_t *item, int handle ) {
	if (!PC_Float_Parse(handle, &item->window.borderSize)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_visible( itemDef_t *item, int handle ) {
	int i=0;

	if (!PC_Int_Parse(handle, &i)) {
		return qfalse;
	}
	if (i) {
		item->window.flags |= WINDOW_VISIBLE;
	}
	return qtrue;
}

qboolean ItemParse_ownerdraw( itemDef_t *item, int handle ) {
	if (!PC_Int_Parse(handle, &item->window.ownerDraw)) {
		return qfalse;
	}
	item->type = ITEM_TYPE_OWNERDRAW;
	return qtrue;
}

qboolean ItemParse_align( itemDef_t *item, int handle ) {
	if (!PC_Int_Parse(handle, &item->alignment)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_textalign( itemDef_t *item, int handle ) {
	if (!PC_Int_Parse(handle, &item->textalignment)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_textalignx( itemDef_t *item, int handle ) {
	if (!PC_Float_Parse(handle, &item->textalignx)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_textaligny( itemDef_t *item, int handle ) {
	if (!PC_Float_Parse(handle, &item->textaligny)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_textscale( itemDef_t *item, int handle ) {
	if (!PC_Float_Parse(handle, &item->textscale)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_textstyle( itemDef_t *item, int handle ) {
	if (!PC_Int_Parse(handle, &item->textStyle)) {
		return qfalse;
	}
	return qtrue;
}

//----(SA)	added for forcing a font for a given item
qboolean ItemParse_textfont( itemDef_t *item, int handle ) {
	if (!PC_Int_Parse(handle, &item->font)) {
		return qfalse;
	}
	return qtrue;
}
//----(SA)	end

qboolean ItemParse_backcolor( itemDef_t *item, int handle ) {
	int i;
	float f=0.0f;

	for (i = 0; i < 4; i++) {
		if (!PC_Float_Parse(handle, &f)) {
			return qfalse;
		}
		item->window.backColor[i]  = f;
	}
	return qtrue;
}

qboolean ItemParse_forecolor( itemDef_t *item, int handle ) {
	int i;
	float f=0.0f;

	for (i = 0; i < 4; i++) {
		if (!PC_Float_Parse(handle, &f)) {
			return qfalse;
		}
		item->window.foreColor[i]  = f;
		item->window.flags |= WINDOW_FORECOLORSET;
	}
	return qtrue;
}

qboolean ItemParse_bordercolor( itemDef_t *item, int handle ) {
	int i;
	float f=0.0f;

	for (i = 0; i < 4; i++) {
		if (!PC_Float_Parse(handle, &f)) {
			return qfalse;
		}
		item->window.borderColor[i]  = f;
	}
	return qtrue;
}

qboolean ItemParse_outlinecolor( itemDef_t *item, int handle ) {
	if (!PC_Color_Parse(handle, &item->window.outlineColor)){
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_background( itemDef_t *item, int handle ) {
	const char *temp=NULL;

	if (!PC_String_Parse(handle, &temp)) {
		return qfalse;
	}
	item->window.background = DC->registerShaderNoMip(temp);
	return qtrue;
}

qboolean ItemParse_cinematic( itemDef_t *item, int handle ) {
	if (!PC_String_Parse(handle, &item->window.cinematicName)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_doubleClick( itemDef_t *item, int handle ) {
	listBoxDef_t *listPtr;

	Item_ValidateTypeData(item);
	if (!item->typeData) {
		return qfalse;
	}

	listPtr = (listBoxDef_t*)item->typeData;

	if (!PC_Script_Parse(handle, &listPtr->doubleClick)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_onEsc( itemDef_t *item, int handle ) {
	if( !PC_Script_Parse( handle, &item->onEsc ) ) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_onEnter( itemDef_t *item, int handle ) {
	if( !PC_Script_Parse( handle, &item->onEnter ) ) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_contextMenu( itemDef_t *item, int handle ) {
	listBoxDef_t *listPtr;

	Item_ValidateTypeData(item);
	if (!item->typeData) {
		return qfalse;
	}

	listPtr = (listBoxDef_t*)item->typeData;

	if (!PC_String_Parse(handle, &listPtr->contextMenu)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_onFocus( itemDef_t *item, int handle ) {
	if (!PC_Script_Parse(handle, &item->onFocus)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_leaveFocus( itemDef_t *item, int handle ) {
	if (!PC_Script_Parse(handle, &item->leaveFocus)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_mouseEnter( itemDef_t *item, int handle ) {
	if (!PC_Script_Parse(handle, &item->mouseEnter)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_mouseExit( itemDef_t *item, int handle ) {
	if (!PC_Script_Parse(handle, &item->mouseExit)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_mouseEnterText( itemDef_t *item, int handle ) {
	if (!PC_Script_Parse(handle, &item->mouseEnterText)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_mouseExitText( itemDef_t *item, int handle ) {
	if (!PC_Script_Parse(handle, &item->mouseExitText)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_action( itemDef_t *item, int handle ) {
	if (!PC_Script_Parse(handle, &item->action)) {
		return qfalse;
	}
	return qtrue;
}

// NERVE - SMF
qboolean ItemParse_accept( itemDef_t *item, int handle ) {
	if (!PC_Script_Parse(handle, &item->onAccept)) {
		return qfalse;
	}
	return qtrue;
}
// -NERVE - SMF

qboolean ItemParse_special( itemDef_t *item, int handle ) {
	if (!PC_Float_Parse(handle, &item->special)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_cvarTest( itemDef_t *item, int handle ) {
	if (!PC_String_Parse(handle, &item->cvarTest)) {
		return qfalse;
	}
	return qtrue;
}

qboolean ItemParse_cvar( itemDef_t *item, int handle ) {
	editFieldDef_t *editPtr;

	Item_ValidateTypeData(item);
	if (!PC_String_Parse(handle, &item->cvar)) {
		return qfalse;
	}
	Q_strlwr( (char *)item->cvar );
	if (item->typeData) {
		editPtr = (editFieldDef_t*)item->typeData;
		editPtr->minVal = -1;
		editPtr->maxVal = -1;
		editPtr->defVal = -1;
	}
	return qtrue;
}

qboolean ItemParse_maxChars( itemDef_t *item, int handle ) {
	editFieldDef_t *editPtr;
	int maxChars=0;

	Item_ValidateTypeData(item);
	if (!item->typeData)
		return qfalse;

	if (!PC_Int_Parse(handle, &maxChars)) {
		return qfalse;
	}
	editPtr = (editFieldDef_t*)item->typeData;
	editPtr->maxChars = maxChars;
	return qtrue;
}

qboolean ItemParse_maxPaintChars( itemDef_t *item, int handle ) {
	editFieldDef_t *editPtr;
	int maxChars=0;

	Item_ValidateTypeData(item);
	if (!item->typeData)
		return qfalse;

	if (!PC_Int_Parse(handle, &maxChars)) {
		return qfalse;
	}
	editPtr = (editFieldDef_t*)item->typeData;
	editPtr->maxPaintChars = maxChars;
	return qtrue;
}



qboolean ItemParse_cvarFloat( itemDef_t *item, int handle ) {
	editFieldDef_t *editPtr;

	Item_ValidateTypeData(item);
	if (!item->typeData)
		return qfalse;
	editPtr = (editFieldDef_t*)item->typeData;
	if (PC_String_Parse(handle, &item->cvar) &&
		PC_Float_Parse(handle, &editPtr->defVal) &&
		PC_Float_Parse(handle, &editPtr->minVal) &&
		PC_Float_Parse(handle, &editPtr->maxVal)) {
		return qtrue;
	}
	return qfalse;
}

qboolean ItemParse_cvarStrList( itemDef_t *item, int handle ) {
	pc_token_t token;
	multiDef_t *multiPtr;
	int pass;
	
	Item_ValidateTypeData(item);
	if (!item->typeData)
		return qfalse;
	multiPtr = (multiDef_t*)item->typeData;
	multiPtr->count = 0;
	multiPtr->strDef = qtrue;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (*token.string != '{') {
		return qfalse;
	}

	pass = 0;
	while ( 1 ) {
		if (!trap_PC_ReadToken(handle, &token)) {
			PC_SourceError(handle, "end of file inside menu item\n");
			return qfalse;
		}

		if (*token.string == '}') {
			return qtrue;
		}

		if (*token.string == ',' || *token.string == ';') {
			continue;
		}

		if (pass == 0) {
			multiPtr->cvarList[multiPtr->count] = String_Alloc(token.string);
			pass = 1;
		} else {
			multiPtr->cvarStr[multiPtr->count] = String_Alloc(token.string);
			pass = 0;
			multiPtr->count++;
			if (multiPtr->count >= MAX_MULTI_CVARS) {
				return qfalse;
			}
		}

	}
	return qfalse; 	// bk001205 - LCC missing return value
}

qboolean ItemParse_cvarFloatList( itemDef_t *item, int handle ) {
	pc_token_t token;
	multiDef_t *multiPtr;
	
	Item_ValidateTypeData(item);
	if (!item->typeData)
		return qfalse;
	multiPtr = (multiDef_t*)item->typeData;
	multiPtr->count = 0;
	multiPtr->strDef = qfalse;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (*token.string != '{') {
		return qfalse;
	}

	while ( 1 ) {
		if (!trap_PC_ReadToken(handle, &token)) {
			PC_SourceError(handle, "end of file inside menu item\n");
			return qfalse;
		}

		if (*token.string == '}') {
			return qtrue;
		}

		if (*token.string == ',' || *token.string == ';') {
			continue;
		}

		multiPtr->cvarList[multiPtr->count] = String_Alloc(token.string);
		if (!PC_Float_Parse(handle, &multiPtr->cvarValue[multiPtr->count])) {
			return qfalse;
		}

		multiPtr->count++;
		if (multiPtr->count >= MAX_MULTI_CVARS) {
			return qfalse;
		}

	}
	return qfalse; 	// bk001205 - LCC missing return value
}

qboolean ItemParse_cvarListUndefined( itemDef_t *item, int handle )
{
	pc_token_t token;
	multiDef_t *multiPtr;

	Item_ValidateTypeData( item );
	if( !item->typeData )
		return qfalse;
	multiPtr = (multiDef_t*)item->typeData;

	multiPtr->undefinedStr = NULL;

	if( !trap_PC_ReadToken( handle, &token ) )
		return qfalse;

	multiPtr->undefinedStr = String_Alloc( token.string );

	return qtrue;
}

qboolean ParseColorRange( itemDef_t *item, int handle, int type ) {
	colorRangeDef_t color;

	if(item->numColors && type != item->colorRangeType) {
		PC_SourceError(handle, "both addColorRange and addColorRangeRel - set within same itemdef\n");
		return qfalse;
	}

	item->colorRangeType = type;

	if (PC_Float_Parse(handle, &color.low) &&
		PC_Float_Parse(handle, &color.high) &&
		PC_Color_Parse(handle, &color.color) ) {
		if (item->numColors < MAX_COLOR_RANGES) {
			memcpy(&item->colorRanges[item->numColors], &color, sizeof(color));
			item->numColors++;
		}
		return qtrue;
	}
	return qfalse;
}

qboolean ItemParse_addColorRangeRel( itemDef_t *item, int handle ) {
	return ParseColorRange(item, handle, RANGETYPE_RELATIVE);
}

qboolean ItemParse_addColorRange( itemDef_t *item, int handle ) {
	return ParseColorRange(item, handle, RANGETYPE_ABSOLUTE);
}



qboolean ItemParse_ownerdrawFlag( itemDef_t *item, int handle ) {
	int i=0;
	if (!PC_Int_Parse(handle, &i)) {
		return qfalse;
	}
	item->window.ownerDrawFlags |= i;
	return qtrue;
}

qboolean ItemParse_enableCvar( itemDef_t *item, int handle ) {
	if (PC_Script_Parse(handle, &item->enableCvar)) {
		item->cvarFlags = CVAR_ENABLE;
		return qtrue;
	}
	return qfalse;
}

qboolean ItemParse_disableCvar( itemDef_t *item, int handle ) {
	if (PC_Script_Parse(handle, &item->enableCvar)) {
		item->cvarFlags = CVAR_DISABLE;
		return qtrue;
	}
	return qfalse;
}

qboolean ItemParse_noToggle( itemDef_t *item, int handle ) {
	item->cvarFlags |= CVAR_NOTOGGLE;
	return qtrue;
}

qboolean ItemParse_showCvar( itemDef_t *item, int handle ) {
	if (PC_Script_Parse(handle, &item->enableCvar)) {
		item->cvarFlags = CVAR_SHOW;
		return qtrue;
	}
	return qfalse;
}

qboolean ItemParse_hideCvar( itemDef_t *item, int handle ) {
	if (PC_Script_Parse(handle, &item->enableCvar)) {
		item->cvarFlags = CVAR_HIDE;
		return qtrue;
	}
	return qfalse;
}

// START - TAT 9/16/2002
qboolean ItemParse_execKey( itemDef_t *item, int handle ) {
	char keyname;

	// read in the hotkey
	if ( !PC_Char_Parse( handle, &keyname ) ) {
		return qfalse;
	}

	// store it in the hotkey field
	item->hotkey = keyname;

	// read in the command to execute
	if ( !PC_Script_Parse( handle, &item->onKey ) ) {
		return qfalse;
	}

	return qtrue;
}
// END - TAT 9/16/2002

// OSP - server setting tags
qboolean ItemParse_settingDisabled( itemDef_t *item, int handle )
{
	qboolean fResult = PC_Int_Parse(handle, &item->settingTest);
	if(fResult) item->settingFlags = SVS_DISABLED_SHOW;
	return(fResult);
}

qboolean ItemParse_settingEnabled( itemDef_t *item, int handle )
{
	qboolean fResult = PC_Int_Parse(handle, &item->settingTest);
	if(fResult) item->settingFlags = SVS_ENABLED_SHOW;
	return(fResult);
}

qboolean ItemParse_tooltip( itemDef_t *item, int handle )
{
	return(Item_ValidateTooltipData(item) && PC_String_Parse(handle, &item->toolTipData->text));
}

qboolean ItemParse_tooltipalignx( itemDef_t *item, int handle )
{
	return(Item_ValidateTooltipData(item) && PC_Float_Parse(handle, &item->toolTipData->textalignx));
}

qboolean ItemParse_tooltipaligny( itemDef_t *item, int handle )
{
	return(Item_ValidateTooltipData(item) && PC_Float_Parse(handle, &item->toolTipData->textaligny));
}

qboolean ItemParse_voteFlag( itemDef_t *item, int handle )
{
	return(PC_Int_Parse(handle, &item->voteFlag));
}

keywordHash_t itemParseKeywords[] =
{
	{ "accept",				ItemParse_accept,			NULL },	// NERVE - SMF
	{ "action",				ItemParse_action,			NULL },
	{ "addColorRange",		ItemParse_addColorRange,	NULL },
	{ "addColorRangeRel",	ItemParse_addColorRangeRel,	NULL },
	{ "align",				ItemParse_align,			NULL },
	{ "asset_model",		ItemParse_asset_model,		NULL },
	{ "asset_shader",		ItemParse_asset_shader,		NULL },
	{ "autowrapped",		ItemParse_autowrapped,		NULL },
	{ "backcolor",			ItemParse_backcolor,		NULL },
	{ "background",			ItemParse_background,		NULL },
	{ "border",				ItemParse_border,			NULL },
	{ "bordercolor",		ItemParse_bordercolor,		NULL },
	{ "bordersize",			ItemParse_bordersize,		NULL },
	{ "cinematic",			ItemParse_cinematic,		NULL },
	{ "columns",			ItemParse_columns,			NULL },
	{ "contextmenu",		ItemParse_contextMenu,		NULL },
	{ "cvar",				ItemParse_cvar,				NULL },
	{ "cvarFloat",			ItemParse_cvarFloat,		NULL },
	{ "cvarFloatList",		ItemParse_cvarFloatList,	NULL },
	{ "cvarStrList",		ItemParse_cvarStrList,		NULL },
	{ "cvarListUndefined",	ItemParse_cvarListUndefined,NULL },
	{ "cvarTest",			ItemParse_cvarTest,			NULL },
	{ "decoration",			ItemParse_decoration,		NULL },
	{ "textasint",			ItemParse_textasint,		NULL },
	{ "textasfloat",		ItemParse_textasfloat,		NULL },
	{ "disableCvar",		ItemParse_disableCvar,		NULL },
	{ "doubleclick",		ItemParse_doubleClick,		NULL },
	{ "onEsc",				ItemParse_onEsc,			NULL },
	{ "onEnter",			ItemParse_onEnter,			NULL },
	{ "elementheight",		ItemParse_elementheight,	NULL },
	{ "elementtype",		ItemParse_elementtype,		NULL },
	{ "elementwidth",		ItemParse_elementwidth,		NULL },
	{ "enableCvar",			ItemParse_enableCvar,		NULL },
	{ "execKey",			ItemParse_execKey,			NULL },
	{ "feeder",				ItemParse_feeder,			NULL },
	{ "focusSound",			ItemParse_focusSound,		NULL },
	{ "forecolor",			ItemParse_forecolor,		NULL },
	{ "group",				ItemParse_group,			NULL },
	{ "hideCvar",			ItemParse_hideCvar,			NULL },
	{ "horizontalscroll",	ItemParse_horizontalscroll, NULL },
	{ "leaveFocus",			ItemParse_leaveFocus,		NULL },
	{ "maxChars",			ItemParse_maxChars,			NULL },
	{ "maxPaintChars",		ItemParse_maxPaintChars,	NULL },
	{ "model_angle",		ItemParse_model_angle,		NULL },
	{ "model_animplay",		ItemParse_model_animplay,	NULL },
	{ "model_fovx",			ItemParse_model_fovx,		NULL },
	{ "model_fovy",			ItemParse_model_fovy,		NULL },
	{ "model_origin",		ItemParse_model_origin,		NULL },
	{ "model_rotation",		ItemParse_model_rotation,	NULL },
	{ "mouseEnter",			ItemParse_mouseEnter,		NULL },
	{ "mouseEnterText",		ItemParse_mouseEnterText,	NULL },
	{ "mouseExit",			ItemParse_mouseExit,		NULL },
	{ "mouseExitText",		ItemParse_mouseExitText,	NULL },
	{ "name",				ItemParse_name,				NULL },
	{ "noToggle",			ItemParse_noToggle,			NULL }, // TTimo: use with ITEM_TYPE_YESNO and an action script (see sv_punkbuster)
	{ "notselectable",		ItemParse_notselectable,	NULL },
	{ "onFocus",			ItemParse_onFocus,			NULL },
	{ "origin",				ItemParse_origin,			NULL },	// NERVE - SMF
	{ "outlinecolor",		ItemParse_outlinecolor,		NULL },
	{ "ownerdraw",			ItemParse_ownerdraw,		NULL },
	{ "ownerdrawFlag",		ItemParse_ownerdrawFlag,	NULL },
	{ "rect",				ItemParse_rect,				NULL },
	{ "settingDisabled",	ItemParse_settingDisabled,	NULL },	// OSP
	{ "settingEnabled",		ItemParse_settingEnabled,	NULL },	// OSP
	{ "showCvar",			ItemParse_showCvar,			NULL },
	{ "special",			ItemParse_special,			NULL },
	{ "style",				ItemParse_style,			NULL },
	{ "text",				ItemParse_text,				NULL },
	{ "textalign",			ItemParse_textalign,		NULL },
	{ "textalignx",			ItemParse_textalignx,		NULL },
	{ "textaligny",			ItemParse_textaligny,		NULL },
	{ "textfile",			ItemParse_textfile,			NULL },	//----(SA)	added
	{ "textfont",			ItemParse_textfont,			NULL },	// (SA)
	{ "textscale",			ItemParse_textscale,		NULL },
	{ "textstyle",			ItemParse_textstyle,		NULL },
	{ "tooltip",			ItemParse_tooltip,			NULL },
	{ "tooltipalignx",		ItemParse_tooltipalignx,	NULL },
	{ "tooltipaligny",		ItemParse_tooltipaligny,	NULL },
	{ "type",				ItemParse_type,				NULL },
	{ "visible",			ItemParse_visible,			NULL },
	{ "voteFlag",			ItemParse_voteFlag,			NULL }, // OSP - vote check
	{ "wrapped",			ItemParse_wrapped,			NULL },

	{ NULL,					NULL,						NULL }
};

keywordHash_t *itemParseKeywordHash[KEYWORDHASH_SIZE];

/*
===============
Item_SetupKeywordHash
===============
*/
void Item_SetupKeywordHash(void) {
	int i;

	memset(itemParseKeywordHash, 0, sizeof(itemParseKeywordHash));
	for (i = 0; itemParseKeywords[i].keyword; i++) {
		KeywordHash_Add(itemParseKeywordHash, &itemParseKeywords[i]);
	}
}

/*
===============
Item_Parse
===============
*/
qboolean Item_Parse(int handle, itemDef_t *item) {
	pc_token_t token;
	keywordHash_t *key;


	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (*token.string != '{') {
		return qfalse;
	}
	while ( 1 ) {
		if (!trap_PC_ReadToken(handle, &token)) {
			PC_SourceError(handle, "end of file inside menu item\n");
			return qfalse;
		}

		if (*token.string == '}') {
			return qtrue;
		}

		key = KeywordHash_Find(itemParseKeywordHash, token.string);
		if (!key) {
			PC_SourceError(handle, "unknown menu item keyword %s", token.string);
			continue;
		}
		if ( !key->func(item, handle) ) {
			PC_SourceError(handle, "couldn't parse menu item keyword %s", token.string);
			return qfalse;
		}
	}
	return qfalse; 	// bk001205 - LCC missing return value
}


// Item_InitControls
// init's special control types
void Item_InitControls(itemDef_t *item) {
	if (item == NULL) {
		return;
	}
	if (item->type == ITEM_TYPE_LISTBOX) {
		listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;
		item->cursorPos = 0;
		if (listPtr) {
			listPtr->cursorPos = 0;
			listPtr->startPos = 0;
			listPtr->endPos = 0;
		}
	}

	if(item->toolTipData != NULL ) Tooltip_ComputePosition(item);
}

/*
===============
Menu Keyword Parse functions
===============
*/

qboolean MenuParse_name( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_String_Parse(handle, &menu->window.name)) {
		return qfalse;
	}
	if (Q_stricmp(menu->window.name, "main") == 0) {
		// default main as having focus
		//menu->window.flags |= WINDOW_HASFOCUS;
	}
	return qtrue;
}

qboolean MenuParse_fullscreen( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Int_Parse(handle, (int*)&menu->fullScreen)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_rect( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	return(PC_Rect_Parse(handle, &menu->window.rect));
}

qboolean MenuParse_style( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Int_Parse(handle, &menu->window.style)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_visible( itemDef_t *item, int handle ) {
	int i=0;
	menuDef_t *menu = (menuDef_t*)item;

	if (!PC_Int_Parse(handle, &i)) {
		return qfalse;
	}
	if (i) {
		menu->window.flags |= WINDOW_VISIBLE;
	}
	return qtrue;
}

qboolean MenuParse_onOpen( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Script_Parse(handle, &menu->onOpen)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_onClose( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Script_Parse(handle, &menu->onClose)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_onESC( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Script_Parse(handle, &menu->onESC)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_onEnter( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Script_Parse(handle, &menu->onEnter)) {
		return qfalse;
	}
	return qtrue;
}

// ydnar: menu timeout function
qboolean MenuParse_onTimeout( itemDef_t *item, int handle )
{
	menuDef_t		*menu = (menuDef_t*) item;
	
	if( !PC_Int_Parse( handle, &menu->timeout ) )
		return qfalse;
	if( !PC_Script_Parse( handle, &menu->onTimeout ) )
		return qfalse;
	return qtrue;
}



qboolean MenuParse_border( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Int_Parse(handle, &menu->window.border)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_borderSize( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Float_Parse(handle, &menu->window.borderSize)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_backcolor( itemDef_t *item, int handle ) {
	int i;
	float f=0.0f;
	menuDef_t *menu = (menuDef_t*)item;

	for (i = 0; i < 4; i++) {
		if (!PC_Float_Parse(handle, &f)) {
			return qfalse;
		}
		menu->window.backColor[i]  = f;
	}
	return qtrue;
}

qboolean MenuParse_forecolor( itemDef_t *item, int handle ) {
	int i;
	float f=0.0f;
	menuDef_t *menu = (menuDef_t*)item;

	for (i = 0; i < 4; i++) {
		if (!PC_Float_Parse(handle, &f)) {
			return qfalse;
		}
		menu->window.foreColor[i]  = f;
		menu->window.flags |= WINDOW_FORECOLORSET;
	}
	return qtrue;
}

qboolean MenuParse_bordercolor( itemDef_t *item, int handle ) {
	int i;
	float f=0.0f;
	menuDef_t *menu = (menuDef_t*)item;

	for (i = 0; i < 4; i++) {
		if (!PC_Float_Parse(handle, &f)) {
			return qfalse;
		}
		menu->window.borderColor[i]  = f;
	}
	return qtrue;
}

qboolean MenuParse_focuscolor( itemDef_t *item, int handle ) {
	int i;
	float f=0.0f;
	menuDef_t *menu = (menuDef_t*)item;

	for (i = 0; i < 4; i++) {
		if (!PC_Float_Parse(handle, &f)) {
			return qfalse;
		}
		menu->focusColor[i]  = f;
	}
	item->window.flags |= WINDOW_FOCUSPULSE;
	return qtrue;
}

qboolean MenuParse_disablecolor( itemDef_t *item, int handle ) {
	int i;
	float f=0.0f;
	menuDef_t *menu = (menuDef_t*)item;
	for (i = 0; i < 4; i++) {
		if (!PC_Float_Parse(handle, &f)) {
			return qfalse;
		}
		menu->disableColor[i]  = f;
	}
	return qtrue;
}


qboolean MenuParse_outlinecolor( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Color_Parse(handle, &menu->window.outlineColor)){
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_background( itemDef_t *item, int handle ) {
	const char *buff=NULL;
	menuDef_t *menu = (menuDef_t*)item;

	if (!PC_String_Parse(handle, &buff)) {
		return qfalse;
	}
	menu->window.background = DC->registerShaderNoMip(buff);
	return qtrue;
}

qboolean MenuParse_cinematic( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;

	if (!PC_String_Parse(handle, &menu->window.cinematicName)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_ownerdrawFlag( itemDef_t *item, int handle ) {
	int i=0;
	menuDef_t *menu = (menuDef_t*)item;

	if (!PC_Int_Parse(handle, &i)) {
		return qfalse;
	}
	menu->window.ownerDrawFlags |= i;
	return qtrue;
}

qboolean MenuParse_ownerdraw( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;

	if (!PC_Int_Parse(handle, &menu->window.ownerDraw)) {
		return qfalse;
	}
	return qtrue;
}


// decoration
qboolean MenuParse_popup( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	menu->window.flags |= WINDOW_POPUP;
	return qtrue;
}


qboolean MenuParse_outOfBounds( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;

	menu->window.flags |= WINDOW_OOB_CLICK;
	return qtrue;
}

qboolean MenuParse_soundLoop( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;

	if (!PC_String_Parse(handle, &menu->soundName)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_fadeClamp( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;

	if (!PC_Float_Parse(handle, &menu->fadeClamp)) {
		return qfalse;
	}
	return qtrue;
}

qboolean MenuParse_fadeAmount( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;

	if (!PC_Float_Parse(handle, &menu->fadeAmount)) {
		return qfalse;
	}
	return qtrue;
}


qboolean MenuParse_fadeCycle( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;

	if (!PC_Int_Parse(handle, &menu->fadeCycle)) {
		return qfalse;
	}
	return qtrue;
}


qboolean MenuParse_itemDef( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;

	if (menu->itemCount < MAX_MENUITEMS) {
		menu->items[menu->itemCount] = UI_Alloc(sizeof(itemDef_t));
		Item_Init(menu->items[menu->itemCount]);
		if (!Item_Parse(handle, menu->items[menu->itemCount])) {
			return qfalse;
		}
		menu->items[menu->itemCount]->parent = menu;
		Item_InitControls(menu->items[menu->itemCount++]);

		// START - TAT 9/16/2002
		// If we are storing the hotkeys in the items, we have a little problem, in that
		//		people check with the menu to see if we have a hotkey (see UI_CheckExecKey)
		//		So we sort of need to stuff the hotkey back into the menu for that to work
		//		only do this at all if we're using the item hotkey mode
		//		NOTE:  we couldn't do this earlier because the menu wasn't set, and I don't know
		//		what would happen if we tried to set the menu before the parse had succeeded...
		if (menu->itemHotkeyMode && menu->items[menu->itemCount-1]->hotkey >= 0)
		{
			menu->onKey[menu->items[menu->itemCount-1]->hotkey] = String_Alloc(menu->items[menu->itemCount-1]->onKey);
		}
		// END - TAT 9/16/2002
	}
	return qtrue;
}

// NERVE - SMF
qboolean MenuParse_execKey( itemDef_t *item, int handle ) {
	menuDef_t *menu = ( menuDef_t* )item;
	char keyname=0;
	short int keyindex;

	if ( !PC_Char_Parse( handle, &keyname ) ) {
		return qfalse;
	}
	keyindex = keyname;

	if ( !PC_Script_Parse( handle, &menu->onKey[keyindex] ) ) {
		return qfalse;
	}

	return qtrue;
}

qboolean MenuParse_execKeyInt( itemDef_t *item, int handle ) {
	menuDef_t *menu = ( menuDef_t* )item;
	int keyname=0;

	if ( !PC_Int_Parse( handle, &keyname ) ) {
		return qfalse;
	}

	if ( !PC_Script_Parse( handle, &menu->onKey[keyname] ) ) {
		return qfalse;
	}
	return qtrue;
}
// -NERVE - SMF

qboolean MenuParse_drawAlwaysOnTop( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;
	menu->window.flags |= WINDOW_DRAWALWAYSONTOP;
	return qtrue;
}

// START - TAT 9/16/2002
// parse the command to set if we're looping through all items to find the current hotkey
qboolean MenuParse_itemHotkeyMode( itemDef_t *item, int handle ) {
	// like MenuParse_fullscreen - reading an int
	menuDef_t *menu = (menuDef_t*)item;
	if (!PC_Int_Parse(handle, (int*)&menu->itemHotkeyMode)) {
		return qfalse;
	}
	return qtrue;
}
// END - TAT 9/16/2002

// TTimo
qboolean MenuParse_modal( itemDef_t *item, int handle ) {
	menuDef_t *menu = (menuDef_t*)item;	
	menu->window.flags |= WINDOW_MODAL;
	return qtrue;
}

keywordHash_t menuParseKeywords[] = {
	{"name", MenuParse_name, NULL},
	{"fullscreen", MenuParse_fullscreen, NULL},
	{"rect", MenuParse_rect, NULL},
	{"style", MenuParse_style, NULL},
	{"visible", MenuParse_visible, NULL},
	{"onOpen", MenuParse_onOpen, NULL},
	{"onClose", MenuParse_onClose, NULL},
	{"onTimeout", MenuParse_onTimeout, NULL },	// ydnar: menu timeout function
	{"onESC", MenuParse_onESC, NULL},
	{"onEnter", MenuParse_onEnter, NULL},
	{"border", MenuParse_border, NULL},
	{"borderSize", MenuParse_borderSize, NULL},
	{"backcolor", MenuParse_backcolor, NULL},
	{"forecolor", MenuParse_forecolor, NULL},
	{"bordercolor", MenuParse_bordercolor, NULL},
	{"focuscolor", MenuParse_focuscolor, NULL},
	{"disablecolor", MenuParse_disablecolor, NULL},
	{"outlinecolor", MenuParse_outlinecolor, NULL},
	{"background", MenuParse_background, NULL},
	{"ownerdraw", MenuParse_ownerdraw, NULL},
	{"ownerdrawFlag", MenuParse_ownerdrawFlag, NULL},
	{"outOfBoundsClick", MenuParse_outOfBounds, NULL},
	{"soundLoop", MenuParse_soundLoop, NULL},
	{"itemDef", MenuParse_itemDef, NULL},
	{"cinematic", MenuParse_cinematic, NULL},
	{"popup", MenuParse_popup, NULL},
	{"fadeClamp", MenuParse_fadeClamp, NULL},
	{"fadeCycle", MenuParse_fadeCycle, NULL},
	{"fadeAmount", MenuParse_fadeAmount, NULL},
	{"execKey", MenuParse_execKey, NULL},				// NERVE - SMF
	{"execKeyInt", MenuParse_execKeyInt, NULL},			// NERVE - SMF
	{"alwaysontop", MenuParse_drawAlwaysOnTop, NULL},
	{"modal", MenuParse_modal, NULL },

// START - TAT 9/16/2002
	// parse the command to set if we're looping through all items to find the current hotkey
	{"itemHotkeyMode", MenuParse_itemHotkeyMode, NULL},
// END - TAT 9/16/2002
	{NULL, NULL, NULL}
};

keywordHash_t *menuParseKeywordHash[KEYWORDHASH_SIZE];

/*
===============
Menu_SetupKeywordHash
===============
*/
void Menu_SetupKeywordHash(void) {
	int i;

	memset(menuParseKeywordHash, 0, sizeof(menuParseKeywordHash));
	for (i = 0; menuParseKeywords[i].keyword; i++) {
		KeywordHash_Add(menuParseKeywordHash, &menuParseKeywords[i]);
	}
}

/*
===============
Menu_Parse
===============
*/
qboolean Menu_Parse(int handle, menuDef_t *menu) {
	pc_token_t token;
	keywordHash_t *key;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (*token.string != '{') {
		return qfalse;
	}
    
	while ( 1 ) {

		memset(&token, 0, sizeof(pc_token_t));
		if (!trap_PC_ReadToken(handle, &token)) {
			PC_SourceError(handle, "end of file inside menu\n");
			return qfalse;
		}

		if (*token.string == '}') {
			return qtrue;
		}

		key = KeywordHash_Find(menuParseKeywordHash, token.string);
		if (!key) {
			PC_SourceError(handle, "unknown menu keyword %s", token.string);
			continue;
		}
		if ( !key->func((itemDef_t*)menu, handle) ) {
			PC_SourceError(handle, "couldn't parse menu keyword %s", token.string);
			return qfalse;
		}
	}
	return qfalse; 	// bk001205 - LCC missing return value
}

/*
===============
Menu_New
===============
*/
void Menu_New(int handle) {
	menuDef_t *menu = &Menus[menuCount];

	if (menuCount < MAX_MENUS) {
		Menu_Init(menu);
		if (Menu_Parse(handle, menu)) {
			Menu_PostParse(menu);
			menuCount++;
		}
	}
}

int Menu_Count() {
	return menuCount;
}

menuDef_t *Menu_Get( int handle )
{
	if( handle >= 0 && handle < menuCount )
		return &Menus[handle];
	else
		return NULL;
}

void Menu_PaintAll() {
	int i;
	if (captureFunc) {
		captureFunc(captureData);
	}

	for (i = 0; i < menuCount; i++) {
		if( Menus[i].window.flags & WINDOW_DRAWALWAYSONTOP )
			continue;
		Menu_Paint(&Menus[i], qfalse);
	}

	for (i = 0; i < menuCount; i++) {
		if( Menus[i].window.flags & WINDOW_DRAWALWAYSONTOP ) {
			Menu_Paint(&Menus[i], qfalse);
		}
	}

	if (debugMode) {
		vec4_t v = {1, 1, 1, 1};
		DC->textFont( 0 ); // TC Windows4001fc00 and Linux Menu_PaintAll select font slot0.
		DC->drawText( 5, 10, .2, v, va("fps: %.2f", DC->FPS), 0, 0, 0);
		DC->drawText( 5, 20, .2, v, va("mouse: %i %i", DC->cursorx, DC->cursory), 0, 0, 0);
	}
}

void Menu_Reset() {
	menuCount = 0;
}

displayContextDef_t *Display_GetContext() {
	return DC;
}
 
//static float captureX; // TTimo: unused
//static float captureY; // TTimo: unused

void *Display_CaptureItem(int x, int y) {
	int i;

	for (i = 0; i < menuCount; i++) {
		// turn off focus each item
		// menu->items[i].window.flags &= ~WINDOW_HASFOCUS;
		if (Rect_ContainsPoint(&Menus[i].window.rect, x, y)) {
			return &Menus[i];
		}
	}
	return NULL;
}


// FIXME: 
qboolean Display_MouseMove(void *p, int x, int y) {
	int i;
	menuDef_t *menu = p;

//	menu = Menu_GetFocused();

	if (menu == NULL) {
		menu = Menu_GetFocused();
		if (menu) {
			if (menu->window.flags & WINDOW_POPUP) {
				Menu_HandleMouseMove(menu, x, y);
				return qtrue;
			}
		}
		for (i = 0; i < menuCount; i++) {
			Menu_HandleMouseMove(&Menus[i], x, y);
		}
	} else {
		menu->window.rect.x += x;
		menu->window.rect.y += y;
		Menu_UpdatePosition(menu);
	}
 	return qtrue;

}

int Display_CursorType(int x, int y) {
	int i;
	for (i = 0; i < menuCount; i++) {
		rectDef_t r2;
		r2.x = Menus[i].window.rect.x - 3;
		r2.y = Menus[i].window.rect.y - 3;
		r2.w = r2.h = 7;
		if (Rect_ContainsPoint(&r2, x, y)) {
			return CURSOR_SIZER;
		}
	}
	return CURSOR_ARROW;
}


void Display_HandleKey(int key, qboolean down, int x, int y) {
	menuDef_t *menu = Display_CaptureItem(x, y);
	if (menu == NULL) {  
		menu = Menu_GetFocused();
	}
	if (menu) {
		Menu_HandleKey(menu, key, down );
	}
}

static void Window_CacheContents(windowDef_t *window) {
	if (window) {
		if (window->cinematicName) {
			int cin = DC->playCinematic(window->cinematicName, 0, 0, 0, 0);
			DC->stopCinematic(cin);
		}
	}
}


static void Item_CacheContents(itemDef_t *item) {
	if (item) {
		Window_CacheContents(&item->window);
	}

}

static void Menu_CacheContents(menuDef_t *menu) {
	if (menu) {
		int i;
		Window_CacheContents(&menu->window);
		for (i = 0; i < menu->itemCount; i++) {
			Item_CacheContents(menu->items[i]);
		}

		if (menu->soundName && *menu->soundName) {
			DC->registerSound(menu->soundName, qtrue);
		}
	}

}

void Display_CacheAll() {
	int i;
	for (i = 0; i < menuCount; i++) {
		Menu_CacheContents(&Menus[i]);
	}
}


static qboolean Menu_OverActiveItem(menuDef_t *menu, float x, float y) {
 	if (menu && menu->window.flags & (WINDOW_VISIBLE | WINDOW_FORCED)) {
		if (Rect_ContainsPoint(&menu->window.rect, x, y)) {
			int i;
			for (i = 0; i < menu->itemCount; i++) {
				// turn off focus each item
				// menu->items[i]->window.flags &= ~WINDOW_HASFOCUS;

				if (!(menu->items[i]->window.flags & (WINDOW_VISIBLE | WINDOW_FORCED))) {
					continue;
				}

				if (menu->items[i]->window.flags & WINDOW_DECORATION) {
					continue;
				}

				if (Rect_ContainsPoint(&menu->items[i]->window.rect, x, y)) {
					itemDef_t *overItem = menu->items[i];
					if (overItem->type == ITEM_TYPE_TEXT && overItem->text) {
						if (Rect_ContainsPoint(Item_CorrectedTextRect(overItem), x, y)) {
							return qtrue;
						} else {
							continue;
						}
					} else {
						return qtrue;
					}
				}
			}

		}
	}
	return qfalse;
}

/*
=================
PC_String_Parse_Trans

NERVE - SMF - translates string
=================
*/
qboolean PC_String_Parse_Trans(int handle, const char **out) {
	pc_token_t token;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;

	*(out) = String_Alloc( DC->translateString( token.string ) );
    return qtrue;
}

/*
=================
PC_Rect_Parse
=================
*/
qboolean PC_Rect_Parse(int handle, rectDef_t *r) {
	if (PC_Float_Parse(handle, &r->x)) {
		if (PC_Float_Parse(handle, &r->y)) {
			if (PC_Float_Parse(handle, &r->w)) {
				if (PC_Float_Parse(handle, &r->h)) {
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

// digibob
// Panel Handling
// ======================================================
panel_button_t* bg_focusButton;

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(UIDLL)
enum { cr802X=offsetof(displayContextDef_t,cursorx), cr802Y=offsetof(displayContextDef_t,cursory), cr802RX=offsetof(rectDef_t,x), cr802RY=offsetof(rectDef_t,y), cr802RW=offsetof(rectDef_t,w), cr802RH=offsetof(rectDef_t,h) };
__declspec(naked) qboolean BG_RectContainsPoint(float x,float y,float w,float h,float px,float py) {
 __asm {
 FLD dword ptr [ESP + 0x14]
 FCOMP dword ptr [ESP + 0x4]
 FNSTSW AX
 TEST AH,0x41
 JNZ cr802_30087aea
 FLD dword ptr [ESP + 0x4]
 FADD dword ptr [ESP + 0xc]
 FCOMP dword ptr [ESP + 0x14]
 FNSTSW AX
 TEST AH,0x41
 JNZ cr802_30087aea
 FLD dword ptr [ESP + 0x18]
 FCOMP dword ptr [ESP + 0x8]
 FNSTSW AX
 TEST AH,0x41
 JNZ cr802_30087aea
 FLD dword ptr [ESP + 0x8]
 FADD dword ptr [ESP + 0x10]
 FCOMP dword ptr [ESP + 0x18]
 FNSTSW AX
 TEST AH,0x41
 JNZ cr802_30087aea
 MOV EAX,0x1
 RET
cr802_30087aea:
 XOR EAX,EAX
 RET
 }
}
__declspec(naked) qboolean BG_CursorInRect(rectDef_t *rect) {
 __asm {
 MOV EAX,[DC]
 PUSH ECX
 FILD dword ptr [EAX + cr802Y]
 FSTP dword ptr [ESP]
 FILD dword ptr [EAX + cr802X]
 MOV EAX,dword ptr [ESP + 0x8]
 PUSH ECX
 MOV ECX,dword ptr [EAX + cr802RH]
 MOV EDX,dword ptr [EAX + cr802RW]
 FSTP dword ptr [ESP]
 PUSH ECX
 MOV ECX,dword ptr [EAX + cr802RY]
 PUSH EDX
 MOV EDX,dword ptr [EAX + cr802RX]
 PUSH ECX
 PUSH EDX
 CALL BG_RectContainsPoint
 ADD ESP,0x18
 RET
 }
}
#else
qboolean BG_RectContainsPoint(float x, float y, float w, float h, float px, float py) {
	if(px > x && px < x + w && py > y && py < y + h) {
		return qtrue;
	}
	return qfalse;
}

qboolean BG_CursorInRect( rectDef_t* rect ) {
	return BG_RectContainsPoint( rect->x, rect->y, rect->w, rect->h, DC->cursorx, DC->cursory );
}


#endif

void BG_PanelButton_RenderEdit( panel_button_t* button ) {
	qboolean useCvar = button->data[0] ? qfalse : qtrue;
	int offset = -1;

	if( useCvar ) {
		char buffer[256 + 1];
		trap_Cvar_VariableStringBuffer( button->text, buffer, sizeof(buffer) );

		if( BG_PanelButtons_GetFocusButton() == button && ((DC->realTime / 1000) % 2)) {
			if( trap_Key_GetOverstrikeMode() )
				Q_strcat( buffer, sizeof(buffer), "^0|" );
			else
				Q_strcat( buffer, sizeof(buffer), "^0_" );
		} else {
			Q_strcat( buffer, sizeof(buffer), " " );
		}

		do {
			offset++;
			if( buffer[offset] == '\0' )
				break;
		} while( DC->textWidthExt( buffer + offset, button->font->scalex, 0, button->font->font ) > button->rect.w );

		DC->drawTextExt( button->rect.x, button->rect.y + button->rect.h, button->font->scalex, button->font->scaley, button->font->colour, va( "^7%s", buffer + offset ), 0, 0, button->font->style, button->font->font );
	} else {
		char *s;

		if( BG_PanelButtons_GetFocusButton() == button && ((DC->realTime / 1000) % 2)) {
			if( DC->getOverstrikeMode() )
				s = va( "^7%s^0|", button->text );
			else
				s = va( "^7%s^0_", button->text );
		} else {
			s = va( "^7%s ", button->text );	// space hack to make the text not blink
		}

		do {
			offset++;
			if( s[offset] == '\0' )
				break;
		} while( DC->textWidthExt( s + offset, button->font->scalex, 0, button->font->font ) > button->rect.w );

		DC->drawTextExt( button->rect.x, button->rect.y + button->rect.h, button->font->scalex, button->font->scaley, button->font->colour, s + offset, 0, 0, button->font->style, button->font->font );
	}
}

qboolean BG_PanelButton_EditClick( panel_button_t* button, int key ) {
	if( key == K_MOUSE1 ) {
		if( !BG_CursorInRect( &button->rect ) && BG_PanelButtons_GetFocusButton() == button ) {
			BG_PanelButtons_SetFocusButton( NULL );
			if( button->onFinish ) {
				button->onFinish( button );
			}
			return qfalse;
		} else {
			BG_PanelButtons_SetFocusButton( button );
			return qtrue;
		}
	} else if( BG_PanelButtons_GetFocusButton() != button ) {
		return qfalse;
	} else {
		char buffer[256];
		char *s = NULL;
		int len, maxlen;
		qboolean useCvar = button->data[0] ? qfalse : qtrue;

		if( useCvar ) {
			maxlen = sizeof(buffer);
			DC->getCVarString( button->text, buffer, sizeof(buffer) );
			len = Q_strlenInt( buffer );
		} else {
			maxlen = button->data[0];
			s = (char *)button->text;
			len = Q_strlenInt( s );
		}		

		if( key & K_CHAR_FLAG ) {
			key &= ~K_CHAR_FLAG;

			if( key == 'h' - 'a' + 1 )	{	// ctrl-h is backspace
				if( len ) {
					if( useCvar ) {
						buffer[len-1] = '\0';
						DC->setCVar( button->text, buffer );
					} else {
						s[len-1] = '\0';
					}
				}
	    		return qtrue;
			}

			if( key < 32 ) {
			    return qtrue;
		    }

			if( button->data[1] ) {
				if( key < '0' || key > '9' ) {
					if( button->data[1] == 2 ) {
						return qtrue;
					} else if( !(len == 0 && key == '-') ) {
						return qtrue;
					}
				}
			}

			if( len >= maxlen - 1 ) {
				return qtrue;
			}

			if( useCvar ) {
				buffer[len] = key;
				buffer[len+1] = '\0';
				trap_Cvar_Set( button->text, buffer );
			} else {
				s[len] = key;
				s[len+1] = '\0';
			}
			return qtrue;
		} else {
			// Gordon: FIXME: have this work with all our stuff (use data[x] to store cursorpos etc)
/*			if ( key == K_DEL || key == K_KP_DEL ) {
				if ( item->cursorPos < len ) {
					memmove( buff + item->cursorPos, buff + item->cursorPos + 1, len - item->cursorPos);
					DC->setCVar(item->cvar, buff);
				}
				return qtrue;
			}*/

/*			if ( key == K_RIGHTARROW || key == K_KP_RIGHTARROW ) 
			{
				if (editPtr->maxPaintChars && item->cursorPos >= editPtr->paintOffset + editPtr->maxPaintChars && item->cursorPos < len) {
					item->cursorPos++;
					editPtr->paintOffset++;
					return qtrue;
				}
				if (item->cursorPos < len) {
					item->cursorPos++;
				} 
				return qtrue;
			}

			if ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) 
			{
				if ( item->cursorPos > 0 ) {
					item->cursorPos--;
				}
				if (item->cursorPos < editPtr->paintOffset) {
					editPtr->paintOffset--;
				}
				return qtrue;
			}

			if ( key == K_HOME || key == K_KP_HOME) {// || ( tolower(key) == 'a' && trap_Key_IsDown( K_CTRL ) ) ) {
				item->cursorPos = 0;
				editPtr->paintOffset = 0;
				return qtrue;
			}

			if ( key == K_END || key == K_KP_END)  {// ( tolower(key) == 'e' && trap_Key_IsDown( K_CTRL ) ) ) {
				item->cursorPos = len;
				if(item->cursorPos > editPtr->maxPaintChars) {
					editPtr->paintOffset = len - editPtr->maxPaintChars;
				}
				return qtrue;
			}

			if ( key == K_INS || key == K_KP_INS ) {
				DC->setOverstrikeMode(!DC->getOverstrikeMode());
				return qtrue;
			}*/

			if( key == K_ENTER || key == K_KP_ENTER ) {
				if( button->onFinish ) {
					button->onFinish( button );
				}
				BG_PanelButtons_SetFocusButton( NULL );
				return qfalse;
			}
		}
	}

	return qtrue;
}

qboolean BG_PanelButtonsKeyEvent( int key, qboolean down, panel_button_t** buttons ) {
	panel_button_t* button;

	if( BG_PanelButtons_GetFocusButton() ) {
		for( ; *buttons; buttons++ ) {
			button = (*buttons);

			if( button == BG_PanelButtons_GetFocusButton() ) {
				if( button->onKeyDown && down ) {
					if( !button->onKeyDown( button, key ) ) {
						if( BG_PanelButtons_GetFocusButton() ) {
							return qfalse;
						}
					} else {
						return qtrue;
					}
				}
				if( button->onKeyUp && !down ) {
					if( !button->onKeyUp( button, key ) ) {
						if( BG_PanelButtons_GetFocusButton() ) {
							return qfalse;
						}
					} else {
						return qtrue;
					}
				}
			}
		}
	}

	if( down ) {
		for( ; *buttons; buttons++ ) {
			button = (*buttons);

			if( button->onKeyDown ) {
				if( BG_CursorInRect( &button->rect ) ) {
					if( button->onKeyDown( button, key ) ) {
						return qtrue;
					}
				}
			}			
		}
	} else {
		for( ; *buttons; buttons++ ) {
			button = (*buttons);

			if( button->onKeyUp && BG_CursorInRect( &button->rect )) {
				if( button->onKeyUp( button, key ) ) {
					return qtrue;
				}
			}
		}
	}

	return qfalse;
}

void BG_PanelButtonsSetup( panel_button_t** buttons ) {
	panel_button_t* button;

	for( ; *buttons; buttons++ ) {
		button = (*buttons);

		if( button->shaderNormal ) {
			button->hShaderNormal = trap_R_RegisterShaderNoMip( button->shaderNormal );
		}			
	}
}

panel_button_t* BG_PanelButtonsGetHighlightButton( panel_button_t** buttons ) {
	panel_button_t* button;

	for( ; *buttons; buttons++ ) {
		button = (*buttons);

		if( button->onKeyDown && BG_CursorInRect( &button->rect ) ) {
			return button;
		}
	}

	return NULL;
}

void BG_PanelButtonsRender( panel_button_t** buttons ) {
	panel_button_t* button;

	for( ; *buttons; buttons++ ) {
		button = (*buttons);

		if( button->onDraw ) {
			button->onDraw( button );
		}
	}
}

void BG_PanelButtonsRender_TextExt( panel_button_t* button, const char* text ) {
	float x = button->rect.x;
	float w = button->rect.w;

	if( !button->font ) {
		return;
	}

	if( button->font->align == ITEM_ALIGN_CENTER ) {
			w = DC->textWidthExt( text, button->font->scalex, 0, button->font->font );

			x += ((button->rect.w - w) * 0.5f);
	} else if( button->font->align == ITEM_ALIGN_RIGHT ) {
		x += button->rect.w - DC->textWidthExt( text, button->font->scalex, 0, button->font->font );
	}

	if( button->data[1] ) {
		vec4_t clrBdr = { 0.5f, 0.5f,	0.5f,	1.f };
		vec4_t clrBck = { 0.f,	0.f,	0.f,	0.8f };

		DC->fillRect( button->rect.x, button->rect.y, button->rect.w, button->rect.h, clrBck );
		DC->drawRect( button->rect.x, button->rect.y, button->rect.w, button->rect.h, 1, clrBdr );
	}
	DC->drawTextExt( x, button->rect.y + button->data[0], button->font->scalex, button->font->scaley, button->font->colour, text, 0, 0, button->font->style, button->font->font );
}

void BG_PanelButtonsRender_Text( panel_button_t* button ) {
	BG_PanelButtonsRender_TextExt( button, button->text );
}

void BG_PanelButtonsRender_Img( panel_button_t* button ) {
	vec4_t clr = { 1.f, 1.f, 1.f, 1.f };

	if( button->data[0] ) {
		clr[0] = button->data[1] / 255.f;
		clr[1] = button->data[2] / 255.f;
		clr[2] = button->data[3] / 255.f;
		clr[3] = button->data[4] / 255.f;

		trap_R_SetColor( clr );
	}

	if( button->data[5] ) {
		DC->drawRect( button->rect.x, button->rect.y, button->rect.w, button->rect.h, 1, clr );
	} else {
		DC->drawHandlePic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, button->hShaderNormal );
	}

	if( button->data[0] ) {
		trap_R_SetColor( NULL );
	}
}

panel_button_t* BG_PanelButtons_GetFocusButton( void ) {
	return bg_focusButton;
}

void BG_PanelButtons_SetFocusButton( panel_button_t* button ) {
	bg_focusButton = button;
}

void BG_FitTextToWidth_Ext( char* instr, float scale, float w, int size, fontInfo_t* font ) {
	char buffer[1024];
	char	*s, *p, *c, *ls;
	int		l;
	
	Q_strncpyz(buffer, instr, 1024);
	memset(instr, 0, size);

	c = s = instr;
	p = buffer;
	ls = NULL;
	l = 0;
	while(*p) {
		*c = *p++;
		l++;

		if(*c == ' ') {
			ls = c;
		} // store last space, to try not to break mid word

		c++;

		if(*p == '\n') {
			s = c+1;
			l = 0;
		} else if(DC->textWidthExt( s, scale, 0, font ) > w) {
			if(ls) {
				*ls = '\n';
				s = ls+1;
			} else {
				*c = *(c-1);
				*(c-1) = '\n';
				s = c++;
			}

			ls = NULL;
			l = 0;
		}
	}

	if(c != buffer && (*(c-1) != '\n')) {
		*c++ = '\n';
	}
	
	*c = '\0';
}
