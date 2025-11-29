/** @file audio_time.c
 *  @brief Implementation of `audio_time.c` with sndx_duplex
 *
 *  Almost same as latency for wait-read-write loop
 *
 *  When adjusting for delay, we use `adjust_factor`:
 *      Capture :  read plus queued     : adjust_factor = +1
 *      Playback:  written minus queued : adjust_factor = -1
 *
 *  1. No checking xruns, directly exits, still works, never got an xrun
 *  2. This time error checking `snd_pcm_wait`, `readi`, `writei`, but straight fatal
 *  3. Direct readi, writei
 *  4. Use of mmap is useless however `write_initial_silence` uses mmap like in latency
 *
 *  Adds use of `htstamp` apart from just `tstamp` using:
 *      - `snd_pcm_audio_tstamp_config_t`
 *      - `snd_pcm_audio_tstamp_report_t`
 *
 *  TODO:
 *
 *  1. Set requested config:
 *    audio_tstamp_config_p.type_requested = type;
 *    audio_tstamp_config_p.report_delay   = do_delay;
 *
 *  2. Get timestamp
 *      ```c
 *      _gettimestamp(snd_pcm_t    *handle,
 *                    htimestamp_t *timestamp,              // tstamp_c
 *                    htimestamp_t *trigger_timestamp,      // trigger_tstamp_c
 *                    htimestamp_t *audio_timestamp,        // audio_tstamp_c
 *                    config_t     *audio_tstamp_config,    // config_c
 *                    report_t     *audio_tstamp_report,    // report_c
 *                    uframes_t    *avail,                  // avail_c = snd_pcm_status_get_avail(status)
 *                    sframes_t    *delay)                  // delay_c = snd_pcm_status_get_delay(status)
 *      ```
 *
 *  3. Report (every cycle):
 *      - systime   : timediff(tstamp_p, trigger_tstamp_p)
 *      - audio time: timestamp2ns(audio_tstamp_p)
 *      - delta     : systime - audio time
 *      - resolution: audio_tstamp_report_p.accuracy
 *
 */
#include "sndx/duplex.h"
#include "sndx/timer.h"

// Application
int main()
{
    int err; ///< Brief description after the member

    output_t* output;
    err = snd_output_stdio_attach(&output, stderr, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    // play: Blocking, capt: Nonblock
    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:A96,0", "hw:A96,0",          //
        SND_PCM_FORMAT_S32_LE,           //
        48000, 128, 2,                   //
        SND_PCM_ACCESS_MMAP_INTERLEAVED, //
        output);
    SndFatal_(err, "Failed sndx_duplex_open: %s");

    sndx_dump_duplex(d, d->out);

    // Enable highresolution timestamps
    sndx_hstats_t ht_play;
    sndx_hstats_t ht_capt;

    bool          do_delay = true;
    tstamp_type_t type     = SND_PCM_AUDIO_TSTAMP_TYPE_DEFAULT;

    err = sndx_hstats_enable(&ht_play, d->play, d->rate, type, do_delay, d->out);
    SndGoto_(err, __close, "Failed: sndx_hstats_enable (play): %s");

    err = sndx_hstats_enable(&ht_capt, d->capt, d->rate, type, do_delay, d->out);
    SndGoto_(err, __close, "Failed: sndx_hstats_enable (capt): %s");

    // PREPARED -> RUNNING
    err = sndx_duplex_start(d);
    SndGoto_(err, __close, "Failed: sndx_duplex_start: %s");

    // Update playback for silence
    err = sndx_hstats_update(&ht_play, d->play, d->period_size * d->periods, d->out);
    SndGoto_(err, __close, "Failed: sndx_hstats_update (play): %s");

    while (d->timer->frames_capt < d->rate * 10)
    {
        // Wait -> no error checking, still works somehow
        err = snd_pcm_wait(d->capt, 1000);
        SndGoto_(err, __close, "Failed: snd_pcm_wait: %s");

        // Read - prob period_size as that is guaranteed in avail_min?
        uframes_t len    = d->period_size;
        sframes_t frames = 0;

        frames = snd_pcm_mmap_readi(d->capt, d->buf_capt->devdata, len);
        SndGoto_(frames, __close, "Failed: snd_pcm_mmap_readi: %s");

        // Even if r == 0 ; if r < 0, caught above
        d->timer->frames_capt += frames;

        err = sndx_hstats_update(&ht_capt, d->capt, frames, d->out);
        SndGoto_(err, __close, "Failed: sndx_hstats_update (capt): %s");

        // To soft buffer -> reads from devdata
        sndx_buffer_dev_to_buf(d->buf_capt, 0, frames);

        // Copy soft buffer
        isize pos_play, pos_capt;
        RANGE(chn, d->ch_play)
        RANGE(i, frames)
        {
            pos_play = i + chn * d->buf_play->frames;
            pos_capt = i;

            d->buf_play->bufdata[pos_play] = d->buf_capt->bufdata[pos_capt];
        }

        // To write buffer -> writes to devdata
        sndx_buffer_buf_to_dev(d->buf_play, 0, frames);

        // Write
        frames = snd_pcm_mmap_writei(d->play, d->buf_play->devdata, len);
        SndGoto_(frames, __close, "Failed: snd_pcm_mmap_writei: %s");

        d->timer->frames_play += frames;

        err = sndx_hstats_update(&ht_play, d->play, frames, d->out);
        SndGoto_(err, __close, "Failed: sndx_hstats_update (play): %s");

        a_title("Capture");
        sndx_dump_hstats(&ht_capt, +1, d->out);

        a_title("Playback");
        sndx_dump_hstats(&ht_play, -1, d->out);
    }

__close:

    err = sndx_duplex_stop(d);
    SndFatal_(err, "Failed sndx_duplex_stop: %s");

    err = sndx_duplex_close(d);
    SndFatal(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
