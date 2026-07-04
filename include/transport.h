#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stddef.h>
#include "socket.h"

typedef struct {
    int sockfd;
    void *ssl;      /* SSL*  - opaque */
    void *ctx;      /* SSL_CTX* - opaque */
    ProxyConfig proxy;
    int use_proxy;
} Transport;

int transport_connect(Transport *t, const char *host, int port, int use_ssl);
ssize_t transport_send(Transport *t, const void *buf, size_t len);
ssize_t transport_recv(Transport *t, void *buf, size_t len);
void transport_close(Transport *t);
void transport_set_insecure(int insecure);
void transport_set_verbose(int verbose);

#endif
