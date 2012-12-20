#include "mtimer.h"
#include "time_tool.h"

#include <stdlib.h>

struct mtimer {
    mtimer_t      *next;
    uint64_t       next_expiration;

    mtimer_kind_t  kind;
    uint32_t       period;
    mtimer_cb_f    cb;
    void          *data;
};

static mtimer_t *list = NULL;

static inline void mtimer_insert(mtimer_t *t)
{
    mtimer_t **previous;
    mtimer_t  *current;

    previous = &list;
    while(*previous) {
        current = *previous;
        if(t->next_expiration < current->next_expiration) {
            t->next   = current;
            *previous = t;
            return;
        }
        previous = &(*previous)->next;
    }

    t->next   = NULL;
    *previous = t;
}

mtimer_t * mtimer_add(uint32_t period, mtimer_kind_t kind, mtimer_cb_f cb, void *data)
{
    mtimer_t         *t;
    struct timeval    now;

    gettimeofday(&now, NULL);

    t = (mtimer_t *) malloc(sizeof(mtimer_t));

    t->kind            = kind;
    t->period          = period;
    t->cb              = cb;
    t->data            = data;

    t->next            = NULL;
    t->next_expiration = timeval_to_usec(&now) + period * 1000;

    mtimer_insert(t);

    return t;
}

int64_t mtimer_manage(struct timeval *now)
{
    uint64_t           cur;
    mtimer_t          *current;

    cur  = timeval_to_usec(now);

    while(list && list->next_expiration < cur + 1000) {
        current = list;
        list    = list->next;

        current->cb(current, now, current->data);
        if(current->kind == MTIMER_KIND_PERIODIC) {
            current->next_expiration += current->period * 1000;
            mtimer_insert(current);
        } else {
            free(current);
        }
    }

    if(list == NULL)
        return -1;
    return list->next_expiration - cur;
}

int mtimer_cancel(mtimer_t *t)
{
    mtimer_t **previous;
    mtimer_t  *current;

    t->kind  = -1;
    previous = &list;
    while(*previous) {
        current = *previous;
        if(current == t) {
            *previous = (*previous)->next;
            free(current);
            return 0;
        }
        previous = &(*previous)->next;
    }

    return 0;
}
