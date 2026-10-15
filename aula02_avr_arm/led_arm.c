#include <stdint.h>
#define GPIOB_BSRR (*(volatile uint32_t *)0x42020418UL)
#define GPIOB_ODR  (*(volatile uint32_t *)0x42020414UL)
void led_liga(void)    { GPIOB_BSRR = (1U << 0); }
void led_desliga(void) { GPIOB_BSRR = (1U << 16); }
void led_liga_rmw(void){ GPIOB_ODR |= (1U << 0); }
