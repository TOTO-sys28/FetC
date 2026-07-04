#ifndef SOCKET_H
#define SOCKET_H

typedef struct {
    char host[256];
    int port;
} ProxyConfig;

int socket_connect(const char *hostname, int port);
int socket_connect_proxy(const char *hostname, int port, const ProxyConfig *proxy);
int proxy_parse_env(const char *env_var, ProxyConfig *out);

#endif
