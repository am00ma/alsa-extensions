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

        char* data;

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

int sndx_buffer_read_from_area(format_t format);
int sndx_buffer_write_to_area(format_t format);
