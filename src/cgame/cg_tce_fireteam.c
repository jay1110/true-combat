/* TCE fireteam presentation. The optional packet does not alter ET tinfo or
 * public engine structures, and never grants information about the enemy. */
#include "cg_local.h"
#include "../game/tce_bg.h"
#include <errno.h>
#include <limits.h>

typedef struct
{
	qboolean valid;
	int time, leader, health, area, objective, weapon, playerClass;
	vec3_t origin;
} tce_fireteamInfo_t;

static tce_fireteamInfo_t fireteamInfo[MAX_CLIENTS];
static vmCvar_t fireteamCapability, fireteamGearCapability, fireteamDrawIcons;
static qhandle_t fireteamObjectiveShaders[5], fireteamClassShaders[3];

void CG_TCEFireteamInvalidate(void)
{
	memset(fireteamInfo, 0, sizeof(fireteamInfo));
}

void CG_TCEFireteamReset(void)
{
	CG_TCEFireteamInvalidate();
	memset(fireteamObjectiveShaders, 0, sizeof(fireteamObjectiveShaders));
	memset(fireteamClassShaders, 0, sizeof(fireteamClassShaders));
	trap_Cvar_Register(&fireteamCapability, "tceft", "1", CVAR_ROM | CVAR_USERINFO);
	trap_Cvar_Register(&fireteamGearCapability, "tceftgear", "1", CVAR_ROM | CVAR_USERINFO);
	trap_Cvar_Register(&fireteamDrawIcons, "cg_drawIcons", "1", CVAR_ARCHIVE);
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
	int count, leader, i, axis, fieldsPerMember;
	if (cg.clientNum < 0 || cg.clientNum >= MAX_CLIENTS ||
	    !(ft = CG_IsOnFireteam(cg.clientNum)) ||
	    !CG_TCEFireteamInteger(CG_Argv(1), &leader) || leader != ft->leader ||
	    !CG_TCEFireteamInteger(CG_Argv(2), &count) || count < 1 || count > 6)
	{
		return;
	}
	/* Accept both previous private packet layouts. Missing gear is explicitly
	 * unknown, never inferred from a stale PVS entity or scoreboard cache. */
	fieldsPerMember = trap_Argc() == 3 + count * 9 ? 9 : trap_Argc() == 3 + count * 7 ? 7 : 6;
	if (trap_Argc() != 3 + count * fieldsPerMember) return;
	memset(pending, 0, sizeof(pending));
	for (i = 0; i < count; ++i)
	{
		int fields[9] = { 0, 0, 0, 0, 0, 0, 0, -1, -1 }, field, id;
		for (field = 0; field < fieldsPerMember; ++field)
			if (!CG_TCEFireteamInteger(CG_Argv(3 + i * fieldsPerMember + field), &fields[field])) return;
		id = fields[0];
		if (id < 0 || id >= MAX_CLIENTS || seen[id] || !cgs.clientinfo[id].infoValid ||
		    CG_IsOnFireteam(id) != ft || cgs.clientinfo[id].team != cgs.clientinfo[cg.clientNum].team ||
		    fields[1] < -1 || fields[1] > 999 || fields[2] < 0 || fields[2] >= TCE_MAX_LOCATIONS ||
		    fields[6] < 0 || fields[6] > 4 || (fields[1] <= 0 && fields[6]) ||
		    fields[7] < -1 || fields[7] >= TCE_MAX_WEAPONS ||
		    fields[8] < -1 || fields[8] >= NUM_PLAYER_CLASSES) return;
		for (axis = 0; axis < 3; ++axis)
			if (fields[3 + axis] < -1048576 || fields[3 + axis] > 1048576) return;
		seen[id] = qtrue;
		pending[id].valid = qtrue; pending[id].time = cg.time; pending[id].leader = leader;
		pending[id].health = fields[1]; pending[id].area = fields[2];
		pending[id].objective = fields[6];
		pending[id].weapon = fields[7]; pending[id].playerClass = fields[8];
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

static void CG_TCEFireteamName(char *text)
{
	char *p;
	Q_CleanStr(text);
	for (p = text; *p; ++p) if (*p == Q_COLOR_ESCAPE) *p = ' ';
	if (strlen(text) > 10) { text[10] = '\0'; strcat(text, "..."); }
}

static qhandle_t CG_TCEFireteamObjectiveIcon(int objective)
{
	static const char *paths[5] = { "", "sprites/bombcarrier.tga", "sprites/VIP.tga",
	                              "gfx/tce2/nav/flag_terror.tga", "gfx/tce2/nav/flag_specops.tga" };
	if (objective < 1 || objective > 4) return 0;
	if (!fireteamObjectiveShaders[objective])
		fireteamObjectiveShaders[objective] = trap_R_RegisterShaderNoMip(paths[objective]);
	return fireteamObjectiveShaders[objective];
}

/* The native class table still references ET medic/engineer artwork. Use
 * explicit TC roles; no false ET class symbol or cached scoreboard class. */
static qhandle_t CG_TCEFireteamClassIcon(int playerClass)
{
	static const char *paths[3] = { "gfx/tce2/fireteam/assault.tga",
	                              "gfx/tce2/fireteam/recon.tga", "gfx/tce2/fireteam/sniper.tga" };
	int role;
	if (playerClass < 0 || playerClass >= NUM_PLAYER_CLASSES) return 0;
	if (playerClass == PC_VIP) return CG_TCEFireteamObjectiveIcon(2);
	role = BG_WolfClassToTCE(playerClass);
	if (!fireteamClassShaders[role]) fireteamClassShaders[role] = trap_R_RegisterShaderNoMip(paths[role]);
	return fireteamClassShaders[role];
}

/* Exact current gear, including custom map gear. TC's .weap icon fields are
 * inherited ET placeholders. Match TCE_CG_Item's pickup -> third-person model
 * choice: stock files omit pickupModel and use their weapon-only TP mesh.
 * Never use the first-person mesh containing arms. */
static void CG_TCEFireteamWeaponIcon(float x, float y, int weapon)
{
	refdef_t view;
	refEntity_t entity;
	weaponInfo_t *media;
	weaponModel_t *model;
	vec3_t mins, maxs;
	float w = 34.f, h = 12.f, tangent, distance, spanX, spanY, spanZ;
	int axis;
	if (!fireteamDrawIcons.integer || weapon <= 0 || weapon >= TCE_MAX_WEAPONS || !gearDef.weaponFile[weapon][0]) return;
	CG_RegisterWeapon(weapon, qfalse); media = &cg_weapons[weapon];
	model = &media->weaponModel[W_PU_MODEL];
	if (!model->model) model = &media->weaponModel[W_TP_MODEL];
	if (!model->model) return;
	trap_R_ModelBounds(model->model, mins, maxs);
	for (axis = 0; axis < 3; ++axis)
		if (!(mins[axis] >= -4096.f && maxs[axis] <= 4096.f && maxs[axis] >= mins[axis])) return;
	spanX = maxs[0] - mins[0]; spanY = maxs[1] - mins[1]; spanZ = maxs[2] - mins[2];
	if (!(spanX > 0.f || spanZ > 0.f)) return;
	CG_AdjustFrom640(&x, &y, &w, &h);
	if (!(w >= 1.f && h >= 1.f && w <= 16384.f && h <= 16384.f)) return;
	memset(&view, 0, sizeof(view)); memset(&entity, 0, sizeof(entity));
	view.x = (int)x; view.y = (int)y; view.width = (int)w; view.height = (int)h;
	view.fov_y = 30.f; tangent = (float)tan(M_PI / 12.0);
	view.fov_x = (float)(atan(tangent * view.width / view.height) * 360.0 / M_PI);
	distance = spanX * .5f / (tangent * view.width / view.height);
	if (spanZ * .5f / tangent > distance) distance = spanZ * .5f / tangent;
	distance = distance * 1.12f + spanY * .5f;
	if (distance < 1.f) distance = 1.f;
	AxisClear(view.viewaxis); view.rdflags = RDF_NOWORLDMODEL; view.time = cg.time;
	/* Rotate the barrel into the horizontal image axis, centered on bounds. */
	entity.axis[0][1] = 1.f; entity.axis[1][0] = -1.f; entity.axis[2][2] = 1.f;
	VectorSet(entity.origin, distance + (mins[1] + maxs[1]) * .5f,
	          -(mins[0] + maxs[0]) * .5f, -(mins[2] + maxs[2]) * .5f);
	entity.hModel = model->model;
	entity.customSkin = model->skin[0];
	entity.renderfx = RF_NOSHADOW | RF_MINLIGHT | RF_HILIGHT;
	trap_R_ClearScene(); trap_R_AddRefEntityToScene(&entity); trap_R_RenderScene(&view);
}

float CG_TCEFireteamHeight(void)
{
	int count = 0;
	if (!cg.snap || !cg_drawFireteamOverlay.integer || cg.showFireteamMenu || cg_paused.integer ||
	    cg.clientNum < 0 || cg.clientNum >= MAX_CLIENTS || !CG_IsOnFireteam(cg.clientNum) ||
	    cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) return 0.f;
	while (count < 6 && CG_SortedFireTeamPlayerForPosition(count, 6)) ++count;
	return count ? 18.f + count * 14.f : 0.f;
}

void CG_DrawTCEFireteamOverlay(rectDef_t *rect)
{
	vec4_t background = { .035f, .045f, .04f, .82f }, border = { .44f, .49f, .42f, .7f };
	vec4_t muted = { .69f, .74f, .67f, 1.f };
	vec4_t white = { .92f, .94f, .88f, 1.f }, good = { .48f, .8f, .38f, 1.f };
	vec4_t warn = { .96f, .68f, .25f, 1.f }, bad = { .93f, .3f, .25f, 1.f };
	vec4_t selected = { .43f, .35f, .12f, .65f }, own = { .18f, .26f, .23f, .45f };
	vec4_t bar = { .15f, .18f, .14f, 1.f };
	fireteamData_t *ft;
	clientInfo_t *members[6];
	int count = 0, i;
	float x = rect->x, y = rect->y, width, nameWidth = 0.f, barX, classX, weaponX, locationX;
	char text[256];
	if (!cg.snap || !cg_drawFireteamOverlay.integer || cg.clientNum < 0 || cg.clientNum >= MAX_CLIENTS ||
	    !(ft = CG_IsOnFireteam(cg.clientNum)) || cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) return;
	while (count < 6 && (members[count] = CG_SortedFireTeamPlayerForPosition(count, 6)) != NULL) ++count;
	if (!count) return;
	trap_Cvar_Update(&fireteamDrawIcons);
	/* Shared measured columns stay aligned without the old fixed name gap. */
	for (i = 0; i < count; ++i)
	{
		float measured;
		Q_strncpyz(text, members[i]->name, sizeof(text)); CG_TCEFireteamName(text);
		measured = (float)CG_Text_Width_Ext(text, .14f, 0, &cgs.media.limboFont2);
		if (measured > nameWidth) nameWidth = measured;
	}
	barX = 24.f + nameWidth + 5.f; classX = barX + 30.f;
	weaponX = classX + 13.f; locationX = weaponX + 39.f; width = locationX + 84.f;
	CG_FillRect(x, y, width, 18.f + count * 14.f, background);
	CG_DrawRect(x, y, width, 18.f + count * 14.f, 1.f, border);
	Com_sprintf(text, sizeof(text), "Fireteam %s", ft->ident >= 0 && ft->ident < MAX_FIRETEAMS / 2 ?
	            bg_fireteamNames[ft->ident] : "?");
	CG_Text_Paint_Ext(x + 4, y + 11, .14f, .14f, muted, text, 0, 0, 0, &cgs.media.limboFont2);
	for (i = 0; i < count; ++i)
	{
		clientInfo_t *ci = members[i];
		int id = (int)(ci - cgs.clientinfo), health = 0, area = 0, objective = 0, weapon = -1, playerClass = -1;
		qboolean known = qfalse;
		vec2_t position;
		float *color = muted;
		float row = y + 16.f + i * 14.f;
		position[0] = (float)ci->location[0]; position[1] = (float)ci->location[1];
		if (fireteamInfo[id].valid && fireteamInfo[id].leader == ft->leader &&
		    cg.time >= fireteamInfo[id].time && cg.time - fireteamInfo[id].time <= 2500)
		{
			known = qtrue; health = fireteamInfo[id].health; area = fireteamInfo[id].area;
			objective = fireteamInfo[id].objective;
			weapon = fireteamInfo[id].weapon; playerClass = fireteamInfo[id].playerClass;
			Vector2Copy(fireteamInfo[id].origin, position);
		}
		if (id == cg.snap->ps.clientNum && !(cg.snap->ps.pm_flags & PMF_FOLLOW))
		{
			known = qtrue; health = cg.snap->ps.pm_flags & PMF_LIMBO ? -1 : cg.snap->ps.stats[STAT_HEALTH];
			if (!(cg.snap->ps.pm_flags & PMF_LIMBO) && health < 0) health = 0;
			area = cg.snap->ps.holdable[13]; Vector2Copy(cg.snap->ps.origin, position);
			weapon = cg.snap->ps.weapon; playerClass = cg.snap->ps.stats[STAT_PLAYER_CLASS];
			objective = health <= 0 ? 0 : (cg.snap->ps.stats[STAT_TCE_WEAPON_FLAGS] & 0x100) ? 1 :
			            (cg.snap->ps.stats[STAT_TCE_FLAGS] & 0x100) ? 2 :
			            cg.snap->ps.powerups[PW_REDFLAG] ? 3 : cg.snap->ps.powerups[PW_BLUEFLAG] ? 4 : 0;
		}
		if (ci->selected || id == cg.clientNum) CG_FillRect(x + 2, row, width - 4, 13, ci->selected ? selected : own);
		if (known) color = health > 66 ? good : health > 33 ? warn : bad;
		if (id == ft->leader)
			CG_Text_Paint_Ext(x + 4, row + 10, .13f, .13f, warn, "L", 0, 0, 0, &cgs.media.limboFont2);
		if (known && health > 0 && objective)
			CG_DrawPic(x + 11, row + 1, 11, 11, CG_TCEFireteamObjectiveIcon(objective));
		Q_strncpyz(text, ci->name, sizeof(text)); CG_TCEFireteamName(text);
		CG_Text_Paint_Ext(x + 24, row + 10, .14f, .14f, white, text, 0, 0, 0, &cgs.media.limboFont2);
		CG_FillRect(x + barX, row + 5, 25, 4, bar);
		if (known && health > 0)
		{
			CG_FillRect(x + barX, row + 5, 25 * (health > 100 ? 100 : health) / 100.f, 4, color);
		}
		if (known && health > 0 && fireteamDrawIcons.integer)
		{
			qhandle_t classIcon = CG_TCEFireteamClassIcon(playerClass);
			if (classIcon) CG_DrawPic(x + classX, row + 1, 11, 11, classIcon);
			CG_TCEFireteamWeaponIcon(x + weaponX, row + 1, weapon);
		}
		if (!known) Q_strncpyz(text, "...", sizeof(text));
		else if (health <= 0) Q_strncpyz(text, health == -1 ? "WAIT" : "DEAD", sizeof(text));
		else
		{
			const char *region = area > 0 && area < TCE_MAX_LOCATIONS ? CG_ConfigString(CS_LOCATIONS + area) : "";
			Q_strncpyz(text, region && *region ? region : BG_GetLocationString(position), sizeof(text));
		}
		CG_TCEFireteamFit(text, 78.f, .13f);
		CG_Text_Paint_Ext(x + locationX, row + 10, .13f, .13f, muted, text, 0, 0, 0, &cgs.media.limboFont2);
	}
}
