/* Read only list */
#include <common-types.h>
#include <pthread.h>

typedef struct list_t list_t;
typedef struct list_t
{

    isize key;
    i32   val;

    list_t* prev;
    list_t* next;

} list_t;

typedef struct
{
    isize     idx;
    pthread_t tid;

    list_t* head;

    isize query;
    i32   val;

} data_t;

void* job_reader(void* data)
{
    data_t* d = data;

    list_t* head = d->head;

    while (head)
    {
        if (d->query == head->key) { break; }
        head = head->next;
    }

    AssertMsg(head, "Query not found: %ld", d->query);
    Assert((d->val == head->val));

    return 0;
}

int main()
{
    int err;

    constexpr isize num_readers = 10;
    constexpr isize num_data    = 1'000;

    list_t* data = calloc(num_data, sizeof(list_t));
    Goto((!data), __close_rtobject, "Failed: calloc");

    isize a = 1103515245;
    isize c = 12345;
    isize m = 1 << 31;

    // Connect the list

    data[0] = (list_t){
        .key  = 0,
        .val  = 12345,
        .next = &data[1],
        .prev = nullptr,
    };

    RANGE(i, 1, num_data - 1)
    {
        data[i].key      = i;
        data[i].val      = (a * data[i - 1].val + c) % m;
        data[i].next     = &data[i + 1];
        data[i + 1].prev = &data[i];
    }

    data[num_data - 1] = (list_t){
        .key  = num_data - 1,
        .val  = (a * data[num_data - 2].val + c) % m,
        .next = nullptr,
        .prev = &data[num_data - 2],
    };

    data_t readers[num_readers];
    RANGE(i, num_readers)
    {
        readers[i].idx   = i;
        readers[i].head  = data;
        readers[i].query = num_data - i - 1;           // Query from end
        readers[i].val   = data[num_data - i - 1].val; // Since we know ordered
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
    p_info("Elapsed: %.4f millisecs", elapsed * 1e3);

    return EXIT_SUCCESS;

__close_rtobject:

    if (data) free(data);

    return EXIT_FAILURE;
}
