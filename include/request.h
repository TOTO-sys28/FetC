#ifndef REQUEST_H
#define REQUEST_H

#include <stddef.h>

int request_build(const char *method, const char *host, const char *path, size_t range_start, char *buf, size_t buf_size);

#endif
