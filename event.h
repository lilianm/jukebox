#ifndef __EVENT_H__
#define __EVENT_H__

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>

typedef struct io_event io_event_t;

/* Init and event loop */

void event_loop(void);

void event_init(void);

/* Server function */

typedef void (*on_accept_f)(io_event_t *ev, int sck,
                            struct sockaddr_in *addr, void *data);

io_event_t * event_server_add(uint16_t port, on_accept_f on_accept, void *data);

/* Client function */

typedef void (*on_data_f)(io_event_t *ev, int sck, void *data);

io_event_t * event_client_add(int sck, void *data);

int event_client_set_on_read(io_event_t *ev, on_data_f on_read);

int event_client_clr_on_read(io_event_t *ev);

int event_client_set_on_write(io_event_t *ev, on_data_f on_write);

int event_client_clr_on_write(io_event_t *ev);

int event_client_set_on_disconnect(io_event_t *ev, on_data_f on_disconnect);

int event_client_clr_on_disconnect(io_event_t *ev);

/* Clean up function */

void event_delete(io_event_t *ev);

/* Manipulate event */

void event_set_data(io_event_t *ev, void *data);

void * event_get_data(io_event_t *ev);

int event_get_fd(io_event_t *ev);

#endif /* __EVENT_H__ */
