#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "chunked.h"

/* Mock reader: feeds a fixed in-memory buffer, byte-limited per call
 * to also exercise the "partial read" code paths (max_chunk). */
typedef struct {
    const char *data;
    size_t len;
    size_t pos;
    size_t max_chunk; /* 0 = no limit */
} MockReader;

static ssize_t mock_read(void *ctx, char *buf, size_t len)
{
    MockReader *m = (MockReader *)ctx;
    if (m->pos >= m->len)
        return 0; /* EOF */
    size_t avail = m->len - m->pos;
    size_t to_copy = avail < len ? avail : len;
    if (m->max_chunk > 0 && to_copy > m->max_chunk)
        to_copy = m->max_chunk;
    memcpy(buf, m->data + m->pos, to_copy);
    m->pos += to_copy;
    return (ssize_t)to_copy;
}

/* Reads the whole temp file into buf (NUL-terminated), for assertions. */
static size_t read_temp_file(const char *path, char *buf, size_t buf_size)
{
    FILE *fp = fopen(path, "rb");
    assert(fp != NULL);
    size_t n = fread(buf, 1, buf_size - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    return n;
}

static ssize_t mock_write(void *ctx, const char *buf, size_t len)
{
    FILE *fp = (FILE *)ctx;
    return fwrite(buf, 1, len, fp);
}

static void test_chunked_basic(void)
{
    const char *raw =
        "5\r\nHello\r\n"
        "6\r\n World\r\n"
        "0\r\n\r\n";

    MockReader reader = { raw, strlen(raw), 0, 0 };

    char out_path[] = "/tmp/test_chunked_basic_XXXXXX";
    int fd = mkstemp(out_path);
    assert(fd >= 0);
    FILE *fp = fdopen(fd, "wb");
    assert(fp != NULL);

    size_t written = 0;
    int rc = chunked_decode_stream_ex(mock_read, &reader, NULL, 0,
                                      mock_write, fp, &written, NULL, NULL, 0, 0);
    fclose(fp);

    assert(rc == 0);
    assert(written == 11);

    char buf[32];
    size_t n = read_temp_file(out_path, buf, sizeof(buf));
    unlink(out_path);

    assert(n == 11);
    assert(strcmp(buf, "Hello World") == 0);
    printf("  test_chunked_basic: PASS\n");
}

static void test_chunked_with_initial_buffer(void)
{
    /* Simulate data already buffered from the initial HTTP read
     * (as happens in real usage when headers + first chunk arrive
     * in the same TCP segment). */
    const char *initial = "3\r\nfoo\r\n";
    const char *rest = "3\r\nbar\r\n0\r\n\r\n";

    MockReader reader = { rest, strlen(rest), 0, 0 };

    char out_path[] = "/tmp/test_chunked_initial_XXXXXX";
    int fd = mkstemp(out_path);
    assert(fd >= 0);
    FILE *fp = fdopen(fd, "wb");
    assert(fp != NULL);

    size_t written = 0;
    int rc = chunked_decode_stream_ex(mock_read, &reader,
                                      initial, strlen(initial),
                                      mock_write, fp, &written, NULL, NULL, 0, 0);
    fclose(fp);

    assert(rc == 0);
    assert(written == 6);

    char buf[16];
    read_temp_file(out_path, buf, sizeof(buf));
    unlink(out_path);

    assert(strcmp(buf, "foobar") == 0);
    printf("  test_chunked_with_initial_buffer: PASS\n");
}

static void test_chunked_split_reads(void)
{
    /* Force the mock reader to only return 1 byte per call, exercising
     * partial-read accumulation logic (stream_read_line, stream_read_exact). */
    const char *raw = "4\r\ntest\r\n0\r\n\r\n";
    MockReader reader = { raw, strlen(raw), 0, 1 };

    char out_path[] = "/tmp/test_chunked_split_XXXXXX";
    int fd = mkstemp(out_path);
    assert(fd >= 0);
    FILE *fp = fdopen(fd, "wb");
    assert(fp != NULL);

    size_t written = 0;
    int rc = chunked_decode_stream_ex(mock_read, &reader, NULL, 0,
                                      mock_write, fp, &written, NULL, NULL, 0, 0);
    fclose(fp);

    assert(rc == 0);
    assert(written == 4);

    char buf[16];
    read_temp_file(out_path, buf, sizeof(buf));
    unlink(out_path);

    assert(strcmp(buf, "test") == 0);
    printf("  test_chunked_split_reads: PASS\n");
}

typedef struct {
    size_t last_current;
    size_t call_count;
} ProgressCtx;

static void progress_cb(size_t current, size_t total, void *user_data)
{
    (void)total;
    ProgressCtx *ctx = (ProgressCtx *)user_data;
    ctx->last_current = current;
    ctx->call_count++;
}

static void test_chunked_progress_callback(void)
{
    const char *raw = "5\r\nhello\r\n0\r\n\r\n";
    MockReader reader = { raw, strlen(raw), 0, 0 };

    char out_path[] = "/tmp/test_chunked_progress_XXXXXX";
    int fd = mkstemp(out_path);
    assert(fd >= 0);
    FILE *fp = fdopen(fd, "wb");
    assert(fp != NULL);

    ProgressCtx ctx = {0, 0};

    size_t written = 0;
    int rc = chunked_decode_stream_ex(mock_read, &reader, NULL, 0,
                                      mock_write, fp, &written, progress_cb, &ctx, 100, 200);
    fclose(fp);
    unlink(out_path);

    assert(rc == 0);
    assert(written == 5);
    assert(ctx.call_count >= 1);
    assert(ctx.last_current == 105); /* offset(100) + written(5) */
    printf("  test_chunked_progress_callback: PASS\n");
}

static void test_chunked_malformed_no_terminator(void)
{
    /* Missing final "0\r\n\r\n" terminator: should fail cleanly, not hang or crash */
    const char *raw = "3\r\nabc\r\n";
    MockReader reader = { raw, strlen(raw), 0, 0 };

    char out_path[] = "/tmp/test_chunked_malformed_XXXXXX";
    int fd = mkstemp(out_path);
    assert(fd >= 0);
    FILE *fp = fdopen(fd, "wb");
    assert(fp != NULL);

    size_t written = 0;
    int rc = chunked_decode_stream_ex(mock_read, &reader, NULL, 0,
                                      mock_write, fp, &written, NULL, NULL, 0, 0);
    fclose(fp);
    unlink(out_path);

    assert(rc == -1);
    printf("  test_chunked_malformed_no_terminator: PASS\n");
}

int main(void)
{
    printf("test_chunked:\n");
    test_chunked_basic();
    test_chunked_with_initial_buffer();
    test_chunked_split_reads();
    test_chunked_progress_callback();
    test_chunked_malformed_no_terminator();
    printf("All chunked tests passed.\n\n");
    return 0;
}
