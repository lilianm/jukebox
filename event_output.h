#ifndef __EVENT_OUTPUT_H__
#define __EVENT_OUTPUT_H__

#include <stdint.h>

#include "event.h"


#ifndef EVENT_OUTPUT_MAX_BUFFER
#define EVENT_OUTPUT_MAX_BUFFER 16
#endif /* EVENT_OUTPUT_MAX_BUFFER */

typedef void (*free_f)(void *data, void *user_data);

typedef struct output_buffer {
    void                        *data;
    size_t                       size;
    free_f                       free;
    void                        *user_data;
} output_buffer_t;

typedef struct event_output_t {
    output_buffer_t              buffer[EVENT_OUTPUT_MAX_BUFFER];

    int                          read;
    int                          write;
    int                          offset;
} event_output_t;

event_output_t * event_output_new(void);

void event_output_init(event_output_t *output);

int event_output_send(event_t *ev, int sck, void *data, size_t size, free_f free_cb, void *user_data);

void event_output_clean(event_output_t *output);

void event_output_free(event_output_t *output);

#endif /* __EVENT_OUTPUT_H__ */
