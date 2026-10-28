#include "tce_viewport.h"
#include <float.h>

static int tceViewportDrawMode;
int TCE_ViewportDrawMode(void) { return tceViewportDrawMode; }
int TCE_ViewportSetDrawMode(int mode)
{
	int previous = tceViewportDrawMode;
	tceViewportDrawMode = mode;
	return previous;
}

static int TCE_ViewportFinite(double value)
{
	return value >= -FLT_MAX && value <= FLT_MAX;
}

int TCE_ViewportCover(tce_viewport_t *out, int width, int height,
                       float canvasWidth, float canvasHeight)
{
	double scale, vertical, offsetX, offsetY;
	if (!out || width <= 0 || height <= 0 || !TCE_ViewportFinite(canvasWidth) ||
	    !TCE_ViewportFinite(canvasHeight) || canvasWidth <= 0 || canvasHeight <= 0) return 0;
	scale = (double)width / canvasWidth;
	vertical = (double)height / canvasHeight;
	if (vertical > scale) scale = vertical;
	if (!(scale > 0) || !TCE_ViewportFinite(scale)) return 0;
	offsetX = ((double)width - scale * canvasWidth) * .5;
	offsetY = ((double)height - scale * canvasHeight) * .5;
	if (!TCE_ViewportFinite(offsetX) || !TCE_ViewportFinite(offsetY)) return 0;
	out->scale = (float)scale;
	out->offsetX = (float)offsetX;
	out->offsetY = (float)offsetY;
	return 1;
}

float TCE_ViewportCursorSize(int width, int height)
{
	tce_viewport_t viewport;
	if (!TCE_ViewportFit(&viewport,width,height,TCE_VIEWPORT_CANVAS_WIDTH,
	                     TCE_VIEWPORT_CANVAS_HEIGHT,0,1,.5f,.5f)) return 0;
	/* Resolution dependent, never multiplied by menu/HUD preferences. */
	return 20.f * viewport.scale;
}

int TCE_ViewportFit(tce_viewport_t *out, int width, int height,
                    float canvasWidth, float canvasHeight, float maxHeight,
                    float userScale, float anchorX, float anchorY)
{
	tce_viewport_t next;
	double scale, fit, candidate, offsetX, offsetY;
	if (!out || width <= 0 || height <= 0 ||
	    !TCE_ViewportFinite(canvasWidth) || !TCE_ViewportFinite(canvasHeight) ||
	    canvasWidth <= 0.f || canvasHeight <= 0.f) return 0;
	if (!TCE_ViewportFinite(userScale)) userScale = 1.f;
	if (userScale < .5f) userScale = .5f;
	if (userScale > 1.5f) userScale = 1.5f;
	if (!TCE_ViewportFinite(anchorX)) anchorX = .5f;
	if (!TCE_ViewportFinite(anchorY)) anchorY = .5f;
	if (anchorX < 0.f) anchorX = 0.f;
	if (anchorX > 1.f) anchorX = 1.f;
	if (anchorY < 0.f) anchorY = 0.f;
	if (anchorY > 1.f) anchorY = 1.f;
	scale = (double)width / canvasWidth;
	candidate = (double)height / canvasHeight;
	if (candidate < scale) scale = candidate;
	fit = scale;
	if (TCE_ViewportFinite(maxHeight) && maxHeight > 0.f)
	{
		candidate = (double)maxHeight / canvasHeight;
		if (candidate < scale) scale = candidate;
	}
	scale *= userScale;
	/* Applying enlargement after fitting used to place menu edges outside
	 * the frame. A size preference must never make controls unreachable. */
	if (scale > fit) scale = fit;
	offsetX = ((double)width - scale * canvasWidth) * anchorX;
	offsetY = ((double)height - scale * canvasHeight) * anchorY;
	if (!(scale > 0.0) || !TCE_ViewportFinite(scale) ||
	    !TCE_ViewportFinite(offsetX) || !TCE_ViewportFinite(offsetY)) return 0;
	next.scale = (float)scale;
	if (!(next.scale > 0.f)) return 0;
	next.offsetX = (float)offsetX; next.offsetY = (float)offsetY;
	*out = next;
	return 1;
}

void TCE_ViewportProject(const tce_viewport_t *view, float *x, float *y, float *w, float *h)
{
	double values[4];
	if (!view || !(view->scale > 0.f) || !TCE_ViewportFinite(view->scale) ||
	    !TCE_ViewportFinite(view->offsetX) || !TCE_ViewportFinite(view->offsetY)) return;
	values[0] = x ? (double)*x * view->scale + view->offsetX : 0.0;
	values[1] = y ? (double)*y * view->scale + view->offsetY : 0.0;
	values[2] = w ? (double)*w * view->scale : 0.0;
	values[3] = h ? (double)*h * view->scale : 0.0;
	if (!TCE_ViewportFinite(values[0]) || !TCE_ViewportFinite(values[1]) ||
	    !TCE_ViewportFinite(values[2]) || !TCE_ViewportFinite(values[3])) return;
	if (x) *x = (float)values[0];
	if (y) *y = (float)values[1];
	if (w) *w = (float)values[2];
	if (h) *h = (float)values[3];
}
