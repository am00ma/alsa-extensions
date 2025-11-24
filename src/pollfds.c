#include "pollfds.h"
#include "timer.h" // get_microseconds

#define MAX_RETRY_COUNT 5

int sndx_pollfds_open( //
    sndx_pollfds_t** pfdsp,
    snd_pcm_t*       play,
    snd_pcm_t*       capt,
    u32              rate,
    u64              period_size,
    snd_output_t*    output)
{
    int err;

    sndx_pollfds_t* p;

    // NOTE: calloc resets all stats to zero
    p = calloc(1, sizeof(*p));
    RetVal_(!p, -ENOMEM, "Failed calloc pollfds_t* p");

    p->rate        = rate;
    p->period_size = period_size;

    p->period_usecs = (u64)floor((((float)p->period_size) / p->rate) * 1000000.0f);
    p->poll_timeout = (int)floor(1.5f * p->period_usecs);

    p->play_nfds = snd_pcm_poll_descriptors_count(play);
    p->capt_nfds = snd_pcm_poll_descriptors_count(capt);

    p->addr = calloc(p->play_nfds + p->capt_nfds, sizeof(pfd_t));
    err     = -(!p->addr);
    Goto_(err, __close, "Failed calloc pft_t* addr");

    *pfdsp = p;

    return 0;

__close:
    sndx_pollfds_close(p);
    *pfdsp = nullptr;

    return err;
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

    p->play_nfds = snd_pcm_poll_descriptors_count(play);
    p->capt_nfds = snd_pcm_poll_descriptors_count(capt);

    Free(p->addr);
    p->addr = calloc(p->play_nfds + p->capt_nfds, sizeof(pfd_t));
    RetVal_(!p->addr, -ENOMEM, "Failed: calloc pft_t* addr while resetting poll fds");

    return 0;
}

void sndx_pollfds_xrun(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output)
{

    // NOTE: Strangely, none of these return on error, instead print error and continue ??

    int err;

    // Asserting here to show that sndx_pollfds_xrun can be of return type void
    AssertMsg(play != nullptr, "Invalid playback handle");
    AssertMsg(capt != nullptr, "Invalid capture handle");

    status_t* status;
    snd_pcm_status_alloca(&status);

    // NOTE: Jack only checks capt, since it uses `if (capt) {...}`,
    //       and we are assured capture handle
    err = snd_pcm_status(capt, status);
    SndCheck_(err, "Failed: snd_pcm_status (capt): %s");

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

    // TODO: Jack calls restart here, only point of failure, all else are just checks
}

sndx_pollfds_error_t sndx_pollfds_wait(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output)
{
    int err;

    u64 poll_enter  = 0;
    u64 poll_ret    = 0;
    int poll_result = 0;
    u16 revents     = 0;

    // NOTE: Jack does not 'listen' to playback if extra_fd is present

    bool xrun_detected = false; ///< Got xrun, so restrart
    bool need_playback = true;  ///< Waiting for playback poll
    bool need_capture  = true;  ///< Waiting for capture poll

    p->delayed_usecs = 0; ///< Reset delayed_usecs

    // Assuming both playback and capture handles are not null
    // so we avoid all the checks like jack for now
    RetVal_(play == nullptr, POLLFD_FATAL, "Invalid playback handle");
    RetVal_(capt == nullptr, POLLFD_FATAL, "Invalid capture handle");

    // Keeping track of retry count here as opposed to jack
    if (p->retry_count != 0) a_info("Wait: retrying: %d", p->retry_count);

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

        RANGE(i, nfds) { p->addr[i].events |= POLLERR; }

        poll_enter = get_microseconds(); // system time

        // NOTE: This will always be true on the first cycle, as we dont set poll_next
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
            if (errno == EINTR) { RetVal_(EINTR, POLLFD_FATAL, "Poll call failed due to interrupt"); }
            RetVal_(EINTR, POLLFD_FATAL, "Poll call failed due to interrupt");
        }

        poll_ret = get_microseconds(); // system time

        if (poll_result == 0)
        {
            p->retry_count++;
            if (p->retry_count > MAX_RETRY_COUNT)
            {
                RetVal_(-1, POLLFD_FATAL,
                        "Poll time out"
                        "   Polled for            = %ld usecs\n"
                        "   Reached max retry cnt = %d       \n"
                        "Exiting... ",
                        poll_ret - poll_enter, MAX_RETRY_COUNT);
            }

            // NOTE: Request restart instead, we are now tracking retry_count within p
            //       jack instead calls `xrun_recovery`
            RetVal_(-1, POLLFD_NEEDS_RESTART,                   //
                    "Poll time out, polled for %ld usecs,"      //
                    "Retrying with a recovery, retry cnt = %d", //
                    poll_ret - poll_enter, p->retry_count);
        }

        // Reset on successful poll
        p->retry_count = 0;

        // Compute delay if poll_next is known and was underestimated
        if (p->poll_next && poll_ret > p->poll_next) { p->delayed_usecs = poll_ret - p->poll_next; }

        // poll_next is set here for first time
        p->poll_last = poll_ret;
        p->poll_next = poll_ret + p->period_usecs;

        // NOTE: If poll() sets the POLLOUT flag then:
        //   -> at least one byte may be written without blocking
        //
        // - Assuming this means success:
        //      - Implies that need_capture, need_playback should both be satisfied for 'successful' wait
        //          - i.e. we are waiting on both, and only wake when both are available
        //      - Also implies that loop breaks in case of xrun on either play/capt
        //          - So then, how do we recover and prepare the correct device?
        //              - Jack always 'prepares' capture if available
        //          - Probably the reason we 'start' and 'stop' completely instead?
        // - Does this mean that minimum of `period_size` is available to read/write?

        if (need_playback)
        {
            err = snd_pcm_poll_descriptors_revents(play, &p->addr[0], p->play_nfds, &revents);
            SndRetVal_(err, POLLFD_FATAL, "Failed: snd_pcm_poll_descriptors_revents (play): %s");
            if (revents & POLLNVAL) SndRetVal_(-POLLNVAL, POLLFD_FATAL, "Device disconnected (play): %s");
            if (revents & POLLERR)
            {
                xrun_detected = true;
                a_error("playback xrun");
            } // Failure
            if (revents & POLLOUT) // NOTE: POLLOUT for playback
            {
                need_playback = 0;
            } // Success
        }

        if (need_capture)
        {
            err = snd_pcm_poll_descriptors_revents(capt, &p->addr[ci], p->capt_nfds, &revents);
            SndRetVal_(err, POLLFD_FATAL, "Failed: snd_pcm_poll_descriptors_revents (capt): %s");
            if (revents & POLLNVAL) SndRetVal_(-POLLNVAL, POLLFD_FATAL, "Device disconnected (capt): %s");
            if (revents & POLLERR)
            {
                xrun_detected = true;
                a_error("capture xrun");
            } // Failure
            if (revents & POLLIN) // NOTE: POLLIN for capture
            {
                need_capture = 0;
            } // Success
        }
    }

    // NOTE: Request restart instead, we are now tracking retry_count within p
    RetVal_(xrun_detected, POLLFD_NEEDS_RESTART, "Xrun detected, needs retart");

    // NOTE: jack also gets avail here as minimum of capt and play
    // we extract it to a different function

    // NOTE: This is done after avail, but probably is fine here
    p->last_wait_ust = poll_ret;

    return POLLFD_SUCCESS;
}

sndx_pollfds_error_t sndx_pollfds_avail( //
    sndx_pollfds_t* p,
    snd_pcm_t*      play,
    snd_pcm_t*      capt,
    sframes_t*      avail,
    output_t*       output)
{
    // reason for no INT_MAX
    RetVal_(play == nullptr, POLLFD_FATAL, "Invalid playback handle");
    RetVal_(capt == nullptr, POLLFD_FATAL, "Invalid capture handle");

    // NOTE: In case of unknown avail_update, using FATAL
    //       jack just uses the value after printing an error

    sframes_t capt_avail = snd_pcm_avail_update(capt);
    if (capt_avail < 0)
    {
        if (capt_avail == -EPIPE) { return POLLFD_NEEDS_RESTART; }
        else RetVal_(capt_avail, POLLFD_FATAL, "Unknown avail_update value: %ld", capt_avail);
    }

    sframes_t play_avail = snd_pcm_avail_update(play);
    if (play_avail < 0)
    {
        if (play_avail == -EPIPE) { return POLLFD_NEEDS_RESTART; }
        else RetVal_(play_avail, POLLFD_FATAL, "Unknown avail_update value: %ld", play_avail);
    }

    // Choose the minimum of the two
    *avail = capt_avail < play_avail ? capt_avail : play_avail;

    // NOTE: Another connection with duplex, need period_size
    *avail = *avail - (*avail % p->period_size);

    return POLLFD_SUCCESS;
}
