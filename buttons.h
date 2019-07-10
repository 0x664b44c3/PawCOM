#ifndef BUTTONS_H
#define BUTTONS_H
#include <stdint.h>

enum buttonEvents
{
	btnNone    = 0x0000,
	btnEncoder = 0x0001,
	btnPTT     = 0x0002,
	btnCall    = 0x0003,
	btnAux     = 0x0004,
	btnMask    = 0x00ff,
	evtMask    = 0xff00,
	evtDown    = 0x0100,
	evtUp      = 0x0200,
	evtPressed = 0x0400,
	evtHold    = 0x0800,
	evtShort   = 0x1000
};


uint8_t event_queueSize(void);
uint16_t event_get(void);
void event_push(uint16_t evt);

struct btn_context;
struct btn_context * button_createContext(void);

void button_proc(uint8_t id, uint8_t state);
uint8_t button_register(uint8_t code);
uint8_t button_state(uint8_t id);
#endif // BUTTONS_H
