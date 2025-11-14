/** @file latency.c
 *  @brief Implementation of `latency.c` with sndx_duplex
 *
 *  Seems like simplest possible implementation.
 */
#include "latency.h"

int app_open(sndx_duplex_t* d,
             const char*    playback_device,
             const char*    capture_device,
             sndx_params_t* params,
             void*          data,
             output_t*      output)
{
    int err = sndx_duplex_open(          //
        &d,                              //
        playback_device, capture_device, //
        params->format,                  //
        params->rate,                    //
        params->buffer_size,             //
        params->period_size,             //
        params->access,                  //
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    app_data_t* adata = data;
    adata->gain       = 0.5;

    // Allocate samples temporarily
    adata->capt_size = d->buf_capt->frames * d->buf_capt->channels * d->buf_capt->bytes;
    adata->capt_buf  = calloc(adata->capt_size, sizeof(char));

    adata->play_size = d->buf_play->frames * d->buf_play->channels * d->buf_play->bytes;
    adata->play_buf  = calloc(adata->play_size, sizeof(char));

    sndx_buffer_map_dev_to_samples(d->buf_play, adata->play_buf);
    sndx_buffer_map_dev_to_samples(d->buf_capt, adata->capt_buf);

    return 0;
}

int app_close(sndx_duplex_t* d, void* data)
{
    output_t*   output = d->out;
    app_data_t* adata  = data;

    if (adata->capt_buf)
    {
        free(adata->capt_buf);
        adata->capt_buf = nullptr;
    }
    if (adata->play_buf)
    {
        free(adata->play_buf);
        adata->play_buf = nullptr;
    }

    int err = sndx_duplex_close(d);
    SndCheck_(err, "Failed sndx_duplex_close: %s");
    return 0;
}

int app_start(sndx_duplex_t* d, void* data)
{
    int         err;
    output_t*   output = d->out;
    app_data_t* adata  = data;

    // Set silence
    uframes_t frames_silence = 0;

    err = sndx_duplex_write_initial_silence(d, adata->play_buf, &frames_silence);
    SndFatal_(err, "Failed sndx_duplex_write_initial_silence: %s");

    return 0;
}

int app_stop(sndx_duplex_t* d, void*)
{
    snd_pcm_drop(d->capt);
    snd_pcm_drain(d->play);
    snd_pcm_nonblock(d->play, SND_PCM_NONBLOCK);

    snd_pcm_unlink(d->capt);
    snd_pcm_hw_free(d->play);
    snd_pcm_hw_free(d->capt);

    return 0;
}

int app_restart(sndx_duplex_t*, void*) { return 0; }

int app_wait(sndx_duplex_t* d, void*)
{
    // TODO: poll_timeout
    // snd_pcm_wait(d->capt, d->pfds.poll_timeout);
    snd_pcm_wait(d->capt, 1000);
    return 0;
}

int app_read( //
    sndx_duplex_t* d,
    uframes_t      capt_avail,
    uframes_t*     frames_in,
    uframes_t*     frames_read,
    uframes_t*     in_max,
    void*          data)
{
    int         err;
    output_t*   output = d->out;
    app_data_t* adata  = data;

    err = sndx_duplex_readbuf(d, adata->capt_buf, capt_avail, 0, frames_in, in_max);
    SndReturn_(err, "Failed readbuf: %s");

    // Read based on what snd_pcm_read.. returns
    // May be less than frames asked to write
    *frames_read = err;

    return 0;
}

int app_write( //
    sndx_duplex_t* d,
    uframes_t      play_avail,
    uframes_t*     frames_out,
    uframes_t*     frames_written,
    uframes_t*     out_max,
    void*          data)
{
    int         err;
    output_t*   output = d->out;
    app_data_t* adata  = data;

    err = sndx_duplex_writebuf(d, adata->play_buf, play_avail, 0, frames_out, out_max);
    SndReturn_(err, "Failed writebuf: %s");

    // Always write whatever was asked
    *frames_written = *frames_out;

    return 0;
}

int app_callback(sndx_duplex_t* d, sframes_t /* offset */, sframes_t frames, void* data)
{
    app_data_t* adata = data;

    // Copy first capture channel to all playback channels
    sndx_duplex_copy_capt_to_play(d->buf_capt, d->buf_play, frames, &adata->gain);

    return 0;
}

// Application
int main()
{
    int err; ///< Brief description after the member

    output_t* output;
    err = snd_output_stdio_attach(&output, stderr, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    snd_output_close(output);

    return 0;
}
