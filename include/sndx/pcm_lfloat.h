#pragma once

#include "sndx/plugin_ops.h"
#include "sndx/types.h"

typedef union snd_tmp_float {
    f32 f;
    i32 i;
} snd_tmp_float_t;

typedef union snd_tmp_double {
    f64 d;
    i64 l;
} snd_tmp_double_t;

int snd_pcm_linear_get_index(format_t src_format, format_t dst_format);
int snd_pcm_linear_put_index(format_t src_format, format_t dst_format);
int snd_pcm_lfloat_get_s32_index(format_t format);
int snd_pcm_lfloat_put_s32_index(format_t format);

void snd_pcm_lfloat_convert_integer_float( //
    const area_t* dst_areas,
    uframes_t     dst_offset,
    const area_t* src_areas,
    uframes_t     src_offset,
    unsigned int  channels,
    uframes_t     frames,
    unsigned int  get32idx,
    unsigned int  put32floatidx);

void snd_pcm_lfloat_convert_float_integer( //
    const area_t* dst_areas,
    uframes_t     dst_offset,
    const area_t* src_areas,
    uframes_t     src_offset,
    unsigned int  channels,
    uframes_t     frames,
    unsigned int  put32idx,
    unsigned int  get32floatidx);
