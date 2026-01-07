/* Single writer
 *
 * TODO: How to tell if correct and no data race?
 *
 * */
#include <common-types.h>
#include <pthread.h>

typedef struct list_t list_t;
typedef struct list_t
{

    list_t* next;

} list_t;

typedef struct
{
    pthread_t tid;

    list_t* head;

    list_t* new;
    isize   len;

} writer_data_t;

typedef struct
{
    isize     idx;
    pthread_t tid;

    writer_data_t* data;
    isize          len;

} reader_data_t;

void* job_writer_insert(void* data)
{
    writer_data_t* d = data;

    RANGE(i, d->len)
    {
        // Prepare new node
        d->new[i].next = d->head;

        // Publish
        d->head = &d->new[i];
    }

    return 0;
}

void* job_reader(void* data)
{
    reader_data_t* d = data;

    // Subscribe
    list_t* head = d->data->head;

    // Process 'new' state
    isize len = 1;
    while (head->next)
    {
        len++;
        head = head->next;
    }
    d->len = len;

    return 0;
}

int main()
{
    int err = 0;

    constexpr isize num_readers = 10;
    constexpr isize num_data    = 1'000;
    constexpr isize num_new     = 1'000'000;

    list_t*        data     = nullptr;
    list_t*        data_new = nullptr;
    reader_data_t* readers  = nullptr;

    data = calloc(num_data, sizeof(list_t));
    err  = (!data);
    Goto(err, __close_rtobject, "Failed: calloc: data");

    data_new = calloc(num_new, sizeof(list_t));
    err      = (!data_new);
    Goto(err, __close_rtobject, "Failed: calloc: data_new");

    readers = calloc(num_readers, sizeof(reader_data_t));
    err     = (!readers);
    Goto(err, __close_rtobject, "Failed: calloc: readers");

    RANGE(i, num_data - 1) { data[i].next = &data[i + 1]; }

    writer_data_t writer = {
        .tid  = 0,
        .head = data,
        .new  = data_new,
        .len  = num_new,
    };

    RANGE(i, num_readers)
    {
        readers[i].idx  = i;
        readers[i].data = &writer;
        readers[i].len  = -1;
    }

    Timestamp(start);

    err = pthread_create(&writer.tid, NULL, job_writer_insert, &writer);
    Goto(err, __close_rtobject, "Failed: pthread_create: job_writer_insert");

    RANGE(i, num_readers)
    {
        err = pthread_create(&readers[i].tid, NULL, job_reader, &readers[i]);
        Goto(err, __close_rtobject, "Failed: pthread_create: job_reader");
    }

    err = pthread_join(writer.tid, 0);
    Goto(err, __close_rtobject, "Failed: pthread_join: writer");

    RANGE(i, num_readers)
    {
        err = pthread_join(readers[i].tid, 0);
        Goto(err, __close_rtobject, "Failed: pthread_join: readers[%ld]", i);
    }

    Timestamp(stop);

    f32 elapsed = ToSec(stop) - ToSec(start);
    p_info("Elapsed: %.4f millisecs", elapsed * 1e3);

    RANGE(i, num_readers) { p_info("%ld: %ld", i, readers[i].len); }

    err = 0;

__close_rtobject:

    if (data) free(data);
    if (data_new) free(data_new);
    if (readers) free(readers);

    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
