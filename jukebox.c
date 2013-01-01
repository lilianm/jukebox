#include <stdlib.h>
#include <poll.h>
#include <sys/time.h>
#include <errno.h>

#include "encoder.h"
#include "display.h"
#include "channel.h"
#include "event.h"
#include "http.h"


static char basic_reponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: audio/mpeg\r\n"
    "Connection: Close\r\n"
    "\r\n";

static void on_http_data(io_event_t *ev, int sck, void *data)
{
    char buffer[1024];
    int  ret;

    (void) data;

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
    io_event_t *client;

    (void) ev;
    (void) addr;
    (void) data;

    client = event_client_add(sck, NULL);
    event_client_set_on_read(client, on_http_data);
}

int main(int argc, char *argv[])
{
    int port = 8080;

    (void) argc;
    (void) argv;

    encoder_init("mp3", "encoded", 2);
    channel_init();
    encoder_scan();
    event_init();

    if(event_server_add(port, on_http_server, NULL) == NULL) {
        print_error("Can't listen port %i: %m", port);
        return -1;
    }

    http_server_new(8084, NULL);

    print_log("Jukebox started");

    event_loop();

    return 0;
}
