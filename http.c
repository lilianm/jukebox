#include <stdio.h>

#include "stream.h"
#include "sck.h"
#include "vector.h"

#define PORT 8080

typedef struct http_option_t {
    stream_t    name;
    stream_t    value;
} http_option_t;

VECTOR_T(option, http_option_t);
VECTOR_T(line, stream_t);

typedef struct http_request_t {
    stream_t            methode;
    stream_t            uri;
    vector_option_t     options;
    char                header[1024*1024*64];
    int                 header_length;
    int                 content_length;
    void*               data;
} http_request_t;

void http_request_init(http_request_t *hr)
{
    vector_option_init(&hr->options);
    hr->header_length = 0;
}

void http_request_line_decode(http_request_t *hr, stream_t *line)
{
    assert(hr);
    assert(line);

    stream_find_chr(line, ' ', &hr->methode);
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

int http_header_decode(http_request_t *hr)
{
    vector_line_t       opt;
    stream_t            line;
    stream_t            content_length;
    stream_t            data;
    stream_t            header;

    assert(hr);

    data = stream_init(hr->header, hr->header_length);

    if(stream_find_mem(&data, "\r\n\r\n", 4, &header) == -1)
        return -1;
    stream_expand(&header, 2);

    vector_line_init(&opt);
    while(stream_find_mem(&header, "\r\n", 2, &line) != -1)
    {
        vector_line_push(&opt, &line);
        stream_skip(&header, 2);
    }

    http_request_line_decode(hr, &opt.data[0]);
    http_options_decode(hr, &opt.data[1], opt.len - 1);
    vector_line_clean(&opt);

    if(http_option_search(hr, "Content-Length", &content_length) == -1) {
        hr->content_length = 0;
    } else {
        hr->content_length = atoi(content_length.data);
    }

    assert(stream_len(&header) == 0);
    return 0;
}

http_request_t hr;

typedef enum http_page_kind_t {
    HTTP_PAGE_KIND_MEM,
    HTTP_PAGE_KIND_FILE,
    HTTP_PAGE_KIND_FUNC,
} http_page_kind_t;

typedef struct http_page_mem_t
{
    void *data;
    int   size;
} http_page_mem_t;

typedef struct http_page_file_t
{
    void *data;
    int   size;
    int   fd;
} http_page_file_t;

typedef union http_page_data_t
{
    http_page_mem_t  mem;
    http_page_file_t file;
} http_page_data_t;

typedef struct http_page_t
{
    char*               url;
    char*               content_type;
    http_page_kind_t    kind;    
} http_page_t;

VECTOR_T(page, http_page_t);

typedef struct http_server_t
{
    int                 srv;
    int                 client;

    vector_page_t       pages;
    http_request_t      hr;    
} http_server_t;

void http_server_init(http_server_t *hs)
{
    hs->srv    = -1;
    hs->client = -1;

    vector_page_init(&hs->pages);
    http_request_init(&hs->hr);
}

http_server_t * http_server_new(uint16_t port)
{
    http_server_t* hs;
    struct sockaddr_in addr;

    hs = (http_server_t*) malloc(sizeof(http_server_t));

    http_server_init(hs);

    hs->srv     = xlisten(port);
    hs->client  = xaccept(hs->srv, &addr);

    http_request_init(&hr);
    hs->hr.header_length = recv(hs->client, hs->hr.header, sizeof(hs->hr.header), 0);
    http_header_decode(&hs->hr);

    return hs;
}

int main(void)
{
    http_server_new(PORT);

    
    return 0;
}
