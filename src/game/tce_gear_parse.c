/* Recovered from Windows cgame:30007b90, corroborated by Linux BG_ParseGearDef. */
#include "q_shared.h"
#include "bg_public.h"
#include "tce_bg.h"
#include "tce_float_integer.h"

/* Preserve the numeric result of the original masked x87 conversion.
 * Floating-point exception flags and traps are not reproduced here. */
static int TCE_GearSkillToInt(float value)
{
#if defined(_MSC_VER) && defined(_M_IX86)
	return TCE_FloatToInt64Low(value);
#elif defined(__linux__) && defined(__i386__)
	if (!(value >= -2147483648.0f && value < 2147483648.0f))
	{
		return -2147483647 - 1;
	}
	return (int)value;
#else
	return (int)value;
#endif
}

int BG_ParseGearDef(const char *primary, const char *fallback, tce_gearDef_t *def)
{
	int        handle, weapon = 0, i;
	pc_token_t token;
	char       value[64];
	const char *setting;
	vec3_t     skill;
	handle = trap_PC_LoadSource(primary);
	if (!handle)
	{
		handle = trap_PC_LoadSource(fallback);
	}
	if (!handle)
	{
		handle = trap_PC_LoadSource("maps/default.gear");
	}
	if (!handle)
	{
		return 0;
	}
	if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "gear_manager"))
	{
		return TCE_PWF_ParseError(handle, "expected 'gear_manager'");
	}
	if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{"))
	{
		return TCE_PWF_ParseError(handle, "expected '{'");
	}
	while (trap_PC_ReadToken(handle, &token) && token.string[0] != '}')
	{
#define GROUP(key, field, error) \
		if (!Q_stricmp(token.string, key)) { \
			if (!PC_String_ParseNoAlloc(handle, def->field, 64)) \
			return TCE_PWF_ParseError(handle, error); \
			continue; \
		}
		GROUP("gm_playerSkinGroup", playerSkinGroup, "expected playerSkinGroup foldername")
		GROUP("gm_playerVoiceGroup", playerVoiceGroup, "expected playerVoiceGroup foldername")
		GROUP("gm_playerIconGroup", playerIconGroup, "expected playerIconGroup foldername")
		GROUP("gm_custom_weaponGroup", customWeaponGroup, "expected gm_custom_weaponGroup foldername")
#undef GROUP
		if (!Q_stricmp(token.string, "gm_mapoverbrightbits"))
		{
			if (!PC_Int_Parse(handle, &def->mapOverbrightBits))
			{
				return TCE_PWF_ParseError(handle, "expected gm_mapoverbrightbits value");
			}
		}
		else if (!Q_stricmp(token.string, "gm_weapon"))
		{
			if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{"))
			{
				return TCE_PWF_ParseError(handle, "expected '{'");
			}
			/* The original deliberately retains the previous block's weapon index. */
			while (trap_PC_ReadToken(handle, &token) && token.string[0] != '}')
			{
				if (!Q_stricmp(token.string, "weaponID"))
				{
					if (!PC_String_ParseNoAlloc(handle, value, 64))
					{
						return TCE_PWF_ParseError(handle, "expected weaponID");
					}
					weapon = BG_WeapIDToWeaponNum(value);
				}
				else if (!Q_stricmp(token.string, "weaponFile"))
				{
					if (!PC_String_ParseNoAlloc(handle, def->weaponFile[weapon], 64))
					{
						return TCE_PWF_ParseError(handle, "expected weaponFile filename");
					}
				}
				else if (!Q_stricmp(token.string, "team"))
				{
					if (!PC_String_ParseNoAlloc(handle, value, 64))
					{
						return TCE_PWF_ParseError(handle, "expected team name");
					}
					setting           = va("%s", value);
					def->team[weapon] = !Q_stricmp(setting, "terrorists") ? 0 :
					                    !Q_stricmp(setting, "specops") ? 1 : 2;
				}
				else if (!Q_stricmp(token.string, "slot"))
				{
					if (!PC_String_ParseNoAlloc(handle, value, 64))
					{
						return TCE_PWF_ParseError(handle, "expected slot name");
					}
					setting           = va("%s", value);
					def->slot[weapon] = !Q_stricmp(setting, "secondary") ? 2 : 1;
				}
				else if (!Q_stricmp(token.string, "requiredSkill"))
				{
					if (!PC_Vec_Parse(handle, &skill))
					{
						return TCE_PWF_ParseError(handle, "expected 3 requiredSkill values");
					}
					for (i = 0; i < 3; ++i)
						def->requiredSkill[weapon][i] = TCE_GearSkillToInt(skill[i]);
				}
				else if (!Q_stricmp(token.string, "startClips"))
				{
					if (!PC_Int_Parse(handle, &def->startClips[weapon]))
					{
						return TCE_PWF_ParseError(handle, "expected startClips value");
					}
				}
				else if (!Q_stricmp(token.string, "equivalentWeaponID"))
				{
					if (!PC_String_ParseNoAlloc(handle, value, 64))
					{
						return TCE_PWF_ParseError(handle, "expected equivalentWeaponID");
					}
					def->equivalentWeapon[weapon] = BG_WeapIDToWeaponNum(value);
				}
				/* Unknown tokens inside gm_weapon are ignored by the original. */
			}
		}
		else
		{
			return TCE_PWF_ParseError(handle, "unknown token '%s'", token.string);
		}
	}
	def->parsed = 1;
	trap_PC_FreeSource(handle);
	return 1;
}
