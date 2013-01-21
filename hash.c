#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "hash.h"

struct hash_entry {
    hash_entry_t      *next;
    void              *key;
    void              *data;
    uint32_t           hash;
};

static inline void hash_entry_init(hash_t *h)
{
    unsigned int i;

    memset(h->entry, 0,
           (h->size + (h->size >> 2)) * sizeof(hash_entry_t));
    for(i = 0; i < (h->size >> 2) - 1; ++i) {
        h->entry[h->size + i].next = &h->entry[h->size + i + 1];
    }
    h->free_list = &h->entry[h->size];
}

void hash_init(hash_t *h, cmp_f cmp, hash_f hash, size_t size)
{
    unsigned int nb;

    assert(cmp);
    assert(hash);
    assert(size > 1 && ((size & (size - 1)) == 0)); // Check size is n^2

    nb           = size + (size >> 2); // Alloc 25% overhead

    h->entry     = (hash_entry_t *) malloc(nb * sizeof(hash_entry_t));
    h->size      = size;
    h->nb_entry  = 0;
    h->cmp_fn    = cmp;
    h->hash_fn   = hash;

    hash_entry_init(h);
}

hash_t * hash_new(cmp_f cmp, hash_f hash, size_t size)
{
    hash_t *h;

    h = (hash_t*) malloc(sizeof(hash_t));

    hash_init(h, cmp, hash, size);

    return h;
}

void * hash_get(hash_t *h, void *key)
{
    uint32_t      hash;
    size_t        mask;
    hash_entry_t *entry;

    hash = h->hash_fn(key);
    mask = h->size - 1;

    entry = &h->entry[hash & mask];

    if(entry->key == NULL)
        return NULL;

    while(entry) {
        if(entry->hash == hash &&
           h->cmp_fn(key, entry->key))
            break;
        entry = entry->next;
    }

    if(entry == NULL)
        return NULL;

    return entry->data;
}

static inline void hash_extend(hash_t *h)
{
    hash_entry_t *nentry;
    hash_entry_t *tentry;
    hash_entry_t *entry;
    unsigned int  nb;

    hash_entry_t *oentry;
    size_t        osize;

    size_t        i;
    size_t        mask;

    oentry    = h->entry;
    osize     = h->size;
    h->size <<= 1;
    mask      = h->size - 1;
    nb        = h->size + (h->size >> 2); // Alloc 25% overhead

    entry     = (hash_entry_t *) malloc(nb * sizeof(hash_entry_t));
    h->entry  = entry;

    hash_entry_init(h);

    for(i = 0; i < osize; ++i) {
        entry = &oentry[i];
        if(entry->key == NULL)
            continue;

        while(entry) {
            nentry = &h->entry[entry->hash & mask];

            if(nentry->key == NULL) {
                nentry->data  = entry->data;
                nentry->key   = entry->key;
                nentry->hash  = entry->hash;
                nentry->next  = NULL;
            } else {
                tentry       = h->free_list;
                h->free_list = h->free_list->next;
                tentry->next = nentry->next;
                nentry->next = tentry;
                nentry       = tentry;
                nentry->data = entry->data;
                nentry->key  = entry->key;
                nentry->hash = entry->hash;
            }

            entry = entry->next;
        }
    }
    
    free(oentry);
}

int hash_add(hash_t *h, void *key, void *d)
{
    uint32_t       hash;
    size_t         mask;
    hash_entry_t  *entry;
    hash_entry_t **nentry;

    // Hash load >= 75%
    if(h->nb_entry >= ((h->size >> 1) + (h->size >> 2)))
        hash_extend(h);

    hash = h->hash_fn(key);
    mask = h->size - 1;

    entry = &h->entry[hash & mask];

    if(entry->key == NULL) {
        entry->key  = key;
        entry->data = d;
        entry->hash = hash;

        h->nb_entry++;
        return 0;
    }

    nentry = &entry->next;
    while(*nentry) {
        entry = *nentry;
        if(entry->hash == hash &&
           h->cmp_fn(key, (entry->key)))
            break;
        nentry = &entry->next;
    }

    entry = *nentry;

    if(entry) {
        entry->data = d;

        return 0;
    }

    if(h->free_list == NULL) {
        hash_extend(h);
        return hash_add(h, key, d);
    }

    entry        = h->free_list;
    h->free_list = h->free_list->next;

    entry->key   = key;
    entry->data  = d;
    entry->hash  = hash;
    entry->next  = NULL;

    *nentry      = entry;

    h->nb_entry++;

    return 0;
}

void * hash_remove(hash_t *h, void *key)
{
    uint32_t       hash;
    size_t         mask;
    hash_entry_t  *entry;
    hash_entry_t  *old_entry;
    hash_entry_t **nentry;
    void          *d;

    hash = h->hash_fn(key);
    mask = h->size - 1;

    entry = &h->entry[hash & mask];

    if(entry->key == NULL)
        return NULL;

    nentry = NULL;
    while(entry) {
        if(entry->hash == hash &&
           h->cmp_fn(key, (entry->key)))
            break;
        nentry = &entry->next;
        entry  = entry->next;
    }

    if(entry == NULL)
        return NULL;

    if(nentry == NULL) {
        d = entry->data;

        if(entry->next) {
            old_entry       = entry->next;
            *entry          = *entry->next;
            old_entry->next = h->free_list;
            h->free_list    = old_entry;
        } else {
            entry->data = NULL;
            entry->key  = NULL;
        }

        h->nb_entry--;
        return d;
    }

    d            = entry->data;
    *nentry      = entry->next;
    entry->next  = h->free_list;
    h->free_list = entry;

    h->nb_entry--;

    return d;
}

void hash_clean(hash_t *h)
{
    free(h->entry);
    h->size     = 0;
    h->nb_entry = 0;
}

void hash_delete(hash_t *h)
{
    hash_clean(h);
    free(h);
}

#ifdef TEST

#include <stdio.h>

static uint32_t hash_str(void *e)
{
    uint32_t hash = 0;
    char     *s = (char *)e;

    while(*s) {
        hash += *s;
        s++;
    }

    return hash;
}

static int cmp_str(void *e1, void *e2)
{
    return (strcmp(e1, e2) == 0);
}

#define CHECK_EQ(test, value)                                           \
({                                                                      \
    nb_test++;                                                          \
    if((test) == (value))                                               \
        nb_success ++;                                                  \
    else                                                                \
        fprintf(stderr, "Line %i Need return " #value " for test " #test "\n",  __LINE__); \
})

#define CHECK_NEQ(test, value)                                          \
({                                                                      \
    nb_test++;                                                          \
    if((test) != (value))                                               \
        nb_success ++;                                                  \
    else                                                                \
        fprintf(stderr, "Line %i Need return " #value " for test " #test "\n",  __LINE__); \
})

int main(void)
{
    hash_t *h;
    int     nb_test    = 0;
    int     nb_success = 0;

    CHECK_NEQ(h = hash_new(cmp_str, hash_str, 4), NULL);
    CHECK_EQ(hash_add(h, "toto",  (void *) 1), 0);
    CHECK_EQ(hash_add(h, "1toto", (void *) 2), 0);
    CHECK_EQ(hash_add(h, "2toto", (void *) 3), 0);
    CHECK_EQ(hash_add(h, "3toto", (void *) 4), 0);
    CHECK_EQ(hash_add(h, "5toto", (void *) 5), 0);
    CHECK_EQ(hash_add(h, "t5oto", (void *)15), 0);
    CHECK_EQ(hash_add(h, "to5to", (void *)-1), 0);
    CHECK_EQ(hash_add(h, "to5to", (void *)25), 0);

    CHECK_EQ(hash_get(h, "to5to"), (void*)25);
    CHECK_EQ(hash_get(h, "t5oto"), (void*)15);
    CHECK_EQ(hash_get(h, "5toto"), (void*)5);
    CHECK_EQ(hash_get(h, "3toto"), (void*)4);
    CHECK_EQ(hash_get(h, "2toto"), (void*)3);
    CHECK_EQ(hash_get(h, "1toto"), (void*)2);
    CHECK_EQ(hash_get(h, "toto") , (void*)1);

    CHECK_EQ(hash_remove(h, "tot5o"), (void*)0);
    CHECK_EQ(hash_remove(h, "to5to"), (void*)25);
    CHECK_EQ(hash_get(h, "to5to")   , (void*)0);
    CHECK_EQ(hash_get(h, "t5oto")   , (void*)15);
    CHECK_EQ(hash_remove(h, "5toto"), (void*)5);
    CHECK_EQ(hash_get(h, "5toto")   , (void*)0);
    CHECK_EQ(hash_get(h, "t5oto")   , (void*)15);
    CHECK_EQ(hash_remove(h, "2toto"), (void*)3);
    CHECK_EQ(hash_get(h, "2toto")   , (void*)0);
    CHECK_EQ(hash_remove(h, "2toto"), (void*)0);
    CHECK_EQ(hash_get(h, "4toto")   , (void*)0);
    CHECK_EQ(hash_get(h, "t5oto")   , (void*)15);
    CHECK_EQ(hash_get(h, "5toto")   , (void*)0);

    hash_delete(h);

    CHECK_NEQ(h = hash_new(cmp_str, hash_str, 4), NULL);

    CHECK_EQ(hash_add(h, "t5oto", (void *)15), 0);
    CHECK_EQ(hash_add(h, "to5to", (void *)-1), 0);
    CHECK_EQ(hash_add(h, "tot5o", (void *)25), 0);

    hash_delete(h);

    fprintf(stderr, "Nb test success %i/%i\n", nb_success, nb_test);

    if(nb_success != nb_test)
        return 1;
    return 0;
}

#endif /* TEST */
