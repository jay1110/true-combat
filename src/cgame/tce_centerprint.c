#include "tce_centerprint.h"
void TCE_CenterPrint(const char *text,int time,int *printTime,int *priority,int shader,tce_centerPopup_t popup) {
 if(!*printTime || *priority<1){popup(4,text,shader);*printTime=time;*priority=0;}
}
void TCE_PriorityCenterPrint(const char *text,int time,int requested,int *printTime,int *priority,int shader,tce_centerPopup_t popup) {
 if(!*printTime || *priority<=requested){popup(4,text,shader);*printTime=time+2000;*priority=requested;}
}
