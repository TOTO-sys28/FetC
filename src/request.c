#include <stdio.h>
#include <string.h>
#include "request.h"

int request_build(const char *method, const char *host, const char *path, size_t range_start, char *buf, size_t buf_size)
{
    int n;
    if (range_start > 0) {
        n = snprintf(buf, buf_size,
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: BDownloader/0.1\r\n"
            "Range: bytes=%zu-\r\n"
            "Connection: keep-alive\r\n"
            "\r\n",
            method, path, host, range_start);
    } else {
        n = snprintf(buf, buf_size,
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: BDownloader/0.1\r\n"
            "Connection: keep-alive\r\n"
            "\r\n",
            method, path, host);
    }

    if (n < 0 || (size_t)n >= buf_size)
        return -1;

    return 0;
}
