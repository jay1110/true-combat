#ifndef TCE_VIDEO_MODES_H
#define TCE_VIDEO_MODES_H
int TCE_UI_GetVideoMode(void);
int TCE_UI_ApplyVideoMode(int mode);
void TCE_UI_SaveVideoMode(void);
void TCE_UI_ResetVideoMode(void);
#endif
