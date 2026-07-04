#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "chunked.h"

typedef struct {
    chunked_read_fn read_fn;
    void *read_ctx;
    const char *buf;
    size_t len;
    size_t pos;
} Stream;

static ssize_t stream_read(Stream *s, char *buf, size_t len)
{
    if (s->pos < s->len) {
        size_t avail = s->len - s->pos;
        size_t to_copy = avail < len ? avail : len;
        memcpy(buf, s->buf + s->pos, to_copy);
        s->pos += to_copy;
        return (ssize_t)to_copy;
    }
    return s->read_fn(s->read_ctx, buf, len);
}

static int stream_read_exact(Stream *s, char *buf, size_t len)
{
    size_t total = 0;
    while (total < len) {
        ssize_t n = stream_read(s, buf + total, len - total);
        if (n <= 0)
            return -1;
        total += n;
    }
    return 0;
}

static int stream_read_line(Stream *s, char *line, size_t max)
{
    size_t i = 0;
    while (i < max - 1) {
        char c;
        if (stream_read(s, &c, 1) != 1)
            return -1;
        line[i++] = c;
        if (i >= 2 && line[i-2] == '\r' && line[i-1] == '\n') {
            line[i] = '\0';
            return 0;
        }
    }
    return -1;
}

static size_t parse_hex(const char *s)
{
    size_t val = 0;
    while (*s) {
        char c = *s++;
        if (c >= '0' && c <= '9')
            val = val * 16 + (c - '0');
        else if (c >= 'a' && c <= 'f')
            val = val * 16 + (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            val = val * 16 + (c - 'A' + 10);
        else
            break;
    }
    return val;
}

int chunked_decode_stream_ex(chunked_read_fn read_fn, void *read_ctx,
                             const char *initial, size_t initial_len,
                             FILE *output, size_t *total_written,
                             chunked_progress_cb cb, void *user_data,
                             size_t offset, size_t total)
{
    Stream s = {read_fn, read_ctx, initial, initial_len, 0};
    size_t written = 0;

    while (1) {
        char line[64];
        if (stream_read_line(&s, line, sizeof(line)) != 0)
            return -1;

        size_t chunk_size = parse_hex(line);
        if (chunk_size == 0)
            break;

        char buf[4096];
        size_t remaining = chunk_size;
        while (remaining > 0) {
            size_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
            ssize_t n = stream_read(&s, buf, to_read);
            if (n <= 0)
                return -1;
            if (fwrite(buf, 1, n, output) != (size_t)n)
                return -1;
            written += n;
            remaining -= n;
            if (cb)
                cb(offset + written, total, user_data);
        }

        char crlf[2];
        if (stream_read_exact(&s, crlf, 2) != 0)
            return -1;
        if (crlf[0] != '\r' || crlf[1] != '\n')
            return -1;
    }

    while (1) {
        char line[4096];
        if (stream_read_line(&s, line, sizeof(line)) != 0)
            return -1;
        if (strlen(line) == 2 && line[0] == '\r')
            break;
    }

    *total_written = written;
    return 0;
}

static ssize_t transport_read_adapter(void *ctx, char *buf, size_t len)
{
    return transport_recv((Transport *)ctx, buf, len);
}

int chunked_decode_stream(Transport *t, const char *initial, size_t initial_len,
                          FILE *output, size_t *total_written,
                          chunked_progress_cb cb, void *user_data,
                          size_t offset, size_t total)
{
    return chunked_decode_stream_ex(transport_read_adapter, t,
                                    initial, initial_len,
                                    output, total_written,
                                    cb, user_data, offset, total);
}
