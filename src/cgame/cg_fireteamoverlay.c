/******************************************************************************
***** teh firetams! (sic)
****/

#include "cg_local.h"

/******************************************************************************
***** Defines, constants, etc
****/

static int sortedFireTeamClients[MAX_CLIENTS];

/******************************************************************************
***** Support Routines
****/

int QDECL CG_SortFireTeam(const void *a, const void *b)
{
	clientInfo_t *ca, *cb;
	int          cna, cnb;
	qboolean     memberA, memberB;

	cna = *(int *)a;
	cnb = *(int *)b;
	if (cna == cnb) return 0;

	ca = &cgs.clientinfo[cna];
	cb = &cgs.clientinfo[cnb];

	// Not on our team, so shove back
	memberA = ca->infoValid && ca->team == cgs.clientinfo[cg.clientNum].team && CG_IsOnSameFireteam(cna, cg.clientNum) != NULL;
	memberB = cb->infoValid && cb->team == cgs.clientinfo[cg.clientNum].team && CG_IsOnSameFireteam(cnb, cg.clientNum) != NULL;
	if (memberA != memberB) return memberA ? -1 : 1;
	if (!memberA) return cna < cnb ? -1 : 1;

	// Leader comes first
	if (CG_IsFireTeamLeader(cna))
	{
		return -1;
	}
	if (CG_IsFireTeamLeader(cnb))
	{
		return 1;
	}

	// Then higher ranks
	if (ca->rank > cb->rank)
	{
		return -1;
	}
	if (cb->rank > ca->rank)
	{
		return 1;
	}

	// Then score
/*	if ( ca->score > cb->score ) {
		return -1;
	}
	if ( cb->score > ca->score ) {
		return 1;
	}*/// not atm

	return cna < cnb ? -1 : 1;
}

// Sorts client's fireteam by leader then rank
void CG_SortClientFireteam()
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		sortedFireTeamClients[i] = i;
	}

	qsort(sortedFireTeamClients, MAX_CLIENTS, sizeof(sortedFireTeamClients[0]), CG_SortFireTeam);

/*	for(i = 0; i < MAX_CLIENTS; i++) {
		CG_Printf( "%i ", sortedFireTeamClients[i] );
	}

	CG_Printf( "\n" );*/
}

// Parses fireteam servercommand
void CG_ParseFireteams()
{
	int        i, j;
	char       *s;
	const char *p;
	int        clnts[2];

	qboolean onFireteam2;
	qboolean isLeader2;
	CG_TCEFireteamInvalidate();

//	qboolean onFireteam =	CG_IsOnFireteam( cg.clientNum ) ? qtrue : qfalse;
//	qboolean isLeader =		CG_IsFireTeamLeader( cg.clientNum ) ? qtrue : qfalse;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		cgs.clientinfo[i].fireteamData = NULL;
	}

	for (i = 0; i < MAX_FIRETEAMS; i++)
	{
		char hexbuffer[11] = "0x00000000";
		p = CG_ConfigString(CS_FIRETEAMS + i);
		memset(&cg.fireTeams[i], 0, sizeof(cg.fireTeams[i]));
		if (!p || !*p || !*Info_ValueForKey(p, "id")) continue;

/*		s = Info_ValueForKey(p, "n");
		if(!s || !*s) {
			cg.fireTeams[i].inuse = qfalse;
			continue;
		} else {
			cg.fireTeams[i].inuse = qtrue;
		}*/

//		Q_strncpyz(cg.fireTeams[i].name, s, 32);
//		CG_Printf("Fireteam: %s\n", cg.fireTeams[i].name);

		j = atoi(Info_ValueForKey(p, "id"));
		if (j < 0 || j >= MAX_FIRETEAMS / 2)
		{
			cg.fireTeams[i].inuse = qfalse;
			continue;
		}
		else
		{
			cg.fireTeams[i].inuse = qtrue;
			cg.fireTeams[i].ident = j;
		}

		s                      = Info_ValueForKey(p, "l");
		cg.fireTeams[i].leader = atoi(s);
		if (!*s || cg.fireTeams[i].leader < 0 || cg.fireTeams[i].leader >= MAX_CLIENTS)
		{
			cg.fireTeams[i].inuse = qfalse;
			continue;
		}

		s = Info_ValueForKey(p, "c");
		if (strlen(s) != 16 || strspn(s, "0123456789abcdefABCDEF") != 16)
		{
			cg.fireTeams[i].inuse = qfalse;
			continue;
		}
		Q_strncpyz(hexbuffer + 2, s, 9);
		sscanf(hexbuffer, "%x", &clnts[1]);
		Q_strncpyz(hexbuffer + 2, s + 8, 9);
		sscanf(hexbuffer, "%x", &clnts[0]);

		for (j = 0; j < MAX_CLIENTS; j++)
		{
			if (COM_BitCheck(clnts, j))
			{
				cg.fireTeams[i].joinOrder[j]   = qtrue;
				cgs.clientinfo[j].fireteamData = &cg.fireTeams[i];
//				CG_Printf("%s\n", cgs.clientinfo[j].name);
			}
			else
			{
				cg.fireTeams[i].joinOrder[j] = qfalse;
			}
		}
	}

	CG_SortClientFireteam();

	onFireteam2 = CG_IsOnFireteam(cg.clientNum) ? qtrue : qfalse;
	isLeader2   = CG_IsFireTeamLeader(cg.clientNum) ? qtrue : qfalse;
}

// Fireteam that both specified clients are on, if they both are on the same team
fireteamData_t * CG_IsOnSameFireteam(int clientNum, int clientNum2)
{
	if (CG_IsOnFireteam(clientNum) == CG_IsOnFireteam(clientNum2))
	{
		return CG_IsOnFireteam(clientNum);
	}

	return NULL;
}

// Fireteam that specified client is leader of, or NULL if none
fireteamData_t * CG_IsFireTeamLeader(int clientNum)
{
	fireteamData_t *f;

	if (!(f = CG_IsOnFireteam(clientNum)))
	{
		return NULL;
	}

	if (f->leader != clientNum)
	{
		return NULL;
	}

	return f ;
}

// Client, not on a fireteam, not sorted, but on your team
clientInfo_t * CG_ClientInfoForPosition(int pos, int max)
{
	int i, cnt = 0;

	for (i = 0; i < MAX_CLIENTS && cnt < max; i++)
	{
		if (cg.clientNum != i && cgs.clientinfo[i].infoValid && !CG_IsOnFireteam(i) && cgs.clientinfo[cg.clientNum].team == cgs.clientinfo[i].team)
		{
			if (cnt == pos)
			{
				return &cgs.clientinfo[i];
			}
			cnt++;
		}
	}

	return NULL;
}

// Fireteam, that's on your same team
fireteamData_t * CG_FireTeamForPosition(int pos, int max)
{
	int i, cnt = 0;

	for (i = 0; i < MAX_FIRETEAMS && cnt < max; i++)
	{
		if (cg.fireTeams[i].inuse && cgs.clientinfo[cg.fireTeams[i].leader].team == cgs.clientinfo[cg.clientNum].team)
		{
			if (cnt == pos)
			{
				return &cg.fireTeams[i];
			}
			cnt++;
		}
	}

	return NULL;
}

// Client, not sorted by rank, on CLIENT'S fireteam
clientInfo_t * CG_FireTeamPlayerForPosition(int pos, int max)
{
	int            i, cnt = 0;
	fireteamData_t *f = CG_IsOnFireteam(cg.clientNum);

	if (!f)
	{
		return NULL;
	}

	for (i = 0; i < MAX_CLIENTS && cnt < max; i++)
	{
		if (cgs.clientinfo[i].infoValid && cgs.clientinfo[cg.clientNum].team == cgs.clientinfo[i].team)
		{
			if (!(f == CG_IsOnFireteam(i)))
			{
				continue;
			}

			if (cnt == pos)
			{
				return &cgs.clientinfo[i];
			}
			cnt++;
		}
	}

	return NULL;
}

// Client, sorted by rank, on CLIENT'S fireteam
clientInfo_t * CG_SortedFireTeamPlayerForPosition(int pos, int max)
{
	int            i, cnt = 0;
	fireteamData_t *f = CG_IsOnFireteam(cg.clientNum);

	if (!f)
	{
		return NULL;
	}

	for (i = 0; i < MAX_CLIENTS && cnt < max; i++)
	{
		clientInfo_t *ci = &cgs.clientinfo[sortedFireTeamClients[i]];
		if (!ci->infoValid || ci->team != cgs.clientinfo[cg.clientNum].team || !(f == CG_IsOnFireteam(sortedFireTeamClients[i])))
		{
			continue;
		}

		if (cnt == pos)
		{
			return &cgs.clientinfo[sortedFireTeamClients[i]];
		}
		cnt++;
	}

	return NULL;
}

/******************************************************************************
***** Main Functions
****/

#define FT_BAR_YSPACING 2.f
#define FT_BAR_HEIGHT 10.f
void CG_DrawFireTeamOverlay(rectDef_t *rect)
{
	CG_DrawTCEFireteamOverlay(rect);
}

qboolean CG_FireteamGetBoxNeedsButtons()
{
	if (cgs.applicationEndTime > cg.time)
	{
		if (cgs.applicationClient < 0)
		{
			return qfalse;
		}
		return qtrue;
	}

	if (cgs.invitationEndTime > cg.time)
	{
		if (cgs.invitationClient < 0)
		{
			return qfalse;
		}
		return qtrue;
	}

	if (cgs.propositionEndTime > cg.time)
	{
		if (cgs.propositionClient < 0)
		{
			return qfalse;
		}
		return qtrue;
	}

	return qfalse;
}

const char * CG_FireteamGetBoxText()
{
	if (cgs.applicationEndTime > cg.time)
	{
		if (cgs.applicationClient == -1)
		{
			return "Sent";
		}

		if (cgs.applicationClient == -2)
		{
			return "Failed";
		}

		if (cgs.applicationClient == -3)
		{
			return "Accepted";
		}

		if (cgs.applicationClient == -4)
		{
			return "Sent";
		}

		if (cgs.applicationClient < 0)
		{
			return NULL;
		}

		return va("Accept application from %s?", cgs.clientinfo[cgs.applicationClient].name);
	}

	if (cgs.invitationEndTime > cg.time)
	{
		if (cgs.invitationClient == -1)
		{
			return "Sent";
		}

		if (cgs.invitationClient == -2)
		{
			return "Failed";
		}

		if (cgs.invitationClient == -3)
		{
			return "Accepted";
		}

		if (cgs.invitationClient == -4)
		{
			return "Sent";
		}

		if (cgs.invitationClient < 0)
		{
			return NULL;
		}

		return va("Accept invitiation from %s?", cgs.clientinfo[cgs.invitationClient].name);
	}

	if (cgs.propositionEndTime > cg.time)
	{
		if (cgs.propositionClient == -1)
		{
			return "Sent";
		}

		if (cgs.propositionClient == -2)
		{
			return "Failed";
		}

		if (cgs.propositionClient == -3)
		{
			return "Accepted";
		}

		if (cgs.propositionClient == -4)
		{
			return "Sent";
		}

		if (cgs.propositionClient < 0)
		{
			return NULL;
		}

		return va("Accept %s's proposition to invite %s to join your fireteam?", cgs.clientinfo[cgs.propositionClient2].name, cgs.clientinfo[cgs.propositionClient].name);
	}

	return NULL;
}

qboolean CG_FireteamHasClass(int classnum, qboolean selectedonly)
{
	fireteamData_t *ft;
	int            i;

	if (!(ft = CG_IsOnFireteam(cg.clientNum)))
	{
		return qfalse;
	}

	for ( i = 0; i < MAX_CLIENTS; i++ )
	{
/*		if( i == cgs.clientinfo ) {
			continue;
		}*/

		if (!cgs.clientinfo[i].infoValid)
		{
			continue;
		}

		if (ft != CG_IsOnFireteam(i))
		{
			continue;
		}

		if (cgs.clientinfo[i].cls != classnum)
		{
			continue;
		}

		if (selectedonly && !cgs.clientinfo[i].selected)
		{
			continue;
		}

		return qtrue;
	}

	return qfalse;
}

const char * CG_BuildSelectedFirteamString(void)
{
	char         buffer[256];
	clientInfo_t *ci;
	int          cnt = 0;
	int          i;

	*buffer = '\0';
	for ( i = 0; i < 6; i++ )
	{
		ci = CG_SortedFireTeamPlayerForPosition(i, 6);
		if (!ci)
		{
			break;
		}

		if (!ci->selected)
		{
			continue;
		}

		cnt++;

		Q_strcat(buffer, sizeof(buffer), va("%i ", ci->clientNum));
	}

	if (cnt == 0)
	{
		return "0";
	}

	if (!cgs.clientinfo[cg.clientNum].selected)
	{
		Q_strcat(buffer, sizeof(buffer), va("%i ", cg.clientNum));
		cnt++;
	}

	return va("%i %s", cnt, buffer);
}
