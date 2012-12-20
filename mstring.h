#ifndef __MSTRING_H__
#define __MSTRING_H__

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef enum string_alloc_t {
    STRING_ALLOC_DYNAMIC,
    STRING_ALLOC_STATIC,
} string_alloc_t;

typedef struct string_t {
    char        *txt;
    uint32_t     len  :14;
    uint32_t     size :14;
    uint32_t     alloc: 4;
} string_t;

#define STRING_INIT_CSTR(str)                                           \
(string_t) {                                                            \
    .txt   = str,                                                       \
    .len   = sizeof(str) - 1,                                           \
    .size  = sizeof(str) - 1,                                           \
    .alloc = STRING_ALLOC_STATIC                                        \
}

#define STRING_INIT_NULL                                                \
(string_t) {                                                            \
    .txt   = NULL,                                                      \
    .len   = 0,                                                         \
    .size  = 0,                                                         \
    .alloc = STRING_ALLOC_STATIC                                        \
}

static inline string_t string_init_full(char *txt, size_t len,
                                        size_t size, string_alloc_t alloc)
{
    string_t str = {
        .txt   = txt,
        .len   = len,
        .size  = size,
        .alloc = alloc
    };

    return str;
}

static inline string_t string_init_static(char *txt)
{
    size_t len;

    len = strlen(txt);

    return string_init_full(txt, len, len + 1, STRING_ALLOC_STATIC);
}

static inline string_t string_init(char *txt)
{
    size_t len;

    len = strlen(txt);

    return string_init_full(txt, len, len + 1, STRING_ALLOC_DYNAMIC);
}

static inline string_t string_new(void)
{
    return string_init_full(NULL, 0, 0, STRING_ALLOC_DYNAMIC);
}

static inline void string_clean(string_t *str)
{
    switch(str->alloc) {
    case STRING_ALLOC_DYNAMIC:
        free(str->txt);
        break;

    case STRING_ALLOC_STATIC:
        break;
    }

    str->txt  = NULL;
    str->len  = 0;
    str->size = 0;
}

static inline string_t string_dup(string_t str)
{
    string_t ret;

    switch(str.alloc) {
    case STRING_ALLOC_DYNAMIC:
    case STRING_ALLOC_STATIC:
        ret.alloc = STRING_ALLOC_DYNAMIC;
        ret.size  = str.size;
        ret.len   = str.len;
        ret.txt   = malloc(str.size);

        memcpy(ret.txt, str.txt, ret.len + 1);
        break;
    }
    return ret;
}

string_t string_concat(string_t txt1, string_t txt2);

string_t string_add_chr(string_t str, char c);

static inline string_t string_add_date(string_t str, char *format)
{
    time_t    t;
    struct tm tmp;
    size_t    len;

    t = time(NULL);
    localtime_r(&t, &tmp);

    len = strftime(str.txt + str.len, str.size - str.len, format, &tmp);
    str.len += len;

    return str;
}

static inline void string_dump(string_t str)
{
    printf("%.*s\n", str.len, str.txt);
}

#endif /* __MSTRING_H__ */
