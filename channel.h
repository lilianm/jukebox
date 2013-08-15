#ifndef __CHANNEL_H__
#define __CHANNEL_H__

typedef struct channel channel_t;

#include "user.h"
#include "song.h"
#include "song_queue.h"

void channel_init(void);

channel_t * channel_add_user(char *channel, user_t *u);

int channel_next(channel_t *channel);

song_t * channel_current_song(channel_t *c);

int channel_current_song_elapsed(channel_t *c);

song_queue_t * channel_get_queue(channel_t *c);

#endif /* __CHANNEL_H__ */
