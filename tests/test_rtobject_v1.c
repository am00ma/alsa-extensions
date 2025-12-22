/* Read only list */
#include "sndx/types.h"
#include <common-types.h>
#include <pthread.h>

typedef struct
{
    isize     idx;
    pthread_t tid;

    i32*  data;
    isize len;

    isize key;
    i32   val;

} data_t;

void* job_reader(void* data)
{
    data_t* d = data;

    isize found = -1;
    RANGE(i, d->len)
    {
        if (d->val != d->data[i]) continue;
        found = i;
        break;
    }

    // p_info("%16ld -> %16d @ %6ld", d->tid, d->val, found);
    Assert((found == d->key));

    return 0;
}

int main()
{
    int err;

    constexpr isize num_readers = 1'000;
    constexpr isize num_data    = 10'000'000;

    i32* data = calloc(num_data, sizeof(i32));
    Goto((!data), __close_rtobject, "Failed: calloc");

    isize a = 1103515245;
    isize c = 12345;
    isize m = 1 << 31;

    data[0] = 123456;
    RANGE(i, 1, num_data) { data[i] = (a * data[i - 1] + c) % m; }

    data_t readers[num_readers];
    RANGE(i, num_readers)
    {
        readers[i].idx  = i;
        readers[i].data = data;
        readers[i].len  = num_data;
        readers[i].key  = num_data - i - 1;
        readers[i].val  = data[num_data - i - 1];
    }

    Timestamp(start);

    // Start
    RANGE(i, num_readers)
    {
        err = pthread_create(&readers[i].tid, NULL, job_reader, &readers[i]);
        Goto(err, __close_rtobject, "Failed: pthread_create");
    }

    // Finish
    RANGE(i, num_readers)
    {
        err = pthread_join(readers[i].tid, 0);
        Goto(err, __close_rtobject, "Failed: pthread_join");
    }

    Timestamp(stop);

    f32 elapsed = ToSec(stop) - ToSec(start);
    p_info("Elapsed: %.4f secs", elapsed);

    return EXIT_SUCCESS;

__close_rtobject:

    if (data) free(data);

    return EXIT_FAILURE;
}
