#include "cg_local.h"
#include "../ui/ui_shared.h"

extern displayContextDef_t *DC;
extern qboolean tce_uiCoordinates;

qboolean	bg_loadscreeninited =	qfalse;
qboolean	bg_loadscreeninteractive;
fontInfo_t	bg_loadscreenfont1;
fontInfo_t	bg_loadscreenfont2;
qhandle_t	bg_axispin;
qhandle_t	bg_alliedpin;
qhandle_t	bg_neutralpin;
qhandle_t	bg_pin;

qhandle_t	bg_filter_pb;
qhandle_t	bg_filter_ff;
qhandle_t	bg_filter_hw;
qhandle_t	bg_filter_lv;
qhandle_t	bg_filter_al;
qhandle_t	bg_filter_bt;

qhandle_t	bg_mappic;

// panel_button_text_t FONTNAME = { SCALEX, SCALEY, COLOUR, STYLE, FONT };

panel_button_text_t missiondescriptionTxt = {
	0.2f, 0.2f,
	{ 0.0f, 0.0f, 0.0f, 1.f },
	0, 0,
	&bg_loadscreenfont2,
};

panel_button_text_t missiondescriptionHeaderTxt = {
	0.2f, 0.2f,
	{ 0.0f, 0.0f, 0.0f, 0.8f },
	 0,ITEM_ALIGN_CENTER,
	&bg_loadscreenfont2,
};

panel_button_text_t campaignpheaderTxt = {
	0.2f, 0.2f,
	{ 1.0f, 1.0f, 1.0f, 0.6f },
	0, 0,
	&bg_loadscreenfont2,
};

panel_button_text_t campaignpTxt = {
	0.2f, 0.2f,
	{ 1.0f, 1.0f, 1.0f, 0.6f },
	0, 0,
	&bg_loadscreenfont2,
};

panel_button_text_t loadScreenMeterBackTxt = {
	0.22f, 0.22f,
	{ 0.1f, 0.1f, 0.1f, 0.8f },
	 0, ITEM_ALIGN_CENTER,
	&bg_loadscreenfont2,
};

panel_button_t loadScreenMap = {
	"gfx/loading/camp_map",
	NULL,
	{ 0, 0, 440, 480 },	// shouldn't this be square?? // Gordon: no, the map is actually WIDER that tall, which makes it even worse...
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/	
	BG_PanelButtonsRender_Img,
	NULL,
};

panel_button_t loadScreenBack = {
	"gfx/loading/camp_side",
	NULL,
	{ -5, -110, 862, 700 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/	
	BG_PanelButtonsRender_Img,
	NULL,
};

panel_button_t loadScreenPins = {
	NULL,
	NULL,
	{ 0, 0, 640, 480 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/	
	CG_LoadPanel_RenderCampaignPins,
	NULL,
};

panel_button_t missiondescriptionPanelHeaderText = {
	NULL,
	"***TOP SECRET***",
	{ 440, 72, 200, 32 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&missiondescriptionHeaderTxt,	/* font		*/
	NULL,					/* keyDown	*/
	NULL,					/* keyUp	*/
	BG_PanelButtonsRender_Text,
	NULL,
};

panel_button_t missiondescriptionPanelText = {
	NULL,
	NULL,
	{ 460, 84, 160, 232 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&missiondescriptionTxt,	/* font		*/
	NULL,					/* keyDown	*/
	NULL,					/* keyUp	*/
	CG_LoadPanel_RenderMissionDescriptionText,
	NULL,
};

panel_button_t campaignheaderPanelText = {
	NULL,
	NULL,
	{ 20, 412, 152, 32 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&campaignpheaderTxt,	/* font		*/
	NULL,					/* keyDown	*/
	NULL,					/* keyUp	*/
	CG_LoadPanel_RenderCampaignTypeText,
	NULL,
};

panel_button_t campaignPanelText = {
	NULL,
	NULL,
	{ 20, 402, 152, 32 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&campaignpTxt,			/* font		*/
	NULL,					/* keyDown	*/
	NULL,					/* keyUp	*/
	CG_LoadPanel_RenderCampaignNameText,
	NULL,
};

panel_button_t loadScreenMeterBack = {
	"gfx/loading/progressbar_back",
	NULL,
	{ 326, 460, 200, 6 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/	
	BG_PanelButtonsRender_Img,
	NULL,
};

panel_button_t loadScreenMeterBack2 = {
	"gfx/loading/progressbar",
	NULL,
	{ 327, 461, 198, 4 },
	{ 1, 255, 0, 0, 255, 0, 0, 0 },
	NULL,	/* font		*/
	NULL,	/* keyDown	*/
	NULL,	/* keyUp	*/	
	CG_LoadPanel_RenderLoadingBar,
	NULL,
};

panel_button_t loadScreenMeterBackText = {
	NULL,
	"LOADING",
	{ 440+28, 480-28+12+1, 200-56-2, 16 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	&loadScreenMeterBackTxt,	/* font		*/
	NULL,						/* keyDown	*/
	NULL,						/* keyUp	*/	
	BG_PanelButtonsRender_Text,
	NULL,
};

/* Original TC:E draws the background/meter first, map labels over the levelshot. */
panel_button_t* loadpanelButtons[] = {
    &loadScreenBack, &loadScreenMeterBack, &loadScreenMeterBack2, NULL,
};
panel_button_t* loadpanelMapname[] = {
    &campaignheaderPanelText, &campaignPanelText, NULL,
};

/*
================
CG_DrawConnectScreen
================
*/

void CG_DrawConnectScreen( qboolean interactive, qboolean forcerefresh ) {
	static qboolean inside = qfalse;
	char buffer[1024];
    qboolean previousCoordinates;
    vec4_t headerColor = { 1.f, 1.f, 1.f, .6f };

	bg_loadscreeninteractive = interactive;

	if( !DC ) {
		return;
	}

	if( inside ) {
		return;
	}

	inside = qtrue;
    previousCoordinates = tce_uiCoordinates;
    tce_uiCoordinates = qtrue;

	if( !bg_loadscreeninited ) {
		trap_Cvar_Set( "ui_connecting", "0" );

		DC->registerFont( "courbd", 30, &bg_loadscreenfont1 );
		DC->registerFont( "courbd", 30, &bg_loadscreenfont2 );

		bg_axispin =	DC->registerShaderNoMip( "gfx/loading/pin_axis" );
		bg_alliedpin =	DC->registerShaderNoMip( "gfx/loading/pin_allied" );
		bg_neutralpin =	DC->registerShaderNoMip( "gfx/loading/pin_neutral" );
		bg_pin =		DC->registerShaderNoMip( "gfx/loading/pin_shot" );
				

		bg_filter_pb =	DC->registerShaderNoMip( "ui/assets/filter_pb" );
		bg_filter_ff =	DC->registerShaderNoMip( "ui/assets/filter_ff" );
		bg_filter_hw =	DC->registerShaderNoMip( "ui/assets/filter_weap" );
		bg_filter_lv =	DC->registerShaderNoMip( "ui/assets/filter_lives" );
		bg_filter_al =	DC->registerShaderNoMip( "ui/assets/filter_antilag" );
		bg_filter_bt =	DC->registerShaderNoMip( "ui/assets/filter_balance" );


		bg_mappic =		0;

		BG_PanelButtonsSetup( loadpanelButtons );
        BG_PanelButtonsRender( loadpanelMapname );

		bg_loadscreeninited = qtrue;
	}

	BG_PanelButtonsRender( loadpanelButtons );

	if( interactive ) {
		DC->drawHandlePic( DC->cursorx, DC->cursory, 32, 32, DC->Assets.cursor );
	}

	DC->getConfigString( CS_SERVERINFO, buffer, sizeof( buffer ) );

	if( *cgs.rawmapname ) {
		if( !bg_mappic ) {
			bg_mappic = DC->registerShaderNoMip( va( "levelshots/%s", cgs.rawmapname ) );

			if( !bg_mappic ) {
				bg_mappic = DC->registerShaderNoMip( "levelshots/unknownmap" );	
			}
		}

        trap_R_SetColor( NULL );
        CG_DrawPic( 0, 58, 852, 364, bg_mappic );
    }

    BG_PanelButtonsRender( loadpanelMapname );
    CG_Text_Paint_Centred_Ext( 93, 87, .5f, .5f, headerColor, "TCE TEST", 0, 0, 0, &bg_loadscreenfont1 );
    CG_Text_Paint_Centred_Ext( 627, 87, .3f, .3f, headerColor,
        cgs.tceVersion, 0, 0, 0, &bg_loadscreenfont1 );

	if( forcerefresh ) {
		DC->updateScreen();
	}

	tce_uiCoordinates = previousCoordinates;
	inside = qfalse;
}

void CG_LoadPanel_RenderCampaignTypeText( panel_button_t* button ) {
/*	char buffer[1024];
	const char* str;
	DC->getConfigString( CS_SERVERINFO, buffer, sizeof( buffer ) );
	if( !*buffer ) {
		return;
	}

	str = Info_ValueForKey( buffer, "g_gametype" );
*/
	CG_Text_Paint_Ext( button->rect.x, button->rect.y, button->font->scalex, button->font->scaley, button->font->colour, va( "%s", CG_LoadPanel_GameTypeName( cgs.gametype ) ), 0, 0, button->font->style, button->font->font );
}


void CG_LoadPanel_RenderCampaignNameText( panel_button_t* button ) {
	const char* cs;
	float w;
	//char buffer[1024];
	//int gametype;

	//DC->getConfigString( CS_SERVERINFO, buffer, sizeof( buffer ) );
	//cs = Info_ValueForKey( buffer, "g_gametype" );
	//gametype = atoi(cs);

	if( cgs.gametype == GT_WOLF_CAMPAIGN ) {

		cs = DC->nameForCampaign();
		if( !cs ) {
			return;
		}

		cs = va( "%s %iof%i", cs, cgs.currentCampaignMap+1, cgs.campaignData.mapCount );

		w = CG_Text_Width_Ext( cs, button->font->scalex, 0, button->font->font );
		CG_Text_Paint_Ext( button->rect.x + (button->rect.w - w)*0.5f, button->rect.y, button->font->scalex, button->font->scaley, button->font->colour, cs, 0, 0, 0, button->font->font );

	} else {

		if( !cgs.arenaInfoLoaded ) {
			return;
		}

        /* Preserve the original measurement callback as well as draw order. */
        CG_Text_Width_Ext( cgs.arenaData.longname, button->font->scalex, 0, button->font->font );
        CG_Text_Paint_Ext( button->rect.x, button->rect.y, button->font->scalex, button->font->scaley, button->font->colour, cgs.arenaData.longname, 0, 0, 0, button->font->font );
        w = CG_Text_Width_Ext( cgs.arenaData.authors, button->font->scalex, 0, button->font->font );
        CG_Text_Paint_Ext( button->rect.x + 750.f - w, button->rect.y, button->font->scalex, button->font->scaley, button->font->colour, cgs.arenaData.authors, 0, 0, 0, button->font->font );
	}
}

void CG_LoadPanel_RenderMissionDescriptionText( panel_button_t* button ) {
	const char* cs;
	char *s, *p;
	char buffer[1024];
	float y;
	//int gametype;

	//DC->getConfigString( CS_SERVERINFO, buffer, sizeof( buffer ) );
	//cs = Info_ValueForKey( buffer, "g_gametype" );
	//gametype = atoi(cs);

//	DC->fillRect( button->rect.x, button->rect.y, button->rect.w, button->rect.h, colorRed );

	if( cgs.gametype == GT_WOLF_CAMPAIGN ) {

		cs = DC->descriptionForCampaign();
		if( !cs ) {
			return;
		}

	} else if( cgs.gametype == GT_WOLF_LMS ) {

		//cs = CG_ConfigString( CS_MULTI_MAPDESC3 );

		if( !cgs.arenaInfoLoaded ) {
			return;
		}

		cs = cgs.arenaData.lmsdescription;

	} else {

		if( !cgs.arenaInfoLoaded ) {
			return;
		}

		cs = cgs.arenaData.description;
	}

	Q_strncpyz( buffer, cs, sizeof(buffer) );
	while ((s = strchr(buffer, '*'))) {
		*s = '\n';
	}

	BG_FitTextToWidth_Ext( buffer, button->font->scalex, button->rect.w - 16, sizeof(buffer), button->font->font );

	y = button->rect.y + 12;

	s = p = buffer;
	while(*p) {
		if(*p == '\n') {
			*p++ = '\0';
			DC->drawTextExt( button->rect.x + 4, y, button->font->scalex, button->font->scaley, button->font->colour, s, 0, 0, 0, button->font->font );
			y += 8;
			s = p;
		} else {
			p++; 
		}
	}
}

void CG_LoadPanel_KeyHandling( int key, qboolean down ) {
	if( BG_PanelButtonsKeyEvent( key, down, loadpanelButtons ) ) {
		return;
	}
}

qboolean CG_LoadPanel_ContinueButtonKeyDown( panel_button_t* button, int key ) {
	if( key == K_MOUSE1 ) {
		CG_EventHandling( CGAME_EVENT_GAMEVIEW, qfalse );
		return qtrue;
	}

	return qfalse;
}


void CG_LoadPanel_DrawPin( const char* text, float px, float py, float sx, float sy, qhandle_t shader, float pinsize, float backheight ) {
	float x, y, w, h;
	vec4_t colourFadedBlack = { 0.f, 0.f, 0.f, 0.4f };

	w = DC->textWidthExt( text, sx, 0, &bg_loadscreenfont2 );
	if( px + 30 + w > 440 ) {
		DC->fillRect( px - w - 28 + 2, py - (backheight/2.f) + 2, 28 + w, backheight, colourFadedBlack );
		DC->fillRect( px - w - 28, py - (backheight/2.f), 28 + w, backheight, colorBlack );
	} else {
		DC->fillRect( px + 2, py - (backheight/2.f) + 2, 28 + w, backheight, colourFadedBlack );
		DC->fillRect( px, py - (backheight/2.f), 28 + w, backheight, colorBlack );
	}

	x = px - pinsize;
	y = py - pinsize;
	w = pinsize * 2.f;
	h = pinsize * 2.f;

	DC->drawHandlePic( x, y, w, h, shader );

	if( px + 30 + w > 440 ) {
		DC->drawTextExt( px - 12 - w - 28, py + 4, sx, sy, colorWhite, text, 0, 0, 0, &bg_loadscreenfont2 );
	} else {
		DC->drawTextExt( px + 16, py + 4, sx, sy, colorWhite, text, 0, 0, 0, &bg_loadscreenfont2 );
	}
}

void CG_LoadPanel_RenderCampaignPins( panel_button_t* button ) {
	int i;
	qhandle_t shader;
	/*char buffer[1024];
	char *s;
	int gametype;

	DC->getConfigString( CS_SERVERINFO, buffer, sizeof( buffer ) );
	s = Info_ValueForKey( buffer, "g_gametype" );
	gametype = atoi(s);*/

	if( cgs.gametype == GT_WOLF_STOPWATCH || cgs.gametype == GT_WOLF_LMS || cgs.gametype == GT_WOLF ) {
		float px, py;

		if( !cgs.arenaInfoLoaded ) {
			return;
		}

		px = ( cgs.arenaData.mappos[0] / 1024.f ) * 440.f;
		py = ( cgs.arenaData.mappos[1] / 1024.f ) * 480.f;

		CG_LoadPanel_DrawPin( cgs.arenaData.longname, px, py, 0.22f, 0.25f, bg_neutralpin, 16.f, 16.f );
	} else {
		if( !cgs.campaignInfoLoaded ) {
			return;
		}

		for( i = 0; i < cgs.campaignData.mapCount; i++ ) {
			float px, py;

			cg.teamWonRounds[1] = atoi( CG_ConfigString( CS_ROUNDSCORES1 ) );
			cg.teamWonRounds[0] = atoi( CG_ConfigString( CS_ROUNDSCORES2 ) );

			if( cg.teamWonRounds[1] & (1 << i) ) {
				shader = bg_axispin;
			} else if( cg.teamWonRounds[0] & (1 << i) ) {
				shader = bg_alliedpin;
			} else {
				shader = bg_neutralpin;
			}

			px = ( cgs.campaignData.arenas[i].mappos[0] / 1024.f ) * 440.f;
			py = ( cgs.campaignData.arenas[i].mappos[1] / 1024.f ) * 480.f;

			CG_LoadPanel_DrawPin( cgs.campaignData.arenas[i].longname, px, py, 0.22f, 0.25f, shader, 16.f, 16.f );
		}
	}
}
