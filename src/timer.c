#include "timer.h"
#include "types.h"
#include <alsa/asoundlib.h>

#define TSTAMP_TYPE SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW

i64 timespec_diff_now_usecs(tspec_t* tspec)
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

i64 timespec_diff_usecs(tspec_t* start, tspec_t* end)
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

i64 timestamp_diff_usecs(tstamp_t start, tstamp_t end)
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
    SndFatal(err, "Stream status error: %s");
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

i64 htimestamp_nsecs(htstamp_t t)
{
    i64 nsec;
    nsec  = t.tv_sec * 1000000000ULL;
    nsec += t.tv_nsec;
    return nsec;
}

i64 htstamp_diff_nsecs(htstamp_t t1, htstamp_t t2)
{
    i64 nsec1, nsec2;
    nsec1 = htimestamp_nsecs(t1);
    nsec2 = htimestamp_nsecs(t2);
    return nsec1 - nsec2;
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

    i64 diff  = timespec_diff_usecs(&t->start_sys, &t->stop_sys);
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
    a_info("     Diff : %li", timestamp_diff_usecs(t->start_play, t->start_capt));
}

int sndx_hstats_enable( //
    sndx_hstats_t* t,
    snd_pcm_t*     pcm,
    u32            rate,
    tstamp_type_t  type,
    bool           do_delay,
    snd_output_t*  output)
{
    int err;

    // Reset anything already there
    t->avail    = 0;
    t->delay    = 0;
    t->frames   = 0;
    t->rate     = rate;
    t->do_delay = do_delay;
    memset(&t->config, 0, sizeof(tstamp_config_t));
    memset(&t->report, 0, sizeof(tstamp_report_t));

    // NOTE: Not sure if we have to do this each time or only on enable
    t->config.type_requested = type;
    t->config.report_delay   = do_delay;

    snd_pcm_hw_params_t* hwparams_p;
    snd_pcm_hw_params_alloca(&hwparams_p);
    err = snd_pcm_hw_params_current(pcm, hwparams_p);
    SndReturn_(err, "Failed: snd_pcm_hw_params_current: %s");

    if (snd_pcm_hw_params_supports_audio_ts_type(hwparams_p, SND_PCM_AUDIO_TSTAMP_TYPE_COMPAT))
        a_info("Playback supports audio compat timestamps");
    if (snd_pcm_hw_params_supports_audio_ts_type(hwparams_p, SND_PCM_AUDIO_TSTAMP_TYPE_DEFAULT))
        a_info("Playback supports audio default timestamps");
    if (snd_pcm_hw_params_supports_audio_ts_type(hwparams_p, SND_PCM_AUDIO_TSTAMP_TYPE_LINK))
        a_info("Playback supports audio link timestamps");
    if (snd_pcm_hw_params_supports_audio_ts_type(hwparams_p, SND_PCM_AUDIO_TSTAMP_TYPE_LINK_ABSOLUTE))
        a_info("Playback supports audio link absolute timestamps");
    if (snd_pcm_hw_params_supports_audio_ts_type(hwparams_p, SND_PCM_AUDIO_TSTAMP_TYPE_LINK_ESTIMATED))
        a_info("Playback supports audio link estimated timestamps");
    if (snd_pcm_hw_params_supports_audio_ts_type(hwparams_p, SND_PCM_AUDIO_TSTAMP_TYPE_LINK_SYNCHRONIZED))
        a_info("Playback supports audio link synchronized timestamps");

    snd_pcm_sw_params_t* swparams_p;
    snd_pcm_sw_params_alloca(&swparams_p);
    err = snd_pcm_sw_params_current(pcm, swparams_p);
    SndReturn_(err, "Failed: snd_pcm_sw_params_current: %s");

    err = snd_pcm_sw_params_set_tstamp_mode(pcm, swparams_p, SND_PCM_TSTAMP_ENABLE);
    SndReturn_(err, "Failed: snd_pcm_sw_params_set_tstamp_mode: %s");

    err = snd_pcm_sw_params_set_tstamp_type(pcm, swparams_p, TSTAMP_TYPE);
    SndReturn_(err, "Failed: snd_pcm_sw_params_set_tstamp_type: %s");

    err = snd_pcm_sw_params(pcm, swparams_p);
    SndReturn_(err, "Failed: snd_pcm_sw_params: %s");

    return 0;
}

int sndx_hstats_update(sndx_hstats_t* t, snd_pcm_t* handle, uframes_t frames_processed, output_t* output)
{
    int err;

    snd_pcm_status_t* status;
    snd_pcm_status_alloca(&status);
    snd_pcm_status_set_audio_htstamp_config(status, &t->config);

    err = snd_pcm_status(handle, status);
    SndReturn_(err, "Failed: snd_pcm_status: %s");

    snd_pcm_status_get_trigger_htstamp(status, &t->trigger);
    snd_pcm_status_get_htstamp(status, &t->tstamp);
    snd_pcm_status_get_audio_htstamp(status, &t->audio);
    snd_pcm_status_get_audio_htstamp_report(status, &t->report);

    t->avail = snd_pcm_status_get_avail(status);
    t->delay = snd_pcm_status_get_delay(status);

    t->frames += frames_processed; /* read plus queued */

    return 0;
}

/** @brief Print report of current snapshot and print difference in sys and snd time */
void sndx_dump_hstats(sndx_hstats_t* t, snd_output_t* output)
{
    if (t->report.valid == 0) a_info("Audio timestamp report invalid");
    if (t->report.accuracy_report == 0) a_info("Audio timestamp accuracy report invalid");

    a_info("  systime      : %ld nsec \n"
           "  audio time   : %ld nsec \n"
           "  systime delta: %ld nsec \n"
           "  resolution   : %d       \n",
           htstamp_diff_nsecs(t->tstamp, t->trigger),                              //
           htimestamp_nsecs(t->audio),                                             //
           htstamp_diff_nsecs(t->tstamp, t->trigger) - htimestamp_nsecs(t->audio), //
           t->report.accuracy);

    i64 current = t->frames + t->delay; /* read plus queued */

    i64 curr_count = (i64)current * 1000000000LL / t->rate;

    a_info("  do_delay    : %d  \n"
           "  curr_count  : %ld \n"
           "  driver count: %ld \n"
           "  delta       : %ld nsec (%ld usec, %ld msec) \n", //
           t->do_delay,                                        //
           curr_count,                                         //
           htimestamp_nsecs(t->audio),                         //
           curr_count - htimestamp_nsecs(t->audio),            //
           (curr_count - htimestamp_nsecs(t->audio)) / 1'000,  //
           (curr_count - htimestamp_nsecs(t->audio)) / 1'000'000);
}
