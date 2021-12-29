#include <inttypes.h>
#ifndef __TIMER_H
#define __TIMER_H

enum {
	TMR_INVALID=0xff
};
void timer_init(void);
uint8_t timer_reg(void);
uint16_t timer_get(uint8_t id);
uint8_t timer_expired(uint8_t id);
void timer_free(uint8_t id);
void timer_set(uint8_t id, uint16_t val);

extern volatile uint16_t systick;
#endif
