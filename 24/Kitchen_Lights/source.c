#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <stdlib.h>
#include "source.h"

uint8_t brightness = 128; /* for all 8 channels. 128 is fully off, 0 is fully on */
uint16_t auto_adj_fade_delay = (uint16_t)(__fade_delay); // this will be auto-adjusted by an LDR

volatile uint16_t system_ticks = 0;

int main(void)
{
        setup_hw();
        delay(6000);
        for (;;) {
                loop();
                // this saved about 2mA on my dev board
                sleep_enable(); // make it possible to have some zzzzz-s
                sleep_cpu();    // good night
                sleep_disable(); // we've just woken up again
        }
};

void loop(void)
{
        //adc_test(1); // shows ADCH on the 8 LEDs. timer1 should be OFF (or it blinks like mad --> headache)
        //adjust_fade_delay(7); // read an LDR on PA7
        kitchen_lights(1);
}

void kitchen_lights(uint8_t channel)
{
        // 0: off or user has decreased brightness
        // 1: on or user has increased brightness
        static uint8_t lamp_state = 0;
        static uint8_t switches_state = 0;
        uint8_t adc_tmp = read_adc(channel); // switches + resistor network connected to PA1
        static uint8_t ctr = 128;

        /*
	    board 1: tested with adc_test(1) and timer1 OFF !

            switches pressed    voltage     ADCH    state

            3                   -.--        214     3
            2                   -.--        184     2
            1                   -.--        144     1
        */

        /*
	    board 2: tested with adc_test(1) and timer1 OFF !

            switches pressed    voltage     ADCH    state

            3                   -.--        214     3
            2                   -.--        185     2
            1                   -.--        145     1
        */

        // evaluate ADCH and translate it to which buttons are pressed

        switches_state = 0; // reset

        if ( adc_tmp > 209 && adc_tmp < 219 ) {
                switches_state = 3;
        }

        if ( adc_tmp > 180 && adc_tmp < 190 ) {
                switches_state = 2;
        }

        if ( adc_tmp > 140 && adc_tmp < 150 ) {
                switches_state = 1;
        }

        if ( switches_state == 1 ) {
                lamp_state = 0;
                __LED_OFF;

                brightness = ctr;

                if (ctr < 128) {
                        ctr++;
                }
                delay(2*__fade_delay); // manually fading out with fixed speed
        }

        if ( switches_state == 3 ) {
                lamp_state = 1;
                __LED_ON;

                brightness = ctr;

                if (ctr > 0) {
                        ctr--;
                }
                delay(4*__fade_delay); // manually fading in with fixed speed, but slower ( better for the eyes in the morning ;-) )
        }

        if ( switches_state == 2 ) {
                if (ctr == 128) {	// fully off
                        lamp_state = 1;	// we increased brightness
                        __LED_ON;
                        fade_in(ctr,auto_adj_fade_delay); // auto-fadein adjusted by LDR
                        ctr = 0;
                } else if (ctr == 0) {	// fully on
                        lamp_state = 0;	// we decreased brightness
                        __LED_OFF;
                        fade_out(ctr,2*__fade_delay); // auto-fadeout with fixed speed
                        ctr = 128;
                }

                if (lamp_state == 0) {	// user pressed "-"
                        fade_out(ctr,2*__fade_delay);
                        ctr = 128;
                }
                if (lamp_state == 1) {	// user pressed "+"
                        fade_in(ctr,auto_adj_fade_delay);
                        ctr = 0;
                }
                delay(1500);	// until I have debounced buttons...
        }
}

inline void setup_hw(void)
{
        cli();              // turn interrupts off

        /*
         * power savings !
         *
         * total optimized power consumption of the cpu module: 2.2mA
         * incl. status LED (PWMed)
         *
         * the MBI5168 LED drivers take about 15mA each in idle mode :-(
         *
         */

        // turn the watchdog off
        wdt_reset();
        MCUSR= 0x00;
        WDTCSR |= ( _BV(WDCE) | _BV(WDE) ); // timed sequence !
        WDTCSR = 0x00;

        // turn all pins to inputs + pull-up on
        // saved about another 0.5mA on my board
        DDRA = 0x00;
        DDRB = 0x00;
        PORTA = 0xFF;
        PORTB = 0xFF;

        /*
         * now configure the pins we actually need
         */
        DDRB |= _BV(PB0);	// display enable pin as output
        PORTB |= _BV(PB0);	// pullup on

        DDRB |= _BV(PB2);   // led pin as output
        PORTB |= _BV(PB2);  // led on

        // USI stuff

        DDRB |= _BV(PB1);   // latch pin as output

        DDRA |= _BV(PA5);	// as output (DO)
        DDRA |= _BV(PA4);	// as output (USISCK)
        DDRA &= ~_BV(PA6);	// as input (DI)
        PORTA |= _BV(PA6);	// pullup on (DI)

        /* setup ADC
         *
         *   using single conversion mode
         *   --> setup in read_adc() for every conversion necessary!
         *
         */

        PORTA &= _BV(PA1);  // turn internal pull-up off

        // sleep mode
        set_sleep_mode(SLEEP_MODE_IDLE);

        /*
         * getting ready
         */
        sei(); // turn global irq flag on
        setup_system_ticker();
        signal_reset();
        setup_timer1_ctc(); // disable for adc_test()
}

void fade_in(uint8_t start_at, uint16_t fade_delay)
{
        uint8_t ctr1;
        for (ctr1 = start_at; (ctr1 > 0); ctr1--) {
                brightness = ctr1;
                delay(fade_delay);
        }
}

void fade_out(uint8_t start_at, uint16_t fade_delay)
{
        uint8_t ctr1;
        for (ctr1 = start_at; ctr1 <= 128; ctr1++) {
                brightness = ctr1;
                delay(fade_delay);
        }
}

uint16_t time(void)
{
        uint8_t _sreg = SREG;
        uint16_t time;
        cli();
        time = system_ticks;
        SREG = _sreg;
        return time;
}

void delay(uint16_t ticks)
{
        uint16_t start_time = time();
        while ((time() - start_time) < ticks) {
                // just wait here
        }
}

void signal_reset(void)
{
        __TOGGLE_LED;
        delay(1000);
        __TOGGLE_LED;
        delay(1000);
        __TOGGLE_LED;
        delay(1000);
        __TOGGLE_LED;
        delay(1000);
        /*
        // only for debugging
        while(1) {
                if(!(PINA & _BV(PA0))) {
                        __LED_ON;
                } else {
                        __LED_OFF;
                }
                if(!(PINA & _BV(PA1))) {
                        __LED_ON;
                } else {
                        __LED_OFF;
                }
                if(!(PINA & _BV(PA2))) {
                        __LED_ON;
                } else {
                        __LED_OFF;
                }
        }
        */
}

/*
Functions dealing with hardware specific jobs / settings
*/

inline uint8_t spi_transfer(uint8_t data)
{
        USIDR = data;
        USISR = _BV(USIOIF);	// clear flag

        while ((USISR & _BV(USIOIF)) == 0) {
                USICR = (1 << USIWM0) | (1 << USICS1) | (1 << USICLK) | (1 << USITC);
        }
        return USIDR;
}

inline void setup_system_ticker(void)
{
        /* save SREG and turn interrupts off globally */
        uint8_t _sreg = SREG;
        cli();
        /* using timer0 */
        /* setting prescaler to 8 */
        TCCR0B |= _BV(CS01);
        TCCR0B &= ~(_BV(CS00) | _BV(CS02));
        /* set WGM mode 0 */
        TCCR0A &= ~(_BV(WGM01) | _BV(WGM00));
        TCCR0B &= ~_BV(WGM02);
        /* normal operation - disconnect PWM pins */
        TCCR0A &= ~(_BV(COM0A1) | _BV(COM0A0) | _BV(COM0B1) | _BV(COM0B0));
        /* enabling overflow interrupt */
        TIMSK0 |= _BV(TOIE0);
        /* restore SREG */
        SREG = _sreg;
}

inline void setup_timer1_ctc(void)
{
        uint8_t _sreg = SREG; /* save SREG */
        cli(); /* disable all interrupts while messing with the register setup */

        /* multiplexed TRUE-RGB PWM mode (quite dim) */
        /* set prescaler to 256 */
        TCCR1B |= (_BV(CS12));
        TCCR1B &= ~(_BV(CS11) | _BV(CS10));
        /* set WGM mode 4: CTC using OCR1A */
        TCCR1A &= ~(_BV(WGM11) | _BV(WGM10));
        TCCR1B |= _BV(WGM12);
        TCCR1B &= ~_BV(WGM13);
        /* normal operation - disconnect PWM pins */
        TCCR1A &= ~(_BV(COM1A1) | _BV(COM1A0) | _BV(COM1B1) | _BV(COM1B0));
        /* set top value for TCNT1 */
        OCR1A = __OCR1A_max;
        /* enable COMPA isr */
        TIMSK1 |= _BV(OCIE1A);
        /* restore SREG with global interrupt flag */
        SREG = _sreg;
}

ISR(TIM0_OVF_vect) /* on attiny2313/4313 this is named TIMER0_OVF_vect */
{
        system_ticks++;
}

ISR(TIM1_COMPA_vect) /* on attiny2313/4313 this is named TIMER1_COMPA_vect */
{
        /* Framebuffer interrupt routine */
        __DISPLAY_OFF;

        /*
         * init as off and accept that 128 = 0% and 0 = 100%
         * if inited as on, it won't go down to zero brightness
         * which is more annoying than not getting to 100%
         *
         */
        static uint8_t data = 0;
        static uint8_t index = 0;
        static uint16_t OCR1A_TMP = 0;

        /* starts with index = 0 */
        /* now calculate when to run the next time and turn on all 8-ch */

        switch(index) {
        case 0:
                OCR1A_TMP = brightness;
                index++;
                break;
        case 1:
                if ( brightness == 128) {
                        /* if the leds should be at 0%, do nothing */
                } else {
                        data = 0xFF; // turn all 8-ch on
                }
                /* calculate when to turn everything off */
                OCR1A_TMP = ( 128 - brightness );
                index++;
                break;
        case 2:
                /* cycle completed, reset everything */
                data = 0;
                index = 0;
                /* immediately restart */
                OCR1A_TMP = 0;
                /* DON'T increase the index counter ! */
                break;
        default:
                break;
        }

        __LATCH_LOW;
        spi_transfer(data);	// send the date for each MBI5168
        __LATCH_HIGH;

        __DISPLAY_ON;

        OCR1A = OCR1A_TMP;
}

void adc_test(uint8_t channel)
{
        __TOGGLE_LED;
        delay(50);
        __TOGGLE_LED;
        delay(50);
        __TOGGLE_LED;
        delay(50);
        __TOGGLE_LED;

        uint8_t tmp = read_adc(channel);

        __DISPLAY_OFF;
        __LATCH_LOW;
        spi_transfer(tmp);
        __LATCH_HIGH;
        __DISPLAY_ON;
}

uint8_t read_adc(uint8_t channel)
{
        ADCSRA |= _BV(ADEN);    // enable ADC
        ADCSRA |= ( _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0) ); // set prescaler to 128 for stable readings
        ADCSRB |= _BV(ADLAR);   // set to left-aligned. we only need 8-bit and read ADCH only
        ADMUX &= ~( _BV(REFS1) | _BV(REFS0) );          // VREF = VCC
        ADMUX &= ~( _BV(MUX2) | _BV(MUX1) | _BV(MUX0) ); // reset to channel PA0

        ADMUX |= channel; // valid values for this board: 0,1,2,3,7
        ADCSRA |= _BV(ADSC);    // start conversion
        while( ADCSRA & _BV(ADSC) ) {
                // wait until ADC is done
        }
        return ADCH;
}

/*
void adjust_fade_delay(uint8_t channel)
{
        auto_adj_fade_delay = 1024 - 4* (uint16_t)(read_adc(7));
}
*/
