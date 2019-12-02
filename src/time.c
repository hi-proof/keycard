#include "time.h"
#include "app_timer.h"

APP_TIMER_DEF(time_timer);

static volatile uint32_t accum_ms = 0;
static volatile uint32_t last_ticks = 0;

static void timer_handler(void * context)
{
    last_ticks = app_timer_cnt_get();
    accum_ms += 1000;
}

void time_init()
{
    app_timer_create(&time_timer, APP_TIMER_MODE_REPEATED, timer_handler);
    app_timer_start(time_timer, APP_TIMER_TICKS(1000), NULL);
}

uint32_t time_millis()
{
    uint32_t ticks_old = last_ticks;
    uint32_t ticks_new = app_timer_cnt_get();
    uint32_t ticks_diff = app_timer_cnt_diff_compute(ticks_new, ticks_old);
    uint32_t ms = ticks_diff * ( (APP_TIMER_CONFIG_RTC_FREQUENCY + 1 ) * 1000 ) / APP_TIMER_CLOCK_FREQ;

    return accum_ms + ms;
}
