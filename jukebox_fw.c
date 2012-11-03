#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <ruby.h>

#include "display.h"
#include "mp3.h"

VALUE cMp3Stream;
VALUE cMp3Info;

static VALUE mp3_stream_rb_read(VALUE self, VALUE pos)
{
    mp3_stream_t *stream;
    mp3_buffer_t  buffer;
    VALUE         rb_buffer;
    double        cpos;
    VALUE         mp3Buffer;

    Data_Get_Struct(self, mp3_stream_t, stream);

    cpos = NUM2DBL(pos);
    if(mp3_stream_read(stream, cpos, &buffer) == -1)
        return Qnil;

    rb_buffer = rb_tainted_str_new(buffer.buf, buffer.size);
    mp3Buffer = rb_const_get(rb_cObject, rb_intern("Mp3Buffer"));
    return rb_funcall(mp3Buffer, rb_intern("new"), 2,
                      rb_buffer, rb_float_new(buffer.duration));;
}

static VALUE mp3_stream_new(VALUE class, VALUE file)
{
    mp3_stream_t *stream;
    VALUE         tdata;

    stream = ALLOC(mp3_stream_t);
    mp3_stream_init(stream, StringValuePtr(file));
    tdata  = Data_Wrap_Struct(class, 0, mp3_stream_close, stream);
    rb_obj_call_init(tdata, 0, NULL);
    return tdata;
}

static VALUE mp3_duration(VALUE self)
{
    mp3_info_t *info;

    Data_Get_Struct(self, mp3_info_t, info);

    return INT2FIX((int)info->duration);
}

static VALUE mp3_artist(VALUE self)
{
    mp3_info_t *info;

    Data_Get_Struct(self, mp3_info_t, info);

    if(info->artist == NULL)
        return Qnil;
    return rb_tainted_str_new2(info->artist);
}

static VALUE mp3_album(VALUE self)
{
    mp3_info_t *info;

    Data_Get_Struct(self, mp3_info_t, info);

    if(info->album == NULL)
        return Qnil;
    return rb_tainted_str_new2(info->album);
}

static VALUE mp3_title(VALUE self)
{
    mp3_info_t *info;

    Data_Get_Struct(self, mp3_info_t, info);

    if(info->title == NULL)
        return Qnil;
    return rb_tainted_str_new2(info->title);
}

static VALUE mp3_years(VALUE self)
{
    mp3_info_t *info;

    Data_Get_Struct(self, mp3_info_t, info);

    return INT2FIX((int)info->years);
}

static VALUE mp3_track(VALUE self)
{
    mp3_info_t *info;

    Data_Get_Struct(self, mp3_info_t, info);

    return INT2FIX((int)info->track);
}

static VALUE mp3_nb_track(VALUE self)
{
    mp3_info_t *info;

    Data_Get_Struct(self, mp3_info_t, info);

    return INT2FIX((int)info->nb_track);
}

static void mp3_free(mp3_info_t *info)
{
    mp3_info_free(info);
    xfree(info);
}

static VALUE mp3_new(VALUE class, VALUE file)
{
  mp3_info_t *info = ALLOC(mp3_info_t);
  mp3_info_decode(info, StringValuePtr(file));
  VALUE tdata = Data_Wrap_Struct(class, 0, mp3_free, info);
  rb_obj_call_init(tdata, 0, NULL);
  return tdata;
}

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

void Init_jukebox_fw() {
    rb_require("./mp3_struct.rb");

    rb_define_global_function("log",         display_log,         1);
    rb_define_global_function("error",       display_error,       1);
    rb_define_global_function("warning",     display_warning,     1);
    rb_define_global_function("debug",       display_debug,       1);
    rb_define_global_function("dump_events", display_dump_events, 0);

    cMp3Stream = rb_define_class("Mp3Stream", rb_cObject);
    rb_define_singleton_method(cMp3Stream, "new",  mp3_stream_new, 1);
    rb_define_method(cMp3Stream,           "read", mp3_stream_rb_read, 1);

    cMp3Info = rb_define_class("Mp3Info", rb_cObject);
    rb_define_singleton_method(cMp3Info, "new", mp3_new, 1);
    rb_define_method(cMp3Info, "duration", mp3_duration, 0);
    rb_define_method(cMp3Info, "title", mp3_title, 0);
    rb_define_method(cMp3Info, "album", mp3_album, 0);
    rb_define_method(cMp3Info, "artist", mp3_artist, 0);
    rb_define_method(cMp3Info, "years", mp3_years, 0);
    rb_define_method(cMp3Info, "track", mp3_track, 0);
    rb_define_method(cMp3Info, "nb_track", mp3_nb_track, 0);
}
