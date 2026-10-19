#ifndef TCE_POPUP_DRAW_H
#define TCE_POPUP_DRAW_H
#include <stddef.h>
typedef struct tce_popup_draw_item_s
{
	int type, inuse, time;
	char message[128];
	int shader;
	struct tce_popup_draw_item_s *next;
} tce_popup_draw_item_t;
/* Native popup fields are shared by the queue and the reconstructed renderer. */
typedef char tcePopupDrawLayout_t[(sizeof(int) == 4 &&
                                 offsetof(tce_popup_draw_item_t, inuse) == 0x04 &&
                                 offsetof(tce_popup_draw_item_t, time) == 0x08 &&
                                 offsetof(tce_popup_draw_item_t, message) == 0x0c &&
                                 offsetof(tce_popup_draw_item_t, shader) == 0x8c &&
                                 offsetof(tce_popup_draw_item_t, next) == 0x90) ? 1 : -1];
typedef char tcePopupDrawNative32Size_t[(sizeof(void *) != 4 ||
                                       sizeof(tce_popup_draw_item_t) == 0x94) ? 1 : -1];
typedef struct
{
	void (*color)(const float *);
	void (*pic)(float, float, float, float, int);
	void (*paint)(float, float, float, float, float *, const char *, float, int, int, void *);
	int (*duration)(int);
	void *font;
} tce_popup_draw_api_t;
void TCE_DrawPMItems(int, int, int, const tce_popup_draw_item_t *, const tce_popup_draw_item_t *, const tce_popup_draw_api_t *);
void TCE_DrawPMItemsBig(int, int, const tce_popup_draw_item_t *, const tce_popup_draw_api_t *);
#endif
