# Variants

| no  | source        | program         | duplex | hw      | sw                  | linked | blocking | access | wait              | read     | write    | timing  | resampling | file | mixer |
| --- | ------------- | --------------- | ------ | ------- | ------------------- | ------ | -------- | ------ | ----------------- | -------- | -------- | ------- | ---------- | ---- | ----- |
| 1   | alsa-lib/test | pcm-simple      | no     | exact   | -                   | na     | yes      |        |                   |          |          |         | no         | no   | no    |
| 2   | alsa-lib/test | pcm             | no     | bufsize | -                   | na     | both     | both   | wait, poll, async | rw, mmap | rw, mmap |         |            | no   | no    |
| 3   | alsa-lib/test | pcm-multithread | no     | exact   |                     | na     |          |        |                   |          |          |         |            |      | no    |
| 4   | alsa-lib/test | audio-timer     | yes    | bufsize | enable timerstamps  | both   |          |        |                   |          |          |         |            |      |       |
| 5   | alsa-lib/test | latency         | yes    | latency | max start threshold | yes    |          |        |                   |          |          | tstamps |            |      | no    |
| 6   | alsa-utils    | aplay           | no     | any     |                     |        |          |        |                   |          |          |         |            | yes  |       |
| 7   | alsa-utils    | axfer           | no     | any     |                     |        |          |        |                   |          |          |         |            |      |       |
| 8   | alsa-utils    | aloop           | yes    | latency |                     |        |          |        |                   |          |          |         |            |      | yes   |
| 9   | jack2         | alsa-driver     | yes    | latency |                     |        |          |        |                   |          |          |         |            |      |       |
| 10  | juce          | alsa-driver     | yes    | latency |                     |        |          |        |                   |          |          |         |            |      |       |
| 11  | rtaudio       | linux-alsa      | yes    | simple  |                     |        |          |        |                   |          |          |         |            |      |       |
