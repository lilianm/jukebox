#include <stdlib.h>
#include <poll.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "encoder.h"
#include "display.h"
#include "channel.h"
#include "event.h"
#include "http.h"
#include "user.h"
#include "http_tool.h"

static char basic_reponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: audio/mpeg\r\n"
    "Connection: Close\r\n"
    "\r\n";

static int on_stream(http_request_t *hr, void *data, const char *remaining, size_t size)
{
    int     sck;
    user_t *user;

    (void) remaining;
    (void) size;
    (void) data;

    user = http_request_get_data(hr);
    if(user == NULL)
        return -1;

    sck  = http_request_detach(hr);

    send(sck, basic_reponse, sizeof(basic_reponse) - 1, MSG_DONTWAIT);
    if(user_add_socket(user, sck) == -1) {
        close(sck);
    }
    channel_add_user(user_get_name(user), user);
 
    return 0;
}

int auth_session(http_request_t *hr, char *login, char *password)
{
    user_t *user;

    if(login && password &&
       strcmp(login, password) == 0) {
        user = user_get(login);
        http_request_set_data(hr, user, NULL);
        return 0;
    }
    return -1;
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
    http_map_directory(server, "/", "html");
    http_server_set_auth_cb(server, auth_session);

    print_log("Jukebox started");

    event_loop();

    return 0;
}
