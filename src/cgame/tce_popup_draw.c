#include "tce_popup_draw.h"
static void draw(const tce_popup_draw_item_t *item,float y,int time,const tce_popup_draw_api_t *a) {
 float color[4]={1,1,1,1};
 float end=(float)(a->duration(item->type)+2000+item->time);
 if(end<(float)time)color[3]=1.f-((float)time-end)*.0004f;
 a->color(color);a->pic(4,y,12,12,item->shader);a->color(0);
 a->paint(18,y+12,.2f,.2f,color,item->message,0,0,0,a->font);
}
void TCE_DrawPMItems(int aspect,int respawns,int time,const tce_popup_draw_item_t *waiting,const tce_popup_draw_item_t *old,const tce_popup_draw_api_t *a) {
 float y;int i;
 if(!waiting)return;
 y=aspect==1?-20.67f:aspect==2?-74.f:6.f;
 y+=4;if(respawns>=0)y-=20;
 draw(waiting,y,time,a);
 for(i=0;i<4&&old;++i,old=old->next){y+=14;draw(old,y,time,a);}
}

void TCE_DrawPMItemsBig(int aspect,int time,const tce_popup_draw_item_t *waiting,const tce_popup_draw_api_t *a) {
 float y,end,color[4]={1,1,1,1};
 if(!waiting)return;
 y=aspect==1?414.67f:aspect==2?468.f:388.f;
 end=(float)(a->duration(waiting->type)+3500+waiting->time);
 if(end<(float)time)color[3]=1.f-((float)time-end)*.001f;
 a->color(color);a->pic(4,y,12,12,waiting->shader);a->color(0);
 a->paint(18,y+12,.22f,.22f,color,waiting->message,0,0,0,a->font);
}
