/*
**  Character loading
*/

#include "g_local.h"
#include "tce_numeric.h"
#include "g_tce_pose.h"

static char text[100000];           // <- was causing callstacks >64k

/*
=====================
G_ParseAnimationFiles

  Read in all the configuration and script files for this model.
=====================
*/
static qboolean G_ParseAnimationFiles(bg_character_t *character, const char *animationGroup, const char *animationScript)
{
	fileHandle_t f;
	int          len;

	// set the name of the animationGroup and animationScript in the animModelInfo structure
	Q_strncpyz(character->animModelInfo->animationGroup, animationGroup, sizeof(character->animModelInfo->animationGroup));
	Q_strncpyz(character->animModelInfo->animationScript, animationScript, sizeof(character->animModelInfo->animationScript));

	BG_R_RegisterAnimationGroup(animationGroup, character->animModelInfo);

	// load the script file
	len = trap_FS_FOpenFile(animationScript, &f, FS_READ);
	if (len <= 0)
	{
		return qfalse;
	}
	if (len >= sizeof(text) - 1)
	{
		/* Original printed an uninitialized filename buffer here. Preserve the
		 * diagnostic contract with the actual requested script, not stack data. */
		G_Printf("File %s is too long\n", animationScript);
		return qfalse;
	}
	trap_FS_Read(text, len, f);
	text[len] = 0;
	trap_FS_FCloseFile(f);

	// parse the text
	BG_AnimParseAnimScript(character->animModelInfo, &level.animScriptData, animationScript, text);

	return qtrue;
}

/*
==================
G_CheckForExistingAnimModelInfo

  If this player model has already been parsed, then use the existing information.
  Otherwise, set the modelInfo pointer to the first free slot.

  returns qtrue if existing model found, qfalse otherwise
==================
*/
static qboolean G_CheckForExistingAnimModelInfo(const char *animationGroup, const char *animationScript, animModelInfo_t **animModelInfo)
{
	int             i;
	animModelInfo_t *trav, *firstFree = NULL;

	for ( i = 0, trav = level.animScriptData.modelInfo; i < MAX_ANIMSCRIPT_MODELS; i++, trav++ )
	{
		if (*trav->animationGroup && *trav->animationScript)
		{
			if (!Q_stricmp(trav->animationGroup, animationGroup) && !Q_stricmp(trav->animationScript, animationScript))
			{
				// found a match, use this animModelInfo
				*animModelInfo = trav;
				return qtrue;
			}
		}
		else if (!firstFree)
		{
			firstFree = trav;
		}
	}

	if (!firstFree)
	{
		G_Error("unable to find a free modelinfo slot, cannot continue\n");
	}
	else
	{
		*animModelInfo = firstFree;
		/* The original i386 code clears only the first four bytes. */
		memset(*animModelInfo, 0, 4);
	}

	// qfalse signifies that we need to parse the information from the script files
	return qfalse;
}

/*
===================
G_RegisterCharacter
===================
*/
qboolean G_RegisterCharacter(const char *characterFile, bg_character_t *character)
{
	bg_characterDef_t characterDef;

	memset(&characterDef, 0, sizeof(characterDef));

	if (!BG_ParseCharacterFile(characterFile, &characterDef))
	{
		return qfalse;  // the parser will provide the error message
	}

	// Parse Animation Files
	if (!G_CheckForExistingAnimModelInfo(characterDef.animationGroup, characterDef.animationScript, &character->animModelInfo))
	{
		if (!G_ParseAnimationFiles(character, characterDef.animationGroup, characterDef.animationScript))
		{
			G_Printf(S_COLOR_YELLOW "WARNING: failed to load animation files referenced from '%s'\n", characterFile);
			return qfalse;
		}
	}

	/* Missing/unsupported pose assets retain the existing gameplay fallback. */
	if (!G_TCEPoseRegisterCharacter(character, &characterDef))
	{
		G_Printf("Pose cache: unavailable for '%s'; existing damage volumes retained.\n", characterFile);
	}
	return qtrue;
}

/*
=======================
G_RegisterPlayerClasses
=======================
*/
void G_RegisterPlayerClasses(void)
{
	bg_playerclass_t *classInfo;
	bg_character_t   *character;
	int              team, cls;

	for ( team = TEAM_AXIS; team <= TEAM_ALLIES; team++ )
	{
		for ( cls = PC_SOLDIER; cls < NUM_PLAYER_CLASSES; cls++ )
		{
			classInfo = BG_GetPlayerClassInfo(team, cls);
			character = BG_GetCharacter(team, cls);

			Q_strncpyz(character->characterFile, classInfo->characterFile, sizeof(character->characterFile));

			if (!G_RegisterCharacter(character->characterFile, character))
			{
				G_Error("ERROR: G_RegisterPlayerClasses: failed to load character file '%s' for the %s %s\n", character->characterFile, (team == TEAM_AXIS ? "Axis" : "Allied"), BG_ClassnameForNumber(classInfo->classNum));
			}
		}
	}
}

/*
=================
G_UpdateCharacter
=================
*/
/* Userinfo may request a character, but cannot authorize a configstring read
 * outside the character table or a different multiplayer team uniform. */
void G_UpdateCharacter(gclient_t *client)
{
	char           infostring[MAX_INFO_STRING];
	char           *s;
	char           *end;
	long           requested;
	int            characterIndex = -1;
	int            clientNum = (int)(client - level.clients);
	bg_character_t *character;

	character = BG_GetCharacter(client->sess.sessionTeam, client->sess.playerType);
	trap_GetUserinfo(clientNum, infostring, sizeof(infostring));
	s = Info_ValueForKey(infostring, "ch");
	if (*s && ((g_entities[clientNum].r.svFlags & SVF_BOT) ||
	           (g_gametype.integer != 2 && g_gametype.integer != 5 &&
	            g_gametype.integer != 6 && g_gametype.integer != 7)))
	{
		requested = strtol(s, &end, 10);
		if (s[0] >= '0' && s[0] <= '9' && !*end && requested >= 0 && requested < MAX_CHARACTERS)
		{
			trap_GetConfigstring(CS_CHARACTERS + (int)requested, infostring, sizeof(infostring));
			if (*infostring)
			{
				characterIndex = (int)requested;
				character = BG_FindCharacter(infostring);
				if (!character)
				{
					/* These filenames are server-published map/bot assets, not
					 * client paths. Keep their registration failure explicit. */
					character = BG_FindFreeCharacter(infostring);
					if (!character || !G_RegisterCharacter(infostring, character))
					{
						G_Error("ERROR: G_UpdateCharacter: invalid server character '%s'\n", infostring);
						return;
					}
				}
			}
		}
	}
	if (client->pers.character == character && client->pers.characterIndex == characterIndex)
	{
		return;
	}
	client->pers.characterIndex = characterIndex;
	client->pers.character      = character;
	client->ps.legsAnim   = 0;
	client->ps.torsoAnim  = 0;
	client->ps.legsTimer  = 0;
	client->ps.torsoTimer = 0;
	G_TCEPoseCharacterChanged(client);
}
