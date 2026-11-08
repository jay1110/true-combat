#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include "platform/platform_window.h"

/* Own off-screen test windows only; never sends input to another application. */
static HWND MakeWindow(const char *name, DWORD style)
{
	WNDCLASSA type = { 0 };
	type.lpfnWndProc = DefWindowProcA;
	type.hInstance = GetModuleHandle(NULL);
	type.lpszClassName = name;
	RegisterClassA(&type);
	return CreateWindowExA(WS_EX_TOOLWINDOW, name, "TCE window regression",
	                      style, -30000, -30000, 160, 100, NULL, NULL,
	                      type.hInstance, NULL);
}

int main(void)
{
	HWND unknown, host;
	LONG_PTR before;
	assert(TCE_PlatformWindowMinimize() == 0);
	unknown = MakeWindow("TCE unknown helper", WS_CAPTION | WS_SYSMENU | WS_VISIBLE);
	assert(unknown);
	before = GetWindowLongPtr(unknown, GWL_STYLE);
	TCE_PlatformWindowInit();
	assert(GetWindowLongPtr(unknown, GWL_STYLE) == before);
	assert(TCE_PlatformWindowMinimize() == 0);
	host = MakeWindow("Enemy Territory", WS_CAPTION | WS_SYSMENU | WS_VISIBLE);
	assert(host);
	TCE_PlatformWindowInit();
	assert(GetWindowLongPtr(host, GWL_STYLE) & WS_MINIMIZEBOX);
	assert(GetWindowLongPtr(unknown, GWL_STYLE) == before);
	assert(TCE_PlatformWindowMinimize() == 1);
	assert(IsIconic(host));
	assert(!IsIconic(unknown));
	DestroyWindow(host);
	host = MakeWindow("SDL_app", WS_POPUP | WS_VISIBLE);
	assert(host);
	before = GetWindowLongPtr(host, GWL_STYLE);
	TCE_PlatformWindowInit();
	assert(GetWindowLongPtr(host, GWL_STYLE) == before);
	assert(!(before & WS_CAPTION));
	assert(TCE_PlatformWindowMinimize() == 1);
	assert(IsIconic(host));
	DestroyWindow(host);
	DestroyWindow(unknown);
	return 0;
}
