#include "pollfds.h"
#include "timer.h"

#define MAX_RETRY_COUNT 5

int sndx_pollfds_open(sndx_pollfds_t** pfdsp, snd_pcm_t* play, snd_pcm_t* capt, snd_output_t* output)
{

    sndx_pollfds_t* p;
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

    free(p);
    p = nullptr;
}

sndx_pollfds_poll_error_t sndx_pollfds_poll(sndx_pollfds_t* p, snd_pcm_t* play, snd_pcm_t* capt, output_t* output)
{
    int err;

    u32    retry_cnt     = 0;
    float* delayed_usecs = 0;
    u64    poll_enter    = 0;
    u64    poll_ret      = 0;
    int    poll_result   = 0;
    u16    revents;

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
            SysRetVal_(errno, -POLLFD_FATAL, "Poll call failed  %s");
        }

        poll_ret = get_microseconds();

        if (poll_result == 0)
        {
            retry_cnt++;
            if (retry_cnt > MAX_RETRY_COUNT)
            {
                RetVal_(errno, -POLLFD_FATAL,                                                       //
                        "Poll time out, polled for %ld usecs, Reached max retry cnt = %d, Exiting", //
                        poll_ret - poll_enter, MAX_RETRY_COUNT);
            }

            a_error("Poll time out, polled for %ld usecs, Retrying with a recovery, retry cnt = %d", //
                    poll_ret - poll_enter, retry_cnt);

            // TODO: Run xrun recovery, but keeping track of retry count, i.e. restart the driver, if that fails, is fatal
            // err = alsa_driver_xrun_recovery(d, delayed_usecs);
            // RetVal_(err, -POLLFD_FATAL, "Poll time out, recovery failed with status = %d", err);
        }

        if (p->poll_next && poll_ret > p->poll_next) { *delayed_usecs = poll_ret - p->poll_next; }
        p->poll_last = poll_ret;
        p->poll_next = poll_ret + p->period_usecs;

        if (need_playback)
        {
            err = snd_pcm_poll_descriptors_revents(play, &p->addr[0], p->play_nfds, &revents);
            RetVal_(err, -POLLFD_FATAL, "Playback revents failed");
            if (revents & POLLNVAL) { RetVal_(-POLLNVAL, -POLLFD_FATAL, "Playback device disconnected"); }
            if (revents & POLLERR) { xrun_detected = true; }
            if (revents & POLLOUT) { need_playback = 0; }
        }

        if (need_capture)
        {
            err = snd_pcm_poll_descriptors_revents(capt, &p->addr[ci], p->capt_nfds, &revents);
            RetVal_(err, -POLLFD_FATAL, "Capture revents failed");
            if (revents & POLLNVAL) { RetVal_(-POLLNVAL, -POLLFD_FATAL, "Capture device disconnected"); }
            if (revents & POLLERR) { xrun_detected = true; }
            if (revents & POLLOUT) { need_capture = 0; }
        }
    }

    if (xrun_detected)
    {
        // TODO: Basically request to restart the driver, will reset retry count
        // err = alsa_driver_xrun_recovery(p, delayed_usecs);
        // return 0;
    }

    return 0;
}
