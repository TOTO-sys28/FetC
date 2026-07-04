#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "request.h"

static void test_request_build_get(void)
{
    char buf[2048];
    assert(request_build("GET", "example.com", "/file.zip", 0, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "GET /file.zip HTTP/1.1") != NULL);
    assert(strstr(buf, "Host: example.com") != NULL);
    assert(strstr(buf, "Connection: keep-alive") != NULL);
    printf("  test_request_build_get: PASS\n");
}

static void test_request_build_range(void)
{
    char buf[2048];
    assert(request_build("GET", "example.com", "/file.zip", 1024, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "Range: bytes=1024-") != NULL);
    printf("  test_request_build_range: PASS\n");
}

static void test_request_build_head(void)
{
    char buf[2048];
    assert(request_build("HEAD", "example.com", "/", 0, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "HEAD / HTTP/1.1") != NULL);
    printf("  test_request_build_head: PASS\n");
}

int main(void)
{
    printf("test_request:\n");
    test_request_build_get();
    test_request_build_range();
    test_request_build_head();
    printf("All request tests passed.\n\n");
    return 0;
}
