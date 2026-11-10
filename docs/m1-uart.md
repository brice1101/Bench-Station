# M1: Build System and Serial Output

## Goal
Makefile, register-level blink, a UART driver with an interrupt-driven receive into a ring buffer, and `printf` routed to the UART.

**Done when:** `make flash` works from the terminal, the board prints a banner and echoes back what I type, and the ring buffer has host unit tests.

## Status
- [x] Toolchain (avr-gcc, avrdude) working on Fedora
- [x] Register-level blink (PB7, onboard LED on D13)
- [x] Makefile: `make`, `make flash`, `make disasm`, `make clean`, `make test`
- [x] `ring_buffer.h` API and contract
- [ ] `ring_buffer.c` written test-first
- [ ] Step 1: polled transmit, "Hello" visible in picocom
- [ ] Step 2: `printf` via `FDEV_SETUP_STREAM`
- [ ] Step 3: RX interrupt into ring buffer, with echo
- [ ] Tag `m1-uart`, update README, photo

## Plan: three steps

1. **Polled transmit.** Wait until the UART can take a byte, write it, repeat.
   No interrupts. Proves the baud rate and wiring are right.
2. **printf.** Connect avr-libc's `stdout` to my transmit function, so
   `printf("uptime %lu\n", t)` works.
3. **Interrupt-driven receive.** The RX interrupt puts each incoming byte into
   a ring buffer; the main loop takes bytes out and echoes them back.

## Key decisions
| Decision | Choice | Why |
| --- | --- | --- |
| Baud rate | 115200, `U2X0` = 1, `UBRR0` = 16 | Gives 117647 baud: +2.1% error, within tolerance. Fallbacks if I see garbage: 76800 or 250000 (0% error), or 38400 |
| Transmit wait | Poll `UDRE0` (data register empty) | `UDRE0` means "ready for the next byte". `TXC0` means "the last bit has fully left the pin", which only matters before shutting down or turning a bus around |
| RX/TX buffers | One `ring_buffer_t` each, `static` in `uart.c` | Only the UART driver should touch them |
| Buffer size | 64 bytes | A pasted 20–30 character command fits with room to spare; 128 bytes of RAM for two buffers out of 8 KB |
| When full | Drop the new byte, `rb_put` returns false, ISR counts `rx_overflows` | The ISR must never wait. The counter makes data loss visible (shown in `stats` later) |
| Tests | Plain `assert()` now, Unity later | Get the habit first, then the framework |

The baud rate error matters here, as the two clocks falling out of sync can cause garbage characters to come through as the drift accumulates. For a 10-bit frame (8 bits, plus a start and stop bit), an error rate of 2.1% drifts to 21% over the frame, meaning bits may be miss-sampled

## Ring Buffer

### What's it for?

The UART hardware holds only about 2 received bytes. If the main loop is busy (updating the LCD for example) when more arrive, they're lost. The ring buffer is a waiting room between the interrupt, which must be fast, and the main loop, which gets to the bytes when it can.

```
RX (receive):
    serial line -> UDR0 -> [RX ISR] --rb_put--> rx_buf --rb_get--> [main loop] -> shell
                           producer                                 consumer
```

### The TX side: the producer and consumer swap places

On TX, **main is the producer**: `printf` puts bytes in. The consumer is the **UDRE interrupt**, which fires whenever the UART's data register is empty and ready for another byte. Each time it fires, the ISR takes one byte with `rb_get` and writes it to `UDR0`. When the buffer is empty, the ISR turns its own interrupt off (`UDRIE0`), otherwise it would fire continuously. `rb_put` turns it back on.

So the same ring buffer code is used both ways round. What stays the same is the rule: **one context puts, a different single context gets.**

### Why `rb_init` must come before interrupts are enabled

`rb_init` writes `head` *and* `tail`. That breaks the "one writer per index" rule: it writes the index the ISR owns. If the RX interrupt is already enabled, a byte could arrive in the middle of `rb_init`, and the ISR would `rb_put` into a half-reset buffer. The fix is about ordering: initialise both buffers, *then* enable the interrupts (`RXCIE0`, then `sei()`).

### Design

- `uint8_t data[RB_SIZE]`, plus `uint8_t head` (next write) and `uint8_t tail`
  (next read).
- **Free-running indices:** head and tail just keep counting up and wrap from
  255 to 0 naturally. The slot is `index & (RB_SIZE - 1)`.
- **Count** = `head - tail`. **Empty** when equal; **full** when the count is
  `RB_SIZE`.
- **Capacity is all `RB_SIZE` slots.** Many ring buffers waste one slot to
  tell full and empty apart; free-running indices avoid that.
- **Size limits** (enforced with `_Static_assert`): a power of two, so masking
  works; at least 1; at most 128, so that a count of `RB_SIZE` is
  distinguishable from 0 in a `uint8_t` (a count of 256 wraps to 0, so full
  looks empty).

### Integer promotion
In C, `uint8_t` values are promoted to `int` before arithmetic. So
`head - tail` with head = 2 and tail = 250 gives **-248** as an `int`, not 8.
Unsigned wrap-around only happens when the result is stored back in, or cast
to, a `uint8_t`. Write a test that crosses the 255 → 0 boundary to catch this.

### Contract summary

**Concurrency rule:**

### Tests
1. New buffer is empty
2. Put one byte, get the same byte back
3. Bytes come out in FIFO order
4. Fill to `RB_SIZE`: every put succeeds
5. Put into a full buffer returns false and doesn't corrupt data
6. Wrap-around: fill, drain, fill again
7. Free-running wrap: run past 255 -> 0
8. Stress: 1000 put/get cycles

**assert pitfall:** never do real work inside `assert()`. If the code is ever compiled with `NDEBUG` defined, `assert` disappears and so does the call. Assign the result to a variable, then assert on the variable

## Known caveat
The DHT11 read takes about 4 ms with interrupts off. At 115200 baud a byte arrives roughly every 87 microseconds, so about 46 bytes could arrive in that time, but the hardware only holds about 2 more. The rest are lost, and the chip sets the `DOR0` (data overrun) flag. The ring buffer only helps while interrupts are on.

## Glossary
Short definitions, plus where to read more. **DS** = ATmega2560 datasheet

**Register / memory-mapped I/O.** Hardware control and status live at fixed
memory addresses. Writing `PORTB` writes to the hardware. *DS: "Register
Summary"; `<avr/io.h>` names them.*

**Bit manipulation.** Set: `reg |= (1 << BIT)`. Clear: `reg &= ~(1 << BIT)`.
Toggle: `^=`. Test: `reg & (1 << BIT)`. *King, ch. 20 "Low-Level Programming".*

**UART / USART.** Asynchronous serial: no shared clock, so both sides must
agree on the baud rate. The ATmega2560 has four USARTs; USB serial is USART0.
*DS: "USART" chapter.*

**Frame (8N1).** 1 start bit, 8 data bits, no parity, 1 stop bit, so 10 bits
per byte. At 115200 baud that's 11520 bytes/s, one about every 87 µs. *DS:
USART "Frame Formats".*

**Baud rate / UBRR0 / U2X0.** `UBRR0` sets the baud rate divider. `U2X0`
doubles the speed: the divider becomes 8 instead of 16, which gives finer
choices. With U2X0 = 1: `UBRR = F_CPU / (8 × baud) − 1`. *DS: USART "Clock
Generation" and the "Examples of Baud Rate Setting" table for 16 MHz.*

**UDR0.** The data register. Writing it sends a byte; reading it collects a
received byte. One address, two separate hardware buffers. *DS: USART
"Register Description".*

**Status flags (UCSR0A).** `RXC0`: a received byte is waiting. `UDRE0`: ready
for a new byte to send. `TXC0`: transmission fully complete. `DOR0`: data
overrun, meaning a byte was lost. *DS: UCSR0A.*

**Control bits (UCSR0B).** `RXEN0`/`TXEN0` enable the receiver and
transmitter. `RXCIE0` enables the RX-complete interrupt. `UDRIE0` enables the
data-register-empty interrupt. *DS: UCSR0B.*

**Polling vs. interrupts.** Polling means looping and checking a flag; simple,
but the CPU waits. An interrupt means the hardware calls your code when the
event happens.

**Interrupt / ISR / vector.** When an enabled event happens, the CPU finishes
its current instruction, jumps to the ISR through the vector table, runs it,
and returns. In avr-libc: `ISR(USART0_RX_vect) { ... }`. Keep ISRs short:
never `printf` in one. *DS: "Interrupts" chapter, "Reset and Interrupt
Vectors" table; avr-libc `<avr/interrupt.h>`.*

**sei() / cli() / SREG I-bit.** The global interrupt enable bit. `sei()`
turns interrupts on, `cli()` turns them off. Gotcha: enabling an interrupt
that has no ISR jumps to the reset vector, and the board appears to reboot
randomly. *DS: "Status Register".*

**Execution context.** "Who" is running: the main loop or an ISR. An ISR can
start between any two main-loop instructions; main cannot interrupt an ISR.

**Race condition.** A bug where the result depends on the timing of an
interrupt. Classic example: main reads a multi-byte variable, the ISR updates
it halfway through the read, and main gets a torn value.

**Atomic.** It completes without any chance of an interrupt in the middle. On
AVR, a single `uint8_t` load or store is atomic; a `uint16_t` or `uint32_t`
read takes several instructions, so it is not.

**Critical section / ATOMIC_BLOCK.** Code run with interrupts briefly off so
it can't be interrupted. `ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { ... }` from
`<util/atomic.h>` does this safely. *avr-libc `<util/atomic.h>` docs.*

**volatile.** Tells the compiler the variable can change behind its back (in
an ISR), so it must re-read memory every time instead of keeping a copy in a
register. It does **not** make an access atomic. *avr-libc FAQ: "My program
doesn't recognize a variable updated within an interrupt routine".*

**Single-producer / single-consumer (SPSC).** One context only writes,
another only reads. If each index has one writer and index accesses are
atomic, no locking is needed. This is why the ring buffer works without
`cli()`.

**Ring (circular) buffer.** A fixed array used as a FIFO queue, where the
indices wrap around the end. *Wikipedia: "Circular buffer".*

**Power-of-two masking.** For a size of 2ⁿ, `i & (size − 1)` equals
`i % size`. AVR has no hardware divide, so `%` becomes a slow library call.
Check with `make disasm` or Compiler Explorer.

**Integer promotion.** Types smaller than `int` are converted to `int` before
arithmetic. *King, ch. 7 "Basic Types", implicit
conversions.*

**Contract.** The agreement between a function and its caller:
*preconditions* (what the caller must guarantee), *postconditions* (what the
function guarantees), and behaviour on bad input, either defined or "the
caller's bug".

**Invariant.** Something always true between calls, e.g.
`0 ≤ head − tail ≤ RB_SIZE`.

**_Static_assert.** A compile-time check: `_Static_assert(expr, "msg")`.
`expr` must be an integer constant expression, so no function calls like
`log2`. A failure stops the build.

**FDEV_SETUP_STREAM / stdout.** avr-libc's way to make `printf` call your own
"put one character" function. *avr-libc `<stdio.h>` docs.*

**TDD (red → green → refactor).** Write a failing test, write just enough code
to pass it, tidy up, repeat. *Grenning, "Test Driven Development for Embedded
C".*

**Make.** A target is rebuilt when any prerequisite is newer. `| build` is an
*order-only* prerequisite: the directory must exist, but its timestamp doesn't
trigger rebuilds. `.PHONY` marks targets that aren't files. *GNU Make manual:
"Types of Prerequisites".*

## References
- ATmega640/1280/1281/2560/2561 datasheet (Microchip website, search
  "ATmega2560 datasheet"). Chapters: USART, Interrupts, Status Register,
  Register Summary
- AVR Instruction Set Manual (Microchip): what `LDS`, `STS`, `CLI` and `SEI` do
- avr-libc reference manual: `<avr/interrupt.h>`, `<util/atomic.h>`,
  `<stdio.h>`, and the FAQ
- Arduino Mega 2560 pinout and schematic (store.arduino.cc), to map D13 to PB7
  and the USB serial port to USART0
- K. N. King, *C Programming: A Modern Approach*, ch. 7 (types and
  conversions) and ch. 20 (low-level programming)
- James Grenning, *Test Driven Development for Embedded C*
- Compiler Explorer (godbolt.org), with the "AVR gcc" compiler and `-Os`
- GNU Make manual (gnu.org)