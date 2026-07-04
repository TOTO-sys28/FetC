# FetC - A lightweight HTTP/HTTPS download library

PREFIX      ?= /usr/local
LIBDIR      ?= $(PREFIX)/lib
INCLUDEDIR  ?= $(PREFIX)/include
BINDIR      ?= $(PREFIX)/bin
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig

CC          := gcc
AR          := ar
CFLAGS      := -Wall -Wextra -O2 -Iinclude
LDFLAGS += -lssl -lcrypto -lpthread -lz

SRCS      := src/url.c src/socket.c src/transport.c src/request.c \
               src/headers.c src/chunked.c src/file.c src/download.c \
               src/resume.c src/segmented.c src/pool.c
OBJS        := $(SRCS:src/%.c=%.o)
LIB         := lib/libfetc.a
BIN         := bin/fetc

TEST_SRCS   := tests/test_url.c tests/test_headers.c tests/test_request.c \
               tests/test_chunked.c tests/test_resume.c
TEST_BINS   := tests/test_url tests/test_headers tests/test_request \
               tests/test_chunked tests/test_resume

.PHONY: all clean install uninstall test

all: $(LIB) $(BIN)

$(LIB): $(OBJS)
	@mkdir -p lib
	$(AR) rcs $@ $^

$(BIN): main.c $(LIB)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $< -Llib -lfetc $(LDFLAGS)

%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Tests
tests/test_url: tests/test_url.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -Llib -lfetc $(LDFLAGS)

tests/test_headers: tests/test_headers.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -Llib -lfetc $(LDFLAGS)

tests/test_request: tests/test_request.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -Llib -lfetc $(LDFLAGS)

tests/test_chunked: tests/test_chunked.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -Llib -lfetc $(LDFLAGS)

tests/test_resume: tests/test_resume.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -Llib -lfetc $(LDFLAGS)

test: $(LIB) $(TEST_BINS)
	@./tests/test_url
	@./tests/test_headers
	@./tests/test_request
	@./tests/test_chunked
	@./tests/test_resume
	@echo ""
	@echo "========================================"
	@echo "All tests PASSED!"
	@echo "========================================"

libfetc.pc: Makefile
	@echo "prefix=$(PREFIX)" > $@
	@echo "exec_prefix=\$${prefix}" >> $@
	@echo "libdir=\$${prefix}/lib" >> $@
	@echo "includedir=\$${prefix}/include" >> $@
	@echo "" >> $@
	@echo "Name: fetc" >> $@
	@echo "Description: Lightweight HTTP/HTTPS download library" >> $@
	@echo "Version: 0.1.0" >> $@
	@echo "Libs: -L\$${libdir} -lfetc -lssl -lcrypto -lpthread" >> $@
	@echo "Cflags: -I\$${includedir}" >> $@

install: all libfetc.pc
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	install -m 644 $(LIB) $(DESTDIR)$(LIBDIR)/
	install -m 644 include/fetc.h $(DESTDIR)$(INCLUDEDIR)/
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/
	install -m 644 libfetc.pc $(DESTDIR)$(PKGCONFIGDIR)/

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/libfetc.a
	rm -f $(DESTDIR)$(INCLUDEDIR)/fetc.h
	rm -f $(DESTDIR)$(BINDIR)/fetc
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/libfetc.pc

clean:
	rm -f $(OBJS) $(LIB) $(BIN) libfetc.pc
	rm -f $(TEST_BINS)
	rm -f *.o
