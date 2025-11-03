#pragma once

#include "types.h"
#include <math.h> // IWYU pragma: keep

// clang-format off
void sample_move_d32_sS(char* dst, float* src, u32 dst_skip, u32 src_skip, uframes_t frames);
void sample_move_d24_sS(char* dst, float* src, u32 dst_skip, u32 src_skip, uframes_t frames);
void sample_move_d16_sS(char* dst, float* src, u32 dst_skip, u32 src_skip, uframes_t frames);

void sample_move_dS_s32(float* dst, char* src, u32 dst_skip, u32 src_skip, uframes_t frames);
void sample_move_dS_s24(float* dst, char* src, u32 dst_skip, u32 src_skip, uframes_t frames);
void sample_move_dS_s16(float* dst, char* src, u32 dst_skip, u32 src_skip, uframes_t frames);
// clang-format on

/* Notes about these *_SCALING values.

   the MAX_<N>BIT values are floating point. when multiplied by
   a full-scale normalized floating point sample value (-1.0..+1.0)
   they should give the maximum value representable with an integer
   sample type of N bits. Note that this is asymmetric. Sample ranges
   for signed integer, 2's complement values are -(2^(N-1) to +(2^(N-1)-1)

   Complications
   -------------
   If we use +2^(N-1) for the scaling factors, we run into a problem:

   if we start with a normalized float value of -1.0, scaling
   to 24 bits would give -8388608 (-2^23), which is ideal.
   But with +1.0, we get +8388608, which is technically out of range.

   We never multiply a full range normalized value by this constant,
   but we could multiply it by a positive value that is close enough to +1.0
   to produce a value > +(2^(N-1)-1.

   There is no way around this paradox without wasting CPU cycles to determine
   which scaling factor to use (i.e. determine if its negative or not,
   use the right factor).

   So, for now (October 2008) we use 2^(N-1)-1 as the scaling factor.
*/

#define SAMPLE_32BIT_SCALING 2147483647.0
#define SAMPLE_24BIT_SCALING 8388607.0f
#define SAMPLE_16BIT_SCALING 32767.0f

/* these are just values to use if the floating point value was out of range

   advice from Fons Adriaensen: make the limits symmetrical
 */

#define SAMPLE_32BIT_MAX   2147483647
#define SAMPLE_32BIT_MIN   -2147483647
#define SAMPLE_32BIT_MAX_D 2147483647.0
#define SAMPLE_32BIT_MIN_D -2147483647.0

#define SAMPLE_24BIT_MAX   8388607
#define SAMPLE_24BIT_MIN   -8388607
#define SAMPLE_24BIT_MAX_F 8388607.0f
#define SAMPLE_24BIT_MIN_F -8388607.0f

#define SAMPLE_16BIT_MAX   32767
#define SAMPLE_16BIT_MIN   -32767
#define SAMPLE_16BIT_MAX_F 32767.0f
#define SAMPLE_16BIT_MIN_F -32767.0f

/* these mark the outer edges of the range considered "within" range
   for a floating point sample value. values outside (and on the boundaries)
   of this range will be clipped before conversion; values within this
   range will be scaled to appropriate values for the target sample
   type.
*/

#define NORMALIZED_FLOAT_MIN -1.0f
#define NORMALIZED_FLOAT_MAX 1.0f

/* define this in case we end up on a platform that is missing
   the real lrintf functions
*/

#define f_round(f) lrintf(f)
#define d_round(f) lrint(f)

#define float_16(s, d)                                                                                                 \
    if ((s) <= NORMALIZED_FLOAT_MIN) { (d) = SAMPLE_16BIT_MIN; }                                                       \
    else if ((s) >= NORMALIZED_FLOAT_MAX) { (d) = SAMPLE_16BIT_MAX; }                                                  \
    else {                                                                                                             \
        (d) = f_round((s) * SAMPLE_16BIT_SCALING);                                                                     \
    }

/* call this when "s" has already been scaled (e.g. when dithering)
 */

#define float_16_scaled(s, d)                                                                                          \
    if ((s) <= SAMPLE_16BIT_MIN_F) { (d) = SAMPLE_16BIT_MIN_F; }                                                       \
    else if ((s) >= SAMPLE_16BIT_MAX_F) { (d) = SAMPLE_16BIT_MAX; }                                                    \
    else {                                                                                                             \
        (d) = f_round((s));                                                                                            \
    }

#define float_24u32(s, d)                                                                                              \
    if ((s) <= NORMALIZED_FLOAT_MIN) { (d) = SAMPLE_24BIT_MIN << 8; }                                                  \
    else if ((s) >= NORMALIZED_FLOAT_MAX) { (d) = SAMPLE_24BIT_MAX << 8; }                                             \
    else {                                                                                                             \
        (d) = f_round((s) * SAMPLE_24BIT_SCALING) << 8;                                                                \
    }

#define float_24l32(s, d)                                                                                              \
    if ((s) <= NORMALIZED_FLOAT_MIN) { (d) = SAMPLE_24BIT_MIN; }                                                       \
    else if ((s) >= NORMALIZED_FLOAT_MAX) { (d) = SAMPLE_24BIT_MAX; }                                                  \
    else {                                                                                                             \
        (d) = f_round((s) * SAMPLE_24BIT_SCALING);                                                                     \
    }

#define float_32(s, d)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        double clipped = fmin(NORMALIZED_FLOAT_MAX, fmax((double)(s), NORMALIZED_FLOAT_MIN));                          \
        double scaled  = clipped * SAMPLE_32BIT_MAX_D;                                                                 \
        (d)            = d_round(scaled);                                                                              \
    } while (0)

/* call this when "s" has already been scaled (e.g. when dithering)
 */

#define float_24u32_scaled(s, d)                                                                                       \
    if ((s) <= SAMPLE_24BIT_MIN_F) { (d) = SAMPLE_24BIT_MIN << 8; }                                                    \
    else if ((s) >= SAMPLE_24BIT_MAX_F) { (d) = SAMPLE_24BIT_MAX << 8; }                                               \
    else {                                                                                                             \
        (d) = f_round((s)) << 8;                                                                                       \
    }

#define float_24(s, d)                                                                                                 \
    if ((s) <= NORMALIZED_FLOAT_MIN) { (d) = SAMPLE_24BIT_MIN; }                                                       \
    else if ((s) >= NORMALIZED_FLOAT_MAX) { (d) = SAMPLE_24BIT_MAX; }                                                  \
    else {                                                                                                             \
        (d) = f_round((s) * SAMPLE_24BIT_SCALING);                                                                     \
    }

/* call this when "s" has already been scaled (e.g. when dithering)
 */

#define float_24_scaled(s, d)                                                                                          \
    if ((s) <= SAMPLE_24BIT_MIN_F) { (d) = SAMPLE_24BIT_MIN; }                                                         \
    else if ((s) >= SAMPLE_24BIT_MAX_F) { (d) = SAMPLE_24BIT_MAX; }                                                    \
    else {                                                                                                             \
        (d) = f_round((s));                                                                                            \
    }
