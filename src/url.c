#include <string.h>
#include "fetc.h"

int url_parse(const char *input, URL *url)
{
    const char *protocol_end = strstr(input, "://");

    if (!protocol_end)
        return -1;

    size_t protocol_length = protocol_end - input;

    if (protocol_length >= sizeof(url->protocol))
        return -1;

    memcpy(url->protocol, input, protocol_length);
    url->protocol[protocol_length] = '\0';

    if (strcmp(url->protocol, "http") == 0)
        url->port = 80;
    else if (strcmp(url->protocol, "https") == 0)
        url->port = 443;
    else
        return -1;

    const char *host_start = protocol_end + 3;

    const char *path_start = strchr(host_start, '/');

    if (!path_start)
    {
        if (strlen(host_start) >= sizeof(url->host))
            return -1;
        strcpy(url->host, host_start);
        strcpy(url->path, "/");
        return 0;
    }

    size_t host_length = path_start - host_start;

    if (host_length >= sizeof(url->host))
        return -1;

    memcpy(url->host, host_start, host_length);
    url->host[host_length] = '\0';

    if (strlen(path_start) >= sizeof(url->path))
        return -1;
    strcpy(url->path, path_start);

    return 0;
}

const char *url_filename(const URL *url)
{
    const char *last_slash = strrchr(url->path, '/');

    if (last_slash && *(last_slash + 1) != '\0')
        return last_slash + 1;

    return "index.html";
}
