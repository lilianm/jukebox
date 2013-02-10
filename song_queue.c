#include <stdlib.h>
#include <assert.h>

#include "song_queue.h"

struct song_queue {
    size_t first;
    size_t pos;
    size_t nb;

    size_t size;
    int    mids[0];
};

void song_queue_clean(song_queue_t *q)
{
    assert(q);

    q->nb    = 0;
    q->pos   = -1;
    q->first = 0;
}

void song_queue_free(song_queue_t *q)
{
    free(q);
}

void song_queue_init(song_queue_t *q, size_t size)
{
    assert(q);

    q->size  = size;
    song_queue_clean(q);
}

song_queue_t * song_queue_new(size_t size)
{
    song_queue_t *q;

    q = (song_queue_t *) malloc(sizeof(song_queue_t) + size * sizeof(int));

    song_queue_init(q, size);

    return q;
}

int song_queue_get(song_queue_t *q, int idx)
{
    if(q->nb >= ((idx + q->pos) & (q->size - 1)) ||
       q->pos + idx < q->first)
        return -1;

    idx = (q->first + q->pos) & (q->size - 1);

    return q->mids[idx];
}

void song_queue_add(song_queue_t *q, int mid)
{
    int idx;

    if(q->size == q->nb) {
        q->first++;
    } else {
        q->nb++;
    }

    idx = (q->first + q->nb - 1) & (q->size - 1);

    q->mids[idx] = mid;
}

int song_queue_next(song_queue_t *q)
{
    int idx;

    if(q->pos == q->first + q->nb - 1)
        return -1;

    q->pos++;

    idx = q->pos & (q->size - 1);

    return q->mids[idx];
}

int song_queue_previous(song_queue_t *q)
{
    int idx;

    if(q->pos == (unsigned)-1)
        return -1;

    if(q->pos != q->first)
        q->pos--;

    idx = q->pos & (q->size - 1);

    return q->mids[idx];
}

void song_queue_shuffle(song_queue_t *q)
{
    int nb  = (q->nb - (q->pos - q->first)) & (q->size - 1);
    int idx1, idx2;
    int tmp;
    int i;

    for(i = nb; --i;) {
        idx1 = rand() % i; /* Change this stupid line */
        idx1 = (q->first + q->pos + idx1) & (q->size - 1);
        idx2 = (q->first + q->pos + i)    & (q->size - 1);

        /* Swap */
        tmp           = q->mids[idx1];
        q->mids[idx1] = q->mids[idx2];
        q->mids[idx2] = tmp;
    }
}
