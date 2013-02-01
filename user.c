#include <string.h>

#include "user.h"
#include "hash.h"
#include "channel.h"

#define MAX_SCK_BY_USER 4

struct user {
    char      *name;
    int        sck[MAX_SCK_BY_USER];
    int        nb_sck;
    channel_t *channel;
};

static hash_t *users = NULL;

user_t * user_new(char *user)
{
    user_t *u;

    u = (user_t *) malloc(sizeof(user_t));
    u->nb_sck = 0;
    u->name   = strdup(user);

    hash_add(users, u->name, u);

    return u;
}

user_t * user_get(char *user)
{
    user_t *u;

    if(users == NULL) {
        users = hash_new(hash_str_cmp, hash_str_hash, 8);
    }

    u = hash_get(users, user);
    if(u)
        return u;

    return user_new(user);    
}

int user_add_socket(user_t *user, int sck)
{
    if(user->nb_sck >= MAX_SCK_BY_USER)
        return -1;

    user->sck[user->nb_sck++] = sck;

    return 0;
}

int user_remove_socket(user_t *user, int sck)
{
    int i;

    for(i = 0; i < user->nb_sck; ++i) {
        if(user->sck[i] == sck) {
            user->sck[i] = user->sck[--user->nb_sck];
            return sck;
        }
    }
    return -1;    
}

int * user_get_socket(user_t *user)
{
    if(user->nb_sck)
        return user->sck;
    return NULL;
}

int user_get_nb_socket(user_t *user)
{
    return user->nb_sck;
}

char * user_get_name(user_t *user)
{
    return user->name;
}


void user_set_channel(user_t *user, channel_t *channel)
{
    user->channel = channel;
}

channel_t * user_get_channel(user_t *user)
{
    return user->channel;
}

