#include "tone.h"
#include "nrf_drv_pwm.h"

static nrf_drv_pwm_t m_pwm0 = NRF_DRV_PWM_INSTANCE(0);


static tone_t * tone_active_sequence = NULL;
static uint8_t tone_active_seq_length = 0;
static uint8_t tone_active_seq_index = 0;
static void (*tone_sequence_done)() = NULL;


void tone_play(uint32_t frequency, uint32_t duration);


static void tone_handler(nrf_drv_pwm_evt_type_t event_type)
{
    if (event_type == NRF_DRV_PWM_EVT_FINISHED) {
        if (tone_active_sequence) {
            if (++tone_active_seq_index >= tone_active_seq_length) {
                tone_active_sequence = NULL;
                if (tone_sequence_done) {
                    tone_sequence_done();
                }
                return;
            }

            tone_t * t = &tone_active_sequence[tone_active_seq_index];
            tone_play(t->frequency, t->duration);
        }
    }
}

void tone_init(uint8_t pin)
{
    nrf_drv_pwm_config_t const config0 =
    {
        .output_pins =
        {
            pin,
            NRF_DRV_PWM_PIN_NOT_USED,
            NRF_DRV_PWM_PIN_NOT_USED,
            NRF_DRV_PWM_PIN_NOT_USED,
        },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock   = NRF_PWM_CLK_125kHz,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = 0,
        .load_mode    = NRF_PWM_LOAD_COMMON,
        .step_mode    = NRF_PWM_STEP_AUTO
    };
    APP_ERROR_CHECK(nrf_drv_pwm_init(&m_pwm0, &config0, tone_handler));
}

void tone_play(uint32_t frequency, uint32_t duration)
{
    uint16_t top_value =  (1.0 / frequency) / 0.000008;
    if (frequency == 0) {
        top_value = 125;
    }    

    nrf_drv_pwm_stop(&m_pwm0, true);

    nrf_pwm_configure(m_pwm0.p_registers, NRF_PWM_CLK_125kHz, NRF_PWM_MODE_UP, top_value);

    static uint16_t /*const*/ seq_values[1];
    seq_values[0] = (frequency == 0) ? 0 : top_value / 2;

    nrf_pwm_sequence_t const seq = {
        .values.p_common     = seq_values,
        .length              = NRF_PWM_VALUES_LENGTH(seq_values),
        .repeats             = 0,// duration / (1.0 / frequency) - 1,
        .end_delay           = 0
    };

    uint16_t rep_count = (frequency == 0) ? duration : (duration / 1000.0) / (1.0 / frequency);

    if (rep_count == 0) {
        rep_count = 1;
    }

    (void)nrf_drv_pwm_simple_playback(&m_pwm0, &seq, rep_count, NRF_DRV_PWM_FLAG_STOP);
}

void tone_play_sequence(tone_t tones[], uint8_t seq_length, void (*done)())
{
    tone_active_sequence = tones;
    tone_active_seq_length = seq_length;
    tone_sequence_done = done;
    tone_active_seq_index = 0;

    tone_play(tones[0].frequency, tones[0].duration);
}
