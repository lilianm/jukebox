#ifndef __HTTP_H__
#define __HTTP_H__

#include <stdint.h>

#include "event_output.h"

typedef struct http_server http_server_t;

typedef struct http_request http_request_t;

typedef int (*http_node_f)(http_request_t *hr, void *data, const char *remaining, size_t size);

typedef void (*free_session_f)(void *);

typedef int (*auth_session_f)(http_request_t *hr, char *login, char *password);

int http_node_new(http_server_t *server, char *path, http_node_f cb, void *data);

int http_request_detach(http_request_t *hr);

void http_request_set_data(http_request_t *hr, void *data, free_session_f free);

void * http_request_get_data(http_request_t *hr);

void * http_request_get_content(http_request_t *hr, size_t *len);

http_server_t * http_server_new(uint16_t port);

void http_dump_tree(http_server_t *server);

int http_server_set_auth_cb(http_server_t *server, auth_session_f as);

void http_send_reponse(http_request_t *hr, char *content_type, void *buffer, size_t size, free_f free_cb, void *user_data);

#endif  /* __HTTP_H__ */
