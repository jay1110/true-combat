#ifndef TCE_VIEWPORT_H
#define TCE_VIEWPORT_H

#define TCE_VIEWPORT_CANVAS_WIDTH (640.f / .75f)
#define TCE_VIEWPORT_CANVAS_HEIGHT 480.f

typedef struct
{
	float scale, offsetX, offsetY;
} tce_viewport_t;

/* Fits one logical canvas uniformly inside a physical viewport. Anchor values
 * choose the remaining margin: 0 left/top, .5 centered, 1 right/bottom.
 * A nonpositive maximum height disables the physical size cap. User scaling
 * may enlarge a capped canvas, but never beyond the physical viewport. */
int TCE_ViewportFit(tce_viewport_t *out, int width, int height,
                    float canvasWidth, float canvasHeight, float maxHeight,
                    float userScale, float anchorX, float anchorY);
void TCE_ViewportProject(const tce_viewport_t *view,
                          float *x, float *y, float *w, float *h);
/* Temporary paint scopes; callers restore the returned previous mode. */
enum { TCE_VIEWPORT_MENU, TCE_VIEWPORT_FULLSCREEN, TCE_VIEWPORT_BACKDROP };
int TCE_ViewportDrawMode(void);
int TCE_ViewportSetDrawMode(int mode);
int TCE_ViewportCover(tce_viewport_t *out, int width, int height,
                       float canvasWidth, float canvasHeight);
float TCE_ViewportCursorSize(int width, int height);
/* Header-only modern hubs stay attached to the top edge when reduced. All
 * full-page menus/dialogs retain the centered canvas. Shared with UI drawing. */
float TCE_MenuViewportAnchorY(void);
int TCE_MenuViewportBackground(void);

#endif
