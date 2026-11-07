MCU		= atmega2560
F_CPU	= 16000000UL
PORT   ?= /dev/ttyACM0
TARGET	= main

CC		= avr-gcc
CFLAGS	= -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=c11

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

.PHONY: all flash disasm clean