# ALSA extensions

Extensions to alsa-lib API for duplex operation.

- Prior art: `alsa-lib`, `alsa-utils`, `jack2`, `rtaudio`, `JUCE`
- Structure:
  - Open, Close
  - Start, Stop
  - Wait, Xrun
  - Read, Write
- Assuming standard application looks like:
  - Open
  - Start
  - Loop
    - Wait or Xrun (possibly Stop and Start)
    - Read
    - Write
  - Stop
  - Close
- Start and Stop are called multiple times, and behaviour depends on whether xrun happened or not.
- Assuming we use float as our internal format, the minimum number of copies/conversions:
  - Read -> directly from device channel areas to float buffer
  - Write -> directly from float buffer to device channel areas
  - TODO: How to handle fx chains?

Details:

- start threshold affects if `snd_pcm_start` needs to be called
- `nonblock` is handled kind of inconsistently in references
- resampling needs to be studied in light of `latency.c`

## Open, Close

- Open: called once at start of program
- Close: called once at the end of the program
- If error in Open, is usually fatal.
- Sometimes, buffer_size may need readjustment

### Hardware Params setup

- rate
- format
- channels
- periods
- period size
- buffer size
- period time
- buffer time
- access

| no  | program         | hw      | comments                                                                                                                                                                                                                             |
| --- | --------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 1   | pcm-simple      | exact   | `snd_pcm_set_params`                                                                                                                                                                                                                 |
| 2   | pcm             | buftime | custom `set_hwparams`, also sets `resample=1`, uses `buffer_time_near` , `period_time_near`                                                                                                                                          |
| 3   | pcm-multithread | exact   | custom `setup_params` with no error handling                                                                                                                                                                                         |
| 4   | audio-time      | exact   | `snd_pcm_set_params`, also uses `snd_pcm_hw_params_supports_audio_ts_type`                                                                                                                                                           |
| 5   | latency         | bufsize | custom `setparams_stream` (uses `rate_near`), special `setparams_bufsize`; also `snd_pcm_hw_params_set_period_wakeup` if using `sys_latency`                                                                                         |
| 6   | aplay           | -       |                                                                                                                                                                                                                                      |
| 7   | axfer           | -       |                                                                                                                                                                                                                                      |
| 8   | aloop           | -       |                                                                                                                                                                                                                                      |
| 9   | jack            | exact   | custom `alsa_driver_configure_stream`                                                                                                                                                                                                |
| 10  | juce            | near    | custom `setParameters`, follows JACK, but uses `_near` for rate, period_size, periods; latency calculation is interesting (`latency = (int) frames * ((int) periods - 1); // (this is the method JACK uses to guess the latency..)`) |
| 11  | rtaudio         | near    | set channels as `channels_max`, use `snd_..._test_...` for rates and formats, use `rate_near`, `period_size_near`, `periods_near`                                                                                                    |

### Software Params setup

- start threshold
- stop threshold
- silence threshold
- silence size
- avail_min
- period_wakeup
- timestamps

| no  | program         | sw                                                                            | comments                                                                                                                                     |
| --- | --------------- | ----------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | pcm-simple      | start, avail_min                                                              | `snd_pcm_set_params`                                                                                                                         |
| 2   | pcm             | start, avail_min, period_wakeup                                               | custom `set_swparams`, customization to set `snd_pcm_sw_params_set_period_event` based on `sys_latency`                                      |
| 3   | pcm-multithread | -                                                                             | no sw params set                                                                                                                             |
| 4   | audio-time      | tstamp_mode, tstamp_type                                                      | custom to set timestamps                                                                                                                     |
| 5   | latency         | start, avail_min                                                              | sets start to highest possible so that we have to manually `snd_pcm_start`                                                                   |
| 6   | aplay           |                                                                               |                                                                                                                                              |
| 7   | axfer           |                                                                               |                                                                                                                                              |
| 8   | aloop           |                                                                               |                                                                                                                                              |
| 9   | jack            | start, stop, silence_threshold, silence_size(deprecated?), avail_min, tstamps | TODO: what do the silence functions do? otherwise start is zero and stop is usual period_size \* periods (TODO: except in case of soft-mode) |
| 10  | juce            | start, stop, silence_threshold, silence_size                                  | start = period_size, stop = boundary, silence_threshold = 0, silence_size = boundary                                                         |
| 11  | rtaudio         | start, stop, silence, silence_size                                            | silence_size is set to boundary, start is buffersize, stop is infinite, silence_threshold is 0; strangely, `avail_min` is not used           |

### Blocking/Non-blocking

- whether to use wait

| no  | program         | nonblock            | comments                                       |
| --- | --------------- | ------------------- | ---------------------------------------------- |
| 1   | pcm-simple      | no                  | play: no                                       |
| 2   | pcm             | no                  | play: no                                       |
| 3   | pcm-multithread | no                  | play: no                                       |
| 4   | audio-time      | yes                 | capt: yes, play: no                            |
| 5   | latency         | yes, based on block | both: block                                    |
| 6   | aplay           |                     |                                                |
| 7   | axfer           |                     |                                                |
| 8   | aloop           |                     |                                                |
| 9   | jack            | yes then no         | Initally both nonblock, then both set to block |
| 10  | juce            | yes                 | both: yes                                      |
| 11  | rtaudio         | yes                 | both: yes                                      |

### Linking devices

- whether to manually start other devices

| no  | program         | link | comments               |
| --- | --------------- | ---- | ---------------------- |
| 1   | pcm-simple      | -    |                        |
| 2   | pcm             | -    |                        |
| 3   | pcm-multithread | -    |                        |
| 4   | audio-time      | yes  | capt, play             |
| 5   | latency         | yes  | capt, play             |
| 6   | aplay           |      |                        |
| 7   | axfer           |      |                        |
| 8   | aloop           |      |                        |
| 9   | jack            | yes  | play, capt             |
| 10  | juce            | -    |                        |
| 11  | rtaudio         | yes  | handles[0], handles[1] |

### Mixers

- `snd_ctl_...`
  TODO

## Start, Stop

Preparing and dropping frames from pcm, may be called from xrun recovery

### Timing

3 ways to keep time (TODO: Which are frames and which are microseconds?):

- snd timestamps
- system timestamps
- count of frames in, frames out

Relevant snd function: `snd_pcm_delay`

| no  | program         | timing | comments |
| --- | --------------- | ------ | -------- |
| 1   | pcm-simple      |        |          |
| 2   | pcm             |        |          |
| 3   | pcm-multithread |        |          |
| 4   | audio-timer     |        |          |
| 5   | latency         |        |          |
| 6   | aplay           |        |          |
| 7   | axfer           |        |          |
| 8   | aloop           |        |          |
| 9   | jack            |        |          |
| 10  | juce            |        |          |
| 11  | rtaudio         |        |          |

### Resampling

- pitch, drift measurement

| no  | program         | resampling | comments |
| --- | --------------- | ---------- | -------- |
| 1   | pcm-simple      |            |          |
| 2   | pcm             |            |          |
| 3   | pcm-multithread |            |          |
| 4   | audio-timer     |            |          |
| 5   | latency         |            |          |
| 6   | aplay           |            |          |
| 7   | axfer           |            |          |
| 8   | aloop           |            |          |
| 9   | jack            |            |          |
| 10  | juce            |            |          |
| 11  | rtaudio         |            |          |

## Wait, Xrun

### Wait op

- if blocking, no wait required
- `snd_pcm_wait`, `poll`, `epoll`, `select`

| no  | program         | wait                                    | comments |
| --- | --------------- | --------------------------------------- | -------- |
| 1   | pcm-simple      | -                                       |          |
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

### Xrun recovery

| no  | program         | xrun | comments                                                                                                                                                          | comments |
| --- | --------------- | ---- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- |
| 1   | pcm-simple      |      | no                                                                                                                                                                |          |
| 2   | pcm             |      | `xrun_recovery(handle, err)`                                                                                                                                      |          |
| 3   | pcm-multithread |      | `pcm_recover` once                                                                                                                                                |          |
| 4   | audio-time      |      | `pcm_wait`, exit if error                                                                                                                                         |          |
| 5   | latency         |      | `pcm_wait`, `get_avail` -> checks avail till not -EAGAIN                                                                                                          |          |
| 6   | aplay           |      | `xrun()`, `suspend()`, check EPIPE, ESTRPIPE                                                                                                                      |          |
| 7   | axfer           |      | libasound.c: check EPIPE, ESTRPIPE, but also `pcm_prepare`; timer-mmap.c: based on `SND_PCM_STATE_...`, separate for read, write; irq-mmap: similar to timer-mmap |          |
| 8   | alsaloop        |      | -EPIPE, -ESTRPIPE, `xrun()`, `suspend()` for `avail_update`, `writei`, `readi`, `delay` functions                                                                 |          |
| 9   | jack            |      | -EPIPE, -ESTRPIPE for `avail_update`, `mmap_commit`; `alsa_driver_xrun_recovery` -> complete with different statuses                                              |          |
| 10  | juce            |      | only -EPIPE and `pcm_recover`                                                                                                                                     |          |
| 11  | rtaudio         |      | mix of -EPIPE and state based, `pcm_recover`                                                                                                                      |          |

## Read, Write

### To/From Device

- readi/readn, mmap, ringbuffers

### To/From Files

- direct from alsa, wav files
