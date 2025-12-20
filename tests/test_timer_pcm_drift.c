#include "sndx/duplex.h"

#define Min(a, b)                                                                                                      \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a < _b ? _a : _b;                                                                                             \
    })

#define USEC_PER_SEC 1000000L
#define NSEC_PER_SEC 1000000000L
#define TimeNano(t)  (t.tv_sec * NSEC_PER_SEC + t.tv_nsec)

typedef struct timespec tspec_t;

typedef struct Timer
{
    u32 rate;        ///< Samplerate
    u32 period_size; ///< Frames per cycle

    u64 frames;       ///< Number of frames of play/capt (from avail?)
    u64 this_wakeup;  ///< Current time of exit from poll
    u64 prev_wakeup;  ///< Prev value for computing diff
    i64 diff_wakeup;  ///< Compute difference
    f32 period_usecs; ///< From difference

    u32 read_iters;  ///< Number of iterations to read
    u32 write_iters; ///< Number of iterations to write

    i64 read_avail;  ///< Available read on each iteration
    i64 write_avail; ///< Available write on each iteration
    i64 min_avail;   ///< Minimum of above and period size

    int err; ///< return value

} Timer;

Timer*          stats;
constexpr isize slen = 1000;

Timer new_timer(u32 rate, u32 period_size)
{
    Timer timer = {
        .rate        = rate,
        .period_size = period_size,
    };
    timer.period_usecs = (f32)timer.period_size / (f32)timer.rate * USEC_PER_SEC;
    return timer;
}

void timer_dump(Timer* t, isize iter)
{
    p_info("Iter: %ld\n"
           "  frames      : %ld\n"
           "  this_wakeup : %ld\n"
           "  prev_wakeup : %ld\n"
           "  diff_wakeup : %ld\n"
           "  period_usecs: %f\n",
           iter, t->frames, t->this_wakeup, t->prev_wakeup, t->diff_wakeup, t->period_usecs);
}

int main()
{
    int err;

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
        if (!t->frames) { t->this_wakeup = callback_usecs - (u64)t->period_usecs; }

        t->frames      += t->period_size;
        t->prev_wakeup  = t->this_wakeup;
        t->this_wakeup  = callback_usecs;
        t->diff_wakeup  = -(t->this_wakeup - t->prev_wakeup);

        t->read_avail  = avail_capt;
        t->write_avail = avail_play;
        t->min_avail   = avail;
        t->read_iters  = read_iters;
        t->write_iters = write_iters;

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
           "this_wakeup,"
           "prev_wakeup,"
           "diff_wakeup,"
           "read_avail,"
           "write_avail,"
           "min_avail,"
           "read_iters,"
           "write_iters,"
           "period_usecs\n");

    RANGE(i, slen)
    {
        Timer* t = &stats[i];

        printf("%ld,"  // iter,
               "%ld,"  // frames,
               "%ld,"  // this_wakeup,
               "%ld,"  // prev_wakeup,
               "%ld,"  // diff_wakeup,
               "%ld,"  // read_avail,
               "%ld,"  // write_avail,
               "%ld,"  // min_avail,
               "%d,"   // read_iters,
               "%d,"   // write_iters,
               "%f\n", // period_usecs
               //
               i, t->frames,                                   //
               t->this_wakeup, t->prev_wakeup, t->diff_wakeup, //
               t->read_avail, t->write_avail, t->min_avail,    //
               t->read_iters, t->write_iters, t->period_usecs);
    }

    free(stats);
    free(buf);

    return 0;
}
