#include "../game/g_local.h"

enum { rosterSize = 2 * NUM_PLAYER_CLASSES };
level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
static gclient_t roster[rosterSize];
static int messages;

int Q_stricmp(const char *a, const char *b)
{
	return strcmp(a, b);
}

/* This test counts deliveries. Message formatting belongs to the engine. */
char *QDECL va(const char *format, ...)
{
	(void)format;
	return "system message";
}

void trap_SendServerCommand(int clientNum, const char *text)
{
	(void)clientNum;
	(void)text;
	++messages;
}

int main(void)
{
	int i;
	level.maxclients = rosterSize;
	level.time = 60000;
	for (i = 0; i < rosterSize; ++i)
	{
		g_entities[i].inuse = qtrue;
		g_entities[i].client = &roster[i];
		roster[i].sess.sessionTeam = i < NUM_PLAYER_CLASSES ? TEAM_AXIS : TEAM_ALLIES;
		roster[i].sess.playerType = i % NUM_PLAYER_CLASSES;
	}

	G_CheckForNeededClasses();
	if (messages != 0)
	{
		fprintf(stderr, "Full rosters received %d class requests\n", messages);
		return 1;
	}

	/* Leave exactly one class missing on one team. */
	roster[PC_MEDIC].sess.playerType = PC_SOLDIER;
	level.time += 60000;
	G_CheckForNeededClasses();
	if (messages != NUM_PLAYER_CLASSES)
	{
		fprintf(stderr, "Expected one team broadcast, got %d deliveries\n", messages);
		return 1;
	}
	return 0;
}
