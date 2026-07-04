#ifndef HEADERS_H
#define HEADERS_H

#include <stddef.h>
#include "transport.h"

typedef struct {
    int status_code;
    size_t content_length;
    int chunked;
    int connection_close;
    int accept_ranges;
    size_t range_start;
    size_t range_end;
    size_t range_total;
    char content_type[64];
    char last_modified[64];
} HttpHeaders;

int headers_read(Transport *t, char *buf, size_t buf_size, char **body_ptr, size_t *initial_body_len);
int headers_parse(const char *headers_text, HttpHeaders *out);

#endif
