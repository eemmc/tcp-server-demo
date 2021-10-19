#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcpserver.h"

int client_on_readable(int sfd, void *data, uint32_t len, void *user) {
    tcp_server_t *server = (tcp_server_t *) user;
    tcp_server_write(server, sfd, data, len, 0);
    return 0;
}


int main()
{
    tcp_server_t server;

    tcp_server_attr_t attrs;
    memset(&attrs, 0, sizeof(tcp_server_attr_t));
    attrs.on_readable = client_on_readable;
    //
    fprintf(stderr, "server start...\n");
    tcp_server_setup(&server, 8088, &attrs,  &server);

    return 0;
}
