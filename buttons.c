#include <buttons.h>

volatile uint8_t evt_queue_wr=0;
volatile uint8_t evt_queue_rd=0;
volatile uint8_t __evt_queued = 0;
volatile uint16_t eventQueue[16];

#define MAX_BUTTONS 8
#define DETECT_THRESH 5
#define DEBOUNCE_MAX 4
#define SHORT_THRESH 250
#define HOLD_THRESH 1500
#define REPEAT_THRESH 250
#define REPEAT_RATE 50

struct btn_context
{
	uint8_t code;
	uint8_t repeat_ctr;
	uint8_t last_state;
	uint8_t debounce_counter;
	uint16_t hold_counter;
};

static struct btn_context __btnCtx[MAX_BUTTONS];
uint8_t nButtons=0;


uint8_t event_queueSize()
{
	    return __evt_queued;
}

uint16_t event_get()
{
	if (evt_queue_rd == evt_queue_wr)
		return btnNone;
	uint16_t ret = eventQueue[evt_queue_rd++];
	evt_queue_rd&=0x0f;
	__evt_queued--;
	return ret;
}
void event_push(uint16_t evt)
{
	if (__evt_queued == 16)
		return;
	eventQueue[evt_queue_wr++] = evt;
	evt_queue_wr&=0x0f;
	++__evt_queued;
}

void button_proc(uint8_t id, uint8_t state)
{
	if (id>=nButtons)
		return;
	struct btn_context * ctx = &__btnCtx[id];
	uint8_t code = ctx->code;
	if (state)
	{
		if (!ctx->last_state)
		{
			if (ctx->hold_counter == DETECT_THRESH)
			{
				event_push(evtDown | code);
				event_push(evtPressed | code);
				ctx->debounce_counter = DEBOUNCE_MAX;
				ctx->last_state = 1;
				ctx->repeat_ctr = REPEAT_THRESH;
			}
		}
		if (ctx->repeat_ctr == 0)
		{
			if (REPEAT_RATE)
			{
				event_push(evtPressed | code);
				ctx->repeat_ctr = REPEAT_RATE;
			}
		}
		else
		{
			ctx->repeat_ctr--;
		}
		if (ctx->hold_counter == HOLD_THRESH)
			event_push(evtHold | code);
		if (ctx->hold_counter < 8000) // value is arbitrary but larger than long hold value
			++ctx->hold_counter;
	}
	else
	{
		if (ctx->debounce_counter == 0)
		{
			if (ctx->last_state)
			{
				event_push(evtUp
				           | ((ctx->hold_counter < SHORT_THRESH)?evtShort:0)
				           | code
				           );
			}
			ctx->last_state = 0;
			ctx->repeat_ctr = 0;
			ctx->hold_counter = 0;
		}
		else
		{
			ctx->debounce_counter--;
		}
	}
}

uint8_t button_register(uint8_t code)
{
	if (nButtons >= MAX_BUTTONS)
		return 0xff;
	__btnCtx[nButtons].code = code;
	__btnCtx[nButtons].last_state = 0;
	__btnCtx[nButtons].debounce_counter = 0;
	__btnCtx[nButtons].hold_counter     = 0;
	return nButtons++;
}
