#include "hashmap.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct hash_map_node{
    uint32_t key;
    void* value;
    struct hash_map_node *next;
} hash_map_node_t;

typedef hash_map_node_t* hash_map_node_ptr;

typedef struct {
    hash_map_node_ptr *data;
    uint32_t capacity;
    uint32_t limit;
    uint32_t size;
} hash_map_private;


uint32_t hash_map_index(uint32_t capacity, uint32_t key){
    return key & (capacity - 1);
}

int hash_map_set(hash_map_node_ptr *container, uint32_t index, hash_map_node_t *new_node){
    assert(container);

    hash_map_node_t *node = container[index];
    if(node){
        new_node->next = node;
    }

    container[index] = new_node;

    return 0;
}

int hash_map_resize(hash_map_private *private) {
    assert(private);

    hash_map_node_ptr *container;
    uint32_t new_capacity = private->capacity << 1;
    container = (hash_map_node_ptr*)malloc(sizeof(hash_map_node_ptr) * new_capacity);
    memset(container, 0, sizeof(hash_map_node_ptr));
    //
    uint32_t i, index;
    hash_map_node_t *node, *swap;
    for(i = 0; i < private->capacity; i++) {
        swap = private->data[i];
        while(swap) {
            node = swap;
            swap = node->next;
            node->next = NULL;

            index = hash_map_index(new_capacity, node->key);
            (void)hash_map_set(container, index, node);
        }
    }
    //
    free(private->data);
    //
    private->capacity = new_capacity;
    private->data     = container;
    private->limit    = private->capacity * 0.75;
    //
    return 0;
}

int hash_map_init(hash_map_t *hash, uint32_t capacity) {
    assert(hash);
    assert(capacity > 1);
    assert((capacity & (capacity -1)) == 0);
    //
    hash_map_private *private = (hash_map_private*)malloc(sizeof(hash_map_private));
    private->data = (hash_map_node_ptr*)malloc(sizeof(hash_map_node_ptr) * capacity);
    memset(private->data, 0, sizeof(hash_map_node_ptr) * capacity);
    private->capacity = capacity;
    private->size = 0;
    //
    private->limit = private->capacity * 0.75;
    //
    hash->priv = private;
    //
    return 0;
}

int hash_map_add(hash_map_t *hash, uint32_t key, void* value) {
    assert(hash);
    hash_map_private *private = (hash_map_private*)hash->priv;
    assert(private);

    if(private->limit < private->size) {
        (void) hash_map_resize(private);
    }

    hash_map_node_t *new_node = (hash_map_node_t *)malloc(sizeof(hash_map_node_t));
    memset(new_node, 0, sizeof(hash_map_node_t));
    new_node->key   = key;
    new_node->value = value;

    uint32_t index = hash_map_index(private->capacity, key);
    (void)hash_map_set(private->data, index, new_node);
    private->size += 1;

    return 0;
}

void* hash_map_del(hash_map_t *hash, uint32_t key) {
    assert(hash);
    hash_map_private *private = (hash_map_private*)hash->priv;
    assert(private);

    uint32_t index;
    void* value = NULL;
    hash_map_node_t *prev = NULL, *node = NULL, *next = NULL;
    index = hash_map_index(private->capacity, key);
    next = private->data[index];
    while(next) {
        node = next;
        next = node->next;
        //
        if(node->key != key){
            prev = node;
            continue;
        }
        //
        if(prev == NULL){
            private->data[index] = next;
        } else {
            prev->next = next;
        }
        //
        value = node->value;
        free(node);
        //
        break;
    }

    return value;
}

void* hash_map_get(hash_map_t *hash, uint32_t key) {
    assert(hash);
    hash_map_private *private = (hash_map_private*)hash->priv;
    assert(private);

    uint32_t index;
    void* value = NULL;
    hash_map_node_t *node = NULL, *next = NULL;
    index = hash_map_index(private->capacity, key);
    next = private->data[index];
    while(next) {
        node = next;
        next = node->next;
        //
        if(node->key == key){
            value = node->value;
            break;
        }
    }

    return value;
}

int hash_map_foreach(hash_map_t *hash, hash_map_iter iter, void* user) {
    assert(hash);
    assert(iter);
    hash_map_private *private = (hash_map_private*)hash->priv;
    assert(private);

    uint32_t i;
    hash_map_node_t *node, *next;
    for(i = 0; i < private->capacity; i++) {
        next = private->data[i];
        while(next) {
            node = next;
            next = node->next;
            //
            (void)iter(node->key, node->value, user);
            //
        }
    }

    return 0;
}

int hash_map_clear(hash_map_t *hash) {
    assert(hash);
    hash_map_private *private = (hash_map_private*)hash->priv;
    assert(private);

    uint32_t i;
    hash_map_node_t *node, *next;
    for(i = 0; i < private->capacity; i++) {
        next = private->data[i];
        while(next) {
            node = next;
            next = node->next;
            //
            free(node);
            //
        }
    }

    return 0;
}

int hash_map_free(hash_map_t *hash) {
    assert(hash);
    hash_map_private *private = (hash_map_private*)hash->priv;
    assert(private);

    uint32_t i;
    hash_map_node_t *node, *next;
    for(i = 0; i < private->capacity; i++) {
        next = private->data[i];
        while(next) {
            node = next;
            next = node->next;
            //
            free(node);
            //
        }
    }
    //
    free(private->data);
    free(private);
    //
    hash->priv = NULL;
    //
    return 0;
}
