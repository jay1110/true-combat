#include "tce_viewport.h"
#include <float.h>

static int TCE_ViewportFinite(double value)
{
	return value >= -FLT_MAX && value <= FLT_MAX;
}

int TCE_ViewportFit(tce_viewport_t *out, int width, int height,
                    float canvasWidth, float canvasHeight, float maxHeight,
                    float userScale, float anchorX, float anchorY)
{
	tce_viewport_t next;
	double scale, candidate, offsetX, offsetY;
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
	if (TCE_ViewportFinite(maxHeight) && maxHeight > 0.f)
	{
		candidate = (double)maxHeight / canvasHeight;
		if (candidate < scale) scale = candidate;
	}
	scale *= userScale;
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
