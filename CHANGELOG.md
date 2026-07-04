# Changelog

All notable changes to FetC will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-07-04

### Added
- HTTP and HTTPS download support
- HTTP/1.1 protocol implementation
- Download resume capability using HTTP Range requests
- Automatic redirect handling (301, 302, 307, 308)
- Chunked Transfer-Encoding decoding
- Segmented (multi-threaded) downloads for improved performance
- Progress callback API for real-time download monitoring
- HEAD request support for metadata retrieval
- Static library (`libfetc.a`) for programmatic use
- Command-line downloader with progress bar
- Connection pooling for efficient connection reuse
- Socket timeouts (30 seconds) to prevent indefinite blocking
- TLS certificate verification by default
- Optional `--insecure` flag for debugging (disables TLS verification)
- Verbose mode (`--verbose`) for debug output
- Comprehensive unit test suite covering core components

### Security
- TLS certificate verification enabled by default using `SSL_VERIFY_PEER`
- System CA bundle loading with `SSL_CTX_set_default_verify_paths()`
- Buffer safety improvements using `strncpy()` instead of `strcpy()`
- Socket timeouts to prevent hanging connections

### Tested
- URL parsing (basic, HTTPS, no path, invalid, filename extraction)
- HTTP header parsing (basic, chunked, range, redirect)
- HTTP request building (GET, HEAD, Range headers)
- Chunked transfer decoding (basic, initial buffer, split reads, progress callbacks, malformed input)
- Resume functionality (missing file, existing file)

### Documentation
- Comprehensive README with usage examples
- MIT License
- Contributing guidelines with coding conventions
- API documentation in header files

### Project Structure
- Modular design with clear separation of concerns
- Transport layer abstraction for HTTP/HTTPS
- Mockable interfaces for unit testing
- Thread-safe connection pooling
- K&R-style coding conventions throughout
