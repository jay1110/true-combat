#include "tce_score_objectives.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Descriptive name for Windows 3001f420; the original symbol is unknown. */
void TCE_DrawScoreWindow(const float *r,const char *title,int align,int height,
 float scale,int baseline,const tce_objectivesContext_t *c) {
 const float back[]={0,0,0,.6f},bar[]={.2f,.2f,.2f,.6f},text[]={.6f,.6f,.6f,1};
 float x;
 c->fill(r[0],r[1],r[2],r[3],back);
 c->border(r[0],r[1],r[2],r[3],1,back);
 c->fill(r[0]+2,r[1]+2,r[2]-4,(float)height,bar);
 if(!title)return;
 if(align==1)x=r[0]+(r[2]-c->width(title,scale))*.5f;
 else if(align==2)x=r[0]+r[2]-c->width(title,scale);
 else x=r[0]+5;
 c->text(x,r[1]+baseline,scale,text,title);
}

int TCE_WM_DrawObjectives(int x,int y,int width,float fade,const tce_objectivesContext_t *c) {
 const float color[]={.6f,.6f,.6f,1};
 char s[1024];const char *title,*label,*value;int ms=0,mins=0,seconds=0,tens=0,n;
 (void)fade;
 if(c->pmType==5){
  /* Original CS_MULTI_MAPWINNER=14; only the winner key is required. */
  const char *info=c->config(14);int winner=0;
  winner=atoi(c->infoValue(info,"winner"));
  if(winner!=-1){
   const float rect[]={15,35,822,20};
   title=winner?"SPECOPS WIN":"TERRORISTS WIN";
   /* Original caller passes a promoted double to the old float prototype:
    * scale becomes 0, baseline is the high word of double .25. Preserve
    * its otherwise invisible title draw; the visible line follows. */
   TCE_DrawScoreWindow(rect,title,0,18,0,0x3fd00000,c);
   c->text(20,51,.25f,color,title);
   sprintf(s,"%s [%i]:[%i]",c->translate("SCORE"),c->wonRounds[1],c->wonRounds[0]);
   c->text(320-c->width(s,.25f)*.5f,51,.25f,color,s);
   n=5-(c->time-c->intermissionStartTime)/1000;if(n<0)n=0;
   sprintf(s,"%i SECS TO NEXT DEPLOYMENT",n);
   c->text((float)(833-c->width(s,.25f)),51,.25f,color,s);
  }
  return y+80;
 }
 c->fill((float)x-5,(float)y-2,(float)width+5,21,c->backColor);
 c->fill((float)x-5,(float)y-2,(float)width+5,21,c->barColor);
 c->border((float)x-5,(float)y-2,(float)width+5,21,1,c->black);
 if(c->timelimit>0){ms=(int)((double)c->timelimit*60000.0-(c->time-c->startTime));mins=ms/1000/60;seconds=ms/1000%60;tens=seconds/10;seconds%=10;}
 if(c->gameState!=0|| (ms<0&&c->timelimit>0)){
  value=c->translate(c->gameState!=0?"WARMUP":"SUDDEN DEATH");label=c->translate("MISSION TIME:");sprintf(s,"%s %s",label,value);
 }else sprintf(s,"%s   %2.0f:%i%i",c->translate("MISSION TIME:"),(double)mins,tens,seconds);
 c->text((float)x,(float)y+13,.25f,color,s);
 if(c->gameType!=5&&(c->viewedTeam==1||c->viewedTeam==2)){
  ms=c->reinforce(0)*1000;
  if(ms){n=ms/1000;seconds=n%60;sprintf(s,"%s %2.0f:%i%i",c->translate("REINFORCE TIME:"),(double)(n/60),seconds/10,seconds%10);c->text((float)(832-c->width(s,.25f)),(float)y+13,.25f,color,s);}
 }
 if(c->gameType==3)sprintf(s,"%s %i",c->translate("STOPWATCH ROUND"),c->currentRound+1);
 else if(c->gameType==2||c->gameType==5||c->gameType==7){label=c->translate("SCORE");title=c->translate("ROUND");sprintf(s,"%s %i  %s [%i]:[%i]",title,c->currentRound+1,label,c->wonRounds[1],c->wonRounds[0]);}
 else if(c->gameType==4)sprintf(s,"MAP %i of %i",c->campaignMap+1,c->campaignMaps);
 else return y+32;
 c->text(x+300-c->width(s,.25f)*.5f,(float)y+13,.25f,color,s);
 return y+32;
}
