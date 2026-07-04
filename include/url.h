#ifndef URL_H
#define URL_H

typedef struct {
    char protocol[16];
    char host[256];
    char path[1024];
    int port;
} URL;

int url_parse(const char *input, URL *url);
const char *url_filename(const URL *url);

#endif
