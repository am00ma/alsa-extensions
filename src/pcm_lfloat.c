#include "sndx/pcm_lfloat.h"

int snd_pcm_linear_get_index(format_t src_format, format_t dst_format)
{
    int sign, width, pwidth, endian;
    sign = (snd_pcm_format_signed(src_format) != snd_pcm_format_signed(dst_format));
#ifdef SND_LITTLE_ENDIAN
    endian = snd_pcm_format_big_endian(src_format);
#else
    endian = snd_pcm_format_little_endian(src_format);
#endif
    if (endian < 0) endian = 0;
    pwidth = snd_pcm_format_physical_width(src_format);
    width  = snd_pcm_format_width(src_format);
    if (pwidth == 24)
    {
        switch (width)
        {
        case 24: width = 0; break;
        case 20: width = 1; break;
        case 18:
        default: width = 2; break;
        }
        return width * 4 + endian * 2 + sign + 20;
    }
    else
    {
        if (width == 20) width = 40;

        width = width / 8 - 1;
        return width * 4 + endian * 2 + sign;
    }
}

int snd_pcm_linear_put_index(format_t src_format, format_t dst_format)
{
    int sign, width, pwidth, endian;
    sign = (snd_pcm_format_signed(src_format) != snd_pcm_format_signed(dst_format));
#ifdef SND_LITTLE_ENDIAN
    endian = snd_pcm_format_big_endian(dst_format);
#else
    endian = snd_pcm_format_little_endian(dst_format);
#endif
    if (endian < 0) endian = 0;
    pwidth = snd_pcm_format_physical_width(dst_format);
    width  = snd_pcm_format_width(dst_format);
    if (pwidth == 24)
    {
        switch (width)
        {
        case 24: width = 0; break;
        case 20: width = 1; break;
        case 18:
        default: width = 2; break;
        }
        return width * 4 + endian * 2 + sign + 20;
    }
    else
    {
        if (width == 20) width = 40;

        width = width / 8 - 1;
        return width * 4 + endian * 2 + sign;
    }
}

int snd_pcm_lfloat_get_s32_index(format_t format)
{
    int width, endian;

    switch (format)
    {
    case SND_PCM_FORMAT_FLOAT_LE:
    case SND_PCM_FORMAT_FLOAT_BE: width = 32; break;
    case SND_PCM_FORMAT_FLOAT64_LE:
    case SND_PCM_FORMAT_FLOAT64_BE: width = 64; break;
    default: return -EINVAL;
    }
#ifdef SND_LITTLE_ENDIAN
    endian = snd_pcm_format_big_endian(format);
#else
    endian = snd_pcm_format_little_endian(format);
#endif
    return ((width / 32) - 1) * 2 + endian;
}

int snd_pcm_lfloat_put_s32_index(format_t format) { return snd_pcm_lfloat_get_s32_index(format); }

void snd_pcm_lfloat_convert_integer_float(const area_t* dst_areas,
                                          uframes_t     dst_offset,
                                          const area_t* src_areas,
                                          uframes_t     src_offset,
                                          unsigned int  channels,
                                          uframes_t     frames,
                                          unsigned int  get32idx,
                                          unsigned int  put32floatidx)
{
#define GET32_LABELS
#define PUT32F_LABELS
#include "sndx/plugin_ops.h"
#undef PUT32F_LABELS
#undef GET32_LABELS
    void*        get32      = get32_labels[get32idx];
    void*        put32float = put32float_labels[put32floatidx];
    unsigned int channel;
    for (channel = 0; channel < channels; ++channel)
    {
        const char*      src;
        char*            dst;
        int              src_step, dst_step;
        uframes_t        frames1;
        int32_t          sample = 0;
        snd_tmp_float_t  tmp_float;
        snd_tmp_double_t tmp_double;
        const area_t*    src_area = &src_areas[channel];
        const area_t*    dst_area = &dst_areas[channel];
        src                       = snd_pcm_channel_area_addr(src_area, src_offset);
        dst                       = snd_pcm_channel_area_addr(dst_area, dst_offset);
        src_step                  = snd_pcm_channel_area_step(src_area);
        dst_step                  = snd_pcm_channel_area_step(dst_area);
        frames1                   = frames;
        while (frames1-- > 0)
        {
            goto* get32;
#define GET32_END sample_loaded
#include "sndx/plugin_ops.h"
#undef GET32_END
        sample_loaded:
            goto* put32float;
#define PUT32F_END sample_put
#include "sndx/plugin_ops.h"
#undef PUT32F_END
        sample_put:
            src += src_step;
            dst += dst_step;
        }
    }
}

void snd_pcm_lfloat_convert_float_integer(const area_t* dst_areas,
                                          uframes_t     dst_offset,
                                          const area_t* src_areas,
                                          uframes_t     src_offset,
                                          unsigned int  channels,
                                          uframes_t     frames,
                                          unsigned int  put32idx,
                                          unsigned int  get32floatidx)
{
#define PUT32_LABELS
#define GET32F_LABELS
#include "sndx/plugin_ops.h"
#undef GET32F_LABELS
#undef PUT32_LABELS
    void*        put32      = put32_labels[put32idx];
    void*        get32float = get32float_labels[get32floatidx];
    unsigned int channel;
    for (channel = 0; channel < channels; ++channel)
    {
        const char*      src;
        char*            dst;
        int              src_step, dst_step;
        uframes_t        frames1;
        int32_t          sample = 0;
        snd_tmp_float_t  tmp_float;
        snd_tmp_double_t tmp_double;
        const area_t*    src_area = &src_areas[channel];
        const area_t*    dst_area = &dst_areas[channel];
        src                       = snd_pcm_channel_area_addr(src_area, src_offset);
        dst                       = snd_pcm_channel_area_addr(dst_area, dst_offset);
        src_step                  = snd_pcm_channel_area_step(src_area);
        dst_step                  = snd_pcm_channel_area_step(dst_area);
        frames1                   = frames;
        while (frames1-- > 0)
        {
            goto* get32float;
#define GET32F_END sample_loaded
#include "sndx/plugin_ops.h"
#undef GET32F_END
        sample_loaded:
            goto* put32;
#define PUT32_END sample_put
#include "sndx/plugin_ops.h"
#undef PUT32_END
        sample_put:
            src += src_step;
            dst += dst_step;
        }
    }
}
