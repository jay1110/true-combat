#ifndef TCE_CLIENT_SCORE_H
#define TCE_CLIENT_SCORE_H
#include "../game/tce_score.h"

typedef struct {
    const char *name;
    int team, powerups, health;
    int localTeam, viewedClient, viewedTeam, viewedHealth, pmType;
    int gameType, killMessage;
    int objectiveShader, eliminatedShader, medicShader;
    void (*fill)(float,float,float,float,const float *);
    void (*pic)(float,float,float,float,int);
    void (*text)(float,float,float,const float *,const char *);
    int (*drawStrlen)(const char *);
    const char *(*translate)(const char *);
    void (*locateMergedClient)(int);
} tce_clientScoreContext_t;

void TCE_WM_DrawClientScore(int x,int y,const tce_score_t *score,
    const float *color,float fade,const tce_clientScoreContext_t *ctx);
#endif
