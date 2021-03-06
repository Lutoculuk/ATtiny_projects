#ifndef IR_receiver_h
#define IR_receiver_h

//#define DEBUG
#define TRANSLATE_REPEAT_CODE	// instead of outputting 'repeat code' output the previously recognized IR code

#if defined(__AVR_ATmega168__)
#define IR_pin          PB0
#define IR_PORT         PORTB
#define IR_PIN          PINB
#define IR_DIR          DDRB
#define PC_PIN_MASK     PCMSK0
#define PC_PIN_MASK_BIT PCINT0
#define PC_ISR_REG      PCICR
#define PC_ISR_BIT      PCIE0
#endif

#if defined(__AVR_ATtiny85__)
#define IR_pin              PB1
#define IR_PORT             PORTB
#define IR_PIN              PINB
#define IR_DIR              DDRB
#define PC_PIN_MASK         PCMSK
#define PC_PIN_MASK_BIT     PCINT1
#define PC_ISR_REG          GIMSK
#define PC_ISR_BIT          PCIE
#endif

// all elements EXCEPT (REPEAT_CODE, MISMATCH and NOT_SURE_YET) must be in the same
// order (if present at all) as in the PROGMEM array IRsignals! These 3 special elements
// must ALWAYS be there.
typedef enum {
    VOL_DOWN,
    VOL_UP,
    ARROW_UP,
    ARROW_DOWN,
    DIGIT_1,
    DIGIT_2,
    DIGIT_3,
    DIGIT_0_OR_10,
    // DO NOT EDIT THESE LAST 3!
    REPEAT_CODE = 66,
    MISMATCH = 77,		// only returned if something goes terribly wrong
    NOT_SURE_YET = 99,	// this should NEVER be returned as a result
} IR_code_t;

void init_IR(void);
uint8_t IR_available(void);
IR_code_t eval_IR_code(void);

#endif
