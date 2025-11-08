# Waits

1. Blocking read/write
2. `snd_pcm_wait`
3. `snd_pcm_async`
4. `poll`
5. `select`
6. `uring`?

| no  | program         | wait method                             | comments |
| --- | --------------- | --------------------------------------- | -------- |
| 1   | pcm-simple      | na                                      |          |
| 2   | pcm             | `wait`, `poll`, `async`                 |          |
| 3   | pcm-multithread |                                         |          |
| 4   | audio-timer     |                                         |          |
| 5   | latency         |                                         |          |
| 6   | aplay           |                                         |          |
| 7   | axfer           |                                         |          |
| 8   | alsaloop        |                                         |          |
| 9   | jack2           | `poll`                                  |          |
| 10  | juce            | `snd_pcm_wait`                          |          |
| 11  | rtaudio         | no waiting (wierdly, also non-blocking) |          |

## snd wait

- locks mutex and calls `__snd_pcm_wait_in_lock`
- check pcm state, check if may wait for avail min
- call `snd_pcm_wait_nocheck` -> macro, cannot find source code

Pseudocode:

1. Locals:
   - `pfd_t* pfd`
   - `u16 revents`
   - `i32 npfds`
   - `i32 err`
   - `i32 err_poll`

2. `snd_pcm_poll_desciptors_count`
3. `pfd = alloca(npfds)`
4. `snd_pcm_poll_desciptors`
   - `if < 0`: return err
   - `if err != npfds`: return `-EIO`
5. if `timeout` is specified, set appropriate `io_timeout`, `drain_timeout`
6. `do {...} while(!(revents & (POLLIN | POLLOUT)))`
   - unlock
   - `err_poll = poll(...)`
     - `if < 0`:
       - continue if `errno == EINTR && !PCMINABORT(pcm) && !(pcm->mode & SND_PCM_EINTR)`
       - else return `-errno`
   - `if !err_poll`:
     - break the while loop
   - `err = snd_pcm_poll_revents(...)`
     - `if < 0`: return err
     - `if (revents & (POLLERR | POLLNVAL))`:
       - `pcm_state_to_error(__snd_pcm_state(pcm))`
       - return `err < 0 ? err : -EIO`
7. Check optional errors
   - `avail_update = snd_pcm_avail_update(...)`
   - if `avail_update < avail_min`
     - print error and continue
8. Return `err_poll > 0 ? 1 : 0`

```c
/*
 * like snd_pcm_wait() but doesn't check mmap_avail before calling poll()
 *
 * used in drain code in some plugins
 *
 * This function is called inside pcm lock.
 */
int snd_pcm_wait_nocheck(snd_pcm_t *pcm, int timeout)
{
	struct pollfd *pfd;
	unsigned short revents = 0;
	int npfds, err, err_poll;

	npfds = __snd_pcm_poll_descriptors_count(pcm);
	if (npfds <= 0 || npfds >= 16) {
		SNDERR("Invalid poll_fds %d", npfds);
		return -EIO;
	}
	pfd = alloca(sizeof(*pfd) * npfds);
	err = __snd_pcm_poll_descriptors(pcm, pfd, npfds);
	if (err < 0)
		return err;
	if (err != npfds) {
		SNDMSG("invalid poll descriptors %d", err);
		return -EIO;
	}
	if (timeout == SND_PCM_WAIT_IO)
		timeout = __snd_pcm_wait_io_timeout(pcm);
	else if (timeout == SND_PCM_WAIT_DRAIN)
		timeout = __snd_pcm_wait_drain_timeout(pcm);
	else if (timeout < -1)
		SNDMSG("invalid snd_pcm_wait timeout argument %d", timeout);
	do {
		__snd_pcm_unlock(pcm->fast_op_arg);
		err_poll = poll(pfd, npfds, timeout);
		__snd_pcm_lock(pcm->fast_op_arg);
		if (err_poll < 0) {
			if (errno == EINTR && !PCMINABORT(pcm) && !(pcm->mode & SND_PCM_EINTR))
		                continue;
			return -errno;
                }
		if (! err_poll)
			break;
		err = __snd_pcm_poll_revents(pcm, pfd, npfds, &revents);
		if (err < 0)
			return err;
		if (revents & (POLLERR | POLLNVAL)) {
			/* check more precisely */
			err = pcm_state_to_error(__snd_pcm_state(pcm));
			return err < 0 ? err : -EIO;
		}
	} while (!(revents & (POLLIN | POLLOUT)));
#if 0 /* very useful code to test poll related problems */
	{
		snd_pcm_sframes_t avail_update;
		__snd_pcm_hwsync(pcm);
		avail_update = __snd_pcm_avail_update(pcm);
		if (avail_update < (snd_pcm_sframes_t)pcm->avail_min) {
			printf("*** snd_pcm_wait() FATAL ERROR!!!\n");
			printf("avail_min = %li, avail_update = %li\n", pcm->avail_min, avail_update);
		}
	}
#endif
	return err_poll > 0 ? 1 : 0;
}
```

## jack2 wait

Pseudocode:

1. Args:
   - `status`
   - `delayed_usecs`
   - `extra_fd`
   - Returns: Number of frames available

2. Locals:
   - `avail`, `capture_avail`, `playback_avail`
   - `need_capture`, `need_playback`
   - `xrun_detected`
   - `retry_cnt`
   - `poll_enter`, `poll_ret`

3. if `extra_fd >=0`, set `need_playback` to 0 TODO: Why??
4. Mark start of while loop with label `again`

5. `while ((need_playback || need_capture) && !xrun_detected)`
   1. More locals:
      - `poll_result`
      - `ci`
      - `nfds`
      - `revents`
   2. for playback and capture, `snd_pcm_poll_desciptors`, add to nfds
   3. Set poll events including `POLLERR`
   4. Mark start time as `poll_enter` (`jack_get_microseconds`)
   5. `if (poll_enter > driver->poll_next)`
      - This processing cycle was delayed past the next due interrupt! Do not account this as a wakeup delay:
      - `driver->poll_next = 0; driver->poll_late++;`
   6. `poll_result = poll(...)`
      - if `poll_result < 0`:
        - check for `EINTR`
          - if `under_gdb`, just restart poll loop
          - else print error and return 0, with `status = -2`
        - print error and return 0, with `status -3`
   7. Mark end time as `poll_ret` (`jack_get_microseconds`)
   8. Check `poll_result`:
      - if `== 0`
        - increment `retry_cnt`
          - if already past max, return with `status = -5`
        - print error
        - do `xrun_recovery`
          - if recovery failed, return `status` = error from `xrun_recovery`
   9. Set jack time with `poll_ret`
   10. Check `extra_fd`
       - if `< 0`, update `poll_last`, `poll_next`, `delayed_usecs`
       - if `>= 0`:
         - check `driver->pfd[nfds-1].revents == 0`
           - if so, return -1, with `status = -4`
         - reset status to 0
         - check if only `POLLIN` was set
   11. For playback and capture, `snd_pcm_poll_descriptors_revents`
       - if `POLLINVAL`, return 0 with `status = -7`
       - if `POLLERR`, set `xrun_detected` and continue
       - if `POLLOUT`, set `need_playback/need_capture` to 0 and continue
6. For playback and capture, check `snd_pcm_avail_update`
   - if -EPIPE, set `xrun_detected` and continue
   - else print error and continue
   - if handles dont exist, set avail to max, as we will take min of playback and capture later
7. if `xrun_detected`
   - `*status = alsa_driver_xrun_recovery (driver, delayed_usecs)`
   - return 0 -> i.e. nothing available
8. If we reached here, all good, set `*status = 0`, `driver->last_wait_ust = poll_ret;`
9. Set `avail` to minimum of capture and playback avail
10. Mark channels done `bitset_copy`
11. Finally, return `avail - (avail % driver->frames_per_cycle)`

--

```c
jack_nframes_t
alsa_driver_wait (alsa_driver_t *driver, int extra_fd, int *status, float
		  *delayed_usecs)
{
	snd_pcm_sframes_t avail = 0;
	snd_pcm_sframes_t capture_avail = 0;
	snd_pcm_sframes_t playback_avail = 0;
	int xrun_detected = FALSE;
	int need_capture;
	int need_playback;
	int retry_cnt = 0;
	unsigned int i;
	jack_time_t poll_enter;
	jack_time_t poll_ret = 0;

	*status = -1;
	*delayed_usecs = 0;

	need_capture = driver->capture_handle ? 1 : 0;

	if (extra_fd >= 0) {
		need_playback = 0;
	} else {
		need_playback = driver->playback_handle ? 1 : 0;
	}

  again:

	while ((need_playback || need_capture) && !xrun_detected) {

		int poll_result;
		unsigned int ci = 0;
		unsigned int nfds;
		unsigned short revents;

		nfds = 0;

		if (need_playback) {
			snd_pcm_poll_descriptors (driver->playback_handle,
						  &driver->pfd[0],
						  driver->playback_nfds);
			nfds += driver->playback_nfds;
		}

		if (need_capture) {
			snd_pcm_poll_descriptors (driver->capture_handle,
						  &driver->pfd[nfds],
						  driver->capture_nfds);
			ci = nfds;
			nfds += driver->capture_nfds;
		}

		/* ALSA doesn't set POLLERR in some versions of 0.9.X */

		for (i = 0; i < nfds; i++) {
			driver->pfd[i].events |= POLLERR;
		}

		if (extra_fd >= 0) {
			driver->pfd[nfds].fd = extra_fd;
			driver->pfd[nfds].events =
				POLLIN|POLLERR|POLLHUP|POLLNVAL;
			nfds++;
		}

		poll_enter = jack_get_microseconds ();

		if (poll_enter > driver->poll_next) {
			/*
			 * This processing cycle was delayed past the
			 * next due interrupt!  Do not account this as
			 * a wakeup delay:
			 */
			driver->poll_next = 0;
			driver->poll_late++;
		}

#ifdef __ANDROID__
		poll_result = poll (driver->pfd, nfds, -1);  //fix for sleep issue
#else
		poll_result = poll (driver->pfd, nfds, driver->poll_timeout);
#endif
		if (poll_result < 0) {

			if (errno == EINTR) {
				const char poll_log[] = "ALSA: poll interrupt";
				// this happens mostly when run
				// under gdb, or when exiting due to a signal
				if (under_gdb) {
					jack_info(poll_log);
					goto again;
				}
				jack_error(poll_log);
				*status = -2;
				return 0;
			}

			jack_error ("ALSA: poll call failed (%s)",
				    strerror (errno));
			*status = -3;
			return 0;

		}

		poll_ret = jack_get_microseconds ();

		if (poll_result == 0) {
			retry_cnt++;
			if(retry_cnt > MAX_RETRY_COUNT) {
				jack_error ("ALSA: poll time out, polled for %" PRIu64
					    " usecs, Reached max retry cnt = %d, Exiting",
					    poll_ret - poll_enter, MAX_RETRY_COUNT);
				*status = -5;
				return 0;
			}
			jack_error ("ALSA: poll time out, polled for %" PRIu64
				    " usecs, Retrying with a recovery, retry cnt = %d",
				    poll_ret - poll_enter, retry_cnt);
			*status = alsa_driver_xrun_recovery (driver, delayed_usecs);
			if(*status != 0) {
				jack_error ("ALSA: poll time out, recovery failed with status = %d", *status);
				return 0;
			}
		}

        // JACK2
        SetTime(poll_ret);

		if (extra_fd < 0) {
			if (driver->poll_next && poll_ret > driver->poll_next) {
				*delayed_usecs = poll_ret - driver->poll_next;
			}
			driver->poll_last = poll_ret;
			driver->poll_next = poll_ret + driver->period_usecs;
            // JACK2
            /* driver->engine->transport_cycle_start (driver->engine, poll_ret); */
		}

#ifdef DEBUG_WAKEUP
		fprintf (stderr, "%" PRIu64 ": checked %d fds, started at %" PRIu64 " %" PRIu64 "  usecs since poll entered\n",
			 poll_ret, nfds, poll_enter, poll_ret - poll_enter);
#endif

		/* check to see if it was the extra FD that caused us
		 * to return from poll */

		if (extra_fd >= 0) {

			if (driver->pfd[nfds-1].revents == 0) {
				/* we timed out on the extra fd */

				*status = -4;
				return -1;
			}

			/* if POLLIN was the only bit set, we're OK */

			*status = 0;
			return (driver->pfd[nfds-1].revents == POLLIN) ? 0 : -1;
		}

		if (need_playback) {
			if (snd_pcm_poll_descriptors_revents
			    (driver->playback_handle, &driver->pfd[0],
			     driver->playback_nfds, &revents) < 0) {
				jack_error ("ALSA: playback revents failed");
				*status = -6;
				return 0;
			}

			if (revents & POLLNVAL) {
				jack_error ("ALSA: playback device disconnected");
				*status = -7;
				return 0;
			}

			if (revents & POLLERR) {
				xrun_detected = TRUE;
			}

			if (revents & POLLOUT) {
				need_playback = 0;
#ifdef DEBUG_WAKEUP
				fprintf (stderr, "%" PRIu64
					 " playback stream ready\n",
					 poll_ret);
#endif
			}
		}

		if (need_capture) {
			if (snd_pcm_poll_descriptors_revents
			    (driver->capture_handle, &driver->pfd[ci],
			     driver->capture_nfds, &revents) < 0) {
				jack_error ("ALSA: capture revents failed");
				*status = -6;
				return 0;
			}

			if (revents & POLLNVAL) {
				jack_error ("ALSA: capture device disconnected");
				*status = -7;
				return 0;
			}

			if (revents & POLLERR) {
				xrun_detected = TRUE;
			}

			if (revents & POLLIN) {
				need_capture = 0;
#ifdef DEBUG_WAKEUP
				fprintf (stderr, "%" PRIu64
					 " capture stream ready\n",
					 poll_ret);
#endif
			}
		}
	}

	if (driver->capture_handle) {
		if ((capture_avail = snd_pcm_avail_update (
			     driver->capture_handle)) < 0) {
			if (capture_avail == -EPIPE) {
				xrun_detected = TRUE;
			} else {
				jack_error ("unknown ALSA avail_update return"
					    " value (%u)", capture_avail);
			}
		}
	} else {
		/* odd, but see min() computation below */
		capture_avail = INT_MAX;
	}

	if (driver->playback_handle) {
		if ((playback_avail = snd_pcm_avail_update (
			     driver->playback_handle)) < 0) {
			if (playback_avail == -EPIPE) {
				xrun_detected = TRUE;
			} else {
				jack_error ("unknown ALSA avail_update return"
					    " value (%u)", playback_avail);
			}
		}
	} else {
		/* odd, but see min() computation below */
		playback_avail = INT_MAX;
	}

	if (xrun_detected) {
		*status = alsa_driver_xrun_recovery (driver, delayed_usecs);
		return 0;
	}

	*status = 0;
	driver->last_wait_ust = poll_ret;

	avail = capture_avail < playback_avail ? capture_avail : playback_avail;

#ifdef DEBUG_WAKEUP
	fprintf (stderr, "wakeup complete, avail = %lu, pavail = %lu "
		 "cavail = %lu\n",
		 avail, playback_avail, capture_avail);
#endif

	/* mark all channels not done for now. read/write will change this */

	bitset_copy (driver->channels_not_done, driver->channels_done);

	/* constrain the available count to the nearest (round down) number of
	   periods.
	*/

	return avail - (avail % driver->frames_per_cycle);
}
```

Compare to: (alsaloop)

```c

    avail = snd_pcm_avail_update(lhandle->handle);
    if (avail == -EPIPE)
    {
        return xrun(lhandle);
    } else if (avail == -ESTRPIPE)
    {
        if ((err = suspend(lhandle)) < 0) return err;
    }
```

## JUCE

- `snd_pcm_wait` before read and write
- just straight out 1. `readi` and convert, 2. convert and `writei`
- example of how to position callback, else silence

## RtAudio

- just straight out 1. `readi` and convert, 2. convert and `writei`, now waiting nothing
- feels like syncing time is simply with pthreads and measuring `snd_pcm_delay`
