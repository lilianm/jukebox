#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include "user.h"

int channel_add_user(char *channel, user_t *u);

void channel_init(void);

#endif /* __CHANNEL_H__ */
