MCU		= atmega2560
F_CPU	= 16000000UL
PORT   ?= /dev/ttyACM0
TARGET	= main

CC		= avr-gcc
CFLAGS	= -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=c11

HOST_CC		= gcc
HOST_FLAGS	= -Wall -Wextra -Werror -std=c11 -g -Ilib

all: $(TARGET).hex

$(TARGET).elf: $(TARGET).c
	$(CC) $(CFLAGS) -o $@ $<
	avr-size --mcu=$(MCU) -C $@

$(TARGET).hex: $(TARGET).elf
	avr-objcopy -O ihex -R .eeprom $< $@

flash: $(TARGET).hex
	avrdude -p m2560 -c wiring -P $(PORT) -b 115200 -D -U flash:w:$<:i

disasm: $(TARGET).elf
	avr-objdump -d $< | less

clean:
	rm -f *.elf *.hex

test: build/test_ring_buffer
	./build/test_ring_buffer

build/test_ring_buffer: test/test_ring_buffer.c lib/ring_buffer.c lib/ring_buffer.h | build
	$(HOST_CC) $(HOST_CFLAGS) -o $@ test/test_ring_buffer.c lib/ring_buffer.c

build:
	mkdir -p build

.PHONY: all flash disasm clean test