#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include "compat/legacy/game/tce_weather_precision.h"

/* Frozen pre-optimization production body, from weather header SHA256
 * 5e3dac5f7b7be569aa5e5e0c735d6878f3f1422403d74c037547f54dcc6459e3.
 * Keep this independent body so validation does not require ignored reports. */
static __inline int FrozenWeatherOutside(const float *position,
                                             const float *view, int *outside)
{
    tceTriggerSum_t difference[2], value, squared, sum, limit = { { 0 }, 0 };
    unsigned control;
    int i;

    __control87_2(0, 0, &control, NULL);
    if (!TCE_FiniteExceptionsMasked(control))
    {
        return 0;
    }
    for (i = 0; i < 2; ++i)
    {
        if (!TCE_TriggerProduct(&difference[i], position[i], 1.0f, control) ||
            !TCE_TriggerProduct(&value, view[i], 1.0f, control))
        {
            return 0;
        }
        value.negative ^= 1u;
        TCE_RotateAdd(&difference[i], &value, control);
    }
    TCE_SmokeSquareDifference(&difference[1], &sum, control);
    TCE_SmokeSquareDifference(&difference[0], &squared, control);
    TCE_RotateAdd(&sum, &squared, control);
    TCE_TriggerPlace(&limit, 1000000u, 298);
    *outside = TCE_TriggerCompare(&sum, &limit) > 0;
    return 1;
}

static volatile unsigned sink;
static float positions[4000][3], views[4000][3], velocities[4000][3];
static unsigned state=713;
static unsigned random_word(void) {state=1664525u*state+1013904223u;return state;}
static double seconds(void) {LARGE_INTEGER now,f;QueryPerformanceCounter(&now);QueryPerformanceFrequency(&f);return(double)now.QuadPart/f.QuadPart;}
__declspec(noinline) static int outside_soft(const float *p,const float*v,int*out) {return TCE_FiniteWeatherOutside(p,v,out);}
__declspec(noinline) static int outside_frozen(const float *p,const float*v,int*out) {return FrozenWeatherOutside(p,v,out);}
__declspec(noinline) static int outside_fallback(const float *p,const float*v,int*out)
{
    long double dx=(long double)p[0]-v[0],dy=(long double)p[1]-v[1];
    *out=dy*dy+dx*dx>1000000.0L;return 1;
}
__declspec(noinline) static int move_soft(int ms,float*p,const float*v) {return TCE_FiniteWeatherMove(ms,p,v,NULL,NULL);}
__declspec(noinline) static int move_fallback(int ms,float*p,const float*v)
{
    long double elapsed=(long double)ms*(long double)(double)0.001;
    p[0]=(float)(elapsed*v[0]+p[0]);p[1]=(float)(elapsed*v[1]+p[1]);p[2]=(float)(elapsed*v[2]+p[2]);return 1;
}
static double time_outside(int soft)
{
    int frame,i,out;double start=seconds();
    for(frame=0;frame<200;++frame)for(i=0;i<4000;++i) {
        if(soft==2) outside_frozen(positions[i],views[i],&out);
        else if(soft) outside_soft(positions[i],views[i],&out);else outside_fallback(positions[i],views[i],&out);
        sink+=(unsigned)out;
    }
    return(seconds()-start)*1000/200;
}
static double time_move(int soft)
{
    int frame,i;float p[3];double start=seconds();
    for(frame=0;frame<200;++frame)for(i=0;i<4000;++i) {
        memcpy(p,positions[i],sizeof(p));
        if(soft)move_soft(16,p,velocities[i]);else move_fallback(16,p,velocities[i]);
        {unsigned bits;memcpy(&bits,p,4);sink+=bits;}
    }
    return(seconds()-start)*1000/200;
}
#if defined(TCE_HAVE_WEATHER_FASTPATH)
static unsigned checks, failures, accepted, declined, worldAccepted, worldDeclined;
static float from_bits(unsigned bits) {float v;memcpy(&v,&bits,4);return v;}
static void compare(const float *position,const float *view,unsigned control)
{
    int a=777,b=777,ra,rb,fast=777;
    unsigned xa,sa,xb,sb,beforeX,beforeS,afterControl;
    /* Seed a masked SSE exception before each call. Integer-only paths must
     * preserve preexisting status, not merely leave freshly clear flags clear. */
    volatile double one=1,zero=0,result;
    _clearfp();result=one/zero;(void)result;_statusfp2(&beforeX,&beforeS);
    ra=FrozenWeatherOutside(position,view,&a);_statusfp2(&xa,&sa);
    _clearfp();result=one/zero;(void)result;
    rb=TCE_FiniteWeatherOutside(position,view,&b);_statusfp2(&xb,&sb);
    __control87_2(0,0,&afterControl,NULL);
    ++checks;
    if(ra!=rb||a!=b||xa!=xb||sa!=sb||xa!=beforeX||sa!=beforeS||
       (afterControl&(_MCW_EM|_MCW_PC|_MCW_RC))!=(control&(_MCW_EM|_MCW_PC|_MCW_RC))) {
        if(++failures<=12)printf("MISMATCH pc/rc=%x return=%d/%d out=%d/%d status=%x,%x/%x,%x seed=%x,%x\n",control,ra,rb,a,b,xa,sa,xb,sb,beforeX,beforeS);
    }
    if(TCE_WeatherOutsideFast(position,view,&fast)) {
        ++accepted;
        if(ra&&fast!=a) {++failures;printf("FAST_BOUND_MISMATCH\n");}
    } else ++declined;
}
static void run_fastpath_tests(void)
{
    const unsigned precision[]={_PC_24,_PC_53,_PC_64};
    const unsigned rounding[]={_RC_NEAR,_RC_DOWN,_RC_UP,_RC_CHOP};
    const unsigned special[]={0,0x80000000u,1,0x80000001u,0x007fffffu,0x807fffffu,
        0x3d7fffffu,0x3d800000u,0x3d800001u,0x47800000u,0xc7800000u,
        0x47800001u,0xc7800001u,0x7f7fffffu,0xff7fffffu,0x7f800000u,
        0xff800000u,0x7fc00001u,0x7f800001u};
    unsigned pc,rc,control,i,j;float p[3],v[3];
    for(pc=0;pc<3;++pc)for(rc=0;rc<4;++rc) {
        __control87_2(_MCW_EM|precision[pc]|rounding[rc],_MCW_EM|_MCW_PC|_MCW_RC,&control,NULL);
        for(i=0;i<4000;++i) {
            unsigned prior=accepted;
            compare(positions[i],views[i],control);
            if(accepted!=prior) ++worldAccepted;else ++worldDeclined;
        }
        for(i=0;i<sizeof(special)/sizeof(special[0]);++i)for(j=0;j<sizeof(special)/sizeof(special[0]);++j) {
            p[0]=from_bits(special[i]);p[1]=from_bits(special[j]);p[2]=0;
            v[0]=from_bits(special[j]);v[1]=from_bits(special[i]);v[2]=0;
            compare(p,v,control);
        }
        /* Radius neighbours, Pythagorean boundaries and large cancellation. */
        for(i=0;i<513;++i) {
            p[0]=from_bits(0x447a0000u+i-256);p[1]=0;v[0]=v[1]=0;compare(p,v,control);
            p[0]=from_bits(0x44160000u+i-256);p[1]=800;compare(p,v,control);
            p[0]=64536;p[1]=from_bits(0x47800000u-i);v[0]=65536;v[1]=65536;compare(p,v,control);
            p[0]=-64536;p[1]=from_bits(0xc7800000u-i);v[0]=-65536;v[1]=-65536;compare(p,v,control);
        }
        for(i=0;i<12000;++i) {
            p[0]=from_bits(random_word());p[1]=from_bits(random_word());
            v[0]=from_bits(random_word());v[1]=from_bits(random_word());compare(p,v,control);
        }
    }
    /* No native floating fallback is invoked while unmasked: both actual
     * helpers must decline and leave the caller's output/control unchanged. */
    for(i=0;i<6;++i) {
        const unsigned exceptions[]={_EM_INVALID,_EM_DENORMAL,_EM_ZERODIVIDE,_EM_OVERFLOW,_EM_UNDERFLOW,_EM_INEXACT};
        int a=777,b=777,ra,rb;
        unsigned beforeX,beforeS,afterX,afterS,afterControl;
        _clearfp();__control87_2(_MCW_EM&~exceptions[i],_MCW_EM,&control,NULL);
        _statusfp2(&beforeX,&beforeS);
        p[0]=p[1]=v[0]=v[1]=0;
        ra=FrozenWeatherOutside(p,v,&a);rb=TCE_FiniteWeatherOutside(p,v,&b);
        _statusfp2(&afterX,&afterS);__control87_2(0,0,&afterControl,NULL);
        ++checks;if(ra!=0||rb!=0||a!=777||b!=777||beforeX!=afterX||beforeS!=afterS||
           (afterControl&(_MCW_EM|_MCW_PC|_MCW_RC))!=(control&(_MCW_EM|_MCW_PC|_MCW_RC)))++failures;
        __control87_2(_MCW_EM,_MCW_EM,&control,NULL);
    }
    printf("FASTPATH checks=%u failures=%u accepted=%u declined=%u world_accepted=%u world_declined=%u\n",checks,failures,accepted,declined,worldAccepted,worldDeclined);
}
#endif
int main(void)
{
    unsigned original,control;int i,repeat;
    __control87_2(0,0,&original,NULL);
    __control87_2(_MCW_EM|_PC_64|_RC_NEAR,_MCW_EM|_MCW_PC|_MCW_RC,&control,NULL);
    for(i=0;i<4000;++i) {
        int axis;
        for(axis=0;axis<3;++axis) {
            views[i][axis]=(float)((int)(random_word()%8192)-4096);
            positions[i][axis]=views[i][axis]+((int)(random_word()%35201)-17600)/16.0f;
            velocities[i][axis]=((int)(random_word()%3201)-1600)/16.0f;
        }
    }
    for(repeat=0;repeat<3;++repeat) {
        double a=time_move(1),b=time_move(0),c=time_outside(1),d=time_outside(0),e=time_outside(2);
        printf("ATTRIBUTION sample=%d n=4000 move_soft_ms=%.6f move_fallback_ms=%.6f outside_soft_ms=%.6f outside_fallback_ms=%.6f outside_frozen_ms=%.6f\n",repeat,a,b,c,d,e);
    }
#if defined(TCE_HAVE_WEATHER_FASTPATH)
    run_fastpath_tests();
#endif
    __control87_2(original,_MCW_EM|_MCW_PC|_MCW_RC,&control,NULL);
#if defined(TCE_HAVE_WEATHER_FASTPATH)
    return failures!=0;
#else
    return 0;
#endif
}
