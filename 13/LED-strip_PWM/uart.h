#ifndef uart_h
#define uart_h

// at 9600 one bit should take 0.104ms
#define HALF_BIT_DELAY          21 // tuned a bit after looking at the sample times. 1 tick ~ 1.7µs (4.8MHz system-clock)

#define ENABLE_PCINT0_VECT  GIMSK |= _BV(PCIE)
#define DISABLE_PCINT0_VECT GIMSK &= ~_BV(PCIE)
#define CLEAR_PCINT0_FLAG   GIFR |= _BV(PCIF)

#define ENABLE_TIM0_COMPB_VECT  TIMSK0 |= _BV(OCIE0B)
#define DISABLE_TIM0_COMPB_VECT TIMSK0 &= ~_BV(OCIE0B)
#define CLEAR_TIM0_COMPB_FLAG   TIFR0 = _BV(OCF0B) // clear the flag by writing a 1 (see datasheet);

void soft_uart_setup(void);
void soft_uart_send(uint8_t byte, uint8_t half_bit_delay);
uint8_t soft_uart_read(void);
uint8_t soft_uart_peek(void);

#endif
