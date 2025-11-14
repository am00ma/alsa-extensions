/** @file latency.h
 *  @brief API of `latency.c` with sndx_duplex
 *
 *  Mapping `latency.c` program to duplex interface.
 *
 *  Uses `snd_pcm_wait`.
 */
#pragma once

#include "duplex.h"
#include "interface.h"
#include "params.h"

typedef struct
{
    // Temorary buffers for capture and play
    char* capt_buf;
    isize capt_size;
    char* play_buf;
    isize play_size;

    // Read write loop
    sframes_t play_avail;
    sframes_t capt_avail;

    // Total frames in/out
    uframes_t play_frames;
    uframes_t capt_frames;

    // Max size of in/out 'avail'
    uframes_t play_max;
    uframes_t capt_max;

    // User data
    float gain;

} app_data_t;

int app_open(sndx_duplex_t* d,
             const char*    playback_device,
             const char*    capture_device,
             sndx_params_t* params,
             void*          data,
             output_t*      output);
int app_close(sndx_duplex_t* d, void* data);

int app_start(sndx_duplex_t* d, void* data);
int app_stop(sndx_duplex_t* d, void* data);
int app_restart(sndx_duplex_t* d, void* data);

int app_wait(sndx_duplex_t* d, void* data);

int app_read( //
    sndx_duplex_t* d,
    uframes_t      capt_avail,
    uframes_t*     frames_in,
    uframes_t*     frames_read,
    uframes_t*     in_max,
    void*          data);

int app_write( //
    sndx_duplex_t* d,
    uframes_t      play_avail,
    uframes_t*     frames_out,
    uframes_t*     frames_written,
    uframes_t*     out_max,
    void*          data);

int app_callback(sndx_duplex_t* d, sframes_t /* offset */, sframes_t frames, void* data);

const sndx_duplex_ops_t app_ops = {
    .open_fn     = app_open,    //
    .close_fn    = app_close,   //
    .start_fn    = app_start,   //
    .stop_fn     = app_stop,    //
    .restart_fn  = app_restart, //
    .wait_fn     = app_wait,    //
    .read_fn     = app_read,    //
    .write_fn    = app_write,   //
    .callback_fn = app_callback //
};
