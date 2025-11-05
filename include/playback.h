#pragma once

#include "buffer.h"
#include "timer.h"

typedef struct
{
    snd_pcm_t* play;

    format_t format;
    u32      rate;
    u32      period_size;
    u32      nperiods;
    u32      ch_play;

    sndx_buffer_t* buf_play;

    sndx_timer_t timer;

    output_t* out;

} sndx_playback_t;

void sndx_dump_playback(sndx_playback_t* d, snd_output_t* output);
void sndx_dump_playback_status(sndx_playback_t* d, output_t* output);

int sndx_playback_open(                //
    sndx_playback_t** playbackp,       //
    const char*       playback_device, //
    format_t          format,          //
    u32               rate,            //
    uframes_t         buffer_size,     //
    uframes_t         period_size,     //
    access_t          _access,         //
    snd_output_t*     output);
int sndx_playback_close(sndx_playback_t* d);

sframes_t sndx_playback_writebuf(sndx_playback_t* d, char* buf, long len, size_t* frames);

int sndx_playback_write_initial_silence(sndx_playback_t* d, char* play_buf, uframes_t* frames_silence);
int sndx_playback_start(sndx_playback_t* d, char** play_bufp, uframes_t loop_limit);
int sndx_playback_stop(sndx_playback_t* d, char* play_buf);

void sndx_playback_timer_start(sndx_playback_t* d);
void sndx_playback_timer_stop(sndx_playback_t* d, uframes_t frames_in, output_t* output);

void sndx_playback_callback(sndx_buffer_t* buf_play, sframes_t len, void* data);
