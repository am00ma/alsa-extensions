#include "sndx/duplex.h"
#include "sndx/types.h"
#include <common-types.h>
#include <stdlib.h>

// ----------------------------------------
// ----- Helpers -----
// ----------------------------------------

#define Min(a, b)                                                                                                      \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a < _b ? _a : _b;                                                                                             \
    })

#define USEC_PER_SEC 1000000L
#define NSEC_PER_SEC 1000000000L
#define TimeNano(t)  (t.tv_sec * NSEC_PER_SEC + t.tv_nsec)

// ----------------------------------------
// ----- Timer -----
// ----------------------------------------

typedef struct Timer
{
    u32 rate;        ///< Samplerate
    u32 period_size; ///< Frames per cycle

    u64 frames;           ///< Number of frames of play/capt (from avail?)
    u64 this_wakeup_usec; ///< Current time of exit from poll
    u64 prev_wakeup_usec; ///< Prev value for computing diff
    i64 diff_wakeup_usec; ///< Compute difference
    f32 period_usecs;     ///< From difference

    u32 read_iters;  ///< Number of iterations to read
    u32 write_iters; ///< Number of iterations to write

    i64 read_avail;  ///< Available read on each iteration
    i64 write_avail; ///< Available write on each iteration
    i64 min_avail;   ///< Minimum of above and period size

    u64 play_trigger_nsec; ///< device trigger timestamp (nsecs)
    u64 capt_audio_nsec;   ///< device high resolution timestamp for capture (nsecs)
    u64 play_sys_nsec;     ///< system timestamp (nsecs)
    i64 play_diff_nsecs;   ///< audio - (system - trigger) (nsecs)

    u64 capt_trigger_nsec; ///< device trigger timestamp (nsecs)
    u64 play_audio_nsec;   ///< device high resolution timestamp for playback (nsecs)
    u64 capt_sys_nsec;     ///< system timestamp (nsecs)
    i64 capt_diff_nsecs;   ///< audio - (system - trigger) (nsecs)

    int err; ///< return value

} Timer;

Timer new_timer(u32 rate, u32 period_size)
{
    Timer timer = {
        .rate        = rate,
        .period_size = period_size,
    };
    timer.period_usecs = (f32)timer.period_size / (f32)timer.rate * USEC_PER_SEC;
    return timer;
}

int timer_timestamp( //
    snd_pcm_t*       handle,
    htstamp_t*       timestamp,
    htstamp_t*       trigger_timestamp,
    htstamp_t*       audio_timestamp,
    tstamp_config_t* audio_tstamp_config,
    tstamp_report_t* audio_tstamp_report,
    uframes_t*       avail,
    sframes_t*       delay,
    output_t*        output)
{
    int               err;
    snd_pcm_status_t* status;

    snd_pcm_status_alloca(&status);

    snd_pcm_status_set_audio_htstamp_config(status, audio_tstamp_config);

    err = snd_pcm_status(handle, status);
    SndGoto_(err, __failed, "Failed: snd_pcm_status: %s");

    snd_pcm_status_get_trigger_htstamp(status, trigger_timestamp);
    snd_pcm_status_get_htstamp(status, timestamp);
    snd_pcm_status_get_audio_htstamp(status, audio_timestamp);
    snd_pcm_status_get_audio_htstamp_report(status, audio_tstamp_report);
    *avail = snd_pcm_status_get_avail(status);
    *delay = snd_pcm_status_get_delay(status);

    return 0;

__failed:
    return err;
}

// ----------------------------------------
// ----- Globals -----
// ----------------------------------------

Timer*          stats;
constexpr isize slen = 10000;

// ----------------------------------------
// ----- Main -----
// ----------------------------------------

int main(int argc, char* argv[])
{
    int err;

    // 0 - Compat
    // 1 - default
    // 2 - link
    // 3 - link_absolute
    // 4 - link_estimated
    // 5 - link_synchronized
    int type = 2;

    // Include delay in report
    int do_delay = 0;

    if (argc == 2)
    {
        int opt_type = atoi(argv[1]);
        type         = opt_type;
    }

    if (argc == 3)
    {
        int opt_do_delay = atoi(argv[2]);
        do_delay         = opt_do_delay;
    }

    tstamp_config_t config_p;
    tstamp_config_t config_c;
    tstamp_report_t report_p;
    tstamp_report_t report_c;

    memset(&config_p, 0, sizeof(tstamp_config_t));
    memset(&config_c, 0, sizeof(tstamp_config_t));
    memset(&report_p, 0, sizeof(tstamp_report_t));
    memset(&report_c, 0, sizeof(tstamp_report_t));

    config_p.type_requested = type;
    config_c.type_requested = type;
    config_p.report_delay   = do_delay;
    config_c.report_delay   = do_delay;

    stats = calloc(slen, sizeof(Timer));
    Fatal(!stats, "Failed calloc: %s", strerror(errno));

    u8* buf = calloc(1024 * 1024, sizeof(u8));
    Fatal(!buf, "Failed calloc: %s", strerror(errno));

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(            //
        &d,                            //
        "hw:A96,0", "hw:A96,0",        //
        SND_PCM_FORMAT_S32_LE,         //
        48000, 128, 2,                 //
        SND_PCM_ACCESS_RW_INTERLEAVED, //
        output);
    SndFatal_(err, "Failed sndx_duplex_open: %s");

    // Same rate and period_size
    Timer  timer = new_timer(48000, 128);
    Timer* t     = &timer;

    // PREPARED -> RUNNING
    err = sndx_duplex_start(d);
    SndGoto_(err, __close, "Failed: sndx_duplex_start: %s");

    // Start of loop, should measure `snd_pcm_start`
    tspec_t start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    isize iter = 0;
    while (iter < slen)
    {
        tspec_t poll_enter, poll_exit;

        clock_gettime(CLOCK_MONOTONIC, &poll_enter);

        err = snd_pcm_wait(d->capt, 1000);
        SndGoto_(err, __close, "Failed: snd_pcm_wait: %s");

        htstamp_t p_ht_sys;
        htstamp_t p_ht_trigger;
        htstamp_t p_ht_audio;
        uframes_t p_avail;
        sframes_t p_delay;

        // Get audio timestamps
        err = timer_timestamp(                     //
            d->play,                               //
            &p_ht_sys, &p_ht_trigger, &p_ht_audio, //
            &config_p, &report_p,                  //
            &p_avail, &p_delay, output);
        SndGoto_(err, __close, "Failed: timer_timestamp (play) : %s");

        htstamp_t c_ht_sys;
        htstamp_t c_ht_trigger;
        htstamp_t c_ht_audio;
        uframes_t c_avail;
        sframes_t c_delay;

        // Get audio timestamps
        err = timer_timestamp(                     //
            d->capt,                               //
            &c_ht_sys, &c_ht_trigger, &c_ht_audio, //
            &config_c, &report_c,                  //
            &c_avail, &c_delay, output);
        SndGoto_(err, __close, "Failed: timer_timestamp (capt): %s");

        // Overwrite, till last exit
        clock_gettime(CLOCK_MONOTONIC, &poll_exit);

        // Get last poll_exit timing (SetTime) (poll_exit timing is in nanosecs)
        u64 callback_usecs = TimeNano(poll_exit) / 1000;

        // Read and write equal number of samples
        i64 avail_capt = snd_pcm_avail_update(d->capt);
        SndGoto_(avail_capt, __close, "Failed: snd_pcm_avail: %s");

        i64 avail_play = snd_pcm_avail_update(d->play);
        SndGoto_(avail_play, __close, "Failed: snd_pcm_avail: %s");

        i64 avail = Min(avail_play, avail_capt);
        avail     = Min(avail, t->period_size);

        i64 nread      = 0;
        u32 read_iters = 0;
        while (nread < avail)
        {
            err = snd_pcm_readi(d->capt, buf, avail - nread);
            SndGoto_(err, __close, "Failed: snd_pcm_readi: %s");
            nread += err;
            read_iters++;
        }

        i64 nwritten    = 0;
        u32 write_iters = 0;
        while (nwritten < avail)
        {
            err = snd_pcm_writei(d->play, buf, avail - nwritten);
            SndGoto_(err, __close, "Failed: snd_pcm_writei: %s");
            nwritten += err;
            write_iters++;
        }

        // First init, set for prev_wakeup
        if (!t->frames) { t->this_wakeup_usec = callback_usecs - (u64)t->period_usecs; }

        t->frames           += t->period_size;
        t->prev_wakeup_usec  = t->this_wakeup_usec;
        t->this_wakeup_usec  = callback_usecs;
        t->diff_wakeup_usec  = -(t->this_wakeup_usec - t->prev_wakeup_usec);

        t->read_avail  = avail_capt;
        t->write_avail = avail_play;
        t->min_avail   = avail;
        t->read_iters  = read_iters;
        t->write_iters = write_iters;

        t->play_trigger_nsec = ToNano(p_ht_trigger);
        t->play_sys_nsec     = ToNano(p_ht_sys) - t->play_trigger_nsec;
        t->play_audio_nsec   = ToNano(p_ht_audio);
        t->play_diff_nsecs   = t->play_sys_nsec - t->play_audio_nsec;

        t->capt_trigger_nsec = ToNano(c_ht_trigger);
        t->capt_sys_nsec     = ToNano(c_ht_sys) - t->capt_trigger_nsec;
        t->capt_audio_nsec   = ToNano(c_ht_audio);
        t->capt_diff_nsecs   = t->capt_sys_nsec - t->capt_audio_nsec;

        // Log necessary stats
        stats[iter] = *t;

        iter++;
    }

__close:

    err = sndx_duplex_stop(d);
    SndFatal_(err, "Failed sndx_duplex_stop: %s");

    err = sndx_duplex_close(d);
    SndFatal_(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    printf("iter,"
           "frames,"
           "this_wakeup_usec,"
           "prev_wakeup_usec,"
           "diff_wakeup_usec,"
           "read_avail,"
           "write_avail,"
           "min_avail,"
           "read_iters,"
           "write_iters,"
           "play_sys_nsec,"
           "play_audio_nsec,"
           "play_trigger_nsec,"
           "play_diff_nsecs,"
           "capt_sys_nsec,"
           "capt_audio_nsec,"
           "capt_trigger_nsec,"
           "capt_diff_nsecs,"
           "period_usecs\n");

    RANGE(i, slen)
    {
        Timer* t = &stats[i];

        printf("%ld,"  // iter,
               "%ld,"  // frames,
               "%ld,"  // this_wakeup_usec,
               "%ld,"  // prev_wakeup_usec,
               "%ld,"  // diff_wakeup_usec,
               "%ld,"  // read_avail,
               "%ld,"  // write_avail,
               "%ld,"  // min_avail,
               "%d,"   // read_iters,
               "%d,"   // write_iters,
               "%ld,"  // play_sys_nsec,
               "%ld,"  // play_audio_nsec,
               "%ld,"  // play_trigger_nsec,
               "%ld,"  // play_diff_nsecs,
               "%ld,"  // capt_sys_nsec,
               "%ld,"  // capt_audio_nsec,
               "%ld,"  // capt_trigger_nsec,
               "%ld,"  // capt_diff_nsecs,
               "%f\n", // period_usecs
               //
               i, t->frames,                                                                   //
               t->this_wakeup_usec, t->prev_wakeup_usec, t->diff_wakeup_usec,                  //
               t->read_avail, t->write_avail, t->min_avail,                                    //
               t->read_iters, t->write_iters,                                                  //
               t->play_sys_nsec, t->play_audio_nsec, t->play_trigger_nsec, t->play_diff_nsecs, //
               t->capt_sys_nsec, t->capt_audio_nsec, t->capt_trigger_nsec, t->capt_diff_nsecs, //
               t->period_usecs);
    }

    free(stats);
    free(buf);

    return 0;
}
