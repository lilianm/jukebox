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

static int on_stream(http_request_t *hr, void *data, const char *remaining, size_t size)
{
    int sck;

    (void) remaining;
    (void) size;
    (void) data;

    sck = http_request_detach(hr);

    send(sck, basic_reponse, sizeof(basic_reponse) - 1, MSG_DONTWAIT);
    channel_create(sck);
 
    return 0;
}

int main(int argc, char *argv[])
{
    int            port           = 8080;
    http_server_t *server;

    (void) argc;
    (void) argv;

    encoder_init("mp3", "encoded", 2);
    channel_init();
    encoder_scan();
    event_init();

    server = http_server_new(port);
    http_node_new(server, "/stream", on_stream, NULL);

    print_log("Jukebox started");

    event_loop();

    return 0;
}
