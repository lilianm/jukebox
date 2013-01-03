#ifndef __HTTP_H__
#define __HTTP_H__

#include <stdint.h>

typedef struct http_server http_server_t;

typedef struct http_request http_request_t;

typedef int (*http_node_callback)(http_request_t *hr, void *data, const char *remaining, size_t size);

int http_node_new(http_server_t *server, char *path, http_node_callback cb, void *data);

int http_request_detach(http_request_t *hr);

http_server_t * http_server_new(uint16_t port);

#endif  /* __HTTP_H__ */
