#ifndef CHUNKED_H
#define CHUNKED_H

#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include "transport.h"

typedef void (*chunked_progress_cb)(size_t current, size_t total, void *user_data);

/* Generic byte-reader callback: same signature/semantics as recv() —
 * returns bytes read (>0), 0 on EOF, <0 on error. Used to decouple
 * the chunked-decoding logic from any specific transport, which makes
 * it independently unit-testable with a mock reader. */
typedef ssize_t (*chunked_read_fn)(void *ctx, char *buf, size_t len);

int chunked_decode_stream_ex(chunked_read_fn read_fn, void *read_ctx,
                             const char *initial, size_t initial_len,
                             FILE *output, size_t *total_written,
                             chunked_progress_cb cb, void *user_data,
                             size_t offset, size_t total);

/* Convenience wrapper used by production code: reads from a real Transport. */
int chunked_decode_stream(Transport *t, const char *initial, size_t initial_len,
                          FILE *output, size_t *total_written,
                          chunked_progress_cb cb, void *user_data,
                          size_t offset, size_t total);

#endif
