#ifndef POOL_H
#define POOL_H

#include "transport.h"

/* Acquire a connection to host:port (ssl or not). Reuses an idle pooled
 * connection for the same host:port:ssl if one exists; otherwise opens
 * a fresh one. Returns NULL on connection failure. Thread-safe. */
Transport *pool_acquire(const char *host, int port, int use_ssl);

/* Return a connection after use. If keep_alive is non-zero, it is kept
 * idle in the pool for reuse by a future pool_acquire() call with the
 * same host:port:ssl. Otherwise it is closed and discarded. Thread-safe. */
void pool_release(Transport *t, const char *host, int port, int use_ssl, int keep_alive);

/* Close and discard every idle pooled connection. Call at program exit
 * for clean shutdown (not strictly required, but tidy). */
void pool_shutdown(void);

#endif
