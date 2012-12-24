#ifndef __EVENT_H__
#define __EVENT_H__

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef struct io_event io_event_t;

typedef void (*on_accept_f)(io_event_t *ev, int sck,
                            struct sockaddr_in *addr, void *data);

typedef void (*on_data_f)(io_event_t *ev, int sck, void *data);

io_event_t * event_add_server(uint16_t port, on_accept_f on_accept, void *data);

io_event_t * event_add_client(int sck, on_data_f on_data, void *data);

void event_delete(io_event_t *ev);

void event_loop(void);

void event_init(void);

#endif /* __EVENT_H__ */
