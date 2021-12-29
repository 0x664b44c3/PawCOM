#include "gptimer.h"
#include <avr/interrupt.h>
volatile uint16_t systick=0;

#define NTMR 8

#define ntimers 8

volatile uint16_t gptimer[8];
volatile uint8_t timer_used=0x00;
volatile uint16_t tick=0,cmtick=0;
volatile uint16_t tickhelper=0;
volatile uint8_t ledblink=0;

void timer_init() {
	timer_used=0;
	for(uint8_t i=0; i<NTMR; ++i)
		gptimer[i]=0;
}

static inline void timer_tick()
{
	++systick;
	for(uint8_t i=0;i<NTMR;++i)
		if (gptimer[i])
			--gptimer[i];
}
volatile uint8_t cycle=0;
ISR(TIMER2_COMPA_vect) // 8kHz
{
	cycle+=16;
	if (cycle==0 )
		timer_tick();

}

uint8_t timer_reg() {
	uint8_t i=0;
	if (timer_used==0xff)
		return 0xff;
	while(timer_used&_BV(i))
		i++; //find a free slot
	timer_used|=_BV(i);
	gptimer[i]=0;
	return i;
}

uint16_t timer_get(uint8_t id) {
	id&=7;
	return gptimer[id];
}

uint8_t timer_expired(uint8_t id) {
	id&=7;
	return (gptimer[id]==0)?1:0;
}

void timer_set(uint8_t id, uint16_t val) {
	id&=7;
	gptimer[id]=val;
}

void timer_free(uint8_t id) {
	id&=7;
	timer_used&=~_BV(id);
}

uint8_t timers_used(void) {
		uint8_t x=0;
		for (uint8_t i=0;i<8;i++)
			if (timer_used&_BV(i))
				x++;
		return x;
}
