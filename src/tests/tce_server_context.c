/* SDK globals for the isolated server-loader test process. */
#include "../game/g_local.h"
level_locals_t level;
vmCvar_t g_gametype;
void captureLoaderWarning(const char *message);
void setServerLoaderContext(int mode, const char *map) {
    g_gametype.integer = mode;
    Q_strncpyz(level.rawmapname, map, sizeof(level.rawmapname));
}
void QDECL G_Printf(const char *format, ...) {
    char message[2048]; va_list args;
    va_start(args, format); Q_vsnprintf(message, sizeof(message), format, args); va_end(args);
    captureLoaderWarning(message);
}
