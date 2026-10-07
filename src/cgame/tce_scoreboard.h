#ifndef TCE_SCOREBOARD_H
#define TCE_SCOREBOARD_H
typedef struct {
    int paused, warmup, demoPlayback, snapshotPmType, showScores, cameraMode;
    int predictedPmType, scoreFadeTime, gametype;
    char *killerName;
    float *(*fadeColor)(int, int);
    int (*objectives)(int, int, int, float);
    int (*infoLine)(int, int, float);
    int (*teamBoard)(int, int, int, float, int);
} tce_scoreboardContext_t;
int TCE_CG_DrawScoreboard(const tce_scoreboardContext_t *ctx);
#endif
