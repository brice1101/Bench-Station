#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

// RB_SIZE must be a power of 2, no more than 128
// RB_SIZE cannot be 256, as does not leave room for the extra bit to account for "head-tail" full/empty calculation
// RB_SIZE cannot be >256, as uint8_t cannot index numbers greater than this
#define RB_SIZE 64  // small RAM cost, covers pasting 20-30 character commands with room to spare
_Static_assert(RB_SIZE <= 128, "Ring buffer size must be no more than 128");
_Static_assert(RB_SIZE > 0 && (RB_SIZE & (RB_SIZE - 1)) == 0, "Ring buffer size must be a power of two");

// Caller should not write head or tail directly, this is managed by get/put
typedef struct {
    uint8_t data[RB_SIZE];
    uint8_t head;
    uint8_t tail;
} ring_buffer_t;


// initializes the ring buffer, sets the buffer to empty. must be called before any other rb_ function
// If called on a buffer that holds bytes, those bytes will be discarded
// If called on NULL, nothing is initialized
void rb_init(ring_buffer_t *rb);

// removes the oldest byte and writes it to *out. returns true on success
// if buffer is empty, returns false and leaves *out unchanged
// If rb is NULL, returns FALSE, leaves out unchanged
// If out is NULL, value is dumped
bool rb_get(ring_buffer_t *rb, uint8_t *out);

// Puts a value in the ring buffer
// Can hold RB_SIZE bytes before dropping the value, returning false
// If rb is NULL, returns FALSE
bool rb_put(ring_buffer_t *rb, uint8_t in);

#endif