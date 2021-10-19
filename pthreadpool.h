#ifndef PTHREADPOOL_H
#define PTHREADPOOL_H


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *priv;
} pthread_pool_t;


int pthread_pool_init(
    pthread_pool_t *pool,
    unsigned int size
);

int pthread_pool_spawn(
    pthread_pool_t *pool,
    void *(*__start_routine)(void *),
    void *__restrict __arg
);

int pthread_pool_destroy(
    pthread_pool_t *pool
);


#ifdef __cplusplus
}
#endif

#endif // PTHREADPOOL_H
