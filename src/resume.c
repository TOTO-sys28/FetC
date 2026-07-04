#include <sys/stat.h>
#include "resume.h"

int resume_get_local_size(const char *filename, size_t *size)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        *size = 0;
        return 0;
    }
    *size = (size_t)st.st_size;
    return 0;
}
