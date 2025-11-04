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

} buffer_t;

void dump_buffer(buffer_t* b, snd_output_t* output);

int  buffer_setup(buffer_t** bufp, fmt_t format, u32 channels, uframes_t frames, snd_output_t* output);
void buffer_destroy(buffer_t* b);

void buffer_map_dev_to_samples(buffer_t* b, char* samples);
