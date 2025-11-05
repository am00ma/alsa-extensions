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
