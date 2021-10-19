#include "pthreadpool.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


typedef void*(*pthread_pool_task)(void*);

typedef struct pthread_pool_task_node {
    void* args;
    pthread_pool_task task;
    struct pthread_pool_task_node *next;
} pthread_pool_task_node_t;

typedef struct {
    unsigned int thread_count;
    pthread_t *threads;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_pool_task_node_t *head;
    pthread_pool_task_node_t *tail;
    unsigned int task_count;
    int finished;
} pthread_pool_private;


void* pthread_pool_handle(void *ptr) {
    pthread_pool_private *private = (pthread_pool_private *)ptr;
    assert(private);
    while(!private->finished){
        pthread_pool_task_node_t *node = NULL;
        {
            pthread_mutex_lock(&private->mutex);

            if(private->task_count < 1) {
                pthread_cond_wait(&private->cond, &private->mutex);
            }

            if(private->task_count > 0) {
                node = private->head;
                if(node != NULL) {
                    private->head = node->next;
                }
                if(private->head == NULL) {
                    private->tail = NULL;
                }

                private->task_count -= 1;
            }

            pthread_mutex_unlock(&private->mutex);
        }

        //
        if(node != NULL) {
            (void) node->task(node->args);
            free(node);
        }
    }

    return NULL;
}



int pthread_pool_init(pthread_pool_t *pool, unsigned int size) {
    assert(pool);
    pthread_pool_private *private = (pthread_pool_private *)malloc(sizeof(pthread_pool_private));
    memset(private, 0, sizeof(pthread_pool_private));
    private->threads = (pthread_t *)malloc(sizeof(pthread_t) * size);
    memset(private->threads, 0, sizeof(pthread_t) * size);
    private->thread_count = size;

    pthread_mutex_init(&private->mutex, NULL);
    pthread_cond_init(&private->cond, NULL);

    unsigned int i;
    for(i = 0; i < size; i++){
        pthread_create(&private->threads[i], NULL, pthread_pool_handle, private);
    }

    //
    pool->priv = private;

    return 0;
}

int pthread_pool_spawn(pthread_pool_t *pool, void *(*__start_routine)(void *), void *__restrict __arg) {
    assert(pool);
    pthread_pool_private *private = (pthread_pool_private *)pool->priv;
    assert(private);
    {
        pthread_pool_task_node_t *node;
        pthread_mutex_lock(&private->mutex);
        //
        node = (pthread_pool_task_node_t*)malloc(sizeof(pthread_pool_task_node_t));
        node->args = __arg;
        node->task = __start_routine;
        node->next = NULL;
        //
        if(private->head == NULL){
            private->head = node;
            private->tail = node;
        } else {
            private->tail->next = node;
            private->tail = node;
        }
        //
        private->task_count += 1;
        //
        pthread_cond_signal(&private->cond);
        pthread_mutex_unlock(&private->mutex);
    }

    return 0;
}

int pthread_pool_destroy(pthread_pool_t *pool) {
    assert(pool);
    pthread_pool_private *private = (pthread_pool_private *)pool->priv;
    assert(private);

    pthread_mutex_lock(&private->mutex);
    private->finished = 1;

    pthread_pool_task_node_t *node;
    while(private->head != NULL){
        node = private->head;
        private->head = node->next;
        free(node);
    }
    private->head = NULL;
    private->tail = NULL;

    pthread_cond_broadcast(&private->cond);
    pthread_mutex_unlock(&private->mutex);
    //
    unsigned int i;
    for(i = 0; i < private->thread_count; i++) {
        pthread_join(private->threads[i], NULL);
    }
    //
    pthread_cond_destroy(&private->cond);
    pthread_mutex_destroy(&private->mutex);

    free(private->threads);
    free(private);
    //
    pool->priv = NULL;

    return 0;
}
