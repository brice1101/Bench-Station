// Want 2 buffers, one for RX and one for TX. Make struct with one global instance per buffer
// Each slot holds ASCII character, size uint8_t
// Free-running uint8_t indices
// 
// Drop new byte when full, return false, have ISR count overflows
// Bool returns plus pointer to identify empties

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

// RB_SIZE must be a power of 2, no more than 128
#define RB_SIZE 64  // small RAM cost, covers pasting 20-30 character commands with room to spare

typedef struct {
    uint8_t data[RB_SIZE];
    uint8_t head;
    uint8_t tail;
} ring_buffer_t;


// initializes the ring buffer, sets the buffer to empty. must be called before any other rb_ function
void rb_init(ring_buffer_t *rb);

// removes the oldest byte and writes it to *out. returns true on success
// if buffer is empty, returns false and leaves *out unchanged
bool rb_get(ring_buffer_t *rb, uint8_t *out);

// puts a value in the ring buffer. Returns false if the buffer is full
bool rb_put(ring_buffer_t *rb, uint8_t in);

#endif