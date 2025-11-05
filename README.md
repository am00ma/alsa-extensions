# ALSA extensions

Extensions to alsa-lib API for duplex operation.

Dimensions:

## 1. Hardware setup

- rate
- format
- channels
- period size
- buffer size

## 2. Software setup

- start threshold
- stop threshold
- timestamps

## 3. Linking devices

- whether to manually start other devices

## 4. Blocking/Non-blocking

- whether to use wait

## 5. Access

- mmap / rw

## 6. Wait op

- `snd_pcm_wait`, `poll`, `epoll`, `select`

## 7. Read op

- readi/readn, mmap, ringbuffers

## 8. Write op

- writei/writen, mmap, ringbuffers

## 9. Timing

- timestamps, slave/master clocks

## 10. Resampling

- pitch, drift measurement

## 11. File read/write

- direct from alsa, wav files

## 12. Mixers

- `snd_ctl_...`
