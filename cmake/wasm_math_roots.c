#include <math.h>

/* Pull just these long-double functions and their private dependencies from
 * the SDK libc during the relocatable link. The engine owns the rest of libc. */
static long double (*const unary_math[])(long double) __attribute__((used)) = {
    sqrtl, floorl, sinl, cosl
};
static long double (*const binary_math)(long double, long double)
    __attribute__((used)) = atan2l;
