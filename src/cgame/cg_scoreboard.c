// cg_scoreboard -- draw the scoreboard on top of the game screen
#include "cg_local.h"


#define	SCOREBOARD_WIDTH	(31*BIGCHAR_WIDTH)

vec4_t clrUiBack = { 0.f, 0.f, 0.f, .6f };
vec4_t clrUiBar = { .16f, .2f, .17f, .8f };

/*
=================
WM_DrawObjectives
=================
*/

#define INFO_PLAYER_WIDTH		134
#define INFO_SCORE_WIDTH		56
#define INFO_XP_WIDTH			36
#define INFO_CLASS_WIDTH		50
#define INFO_LATENCY_WIDTH		40
#define INFO_LIVES_WIDTH		20
#define INFO_TEAM_HEIGHT		24
#define INFO_BORDER				2
#define INFO_LINE_HEIGHT		30
#define INFO_TOTAL_WIDTH		(INFO_PLAYER_WIDTH + INFO_CLASS_WIDTH + INFO_SCORE_WIDTH + INFO_LATENCY_WIDTH)

static void WM_DrawClientScore( int x, int y, score_t *score, float *color, float fade ) {
	int maxchars, offset;
	int i, j;
	float tempx;
	vec4_t hcolor;
	clientInfo_t *ci;
	char buf[64];

	if ( y + SMALLCHAR_HEIGHT >= 470 )
		return;

	ci = &cgs.clientinfo[score->client];

	if ( score->client == cg.snap->ps.clientNum ) {
		tempx = x;

		hcolor[3] = fade * 0.3;
		VectorSet( hcolor, .5f, .5f, .2f );			// DARK-RED

		CG_FillRect( tempx, y + 1, INFO_PLAYER_WIDTH - INFO_BORDER, SMALLCHAR_HEIGHT - 1, hcolor );
		tempx += INFO_PLAYER_WIDTH;

		if ( ci->team == TEAM_SPECTATOR ) {
			int width;
			width = INFO_CLASS_WIDTH + INFO_SCORE_WIDTH + INFO_LATENCY_WIDTH;

			CG_FillRect( tempx, y + 1, width - INFO_BORDER, SMALLCHAR_HEIGHT - 1, hcolor );
			tempx += width;
		} else {
			CG_FillRect( tempx, y + 1, INFO_CLASS_WIDTH - INFO_BORDER, SMALLCHAR_HEIGHT - 1, hcolor );
			tempx += INFO_CLASS_WIDTH;

			if( cg_gameType.integer == GT_WOLF_LMS ) {
				CG_FillRect( tempx, y + 1, INFO_SCORE_WIDTH - INFO_BORDER, SMALLCHAR_HEIGHT - 1, hcolor );
				tempx += INFO_SCORE_WIDTH;
			} else {
				CG_FillRect( tempx, y + 1, INFO_XP_WIDTH - INFO_BORDER, SMALLCHAR_HEIGHT - 1, hcolor );
				tempx += INFO_XP_WIDTH;				
			}

			CG_FillRect( tempx, y + 1, INFO_LATENCY_WIDTH - INFO_BORDER, SMALLCHAR_HEIGHT - 1, hcolor );
			tempx += INFO_LATENCY_WIDTH;

			if( cg_gameType.integer != GT_WOLF_LMS ) {
				CG_FillRect( tempx, y + 1, INFO_LIVES_WIDTH - INFO_BORDER, SMALLCHAR_HEIGHT - 1, hcolor );
				tempx += INFO_LIVES_WIDTH;
			}
		}
	}

	tempx = x;

	// DHM - Nerve
	VectorSet( hcolor, 1, 1, 1 );
	hcolor[3] = fade;

	maxchars = 16;
	offset = 0;

	if ( ci->team != TEAM_SPECTATOR ) {
		if ( ci->powerups & ( (1 << PW_REDFLAG) | (1 << PW_BLUEFLAG) ) ) {
			CG_DrawPic( tempx-4, y, 16, 16, cgs.media.objectiveShader );
			offset += 8;
			tempx += 12;
			maxchars -= 2;
		}

		// draw the skull icon if out of lives
		if( score->respawnsLeft == -2 || (cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR && ci->team == cgs.clientinfo[cg.clientNum].team && cgs.clientinfo[score->client].health == -1 ) ) {
			CG_DrawPic( tempx, y, 18, 18, cgs.media.scoreEliminatedShader );
			offset += 18;
			tempx += 18;
			maxchars -= 2;
		} else if( cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR && ci->team == cgs.clientinfo[cg.clientNum].team && cgs.clientinfo[score->client].health == 0 ) {
			CG_DrawPic( tempx + 1, y + 1, 16, 16, cgs.media.medicIcon );
			offset += 18;
			tempx += 18;
			maxchars -= 2;			
		}
	}

	// draw name
	CG_DrawStringExt( tempx, y, ci->name, hcolor, qfalse, qfalse, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, maxchars );
	maxchars -= CG_DrawStrlen( ci->name );

	// draw medals
	buf[0] = '\0';
	for( i = 0; i < SK_NUM_SKILLS; i++ ) {
		for( j = 0; j < ci->medals[i]; j++ )
			Q_strcat( buf, sizeof(buf), va( "^%c%c", COLOR_RED + i, skillNames[i][0] ) );
	}
	maxchars--;
	CG_DrawStringExt( tempx + (BG_drawStrlen(ci->name) * SMALLCHAR_WIDTH + SMALLCHAR_WIDTH), y, buf, hcolor, qfalse, qfalse, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, maxchars );

	tempx += INFO_PLAYER_WIDTH - offset;

	if ( ci->team == TEAM_SPECTATOR ) {
		const char *s;
		int w, totalwidth;

		totalwidth = INFO_CLASS_WIDTH + INFO_SCORE_WIDTH + INFO_LATENCY_WIDTH - 8;

		s = CG_TranslateString( "^3SPECTATOR" );
		w = CG_DrawStrlen( s ) * SMALLCHAR_WIDTH;

		CG_DrawSmallString( tempx + totalwidth - w, y, s, fade );
		return;
	}
	// OSP - allow MV clients see the class of its merged client's on the scoreboard
	else if ( cg.snap->ps.persistant[PERS_TEAM] == ci->team || CG_mvMergedClientLocate(score->client) ) {
		CG_DrawSmallString( tempx, y, CG_TranslateString( BG_ShortClassnameForNumber( score->playerClass ) ), fade );
	}
	tempx += INFO_CLASS_WIDTH;

	CG_DrawSmallString( tempx, y, va( "%3i", score->score ), fade );
	if( cg_gameType.integer == GT_WOLF_LMS ) {
		tempx += INFO_SCORE_WIDTH;
	} else {
		tempx += INFO_XP_WIDTH;
	}

	CG_DrawSmallString( tempx, y, va( "%4i", score->ping ), fade );
	tempx += INFO_LATENCY_WIDTH;

	if( cg_gameType.integer != GT_WOLF_LMS ) {
		if( score->respawnsLeft >= 0 ) {
			CG_DrawSmallString( tempx, y, va( "%2i", score->respawnsLeft ), fade );
		} else {
			CG_DrawSmallString( tempx, y, " -", fade );
		}
		tempx += INFO_LIVES_WIDTH;
	}
}

const char* WM_TimeToString( float msec ) {
	int mins, seconds, tens;

	/* Original30059d90 multiplies the binary32 constant before truncation. */
	seconds = (int)((double)msec * (double)0.001f);
	mins = seconds / 60;
	seconds -= mins * 60;
	tens = seconds / 10;
	seconds -= tens * 10;

	return va( "%i:%i%i", mins, tens, seconds );
}

static void WM_DrawClientScore_Small( int x, int y, score_t *score, float *color, float fade ) {
	int maxchars, offset;
	float tempx;
	vec4_t hcolor;
	clientInfo_t *ci;

	if ( y + SMALLCHAR_HEIGHT >= 470 )
		return;

	ci = &cgs.clientinfo[score->client];

	if ( score->client == cg.snap->ps.clientNum ) {
		tempx = x;

		hcolor[3] = fade * 0.3;
		VectorSet( hcolor, .5f, .5f, .2f );			// DARK-RED

		CG_FillRect( tempx, y + 1, INFO_PLAYER_WIDTH - INFO_BORDER, MINICHAR_HEIGHT - 1, hcolor );
		tempx += INFO_PLAYER_WIDTH;

		if ( ci->team == TEAM_SPECTATOR ) {
			int width;
			width = INFO_CLASS_WIDTH + INFO_SCORE_WIDTH + INFO_LATENCY_WIDTH;

			CG_FillRect( tempx, y + 1, width - INFO_BORDER, MINICHAR_HEIGHT - 1, hcolor );
			tempx += width;
		} else {
			CG_FillRect( tempx, y + 1, INFO_CLASS_WIDTH - INFO_BORDER, MINICHAR_HEIGHT - 1, hcolor );
			tempx += INFO_CLASS_WIDTH;

			if( cg_gameType.integer == GT_WOLF_LMS ) {
				CG_FillRect( tempx, y + 1, INFO_SCORE_WIDTH - INFO_BORDER, MINICHAR_HEIGHT - 1, hcolor );
				tempx += INFO_SCORE_WIDTH;
			} else {
				CG_FillRect( tempx, y + 1, INFO_XP_WIDTH - INFO_BORDER, MINICHAR_HEIGHT - 1, hcolor );
				tempx += INFO_XP_WIDTH;				
			}

			CG_FillRect( tempx, y + 1, INFO_LATENCY_WIDTH - INFO_BORDER, MINICHAR_HEIGHT - 1, hcolor );
			tempx += INFO_LATENCY_WIDTH;

			if( cg_gameType.integer != GT_WOLF_LMS ) {
				CG_FillRect( tempx, y + 1, INFO_LIVES_WIDTH - INFO_BORDER, MINICHAR_HEIGHT - 1, hcolor );
				tempx += INFO_LIVES_WIDTH;
			}
		}
	}

	tempx = x;

	// DHM - Nerve
	VectorSet( hcolor, 1, 1, 1 );
	hcolor[3] = fade;

	maxchars = 17;
	offset = 0;

	if ( ci->team != TEAM_SPECTATOR ) {
		if ( ci->powerups & ( (1 << PW_REDFLAG) | (1 << PW_BLUEFLAG) ) ) {
			CG_DrawPic( tempx-2, y-4, 20, 20, trap_R_RegisterShader( "models/multiplayer/treasure/treasure" ) );
			offset += 14;
			tempx += 14;
			maxchars -= 2;
		}

		// draw the skull icon if out of lives
		if ( score->respawnsLeft == -2 || ( cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR && ci->team == cgs.clientinfo[cg.clientNum].team && cgs.clientinfo[score->client].health == -1 ) ) {
			CG_DrawPic( tempx, y, 12, 12, cgs.media.scoreEliminatedShader );
			offset += 14;
			tempx += 14;
			maxchars -= 2;
		} else if( cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR && ci->team == cgs.clientinfo[cg.clientNum].team && cgs.clientinfo[score->client].health == 0 ) {
			CG_DrawPic( tempx + 1, y + 1, 10, 10, cgs.media.medicIcon );
			offset += 14;
			tempx += 14;
			maxchars -= 2;
		}
	}

	// draw name
	CG_DrawStringExt( tempx, y, ci->name, hcolor, qfalse, qfalse, MINICHAR_WIDTH, MINICHAR_HEIGHT, maxchars );
	tempx += INFO_PLAYER_WIDTH - offset;
	// dhm - nerve

	if ( ci->team == TEAM_SPECTATOR ) {
		const char *s;
		int w, totalwidth;

		totalwidth = INFO_CLASS_WIDTH + INFO_SCORE_WIDTH + INFO_LATENCY_WIDTH - 8;

		s = CG_TranslateString( "^3SPECTATOR" );
		w = CG_DrawStrlen( s ) * MINICHAR_WIDTH;

		CG_DrawSmallString( tempx + totalwidth - w, y, s, fade );
		return;
	}
	else if ( cg.snap->ps.persistant[PERS_TEAM] == ci->team ) {
		CG_DrawStringExt(	tempx, y, CG_TranslateString( BG_ShortClassnameForNumber( score->playerClass ) ), hcolor, qfalse, qfalse, MINICHAR_WIDTH, MINICHAR_HEIGHT, 0 );
//		CG_DrawSmallString( tempx, y, CG_TranslateString( s ), fade );
	}
	tempx += INFO_CLASS_WIDTH;

	CG_DrawStringExt( tempx, y, va( "%3i", score->score ), hcolor, qfalse, qfalse, MINICHAR_WIDTH, MINICHAR_HEIGHT, 0 );
	if( cg_gameType.integer == GT_WOLF_LMS ) {
		tempx += INFO_SCORE_WIDTH;
	} else {
		tempx += INFO_XP_WIDTH;
	}

	CG_DrawStringExt( tempx, y, va( "%4i", score->ping ), hcolor, qfalse, qfalse, MINICHAR_WIDTH, MINICHAR_HEIGHT, 0 );
	tempx += INFO_LATENCY_WIDTH;

	if( cg_gameType.integer != GT_WOLF_LMS ) {
		if( score->respawnsLeft >= 0 ) {
			CG_DrawStringExt( tempx, y, va( "%2i", score->respawnsLeft ), hcolor, qfalse, qfalse, MINICHAR_WIDTH, MINICHAR_HEIGHT, 0 );
		} else {
			CG_DrawStringExt( tempx, y, " -", hcolor, qfalse, qfalse, MINICHAR_WIDTH, MINICHAR_HEIGHT, 0 );
		}
		tempx += INFO_LIVES_WIDTH;
	}
}

static int WM_DrawInfoLine( int x, int y, float fade ) {
	int w, defender, winner;
	const char *s;
	vec4_t tclr =	{ 0.6f,		0.6f,		0.6f,		1.0f };

	if ( cg.snap->ps.pm_type != PM_INTERMISSION ) {
		return y;
	}

	w = 360;
//	CG_DrawPic( 426 - w/2, y, w, INFO_LINE_HEIGHT, trap_R_RegisterShaderNoMip( "ui/assets/mp_line_strip.tga" ) );

	s = CG_ConfigString( CS_MULTI_INFO );
	defender = atoi( Info_ValueForKey( s, "defender" ) );

	s = CG_ConfigString( CS_MULTI_MAPWINNER );
	winner = atoi( Info_ValueForKey( s, "winner" ) );

	if ( cgs.currentRound ) {
		// first round
		s = va( CG_TranslateString( "CLOCK IS NOW SET TO %s!" ), WM_TimeToString( cgs.nextTimeLimit * 60000.f ) );
	}
	else {
		// second round
		if ( !defender ) {
			if ( winner != defender )
				s = "ALLIES SUCCESSFULLY BEAT THE CLOCK!";
			else
				s = "ALLIES COULDN'T BEAT THE CLOCK!";
		}
		else {
			if ( winner != defender )
				s = "AXIS SUCCESSFULLY BEAT THE CLOCK!";
			else
				s = "AXIS COULDN'T BEAT THE CLOCK!";
		}

		s = CG_TranslateString( s );
	}

	CG_FillRect( 426 - w/2, y, w, 20, clrUiBar );
	CG_DrawRect_FixedBorder( 426 - w/2, y, w, 20, 1, colorBlack );

	w = CG_Text_Width_Ext( s, 0.25f, 0, &cgs.media.limboFont1 );

	CG_Text_Paint_Ext( 426 - w*0.5f, y + 15, 0.25f, 0.25f, tclr, s, 0, 0, 0, &cgs.media.limboFont1 );
//	CG_DrawSmallString( 426 - w/2, ( y + INFO_LINE_HEIGHT / 2 ) - SMALLCHAR_HEIGHT / 2, s, fade );
	return y + INFO_LINE_HEIGHT + 6;
}

static int WM_TeamScoreboard( int x, int y, team_t team, float fade, int maxrows ) {
	vec4_t hcolor;
	float tempx, tempy;
	int height, width;
	int i;
	int count = 0;
	vec4_t tclr =	{ 0.6f,		0.6f,		0.6f,		1.0f };

	height = SMALLCHAR_HEIGHT * maxrows;
	width = INFO_PLAYER_WIDTH + INFO_CLASS_WIDTH + INFO_SCORE_WIDTH + INFO_LATENCY_WIDTH;

	CG_FillRect( x-5, y-2, width+5, 21, clrUiBack );
	CG_FillRect( x-5, y-2, width+5, 21, clrUiBar );	
	
	Vector4Set( hcolor, 0, 0, 0, fade );
	CG_DrawRect_FixedBorder( x-5, y-2, width+5, 21, 1, colorBlack );

	// draw header
	if( cg_gameType.integer == GT_WOLF_LMS ) {
		char *s;
		if ( team == TEAM_AXIS ) {
			s = va( "%s [%d] (%d %s)", CG_TranslateString( "TERRORISTS" ), cg.teamScores[0], cg.teamPlayers[team], CG_TranslateString("PLAYERS") );
			s = va( "%s ^3%s", s, cg.teamFirstBlood == TEAM_AXIS ? CG_TranslateString("FIRST BLOOD") : "" );

			CG_Text_Paint_Ext( x, y + 13, 0.25f, 0.25f, tclr, s, 0, 0, 0, &cgs.media.limboFont1 );
		} else if ( team == TEAM_ALLIES ) {
			s = va( "%s [%d] (%d %s)", CG_TranslateString( "SPECOPS" ), cg.teamScores[1], cg.teamPlayers[team], CG_TranslateString("PLAYERS") );
			s = va( "%s ^3%s", s, cg.teamFirstBlood == TEAM_ALLIES ? CG_TranslateString("FIRST BLOOD") : "" );

			CG_Text_Paint_Ext( x, y + 13, 0.25f, 0.25f, tclr, s, 0, 0, 0, &cgs.media.limboFont1 );
		}
	} else {
		if ( team == TEAM_AXIS ) {
			CG_Text_Paint_Ext( x, y + 13, 0.25f, 0.25f, tclr, va( "%s [%d] (%d %s)", CG_TranslateString( "TERRORISTS" ), cg.teamScores[0], cg.teamPlayers[team], CG_TranslateString("PLAYERS") ), 0, 0, 0, &cgs.media.limboFont1 );
		} else if ( team == TEAM_ALLIES ) {
			CG_Text_Paint_Ext( x, y + 13, 0.25f, 0.25f, tclr, va( "%s [%d] (%d %s)", CG_TranslateString( "SPECOPS" ), cg.teamScores[1], cg.teamPlayers[team], CG_TranslateString("PLAYERS") ), 0, 0, 0, &cgs.media.limboFont1 );
		}
	}

	y += SMALLCHAR_HEIGHT + 3;

	// save off y val
	tempy = y;

	// draw color bands
	for ( i = 0; i <= maxrows; i++ ) {
		if ( i % 2 == 0 )
			VectorSet( hcolor, (80.f/255.f), (80.f/255.f), (80.f/255.f) );			// LIGHT BLUE
		else
			VectorSet( hcolor, (0.f/255.f), (0.f/255.f), (0.f/255.f) );			// DARK BLUE
		hcolor[3] = fade * 0.3;

		CG_FillRect( x-5, y, width+5, SMALLCHAR_HEIGHT+1, hcolor );
		trap_R_SetColor( colorBlack );
		CG_DrawTopBottom( x-5, y, width+5, SMALLCHAR_HEIGHT+1, 1 );
		trap_R_SetColor( NULL );

		y += SMALLCHAR_HEIGHT;
	}
		hcolor[3] = 1;

	y = tempy;

	tempx = x;

	CG_FillRect( x-5, y-1, width+5, 18, clrUiBack );
	//CG_FillRect( x-5, y-1, width+5, 18, clrUiBar );
	trap_R_SetColor( colorBlack );
	CG_DrawTopBottom( x-5, y-1, width+5, 18, 1 );
	trap_R_SetColor( NULL );

	// draw player info headings
	CG_DrawSmallString( tempx, y, CG_TranslateString( "Name" ), fade );
	tempx += INFO_PLAYER_WIDTH;

	CG_DrawSmallString( tempx, y, CG_TranslateString( "Class" ), fade );
	tempx += INFO_CLASS_WIDTH;

	if( cgs.gametype == GT_WOLF_LMS ) {
		CG_DrawSmallString( tempx, y, CG_TranslateString( "Score" ), fade );
		tempx += INFO_SCORE_WIDTH;
	} else {
		CG_DrawSmallString( tempx + 1 * SMALLCHAR_WIDTH, y, CG_TranslateString( "XP" ), fade );
		tempx += INFO_XP_WIDTH;
	}

	CG_DrawSmallString( tempx, y, CG_TranslateString( "Ping" ), fade );
	tempx += INFO_LATENCY_WIDTH;

	if( cgs.gametype != GT_WOLF_LMS ) {
		CG_DrawPicST( tempx + 2, y, INFO_LIVES_WIDTH - 4, 16, 0.f, 0.f, 0.5f, 1.f, team == TEAM_ALLIES ? cgs.media.hudAlliedHelmet : cgs.media.hudAxisHelmet );
		tempx += INFO_LIVES_WIDTH;
	}
	

	y += SMALLCHAR_HEIGHT;

	// draw player info
	VectorSet( hcolor, 1, 1, 1 );
	hcolor[3] = fade;

	cg.teamPlayers[team] = 0; // JPW NERVE
	for ( i = 0; i < cg.numScores; i++ ) {
		if ( team != cgs.clientinfo[ cg.scores[i].client ].team )
			continue;

		cg.teamPlayers[team]++;
	}

	count = 0;
	for( i = 0; i < cg.numScores && count < maxrows; i++ ) {
		if( team != cgs.clientinfo[ cg.scores[i].client ].team ) {
			continue;
		}

		if( cg.teamPlayers[team] > maxrows ) {
			WM_DrawClientScore_Small( x, y, &cg.scores[i], hcolor, fade );
			y += MINICHAR_HEIGHT;
		} else {
			WM_DrawClientScore( x, y, &cg.scores[i], hcolor, fade );
			y += SMALLCHAR_HEIGHT;
		}

		count++;
	}

	// draw spectators
	y += SMALLCHAR_HEIGHT;

	for ( i = 0; i < cg.numScores; i++ ) {
		if ( cgs.clientinfo[ cg.scores[i].client ].team != TEAM_SPECTATOR )
			continue;
		if ( team == TEAM_AXIS && ( i % 2 ) )
			continue;
		if ( team == TEAM_ALLIES && ( ( i + 1 ) % 2 ) )
			continue;

		WM_DrawClientScore( x, y, &cg.scores[i], hcolor, fade );
		y += SMALLCHAR_HEIGHT;
	}

	return y;
}
// -NERVE - SMF

/*
=================
CG_DrawScoreboard

Draw the normal in-game scoreboard
=================
*/
/* Original TC table geometry is converted at the drawing boundary only. */
#include "tce_scoreboard.h"
#include "tce_score_objectives.h"
#include "tce_team_scoreboard.h"
#include "tce_client_score.h"
/* Native TC854 coordinates: CG_AdjustFrom640 performs aspect conversion. */
static void TCE_ScoreFill(float x,float y,float w,float h,const float *c) {
    CG_FillRect(x,y,w,h,c);
}
static void TCE_ScoreBorder(float x,float y,float w,float h,int size,const float *c) {
    vec4_t color;
    memcpy(color,c,sizeof(color));
    CG_DrawRect_FixedBorder(x,y,w,h,size,color);
}
static void TCE_ScoreTopBottom(float x,float y,float w,float h,float size) {
    CG_DrawTopBottom_NoScale(x,y,w,h,size);
}
static void TCE_ScoreText(float x,float y,float scale,const float *c,const char *text) {
    vec4_t color;
    memcpy(color,c,sizeof(color));
    CG_Text_Paint_Ext(x,y,scale,scale,color,text,0,0,0,&cgs.media.limboFont1);
}
static void TCE_ScoreSmallText(int x,int y,const char *text,float fade) {
    CG_DrawSmallString((int)(x),y,text,fade);
}
static const char *TCE_ScoreTranslate(const char *text) { return CG_TranslateString(text); }
/* Seven-field SDK servers do not provide the TC statistics used below. */
static void TCE_ScoreLegacyClientRow(int x,int y,int index,const float *color,float fade) {
    score_t *score=&cg.scores[index];
    clientInfo_t *ci=&cgs.clientinfo[score->client];
    vec4_t highlight={.75f,.5f,0,0};
    char name[MAX_NAME_LENGTH];
    int i;float tx=(float)x+140;
    if(y+16>469)return;
    if(score->client==cg.snap->ps.clientNum){highlight[3]=fade*.3f;TCE_ScoreFill((float)x,(float)(y-13),394,15,highlight);}
    Q_strncpyz(name,ci->name,sizeof(name));
    while(name[0]&&CG_Text_Width_Ext(name,.2f,0,&cgs.media.limboFont1)>132)name[strlen(name)-1]=0;
    TCE_ScoreText((float)x,(float)y,.2f,color,name);
    if(ci->team==TEAM_SPECTATOR){TCE_ScoreText(tx+36,(float)y,.2f,color,CG_TranslateString("Spectator"));return;}
    if(cgs.gametype==2||(cgs.gametype>=5&&cgs.gametype<=7)) {
        for(i=0;i<4;++i){const int values[4]={score->kills,score->deaths,score->suicides,score->teamKills};TCE_ScoreText(tx,(float)y,.2f,color,tce_scoreExtended[index]?va("%d",values[i]):"-");tx+=28;}
        TCE_ScoreText(tx,(float)y,.2f,color,va("%d",score->score));tx+=42;
        TCE_ScoreText(tx,(float)y,.2f,color,tce_scoreExtended[index]?va("%4.2f",score->damageRating*.01f):"-");tx+=40;
    } else {TCE_ScoreText(tx,(float)y,.2f,color,va("%d",score->score));tx+=36;}
    TCE_ScoreText(tx,(float)y,.2f,color,tce_scoreExtended[index]?va("%d",score->classRating+1):"-");tx+=28;
    TCE_ScoreText(tx,(float)y,.2f,color,va("%d",score->ping));
}
static void TCE_ScorePic(float x,float y,float w,float h,int shader) {
    CG_DrawPic(x,y,w,h,shader);
}
static void TCE_ScoreLocateMerged(int client) { (void)CG_mvMergedClientLocate(client); }
static void TCE_ScoreClientRow(int x,int y,int index,const float *color,float fade) {
    const score_t *score=&cg.scores[index];
    const clientInfo_t *ci=&cgs.clientinfo[score->client];
    tce_clientScoreContext_t ctx;
    if(!tce_scoreExtended[index]) {
        TCE_ScoreLegacyClientRow(x,y,index,color,fade);return;
    }
    memset(&ctx,0,sizeof(ctx));
    ctx.name=ci->name;ctx.team=ci->team;ctx.powerups=ci->powerups;ctx.health=ci->health;
    ctx.localTeam=cgs.clientinfo[cg.clientNum].team;
    ctx.viewedClient=cg.snap->ps.clientNum;
    ctx.viewedTeam=cg.snap->ps.persistant[PERS_TEAM];
    ctx.viewedHealth=cg.snap->ps.stats[STAT_HEALTH];ctx.pmType=cg.snap->ps.pm_type;
    ctx.gameType=cg_gameType.integer;ctx.killMessage=cgs.tceKillMessage;
    ctx.objectiveShader=cgs.media.objectiveShader;
    ctx.eliminatedShader=cgs.media.scoreEliminatedShader;ctx.medicShader=cgs.media.medicIcon;
    ctx.fill=TCE_ScoreFill;ctx.pic=TCE_ScorePic;ctx.text=TCE_ScoreText;
    ctx.drawStrlen=CG_DrawStrlen;ctx.translate=TCE_ScoreTranslate;
    ctx.locateMergedClient=TCE_ScoreLocateMerged;
    TCE_WM_DrawClientScore(x,y,score,color,fade,&ctx);
}
static int TCE_ObjectivesReinforce(int menu) {
    return CG_CalculateReinfTime((qboolean)menu);
}

static int TCE_ObjectivesWidth(const char *text,float scale) {
    return CG_Text_Width_Ext(text,scale,0,&cgs.media.limboFont1);
}
int WM_DrawObjectives(int x,int y,int width,float fade) {
    tce_objectivesContext_t c;
    memset(&c,0,sizeof(c));
    c.pmType=cg.snap->ps.pm_type;c.gameState=cgs.gamestate;c.gameType=cgs.gametype;
    c.time=cg.time;c.startTime=cgs.levelStartTime;c.intermissionStartTime=cgs.intermissionStartTime;
    c.viewedTeam=cgs.clientinfo[cg.snap->ps.clientNum].team;c.currentRound=cgs.currentRound;
    c.wonRounds[0]=cg.teamWonRounds[0];c.wonRounds[1]=cg.teamWonRounds[1];
    c.campaignMap=cgs.currentCampaignMap;c.campaignMaps=cgs.campaignData.mapCount;c.timelimit=cgs.timelimit;
    c.backColor=clrUiBack;c.barColor=clrUiBar;c.black=colorBlack;
    c.config=CG_ConfigString;c.infoValue=Info_ValueForKey;c.translate=TCE_ScoreTranslate;
    c.reinforce=TCE_ObjectivesReinforce;c.width=TCE_ObjectivesWidth;
    c.fill=TCE_ScoreFill;c.border=TCE_ScoreBorder;c.text=TCE_ScoreText;
    return TCE_WM_DrawObjectives(x,y,width,fade,&c);
}
static int TCE_ScoreObjectives(int x,int y,int width,float fade) {
    return WM_DrawObjectives(x,y,width,fade);
}
static int TCE_ScoreTeam(int x,int y,int team,float fade,int rows) {
    tce_teamScoreboardContext_t ctx;int clients[MAX_CLIENTS],teams[MAX_CLIENTS],i;
    memset(&ctx,0,sizeof(ctx));
    for(i=0;i<MAX_CLIENTS;++i){clients[i]=cg.scores[i].client;teams[i]=cgs.clientinfo[i].team;}
    ctx.gametype=cgs.gametype;ctx.gameTypeCvar=cg_gameType.integer;
    ctx.firstBlood=cg.teamFirstBlood;ctx.teamScores[0]=cg.teamScores[0];ctx.teamScores[1]=cg.teamScores[1];
    ctx.numScores=cg.numScores;ctx.teamPlayers=cg.teamPlayers;ctx.scoreClients=clients;ctx.clientTeams=teams;
    ctx.backColor=clrUiBack;ctx.barColor=clrUiBar;ctx.black=colorBlack;
    ctx.translate=TCE_ScoreTranslate;ctx.fill=TCE_ScoreFill;ctx.border=TCE_ScoreBorder;
    ctx.setColor=trap_R_SetColor;ctx.topBottom=TCE_ScoreTopBottom;ctx.text=TCE_ScoreText;
    ctx.smallText=TCE_ScoreSmallText;ctx.clientRow=TCE_ScoreClientRow;
    return TCE_WM_TeamScoreboard(x,y,team,fade,rows,&ctx);
}
qboolean CG_DrawScoreboard(void) {
    extern qboolean tce_uiCoordinates;
    qboolean previousCoordinates=tce_uiCoordinates;
    int result;
    tce_scoreboardContext_t ctx;
    ctx.paused=cg_paused.integer;ctx.warmup=cg.warmup;
    ctx.demoPlayback=cg.demoPlayback;ctx.snapshotPmType=cg.snap->ps.pm_type;
    ctx.showScores=cg.showScores;ctx.cameraMode=cg.cameraMode;
    ctx.predictedPmType=cg.predictedPlayerState.pm_type;
    ctx.scoreFadeTime=cg.scoreFadeTime;ctx.gametype=cgs.gametype;
    ctx.killerName=cg.killerName;ctx.fadeColor=CG_FadeColor;
    ctx.objectives=TCE_ScoreObjectives;ctx.infoLine=WM_DrawInfoLine;
    ctx.teamBoard=TCE_ScoreTeam;
    tce_uiCoordinates=qtrue;
    result=TCE_CG_DrawScoreboard(&ctx);
    tce_uiCoordinates=previousCoordinates;
    return result;
}

