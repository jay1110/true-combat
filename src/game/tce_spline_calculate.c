#include "q_shared.h"
#include "bg_public.h"

#if defined(_WIN32) || defined(__linux__)
/* Reduce the control polygon to its last two De Casteljau points. */
void BG_CalculateSpline_r(splinePath_t *spline, vec3_t out1, vec3_t out2, float tension) {
    vec3_t points[18];
    int i, controls = spline->numControls;
    int count = controls + 2;

    memcpy(points[0], spline->point.origin, sizeof(vec3_t));
    for (i = 0; i < controls; ++i) {
        memcpy(points[i + 1], spline->controls[i].origin, sizeof(vec3_t));
    }
    if (!spline->next) return;
    memcpy(points[i + 1], spline->next->point.origin, sizeof(vec3_t));

    while (count > 2) {
        for (i = 0; i < count - 1; ++i) {
            volatile vec_t *current = points[i];
            const vec_t *next = points[i + 1];
#if defined(_WIN32)
            long double dx = (long double)next[0] - current[0];
            volatile float dy = (float)((long double)next[1] - current[1]);
            volatile float dz = (float)((long double)next[2] - current[2]);
            long double scaled = dx * tension;
            long double result = scaled + current[0];

            current[0] = (float)result;
            scaled = (long double)dy * tension;
            result = scaled + current[1];
            current[1] = (float)result;
            scaled = (long double)dz * tension;
            result = scaled + current[2];
            current[2] = (float)result;
#else
            long double x = current[0], nextX = next[0];
            long double y = current[1], nextY = next[1];
            long double dx = nextX - x;
            long double z = current[2];
            long double dy = nextY - y;
            long double nextZ = next[2];
            long double scaledX = dx * tension;
            long double dz = nextZ - z;
            long double resultX = scaledX + x;
            long double scaledY = dy * tension;
            long double resultY, scaledZ, resultZ;

            current[0] = (float)resultX;
            resultY = y + scaledY;
            scaledZ = dz * tension;
            current[1] = (float)resultY;
            resultZ = scaledZ + z;
            current[2] = (float)resultZ;
            if (i == count - 2) {
                /* Native Linux spills the final row's differences in Z/Y/X order. */
                volatile float discardedDifference;
                discardedDifference = (float)dz;
                discardedDifference = (float)dy;
                discardedDifference = (float)dx;
            }
#endif
        }
        --count;
    }
#if defined(_WIN32)
    /* X passes through floating storage; Y/Z are raw component copies. */
    out1[0] = (float)(long double)points[0][0];
    memcpy(out1 + 1, points[0] + 1, 2 * sizeof(vec_t));
    out2[0] = (float)(long double)points[1][0];
    memcpy(out2 + 1, points[1] + 1, 2 * sizeof(vec_t));
#else
    memcpy(out1, points[0], sizeof(vec3_t));
    memcpy(out2, points[1], sizeof(vec3_t));
#endif
}
#else
#ifdef CGAMEDLL
void BG_CalculateSpline_r(splinePath_t *spline, vec3_t out1, vec3_t out2, float tension) {
    vec3_t points[18];
    int i, count = spline->numControls + 2;
    VectorCopy(spline->point.origin, points[0]);
    for (i = 0; i < spline->numControls; ++i) {
        VectorCopy(spline->controls[i].origin, points[i + 1]);
    }
    if (!spline->next) return;
    VectorCopy(spline->next->point.origin, points[i + 1]);
    while (count > 2) {
        for (i = 0; i < count - 1; ++i) {
            float dy = points[i + 1][1] - points[i][1];
            float dz = points[i + 1][2] - points[i][2];
            points[i][0] = (float)(((double)points[i + 1][0] - points[i][0]) * tension + points[i][0]);
            points[i][1] = (float)((double)dy * tension + points[i][1]);
            points[i][2] = (float)((double)dz * tension + points[i][2]);
        }
        --count;
    }
    VectorCopy(points[0], out1);
    VectorCopy(points[1], out2);
}
#else
void BG_CalculateSpline_r(splinePath_t* spline, vec3_t out1, vec3_t out2, float tension) {
	vec3_t points[18];
	int i;
	int count = spline->numControls + 2;
	vec3_t dist;

	VectorCopy( spline->point.origin, points[0] );
	for( i = 0; i < spline->numControls; i++ ) {
		VectorCopy( spline->controls[i].origin, points[i+1] );
	}
	if(!spline->next) {
		return;
//		Com_Error( ERR_DROP, "Spline (%s) with no target referenced", spline->point.name );
	}
	VectorCopy( spline->next->point.origin, points[i+1] );


	while(count > 2) {
		for( i = 0; i < count-1; i++ ) {
#ifdef _WIN32
			/* TC2002bc5a: X stays extended; Y/Z differences are stored
			 * before the multiply/add, then each result is stored once. */
			double dx = (double)points[i+1][0] - (double)points[i][0];
			dist[1] = points[i+1][1] - points[i][1];
			dist[2] = points[i+1][2] - points[i][2];
			points[i][0] = (float)(dx * (double)tension + (double)points[i][0]);
			points[i][1] = (float)((double)dist[1] * (double)tension + (double)points[i][1]);
			points[i][2] = (float)((double)dist[2] * (double)tension + (double)points[i][2]);
#else
			VectorSubtract( points[i+1], points[i], dist );
			VectorMA(points[i], tension, dist, points[i]);
#endif
		}
		count--;
	}

	VectorCopy( points[0], out1 );
	VectorCopy( points[1], out2 );
}
#endif
#endif
