#include <poll.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>

#include "event.h"
#include "vector.h"
#include "display.h"
#include "time_tool.h"

typedef enum event_kind {
    EVENT_KIND_DELETE = 0,
    EVENT_KIND_LISTEN = 1,
    EVENT_KIND_SOCKET = 2,
    EVENT_KIND_TIMER  = 3,
} event_kind_t;

struct event {
    event_kind_t                 kind;
    void                        *data;
    union {
        struct {
            on_accept_f          on_accept;
        } server;
        struct {
            on_data_f            on_read;
            on_data_f            on_write;
            on_data_f            on_disconnect;
        } client;
        struct {
            event_t             *next;
                                
            on_timer_f           on_timer;
            uint64_t             next_expiration;

            event_timer_kind_t   kind;
            uint32_t             period;
        } timer;
    };
};


VECTOR_T(pollfd, struct pollfd);
VECTOR_T(event, event_t *);

vector_pollfd_t  *poll_sck;
vector_event_t   *events;
static event_t   *timer_list = NULL;


void * event_get_data(event_t *ev)
{
    return ev->data;
}

void event_set_data(event_t *ev, void *data)
{
    ev->data = data;
}


void event_init(void)
{
    poll_sck = vector_pollfd_new();
    events   = vector_event_new();
}

event_t * event_server_add(uint16_t port, on_accept_f on_accept, void *data)
{
    event_t             *event;
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

    event = (event_t *) malloc(sizeof(event_t));

    event->kind             = EVENT_KIND_LISTEN;
    event->server.on_accept = on_accept;
    event->data             = data;

    vector_pollfd_push(poll_sck, &((struct pollfd) {
                .fd      = s,
                .events  = POLLIN,
                .revents = 0 }));

    vector_event_push(events, &event);

    return event;
}

event_t * event_client_add(int sck, void *data)
{
    event_t          *event;

    event = (event_t *) malloc(sizeof(event_t));

    event->kind                  = EVENT_KIND_SOCKET;
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

static int event_search(event_t *ev)
{
    event_t **v;

    VECTOR_EACH(events, v) {
        if(*v == ev) {
            return VECTOR_GET_INDEX(events, v);
        }
    }
    return -1;
}

int event_client_set_on_read(event_t *ev, on_data_f on_read)
{
    int i;

    assert(ev);
    assert(ev->kind == EVENT_KIND_SOCKET);

    if(on_read  == NULL)
        return -1;

    if(ev->kind != EVENT_KIND_SOCKET)
        return -1;

    i = event_search(ev);

    ev->client.on_read = on_read;

    VECTOR_GET_BY_INDEX(poll_sck, i)->events |= POLLIN;

    return 0;
}

int event_client_clr_on_read(event_t *ev)
{
    int i;

    assert(ev);
    assert(ev->kind == EVENT_KIND_SOCKET);

    if(ev->kind != EVENT_KIND_SOCKET)
        return -1;

    i = event_search(ev);

    ev->client.on_read = NULL;

    VECTOR_GET_BY_INDEX(poll_sck, i)->events &= ~POLLIN;

    return 0;
}

int event_client_set_on_write(event_t *ev, on_data_f on_write)
{
    int i;

    assert(ev);
    assert(ev->kind == EVENT_KIND_SOCKET);

    if(on_write == NULL)
        return -1;

    if(ev->kind != EVENT_KIND_SOCKET)
        return -1;

    i = event_search(ev);

    ev->client.on_write = on_write;

    VECTOR_GET_BY_INDEX(poll_sck, i)->events |= POLLOUT;

    return 0;
}

int event_client_clr_on_write(event_t *ev)
{
    int i;

    assert(ev);
    assert(ev->kind == EVENT_KIND_SOCKET);

    if(ev->kind != EVENT_KIND_SOCKET)
        return -1;

    i = event_search(ev);

    ev->client.on_write = NULL;

    VECTOR_GET_BY_INDEX(poll_sck, i)->events &= ~POLLOUT;

    return 0;
}

int event_client_set_on_disconnect(event_t *ev, on_data_f on_disconnect)
{
    assert(ev);
    assert(ev->kind == EVENT_KIND_SOCKET);

    if(on_disconnect == NULL)
        return -1;

    if(ev->kind != EVENT_KIND_SOCKET)
        return -1;

    ev->client.on_disconnect = on_disconnect;

    return 0;
}

int event_client_clr_on_disconnect(event_t *ev)
{
    assert(ev);
    assert(ev->kind == EVENT_KIND_SOCKET);

    if(ev->kind != EVENT_KIND_SOCKET)
        return -1;

    ev->client.on_disconnect = NULL;

    return 0;
}

int event_get_fd(event_t *ev)
{
    int i;

    assert(ev);
    assert(ev->kind == EVENT_KIND_SOCKET);

    if(ev->kind != EVENT_KIND_SOCKET &&
       ev->kind != EVENT_KIND_LISTEN)
        return -1;

    i = event_search(ev);

    return VECTOR_GET_BY_INDEX(poll_sck, i)->fd;
}

void event_delete(event_t *ev)
{
    event_t **v;
    event_t  *c;
    int       i;

    if(ev == NULL)
        return;

    switch(ev->kind) {
    case EVENT_KIND_TIMER:
        v = &timer_list;
        while(*v) {
            c = *v;
            if(c == ev) {
                *v = (*v)->timer.next;
                free(c);
                break;
            }
            v = &(*v)->timer.next;
        }
        break;

    case EVENT_KIND_LISTEN:
    case EVENT_KIND_SOCKET:
        VECTOR_EACH(events, v) {
            if(*v == ev) {
                i = VECTOR_GET_INDEX(events, v);
                vector_pollfd_delete_by_index(poll_sck, i);
                vector_event_delete_by_index(events, i);
                break;
            }
        }
        break;

    case EVENT_KIND_DELETE:
        break;
    }
    ev->kind = EVENT_KIND_DELETE;
}

static inline void event_timer_insert(event_t *e)
{
    event_t **previous;
    event_t  *current;

    previous = &timer_list;
    while(*previous) {
        current = *previous;
        if(e->timer.next_expiration < current->timer.next_expiration) {
            e->timer.next   = current;
            *previous       = e;
            return;
        }
        previous = &(*previous)->timer.next;
    }

    e->timer.next = NULL;
    *previous     = e;
}


event_t * event_timer_add(uint32_t period, event_timer_kind_t kind, on_timer_f on_timer, void *data)
{
    event_t          *e;
    struct timeval    now;

    gettimeofday(&now, NULL);

    e = (event_t *) malloc(sizeof(event_t));

    e->kind                  = EVENT_KIND_TIMER;
    e->timer.kind            = kind;
    e->timer.period          = period;
    e->timer.on_timer        = on_timer;
    e->data                  = data;
    e->timer.next            = NULL;
    e->timer.next_expiration = timeval_to_usec(&now) + period * 1000;

    event_timer_insert(e);

    return e;
}

static int64_t event_timer_manage(struct timeval *now)
{
    uint64_t           cur;
    event_t           *current;

    cur  = timeval_to_usec(now);

    while(timer_list && timer_list->timer.next_expiration < cur + 1000) {
        current    = timer_list;
        timer_list = timer_list->timer.next;

        current->timer.on_timer(current, now, current->data);
        if(current->timer.kind == EVENT_TIMER_KIND_PERIODIC) {
            current->timer.next_expiration += current->timer.period * 1000;
            event_timer_insert(current);
        } else {
            free(current);
        }
    }

    if(timer_list == NULL)
        return -1;
    return timer_list->timer.next_expiration - cur;
}

void event_loop(void)
{
    int64_t              diff;
    struct timeval       now;
    struct pollfd       *v;
    struct sockaddr_in   addr;
    int                  addrlen =  sizeof (struct sockaddr_in);
    event_t             *e;
    int                  sck;

    while(1) {
        gettimeofday(&now, NULL);

        diff = event_timer_manage(&now);
        if(diff != -1)
            diff /= 1000;

        poll(poll_sck->data, VECTOR_GET_LEN(poll_sck), diff);
        VECTOR_REVERSE_EACH(poll_sck, v) {
            int revents = v->revents;

            if(revents == 0)
                continue;            

            v->revents = 0;

            e = *VECTOR_GET_BY_INDEX(events, VECTOR_GET_INDEX(poll_sck, v));
            switch(e->kind) {
            case EVENT_KIND_LISTEN:
                sck = accept(v->fd, (struct sockaddr *) &addr, (unsigned int *)&addrlen);
                if(sck != -1) {
                    print_debug("New connection fd=%i", sck);
                    e->server.on_accept(e, sck, &addr, e->data);
                }
                break;
            case EVENT_KIND_SOCKET:
                sck = v->fd;
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
            case EVENT_KIND_DELETE:
            case EVENT_KIND_TIMER:
                break;
            }
            if(e->kind == EVENT_KIND_DELETE) {
                free(e);
            }
        }
    }

}
