#include <avr/io.h>
#include <avr/interrupt.h>
#include "can_api.h"

// gFlag
#define UPDATE_STATUS 0

#define MOB_WHEEL_SPEED_SEND 0

#define LED_WRITE_REGISTER PORTB
#define LED_PIN PB0

#define FRONT_READ_REGISTER PINB
#define LEFT_READ_REGISTER PINB

#define FRONT_PIN PB3
#define LEFT_PIN PB4

#define LED_DDR DDRB
#define LED_PIN PB0

volatile uint8_t gFlag = 0x00;
volatile uint8_t wheel_speed_current_count = 0;

ISR(PCINT0_vect) {
    // Interrupt for pin PB4
    // Whenever PB4 is triggered, flip the LED
    // PB5 is the sense_in pin
    // not necessary to add any additional logic in this interrupt since
    // PB5 (PCINT5) is the only interrupt enabled (see PCMSK0 in main)
    LED_WRITE_REGISTER ^= _BV(LED_PIN);
    wheel_speed_current_count += 1;
}

void initTimer(void) {
    // Target frequency: 200Hz
    TCCR0A = _BV(WGM01); // Clear Timer on Compare match (CTC) mode
    TCCR0B |= _BV(CS02) | _BV(CS00); // clkio/1024 prescaler
    OCR0A = 20; // set value of compare match A
    // 4MHz CPU, prescaler 1024, compare match on 20 -> (4000000/1024/20) ~= 196 Hz
    TIMSK0 |= _BV(OCIE0A); // enable interrupt on compare match A
}

ISR(TIMER0_COMPA_vect) {
	gFlag |= _BV(UPDATE_STATUS);
}

void setCANIDLEN(uint16_t * CAN_ID, uint16_t * CAN_LEN) {
    /**
     * Check which board this is and set the ID and LEN based on that
     */
    int front_set = bit_is_set(FRONT_READ_REGISTER, FRONT_PIN);
    int left_set = bit_is_set(LEFT_READ_REGISTER, LEFT_PIN);

    if(front_set && left_set) {
        *CAN_ID = CAN_ID_WHEEL_SPEED_FL;
        *CAN_LEN = CAN_LEN_WHEEL_SPEED_FL;
    } else if (front_set && !left_set) {
        *CAN_ID = CAN_ID_WHEEL_SPEED_FR;
        *CAN_LEN = CAN_LEN_WHEEL_SPEED_FR;
    } else if (!front_set && left_set) {
        *CAN_ID = CAN_ID_WHEEL_SPEED_BL;
        *CAN_LEN = CAN_LEN_WHEEL_SPEED_BL;
    } else if (!front_set && !left_set) {
        *CAN_ID = CAN_ID_WHEEL_SPEED_BR;
        *CAN_LEN = CAN_LEN_WHEEL_SPEED_BR;
    }
}

void reportSpeed(uint16_t CAN_ID, uint16_t CAN_LEN) {
    /**
     * Report speed over CAN
     */
    uint8_t wheel_speed_msg[2] = {0, 0};
    wheel_speed_msg[0] = 0x00; // no error codes defined yet
    wheel_speed_msg[1] = wheel_speed_current_count;
    wheel_speed_current_count = 0;

    if (CAN_ID && CAN_LEN) {
        // Only transmit if CAN_ID and CAN_LEN are set
        // If CAN_ID or CAN_LEN are somehow 0 we don't want to introduce incorrect CAN messages to the bus.
        CAN_transmit(MOB_WHEEL_SPEED_SEND, CAN_ID, CAN_LEN, wheel_speed_msg);
    }

}


int main(void) {
    // Set LED to output
    LED_DDR |= _BV(LED_PIN);
    
    // Enable interrupts
    sei();
    PCICR |= _BV(PCIE0);
    PCMSK0 |= _BV(PCINT5);

    // Init timer and CAN
    initTimer();
    CAN_init(CAN_ENABLED);
    
    // Set CAN ID and CAN LEN
    uint16_t * CAN_ID = 0;
    uint16_t * CAN_LEN = 0;
    setCANIDLEN(CAN_ID, CAN_LEN);

    while(1) {
        if(bit_is_set(gFlag, UPDATE_STATUS)) {
            gFlag &= ~_BV(UPDATE_STATUS);
            reportSpeed(*CAN_ID, *CAN_LEN);
        }
    }
}

