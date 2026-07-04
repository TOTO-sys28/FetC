#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stddef.h>

typedef struct {
    int sockfd;
    void *ssl;      /* SSL*  - opaque */
    void *ctx;      /* SSL_CTX* - opaque */
} Transport;

int transport_connect(Transport *t, const char *host, int port, int use_ssl);
ssize_t transport_send(Transport *t, const void *buf, size_t len);
ssize_t transport_recv(Transport *t, void *buf, size_t len);
void transport_close(Transport *t);

#endif
