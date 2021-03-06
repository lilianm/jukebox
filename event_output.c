#include "event_output.h"

#include <errno.h>
#include <stdlib.h>
#include <assert.h>

void event_output_init(event_output_t *output)
{
    assert(output);

    output->read   = 0;
    output->write  = 0;
}

event_output_t * event_output_new(void)
{
    event_output_t *output;

    output = (event_output_t *) malloc(sizeof(event_output_t));
    event_output_init(output);

    return output;
}

void event_output_clean(event_output_t *output)
{
    int idx;

    assert(output);

    while(output->write != output->read) {
        idx = output->read & (EVENT_OUTPUT_MAX_BUFFER - 1);
        output->buffer[idx].free(output->buffer[idx].data, output->buffer[idx].user_data);
        output->read++;
    }
}

void event_output_free(event_output_t *output)
{
    event_output_clean(output);
    free(output);
}

static void event_output_send_callback(event_t *ev, int sck, void *data_user)
{
    event_output_t *output;
    int             ret         = -1;
    int             offset      =  0;
    int             idx;
    void           *data;
    void           *user_data;
    size_t          size;
    free_f          free_cb;

    output = (event_output_t *) data_user;
    offset = output->offset;

    if(output->write == output->read) { // What ???
        event_client_clr_on_write(ev);
        return;
    }

    while(output->write != output->read) {
        idx       = output->read & (EVENT_OUTPUT_MAX_BUFFER - 1);
        data      = output->buffer[idx].data;
        size      = output->buffer[idx].size;
        free_cb   = output->buffer[idx].free;
        user_data = output->buffer[idx].user_data;
        retry:
        ret = send(sck, data + offset, size - offset,
                   MSG_DONTWAIT | MSG_NOSIGNAL);
        if(ret > 0) {
            if(ret == (signed) size - offset) {
                if(free_cb)
                    free_cb(data, user_data);
                output->read++;
                offset = 0;
                continue;
            }
            offset += ret;
        } else {
            if(ret == -1) {
                if(errno == EINTR)
                    goto retry;

                if(errno == EAGAIN ||
                   errno == EWOULDBLOCK ||
                   errno == ENOBUFS)
                    break;
            }

            while(output->write != output->read) {
                idx     = output->read & (EVENT_OUTPUT_MAX_BUFFER - 1);
                data    = output->buffer[idx].data;
                free_cb = output->buffer[idx].free;

                if(free_cb)
                    free_cb(data, user_data);
                output->read++;
            }

            event_client_clr_on_write(ev);
            output->offset = 0;
            return;
        }
    }

    output->offset = offset;
    if(output->write == output->read) {
        event_client_clr_on_write(ev);
    }
}

int event_output_send(event_t *ev, int sck, void *data, size_t size, free_f free_cb, void *user_data)
{
    event_output_t *output;
    int             ret         = -1;
    int             offset      =  0;
    int             idx;

    output = (event_output_t *) event_get_data(ev);

    if(output == NULL)
        return -1;

    if(output->write == output->read) { // Try to send
        while(ret == -1) {
            ret = send(sck, data, size, MSG_DONTWAIT | MSG_NOSIGNAL);
            if(ret >= 0) {
                if(ret == (signed) size) {
                    if(free_cb)
                        free_cb(data, user_data);
                    return 0;
                }
                offset = ret;
            } else {
                if(errno == EINTR)
                    continue;

                if(errno == EAGAIN ||
                   errno == EWOULDBLOCK ||
                   errno == ENOBUFS)
                    break;

                if(free_cb)
                    free_cb(data, user_data);
                return -1;
            }
        }
    }

    if((output->write - output->read) >= EVENT_OUTPUT_MAX_BUFFER)
        return -1;

    idx = output->write & (EVENT_OUTPUT_MAX_BUFFER - 1);

    output->buffer[idx].data      = data;
    output->buffer[idx].size      = size;
    output->buffer[idx].free      = free_cb;
    output->buffer[idx].user_data = user_data;
    output->offset                = offset;

    output->write++;

    event_client_set_on_write(ev, event_output_send_callback);

    return 0;
}
