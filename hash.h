#ifndef __HASH_H__
#define __HASH_H__

#include <stdint.h>
#include <stdlib.h>

typedef int (*cmp_f)(void *e1, void *e2);

typedef uint32_t (*hash_f)(void *e);

typedef struct hash_entry hash_entry_t;

typedef struct hash {
    hash_entry_t *entry;

    size_t        size;
    size_t        nb_entry;

    cmp_f         cmp_fn;
    hash_f        hash_fn;

    hash_entry_t *free_list;
} hash_t;

void hash_init(hash_t *h, cmp_f cmp, hash_f hash, size_t size);

hash_t * hash_new(cmp_f cmp, hash_f hash, size_t size);

void * hash_get(hash_t *h, void *key);

int hash_add(hash_t *h, void *key, void *d);

void * hash_remove(hash_t *h, void *key);

void hash_clean(hash_t *h);

void hash_delete(hash_t *h);

/* Add hash/cmp function (DJB algo) */

int hash_str_cmp(void *v1, void *v2);

uint32_t hash_str_hash(void *v);

#endif /* __HASH_H__ */
