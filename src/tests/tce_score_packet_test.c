#include "../game/tce_score.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "score transport check failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

static char words[256][32];
static int argcValue, numScores, teamScores[2], formats[64];
static tce_score_t scores[64];
static tce_scoreClient_t clients[64];
static const char *argument(int index) { return index >= 0 && index < argcValue ? words[index] : ""; }

static void packet(int part, int first, int count, int fields)
{
	int offset = part == 1 ? 4 : 2, i, j;
	memset(words, 0, sizeof(words));
	strcpy(words[0], part == 1 ? "sc0" : "sc1");
	if (part == 1) { strcpy(words[1], "19"); strcpy(words[2], "-7"); }
	sprintf(words[offset - 1], "%d", count);
	argcValue = offset + count * fields;
	for (i = 0; i < count; ++i)
	{
		int id = first + i;
		int values[14] = { 0, 0, 25, 2, 3, 1, -2, 4, 5, 600, 7, 8, 9, 10 };
		values[0] = id; values[1] = id * 101; values[2] += id;
		for (j = 0; j < fields; ++j) { sprintf(words[offset + i * fields + j], "%d", values[j]); }
	}
}

static int rejectedWithoutChanges(const tce_scoreParseContext_t *ctx, int part)
{
	tce_score_t oldScores[64]; tce_scoreClient_t oldClients[64];
	int oldFormats[64], oldTeams[2], oldCount = numScores;
	memcpy(oldScores, scores, sizeof(scores)); memcpy(oldClients, clients, sizeof(clients));
	memcpy(oldFormats, formats, sizeof(formats)); memcpy(oldTeams, teamScores, sizeof(teamScores));
	return !TCE_CG_AcceptScorePacket(part, argcValue, ctx, formats) && oldCount == numScores &&
	       !memcmp(oldScores, scores, sizeof(scores)) && !memcmp(oldClients, clients, sizeof(clients)) &&
	       !memcmp(oldFormats, formats, sizeof(formats)) && !memcmp(oldTeams, teamScores, sizeof(teamScores));
}

int main(void)
{
	tce_scoreParseContext_t ctx = { &numScores, teamScores, scores, clients, argument };
	const int totals[] = { 0, 1, 16, 18, 19, 31, 32, 33, 63, 64 };
	const char *invalid[] = { "-1", "64", "2147483648", "1x", "", " 2", "--2" };
	int mode, trial, first, count, i, fields;
	for (mode = 0; mode < 2; ++mode)
	{
		fields = mode ? 14 : 7;
		for (trial = 0; trial < (int)(sizeof(totals) / sizeof(totals[0])); ++trial)
		{
			memset(scores, 0, sizeof(scores)); memset(formats, 0, sizeof(formats)); numScores = 0;
			for (i = 0; i < 64; ++i) { clients[i].team = i % 3 + 1; }
			first = 0;
			do
			{
				int part = first ? 2 : 1;
				count = totals[trial] - first;
				if (count > (mode ? 18 : 32)) { count = mode ? 18 : 32; }
				packet(part, first, count, fields);
				CHECK(argcValue <= 256);
				CHECK(TCE_CG_AcceptScorePacket(part, argcValue, &ctx, formats));
				CHECK(numScores == first + count);
				if (part == 2)
				{
					/* Replayed continuation packets update rows, never append copies. */
					CHECK(TCE_CG_AcceptScorePacket(part, argcValue, &ctx, formats));
					CHECK(numScores == first + count);
				}
				first += count;
			} while (first < totals[trial]);
			CHECK(numScores == totals[trial] && teamScores[0] == 19 && teamScores[1] == -7);
			for (i = 0; i < numScores; ++i)
			{
				CHECK(scores[i].client == i && scores[i].score == i * 101);
				CHECK(scores[i].team == clients[i].team && clients[i].powerups == 3);
				CHECK(formats[i] == mode && scores[i].kills == (mode ? 7 : 0));
			}
		}
	}
	for (i = 0; i < (int)(sizeof(invalid) / sizeof(invalid[0])); ++i)
	{
		packet(1, 0, 1, 14); strcpy(words[4], invalid[i]); CHECK(rejectedWithoutChanges(&ctx, 1));
	}
	/* Reproduce the old boundary defect: its size checks admitted this packet,
	 * and the original parser silently turned ID64 into a second client zero. */
	packet(1, 0, 2, 14); strcpy(words[18], "64");
	TCE_CG_ParseScore(1, &ctx);
	CHECK(numScores == 2 && scores[0].client == 0 && scores[1].client == 0);
	CHECK(rejectedWithoutChanges(&ctx, 1));
	packet(1, 0, 1, 14); CHECK(TCE_CG_AcceptScorePacket(1, argcValue, &ctx, formats) && numScores == 1);
	packet(1, 0, 2, 14); strcpy(words[4 + 14 + 7], "not-a-number"); CHECK(rejectedWithoutChanges(&ctx, 1));
	packet(1, 0, 18, 14); --argcValue; CHECK(rejectedWithoutChanges(&ctx, 1));
	packet(1, 0, 18, 14); ++argcValue; CHECK(rejectedWithoutChanges(&ctx, 1));
	packet(1, 0, 1, 14); strcpy(words[3], "-1"); CHECK(rejectedWithoutChanges(&ctx, 1));
	packet(1, 0, 1, 14); strcpy(words[3], "64"); CHECK(rejectedWithoutChanges(&ctx, 1));
	packet(1, 0, 2, 14); strcpy(words[18], "0");
	CHECK(TCE_CG_AcceptScorePacket(1, argcValue, &ctx, formats) && numScores == 1);
	numScores = 3; scores[0].client = 4; scores[1].client = 4; scores[2].client = -7;
	packet(2, 6, 1, 14); CHECK(TCE_CG_AcceptScorePacket(2, argcValue, &ctx, formats));
	CHECK(numScores == 2 && scores[0].client == 4 && scores[1].client == 6);
	numScores = 99; packet(1, 2, 1, 14);
	CHECK(TCE_CG_AcceptScorePacket(1, argcValue, &ctx, formats) && numScores == 1 && scores[0].client == 2);
	packet(1, 0, 1, 14); strcpy(words[5], "-2147483648");
	CHECK(TCE_CG_AcceptScorePacket(1, argcValue, &ctx, formats) && scores[0].score == (-2147483647 - 1));
	puts("PASS score transport: 0..64 clients, 7/14 fields, bounded chunks, repeated/duplicate/invalid rows and atomic rejection");
	return 0;
}
