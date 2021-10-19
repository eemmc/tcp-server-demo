#include "tcpserver.h"
#include "hashmap.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define MAX_WAIT_EVENTS 16
#define BUFFER_SIZE 0x12000 //72k

typedef struct {
    void *data;
    uint32_t cap;
    uint32_t len;
    uint32_t pos;
} tcp_server_buffer;

int tcp_server_buffer_init(tcp_server_buffer **pointer, uint32_t capacity) {
    assert(pointer);
    tcp_server_buffer *buffer;
    buffer = (tcp_server_buffer *)malloc(sizeof(tcp_server_buffer));
    //
    assert(buffer);
    buffer->data = malloc(capacity);
    buffer->cap  = capacity;
    buffer->pos  = 0;
    buffer->len  = 0;
    //
    *pointer = buffer;
    //
    return 0;
}

int tcp_server_buffer_free(tcp_server_buffer **pointer) {
    assert(pointer);
    tcp_server_buffer *buffer = *pointer;
    //
    free(buffer->data);
    buffer->data = NULL;
    buffer->cap  = 0;
    buffer->len  = 0;
    buffer->pos  = 0;
    free(buffer);
    //
    *pointer = NULL;
    //
    return 0;
}

typedef struct {
    int handle;
    tcp_server_buffer *rbuffer;
    tcp_server_buffer *wbuffer;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} tcp_server_connect;

int tcp_server_connect_init(tcp_server_connect **pointer, int sfd) {
    assert(pointer);
    tcp_server_connect *connect = NULL;
    //
    connect = (tcp_server_connect *)malloc(sizeof(tcp_server_connect));
    memset(connect, 0, sizeof(tcp_server_connect));
    connect->handle = sfd;
    //
    tcp_server_buffer_init(&connect->rbuffer, BUFFER_SIZE);
    tcp_server_buffer_init(&connect->wbuffer, BUFFER_SIZE);
    //
    pthread_mutex_init(&connect->mutex, NULL);
    pthread_cond_init(&connect->cond, NULL);
    //
    *pointer = connect;
    //
    return 0;
}

int tcp_server_connect_free(tcp_server_connect **pointer) {
    assert(pointer);
    tcp_server_connect *connect = *pointer;
    //
    assert(connect);
    pthread_cond_broadcast(&connect->cond);
    shutdown(connect->handle, SHUT_RDWR);
    //
    pthread_cond_destroy(&connect->cond);
    pthread_mutex_destroy(&connect->mutex);
    //
    tcp_server_buffer_free(&connect->rbuffer);
    tcp_server_buffer_free(&connect->wbuffer);
    //
    free(connect);
    *pointer = NULL;
    //
    return 0;
}

int tcp_server_connect_read(tcp_server_connect *connect) {
    assert(connect);
    tcp_server_buffer *buffer = connect->rbuffer;
    //
    int count;
    assert(buffer);
    while(1) {
        count = recv(connect->handle, buffer->data + buffer->len, buffer->cap - buffer->len, 0);
        fprintf(stderr, "count => %d (errno: %d)\n", count, errno);
        if(count > 0) {
            buffer->len += count;
            continue;
        }
        else if(count == 0) {
            return -1;
        }
        else if(errno == EWOULDBLOCK) {
            return 0;
        }
        else if(errno == EAGAIN) {
            continue;
        }
        else {
            return -2;
        }
    }
}

int tcp_server_connect_write(tcp_server_connect *connect) {
    assert(connect);
    tcp_server_buffer *buffer = connect->wbuffer;
    //
    assert(buffer);
    assert(buffer->len > buffer->pos);
    size_t size = buffer->len - buffer->pos;
    //
    int count;
    while(1) {
        count = send(connect->handle, buffer->data + buffer->pos, size, 0);
        if(count > 0) {
            buffer->pos += count;
            if(buffer->pos < buffer->len) {
                continue;
            } else {
                return buffer->pos;
            }
        }
        else if(count == 0) {
            return -1;
        }
        else if(errno == EWOULDBLOCK) {
            return 0;
        }
        else if(errno == EAGAIN) {
            continue;
        }

        else {
            return -2;
        }
    }
    return 0;
}

typedef struct {
    struct sockaddr_in addr[2];
    struct epoll_event events[MAX_WAIT_EVENTS];
    tcp_server_attr_t attrs;
    hash_map_t connects;
    int epollfd;
    int eventfd;
    int listenfd;
    void *user;
} tcp_server_private;

int tcp_server_make_non_blocking(int sockfd) {
    int flags, ret;
    //
    flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1) {
        perror("fcntl(F_GETFL)");
        return -1;
    }
    //
    ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if(ret == -1) {
        perror("fcntl(F_SETFL)");
        return -1;
    }
    //
    return 0;
}

int tcp_server_disconnect(tcp_server_private *private, tcp_server_connect *connect) {
    assert(private);
    assert(connect);
    //
    if(private->attrs.on_disconnect) {
        private->attrs.on_disconnect(connect->handle, private->user);
    }

    if(epoll_ctl(private->epollfd, EPOLL_CTL_DEL, connect->handle, NULL) < 0) {
        perror("epoll_ctl(DEL");
    }

    tcp_server_connect_free(&connect);
    //
    return 0;
}

int tcp_server_foreach_disconnect(uint32_t key, void* value, void* user) {
    (void)key;
    //
    assert(user);
    tcp_server_private *private = (tcp_server_private *)user;
    //
    assert(private);
    tcp_server_connect *connect = (tcp_server_connect *)value;
    //
    assert(connect);
    tcp_server_disconnect(private, connect);
    //
    return 0;
}

int tcp_server_loop(tcp_server_private *private) {
    assert(private);
    //
    int count;
    int finished = 0;
    while(!finished) {
        count = epoll_wait(private->epollfd, private->events, MAX_WAIT_EVENTS, 0);
        if(count == -1 && errno != EINTR) {
            perror("epoll_wait");
            break;
        }
        //
        int i;
        struct epoll_event *event;
        for(i = 0; i < count; i++) {
            event = private->events + i;
            //
            if(event->data.fd == private->eventfd) {
                eventfd_t val;
                (void) eventfd_read(private->eventfd, &val);
                // TODO handle eventfd event.
                finished = 1;
                break;
            }
            else if(event->data.fd == private->listenfd) {
                socklen_t len = sizeof(private->addr[1]);
                int sockfd = accept(private->listenfd, (struct sockaddr*)&private->addr[1], &len);
                if(sockfd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("accpet");
                    finished = 1;
                    break;
                }
                //
                tcp_server_connect *connect;
                tcp_server_connect_init(&connect, sockfd);
                hash_map_add(&private->connects, sockfd, connect);
                //
                if(tcp_server_make_non_blocking(sockfd) != 0) {
                    perror("accept non blocking");
                    finished = 1;
                    break;
                }
                //
                struct epoll_event event = {};
                event.data.fd = sockfd;
                event.events  = EPOLLIN;
                if(epoll_ctl(private->epollfd, EPOLL_CTL_ADD, sockfd, &event) < 0) {
                    perror("accept epoll_ctl(ADD)");
                    finished = 1;
                    break;
                }
                //
                if(private->attrs.on_connect) {
                    private->attrs.on_connect(sockfd, private->user);
                }
                //
                continue;
            }
            else if(event->events & EPOLLIN) {
                tcp_server_connect *connect;
                connect = hash_map_get(&private->connects, event->data.fd);
                //
                int state = tcp_server_connect_read(connect);
                if(state == -1) {
                    if(private->attrs.on_disconnect) {
                        private->attrs.on_disconnect(
                            connect->handle,
                            private->user
                        );
                    }

                    tcp_server_disconnect(private, connect);
                    continue;
                }
                else if(state == -2) {
                    perror("read failed");
                    finished = 1;
                    break;
                }
                else {
                    if(private->attrs.on_readable) {
                        private->attrs.on_readable(
                            connect->handle,
                            connect->rbuffer->data,
                            connect->rbuffer->len,
                            private->user
                        );
                    }
                    // reset.
                    connect->rbuffer->pos = 0;
                    connect->rbuffer->len = 0;
                }

            }
            else if(event->events & EPOLLOUT) {
                tcp_server_connect *connect;
                connect = hash_map_get(&private->connects, event->data.fd);
                //
                fprintf(stderr, "write...\n");
                int state = tcp_server_connect_write(connect);
                if(state == -1) {
                    if(private->attrs.on_disconnect) {
                        private->attrs.on_disconnect(
                            connect->handle,
                            private->user
                        );
                    }

                    tcp_server_disconnect(private, connect);
                    continue;
                }
                else if(state == -2) {
                    perror("read failed");
                    finished = 1;
                    break;
                }
                else {
                    if(state == connect->wbuffer->len) {
                        struct epoll_event event = {};
                        event.data.fd = connect->handle;
                        event.events  = EPOLLIN;
                        if(epoll_ctl(private->epollfd, EPOLL_CTL_MOD, connect->handle, &event) < 0) {
                            perror("epoll_ctl(MOD)");
                            finished = 1;
                            break;
                        }
                    }
                }
            }

        }
    }
    //
    return 0;
}

int tcp_server_setup(tcp_server_t *server, uint16_t port, tcp_server_attr_t *atts, void *user)  {
    assert(server);
    tcp_server_private *private;
    private = (tcp_server_private *)malloc(sizeof(tcp_server_private));
    memset(private, 0, sizeof(tcp_server_private));
    server->priv = private;
    //
    private->user = user;
    if(atts) {
        memcpy(&private->attrs, atts, sizeof(tcp_server_attr_t));
    }
    (void) hash_map_init(&private->connects, 32);
    //
    private->addr[0].sin_family = AF_INET;
    private->addr[0].sin_addr.s_addr = INADDR_ANY;
    private->addr[0].sin_port   = htons(port);
    //
    private->epollfd = epoll_create(1024);
    if(private->epollfd < 0) {
        perror("epoll_create");
        goto FINISH;
    }
    //
    private->eventfd = eventfd(0, EFD_NONBLOCK);
    if(private->eventfd < 0) {
        perror("eventfd");
        goto FINISH;
    }
    //
    struct epoll_event event = {};
    //
    event.data.fd = private->eventfd;
    event.events  = EPOLLIN | EPOLLET;
    if(epoll_ctl(private->epollfd, EPOLL_CTL_ADD, private->eventfd, &event) < 0){
        perror("epoll_ctl(ADD)");
        goto FINISH;
    }
    //
    private->listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(private->listenfd < 0) {
        perror("socket");
        goto FINISH;
    }
    //
    if(tcp_server_make_non_blocking(private->listenfd) != 0) {
        perror("listen non blocking");
        goto FINISH;
    }
    //
    int opt = 1;
    if(setsockopt(private->listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        goto FINISH;
    }
    //
    event.data.fd = private->listenfd;
    event.events  = EPOLLIN | EPOLLET;
    if(epoll_ctl(private->epollfd, EPOLL_CTL_ADD, private->listenfd, &event) < 0) {
        perror("epoll_ctl(MOD)");
        goto FINISH;
    }
    //
    socklen_t len = sizeof(private->addr[0]);
    if(bind(private->listenfd, (struct sockaddr *)&private->addr[0], len) < 0) {
        perror("bind");
        goto FINISH;
    }
    //
    if(listen(private->listenfd, 8) < 0) {
        perror("listen");
        goto FINISH;
    }
    //
    (void) tcp_server_loop(private);
    //
FINISH:
    //
    hash_map_foreach(&private->connects, tcp_server_foreach_disconnect, private);
    hash_map_free(&private->connects);
    //
    if(epoll_ctl(private->epollfd, EPOLL_CTL_DEL, private->listenfd, NULL) < 0) {
        perror("epoll_ctl(DEL, listenfd)");
    }
    close(private->listenfd);
    private->listenfd = -1;
    //
    if(epoll_ctl(private->epollfd, EPOLL_CTL_DEL, private->eventfd, NULL) < 0) {
        perror("epoll_ctl(DEL, eventfd)");
    }
    close(private->eventfd);
    private->eventfd = -1;
    //
    close(private->epollfd);
    private->epollfd = -1;
    //
    free(private);
    server->priv = NULL;
    //

    return 0;
}

int tcp_server_write(tcp_server_t *server, int sfd, void *data, uint32_t len, int blocking) {
    assert(server);
    tcp_server_private *private = (tcp_server_private *)server->priv;
    //
    assert(private);
    tcp_server_buffer *buffer;
    tcp_server_connect *connect;
    connect = hash_map_get(&private->connects, sfd);
    buffer  = connect->wbuffer;
    //
    if(blocking) {
        pthread_mutex_lock(&connect->mutex);
    }

    memcpy(buffer->data, data, len);
    buffer->len = len;
    buffer->pos = 0;

    struct epoll_event event = {};
    event.data.fd = connect->handle;
    event.events  = EPOLLIN | EPOLLOUT;
    if(epoll_ctl(private->epollfd, EPOLL_CTL_MOD, connect->handle, &event) < 0) {
        perror("epoll_ctl(MOD)");
        return -1;
    }

    if(blocking) {
        pthread_cond_wait(&connect->cond, &connect->mutex);
        pthread_mutex_unlock(&connect->mutex);
    }
    //
    return 0;
}

int tcp_server_shutdown(tcp_server_t *server) {
    assert(server);
    tcp_server_private *private = (tcp_server_private *)server->priv;
    //
    assert(private);
    (void) eventfd_write(private->eventfd, 1);
    //
    return 0;
}

