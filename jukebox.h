#ifndef __JUKEBOX_H__
#define __JUKEBOX_H__

#include "http.h"

http_server_t * jukebox_init(int port);

void jukebox_launch(void);

#endif /* __JUKEBOX_H__ */
