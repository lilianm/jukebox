#ifndef __TIME_TOOL_H__
#define __TIME_TOOL_H__

static inline int64_t timeval_diff(const struct timeval *new,
                                   const struct timeval *old)
{
    return ((int64_t)new->tv_sec - old->tv_sec) * 1000000 +
        (new->tv_usec - old->tv_usec);
}

static inline void timeval_add_usec(struct timeval *tv, uint64_t v)
{
    tv->tv_sec  += v / 1000000;
    tv->tv_usec += v % 1000000;
    if(tv->tv_usec > 1000000) { // Wrap
        tv->tv_usec -= 1000000;
        tv->tv_sec  += 1;
    }
}

static inline uint64_t timeval_to_usec(const struct timeval *tv)
{
    return ((int64_t)tv->tv_sec) * 1000000 + tv->tv_usec;
}

#endif /* __TIME_TOOL_H__ */
