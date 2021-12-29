/** @file twi.c
 * @author Andreas Lang
 * @c (C) Andreas Lang, 2009
 * @version 1.0
 *******************************
 * @brief a master mode twi driver
**/


#include "i2c.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/io.h>

typedef enum  {
	__twi_idle,
	__twi_request_write, ///< init a write transfer
	__twi_request_read, ///< init a read transfer
	__twi_running, ///< transfer in progress
	__twi_waitForStop

} __twi_status_t;
volatile __twi_status_t __twi_status=__twi_idle;

volatile uint8_t* __twi_data_ptr;
volatile uint8_t __twi_slaveAddr=0, __twi_bytesLeft=0, __twi_flags=0;

#define __TWI_NOSTOP 1
#define __TWI_NACK 2


void _i2c_init(void) {
	TWCR0 = 0;
	asm("nop");
	TWSR0 = _BV(TWPS00);
	TWBR0 = 16;
	TWCR0 |= _BV(TWEN0)|_BV(TWIE0);
}


inline void __i2c_handler(void) {

	uint8_t ret=_BV(TWEN0)|_BV(TWINT0)|(TWCR0&_BV(TWIE0));
	switch(TWSR0&0xf8) {
		case 0x08: //start condition transmitted
		case 0x10: //repeated start condition transmitted
			if (__twi_status==__twi_request_write) {
				TWDR0=__twi_slaveAddr&0xfe;
				break;
			}
			if (__twi_status==__twi_request_read) {
				TWDR0=__twi_slaveAddr|0x01;
				break;
			}
			break;
		case 0x18: //SLA+W transmitted, ACK
			__twi_status=__twi_running;
			if (__twi_bytesLeft) {
				TWDR0=*__twi_data_ptr++;
				__twi_bytesLeft--;
			}else{
				ret|=_BV(TWSTO0);
				__twi_status=__twi_idle;
			}
			break;
		case 0x20: //SLA+W transmitted, NACK
			ret|=_BV(TWSTO0);
			__twi_status=__twi_idle;
			__twi_flags|=__TWI_NACK;
			break;
		case 0x28: //data byte sent, ACK
			if (__twi_bytesLeft) {
				TWDR0=*__twi_data_ptr++;
				__twi_bytesLeft--;
			}else{
				if (!(__twi_flags&__TWI_NOSTOP)) {
					ret|=_BV(TWSTO0);
				}
				__twi_status=__twi_idle;
			}
			break;
		case 0x30: //data byte sent, NACK
			ret|=_BV(TWSTO0);
			__twi_status=__twi_idle;
			__twi_flags|=__TWI_NACK;
			break;
		case 0x38: //arbitration lost
			ret|=_BV(TWSTA0);
			break;
		case 0x40: //SLA+R transmitted, ACK
			__twi_status=__twi_running;
			if (__twi_bytesLeft>1) {
				ret|=_BV(TWEA0);
			}
			break;
		case 0x48: //SLA+R transmitted, NACK
			ret|=_BV(TWSTO0);
			__twi_status=__twi_idle;
			__twi_flags|=__TWI_NACK;
			break;
		case 0x50: //data byte received, ACK returned
			if (__twi_bytesLeft) {
				*__twi_data_ptr++=TWDR0;
				__twi_bytesLeft--;
			}
			if (__twi_bytesLeft>1) {
				ret|=_BV(TWEA0);
			}
			break;
		case 0x58: //data byte received, NACK returned
			if (__twi_bytesLeft) {
				*__twi_data_ptr++=TWDR0;
				__twi_bytesLeft--;
			}
			if (!(__twi_flags&__TWI_NOSTOP)) {
				ret|=_BV(TWSTO0);
			}
			__twi_status=__twi_idle;
			break;
		case 0x00: //bus error
			ret|=_BV(TWSTO0)|_BV(TWEA0);
			__twi_status=__twi_idle;
			break;
		case 0xf8: //no relevant status info
			return;
		default: //anything else
			break;
	}
	TWCR0=ret;
}


/** @brief twi_task - use only if not in IRQ mode!
*/
void _i2c_task(void) {
	if(TWCR0&_BV(TWIE0)) return;
	if (TWCR0&(1<<TWINT0)) {
		__i2c_handler();
	}
}

/** @brief for irq based twi
*/
ISR(TWI0_vect) {
	__i2c_handler();
}


inline uint8_t __twi_ready(void) {
	if ((__twi_status==__twi_idle))
		//&&(!(TWCR&_BV(TWSTO))))
		return 1;
	return 0;
}

void __twi_wait_ready(void) {
	while(!__twi_ready()) {
		_i2c_task();
	}
}


uint8_t _twi_probeSlave(uint8_t addr)
{
	__twi_wait_ready();
	__twi_slaveAddr=addr&0xfe;
	__twi_bytesLeft=0;
	__twi_flags=0;
	__twi_status=__twi_request_write;
	TWCR0=(TWCR0&(_BV(TWIE0)))|_BV(TWSTA0)|_BV(TWEN0);
	__twi_wait_ready();
	if (__twi_flags & __TWI_NACK)
		return 0;
	return 1;
}

void _twi_send_data_no_stop(uint8_t slaveAddr, uint8_t * data, uint8_t length) {
	__twi_wait_ready();
	__twi_data_ptr=data;
	__twi_slaveAddr=slaveAddr&0xfe;
	__twi_bytesLeft=length;
	__twi_flags=__TWI_NOSTOP;
	__twi_status=__twi_request_write;
	TWCR0=(TWCR0&(_BV(TWIE0)|_BV(TWIE0)))|_BV(TWSTA0)|_BV(TWEN0);
}

void _twi_send_data(uint8_t slaveAddr, uint8_t * data, uint8_t length) {
	__twi_wait_ready();
	__twi_data_ptr=data;
	__twi_slaveAddr=slaveAddr&0xfe;
	__twi_bytesLeft=length;
	__twi_flags=0;
	__twi_status=__twi_request_write;
	TWCR0=(TWCR0&(_BV(TWIE0)|_BV(TWIE0)))|_BV(TWSTA0)|_BV(TWEN0);
}

void _twi_read_data(uint8_t slaveAddr, uint8_t * data, uint8_t length) {
	__twi_wait_ready();
	__twi_data_ptr=data;
	__twi_slaveAddr=slaveAddr|0x01;
	__twi_bytesLeft=length;
	__twi_flags=0;
	__twi_status=__twi_request_read;
	TWCR0=(TWCR0&(_BV(TWIE0)|_BV(TWIE0)))|_BV(TWSTA0)|_BV(TWEN0);
}

