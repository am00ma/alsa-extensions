#include "pollfds.h"
#include "timer.h"

#define MAX_RETRY_COUNT 5

int sndx_pollfds_open(sndx_pollfds_t** pfdsp, snd_pcm_t* play, snd_pcm_t* capt, snd_output_t* output)
{

    sndx_pollfds_t* p;

    // NOTE: calloc resets all stats to zero
    p = calloc(1, sizeof(*p));
    RetVal_(!p, -ENOMEM, "Failed calloc pollfds_t* p");

    p->play_nfds = snd_pcm_poll_descriptors_count(play);
    p->capt_nfds = snd_pcm_poll_descriptors_count(capt);

    p->addr = calloc(p->play_nfds + p->capt_nfds, sizeof(pfd_t));
    RetVal_(!p->addr, -ENOMEM, "Failed calloc pft_t* addr");

    *pfdsp = p;

    return 0;
}

void sndx_pollfds_close(sndx_pollfds_t* p)
{
    if (!p) return;

    Free(p->addr);
    Free(p);
}

int sndx_pollfds_reset(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, snd_output_t* output)
{
    if (!p) return -1; // NOTE: Silently returns err

    Free(p->addr);

    p->play_nfds = snd_pcm_poll_descriptors_count(play);
    p->capt_nfds = snd_pcm_poll_descriptors_count(capt);

    p->addr = calloc(p->play_nfds + p->capt_nfds, sizeof(pfd_t));
    RetVal_(!p->addr, -ENOMEM, "Failed calloc pft_t* addr");

    return 0;
}

int sndx_pollfds_xrun(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output)
{
    int err;

    status_t* status;
    snd_pcm_status_alloca(&status);

    err = snd_pcm_status(capt, status);
    SndCheck_(err, "Failed: snd_pcm_status (capt): %s");

    // Jack only checks capt
    // err = snd_pcm_status(play, status);
    // SndCheck_(err, "Failed: snd_pcm_status (play): %s");

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_SUSPENDED)
    {
        a_info("**** alsa_pcm: pcm in suspended state, resuming it");

        err = snd_pcm_prepare(capt);
        SndCheck_(err, "Failed: snd_pcm_prepare after suspend (capt): %s");

        err = snd_pcm_prepare(play);
        SndCheck_(err, "Failed: snd_pcm_prepare after suspend (play): %s");
    }

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN)
    {
        p->xrun_count++;

        struct timeval now, diff, tstamp;
        snd_pcm_status_get_tstamp(status, &now);
        snd_pcm_status_get_trigger_tstamp(status, &tstamp);

        timersub(&now, &tstamp, &diff);
        p->delayed_usecs = diff.tv_sec * 1000000.0 + diff.tv_usec;
        a_info("**** alsa_pcm: xrun of at least %.3f msecs", p->delayed_usecs / 1000.0);

        a_info("Repreparing capture");
        err = snd_pcm_prepare(capt);
        SndCheck_(err, "Failed: snd_pcm_prepare after xrun (capt): %s");

        a_info("Repreparing playback");
        err = snd_pcm_prepare(play);
        SndCheck_(err, "Failed: snd_pcm_prepare after xrun (play): %s");
    }

    // TODO: Jack calls it restart

    // err = snd_pcm_drop(play);
    // SndCheck_(err, "Failed snd_pcm_drop play: %s");

    // // Damn, need a lot here if not mixed with duplex
    // if (!d->linked)
    // {
    //     err = snd_pcm_drop(capt);
    //     SndCheck_(err, "Failed snd_pcm_drop capt: %s");
    // }

    return 0;
}

int sndx_pollfds_wait(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output)
{
    int err;

    u64 poll_enter  = 0;
    u64 poll_ret    = 0;
    int poll_result = 0;
    u16 revents;

    bool xrun_detected = false;
    bool need_playback = true;
    bool need_capture  = true;

    while ((need_playback || need_capture) && !xrun_detected)
    {
        u32 nfds = 0;
        u32 ci   = 0;

        if (need_playback)
        {
            snd_pcm_poll_descriptors(play, &p->addr[0], p->play_nfds);
            nfds += p->play_nfds;
        }

        if (need_capture)
        {
            snd_pcm_poll_descriptors(capt, &p->addr[nfds], p->capt_nfds);
            ci    = nfds;
            nfds += p->capt_nfds;
        }

        poll_enter = get_microseconds();

        if (poll_enter > p->poll_next)
        {
            // This processing cycle was delayed past the next due interrupt!  Do not account this as a wakeup delay
            p->poll_next = 0;
            p->poll_late++;
        }

        poll_result = poll(p->addr, nfds, p->poll_timeout);
        if (poll_result < 0)
        {
            // Currently same as any errno, but jack has special handling for gdb errors here
            if (errno == EINTR) { RetVal_(EINTR, -POLLFD_FATAL, "Received poll interrupt"); }
            SysReturn_(errno, "Poll call failed  %s");
        }

        poll_ret = get_microseconds();

        if (poll_result == 0)
        {
            p->retry_count++;
            if (p->retry_count > MAX_RETRY_COUNT)
            {
                SysReturn_(errno,
                           "Poll time out"
                           "   Polled for            = %ld usecs\n"
                           "   Reached max retry cnt = %d       \n"
                           "Exiting (%s)",
                           poll_ret - poll_enter, MAX_RETRY_COUNT);
            }

            a_error("Poll time out, polled for %ld usecs, Retrying with a recovery, retry cnt = %d", //
                    poll_ret - poll_enter, p->retry_count);

            // TODO: Request restart instead, we are now tracking retry_count within p

            // QUESTION: Then what happens to continuation of polling? Won't the restart drop us back here?

            // NOTE: Run xrun recovery, but keeping track of retry count, i.e. restart the driver, if that fails, is fatal
            err = sndx_pollfds_xrun(p, play, capt, output);
            SndReturn_(err, "Poll time out, xrun recovery failed with status = %d: %s", err);
        }

        if (p->poll_next && poll_ret > p->poll_next) { p->delayed_usecs = poll_ret - p->poll_next; }
        p->poll_last = poll_ret;
        p->poll_next = poll_ret + p->period_usecs;

        // NOTE:
        // If poll() sets the POLLOUT flag then:
        //   -> at least one byte may be written without blocking
        // Assuming this means success:
        //   Implies that need_capture, need_playback should both be satisfied for 'successful' wait
        //     i.e. we are waiting on both, and only wake when both are available
        //   Also implies that loop breaks on xrun on either play/capt
        //     So then, how do we recover and prepare the correct device?
        //     Probably the reason we 'start' and 'stop' completely instead?
        // Does this mean that minimum of `period_size` is available to read/write?

        if (need_playback)
        {
            err = snd_pcm_poll_descriptors_revents(play, &p->addr[0], p->play_nfds, &revents);
            SndReturn_(err, "Failed: snd_pcm_poll_descriptors_revents (play): %s");
            if (revents & POLLNVAL) { SndReturn_(-POLLNVAL, "Device disconnected (play): %s"); }
            if (revents & POLLERR) { xrun_detected = true; }
            if (revents & POLLOUT) { need_playback = 0; } // NOTE: Success?
        }

        if (need_capture)
        {
            err = snd_pcm_poll_descriptors_revents(capt, &p->addr[ci], p->capt_nfds, &revents);
            SndReturn_(err, "Failed: snd_pcm_poll_descriptors_revents (capt): %s");
            if (revents & POLLNVAL) { SndReturn_(-POLLNVAL, "Device disconnected (capt): %s"); }
            if (revents & POLLERR) { xrun_detected = true; }
            if (revents & POLLOUT) { need_capture = 0; } // NOTE: Success?
        }
    }

    if (xrun_detected)
    {
        // TODO: Request restart instead, we are now tracking retry_count within p

        // NOTE: Run xrun recovery preparation
        err = sndx_pollfds_xrun(p, play, capt, output);
        SndReturn_(err, "Poll time out, xrun recovery failed with status = %d: %s", err);
    }

    // All good, so resetting retry_count
    p->retry_count = 0;

    // NOTE: jack also gets avail here as minimum of capt and play
    // we extract it to a different function

    return POLLFD_SUCCESS;
}

// int sndx_pollfds_avail(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output)
// {
//     return POLLFD_SUCCESS;
// }
