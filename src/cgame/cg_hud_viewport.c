#include "cg_local.h"
#include "cg_hud_viewport.h"
#include "../ui/tce_viewport.h"
#include "../ui/tce_modern_ui.h"

static vmCvar_t wideScreen, hudScale, hudMaxHeight, menuScale, menuMaxHeight;
static vmCvar_t compassScale, fireteamScale;
static cg_hud_widget_scope_t widgetScope;
static int hudViewportActive, hudViewportFull, hudViewportWorld;
static unsigned menuViewportDepth;
static float hudAnchorX = .5f, hudAnchorY = .5f;

void CG_HudViewportInit(void)
{
	trap_Cvar_Register(&wideScreen, "cg_wideScreen", "1", CVAR_ARCHIVE);
	trap_Cvar_Register(&hudScale, "cg_hudScale", "0.85", CVAR_ARCHIVE);
	trap_Cvar_Register(&hudMaxHeight, "cg_hudMaxHeight", "1080", CVAR_ARCHIVE);
	trap_Cvar_Register(&menuScale, "ui_menuScale", "1", CVAR_ARCHIVE);
	trap_Cvar_Register(&menuMaxHeight, "ui_menuMaxHeight", "1080", CVAR_ARCHIVE);
	trap_Cvar_Register(&compassScale, "cg_compassScale", "0.85", CVAR_ARCHIVE);
	trap_Cvar_Register(&fireteamScale, "cg_fireteamScale", "0.85", CVAR_ARCHIVE);
	hudViewportActive = hudViewportFull = hudViewportWorld = 0;
	widgetScope.active = 0;
	menuViewportDepth = 0;
}

void CG_HudViewportUpdate(void)
{
	trap_Cvar_Update(&wideScreen);
	trap_Cvar_Update(&hudScale);
	trap_Cvar_Update(&hudMaxHeight);
	trap_Cvar_Update(&menuScale);
	trap_Cvar_Update(&menuMaxHeight);
	trap_Cvar_Update(&compassScale);
	trap_Cvar_Update(&fireteamScale);
}

int CG_WideScreenEnabled(void) { return wideScreen.integer != 0; }
int CG_HudAspectMode(void)
{
	extern vmCvar_t cg_aspectMode;
	return CG_WideScreenEnabled() ? 0 : cg_aspectMode.integer;
}

void CG_MenuViewportBegin(void) { ++menuViewportDepth; }
void CG_MenuViewportEnd(void) { if (menuViewportDepth) --menuViewportDepth; }
int CG_MenuViewportActive(void) { return menuViewportDepth != 0; }

int CG_HudWidgetBegin(cg_hud_widget_scope_t *previous, int widget,
                      float x, float y, float w, float h, int tceCanvas)
{
	tce_viewport_t view;
	float width = cgs.glconfig.vidWidth, height = cgs.glconfig.vidHeight;
	float scale, right, bottom, canvas = tceCanvas ? 640.f / .75f : 640.f;
	if (!previous) return 0;
	*previous = widgetScope;
	if (widget != CG_HUD_WIDGET_COMPASS && widget != CG_HUD_WIDGET_FIRETEAM) return 0;
	if (!(x >= -32768.f && x <= 32768.f && y >= -32768.f && y <= 32768.f &&
	      w > 0.f && w <= 32768.f && h > 0.f && h <= 32768.f)) return 0;
	scale = widget == CG_HUD_WIDGET_COMPASS ? compassScale.value : fireteamScale.value;
	if (scale != scale) scale = .85f;
	if (scale < .5f) scale = .5f;
	if (scale > 1.5f) scale = 1.5f;
	/* A widget has its own size and stable corner, independent of HUD/menu
	 * preferences. Keep the previous nominal1080px cap at the default. */
	if (!TCE_ViewportFit(&view, cgs.glconfig.vidWidth, cgs.glconfig.vidHeight,
	                     canvas, 480.f, 1080.f, 1.f,
	                     widget == CG_HUD_WIDGET_COMPASS ? 1.f : 0.f, 0.f)) return 0;
	view.scale *= scale;
	if (w * view.scale > width) view.scale = width / w;
	if (h * view.scale > height) view.scale = height / h;
	if (!(view.scale > 0.f)) return 0;
	view.offsetX = widget == CG_HUD_WIDGET_COMPASS ? width - canvas * view.scale : 0.f;
	view.offsetY = 0.f;
	right = view.offsetX + (x + w) * view.scale;
	bottom = view.offsetY + (y + h) * view.scale;
	if (right > width) view.offsetX -= right - width;
	if (bottom > height) view.offsetY -= bottom - height;
	if (view.offsetX + x * view.scale < 0.f) view.offsetX = -x * view.scale;
	if (view.offsetY + y * view.scale < 0.f) view.offsetY = -y * view.scale;
	widgetScope.active = 1; widgetScope.scale = view.scale;
	widgetScope.offsetX = view.offsetX; widgetScope.offsetY = view.offsetY;
	return 1;
}

void CG_HudWidgetEnd(const cg_hud_widget_scope_t *previous)
{
	if (previous) widgetScope = *previous;
}

void CG_HudViewportBegin(float anchorX, float anchorY)
{
	hudViewportActive = CG_WideScreenEnabled();
	hudViewportFull = hudViewportWorld = 0;
	hudAnchorX = anchorX; hudAnchorY = anchorY;
}

void CG_HudViewportAnchor(float anchorX, float anchorY)
{
	hudViewportFull = hudViewportWorld = 0;
	hudAnchorX = anchorX; hudAnchorY = anchorY;
}

void CG_HudViewportEnd(void) { hudViewportActive = hudViewportFull = hudViewportWorld = 0; }
void CG_HudViewportFull(void) { hudViewportFull = 1; hudViewportWorld = 0; }
void CG_HudViewportWorld(void)
{
	hudViewportActive = CG_WideScreenEnabled();
	hudViewportFull = 0; hudViewportWorld = 1;
}

int CG_HudViewportAdjust(float *x, float *y, float *w, float *h, int tceCanvas)
{
	tce_viewport_t viewport;
	float canvasWidth = tceCanvas ? 640.f / .75f : 640.f;
	if (widgetScope.active)
	{
		viewport.scale = widgetScope.scale; viewport.offsetX = widgetScope.offsetX; viewport.offsetY = widgetScope.offsetY;
		TCE_ViewportProject(&viewport, x, y, w, h);
		return 1;
	}
	if (TCE_ModernUIEnabled() && TCE_ViewportDrawMode() != TCE_VIEWPORT_MENU)
	{
		int fitted = TCE_ViewportDrawMode() == TCE_VIEWPORT_BACKDROP ?
		             TCE_ViewportCover(&viewport,cgs.glconfig.vidWidth,cgs.glconfig.vidHeight,canvasWidth,480.f) :
		             TCE_ViewportFit(&viewport,cgs.glconfig.vidWidth,cgs.glconfig.vidHeight,canvasWidth,480.f,0,1,.5f,.5f);
		if (fitted) TCE_ViewportProject(&viewport,x,y,w,h);
		return 1;
	}
	if (menuViewportDepth)
	{
		/* Modern native menus must fit even when the legacy HUD mode is
		 * selected. Without the optional theme that legacy path is retained. */
		if (!CG_WideScreenEnabled() && !TCE_ModernUIEnabled()) return 0;
		if (TCE_ViewportFit(&viewport, cgs.glconfig.vidWidth, cgs.glconfig.vidHeight,
		                     canvasWidth, 480.f, menuMaxHeight.value, menuScale.value,
		                     .5f, .5f)) TCE_ViewportProject(&viewport, x, y, w, h);
		return 1;
	}
	if (!hudViewportActive) return 0;
	if (hudViewportWorld)
	{
		/* TC portal scenes already use the native TC projection outside HUD
		 * scope. Returning zero delegates to that exact projection again,
		 * including its platform-specific rounding and view displacement. */
		if (tceCanvas) return 0;
		/* SDK binocular art is centered on the same uniform world scale;
		 * stretching its 640x480 texture across the screen distorts circles. */
		viewport.scale = cgs.glconfig.vidWidth / (640.f / .75f);
		viewport.offsetX = (cgs.glconfig.vidWidth - 640.f * viewport.scale) * .5f;
		viewport.offsetY = (cgs.glconfig.vidHeight - 480.f * viewport.scale) * .5f;
		TCE_ViewportProject(&viewport, x, y, w, h);
		return 1;
	}
	/* Fullscreen fades/masks cover the real frame; they are not HUD widgets. */
	if (hudViewportFull)
	{
		float sx = cgs.glconfig.vidWidth / canvasWidth, sy = cgs.glconfig.vidHeight / 480.f;
		*x *= sx; *y *= sy; *w *= sx; *h *= sy;
		return 1;
	}
	if (!TCE_ViewportFit(&viewport, cgs.glconfig.vidWidth, cgs.glconfig.vidHeight,
	                     canvasWidth, 480.f, hudMaxHeight.value, hudScale.value,
	                     hudAnchorX, hudAnchorY)) return 0;
	TCE_ViewportProject(&viewport, x, y, w, h);
	return 1;
}

void CG_HudViewportMaskMargins(int tceCanvas)
{
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	float x = 0, y = 0, w = tceCanvas ? 640.f / .75f : 640.f, h = 480.f;
	float right, bottom, screenW = cgs.glconfig.vidWidth, screenH = cgs.glconfig.vidHeight;
	vec4_t black = { 0, 0, 0, 1 };
	if (!hudViewportActive || !hudViewportWorld || screenW <= 0 || screenH <= 0) return;
	previous = tce_uiCoordinates; tce_uiCoordinates = tceCanvas != 0;
	CG_AdjustFrom640(&x, &y, &w, &h); tce_uiCoordinates = previous;
	right = x + w; bottom = y + h;
	if (x < 0) x = 0; if (x > screenW) x = screenW;
	if (y < 0) y = 0; if (y > screenH) y = screenH;
	if (right < 0) right = 0; if (right > screenW) right = screenW;
	if (bottom < 0) bottom = 0; if (bottom > screenH) bottom = screenH;
	trap_R_SetColor(black);
	if (y > 0) trap_R_DrawStretchPic(0, 0, screenW, y, 0, 0, 0, 0, cgs.media.whiteShader);
	if (bottom < screenH) trap_R_DrawStretchPic(0, bottom, screenW, screenH - bottom, 0, 0, 0, 0, cgs.media.whiteShader);
	if (x > 0 && bottom > y) trap_R_DrawStretchPic(0, y, x, bottom - y, 0, 0, 0, 0, cgs.media.whiteShader);
	if (right < screenW && bottom > y) trap_R_DrawStretchPic(right, y, screenW - right, bottom - y, 0, 0, 0, 0, cgs.media.whiteShader);
	trap_R_SetColor(NULL);
}

int CG_HudViewportDimensions(float *w, float *h, int tceCanvas)
{
	extern qboolean tce_uiCoordinates;
	qboolean previous;
	float x = 0, y = 0;
	if (menuViewportDepth && !widgetScope.active && !CG_WideScreenEnabled() && !TCE_ModernUIEnabled()) return 0;
	if (!hudViewportActive && !menuViewportDepth && !widgetScope.active) return 0;
	previous = tce_uiCoordinates; tce_uiCoordinates = tceCanvas != 0;
	CG_AdjustFrom640(&x, &y, w, h); tce_uiCoordinates = previous;
	return 1;
}
