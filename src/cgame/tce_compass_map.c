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
                          const float maxs[2], float yaw, float radius, float x, float y,
                          float st[2])
{
	float radians, forwardX, forwardY, worldX, worldY;
	if (!origin || !mins || !maxs || !st || mins[0] == maxs[0] || mins[1] == maxs[1] ||
	    !(radius > 0.0f) || !isfinite(radius))
	{
		return 0;
	}
	radians = yaw * (3.14159265358979323846f / 180.0f);
	forwardX = (float)cos(radians); forwardY = (float)sin(radians);
	worldX = origin[0] + radius * (x * forwardY - y * forwardX);
	worldY = origin[1] + radius * (-x * forwardX - y * forwardY);
	st[0] = (worldX - mins[0]) / (maxs[0] - mins[0]);
	st[1] = (worldY - mins[1]) / (maxs[1] - mins[1]);
	return isfinite(st[0]) && isfinite(st[1]);
}

int TCE_CompassPoint(const float origin[2], const float target[2], float yaw,
                     float radius, int mapVisible, float point[2])
{
	float dx, dy, distance, fraction, radians;
	if (!origin || !target || !point || !(radius > 0.0f) || !isfinite(radius))
	{
		return 0;
	}
	dx = target[0] - origin[0]; dy = target[1] - origin[1];
	distance = (float)sqrt(dx * dx + dy * dy);
	if (!isfinite(distance) || !isfinite(yaw))
	{
		return 0;
	}
	if (distance == 0.0f)
	{
		point[0] = point[1] = 0.0f;
		return 1;
	}
	fraction = distance / radius;
	if (fraction > 1.0f) { fraction = 1.0f; }
	if (!mapVisible) { fraction = (float)sin(fraction * 1.5707963267948966f); }
	radians = yaw * (3.14159265358979323846f / 180.0f);
	point[0] = (dx * (float)sin(radians) - dy * (float)cos(radians)) / distance * fraction;
	point[1] = -(dx * (float)cos(radians) + dy * (float)sin(radians)) / distance * fraction;
	return 1;
}

int TCE_CommandMapPoint(const float target[2], const float mins[2],
                        const float maxs[2], float point[2])
{
	if (!target || !mins || !maxs || !point || mins[0] == maxs[0] || mins[1] == maxs[1])
	{
		return 0;
	}
	point[0] = (target[0] - mins[0]) / (maxs[0] - mins[0]);
	point[1] = (target[1] - mins[1]) / (maxs[1] - mins[1]);
	return isfinite(point[0]) && isfinite(point[1]) && point[0] >= 0.0f &&
	       point[0] <= 1.0f && point[1] >= 0.0f && point[1] <= 1.0f;
}
