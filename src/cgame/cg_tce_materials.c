/* Optional native ET material text, installed before BSP shader registration.
 * These files are deliberately outside scripts/: disabling this feature on a
 * fresh renderer leaves every original material untouched. No runtime remaps. */
#include "cg_local.h"
#include "cg_tce_materials.h"

#define TCE_MATERIAL_MAX_BYTES 8192

static vmCvar_t cg_materialEffects;

/* Pinned to assets/textures/tce2/details/SOURCE.json. Never accept a material
 * name or VFS path from a server command, map entity, or arbitrary manifest. */
static const struct {
	const char *path, *name;
} tceMaterials[] = {
	{ "materials/tce2/world/asphalt_e1.shader", "textures/coroner/asphalt_e1" },
	{ "materials/tce2/world/cement_s1.shader", "textures/coroner/cement_s1" },
	{ "materials/tce2/world/brick_dirt_7.shader", "textures/twy_brickstiles/brick_dirt_7" },
	{ "materials/tce2/world/concrete_blue.shader", "textures/bruce_trim/concrete_blue" },
	{ "materials/tce2/world/wood_hout4.shader", "textures/liq/hout4" }
};

static qboolean CG_TCEMaterialMap(const char *mapName)
{
	static const char *maps[] = {
		"maps/obj_delta.bsp", "maps/obj_northport.bsp", "maps/obj_railhouse.bsp",
		"maps/obj_snow.bsp", "maps/obj_stadtrand.bsp", "maps/obj_village.bsp"
	};
	int i;
	if (!mapName) return qfalse;
	for (i = 0; i < (int)(sizeof(maps) / sizeof(maps[0])); ++i)
		if (!Q_stricmp(mapName, maps[i])) return qtrue;
	return qfalse;
}

/* A small bounded lexer for the container, not a second renderer material
 * compiler. Braces in comments/quoted tokens are not structural. Like ET's
 * COM_ParseExt, quoted tokens end at the next quote; backslash is literal. */
static int CG_TCEMaterialToken(const char **cursor, const char *end,
	                          char *token, int tokenSize)
{
	const char *p = *cursor, *start;
	int length, kind = 1;
	for (;;)
	{
		while (p < end && (unsigned char)*p <= ' ') ++p;
		if (p == end) { *cursor = p; return 0; }
		if (end - p >= 2 && p[0] == '/' && p[1] == '/')
		{
			p += 2;
			while (p < end && *p != '\n') ++p;
			continue;
		}
		if (end - p >= 2 && p[0] == '/' && p[1] == '*')
		{
			p += 2;
			while (end - p >= 2 && !(p[0] == '*' && p[1] == '/')) ++p;
			if (end - p < 2) return -1;
			p += 2;
			continue;
		}
		break;
	}
	start = p;
	if (*p == '{' || *p == '}') { kind = *p == '{' ? 2 : 3; ++p; }
	else if (*p == '"')
	{
		start = ++p;
		while (p < end && *p != '"') ++p;
		if (p == end) return -1;
		length = (int)(p - start);
		++p;
		if (token)
		{
			if (length >= tokenSize) return -1;
			memcpy(token, start, length); token[length] = '\0';
		}
		*cursor = p;
		return kind;
	}
	else
	{
		while (p < end && (unsigned char)*p > ' ')
		{
			/* Native material syntax separates braces and quoted tokens. */
			if (*p == '{' || *p == '}' || *p == '"') return -1;
			++p;
		}
	}
	length = (int)(p - start);
	if (token)
	{
		if (length >= tokenSize) return -1;
		memcpy(token, start, length); token[length] = '\0';
	}
	*cursor = p;
	return kind;
}

static qboolean CG_TCEMaterialText(const char *text, int length, const char *name)
{
	const char *p = text, *end;
	char token[MAX_QPATH];
	int kind, depth = 1, stages = 0;
	if (!text || !name || length <= 0 || length > TCE_MATERIAL_MAX_BYTES || memchr(text, '\0', length)) return qfalse;
	end = text + length;
	if (CG_TCEMaterialToken(&p, end, token, sizeof(token)) != 1 || strcmp(token, name)) return qfalse;
	if (CG_TCEMaterialToken(&p, end, NULL, 0) != 2) return qfalse;
	while (depth > 0)
	{
		kind = CG_TCEMaterialToken(&p, end, NULL, 0);
		if (kind <= 0) return qfalse;
		if (kind == 2)
		{
			if (++depth > 8) return qfalse;
			if (depth == 2) ++stages;
		}
		else if (kind == 3) --depth;
	}
	return stages > 0 && CG_TCEMaterialToken(&p, end, NULL, 0) == 0;
}

void CG_TCEInstallMaterials(const char *mapName)
{
	char text[TCE_MATERIAL_MAX_BYTES + 1];
	int i, length, added = 0, unchanged = 0, skipped = 0;
	fileHandle_t file;
	trap_Cvar_Register(&cg_materialEffects, "cg_materialEffects", "1", CVAR_ARCHIVE);
	if (!cg_materialEffects.integer || !CG_TCEMaterialMap(mapName)) return;
	for (i = 0; i < (int)(sizeof(tceMaterials) / sizeof(tceMaterials[0])); ++i)
	{
		file = 0;
		length = trap_FS_FOpenFile(tceMaterials[i].path, &file, FS_READ);
		if (!file || length <= 0 || length > TCE_MATERIAL_MAX_BYTES)
		{
			if (file) trap_FS_FCloseFile(file);
			++skipped;
			continue;
		}
		trap_FS_Read(text, length, file);
		trap_FS_FCloseFile(file);
		text[length] = '\0';
		if (!CG_TCEMaterialText(text, length, tceMaterials[i].name))
		{
			++skipped;
			continue;
		}
		/* Never delete a dynamic shader or purge another subsystem's list.
		 * On CGame-only reinit the renderer may already own this definition:
		 * false means it was not added, not that the existing material vanished. */
		if (trap_R_LoadDynamicShader(tceMaterials[i].name, text)) ++added;
		else ++unchanged;
	}
	CG_Printf("TCE materials: %d added, %d unchanged, %d files skipped. Changes require vid_restart.\n",
	          added, unchanged, skipped);
}
