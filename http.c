#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <glib.h>

#include "http.h"
#include "stream.h"
#include "event.h"
#include "vector.h"
#include "display.h"
#include "event_output.h"

#define CRLF "\r\n"

typedef struct http_node {
    char                *name;
    GHashTable          *child;
    http_node_callback   callback;
    void                *data;
} http_node_t;

typedef struct http_option_t {
    stream_t    name;
    stream_t    value;
} http_option_t;

VECTOR_T(option, http_option_t);
VECTOR_T(line, stream_t);

struct http_request {
    event_output_t      output;

    io_event_t         *event;
    http_node_t        *root;

    stream_t            method;
    stream_t            uri;
    vector_option_t     options;
    char                header[64*1024];
    int                 header_length;
    int                 content_length;
    void*               data;
};

struct http_server
{
    io_event_t         *event;
    http_node_t        *root;
};

int http_request_detach(http_request_t *hr)
{
    int fd;

    fd = event_get_fd(hr->event);
    event_delete(hr->event);
    event_output_clean(&hr->output);
    free(hr);

    return fd;
}

void http_request_init(http_request_t *hr)
{
    event_output_init(&hr->output);
    vector_option_init(&hr->options);
    hr->header_length = 0;
}

http_request_t * http_request_new(void)
{
    http_request_t *hr;

    hr = (http_request_t *) malloc(sizeof(http_request_t));

    http_request_init(hr);

    return hr;
}

void http_request_line_decode(http_request_t *hr, stream_t *line)
{
    assert(hr);
    assert(line);

    stream_find_chr(line, ' ', &hr->method);
    stream_skip(line, 1);
    stream_find_chr(line, ' ', &hr->uri);
    stream_skip(line, 1);
    /* TODO: Check HTTP version */
}

int http_option_search(http_request_t *hr, char *name, stream_t *out)
{
    unsigned int        i;
    http_option_t*      opt;

    for(i = 0; i < hr->options.len; ++i) {
        opt = &hr->options.data[hr->options.offset + i];
        if(strncasecmp(name, opt->name.data, stream_len(&opt->name)) == 0) {
            *out = opt->name;
            return 0;
        }
    }
    return -1;
}

void http_options_decode(http_request_t *hr, stream_t *opts, int n)
{
    int                 i;
    http_option_t       opt;

    assert(hr);
    assert(opts);

    for(i = 0; i < n; ++i) {
        opt.value = opts[i];
        stream_find_chr(&opt.value, ':', &opt.name);
        stream_skip(&opt.value, 1);
        stream_strip(&opt.value);
        vector_option_push(&hr->options, &opt);
    }
}

static gboolean http_str_equal(gconstpointer v1, gconstpointer v2)
{
    return strcmp(v1, v2) == 0;
}

static guint    http_str_hash(gconstpointer v)
{
    guint        hash = 5381;
    const char  *text = v;

    for(; *text; text++) {
        hash *= 33;
        hash += *text;
    }

    return hash;
}

static struct http_node * http_node_search(struct http_node *n, const char *path, const char **remaining)
{
    char                *name;
    size_t               name_size;
    struct http_node    *next;
    const char          *save_path;

    assert(n);
    assert(path);
    assert(remaining);

    save_path = path;

    while(*path && *path == '/')
        path++;

    if(*path == 0) {
        *remaining = path;
        return n;
    }

    *remaining = strchrnul(path, '/');

    if(*remaining == 0) {
        next = g_hash_table_lookup(n->child, path);
        return next;
    }

    name_size = *remaining - path;
    name      = alloca(name_size + 1);
    memcpy(name, path, name_size);
    name[name_size] = 0;

    next = g_hash_table_lookup(n->child, name);
    if(next)
        return http_node_search(next, *remaining, remaining);
    *remaining = save_path;
    return n;
}

static struct http_node * http_node_search_callback(struct http_node *n, const char *path, size_t len, const char **remaining, size_t *remaining_len)
{
    char                *name;
    size_t               name_size;
    struct http_node    *next;
    const char          *save_path;
    size_t               save_len;
    const char          *end;

    assert(n);
    assert(remaining);

    end       = path + len;
    save_path = path;
    save_len  = len;

    while(*path && *path == '/') {
        path++;
        len--;
    }

    if(len == 0) {
        *remaining     = path;
        *remaining_len = len;
        if(n->callback)
            return n;
        return NULL;
    }

    *remaining = memchr(path, '/', len);

    if(*remaining == NULL) {
        *remaining = end;
    }

    *remaining_len = end - *remaining;
    name_size      = *remaining - path;
    name           = alloca(name_size + 1);
    memcpy(name, path, name_size);
    name[name_size] = 0;

    next = g_hash_table_lookup(n->child, name);
    if(next)
        next = http_node_search_callback(next, *remaining, *remaining_len,
                                         remaining, remaining_len);
    if(next)
        return next;

    *remaining     = save_path;
    *remaining_len = save_len;
    if(n->callback)
        return n;
    return NULL;
}

static struct http_node * http_node_malloc(const char *name, size_t len,
                                           http_node_callback cb, void *data)
{
    struct http_node *n;

    n = (struct http_node *) malloc(sizeof(struct http_node));
    n->child           = g_hash_table_new(http_str_hash, http_str_equal);
    n->name            = NULL;
    if(name) {
        n->name            = malloc(len + 1);
        memcpy(n->name, name, len);
        n->name[len]       = 0;
    }
    n->data            = data;
    n->callback        = cb;

    return n;
}


static struct http_node * http_node_create_path(struct http_node *n, const char *path)
{
    char                *remaining;
    size_t               name_size;
    struct http_node    *c;

    assert(n);
    assert(path);

    while(*path) {
        while(*path && *path == '/')
            path++;
        if(*path == 0)
            return n;

        remaining = strchrnul(path, '/');
        name_size = remaining - path;
        
        c = http_node_malloc(path, name_size, NULL, NULL);

        g_hash_table_insert(n->child, c->name, c);

        n    = c;
        path = remaining;
    }

    return n;
}

http_node_t * __http_node_new(http_node_t *root, char *path, http_node_callback cb, void *data)
{
    struct http_node    *n;
    const char          *remaining;

    assert(!((root == NULL) ^ (path == NULL)));

    if(root == NULL)
        return http_node_malloc(NULL, 0, cb, data);

    n = http_node_search(root, path, &remaining);
    if(*remaining)
        n = http_node_create_path(n, remaining);

    n->callback = cb;
    n->data     = data;

    return n;
}

int http_node_new(http_server_t *server, char *path, http_node_callback cb, void *data)
{
    if(__http_node_new(server->root, path, cb, data) == NULL)
        return -1;
    return 0;
}

void http_server_init(http_server_t *hs, io_event_t *ev, http_node_t *root)
{
    hs->event = ev;
    hs->root  = root;
}

static char not_found_response[] =
    "HTTP/1.1 404 Not found" CRLF
    "Content-Type: text/html" CRLF
    "Content-Length: 85" CRLF
    "Connection: Keep-Alive" CRLF
    CRLF CRLF
    "<html><head><title>404 Not found</title></head><body><H1>Not found</H1></body></head>";

static char internal_error_response[] =
    "HTTP/1.1 500 Internal error" CRLF
    "Content-Type: text/html" CRLF
    "Content-Length: 95" CRLF
    "Connection: Keep-Alive" CRLF
    CRLF CRLF
    "<html><head><title>500 Internal error</title></head><body><H1>Internal error</H1></body></head>";

void http_send_404(http_request_t *hr)
{
    int sck;

    sck = event_get_fd(hr->event);
    event_output_send(hr->event, sck, 
                      not_found_response, sizeof(not_found_response) - 1, NULL, NULL);
}

void http_send_500(http_request_t *hr)
{
    int sck;

    sck = event_get_fd(hr->event);
    event_output_send(hr->event, sck,
                      internal_error_response, sizeof(internal_error_response) - 1, NULL, NULL);
}

static int http_header_decode(http_request_t *hr, int sck)
{
    vector_line_t       opt;
    stream_t            line;
    stream_t            content_length;
    stream_t            data;
    stream_t            header;

    assert(hr);

    (void) sck;

    data = stream_init(hr->header, hr->header_length);

    if(stream_find_mem(&data, "\r\n\r\n", 4, &header) == -1)
        return -1;
    stream_expand(&header, 2);

    vector_line_init(&opt);
    while(stream_find_mem(&header, "\r\n", 2, &line) != -1)
    {
        vector_line_push(&opt, &line);
    }

    http_request_line_decode(hr, &opt.data[0]);
    http_options_decode(hr, &opt.data[1], opt.len - 1);
    vector_line_clean(&opt);

    if(http_option_search(hr, "Content-Length", &content_length) == -1) {
        hr->content_length = 0;
    } else {
        hr->content_length = atoi(content_length.data);
    }

    const char            *remaining;
    http_node_t           *root           = hr->root;
    struct http_node      *current;
    size_t                 remaining_size;
    current = http_node_search_callback(root, hr->uri.data, stream_len(&hr->uri),
                                        &remaining, &remaining_size);
    if(current) {
        if(current->callback(hr, current->data, remaining, remaining_size) < 0)
            http_send_500(hr);
    } else {
        http_send_404(hr);
    }

    hr->header_length = stream_len(&data);
    memmove(hr->header, data.data, hr->header_length);

    return 0;
}

static void on_http_client_data(io_event_t *ev, int sck, void *data)
{
    int                 ret;
    http_request_t     *hr;

    hr = (http_request_t *)data;

    retry:
    ret = recv(sck, hr->header + hr->header_length,
               sizeof(hr->header) - hr->header_length, MSG_NOSIGNAL);
    if(ret == -1) {
        if(errno == EINTR)
            goto retry;

        print_debug("HTTP Connection close: %m");
        event_delete(ev);
        close(sck);
        return;
    }
    
    hr->header_length += ret;
    if(http_header_decode(hr, sck) != 0) {
        if(hr->header_length == sizeof(hr->header)) {
            print_error("HTTP Header size too big");
            event_delete(ev);
            free(data);
            close(sck);
        }
        return;
    }
}

void http_send_reponse(http_request_t *hr, void *buffer, size_t size, free_f free_cb, void *user_data)
{
    int     sck;
    char   *header;
    size_t  header_size;
    char    basic_response[] = {
        "HTTP/1.1 200 OK" CRLF
        "Content-Length: %i" CRLF
        "Connection: keep-alive" CRLF
        CRLF
    };

    sck = event_get_fd(hr->event);

    header = (char *) malloc(256);
    header_size = snprintf(header, 256, basic_response, size);

    event_output_send(hr->event, sck, header, header_size, (free_f) free, NULL);
    event_output_send(hr->event, sck, buffer, size, free_cb, user_data);
}

static void on_http_new_connection(io_event_t *ev, int sck,
                                   struct sockaddr_in *addr, void *data)
{
    http_request_t *hr;
    http_server_t  *hs;

    (void) ev;
    (void) addr;
    hs   = (http_server_t *) data;

    hr        = http_request_new();
    hr->root  = hs->root;
    hr->event = event_client_add(sck, hr);

    event_client_set_on_read(hr->event, on_http_client_data);
}

http_server_t * http_server_new(uint16_t port)
{
    http_server_t *hs;
    io_event_t    *ev;
    http_node_t   *root;

    hs   = (http_server_t*) malloc(sizeof(http_server_t));
    ev   = event_server_add(port, on_http_new_connection, hs);
    root = __http_node_new(NULL, NULL, NULL, NULL);

    http_server_init(hs, ev, root);

    return hs;
}

#include <stdio.h>

static void http_dump_node(gpointer key, gpointer value, gpointer user_data)
{
    http_node_t *n;
    char        *name;
    int         *level;

    name  = key;
    n     = value;
    level = user_data;

    fprintf(stderr, "%*s%s cb=%p data=%p\n", *level, "", name, n->callback, n->data);

    *level += 1;
    g_hash_table_foreach(n->child, http_dump_node, user_data);
    *level -= 1;
}

void http_dump_tree(http_server_t *server)
{
    int level = 0;

    if(server == NULL) {
        fprintf(stderr, "HTTP server (null)\n");
        return;
    }

    if(server->root == NULL) {
        fprintf(stderr, "HTTP server (null)\n");
        return;
    }

    http_dump_node("root", server->root, &level);
}
