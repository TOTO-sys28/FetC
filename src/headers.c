#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include "headers.h"

int headers_read(Transport *t, char *buf, size_t buf_size, char **body_ptr, size_t *initial_body_len)
{
    size_t total = 0;

    while (total < buf_size) {
        ssize_t n = transport_recv(t, buf + total, buf_size - total);
        if (n <= 0)
            return -1;
        total += n;

        for (size_t i = 3; i < total; i++) {
            if (buf[i-3] == '\r' && buf[i-2] == '\n' &&
                buf[i-1] == '\r' && buf[i] == '\n') {
                *body_ptr = buf + i + 1;
                *initial_body_len = total - (i + 1);
                buf[i-3] = '\0';
                return 0;
            }
        }
    }

    return -1;
}

static void extract_header_value(const char *src, size_t skip, char *dst, size_t dst_size)
{
    const char *p = src + skip;
    while (*p == ' ' || *p == '\t')
        p++;
    const char *end = strchr(p, '\r');
    if (!end) end = strchr(p, '\n');
    if (!end) end = p + strlen(p);
    size_t len = end - p;
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, p, len);
    dst[len] = '\0';
}

int headers_parse(const char *headers_text, HttpHeaders *out)
{
    memset(out, 0, sizeof(*out));

    const char *p = headers_text;
    while (*p && *p != ' ')
        p++;
    if (*p == ' ') {
        p++;
        out->status_code = atoi(p);
    }

    const char *cl = strstr(headers_text, "Content-Length:");
    if (cl) {
        cl += 15;
        while (*cl == ' ' || *cl == '\t')
            cl++;
        out->content_length = (size_t)atoll(cl);
    }

    if (strstr(headers_text, "Transfer-Encoding: chunked"))
        out->chunked = 1;

    const char *conn = strstr(headers_text, "Connection:");
    if (conn) {
        conn += 11;
        while (*conn == ' ' || *conn == '\t')
            conn++;
        if (strncasecmp(conn, "close", 5) == 0)
            out->connection_close = 1;
    }

    const char *ar = strstr(headers_text, "Accept-Ranges:");
    if (ar) {
        ar += 14;
        while (*ar == ' ' || *ar == '\t')
            ar++;
        if (strncasecmp(ar, "bytes", 5) == 0)
            out->accept_ranges = 1;
    }

    const char *cr = strstr(headers_text, "Content-Range:");
    if (cr) {
        cr += 14;
        while (*cr == ' ' || *cr == '\t')
            cr++;

        if (strncasecmp(cr, "bytes", 5) == 0) {
            cr += 5;
            while (*cr == ' ' || *cr == '\t')
                cr++;

            out->range_start = (size_t)atoll(cr);

            const char *dash = strchr(cr, '-');
            if (dash) {
                out->range_end = (size_t)atoll(dash + 1);
                const char *slash = strchr(dash, '/');
                if (slash && *(slash + 1) != '*') {
                    out->range_total = (size_t)atoll(slash + 1);
                }
            }
        }
    }

    const char *ct = strstr(headers_text, "Content-Type:");
    if (ct)
        extract_header_value(ct, 13, out->content_type, sizeof(out->content_type));

    const char *lm = strstr(headers_text, "Last-Modified:");
    if (lm)
        extract_header_value(lm, 14, out->last_modified, sizeof(out->last_modified));

    return 0;
}
