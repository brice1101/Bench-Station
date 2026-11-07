#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#define LED PB7

int main() {
    DDRB |= (1 << LED);
    while (1) {
        PORTB |= (1 << LED);
        _delay_ms(1000);
        PORTB &= ~(1 << LED);
        _delay_ms(1000);
    }
}