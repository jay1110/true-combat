#ifndef TCE_COMPAT_VECTOR_H
#define TCE_COMPAT_VECTOR_H

/* Class C: the reconstructed wide-return helper is implemented only by the
 * existing Windows/Linux q_math paths. Other targets retain the SDK's public
 * Float normalization and widen its result for TC-specific effect consumers.
 * This adapter changes no SDK function or vector arithmetic and makes no
 * x87-precision claim for targets without that implementation. Include the
 * SDK declarations before using this adapter. */
#if !defined(_WIN32) && !defined(__linux__)
#define TCE_VectorNormalizeValue(vector) ((long double)VectorNormalize(vector))
#endif

#endif
