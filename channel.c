#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "channel.h"
#include "mtimer.h"
#include "mp3.h"
#include "vector.h"
#include "db.h"
#include "time_tool.h"
#include "display.h"

typedef struct channel {
    int              fd; // add fd vector
    struct timeval   start;
    mp3_stream_t    *stream;
} channel_t;

VECTOR_T(channel, channel_t);

static vector_channel_t *channel;

static void channel_update(mtimer_t *t, const struct timeval *now, void *data)
{
    vector_channel_t *channel;
    channel_t        *c;
    int64_t           pos;
    mp3_buffer_t      cur_buf;
    int               ret;

    channel = (vector_channel_t *) data;

    t = t;

    VECTOR_REVERSE_EACH(channel, c) {
        int running = 1;
        while(running) {
            if(c->stream == NULL) {
                // Get random song
                c->stream = db_get_song();
            }
            pos = timeval_diff(now, &c->start);
            ret = mp3_stream_read(c->stream, pos, &cur_buf);
            if(ret == -1) {
                print_error("Error on reading song fd=%i: %m", c->fd);
                mp3_stream_close(c->stream);
                c->stream = NULL;
                continue;
            }

            retry:
            ret = send(c->fd, cur_buf.buf, cur_buf.size, MSG_DONTWAIT | MSG_NOSIGNAL);
            if(ret == -1) {
                if(errno == EINTR)
                    goto retry;
                        
                running = 0;
                if(errno == EAGAIN ||
                   errno == EWOULDBLOCK ||
                   errno == ENOBUFS) {
                    print_debug("Skip %i bytes fd=%i", cur_buf.size, c->fd);
                    continue;
                }

                mp3_stream_close(c->stream);
                print_debug("Close connection fd=%i: %m", c->fd);
                vector_channel_delete(channel, c);
                close(c->fd);
                continue;
            }
            if(c->stream->offset == c->stream->data_size) {
                print_debug("End of Song fd=%i", c->fd);
                timeval_add_usec(&c->start, c->stream->pos);
                mp3_stream_close(c->stream);
                c->stream = NULL;
                continue;
            }
            break;
        }
    }
}

int channel_create(int fd)
{
    struct timeval now;
    channel_t      c;

    gettimeofday(&now, NULL);

    c.fd     = fd;
    c.start  = now;
    c.stream = NULL;

    vector_channel_push(channel, &c);

    return 0;
}

void channel_init(void)
{   
    channel = vector_channel_new();
    mtimer_add(200, MTIMER_KIND_PERIODIC, channel_update, channel);
}
