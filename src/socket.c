#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
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

    if (connect(sockfd, result->ai_addr, result->ai_addrlen) != 0) {
        freeaddrinfo(result);
        close(sockfd);
        return -1;
    }

    freeaddrinfo(result);
    return sockfd;
}
