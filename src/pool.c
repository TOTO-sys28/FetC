#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "pool.h"

/* Maximum number of distinct connection pools (keyed by host:port:ssl:proxy).
 * 32 is generous for a download tool that typically talks to only a
 * handful of servers per session.  Each pool holds its own array of
 * idle connections, so the total memory footprint is modest. */
#define MAX_POOLS 32

/* Maximum idle connections kept per pool.  8 covers the common case
 * of segmented downloads (default 4 segments) with headroom for
 * concurrent HEAD + GET request patterns on the same host. */
#define MAX_POOL_SIZE 8

typedef struct {
    char host[256];
    int port;
    int use_ssl;
    char proxy_host[256];   /* "" when no proxy is in use */
    int proxy_port;         /* 0 when no proxy is in use */
    int count;
    Transport *connections[MAX_POOL_SIZE];
} Pool;

static Pool pools[MAX_POOLS];
static int pool_count = 0;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Resolve the current proxy configuration from environment variables.
 * Writes the proxy host and port into the caller-supplied buffers.
 * When no proxy is configured, proxy_host is set to "" and
 * proxy_port is set to 0. */
static void resolve_current_proxy(int use_ssl,
                                  char *proxy_host, size_t host_size,
                                  int *proxy_port)
{
    ProxyConfig pc;

    /* Try uppercase first, then lowercase — mirrors transport_connect() */
    const char *env = use_ssl ? "HTTPS_PROXY" : "HTTP_PROXY";
    if (proxy_parse_env(env, &pc) == 0) {
        snprintf(proxy_host, host_size, "%s", pc.host);
        *proxy_port = pc.port;
        return;
    }

    env = use_ssl ? "https_proxy" : "http_proxy";
    if (proxy_parse_env(env, &pc) == 0) {
        snprintf(proxy_host, host_size, "%s", pc.host);
        *proxy_port = pc.port;
        return;
    }

    proxy_host[0] = '\0';
    *proxy_port = 0;
}

static Pool *find_or_create_pool(const char *host, int port, int use_ssl,
                                 const char *proxy_host, int proxy_port)
{
    for (int i = 0; i < pool_count; i++) {
        if (pools[i].port == port && pools[i].use_ssl == use_ssl &&
            pools[i].proxy_port == proxy_port &&
            strcmp(pools[i].host, host) == 0 &&
            strcmp(pools[i].proxy_host, proxy_host) == 0) {
            return &pools[i];
        }
    }

    if (pool_count >= MAX_POOLS)
        return NULL;

    Pool *p = &pools[pool_count++];
    strncpy(p->host, host, sizeof(p->host) - 1);
    p->host[sizeof(p->host) - 1] = '\0';
    p->port = port;
    p->use_ssl = use_ssl;
    strncpy(p->proxy_host, proxy_host, sizeof(p->proxy_host) - 1);
    p->proxy_host[sizeof(p->proxy_host) - 1] = '\0';
    p->proxy_port = proxy_port;
    p->count = 0;
    for (int i = 0; i < MAX_POOL_SIZE; i++)
        p->connections[i] = NULL;

    return p;
}

Transport *pool_acquire(const char *host, int port, int use_ssl)
{
    /* Snapshot the current proxy configuration so the pool key reflects
     * the network path that new connections will actually use.  If the
     * proxy environment changes between runs, connections established
     * through the old proxy will NOT be reused. */
    char cur_proxy_host[256];
    int  cur_proxy_port;
    resolve_current_proxy(use_ssl, cur_proxy_host, sizeof(cur_proxy_host),
                          &cur_proxy_port);

    pthread_mutex_lock(&pool_mutex);

    Pool *p = find_or_create_pool(host, port, use_ssl,
                                  cur_proxy_host, cur_proxy_port);
    if (!p) {
        pthread_mutex_unlock(&pool_mutex);
        return NULL;
    }

    /* Try to reuse an idle connection */
    if (p->count > 0) {
        Transport *t = p->connections[--p->count];
        p->connections[p->count] = NULL;
        pthread_mutex_unlock(&pool_mutex);
        return t;
    }

    pthread_mutex_unlock(&pool_mutex);

    /* No idle connection available, create a new one */
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
    if (!t)
        return;

    if (!keep_alive) {
        transport_close(t);
        free(t);
        return;
    }

    /* Use the proxy configuration that was active when this connection
     * was established (stored inside the Transport struct itself) as
     * the pool key, so the connection is filed under the correct
     * network path regardless of any later environment changes. */
    const char *proxy_host = t->use_proxy ? t->proxy.host : "";
    int         proxy_port = t->use_proxy ? t->proxy.port : 0;

    pthread_mutex_lock(&pool_mutex);

    Pool *p = find_or_create_pool(host, port, use_ssl,
                                  proxy_host, proxy_port);
    if (p && p->count < MAX_POOL_SIZE) {
        p->connections[p->count++] = t;
    } else {
        /* Pool full or not found, close the connection */
        pthread_mutex_unlock(&pool_mutex);
        transport_close(t);
        free(t);
        return;
    }

    pthread_mutex_unlock(&pool_mutex);
}

void pool_shutdown(void)
{
    pthread_mutex_lock(&pool_mutex);

    for (int i = 0; i < pool_count; i++) {
        for (int j = 0; j < pools[i].count; j++) {
            if (pools[i].connections[j]) {
                transport_close(pools[i].connections[j]);
                free(pools[i].connections[j]);
                pools[i].connections[j] = NULL;
            }
        }
        pools[i].count = 0;
    }

    pool_count = 0;

    pthread_mutex_unlock(&pool_mutex);
}
