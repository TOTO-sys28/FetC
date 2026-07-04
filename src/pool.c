#include <stdlib.h>
#include <string.h>
#include "pool.h"

Transport *pool_acquire(const char *host, int port, int use_ssl)
{
    Transport *t = malloc(sizeof(*t));
    if (!t)
        return NULL;

    memset(t, 0, sizeof(*t));

    if (transport_connect(t, host, port, use_ssl) != 0) {
        free(t);
        return NULL;
    }

    return t;
}

void pool_release(Transport *t,
                  const char *host,
                  int port,
                  int use_ssl,
                  int keep_alive)
{
    (void)host;
    (void)port;
    (void)use_ssl;
    (void)keep_alive;

    if (!t)
        return;

    transport_close(t);
    free(t);
}

void pool_shutdown(void)
{
}
