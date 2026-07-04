#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <time.h>
#include "url.h"

typedef void (*download_progress_cb)(size_t current, size_t total, void *user_data);

typedef struct {
    download_progress_cb on_progress;
    void *on_progress_ud;

    struct timespec _last_progress_time;
    size_t _last_progress_bytes;
} Downloader;

int download_init(Downloader *dl);
int download_file(Downloader *dl, const URL *url);
void download_destroy(Downloader *dl);

#endif
