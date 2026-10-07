#include "tce_limbo_widgets.h"
#include <stdio.h>
/* CG_DrawBorder 30042020: TC uses translucent fills, no ET surround textures. */
void TCE_LimboDrawBorder(float x,float y,float w,float h,int fill,int hover,
 int (*inside)(const tce_limboRect_t *),
 void (*rect)(float,float,float,float,const float *)) {
 const float back[4]={.1f,.1f,.1f,.7f},highlight[4]={0,.2f,.4f,.4f};
 tce_limboRect_t bounds={x,y,w,h};
 if(fill)rect(x,y,w,h,hover&&inside(&bounds)?highlight:back);
}
void TCE_LimboClass(const tce_limboButton_t *b,const tce_limboWidgets_t *c) {
 c->pic(b->x,b->y,b->w,b->h,c->classOff);
 if(c->team()!=3&&b->data[1]==c->playerClass())c->pic(b->x,b->y,b->w,b->h,c->classOn);
}
void TCE_LimboCounter(const tce_limboButton_t *b,const tce_limboWidgets_t *c) {
 const float on[4]={.99f,.99f,.99f,1},off[4]={.5f,.5f,.5f,1};
 char text[64];int n=c->counter(b);const float *color;
 snprintf(text,sizeof(text),n<10?"[0%i]":"[%i]",n);
 color=c->team()==c->teamOrder[b->data[1]]?on:off;
 c->text(b->x+3.f,(float)((double)b->h+b->y-6.0),.16f,.2f,color,text);
}
void TCE_LimboWeaponLight(const tce_limboButton_t *b,const tce_limboWidgets_t *c) {
 int shader;
 c->border(b->x+2,b->y+2,b->w-4,b->h-4,0,0);
 shader=c->weaponOff;
 if(c->team()!=3&&b->data[0]==c->selectedSlot)shader=c->weaponOn;
 c->pic(b->x,b->y,b->w,b->h,shader);
}
void TCE_LimboBorder(const tce_limboButton_t *b,const tce_limboWidgets_t *c) {
 c->border(b->x,b->y,b->w,b->h,1,1);
}
