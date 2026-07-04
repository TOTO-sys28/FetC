# FetC

**FetC** is a lightweight HTTP/HTTPS download library and command-line downloader written in C. It focuses on simplicity, portability, and a clean codebase while supporting modern HTTP features such as resumable downloads, chunked transfer decoding, redirects, and multithreaded segmented downloads.

The project is intended as both a usable downloader and a learning resource for network programming in C.

---

## Features

* HTTP and HTTPS downloads
* HTTP/1.1 support
* Download resume using HTTP Range requests
* Automatic redirect handling (301, 302, 307, 308)
* Chunked Transfer-Encoding decoding
* Segmented (multi-threaded) downloads
* Progress callback API
* HEAD requests for download metadata
* Static library (`libfetc.a`)
* Command-line downloader
* Unit tests for core components
* Portable POSIX implementation

---

## Project Structure

```
include/
    fetc.h
    transport.h
    request.h
    headers.h
    chunked.h
    resume.h
    pool.h
    ...

src/
    url.c
    socket.c
    transport.c
    request.c
    headers.c
    chunked.c
    download.c
    segmented.c
    resume.c
    pool.c
    ...

tests/
    test_url.c
    test_headers.c
    test_request.c
    test_chunked.c
    test_resume.c
```

---

## Building

Requirements:

* GCC
* OpenSSL
* POSIX Threads

Compile everything:

```bash
make
```

Run the test suite:

```bash
make test
```

Install:

```bash
sudo make install
```

---

## Usage

Download a file:

```bash
./bin/fetc https://example.com/file.zip
```

Download using segmented mode:

```bash
./bin/fetc -s https://example.com/file.zip
```

---

## Library Example

```c
#include "fetc.h"

int main(void)
{
    URL url;
    Downloader dl;

    url_parse("https://example.com/file.zip", &url);

    download_init(&dl);
    download_file(&dl, &url);
    download_destroy(&dl);

    return 0;
}
```

---

## Tested Components

Current unit tests cover:

* URL parsing
* HTTP header parsing
* HTTP request generation
* Chunked transfer decoding
* Resume logic

Run them with:

```bash
make test
```

---

## Current Status

The project is functional and all available unit tests pass successfully.

Implemented:

* HTTP/HTTPS transport
* Redirect handling
* Resume support
* Chunked decoding
* Segmented downloads
* Progress callbacks
* Basic connection pool interface

Future improvements may include:

* HTTP/2 support
* Better connection pooling
* Cookie handling
* Proxy support
* Compression (gzip/deflate)
* Download rate limiting
* Windows compatibility

---

## License

MIT License.
