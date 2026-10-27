#ifndef TCE_COMPASS_MAP_H
#define TCE_COMPASS_MAP_H

#include <stddef.h>

int TCE_CompassMapPath(int mode, const char *map, char *path, size_t size);
/* Screen offsets are fractions of the 2000-unit compass radius. */
int TCE_CompassMapTexCoord(const float origin[2], const float mins[2],
                          const float maxs[2], float yaw, float x, float y,
                          float st[2]);

#endif
