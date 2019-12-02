#pragma once

#include <stdint.h>

typedef struct _tone_t
{
    uint16_t frequency;
    uint16_t duration;
} tone_t;

void tone_init(uint8_t pin);
void tone_play(uint32_t frequency, uint32_t duration);
void tone_play_sequence(tone_t tones[], uint8_t seq_length, void (*done)());
