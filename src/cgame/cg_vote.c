#include "cg_local.h"
char *BindingFromName(const char *name);

/* Original Windows 30025db0: vote/complaint/fireteam text controller. */
void CG_DrawVote(void) {
	char	*s;
	char str1[32], str2[32];
	float color[4] = { 1, 1, 0, 1 };
	int		sec;

	if( cgs.complaintEndTime > cg.time && !cg.demoPlayback && cg_complaintPopUp.integer > 0 && cgs.complaintClient >= 0 ) {
		Q_strncpyz( str1, BindingFromName( "vote yes" ), 32 );
		Q_strncpyz( str2, BindingFromName( "vote no" ), 32 );

		s = va( CG_TranslateString( "File complaint against %s for team-killing?" ), cgs.clientinfo[cgs.complaintClient].name);
		CG_Text_Paint_Ext(8, 200, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);

		s = va( CG_TranslateString( "Press '%s' for YES, or '%s' for No" ), str1, str2 );
		CG_Text_Paint_Ext(8, 214, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);
		return;
	}

	if( cgs.applicationEndTime > cg.time && cgs.applicationClient >= 0 ) {
		Q_strncpyz( str1, BindingFromName( "vote yes" ), 32 );
		Q_strncpyz( str2, BindingFromName( "vote no" ), 32 );

		s = va( CG_TranslateString( "Accept %s's application to join your fireteam?" ), cgs.clientinfo[cgs.applicationClient].name);
		CG_Text_Paint_Ext(8, 200, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);

		s = va( CG_TranslateString( "Press '%s' for YES, or '%s' for No" ), str1, str2 );
		CG_Text_Paint_Ext(8, 214, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);
		return;
	}

	if( cgs.propositionEndTime > cg.time && cgs.propositionClient >= 0) {
		Q_strncpyz( str1, BindingFromName( "vote yes" ), 32 );
		Q_strncpyz( str2, BindingFromName( "vote no" ), 32 );

		s = va( CG_TranslateString( "Accept %s's proposition to invite %s to join your fireteam?" ), cgs.clientinfo[cgs.propositionClient2].name, cgs.clientinfo[cgs.propositionClient].name);
		CG_Text_Paint_Ext(8, 200, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);

		s = va( CG_TranslateString( "Press '%s' for YES, or '%s' for No" ), str1, str2 );
		CG_Text_Paint_Ext(8, 214, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);
		return;
	}

	if ( cgs.invitationEndTime > cg.time && cgs.invitationClient >= 0 ) {
		Q_strncpyz( str1, BindingFromName( "vote yes" ), 32 );
		Q_strncpyz( str2, BindingFromName( "vote no" ), 32 );

		s = va( CG_TranslateString( "Accept %s's invitation to join their fireteam?" ), cgs.clientinfo[cgs.invitationClient].name);
		CG_Text_Paint_Ext(8, 200, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);

		s = va( CG_TranslateString( "Press '%s' for YES, or '%s' for No" ), str1, str2 );
		CG_Text_Paint_Ext(8, 214, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);
		return;
	}

	if ( cgs.autoFireteamEndTime > cg.time && cgs.autoFireteamNum == -1 ) {
		Q_strncpyz( str1, BindingFromName( "vote yes" ), 32 );
		Q_strncpyz( str2, BindingFromName( "vote no" ), 32 );

		s = "Make Fireteam private?";
		CG_Text_Paint_Ext(8, 200, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);

		s = va( CG_TranslateString( "Press '%s' for YES, or '%s' for No" ), str1, str2 );
		CG_Text_Paint_Ext(8, 214, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);
		return;
	}

	if ( cgs.autoFireteamCreateEndTime > cg.time && cgs.autoFireteamCreateNum == -1 ) {
		Q_strncpyz( str1, BindingFromName( "vote yes" ), 32 );
		Q_strncpyz( str2, BindingFromName( "vote no" ), 32 );

		s = "Create a Fireteam?";
		CG_Text_Paint_Ext(8, 200, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);

		s = va( CG_TranslateString( "Press '%s' for YES, or '%s' for No" ), str1, str2 );
		CG_Text_Paint_Ext(8, 214, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);
		return;
	}
	
	if ( cgs.autoFireteamJoinEndTime > cg.time && cgs.autoFireteamJoinNum == -1 ) {
		Q_strncpyz( str1, BindingFromName( "vote yes" ), 32 );
		Q_strncpyz( str2, BindingFromName( "vote no" ), 32 );

		s = "Join a Fireteam?";
		CG_Text_Paint_Ext(8, 200, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);

		s = va( CG_TranslateString( "Press '%s' for YES, or '%s' for No" ), str1, str2 );
		CG_Text_Paint_Ext(8, 214, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);
		return;
	}
	

	if( cgs.voteTime ) {
		Q_strncpyz( str1, BindingFromName( "vote yes" ), 32 );
		Q_strncpyz( str2, BindingFromName( "vote no" ), 32 );

		// play a talk beep whenever it is modified
		if( cgs.voteModified ) {
			cgs.voteModified = qfalse;
		}

		sec = ( VOTE_TIME - ( cg.time - cgs.voteTime ) ) / 1000;
		if( sec < 0 ) {
			sec = 0;
		}

		if( !Q_stricmpn( cgs.voteString, "kick", 4 ) ) {
			if( strlen( cgs.voteString ) > 5 ) {
				int nameindex;
				char buffer[ 128 ];
				Q_strncpyz( buffer, cgs.voteString + 5, sizeof( buffer ) );
				Q_CleanStr( buffer );

				for( nameindex = 0; nameindex < MAX_CLIENTS; nameindex++ ) {
					if( !cgs.clientinfo[ nameindex ].infoValid ) {
						continue;
					}

					if( !Q_stricmp( cgs.clientinfo[ nameindex ].cleanname, buffer ) ) {
						if( cgs.clientinfo[ nameindex ].team != TEAM_SPECTATOR && cgs.clientinfo[ nameindex ].team != cgs.clientinfo[ cg.clientNum ].team ) {
							return;
						}
					}
				}
			}
		}

		if ( !(cg.snap->ps.eFlags & EF_VOTED) ) {
			s = va( CG_TranslateString( "VOTE(%i): %s" ), sec, cgs.voteString);
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);

			if( cgs.clientinfo[cg.clientNum].team != TEAM_AXIS && cgs.clientinfo[cg.clientNum].team != TEAM_ALLIES ) {
				s = CG_TranslateString( "Cannot vote as Spectator" );
			} else {
				s = va( CG_TranslateString( "YES(%s):%i, NO(%s):%i" ), str1, cgs.voteYes, str2, cgs.voteNo );
			}
			CG_Text_Paint_Ext(8, 214, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);
			return;
		} else {
			s = va( CG_TranslateString( "YOU VOTED ON: %s" ), cgs.voteString);
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);

			s = va( CG_TranslateString( "Y:%i, N:%i" ), cgs.voteYes, cgs.voteNo );
			CG_Text_Paint_Ext(8, 214, .2f, .2f, color, s, 0, 0, 3, &cgs.media.limboFont1);
			return;
		}
	}

	if( cgs.complaintEndTime > cg.time && !cg.demoPlayback && cg_complaintPopUp.integer > 0 && cgs.complaintClient < 0 ) {
		if( cgs.complaintClient == -1 ) {
			s = "Your complaint has been filed";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}
		if( cgs.complaintClient == -2 ) {
			s = "Complaint dismissed";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}
		if( cgs.complaintClient == -3 ) {
			s = "Server Host cannot be complained against";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}
		if( cgs.complaintClient == -4 ) {
			s = "You were team-killed by the Server Host";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}
	}

	if( cgs.applicationEndTime > cg.time && cgs.applicationClient < 0 ) {
		if( cgs.applicationClient == -1 ) {
			s = "Your application has been submitted";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.applicationClient == -2 ) {
			s = "Your application failed";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.applicationClient == -3 ) {
			s = "Your application has been approved";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.applicationClient == -4 ) {
			s = "Your application reply has been sent";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}
	}

	if( cgs.propositionEndTime > cg.time && cgs.propositionClient < 0) {
		if( cgs.propositionClient == -1 ) {
			s = "Your proposition has been submitted";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.propositionClient == -2 ) {
			s = "Your proposition was rejected";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.propositionClient == -3 ) {
			s = "Your proposition was accepted";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.propositionClient == -4 ) {
			s = "Your proposition reply has been sent";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}
	}

	if( cgs.invitationEndTime > cg.time && cgs.invitationClient < 0 ) {
		if( cgs.invitationClient == -1 ) {
			s = "Your invitation has been submitted";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.invitationClient == -2 ) {
			s = "Your invitation was rejected";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.invitationClient == -3 ) {
			s = "Your invitation was accepted";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.invitationClient == -4 ) {
			s = "Your invitation reply has been sent";
			CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
			return;
		}

		if( cgs.invitationClient < 0 ) {
			return;
		}
	}

	if( (cgs.autoFireteamEndTime > cg.time && cgs.autoFireteamNum == -2) || (cgs.autoFireteamCreateEndTime > cg.time && cgs.autoFireteamCreateNum == -2) || (cgs.autoFireteamJoinEndTime > cg.time && cgs.autoFireteamJoinNum == -2)) {
		s = "Response Sent";
		CG_Text_Paint_Ext(8, 200, .2f, .2f, color, CG_TranslateString( s ), 0, 0, 3, &cgs.media.limboFont1);
		return;
	}
}
