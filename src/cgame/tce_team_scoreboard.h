#ifndef TCE_TEAM_SCOREBOARD_H
#define TCE_TEAM_SCOREBOARD_H
typedef struct {
    int gametype, gameTypeCvar, firstBlood, teamScores[2], numScores;
    int *teamPlayers;
    const int *scoreClients, *clientTeams;
    const float *backColor, *barColor, *black;
    const char *(*translate)(const char *);
    void (*fill)(float,float,float,float,const float *);
    void (*border)(float,float,float,float,int,const float *);
    void (*setColor)(const float *);
    void (*topBottom)(float,float,float,float,float);
    void (*text)(float,float,float,const float *,const char *);
    void (*smallText)(int,int,const char *,float);
    void (*clientRow)(int,int,int,const float *,float);
} tce_teamScoreboardContext_t;
int TCE_WM_TeamScoreboard(int x,int y,int team,float fade,int maxrows,
    const tce_teamScoreboardContext_t *ctx);
#endif
