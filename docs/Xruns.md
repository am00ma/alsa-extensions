# Xruns

Examples:

| no  | program         | xrun                                                                                                                                                                                                          | comments |
| --- | --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- |
| 1   | pcm-simple      | no                                                                                                                                                                                                            |          |
| 2   | pcm             | `xrun_recovery(handle, err)`                                                                                                                                                                                  |          |
| 3   | pcm-multithread | `pcm_recover` once                                                                                                                                                                                            |          |
| 4   | audio-timer     | `pcm_wait`, exit if error                                                                                                                                                                                     |          |
| 5   | latency         | `pcm_wait`, `get_avail` -> checks avail till not -EAGAIN                                                                                                                                                      |          |
| 6   | aplay           | `xrun()`, `suspend()`, check EPIPE, ESTRPIPE                                                                                                                                                                  |          |
| 7   | axfer           | libasound.c: check EPIPE, ESTRPIPE, but also `pcm_prepare`; timer-mmap.c: based on `SND_PCM_STATE_...`, separate for read, write; irq-mmap: similar to timer-mmap                                             |          |
| 8   | alsaloop        | -EPIPE, -ESTRPIPE, `xrun()`, `suspend()` for `avail_update`, `writei`, `readi`, `delay` functions                                                                                                             |          |
| 9   | jack            | -EPIPE, -ESTRPIPE for `avail_update`, `mmap_commit`; `alsa_driver_xrun_recovery` -> complete with different statuses, though only prints, does not distinguish on return, also keeps track of `delayed_usecs` |          |
| 10  | juce            | only -EPIPE and `pcm_recover`                                                                                                                                                                                 |          |
| 11  | rtaudio         | mix of -EPIPE and state based, `pcm_recover`                                                                                                                                                                  |          |

Functions to recover from:

1. `snd_pcm_wait`, `poll`
2. `snd_pcm_avail_update`
3. `snd_pcm_mmap_begin`, `snd_pcm_mmap_commit`
4. `snd_pcm_readi`, `snd_pcm_writei`

## pcm

Pseudocode:

Given -> `err`, cases:

1. `-EPIPE`: `snd_pcm_prepare`:
   - if successful return 0
   - else just print and return 0 anyway
2. `-ESTRPIPE`:
   - sleep in loop till `snd_pcm_resume` is `-EAGAIN`
   - if no error, return 0
   - if err, `snd_pcm_prepare`:
     - if successful return 0
   - else just print and return 0 anyway
3. Else -> just return err

NOTE: Provided by ALSA as `snd_pcm_recover`

```c
static int xrun_recovery(snd_pcm_t* handle, int err)
{
    printf("stream recovery\n");
    if (err == -EPIPE)
    { /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0) printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE)
    {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
        {
            snd_output_printf(output, "sleeping\n");
            sleep(1); /* wait until the suspend flag is released */
        }
        if (err < 0)
        {
            err = snd_pcm_prepare(handle);
            if (err < 0) printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}

```

## aplay

```cpp

/* I/O error handler */
static void xrun(void)
{
	snd_pcm_status_t *status;
	int res;

	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(handle, status))<0) {
		error(_("status error: %s"), snd_strerror(res));
		prg_exit(EXIT_FAILURE);
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
		if (monotonic) {
#ifdef HAVE_CLOCK_GETTIME
			struct timespec now, diff, tstamp;
			clock_gettime(CLOCK_MONOTONIC, &now);
			snd_pcm_status_get_trigger_htstamp(status, &tstamp);
			timermsub(&now, &tstamp, &diff);
			fprintf(stderr, _("%s!!! (at least %.3f ms long)\n"),
				stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
				diff.tv_sec * 1000 + diff.tv_nsec / 1000000.0);
#else
			fprintf(stderr, "%s !!!\n", _("underrun"));
#endif
		} else {
			struct timeval now, diff, tstamp;
			gettimeofday(&now, 0);
			snd_pcm_status_get_trigger_tstamp(status, &tstamp);
			timersub(&now, &tstamp, &diff);
			fprintf(stderr, _("%s!!! (at least %.3f ms long)\n"),
				stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
				diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
		}
		if (verbose) {
			fprintf(stderr, _("Status:\n"));
			snd_pcm_status_dump(status, log);
		}
		if (fatal_errors) {
			error(_("fatal %s: %s"),
					stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
					snd_strerror(res));
			prg_exit(EXIT_FAILURE);
		}
		if ((res = snd_pcm_prepare(handle))<0) {
			error(_("xrun: prepare error: %s"), snd_strerror(res));
			prg_exit(EXIT_FAILURE);
		}
		return;		/* ok, data should be accepted again */
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
		if (verbose) {
			fprintf(stderr, _("Status(DRAINING):\n"));
			snd_pcm_status_dump(status, log);
		}
		if (stream == SND_PCM_STREAM_CAPTURE) {
			fprintf(stderr, _("capture stream format change? attempting recover...\n"));
			if ((res = snd_pcm_prepare(handle))<0) {
				error(_("xrun(DRAINING): prepare error: %s"), snd_strerror(res));
				prg_exit(EXIT_FAILURE);
			}
			return;
		}
	}
	if (verbose) {
		fprintf(stderr, _("Status(R/W):\n"));
		snd_pcm_status_dump(status, log);
	}
	error(_("read/write error, state = %s"), snd_pcm_state_name(snd_pcm_status_get_state(status)));
	prg_exit(EXIT_FAILURE);
}
```

```cpp
/* I/O suspend handler */
static void suspend(void)
{
	int res;

	if (!quiet_mode) {
		fprintf(stderr, _("Suspended. Trying resume. ")); fflush(stderr);
	}
	while ((res = snd_pcm_resume(handle)) == -EAGAIN)
		sleep(1);	/* wait until suspend flag is released */
	if (res < 0) {
		if (!quiet_mode) {
			fprintf(stderr, _("Failed. Restarting stream. ")); fflush(stderr);
		}
		if ((res = snd_pcm_prepare(handle)) < 0) {
			error(_("suspend: prepare error: %s"), snd_strerror(res));
			prg_exit(EXIT_FAILURE);
		}
	}
	if (!quiet_mode)
		fprintf(stderr, _("Done.\n"));
}
```

## alsaloop

```c

static void xrun_profile0(struct loopback* loop)
{
    snd_pcm_sframes_t pdelay, cdelay;

    if (snd_pcm_delay(loop->play->handle, &pdelay) >= 0 && snd_pcm_delay(loop->capt->handle, &cdelay) >= 0)
    {
        getcurtimestamp(&loop->xrun_last_update);
        loop->xrun_last_pdelay = pdelay;
        loop->xrun_last_cdelay = cdelay;
        loop->xrun_buf_pcount  = loop->play->buf_count;
        loop->xrun_buf_ccount  = loop->capt->buf_count;
#ifdef USE_SAMPLERATE
        loop->xrun_out_frames = loop->src_out_frames;
#endif
    }
}

static inline void xrun_profile(struct loopback* loop)
{
    if (loop->xrun) xrun_profile0(loop);
}

static void xrun_stats0(struct loopback* loop)
{
    snd_timestamp_t t;
    double          expected, last, wake, check, queued = -1, proc, missing = -1;
    double          maxbuf, pfilled, cfilled, cqueued = -1, avail_min;
    double          sincejob;

    expected = ((double)loop->latency / (double)loop->play->rate_req) * 1000;
    getcurtimestamp(&t);
    last     = (double)timediff(t, loop->xrun_last_update) / 1000;
    wake     = (double)timediff(t, loop->xrun_last_wake) / 1000;
    check    = (double)timediff(t, loop->xrun_last_check) / 1000;
    sincejob = (double)timediff(t, loop->tstamp_start) / 1000;
    if (loop->xrun_last_pdelay != XRUN_PROFILE_UNKNOWN)
        queued = ((double)loop->xrun_last_pdelay / (double)loop->play->rate) * 1000;
    if (loop->xrun_last_cdelay != XRUN_PROFILE_UNKNOWN)
        cqueued = ((double)loop->xrun_last_cdelay / (double)loop->capt->rate) * 1000;
    maxbuf    = ((double)loop->play->buffer_size / (double)loop->play->rate) * 1000;
    proc      = (double)loop->xrun_max_proctime / 1000;
    pfilled   = ((double)(loop->xrun_buf_pcount + loop->xrun_out_frames) / (double)loop->play->rate) * 1000;
    cfilled   = ((double)loop->xrun_buf_ccount / (double)loop->capt->rate) * 1000;
    avail_min = (((double)loop->play->buffer_size - (double)loop->play->avail_min) / (double)loop->play->rate) * 1000;
    avail_min = expected - avail_min;
    if (queued >= 0) missing = last - queued;
    if (missing >= 0 && loop->xrun_max_missing < missing) loop->xrun_max_missing = missing;
    loop->xrun_max_proctime = 0;
    getcurtimestamp(&t);
    logit(LOG_INFO, "  last write before %.4fms, queued %.4fms/%.4fms -> missing %.4fms\n", last, queued, cqueued,
          missing);
    logit(LOG_INFO, "  expected %.4fms, processing %.4fms, max missing %.4fms\n", expected, proc,
          loop->xrun_max_missing);
    logit(LOG_INFO, "  last wake %.4fms, last check %.4fms, avail_min %.4fms\n", wake, check, avail_min);
    logit(LOG_INFO, "  max buf %.4fms, pfilled %.4fms, cfilled %.4fms\n", maxbuf, pfilled, cfilled);
    logit(LOG_INFO, "  job started before %.4fms\n", sincejob);
}
static int xrun(struct loopback_handle* lhandle)
{
    int err;

    if (lhandle == lhandle->loopback->play)
    {
        logit(LOG_DEBUG, "underrun for %s\n", lhandle->id);
        xrun_stats(lhandle->loopback);
        if ((err = snd_pcm_prepare(lhandle->handle)) < 0) return err;
        lhandle->xrun_pending = 1;
    } else {
        logit(LOG_DEBUG, "overrun for %s\n", lhandle->id);
        xrun_stats(lhandle->loopback);
        if ((err = snd_pcm_prepare(lhandle->handle)) < 0) return err;
        lhandle->xrun_pending = 1;
    }
    return 0;
}

static int suspend(struct loopback_handle* lhandle)
{
    int err;

    while ((err = snd_pcm_resume(lhandle->handle)) == -EAGAIN) usleep(1);
    if (err < 0) return xrun(lhandle);
    return 0;
}
```

## jack2

```cpp

static int
alsa_driver_xrun_recovery (alsa_driver_t *driver, float *delayed_usecs)
{
	snd_pcm_status_t *status;
	int res;

	snd_pcm_status_alloca(&status);

	if (driver->capture_handle) {
		if ((res = snd_pcm_status(driver->capture_handle, status))
		    < 0) {
			jack_error("status error: %s", snd_strerror(res));
		}
	} else {
		if ((res = snd_pcm_status(driver->playback_handle, status))
		    < 0) {
			jack_error("status error: %s", snd_strerror(res));
		}
	}

	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_SUSPENDED)
	{
		jack_log("**** alsa_pcm: pcm in suspended state, resuming it" );
		if (driver->capture_handle) {
			if ((res = snd_pcm_prepare(driver->capture_handle))
			    < 0) {
				jack_error("error preparing after suspend: %s", snd_strerror(res));
			}
		}
		if (driver->playback_handle) {
			if ((res = snd_pcm_prepare(driver->playback_handle))
			    < 0) {
				jack_error("error preparing after suspend: %s", snd_strerror(res));
			}
		}
	}

	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN
	    && driver->process_count > XRUN_REPORT_DELAY) {
		struct timeval now, diff, tstamp;
		driver->xrun_count++;
		snd_pcm_status_get_tstamp(status,&now);
		snd_pcm_status_get_trigger_tstamp(status, &tstamp);
		timersub(&now, &tstamp, &diff);
		*delayed_usecs = diff.tv_sec * 1000000.0 + diff.tv_usec;
		jack_log("**** alsa_pcm: xrun of at least %.3f msecs",*delayed_usecs / 1000.0);
		if (driver->capture_handle) {
			jack_log("Repreparing capture");
			if ((res = snd_pcm_prepare(driver->capture_handle)) < 0) {
				jack_error("error preparing after xrun: %s", snd_strerror(res));
			}
		}
		if (driver->playback_handle) {
			jack_log("Repreparing playback");
			if ((res = snd_pcm_prepare(driver->playback_handle)) < 0) {
				jack_error("error preparing after xrun: %s", snd_strerror(res));
			}
		}
	}

	if (alsa_driver_restart (driver)) {
		return -1;
	}
	return 0;
}
```

## JUCE

Nothing explicit, but they use `snd_pcm_recover`...

```c
/**
 * \brief Recover the stream state from an error or suspend
 * \param pcm PCM handle
 * \param err error number
 * \param silent do not print error reason
 * \return 0 when error code was handled successfuly, otherwise a negative error code
 *
 * This a high-level helper function building on other functions.
 *
 * This functions handles -EINTR (interrupted system call),
 * -EPIPE (overrun or underrun) and -ESTRPIPE (stream is suspended)
 * error codes trying to prepare given stream for next I/O.
 *
 * Note that this function returns the original error code when it is not
 * handled inside this function (for example -EAGAIN is returned back).
 */
int snd_pcm_recover(snd_pcm_t *pcm, int err, int silent)
{
        if (err > 0)
                err = -err;
        if (err == -EINTR)	/* nothing to do, continue */
                return 0;
        if (err == -EPIPE) {
                const char *s;
                if (snd_pcm_stream(pcm) == SND_PCM_STREAM_PLAYBACK)
                        s = "underrun";
                else
                        s = "overrun";
                if (!silent)
                        SNDERR("%s occurred", s);
                err = snd_pcm_prepare(pcm);
                if (err < 0) {
                        SNDERR("cannot recovery from %s, prepare failed: %s", s, snd_strerror(err));
                        return err;
                }
                return 0;
        }
        if (err == -ESTRPIPE) {
                while ((err = snd_pcm_resume(pcm)) == -EAGAIN)
                        /* wait until suspend flag is released */
                        poll(NULL, 0, 1000);
                if (err < 0) {
                        err = snd_pcm_prepare(pcm);
                        if (err < 0) {
                                SNDERR("cannot recovery from suspend, prepare failed: %s", snd_strerror(err));
                                return err;
                        }
                }
                return 0;
        }
        return err;
}
```
