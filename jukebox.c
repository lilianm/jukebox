#include <stdlib.h>
#include <poll.h>
#include <sys/time.h>
#include <errno.h>

#include "sck.h"
#include "vector.h"
#include "mtimer.h"
#include "encoder.h"
#include "display.h"
#include "channel.h"

VECTOR_T(pollfd, struct pollfd);

static char basic_reponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: audio/mpeg\r\n"
    "Connection: Close\r\n"
    "\r\n";


int main(int argc, char *argv[])
{
    int               port = 8080;
    int               sck;
    vector_pollfd_t  *poll_sck;
    struct pollfd    *v;
    char              buffer[1024];
    int               ret;
    struct timeval    next_tick;
    struct timeval    now;
    int64_t           diff;

    gettimeofday(&next_tick, NULL);

    argc = argc;
    argv = argv;

    encoder_init("mp3", "encoded", 2);
    channel_init();
    encoder_scan();

    poll_sck   = vector_pollfd_new();

    sck = xlisten(port);
    if(sck == -1) {
        print_error("Can't listen port %i: %m", port);
        return -1;
    }

    vector_pollfd_push(poll_sck, &((struct pollfd) {
                .fd      = sck,
                .events  = POLLIN,
                .revents = 0 }));

    print_log("Jukebox started");

    while(1) {
        gettimeofday(&now, NULL);

        diff = mtimer_manage(&now);

        poll(VECTOR_GET_INDEX(poll_sck, 0), VECTOR_GET_LEN(poll_sck), diff / 1000);
        VECTOR_REVERSE_EACH(poll_sck, v) {
            if(v->revents == 0)
                continue;

            if(v == VECTOR_GET_INDEX(poll_sck, 0)) {
                sck = xaccept(v->fd, NULL);
                print_debug("New connection fd=%i", sck);

                vector_pollfd_push(poll_sck, &((struct pollfd) {
                                .fd      = sck,
                                .events  = POLLIN,
                                .revents = 0 }));
            } else {
                ret = recv(v->fd, buffer, sizeof(buffer), 0);
                if(ret != 0 && ret != -1) {
                    send(v->fd, basic_reponse, sizeof(basic_reponse) - 1, MSG_DONTWAIT);
                    channel_create(v->fd);
                }                    
                vector_pollfd_delete(poll_sck, v);
            }
            
            v->revents = 0;
        }
    }
    return 0;
}
