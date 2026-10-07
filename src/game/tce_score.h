#ifndef TCE_SCORE_H
#define TCE_SCORE_H
/* Original TC score record: fifteen 32-bit fields, fourteen transmitted.
 * powerUps is retained storage, not written by the original parser. */
typedef struct {
    int client, score, ping, time, powerUps, team, playerClass, respawnsLeft;
    int classRating, previousClassRating, damageRating, kills, deaths, suicides, teamKills;
} tce_score_t;
typedef struct {int team, score, powerups, classRating, previousClassRating, playerClass;} tce_scoreClient_t;
typedef struct {
    int *numScores, *teamScores;
    tce_score_t *scores;
    tce_scoreClient_t *clients;
    const char *(*argv)(int);
} tce_scoreParseContext_t;
/* Caller validates packet size and destination capacity before entering. */
void TCE_CG_ParseScore(int part,const tce_scoreParseContext_t *ctx);
#endif
