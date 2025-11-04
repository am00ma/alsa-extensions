#pragma once

#include "types.h"

#define MAX_RETRY_COUNT 5

typedef struct pollfd pfd_t;

typedef struct
{
    pfd_t* pfds;
    u32    nfds;

    u32 play_nfds;
    u32 capt_nfds;

} sndx_pollfds_t;

int sndx_pollfds_open(sndx_pollfds_t** fdsp, snd_pcm_t* play, snd_pcm_t* capt, output_t* output);

void sndx_pollfds_close(sndx_pollfds_t* obj);

int sndx_pollfds_wait_jack2(       //
    snd_pcm_t*      capt,          //
    snd_pcm_t*      play,          //
    sndx_pollfds_t* poll_fds,      //
    int             poll_timeout,  //
    int             period_size,   //
    int*            status,        //
    float*          delayed_usecs, //
    output_t*       output);
