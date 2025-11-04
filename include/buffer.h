#pragma once

#include "types.h"

typedef snd_pcm_format_t fmt_t;

typedef struct
{
    fmt_t     format;
    u32       bytes;
    u32       channels;
    uframes_t frames;

    area_t* dev;  // Interleaved
    area_t* buf;  // Non-interleaved
    float*  data; // bare data

} sndx_buffer_t;

void sndx_dump_buffer(sndx_buffer_t* b, snd_output_t* output);
void sndx_dump_buffer_areas(sndx_buffer_t* b, uframes_t offset, uframes_t frames, snd_output_t* output);

int  sndx_buffer_open(sndx_buffer_t** bufp, fmt_t format, u32 channels, uframes_t frames, snd_output_t* output);
void sndx_buffer_close(sndx_buffer_t* b, snd_output_t* output);

void sndx_buffer_map_dev_to_samples(sndx_buffer_t* b, char* samples);
