# Midi

1. Need threadsafe ringbuffer
2. Need to launch separate audio and midi threads
3. Uses same avail, wait mechanism

Questions:

1. How to keep it sample-precise?
2. How to play/record midi clip with timestamps?
3. How does threading work?
4. What does the sequencer do?

| no  | program          | midi     | comments                                                                                                                                                                                                         |
| --- | ---------------- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | amidi            | raw      |                                                                                                                                                                                                                  |
| 2   | seq/aplaymidi    | seq      | Computes timestamps and sends to sequencer (playback port), `snd_seq_event_output` blocks when the output pool has been filled                                                                                   |
| 3   | seq/arecordmidi  | seq      | Enumeration of possible messages, meanings; uses `snd_seq_poll_descriptors`, so no rawmidi                                                                                                                       |
| 4   | seq/aplaymidi2   | UMP      |                                                                                                                                                                                                                  |
| 5   | seq/arecordmidi2 | UMP      |                                                                                                                                                                                                                  |
| 6   | jack2-rawmidi    | raw      | Uses 2 ringbuffers: `data_ring`, `event_ring`; real-time thread with priority 80?; separate threads for input,output,scan                                                                                        |
| 7   | jack2-seq        | seq      | Ringbuffer for `early_events`; Also `void* jack_buf`                                                                                                                                                             |
| 7   | rtmidi           |          |                                                                                                                                                                                                                  |
| 8   | juce             |          |                                                                                                                                                                                                                  |
| 9   | fluidsynth       | raw, seq | Uses `snd_rawmidi_poll_descriptors`, `snd_seq_poll_descriptors`, `poll`; Threads: alsa-midi-raw, alsa-midi-seq -> realtime_prio; simple char\* buffer; handle event immediately upon receiving, so no ringbuffer |
