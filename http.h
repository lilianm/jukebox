#ifndef __HTTP_H__
#define __HTTP_H__

#include <stdint.h>

#include "event_output.h"

typedef struct http_server http_server_t;

typedef struct http_request http_request_t;

typedef int (*http_node_f)(http_request_t *hr, void *data, const char *remaining, size_t size);

int http_node_new(http_server_t *server, char *path, http_node_f cb, void *data);

int http_request_detach(http_request_t *hr);

http_server_t * http_server_new(uint16_t port);

void http_dump_tree(http_server_t *server);

void http_send_reponse(http_request_t *hr, char *content_type, void *buffer, size_t size, free_f free_cb, void *user_data);

#endif  /* __HTTP_H__ */
