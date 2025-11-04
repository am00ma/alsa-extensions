#pragma once

#include "types.h"

typedef struct
{
    format_t  format;
    u32       bytes;
    u32       channels;
    uframes_t frames;

    area_t* dev;  // Interleaved, backed by device
    area_t* buf;  // Non-interleaved, backed by `float* data`
    float*  data; // Backing for buf [frames * channels]

} sndx_buffer_t;

// Debugging
void sndx_dump_buffer(sndx_buffer_t* b, snd_output_t* output);
void sndx_dump_buffer_areas(sndx_buffer_t* b, uframes_t offset, uframes_t frames, snd_output_t* output);

// Lifetime
int  sndx_buffer_open(sndx_buffer_t** bufp, format_t format, u32 channels, uframes_t frames, snd_output_t* output);
void sndx_buffer_close(sndx_buffer_t* b);

// buffer <-> device
void sndx_buffer_buf_to_dev(sndx_buffer_t* b, uframes_t offset, uframes_t frames);
void sndx_buffer_dev_to_buf(sndx_buffer_t* b, uframes_t offset, uframes_t frames);

// Helper for testing
void sndx_buffer_map_dev_to_samples(sndx_buffer_t* b, char* samples);
