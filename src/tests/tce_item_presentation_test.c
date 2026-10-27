#include "../cgame/cg_local.h"

cg_t cg; cgs_t cgs; weaponInfo_t cg_weapons[MAX_WEAPONS]; itemInfo_t cg_items[MAX_ITEMS];
gitem_t bg_itemlist[128]; int bg_numItems = 128; vmCvar_t cg_drawCrosshairPickups;
static snapshot_t snap;
static int draws, bounds, lastModel;
void TCE_CG_Item(centity_t *cent);
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "item presentation failed line%d: %s\n", __LINE__, #c); return 1; } } while (0)
void QDECL CG_Error(const char *format, ...) { (void)format; exit(2); }
char *QDECL va(const char *format, ...) { static char text[128]; va_list args; va_start(args, format); vsprintf(text, format, args); va_end(args); return text; }
void AnglesToAxis(const vec3_t angles, vec3_t axis[3]) { int i; (void)angles; memset(axis, 0, sizeof(vec3_t) * 3); for (i = 0; i < 3; ++i) { axis[i][i] = 1; } }
void AxisCopy(vec3_t in[3], vec3_t out[3]) { memcpy(out, in, sizeof(vec3_t) * 3); }
qboolean CG_PlayerSeesItem(playerState_t *ps, entityState_t *es, int time, int type) { (void)ps; (void)es; (void)time; (void)type; return qtrue; }
void trap_R_ModelBounds(qhandle_t model, vec3_t mins, vec3_t maxs) { ++bounds; lastModel = model; memset(mins, 0, sizeof(vec3_t)); memset(maxs, 0, sizeof(vec3_t)); }
void trap_R_AddRefEntityToScene(const refEntity_t *ent) { ++draws; lastModel = ent->hModel; }
void trap_R_AddCoronaToScene(const vec3_t origin, float r, float g, float b, float scale, int id, qboolean visible) { (void)origin; (void)r; (void)g; (void)b; (void)scale; (void)id; (void)visible; }
void CG_PositionEntityOnTag(refEntity_t *ent, const refEntity_t *parent, const char *tag, int start, vec3_t *offset) { (void)ent; (void)parent; (void)tag; (void)start; (void)offset; }

int main(void)
{
	centity_t cent; int team, client;
	cg.snap = &snap; cg_items[77].models[0] = 777;
	/* The item visibility field contains a team, never a client number. */
	for (team = TEAM_AXIS; team <= TEAM_ALLIES; ++team)
	{
		for (client = 0; client < MAX_CLIENTS; ++client)
		{
			memset(&cent, 0, sizeof(cent)); cent.currentState.modelindex = 77;
			bg_itemlist[77].giType = IT_AMMO; cent.currentState.otherEntityNum2 = team;
			snap.ps.clientNum = client; snap.ps.persistant[PERS_TEAM] = team;
			draws = bounds = 0; TCE_CG_Item(&cent); CHECK(draws == 1 && bounds == 1 && lastModel == 777);
			snap.ps.persistant[PERS_TEAM] = team == TEAM_AXIS ? TEAM_ALLIES : TEAM_AXIS;
			draws = bounds = 0; TCE_CG_Item(&cent); CHECK(!draws && !bounds);
			bg_itemlist[77].giType = IT_HEALTH; draws = bounds = 0;
			TCE_CG_Item(&cent); CHECK(draws == 1);
			snap.ps.persistant[PERS_TEAM] = team; draws = 0;
			TCE_CG_Item(&cent); CHECK(!draws);
		}
	}
	/* Custom objective models survive the server-to-renderer path. */
	memset(&cent, 0, sizeof(cent)); cent.currentState.modelindex = 91; cent.currentState.modelindex2 = 3;
	bg_itemlist[91].giType = IT_TEAM; cgs.gameModels[3] = 333;
	draws = bounds = 0; TCE_CG_Item(&cent); CHECK(draws == 1 && bounds == 1 && lastModel == 333);
	cgs.gameModels[3] = 0; draws = bounds = 0; TCE_CG_Item(&cent); CHECK(!draws && !bounds);
	puts("PASS item presentation: both teams/all64 client IDs, bomb visibility, health visibility, custom/missing objective models");
	return 0;
}
