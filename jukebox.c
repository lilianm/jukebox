#include <stdlib.h>
#include <poll.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "encoder.h"
#include "display.h"
#include "channel.h"
#include "event.h"
#include "http.h"
#include "mstring.h"

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

static void munmap_wrapper(void *data, void *user_data)
{
    int *size = (int *) user_data;

    munmap(data, *size);
    free(user_data);
}

static int on_root(http_request_t *hr, void *data, const char *remaining, size_t size)
{
    char         buffer[2048];

    string_t     path;
    char        *base_path;
    int          fd;
    struct stat  stat_buf;
    void        *file_buffer;
    int         *mmap_size;

    (void) hr;

    base_path = (char *) data;

    if(memmem(remaining, size, "/../", 4))
        return -1;

    path = string_init_full(buffer, 0, sizeof(buffer), STRING_ALLOC_STATIC);

    path = string_concat(path, string_init_static(base_path));
    path = string_add_chr(path, '/');
    path = string_concat(path, string_init_full((char *)remaining, size, size, STRING_ALLOC_STATIC));
    path = string_add_chr(path, 0);

    fd = open(path.txt,  O_RDONLY);
    if(fd == -1)
        return -1;

    fstat(fd, &stat_buf);

    if(S_ISDIR(stat_buf.st_mode)) {
        close(fd);
        path.len--; // remove \0 char
        path = string_add_chr(path, '/');
        path = string_concat(path, STRING_INIT_CSTR("index.html"));

        fd = open(path.txt,  O_RDONLY);
        if(fd == -1)
            return -1;
        fstat(fd, &stat_buf);
    }

    file_buffer = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    mmap_size = (int *) malloc(sizeof(int));

    *mmap_size = stat_buf.st_size;
    http_send_reponse(hr, file_buffer, stat_buf.st_size, munmap_wrapper, mmap_size);

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
