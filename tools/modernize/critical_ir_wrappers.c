/* Compiler-comparison scalar slices. No gameplay implementation is replaced.
 * Actual helper/controller bodies and old slices are inserted by the runner. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "src/game/q_shared.h"
#include "compat/legacy/game/tce_ratio_precision.h"
#include "compat/legacy/game/tce_weather_precision.h"
#include "compat/legacy/cgame/tce_player_animation_precision.h"
#include "compat/legacy/ui/tce_ui_text_precision.h"
#include "tools/modernize/compat_word_oracle.h"

/* ACTUAL_FUNCTIONS */

#define IR_NOINLINE __declspec(noinline)
IR_NOINLINE void ir_old_place(tceTriggerSum_t *v, unsigned long long n, int s) { Oracle_TCE_TriggerPlace(v,n,s); }
IR_NOINLINE void ir_new_place(tceTriggerSum_t *v, unsigned long long n, int s) { TCE_TriggerPlace(v,n,s); }
IR_NOINLINE void ir_old_round(tceTriggerSum_t *v, unsigned c) { Oracle_TCE_TriggerRound(v,c); }
IR_NOINLINE void ir_new_round(tceTriggerSum_t *v, unsigned c) { TCE_TriggerRound(v,c); }
IR_NOINLINE float ir_old_store(tceTriggerSum_t v, unsigned c) { return Oracle_TCE_RotateStore(v,c); }
IR_NOINLINE float ir_new_store(tceTriggerSum_t v, unsigned c) { return TCE_RotateStore(v,c); }
IR_NOINLINE tceTriggerSum_t ir_old_ratio(unsigned long long n,unsigned long long d,int s,unsigned c) { return IR_OldRatioValue(n,d,s,0,c); }
IR_NOINLINE tceTriggerSum_t ir_new_ratio(unsigned long long n,unsigned long long d,int s,unsigned c) { return TCE_RatioValue(n,d,s,0,c); }
IR_NOINLINE int ir_old_divide(const tceTriggerSum_t *v,float d,unsigned c,tceTriggerSum_t *r) { return IR_OldFixedDivideFloat(v,d,c,r); }
IR_NOINLINE int ir_new_divide(const tceTriggerSum_t *v,float d,unsigned c,tceTriggerSum_t *r) { return TCE_FixedDivideFloat(v,d,c,r); }
IR_NOINLINE int ir_old_highest(unsigned long long n) { unsigned long long probe; int numeratorBit=0; /* BASELINE_BIT_LOOP */ return numeratorBit; }
IR_NOINLINE int ir_new_highest(unsigned long long n) { return TCE_RatioHighestBit(n); }
IR_NOINLINE int ir_old_weather(const float *p,const float *v,int *o) { return IR_OldWeatherOutside(p,v,o); }
IR_NOINLINE int ir_new_weather(const float *p,const float *v,int *o) { return TCE_FiniteWeatherOutside(p,v,o); }
/* This pair records a partial certifier: a declined new call is not a miss. */
IR_NOINLINE int ir_old_weather_fast(const float *p,const float *v,int *o) { return IR_OldWeatherOutside(p,v,o); }
IR_NOINLINE int ir_new_weather_fast(const float *p,const float *v,int *o) { return TCE_WeatherOutsideFast(p,v,o); }
IR_NOINLINE float ir_old_backlerp(int e,int d) { /* BASELINE_BACKLERP */ }
IR_NOINLINE float ir_new_backlerp(int e,int d) { return TCE_PlayerAnimationBacklerp(e,d); }
IR_NOINLINE float ir_old_speed(const float *a,const float *b,int e,int s) { /* BASELINE_SPEED */ }
IR_NOINLINE float ir_new_speed(const float *a,const float *b,int e,int s) { return TCE_PlayerAnimationSpeedScale(a,b,e,s); }
IR_NOINLINE float ir_old_radar(const float *d) { return VectorLength(d); }
IR_NOINLINE float ir_new_radar(const float *d) { return (float)TCE_VectorLengthValue(d); }
IR_NOINLINE float ir_old_ui(float t,int w,float x) { /* BASELINE_UI */ }
IR_NOINLINE float ir_new_ui(float t,int w,float x) { return TCE_UiDescriptionX(t,w,x); }

static unsigned bits(float v) { unsigned n; memcpy(&n,&v,4); return n; }
int main(void)
{
    tceTriggerSum_t a={{0},0},b,c; unsigned control,original;
    volatile unsigned long long large=0xc999998400000000ULL;
    const float from[3]={0,0,0},to[3]={3,4,0},far[3]={1100,0,0};
    int x=-1,y=-1,checks=0,failures=0,observed=0;
#define CHECK(v) do { ++checks; if(!(v)) ++failures; } while(0)
    __control87_2(0,0,&original,NULL);
    __control87_2(_MCW_EM|_PC_53|_RC_NEAR,_MCW_EM|_MCW_PC|_MCW_RC,&control,NULL);
    b=a;ir_old_place(&a,7,296);ir_new_place(&b,7,296);CHECK(!memcmp(&a,&b,sizeof(a)));
    c=b;ir_old_round(&b,control);ir_new_round(&c,control);CHECK(!memcmp(&b,&c,sizeof(b)));
    CHECK(bits(ir_old_store(b,control))==bits(ir_new_store(c,control)));
    b=ir_new_ratio(large,9437184u,-41,control);CHECK(bits(ir_new_store(b,control))==0x3f333333u);
    c=ir_old_ratio(large,9437184u,-41,control);observed+=memcmp(&b,&c,sizeof(b))!=0;
    CHECK(ir_new_divide(&a,2.0f,control,&b)&&bits(ir_new_store(b,control))==0x3f600000u);
    if(ir_old_divide(&a,2.0f,control,&c))observed+=memcmp(&b,&c,sizeof(b))!=0;
    CHECK(ir_new_highest(large)==63);observed+=ir_old_highest(large)!=63;
    CHECK(ir_old_weather(far,from,&x)&&ir_new_weather(far,from,&y)&&x==1&&y==1);
    x=y=-1;CHECK(ir_old_weather_fast(far,from,&x)&&ir_new_weather_fast(far,from,&y)&&x==1&&y==1);
    CHECK(bits(ir_new_backlerp(1,3))==0x3f2aaaabu);observed+=bits(ir_old_backlerp(1,3))!=bits(ir_new_backlerp(1,3));
    CHECK(bits(ir_new_speed(from,to,100,50))==0x3f800000u);observed+=bits(ir_old_speed(from,to,100,50))!=0x3f800000u;
    CHECK(bits(ir_new_radar(to))==0x40a00000u);observed+=bits(ir_old_radar(to))!=0x40a00000u;
    CHECK(bits(ir_new_ui(0.1f,100,100.0f))==bits(0.1f));observed+=bits(ir_old_ui(0.1f,100,100.0f))!=bits(0.1f);
    __control87_2(original,_MCW_EM|_MCW_PC|_MCW_RC,&control,NULL);
    printf("{\"checks\":%d,\"failures\":%d,\"observed_legacy_mismatches\":%d}\n",checks,failures,observed);
    return failures!=0;
}
