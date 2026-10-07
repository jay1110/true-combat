#include "tce_popup_text.h"
#include <stdio.h>
const char *TCE_GetPMItemText(const tce_popup_text_t *s,const tce_popup_text_api_t *a,char *out,unsigned int size) {
 const char *format=0,*team;
 switch(s->kind) {
 case 0:
  if(s->action==0)format="Planted at %s.";else if(s->action==1)format="Defused at %s.";
  if(format)snprintf(out,size,format,a->config(0x301+s->client));else return 0;
  break;
 case 1:
  if(s->action==-1)return a->config(0x38f+s->client);
  if(s->action!=0)return 0;
  snprintf(out,size,"%s has been constructed.",a->config(0x301+s->client));break;
 case 2:
  if(s->localTeam==s->action)return 0;
  snprintf(out,size,"Spotted by %s^7 at %s",a->name(s->client),a->location(s->origin));break;
 case 4:
  if(s->snapshotTeam!=s->action)return 0;
  switch(s->density) {
   case 0:format="%s^7 has received the bomb^7!";break;
   case 1:format="%s^7 has dropped the bomb^7!";break;
   case 2:format="%s^7 has picked up the bomb^7!";break;
   case 3:format="%s^7 is the VIP^7!";break;
   case 4:format="%s^7 has dropped the VIP's files^7!";break;
   default:return 0;
  }
  snprintf(out,size,format,a->name(s->client));break;
 case 5:
  if(s->density==0)format="%s have stolen %s!";else if(s->density==1)format="%s have returned %s!";else return 0;
  team=s->action==2?"Specops":"Terrorist";
  snprintf(out,size,format,team,a->config(0x38f+s->client));break;
 case 6:
  if(s->action==0)format="%s has been damaged.";else if(s->action==1)format="%s has been destroyed.";else return 0;
  snprintf(out,size,format,a->config(0x301+s->client));break;
 case 7:
  if(s->density==0){team=s->action==1?"Terrorists":s->action==2?"Specops":"Spectators";snprintf(out,size,"%s^7 has joined the %s^7!",a->name(s->client),team);}
  else if(s->density==1)snprintf(out,size,"%s^7 disconnected",a->name(s->client));else return 0;
  break;
 default:return 0;
 }
 return out;
}
