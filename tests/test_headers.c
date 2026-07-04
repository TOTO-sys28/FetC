#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "headers.h"

static void test_headers_parse_basic(void)
{
    const char *raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 1024\r\n"
        "Content-Type: text/html\r\n"
        "Accept-Ranges: bytes\r\n"
        "Last-Modified: Wed, 01 Jul 2026 17:52:37 GMT\r\n"
        "\r\n";

    HttpHeaders hdr;
    assert(headers_parse(raw, &hdr) == 0);
    assert(hdr.status_code == 200);
    assert(hdr.content_length == 1024);
    assert(hdr.accept_ranges == 1);
    assert(strcmp(hdr.content_type, "text/html") == 0);
    printf("  test_headers_parse_basic: PASS\n");
}

static void test_headers_parse_chunked(void)
{
    const char *raw =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n";

    HttpHeaders hdr;
    assert(headers_parse(raw, &hdr) == 0);
    assert(hdr.chunked == 1);
    assert(hdr.connection_close == 1);
    assert(hdr.content_length == 0);
    printf("  test_headers_parse_chunked: PASS\n");
}

static void test_headers_parse_range(void)
{
    const char *raw =
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Range: bytes 1000-1999/5000\r\n"
        "Content-Length: 1000\r\n"
        "\r\n";

    HttpHeaders hdr;
    assert(headers_parse(raw, &hdr) == 0);
    assert(hdr.status_code == 206);
    assert(hdr.range_start == 1000);
    assert(hdr.range_end == 1999);
    assert(hdr.range_total == 5000);
    printf("  test_headers_parse_range: PASS\n");
}

static void test_headers_parse_redirect(void)
{
    const char *raw =
        "HTTP/1.1 302 Found\r\n"
        "Location: /new-path\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    HttpHeaders hdr;
    assert(headers_parse(raw, &hdr) == 0);
    assert(hdr.status_code == 302);
    printf("  test_headers_parse_redirect: PASS\n");
}

int main(void)
{
    printf("test_headers:\n");
    test_headers_parse_basic();
    test_headers_parse_chunked();
    test_headers_parse_range();
    test_headers_parse_redirect();
    printf("All headers tests passed.\n\n");
    return 0;
}
