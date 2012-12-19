#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/time.h>
#include <errno.h>

#include "sck.h"
#include "vector.h"
#include "mp3.h"

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

static inline int64_t diff_timeval(struct timeval *new, struct timeval *old)
{
    return ((int64_t)new->tv_sec - old->tv_sec) * 1000000 +
        (new->tv_usec - old->tv_usec);
}

int main(int argc, char *argv[])
{
    int               port = 8080;
    int               sck;
    vector_pollfd_t  *poll_sck;
    struct pollfd    *v;
    vector_channel_t *channel;
    channel_t        *c;
    char              buffer[1024];
    int               ret;
    struct timeval    next_tick;
    struct timeval    now;
    int               tick = 200000; // 200ms
    int64_t           diff;
    int64_t           pos;
    mp3_buffer_t      cur_buf;

    gettimeofday(&next_tick, NULL);

    argc = argc;
    argv = argv;

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

    while(1) {
        gettimeofday(&now, NULL);
        diff = diff_timeval(&next_tick, &now);
        if(diff < 0)
            diff = 0;
        if(diff < 1000) {
            VECTOR_REVERSE_EACH(channel, c) {
                int running = 1;
                while(running) {
                    if(c->stream == NULL) {
                        // Get random song
                        c->stream = mp3_stream_open("test.mp3");
                    }
                    pos = diff_timeval(&now, &c->start);
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
                        c->start.tv_sec  += c->stream->pos / 1000000;
                        c->start.tv_usec += c->stream->pos % 1000000;
                        if(c->start.tv_usec > 1000000) {
                            c->start.tv_usec -= 1000000;
                            c->start.tv_sec  += 1;
                        }
                        mp3_stream_close(c->stream);
                        c->stream = NULL;
                        continue;
                    }
                    break;
                }
//                printf("tick %i %i\n", s->fd, ret);
            }
            next_tick.tv_usec += tick;
            if(next_tick.tv_usec > 1000000) {
                next_tick.tv_usec -= 1000000;
                next_tick.tv_sec  += 1;
            }
            diff = diff_timeval(&next_tick, &now);
        }

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
