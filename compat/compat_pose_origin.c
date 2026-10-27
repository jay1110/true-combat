#include "../src/game/q_shared.h"
#include "../src/game/bg_public.h"
#include "compat_pose_origin.h"
#include <math.h>

qboolean BG_RaySphereIntersection(float radius,vec3_t origin,
    splineSegment_t *segment,float *first,float *second);

/* Same mod BG_LinearPathOrigin2 segment/root ordering, with a finite affected-
 * only traversal. One complete map spline population plus the initial segment
 * revisited covers all possible roots (the first-segment direction gate differs
 * from later segments). A cyclic graph with no root must not hang a shot/draw.
 * Ordinary spline movement keeps its existing implementation unchanged. */
int TCE_PosePathOffset(float radius,splinePath_t **path,float *fraction,vec3_t point) {
    long double position=(long double)*fraction*MAX_SPLINE_SEGMENTS;
    float within;int index,steps=0;qboolean first=qtrue,reverse;
    if(!*path||!isfinite(radius)||!isfinite(*fraction)||*fraction<0||*fraction>1)return 0;
#if defined(_WIN32)
    index=(int)floor((double)position);reverse=!(radius>=0);
#else
    index=(int)floorl(position);reverse=radius<0;
#endif
    if(index>=MAX_SPLINE_SEGMENTS) {index=MAX_SPLINE_SEGMENTS-1;within=1;}
    else {
#if defined(_WIN32)
        within=(float)((long double)*fraction*MAX_SPLINE_SEGMENTS-index);
#else
        within=(float)(position-index);
#endif
    }
    for(;;) {
        float t0,t1;
        if(++steps>MAX_SPLINE_PATHS*MAX_SPLINE_SEGMENTS+1)return 0;
        if(BG_RaySphereIntersection(radius,point,&(*path)->segments[index],&t0,&t1)) {
            const splineSegment_t *segment=&(*path)->segments[index];
            long double normalized0,normalized1,firstRoot,secondRoot,selected=0;
            qboolean found=qfalse,preferFirst;int axis;
#if defined(_WIN32)
            normalized0=(long double)t0/segment->length;t0=(float)normalized0;
            normalized1=(long double)t1/segment->length;t1=(float)normalized1;
            firstRoot=t0;secondRoot=t1;
#else
            normalized0=(1.0L/segment->length)*t0;t0=(float)normalized0;
            normalized1=(1.0L/segment->length)*t1;t1=(float)normalized1;
            firstRoot=reverse?normalized0:(long double)t0;
            secondRoot=reverse&&!first?normalized1:(long double)t1;
#endif
            if(first) {
                preferFirst=reverse?!(firstRoot>=within):firstRoot>within;
                if(preferFirst&&firstRoot>=0&&firstRoot<=1) {selected=firstRoot;found=qtrue;}
                else if(reverse?!(secondRoot>=within):secondRoot>within) {selected=secondRoot;found=qtrue;}
            } else {
                if(reverse)preferFirst=!(firstRoot>=secondRoot);
                else {
#if defined(_WIN32)
                    preferFirst=firstRoot>secondRoot;
#else
                    preferFirst=!(secondRoot>=firstRoot);
#endif
                }
                selected=preferFirst&&firstRoot>=0&&firstRoot<=1?firstRoot:secondRoot;found=qtrue;
            }
            if(found&&selected>=0&&selected<=1) {
                *fraction=(float)((long double)index*.0625f+selected*.0625f);
                for(axis=0;axis<3;++axis) {
                    segment=&(*path)->segments[index];
                    point[axis]=(float)((selected*segment->length)*segment->v_norm[axis]+segment->start[axis]);
                }
                return 1;
            }
        }
        first=qfalse;
        if(reverse) {
            if(--index>=0)continue;
            index=MAX_SPLINE_SEGMENTS-1;if(!(*path)->prev)return 1;*path=(*path)->prev;
        } else {
            if(++index<MAX_SPLINE_SEGMENTS)continue;
            index=0;if(!(*path)->next)return 1;*path=(*path)->next;
        }
    }
}
