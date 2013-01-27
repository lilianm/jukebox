#ifndef __USER_H__
#define __USER_H__

typedef struct user user_t;

user_t * user_get(char *user);

int user_add_socket(user_t *user, int sck);

int user_remove_socket(user_t *user, int sck);

int * user_get_socket(user_t *user);

char * user_get_name(user_t *user);

int user_get_nb_socket(user_t *user);

#endif /* __USER_H__ */
