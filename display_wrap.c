#include <ruby.h>

#include "display.h"
#include "string.h"


static VALUE display_log(VALUE obj, VALUE txt)
{
    obj = obj;

    if(TYPE(txt) != T_STRING)
        rb_raise(rb_eTypeError, "not valid value");

    print_log(string_init_full(RSTRING_PTR(txt),
                               RSTRING_LEN(txt), RSTRING_LEN(txt),
                               STRING_ALLOC_STATIC));
    return Qtrue;
}

static VALUE display_error(VALUE obj, VALUE txt)
{
    obj = obj;

    if(TYPE(txt) != T_STRING)
        rb_raise(rb_eTypeError, "not valid value");

    print_error(string_init_full(RSTRING_PTR(txt),
                                 RSTRING_LEN(txt), RSTRING_LEN(txt),
                                 STRING_ALLOC_STATIC));
    return Qtrue;
}

static VALUE display_warning(VALUE obj, VALUE txt)
{
    obj = obj;

    if(TYPE(txt) != T_STRING)
        rb_raise(rb_eTypeError, "not valid value");

    print_warning(string_init_full(RSTRING_PTR(txt),
                                   RSTRING_LEN(txt), RSTRING_LEN(txt),
                                   STRING_ALLOC_STATIC));
    return Qtrue;
}

static VALUE display_debug(VALUE obj, VALUE txt)
{
    obj = obj;

    if(TYPE(txt) != T_STRING)
        rb_raise(rb_eTypeError, "not valid value");

    print_debug(string_init_full(RSTRING_PTR(txt),
                                 RSTRING_LEN(txt), RSTRING_LEN(txt),
                                 STRING_ALLOC_STATIC));
    return Qtrue;
}

static VALUE display_dump_events(VALUE obj)
{
    VALUE       ret;
    string_t    v;
    char       *ptr;
    size_t      len;

    v = dump_log();
    obj = obj;

    len = v.len;
    ptr = ALLOC_N(char, len);

    memcpy(ptr, v.txt, len);
    ret = rb_str_new(ptr, len);

    string_clean(&v);

    return ret;
}

void Init_display() {
    rb_define_global_function("log",         display_log,         1);
    rb_define_global_function("error",       display_error,       1);
    rb_define_global_function("warning",     display_warning,     1);
    rb_define_global_function("debug",       display_debug,       1);
    rb_define_global_function("dump_events", display_dump_events, 0);
}
