# Milestones

## M1: Build system and serial output

Build Makefile, register-level blink, a UART driver with interrupt-driven receive into a ring buffer, and printf routed to the UART

**Done when**: `make flash` works from te terminal, the board prints a banner, and echoes back what I type. The ring buffer has host unit tests