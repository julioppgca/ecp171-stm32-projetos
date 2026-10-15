#include <avr/io.h>
void led_liga(void)    { PORTB |= (1 << PORTB5); }
void led_desliga(void) { PORTB &= ~(1 << PORTB5); }
