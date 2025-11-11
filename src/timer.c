#include "timer.h"

u64 timestamp_diff_now(tspec_t* tstamp)
{
    tspec_t now, diff;
    timestamp_now(&now);
    if (tstamp->tv_nsec > now.tv_nsec)
    {
        diff.tv_sec  = now.tv_sec - tstamp->tv_sec - 1;
        diff.tv_nsec = (now.tv_nsec + 1000000000L) - tstamp->tv_nsec;
    }
    else {
        diff.tv_sec  = now.tv_sec - tstamp->tv_sec;
        diff.tv_nsec = now.tv_nsec - tstamp->tv_nsec;
    }
    /* microseconds */
    return (diff.tv_sec * 1000000) + ((diff.tv_nsec + 500L) / 1000L);
}

long timestamp_diff(snd_timestamp_t t1, snd_timestamp_t t2)
{
    signed long l;

    t1.tv_sec -= t2.tv_sec;
    l          = (signed long)t1.tv_usec - (signed long)t2.tv_usec;
    if (l < 0)
    {
        t1.tv_sec--;
        l  = 1000000 + l;
        l %= 1000000;
    }
    return (t1.tv_sec * 1000000) + l;
}

void timestamp_get(snd_pcm_t* handle, snd_timestamp_t* timestamp)
{
    status_t* status;
    snd_pcm_status_alloca(&status);
    int err = snd_pcm_status(handle, status);
    SndFatal(err, "Stream status error: %s\n");
    snd_pcm_status_get_trigger_tstamp(status, timestamp);
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

void sndx_duplex_timer_start(sndx_timer_t* t, snd_pcm_t* play, snd_pcm_t* capt)
{
    timestamp_now(&t->start);
    timestamp_get(play, &t->play);
    timestamp_get(capt, &t->capt);
}

void sndx_duplex_timer_stop(sndx_timer_t* t, uframes_t frames_in, u32 rate, output_t* output)
{
    a_info("Timer status:");

    if (t->play.tv_sec == t->capt.tv_sec && //
        t->play.tv_usec == t->capt.tv_usec)
        a_info("  Hardware sync");

    i64 diff  = timestamp_diff_now(&t->start);
    i64 mtime = frames_to_micro(frames_in, rate);
    a_info("  Elapsed real  : %ld us", diff);
    a_info("  Elapsed device: %ld us", mtime);
    a_info("  Diff (device - real): %ld us", mtime - diff);
    a_info("  Playback = %li.%i", (long)t->play.tv_sec, (int)t->play.tv_usec);
    a_info("  Capture  = %li.%i", (long)t->capt.tv_sec, (int)t->capt.tv_usec);
    a_info("  Diff     = %li", timestamp_diff(t->play, t->capt));
}
