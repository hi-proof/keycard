#include <stdio.h>
#include <string.h>
#include "nrf_drv_pwm.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "boards.h"
#include "bsp.h"
#include "app_timer.h"
#include "nrf_drv_clock.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "tone.h"
#include "time.h"

#define RAM_RETENTION_OFF       (0x00000003UL)  /**< The flag used to turn off RAM retention on nRF52. */

static nrf_drv_pwm_t m_pwm1 = NRF_DRV_PWM_INSTANCE(1);

#define PIN_BUZZER 16

static tone_t seq_good[] = {
    {2300, 130},
    {0, 50},
    {4000, 250},
};

static tone_t seq_bad[] = {
    {4000, 130},
    {0, 50},
    {2300, 250},
};

// shut down after 1 hour of idle
#define IDLE_AUTO_SHUTDOWN (1000 * 60 * 60 * 1)

static uint32_t system_off_time = IDLE_AUTO_SHUTDOWN;
static uint32_t system_idle_time = 0;

static nrf_pwm_values_individual_t led_seq_values = {0};
static nrf_pwm_sequence_t const    led_seq =
{
    .values.p_individual = &led_seq_values,
    .length              = NRF_PWM_VALUES_LENGTH(led_seq_values),
    .repeats             = 0,
    .end_delay           = 0
};

void set_leds(uint16_t red, uint16_t green, uint16_t yellow)
{
    led_seq_values.channel_0 = red;
    led_seq_values.channel_1 = yellow;
    led_seq_values.channel_2 = green;
}

void system_off( void )
{
    nrf_drv_pwm_stop(&m_pwm1, true);

    NRF_POWER->RAM[0].POWER = RAM_RETENTION_OFF;
    NRF_POWER->RAM[1].POWER = RAM_RETENTION_OFF;
    NRF_POWER->RAM[2].POWER = RAM_RETENTION_OFF;
    NRF_POWER->RAM[3].POWER = RAM_RETENTION_OFF;
    NRF_POWER->RAM[4].POWER = RAM_RETENTION_OFF;
    NRF_POWER->RAM[5].POWER = RAM_RETENTION_OFF;
    NRF_POWER->RAM[6].POWER = RAM_RETENTION_OFF;
    NRF_POWER->RAM[7].POWER = RAM_RETENTION_OFF;

    // Set nRF5 into System OFF. Reading out value and looping after setting the register
    // to guarantee System OFF in nRF52.
    NRF_POWER->SYSTEMOFF = 0x1;
    (void) NRF_POWER->SYSTEMOFF;
    while (true);
}

static void bsp_evt_handler(bsp_event_t evt)
{
    static bool off_pending = false;

    switch (evt)
    {
        // Button 1 - switch to the previous demo.
        case BSP_EVENT_KEY_0:
            (void)seq_good;
            tone_play_sequence(seq_good, sizeof(seq_good) / sizeof(seq_good[0]), NULL);
            set_leds(0, 700, 0);
            system_idle_time = time_millis() + 1000;
            system_off_time = time_millis() + IDLE_AUTO_SHUTDOWN;
            break;

        // Button 2 - switch to the next demo.
        case BSP_EVENT_KEY_1:
            (void)seq_bad;
            tone_play_sequence(seq_bad, sizeof(seq_bad) / sizeof(seq_bad[0]), NULL);
            set_leds(700, 0, 0);
            system_idle_time = time_millis() + 1000;
            system_off_time = time_millis() + IDLE_AUTO_SHUTDOWN;
            break;

        case BSP_EVENT_SLEEP:
            set_leds(0, 0, 0);
            system_idle_time = time_millis() + 10000;
            off_pending = true;
            break;

        case BSP_EVENT_SYSOFF:
            if (off_pending) {
                system_off();
            }
            break;

        default:
            return;
    }
}

static void init_bsp()
{
    APP_ERROR_CHECK(nrf_drv_clock_init());
    nrf_drv_clock_lfclk_request(NULL);

    APP_ERROR_CHECK(app_timer_init());
    APP_ERROR_CHECK(bsp_init(BSP_INIT_BUTTONS, bsp_evt_handler));
    APP_ERROR_CHECK(bsp_buttons_enable());

    bsp_event_to_button_action_assign(0, BSP_BUTTON_ACTION_LONG_PUSH, BSP_EVENT_SLEEP);
    bsp_event_to_button_action_assign(1, BSP_BUTTON_ACTION_LONG_PUSH, BSP_EVENT_SLEEP);
    bsp_event_to_button_action_assign(0, BSP_BUTTON_ACTION_RELEASE, BSP_EVENT_SYSOFF);
    bsp_event_to_button_action_assign(1, BSP_BUTTON_ACTION_RELEASE, BSP_EVENT_SYSOFF);

    nrf_drv_pwm_config_t const config_leds =
    {
        .output_pins =
        {
            BSP_LED_0 | NRF_DRV_PWM_PIN_INVERTED,
            BSP_LED_1 | NRF_DRV_PWM_PIN_INVERTED,
            BSP_LED_2 | NRF_DRV_PWM_PIN_INVERTED,
            NRF_DRV_PWM_PIN_NOT_USED,
        },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock   = NRF_PWM_CLK_1MHz,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = 1000,
        .load_mode    = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode    = NRF_PWM_STEP_AUTO
    };
    APP_ERROR_CHECK(nrf_drv_pwm_init(&m_pwm1, &config_leds, NULL));


    (void)nrf_drv_pwm_simple_playback(&m_pwm1, &led_seq, 1,
                                  NRF_DRV_PWM_FLAG_LOOP);
    led_seq_values.channel_0 = 500;
    led_seq_values.channel_1 = 500;
    led_seq_values.channel_2 = 500;
    (void) led_seq;


}


int main(void)
{
    init_bsp();
    tone_init(PIN_BUZZER);
    time_init();

    nrf_delay_ms(50);
    system_idle_time = 1000;

    if (bsp_button_is_pressed(0)) {
        bsp_evt_handler(BSP_EVENT_KEY_0);
    }
    if (bsp_button_is_pressed(1)) {
        bsp_evt_handler(BSP_EVENT_KEY_1);
    }

    for (;;)
    {
        // Wait for an event.
        __WFE();

        // Clear the event register.
        __SEV();
        __WFE();


        if (time_millis() > system_idle_time) {
            // sin wave for yellow?
            set_leds(0, 0, 800);
        }

        if (time_millis() > system_off_time) {
            system_off();
        }
    }
}
