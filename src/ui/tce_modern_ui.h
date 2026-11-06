#ifndef TCE_MODERN_UI_H
#define TCE_MODERN_UI_H

/* Optional presentation only. The marker is data, never an executable config. */
qboolean TCE_ModernUIEnabled(void);
void TCE_ModernUIReset(void);
void TCE_ModernBackgroundPreferencesInit(void);
void TCE_ModernThemeRefresh(void);
/* surface: 0 text, 1 fill, 2 border. Never changes the input paint/state. */
void TCE_ModernThemeColor(const float *source, float *out, int surface);
void TCE_ModernThemePicture(float x, float y, float w, float h, qhandle_t shader, const float *color);
int TCE_ModernMarkerValid(const char *data, int length);
float TCE_ModernProgress(int used, int expected);
int TCE_ModernRosterPages(int count, int rows);
int TCE_ModernHiddenStats(int gameType, int killMessages, int health, int pmType);
/* Pure background policy: phases are morning/day/twilight/night/winter. */
int TCE_ModernBackgroundPhase(int minute, int sunrise, int sunset, int snow);
int TCE_ModernBackgroundPick(int phase, unsigned seed, int previous);
int TCE_ModernBackgroundWinter(int month, const char *country, const char *months);

extern vec4_t tceModernWhite, tceModernMuted, tceModernAmber;
void TCE_ModernFill(float x, float y, float w, float h, const float *color);
void TCE_ModernPanel(float x, float y, float w, float h, float alpha);
void TCE_ModernText(float x, float y, float scale, float width,
                    const float *color, const char *text, int strong);
void TCE_ModernParagraph(float x, float y, float scale, float width,
                         float lineHeight, int lines, const float *color, const char *text);
void TCE_ModernImage(const char *name, float x, float y, float w, float h);
void TCE_ModernBackdrop(const char *name);
void TCE_ModernCursor(float x, float y, qhandle_t shader, int centered);
/* A preferred minimum, applied before layout. Field/paragraph bounds may
 * reduce it again; a final draw must never undo an earlier fit. */
float TCE_ModernReadableScale(float scale, const fontInfo_t *font);
float TCE_ModernInkWidth(const char *text, float scale, int limit, const fontInfo_t *font);
void TCE_ModernTextFit(float x, float y, float scale, float width,
                       const float *color, const char *text, int strong);
void TCE_ModernBar(float x, float y, float w, float h, float progress, int milliseconds);

#endif
