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
#include "stream.h"
#include "user.h"

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

static void munmap_wrapper(void *data, void *user_data)
{
    int *size = (int *) user_data;

    munmap(data, *size);
    free(user_data);
}

char * http_get_mime_type_by_extension(char *s, size_t len)
{
    typedef struct content_type_associate {
        string_t   ext;
        char      *v;
    } content_type_associate_t;

    content_type_associate_t content_type_list[] = {
        { STRING_INIT_CSTR("css") , "text/css" },
        { STRING_INIT_CSTR("html"), "text/html"},
        { STRING_INIT_CSTR("htm") , "text/html"},
        { STRING_INIT_CSTR("js")  , "text/javascript"},
        { STRING_INIT_CSTR("png") , "image/png"},
        { STRING_INIT_CSTR("jpg") , "image/jpeg"},
        { STRING_INIT_CSTR("gif") , "image/gif"},
        { STRING_INIT_CSTR("txt") , "text/plain"},
        { STRING_INIT_NULL        , "application/octet-stream"},
    };

    content_type_associate_t *cur;
    string_t                  ext;

    if(len == 0 || s == NULL)
        return "application/octet-stream";

    ext = string_init_full(s, len, len, STRING_ALLOC_STATIC);

    for(cur = content_type_list; cur->ext.txt != NULL; cur++)
        if(string_cmp(ext, cur->ext) == 0)
            break;

    return cur->v;
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
    string_t     ext;
    stream_t     s;

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
        path = string_add_chr(path, 0);

        fd = open(path.txt,  O_RDONLY);
        if(fd == -1)
            return -1;
        fstat(fd, &stat_buf);
    }

    file_buffer = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    ext = STRING_INIT_NULL;
    s   = stream_init(path.txt, path.len - 1);
    if(stream_find_rchr(&s, '.', NULL) == 0) {
        ext = string_init_full(s.data, stream_len(&s), stream_len(&s), STRING_ALLOC_STATIC);
        if(stream_find_rchr(&s, '/', NULL) == 0) {
            ext = STRING_INIT_NULL;
        }
    }
    mmap_size = (int *) malloc(sizeof(int));

    *mmap_size = stat_buf.st_size;
    http_send_reponse(hr, http_get_mime_type_by_extension(ext.txt, ext.len),
                      file_buffer, stat_buf.st_size, munmap_wrapper, mmap_size);

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
    http_node_new(server, "/", on_root, "html");
    http_server_set_auth_cb(server, auth_session);

    print_log("Jukebox started");

    event_loop();

    return 0;
}
