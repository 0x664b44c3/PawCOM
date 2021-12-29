#ifndef HARDWARE_H
#define HARDWARE_H
#include <stdint.h>

struct configBlock
{
	uint8_t volume;
	uint8_t sidetone;
	uint8_t mic_gain_and_pwr;
	uint8_t call_vol;
	uint8_t reserved[4];
	uint8_t crc;
};


//beep task (tone+duration)
struct tone_task
{
	uint8_t volume;
	uint16_t freq;
	uint16_t duration;
};

extern struct tone_task toneQueue[8];
extern uint8_t tones_queued;
extern int8_t enc_ctr;

//cfg flag bits and masks
enum
{
	cfgMicPower = 0x80,
	cfgMicGainMask = 0x7f
};
//system confog vars (in a struct)
extern struct configBlock sysCfg;


void init_hw(void);
void update_gpio(void);
void init_beep(void);
void init_timers(void);
void push_tone(uint8_t replace, uint16_t freq, uint16_t duration, uint8_t volume);
void beep_setvol(uint8_t volume);
void update_volume(void);
void update_sidetone( void );



//syste state vars
extern volatile uint8_t mute_line;
extern volatile uint8_t beep_18;
extern volatile uint8_t call_in;
extern volatile uint8_t call_send;
extern volatile uint8_t call_led;
extern volatile uint8_t mic_mute;


#endif // HARDWARE_H
