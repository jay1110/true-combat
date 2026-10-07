#ifndef TCE_POPUP_DRAW_H
#define TCE_POPUP_DRAW_H
typedef struct tce_popup_draw_item_s {
 int type,inuse,time;
 char message[128];
 int shader;
 struct tce_popup_draw_item_s *next;
} tce_popup_draw_item_t;
typedef struct {
 void (*color)(const float *);
 void (*pic)(float,float,float,float,int);
 void (*paint)(float,float,float,float,float *,const char *,float,int,int,void *);
 int (*duration)(int);
 void *font;
} tce_popup_draw_api_t;
void TCE_DrawPMItems(int,int,int,const tce_popup_draw_item_t *,const tce_popup_draw_item_t *,const tce_popup_draw_api_t *);
void TCE_DrawPMItemsBig(int,int,const tce_popup_draw_item_t *,const tce_popup_draw_api_t *);
#endif
