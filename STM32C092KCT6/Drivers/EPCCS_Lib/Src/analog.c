#include <stdint.h>
#include <stdio.h>
#include "stm32c0xx_hal.h"
#include "parse_huart1.h"
#include "analog.h"

extern ADC_HandleTypeDef hadc1;

#define ANALOG_CHANNELS  6U
#define ANALOG_REPEAT_MS 2000U
#define ADC_VREF_MV      3300U
#define ADC_FULL_SCALE   4096U

// Labels match the board silkscreen (ADC1_IN2..ADC1_IN7 on PA2..PA7).
static const char *const channel_names[ANALOG_CHANNELS] = {
    "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADC6"
};

typedef enum
{
    ANALOG_REPEAT_NONE = 0,
    ANALOG_REPEAT_F,
    ANALOG_REPEAT_D,
} analog_repeat_t;

static analog_repeat_t analog_repeat = ANALOG_REPEAT_NONE;
static uint32_t analog_repeat_next;

// ADC1 is configured with a fixed sequencer for CHANNEL_2..CHANNEL_7;
// one HAL_ADC_Start services all six conversions in order.
static void ReadAllAdc(uint32_t raw[ANALOG_CHANNELS])
{
    HAL_ADC_Start(&hadc1);
    for (uint32_t i = 0; i < ANALOG_CHANNELS; i++)
    {
        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
        raw[i] = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
}

// Output is chunked (one printf per channel) to keep each call short,
// matching the original AVR approach of writing the response in pieces.
static void PrintAnalogf(void)
{
    uint32_t raw[ANALOG_CHANNELS];
    ReadAllAdc(raw);
    for (uint32_t i = 0; i < ANALOG_CHANNELS; i++)
    {
        uint32_t mv = raw[i] * ADC_VREF_MV / ADC_FULL_SCALE;
        if (i == 0)
            printf("{\"%s\":\"%lu\"", channel_names[i], (unsigned long)mv);
        else if (i < ANALOG_CHANNELS - 1)
            printf(",\"%s\":\"%lu\"", channel_names[i], (unsigned long)mv);
        else
            printf(",\"%s\":\"%lu\"}\r\n", channel_names[i], (unsigned long)mv);
    }
}

static void PrintAnalogd(void)
{
    uint32_t raw[ANALOG_CHANNELS];
    ReadAllAdc(raw);
    for (uint32_t i = 0; i < ANALOG_CHANNELS; i++)
    {
        if (i == 0)
            printf("{\"%s\":\"%lu\"", channel_names[i], (unsigned long)raw[i]);
        else if (i < ANALOG_CHANNELS - 1)
            printf(",\"%s\":\"%lu\"", channel_names[i], (unsigned long)raw[i]);
        else
            printf(",\"%s\":\"%lu\"}\r\n", channel_names[i], (unsigned long)raw[i]);
    }
}

// /analog? -> ADC1..ADC6 in millivolts (mV = raw * 3300 / 4096), repeats every 2s
void Analogf(void)
{
    if (arg_count != 0)
    {
        printf("{\"err\":\"AnalogArgCount\"}\r\n");
        return;
    }

    PrintAnalogf();
    analog_repeat = ANALOG_REPEAT_F;
    analog_repeat_next = HAL_GetTick() + ANALOG_REPEAT_MS;
}

// /adc? -> ADC1..ADC6 raw 12-bit counts, repeats every 2s
void Analogd(void)
{
    if (arg_count != 0)
    {
        printf("{\"err\":\"AdcArgCount\"}\r\n");
        return;
    }

    PrintAnalogd();
    analog_repeat = ANALOG_REPEAT_D;
    analog_repeat_next = HAL_GetTick() + ANALOG_REPEAT_MS;
}

// Non-blocking: re-print the armed reading every ANALOG_REPEAT_MS.
void AnalogRepeatCheck(void)
{
    if (analog_repeat == ANALOG_REPEAT_NONE)
    {
        return;
    }

    if ((int32_t)(HAL_GetTick() - analog_repeat_next) < 0)
    {
        return;
    }

    if (analog_repeat == ANALOG_REPEAT_F)
    {
        PrintAnalogf();
    }
    else
    {
        PrintAnalogd();
    }

    analog_repeat_next = HAL_GetTick() + ANALOG_REPEAT_MS;
}

// Called as soon as any new command line arrives, before dispatch.
void AnalogRepeatCancel(void)
{
    analog_repeat = ANALOG_REPEAT_NONE;
}
