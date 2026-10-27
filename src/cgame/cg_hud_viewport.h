#ifndef CG_HUD_VIEWPORT_H
#define CG_HUD_VIEWPORT_H

void CG_HudViewportInit(void);
void CG_HudViewportUpdate(void);
int CG_WideScreenEnabled(void);
int CG_HudAspectMode(void);
void CG_HudViewportBegin(float anchorX, float anchorY);
void CG_HudViewportAnchor(float anchorX, float anchorY);
void CG_HudViewportEnd(void);
void CG_HudViewportFull(void);
void CG_HudViewportWorld(void);
void CG_HudViewportMaskMargins(int tceCanvas);
int CG_HudViewportDimensions(float *w, float *h, int tceCanvas);
int CG_HudViewportAdjust(float *x, float *y, float *w, float *h, int tceCanvas);

#endif
