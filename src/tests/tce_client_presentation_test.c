#include "../cgame/tce_compass_map.h"
#include "../game/tce_countryflags.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "presentation check failed line%d: %s\n", __LINE__, #c); return 1; } } while (0)
static int near(float a, float b) { return fabs(a - b) < .00001; }

int main(void)
{
	char path[64];
	float origin[2] = { 500, 1000 }, mins[2] = { -2000, 2000 };
	float maxs[2] = { 2000, -2000 }, st[2];
	int i;
	CHECK(!TCE_CompassMapPath(0, "obj_delta", path, sizeof(path)) && !path[0]);
	CHECK(TCE_CompassMapPath(1, "obj_delta", path, sizeof(path)) && !strcmp(path, "levelshots/obj_delta_cc.tga"));
	CHECK(TCE_CompassMapPath(2, "obj_delta", path, sizeof(path)) && !strcmp(path, "levelshots/obj_delta_cc_blueprint.tga"));
	CHECK(!TCE_CompassMapPath(3, "obj_delta", path, sizeof(path)) && !path[0]);
	CHECK(!TCE_CompassMapPath(1, "../obj_delta", path, sizeof(path)));
	CHECK(!TCE_CompassMapPath(1, "", path, sizeof(path)));
	CHECK(!TCE_CompassMapPath(2, "obj_delta", path, 10) && !path[0]);
	CHECK(TCE_CompassMapTexCoord(origin, mins, maxs, 0, 2000, 0, 0, st) && near(st[0], .625f) && near(st[1], .25f));
	/* Looking east: the top is world+x, the right is world-y. */
	CHECK(TCE_CompassMapTexCoord(origin, mins, maxs, 0, 2000, 0, -1, st) && near(st[0], 1.125f) && near(st[1], .25f));
	CHECK(TCE_CompassMapTexCoord(origin, mins, maxs, 0, 2000, 1, 0, st) && near(st[0], .625f) && near(st[1], .75f));
	/* Looking north: the top is world+y, the right is world+x. */
	CHECK(TCE_CompassMapTexCoord(origin, mins, maxs, 90, 2000, 0, -1, st) && near(st[0], .625f) && near(st[1], -.25f));
	CHECK(TCE_CompassMapTexCoord(origin, mins, maxs, 90, 2000, 1, 0, st) && near(st[0], 1.125f) && near(st[1], .25f));
	CHECK(!TCE_CompassMapTexCoord(origin, mins, mins, 0, 2000, 0, 0, st));
	CHECK(TCE_CountryFlagAtlasIndex(-1) == -1 && TCE_CountryFlagAtlasIndex(256) == -1);
	CHECK(TCE_CountryFlagAtlasIndex(0) == 1 && TCE_CountryFlagAtlasIndex(254) == 1 && TCE_CountryFlagAtlasIndex(255) == 1);
	for (i = 1; i < 254; ++i) { CHECK(TCE_CountryFlagAtlasIndex(i) == i); }
	puts("PASS client presentation: compass modes/path bounds/headings/mapcoordinates; country atlas local/unknown mapping");
	return 0;
}
