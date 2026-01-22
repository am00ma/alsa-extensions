# Callbacks

Callbacks from various frameworks:

| Name     | Signature                                                                                                                                                                                                                                                                                                    | Comments                                                                                                        |
| -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------- |
| jack     | `typedef int (*JackProcessCallback)(jack_nframes_t nframes, void *arg)`                                                                                                                                                                                                                                      | Buffer, rate, etc. accessible with jack client id, is synced with midi                                          |
| juce     | `void getNextAudioBlock(const AudioSourceChannelInfo &bufferToFill)`, `void audioDeviceIOCallbackWithContext (const float *const *inputChannelData, int numInputChannels, float *const *outputChannelData, int numOutputChannels, int numSamples, const AudioIODeviceCallbackContext &context [hostTimeNs])` | Use audioDeviceIOCallbackWithContext in case we need duplex, otherwise for playback, just use getNextAudioBlock |
| clap     | `clap_process_status(CLAP_ABI *process)(const struct clap_plugin *plugin, const clap_process_t *process)`                                                                                                                                                                                                    | Uses structs to clean up callback, clap_process_t has all the info we need                                      |
| pipewire | `void on_process(void *userdata)`, `struct pw_buffer *b;b = pw_stream_dequeue_buffer(stream);.. consume stuff in the buffer ...;pw_stream_queue_buffer(stream, b);`                                                                                                                                          | No indication of what info is available                                                                         |
| rtaudio  | `int inout(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void *data)`                                                                                                                                                                    | No idea where info about number of inputs and outputs is stored                                                 |
| rtmidi   | `midiout->sendMessage( &message )`; `stamp = midiin->getMessage(&message)`; `void mycallback(double deltatime, std::vector<unsigned char> *message, void *userData)`                                                                                                                                         | Realtime, or on callback                                                                                        |

## CLAP

```cpp
enum {
   // Processing failed. The output buffer must be discarded.
   CLAP_PROCESS_ERROR = 0,

   // Processing succeeded, keep processing.
   CLAP_PROCESS_CONTINUE = 1,

   // Processing succeeded, keep processing if the output is not quiet.
   CLAP_PROCESS_CONTINUE_IF_NOT_QUIET = 2,

   // Rely upon the plugin's tail to determine if the plugin should continue to process.
   // see clap_plugin_tail
   CLAP_PROCESS_TAIL = 3,

   // Processing succeeded, but no more processing is required,
   // until the next event or variation in audio input.
   CLAP_PROCESS_SLEEP = 4,
};
typedef int32_t clap_process_status;

typedef struct clap_process {
   // A steady sample time counter.
   // This field can be used to calculate the sleep duration between two process calls.
   // This value may be specific to this plugin instance and have no relation to what
   // other plugin instances may receive.
   //
   // Set to -1 if not available, otherwise the value must be greater or equal to 0,
   // and must be increased by at least `frames_count` for the next call to process.
   int64_t steady_time;

   // Number of frames to process
   uint32_t frames_count;

   // time info at sample 0
   // If null, then this is a free running host, no transport events will be provided
   const clap_event_transport_t *transport;

   // Audio buffers, they must have the same count as specified
   // by clap_plugin_audio_ports->count().
   // The index maps to clap_plugin_audio_ports->get().
   // Input buffer and its contents are read-only.
   const clap_audio_buffer_t *audio_inputs;
   clap_audio_buffer_t       *audio_outputs;
   uint32_t                   audio_inputs_count;
   uint32_t                   audio_outputs_count;

   // The input event list can't be modified.
   // Input read-only event list. The host will deliver these sorted in sample order.
   const clap_input_events_t  *in_events;

   // Output event list. The plugin must insert events in sample sorted order when inserting events
   const clap_output_events_t *out_events;
} clap_process_t;

// Sample code for reading a stereo buffer:
//
// bool isLeftConstant = (buffer->constant_mask & (1 << 0)) != 0;
// bool isRightConstant = (buffer->constant_mask & (1 << 1)) != 0;
//
// for (int i = 0; i < N; ++i) {
//    float l = data32[0][isLeftConstant ? 0 : i];
//    float r = data32[1][isRightConstant ? 0 : i];
// }
//
// Note: checking the constant mask is optional, and this implies that
// the buffer must be filled with the constant value.
// Rationale: if a buffer reader doesn't check the constant mask, then it may
// process garbage samples and in result, garbage samples may be transmitted
// to the audio interface with all the bad consequences it can have.
//
// The constant mask is a hint.
typedef struct clap_audio_buffer {
   // Either data32 or data64 pointer will be set.
   float  **data32;
   double **data64;
   uint32_t channel_count;
   uint32_t latency; // latency from/to the audio interface
   uint64_t constant_mask;
} clap_audio_buffer_t;

// Input event list. The host will deliver these sorted in sample order.
typedef struct clap_input_events {
   void *ctx; // reserved pointer for the list

   // returns the number of events in the list
   uint32_t(CLAP_ABI *size)(const struct clap_input_events *list);

   // Don't free the returned event, it belongs to the list
   const clap_event_header_t *(CLAP_ABI *get)(const struct clap_input_events *list, uint32_t index);
} clap_input_events_t;

// Output event list. The plugin must insert events in sample sorted order when inserting events
typedef struct clap_output_events {
   void *ctx; // reserved pointer for the list

   // Pushes a copy of the event
   // returns false if the event could not be pushed to the queue (out of memory?)
   bool(CLAP_ABI *try_push)(const struct clap_output_events *list,
                            const clap_event_header_t       *event);
} clap_output_events_t;
```
