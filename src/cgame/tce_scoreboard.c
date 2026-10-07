#include "tce_scoreboard.h"

/* Original TC 854-wide coordinate space. Rendering adapters own conversion
 * while the surrounding SDK still uses 640-wide UI coordinates. */
int TCE_CG_DrawScoreboard(const tce_scoreboardContext_t *ctx) {
    float fade = 1, *color;
    int y, rows = 20;
    if (ctx->paused) return 0;
    if ((ctx->warmup || (ctx->demoPlayback && ctx->snapshotPmType != 5)) &&
        !ctx->showScores) return 0;
    if (ctx->cameraMode) return 1;
    if (!ctx->showScores && ctx->predictedPmType != 5) {
        color = ctx->fadeColor(ctx->scoreFadeTime, 200);
        if (!color) { ctx->killerName[0] = 0; return 0; }
        fade = color[3];
    }
    y = ctx->objectives(20, 10, 817, fade);
    if (ctx->gametype == 3 && ctx->snapshotPmType == 5) {
        y = ctx->infoLine(20, 155, fade);
        rows = 8;
    }
    ctx->teamBoard(20, y, 1, fade, rows);
    ctx->teamBoard(443, y, 2, fade, rows);
    return 1;
}

