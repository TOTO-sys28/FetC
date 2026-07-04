#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include <stddef.h>

int file_write_chunk(FILE *fp, const void *data, size_t size);

#endif
