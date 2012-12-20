#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/time.h>
#include <errno.h>

#include "sck.h"
#include "vector.h"
#include "mp3.h"
#include "mtimer.h"
#include "time_tool.h"
#include "encoder.h"

typedef struct channel {
    int              fd; // add fd vector
    struct timeval   start;
    mp3_stream_t    *stream;
} channel_t;

VECTOR_T(pollfd, struct pollfd);
VECTOR_T(channel, channel_t);

static char basic_reponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: audio/mpeg\r\n"
    "Connection: Close\r\n"
    "\r\n";


void update_channel(mtimer_t *t, const struct timeval *now, void *data)
{
    vector_channel_t *channel;
    channel_t        *c;
    int64_t           pos;
    mp3_buffer_t      cur_buf;
    int               ret;

    t = t;
    channel = (vector_channel_t *) data;

    VECTOR_REVERSE_EACH(channel, c) {
        int running = 1;
        while(running) {
            if(c->stream == NULL) {
                // Get random song
                c->stream = mp3_stream_open("test.mp3");
            }
            pos = timeval_diff(now, &c->start);
            ret = mp3_stream_read(c->stream, pos, &cur_buf);
            printf("read %li %i %li\n", pos, ret, c->stream->pos);
//                cur += cur_buf.duration;
            retry:
            ret = send(c->fd, cur_buf.buf, cur_buf.size, MSG_DONTWAIT | MSG_NOSIGNAL);
            if(ret != (signed)cur_buf.size) {
                printf("send diff %i %zi\n", ret, cur_buf.size);
            }
            if(ret == -1) {
                if(errno == EINTR)
                    goto retry;
                        
                running = 0;
                if(errno == EAGAIN ||
                   errno == EWOULDBLOCK ||
                   errno == ENOBUFS) {
                    continue;
                }

                mp3_stream_close(c->stream);
                vector_channel_delete(channel, c);
                printf("close %m %i\n", c->fd);
                continue;
            }
            if((signed)c->stream->pos < pos) {
                printf("end\n");
                timeval_add_usec(&c->start, c->stream->pos);
                mp3_stream_close(c->stream);
                c->stream = NULL;
                continue;
            }
            break;
        }
//                printf("tick %i %i\n", s->fd, ret);
    }
}

int main(int argc, char *argv[])
{
    int               port = 8080;
    int               sck;
    vector_pollfd_t  *poll_sck;
    struct pollfd    *v;
    vector_channel_t *channel;
    char              buffer[1024];
    int               ret;
    struct timeval    next_tick;
    struct timeval    now;
    int64_t           diff;

    gettimeofday(&next_tick, NULL);

    argc = argc;
    argv = argv;

    encoder_init("mp3", "encoded", 2);

    encoder_scan();

    poll_sck   = vector_pollfd_new();
    channel    = vector_channel_new();

    sck = xlisten(port);
    if(sck == -1) {
        printf("error %m\n");
        return -1;
    }

    vector_pollfd_push(poll_sck, &((struct pollfd) {
                .fd      = sck,
                .events  = POLLIN,
                .revents = 0 }));

    mtimer_add(200,   MTIMER_KIND_PERIODIC, update_channel, channel);
    mtimer_add(30000, MTIMER_KIND_PERIODIC, (mtimer_cb_f)encoder_scan, NULL);

    while(1) {
        gettimeofday(&now, NULL);

        diff = mtimer_manage(&now);

        poll(VECTOR_GET_INDEX(poll_sck, 0), VECTOR_GET_LEN(poll_sck), diff / 1000);
        VECTOR_REVERSE_EACH(poll_sck, v) {
            if(v->revents == 0)
                continue;

            if(v == VECTOR_GET_INDEX(poll_sck, 0)) {
                sck = xaccept(v->fd, NULL);
                printf("accept\n");
                vector_pollfd_push(poll_sck, &((struct pollfd) {
                                .fd      = sck,
                                .events  = POLLIN,
                                .revents = 0 }));
            } else {
                ret = recv(v->fd, buffer, sizeof(buffer), 0);
                if(ret != 0 && ret != -1) {
                    send(v->fd, basic_reponse, sizeof(basic_reponse) - 1, MSG_DONTWAIT);
                    vector_channel_push(channel, &((channel_t) {
                                    .fd     = v->fd,
                                    .start  = now,
                                    .stream = NULL}));
                }                    
                vector_pollfd_delete(poll_sck, v);
            }
            
            v->revents = 0;
        }
    }
    return 0;
}
