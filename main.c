#include <string.h>
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include "i2c.h"
#include "gptimer.h"
#include <buttons.h>

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

typedef union {
	uint16_t u16;
	struct {
		uint8_t u8l;
		uint8_t u8h;
	};
} accu_t;

enum pcfBits {
	frontLED1 = 0x01,
	frontLED2 = 0x02,
	frontLED3 = 0x04,
	frontLED4 = 0x08,
	frontLEDPtt = 0x10,
	frontBtnPtt = 0x20,
	frontBtnCall= 0x40,
	frontLedCall= 0x80,
	frontOuputMask=0x9f
};

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


volatile uint8_t mute_line=1;
volatile uint8_t beep_18=1;
volatile uint8_t call_in=0;
volatile uint8_t call_send=0;
volatile uint8_t enc_btn=0;
volatile uint8_t mic_pwr=0;
volatile uint8_t mic_gain=0;
volatile uint8_t mic_mute=0;

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

	if (mic_pwr)
		PORTC&= ~_BV(3);
	else
		PORTC |= _BV(3);

	if (mic_mute)
		PORTD|= _BV(2);
	else
		PORTD&=~_BV(2);

	switch(mic_gain)
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
int8_t enc_ctr=0;
ISR(TIMER2_OVF_vect) // 8kHz
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
	OCR2A = 249;
	TIMSK2 = _BV(OCIE2A) | _BV(TOIE2);
}

struct tone_task
{
	uint8_t volume;
	uint16_t freq;
	uint16_t duration;
};
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

uint8_t tmr_tones=TMR_INVALID;
uint8_t tone_mode = 0;


const uint8_t volumes[6] = {127,  // 0dB
                            90,  // -3dB
                            64,  // -6dB
                            45,  // -9dB
                            32,  // -12dB
                            23   // -15dB
                           };

uint8_t call_vol = 4;
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

void signalling_task(void)
{
	if (tmr_tones == TMR_INVALID)
		tmr_tones = timer_reg();
	if (call_in)
	{
		tones_queued = 0;
		mute_line = 0;
		if (tone_mode==0)
		{
			tone_inc=32;
			timer_set(tmr_tones, 125);
			tone_mode=1;
		}
		else
		{
			if (timer_expired(tmr_tones))
			{
				beep_setvol(call_vol);
				timer_set(tmr_tones, 125);
				tone_inc=32-tone_inc;
			}
		}
	}
	else
	{
		if (tone_mode)
			timer_set(tmr_tones, 0);

		tone_mode=0;

		if (!tones_queued)
		{
			if (timer_expired(tmr_tones))
				tone_inc=0;
		}
		else
		{
			if (timer_expired(tmr_tones))
			{
				if (toneQueue[0].volume!=0xff)
					beep_setvol(toneQueue[0].volume);
				uint32_t inc = toneQueue[0].freq;
				if (inc)
				{
					inc*=256;
					inc/=8000;
					tone_inc = inc;
				}
				else
					tone_inc = 0;
				timer_set(tmr_tones, toneQueue[0].duration);
//				tone_inc = 64;
				for(uint8_t i=0; i<7;++i)
					toneQueue[i] = toneQueue[i+1];
				--tones_queued;
			}
		}
	}

}

uint8_t enc_old=0;
enum
{
	lvlVolume=0,
	lvlSideTone=1
};
uint8_t menu_level = lvlVolume;

uint8_t main_volume = 0x00;
uint8_t sidetone    = 0x0f;

void update_volume(void)
{
	static uint8_t buffer[2];
	buffer[0] = 0x00 | ((31 - main_volume) & 0x1f);
	buffer[1] = 0x40 | ((31 - main_volume) & 0x1f);
	_twi_send_data(0x52, buffer, 2);
	__twi_wait_ready();
}

void update_sidetone( void )
{
	static uint8_t buffer[2];
	buffer[0] = 0x00;
	buffer[1] = sidetone * 0x11;
	_twi_send_data(0x50, buffer, 2);
}

uint8_t old_btn_press=0;
uint8_t tmr_btn_press = 0xff;
enum {
	encBtnDown,
	encBtnUp,
	encBtnLong,
	encBtnDouble
};
void encoder_task(void)
{
	if (TMR_INVALID == tmr_btn_press)
	{
		tmr_btn_press = timer_reg();
	}

	int8_t delta = enc_ctr - enc_old;
	switch (menu_level) {
		case lvlVolume:
			if (delta)
			{
				mute_line = 0;
				update_gpio();
			}
			if (delta > 0)
			{
				if (main_volume < 31)
				{
					++main_volume;
					update_volume();
					push_tone(1,2000,15,9);
				}
				else
				{
					push_tone(1,500,50,9);
					push_tone(0,  0,20,9);
					push_tone(0,500,50,9);
				}
			}
			if (delta < 0)
			{
				if (main_volume > 0)
				{
					--main_volume;
					update_volume();
					push_tone(1,2000,15,9);
				}
				else
				{
					push_tone(1,250,50,9);
					push_tone(0,  0,20,9);
					push_tone(0,250,50,9);
				}
			}
			break;
		case lvlSideTone:
			if (delta)
			{
				if ((delta > 0) && (sidetone<15))
				{
					sidetone++;
				}
				if ((delta < 0) && (sidetone>0))
				{
					sidetone--;
				}
				push_tone(1,2000,15,9);
//				twi_buffer[0] = 0x00;
//				twi_buffer[1] = sidetone * 0x10;
//				_twi_send_data(0x50, twi_buffer, 2);
			}
			break;
		default:
			break;
	}


	enc_old = enc_ctr;
}

typedef enum {
	tgt_pcfFrontWrite = 0,
	tgt_pcfFrontQuery,
	tgt_pcfFrontRead,
	tgt_ds1881,
	tgt_max5395,
	tgtEnd
} twi_target_t;

twi_target_t twi_target = tgt_pcfFrontWrite;


uint8_t twi_buffer[3];
uint8_t twi_tasksel=0;
uint8_t front_out=0xff;
uint8_t front_inp=0xff;
void i2c_task(void)
{
	if (!__twi_ready())
		return;
	switch (twi_tasksel)
	{
		case tgt_pcfFrontWrite:
			twi_buffer[0] = (front_out | (~frontOuputMask));
			_twi_send_data(0x40, twi_buffer, 1);
			twi_tasksel = tgt_pcfFrontQuery;
			break;
		case tgt_pcfFrontQuery:
			_twi_read_data(0x40, twi_buffer, 1);
			twi_tasksel = tgt_pcfFrontRead;
			break;
		case tgt_pcfFrontRead:
			front_inp = twi_buffer[0];
			twi_tasksel = tgt_pcfFrontWrite;
			break;
	}
}


int main(void) {



	uint8_t buffer[8];
	init_hw();
	init_timers();
	init_beep();
	_i2c_init();

	sei();

	_delay_ms(1);

	buffer[0] = 0x80;
	buffer[1] = 0x9c;
	_twi_send_data(0x50, buffer, 2);

	_delay_ms(1);
	buffer[0] = 0x00;
	buffer[1] = 0xff;
	_twi_send_data(0x50, buffer, 2);
	_delay_ms(1);
	buffer[0] = 0x00 | 0x1f;
	buffer[1] = 0x40 | 0x1f;
	buffer[2] = 0x84;
	_twi_send_data(0x52, buffer, 3);

	mute_line = 0;
	mic_pwr = 1;
	tone_vol = 0x7f;
	mic_gain= 0;
	mic_mute = 0;
	beep_18 = 0;
	uint8_t tmr_blink = timer_reg();
	mute_line = 1;
	update_gpio();

	//load settings
	main_volume = 10;
	sidetone = 0x0f;
	update_volume();


	push_tone(1, 250, 100, 6);
	push_tone(0, 0, 50, 6);
	push_tone(0, 500, 100, 6);
	push_tone(0, 0, 50, 6);
	push_tone(0, 1000, 100, 6);
	__twi_wait_ready();
	buffer[0] = 0x00;
	_twi_send_data(0x40, buffer, 1);
	uint8_t ctr=0;

	uint8_t button_Enc = button_register(btnEncoder);
	uint8_t button_PTT = button_register(btnPTT);
	uint8_t button_Call = button_register(btnCall);
	uint8_t tmr_LastPTT = timer_reg();
	uint8_t ptt_hold=0;
	uint16_t old_systick=0;
	while(1) {
		update_gpio();
		signalling_task();
		encoder_task();
		i2c_task();
		if (systick != old_systick)
		{
			old_systick = systick;
			button_proc(button_Enc, (PINC&_BV(5))?0:1);
			button_proc(button_PTT, (front_inp & _BV(5))?0:1);
			button_proc(button_Call, (front_inp & _BV(6))?0:1);
			old_systick = systick;
		}
		uint16_t evt = event_get();
		switch(evt & btnMask)
		{
			case btnEncoder:
				if (evt & evtDown)
				{
					mute_line = 1 - mute_line;
					push_tone(1, mute_line?400:800, 50, 6);
					push_tone(0,  0, 50, 6);
					push_tone(0, mute_line?400:800, 50, 6);
				}
				break;
			case btnPTT:
				if (evt & evtDown)
				{
					mic_mute = 0;
					mute_line = 0;
					if (ptt_hold)
					{
						ptt_hold=0;
						push_tone(1, 400, 50, 6);
					}
					else
					{
						if (!timer_expired(tmr_LastPTT))
						{
							ptt_hold = 1;
							push_tone(1, 800, 150, 6);
						}
					}
					timer_set(tmr_LastPTT, 300);
				}
				break;
		}
		if (button_state(button_PTT) || ptt_hold)
		{
			mic_mute = 0;
			front_out &= ~frontLEDPtt;
		}
		else
		{
			if (timer_expired(tmr_LastPTT))
			{
				mic_mute = 1;
				front_out |= frontLEDPtt;
			}
		}
		call_send = button_state(button_Call);


		if (timer_expired(tmr_blink))
		{
			update_gpio();
			timer_set(tmr_blink, 1000);

			if ((mute_line) && (ctr&1))
				front_out |= frontLED1;
			else
				front_out &= ~frontLED1;

			ctr++;
		}
	}
}
