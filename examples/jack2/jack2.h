/** @file waits.h
 *  @brief Various implementations of waiting on pollfds
 */
#pragma once

#include <limits.h>

// static inline u64 get_microseconds()
// {
//     u64     utime;
//     tspec_t time;
//     clock_gettime(CLOCK_MONOTONIC, &time);
//     utime = (u64)time.tv_sec * 1e6 + (u64)time.tv_nsec / 1e3;
//     return utime;
// }

// static int alsa_driver_restart(sndx_duplex_t* driver)
// {
//     int res;
//
//     driver->xrun_recovery = 1;
//     // JACK2
//     /*
//     if ((res = driver->nt_stop((struct _jack_driver_nt *) driver))==0)
//         res = driver->nt_start((struct _jack_driver_nt *) driver);
//     */
//     res                   = Restart();
//     driver->xrun_recovery = 0;
//
//     if (res && driver->midi) (driver->midi->stop)(driver->midi);
//
//     return res;
// }

// /** @brief Reimplementation of alsa_driver_xrun_recovery with sndx_duplex_t as driver.
//  *
//  *  Details.
//  */
// static int sndx_xrun_recovery_jack2(sndx_duplex_t* d, float* delayed_usecs, sndx_duplex_restart_fn restart_fn)
// {
//     output_t* output = d->out;
//     status_t* status;
//     int       err;
//
//     snd_pcm_status_alloca(&status);
//
//     if (d->capt)
//     {
//         err = snd_pcm_status(d->capt, status);
//         SndCheck_(err, "status error: %s");
//     }
//     else {
//         err = snd_pcm_status(d->play, status);
//         SndCheck_(err, "status error: %s");
//     }
//
//     if (snd_pcm_status_get_state(status) == SND_PCM_STATE_SUSPENDED)
//     {
//         a_info("**** alsa_pcm: pcm in suspended state, resuming it");
//         if (d->capt)
//         {
//             err = snd_pcm_prepare(d->capt);
//             SndCheck_(err, "error preparing capture after suspend: %s");
//         }
//         if (d->play)
//         {
//             err = snd_pcm_prepare(d->play);
//             SndCheck_(err, "error preparing playback after suspend: %s");
//         }
//     }
//
//     if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN && d->pfds.process_count > XRUN_REPORT_DELAY)
//     {
//         struct timeval now, diff, tstamp;
//         d->pfds.xrun_count++;
//         snd_pcm_status_get_tstamp(status, &now);
//         snd_pcm_status_get_trigger_tstamp(status, &tstamp);
//         timersub(&now, &tstamp, &diff);
//         *delayed_usecs = diff.tv_sec * 1000000.0 + diff.tv_usec;
//         a_info("**** alsa_pcm: xrun of at least %.3f msecs", *delayed_usecs / 1000.0);
//         if (d->capt)
//         {
//             a_info("Repreparing capture");
//             err = snd_pcm_prepare(d->capt);
//             SndCheck_(err, "error preparing capture after xrun: %s");
//         }
//         if (d->play)
//         {
//             a_info("Repreparing playback");
//             err = snd_pcm_prepare(d->play);
//             SndCheck_(err, "error preparing playback after xrun: %s");
//         }
//     }
//
//     // NOTE: Actually just stop and start everything
//     if (restart_fn(d)) { return -1; }
//
//     return 0;
// }

// /** @brief Reimplementation of alsa_driver_wait with sndx_duplex_t as driver.
//  *
//  *  Details.
//  */
// uframes_t sndx_duplex_wait_jack2(
//     sndx_duplex_t* d, int extra_fd, int* status, float* delayed_usecs, sndx_duplex_restart_fn restart_fn)
// {
//     output_t* output = d->out;
//
//     // 1. args
//     a_debug("extra_fd       : %d", extra_fd);
//     a_debug("status         : %d", *status);
//     a_debug("delayed_usecs  : %f", *delayed_usecs);
//
//     // 2. Locals
//     bool need_capture, need_playback;
//     u64  poll_enter, poll_ret;
//
//     u32       retry_cnt      = 0;
//     bool      xrun_detected  = false;
//     sframes_t avail          = 0;
//     sframes_t capture_avail  = 0;
//     sframes_t playback_avail = 0;
//
//     *status        = -1;
//     *delayed_usecs = 0;
//
//     // 3. Set need_playback based on extra_fd
//     need_capture  = d->capt != nullptr;
//     need_playback = extra_fd >= 0 ? false : d->play != nullptr;
//
//     // 4. Mark start of while loop (to return for gdb/signal error)
// again:
//
//     // 5. --- Start of loop ---
//     while ((need_playback || need_capture) && !xrun_detected)
//     {
//         // L1. More locals
//         int poll_result;
//         // u32 ci = 0; // Needed to know whether we skipped playback or not
//         u32 nfds;
//
//         nfds = 0;
//
//         // L2. for playback and capture, `snd_pcm_poll_desciptors`, add to nfds
//         if (need_playback)
//         {
//             snd_pcm_poll_descriptors(d->play, &d->pfds.addr[0], d->pfds.play_nfds);
//             nfds += d->pfds.play_nfds;
//         }
//
//         if (need_capture)
//         {
//             snd_pcm_poll_descriptors(d->capt, &d->pfds.addr[nfds], d->pfds.capt_nfds);
//             nfds += d->pfds.capt_nfds;
//         }
//
//         // L3. Set poll events including `POLLERR`
//         RANGE(i, nfds) { d->pfds.addr[i].events |= POLLERR; }
//
//         if (extra_fd >= 0)
//         {
//             d->pfds.addr[nfds].fd     = extra_fd;
//             d->pfds.addr[nfds].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
//             nfds++;
//         }
//
//         // L4. Mark start time as `poll_enter` (`jack_get_microseconds`)
//         poll_enter = get_microseconds();
//
//         // L5. `if (poll_enter > driver->poll_next)`
//         //    - This processing cycle was delayed past the next due interrupt! Do not account this as a wakeup delay:
//         //    - `driver->poll_next = 0; driver->poll_late++;`
//         if (poll_enter > d->pfds.poll_next)
//         {
//             /*
//              * This processing cycle was delayed past the
//              * next due interrupt!  Do not account this as
//              * a wakeup delay:
//              */
//             d->pfds.poll_next = 0;
//             d->pfds.poll_late++;
//         }
//
//         // L6. `poll_result = poll(...)`
//         //    - if `poll_result < 0`:
//         //      - check for `EINTR`
//         //        - if `under_gdb`, just restart poll loop
//         //        - else print error and return 0, with `status = -2`
//         //      - print error and return 0, with `status -3`
//         poll_result = poll(d->pfds.addr, nfds, d->pfds.poll_timeout);
//
//         if (poll_result < 0)
//         {
//             if (errno == EINTR)
//             {
//                 const char poll_log[] = "ALSA: poll interrupt";
//                 // this happens mostly when run
//                 // under gdb, or when exiting due to a signal
//                 if (under_gdb)
//                 {
//                     a_error(poll_log);
//                     goto again;
//                 }
//                 a_error(poll_log);
//                 *status = -2; // XXX: Exit point -2
//                 return 0;
//             }
//
//             a_error("ALSA: poll call failed (%s)", strerror(errno));
//             *status = -3; // XXX: Exit point -3
//             return 0;
//         }
//
//         // L7. Mark end time as `poll_ret` (`jack_get_microseconds`)
//         poll_ret = get_microseconds();
//
//         // L8. Check `poll_result`:
//         //    - if `== 0`
//         //      - increment `retry_cnt`
//         //        - if already past max, return with `status = -5`
//         //      - print error
//         //      - do `xrun_recovery`
//         //        - if recovery failed, return `status` = error from `xrun_recovery`
//         if (poll_result == 0)
//         {
//             retry_cnt++;
//             if (retry_cnt > MAX_RETRY_COUNT)
//             {
//                 a_error("ALSA: poll time out, polled for %ld usecs, Reached max retry cnt = %d, Exiting", //
//                         poll_ret - poll_enter, MAX_RETRY_COUNT);
//                 *status = -5; // XXX: Exit point -5
//                 return 0;
//             }
//             a_error("ALSA: poll time out, polled for %ld usecs, Retrying with a recovery, retry cnt = %d",
//                     poll_ret - poll_enter, retry_cnt);
//             *status = sndx_xrun_recovery_jack2(d, delayed_usecs, restart_fn); // XXX: Xrun point 1
//             if (*status != 0)
//             {
//                 a_error("ALSA: poll time out, recovery failed with status = %d", *status);
//                 return 0;
//             }
//         }
//
//         // L9. Set jack time with `poll_ret`
//
//         // L10. Check `extra_fd`
//         //     - if `< 0`, update `poll_last`, `poll_next`, `delayed_usecs`
//         //     - if `>= 0`:
//         //       - check `driver->pfd[nfds-1].revents == 0`
//         //         - if so, return -1, with `status = -4`
//         //       - reset status to 0
//         //       - check if only `POLLIN` was set
//         if (extra_fd < 0)
//         {
//             if (d->pfds.poll_next && poll_ret > d->pfds.poll_next) { *delayed_usecs = poll_ret - d->pfds.poll_next; }
//             d->pfds.poll_last = poll_ret;
//             d->pfds.poll_next = poll_ret + d->pfds.period_usecs;
//             // JACK2
//             /* driver->engine->transport_cycle_start (driver->engine, poll_ret); */
//         }
//
//         a_debug("%ld: checked %d fds, started at %ld %ld  usecs since poll entered", //
//                 poll_ret, nfds, poll_enter, poll_ret - poll_enter);
//
//         if (extra_fd >= 0)
//         {
//             if (d->pfds.addr[nfds - 1].revents == 0)
//             {
//                 /* we timed out on the extra fd */
//                 *status = -4; // XXX: Exit point -4
//                 return -1;
//             }
//             /* if POLLIN was the only bit set, we're OK */
//             *status = 0;
//             return (d->pfds.addr[nfds - 1].revents == POLLIN) ? 0 : -1; // XXX: Exit point, nframes = -1
//         }
//
//         // L11. For playback and capture, `snd_pcm_poll_descriptors_revents`
//         //     - if `POLLINVAL`, return 0 with `status = -7`
//         //     - if `POLLERR`, set `xrun_detected` and continue
//         //     - if `POLLOUT`, set `need_playback/need_capture` to 0 and continue
//         u16 revents = 0;
//         if (need_playback)
//         {
//             if (snd_pcm_poll_descriptors_revents(d->play, &d->pfds.addr[0], d->pfds.play_nfds, &revents) < 0)
//             {
//                 a_error("ALSA: playback revents failed");
//                 *status = -6; // XXX: Exit point -6
//                 return 0;
//             }
//             if (revents & POLLNVAL)
//             {
//                 a_error("ALSA: playback device disconnected");
//                 *status = -7; // XXX: Exit point -7
//                 return 0;
//             }
//             if (revents & POLLERR) { xrun_detected = true; }
//             if (revents & POLLOUT)
//             {
//                 need_playback = 0;
//                 a_debug("%ld playback stream ready", poll_ret);
//             }
//         }
//
//         if (need_capture)
//         {
//             u32 ci = need_playback ? d->pfds.play_nfds : 0;
//             if (snd_pcm_poll_descriptors_revents(d->capt, &d->pfds.addr[ci], d->pfds.capt_nfds, &revents) < 0)
//             {
//                 a_error("ALSA: capture revents failed");
//                 *status = -6; // XXX: Exit point -6
//                 return 0;
//             }
//             if (revents & POLLNVAL)
//             {
//                 a_error("ALSA: capture device disconnected");
//                 *status = -7; // XXX: Exit point -7
//                 return 0;
//             }
//             if (revents & POLLERR) { xrun_detected = true; }
//             if (revents & POLLOUT)
//             {
//                 need_capture = 0;
//                 a_debug("%ld capture stream ready", poll_ret);
//             }
//         }
//
//         // --- End of while loop ---
//     }
//
//     // ========= Finally, start checking avail =========
//
//     // 6. For playback and capture, check `snd_pcm_avail_update`
//     //   - if -EPIPE, set `xrun_detected` and continue
//     //   - else print error and continue
//     //   - if handles dont exist, set avail to max, as we will take min of playback and capture later
//     if (d->capt)
//     {
//         if ((capture_avail = snd_pcm_avail_update(d->capt)) < 0)
//         {
//             if (capture_avail == -EPIPE) { xrun_detected = true; }
//             else {
//                 a_error("unknown ALSA avail_update return value (%ld)", capture_avail);
//             }
//         }
//     }
//     else {
//         /* odd, but see min() computation below */
//         capture_avail = INT_MAX;
//     }
//
//     if (d->play)
//     {
//         if ((playback_avail = snd_pcm_avail_update(d->play)) < 0)
//         {
//             if (playback_avail == -EPIPE) { xrun_detected = true; }
//             else {
//                 a_error("unknown ALSA avail_update return value (%ld)", playback_avail);
//             }
//         }
//     }
//     else {
//         /* odd, but see min() computation below */
//         playback_avail = INT_MAX;
//     }
//
//     // 7. if `xrun_detected`
//     //   - `*status = alsa_driver_xrun_recovery (driver, delayed_usecs)`
//     //   - return 0 -> i.e. nothing available
//     if (xrun_detected)
//     {
//         *status = sndx_xrun_recovery_jack2(d, delayed_usecs, restart_fn);
//         return 0; // XXX: Exit point sndx_xrun_recovery_jack2
//     }
//
//     // 8. If we reached here, all good, set `*status = 0`, `driver->last_wait_ust = poll_ret;`
//     *status               = 0;
//     d->pfds.last_wait_ust = poll_ret;
//
//     // 9. Set `avail` to minimum of capture and playback avail
//     avail = capture_avail < playback_avail ? capture_avail : playback_avail;
//     a_debug("wakeup complete, avail = %lu, pavail = %lu cavail = %lu", //
//             avail, playback_avail, capture_avail);
//
//     // 10. Mark channels done `bitset_copy`
//
//     // 11. Finally, return `avail - (avail % driver->frames_per_cycle)`
//     return avail - (avail % d->period_size);
// }
