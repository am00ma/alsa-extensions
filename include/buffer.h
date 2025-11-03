#pragma once

#include "types.h"

typedef struct
{
    u32       channels;
    u32       bytes;
    bool      interleaved;
    uframes_t frames;

    area_t* areas;
    char**  addrs;
    u32*    skips;

    struct
    {
        area_t* areas;
        char**  addrs;
        u32*    skips;
        u32     bytes;

        float* data;

    } buf;

} sndx_buffer_t;

constexpr format_t sndx_buffer_format_t = SND_PCM_FORMAT_FLOAT_LE;

void sndx_dump_buffer(sndx_buffer_t* buffer, output_t* output);

int sndx_buffer_open(            //
    sndx_buffer_t** bufferp,     //
    u32             channels,    //
    format_t        format,      //
    uframes_t       maxframes,   //
    bool            interleaved, //
    output_t*       output);

int sndx_buffer_close(sndx_buffer_t* b);

int sndx_buffer_samples_alloc(sndx_buffer_t* buf, char** samplesp, output_t* output);
int sndx_buffer_samples_free(char* samples);

int sndx_buffer_to_buf_from_area(sndx_buffer_t* b, uframes_t offset, uframes_t frames, format_t format);
int sndx_buffer_to_area_from_buf(sndx_buffer_t* b, uframes_t offset, uframes_t frames, format_t format);
