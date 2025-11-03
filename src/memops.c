#include "memops.h"

void sample_move_d32_sS(char* dst, float* src, u32 dst_skip, u32 src_skip, uframes_t frames)
{
    while (frames--)
    {
        float_32(*src, *(int32_t*)dst);
        dst += dst_skip;
        src += src_skip;
    }
}

void sample_move_dS_s32(float* dst, char* src, u32 dst_skip, u32 src_skip, uframes_t frames)
{
    const double scaling = 1.0 / SAMPLE_32BIT_SCALING;
    while (frames--)
    {
        int32_t val      = (*((int32_t*)src));
        double  extended = val * scaling;
        *dst             = (float)extended;

        dst += dst_skip;
        src += src_skip;
    }
}

void sample_move_d16_sS(char* dst, float* src, u32 dst_skip, u32 src_skip, uframes_t frames)
{
    while (frames--)
    {
        float_16(*src, *((int16_t*)dst));
        dst += dst_skip;
        src += src_skip;
    }
}

void sample_move_dS_s16(float* dst, char* src, u32 dst_skip, u32 src_skip, uframes_t frames)
{
    const double scaling = 1.0 / SAMPLE_16BIT_SCALING;
    while (frames--)
    {
        *dst  = (*((short*)src)) * scaling;
        dst  += dst_skip;
        src  += src_skip;
    }
}

void sample_move_d24_sS(char* dst, float* src, u32 dst_skip, u32 src_skip, uframes_t frames)
{
    int32_t z;
    while (frames--)
    {
        float_24(*src, z);
#if __BYTE_ORDER == __LITTLE_ENDIAN
        memcpy(dst, &z, 3);
#elif __BYTE_ORDER == __BIG_ENDIAN
        memcpy(dst, (char*)&z + 1, 3);
#endif
        dst += dst_skip;
        src += src_skip;
    }
}

void sample_move_dS_s24(float* dst, char* src, u32 dst_skip, u32 src_skip, uframes_t frames)
{
    const float scaling = 1.f / SAMPLE_24BIT_SCALING;
    while (frames--)
    {
        int x;
#if __BYTE_ORDER == __LITTLE_ENDIAN
        memcpy((char*)&x + 1, src, 3);
#elif __BYTE_ORDER == __BIG_ENDIAN
        memcpy(&x, src, 3);
#endif
        x    >>= 8;
        *dst   = x * scaling;
        dst   += dst_skip;
        src   += src_skip;
    }
}
