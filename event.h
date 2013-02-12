#ifndef __EVENT_H__
#define __EVENT_H__

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/time.h>

typedef struct event event_t;

/* Init and event loop */

void event_loop(void);

void event_init(void);

/* Server function */

typedef void (*on_accept_f)(event_t *ev, int sck,
                            struct sockaddr_in *addr, void *data);

event_t * event_server_add(uint16_t port, on_accept_f on_accept, void *data);

/* Client function */

typedef void (*on_data_f)(event_t *ev, int sck, void *data);

event_t * event_client_add(int sck, void *data);

int event_client_set_on_read(event_t *ev, on_data_f on_read);

int event_client_clr_on_read(event_t *ev);

int event_client_set_on_write(event_t *ev, on_data_f on_write);

int event_client_clr_on_write(event_t *ev);

int event_client_set_on_disconnect(event_t *ev, on_data_f on_disconnect);

int event_client_clr_on_disconnect(event_t *ev);

/* Timer */

typedef enum event_timer_kind {
    EVENT_TIMER_KIND_PERIODIC,
    EVENT_TIMER_KIND_ONESHOT,
} event_timer_kind_t;

typedef void (*on_timer_f)(event_t *, const struct timeval *now, void *data);

event_t * event_timer_add(uint32_t period, event_timer_kind_t kind,
                          on_timer_f on_timer, void *data);

/* Clean up function */

void event_delete(event_t *ev);

/* Manipulate event */

void event_set_data(event_t *ev, void *data);

void * event_get_data(event_t *ev);

int event_get_fd(event_t *ev);

#endif /* __EVENT_H__ */
