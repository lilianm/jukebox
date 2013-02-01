#ifndef __CHANNEL_H__
#define __CHANNEL_H__

typedef struct channel channel_t;

#include "user.h"

void channel_init(void);

channel_t * channel_add_user(char *channel, user_t *u);

int channel_next(channel_t *channel);

#endif /* __CHANNEL_H__ */
