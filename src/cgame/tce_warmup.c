#include "tce_warmup.h"
#include <stdio.h>
#include <string.h>
static void line(const tce_warmup_api_t *a,const char *s,float y) {
 static const float white[4]={1,1,1,1};
 int w=a->width(s,.2f,0,a->font);
 a->paint((float)(426-w/2),y,.2f,.2f,white,s,0,0,3,a->font);
}
/* Windows 30026d20; deliberately retains the original stopwatch branches. */
void TCE_DrawWarmup(tce_warmup_t *s,const tce_warmup_api_t *a) {
 char text[1024],key[32],round[128]; const char *first="",*second=""; int sec;
 if(!s->warmup) {
  if(s->gameState!=2 && s->gameState!=4)return;
  snprintf(text,sizeof(text),a->translate("^3WARMUP:^7 Waiting on ^2%i^7 %s"),s->minClients,s->minClients==1?"player":"players");
  line(a,text,194);
  if(s->demo || s->team==3 || ((s->pmFlags&0x1000)&&!(s->pmFlags&0x4000)))return;
  strncpy(key,a->binding("ready"),31);key[31]=0;
  if(!strcmp(key,"(???" ")"))snprintf(text,sizeof(text),"%s",a->translate("Type ^3\\ready^* in the console to start"));
  else snprintf(text,sizeof(text),a->translate("Press ^3%s^* to start"),key);
  line(a,text,208);return;
 }
 sec=(s->warmup-s->time)/1000;if(sec<0)sec=0;
 snprintf(text,sizeof(text),"%s %i",a->translate("(WARMUP) Match begins in:"),sec+1);
 line(a,text,120);
 if(sec!=s->count) {
  s->count=sec;
  if(s->warmup-s->time-sec*1000 > -100 && sec<=2)a->sound(a->beep,7);
 }
 if(s->gameType!=3)return;
 snprintf(round,sizeof(round),"%s %i",a->translate("Stopwatch Round"),s->round+1);
 if(s->team==1) {
  first=s->round==1?"You have been switched to the Terrorist team":"You are on the Terrorist team";
  if(s->round==1)second=s->defender?"Try to beat the clock!":"Keep the Specops from beating the clock!";
 } else if(s->team==2) {
  first=s->round==1?"You have been switched to the Specops team":"You are on the Specops team";
  if(s->round==1)second=s->defender?"Keep the Axis from beating the clock!":"Try to beat the clock!";
 }
 if(*first)first=a->translate(first);
 if(*second)second=a->translate(second);
 line(a,round,120);line(a,first,120);line(a,second,120);
}
