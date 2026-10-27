/* TCE fireteam presentation. The optional packet does not alter ET tinfo or
 * public engine structures, and never grants information about the enemy. */
#include "cg_local.h"
#include <errno.h>
#include <limits.h>

typedef struct
{
	qboolean valid;
	int time, leader, health, area;
	vec3_t origin;
} tce_fireteamInfo_t;

static tce_fireteamInfo_t fireteamInfo[MAX_CLIENTS];
static vmCvar_t fireteamCapability;

void CG_TCEFireteamInvalidate(void)
{
	memset(fireteamInfo, 0, sizeof(fireteamInfo));
}

void CG_TCEFireteamReset(void)
{
	CG_TCEFireteamInvalidate();
	trap_Cvar_Register(&fireteamCapability, "tceft", "1", CVAR_ROM | CVAR_USERINFO);
}

static qboolean CG_TCEFireteamInteger(const char *text, int *value)
{
	char *end;
	long parsed;
	if (!text || !*text) return qfalse;
	errno = 0;
	parsed = strtol(text, &end, 10);
	if (errno == ERANGE || *end || parsed < INT_MIN || parsed > INT_MAX) return qfalse;
	*value = (int)parsed;
	return qtrue;
}

void CG_ParseTCEFireteamInfo(void)
{
	tce_fireteamInfo_t pending[MAX_CLIENTS];
	qboolean seen[MAX_CLIENTS] = { 0 };
	fireteamData_t *ft;
	int count, leader, i, axis;
	if (cg.clientNum < 0 || cg.clientNum >= MAX_CLIENTS ||
	    !(ft = CG_IsOnFireteam(cg.clientNum)) ||
	    !CG_TCEFireteamInteger(CG_Argv(1), &leader) || leader != ft->leader ||
	    !CG_TCEFireteamInteger(CG_Argv(2), &count) || count < 1 || count > 6 || trap_Argc() != 3 + count * 6)
	{
		return;
	}
	memset(pending, 0, sizeof(pending));
	for (i = 0; i < count; ++i)
	{
		int fields[6], field, id;
		for (field = 0; field < 6; ++field)
			if (!CG_TCEFireteamInteger(CG_Argv(3 + i * 6 + field), &fields[field])) return;
		id = fields[0];
		if (id < 0 || id >= MAX_CLIENTS || seen[id] || !cgs.clientinfo[id].infoValid ||
		    CG_IsOnFireteam(id) != ft || cgs.clientinfo[id].team != cgs.clientinfo[cg.clientNum].team ||
		    fields[1] < -1 || fields[1] > 999 || fields[2] < 0 || fields[2] >= TCE_MAX_LOCATIONS) return;
		for (axis = 0; axis < 3; ++axis)
			if (fields[3 + axis] < -1048576 || fields[3 + axis] > 1048576) return;
		seen[id] = qtrue;
		pending[id].valid = qtrue; pending[id].time = cg.time; pending[id].leader = leader;
		pending[id].health = fields[1]; pending[id].area = fields[2];
		for (axis = 0; axis < 3; ++axis) pending[id].origin[axis] = (float)fields[3 + axis];
	}
	/* Replace only after the complete packet has passed every check. */
	memcpy(fireteamInfo, pending, sizeof(fireteamInfo));
}

static void CG_TCEFireteamFit(char *text, float width, float scale)
{
	int length;
	char *p;
	Q_CleanStr(text);
	/* Literal carets can become new color escapes after cleaning/truncation. */
	for (p = text; *p; ++p) if (*p == Q_COLOR_ESCAPE) *p = ' ';
	length = (int)strlen(text);
	if (CG_Text_Width_Ext(text, scale, 0, &cgs.media.limboFont2) <= width) return;
	while (length > 0)
	{
		text[--length] = '\0';
		if (CG_Text_Width_Ext(text, scale, 0, &cgs.media.limboFont2) +
		    CG_Text_Width_Ext("...", scale, 0, &cgs.media.limboFont2) + 1 <= width) break;
	}
	/* Callers supply buffers larger than the input name/region limit. */
	strcat(text, "...");
}

float CG_TCEFireteamHeight(void)
{
	int count = 0;
	if (!cg.snap || !cg_drawFireteamOverlay.integer || cg.showFireteamMenu || cg_paused.integer ||
	    cg.clientNum < 0 || cg.clientNum >= MAX_CLIENTS || !CG_IsOnFireteam(cg.clientNum) ||
	    cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) return 0.f;
	while (count < 6 && CG_SortedFireTeamPlayerForPosition(count, 6)) ++count;
	return count ? 14.f + count * 14.f : 0.f;
}

void CG_DrawTCEFireteamOverlay(rectDef_t *rect)
{
	vec4_t background = { .035f, .045f, .04f, .82f }, border = { .44f, .49f, .42f, .7f };
	vec4_t heading = { .18f, .22f, .17f, .95f }, muted = { .69f, .74f, .67f, 1.f };
	vec4_t white = { .92f, .94f, .88f, 1.f }, good = { .48f, .8f, .38f, 1.f };
	vec4_t warn = { .96f, .68f, .25f, 1.f }, bad = { .93f, .3f, .25f, 1.f };
	vec4_t selected = { .43f, .35f, .12f, .65f }, own = { .18f, .26f, .23f, .45f };
	vec4_t bar = { .15f, .18f, .14f, 1.f };
	fireteamData_t *ft;
	clientInfo_t *members[6];
	int count = 0, i;
	float x = rect->x, y = rect->y, width = 220.f;
	char text[256];
	if (!cg.snap || !cg_drawFireteamOverlay.integer || cg.clientNum < 0 || cg.clientNum >= MAX_CLIENTS ||
	    !(ft = CG_IsOnFireteam(cg.clientNum)) || cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) return;
	while (count < 6 && (members[count] = CG_SortedFireTeamPlayerForPosition(count, 6)) != NULL) ++count;
	if (!count) return;
	CG_FillRect(x, y, width, 14.f + count * 14.f, background);
	CG_DrawRect(x, y, width, 14.f + count * 14.f, 1.f, border);
	CG_FillRect(x + 1, y + 1, width - 2, 12.f, heading);
	Com_sprintf(text, sizeof(text), "FIRETEAM %s  %i/6", ft->ident >= 0 && ft->ident < MAX_FIRETEAMS / 2 ? bg_fireteamNames[ft->ident] : "", count);
	CG_Text_Paint_Ext(x + 5, y + 10, .15f, .15f, white, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
	for (i = 0; i < count; ++i)
	{
		clientInfo_t *ci = members[i];
		int id = (int)(ci - cgs.clientinfo), health = 0, area = 0;
		qboolean known = qfalse;
		vec2_t position;
		float *color = muted;
		float row = y + 14.f + i * 14.f;
		position[0] = (float)ci->location[0]; position[1] = (float)ci->location[1];
		if (fireteamInfo[id].valid && fireteamInfo[id].leader == ft->leader &&
		    cg.time >= fireteamInfo[id].time && cg.time - fireteamInfo[id].time <= 2500)
		{
			known = qtrue; health = fireteamInfo[id].health; area = fireteamInfo[id].area;
			Vector2Copy(fireteamInfo[id].origin, position);
		}
		if (id == cg.snap->ps.clientNum && !(cg.snap->ps.pm_flags & PMF_FOLLOW))
		{
			known = qtrue; health = cg.snap->ps.pm_flags & PMF_LIMBO ? -1 : cg.snap->ps.stats[STAT_HEALTH];
			if (!(cg.snap->ps.pm_flags & PMF_LIMBO) && health < 0) health = 0;
			area = cg.snap->ps.holdable[13]; Vector2Copy(cg.snap->ps.origin, position);
		}
		if (ci->selected || id == cg.clientNum) CG_FillRect(x + 2, row, width - 4, 13, ci->selected ? selected : own);
		if (known) color = health > 66 ? good : health > 33 ? warn : bad;
		if (id == ft->leader)
			CG_Text_Paint_Ext(x + 4, row + 10, .13f, .13f, warn, "L", 0, 0, 0, &cgs.media.limboFont2);
		Q_strncpyz(text, ci->name, sizeof(text)); CG_TCEFireteamFit(text, 83.f, .15f);
		CG_Text_Paint_Ext(x + 13, row + 10, .15f, .15f, white, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
		Com_sprintf(text, sizeof(text), !known ? "..." : health == -1 ? "WAIT" : health <= 0 ? "DEAD" : "%i", health);
		CG_Text_Paint_Ext(x + 150 - CG_Text_Width_Ext(text, .14f, 0, &cgs.media.limboFont2), row + 10,
		                  .14f, .14f, color, text, 0, 0, 0, &cgs.media.limboFont2);
		if (known && health > 0)
		{
			CG_FillRect(x + 100, row + 6, 25, 3, bar);
			CG_FillRect(x + 100, row + 6, 25 * (health > 100 ? 100 : health) / 100.f, 3, color);
		}
		if (!known) Q_strncpyz(text, "...", sizeof(text));
		else if (health <= 0) text[0] = '\0';
		else
		{
			const char *region = area > 0 && area < TCE_MAX_LOCATIONS ? CG_ConfigString(CS_LOCATIONS + area) : "";
			Q_strncpyz(text, region && *region ? region : BG_GetLocationString(position), sizeof(text));
		}
		CG_TCEFireteamFit(text, 61.f, .13f);
		CG_Text_Paint_Ext(x + 155, row + 10, .13f, .13f, muted, text, 0, 0, 0, &cgs.media.limboFont2);
	}
}
