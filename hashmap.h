#ifndef HASHMAP_H
#define HASHMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct{
    void *priv;
} hash_map_t;

typedef int (*hash_map_iter)(uint32_t key, void* value, void* user);

int hash_map_init(hash_map_t *hash, uint32_t capacity);

int hash_map_add(hash_map_t *hash, uint32_t key, void* value);

void* hash_map_get(hash_map_t *hash, uint32_t key);

void* hash_map_del(hash_map_t *hash, uint32_t key);

int hash_map_foreach(hash_map_t *hash, hash_map_iter iter, void* user);

int hash_map_clear(hash_map_t *hash);

int hash_map_free(hash_map_t *hash);

#ifdef __cplusplus
}
#endif

#endif // HASHMAP_H
