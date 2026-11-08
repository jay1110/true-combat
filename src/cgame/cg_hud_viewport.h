#ifndef CG_HUD_VIEWPORT_H
#define CG_HUD_VIEWPORT_H

void CG_HudViewportInit(void);
void CG_HudViewportUpdate(void);
int CG_WideScreenEnabled(void);
int CG_HudAspectMode(void);
/* Native menus use UI size preferences, independent of a surrounding HUD
 * scope. Nesting preserves that HUD scope and its current anchor. */
void CG_MenuViewportBegin(void);
void CG_MenuViewportEnd(void);
int CG_MenuViewportActive(void);
enum { CG_HUD_WIDGET_COMPASS = 1, CG_HUD_WIDGET_FIRETEAM, CG_HUD_WIDGET_COMMANDMAP };
typedef struct { int active; float scale, offsetX, offsetY; } cg_hud_widget_scope_t;
/* Independent corner widgets restore the surrounding HUD/menu transform.
 * Bounds are in the widget's native640 or TCE canvas, never screen pixels. */
int CG_HudWidgetBegin(cg_hud_widget_scope_t *previous, int widget,
                      float x, float y, float w, float h, int tceCanvas);
void CG_HudWidgetEnd(const cg_hud_widget_scope_t *previous);
void CG_HudViewportBegin(float anchorX, float anchorY);
void CG_HudViewportAnchor(float anchorX, float anchorY);
void CG_HudViewportEnd(void);
void CG_HudViewportFull(void);
void CG_HudViewportWorld(void);
void CG_HudViewportMaskMargins(int tceCanvas);
int CG_HudViewportDimensions(float *w, float *h, int tceCanvas);
int CG_HudViewportAdjust(float *x, float *y, float *w, float *h, int tceCanvas);

#endif
