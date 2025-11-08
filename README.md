# ALSA extensions

Extensions to alsa-lib API for duplex operation.

Dimensions:

## 1. Hardware setup

- rate
- format
- channels
- period size
- buffer size

| no  | program         | hw      | comments |
| --- | --------------- | ------- | -------- |
| 1   | pcm-simple      | exact   |          |
| 2   | pcm             | bufsize |          |
| 3   | pcm-multithread | exact   |          |
| 4   | audio-timer     | bufsize |          |
| 5   | latency         | latency |          |
| 6   | aplay           | any     |          |
| 7   | axfer           | any     |          |
| 8   | aloop           | latency |          |
| 9   | jack            | latency |          |
| 10  | juce            | latency |          |
| 11  | rtaudio         | simple  |          |

## 2. Software setup

- start threshold
- stop threshold
- timestamps

| no  | program         | sw  | comments |
| --- | --------------- | --- | -------- |
| 1   | pcm-simple      |     |          |
| 2   | pcm             |     |          |
| 3   | pcm-multithread |     |          |
| 4   | audio-timer     |     |          |
| 5   | latency         |     |          |
| 6   | aplay           |     |          |
| 7   | axfer           |     |          |
| 8   | aloop           |     |          |
| 9   | jack            |     |          |
| 10  | juce            |     |          |
| 11  | rtaudio         |     |          |

## 3. Linking devices

- whether to manually start other devices

## 4. Blocking/Non-blocking

- whether to use wait

## 5. Access

- mmap / rw

## 6. Wait op

- `snd_pcm_wait`, `poll`, `epoll`, `select`

| no  | program         | wait | comments |
| --- | --------------- | ---- | -------- |
| 1   | pcm-simple      |      |          |
| 2   | pcm             |      |          |
| 3   | pcm-multithread |      |          |
| 4   | audio-timer     |      |          |
| 5   | latency         |      |          |
| 6   | aplay           |      |          |
| 7   | axfer           |      |          |
| 8   | aloop           |      |          |
| 9   | jack            |      |          |
| 10  | juce            |      |          |
| 11  | rtaudio         |      |          |

## 7. Xrun recovery

| no  | program         | xrun                                                                                                                                                              | comments |
| --- | --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- |
| 1   | pcm-simple      | no                                                                                                                                                                |          |
| 2   | pcm             | `xrun_recovery(handle, err)`                                                                                                                                      |          |
| 3   | pcm-multithread | `pcm_recover` once                                                                                                                                                |          |
| 4   | audio-timer     | `pcm_wait`, exit if error                                                                                                                                         |          |
| 5   | latency         | `pcm_wait`, `get_avail` -> checks avail till not -EAGAIN                                                                                                          |          |
| 6   | aplay           | `xrun()`, `suspend()`, check EPIPE, ESTRPIPE                                                                                                                      |          |
| 7   | axfer           | libasound.c: check EPIPE, ESTRPIPE, but also `pcm_prepare`; timer-mmap.c: based on `SND_PCM_STATE_...`, separate for read, write; irq-mmap: similar to timer-mmap |          |
| 8   | alsaloop        | -EPIPE, -ESTRPIPE, `xrun()`, `suspend()` for `avail_update`, `writei`, `readi`, `delay` functions                                                                 |          |
| 9   | jack            | -EPIPE, -ESTRPIPE for `avail_update`, `mmap_commit`; `alsa_driver_xrun_recovery` -> complete with different statuses                                              |          |
| 10  | juce            | only -EPIPE and `pcm_recover`                                                                                                                                     |          |
| 11  | rtaudio         | mix of -EPIPE and state based, `pcm_recover`                                                                                                                      |          |

## 8. Read/Write ops

- readi/readn, mmap, ringbuffers

## 9. Timing

- timestamps, slave/master clocks

| no  | program         | wait | comments |
| --- | --------------- | ---- | -------- |
| 1   | pcm-simple      |      |          |
| 2   | pcm             |      |          |
| 3   | pcm-multithread |      |          |
| 4   | audio-timer     |      |          |
| 5   | latency         |      |          |
| 6   | aplay           |      |          |
| 7   | axfer           |      |          |
| 8   | aloop           |      |          |
| 9   | jack            |      |          |
| 10  | juce            |      |          |
| 11  | rtaudio         |      |          |

## 10. Resampling

- pitch, drift measurement

| no  | program         | wait | comments |
| --- | --------------- | ---- | -------- |
| 1   | pcm-simple      |      |          |
| 2   | pcm             |      |          |
| 3   | pcm-multithread |      |          |
| 4   | audio-timer     |      |          |
| 5   | latency         |      |          |
| 6   | aplay           |      |          |
| 7   | axfer           |      |          |
| 8   | aloop           |      |          |
| 9   | jack            |      |          |
| 10  | juce            |      |          |
| 11  | rtaudio         |      |          |

## 11. File read/write

- direct from alsa, wav files

## 12. Mixers

- `snd_ctl_...`
