/*! \file duplex.h
    \brief A Documented file.

    Details.
*/
#pragma once

#include "buffer.h"
#include "interface.h"
#include "timer.h"
#include <sys/poll.h>

typedef struct pollfd pfd_t;

/** Basics
 *  More details about this class.
 */
typedef struct sndx_duplex_t
{
    snd_pcm_t*         play;
    snd_pcm_t*         capt;
    sndx_duplex_ops_t* ops;

    format_t format;
    u32      rate;
    u32      period_size;
    u32      nperiods;
    u32      ch_play;
    u32      ch_capt;
    bool     linked;

    sndx_buffer_t* buf_play;
    sndx_buffer_t* buf_capt;

    struct
    {

        pfd_t* addr;
        u32    nfds;
        u32    play_nfds;
        u32    capt_nfds;

        u64 poll_next;
        u64 poll_last;
        u64 poll_late;
        u64 poll_timeout;

        u64 period_usecs;
        u64 last_wait_ust;

        u64 xrun_count;
        u64 process_count;

    } pfds;

    sndx_timer_t timer;

    output_t* out;

} sndx_duplex_t;

void sndx_dump_duplex(sndx_duplex_t* d, snd_output_t* output);
void sndx_dump_duplex_status(sndx_duplex_t* d, output_t* output);

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

sframes_t sndx_duplex_readbuf(sndx_duplex_t* d, char* buf, i64 len, uframes_t* frames, uframes_t* max);
sframes_t sndx_duplex_writebuf(sndx_duplex_t* d, char* buf, i64 len, uframes_t* frames, uframes_t* max);
int       sndx_duplex_write_initial_silence(sndx_duplex_t* d, char* play_buf, uframes_t* frames_silence);

void sndx_duplex_copy_capt_to_play(sndx_buffer_t* buf_capt, sndx_buffer_t* buf_play, sframes_t len, void* data);
