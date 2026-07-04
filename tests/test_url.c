#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "fetc.h"

static void test_url_parse_basic(void)
{
    URL url;
    assert(url_parse("http://example.com/file.zip", &url) == 0);
    assert(strcmp(url.protocol, "http") == 0);
    assert(strcmp(url.host, "example.com") == 0);
    assert(strcmp(url.path, "/file.zip") == 0);
    assert(url.port == 80);
    printf("  test_url_parse_basic: PASS\n");
}

static void test_url_parse_https(void)
{
    URL url;
    assert(url_parse("https://example.com/path/to/file.tar.gz", &url) == 0);
    assert(strcmp(url.protocol, "https") == 0);
    assert(url.port == 443);
    assert(strcmp(url_filename(&url), "file.tar.gz") == 0);
    printf("  test_url_parse_https: PASS\n");
}

static void test_url_parse_no_path(void)
{
    URL url;
    assert(url_parse("http://example.com", &url) == 0);
    assert(strcmp(url.path, "/") == 0);
    assert(strcmp(url_filename(&url), "index.html") == 0);
    printf("  test_url_parse_no_path: PASS\n");
}

static void test_url_parse_invalid(void)
{
    URL url;
    assert(url_parse("not-a-url", &url) == -1);
    printf("  test_url_parse_invalid: PASS\n");
}

static void test_url_filename(void)
{
    URL url;
    url_parse("http://example.com/ubuntu.iso", &url);
    assert(strcmp(url_filename(&url), "ubuntu.iso") == 0);

    url_parse("http://example.com/", &url);
    assert(strcmp(url_filename(&url), "index.html") == 0);

    url_parse("http://example.com/dir/", &url);
    assert(strcmp(url_filename(&url), "index.html") == 0);
    printf("  test_url_filename: PASS\n");
}

int main(void)
{
    printf("test_url:\n");
    test_url_parse_basic();
    test_url_parse_https();
    test_url_parse_no_path();
    test_url_parse_invalid();
    test_url_filename();
    printf("All URL tests passed.\n\n");
    return 0;
}
