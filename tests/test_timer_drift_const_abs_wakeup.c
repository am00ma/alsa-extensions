#include "sndx/timer.h"
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

    u64 frames;       ///< Number of frames of play/capt (from avail?)
    u64 this_wakeup;  ///< Current time of exit from poll
    u64 prev_wakeup;  ///< Prev value for computing diff
    i64 diff_wakeup;  ///< Compute difference
    f32 period_usecs; ///< From difference

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
    while (iter < slen)
    {
        tspec_t poll_enter, poll_exit;

        // Before snd_pcm_wait
        clock_gettime(CLOCK_MONOTONIC, &poll_enter);

        // 1.5 times the period as timeout, milliseconds
        i64 poll_timeout = 1.5 * t->period_usecs / 1000;

        err = poll(pfds, 1, poll_timeout);
        Goto(err < 0, __close, "Failed: poll: %s", strerror(errno));

        // After snd_pcm_wait
        clock_gettime(CLOCK_MONOTONIC, &poll_exit);

        // Register timing (SetTime) (poll_exit timing is in nanosecs)
        u64 callback_usecs = TimeNano(poll_exit) / 1000;

        // First init, set for prev_wakeup
        if (!t->frames) { t->this_wakeup = callback_usecs - (u64)t->period_usecs; }

        t->frames      += t->period_size;
        t->prev_wakeup  = t->this_wakeup;
        t->this_wakeup  = callback_usecs;
        t->diff_wakeup  = -(t->this_wakeup - t->prev_wakeup);

        // Log necessary stats
        stats[iter] = *t;

        u64 next_wakeup = t->this_wakeup + (u64)t->period_usecs;

        next_timeout = (struct itimerspec){
            .it_interval = {0, 0},
            .it_value =
                {
                    .tv_sec  = next_wakeup / USEC_PER_SEC,
                    .tv_nsec = (next_wakeup - ((next_wakeup / USEC_PER_SEC) * USEC_PER_SEC)) * 1000, // usecs -> nsecs
                },
        };

        // Set next timeout with constant period_usecs
        err = timerfd_settime(pfds[0].fd, TIMER_ABSTIME, &next_timeout, NULL);
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

    printf("iter        ,"
           "frames      ,"
           "this_wakeup ,"
           "prev_wakeup ,"
           "diff_wakeup ,"
           "period_usecs\n");

    RANGE(i, slen)
    {
        Timer* t = &stats[i];

        printf("%ld,"  // iter,
               "%ld,"  // frames,
               "%ld,"  // this_wakeup,
               "%ld,"  // prev_wakeup,
               "%ld,"  // diff_wakeup,
               "%f\n", // period_usecs
               //
               i, t->frames, t->this_wakeup,   //
               t->prev_wakeup, t->diff_wakeup, //
               t->period_usecs);
    }

    free(stats);

    return 0;
}
