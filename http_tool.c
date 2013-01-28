#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "http_tool.h"
#include "mstring.h"
#include "stream.h"

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

static void munmap_wrapper(void *data, void *user_data)
{
    int *size = (int *) user_data;

    munmap(data, *size);
    free(user_data);
}

static int on_mapping(http_request_t *hr, void *data, const char *remaining, size_t size)
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

int http_map_directory(http_server_t *server, char *server_path, char *file_path)
{
    return http_node_new(server, server_path, on_mapping, file_path);
}
