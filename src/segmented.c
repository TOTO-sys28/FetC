#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <time.h>
#include "fetc.h"
#include "transport.h"
#include "request.h"
#include "headers.h"
#include "chunked.h"
#include "file.h"
#include "resume.h"
#include "pool.h"

typedef struct {
    int id;
    const URL *url;
    size_t start;
    size_t end;
    int fd;
    download_progress_cb progress_cb;
    void *progress_ud;
    size_t *total_downloaded;
    pthread_mutex_t *mutex;
    int success;
} SegmentTask;

/* Build a GET request with an exact byte range (start-end), unlike
 * request_build() which only supports an open-ended "start-" range. */
static int build_range_request(const char *host, const char *path,
                               size_t start, size_t end,
                               char *buf, size_t buf_size)
{
    int n = snprintf(buf, buf_size,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: BDownloader/0.1\r\n"
        "Range: bytes=%zu-%zu\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        path, host, start, end);

    if (n < 0 || (size_t)n >= buf_size)
        return -1;
    return 0;
}

static void *segment_worker(void *arg)
{
    SegmentTask *task = (SegmentTask *)arg;
    int use_ssl = (strcmp(task->url->protocol, "https") == 0);

    Transport *t = pool_acquire(task->url->host, task->url->port, use_ssl);
    if (!t) {
        fprintf(stderr, "[segment %d] Connection failed\n", task->id);
        return NULL;
    }

    char req_buf[2048];
    if (build_range_request(task->url->host, task->url->path,
                            task->start, task->end, req_buf, sizeof(req_buf)) != 0) {
        fprintf(stderr, "[segment %d] Failed to build request\n", task->id);
        return NULL;
    }

    size_t req_len = strlen(req_buf);
    if (transport_send(t, req_buf, req_len) != (ssize_t)req_len) {
        fprintf(stderr, "[segment %d] Send failed\n", task->id);
        return NULL;
    }

    char header_buf[4096];
    char *body_ptr = NULL;
    size_t initial_body_len = 0;

    if (headers_read(t, header_buf, sizeof(header_buf), &body_ptr, &initial_body_len) != 0) {
        fprintf(stderr, "[segment %d] Headers read failed\n", task->id);
        return NULL;
    }

    HttpHeaders hdr;
    if (headers_parse(header_buf, &hdr) != 0) {
        fprintf(stderr, "[segment %d] Headers parse failed\n", task->id);
        return NULL;
    }

    if (hdr.status_code != 206) {
        fprintf(stderr, "[segment %d] Expected 206, got %d\n", task->id, hdr.status_code);
        return NULL;
    }

    size_t remaining = hdr.content_length;
    size_t write_offset = task->start;

    if (initial_body_len > 0) {
        size_t to_write = initial_body_len < remaining ? initial_body_len : remaining;
        if (pwrite(task->fd, body_ptr, to_write, write_offset) != (ssize_t)to_write) {
            fprintf(stderr, "[segment %d] pwrite failed\n", task->id);
            return NULL;
        }
        write_offset += to_write;
        remaining -= to_write;

        pthread_mutex_lock(task->mutex);
        *task->total_downloaded += to_write;
        if (task->progress_cb)
            task->progress_cb(*task->total_downloaded, 0, task->progress_ud);
        pthread_mutex_unlock(task->mutex);
    }

    char buf[4096];
    while (remaining > 0) {
        size_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        ssize_t n = transport_recv(t, buf, to_read);
        if (n <= 0)
            break;
        if (pwrite(task->fd, buf, n, write_offset) != n)
            break;
        write_offset += n;
        remaining -= n;

        pthread_mutex_lock(task->mutex);
        *task->total_downloaded += n;
        if (task->progress_cb)
            task->progress_cb(*task->total_downloaded, 0, task->progress_ud);
        pthread_mutex_unlock(task->mutex);
    }

    task->success = (remaining == 0);
    return NULL;
}

int segmented_download_init(SegmentedDownloader *sd)
{
    memset(sd, 0, sizeof(*sd));
    sd->segments = 4;
    return 0;
}

int segmented_download_file(SegmentedDownloader *sd, const URL *url)
{
    DownloadInfo info;
    if (download_info(url, &info) != 0)
        return -1;

    if (!info.accept_ranges || info.content_length == 0) {
        fprintf(stderr, "[segmented] Server doesn't support ranges or size unknown; falling back to single-threaded\n");
        Downloader dl;
        download_init(&dl);
        dl.on_progress = sd->on_progress;
        dl.on_progress_ud = sd->on_progress_ud;
        int rc = download_file(&dl, url);
        download_destroy(&dl);
        return rc;
    }

    const char *filename = url_filename(url);
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -1;

    if (ftruncate(fd, info.content_length) != 0) {
        close(fd);
        return -1;
    }

    int num_segments = sd->segments;
    size_t total_size = info.content_length;
    size_t seg_size = total_size / num_segments;

    pthread_t *threads = calloc(num_segments, sizeof(pthread_t));
    SegmentTask *tasks = calloc(num_segments, sizeof(SegmentTask));
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    size_t total_downloaded = 0;

    for (int i = 0; i < num_segments; i++) {
        tasks[i].id = i;
        tasks[i].url = url;
        tasks[i].start = i * seg_size;
        tasks[i].end = (i == num_segments - 1) ? total_size - 1 : (i + 1) * seg_size - 1;
        tasks[i].fd = fd;
        tasks[i].progress_cb = sd->on_progress;
        tasks[i].progress_ud = sd->on_progress_ud;
        tasks[i].total_downloaded = &total_downloaded;
        tasks[i].mutex = &mutex;
        tasks[i].success = 0;

        pthread_create(&threads[i], NULL, segment_worker, &tasks[i]);
    }

    int all_success = 1;
    for (int i = 0; i < num_segments; i++) {
        pthread_join(threads[i], NULL);
        if (!tasks[i].success)
            all_success = 0;
    }

    close(fd);
    free(threads);
    free(tasks);

    if (all_success) {
        printf("\nDownloaded: %s\n", filename);
        printf("Size: %zu bytes\n", total_size);
    } else {
        fprintf(stderr, "\n[segmented] One or more segments failed\n");
    }

    return all_success ? 0 : -1;
}
