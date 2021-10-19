#ifndef TCPSERVER_H
#define TCPSERVER_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    void* priv;
} tcp_server_t;

typedef struct {
    int (*on_connect)(int sfd, void* user);
    int (*on_readable)(int sfd, void* data, uint32_t len, void* user);
    int (*on_disconnect)(int sfd, void* user);
} tcp_server_attr_t;


int tcp_server_setup(
    tcp_server_t *server,
    uint16_t port,
    tcp_server_attr_t *attrs,
    void* user
);

int tcp_server_write(
    tcp_server_t *server,
    int sfd,
    void *data,
    uint32_t len,
    int blocking
);

int tcp_server_shutdown(
    tcp_server_t *server
);


#ifdef __cplusplus
}
#endif

#endif // TCPSERVER_H
