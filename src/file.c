#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"

int file_write_chunk(FILE *fp, const void *data, size_t size)
{
    if (fwrite(data, 1, size, fp) != size)
        return -1;
    return 0;
}
