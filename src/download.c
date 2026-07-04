#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

int download_init(Downloader *dl)
{
    memset(dl, 0, sizeof(*dl));
    return 0;
}

static void report_progress(Downloader *dl, size_t current, size_t total)
{
    if (!dl->on_progress)
        return;

    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return;

    if (dl->_last_progress_time.tv_sec != 0) {
        double elapsed = (now.tv_sec - dl->_last_progress_time.tv_sec) +
                         (now.tv_nsec - dl->_last_progress_time.tv_nsec) / 1e9;
        if (elapsed < 0.1 && (current - dl->_last_progress_bytes) < (64 * 1024))
            return;
    }

    dl->_last_progress_time = now;
    dl->_last_progress_bytes = current;
    dl->on_progress(current, total, dl->on_progress_ud);
}

int download_info(const URL *url, DownloadInfo *info)
{
    int use_ssl = (strcmp(url->protocol, "https") == 0);
    Transport *t = pool_acquire(url->host, url->port, use_ssl);
    if (!t)
        return -1;

    char req_buf[2048];
    if (request_build("HEAD", url->host, url->path, 0, req_buf, sizeof(req_buf)) != 0) {
        return -1;
    }

    size_t req_len = strlen(req_buf);
    if (transport_send(t, req_buf, req_len) != (ssize_t)req_len) {
        return -1;
    }

    char header_buf[4096];
    char *body_ptr = NULL;
    size_t initial_body_len = 0;

    if (headers_read(t, header_buf, sizeof(header_buf), &body_ptr, &initial_body_len) != 0) {
        return -1;
    }

    HttpHeaders hdr;
    if (headers_parse(header_buf, &hdr) != 0) {
        return -1;
    }

    /* HEAD responses never carry a body, so there's nothing left to
     * drain: safe to keep the connection alive unless the server
     * explicitly asked to close it. */

    info->status_code = hdr.status_code;
    info->content_length = hdr.content_length;
    info->accept_ranges = hdr.accept_ranges;
    info->chunked = hdr.chunked;
    strncpy(info->content_type, hdr.content_type, sizeof(info->content_type) - 1);
    info->content_type[sizeof(info->content_type) - 1] = '\0';
    strncpy(info->last_modified, hdr.last_modified, sizeof(info->last_modified) - 1);
    info->last_modified[sizeof(info->last_modified) - 1] = '\0';

    return 0;
}

static int is_redirect(int status)
{
    return status == 301 || status == 302 || status == 307 || status == 308;
}

static int parse_location(const char *headers, char *out, size_t out_size)
{
    const char *loc = strstr(headers, "Location:");
    if (!loc) loc = strstr(headers, "location:");
    if (!loc) return -1;

    loc += 9;
    while (*loc == ' ' || *loc == '\t')
        loc++;

    const char *end = strchr(loc, '\r');
    if (!end) end = strchr(loc, '\n');
    if (!end) end = loc + strlen(loc);

    size_t len = end - loc;
    if (len >= out_size) len = out_size - 1;
    memcpy(out, loc, len);
    out[len] = '\0';

    while (len > 0 && (out[len-1] == ' ' || out[len-1] == '\t')) {
        out[len-1] = '\0';
        len--;
    }

    return 0;
}

static int download_file_internal(Downloader *dl, const URL *url, int redirect_count);

int download_file(Downloader *dl, const URL *url)
{
    return download_file_internal(dl, url, 0);
}

static int download_file_internal(Downloader *dl, const URL *url, int redirect_count)
{
    if (redirect_count > 5) {
        fprintf(stderr, "Too many redirects\n");
        return -1;
    }

    const char *filename = url_filename(url);
    size_t local_size = 0;
    resume_get_local_size(filename, &local_size);

    /* Only resume if:
     * - local file exists
     * - HEAD request succeeds
     * - server has Content-Length (not chunked)
     * - server supports ranges
     * - local size < total size
     * (Thanks to keep-alive, this HEAD check and the GET below will
     * often reuse the very same TCP/TLS connection.) */
    if (local_size > 0) {
        DownloadInfo info;
        if (download_info(url, &info) == 0 &&
            info.content_length > 0 &&
            !info.chunked &&
            info.accept_ranges) {
            if (local_size >= info.content_length) {
                printf("File already complete: %s (%zu bytes)\n", filename, local_size);
                return 0;
            }
        } else {
            /* Can't verify: start fresh */
            local_size = 0;
        }
    }

    int use_ssl = (strcmp(url->protocol, "https") == 0);
    int retry_without_range = 0;

retry: ;
    Transport *t = pool_acquire(url->host, url->port, use_ssl);
    if (!t) {
        fprintf(stderr, "Connection failed (host=%s port=%d ssl=%d)\n", url->host, url->port, use_ssl);
        return -1;
    }

    size_t range_start = retry_without_range ? 0 : local_size;

    char req_buf[2048];
    if (request_build("GET", url->host, url->path, range_start, req_buf, sizeof(req_buf)) != 0) {
        return -1;
    }

    size_t req_len = strlen(req_buf);
    if (transport_send(t, req_buf, req_len) != (ssize_t)req_len) {
        fprintf(stderr, "Send failed\n");
        return -1;
    }

    char header_buf[4096];
    char *body_ptr = NULL;
    size_t initial_body_len = 0;

    if (headers_read(t, header_buf, sizeof(header_buf), &body_ptr, &initial_body_len) != 0) {
        fprintf(stderr, "Headers read failed\n");
        return -1;
    }

    HttpHeaders hdr;
    if (headers_parse(header_buf, &hdr) != 0) {
        fprintf(stderr, "Headers parse failed\n");
        return -1;
    }

    if (is_redirect(hdr.status_code)) {
        /* Body (if any) isn't drained here, so the connection can't be
         * safely reused; always close it. */

        char location[1024];
        if (parse_location(header_buf, location, sizeof(location)) != 0) {
            fprintf(stderr, "Redirect without Location header\n");
            return -1;
        }

        URL new_url;
        if (url_parse(location, &new_url) != 0) {
            if (location[0] == '/') {
                new_url = *url;
                strncpy(new_url.path, location, sizeof(new_url.path) - 1);
                new_url.path[sizeof(new_url.path) - 1] = '\0';
            } else {
                fprintf(stderr, "Invalid redirect URL: %s\n", location);
                return -1;
            }
        }

        printf("Redirect (%d) -> %s\n", hdr.status_code, location);
        return download_file_internal(dl, &new_url, redirect_count + 1);
    }

    if (hdr.status_code == 416) {
        if (local_size > 0) {
            fprintf(stderr, "Range not satisfiable, restarting from zero\n");
            unlink(filename);
            local_size = 0;
            retry_without_range = 1;
            goto retry;
        }
        return -1;
    }

    if (hdr.status_code != 200 && hdr.status_code != 206) {
        fprintf(stderr, "Unexpected status code: %d\n", hdr.status_code);
        return -1;
    }

    size_t total_size;
    if (hdr.status_code == 206 && hdr.range_total > 0) {
        total_size = hdr.range_total;
    } else {
        total_size = hdr.content_length;
    }

    const char *mode = (range_start > 0 && hdr.status_code == 206) ? "ab" : "wb";
    FILE *fp = fopen(filename, mode);
    if (!fp) {
        return -1;
    }

    size_t written = range_start;
    int reusable = 0;

    if (hdr.chunked) {
        size_t chunk_written = 0;
        if (chunked_decode_stream(t, body_ptr, initial_body_len,
                                  fp, &chunk_written,
                                  dl->on_progress, dl->on_progress_ud,
                                  range_start, total_size) != 0) {
            fclose(fp);
            return -1;
        }
        written += chunk_written;
        reusable = !hdr.connection_close;
    } else if (hdr.content_length > 0) {
        size_t to_write = initial_body_len;
        if (to_write > hdr.content_length)
            to_write = hdr.content_length;

        if (to_write > 0) {
            if (file_write_chunk(fp, body_ptr, to_write) != 0)
                goto fail;
            written += to_write;
            report_progress(dl, written, total_size);
        }

        size_t remaining = hdr.content_length - to_write;
        char buf[4096];
        while (remaining > 0) {
            size_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
            ssize_t n = transport_recv(t, buf, to_read);
            if (n <= 0)
                goto fail;
            if (file_write_chunk(fp, buf, n) != 0)
                goto fail;
            written += n;
            remaining -= n;
            report_progress(dl, written, total_size);
        }
        reusable = !hdr.connection_close;
    } else {
        /* Body length is indeterminate (no Content-Length, not
         * chunked): the only way to know we've reached the end is the
         * peer closing the connection, so it can never be reused. */
        if (initial_body_len > 0) {
            if (file_write_chunk(fp, body_ptr, initial_body_len) != 0)
                goto fail;
            written += initial_body_len;
            report_progress(dl, written, total_size);
        }

        char buf[4096];
        while (1) {
            ssize_t n = transport_recv(t, buf, sizeof(buf));
            if (n < 0)
                goto fail;
            if (n == 0)
                break;
            if (file_write_chunk(fp, buf, n) != 0)
                goto fail;
            written += n;
            report_progress(dl, written, total_size);
        }
        reusable = 0;
    }

    fclose(fp);
    return 0;

fail:
    fclose(fp);
    return -1;
}

void download_destroy(Downloader *dl)
{
    (void)dl;
}
