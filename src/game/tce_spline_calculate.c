#include "q_shared.h"
#include "bg_public.h"

/* TC cgame 30003cd0. Reduce the control polygon to its final two points.
 * The Windows original retains the X difference in x87, but stores Y/Z
 * differences as floats before multiplying by tension. */
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
