#include "timer.h"

//Timer Events
bool volatile TimerEvent250ms, TimerEvent1s,TimerEvent2s, TimerEvent4000ms, TimerEvent5s,
	TimerEvent8500ms, TimerEvent10s, TimerEvent30s, TimerEvent1min,
	Timer3min, TimerEvent5min, TimerEvent1hr, TimerEvent24h;


extern void my_work_handler(struct k_work *work);

K_WORK_DEFINE(my_work, my_work_handler);

void my_timer_handler(struct k_timer *dummy)
{
	k_work_submit(&my_work);
}

K_TIMER_DEFINE(my_timer, my_timer_handler, NULL);


void TIMER_INIT (uint16_t x){


	k_timer_start(&my_timer, K_MSEC(x), K_MSEC(x));


}