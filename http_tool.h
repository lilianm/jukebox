#ifndef __HTTP_TOOL_H__
#define __HTTP_TOOL_H__

#include <stdlib.h>

#include "http.h"

int http_map_directory(http_server_t *server, char *server_path, char *file_path);

char * http_get_mime_type_by_extension(char *s, size_t len);

void http_send_reponse_and_dup(http_request_t *hr, char *content_type, void *buffer, size_t size);

#endif /* __HTTP_TOOL_H__ */
