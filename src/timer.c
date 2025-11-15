#include "timer.h"

u64 timespec_diff_now(tspec_t* tspec)
{
    tspec_t now, diff;
    timestamp_now(&now);
    if (tspec->tv_nsec > now.tv_nsec)
    {
        diff.tv_sec  = now.tv_sec - tspec->tv_sec - 1;
        diff.tv_nsec = (now.tv_nsec + 1000000000L) - tspec->tv_nsec;
    }
    else
    {
        diff.tv_sec  = now.tv_sec - tspec->tv_sec;
        diff.tv_nsec = now.tv_nsec - tspec->tv_nsec;
    }
    /* microseconds */
    return (diff.tv_sec * 1000000) + ((diff.tv_nsec + 500L) / 1000L);
}

u64 timespec_diff(tspec_t* start, tspec_t* end)
{
    tspec_t diff;
    timestamp_now(end);
    if (start->tv_nsec > end->tv_nsec)
    {
        diff.tv_sec  = end->tv_sec - start->tv_sec - 1;
        diff.tv_nsec = (end->tv_nsec + 1000000000L) - start->tv_nsec;
    }
    else
    {
        diff.tv_sec  = end->tv_sec - start->tv_sec;
        diff.tv_nsec = end->tv_nsec - start->tv_nsec;
    }
    /* microseconds */
    return (diff.tv_sec * 1000000) + ((diff.tv_nsec + 500L) / 1000L);
}

i64 timestamp_diff(tstamp_t start, tstamp_t end)
{
    i64 l;
    start.tv_sec -= end.tv_sec;

    l = (i64)start.tv_usec - (i64)end.tv_usec;
    if (l < 0)
    {
        start.tv_sec--;
        l  = 1000000 + l;
        l %= 1000000;
    }
    return (start.tv_sec * 1000000) + l;
}

void timestamp_get(snd_pcm_t* handle, tstamp_t* tstamp)
{
    status_t* status;
    snd_pcm_status_alloca(&status);
    int err = snd_pcm_status(handle, status);
    SndFatal(err, "Stream status error: %s\n");
    snd_pcm_status_get_trigger_tstamp(status, tstamp);
}

/** @brief Get clock time in microseconds */
u64 get_microseconds()
{
    u64     utime;
    tspec_t time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    utime = (u64)time.tv_sec * 1e6 + (u64)time.tv_nsec / 1e3;
    return utime;
}

void sndx_timer_start(sndx_timer_t* t, u32 rate, snd_pcm_t* play, snd_pcm_t* capt)
{
    t->rate = rate;
    timestamp_now(&t->start_sys);
    timestamp_get(play, &t->start_play);
    timestamp_get(capt, &t->start_capt);
}

void sndx_timer_stop(sndx_timer_t* t, snd_pcm_t* play, snd_pcm_t* capt)
{
    timestamp_now(&t->stop_sys);
    timestamp_get(play, &t->stop_play);
    timestamp_get(capt, &t->stop_capt);
}

void sndx_dump_timer(sndx_timer_t* t, output_t* output)
{
    AssertMsg(t->rate, "Please set rate to measure timing differences.");

    i64 diff  = timespec_diff(&t->start_sys, &t->stop_sys);
    i64 mtime = frames_to_micro(t->frames_capt, t->rate);

    a_info("Timer: sys tspec vs count");
    a_info("  Elapsed real  : %9ld us", diff);
    a_info("  Elapsed device: %9ld us (from frames_capt, rate)", mtime);
    a_info("          Diff  : %9ld us (device - real)", mtime - diff);

    a_info("Timer: snd trigger stamps");
    bool hw_sync = (t->start_play.tv_sec == t->start_capt.tv_sec && //
                    t->start_play.tv_usec == t->start_capt.tv_usec);
    a_info("  HW sync : %s", hw_sync ? "yes" : "no");
    a_info("  Playback: %li.%i", (long)t->start_play.tv_sec, (int)t->start_play.tv_usec);
    a_info("  Capture : %li.%i", (long)t->start_capt.tv_sec, (int)t->start_capt.tv_usec);
    a_info("     Diff : %li", timestamp_diff(t->start_play, t->start_capt));
}
