/* @file fifo.h
 * @brief Following [FAbian's Realtime Box o' Tricks](https://github.com/hogliux/farbot)
 *
 *
 *
 */
#pragma once

#include <common-types.h>
#include <pthread.h>

typedef struct
{
    u32 reserve;

} sndx_fifo_single_t;

typedef struct
{
    u32 reserve;

    struct
    {
        u32 max_threads;
        u32 num_threads;

        struct
        {
            pthread_t tid;
            u32       pos;

        }* tinfos;

    } posinfo;

} sndx_fifo_multiple_t;

typedef struct
{

    isize cap;
    u8*   buf;
    void* reader;
    void* writer;

} sndx_fifo_t;

// No way to check capacity in farbot, is not a ring buffer!
isize sndx_fifo_push(sndx_fifo_t* f, u8* src, isize len);
isize sndx_fifo_pop(sndx_fifo_t* f, u8* dst, isize len);
