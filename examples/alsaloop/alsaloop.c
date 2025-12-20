#include "alsaloop.h"

int                     loopbacks_count = 0;
struct loopback**       loopbacks       = NULL;
struct loopback_thread* threads         = NULL;

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stderr, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    // err = create_loopback_handle(&play, arg_pdevice, arg_pctl, "playback");
    // err = create_loopback_handle(&capt, arg_cdevice, arg_cctl, "capture");
    // err = create_loopback(&loop, play, capt, output);
    // set_loop_time(loop, arg_loop_time);
    // add_loop(loop);

    // threads_count = j;
    // main_job = pthread_self();

    // signal(SIGINT, signal_handler);
    // signal(SIGTERM, signal_handler);
    // signal(SIGABRT, signal_handler);
    // signal(SIGUSR1, signal_handler_state);
    // signal(SIGUSR2, signal_handler_ignore);

    // for (k = 0; k < threads_count; k++)
    //     thread_job(&threads[k]) {
    //
    //         setscheduler();
    //
    //         for (i = 0; i < thread->loopbacks_count; i++)
    //             err = pcmjob_init(thread->loopbacks[i]);
    //
    //         for (i = 0; i < thread->loopbacks_count; i++)
    //             err = pcmjob_start(thread->loopbacks[i]);
    //
    //         while (!quit) {
    //
    //             for (i = j = 0; i < thread->loopbacks_count; i++)
    //                 err = pcmjob_pollfds_init(thread->loopbacks[i], &pfds[j]);
    //
    //                 err = poll(pfds, j, wake);
    //
    //                 for (i = j = 0; i < thread->loopbacks_count; i++) {
    //                     struct loopback* loop = thread->loopbacks[i];
    //                     if (j < loop->active_pollfd_count)
    //                         err = pcmjob_pollfds_handle(loop, &pfds[j]);
    //                     j += loop->active_pollfd_count;
    //                }
    //
    //         }
    //
    //     }
    //
    // if (j > 1)
    //     for (k = 0; k < threads_count; k++)
    //         pthread_join(threads[k].thread, NULL);

    // __close:

    snd_output_close(output);

    return 0;
}
