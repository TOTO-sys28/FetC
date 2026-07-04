#ifndef FETC_H
#define FETC_H

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* URL */
typedef struct {
    char protocol[16];
    char host[256];
    char path[1024];
    int port;
} URL;

int url_parse(const char *input, URL *url);
const char *url_filename(const URL *url);

/* Download Info */
typedef struct {
    int status_code;
    size_t content_length;
    int accept_ranges;
    int chunked;
    char content_type[64];
    char last_modified[64];
} DownloadInfo;

int download_info(const URL *url, DownloadInfo *info);

/* Download */
typedef void (*download_progress_cb)(size_t current, size_t total, void *user_data);

typedef struct {
    download_progress_cb on_progress;
    void *on_progress_ud;

    /* internal */
    struct timespec _last_progress_time;
    size_t _last_progress_bytes;
} Downloader;

int download_init(Downloader *dl);
int download_file(Downloader *dl, const URL *url);
void download_destroy(Downloader *dl);

/* Segmented download */
typedef struct {
    int segments;           /* Number of parallel segments (default: 4) */
    download_progress_cb on_progress;
    void *on_progress_ud;
} SegmentedDownloader;

int segmented_download_init(SegmentedDownloader *sd);
int segmented_download_file(SegmentedDownloader *sd, const URL *url);

#ifdef __cplusplus
}
#endif

#endif
