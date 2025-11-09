/*! \file jack2.c
    \brief Outline of how jack2 uses alsa.

    Details.
*/
#include "duplex.h"

// int jack2_open(
//     sndx_duplex_t* d, const char* playback_device, const char* capture_device, sndx_params_t* params, output_t* output);
// int jack2_close(sndx_duplex_t* d);
// int jack2_start(sndx_duplex_t* d);
// int jack2_stop(sndx_duplex_t* d);
// int jack2_restart(sndx_duplex_t* d);
// int jack2_wait(sndx_duplex_t* d);
// int jack2_read(sndx_duplex_t* d);
// int jack2_write(sndx_duplex_t* d);
//
// sndx_duplex_ops_t jack2_ops = {
//     .open_fn    = jack2_open,    //
//     .close_fn   = jack2_close,   //
//     .start_fn   = jack2_start,   //
//     .stop_fn    = jack2_stop,    //
//     .restart_fn = jack2_restart, //
//     .wait_fn    = jack2_wait,    //
//     .read_fn    = jack2_read,    //
//     .write_fn   = jack2_write    //
// };

// Application
int main()
{
    int err; ///< Brief description after the member

    output_t* output;
    err = snd_output_stdio_attach(&output, stderr, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:FC1,0", "hw:FC1,0",          // play, capt device
        SND_PCM_FORMAT_S16_LE,           // format
        48000,                           // rate
        256, 128,                        // buffer_size, period_size
        SND_PCM_ACCESS_MMAP_INTERLEAVED, // access
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    // d->ops = &jack2_ops;

    err = sndx_duplex_close(d);
    SndCheck_(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
