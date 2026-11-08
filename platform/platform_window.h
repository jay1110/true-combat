#ifndef TCE_PLATFORM_WINDOW_H
#define TCE_PLATFORM_WINDOW_H

/* Native host-window integration. No renderer/engine ownership or SDK ABI change. */
void TCE_PlatformWindowInit(void);
int TCE_PlatformWindowMinimize(void);

#endif
