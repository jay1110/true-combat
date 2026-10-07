#include "cg_local.h"
#include "../game/tce_bg.h"
#include "tce_limbo_widgets.h"
#include "tce_limbo_loadout.h"
#include "tce_weapon_media.h"
#include "../ui/tce_ui_coordinates.h"

qboolean tce_uiCoordinates;
static void CG_LimboPanel_RenderTeamCounter(panel_button_t *button);

#define SOUNDEVENT( sound ) trap_S_StartLocalSound( sound, CHAN_LOCAL_SOUND )

#define SOUND_SELECT	SOUNDEVENT( cgs.media.sndLimboSelect )
#define SOUND_FOCUS		SOUNDEVENT( cgs.media.sndLimboFocus )
#define SOUND_FILTER	SOUNDEVENT( cgs.media.sndLimboFilter )
#define SOUND_CANCEL	SOUNDEVENT( cgs.media.sndLimboCancel )

void CG_DrawBorder( float x, float y, float w, float h, qboolean fill, qboolean drawMouseOver );

team_t teamOrder[3] = {
	TEAM_ALLIES,
	TEAM_AXIS,
	TEAM_SPECTATOR,
};

panel_button_text_t nameEditFont = {
	0.22f, 0.24f,
	{ 1.f, 1.f, 1.f, 0.8f },
	ITEM_TEXTSTYLE_SHADOWED, 0,
	&cgs.media.limboFont2,
};

panel_button_text_t classBarFont = {
	0.22f, 0.24f,
	{ 0.f, 0.f, 0.f, 0.8f },
	0, 0,
	&cgs.media.limboFont2,
};

panel_button_text_t titleLimboFont = {
	0.24f, 0.28f,
	{ 1.f, 1.f, 1.f, 0.6f },
	0, 0,
	&cgs.media.limboFont1,
};

panel_button_text_t titleLimboFontBig = {
	0.3f, 0.3f,
	{ 1.f, 1.f, 1.f, 0.6f },
	0, 0,
	&cgs.media.limboFont1,
};

panel_button_text_t titleLimboFontBigCenter = {
	0.3f, 0.3f,
	{ 1.f, 1.f, 1.f, 0.6f },
	0, ITEM_ALIGN_CENTER,
	&cgs.media.limboFont1,
};

panel_button_text_t spawnLimboFont = {
	0.18f, 0.22f,
	{ 1.f, 1.f, 1.f, 0.6f },
	0, 0,
	&cgs.media.limboFont1,
};

panel_button_text_t weaponButtonFont = {
	0.33f, 0.33f,
	{ 0.f, 0.f, 0.f, 0.6f },
	0, 0,
	&cgs.media.limboFont1,
};

panel_button_text_t weaponPanelNameFont = {
	0.20f, 0.24f,
	{ 1.0f, 1.0f, 1.0f, 0.4f },
	0, 0,
	&cgs.media.limboFont1,
};

panel_button_text_t weaponPanelFilterFont = {
	0.17f, 0.17f,
	{ 1.0f, 1.0f, 1.0f, 0.6f },
	0, 0,
	&cgs.media.limboFont1_lo,
};

panel_button_text_t weaponPanelStatsFont = {
	0.15f, 0.17f,
	{ 1.0f, 1.0f, 1.0f, 0.6f },
	0, 0,
	&cgs.media.limboFont1_lo,
};

panel_button_text_t weaponPanelStatsPercFont = {
	0.2f, 0.2f,
	{ 1.0f, 1.0f, 1.0f, 0.6f },
	0, 0,
	&cgs.media.limboFont1,
};

panel_button_text_t objectivePanelTxt = {
	0.2f, 0.2f,
	{ 0.0f, 0.0f, 0.0f, 0.5f },
	0, 0,
	&cgs.media.limboFont2,
};


panel_button_t rightLimboPannel = {
	"gfx/limbo/limbo_back",
	NULL,
	{ 440, 0, 200, 480 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/	
	BG_PanelButtonsRender_Img,
	NULL,
};

#define MEDAL_PIC_GAP	((MEDAL_PIC_SIZE - (MEDAL_PIC_WIDTH * MEDAL_PIC_COUNT)) / (MEDAL_PIC_COUNT + 1.f))
#define MEDAL_PIC_COUNT	7.f
#define MEDAL_PIC_WIDTH	22.f
#define MEDAL_PIC_X		450.f
#define MEDAL_PIC_SIZE	(630.f - MEDAL_PIC_X)
#define MEDAL_PIC( number  )				\
panel_button_t medalPic##number = {			\
	NULL,									\
	NULL,									\
	{ MEDAL_PIC_X + MEDAL_PIC_GAP + (number*(MEDAL_PIC_GAP + MEDAL_PIC_WIDTH)), 119, MEDAL_PIC_WIDTH, 26 },	\
	{ number, 0, 0, 0, 0, 0, 0, 0 },		\
	NULL,	/* font		*/					\
	NULL,	/* keyDown	*/					\
	NULL,	/* keyUp	*/					\
	CG_LimboPanel_RenderMedal,				\
	NULL,									\
}

MEDAL_PIC( 0 );
MEDAL_PIC( 1 );
MEDAL_PIC( 2 );
MEDAL_PIC( 3 );
MEDAL_PIC( 4 );
MEDAL_PIC( 5 );
MEDAL_PIC( 6 );


#define TEAM_COUNTER_GAP	((TEAM_COUNTER_SIZE - (TEAM_COUNTER_WIDTH * TEAM_COUNTER_COUNT)) / (TEAM_COUNTER_COUNT + 1.f))
#define TEAM_COUNTER_COUNT	3.f
#define TEAM_COUNTER_WIDTH	20.f
#define TEAM_COUNTER_X		432.f
#define TEAM_COUNTER_SIZE	(660.f - TEAM_COUNTER_X)
#define TEAM_COUNTER_BUTTON_DIFF -24.f
#define TEAM_COUNTER_SPACING	4.f

#define TEAM_COUNTER( number  )				\
panel_button_t teamCounter##number = {		\
	NULL,									\
	NULL,									\
	{ TEAM_COUNTER_X + TEAM_COUNTER_GAP + (number*(TEAM_COUNTER_GAP + TEAM_COUNTER_WIDTH)), 236, TEAM_COUNTER_WIDTH, 14 },	\
	{ 1, number, 0, 0, 0, 0, 0, 0 },		\
	NULL,	/* font		*/					\
	NULL,	/* keyDown	*/					\
	NULL,	/* keyUp	*/					\
	CG_LimboPanel_RenderCounter,			\
	NULL,									\
};											\
panel_button_t teamCounterLight##number = {	\
	NULL,									\
	NULL,									\
	{ TEAM_COUNTER_X + TEAM_COUNTER_GAP + (number*(TEAM_COUNTER_GAP + TEAM_COUNTER_WIDTH)) - 20, 236, 16, 16 },	\
	{ 1, number, 0, 0, 0, 0, 0, 0 },		\
	NULL,	/* font		*/					\
	NULL,	/* keyDown	*/					\
	NULL,	/* keyUp	*/					\
	CG_LimboPanel_RenderLight,				\
	NULL,									\
};											\
panel_button_t teamButton##number = {		\
	NULL,									\
	NULL,									\
	{ TEAM_COUNTER_X + TEAM_COUNTER_GAP + (number*(TEAM_COUNTER_GAP + TEAM_COUNTER_WIDTH) + (TEAM_COUNTER_BUTTON_DIFF/2.f)) - 17 + TEAM_COUNTER_SPACING, \
	  188 + TEAM_COUNTER_SPACING, \
	  TEAM_COUNTER_WIDTH - TEAM_COUNTER_BUTTON_DIFF + 20 - 2 * TEAM_COUNTER_SPACING, \
	  44 - 2 * TEAM_COUNTER_SPACING },	\
	{ number, 0, 0, 0, 0, 0, 0, 0 },		\
	NULL,	/* font		*/					\
	CG_LimboPanel_TeamButton_KeyDown,	/* keyDown	*/\
	NULL,	/* keyUp	*/					\
	CG_LimboPanel_RenderTeamButton,			\
	NULL,									\
}

TEAM_COUNTER( 0 );
TEAM_COUNTER( 1 );
TEAM_COUNTER( 2 );

#define CLASS_COUNTER_GAP	((CLASS_COUNTER_SIZE - (CLASS_COUNTER_WIDTH * CLASS_COUNTER_COUNT)) / (CLASS_COUNTER_COUNT + 1.f))
#define CLASS_COUNTER_COUNT	5.f
#define CLASS_COUNTER_WIDTH 20.f
#define CLASS_COUNTER_X		435.f
#define CLASS_COUNTER_SIZE	(645.f - CLASS_COUNTER_X)
#define CLASS_COUNTER_LIGHT_DIFF 4.f
#define CLASS_COUNTER_BUTTON_DIFF -18.f
#define CLASS_COUNTER( number  )			\
panel_button_t classCounter##number = {		\
	NULL,									\
	NULL,									\
	{ CLASS_COUNTER_X + CLASS_COUNTER_GAP + (number*(CLASS_COUNTER_GAP + CLASS_COUNTER_WIDTH)), 302, CLASS_COUNTER_WIDTH, 14 },	\
	{ 0, number, 0, 0, 0, 0, 0, 0 },		\
	NULL,	/* font		*/					\
	NULL,	/* keyDown	*/					\
	NULL,	/* keyUp	*/					\
	CG_LimboPanel_RenderCounter,			\
	NULL,									\
};											\
panel_button_t classButton##number = {		\
	NULL,									\
	NULL,									\
	{ CLASS_COUNTER_X + CLASS_COUNTER_GAP + (number*(CLASS_COUNTER_GAP + CLASS_COUNTER_WIDTH)) + (CLASS_COUNTER_BUTTON_DIFF/2.f), 266, CLASS_COUNTER_WIDTH - CLASS_COUNTER_BUTTON_DIFF, 34 },	\
	{ 0, number, 0, 0, 0, 0, 0, 0 },		\
	NULL,	/* font		*/					\
	CG_LimboPanel_ClassButton_KeyDown,	/* keyDown	*/	\
	NULL,	/* keyUp	*/					\
	CG_LimboPanel_RenderClassButton,		\
	NULL,									\
}
/*panel_button_t classCounterLight##number = {\
	NULL,									\
	NULL,									\
	{ CLASS_COUNTER_X + CLASS_COUNTER_GAP + (number*(CLASS_COUNTER_GAP + CLASS_COUNTER_WIDTH)) + (CLASS_COUNTER_LIGHT_DIFF/2.f), 266, CLASS_COUNTER_WIDTH - CLASS_COUNTER_LIGHT_DIFF, 16 },	\
	{ 0, number, 0, 0, 0, 0, 0, 0 },		\
	NULL,									\
	CG_LimboPanel_ClassButton_KeyDown,		\
	NULL,									\
	CG_LimboPanel_RenderLight,				\
	NULL,									\
};											\*/

panel_button_t classBar = {
	"gfx/limbo/lightup_bar",
	NULL,
	{ 470, 320, 140, 20 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/	
	BG_PanelButtonsRender_Img,
	NULL,
};

panel_button_t classBarText = {
	NULL,
	NULL,
	{ 460, 334, 160, 16 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&classBarFont,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/	
	CG_LimboPanel_ClassBar_Draw,
	NULL,
};

CLASS_COUNTER( 0 );
CLASS_COUNTER( 1 );
CLASS_COUNTER( 2 );
CLASS_COUNTER( 3 );
CLASS_COUNTER( 4 );

#define FILTER_BUTTON( number )	\
panel_button_t filterButton##number = {	\
	NULL,								\
	NULL,								\
	{ 15, 54+(number*31), 26, 26 },		\
	{ number, 0, 0, 0, 0, 0, 0, 0 },	\
	NULL,	/* font		*/				\
	CG_LimboPanel_Filter_KeyDown,	/* keyDown	*/				\
	NULL,	/* keyUp	*/				\
	CG_LimboPanel_Filter_Draw,			\
	NULL,								\
}

FILTER_BUTTON( 0 );
FILTER_BUTTON( 1 );
FILTER_BUTTON( 2 );
FILTER_BUTTON( 3 );
FILTER_BUTTON( 4 );
FILTER_BUTTON( 5 );
FILTER_BUTTON( 6 );
FILTER_BUTTON( 7 );

panel_button_t filterTitleText = {
	NULL,
	"FILTERS",
	{ 8, 36, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&weaponPanelFilterFont,	/* font		*/
	NULL,					/* keyDown	*/
	NULL,					/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

#define LEFT_FRAME( shader, number, x, y, w, h )\
panel_button_t leftFrame0##number = {	\
	shader,								\
	NULL,								\
	{ x, y, w, h },						\
	{ 0, 0, 0, 0, 0, 0, 0, 0 },			\
	NULL,	/* font		*/				\
	NULL,	/* keyDown	*/				\
	NULL,	/* keyUp	*/				\
	BG_PanelButtonsRender_Img,			\
	NULL,								\
}

#define LF_X1 64
#define LF_X2 416
#define LF_X3 440

#define LF_W1 (LF_X1 - 0)
#define LF_W2 (LF_X2 - LF_X1)
#define LF_W3 (LF_X3 - LF_X2)

#define LF_Y1 23
#define LF_Y2 375
#define LF_Y3 480

#define LF_H1 (LF_Y1 - 0)
#define LF_H2 (LF_Y2 - LF_Y1)
#define LF_H3 (LF_Y3 - LF_Y2)

LEFT_FRAME( "gfx/limbo/limbo_frame01",	1, 0,		0,		LF_W1,	LF_H1 );
LEFT_FRAME( "gfx/limbo/limbo_frame02",	2, LF_X1,	0,		LF_W2,	LF_H1 );
LEFT_FRAME( "gfx/limbo/limbo_frame03",	3, LF_X2,	0,		LF_W3,	LF_H1 );

LEFT_FRAME( "gfx/limbo/limbo_frame04",	4, LF_X2,	LF_Y1,	LF_W3,	LF_H2 );

LEFT_FRAME( "gfx/limbo/limbo_frame05",	5, LF_X2,	LF_Y2,	LF_W3,	LF_H3 );
LEFT_FRAME( "gfx/limbo/limbo_frame06",	6, LF_X1,	LF_Y2,	LF_W2,	LF_H3 );
LEFT_FRAME( "gfx/limbo/limbo_frame07",	7, 0,		LF_Y2,	LF_W1,	LF_H3 );

LEFT_FRAME( "gfx/limbo/limbo_frame08",	8, 0,		LF_Y1,	LF_W1,	LF_H2 );

panel_button_t playerLimboHead = {
	NULL,
	NULL,
	{ 456, 30, 68, 84 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderHead,
	NULL,
};

panel_button_t playerXPCounterText = {
	NULL,
	"XP",
	{ 546, 108, 60, 16 },
	{ 2, 0, 0, 0, 0, 0, 0, 0 },
	&spawnLimboFont,	/* font		*/
	NULL,				/* keyDown	*/
	NULL,				/* keyUp	*/
	CG_LimboPanelRenderText_NoLMS,
	NULL,
};

panel_button_t playerXPCounter = {
	NULL,
	NULL,
	{ 564, 96, 60, 16 },
	{ 2, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};

panel_button_t playerSkillCounter0 = {
	NULL,
	NULL,
	{ 552, 36, 60, 16 },
	{ 4, 0, 0, 0, 0, 0, 4, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};

panel_button_t playerSkillCounter1 = {
	NULL,
	NULL,
	{ 552, 56, 60, 16 },
	{ 4, 1, 0, 0, 0, 0, 4, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};

panel_button_t playerSkillCounter2 = {
	NULL,
	NULL,
	{ 552, 76, 60, 16 },
	{ 4, 2, 0, 0, 0, 0, 4, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};

panel_button_t playerSkillIcon0 = {
	NULL,
	NULL,
	{ 532, 36, 16, 16 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderSkillIcon,
	NULL,
};

panel_button_t playerSkillIcon1 = {
	NULL,
	NULL,
	{ 532, 56, 16, 16 },
	{ 1, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderSkillIcon,
	NULL,
};

panel_button_t playerSkillIcon2 = {
	NULL,
	NULL,
	{ 532, 76, 16, 16 },
	{ 2, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderSkillIcon,
	NULL,
};

// =======================


panel_button_t mapTimeCounter = {
	NULL,
	NULL,
	{ 276, 5, 20, 14 },
	{ 5, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};

panel_button_t mapTimeCounter2 = {
	NULL,
	NULL,
	{ 252, 5, 20, 14 },
	{ 5, 1, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};

panel_button_t mapTimeCounterText = {
	NULL,
	"MISSION TIME",
	{ 172, 16, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&spawnLimboFont,/* font		*/
	NULL,			/* keyDown	*/
	NULL,			/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

// =======================

panel_button_t respawnCounter = {
	NULL,
	NULL,
	{ 400, 5, 20, 14 },
	{ 3, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};

panel_button_t respawnCounterText = {
	NULL,
	"REINFORCEMENTS",
	{ 300, 16, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&spawnLimboFont,/* font		*/
	NULL,			/* keyDown	*/
	NULL,			/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

// =======================

panel_button_t limboTitleText = {
	NULL,
	"COMMAND MAP",
	{ 8, 16, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&titleLimboFont,/* font		*/
	NULL,			/* keyDown	*/
	NULL,			/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t playerSetupText = {
	NULL,
	"PLAYER SETUP",
	{ 448, 16, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&titleLimboFont,/* font		*/
	NULL,			/* keyDown	*/
	NULL,			/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t skillsText = {
	NULL,
	"SKILLS",
	{ 532, 32, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&weaponPanelStatsFont,	/* font		*/
	NULL,					/* keyDown	*/
	NULL,					/* keyUp	*/
	CG_LimboPanelRenderText_SkillsText,
	NULL,
};

// =======================

panel_button_t weaponPanel = {
	NULL,
	NULL,
	{ 455, 353, 140, 56 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,								/* font		*/
	CG_LimboPanel_WeaponPanel_KeyDown,	/* keyDown	*/
	CG_LimboPanel_WeaponPanel_KeyUp,	/* keyUp	*/
	CG_LimboPanel_WeaponPanel,
	NULL,
};

panel_button_t weaponLight1 = {
	NULL,
	NULL,
	{ 605, 362, 20, 20 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	CG_LimboPanel_WeaponLights_KeyDown,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_WeaponLights,
	NULL,
};

panel_button_t weaponLight1Text = {
	NULL,
	"1",
	{ 609, 378, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&weaponButtonFont,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t weaponLight2 = {
	NULL,
	NULL,
	{ 605, 386, 20, 20 },
	{ 1, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	CG_LimboPanel_WeaponLights_KeyDown,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_WeaponLights,
	NULL,
};

panel_button_t weaponLight2Text = {
	NULL,
	"2",
	{ 609, 402, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&weaponButtonFont,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t weaponStatsShotsText = {
	NULL,
	"SHOTS",
	{ 460, 422, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&weaponPanelStatsFont,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t weaponStatsShotsCounter = {
	NULL,
	NULL,
	{ 460, 426, 40, 14 },
	{ 6, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};


panel_button_t weaponStatsHitsText = {
	NULL,
	"HITS",
	{ 516, 422, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&weaponPanelStatsFont,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t weaponStatsHitsCounter = {
	NULL,
	NULL,
	{ 516, 426, 40, 14 },
	{ 6, 1, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};


panel_button_t weaponStatsAccText = {
	NULL,
	"ACC",
	{ 570, 422, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&weaponPanelStatsFont,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t weaponStatsAccCounter = {
	NULL,
	NULL,
	{ 570, 426, 30, 14 },
	{ 6, 2, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCounter,
	NULL,
};

panel_button_t weaponStatsAccPercentage = {
	NULL,
	"%",
	{ 600, 436, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&weaponPanelStatsPercFont,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

// =======================

panel_button_t commandmapPanel = {
	NULL,
	NULL,
	{ CC_2D_X, CC_2D_Y, CC_2D_W, CC_2D_H },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderCommandMap,
	NULL,
};

// =======================

panel_button_t objectivePanel = {
	NULL,
	NULL,
	{ 8, 398, 240, 74 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/
	CG_LimboPanel_RenderObjectiveBack,
	NULL,
};

panel_button_t objectivePanelText = {
	NULL,
	NULL,
	{ 8, 398, 240, 74 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&objectivePanelTxt,		/* font		*/
	NULL,					/* keyDown	*/
	NULL,					/* keyUp	*/
	CG_LimboPanel_RenderObjectiveText,
	NULL,
};

panel_button_t objectivePanelTitle = {
	NULL,
	"OBJECTIVES",
	{ 8, 392, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&titleLimboFont,	/* font		*/
	NULL,				/* keyDown	*/
	NULL,				/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t objectivePanelButtonUp = {
	"gfx/limbo/but_objective_up",
	NULL,
	{ 252, 416, 24, 24 },
	{ 0, 0, 0, 0, 0, 0, 0, 1 },
	NULL,									/* font		*/
	CG_LimboPanel_ObjectiveText_KeyDown,	/* keyDown	*/
	NULL,									/* keyUp	*/
	BG_PanelButtonsRender_Img,
	NULL,
};

panel_button_t briefingButton = {
	NULL,
	NULL,
	{ 252, 388, 24, 24 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,									/* font		*/
	CG_LimboPanel_BriefingButton_KeyDown,	/* keyDown	*/
	NULL,									/* keyUp	*/
	CG_LimboPanel_BriefingButton_Draw,
	NULL,
};

panel_button_t objectivePanelButtonDown = {
	"gfx/limbo/but_objective_dn",
	NULL,
	{ 252, 444, 24, 24 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,									/* font		*/
	CG_LimboPanel_ObjectiveText_KeyDown,	/* keyDown	*/
	NULL,									/* keyUp	*/
	BG_PanelButtonsRender_Img,
	NULL,
};

// =======================

panel_button_t okButtonText = {
	NULL,
	"OK",
	{ 484, 469, 100, 40 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&titleLimboFont,					/* font		*/
	NULL,								/* keyDown	*/
	NULL,								/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t okButton = {
	NULL,
	NULL,
	{ 454+2, 454+2, 82-4, 18-4 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,								/* font		*/
	CG_LimboPanel_OkButton_KeyDown,		/* keyDown	*/
	NULL,								/* keyUp	*/
	CG_LimboPanel_Border_Draw,
	NULL,
};

panel_button_t cancelButtonText = {
	NULL,
	"CANCEL",
	{ 556, 469, 100, 40 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&titleLimboFont,					/* font		*/
	NULL,								/* keyDown	*/
	NULL,								/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t cancelButton = {
	NULL,
	NULL,
	{ 543+2, 454+2, 82-4, 18-4 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,								/* font		*/
	CG_LimboPanel_CancelButton_KeyDown,		/* keyDown	*/
	NULL,								/* keyUp	*/
	CG_LimboPanel_Border_Draw,
	NULL,
};

// =======================

panel_button_t nameEdit = {
	NULL,
	"limboname",
	{ 480, 150, 120, 20 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&nameEditFont,						/* font		*/
	BG_PanelButton_EditClick,			/* keyDown	*/
	NULL,								/* keyUp	*/
	BG_PanelButton_RenderEdit,
	CG_LimboPanel_NameEditFinish,
};

panel_button_t plusButton = {
	NULL,
	NULL,
	{ 19, 320, 18, 14 },
	{ 12, 0, 0, 0, 0, 0, 0, 0 },
	NULL,								/* font		*/
	CG_LimboPanel_PlusButton_KeyDown,	/* keyDown	*/
	NULL,								/* keyUp	*/
	CG_LimboPanel_Border_Draw,
	NULL,
};

panel_button_t plusButtonText = {
	NULL,
	"+",
	{ 19, 321, 18, 14 },
	{ 12, 0, 0, 0, 0, 0, 0, 0 },
	&titleLimboFontBigCenter,			/* font		*/
	NULL,								/* keyDown	*/
	NULL,								/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t minusButton = {
	NULL,
	NULL,
	{ 19, 346, 18, 14 },
	{ 12, 0, 0, 0, 0, 0, 0, 0 },
	NULL,								/* font		*/
	CG_LimboPanel_MinusButton_KeyDown,	/* keyDown	*/
	NULL,								/* keyUp	*/
	CG_LimboPanel_Border_Draw,
	NULL,
};

panel_button_t minusButtonText = {
	NULL,
	"-",
	{ 19, 346, 18, 14 },
	{ 12, 0, 0, 0, 0, 0, 0, 0 },
	&titleLimboFontBigCenter,			/* font		*/
	NULL,								/* keyDown	*/
	NULL,								/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

/* Original 35-panel deployment table. See limbo_layout.json for both binary addresses.
 * Layout integration is partial: server identity/loadout callbacks remain SDK. */
static panel_button_text_t titleLimboFontBigGold = { .35f,.45f,{.45f,.4f,.3f,1},3,0,&cgs.media.limboFont2 };
static panel_button_text_t classLimboFont = { .16f,.2f,{.5f,.5f,.5f,1},0,0,&cgs.media.limboFont1 };
static panel_button_text_t weaponButtonFontSmall = { .22f,.22f,{.6f,.6f,.6f,1},0,0,&cgs.media.limboFont2 };
static panel_button_t rightLimboPannelGunMen;
static panel_button_t classButton5;
static panel_button_t nameSetupText;
static panel_button_t teamSetupText;
static panel_button_t gearSetupText;
static panel_button_t classSetupText1;
static panel_button_t classSetupText2;
static panel_button_t classSetupText3;
static panel_button_t weaponSetupText;
static panel_button_t weaponLight3;
static panel_button_t weaponLight3Text;
panel_button_t* limboPanelButtons[] = {
    &rightLimboPannel,
    &rightLimboPannelGunMen,
    &classButton0,
    &classButton1,
    &classButton2,
    &classButton3,
    &classButton4,
    &classButton5,
    &teamButton0,
    &teamButton1,
    &teamButton2,
    &teamCounter0,
    &teamCounter1,
    &teamCounter2,
    &playerLimboHead,
    &nameSetupText,
    &playerSetupText,
    &teamSetupText,
    &gearSetupText,
    &classSetupText1,
    &classSetupText2,
    &classSetupText3,
    &weaponSetupText,
    &okButton,
    &okButtonText,
    &cancelButton,
    &cancelButtonText,
    &nameEdit,
    &weaponLight1,
    &weaponLight2,
    &weaponLight3,
    &weaponLight1Text,
    &weaponLight2Text,
    &weaponLight3Text,
    &weaponPanel,
    NULL
};
static void TCE_LimboLayout(void) {
    { panel_button_t value = { "gfx/limbo/limbo_back", NULL, {632.0,50.0,200.0,364.0}, {0,0,0,0,0,0,0,0}, NULL, NULL, NULL, BG_PanelButtonsRender_Img, NULL, 0 }; rightLimboPannel = value; }
    { panel_button_t value = { "ui/assets/tce_gunmen256", NULL, {636.0,34.0,50.0,50.0}, {0,0,0,0,0,0,0,0}, NULL, NULL, NULL, BG_PanelButtonsRender_Img, NULL, 0 }; rightLimboPannelGunMen = value; }
    { panel_button_t value = { NULL, NULL, {636.0,274.0,32.0,22.0}, {0,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_ClassButton_KeyDown, NULL, CG_LimboPanel_RenderClassButton, NULL, 0 }; classButton0 = value; }
    { panel_button_t value = { NULL, NULL, {668.0,274.0,32.0,22.0}, {0,1,0,0,0,0,0,0}, NULL, CG_LimboPanel_ClassButton_KeyDown, NULL, CG_LimboPanel_RenderClassButton, NULL, 0 }; classButton1 = value; }
    { panel_button_t value = { NULL, NULL, {700.0,274.0,32.0,22.0}, {0,2,0,0,0,0,0,0}, NULL, CG_LimboPanel_ClassButton_KeyDown, NULL, CG_LimboPanel_RenderClassButton, NULL, 0 }; classButton2 = value; }
    { panel_button_t value = { NULL, NULL, {732.0,274.0,32.0,22.0}, {0,3,0,0,0,0,0,0}, NULL, CG_LimboPanel_ClassButton_KeyDown, NULL, CG_LimboPanel_RenderClassButton, NULL, 0 }; classButton3 = value; }
    { panel_button_t value = { NULL, NULL, {764.0,274.0,32.0,22.0}, {0,4,0,0,0,0,0,0}, NULL, CG_LimboPanel_ClassButton_KeyDown, NULL, CG_LimboPanel_RenderClassButton, NULL, 0 }; classButton4 = value; }
    { panel_button_t value = { NULL, NULL, {796.0,274.0,32.0,22.0}, {0,5,0,0,0,0,0,0}, NULL, CG_LimboPanel_ClassButton_KeyDown, NULL, CG_LimboPanel_RenderClassButton, NULL, 0 }; classButton5 = value; }
    { panel_button_t value = { NULL, NULL, {641.0,197.0,56.0,36.0}, {0,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_TeamButton_KeyDown, NULL, CG_LimboPanel_RenderTeamButton, NULL, 0 }; teamButton0 = value; }
    { panel_button_t value = { NULL, NULL, {703.0,197.0,56.0,36.0}, {1,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_TeamButton_KeyDown, NULL, CG_LimboPanel_RenderTeamButton, NULL, 0 }; teamButton1 = value; }
    { panel_button_t value = { NULL, NULL, {765.0,197.0,56.0,36.0}, {2,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_TeamButton_KeyDown, NULL, CG_LimboPanel_RenderTeamButton, NULL, 0 }; teamButton2 = value; }
    { panel_button_t value = { NULL, NULL, {680.0,182.0,20.0,14.0}, {1,0,0,0,0,0,0,0}, NULL, NULL, NULL, CG_LimboPanel_RenderTeamCounter, NULL, 0 }; teamCounter0 = value; }
    { panel_button_t value = { NULL, NULL, {742.0,182.0,20.0,14.0}, {1,1,0,0,0,0,0,0}, NULL, NULL, NULL, CG_LimboPanel_RenderTeamCounter, NULL, 0 }; teamCounter1 = value; }
    { panel_button_t value = { NULL, NULL, {804.0,182.0,20.0,14.0}, {1,2,0,0,0,0,0,0}, NULL, NULL, NULL, CG_LimboPanel_RenderTeamCounter, NULL, 0 }; teamCounter2 = value; }
    { panel_button_t value = { NULL, NULL, {777.0,80.0,43.0,56.0}, {0,0,0,0,0,0,0,0}, NULL, NULL, NULL, CG_LimboPanel_RenderHead, NULL, 0 }; playerLimboHead = value; }
    { panel_button_t value = { NULL, "Name", {640.0,154.0,0.0,0.0}, {0,0,0,0,0,0,0,0}, &titleLimboFont, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; nameSetupText = value; }
    { panel_button_t value = { NULL, "Deployment Menu", {680.0,71.0,0.0,0.0}, {0,0,0,0,0,0,0,0}, &titleLimboFontBigGold, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; playerSetupText = value; }
    { panel_button_t value = { NULL, "Team", {640.0,192.0,0.0,0.0}, {0,0,0,0,0,0,0,0}, &titleLimboFont, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; teamSetupText = value; }
    { panel_button_t value = { NULL, "Identity", {640.0,253.0,0.0,0.0}, {0,0,0,0,0,0,0,0}, &titleLimboFont, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; gearSetupText = value; }
    { panel_button_t value = { NULL, "[ Assault ]", {642.0,268.0,0.0,0.0}, {0,0,0,0,0,0,0,0}, &classLimboFont, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; classSetupText1 = value; }
    { panel_button_t value = { NULL, "[  Recon  ]", {707.0,268.0,0.0,0.0}, {0,0,0,0,0,0,0,0}, &classLimboFont, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; classSetupText2 = value; }
    { panel_button_t value = { NULL, "[  Sniper  ]", {770.0,268.0,0.0,0.0}, {0,0,0,0,0,0,0,0}, &classLimboFont, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; classSetupText3 = value; }
    { panel_button_t value = { NULL, "Gear", {640.0,316.0,0.0,0.0}, {0,0,0,0,0,0,0,0}, &titleLimboFont, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; weaponSetupText = value; }
    { panel_button_t value = { NULL, NULL, {642.0,388.0,86.0,18.0}, {0,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_OkButton_KeyDown, NULL, CG_LimboPanel_Border_Draw, NULL, 0 }; okButton = value; }
    { panel_button_t value = { NULL, "OK", {676.0,403.0,100.0,40.0}, {0,0,0,0,0,0,0,0}, &titleLimboFont, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; okButtonText = value; }
    { panel_button_t value = { NULL, NULL, {735.0,388.0,86.0,18.0}, {0,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_CancelButton_KeyDown, NULL, CG_LimboPanel_Border_Draw, NULL, 0 }; cancelButton = value; }
    { panel_button_t value = { NULL, "CANCEL", {748.0,403.0,100.0,40.0}, {0,0,0,0,0,0,0,0}, &titleLimboFont, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; cancelButtonText = value; }
    { panel_button_t value = { NULL, "limboname", {648.0,152.0,120.0,20.0}, {0,0,0,0,0,0,0,0}, &nameEditFont, BG_PanelButton_EditClick, NULL, BG_PanelButton_RenderEdit, CG_LimboPanel_NameEditFinish, 0 }; nameEdit = value; }
    { panel_button_t value = { NULL, NULL, {642.0,320.0,55.0,18.0}, {1,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_WeaponLights_KeyDown, NULL, CG_LimboPanel_WeaponLights, NULL, 0 }; weaponLight1 = value; }
    { panel_button_t value = { NULL, NULL, {704.0,320.0,55.0,18.0}, {0,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_WeaponLights_KeyDown, NULL, CG_LimboPanel_WeaponLights, NULL, 0 }; weaponLight2 = value; }
    { panel_button_t value = { NULL, NULL, {766.0,320.0,55.0,18.0}, {2,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_WeaponLights_KeyDown, NULL, CG_LimboPanel_WeaponLights, NULL, 0 }; weaponLight3 = value; }
    { panel_button_t value = { NULL, "Primary", {644.0,333.0,55.0,18.0}, {0,0,0,0,0,0,0,0}, &weaponButtonFontSmall, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; weaponLight1Text = value; }
    { panel_button_t value = { NULL, "Sidearm", {706.0,333.0,55.0,18.0}, {0,0,0,0,0,0,0,0}, &weaponButtonFontSmall, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; weaponLight2Text = value; }
    { panel_button_t value = { NULL, "Special", {769.0,333.0,55.0,18.0}, {0,0,0,0,0,0,0,0}, &weaponButtonFontSmall, NULL, NULL, BG_PanelButtonsRender_Text, NULL, 0 }; weaponLight3Text = value; }
    { panel_button_t value = { NULL, NULL, {642.0,346.0,180.0,30.0}, {0,0,0,0,0,0,0,0}, NULL, CG_LimboPanel_WeaponPanel_KeyDown, CG_LimboPanel_WeaponPanel_KeyUp, CG_LimboPanel_WeaponPanel, NULL, 0 }; weaponPanel = value; }
    titleLimboFont.colour[0] = titleLimboFont.colour[1] = titleLimboFont.colour[2] = .6f;
    titleLimboFont.colour[3] = 1; titleLimboFont.font = &cgs.media.limboFont2;
}


qboolean CG_LimboPanel_BriefingButton_KeyDown( panel_button_t* button, int key ) {
	if( cg_gameType.integer == GT_WOLF_LMS ) {
		return qfalse;
	}

	if( key == K_MOUSE1 ) {

		SOUND_SELECT;

		if( cg.limboEndCinematicTime > cg.time ) {
			trap_S_StopStreamingSound( -1 );
			cg.limboEndCinematicTime = 0;

			return qtrue;
		}

		cg.limboEndCinematicTime = cg.time + CG_SoundPlaySoundScript( va( "news_%s", cgs.rawmapname ), NULL, -1, qfalse );

		return qtrue;
	}

	return qfalse;
}

void CG_LimboPanel_BriefingButton_Draw( panel_button_t* button ) {
	if( cg_gameType.integer == GT_WOLF_LMS ) {
		return;
	}

	if( cg.limboEndCinematicTime > cg.time ) {
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, BG_CursorInRect( &button->rect ) ? cgs.media.limboBriefingButtonStopOn : cgs.media.limboBriefingButtonStopOff );
	} else {
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, BG_CursorInRect( &button->rect ) ? cgs.media.limboBriefingButtonOn : cgs.media.limboBriefingButtonOff );
	}
}

void CG_LimboPanel_NameEditFinish( panel_button_t* button ) {
	char buffer[256];
	trap_Cvar_VariableStringBuffer( button->text, buffer, 256 );
	trap_Cvar_Set( "name", buffer );
}

qboolean CG_LimboPanel_CancelButton_KeyDown( panel_button_t* button, int key ) {
	if( key == K_MOUSE1 ) {
		SOUND_CANCEL;

		if( cgs.limboLoadoutModified ) {
			trap_SendClientCommand( "rs" );

			cgs.limboLoadoutSelected = qfalse;
		}

		CG_EventHandling( CGAME_EVENT_NONE, qfalse );

		return qtrue;
	}
	return qfalse;
}

qboolean CG_LimboPanel_PlusButton_KeyDown( panel_button_t* button, int key ) {
	if( key == K_MOUSE1 ) {
		SOUND_SELECT;

		cgs.ccZoomFactor /= 0.75f;
		
		if( cgs.ccZoomFactor > 1.f ) {
			cgs.ccZoomFactor = 1.f;
		}

		return qtrue;
	}

	return qfalse;
}

qboolean CG_LimboPanel_MinusButton_KeyDown( panel_button_t* button, int key ) {
	if( key == K_MOUSE1 ) {
		SOUND_SELECT;

		cgs.ccZoomFactor *= 0.75f;

		if( cgs.ccZoomFactor < (0.75f*0.75f*0.75f*0.75f*0.75f) ) {
			cgs.ccZoomFactor = (0.75f*0.75f*0.75f*0.75f*0.75f);
		}

		return qtrue;
	}

	return qfalse;
}

/* Windows300407f0: full deployment command and popup; the last argument
 * is deliberately zero in the original Windows and Linux clients. */
void CG_LimboPanel_SendSetupMsg(qboolean forceteam) {
    int weapon1, weapon2, weapon3;
    team_t team = forceteam ? CG_LimboPanel_GetTeam() : cgs.clientinfo[cg.clientNum].team;
    const char *code, *label, *name;
    if (team == TEAM_SPECTATOR) {
        if (forceteam) {
            if (cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR)
                trap_SendClientCommand("team s 0 0 0 0\n");
            CG_EventHandling(CGAME_EVENT_NONE, qfalse);
        }
        return;
    }
    weapon1 = CG_LimboPanel_GetSelectedWeaponForSlot(1);
    weapon2 = CG_LimboPanel_GetSelectedWeaponForSlot(0);
    weapon3 = CG_LimboPanel_GetSelectedWeaponForSlot(2);
    code = team == TEAM_AXIS ? "r" : team == TEAM_ALLIES ? "b" : "s";
    trap_SendClientCommand(va("team %s %i %i %i %i %i\n", code,
        CG_LimboPanel_GetClass(), weapon1, weapon2, weapon3, 0));
    if (forceteam) CG_EventHandling(CGAME_EVENT_NONE, qfalse);
    team = CG_LimboPanel_GetTeam();
    label = team == TEAM_AXIS ? "Terrorist" : team == TEAM_ALLIES ? "Specops" : "unknown";
    /* Invalid IDs cannot index the media array; ordinary selected IDs retain
     * the original short-name text, including an intentionally empty name. */
    name = weapon1 >= 0 && weapon1 < TCE_MAX_WEAPONS ?
        tce_cg_weapons[weapon1].deployMenuShortName : "^1UNKNOWN WEAPON";
    CG_AddPMItem((popupMessageType_t)4, va("You will deploy as a %s with a %s.", label, name),
        cgs.media.pmImages[4]);
    cgs.limboLoadoutSelected = qtrue;
    cgs.limboLoadoutModified = qtrue;
}

qboolean CG_LimboPanel_OkButton_KeyDown( panel_button_t* button, int key ) {
	if( key == K_MOUSE1 ) {
		SOUND_SELECT;

		CG_LimboPanel_SendSetupMsg( qtrue );

		return qtrue;
	}

	return qfalse;
}

qboolean CG_LimboPanel_TeamButton_KeyDown( panel_button_t* button, int key ) {
	if( key == K_MOUSE1 ) {
		SOUND_SELECT;

		if(cgs.ccSelectedTeam != button->data[0]) {
			int oldmax = CG_LimboPanel_GetMaxObjectives();

			cgs.ccSelectedTeam = button->data[0];

			if( cgs.ccSelectedObjective == oldmax ) {
				cgs.ccSelectedObjective = CG_LimboPanel_GetMaxObjectives();
			}

			CG_LimboPanel_SetSelectedWeaponNumForSlot( 0, 0 );

			CG_LimboPanel_RequestWeaponStats();

			cgs.limboLoadoutModified = qtrue;
		}

		return qtrue;
	}

	return qfalse;
}

void CG_LimboPanel_RenderTeamButton( panel_button_t* button ) {
//	vec4_t clr = { 1.f, 1.f, 1.f, 1.0f };
	vec4_t clr2 = { 1.f, 1.f, 1.f, 0.4f };

	qhandle_t shader;

	trap_R_SetColor( colorBlack );
	CG_DrawPic( button->rect.x + 1, button->rect.y + 1, button->rect.w, button->rect.h, cgs.media.limboTeamButtonBack_off );

	trap_R_SetColor( NULL );
	CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.limboTeamButtonBack_off );

	if( CG_LimboPanel_GetTeam() == teamOrder[button->data[0]] ) {
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.limboTeamButtonBack_on );
	} else if( BG_CursorInRect( &button->rect ) ) {
		trap_R_SetColor( clr2 );
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.limboTeamButtonBack_on );
		trap_R_SetColor( NULL );
	}

	switch( button->data[0] ) {
		case 0:
			shader = cgs.media.limboTeamButtonAllies;
			break;
		case 1:
			shader = cgs.media.limboTeamButtonAxis;
			break;
		case 2:
			shader = cgs.media.limboTeamButtonSpec;
			break;
		default:
			return;
	}
	
	trap_R_SetColor( NULL );
	CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, shader );
}

qboolean CG_LimboPanel_ClassButton_KeyDown( panel_button_t* button, int key ) {
	if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR ) {
		return qfalse;
	}

	if( key == K_MOUSE1 ) {
		SOUND_SELECT;

		if( cgs.ccSelectedClass != button->data[1] ) {
			cgs.ccSelectedClass = button->data[1];

			CG_LimboPanel_RequestWeaponStats();
		}

		return qtrue;
	}

	return qfalse;
}

void CG_LimboPanel_ClassBar_Draw( panel_button_t* button ) {
	const char* text = NULL;
	char buffer[64];
	float w;

	if( BG_CursorInRect( &medalPic0.rect ) ) {
		text = skillNames[0];
	} else if( BG_CursorInRect( &medalPic1.rect ) ) {
		text = skillNames[1];
	} else if( BG_CursorInRect( &medalPic2.rect ) ) {
		text = skillNames[2];
	} else if( BG_CursorInRect( &medalPic3.rect ) ) {
		text = skillNames[3];
	} else if( BG_CursorInRect( &medalPic4.rect ) ) {
		text = skillNames[4];
	} else if( BG_CursorInRect( &medalPic5.rect ) ) {
		text = skillNames[5];
	} else if( BG_CursorInRect( &medalPic6.rect ) ) {
		text = skillNames[6];
	} else if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR ) {
		text = "JOIN A TEAM";
	} else if( BG_CursorInRect( &classButton0.rect ) ) {
		text = BG_ClassnameForNumber( 0 );
	} else if( BG_CursorInRect( &classButton1.rect ) ) {
		text = BG_ClassnameForNumber( 1 );
	} else if( BG_CursorInRect( &classButton2.rect ) ) {
		text = BG_ClassnameForNumber( 2 );
	} else if( BG_CursorInRect( &classButton3.rect ) ) {
		text = BG_ClassnameForNumber( 3 );
	} else if( BG_CursorInRect( &classButton4.rect ) ) {
		text = BG_ClassnameForNumber( 4 );
	}

	if( !text ) {
		text = BG_ClassnameForNumber( CG_LimboPanel_GetClass() );
	}

	Q_strncpyz( buffer, text, sizeof( buffer ) );
	Q_strupr( buffer );

	w = CG_Text_Width_Ext( buffer, button->font->scalex, 0, button->font->font );
	CG_Text_Paint_Ext( button->rect.x + (button->rect.w - w) * 0.5f, button->rect.y, button->font->scalex, button->font->scaley, button->font->colour, buffer, 0, 0, button->font->style, button->font->font );	
}

static void TCE_LimboText(float x,float y,float sx,float sy,const float *color,const char *text) {
    CG_Text_Paint_Ext(x,y,sx,sy,(float *)color,text,0,0,0,&cgs.media.limboFont1);
}
static int TCE_LimboTeam(void) { return CG_LimboPanel_GetTeam(); }
static int TCE_LimboClassIndex(void) { return CG_LimboPanel_GetClass(); }
int CG_LimboPanel_RenderCounter_ValueForButton(panel_button_t *button);
static const panel_button_t *tce_counterButton;
static int TCE_LimboCounterValue(const tce_limboButton_t *button) {
    return CG_LimboPanel_RenderCounter_ValueForButton((panel_button_t *)tce_counterButton);
}
static void TCE_LimboBorderCall(float x,float y,float w,float h,int fill,int hover) {
    CG_DrawBorder(x,y,w,h,fill,hover);
}
static void TCE_LimboWidget(panel_button_t *button,void (*draw)(const tce_limboButton_t *,const tce_limboWidgets_t *)) {
    tce_limboButton_t b;
    tce_limboWidgets_t ctx;
    b.x=button->rect.x;b.y=button->rect.y;b.w=button->rect.w;b.h=button->rect.h;
    memcpy(b.data,button->data,sizeof(b.data));
    ctx.classOff=cgs.media.limboClassButton2Back_off;ctx.classOn=cgs.media.limboClassButton2Back_on;
    ctx.weaponOff=cgs.media.limboWeaponNumber_off;ctx.weaponOn=cgs.media.limboWeaponNumber_on;
    ctx.selectedSlot=cgs.ccSelectedWeaponNumber;ctx.team=TCE_LimboTeam;ctx.playerClass=TCE_LimboClassIndex;
    ctx.counter=TCE_LimboCounterValue;ctx.teamOrder=(const int *)teamOrder;
    ctx.pic=CG_DrawPic;ctx.border=TCE_LimboBorderCall;ctx.text=TCE_LimboText;
    tce_counterButton=button;draw(&b,&ctx);tce_counterButton=NULL;
}
static void CG_LimboPanel_RenderTeamCounter(panel_button_t *button) { TCE_LimboWidget(button,TCE_LimboCounter); }

void CG_LimboPanel_RenderClassButton( panel_button_t* button ) {
    TCE_LimboWidget(button,TCE_LimboClass);
}

int CG_LimboPanel_GetMaxObjectives( void ) {
	if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR ) {
		return 0;
	}

	return atoi( Info_ValueForKey( CG_ConfigString( CS_MULTI_INFO ), "numobjectives" ) );
}

qboolean CG_LimboPanel_ObjectiveText_KeyDown( panel_button_t* button, int key ) {
	int max;

	if( key == K_MOUSE1 ) {
		SOUND_SELECT;

		max = CG_LimboPanel_GetMaxObjectives();

		if( button->data[7] == 0 ) {
			if( ++cgs.ccSelectedObjective > max ) {
				cgs.ccSelectedObjective = 0;
			}
		} else {
			if( --cgs.ccSelectedObjective < 0 ) {
				cgs.ccSelectedObjective = max;
			}
		}

		CG_LimboPanel_RequestObjective();

		return qtrue;
	}

	return qfalse;
}

void CG_LimboPanel_RenderObjectiveText( panel_button_t* button ) {
	const char* cs;
	char *info, *s, *p;
	float y;
	char buffer[1024];
	int status = 0;

	if( cg_gameType.integer == GT_WOLF_LMS ) {
		//cs = CG_ConfigString( CS_MULTI_MAPDESC ); 
		//Q_strncpyz( buffer, cs, sizeof(buffer) );

		Q_strncpyz( buffer, cg.objMapDescription_Neutral, sizeof(buffer) );
	} else {
		if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR ) {
			//cs = CG_ConfigString( CS_MULTI_MAPDESC3 );
			//Q_strncpyz( buffer, cs, sizeof(buffer) );

			Q_strncpyz( buffer, cg.objMapDescription_Neutral, sizeof(buffer) );
		} else {
			if( cgs.ccSelectedObjective != CG_LimboPanel_GetMaxObjectives() ) {
				cs = CG_ConfigString( CS_MULTI_OBJECTIVE );

				if( CG_LimboPanel_GetTeam() == TEAM_AXIS ) {
					//info = Info_ValueForKey( cs, "axis_desc" );
					info = cg.objDescription_Axis[cgs.ccSelectedObjective];
					status = atoi(Info_ValueForKey( cs, va( "x%i", cgs.ccSelectedObjective+1 )));
				} else {
					//info = Info_ValueForKey( cs, "allied_desc" );
					info = cg.objDescription_Allied[cgs.ccSelectedObjective];
					status = atoi(Info_ValueForKey( cs, va( "a%i", cgs.ccSelectedObjective+1 )));
				}

				if(!(info && *info)) {
					info = "No Information Supplied";
				}
				
				Q_strncpyz( buffer, info, sizeof(buffer) );
			} else {
				//cs = CG_ConfigString( CG_LimboPanel_GetTeam() == TEAM_AXIS ? CS_MULTI_MAPDESC2 : CS_MULTI_MAPDESC );
				//Q_strncpyz( buffer, cs, sizeof(buffer) );

				if( CG_LimboPanel_GetTeam() == TEAM_AXIS ) {
					Q_strncpyz( buffer, cg.objMapDescription_Axis, sizeof(buffer) );
				} else {
					Q_strncpyz( buffer, cg.objMapDescription_Allied, sizeof(buffer) );
				}
			}
		}
	}

	while ((s = strchr(buffer, '*'))) {
		*s = '\n';
	}

	CG_FitTextToWidth_Ext( buffer, button->font->scalex, button->rect.w - 16, sizeof(buffer), &cgs.media.limboFont2 );

	y = button->rect.y + 12;

	s = p = buffer;
	while(*p) {
		if(*p == '\n') {
			*p++ = '\0';
			CG_Text_Paint_Ext( button->rect.x + 4, y, button->font->scalex, button->font->scaley, button->font->colour, s, 0, 0, 0, &cgs.media.limboFont2 );
			y += 8;
			s = p;
		} else {
			p++; 
		}
	}

	if( cg_gameType.integer != GT_WOLF_LMS && CG_LimboPanel_GetTeam() != TEAM_SPECTATOR ) {
		const char* ofTxt;
		float w, x;

		if( cgs.ccSelectedObjective == CG_LimboPanel_GetMaxObjectives() ) {
			ofTxt = va( "1of%i", CG_LimboPanel_GetMaxObjectives()+1 );
		} else {
			ofTxt = va( "%iof%i", cgs.ccSelectedObjective+2, CG_LimboPanel_GetMaxObjectives()+1 );			
		}
		
		w = CG_Text_Width_Ext( ofTxt, 0.2f, 0, &cgs.media.limboFont2 );
		
		x = button->rect.x + button->rect.w - w - 4;

		CG_Text_Paint_Ext( x, button->rect.y + button->rect.h - 2, 0.2f, 0.2f, colorBlack, ofTxt, 0, 0, 0, &cgs.media.limboFont2 );
	}

	if(status == 1) {
		CG_DrawPic( button->rect.x + 87, button->rect.y + 8, button->rect.w - 174, button->rect.h - 8, cgs.media.ccStamps[0] );
	} else if(status == 2) {
		CG_DrawPic( button->rect.x + 87, button->rect.y + 8, button->rect.w - 174, button->rect.h - 8, cgs.media.ccStamps[1] );
	}
}

void CG_LimboPanel_RenderObjectiveBack( panel_button_t* button ) {
//	int max = CG_LimboPanel_GetMaxObjectives();

//	if( cgs.ccSelectedObjective == max ) {
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.limboObjectiveBack[ TEAM_SPECTATOR - TEAM_AXIS ] );
//	} else {
//		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.limboObjectiveBack[ CG_LimboPanel_GetTeam() - TEAM_AXIS ] );
//	}
}

void CG_LimboPanel_RenderCommandMap( panel_button_t* button ) {
	CG_DrawMap( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.ccFilter, NULL, qtrue, 1.f, qtrue );
	CG_CommandMap_DrawHighlightText();
}

qboolean CG_LimboPanel_RenderLight_GetValue( panel_button_t* button ) {
	switch( button->data[0] ) {
		case 0:
			return (CG_LimboPanel_GetClass() == button->data[1]) ? qtrue : qfalse;
		case 1:
			return (CG_LimboPanel_GetTeam() == teamOrder[button->data[1]]) ? qtrue : qfalse;
	}

	return qfalse;
}

void CG_LimboPanel_RenderLight( panel_button_t* button ) {
	if( CG_LimboPanel_RenderLight_GetValue( button ) ) {
//		if( !button->data[2] || (button->data[2] - cg.time < 0) ) {
			button->data[3] = button->data[3] ^ 1;
//			if( button->data[3] ) {
//				button->data[2] = cg.time + rand() % 200;
//			} else {
//				button->data[2] = cg.time + rand() % 1000;
//			}
//		}

		CG_DrawPic( button->rect.x - 4, button->rect.y - 2, button->rect.w + 4, button->rect.h + 4, button->data[3] ? cgs.media.limboLight_on2 : cgs.media.limboLight_on );
	} else {
		CG_DrawPic( button->rect.x - 4, button->rect.y - 2, button->rect.w + 4, button->rect.h + 4, cgs.media.limboLight_off );	
	}
}

void CG_DrawPlayerHead( rectDef_t *rect, bg_character_t* character, bg_character_t* headcharacter, float yaw, float pitch, qboolean drawHat, hudHeadAnimNumber_t animation, qhandle_t painSkin, int rank, qboolean spectator ) {
	float			len;
	vec3_t			origin;
	vec3_t			mins, maxs, angles;
	float			x, y, w, h;
	refdef_t		refdef;
	refEntity_t		head, hat, mrank;

	if( !character ) {
		return;
	}

	trap_R_SaveViewParms();

	x = rect->x;
	y = rect->y;
	w = rect->w;
	h = rect->h;

	CG_AdjustFrom640( &x, &y, &w, &h );

	memset( &refdef, 0, sizeof( refdef ) );

	refdef.rdflags = RDF_NOWORLDMODEL;
	AxisClear( refdef.viewaxis );

	refdef.fov_x = 8;
	refdef.fov_y = 10;

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	refdef.time = cg.time;

	trap_R_ClearScene();

	// offset the origin y and z to center the head
	trap_R_ModelBounds( character->hudhead, mins, maxs );

	origin[2] = -0.7 * ( mins[2] + maxs[2] );
	/* Original 30041270: lateral framing includes eight percent of width. */
	origin[1] = 0.5 * ( mins[1] + maxs[1] ) - 0.08 * ( maxs[1] - mins[1] );

	// calculate distance so the head nearly fills the box
	// assume heads are taller than wide
	len = 3.5f * ( maxs[2] - mins[2] );		
	origin[0] = len / tan( 20 / 2 ); // 0.268;	// len / tan( fov/2 )

	angles[PITCH] = pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;

	memset( &head, 0, sizeof( head ) );
	AnglesToAxis( angles, head.axis );
	VectorCopy( origin, head.origin );
	head.hModel = headcharacter->hudhead;
	head.customSkin = headcharacter->hudheadskin;
	head.renderfx = RF_NOSHADOW | RF_FORCENOLOD;		// no stencil shadows
	
	// ydnar: light the model with the current lightgrid
	//VectorCopy( cg.refdef.vieworg, head.lightingOrigin );
	if( !cg.showGameView )
		head.renderfx |= /*RF_LIGHTING_ORIGIN |*/ RF_MINLIGHT;

	CG_HudHeadAnimation( headcharacter, &cg.predictedPlayerEntity.pe.hudhead, &head.oldframe, &head.frame, &head.backlerp, animation );
	/* TC heads use the static first frame, after advancing the HUD controller. */
	head.oldframe = head.frame = 0;
	head.backlerp = 0;

	if( drawHat ) {
		memset( &hat, 0, sizeof( hat ) );
		hat.hModel = character->accModels[ACC_HAT];
		hat.customSkin = character->accSkins[ACC_HAT];
		hat.renderfx = RF_NOSHADOW | RF_FORCENOLOD;		// no stencil shadows
		
		// ydnar: light the model with the current lightgrid
		//VectorCopy( cg.refdef.vieworg, hat.lightingOrigin );
		if( !cg.showGameView )
			hat.renderfx |= /*RF_LIGHTING_ORIGIN |*/ RF_MINLIGHT;
		
		CG_PositionEntityOnTag( &hat, &head, "tag_mouth", 0, NULL);

		if( rank ) {
			memset( &mrank, 0, sizeof( mrank ) );

			mrank.hModel = character->accModels[ ACC_RANK ];
			mrank.customShader = rankicons[ rank ][ 1 ].shader;
			mrank.renderfx = RF_NOSHADOW | RF_FORCENOLOD;		// no stencil shadows

			CG_PositionEntityOnTag( &mrank, &head, "tag_mouth", 0, NULL);
		}
	}

	head.shaderRGBA[0] = 255;
	head.shaderRGBA[1] = 255;
	head.shaderRGBA[2] = 255;
	head.shaderRGBA[3] = 255;

	hat.shaderRGBA[0] = 255;
	hat.shaderRGBA[1] = 255;
	hat.shaderRGBA[2] = 255;
	hat.shaderRGBA[3] = 255;

	mrank.shaderRGBA[0] = 255;
	mrank.shaderRGBA[1] = 255;
	mrank.shaderRGBA[2] = 255;
	mrank.shaderRGBA[3] = 255;

	/*if( spectator ) {
		head.customShader = cgs.media.limboSpectator;
		head.customSkin = 0;
	}*/
	trap_R_AddRefEntityToScene( &head );

	if( painSkin ) {
		head.customShader = 0;
		head.customSkin = painSkin;
		trap_R_AddRefEntityToScene( &head );
	}

	if( drawHat ) {
		/*if( spectator ) {
			hat.customShader = cgs.media.limboSpectator;
			hat.customSkin = 0;
		}*/
		trap_R_AddRefEntityToScene( &hat );

		if( rank ) {
			trap_R_AddRefEntityToScene( &mrank );
		}
	}
	trap_R_RenderScene( &refdef );

//bani - render to texture api example
//draws the player head on one of the fueldump textures.
#ifdef TEST_API_RENDERTOTEXTURE
	{
		static int texid = 0;

		if( !texid ) {
			texid = trap_R_GetTextureId( "textures/stone/mxsnow3.tga" );
		}
		trap_R_RenderToTexture( texid, 0, 0, 256, 256 );
	}
#endif

	trap_R_RestoreViewParms();
}

void CG_LimboPanel_RenderHead( panel_button_t* button ) {
	vec4_t clrBack = { 0.05f, 0.05f, 0.05f, 1.f };

	if( CG_LimboPanel_GetTeam() != TEAM_SPECTATOR ) {
		CG_FillRect( button->rect.x, button->rect.y, button->rect.w, button->rect.h, clrBack );
		CG_DrawPlayerHead( &button->rect, CG_LimboPanel_GetCharacter(), CG_LimboPanel_GetCharacter(), 150, 0, qtrue, HD_IDLE4, 0, 0, qfalse );
	} else {
		//CG_FillRect( button->rect.x, button->rect.y, button->rect.w, button->rect.h, colorBlack );
		//CG_DrawPlayerHead( &button->rect, BG_GetCharacter( TEAM_ALLIES, PC_SOLDIER ), BG_GetCharacter( TEAM_ALLIES, PC_SOLDIER ), 180, 0, qtrue, HD_IDLE4, 0, 0, qtrue );

		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.limboSpectator );
	}

	VectorSet( clrBack, .6f, .6f, .6f );
	trap_R_SetColor( clrBack );

	// top / bottom
	CG_DrawPic( button->rect.x, button->rect.y - 2, button->rect.w, 2, cgs.media.limboWeaponCardSurroundH );
	CG_DrawPicST( button->rect.x, button->rect.y + button->rect.h, button->rect.w, 2, 0.f, 1.f, 1.f, 0.f, cgs.media.limboWeaponCardSurroundH );

	CG_DrawPic( button->rect.x - 2,					button->rect.y,						2,	button->rect.h, cgs.media.limboWeaponCardSurroundV );
	CG_DrawPicST( button->rect.x +  button->rect.w, button->rect.y,						2,	button->rect.h, 1.f, 0.f, 0.f, 1.f, cgs.media.limboWeaponCardSurroundV );

	CG_DrawPicST( button->rect.x - 2,				button->rect.y - 2,					2,	2, 0.f, 0.f, 1.f, 1.f, cgs.media.limboWeaponCardSurroundC );
	CG_DrawPicST( button->rect.x + button->rect.w,	button->rect.y - 2,					2,	2, 1.f, 0.f, 0.f, 1.f, cgs.media.limboWeaponCardSurroundC );
	CG_DrawPicST( button->rect.x + button->rect.w,	button->rect.y + button->rect.h,	2,	2, 1.f, 1.f, 0.f, 0.f, cgs.media.limboWeaponCardSurroundC );
	CG_DrawPicST( button->rect.x - 2,				button->rect.y + button->rect.h,	2,	2, 0.f, 1.f, 1.f, 0.f, cgs.media.limboWeaponCardSurroundC );

	trap_R_SetColor( NULL );
}

qboolean CG_LimboPanel_Filter_KeyDown( panel_button_t* button, int key ) {
	if( key == K_MOUSE1 ) {
		SOUND_FILTER;

		cgs.ccFilter ^= (1 << button->data[0]);
		return qtrue;
	}

	return qfalse;
}

void CG_LimboPanel_Filter_Draw( panel_button_t* button ) {
	if( cgs.ccFilter & (1 << button->data[0]) ) {
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.ccFilterBackOff );
	} else {
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.ccFilterBackOn );
	}

//	CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.ccFilterPics[button->data[0]] );
	CG_DrawPic( button->rect.x+1, button->rect.y+1, button->rect.w-2, button->rect.h-2, cgs.media.ccFilterPics[button->data[0]] );
}


void CG_LimboPanel_RenderSkillIcon( panel_button_t* button ) {
	qhandle_t shader;
	if( cg_gameType.integer == GT_WOLF_LMS /*|| CG_LimboPanel_GetTeam() == TEAM_SPECTATOR*/ ) {
		return;
	}

	switch( button->data[0] ) {
		case 0:
			shader = cgs.media.limboSkillsBS;
			break;
		case 1:
			shader = cgs.media.limboSkillsLW;
			break;
		case 2:
			shader = cgs.media.limboClassButtons[CG_LimboPanel_GetClass()];
			break;
		default:
			return;
	}

	CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, shader );
}


qboolean CG_LimboPanel_WeaponLights_KeyDown( panel_button_t* button, int key ) {
	if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR ) {
		return qfalse;
	}

	if( key == K_MOUSE1 ) {
		SOUND_SELECT;

		cgs.ccSelectedWeaponNumber = button->data[0];
		CG_LimboPanel_RequestWeaponStats();
		return qtrue;
	}

	return qfalse;
}

void CG_LimboPanel_WeaponLights( panel_button_t* button ) {
    TCE_LimboWidget(button,TCE_LimboWeaponLight);
}

qboolean CG_LimboPanel_WeaponPanel_KeyDown( panel_button_t* button, int key ) {
	button->data[7] = 0;

	if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR ) {
		return qfalse;
	}

	if( key == K_MOUSE1 ) {
		SOUND_SELECT;

		BG_PanelButtons_SetFocusButton( button );
		return qtrue;
	}

	return qfalse;
}

qboolean CG_LimboPanel_WeaponPanel_KeyUp( panel_button_t* button, int key ) {
    int cnt, i, number;
    rectDef_t rect;
    if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR || key != K_MOUSE1 ||
        BG_PanelButtons_GetFocusButton() != button ) return qfalse;
    CG_LimboPanel_GetPlayerClass();
    rect = button->rect;
    rect.y -= rect.h;
    cnt = CG_LimboPanel_WeaponCount();
    for( i = 1; i < cnt; ++i, rect.y -= rect.h ) {
        if( !BG_CursorInRect(&rect) ) continue;
        number = i <= CG_LimboPanel_GetSelectedWeaponNum() ? i - 1 : i;
        if( CG_LimboPanel_RealWeaponIsDisabled(CG_LimboPanel_GetWeaponForNumber(
                number, cgs.ccSelectedWeaponNumber, qtrue)) ) continue;
        number = i;
        if (CG_LimboPanel_GetSelectedWeaponNum() &&
            i <= CG_LimboPanel_GetSelectedWeaponNum()) number = i - 1;
        CG_LimboPanel_SetSelectedWeaponNum(number);
    }
    BG_PanelButtons_SetFocusButton(NULL);
    return qtrue;
}

/* Full TC card renderer, cgame 30041c50. Gear metadata is registered by
 * the weapon loader, not as a side effect of drawing the selection menu. */
void CG_LimboPanel_WeaponPanel_DrawWeapon( rectDef_t* rect, weapon_t weap, qboolean highlight, const char* ofTxt, qboolean disabled ) {
    weaponType_t *wt = WM_FindWeaponTypeForWeapon(weap);
    const char *name, *description, *category;
    vec4_t shade = {0,0,0,.6f};
    const float *color = weaponPanelNameFont.colour;
    CG_Text_Width_Ext(ofTxt,.2f,0,&cgs.media.limboWeaponCountFont);
    if(weap==36 || weap==19 || weap==20 || weap==21) {
        if(!wt)return;
        name=wt->desc;description=wt->description;category=wt->category;
    } else {
        /* Out-of-range IDs are not valid card inputs. */
        if(weap<0 || weap>=TCE_MAX_WEAPONS)return;
        name=tce_cg_weapons[weap].deployMenuShortName;
        description=tce_cg_weapons[weap].deployMenuDescription;
        category=tce_cg_weapons[weap].deployMenuType;
    }
    CG_DrawPic(rect->x,rect->y,rect->w,rect->h,cgs.media.limboWeaponCard);
    if(name) {
        if(highlight && !disabled && BG_CursorInRect(rect)) {
            CG_DrawPic(rect->x,rect->y,rect->w,rect->h,cgs.media.limboWeaponCardHighlight);
            color=colorWhite;
        }
        CG_Text_Paint_Ext(rect->x+4,rect->y+12,weaponPanelNameFont.scalex,weaponPanelNameFont.scaley,
            color,name,0,0,weaponPanelNameFont.style,weaponPanelNameFont.font);
    }
    if(description)CG_Text_Paint_Ext(rect->x+4,rect->y+rect->h-11,.16f,.16f,colorBlack,
        description,0,0,weaponPanelNameFont.style,weaponPanelNameFont.font);
    if(category)CG_Text_Paint_Ext(rect->x+4,rect->y+rect->h-2,.16f,.16f,colorBlack,
        category,0,0,weaponPanelNameFont.style,weaponPanelNameFont.font);
    if(disabled)CG_FillRect(rect->x,rect->y,rect->w,rect->h,shade);
}

#define BRDRSIZE 4
static int TCE_LimboInside(const tce_limboRect_t *bounds) {
    rectDef_t rect={bounds->x,bounds->y,bounds->w,bounds->h};
    return BG_CursorInRect(&rect);
}
void CG_DrawBorder(float x,float y,float w,float h,qboolean fill,qboolean hover) {
    TCE_LimboDrawBorder(x,y,w,h,fill,hover,TCE_LimboInside,CG_FillRect);
}


void CG_LimboPanel_Border_Draw( panel_button_t* button ) {
    TCE_LimboWidget(button,TCE_LimboBorder);
}


void CG_LimboPanel_WeaponPanel( panel_button_t* button ) {
    weapon_t weap;
    int cnt;
    weap=CG_LimboPanel_GetSelectedWeapon();cnt=CG_LimboPanel_WeaponCount();
    if(CG_LimboPanel_GetTeam()!=TEAM_SPECTATOR) {
        clientInfo_t *ci=&cgs.clientinfo[cg.clientNum];
        int role,selected;
        vec4_t grey={.7f,.7f,.7f,1};
        CG_Text_Paint_Ext(button->rect.x+2,button->rect.y-252,.2f,.2f,grey,
                         "Armament Availability",0,0,0,&cgs.media.limboFont2);
        selected=BG_WolfClassToTCE(CG_LimboPanel_GetClass());
        for(role=0;role<3;++role) {
            int rating=role==BG_WolfClassToTCE(ci->tceScorePlayerClass)?ci->tceClassRating:ci->tcePreviousClassRating;
            CG_Text_Paint_Ext(button->rect.x+2,button->rect.y-240+role*11,.2f,.2f,
                selected==role?colorWhite:grey,va("[%s]: %i",role==0?"Assault":role==1?"Recon":"Sniper",rating+1),
                0,0,0,&cgs.media.limboFont2);
        }
    }

    if(cgs.ccSelectedWeapon>=CG_LimboPanel_WeaponCount_ForSlot(1))
        cgs.ccSelectedWeapon=CG_LimboPanel_WeaponCount_ForSlot(1)-1;
    if(cgs.ccSelectedWeapon<0)cgs.ccSelectedWeapon=0;
	if( cgs.ccSelectedWeapon2 >= CG_LimboPanel_WeaponCount_ForSlot( 0 ) ) {
		cgs.ccSelectedWeapon2 = CG_LimboPanel_WeaponCount_ForSlot( 0 ) - 1;
	}
    if(cgs.ccSelectedWeapon2<0)cgs.ccSelectedWeapon2=0;
    if(cgs.ccSelectedWeapon3>=CG_LimboPanel_WeaponCount_ForSlot(2))
        cgs.ccSelectedWeapon3=CG_LimboPanel_WeaponCount_ForSlot(2)-1;

	if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR ) {
		vec4_t clr = { 0.f, 0.f, 0.f, 0.4f };

        CG_DrawBorder(button->rect.x+2,button->rect.y+2,button->rect.w-4,button->rect.h-4,qfalse,qfalse);
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.limboWeaponCard );		

		trap_R_SetColor( clr );
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.limboWeaponBlendThingy );
		trap_R_SetColor( NULL );

		CG_Text_Paint_Ext( button->rect.x + 4, button->rect.y + 12, weaponPanelNameFont.scalex, weaponPanelNameFont.scaley, weaponPanelNameFont.colour, "SPECTATOR", 0, 0, weaponPanelNameFont.style, weaponPanelNameFont.font );

		return;
	}

	if( BG_PanelButtons_GetFocusButton() == button && cnt > 1 ) {
		int i, x;
		rectDef_t rect;
		CG_LimboPanel_GetPlayerClass();
		memcpy( &rect, &button->rect, sizeof( rect ) );

        CG_DrawBorder(rect.x+2,rect.y+2-(cnt-1)*rect.h,rect.w-4,cnt*rect.h-4,qfalse,qfalse);
		CG_LimboPanel_WeaponPanel_DrawWeapon( &rect, weap, qtrue, va( "%iof%i", CG_LimboPanel_GetSelectedWeaponNum()+1, cnt ), CG_LimboPanel_RealWeaponIsDisabled( weap ) );
		if( BG_CursorInRect( &rect ) ) {
			if( button->data[7] != 0 ) {
				SOUND_FOCUS;

				button->data[7] = 0;
			}
		}
		rect.y -= rect.h;

		// render in expanded mode ^
		for( i = 0, x = 1; i < cnt; i++ ) {
			weapon_t cycleWeap = CG_LimboPanel_GetWeaponForNumber( i, cgs.ccSelectedWeaponNumber, qtrue );
			if( cycleWeap != weap ) {
				CG_LimboPanel_WeaponPanel_DrawWeapon( &rect, cycleWeap, qtrue, va( "%iof%i", i+1, cnt ), CG_LimboPanel_RealWeaponIsDisabled( cycleWeap ) );

				if( BG_CursorInRect( &rect ) ) {
					if( button->data[7] != x ) {
						SOUND_FOCUS;

						button->data[7] = x;
					}
				}

				rect.y -= rect.h;
				x++;
			}
		}

	} else {
		vec4_t clr = { 0.f, 0.f, 0.f, 0.4f };
		vec4_t clr2 = { 1.f, 1.f, 1.f, 0.4f };

        CG_DrawBorder(button->rect.x+2,button->rect.y+2,button->rect.w-4,button->rect.h-4,qfalse,qfalse);
		// render in normal mode
		CG_LimboPanel_WeaponPanel_DrawWeapon( &button->rect, weap, cnt > 1 ? qtrue : qfalse, va( "%iof%i", CG_LimboPanel_GetSelectedWeaponNum()+1, cnt ), CG_LimboPanel_RealWeaponIsDisabled( weap ) );

		if( cnt <= 1 || !BG_CursorInRect( &button->rect ) ) {
			trap_R_SetColor( clr2 );
		}
		if(cnt>1)CG_DrawPic( button->rect.x + button->rect.w - 20, button->rect.y, 16, 12, cgs.media.limboWeaponCardArrow );
		

		trap_R_SetColor( clr );
		CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.limboWeaponBlendThingy );
		trap_R_SetColor( NULL );
	}
}

void CG_LimboPanel_RenderCounterNumber( float x, float y, float w, float h, float number, qhandle_t shaderBack, qhandle_t shaderRoll, int numbuttons ) {
	double digitOffset = (double)(numbuttons - 1) - number;
	float numberS = (float)(digitOffset * (1.0 / numbuttons));
	float numberE = (float)((digitOffset + 1.0) * (1.0 / numbuttons));

	CG_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, h, 0, 0,		1, 1,		shaderBack );
	trap_R_DrawStretchPic( x, y, w, h, 0, numberS,	1, numberE, shaderRoll );
}

int CG_LimboPanel_RenderCounter_ValueForButton( panel_button_t* button ) {
	int i, count = 0;

	switch( button->data[0] ) {
		case 0: // class counts
			if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR || CG_LimboPanel_GetRealTeam() != CG_LimboPanel_GetTeam()) {
				return 0; // dont give class counts unless we are on that team (or spec)
			}
			for( i = 0; i < MAX_CLIENTS; i++ ) {
				if( !cgs.clientinfo[i].infoValid ) {
					continue;
				}
				if( cgs.clientinfo[i].team != CG_LimboPanel_GetTeam() || cgs.clientinfo[i].cls != button->data[1]) {
					continue;
				}

				count++;
			}
			return count;
		case 1: // team counts
			for( i = 0; i < MAX_CLIENTS; i++ ) {
				if( !cgs.clientinfo[i].infoValid ) {
					continue;
				}

				if( cgs.clientinfo[i].team != teamOrder[button->data[1]] ) {
					continue;
				}

				count++;
			}
			return count;
		case 2: // xp
			return cg.xp;
		case 3: // respawn time
			return CG_CalculateReinfTime_Float( qtrue );
		case 4: // skills
			switch( button->data[1] ) {
				case 0:
					count = cgs.clientinfo[cg.clientNum].skill[SK_BATTLE_SENSE];
					break;
				case 1:
					count = cgs.clientinfo[cg.clientNum].skill[SK_LIGHT_WEAPONS];
					break;
				case 2:
					count = cgs.clientinfo[cg.clientNum].skill[BG_ClassSkillForClass(CG_LimboPanel_GetClass())];
					break;
			}

			return (1 << count) - 1;
		case 5: // clock
			if( !cgs.timelimit ) {
				return 0;
			}
			/* Original x87 keeps the minute product and elapsed subtraction extended. */
			count = (int)(((double)cgs.timelimit * 60000.0 - (cg.time - cgs.levelStartTime)) * (double)0.001f);
			switch( button->data[1] ) {
				case 0: // secs
					return count % 60;
				case 1: // mins
					return count / 60;
			}
			return 0;
		case 6: // stats
			switch( button->data[1] ) {
				case 0:
					return cgs.ccWeaponShots;
				case 1:
					return cgs.ccWeaponHits;
				case 2:
					return cgs.ccWeaponShots != 0 ? 100 * cgs.ccWeaponHits / cgs.ccWeaponShots : 0;
			}
	}

	return 0;
}

int CG_LimboPanel_RenderCounter_RollTimeForButton( panel_button_t* button ) {
	float diff;
	switch( button->data[0] ) {
		case 0: // class counts
		case 1: // team counts
			return 100.f;

		case 4: // skills
			return 1000.f;

		case 6: // stats
			diff = Q_fabs( button->data[3] - CG_LimboPanel_RenderCounter_ValueForButton( button ));
			if( diff < 5 ) {
				return 200.f / diff;
			} else {
				return 50.f;
			}

		case 5: // clock
		case 3: // respawn time
		case 2: // xp
			return 50.f;
	}

	return 1000.f;
}

int CG_LimboPanel_RenderCounter_MaxChangeForButton( panel_button_t* button ) {
	switch( button->data[0] ) {
		case 2: // xp
		case 6: // stats
			return 5;
	}

	return 1;
}
int CG_LimboPanel_RenderCounter_NumRollers( panel_button_t* button ) {
	switch( button->data[0] ) {
		case 0: // class counts
		case 1: // team counts
		case 5: // clock
		case 3: // respawn time
			return 2;

		case 4: // skills
			if( cg_gameType.integer == GT_WOLF_LMS /*|| CG_LimboPanel_GetTeam() == TEAM_SPECTATOR*/ ) {
				return 0;
			}
			return 4;

		case 6: // stats
			switch( button->data[1] ) {
				case 0:
				case 1:
					return 4;
				case 2:
					return 3;
			}

		case 2: // xp
			if( cg_gameType.integer == GT_WOLF_LMS ) {
				return 0;
			}
			return 6;
	}

	return 0;
}

qboolean CG_LimboPanel_RenderCounter_CountsDown( panel_button_t* button ) {
	switch( button->data[0] ) {
		case 4: // skill
		case 2: // xp
			return qfalse;

		default:
			break;
	}

	return qtrue;
}

qboolean CG_LimboPanel_RenderCounter_CountsUp( panel_button_t* button ) {
	switch( button->data[0] ) {
		case 4: // skill
		case 3: // respawn time
		case 5: // clock
			return qfalse;

		default:
			break;
	}

	return qtrue;
}

qboolean CG_LimboPanel_RenderCounter_StartSet( panel_button_t* button ) {
	switch( button->data[0] ) {
		case 3: // respawn time
		case 5: // clock
			return qtrue;

		default:
			break;
	}

	return qfalse;
}



void CG_LimboPanel_RenderMedal( panel_button_t* button ) {	
	CG_DrawPic( button->rect.x, button->rect.y, button->rect.w, button->rect.h, cgs.media.medal_back );
	if( cgs.clientinfo[cg.clientNum].medals[button->data[0]] ) {
		CG_DrawPic( button->rect.x - 2, button->rect.y, button->rect.w + 4, button->rect.h, cgs.media.medals[button->data[0]] );
	}
}

qboolean CG_LimboPanel_RenderCounter_IsReversed( panel_button_t* button ) {
	switch( button->data[0] ) {
		case 4: // skill
			return qtrue;

		default:
			break;
	}

	return qfalse;
}

void CG_LimboPanel_RenderCounter_GetShaders( panel_button_t* button, qhandle_t* shaderBack, qhandle_t* shaderRoll, int* numimages ) {
	switch( button->data[0] ) {
		case 4: // skills
			*shaderBack = cgs.media.limboStar_back;
			*shaderRoll = cgs.media.limboStar_roll;
			*numimages = 2;
			return;
		default:
			*shaderBack = cgs.media.limboNumber_back;
			*shaderRoll = cgs.media.limboNumber_roll;
			*numimages = 10;
			return;
	}
}

void CG_LimboPanelRenderText_NoLMS( panel_button_t* button ) {
	if( cg_gameType.integer == GT_WOLF_LMS ) {
		return;
	}

	BG_PanelButtonsRender_Text( button );
}

void CG_LimboPanelRenderText_SkillsText( panel_button_t* button ) {
	if( cg_gameType.integer == GT_WOLF_LMS /*|| CG_LimboPanel_GetTeam() == TEAM_SPECTATOR*/) {
		return;
	}

	BG_PanelButtonsRender_Text( button );
}

#define MAX_ROLLERS 8
#define COUNTER_ROLLTOTAL (cg.time - button->data[4])
// Gordon: this function is mental, i love it :)
void CG_LimboPanel_RenderCounter( panel_button_t* button ) {
	float x, w;
	float count[MAX_ROLLERS];
	int i, j;
	qhandle_t shaderBack;
	qhandle_t shaderRoll;
	int numimages;
	
	float counter_rolltime =	CG_LimboPanel_RenderCounter_RollTimeForButton	( button );
	int num =					CG_LimboPanel_RenderCounter_NumRollers			( button );
	int value =					CG_LimboPanel_RenderCounter_ValueForButton		( button );
	if( num > MAX_ROLLERS ) {
		num = MAX_ROLLERS;
	}

	CG_LimboPanel_RenderCounter_GetShaders( button, &shaderBack, &shaderRoll, &numimages );

	if( COUNTER_ROLLTOTAL < counter_rolltime ) {
		// we're rolling
		float frac = (COUNTER_ROLLTOTAL / counter_rolltime);

		for( i = 0, j = 1; i < num; i++, j *= numimages ) {
			int valueOld = (button->data[3] / j) % numimages;
			int valueNew = (button->data[5] / j) % numimages;

			if( valueNew == valueOld ) {
				count[i] = valueOld;
			} else if( (valueNew > valueOld) != (button->data[5] > button->data[3]) ) {
				// we're flipping around so....
				if(button->data[5] > button->data[3]) {
					count[i] = valueOld + frac;
				} else {
					count[i] = valueOld - frac;
				}
			} else {
				// normal flip
				count[i] = valueOld + ((valueNew - valueOld) * frac);
			}
		}
	} else {
		if( button->data[3] != button->data[5] ) {
			button->data[3] = button->data[5];
		} else if( value != button->data[3] ) {
			int maxchange = abs( value - button->data[3] );
			if( maxchange > CG_LimboPanel_RenderCounter_MaxChangeForButton( button ) ) {
				maxchange = CG_LimboPanel_RenderCounter_MaxChangeForButton( button );
			}

			if( value > button->data[3] ) {
				if( CG_LimboPanel_RenderCounter_CountsUp( button ) ) {
					button->data[5] = button->data[3] + maxchange;
				} else {
//					button->data[3] = 
					button->data[5] = value;
				}
			} else {
				if( CG_LimboPanel_RenderCounter_CountsDown( button ) ) {
					button->data[5] = button->data[3] - maxchange;
				} else {
//					button->data[3] = 
					button->data[5] = value;
				}
			}
			button->data[4] = cg.time;
		}

		for( i = 0, j = 1; i < num; i++, j *= numimages ) {
			count[i] = (int)(button->data[3] / j);
		}		
	}

	x = button->rect.x;
	w = button->rect.w / (float)num;

	if( CG_LimboPanel_RenderCounter_IsReversed( button ) ) {
		for( i = 0; i < num; i++ ) {
			CG_LimboPanel_RenderCounterNumber( x, button->rect.y, w, button->rect.h, count[i], shaderBack, shaderRoll, numimages );
			
			x += w + button->data[6];
		}
	} else {
		for( i = num-1; i >= 0; i-- ) {
			CG_LimboPanel_RenderCounterNumber( x, button->rect.y, w, button->rect.h, count[i], shaderBack, shaderRoll, numimages );
			
			x += w + button->data[6];
		}
	}

	if( button->data[0] == 0 || button->data[0] == 1) {
		CG_DrawPic( button->rect.x-2, button->rect.y-2, button->rect.w * 1.4f, button->rect.h+7, cgs.media.limboCounterBorder );
	}
}

void CG_LimboPanel_Setup( void ) {
	panel_button_t* button;
	panel_button_t** buttons = limboPanelButtons;
	clientInfo_t* ci = &cgs.clientinfo[cg.clientNum];
	bg_playerclass_t *classinfo;
	int i;
	char buffer[256];

	cgs.limboLoadoutModified = qfalse;

//	if( !cgs.playedLimboMusic ) {
//		trap_S_StartBackgroundTrack( "sound/music/menu_briefing.wav", "", 0 );
//		cgs.playedLimboMusic = qtrue;
//	}

	trap_Cvar_VariableStringBuffer( "name", buffer, 256 );
	trap_Cvar_Set( "limboname", buffer );

	if( cgs.ccLayers ) {
		cgs.ccSelectedLayer = CG_CurLayerForZ((int)cg.predictedPlayerEntity.lerpOrigin[2]);
	}

	for( ; *buttons; buttons++ ) {
		button = (*buttons);

		if( button->onDraw == CG_LimboPanel_RenderCounter ) {
			if( CG_LimboPanel_RenderCounter_StartSet( button ) ) {
				button->data[3] = button->data[5] = CG_LimboPanel_RenderCounter_ValueForButton( button );
				button->data[4] = 0;
			}
		}
	}

	if( !cgs.limboLoadoutSelected ) {
		bg_playerclass_t* classInfo = CG_LimboPanel_GetPlayerClass();

        int choice = 0;
        int role = BG_WolfClassToTCE(CG_LimboPanel_GetClass());
        int rating = role == BG_WolfClassToTCE(ci->tceScorePlayerClass)
            ? ci->tceClassRating : ci->tcePreviousClassRating;
        for( i = 0; i < MAX_WEAPS_PER_CLASS; ++i ) {
            int weapon = classInfo->classWeapons[i];
            if( !weapon ) { cgs.ccSelectedWeapon = 0; break; }
            if( BG_WeaponIsAvailable(weapon, gearDef.requiredSkill[weapon][role],
                    gearDef.team[weapon], rating + 20, CG_LimboPanel_GetTeam()) ) {
                if( !CG_LimboPanel_RealWeaponIsDisabled(weapon) && weapon == ci->latchedweapon ) {
                    cgs.ccSelectedWeapon = choice;
                    break;
                }
                ++choice;
            }
        }
        if( cgs.ccSelectedWeapon2 >= CG_LimboPanel_WeaponCount_ForSlot(0) )
            cgs.ccSelectedWeapon2 = CG_LimboPanel_WeaponCount_ForSlot(0) - 1;
        if( cgs.ccSelectedWeapon2 < 0 ) cgs.ccSelectedWeapon2 = 0;
        if( cgs.ccSelectedWeapon3 >= CG_LimboPanel_WeaponCount_ForSlot(2) )
            cgs.ccSelectedWeapon3 = CG_LimboPanel_WeaponCount_ForSlot(2) - 1;

		for( i = 0; i < 3; i++ ) {
			if( teamOrder[i] == ci->team ) {
				cgs.ccSelectedTeam = i;
			}
		}

		if( ci->team != TEAM_SPECTATOR ) {			
			cgs.ccSelectedClass = ci->cls;
		}
	}

	CG_LimboPanel_RequestWeaponStats();
	cgs.ccRequestedObjective = cgs.ccSelectedObjective = CG_LimboPanel_GetMaxObjectives();
	CG_LimboPanel_RequestObjective();

	cgs.ccSelectedObjective = CG_LimboPanel_GetMaxObjectives();
	cgs.ccSelectedWeaponNumber = 1;

	classinfo = CG_LimboPanel_GetPlayerClass();
	if( CG_LimboPanel_WeaponIsDisabled( cgs.ccSelectedWeapon ) ) {
		// set weapon to default if disabled
		// NOTE classWeapons[0] must NEVER be disabled
		cgs.ccSelectedWeapon = 0;//classinfo->classWeapons[0];
	}
}


void CG_LimboPanel_Init( void ) {
    TCE_LimboLayout();
	BG_PanelButtonsSetup( limboPanelButtons );
}

static qboolean TCE_LimboDrawContents( void ) {
	static panel_button_t* lastHighlight;
	panel_button_t* hilight;
//	panel_button_t** buttons = limboPanelButtons;

	hilight = BG_PanelButtonsGetHighlightButton( limboPanelButtons );
	if( hilight && hilight != lastHighlight ) {
		lastHighlight = hilight;
//		SOUND_FOCUS;
	}

	if( cg.limboEndCinematicTime > cg.time ) {
		//%	CG_DrawPic( LIMBO_3D_X, LIMBO_3D_Y, LIMBO_3D_W, LIMBO_3D_H, cgs.media.limboRadioBroadcast );
		/* Original TC panel cinematic covers its full 640-unit square. */
		CG_DrawPic( 4, -8, 632, 632, cgs.media.limboRadioBroadcast );
	}

	BG_PanelButtonsRender( limboPanelButtons );

	trap_R_SetColor( NULL );
	/* Original display-context cursor fields are integer-valued. */
	CG_DrawPic( (float)(int)cgDC.cursorx, (float)(int)cgDC.cursory, 32, 32, cgs.media.cursorIcon );

	if( cgs.ccRequestedObjective != -1 ) {
		if( cg.time - cgs.ccLastObjectiveRequestTime > 1000 ) {
			if( CG_LimboPanel_GetTeam() == TEAM_SPECTATOR ) {
				if( cgs.ccCurrentCamObjective != -1 || cgs.ccPortalEnt != -1 ) {
					CG_LimboPanel_RequestObjective();
				}
			} else {
				if( (cgs.ccRequestedObjective == cgs.ccSelectedObjective && (cgs.ccCurrentCamObjective != cgs.ccRequestedObjective || cgs.ccPortalEnt != -1)) ) {
					if( !(cgs.ccRequestedObjective == CG_LimboPanel_GetMaxObjectives() && cgs.ccCurrentCamObjective == -1 && cgs.ccPortalEnt == -1) ) {
						CG_LimboPanel_RequestObjective();
					}
				}
			}
		}
	}

	return qtrue;
}

static void TCE_LimboKeyContents( int key, qboolean down ) {
#ifdef _WIN32
	/* TC Windows 30044500 dispatches only; Linux retains the fallbacks below. */
	BG_PanelButtonsKeyEvent( key, down, limboPanelButtons );
#else
	int b1, b2;
	if( BG_PanelButtonsKeyEvent( key, down, limboPanelButtons ) ) {
		return;
	}

	if( down ) {
		cgDC.getKeysForBinding( "openlimbomenu", &b1, &b2 );
		if( (b1 != -1 && b1 == key) || (b2 != -1 && b2 == key)) {
			CG_EventHandling( CGAME_EVENT_NONE, qfalse );
			return;
		}
	}

	if( down && key ) {
		if( CG_CommandCentreSpawnPointClick() ) {
			return;
		}
	}
#endif
}

void CG_LimboPanel_GetWeaponCardIconData( weapon_t weap, qhandle_t* shader, float* w, float* h, float* s0, float* t0, float* s1, float* t1 ) {
	// setup the shader
	switch( weap ) {
		case WP_MORTAR:
		case WP_PANZERFAUST:
		case WP_FLAMETHROWER:
		case WP_FG42:
		case WP_MOBILE_MG42:
		case WP_MP40:
		case WP_STEN:
		case WP_THOMPSON:
			*shader = cgs.media.limboWeaponCard1;
			break;

		case WP_COLT:
		case WP_LUGER:
		case WP_AKIMBO_COLT:
		case WP_AKIMBO_LUGER:
		case WP_AKIMBO_SILENCEDCOLT:
		case WP_AKIMBO_SILENCEDLUGER:
		case WP_SILENCED_COLT:
		case WP_SILENCER:
		case WP_CARBINE:
		case WP_GARAND:
		case WP_KAR98:
		case WP_K43:
			*shader = cgs.media.limboWeaponCard2;
			break;
		
		default: // shouldn't happen
			*shader = 0;
	}

	// setup s co-ords
	switch( weap ) {
		case WP_SILENCED_COLT:
		case WP_SILENCER:
		case WP_LUGER:
		case WP_COLT:
			*s0 = 0;
			*s1 = 0.5f;
			break;
		default:
			*s0 = 0;
			*s1 = 1;
			break;
	}

	// setup t co-ords
	switch( weap ) {
		case WP_AKIMBO_SILENCEDLUGER:
		case WP_SILENCER:
		case WP_MORTAR:
			*t0 = 0/8.f;
			*t1 = 1/8.f;
			break;
		case WP_AKIMBO_SILENCEDCOLT:
		case WP_SILENCED_COLT:
		case WP_PANZERFAUST:
			*t0 = 1/8.f;
			*t1 = 2/8.f;
			break;
		case WP_LUGER:
		case WP_AKIMBO_LUGER:
		case WP_FLAMETHROWER:
			*t0 = 2/8.f;
			*t1 = 3/8.f;
			break;
		case WP_AKIMBO_COLT:
		case WP_COLT:
		case WP_FG42:
			*t0 = 3/8.f;
			*t1 = 4/8.f;
			break;
		case WP_CARBINE:
		case WP_MOBILE_MG42:
			*t0 = 4/8.f;
			*t1 = 5/8.f;
			break;
		case WP_KAR98:
		case WP_MP40:
			*t0 = 5/8.f;
			*t1 = 6/8.f;
			break;
		case WP_K43:
		case WP_STEN:
			*t0 = 6/8.f;
			*t1 = 7/8.f;
			break;
		case WP_GARAND:
		case WP_THOMPSON:
			*t0 = 7/8.f;
			*t1 = 8/8.f;
			break;
		default: // shouldn't happen
			*t0 = 0.0;
			*t1 = 1.0;
			break;
	}

	*h = 1.f;
	switch( weap ) {
		case WP_SILENCED_COLT:
		case WP_SILENCER:
		case WP_COLT:
		case WP_LUGER:
			*w = 0.5f;
			break;
		default:
			*w = 1.f;
			break;
	}
}

// Gordon: Utility funcs
team_t CG_LimboPanel_GetTeam( void ) {
	return teamOrder[cgs.ccSelectedTeam];
}

team_t CG_LimboPanel_GetRealTeam( void ) {
	return cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR ? CG_LimboPanel_GetTeam() : cgs.clientinfo[cg.clientNum].team;
}

int CG_LimboPanel_GetClass( void ) {
	return cgs.ccSelectedClass;
}

bg_character_t* CG_LimboPanel_GetCharacter( void ) {
	return BG_GetCharacter( CG_LimboPanel_GetTeam(), CG_LimboPanel_GetClass() );
}

bg_playerclass_t* CG_LimboPanel_GetPlayerClass( void ) {
	return BG_GetPlayerClassInfo( CG_LimboPanel_GetTeam(), CG_LimboPanel_GetClass() );
}

int CG_LimboPanel_WeaponCount( void ) {
	return CG_LimboPanel_WeaponCount_ForSlot( cgs.ccSelectedWeaponNumber );
}

static tce_limboLoadout_t TCE_LimboLoadout(void) {
    tce_limboLoadout_t s;
    clientInfo_t *ci=&cgs.clientinfo[cg.clientNum];
    memset(&s,0,sizeof(s));
    s.team=CG_LimboPanel_GetTeam();s.playerClass=CG_LimboPanel_GetClass();
    s.rating=ci->tceClassRating;s.previousRating=ci->tcePreviousClassRating;
    s.scoreClass=ci->tceScorePlayerClass;s.lightSkill=ci->skill[SK_LIGHT_WEAPONS];
    s.heavySkill=ci->skill[SK_HEAVY_WEAPONS];s.gametype=cgs.gametype;
    s.primary=TCE_LimboWeaponForNumber(&s,cgs.ccSelectedWeapon,1);
    return s;
}

int CG_LimboPanel_WeaponCount_ForSlot( int number ) {
    if(gearDef.parsed) { tce_limboLoadout_t s=TCE_LimboLoadout();return TCE_LimboWeaponCount(&s,number); }
	if( number == 1 ) {
		bg_playerclass_t* classInfo = CG_LimboPanel_GetPlayerClass();
		int cnt = 0, i;

		for(i = 0; i < MAX_WEAPS_PER_CLASS; i++) {
			if( !classInfo->classWeapons[i] ) {
				break;
			}

			cnt++;
		}
		return cnt;
	} else {
		if( cgs.clientinfo[cg.clientNum].skill[SK_HEAVY_WEAPONS] >= 4 && CG_LimboPanel_GetClass() == PC_SOLDIER ) {
			if( cgs.clientinfo[cg.clientNum].skill[SK_LIGHT_WEAPONS] >= 4 ) {
				return 3;
			} else {
				return 2;
			}
		} else {
			if( cgs.clientinfo[cg.clientNum].skill[SK_LIGHT_WEAPONS] >= 4 ) {
				return 2;
			} else {
				return 1;
			}
		}
	}
}

int CG_LimboPanel_GetWeaponNumberForPos( int pos ) {
	int i, cnt = 0;

	if( cgs.ccSelectedWeaponNumber == 0 ) {
		return pos;
	}

	if( pos < 0 || pos > CG_LimboPanel_WeaponCount() ) {
		return 0;
	}

	for( i = 0; i <= pos; i++ ) {
		while( CG_LimboPanel_WeaponIsDisabled( i + cnt ) ) {
			cnt++;
		}
	}

	return pos + cnt;
}

/* TC Windows30043790 / Linux0007bf86: enumerate the native class tables. */
weapon_t CG_LimboPanel_GetWeaponForNumber(int number, int slot, qboolean ignoreDisabled) {
    bg_playerclass_t *classInfo;
    clientInfo_t *ci = &cgs.clientinfo[cg.clientNum];
    int role = BG_WolfClassToTCE(CG_LimboPanel_GetClass());
    int rating = role == BG_WolfClassToTCE(ci->tceScorePlayerClass) ?
        ci->tceClassRating : ci->tcePreviousClassRating;
    int i, count = 0, weapon;
    weapon_t *weapons;
    if (CG_LimboPanel_GetTeam() == TEAM_SPECTATOR) return WP_NONE;
    classInfo = CG_LimboPanel_GetPlayerClass();
    if (!classInfo) return WP_NONE;
    if (slot == 1) {
        if (!ignoreDisabled && CG_LimboPanel_WeaponIsDisabled(number)) {
            if (!number) {
                CG_Error("ERROR: Class weapon 0 disabled\n");
                return WP_NONE;
            }
            return classInfo->classWeapons[0];
        }
        weapons = classInfo->classWeapons;
    } else if (slot == 2) {
        if (number) return (weapon_t)36;
        return (weapon_t)BG_GrenadeSelectionForPrimary(CG_LimboPanel_GetSelectedWeaponForSlot(1));
    } else {
        if (ci->skill[SK_HEAVY_WEAPONS] >= 4 && CG_LimboPanel_GetClass() == PC_SOLDIER &&
            number == (ci->skill[SK_LIGHT_WEAPONS] >= 4 ? 2 : 1))
            return (weapon_t)(CG_LimboPanel_GetTeam() == TEAM_AXIS ? 3 : 8);
        if (ci->skill[SK_LIGHT_WEAPONS] >= 4 && number > 0) {
            if (CG_LimboPanel_GetClass() == PC_COVERTOPS)
                return (weapon_t)(CG_LimboPanel_GetTeam() == TEAM_AXIS ? 54 : 53);
            return (weapon_t)(CG_LimboPanel_GetTeam() == TEAM_AXIS ? 38 : 37);
        }
        weapons = classInfo->classWeapons2;
    }
    for (i = 0; i < MAX_WEAPS_PER_CLASS; i++) {
        weapon = weapons[i];
        if (BG_WeaponIsAvailable(weapon, gearDef.requiredSkill[weapon][role],
                                gearDef.team[weapon], rating + 20, CG_LimboPanel_GetTeam())) {
            if (count == number) return (weapon_t)weapon;
            count++;
        }
    }
    if (slot != 1 && number == 0) {
        if (CG_LimboPanel_GetClass() == PC_COVERTOPS)
            return (weapon_t)(CG_LimboPanel_GetTeam() == TEAM_AXIS ? 14 : 52);
        return (weapon_t)(CG_LimboPanel_GetTeam() == TEAM_AXIS ? 2 : 7);
    }
    return WP_NONE;
}

weapon_t CG_LimboPanel_GetSelectedWeaponForSlot( int index ) {
	return CG_LimboPanel_GetWeaponForNumber( index == 2 ? cgs.ccSelectedWeapon3 : index == 1 ? cgs.ccSelectedWeapon : cgs.ccSelectedWeapon2, index, qfalse );
}

void CG_LimboPanel_SetSelectedWeaponNumForSlot( int index, int number ) {
	if( index == 0 ) {
		cgs.ccSelectedWeapon = number;
	} else {
		cgs.ccSelectedWeapon2 = number;
	}
}

weapon_t CG_LimboPanel_GetSelectedWeapon( void ) {
	return CG_LimboPanel_GetWeaponForNumber( CG_LimboPanel_GetSelectedWeaponNum(), cgs.ccSelectedWeaponNumber, qfalse );
}

int CG_LimboPanel_GetSelectedWeaponNum( void ) {
	if( !cgs.ccSelectedWeaponNumber ) {
		return cgs.ccSelectedWeapon2;
	}
	if( cgs.ccSelectedWeaponNumber == 2 ) {
		return cgs.ccSelectedWeapon3;
	}

	if( CG_LimboPanel_WeaponIsDisabled( cgs.ccSelectedWeapon ) ) {
		CG_LimboPanel_SetSelectedWeaponNumForSlot( 0, 0 );
	}

	return cgs.ccSelectedWeapon;
}

void CG_LimboPanel_RequestWeaponStats( void ) {
	extWeaponStats_t weapStat = CG_LimboPanel_GetSelectedWeaponStat();
	if(weapStat == WS_MAX) {
		// Bleh?
		return;
	}

	trap_SendClientCommand( va( "ws %i", weapStat ) );
}

void CG_LimboPanel_RequestObjective( void ) {
	int max = CG_LimboPanel_GetMaxObjectives();

	if( cgs.ccSelectedObjective != max && CG_LimboPanel_GetTeam() != TEAM_SPECTATOR ) {
		trap_SendClientCommand( va("obj %i", cgs.ccSelectedObjective) );
	} else {
		trap_SendClientCommand( va("obj %i", -1) );
	}
	cgs.ccRequestedObjective = cgs.ccSelectedObjective;
	cgs.ccLastObjectiveRequestTime = cg.time;
}


/* TC 30043bc0: the third slot has independent state; requests always run. */
void CG_LimboPanel_SetSelectedWeaponNum( int number ) {
    if( cgs.ccSelectedWeaponNumber == 1 ) {
        if( !CG_LimboPanel_WeaponIsDisabled( number ) ) cgs.ccSelectedWeapon = number;
    } else if( cgs.ccSelectedWeaponNumber == 2 ) {
        cgs.ccSelectedWeapon3 = number;
    } else {
        cgs.ccSelectedWeapon2 = number;
    }
    CG_LimboPanel_RequestWeaponStats();
}

extWeaponStats_t CG_LimboPanel_GetSelectedWeaponStat( void ) {
	return BG_WeapStatForWeapon( CG_LimboPanel_GetSelectedWeapon() );
}

int CG_LimboPanel_TeamCount( weapon_t weap ) {
	int i, cnt;

	if( weap == -1 ) { // we aint checking for a weapon, so always include ourselves
		cnt = 1;
	} else { // we ARE checking for a weapon, so ignore ourselves
		cnt = 0;
	}

	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( i == cg.clientNum ) {
			continue;
		}

		if( !cgs.clientinfo[i].infoValid ) {
			continue;
		}

		if( cgs.clientinfo[i].team != CG_LimboPanel_GetTeam() ) {
			continue;
		}

		if( weap != -1 ) {
			if( cgs.clientinfo[i].weapon != weap && cgs.clientinfo[i].latchedweapon != weap ) {
				continue;
			}
		}

		cnt++;
	}

	return cnt;
}

qboolean CG_IsHeavyWeapon( weapon_t weap ) {
	int i;
	if (gearDef.parsed) return TCE_BG_IsHeavyWeapon((int)weap);

	for( i = 0; i < NUM_HEAVY_WEAPONS; i++ ) {
		if( bg_heavyWeapons[i] == weap ) {
			return qtrue;
		}
	}

	return qfalse;
}

/* TC30043ca0 tests the unfiltered class slot, not the gear-filtered selection. */
qboolean CG_LimboPanel_WeaponIsDisabled(int index) {
    bg_playerclass_t *classinfo;
    int count, wcount;
    CG_LimboPanel_GetTeam();
    classinfo = CG_LimboPanel_GetPlayerClass();
    if (!CG_IsHeavyWeapon(classinfo->classWeapons[index])) return qfalse;
    count = CG_LimboPanel_TeamCount(-1);
    wcount = CG_LimboPanel_TeamCount(classinfo->classWeapons[index]);
    /* Original x87 passes the product directly as double to ceil. */
    return wcount >= ceil((double)count * cgs.weaponRestrictions);
}

/* TC30043d20 uses a rating+1 threshold; enumeration deliberately uses rating+20. */
qboolean CG_LimboPanel_RealWeaponIsDisabled(weapon_t weap) {
    clientInfo_t *ci = &cgs.clientinfo[cg.clientNum];
    int role = BG_WolfClassToTCE(CG_LimboPanel_GetClass());
    int rating = role == BG_WolfClassToTCE(ci->tceScorePlayerClass) ?
        ci->tceClassRating : ci->tcePreviousClassRating;
    int count, wcount;
    if (CG_LimboPanel_GetTeam() == TEAM_SPECTATOR) return qtrue;
    role = BG_WolfClassToTCE(CG_LimboPanel_GetClass());
    if (rating + 1 < gearDef.requiredSkill[weap][role]) return qtrue;
    if (!CG_IsHeavyWeapon(weap)) return qfalse;
    count = CG_LimboPanel_TeamCount(-1);
    wcount = CG_LimboPanel_TeamCount(weap);
    /* Preserve the quota boundary before ceil; do not round to float. */
    return wcount >= ceil((double)count * cgs.weaponRestrictions);
}

/* Scoped adapter: the rest of the still-SDK HUD keeps its 640-coordinate contract.
 * Input is inverted into the same TC canvas for rendering, hover and click dispatch. */
static void TCE_LimboBegin(float *x,float *y,qboolean *previous) {
    double scale,shift;
    *x=cgDC.cursorx;*y=cgDC.cursory;*previous=tce_uiCoordinates;
    scale=cgs.glconfig.vidWidth*480==cgs.glconfig.vidHeight*640 ? .7500000596046448 :
        ((double)cgs.glconfig.vidWidth*480/cgs.glconfig.vidHeight)*.0011718750465661287;
    shift=cgs.glconfig.vidWidth*480==cgs.glconfig.vidHeight*640 ? 80 : (1/scale-1)*240;
    cgDC.cursorx=(float)(*x/.7500000596046448);
    cgDC.cursory=(float)(*y/scale-shift);
    tce_uiCoordinates=qtrue;
}
static void TCE_LimboEnd(float x,float y,qboolean previous) {
    cgDC.cursorx=x;cgDC.cursory=y;tce_uiCoordinates=previous;
}
qboolean CG_LimboPanel_Draw(void) {
    float x,y;qboolean previous,result;
    TCE_LimboBegin(&x,&y,&previous);result=TCE_LimboDrawContents();TCE_LimboEnd(x,y,previous);return result;
}
void CG_LimboPanel_KeyHandling(int key,qboolean down) {
    float x,y;qboolean previous;
    TCE_LimboBegin(&x,&y,&previous);TCE_LimboKeyContents(key,down);TCE_LimboEnd(x,y,previous);
}
