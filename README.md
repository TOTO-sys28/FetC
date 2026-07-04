# FetC

A lightweight HTTP/HTTPS download library and command-line downloader written in C, with resumable downloads, parallel segmented transfers, connection pooling, and transparent gzip/deflate decompression.

[![Build Status](https://github.com/TOTO-sys28/FetC/actions/workflows/build.yml/badge.svg)](https://github.com/TOTO-sys28/FetC/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Why I built this

I wanted to understand HTTP/HTTPS at the socket level instead of just calling libcurl, so I wrote FetC from scratch: raw sockets, manual TLS setup with OpenSSL, my own HTTP request/response parsing, chunked decoding, the works.

It grew into something actually usable along the way. It resumes interrupted downloads, splits a file into parallel segments if the server supports ranges, reuses connections instead of reopening TCP+TLS for every request, follows redirects, decompresses gzip/deflate on the fly, and works through an HTTP proxy.

It doesn't do FTP, SOCKS proxies, or HTTP/2 — wasn't the goal. If you need a full-featured client, use curl. If you want something small enough to actually read and embed in your own project, this might be useful.

## Features

- HTTP/1.1 and HTTPS with real TLS certificate verification (OpenSSL). There's a `--insecure` flag to disable verification, but it's off by default and meant for debugging only.
- Resume interrupted downloads via HTTP `Range` requests.
- Parallel segmented downloads — split a file into N concurrent range requests with pthreads, when the server supports `Accept-Ranges`.
- Connection pooling: idle connections get reused for the same host/port/TLS/proxy combo instead of reopening a handshake every time.
- HTTP proxy support through `HTTP_PROXY`/`HTTPS_PROXY`, including `CONNECT` tunneling for HTTPS.
- Transparent gzip/deflate decompression via zlib.
- Chunked transfer-encoding decoder, independently unit-tested with a mock reader/writer.
- Redirect following (301/302/307/308, capped at 5 hops).
- Structured errors — failures set a readable `error_message` instead of just printing to stderr.
- Progress callbacks, rate-limited so they don't flood your UI.

## Build

### Dependencies

- GCC (or any C99-compatible compiler)
- OpenSSL (`libssl`, `libcrypto`)
- zlib (`libz`)
- POSIX threads (`pthread`)

On Debian/Ubuntu:
```bash
sudo apt-get install build-essential libssl-dev zlib1g-dev
```

On NixOS / nix-shell:
```bash
nix-shell -p openssl zlib
```

### Building

```bash
make            # builds lib/libfetc.a and bin/fetc
make test       # builds and runs the unit test suite
make install    # installs to /usr/local by default; override with PREFIX=
```

## CLI Usage

```bash
# Simple download
./bin/fetc https://example.com/file.zip

# Show file metadata without downloading
./bin/fetc --info https://example.com/file.zip

# Parallel download with 4 segments
./bin/fetc --segments 4 https://example.com/file.zip

# Skip TLS certificate verification (testing only — do not use in production)
./bin/fetc --insecure https://example.com/file.zip

# Verbose logging (connection reuse, proxy resolution, etc.)
./bin/fetc --verbose https://example.com/file.zip
```

Interrupted downloads resume automatically on the next run, provided the server supports `Accept-Ranges` and reports a `Content-Length`.

### Using a proxy

```bash
export HTTP_PROXY=http://proxy.example.com:8080
export HTTPS_PROXY=http://proxy.example.com:8080
./bin/fetc https://example.com/file.zip
```

## Library Usage

```c
#include "fetc.h"

int main(void) {
    URL url;
    url_parse("https://example.com/file.zip", &url);

    Downloader dl;
    download_init(&dl);

    if (download_file(&dl, &url) != 0) {
        fprintf(stderr, "Download failed: %s\n", dl.error_message);
        return 1;
    }

    download_destroy(&dl);
    return 0;
}
```

Compile against the static library:
```bash
gcc myprogram.c -Iinclude -Llib -lfetc -lssl -lcrypto -lpthread -lz -o myprogram
```

### Segmented (parallel) downloads

```c
SegmentedDownloader sd;
segmented_download_init(&sd);
sd.segments = 8;
segmented_download_file(&sd, &url);
```

If the server doesn't support `Accept-Ranges`, doesn't report a size, or serves compressed content (which can't be safely split mid-stream), FetC automatically falls back to a single-threaded download — no data corruption risk.

### Progress callback

```c
void on_progress(size_t current, size_t total, void *user_data) {
    printf("\r%.1f%%", (double)current / total * 100.0);
    fflush(stdout);
}

dl.on_progress = on_progress;
```

## Project Structure
include/    Public headers (fetc.h is the single source of truth for shared types)
src/        Implementation
url.c         URL parsing
socket.c      Raw TCP + proxy CONNECT tunneling
transport.c   TLS/plaintext transport abstraction
request.c     HTTP request construction
headers.c     HTTP response header parsing
chunked.c     Chunked transfer-encoding decoder (mockable, unit-tested)
download.c    Core single-stream download logic (resume, redirects, gzip)
segmented.c   Multi-threaded parallel download
pool.c        Connection pooling
resume.c      Local partial-file size detection
file.c        File write helper
tests/      Unit tests (run via make test) + manual test scripts
main.c      CLI entry point

## What it doesn't do

- No FTP. HTTP/HTTPS only.
- No SOCKS proxies, just HTTP.
- No HTTP/2 — didn't need it for this.
- No proxy authentication.
- Segmented downloads require the server to support `Accept-Ranges` and report `Content-Length` up front; compressed or chunked responses fall back to single-threaded mode by design.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, coding style, and how to run the test suite.

## License

MIT — see [LICENSE](LICENSE).
