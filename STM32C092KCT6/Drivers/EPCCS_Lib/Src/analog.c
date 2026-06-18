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

static const uint32_t channel_ids[ANALOG_CHANNELS] = {
    ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7
};

typedef enum
{
    ANALOG_REPEAT_NONE = 0,
    ANALOG_REPEAT_F,
    ANALOG_REPEAT_D,
} analog_repeat_t;

static analog_repeat_t analog_repeat = ANALOG_REPEAT_NONE;
static uint32_t analog_repeat_next;

// ADC1 runs continuous-conversion with DMA-into-this-buffer, so a report
// never waits on a conversion; it just reads whatever DMA last wrote.
static volatile uint16_t dma_raw[ANALOG_CHANNELS];

// Ascending 0-based indices (0=ADC1 .. 5=ADC6) of the channels currently in
// the ADC1 scan sequence, and how many of them there are.
static uint8_t selected_channels[ANALOG_CHANNELS] = {0, 1, 2, 3, 4, 5};
static uint8_t selected_count = ANALOG_CHANNELS;

static uint8_t SelectionChanged(const uint8_t *indices, uint8_t count)
{
    if (count != selected_count)
    {
        return 1;
    }
    for (uint8_t i = 0; i < count; i++)
    {
        if (indices[i] != selected_channels[i])
        {
            return 1;
        }
    }
    return 0;
}

// Reprogram ADC1's fixed-channel-number scan sequence to convert only the
// given channels, then restart continuous DMA acquisition so dma_raw[] keeps
// refreshing in the background. indices must be ascending.
static void ReconfigureChannels(const uint8_t *indices, uint8_t count)
{
    HAL_ADC_Stop_DMA(&hadc1);

    ADC_ChannelConfTypeDef sConfig = {0};
    for (uint8_t ch = 0; ch < ANALOG_CHANNELS; ch++)
    {
        uint8_t include = 0;
        for (uint8_t i = 0; i < count; i++)
        {
            if (indices[i] == ch)
            {
                include = 1;
                break;
            }
        }
        sConfig.Channel = channel_ids[ch];
        sConfig.Rank = include ? ADC_RANK_CHANNEL_NUMBER : ADC_RANK_NONE;
        HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    }

    for (uint8_t i = 0; i < count; i++)
    {
        selected_channels[i] = indices[i];
    }
    selected_count = count;

    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)dma_raw, count);
}

// Parse 0..MAX_ARGUMENT_COUNT channel-number args (1..6, matching the
// ADC1..ADC6 labels) into a sorted, de-duplicated list of 0-based indices.
// No args means "all six". Prints an error and returns 0 on bad input.
static uint8_t ParseChannelArgs(uint8_t *indices, uint8_t *count)
{
    if (arg_count == 0)
    {
        for (uint8_t i = 0; i < ANALOG_CHANNELS; i++)
        {
            indices[i] = i;
        }
        *count = ANALOG_CHANNELS;
        return 1;
    }

    *count = 0;
    for (uint8_t i = 0; i < arg_count; i++)
    {
        uint8_t ch = is_arg_in_uint8_range(i, 1, ANALOG_CHANNELS);
        if (!ch)
        {
            return 0;
        }
        uint8_t idx = ch - 1;

        uint8_t pos = *count;
        while ((pos > 0) && (indices[pos - 1] >= idx))
        {
            if (indices[pos - 1] == idx)
            {
                printf("{\"err\":\"%sArg%d_Dup\"}\r\n", command, i);
                return 0;
            }
            indices[pos] = indices[pos - 1];
            pos--;
        }
        indices[pos] = idx;
        (*count)++;
    }
    return 1;
}

static void PrintAnalogf(void)
{
    printf("{");
    for (uint8_t i = 0; i < selected_count; i++)
    {
        uint32_t mv = dma_raw[i] * ADC_VREF_MV / ADC_FULL_SCALE;
        printf("%s\"%s\":\"%lu\"", (i == 0) ? "" : ",", channel_names[selected_channels[i]], (unsigned long)mv);
    }
    printf("}\r\n");
}

static void PrintAnalogd(void)
{
    printf("{");
    for (uint8_t i = 0; i < selected_count; i++)
    {
        printf("%s\"%s\":\"%u\"", (i == 0) ? "" : ",", channel_names[selected_channels[i]], dma_raw[i]);
    }
    printf("}\r\n");
}

// Start ADC1 free-running (continuous conversion + circular DMA) on the
// default channel set, so dma_raw[] is already fresh before any command.
void AnalogInit(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)dma_raw, selected_count);
}

void Analogf(void)
{
    uint8_t indices[ANALOG_CHANNELS];
    uint8_t count;
    if (!ParseChannelArgs(indices, &count))
    {
        return;
    }

    if (SelectionChanged(indices, count))
    {
        ReconfigureChannels(indices, count);
    }
    PrintAnalogf();
    analog_repeat = ANALOG_REPEAT_F;
    analog_repeat_next = HAL_GetTick() + ANALOG_REPEAT_MS;
}

void Analogd(void)
{
    uint8_t indices[ANALOG_CHANNELS];
    uint8_t count;
    if (!ParseChannelArgs(indices, &count))
    {
        return;
    }

    if (SelectionChanged(indices, count))
    {
        ReconfigureChannels(indices, count);
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
