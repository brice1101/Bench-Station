HOST_CC		= gcc
HOST_FLAGS	= -Wall -Wextra -Werror -std=c11 -g -Ilib

test: build/test_ring_buffer
	./build/test_ring_buffer

build/test_ring_buffer: test/test_ring_buffer.c lib/ring_buffer.c lib/ring_buffer.h | build
	$(HOST_CC) $(HOST_CFLAGS) -o $@ test/test_ring_buffer.c lib/ring_buffer.c

build:
	mkdir -p build

.PHONY: test