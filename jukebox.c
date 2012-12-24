#include <stdlib.h>
#include <poll.h>
#include <sys/time.h>
#include <errno.h>

#include "sck.h"
#include "encoder.h"
#include "display.h"
#include "channel.h"
#include "event.h"


static char basic_reponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: audio/mpeg\r\n"
    "Connection: Close\r\n"
    "\r\n";

static void on_http_data(io_event_t *ev, int sck, void *data)
{
    char buffer[1024];
    int  ret;

    data = data;

    ret = recv(sck, buffer, sizeof(buffer), 0);
    if(ret != 0 && ret != -1) {
        send(sck, basic_reponse, sizeof(basic_reponse) - 1, MSG_DONTWAIT);
        channel_create(sck);
    }
    event_delete(ev);
}

static void on_http_server(io_event_t *ev, int sck,
                           struct sockaddr_in *addr, void *data)
{
    ev   = ev;
    addr = addr;
    data = data;

    event_add_client(sck, on_http_data, NULL);
}

int main(int argc, char *argv[])
{
    int port = 8080;

    argc = argc;
    argv = argv;

    encoder_init("mp3", "encoded", 2);
    channel_init();
    encoder_scan();
    event_init();

    if(event_add_server(port, on_http_server, NULL) == NULL) {
        print_error("Can't listen port %i: %m", port);
        return -1;
    }

    print_log("Jukebox started");

    event_loop();

    return 0;
}
