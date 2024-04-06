/*
fuses:
low 0xF2
high 0xDE
extended 0xFD
*/

/*
M1 PC0,PC1
M2 PC2,PD0
M3 PD1,PD2
M4 PD3,PD4
M5 PB6,PB7
M6 PD5,PD6
M7 PD7,PB0
M8 PB1,PB2

SW1 PC3
SW2 PC4
SW3 PC5
*/

#define F_CPU 8000000UL

#include <avr/sfr_defs.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>

#define SW1 _BV(PC3)
#define SW2 _BV(PC4)
#define SW3 _BV(PC5)

#define MODE_COUNT 5

uint8_t motorStates[8];
uint8_t portBuffers[3];
void setMotorDriverStates(uint8_t driverStates, uint8_t motorDirections) {
	for (uint_fast8_t i = 0; i < 8; i++) {
		motorStates[i] = (~motorDirections & driverStates & 1 << i) << 1 >> i | (motorDirections & driverStates & 1 << i) >> i;
	}
	
	portBuffers[0] = motorStates[4] << 6 | motorStates[7] << 1 | (motorStates[6] & 2) >> 1;
	portBuffers[1] = (motorStates[1] & 1) << 2 | motorStates[0];
	portBuffers[2] = (motorStates[6] & 1) << 7 | motorStates[5] << 5 | motorStates[3] << 3 | motorStates[2] << 1 | (motorStates[1] & 2) >> 1;
	
	PORTB &= 0b00111000;
	PORTB |= portBuffers[0];
	PORTC &= 0b11111000;
	PORTC |= portBuffers[1];
	PORTD &= 0b00000000;
	PORTD |= portBuffers[2];
}

int main(void) {
	DDRB |= 0b11000111;
	DDRC |= 0b00000111;
	DDRC &= ~0b00111000;
	DDRD |= 0b11111111;
	
	setMotorDriverStates(0, 0);
	
	while (EECR & _BV(EEPE));
	
	EEAR = 0;
	EECR |= _BV(EERE);
	uint8_t mode = EEDR;
	
	// normal operation
	if (PINC & SW1) {
		switch (mode) {
			// box and blocks
			case 1:
				while (1) {
					if (PINC & SW2) {
						setMotorDriverStates(255, 0);
					} else if (PINC & SW3) {
						setMotorDriverStates(255, 255);
					} else {
						setMotorDriverStates(0, 0);
					}
				}
				
				break;
				
			// binary counting
			case 2:
				while (1) {
					if (PINC & SW1) {
						setMotorDriverStates(255, 0);
					} else {
						setMotorDriverStates(0, 0);
					}
				}
				
				break;
				
			// middle finger
			case 3:
				while (1) {
					if (PINC & SW1) {
						setMotorDriverStates(255, 0);
					} else {
						setMotorDriverStates(0, 0);
					}
				}
			
				break;
			
			// individual DOF test
			case 4:
				while (1) {
					if (PINC & SW1) {
						setMotorDriverStates(255, 0);
					} else {
						setMotorDriverStates(0, 0);
					}
				}
			
				break;
			
			// mode 0, open/close
			default:
				while (1) {
					setMotorDriverStates(255, 0);
					_delay_ms(500);
					setMotorDriverStates(0, 0);
					_delay_ms(500);
				}

		}
	} else {
		EECR &= ~(_BV(EEPM1) | _BV(EEPM0));
		
		void writeEEPROM(uint8_t mode) {
			while (EECR & _BV(EEPE));
			
			EEDR = mode;
			EECR = EECR & ~_BV(EEPE) | _BV(EEMPE);
			EECR |= _BV(EEPE);
		}
		
		bool buttonsLocked = false;
		uint8_t lockedPinStates;
		void pollModeInput() {
			if (buttonsLocked) {
				while ((PINC & (SW2 | SW3)) == lockedPinStates);
				
				buttonsLocked = false;
				_delay_ms(50);
			} else {
				uint8_t pinStates = PINC;
				while (pinStates & SW2 && pinStates & SW3) {
					pinStates = PINC;
				}
			
				_delay_ms(10);
			
				if (PINC & pinStates & SW2 && PINC & pinStates & SW3) return;
			
				buttonsLocked = true;
				lockedPinStates = PINC & (SW2 | SW3);
			
				if (!(pinStates & SW2)) {
					mode = mode > 0 && mode < MODE_COUNT ? mode - 1 : MODE_COUNT - 1;
				} else if (!(pinStates & SW3)) {
					mode = (mode + 1) % MODE_COUNT;
				}
			
				writeEEPROM(mode);
			}
		}
		
		while (1) pollModeInput();
	}
}
