#ifndef __MTIMER_H__
#define __MTIMER_H__

#include <sys/time.h>
#include <stdint.h>

typedef struct mtimer mtimer_t;

typedef enum mtimer_kind {
    MTIMER_KIND_PERIODIC,
    MTIMER_KIND_ONESHOT,
} mtimer_kind_t;

typedef void (*mtimer_cb_f)(mtimer_t *, const struct timeval *now, void *data);

int64_t    mtimer_manage(struct timeval *now);
mtimer_t * mtimer_add(uint32_t period, mtimer_kind_t kind, mtimer_cb_f cb, void *data);
int        mtimer_cancel(mtimer_t *t);

#endif /* __MTIMER_H__ */
