#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "display.h"
#include "mstring.h"

#define MAX_STORAGE_TRACE 16

static int      pos_storage_trace = 0;
static string_t storage_trace[MAX_STORAGE_TRACE] = { { .txt = NULL, } };

static inline void save_trace(string_t txt)
{
    string_t storage;

    storage = string_dup(txt);
    string_clean(&storage_trace[pos_storage_trace]);
    storage_trace[pos_storage_trace] = storage;

    pos_storage_trace++;
    pos_storage_trace %= MAX_STORAGE_TRACE;
}

string_t dump_log(void)
{
    string_t ret;
    int      i;

    ret = string_new();

    for(i = 0; i < MAX_STORAGE_TRACE; ++i) {
        int pos = (2 * MAX_STORAGE_TRACE + pos_storage_trace - 1 - i) % MAX_STORAGE_TRACE;
        ret = string_concat(ret, storage_trace[pos]);
    }

    string_dump(ret);
    return ret;
}

int print_log(char *txt, ...)
{
    char     buffer[1024];
    string_t str;
    va_list  ap;

    va_start(ap, txt);

    str = string_init_full(buffer, 0, sizeof(buffer), STRING_ALLOC_STATIC);
    str = string_concat(str, STRING_INIT_CSTR("["));
    str = string_add_date(str, "%a, %d %b %y %T %z");
    str = string_concat(str, STRING_INIT_CSTR("] [\033[32minfo\033[0m] "));
    str = string_add_format(str, txt, ap);
    string_dump(str);
    save_trace(str);

    string_clean(&str);
    va_end(ap);

    return 0;
}

int print_warning(char *txt, ...)
{
    char     buffer[1024];
    string_t str;

    va_list  ap;

    va_start(ap, txt);

    str = string_init_full(buffer, 0, sizeof(buffer), STRING_ALLOC_STATIC);
    str = string_concat(str, STRING_INIT_CSTR("["));
    str = string_add_date(str, "%a, %d %b %y %T %z");
    str = string_concat(str, STRING_INIT_CSTR("] [\033[33mwarning\033[0m] "));
    str = string_add_format(str, txt, ap);
    string_dump(str);
    save_trace(str);

    string_clean(&str);
    va_end(ap);

    return 0;
}

int print_error(char *txt, ...)
{
    char     buffer[1024];
    string_t str;

    va_list  ap;

    va_start(ap, txt);

    str = string_init_full(buffer, 0, sizeof(buffer), STRING_ALLOC_STATIC);
    str = string_concat(str, STRING_INIT_CSTR("["));
    str = string_add_date(str, "%a, %d %b %y %T %z");
    str = string_concat(str, STRING_INIT_CSTR("] [\033[31merror\033[0m] "));
    str = string_add_format(str, txt, ap);
    string_dump(str);
    save_trace(str);

    string_clean(&str);
    va_end(ap);

    return 0;
}

int print_debug(char *txt, ...)
{
    char     buffer[1024];
    string_t str;

    va_list  ap;

    va_start(ap, txt);

    str = string_init_full(buffer, 0, sizeof(buffer), STRING_ALLOC_STATIC);
    str = string_concat(str, STRING_INIT_CSTR("["));
    str = string_add_date(str, "%a, %d %b %y %T %z");
    str = string_concat(str, STRING_INIT_CSTR("] [\033[32mdebug\033[0m] "));
    str = string_add_format(str, txt, ap);
#ifdef DEBUG
    string_dump(str);
#endif /* DEBUG */        
    save_trace(str);

    string_clean(&str);
    va_end(ap);

    return 0;
}
