#include "pollfds.h"
#include "xrun.h"

int sndx_pollfds_open(sndx_pollfds_t** fdsp, snd_pcm_t* play, snd_pcm_t* capt, output_t* output)
{
    int err;

    sndx_pollfds_t* fds;

    fds = calloc(1, sizeof(*fds));
    if (!*fdsp) return -ENOMEM;

    err = snd_pcm_poll_descriptors_count(play);
    SndReturn_(err, "Failed: snd_pcm_poll_descriptors_count: %s");
    fds->play_nfds = err;

    err = snd_pcm_poll_descriptors_count(capt);
    SndReturn_(err, "Failed: snd_pcm_poll_descriptors_count: %s");
    fds->capt_nfds = err;

    fds->nfds = fds->play_nfds + fds->capt_nfds;
    fds->pfds = calloc(fds->nfds, sizeof(pfd_t));

    *fdsp = fds;

    return 0;
}

void sndx_pollfds_close(sndx_pollfds_t* obj)
{
    free(obj->pfds);
    free(obj);
}

int sndx_pollfds_wait_jack2(       //
    snd_pcm_t*      capt,          //
    snd_pcm_t*      play,          //
    sndx_pollfds_t* poll_fds,      //
    int             poll_timeout,  //
    int             period_size,   //
    int*            status,        //
    float*          delayed_usecs, //
    output_t*       output)
{
    int err;
    u32 i;

    sframes_t avail          = 0;
    sframes_t capture_avail  = 0;
    sframes_t playback_avail = 0;

    int xrun_detected = 0;
    int retry_cnt     = 0;

    int need_capture;
    int need_playback;

    // jack_time_t  poll_enter;
    // jack_time_t  poll_ret = 0;

    *status        = -1;
    *delayed_usecs = 0;

    need_capture  = capt ? 1 : 0;
    need_playback = play ? 1 : 0;

    // TODO: Why label not used?
    // again:

    while ((need_playback || need_capture) && !xrun_detected)
    {

        int            poll_result;
        u32            ci = 0;
        u32            nfds;
        unsigned short revents;

        nfds = 0;

        if (need_playback)
        {
            snd_pcm_poll_descriptors(play, &poll_fds->pfds[0], poll_fds->play_nfds);
            nfds += poll_fds->play_nfds;
        }
        if (need_capture)
        {
            snd_pcm_poll_descriptors(capt, &poll_fds->pfds[nfds], poll_fds->capt_nfds);
            ci    = nfds;
            nfds += poll_fds->capt_nfds;
        }

        /* ALSA doesn't set POLLERR in some versions of 0.9.X */
        for (i = 0; i < nfds; i++) { poll_fds->pfds[i].events |= POLLERR; }

        poll_result = poll(poll_fds->pfds, nfds, poll_timeout);

        if (poll_result < 0)
        {
            if (errno == EINTR)
            {
                a_error("ALSA: poll interrupt (%s)", strerror(errno));
                *status = -2;
                return 0;
            }

            a_error("ALSA: poll call failed (%s)", strerror(errno));
            *status = -3;
            return 0;
        }

        if (poll_result == 0)
        {
            retry_cnt++;
            if (retry_cnt > MAX_RETRY_COUNT)
            {
                a_error("ALSA: poll time out, Reached max retry cnt = %d, Exiting", MAX_RETRY_COUNT);
                *status = -5;
                return 0;
            }
            a_error("ALSA: poll time out, Retrying with a recovery, retry cnt = %d", retry_cnt);

            float delayed_usecs = 0;

            *status = sndx_xrun_recovery_jack2(capt, play, &delayed_usecs, output);

            if (*status != 0)
            {
                a_error("ALSA: poll time out, recovery failed with status = %d", *status);
                return 0;
            }
        }

        if (need_playback)
        {
            err = snd_pcm_poll_descriptors_revents(play, &poll_fds->pfds[0], poll_fds->play_nfds, &revents);
            if (err < 0)
            {
                a_error("ALSA: playback revents failed");
                *status = -6;
                return 0;
            }

            if (revents & POLLNVAL)
            {
                a_error("ALSA: playback device disconnected");
                *status = -7;
                return 0;
            }

            if (revents & POLLERR) { xrun_detected = 1; }

            if (revents & POLLOUT) { need_playback = 0; }
        }

        if (need_capture)
        {
            if (snd_pcm_poll_descriptors_revents(capt, &poll_fds->pfds[ci], poll_fds->capt_nfds, &revents) < 0)
            {
                a_error("ALSA: capture revents failed");
                *status = -6;
                return 0;
            }

            if (revents & POLLNVAL)
            {
                a_error("ALSA: capture device disconnected");
                *status = -7;
                return 0;
            }
            if (revents & POLLERR) { xrun_detected = 1; }
            if (revents & POLLIN) { need_capture = 0; }
        }
    }

    if (capt)
    {
        if ((capture_avail = snd_pcm_avail_update(capt)) < 0)
        {
            if (capture_avail == -EPIPE) { xrun_detected = 1; }
            else {
                a_error("unknown ALSA avail_update return value (%ld)", capture_avail);
            }
        }
    }
    else {
        /* odd, but see min() computation below */
        capture_avail = __INT_MAX__;
    }

    if (play)
    {
        if ((playback_avail = snd_pcm_avail_update(play)) < 0)
        {
            if (playback_avail == -EPIPE) { xrun_detected = 1; }
            else {
                a_error("unknown ALSA avail_update return value (%ld)", playback_avail);
            }
        }
    }
    else {
        /* odd, but see min() computation below */
        playback_avail = __INT_MAX__;
    }

    if (xrun_detected)
    {
        float delayed_usecs = 0;

        *status = sndx_xrun_recovery_jack2(capt, play, &delayed_usecs, output);
        return 0;
    }

    *status = 0;

    avail = capture_avail < playback_avail ? capture_avail : playback_avail;

    return avail - (avail % period_size);
}
