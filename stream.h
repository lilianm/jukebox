#ifndef _STREAM_H_
#define _STREAM_H_

#include <string.h>
#include <assert.h>
#include <ctype.h>

typedef struct stream_t {
    void *data;
    void *end;
} stream_t;

static inline stream_t stream_init(void *data, size_t size)
{
    assert(data);

    stream_t ret;
    ret.data = data;
    ret.end  = data + size;

    return ret;
}

static inline stream_t stream_init_ptr(void *data, void *end)
{
    assert(data);
    assert(end);

    stream_t ret;
    ret.data = data;
    ret.end  = end;

    return ret;
}

static inline int stream_len(stream_t *s)
{
    assert(s);
    return (s->end - s->data);
}

static inline void stream_skip(stream_t *s, int n)
{
    assert(n > 0);
    assert(s->data + n <= s->end);
    s->data += n;
}

static inline void stream_expand(stream_t *s, int n)
{
    assert(n > 0);
    s->end += n;
}

static inline void stream_shrink(stream_t *s, int n)
{
    assert(n > 0);
    assert(stream_len(s) >= n);
    s->end -= n;
}

static inline void stream_clip(stream_t *s, void *pos)
{
    assert(pos <= s->end);
    s->data = pos;
}

static inline void stream_strip(stream_t *s)
{
    assert(s);
    while(isspace(((char*)s->data)[ 0]))
        stream_skip(s, 1);
    while(isspace(((char*)s->end )[-1]))
        stream_shrink(s, 1);
}

static inline int stream_find_chr(stream_t *s, int c, stream_t *out)
{
    assert(s);
    assert(out);

    void *pos;

    pos = memchr(s->data, c, stream_len(s));

    if(pos == NULL)
        return -1;

    *out = stream_init_ptr(s->data, pos);
    stream_clip(s, pos);

    return 0;
}

static inline int stream_find_mem(stream_t *s, void *mem, size_t size, stream_t *out)
{
    assert(s);
    assert(out);

    void *pos;

    if((signed)size > stream_len(s))
        return -1;

    pos = memmem(s->data, stream_len(s), mem, size);

    if(pos == NULL)
        return -1;

    *out = stream_init_ptr(s->data, pos);
    stream_clip(s, pos + size);

    return 0;
}



#endif /* _STREAM_H_ */
