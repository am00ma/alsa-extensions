#include "sndx/timer.h"
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <sys/timerfd.h>

#define USEC_PER_SEC 1000000L
#define NSEC_PER_SEC 1000000000L

#define Timestamp(t)                                                                                                   \
    struct timespec t;                                                                                                 \
    clock_gettime(CLOCK_MONOTONIC, &t)
#define TimeSec(t)   ((f32)t.tv_sec + (f32)t.tv_nsec / 1e9)
#define TimeNano(t)  (t.tv_sec * NSEC_PER_SEC + t.tv_nsec)
#define TimeMicro(t) (t.tv_sec * USEC_PER_SEC + t.tv_nsec / 1000)

#define SysFatal(err, ...) p_err(err < 0, exit(-1), __VA_ARGS__, strerror(errno));

typedef struct Timer
{
    u32 rate;        ///< Samplerate
    u32 period_size; ///< Frames per cycle

    bool first_wakeup; ///< In initialization cycle (start/restart)
    bool initialized;  ///< State for readers

    u64 frames;           ///< Number of frames of play/capt (from avail?)
    u64 current_callback; ///< set by SetTime in alsa_driver_wait
    u64 current_wakeup;   ///< Estimated wakeup for current cycle
    u64 next_wakeup;      ///< Estimated wakeup for next cycle
    f32 period_usecs;     ///< Estimated period in microseconds
    f32 filter_omega;     ///< Filter param for DLL loop

    int err; ///< return value

} Timer;

Timer*          stats;
constexpr isize slen = 1'000;

Timer new_timer(u32 rate, u32 period_size)
{
    Timer timer = {
        .rate         = rate,
        .period_size  = period_size,
        .first_wakeup = true,
        .initialized  = false,
    };
    timer.period_usecs = (f32)timer.period_size / (f32)timer.rate * USEC_PER_SEC;
    return timer;
}

void timer_dump(Timer* t, isize iter)
{
    p_info("Iter: %ld\n"
           "  frames          : %ld\n"
           "  current_callback: %ld\n"
           "  current_wakeup  : %ld\n"
           "  next_wakeup     : %ld\n"
           "  period_usecs    : %f (%f)\n"
           "  filter_omega    : %f\n",
           iter, t->frames, t->current_callback,                               //
           t->current_wakeup, t->next_wakeup,                                  //
           t->period_usecs, (f32)t->period_size / (f32)t->rate * USEC_PER_SEC, //
           t->filter_omega

    );
}

void* thread_handler(void* arg)
{
    int    err;
    Timer* t = arg;

    float timeout_secs = t->period_usecs / USEC_PER_SEC;

    struct pollfd     pfds[1];
    struct itimerspec next_timeout = {
        .it_interval = {0, 0},
        .it_value =
            {
                timeout_secs,
                modff(timeout_secs, &timeout_secs) * NSEC_PER_SEC,
            },
    };

    pfds[0].fd = timerfd_create(CLOCK_MONOTONIC, 0);
    err        = (pfds[0].fd == -1);
    SysGoto(err, __close, "Failed: timerfd_create: %s");

    pfds[0].events = POLLIN;

    err = timerfd_settime(pfds[0].fd, 0, &next_timeout, NULL);
    SysGoto(err, __close, "Failed: timerfd_settime: %s");

    // Start of loop, should measure `snd_pcm_start`
    tspec_t start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    isize iter = 0;
    while (iter < 1000)
    {
        tspec_t poll_enter, poll_exit;

        // Before snd_pcm_wait
        clock_gettime(CLOCK_MONOTONIC, &poll_enter);

        err = poll(pfds, 1, 1.5 * t->period_usecs); // 1.5 times the period as timeout
        Goto(err < 0, __close, "Failed: poll: %s", strerror(errno));

        // After snd_pcm_wait
        clock_gettime(CLOCK_MONOTONIC, &poll_exit);

        // Register timing (SetTime) (poll_exit timing is in nanosecs)
        u64 callback_usecs = TimeNano(poll_exit) / 1000;

        // InitFrameTime - if first cycle
        if (t->first_wakeup)
        {
            t->period_usecs     = (f32)t->period_size / (f32)t->rate * USEC_PER_SEC;
            t->current_callback = callback_usecs;
            t->next_wakeup      = callback_usecs;
            t->filter_omega     = t->period_usecs * 7.854e-7f;
            t->first_wakeup     = false;
        }

        // IncFrameTime
        {
            t->frames           += t->period_size; // Constant rate
            t->current_wakeup    = t->next_wakeup; // Expected wakeup
            t->current_callback  = callback_usecs; // Actual wakeup

            // Prediction for next wakeup (can't we just save squared filter_omega?)
            f32 delta        = (f32)((i64)callback_usecs - (i64)t->next_wakeup);
            delta           *= t->filter_omega;
            t->period_usecs += t->filter_omega * delta; // Best estimate of period
            t->next_wakeup  += (i64)floorf(t->period_usecs + 1.41f * delta + 0.5f);

            t->initialized = true; // Mark initialized/valid
        }

        // Log necessary stats
        stats[iter] = *t;

        // // Update to as for correct period
        // timeout_secs = (f32)t->next_wakeup / USEC_PER_SEC;
        // next_timeout = (struct itimerspec){
        //     .it_interval = {0, 0},
        //     .it_value =
        //         {
        //             timeout_secs,
        //             modff(timeout_secs, &timeout_secs) * NSEC_PER_SEC,
        //         },
        // };
        //
        // // Set next timeout
        // err = timerfd_settime(pfds[0].fd, TIMER_ABSTIME, &next_timeout, NULL);
        // SysGoto(err, __close, "Failed: timerfd_settime: %s");

        // Update to as for correct period
        timeout_secs = t->period_usecs / USEC_PER_SEC;
        next_timeout = (struct itimerspec){
            .it_interval = {0, 0},
            .it_value =
                {
                    timeout_secs,
                    modff(timeout_secs, &timeout_secs) * NSEC_PER_SEC,
                },
        };

        // Set next timeout
        err = timerfd_settime(pfds[0].fd, 0, &next_timeout, NULL);
        SysGoto(err, __close, "Failed: timerfd_settime: %s");

        iter++;
    }

    err = 0;

__close:

    t->err = err;

    pthread_exit(NULL);
}

int main()
{
    int err;

    stats = calloc(slen, sizeof(Timer));

    Timer timer = new_timer(48000, 128);

    pthread_t tid;

    err = pthread_create(&tid, NULL, &thread_handler, &timer);
    SysFatal(err, "Failed: pthread_create: %s");

    err = pthread_join(tid, NULL);
    SysFatal(err, "Failed: pthread_join: %s");

    // if (timer.err) { p_info("Thread finished with error: %d", timer.err); }
    // else
    // {
    //     p_info("Thread finished successfully");
    // }

    printf("iter,            "
           "frames,          "
           "current_callback,"
           "current_wakeup,  "
           "next_wakeup,     "
           "period_usecs,    "
           "filter_omega     \n");

    RANGE(i, slen)
    {
        Timer* t = &stats[i];

        printf("%ld,"  // iter,
               "%ld,"  // frames,
               "%ld,"  // current_callback,
               "%ld,"  // current_wakeup,
               "%ld,"  // next_wakeup,
               "%f,"   // period_usecs,
               "%f\n", // filter_omega
               //
               i, t->frames, t->current_callback, //
               t->current_wakeup, t->next_wakeup, //
               t->period_usecs, t->filter_omega);
    }

    free(stats);

    return 0;
}
