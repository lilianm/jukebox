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
#include "hash.h"
#include "song_queue.h"

#define MAX_CHANNEL_QUEUE_SIZE 128

struct channel {
    struct channel  *next;
    char            *name;
    user_t          *user;
    struct timeval   start;
    mp3_stream_t    *stream;

    song_queue_t    *queue;
};

static channel_t        *channel_first = NULL;
static hash_t           *channel_list  = NULL;

static void channel_update(mtimer_t *t, const struct timeval *now, void *data)
{
    channel_t        *c;
    int64_t           pos;
    mp3_buffer_t      cur_buf;
    int               ret;

    (void) t;
    (void) data;

    c = channel_first;

    while(c) {
        int  nb;
        int *fd;

        nb = user_get_nb_socket(c->user);
        fd = user_get_socket(c->user);

        if(nb) {
            if(c->stream == NULL) {
                int mid;
                mid = song_queue_next(c->queue);
                if(mid == -1) {
                    // Get random song
                    c->stream = db_get_song(&mid);
                    song_queue_add(c->queue, mid);
                    song_queue_next(c->queue);
                } else {
                    c->stream = db_get_song(&mid);
                }
            }

            pos = timeval_diff(now, &c->start);
            ret = mp3_stream_read(c->stream, pos, &cur_buf);
            if(ret == -1) {
                print_error("Error on reading song channel %s: %m", c->name);
                mp3_stream_close(c->stream);
                c->stream = NULL;
                continue;
            }
        }

        for(fd = fd + nb -1; nb--; fd--) {
            retry:
            ret = send(*fd, cur_buf.buf, cur_buf.size, MSG_DONTWAIT | MSG_NOSIGNAL);
            if(ret == -1) {
                if(errno == EINTR)
                    goto retry;
                        
                if(errno == EAGAIN ||
                   errno == EWOULDBLOCK ||
                   errno == ENOBUFS) {
                    print_debug("Skip %i bytes channel=%s fd=%i", cur_buf.size, c->name, *fd);
                    continue;
                }

                print_debug("Close connection channel=%s fd=%i: %m", c->name, *fd);
                close(user_remove_socket(c->user, *fd));
                continue;
            }
        }
        if(c->stream->offset == c->stream->data_size) {
            print_debug("End of Song channel=%s", c->name);
            timeval_add_usec(&c->start, c->stream->pos);
            mp3_stream_close(c->stream);
            c->stream = NULL;
            continue;
        }
        c = c->next;
    }
}

static channel_t * channel_new(char *user)
{
    channel_t      *c;
    struct timeval  now;

    gettimeofday(&now, NULL);

    c = (channel_t *) malloc(sizeof(channel_t));

    c->stream     = NULL;
    c->name       = strdup(user);
    c->start      = now;
    c->next       = channel_first;
    c->user       = NULL;
    c->queue      = song_queue_new(MAX_CHANNEL_QUEUE_SIZE);

    channel_first = c;

    hash_add(channel_list, c->name, c);


    return c;
}

channel_t * channel_add_user(char *channel, user_t *u)
{
    channel_t      *c;

    c = hash_get(channel_list, channel);
    if(c) {
        if(c->user != NULL && c->user != u)
            return NULL; // must manage this case

        c->user = u;

        return c;
    }
    c = channel_new(channel);
    c->user = u;

    return c;
}

void channel_init(void)
{   
    channel_list = hash_new(hash_str_cmp, hash_str_hash, 8);
    mtimer_add(200, MTIMER_KIND_PERIODIC, channel_update, NULL);
}

int channel_next(channel_t *c)
{
    mp3_stream_close(c->stream);
    c->stream = NULL;

    return 0;
}

int channel_previous(channel_t *c)
{
    int mid;

    mid = song_queue_previous(c->queue);
    if(mid == -1)
        return -1;

    mp3_stream_close(c->stream);
    c->stream = db_get_song(&mid);
    song_queue_add(c->queue, mid);

    return 0;
}
