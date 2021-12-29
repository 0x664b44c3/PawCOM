#include <string.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdio.h>
#include "i2c.h"
#include "gptimer.h"
#include "buttons.h"
#include "hardware.h"
#include "crc.h"

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
	frontLEDs = 0x0f,
	frontLEDPtt = 0x10,
	frontBtnPtt = 0x20,
	frontBtnCall= 0x40,
	frontLedCall= 0x80u,
	frontOuputMask=0x9fu
};


#define MENU_HOLD_TIME 10000

//countdown counter for call lamp
uint8_t call_sig=0;

//tone duration timer
uint8_t tmr_tones=TMR_INVALID;
//call signalling timneout
uint8_t tmr_call_delay=TMR_INVALID;
//beeper mode
uint8_t tone_mode = 0;


extern volatile uint8_t tone_inc;
void signalling_task(void)
{
	if (tmr_tones == TMR_INVALID)
		tmr_tones = timer_reg();
	if (tmr_call_delay == TMR_INVALID)
		tmr_call_delay = timer_reg();

	if (call_in)
	{
		call_sig = 16;
		mute_line = 0;

		if (timer_expired(tmr_call_delay))
		{
			tones_queued = 0;
			if (tone_mode==0)
			{
				beep_setvol(sysCfg.call_vol);
				tone_inc=32;
				timer_set(tmr_tones, 125);
				tone_mode=1;
			}
			else
			{
				if (timer_expired(tmr_tones))
				{
					beep_setvol(sysCfg.call_vol);
					timer_set(tmr_tones, 125);
					tone_inc=32-tone_inc;
				}
			}
		}
	}
	else
	{
		timer_set(tmr_call_delay, 3000);
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
					inc/=16000;
					tone_inc = inc;
				}
				else
					tone_inc = 0;
				timer_set(tmr_tones, toneQueue[0].duration);
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
	lvlSideTone=1,
	lvlMicGain=2
};
uint8_t menu_level = lvlVolume;

uint8_t tmr_menuEscape = TMR_INVALID;

uint8_t old_btn_press=0;
uint8_t tmr_btn_press = 0xff;

void save_config( void )
{
	sysCfg.crc = crc8(&sysCfg, sizeof(struct configBlock) - 1);
	eeprom_write_block(&sysCfg, (void*) 0x10, sizeof(struct configBlock));
}

void menu_finish( void )
{
	push_tone(1,800, 100, 10);
	push_tone(0,600, 100, 10);
	menu_level = lvlVolume;
	save_config();
}


void encoder_task(void)
{
	if (TMR_INVALID == tmr_btn_press)
	{
		tmr_btn_press = timer_reg();
	}

	int8_t delta = enc_ctr - enc_old;

	if ((menu_level != lvlVolume) && (delta))
		timer_set(tmr_menuEscape, MENU_HOLD_TIME);

	switch (menu_level) {
		case lvlVolume:
			if (delta)
			{
				mute_line = 0;
				update_gpio();
			}
			if (delta > 0)
			{
				if (sysCfg.volume < 31)
				{
					++sysCfg.volume;
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
				if (sysCfg.volume > 0)
				{
					--sysCfg.volume;
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
				if ((delta > 0) && (sysCfg.sidetone<15))
				{
					sysCfg.sidetone++;
				}
				if ((delta < 0) && (sysCfg.sidetone>0))
				{
					sysCfg.sidetone--;
				}
				update_sidetone();
				push_tone(1,2000,15,9);
			}
			break;
		case lvlMicGain:
			if (delta)
			{
				if ((delta > 0) && (sysCfg.mic_gain_and_pwr & cfgMicGainMask) < 3)
				{
					sysCfg.mic_gain_and_pwr++;
				}
				if ((delta < 0) && (sysCfg.mic_gain_and_pwr & cfgMicGainMask) >0)
				{
					sysCfg.mic_gain_and_pwr--;
				}
				push_tone(1,2000,15,9);
			}
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

	tmr_menuEscape = timer_reg();

	sei();

	_delay_ms(1);

	buffer[0] = 0x80;
	buffer[1] = 0x9c;
	_twi_send_data(0x50, buffer, 2);
	__twi_wait_ready();
	buffer[0] = 0x00;
	buffer[1] = 0xff;
	_twi_send_data(0x50, buffer, 2);
	__twi_wait_ready();
	buffer[0] = 0x00 | 0x1f;
	buffer[1] = 0x40 | 0x1f;
	buffer[2] = 0x84;
	_twi_send_data(0x52, buffer, 3);
	__twi_wait_ready();

	mute_line = 0;

	mic_mute = 1;
	beep_18 = 0;
	uint8_t tmr_blink = timer_reg();
	update_gpio();

	eeprom_read_block(&sysCfg, (void*) 0x10, sizeof(struct configBlock));
	uint8_t actual_crc = crc8(&sysCfg, sizeof(struct configBlock) - 1);
	if (sysCfg.crc != actual_crc)
	{
		sysCfg.mic_gain_and_pwr = 0x80;
		sysCfg.call_vol = 10;
		sysCfg.sidetone = 0x0f;
	}

	sysCfg.volume = 15;

	//default settings

	update_sidetone();
	update_volume();

	if (sysCfg.crc == actual_crc)
	{
		push_tone(1, 250, 100, 6);
		push_tone(0, 0, 50, 6);
		push_tone(0, 500, 100, 6);
		push_tone(0, 0, 50, 6);
		push_tone(0, 1000, 100, 6);
	}
	else
	{
		push_tone(1, 250, 100, 6);
		push_tone(0, 0, 50, 6);
		push_tone(0, 250, 100, 6);
		push_tone(0, 0, 50, 6);
		push_tone(0, 250, 100, 6);
	}
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

	//wait till beep sequence done
	while(tones_queued)
	{
		update_gpio();
		signalling_task();
	}
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
			{ //this is a bit more complex....
				switch(menu_level)
				{
					case lvlVolume:
						if (evt & evtDown)
						{
							mute_line = 1 - mute_line;
							push_tone(1, mute_line?400:800, 50, 6);
							push_tone(0,  0, 50, 6);
							push_tone(0, mute_line?400:800, 50, 6);
						}
						if (evt & evtHold)
						{
							mute_line = 0;
							push_tone(1,800, 1000, 10);
							menu_level = lvlSideTone;
							timer_set(tmr_menuEscape, MENU_HOLD_TIME);
						}
						break;
					case lvlSideTone:
						if (evt & evtDown)
						{
							menu_level = lvlMicGain;
							push_tone(1,800, 250, 10);
							timer_set(tmr_menuEscape, MENU_HOLD_TIME);
						}
						break;
					case lvlMicGain:
						if (evt & evtDown)
						{
							menu_level = lvlVolume;
							menu_finish();
						}
						break;
					default:
						menu_level = lvlVolume;
				}
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
			case btnCall:
				if (menu_level != lvlMicGain)
					break;
				if (evt & evtDown)
				{
					timer_set(tmr_menuEscape, MENU_HOLD_TIME);
					sysCfg.mic_gain_and_pwr ^= cfgMicPower;
				}
		}
		if (button_state(button_PTT) || ptt_hold)
		{
			mic_mute = 0;
		}
		else
		{
			if (timer_expired(tmr_LastPTT))
			{
				mic_mute = 1;
			}
		}
		if (menu_level != lvlMicGain)
			call_send = button_state(button_Call);




		uint8_t leds = 0;

		if (timer_expired(tmr_blink))
		{
			update_gpio();
			timer_set(tmr_blink, 125);

			if ((call_sig) && (ctr & 2))
			{
				leds |= frontLedCall;
				if (ctr&1)
					--call_sig;
			}


			switch (menu_level)
			{
				case lvlVolume:
					if ((mute_line == 0) || (ctr&4))
						leds |= frontLED1;
					break;
				case lvlSideTone:
					leds |=frontLED2;
					break;

				case lvlMicGain:
					switch (sysCfg.mic_gain_and_pwr & cfgMicGainMask)
					{
						case 0:
							leds = 0x0f ^ 0x01;
							break;
						case 1:
							leds = 0x0f ^ 0x02;
							break;
						case 2:
							leds = 0x0f ^ 0x04;
							break;
						case 3:
						default:
							leds = 0x0f ^ 0x08;
							break;
					}

					if (ctr&2)
						leds = 0x0f;

					if (sysCfg.mic_gain_and_pwr & cfgMicPower)
						leds|= frontLedCall;
					break;
			}
			if (mic_mute == 0)
				leds |= frontLEDPtt;
			front_out = ~leds;

			if (timer_expired(tmr_menuEscape) && (menu_level!=lvlVolume))
				menu_finish();
			ctr++;
		}
	}
}
