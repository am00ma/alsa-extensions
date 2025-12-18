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

    bool init_cycle; ///< In initialization cycle (start/restart)

    u64 frames;           ///< Number of frames of play/capt (from avail?)
    u64 current_callback; ///< set by SetTime in alsa_driver_wait
    u64 current_wakeup;   ///< Estimated wakeup for current cycle
    u64 next_wakeup;      ///< Estimated wakeup for next cycle
    f32 period_usecs;     ///< Estimated period in microseconds
    f32 filter_omega;     ///< Filter param for DLL loop

    int err; ///< return value

} Timer;

void* thread_handler(void* arg)
{
    int    err;
    Timer* timer = arg;

    struct pollfd pfds[1];

    struct itimerspec itimerspec = {
        .it_interval = {0, 0},
        .it_value    = {0, 0},
    };

    float timeout_secs = timer->period_usecs / USEC_PER_SEC;

    itimerspec.it_value.tv_nsec = modff(timeout_secs, &timeout_secs) * NSEC_PER_SEC;
    itimerspec.it_value.tv_sec  = timeout_secs;

    pfds[0].fd = timerfd_create(CLOCK_MONOTONIC, 0);
    err        = (pfds[0].fd == -1);
    Goto(err, __close, "Failed: timerfd_create");

    pfds[0].events = POLLIN;

    err = timerfd_settime(pfds[0].fd, 0, &itimerspec, NULL);
    Goto(err, __close, "Failed: timerfd_settime");

    tspec_t start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (timer->frames < 10)
    {
        p_info("fFrames : %ld", timer->frames);

        tspec_t poll_enter, poll_exit;

        clock_gettime(CLOCK_MONOTONIC, &poll_enter);

        err = poll(pfds, 1, 1.5 * timer->period_usecs); // 1.5 times the period as timeout
        Goto(err < 0, __close, "Failed: poll");

        clock_gettime(CLOCK_MONOTONIC, &poll_exit);

        // // Check difference before update
        // u64 callback_usecs = TimeMicro(poll_exit);

        // Set next timeout
        err = timerfd_settime(pfds[0].fd, 0, &itimerspec, NULL);
        Goto(err, __close, "Failed: timerfd_settime");

        timer->frames++;
    }

    err = 0;

__close:
    timer->err = err;

    pthread_exit(NULL);
}

int main()
{
    int err;

    Timer timer = {
        .rate        = 48000,
        .period_size = 128,
    };
    timer.period_usecs = (f32)timer.period_size / (f32)timer.rate * USEC_PER_SEC;
    print_(timer.period_usecs, "%.6f");

    pthread_t tid;

    err = pthread_create(&tid, NULL, &thread_handler, &timer);
    SysFatal(err, "Failed: pthread_create: %s");

    err = pthread_join(tid, NULL);
    SysFatal(err, "Failed: pthread_join: %s");

    if (timer.err) { p_info("Thread finished with error: %d", timer.err); }
    else
    {
        p_info("Thread finished successfully");
    }

    return 0;
}
