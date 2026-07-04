#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "transport.h"
#include "socket.h"

static int openssl_initialized = 0;
static int insecure_mode = 0;
static int verbose_mode = 0;

void transport_set_insecure(int insecure)
{
    insecure_mode = insecure;
}

void transport_set_verbose(int verbose)
{
    verbose_mode = verbose;
}

static void openssl_init(void)
{
    if (openssl_initialized)
        return;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#else
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
#endif

    openssl_initialized = 1;
}

int transport_connect(Transport *t, const char *host, int port, int use_ssl)
{
    memset(t, 0, sizeof(*t));

    /* Check for proxy configuration */
    const char *proxy_env = use_ssl ? "HTTPS_PROXY" : "HTTP_PROXY";
    if (proxy_parse_env(proxy_env, &t->proxy) == 0) {
        t->use_proxy = 1;
    } else {
        /* Try lowercase version */
        proxy_env = use_ssl ? "https_proxy" : "http_proxy";
        if (proxy_parse_env(proxy_env, &t->proxy) == 0) {
            t->use_proxy = 1;
        }
    }

    /* Connect via proxy if configured, otherwise direct */
    if (t->use_proxy) {
        if (use_ssl) {
            /* HTTPS: Use CONNECT tunneling */
            t->sockfd = socket_connect_proxy(host, port, &t->proxy);
        } else {
            /* HTTP: Connect directly to proxy */
            t->sockfd = socket_connect(t->proxy.host, t->proxy.port);
        }
    } else {
        t->sockfd = socket_connect(host, port);
    }

    if (t->sockfd < 0) {
        if (verbose_mode)
            fprintf(stderr, "[transport] TCP connect failed: %s:%d\n", host, port);
        return -1;
    }

    if (!use_ssl)
        return 0;

    openssl_init();

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        if (verbose_mode) {
            fprintf(stderr, "[transport] SSL_CTX_new failed:\n");
            ERR_print_errors_fp(stderr);
        }
        goto fail;
    }

    if (insecure_mode) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
        SSL_CTX_set_default_verify_paths(ctx);
    }

    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        if (verbose_mode) {
            fprintf(stderr, "[transport] SSL_new failed:\n");
            ERR_print_errors_fp(stderr);
        }
        goto fail_ctx;
    }

    if (SSL_set_fd(ssl, t->sockfd) != 1) {
        if (verbose_mode) {
            fprintf(stderr, "[transport] SSL_set_fd failed:\n");
            ERR_print_errors_fp(stderr);
        }
        goto fail_ssl;
    }

    if (SSL_set_tlsext_host_name(ssl, host) != 1) {
        if (verbose_mode) {
            fprintf(stderr, "[transport] SSL_set_tlsext_host_name failed:\n");
            ERR_print_errors_fp(stderr);
        }
        goto fail_ssl;
    }

    if (SSL_connect(ssl) <= 0) {
        if (verbose_mode) {
            fprintf(stderr, "[transport] SSL_connect failed for %s:%d:\n", host, port);
            ERR_print_errors_fp(stderr);
        }
        goto fail_ssl;
    }

    t->ssl = ssl;
    t->ctx = ctx;
    return 0;

fail_ssl:
    SSL_free(ssl);
fail_ctx:
    SSL_CTX_free(ctx);
fail:
    close(t->sockfd);
    t->sockfd = -1;
    return -1;
}

ssize_t transport_send(Transport *t, const void *buf, size_t len)
{
    if (t->ssl)
        return SSL_write((SSL *)t->ssl, buf, (int)len);
    return send(t->sockfd, buf, len, 0);
}

ssize_t transport_recv(Transport *t, void *buf, size_t len)
{
    if (t->ssl)
        return SSL_read((SSL *)t->ssl, buf, (int)len);
    return recv(t->sockfd, buf, len, 0);
}

void transport_close(Transport *t)
{
    if (t->ssl) {
        SSL_shutdown((SSL *)t->ssl);
        SSL_free((SSL *)t->ssl);
        SSL_CTX_free((SSL_CTX *)t->ctx);
        t->ssl = NULL;
        t->ctx = NULL;
    }
    if (t->sockfd >= 0) {
        close(t->sockfd);
        t->sockfd = -1;
    }
}
