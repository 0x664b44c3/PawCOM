#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "hardware.h"
#include "i2c.h"

//system state vars
volatile uint8_t mute_line=1;
volatile uint8_t beep_18=1;
volatile uint8_t call_in=0;
volatile uint8_t call_send=0;
volatile uint8_t call_led=0;
volatile uint8_t mic_mute=0;


const uint8_t sine_tab[] = {0x80, 0x8c, 0x98, 0xa4, 0xb0, 0xbb, 0xc6, 0xd0,
                            0xd9, 0xe2, 0xe9, 0xf0, 0xf5, 0xf9, 0xfc, 0xfe,
                            0xfe, 0xfe, 0xfc, 0xf9, 0xf5, 0xf0, 0xe9, 0xe2,
                            0xd9, 0xd0, 0xc6, 0xbb, 0xb0, 0xa4, 0x98, 0x8c,
                            0x80, 0x73, 0x67, 0x5b, 0x4f, 0x44, 0x39, 0x2f,
                            0x26, 0x1d, 0x16, 0x10, 0x0a, 0x06, 0x03, 0x01,
                            0x01, 0x01, 0x03, 0x06, 0x0a, 0x0f, 0x16, 0x1d,
                            0x26, 0x2f, 0x39, 0x44, 0x4f, 0x5b, 0x67, 0x73};

inline uint8_t sineLookup(uint8_t in) {
	uint8_t v1=0, v2=0;
	uint8_t step = in>>2;
	uint8_t sub=in&3;
	v1 = sine_tab[step++];
	if ((step<sizeof(sine_tab))&&(sub)) {
		uint16_t accu=0;
		v2 = sine_tab[step];
		for (register uint8_t i=0;i<4;++i) {
			if (i<sub)
				accu+=v2;
			else
				accu+=v1;
		}
		accu>>=2;
		v1=accu;
	}
	return v1;
}

struct configBlock sysCfg;

int8_t enc_ctr=0;
volatile uint8_t enc_btn=0;
void update_gpio(void)
{
	if (mute_line)
		PORTB&=~_BV(2);
	else
		PORTB|=_BV(2);

	if (beep_18)
		PORTB|=_BV(1);
	else
		PORTB&=~_BV(1);

	call_in = (PINB & _BV(0))==0;
	enc_btn = (PINC&_BV(5))?0:1;

	if (sysCfg.mic_gain_and_pwr & 0x80)
		PORTC&= ~_BV(3);
	else
		PORTC |= _BV(3);

	if (mic_mute)
		PORTD|= _BV(2);
	else
		PORTD&=~_BV(2);

	switch(sysCfg.mic_gain_and_pwr & cfgMicGainMask)
	{
		case 0:
			PORTD|= _BV(4); // -10 (*)
			PORTD&=~_BV(3); // +20 ( ) -10
			break;
		case 1:
			PORTD&=~_BV(4); // -10 ( )
			PORTD&=~_BV(3); // +20 ( ) 0
			break;
		case 2:
			PORTD|= _BV(4); // -10 (*)
			PORTD|= _BV(3); // +20 (*) +10
			break;
		case 3:
		default:
			PORTD&=~_BV(4); // -10 ( )
			PORTD|= _BV(3); // +20 (*) +20
			break;
	}
	if (call_send)
		PORTA|=_BV(0);
	else
		PORTA&=~_BV(0);
}

void init_beep(void)
{
	//PWM stage 0A on
	TCCR0A = _BV(WGM00) | _BV(WGM01) | _BV(COM0A1);
	TCCR0B = 1;
	OCR0A = 0x80;
}

volatile uint8_t tone_accu=0, tone_vol=0x7f;
volatile uint8_t tone_inc = 0;
volatile uint8_t cycle;
static inline void do_audio(void)
{
	if (tone_inc)
	{
		int16_t tone = sineLookup(tone_accu);
		tone -=128;
		tone *=tone_vol;
		tone/=256;
		tone+=128;

		OCR0A=tone;//(tone_accu&128)?0x80 + tone_vol: 0x80-tone_vol;
		tone_accu+=tone_inc;
	}
	else
	{
		OCR0A=0x80;
	}
}

uint8_t oldPins=0;

ISR(TIMER2_OVF_vect) // 16kHz
{	do_audio();
	uint8_t pins = PINC&0xc0;
	if (oldPins!=pins)
	{
		if (pins == 0x00)
		{
			if (oldPins == 0x80)
				enc_ctr--;
			if (oldPins == 0x40)
				enc_ctr++;
		}
	}
	oldPins = pins;
}

void init_timers(void)
{
	timer_init();
	TCCR2A =  _BV(WGM20) | _BV(WGM21);
	TCCR2B =  _BV(WGM22) | 2;
	OCR2A = 249/2;
	TIMSK2 = _BV(OCIE2A) | _BV(TOIE2);
}

struct tone_task toneQueue[8];
uint8_t tones_queued=0;

void push_tone(uint8_t replace, uint16_t freq, uint16_t duration, uint8_t volume)
{
	if (replace)
		tones_queued=0;
	if (tones_queued == 8)
		tones_queued=7;
	toneQueue[tones_queued].freq = freq;
	toneQueue[tones_queued].duration = duration;
	toneQueue[tones_queued].volume = volume;
	++tones_queued;
}


const uint8_t volumes[6] = {127,  // 0dB
                            90,  // -3dB
                            64,  // -6dB
                            45,  // -9dB
                            32,  // -12dB
                            23   // -15dB
                           };

void beep_setvol(uint8_t volume)
{
	uint8_t tmp = volume;
	if (volume)
	{
		if (tmp>12)
			tmp=12;
		tmp--;
		tone_vol = volumes[5 - (tmp%6)];
		beep_18 = (tmp>5)?0:1;
		update_gpio();
	}
	else
	{
		tone_vol = 0;
	}
}

void update_volume(void)
{
	static uint8_t buffer[2];
	buffer[0] = 0x00 | ((31 - sysCfg.volume) & 0x1f);
	buffer[1] = 0x40 | ((31 - sysCfg.volume) & 0x1f);
	_twi_send_data(0x52, buffer, 2);
	__twi_wait_ready();
}

void update_sidetone( void )
{
	static uint8_t buffer[2];
	buffer[0] = 0x00;
	buffer[1] = sysCfg.sidetone * 0x11;
	_twi_send_data(0x50, buffer, 2);
}

void init_hw(void)
{
	DDRA = 0x09;
	DDRB = 0xde;
	PORTC = 0xe8;
	DDRC = 0x08;
	PORTD = 0x04;
	DDRD = 0x3e;
	SPCR0 = _BV(SPE0)|_BV(MSTR0)|_BV(SPR00)|_BV(DORD0);
}
