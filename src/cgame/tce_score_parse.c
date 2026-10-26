#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "../game/tce_score.h"

void TCE_CG_ParseScore(int part, const tce_scoreParseContext_t *ctx)
{
	int offset, count, j, powerups;
	if (part == 1)
	{
		*ctx->numScores    = 0;
		ctx->teamScores[0] = atoi(ctx->argv(1));
		ctx->teamScores[1] = atoi(ctx->argv(2));
		offset             = 4;
	}
	else
	{
		offset = 2;
	}
	count = atoi(ctx->argv(offset - 1));
	for (j = 0; j < count; ++j, offset += 14)
	{
		tce_score_t       *s = &ctx->scores[*ctx->numScores];
		tce_scoreClient_t *c;
		s->client              = atoi(ctx->argv(offset));
		s->score               = atoi(ctx->argv(offset + 1));
		s->ping                = atoi(ctx->argv(offset + 2));
		s->time                = atoi(ctx->argv(offset + 3));
		powerups               = atoi(ctx->argv(offset + 4));
		s->playerClass         = atoi(ctx->argv(offset + 5));
		s->respawnsLeft        = atoi(ctx->argv(offset + 6));
		s->classRating         = atoi(ctx->argv(offset + 7));
		s->previousClassRating = atoi(ctx->argv(offset + 8));
		s->damageRating        = atoi(ctx->argv(offset + 9));
		s->kills               = atoi(ctx->argv(offset + 10));
		s->deaths              = atoi(ctx->argv(offset + 11));
		s->suicides            = atoi(ctx->argv(offset + 12));
		s->teamKills           = atoi(ctx->argv(offset + 13));
		if (s->client < 0 || s->client >= 64)
		{
			s->client = 0;
		}
		c              = &ctx->clients[s->client];
		c->score       = s->score; c->powerups = powerups;
		c->classRating = s->classRating; c->previousClassRating = s->previousClassRating;
		c->playerClass = s->playerClass; s->team = c->team;
		++*ctx->numScores;
	}
}

static int TCE_ScoreInteger(const char *text, int *value)
{
	char *end;
	long parsed;
	if (!text || !*text || (*text != '-' && *text != '+' && (*text < '0' || *text > '9')))
	{
		return 0;
	}
	errno = 0;
	parsed = strtol(text, &end, 10);
	if (end == text || *end || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX)
	{
		return 0;
	}
	*value = (int)parsed;
	return 1;
}

int TCE_CG_AcceptScorePacket(int part, int argc, const tce_scoreParseContext_t *ctx,
                           int extendedRows[64])
{
	tce_score_t staged[64], incoming[64];
	tce_scoreClient_t clients[64];
	int formats[64], powerups[64], values[14], teams[2];
	int offset = part == 1 ? 4 : 2, count, fields, i, j, slot, used = 0;
	if (!ctx || !ctx->argv || !ctx->scores || !ctx->clients || !ctx->numScores ||
	    !ctx->teamScores || !extendedRows || (part != 1 && part != 2) ||
	    argc < offset || argc > 256 ||
	    (part != 1 && (*ctx->numScores < 0 || *ctx->numScores > 64)) ||
	    !TCE_ScoreInteger(ctx->argv(offset - 1), &count) || count < 0 || count > 64)
	{
		return 0;
	}
	if (argc - offset == count * 14)
	{
		fields = 14;
	}
	else if (argc - offset == count * 7)
	{
		fields = 7;
	}
	else
	{
		return 0;
	}
	memcpy(teams, ctx->teamScores, sizeof(teams));
	if (part == 1 && (!TCE_ScoreInteger(ctx->argv(1), &teams[0]) ||
	                  !TCE_ScoreInteger(ctx->argv(2), &teams[1])))
	{
		return 0;
	}
	/* Parse every field before committing any row or client metadata. */
	for (i = 0; i < count; ++i)
	{
		tce_score_t *row = &incoming[i];
		memset(values, 0, sizeof(values));
		for (j = 0; j < fields; ++j)
		{
			if (!TCE_ScoreInteger(ctx->argv(offset + i * fields + j), &values[j]))
			{
				return 0;
			}
		}
		if (values[0] < 0 || values[0] >= 64)
		{
			/* Never convert an invalid/truncated ID into client zero. */
			return 0;
		}
		memset(row, 0, sizeof(*row));
		row->client = values[0]; row->score = values[1]; row->ping = values[2]; row->time = values[3];
		powerups[i] = values[4]; row->playerClass = values[5]; row->respawnsLeft = values[6];
		row->classRating = values[7]; row->previousClassRating = values[8]; row->damageRating = values[9];
		row->kills = values[10]; row->deaths = values[11]; row->suicides = values[12]; row->teamKills = values[13];
		row->team = ctx->clients[row->client].team;
	}
	memcpy(staged, ctx->scores, sizeof(staged));
	memcpy(clients, ctx->clients, sizeof(clients));
	memcpy(formats, extendedRows, sizeof(formats));
	if (part == 1)
	{
		memset(formats, 0, sizeof(formats));
	}
	else
	{
		/* Normalize an old or repeated continuation instead of appending copies. */
		for (i = 0; i < *ctx->numScores; ++i)
		{
			int id = ctx->scores[i].client;
			if (id < 0 || id >= 64)
			{
				continue;
			}
			for (slot = 0; slot < used && staged[slot].client != id; ++slot) {}
			staged[slot] = ctx->scores[i]; formats[slot] = extendedRows[i];
			if (slot == used) { ++used; }
		}
	}
	for (i = 0; i < count; ++i)
	{
		int id = incoming[i].client;
		for (slot = 0; slot < used && staged[slot].client != id; ++slot) {}
		if (slot == used)
		{
			if (used == 64) { return 0; }
			++used;
		}
		staged[slot] = incoming[i]; formats[slot] = fields == 14;
		clients[id].score = incoming[i].score; clients[id].powerups = powerups[i];
		if (fields == 14)
		{
			clients[id].classRating = incoming[i].classRating;
			clients[id].previousClassRating = incoming[i].previousClassRating;
			clients[id].playerClass = incoming[i].playerClass;
		}
	}
	memcpy(ctx->scores, staged, sizeof(staged));
	memcpy(ctx->clients, clients, sizeof(clients));
	memcpy(extendedRows, formats, sizeof(formats));
	memcpy(ctx->teamScores, teams, sizeof(teams));
	*ctx->numScores = used;
	return 1;
}
