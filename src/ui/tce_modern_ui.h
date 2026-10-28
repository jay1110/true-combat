#ifndef TCE_MODERN_UI_H
#define TCE_MODERN_UI_H

/* Optional presentation only. The marker is data, never an executable config. */
qboolean TCE_ModernUIEnabled(void);
void TCE_ModernUIReset(void);
int TCE_ModernMarkerValid(const char *data, int length);
float TCE_ModernProgress(int used, int expected);
int TCE_ModernRosterPages(int count, int rows);
int TCE_ModernHiddenStats(int gameType, int killMessages, int health, int pmType);

extern vec4_t tceModernWhite, tceModernMuted, tceModernAmber;
void TCE_ModernFill(float x, float y, float w, float h, const float *color);
void TCE_ModernPanel(float x, float y, float w, float h, float alpha);
void TCE_ModernText(float x, float y, float scale, float width,
                    const float *color, const char *text, int strong);
void TCE_ModernParagraph(float x, float y, float scale, float width,
                         float lineHeight, int lines, const float *color, const char *text);
void TCE_ModernImage(const char *name, float x, float y, float w, float h);
void TCE_ModernBar(float x, float y, float w, float h, float progress, int milliseconds);

#endif
