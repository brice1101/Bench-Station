#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "ring_buffer.h"

static void test_new_buffer_is_empty(void) {
    ring_buffer_t rb;
    rb_init(&rb);
    uint8_t byte;
    bool got = rb_get(&rb, &byte);
    assert(!got);
}

int main(void) {
    test_new_buffer_is_empty();
    printf("All ring buffer tests passed\n");
    return 0;
}