#include <avr/interrupt.h>
#include <stdint.h>
#include "system_ticker.hpp"

volatile uint8_t enc_evt = 0;

void system_ticker_setup(void)
{
    // using timer0
    // setting prescaler to 64
    TCCR0B |= _BV(CS01) | _BV(CS00) ;
    TCCR0B &= ~_BV(CS02);
    // set WGM mode 3 (Fast PWM)
    TCCR0A |= (_BV(WGM01) | _BV(WGM00));
    TCCR0B &= ~_BV(WGM02);
    // inv. PWM
    TCCR0A &= ~(_BV(COM0B1) | _BV(COM0B0));
    TCCR0A |= (_BV(COM0A1) | _BV(COM0A0));
    // enabling overflow interrupt
    TIMSK |= _BV(TOIE0);
    OCR0A = 255; // inverted PWM - start OFF

	// encoder //
	DDRB &= ~0x07; // make PB0 (button), PB1 (ENC_A), PB2 (ENC_B) inputs
	PORTB |= 0x07; // pull-ups on
}

/*
 * the stuff below comes from Arduino's wiring.c
 * but I replaced all stuff that wasn't "uintXX_t" like.
 *
 */

#if (F_CPU == 16000000UL)
#define MICROSECONDS_PER_TIMER0_OVERFLOW 1024
#endif
#if (F_CPU == 8000000UL)
#define MICROSECONDS_PER_TIMER0_OVERFLOW 2048
#endif
#define MILLIS_INC (MICROSECONDS_PER_TIMER0_OVERFLOW / 1000)
#define FRACT_INC ((MICROSECONDS_PER_TIMER0_OVERFLOW % 1000) >> 3)
#define FRACT_MAX (1000 >> 3)

volatile uint32_t timer0_overflow_count = 0;
volatile uint32_t timer0_millis = 0;
static uint8_t timer0_fract = 0;

ISR(TIMER0_OVF_vect)
{
    // copy these to local variables so they can be stored in registers
    // (volatile variables must be read from memory on every access)
    uint32_t m = timer0_millis;
    uint8_t f = timer0_fract;

    m += MILLIS_INC;
    f += FRACT_INC;
    if (f >= FRACT_MAX) {
        f -= FRACT_MAX;
        m += 1;
    }

    timer0_fract = f;
    timer0_millis = m;
    timer0_overflow_count++;

    // reading the quadrature encoder //
	uint8_t cur_enc_state;
	static uint8_t prev_enc_state = 0;

	cur_enc_state = ~(PINB & 0x07);  // bit 2: ENC_B, bit 1: ENC_A, bit 0: button

	// full resolution, 4 counting-ticks per 1 mechanical tick
	// uint8_t enc_rot_trans[16] = {0, 2, 1, 0, 1, 0, 0, 2, 2, 0, 0, 1, 0, 1, 2, 0};
		
	// encoder transitions:
	//
	// 0 --> 0, 2 --> +1, 1 --> -1
	//
	// 2 and 1 are used instead of just +1 and -1, as this keeps the bits separate.
	// 2 is 0b00000010 and 1 is 0b00000001, so going forward or backward can be
	// polled as two individual flag-bits in the 'enc_evt' byte.
	//
	// #	from-to : step to take
	//
	// 0	00-00 : 0
	// 1	00-01 : 2
	// 2	00-10 : 1
	// 3	00-11 : 0
	// 4	01-00 : 1
	// 5	01-01 : 0
	// 6	01-10 : 0
	// 7	01-11 : 2
	// 8	10-00 : 2
	// 9	10-01 : 0
	// 10	10-10 : 0
	// 11	10-11 : 1
	// 12	11-00 : 0
	// 13	11-01 : 1
	// 14	11-10 : 2
	// 15	11-11 : 0

	// just 1 counting tick per 1 mechnical tick
	uint8_t enc_rot_trans[16] = {0, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	// enc_evt:  bit 4: button state, bit 3: button just pressed, bit 2: button just released, bit 1: count+, bit 0: count-

	// always write the current 'live' button state - figure out debouncing later
	if( cur_enc_state & 0x01 ) {
		enc_evt |= _BV(4);
	} else {
		enc_evt &= ~_BV(4);
	}

	// only OR the changes into it, so encoder_get() has to zero them out and the last change doesn't get
	// overwritten by the next ISR invocation
	enc_evt |= ( ( ( (prev_enc_state & 0x01) ^ (cur_enc_state & 0x01) ) & (cur_enc_state & 0x01) ) << 3 ) + \
			   ( ( ( (prev_enc_state & 0x01) ^ (cur_enc_state & 0x01) ) & (prev_enc_state & 0x01) ) << 2 ) + \
			   enc_rot_trans[ ( (prev_enc_state & 0x06) << 1 ) + ( (cur_enc_state & 0x06) >> 1 ) ];

	prev_enc_state = cur_enc_state;
}

uint32_t millis(void)
{
    uint32_t m;
    uint8_t _sreg = SREG;
    cli();
    m = timer0_millis;
    SREG = _sreg;
    return m;
}

uint32_t micros(void)
{
    uint32_t m;
    uint8_t t;
    uint8_t _sreg = SREG;
    cli();
    m = timer0_overflow_count;
    t = TCNT0;
#if defined(TIFR0)
    if ((TIFR0 & _BV(TOV0)) && (t < 255)) {
        m++;
    }
#endif
#if defined(TIFR)
    if ((TIFR & _BV(TOV0)) && (t < 255)) {
        m++;
    }
#endif
    SREG = _sreg;
#if (F_CPU == 16000000UL)
    return (((m << 8) + t) * 4);
#endif
#if (F_CPU == 8000000UL)
    return (((m << 8) + t) * 8);
#endif
}

void delay(uint32_t ms)
{
    uint16_t start = (uint16_t)micros();

    while (ms > 0) {
        if (((uint16_t)micros() - start) >= 1000) {
            ms--;
            start += 1000;
        }
    }
}

/* Delay for the given number of microseconds.  Assumes a 8 or 16 MHz clock. */
void delayMicroseconds(uint16_t us)
{
    // calling avrlib's delay_us() function with low values (e.g. 1 or
    // 2 microseconds) gives delays longer than desired.
    //delay_us(us);

#if (F_CPU >= 16000000L)
    // for the 16 MHz clock on most Arduino boards

    // for a one-microsecond delay, simply return.  the overhead
    // of the function call yields a delay of approximately 1 1/8 us.
    if (--us == 0)
        return;

    // the following loop takes a quarter of a microsecond (4 cycles)
    // per iteration, so execute it four times for each microsecond of
    // delay requested.
    us <<= 2;

    // account for the time taken in the preceeding commands.
    us -= 2;
#else
    // for the 8 MHz internal clock on the ATmega168

    // for a one- or two-microsecond delay, simply return.  the overhead of
    // the function calls takes more than two microseconds.  can't just
    // subtract two, since us is unsigned; we'd overflow.
    if (--us == 0)
        return;
    if (--us == 0)
        return;

    // the following loop takes half of a microsecond (4 cycles)
    // per iteration, so execute it twice for each microsecond of
    // delay requested.
    us <<= 1;

    // partially compensate for the time taken by the preceeding commands.
    // we can't subtract any more than this or we'd overflow w/ small delays.
    us--;
#endif

    // busy wait
    __asm__ __volatile__ (
        "1: sbiw %0,1" "\n\t" // 2 cycles
        "brne 1b" : "=w" (us) : "0" (us) // 2 cycles
    );
}

uint8_t encoder_get(uint8_t whatbit) {
	uint8_t _sreg = SREG;
	cli();
	if( enc_evt & _BV(whatbit) ) {
		if( whatbit != BUTTON_STATE ) {
			enc_evt &= ~_BV(whatbit); // reset flag
		}
		SREG = _sreg;
		return 1;
	} else {
		SREG = _sreg;
		return 0;
	}
}
