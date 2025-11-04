#pragma once

#include "types.h"

typedef struct
{
    snd_pcm_t* play;
    snd_pcm_t* capt;

    bool     linked;
    format_t format;
    u32      rate;
    u32      period_size;
    u32      nperiods;
    u32      ch_play;
    u32      ch_capt;

    output_t* out;

} sndx_duplex_t;

void sndx_dump_duplex(sndx_duplex_t* d, snd_output_t* output);

int sndx_duplex_open(                //
    sndx_duplex_t** duplexp,         //
    const char*     playback_device, //
    const char*     capture_device,  //
    format_t        format,          //
    u32             rate,            //
    uframes_t       buffer_size,     //
    uframes_t       period_size,     //
    access_t        _access,         //
    snd_output_t*   output);

int sndx_duplex_close(sndx_duplex_t* d);
