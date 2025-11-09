/*! \file latency.c
    \brief A Documented file.

    Details.
*/

#include "duplex.h"
#include "types.h"

typedef struct
{
    // Temorary buffers for capture and play
    char* capt_buf;
    isize capt_size;
    char* play_buf;
    isize play_size;

    // Timing for loop
    uframes_t frames_silence;
    uframes_t frames_out;
    uframes_t frames_in;
    uframes_t in_max;

    // Read write loop
    sframes_t r, cap_avail;
    float     gain;

} app_data_t;

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
    snd_pcm_wait(d->capt, d->pfds.poll_timeout);
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

    err = sndx_duplex_readbuf(d, adata->capt_buf, adata->cap_avail, &adata->frames_in, &adata->in_max);
    SndReturn_(err, "Failed readbuf: %s");

    // Frames read
    adata->r = err;

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

    err = sndx_duplex_writebuf(d, adata->play_buf, adata->r, &adata->frames_out, out_max);
    SndReturn_(err, "Failed writebuf: %s");

    return 0;
}

int app_callback(sndx_duplex_t* d, sframes_t /* offset */, sframes_t frames, void* data)
{
    float* gain = data;
    // Copy capture channels to playback
    sndx_duplex_copy_capt_to_play(d->buf_capt, d->buf_play, frames, &gain);
    return 0;
}

sndx_duplex_ops_t app_ops = {
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

// Form of start and stop in this program
int sndx_duplex_start(sndx_duplex_t* d, char** play_bufp, char** capt_bufp, uframes_t loop_limit);
int sndx_duplex_stop(sndx_duplex_t* d, char* play_buf, char* capt_buf);

// Implementation
int sndx_duplex_start(sndx_duplex_t* d, char** play_bufp, char** capt_bufp, uframes_t loop_limit)
{
    int err;

    output_t* output = d->out;

    // Allocate samples temporarily
    isize capt_size = d->buf_capt->frames * d->buf_capt->channels * d->buf_capt->bytes;
    char* capt_buf  = calloc(capt_size, sizeof(char));
    *capt_bufp      = capt_buf;

    isize play_size = d->buf_play->frames * d->buf_play->channels * d->buf_play->bytes;
    char* play_buf  = calloc(play_size, sizeof(char));
    *play_bufp      = play_buf;

    sndx_buffer_map_dev_to_samples(d->buf_play, play_buf);
    sndx_buffer_map_dev_to_samples(d->buf_capt, capt_buf);

    // Set silence
    uframes_t frames_silence = 0;

    err = sndx_duplex_write_initial_silence(d, play_buf, &frames_silence);
    SndFatal_(err, "Failed sndx_duplex_write_initial_silence: %s");

    // After silence, stream is already started (start threshold is 0x7fffffff in latency, 2*period_size for duplex)
    sndx_duplex_timer_start(&d->timer, d->play, d->capt);

    // Debugging
    sndx_dump_duplex(d, output);

    // Start prep for loop -> on error goto __error
    uframes_t frames_out = frames_silence;
    uframes_t frames_in  = 0;
    uframes_t in_max     = 0;
    uframes_t out_max    = 0;

    // Read write loop
    sframes_t r, cap_avail;
    float     gain = 0.5;
    while (frames_in < loop_limit)
    {
        cap_avail = d->period_size;
        snd_pcm_wait(d->capt, 1000);

        r = sndx_duplex_readbuf(d, capt_buf, cap_avail, &frames_in, &in_max);
        SndReturn_(r, "Failed readbuf: %s");

        // Copy capture channels to playback
        sndx_duplex_copy_capt_to_play(d->buf_capt, d->buf_play, r, &gain);

        err = sndx_duplex_writebuf(d, play_buf, r, &frames_out, &out_max);
        SndReturn_(err, "Failed writebuf: %s");
    }

    // Report final timings
    sndx_duplex_timer_stop(&d->timer, frames_in, d->rate, d->out);

    return 0;
}

int sndx_duplex_stop(sndx_duplex_t* d, char* play_buf, char* capt_buf)
{
    snd_pcm_drop(d->capt);
    snd_pcm_drain(d->play);
    snd_pcm_nonblock(d->play, SND_PCM_NONBLOCK);

    snd_pcm_unlink(d->capt);
    snd_pcm_hw_free(d->play);
    snd_pcm_hw_free(d->capt);

    free(capt_buf);
    free(play_buf);

    return 0;
}

// Application
int main()
{
    int err; ///< Brief description after the member

    output_t* output;
    err = snd_output_stdio_attach(&output, stderr, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:FC1,0", "hw:FC1,0",          // play, capt device
        SND_PCM_FORMAT_S16_LE,           // format
        48000,                           // rate
        256, 128,                        // buffer_size, period_size
        SND_PCM_ACCESS_MMAP_INTERLEAVED, // access
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    char*     play_buf;
    char*     capt_buf;
    uframes_t loop_limit = d->rate / 100; // 0.01 secs

    err = sndx_duplex_start(d, &play_buf, &capt_buf, loop_limit);
    SndCheck_(err, "Failed sndx_duplex_start: %s");

    err = sndx_duplex_stop(d, play_buf, capt_buf);
    SndCheck_(err, "Failed sndx_duplex_stop: %s");

    err = sndx_duplex_close(d);
    SndCheck_(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
