#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "socket.h"

int socket_connect(const char *hostname, int port)
{
    struct addrinfo hints, *result;
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(hostname, port_str, &hints, &result);
    if (err != 0)
        return -1;

    int sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(result);
        return -1;
    }

    /* Set socket timeouts (30 seconds) */
    struct timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        freeaddrinfo(result);
        close(sockfd);
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        freeaddrinfo(result);
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, result->ai_addr, result->ai_addrlen) != 0) {
        freeaddrinfo(result);
        close(sockfd);
        return -1;
    }

    freeaddrinfo(result);
    return sockfd;
}

int proxy_parse_env(const char *env_var, ProxyConfig *out)
{
    const char *proxy_str = getenv(env_var);
    if (!proxy_str || strlen(proxy_str) == 0) {
        return -1;
    }

    /* Parse format: http://proxy.example.com:8080 or proxy.example.com:8080 */
    const char *proto_sep = strstr(proxy_str, "://");
    const char *host_start = proto_sep ? proto_sep + 3 : proxy_str;

    const char *port_sep = strchr(host_start, ':');
    if (!port_sep) {
        return -1;
    }

    size_t host_len = port_sep - host_start;
    if (host_len >= sizeof(out->host)) {
        return -1;
    }

    strncpy(out->host, host_start, host_len);
    out->host[host_len] = '\0';
    out->port = atoi(port_sep + 1);

    if (out->port <= 0 || out->port > 65535) {
        return -1;
    }

    return 0;
}

int socket_connect_proxy(const char *hostname, int port, const ProxyConfig *proxy)
{
    /* Connect to proxy instead of target */
    int proxy_sockfd = socket_connect(proxy->host, proxy->port);
    if (proxy_sockfd < 0) {
        return -1;
    }

    /* Send CONNECT request for HTTPS tunneling */
    char connect_req[512];
    int len = snprintf(connect_req, sizeof(connect_req),
                       "CONNECT %s:%d HTTP/1.1\r\n"
                       "Host: %s:%d\r\n"
                       "\r\n",
                       hostname, port, hostname, port);

    if (len < 0 || (size_t)len >= sizeof(connect_req)) {
        close(proxy_sockfd);
        return -1;
    }

    if (send(proxy_sockfd, connect_req, len, 0) != len) {
        close(proxy_sockfd);
        return -1;
    }

    /* Read proxy response */
    char response[1024];
    int total = 0;
    while (total < (int)(sizeof(response) - 1)) {
        int n = recv(proxy_sockfd, response + total, (int)(sizeof(response) - 1 - total), 0);
        if (n <= 0) {
            close(proxy_sockfd);
            return -1;
        }
        total += n;

        /* Check for end of headers */
        if (total >= 4) {
            if (response[total - 4] == '\r' && response[total - 3] == '\n' &&
                response[total - 2] == '\r' && response[total - 1] == '\n') {
                break;
            }
        }
    }

    response[total] = '\0';

    /* Check for successful response (HTTP/1.1 200) */
    if (strstr(response, "200") == NULL) {
        close(proxy_sockfd);
        return -1;
    }

    return proxy_sockfd;
}
