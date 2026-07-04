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
#include <zlib.h>
#include "pool.h"

typedef struct {
    FILE *fp;
    int use_zlib;
    z_stream strm;
    char out_buf[16384];
    Downloader *dl;
    int error;
} WriteContext;

static ssize_t generic_write_fn(void *ctx, const char *buf, size_t len)
{
    WriteContext *w = (WriteContext *)ctx;
    if (w->error) return -1;

    if (!w->use_zlib) {
        if (fwrite(buf, 1, len, w->fp) != len) {
            snprintf(w->dl->error_message, sizeof(w->dl->error_message), "File write failed");
            w->error = 1;
            return -1;
        }
        return len;
    }

    w->strm.next_in = (Bytef *)buf;
    w->strm.avail_in = len;

    while (w->strm.avail_in > 0) {
        w->strm.next_out = (Bytef *)w->out_buf;
        w->strm.avail_out = sizeof(w->out_buf);

        int ret = inflate(&w->strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
            snprintf(w->dl->error_message, sizeof(w->dl->error_message), "zlib inflate error: %d", ret);
            w->error = 1;
            return -1;
        }

        size_t have = sizeof(w->out_buf) - w->strm.avail_out;
        if (have > 0) {
            if (fwrite(w->out_buf, 1, have, w->fp) != have) {
                snprintf(w->dl->error_message, sizeof(w->dl->error_message), "File write error during decompression");
                w->error = 1;
                return -1;
            }
        }

        if (ret == Z_STREAM_END)
            break;
    }

    return len;
}

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
    if (t->use_proxy && !use_ssl) {
        /* HTTP via proxy: use full URL in request */
        char full_url[1536];
        snprintf(full_url, sizeof(full_url), "http://%s:%d%s", url->host, url->port, url->path);
        if (request_build_proxy("HEAD", full_url, 0, req_buf, sizeof(req_buf)) != 0) {
            pool_release(t, url->host, url->port, use_ssl, 0);
            return -1;
        }
    } else {
        /* Direct connection or HTTPS via CONNECT tunnel */
        if (request_build("HEAD", url->host, url->path, 0, req_buf, sizeof(req_buf)) != 0) {
            pool_release(t, url->host, url->port, use_ssl, 0);
            return -1;
        }
    }

    size_t req_len = strlen(req_buf);
    if (transport_send(t, req_buf, req_len) != (ssize_t)req_len) {
        pool_release(t, url->host, url->port, use_ssl, 0);
        return -1;
    }

    char header_buf[4096];
    char *body_ptr = NULL;
    size_t initial_body_len = 0;

    if (headers_read(t, header_buf, sizeof(header_buf), &body_ptr, &initial_body_len) != 0) {
        pool_release(t, url->host, url->port, use_ssl, 0);
        return -1;
    }

    HttpHeaders hdr;
    if (headers_parse(header_buf, &hdr) != 0) {
        pool_release(t, url->host, url->port, use_ssl, 0);
        return -1;
    }

    /* HEAD responses never carry a body, so there's nothing left to
     * drain: safe to keep the connection alive unless the server
     * explicitly asked to close it. */

    info->status_code = hdr.status_code;
    info->content_length = hdr.content_length;
    info->accept_ranges = hdr.accept_ranges;
    info->chunked = hdr.chunked;
    info->content_encoding = hdr.content_encoding;
    strncpy(info->content_type, hdr.content_type, sizeof(info->content_type) - 1);
    info->content_type[sizeof(info->content_type) - 1] = '\0';
    strncpy(info->last_modified, hdr.last_modified, sizeof(info->last_modified) - 1);
    info->last_modified[sizeof(info->last_modified) - 1] = '\0';

    pool_release(t, url->host, url->port, use_ssl, !hdr.connection_close);
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
        snprintf(dl->error_message, sizeof(dl->error_message), "Too many redirects");
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
        snprintf(dl->error_message, sizeof(dl->error_message), "Connection failed (host=%s port=%d ssl=%d)", url->host, url->port, use_ssl);
        return -1;
    }

    size_t range_start = retry_without_range ? 0 : local_size;

    char req_buf[2048];
    if (t->use_proxy && !use_ssl) {
        char full_url[1536];
        snprintf(full_url, sizeof(full_url), "http://%s:%d%s", url->host, url->port, url->path);
        if (request_build_proxy("GET", full_url, range_start, req_buf, sizeof(req_buf)) != 0) {
            pool_release(t, url->host, url->port, use_ssl, 0);
            snprintf(dl->error_message, sizeof(dl->error_message), "Failed to build proxy request");
            return -1;
        }
    } else {
        if (request_build("GET", url->host, url->path, range_start, req_buf, sizeof(req_buf)) != 0) {
            pool_release(t, url->host, url->port, use_ssl, 0);
            snprintf(dl->error_message, sizeof(dl->error_message), "Failed to build request");
            return -1;
        }
    }

    size_t req_len = strlen(req_buf);
    if (transport_send(t, req_buf, req_len) != (ssize_t)req_len) {
        snprintf(dl->error_message, sizeof(dl->error_message), "Send request failed");
        pool_release(t, url->host, url->port, use_ssl, 0);
        return -1;
    }

    char header_buf[4096];
    char *body_ptr = NULL;
    size_t initial_body_len = 0;

    if (headers_read(t, header_buf, sizeof(header_buf), &body_ptr, &initial_body_len) != 0) {
        snprintf(dl->error_message, sizeof(dl->error_message), "Failed to read response headers");
        pool_release(t, url->host, url->port, use_ssl, 0);
        return -1;
    }

    HttpHeaders hdr;
    if (headers_parse(header_buf, &hdr) != 0) {
        snprintf(dl->error_message, sizeof(dl->error_message), "Failed to parse response headers");
        pool_release(t, url->host, url->port, use_ssl, 0);
        return -1;
    }

    if (is_redirect(hdr.status_code)) {
        /* Body (if any) isn't drained here, so the connection can't be
         * safely reused; always close it. */
        pool_release(t, url->host, url->port, use_ssl, 0);

        char location[1024];
        if (parse_location(header_buf, location, sizeof(location)) != 0) {
            snprintf(dl->error_message, sizeof(dl->error_message), "Redirect without Location header");
            return -1;
        }

        URL new_url;
        if (url_parse(location, &new_url) != 0) {
            if (location[0] == '/') {
                new_url = *url;
                strncpy(new_url.path, location, sizeof(new_url.path) - 1);
                new_url.path[sizeof(new_url.path) - 1] = '\0';
            } else {
                snprintf(dl->error_message, sizeof(dl->error_message), "Invalid redirect URL: %s", location);
                return -1;
            }
        }

        printf("Redirect (%d) -> %s\n", hdr.status_code, location);
        return download_file_internal(dl, &new_url, redirect_count + 1);
    }

    if (hdr.status_code == 416) {
        pool_release(t, url->host, url->port, use_ssl, 0);
        if (local_size > 0) {
            printf("Range not satisfiable, restarting from zero\n");
            unlink(filename);
            local_size = 0;
            retry_without_range = 1;
            goto retry;
        }
        return -1;
    }

    if (hdr.status_code != 200 && hdr.status_code != 206) {
        snprintf(dl->error_message, sizeof(dl->error_message), "Unexpected HTTP status code: %d", hdr.status_code);
        pool_release(t, url->host, url->port, use_ssl, 0);
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
        snprintf(dl->error_message, sizeof(dl->error_message), "Failed to open output file: %s", filename);
        pool_release(t, url->host, url->port, use_ssl, 0);
        return -1;
    }

    WriteContext wctx;
    memset(&wctx, 0, sizeof(wctx));
    wctx.fp = fp;
    wctx.dl = dl;

    if (hdr.content_encoding == 1 || hdr.content_encoding == 2) {
        wctx.use_zlib = 1;
        wctx.strm.zalloc = Z_NULL;
        wctx.strm.zfree = Z_NULL;
        wctx.strm.opaque = Z_NULL;
        /* 32 + MAX_WBITS enables automatic header detection for gzip or zlib */
        if (inflateInit2(&wctx.strm, 32 + 15) != Z_OK) {
            snprintf(dl->error_message, sizeof(dl->error_message), "Failed to initialize zlib");
            fclose(fp);
            pool_release(t, url->host, url->port, use_ssl, 0);
            return -1;
        }
    }

    size_t written = range_start;
    int reusable = 0;

    if (hdr.chunked) {
        size_t chunk_written = 0;
        if (chunked_decode_stream(t, body_ptr, initial_body_len,
                                  generic_write_fn, &wctx, &chunk_written,
                                  dl->on_progress, dl->on_progress_ud,
                                  range_start, total_size) != 0) {
            if (!dl->error_message[0])
                snprintf(dl->error_message, sizeof(dl->error_message), "Chunked decode failed");
            goto fail;
        }
        written += chunk_written;
        reusable = !hdr.connection_close;
    } else if (hdr.content_length > 0) {
        size_t to_write = initial_body_len;
        if (to_write > hdr.content_length)
            to_write = hdr.content_length;

        if (to_write > 0) {
            if (generic_write_fn(&wctx, body_ptr, to_write) != (ssize_t)to_write)
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
            if (generic_write_fn(&wctx, buf, n) != (ssize_t)n)
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
            if (generic_write_fn(&wctx, body_ptr, initial_body_len) != (ssize_t)initial_body_len)
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
            if (generic_write_fn(&wctx, buf, n) != (ssize_t)n)
                goto fail;
            written += n;
            report_progress(dl, written, total_size);
        }
        reusable = 0;
    }

    if (wctx.use_zlib)
        inflateEnd(&wctx.strm);
    fclose(fp);
    pool_release(t, url->host, url->port, use_ssl, reusable);
    return 0;

fail:
    if (wctx.use_zlib)
        inflateEnd(&wctx.strm);
    if (!dl->error_message[0])
        snprintf(dl->error_message, sizeof(dl->error_message), "Network or file I/O error during download");
    fclose(fp);
    pool_release(t, url->host, url->port, use_ssl, 0);
    return -1;
}

void download_destroy(Downloader *dl)
{
    (void)dl;
}
