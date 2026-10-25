#ifndef TCE_MORTAR_MAP_H
#define TCE_MORTAR_MAP_H

#if defined(__linux__) && !defined(Q3_VM)
/* The Linux controller keeps its arithmetic in x87 registers until these
 * explicit Float stores. Import viewport integers without a Float spill. */
static __inline void TCE_MortarMapProject(const float *position, int px, int py,
                                         int pw, int ph, const mapScissor_t *scissor,
                                         float *point)
{
	long double x, y;

	x = ((long double)position[0] - cg.mapcoordsMins[0]) * cg.mapcoordsScale[0];
	x *= (long double)pw;
	if (scissor)
	{
		x *= (long double)scissor->zoomFactor;
	}
	else
	{
		x += (long double)px;
	}
	point[0] = (float)x;
	y = ((long double)position[1] - cg.mapcoordsMins[1]) * cg.mapcoordsScale[1];
	y *= (long double)ph;
	if (scissor)
	{
		y *= (long double)scissor->zoomFactor;
	}
	else
	{
		y += (long double)py;
	}
	point[1] = (float)y;
}

static __inline qboolean TCE_MortarMapCulled(const float *point,
                                           const mapScissor_t *scissor)
{
	long double width, radiusSquared, dy, distanceSquared;
	float dx;

	if (!scissor->circular)
	{
		return scissor->tl[0] > point[0] || point[0] > scissor->br[0] ||
		       !(point[1] >= scissor->tl[1]) || point[1] > scissor->br[1];
	}
	width = (long double)scissor->br[0] - scissor->tl[0];
	radiusSquared = (width * width) * 0.25L;
	dx = (float)((long double)point[0] - (width * 0.5L + scissor->tl[0]));
	dy = (long double)point[1] -
	     (scissor->tl[1] + ((long double)scissor->br[1] - scissor->tl[1]) * 0.5L);
	/* The native FST of dy retains its wide value for the following square. */
	distanceSquared = dy * dy + (long double)dx * dx;
	return distanceSquared > radiusSquared;
}

static __inline void TCE_MortarMapOffset(float *point, int px, int py,
                                        const mapScissor_t *scissor)
{
	point[0] = (float)((long double)point[0] + ((long double)px - scissor->tl[0]));
	point[1] = (float)(((long double)py - scissor->tl[1]) + point[1]);
}
#endif
#endif
