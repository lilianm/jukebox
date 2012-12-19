#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/time.h>
#include <errno.h>

#include "sck.h"
#include "vector.h"
#include "mp3.h"

typedef struct stream {
    int fd;
} stream_t;

VECTOR_T(pollfd, struct pollfd);
VECTOR_T(stream, stream_t);

static char basic_reponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: audio/mpeg\r\n"
    "Connection: Close\r\n"
    "\r\n";

int main(int argc, char *argv[])
{
    int              port = 8080;
    int              sck;
    vector_pollfd_t *poll_sck;
    struct pollfd   *v;
    vector_stream_t *stream;
    stream_t        *s;
    char             buffer[1024];
    int              ret;
    struct timeval   next_tick;
    struct timeval   now;
    int              tick = 200000; // 200ms
    int              diff;
    float            pos = 0.0;
    float            cur;
    mp3_stream_t    *cur_stream;
    mp3_buffer_t     cur_buf;

    gettimeofday(&next_tick, NULL);

    argc = argc;
    argv = argv;

    poll_sck   = vector_pollfd_new();
    stream     = vector_stream_new();
    cur_stream = mp3_stream_open("test.mp3");

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
        if(now.tv_sec > next_tick.tv_sec ||
           (now.tv_sec == next_tick.tv_sec && now.tv_usec + 1000 >= next_tick.tv_usec)) {
            cur  = pos;
            pos += .200;
            /* while(cur < pos) { */
                ret = mp3_stream_read(cur_stream, pos, &cur_buf);
                cur += cur_buf.duration;
                if(cur < pos)
                    printf("not enought %i\n", ret);
                VECTOR_REVERSE_EACH(stream, s) {
                    retry:
                    ret = send(s->fd, cur_buf.buf, cur_buf.size, MSG_DONTWAIT | MSG_NOSIGNAL);
                    if(ret != cur_buf.size) {
                        printf("send diff %i %i\n", ret, cur_buf.size);
                    }
                    if(ret == -1) {
                        if(errno == EINTR)
                            goto retry;
                        
                        if(errno == EAGAIN ||
                           errno == EWOULDBLOCK ||
                           errno == ENOBUFS)
                            continue;

                        vector_stream_delete(stream, s);
                        printf("close %m\n", s->fd, ret);
                        continue;
                    }
                    printf("tick %i %i\n", s->fd, ret);
                }
            /* } */
            next_tick.tv_usec += tick;
            if(next_tick.tv_usec > 1000000) {
                next_tick.tv_usec -= 1000000;
                next_tick.tv_sec  += 1;
            }
        }

        diff = ((next_tick.tv_sec - now.tv_sec) * 1000) +
            ((next_tick.tv_usec - now.tv_usec) / 1000);

        poll(VECTOR_GET_INDEX(poll_sck, 0), VECTOR_GET_LEN(poll_sck), diff);
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
                    vector_stream_push(stream, &((stream_t) { .fd = v->fd }));
                }                    
                vector_pollfd_delete(poll_sck, v);
            }
            
            v->revents = 0;
        }
    }
    return 0;
}
