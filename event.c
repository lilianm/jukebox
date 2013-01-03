#include <poll.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>

#include "event.h"
#include "mtimer.h"
#include "vector.h"
#include "display.h"

typedef enum io_event_kind {
    IO_EVENT_KIND_DELETE = 0,
    IO_EVENT_KIND_LISTEN = 1,
    IO_EVENT_KIND_SOCKET,
} io_event_kind_t;

struct io_event {
    io_event_kind_t  kind;
    void            *data;
    union {
        struct {
            on_accept_f on_accept;
        } server;
        struct {
            on_data_f on_read;
            on_data_f on_write;
            on_data_f on_disconnect;
        } client;
    };
};


VECTOR_T(pollfd, struct pollfd);
VECTOR_T(event, io_event_t *);

vector_pollfd_t  *poll_sck;
vector_event_t   *events;

void * event_get_data(io_event_t *ev)
{
    return ev->data;
}

void event_set_data(io_event_t *ev, void *data)
{
    ev->data = data;
}


void event_init(void)
{
    poll_sck = vector_pollfd_new();
    events   = vector_event_new();
}

io_event_t * event_server_add(uint16_t port, on_accept_f on_accept, void *data)
{
    io_event_t          *event;
    int                  s;
    struct sockaddr_in   addr;
    int                  opt = 1;

    if(on_accept == NULL)
        return NULL;

    addr = (struct sockaddr_in) {
        .sin_family      = AF_INET,
        .sin_port        = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    s    = socket (AF_INET, SOCK_STREAM, 0);

    if(bind(s, (struct sockaddr *) &addr, sizeof (addr)) == -1)
        return NULL;

    if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt,sizeof(opt)) == -1)
        return NULL;

    if(listen(s, 5) == -1)
        return NULL;

    event = (io_event_t *) malloc(sizeof(io_event_t));

    event->kind             = IO_EVENT_KIND_LISTEN;
    event->server.on_accept = on_accept;
    event->data             = data;

    vector_pollfd_push(poll_sck, &((struct pollfd) {
                .fd      = s,
                .events  = POLLIN,
                .revents = 0 }));

    vector_event_push(events, &event);

    return event;
}

io_event_t * event_client_add(int sck, void *data)
{
    io_event_t          *event;

    event = (io_event_t *) malloc(sizeof(io_event_t));

    event->kind                  = IO_EVENT_KIND_SOCKET;
    event->client.on_read        = NULL;
    event->client.on_write       = NULL;
    event->client.on_disconnect  = NULL;
    event->data                  = data;

    vector_pollfd_push(poll_sck, &((struct pollfd) {
                .fd      = sck,
                .events  = 0,
                .revents = 0 }));

    vector_event_push(events, &event);

    return event;
}

static int event_search(io_event_t *ev)
{
    io_event_t **v;

    VECTOR_EACH(events, v) {
        if(*v == ev) {
            return VECTOR_GET_INDEX(events, v);
        }
    }
    return -1;
}

int event_client_set_on_read(io_event_t *ev, on_data_f on_read)
{
    int i;

    assert(ev);
    assert(ev->kind == IO_EVENT_KIND_SOCKET);

    if(on_read  == NULL)
        return -1;

    if(ev->kind != IO_EVENT_KIND_SOCKET)
        return -1;

    i = event_search(ev);

    ev->client.on_read = on_read;

    VECTOR_GET_BY_INDEX(poll_sck, i)->events |= POLLIN;

    return 0;
}

int event_client_clr_on_read(io_event_t *ev)
{
    int i;

    assert(ev);
    assert(ev->kind == IO_EVENT_KIND_SOCKET);

    if(ev->kind != IO_EVENT_KIND_SOCKET)
        return -1;

    i = event_search(ev);

    ev->client.on_read = NULL;

    VECTOR_GET_BY_INDEX(poll_sck, i)->events &= ~POLLIN;

    return 0;
}

int event_client_set_on_write(io_event_t *ev, on_data_f on_write)
{
    int i;

    assert(ev);
    assert(ev->kind == IO_EVENT_KIND_SOCKET);

    if(on_write == NULL)
        return -1;

    if(ev->kind != IO_EVENT_KIND_SOCKET)
        return -1;

    i = event_search(ev);

    ev->client.on_write = on_write;

    VECTOR_GET_BY_INDEX(poll_sck, i)->events |= POLLOUT;

    return 0;
}

int event_client_clr_on_write(io_event_t *ev)
{
    int i;

    assert(ev);
    assert(ev->kind == IO_EVENT_KIND_SOCKET);

    if(ev->kind != IO_EVENT_KIND_SOCKET)
        return -1;

    i = event_search(ev);

    ev->client.on_write = NULL;

    VECTOR_GET_BY_INDEX(poll_sck, i)->events &= ~POLLOUT;

    return 0;
}

int event_client_set_on_disconnect(io_event_t *ev, on_data_f on_disconnect)
{
    assert(ev);
    assert(ev->kind == IO_EVENT_KIND_SOCKET);

    if(on_disconnect == NULL)
        return -1;

    if(ev->kind != IO_EVENT_KIND_SOCKET)
        return -1;

    ev->client.on_disconnect = on_disconnect;

    return 0;
}

int event_client_clr_on_disconnect(io_event_t *ev)
{
    assert(ev);
    assert(ev->kind == IO_EVENT_KIND_SOCKET);

    if(ev->kind != IO_EVENT_KIND_SOCKET)
        return -1;

    ev->client.on_disconnect = NULL;

    return 0;
}

void event_delete(io_event_t *ev)
{
    io_event_t **v;
    int          i;

    if(ev == NULL)
        return;

    if(ev->kind == IO_EVENT_KIND_DELETE)
        return;

    VECTOR_EACH(events, v) {
        if(*v == ev) {
            i = VECTOR_GET_INDEX(events, v);
            ev->kind = IO_EVENT_KIND_DELETE;
            vector_pollfd_delete_by_index(poll_sck, i);
            vector_event_delete_by_index(events, i);
            break;
        }
    }
}

void event_loop(void)
{
    int64_t              diff;
    struct timeval       now;
    struct pollfd       *v;
    struct sockaddr_in   addr;
    int                  addrlen =  sizeof (struct sockaddr_in);
    io_event_t          *e;
    int                  sck;

    while(1) {
        gettimeofday(&now, NULL);

        diff = mtimer_manage(&now);
        if(diff != -1)
            diff /= 1000;

        poll(poll_sck->data, VECTOR_GET_LEN(poll_sck), diff);
        VECTOR_REVERSE_EACH(poll_sck, v) {
            int revents = v->revents;

            if(revents == 0)
                continue;            

            e = *VECTOR_GET_BY_INDEX(events, VECTOR_GET_INDEX(poll_sck, v));
            switch(e->kind) {
            case IO_EVENT_KIND_LISTEN:
                sck = accept(v->fd, (struct sockaddr *) &addr, (unsigned int *)&addrlen);
                if(sck != -1) {
                    print_debug("New connection fd=%i", sck);
                    e->server.on_accept(e, sck, &addr, e->data);
                }
                break;
            case IO_EVENT_KIND_SOCKET:
                if(revents & POLLIN)
                    e->client.on_read(e, sck, e->data);
                if(revents & POLLHUP) {
                    if(e->client.on_disconnect)
                        e->client.on_disconnect(e, sck, e->data);
                    close(sck);
                    event_delete(e);
                }
                if(revents & POLLOUT)
                    e->client.on_write(e, sck, e->data);
                break;
            case IO_EVENT_KIND_DELETE:
                break;
            }
            if(e->kind == IO_EVENT_KIND_DELETE) {
                free(e);
            } else {
                v->revents = 0;
            }
        }
    }

}
