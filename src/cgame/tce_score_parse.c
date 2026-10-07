#include <stdlib.h>
#include "../game/tce_score.h"

void TCE_CG_ParseScore(int part,const tce_scoreParseContext_t *ctx) {
    int offset, count, j, powerups;
    if(part==1) {
        *ctx->numScores=0;
        ctx->teamScores[0]=atoi(ctx->argv(1));
        ctx->teamScores[1]=atoi(ctx->argv(2));
        offset=4;
    } else offset=2;
    count=atoi(ctx->argv(offset-1));
    for(j=0;j<count;++j,offset+=14) {
        tce_score_t *s=&ctx->scores[*ctx->numScores];
        tce_scoreClient_t *c;
        s->client=atoi(ctx->argv(offset));
        s->score=atoi(ctx->argv(offset+1));
        s->ping=atoi(ctx->argv(offset+2));
        s->time=atoi(ctx->argv(offset+3));
        powerups=atoi(ctx->argv(offset+4));
        s->playerClass=atoi(ctx->argv(offset+5));
        s->respawnsLeft=atoi(ctx->argv(offset+6));
        s->classRating=atoi(ctx->argv(offset+7));
        s->previousClassRating=atoi(ctx->argv(offset+8));
        s->damageRating=atoi(ctx->argv(offset+9));
        s->kills=atoi(ctx->argv(offset+10));
        s->deaths=atoi(ctx->argv(offset+11));
        s->suicides=atoi(ctx->argv(offset+12));
        s->teamKills=atoi(ctx->argv(offset+13));
        if(s->client<0||s->client>=64)s->client=0;
        c=&ctx->clients[s->client];
        c->score=s->score;c->powerups=powerups;
        c->classRating=s->classRating;c->previousClassRating=s->previousClassRating;
        c->playerClass=s->playerClass;s->team=c->team;
        ++*ctx->numScores;
    }
}
