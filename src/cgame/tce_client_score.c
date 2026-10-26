#include "tce_client_score.h"
#include <stdio.h>
#include "../game/tce_numeric.h"

static void number(const tce_clientScoreContext_t *c, float x, float y,
                   const float *color, const char *format, int value,
                   char *(*formatText)(const char *, ...))
{
	char text[64];
	const char *valueText;
	if (formatText)
	{
		valueText = formatText(format, value);
	}
	else
	{
		snprintf(text, sizeof(text), format, value);
		valueText = text;
	}
	c->text(x, y, .2f, color, valueText);
}

/* Windows 3005a960..3005b281. The input color is unused in the original. */
static void TCE_DrawClientScore(int x, int y, const tce_score_t *s,
                                const float *color, float fade,
                                const tce_clientScoreContext_t *c,
                                char *(*formatText)(const char *, ...))
{
	float      tx, fy, highlight[4] = { .75f, .5f, 0, 0 }, white[4] = { 1, 1, 1, 0 };
	int        offset = 0, i, extended = c->gameType == 2 || c->gameType == 5 || c->gameType == 6 || c->gameType == 7;
	int        teammate = c->localTeam != 3 && c->team == c->localTeam;
	int        hidden;
	char       text[64];
	const char *ratingText;
	const char *spectator;
	(void)color;
	if (TCE_AddInt32(y, 16) >= 470)
	{
		return;
	}
	if (s->client == c->viewedClient)
	{
		highlight[3] = (float)(fade * .3);
		fy           = (float)(y - 13);
		c->fill((float)x, fy, 138, 15, highlight);
		tx = (float)x + 140;
		if (c->team == 3)
		{
			c->fill(tx, fy, 248, 15, highlight);
		}
		else
		{
			if (extended)
			{
				for (i = 0; i < 4; ++i)
				{
					c->fill(tx, fy, 26, 15, highlight); tx += 28;
				}
				c->fill(tx, fy, 40, 15, highlight); tx += 42;
				c->fill(tx, fy, 38, 15, highlight); tx += 40;
			}
			else
			{
				c->fill(tx, fy, 34, 15, highlight); tx += 36;
			}
			c->fill(tx, fy, 26, 15, highlight);
			c->fill(tx + 28, fy, 30, 15, highlight);
		}
	}
	tx = (float)x; fy = (float)y; white[3] = fade;
	if (c->pmType == 5 || (c->team != 3 && c->team == c->localTeam))
	{
		if (c->powerups & 0xc0)
		{
			c->pic(tx - 4, (float)(y - 14), 16, 16, c->objectiveShader);
			tx += 12; offset = 8;
		}
		if (s->respawnsLeft == -2 || (teammate && c->health == -1))
		{
			c->pic(tx, (float)(y - 14), 18, 18, c->eliminatedShader);
			tx += 18; offset += 18;
		}
		else if (teammate && c->health == 0)
		{
			c->pic(tx + 1, (float)(y - 13), 16, 16, c->medicShader);
			tx += 18; offset += 18;
		}
	}
	if (c->namePrefix)
	{
		c->namePrefix(tx, fy, s->client, fade);
		tx += 18; offset += 18;
	}
	if (c->nameText)
	{
		c->nameText(tx, fy, (float)x + 132 - tx, white, c->name);
	}
	else
	{
		c->text(tx, fy, .2f, white, c->name);
	}
	c->drawStrlen(c->name);
	tx += (float)(140 - offset);
	if (c->team == 3)
	{
		spectator = c->translate("Spectator"); c->drawStrlen(spectator);
		c->text(tx + 36, fy, .2f, white, spectator); return;
	}
	if (c->viewedTeam != c->team)
	{
		c->locateMergedClient(s->client);
	}
	hidden                                         = c->gameType == 5 && c->killMessage == 0 && c->viewedHealth > 0 && c->pmType != 5;
	number(c, tx, fy, white, "%2i", s->kills, formatText); tx += 28;
	if (hidden)
	{
		c->text(tx, fy, .2f, white, formatText?formatText("n/a"):"n/a");
	}
	else
	{
		number(c, tx, fy, white, "%2i", s->deaths, formatText);
	}
	tx += 28;
	if (hidden)
	{
		c->text(tx, fy, .2f, white, formatText?formatText("n/a"):"n/a");
	}
	else
	{
		number(c, tx, fy, white, "%2i", s->suicides, formatText);
	}
	tx                                                += 28;
	number(c, tx, fy, white, "%2i", s->teamKills, formatText); tx += 28;
	if (hidden)
	{
		c->text(tx, fy, .2f, white, formatText?formatText("n/a"):"n/a");
	}
	else
	{
		number(c, tx, fy, white, "%3i", s->score, formatText);
	}
	tx += 42;
	if (hidden)
	{
		c->text(tx, fy, .2f, white, formatText?formatText("n/a"):"n/a");
	}
	else
	{
		/* The original x87 promotes the product straight to double for va. */
		if (formatText)
		{
			ratingText = formatText("%4.2f", (double)s->damageRating * (double).01f);
		}
		else
		{
			snprintf(text, sizeof(text), "%4.2f", (double)s->damageRating * (double).01f);
			ratingText = text;
		}
		c->text(tx, fy, .2f, white, ratingText);
	}
	tx += extended?40:36;
	number(c, tx, fy, white, "%2i", TCE_AddInt32(s->classRating, 1), formatText);
	number(c, tx + 28, fy, white, "%4i", s->ping, formatText);
}

void TCE_WM_DrawClientScore(int x, int y, const tce_score_t *s,
                            const float *color, float fade, const tce_clientScoreContext_t *c)
{
	TCE_DrawClientScore(x, y, s, color, fade, c, NULL);
}

/* Supply the module's va to retain each original formatting side effect. */
void TCE_WM_DrawClientScoreWithFormat(int x, int y, const tce_score_t *s,
                                      const float *color, float fade,
                                      const tce_clientScoreContext_t *c,
                                      char *(*formatText)(const char *, ...))
{
	TCE_DrawClientScore(x, y, s, color, fade, c, formatText);
}
