#ifndef TCE_COMPASS_MAP_H
#define TCE_COMPASS_MAP_H

#include <stddef.h>

int TCE_CompassMapPath(int mode, const char *map, char *path, size_t size);
/* Screen offsets are fractions of the current compass radius. */
int TCE_CompassMapTexCoord(const float origin[2], const float mins[2],
                          const float maxs[2], float yaw, float radius, float x, float y,
                          float st[2]);
/* Project ground-plane coordinates into a heading-up compass. */
int TCE_CompassPoint(const float origin[2], const float target[2], float yaw,
                     float radius, int mapVisible, float point[2]);
int TCE_CommandMapPoint(const float target[2], const float mins[2],
                        const float maxs[2], float point[2]);

#endif
