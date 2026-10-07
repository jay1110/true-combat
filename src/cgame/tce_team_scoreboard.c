#include <stdio.h>
#include "tce_team_scoreboard.h"

int TCE_WM_TeamScoreboard(int x,int y,int team,float fade,int maxrows,
    const tce_teamScoreboardContext_t *ctx) {
    float header[4]={.6f,.6f,.6f,1}, white[4]={1,1,1,fade}, band[4];
    float tx;
    int i, count, top;
    char title[1024], decorated[1200];
    const char *players,*name;
    ctx->fill((float)(x-5),(float)(y-2),399,21,ctx->backColor);
    ctx->fill((float)(x-5),(float)(y-2),399,21,ctx->barColor);
    ctx->border((float)(x-5),(float)(y-2),399,21,1,ctx->black);
    if(team==1||team==2) {
        players=ctx->translate("PLAYERS");
        name=ctx->translate(team==1?"TERRORISTS":"SPECOPS");
        snprintf(title,sizeof(title),"%s [%d] (%d %s)",name,ctx->teamScores[team-1],ctx->teamPlayers[team],players);
        if(ctx->gameTypeCvar==7) {
            snprintf(decorated,sizeof(decorated),"%s ^3%s",title,
                ctx->firstBlood==team?ctx->translate("FIRST BLOOD"):"");
            ctx->text((float)x,(float)(y+13),.25f,header,decorated);
        } else ctx->text((float)x,(float)(y+13),.25f,header,title);
    }
    y+=19;top=y;
    for(i=0;i<=maxrows;++i) {
        band[0]=band[1]=band[2]=(i&1)?0:80.f/255.f;
        band[3]=(float)(fade*.3);
        ctx->fill((float)(x-5),(float)y,399,17,band);
        ctx->setColor(ctx->black);
        ctx->topBottom((float)(x-5),(float)y,399,17,1);
        ctx->setColor(NULL);y+=16;
    }
    y=top;
    ctx->fill((float)(x-5),(float)(y-1),399,18,ctx->backColor);
    ctx->setColor(ctx->black);
    ctx->topBottom((float)(x-5),(float)(y-1),399,18,1);
    ctx->setColor(NULL);
    tx=(float)x;
    ctx->text(tx,(float)(y+14),.2f,white,ctx->translate("Name"));tx+=140;
    if(ctx->gametype==2||(ctx->gametype>=5&&ctx->gametype<=7)) {
        static const char *labels[]={"Kll","Dth","Sui","Tk"};
        for(i=0;i<4;++i){ctx->text(tx,(float)(y+14),.2f,white,ctx->translate(labels[i]));tx+=28;}
        ctx->text(tx,(float)(y+14),.2f,white,ctx->translate("Score"));tx+=42;
        ctx->text(tx,(float)(y+14),.2f,white,ctx->translate("DR"));tx+=40;
    } else {
        ctx->smallText((int)(tx+8),y+14,ctx->translate("XP"),fade);tx+=36;
    }
    ctx->text(tx,(float)(y+14),.2f,white,ctx->translate("AA"));tx+=28;
    ctx->text(tx,(float)(y+14),.2f,white,ctx->translate("Ping"));
    y+=30;ctx->teamPlayers[team]=0;
    for(i=0;i<ctx->numScores;++i)
        if(ctx->clientTeams[ctx->scoreClients[i]]==team)++ctx->teamPlayers[team];
    count=0;
    for(i=0;i<ctx->numScores&&count<maxrows;++i) {
        if(ctx->clientTeams[ctx->scoreClients[i]]!=team)continue;
        ctx->clientRow(x,y,i,white,fade);
        y+=ctx->teamPlayers[team]>maxrows?12:16;++count;
    }
    y+=16;
    for(i=0;i<ctx->numScores;++i) {
        if(ctx->clientTeams[ctx->scoreClients[i]]!=3)continue;
        if(team==1&&(i&1))continue;
        if(team==2&&((i+1)&1))continue;
        ctx->clientRow(x,y,i,white,fade);y+=16;
    }
    return y;
}
