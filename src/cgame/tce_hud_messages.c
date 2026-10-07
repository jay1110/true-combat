/* cgame 30026920, 30026ac0, 30027d40. State projected by cg_draw.c. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tce_hud_messages.h"
static int sameKey(const char *a,const char *b) {
 while(*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)){++a;++b;}
 return !*a && !*b;
}
static const float white[4]={1,1,1,1};
static void paint(const tce_hud_messages_api_t *a,float x,float y,float scale,const char *text) {
 a->paint(x,y,scale,scale,white,text,0,0,3,a->font);
}
void TCE_DrawLimboMessage(const tce_hud_messages_t *s,const tce_hud_messages_api_t *a) {
 char text[1024]; const char *key;
 if(s->health>0 || (s->pmFlags&0x4000) || s->localTeam==3 || !s->descriptive || s->gameType==5)return;
 if(s->gameType==7 || s->gameType==2) {
  key=a->binding("openlimbomenu");
  if(sameKey(key,"(openlimbomenu)"))key="ESCAPE";
  sprintf(text,a->translate("Press %s to open Deploment Menu"),key);
  paint(a,8,154,.2f,text);
  key=a->binding("+moveup");
  sprintf(text,a->translate("Press %s to go into reinforcement queue"),key);
  paint(a,8,168,.2f,text);
 } else {
  if(!s->respawns)sprintf(text,"%s",a->translate("No more reinforcements this round."));
  else sprintf(text,a->translate("Reinforcements deploy in %d seconds."),a->reinf(0));
  a->small(8,118,text,white);
 }
 a->setColor(0);
}
int TCE_DrawFollow(const tce_hud_messages_t *s,const tce_hud_messages_api_t *a) {
 char text[1024];int period;
 if(a->viewing())return 1;
 if(!(s->pmFlags&0x1000))return 0;
 if(s->pmFlags&0x4000) {
  if(s->gameType!=5) {
   if(!s->respawns) {
    if(s->penalty>=0) {
     period=(int)((double)(s->followTeam==1?s->redTime:s->blueTime)*(double).001f);
     sprintf(text,a->translate("Bonus Life! Deploying in %d seconds"),a->reinf(0)+s->penalty*period);
    } else sprintf(text,"%s",a->translate("No more deployments this round"));
   } else sprintf(text,a->translate("Deploying in %d seconds"),a->reinf(0));
   paint(a,8,118,.25f,text);
  }
  if(s->client!=s->localClient) {
   sprintf(text,"(%s %s)",a->translate("Following"),s->name);
   paint(a,8,136,.25f,text);
  }
 } else {
  paint(a,8,118,.25f,a->translate("Following"));
  paint(a,84,118,.25f,s->name);
 }
 return 1;
}
void TCE_DrawObjectiveInfo(tce_hud_messages_t *s,const tce_hud_messages_api_t *a) {
 const char *start;char line[1024];int n,y,width;float *color;
 if(!s->objectiveTime)return;
 color=a->fade(s->objectiveTime,250);
 if(!color){s->objectiveTime=0;return;}
 a->setColor(color);
 /* Original first pass computes unused rectangle bounds; there is no box. */
 start=s->objective;y=410-s->objectiveLines*8;
 for(;;) {
  for(n=0;n<56 && start[n] && start[n]!='\n';++n)line[n]=start[n];
  line[n]=0;
  width=a->width(line,.2f,0,a->font);
  a->paint((float)(426-width/2),(float)y,.2f,.2f,color,line,0,0,3,a->font);
  y=(int)(y+s->objectiveCharWidth*1.5);
  while(*start && *start!='\n')++start;
  if(!*start)break;
  ++start;
 }
 a->setColor(0);
}

