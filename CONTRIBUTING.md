# Contributing to FetC

Thank you for your interest in contributing to FetC! This document provides guidelines for building, testing, and contributing to the project.

## Building Locally

### Prerequisites

- GCC compiler
- OpenSSL development libraries (libssl-dev)
- POSIX Threads (pthread)
- Make

### Build Commands

```bash
# Build the library and CLI tool
make

# Build only the library
make lib/libfetc.a

# Build only the CLI tool
make bin/fetc
```

### Clean Build

```bash
make clean
make
```

## Running Tests

FetC includes a comprehensive test suite covering core functionality:

```bash
# Run all tests
make test

# Run individual tests
./tests/test_url
./tests/test_headers
./tests/test_request
./tests/test_chunked
./tests/test_resume
```

The test suite includes:
- URL parsing tests
- HTTP header parsing tests
- HTTP request building tests
- Chunked transfer decoding tests
- Resume functionality tests

## Installation

```bash
# Install to system directories (requires sudo)
sudo make install

# Uninstall
sudo make uninstall
```

## Coding Conventions

FetC follows K&R-style coding conventions:

### Formatting

- 4-space indentation (no tabs)
- Opening braces on the same line as the function/control statement
- Spaces around operators and after commas
- No trailing whitespace

### Naming

- Functions: `snake_case`
- Variables: `snake_case`
- Types/Structs: `snake_case`
- Constants: `UPPER_CASE`

### Examples

```c
/* Function definition */
int example_function(int param1, const char *param2)
{
    if (param1 > 0) {
        return param1;
    }
    return 0;
}

/* Struct definition */
typedef struct {
    int field1;
    char field2[64];
} ExampleStruct;

/* Control flow */
if (condition) {
    do_something();
} else {
    do_other();
}

/* Loops */
for (int i = 0; i < count; i++) {
    process_item(i);
}

while (condition) {
    process();
}
```

### Code Style Rules

- Keep functions focused and concise
- Add comments for complex logic
- Use `const` for read-only parameters
- Check return values from system calls
- Initialize variables before use
- Avoid magic numbers; use named constants

### Memory Management

- Always `free()` memory allocated with `malloc()`
- Close file descriptors and sockets
- Clean up resources in error paths
- Use `goto` for cleanup in complex functions when appropriate

### Error Handling

- Return meaningful error codes (typically -1 for errors, 0 for success)
- Provide descriptive error messages via `fprintf(stderr, ...)`
- Use verbose mode for debug output (controlled by `--verbose` flag)

## Submitting Changes

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes following the coding conventions
4. Run tests to ensure nothing is broken (`make test`)
5. Commit your changes with descriptive messages
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

## Commit Message Style

Use clear, descriptive commit messages:

```
Add support for HTTP proxy environment variables

- Parse HTTP_PROXY and HTTPS_PROXY environment variables
- Implement CONNECT tunneling for HTTPS over HTTP proxy
- Add proxy configuration to transport layer
- Update documentation with proxy usage examples
```

## Testing Your Changes

Before submitting a PR, ensure:

- All existing tests pass (`make test`)
- New features include appropriate tests
- Code follows the project's coding conventions
- No compiler warnings are introduced
- Changes are documented in relevant files

## Reporting Issues

When reporting bugs, please include:

- Operating system and version
- GCC version
- OpenSSL version
- Steps to reproduce the issue
- Expected vs actual behavior
- Any relevant error messages or logs

## License

By contributing to FetC, you agree that your contributions will be licensed under the MIT License.
