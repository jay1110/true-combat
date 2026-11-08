#include "platform_window.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

/* Unlike a foreground-window shortcut, this cannot minimize another application
 * when the player changes focus while a menu action is being processed. */
static BOOL CALLBACK TCE_FindHostWindow(HWND window, LPARAM result)
{
	DWORD processId = 0;
	char className[64];
	GetWindowThreadProcessId(window, &processId);
	if (processId != GetCurrentProcessId() || GetWindow(window, GW_OWNER) ||
	    !IsWindowVisible(window))
	{
		return TRUE;
	}
	if (!GetClassNameA(window, className, sizeof(className)))
	{
		return TRUE;
	}
	if (strcmp(className, "Enemy Territory") && strcmp(className, "SDL_app") &&
	    strcmp(className, "SDL2_app"))
	{
		return TRUE;
	}
	*(HWND *)result = window;
	return FALSE;
}

static HWND TCE_HostWindow(void)
{
	HWND window = NULL;
	EnumWindows(TCE_FindHostWindow, (LPARAM)&window);
	return window;
}

void TCE_PlatformWindowInit(void)
{
	HWND window = TCE_HostWindow();
	if (window)
	{
		LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);
		/* A fullscreen popup must not acquire a caption or window borders. */
		if ((style & WS_CAPTION) == WS_CAPTION && !(style & WS_MINIMIZEBOX))
		{
			SetWindowLongPtr(window, GWL_STYLE, style | WS_MINIMIZEBOX);
			SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |
			             SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
	}
}

int TCE_PlatformWindowMinimize(void)
{
	HWND window = TCE_HostWindow();
	if (!window)
	{
		return 0;
	}
	ShowWindow(window, SW_MINIMIZE);
	return 1;
}
#else
void TCE_PlatformWindowInit(void) {}
int TCE_PlatformWindowMinimize(void) { return 0; }
#endif
