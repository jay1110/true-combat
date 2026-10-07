#include "tce_popup_text.h"
#include "tce_popup_draw.h"
#include "cg_local.h"

#define NUM_PM_STACK_ITEMS	32
#define MAX_VISIBLE_ITEMS	5

#define NUM_PM_STACK_ITEMS_BIG 8 // Gordon: we shouldn't need many of these

typedef tce_popup_draw_item_t pmListItem_t;
typedef tce_popup_draw_item_t pmListItemBig_t;





pmListItem_t		cg_pmStack[NUM_PM_STACK_ITEMS];
pmListItem_t*		cg_pmOldList;
pmListItem_t*		cg_pmWaitingList;
pmListItemBig_t*	cg_pmWaitingListBig;

pmListItemBig_t		cg_pmStackBig[NUM_PM_STACK_ITEMS_BIG];


const char* cg_skillRewards[SK_NUM_SKILLS][NUM_SKILL_LEVELS-1] = {
	{ "Binoculars", "Improved Physical Fitness", "Improved Health", "Trap Awareness" },											// battle sense
	{ "Improved use of Explosive Ammunition", "Improved Dexterity", "Improved Construction and Destruction", "a Flak Jacket" },	// explosives & construction
	{ "Medic Ammo", "Improved Resources", "Full Revive", "Adrenalin Self" },													// first aid
	{ "Improved Resources", "Improved Signals", "Improved Air and Ground Support", "Enemy Recognition" },						// signals
	{ "Improved use of Light Weapon Ammunition", "Faster Reload", "Improved Light Weapon Handling", "Dual-Wield Pistols" },		// light weapons
	{ "Improved Projectile Resources", "Heavy Weapon Proficiency", "Improved Dexterity", "Improved Weapon Handling" },			// heavy weapons
	{ "Improved use of Scoped Weapon Ammunition", "Improved use of Sabotage and Misdirection", "Breath Control", "Assassin" }	// scoped weapons & military intelligence
};

void CG_PMItemBigSound( pmListItemBig_t* item );




void CG_InitPMGraphics( void ) {
	cgs.media.pmImages[PM_DYNAMITE] =		trap_R_RegisterShaderNoMip( "gfx/limbo/cm_dynamite" );
	cgs.media.pmImages[PM_CONSTRUCTION] =	trap_R_RegisterShaderNoMip( "sprites/voiceChat" );
	cgs.media.pmImages[PM_MINES] =			trap_R_RegisterShaderNoMip( "sprites/voiceChat" );
	cgs.media.pmImages[PM_DEATH] =			trap_R_RegisterShaderNoMip( "gfx/hud/pm_death" );
	cgs.media.pmImages[PM_MESSAGE] =		trap_R_RegisterShaderNoMip( "sprites/voiceChat" );
	cgs.media.pmImages[PM_OBJECTIVE] =		trap_R_RegisterShaderNoMip( "sprites/objective" );
	cgs.media.pmImages[PM_DESTRUCTION] =	trap_R_RegisterShaderNoMip( "sprites/voiceChat" );
	cgs.media.pmImages[PM_TEAM] =			trap_R_RegisterShaderNoMip( "sprites/voiceChat" );	

	cgs.media.pmImageAlliesConstruct =		trap_R_RegisterShaderNoMip( "gfx/hud/pm_constallied" );
	cgs.media.pmImageAxisConstruct =		trap_R_RegisterShaderNoMip( "gfx/hud/pm_constaxis" );
	cgs.media.pmImageAlliesMine =			trap_R_RegisterShaderNoMip( "gfx/hud/pm_mineallied" );
	cgs.media.pmImageAxisMine =				trap_R_RegisterShaderNoMip( "gfx/hud/pm_mineaxis" );
	cgs.media.hintKey =						trap_R_RegisterShaderNoMip( "gfx/hud/keyboardkey_old" );
}

void CG_InitPM( void ) {
	memset( &cg_pmStack,	0, sizeof( cg_pmStack ) );
	memset( &cg_pmStackBig, 0, sizeof( cg_pmStackBig ) );

	cg_pmOldList =			NULL;
	cg_pmWaitingList =		NULL;
	cg_pmWaitingListBig =	NULL;
}

#define PM_FADETIME 2500
#define PM_WAITTIME 2000

#define PM_FADETIME_BIG 1000
#define PM_WAITTIME_BIG 3500

int CG_TimeForPopup( popupMessageType_t type ) {
	switch( type ) {
		default:
			return 1000;
	}
}

int CG_TimeForBigPopup( popupMessageBigType_t type ) {
	switch( type ) {
		default:
			return 2500;
	}
}

void CG_AddToListFront( pmListItem_t** list, pmListItem_t* item ) {
	item->next = *list;
	*list = item;
}

void CG_UpdatePMLists( void ) {
	pmListItem_t* listItem;
	pmListItem_t* lastItem;
	pmListItemBig_t* listItem2;

	if ((listItem = cg_pmWaitingList)) {
		int t = (CG_TimeForPopup( listItem->type ) + listItem->time);
		if( cg.time > t ) {
			if( listItem->next ) {
				// there's another item waiting to come on, so move to old list
				cg_pmWaitingList = listItem->next;
				cg_pmWaitingList->time = cg.time; // set time we popped up at

				CG_AddToListFront( &cg_pmOldList, listItem );
			} else {
				if( cg.time > t + PM_WAITTIME + PM_FADETIME ) {
					// we're gone completely
					cg_pmWaitingList = NULL;
					listItem->inuse = qfalse;
					listItem->next = NULL;
				} else {
					// just sit where we are, no pressure to do anything...
				}
			}
		}
	}

	listItem = cg_pmOldList;
	lastItem = NULL;
	while( listItem ) {
		int t = (CG_TimeForPopup( listItem->type ) + listItem->time + PM_WAITTIME + PM_FADETIME);
		if( cg.time > t ) {
			// nuke this, and everything below it (though there shouldn't BE anything below us anyway)
			pmListItem_t* next;

			if( !lastItem ) {
				// we're the top of the old list, so set to NULL
				cg_pmOldList = NULL;
			} else {
				lastItem->next = NULL;
			}

			do {
				next = listItem->next;

				listItem->next = NULL;
				listItem->inuse = qfalse;

			} while ((listItem = next));
			

			break;
		}

		lastItem = listItem;
		listItem = listItem->next;
	}


	if ((listItem2 = cg_pmWaitingListBig)) {
		int t = CG_TimeForBigPopup( listItem2->type ) + listItem2->time;
		if( cg.time > t ) {
			if( listItem2->next ) {
				// there's another item waiting to come on, so kill us and shove the next one to the front
				cg_pmWaitingListBig = listItem2->next;
				cg_pmWaitingListBig->time = cg.time; // set time we popped up at

				CG_PMItemBigSound( cg_pmWaitingListBig );

				listItem2->inuse = qfalse;
				listItem2->next = NULL;
			} else {
				if( cg.time > t + PM_WAITTIME + PM_FADETIME ) {
					// we're gone completely
					cg_pmWaitingListBig = NULL;
					listItem2->inuse = qfalse;
					listItem2->next = NULL;
				} else {
					// just sit where we are, no pressure to do anything...
				}
			}
		}
	}
}

pmListItemBig_t* CG_FindFreePMItem2( void ) {
	int i = 0;
	for( ; i < NUM_PM_STACK_ITEMS_BIG; i++ ) {
		if( !cg_pmStackBig[i].inuse ) {
			return &cg_pmStackBig[i];
		}
	}

	return NULL;
}

pmListItem_t* CG_FindFreePMItem( void ) {
	pmListItem_t* listItem;
	pmListItem_t* lastItem;

	int i = 0;
	for( ; i < NUM_PM_STACK_ITEMS; i++ ) {
		if( !cg_pmStack[i].inuse ) {
			return &cg_pmStack[i];
		}
	}

	// no totally free items, so just grab the last item in the oldlist
	if ((lastItem = listItem = cg_pmOldList)) {
		while( listItem->next ) {
			lastItem = listItem;
			listItem = listItem->next;			
		}

		if( lastItem == cg_pmOldList ) {
			cg_pmOldList = NULL;
		} else {
			lastItem->next = NULL;
		}

		listItem->inuse = qfalse;

		return listItem;
	} else {
		// there is no old list... PANIC!
		return NULL;
	}
}

void CG_AddPMItem( popupMessageType_t type, const char* message, qhandle_t shader ) {
	pmListItem_t* listItem;
	char* end;

	if( !message || !*message ) {
		return;
	}
	if (type < 0 || type >= PM_NUM_TYPES) {
		CG_Printf("Invalid popup type: %d\n", type);
		return;
	}

	listItem = CG_FindFreePMItem();

	if( !listItem ) {
		return;
	}

	if( shader ) {
		listItem->shader = shader;
	} else {
		listItem->shader = cgs.media.pmImages[type];
	}

	listItem->inuse = qtrue;
	listItem->type = type;
	Q_strncpyz( listItem->message, message, sizeof( cg_pmStack[0].message ) );

	// rain - moved this: print and THEN chop off the newline, as the
	// console deals with newlines perfectly.  We do chop off the newline
	// at the end, if any, though.
	if (listItem->message[strlen(listItem->message) - 1] == '\n')
		listItem->message[strlen(listItem->message) - 1] = 0;

	trap_Print( va( "%s\n", listItem->message ) );

	// rain - added parens
	while ((end = strchr(listItem->message, '\n'))) {
		*end = '\0';
	}

	// rain - don't eat popups for empty lines
	if (*listItem->message == '\0')
		return;

	if( !cg_pmWaitingList ) {
		cg_pmWaitingList = listItem;
		listItem->time = cg.time;
	} else {
		pmListItem_t* loop = cg_pmWaitingList;
		while( loop->next ) {
			loop = loop->next;
		}

		loop->next = listItem;
	}
}

void CG_PMItemBigSound( pmListItemBig_t* item ) {
	if( !cg.snap ) {
		return;
	}

	switch( item->type ) {
		case PM_RANK:
			trap_S_StartSound( NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.media.sndRankUp );
			break;
		case PM_SKILL:
			trap_S_StartSound( NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.media.sndSkillUp );
			break;
		default:
			break;
	}
}

void CG_AddPMItemBig( popupMessageBigType_t type, const char* message, qhandle_t shader ) {
	pmListItemBig_t* listItem = CG_FindFreePMItem2();
	if( !listItem ) {
		return;
	}

	if( shader ) {
		listItem->shader = shader;
	} else {
		listItem->shader = cgs.media.pmImages[type];
	}

	listItem->inuse = qtrue;
	listItem->type = type;
	listItem->next = NULL;
	Q_strncpyz( listItem->message, message, sizeof( cg_pmStackBig[0].message ) );

	if( !cg_pmWaitingListBig ) {
		cg_pmWaitingListBig = listItem;
		listItem->time = cg.time;

		CG_PMItemBigSound( listItem );
	} else {
		pmListItemBig_t* loop = cg_pmWaitingListBig;
		while( loop->next ) {
			loop = loop->next;
		}

		loop->next = listItem;
	}
}

#define PM_ICON_SIZE_NORMAL 20
#define PM_ICON_SIZE_SMALL 12
static void CG_PopupPaint(float x,float y,float sx,float sy,float *color,const char *text,float adjust,int limit,int style,void *font) {
 CG_Text_Paint_Ext(x,y,sx,sy,color,text,adjust,limit,style,(fontInfo_t *)font);
}
static int CG_PopupDuration(int type) { return CG_TimeForPopup((popupMessageType_t)type); }
void CG_DrawPMItems(void) {
 extern qboolean tce_uiCoordinates;
 char aspectMode[16];
 qboolean previous=tce_uiCoordinates;
 tce_popup_draw_api_t api={trap_R_SetColor,CG_DrawPic,CG_PopupPaint,CG_PopupDuration,&cgs.media.limboFont1};
 tce_uiCoordinates=qtrue;
 trap_Cvar_VariableStringBuffer("cg_aspectMode",aspectMode,sizeof(aspectMode));
 TCE_DrawPMItems(atoi(aspectMode),cg.snap->ps.persistant[PERS_RESPAWNS_LEFT],cg.time,cg_pmWaitingList,cg_pmOldList,&api);
 tce_uiCoordinates=previous;
}

static int CG_PopupBigDuration(int type) { return CG_TimeForBigPopup((popupMessageBigType_t)type); }
void CG_DrawPMItemsBig(void) {
 extern qboolean tce_uiCoordinates;
 char aspectMode[16];
 qboolean previous=tce_uiCoordinates;
 tce_popup_draw_api_t api={trap_R_SetColor,CG_DrawPic,CG_PopupPaint,CG_PopupBigDuration,&cgs.media.limboFont1};
 trap_Cvar_VariableStringBuffer("cg_aspectMode",aspectMode,sizeof(aspectMode));
 tce_uiCoordinates=qtrue;
 TCE_DrawPMItemsBig(atoi(aspectMode),cg.time,cg_pmWaitingListBig,&api);
 tce_uiCoordinates=previous;
}

static const char *CG_PopupName(int client) { return cgs.clientinfo[client].name; }
static const char *CG_PopupLocation(const float *origin) { return BG_GetLocationString((float *)origin); }
const char* CG_GetPMItemText(centity_t *cent) {
 static char text[1024];
 tce_popup_text_t state;
 tce_popup_text_api_t api={CG_ConfigString,CG_PopupName,CG_PopupLocation};
 state.kind=cent->currentState.effect1Time;state.action=cent->currentState.effect2Time;
 state.client=cent->currentState.effect3Time;state.density=cent->currentState.density;
 state.localTeam=cgs.clientinfo[cg.clientNum].team;
 state.snapshotTeam=cg.snap->ps.persistant[PERS_TEAM];VectorCopy(cent->currentState.origin,state.origin);
 return TCE_GetPMItemText(&state,&api,text,sizeof(text));
}

void CG_PlayPMItemSound( centity_t *cent )
{
	switch( cent->currentState.effect1Time ) {
		case PM_DYNAMITE:
			switch( cent->currentState.effect2Time ) {
				case 0:
					if( cent->currentState.teamNum == TEAM_AXIS )
						CG_SoundPlaySoundScript( "axis_hq_dynamite_planted", NULL, -1, qtrue );
					else
						CG_SoundPlaySoundScript( "allies_hq_dynamite_planted", NULL, -1, qtrue );
					break;
				case 1:
					if( cent->currentState.teamNum == TEAM_AXIS )
						CG_SoundPlaySoundScript( "axis_hq_dynamite_defused", NULL, -1, qtrue );
					else
						CG_SoundPlaySoundScript( "allies_hq_dynamite_defused", NULL, -1, qtrue );
					break;
			}
			break;
		case PM_MINES:
			if( cgs.clientinfo[cg.clientNum].team != cent->currentState.effect2Time ) {
				// inverted teams
				if( cent->currentState.effect2Time == TEAM_AXIS ) {
					CG_SoundPlaySoundScript( "allies_hq_mines_spotted", NULL, -1, qtrue );
				} else {
					CG_SoundPlaySoundScript( "axis_hq_mines_spotted", NULL, -1, qtrue );
				}
			}
			break;
		case PM_OBJECTIVE:
			switch( cent->currentState.density ) {
				case 0:
					if( cent->currentState.effect2Time == TEAM_AXIS )
						CG_SoundPlaySoundScript( "axis_hq_objective_taken", NULL, -1, qtrue );
					else
						CG_SoundPlaySoundScript( "allies_hq_objective_taken", NULL, -1, qtrue );
					break;
				case 1:
					if( cent->currentState.effect2Time == TEAM_AXIS )
						CG_SoundPlaySoundScript( "axis_hq_objective_secure", NULL, -1, qtrue );
					else
						CG_SoundPlaySoundScript( "allies_hq_objective_secure", NULL, -1, qtrue );
					break;
			}
			break;
		default:
			break;
	}
}

qhandle_t CG_GetPMItemIcon( centity_t* cent ) {
	switch( cent->currentState.effect1Time ) {
		case PM_CONSTRUCTION:
			if( cent->currentState.density == TEAM_AXIS ) {
				return cgs.media.pmImageAxisConstruct;
			}
			return cgs.media.pmImageAlliesConstruct;
		case PM_MINES:
			if( cent->currentState.effect2Time == TEAM_AXIS ) {
				return cgs.media.pmImageAlliesMine;
			}
			return cgs.media.pmImageAxisMine;
		default:
			return cgs.media.pmImages[cent->currentState.effect1Time];
	}

	return 0;
}



void CG_DrawKeyHint( rectDef_t* rect, const char* binding ) {
/*	int k1, k2;
	char buffer[256];
	char k[2] = { 0, 0 };
	float w;

	trap_Key_KeysForBinding( binding, &k1, &k2 );

	if( k1 != -1 ) {
		trap_Key_KeynumToStringBuf( k1, buffer, 256 );
		if( strlen( buffer ) != 1 ) {
			if( k2 != -1 ) {
				trap_Key_KeynumToStringBuf( k2, buffer, 256 );
				if( strlen( buffer ) == 1 ) {
					*k = toupper( *buffer );
				}
			}
		} else {
			*k = toupper( *buffer );
		}
	}

	if( !*k ) {
		return;
	}

	CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.hintKey );

	w = CG_Text_Width_Ext( k, 0.2f, 0, &cgs.media.limboFont1 );
	CG_Text_Paint_Ext( rect->x + ((rect->w - w) * 0.5f), rect->y + 14, 0.2f, 0.2f, colorWhite, k, 0, 0, 0, &cgs.media.limboFont1 );*/
}
