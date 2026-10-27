#include "tce_compass_map.h"
#include <math.h>
#include <stdio.h>

int TCE_CompassMapPath(int mode, const char *map, char *path, size_t size)
{
	int length;
	const char *p;
	if (!path || !size)
	{
		return 0;
	}
	path[0] = 0;
	if ((mode != 1 && mode != 2) || !map || !*map)
	{
		return 0;
	}
	for (p = map; *p; ++p)
	{
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		      (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
		{
			return 0;
		}
	}
	length = snprintf(path, size, "levelshots/%s_cc%s.tga", map,
	                  mode == 2 ? "_blueprint" : "");
	if (length < 0 || (size_t)length >= size)
	{
		path[0] = 0;
		return 0;
	}
	return 1;
}

int TCE_CompassMapTexCoord(const float origin[2], const float mins[2],
                          const float maxs[2], float yaw, float x, float y,
                          float st[2])
{
	float radians, forwardX, forwardY, worldX, worldY;
	if (!origin || !mins || !maxs || !st || mins[0] == maxs[0] || mins[1] == maxs[1])
	{
		return 0;
	}
	radians = yaw * (3.14159265358979323846f / 180.0f);
	forwardX = (float)cos(radians); forwardY = (float)sin(radians);
	worldX = origin[0] + 2000.0f * (x * forwardY - y * forwardX);
	worldY = origin[1] + 2000.0f * (-x * forwardX - y * forwardY);
	st[0] = (worldX - mins[0]) / (maxs[0] - mins[0]);
	st[1] = (worldY - mins[1]) / (maxs[1] - mins[1]);
	return 1;
}
