#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32c0xx_hal.h"
#include "main.h"
#include "parse_huart1.h"
#include "cs_io.h"

#define CS_ARG_MIN 1
#define CS_ARG_MAX 5

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    const char *name;
} cs_io_t;

// indexed 0..4 for CS1..CS5 (CS5 also drives CS6 from the same pin)
static const cs_io_t cs_io[CS_ARG_MAX] = {
    {CS1_GPIO_Port, CS1_Pin, "CS1"},
    {CS2_GPIO_Port, CS2_Pin, "CS2"},
    {CS3_GPIO_Port, CS3_Pin, "CS3"},
    {CS4_GPIO_Port, CS4_Pin, "CS4"},
    {CS5_6_GPIO_Port, CS5_6_Pin, "CS5_6"},
};

static void PrintCsState(const cs_io_t *io)
{
    GPIO_PinState state = HAL_GPIO_ReadPin(io->port, io->pin);
    printf("{\"%s\":\"%s\"}\r\n", io->name, (state == GPIO_PIN_SET) ? "HIGH" : "LOW");
}

// /iowrt 1..5,HIGH|LOW -> JSON {"CSn":"HIGH|LOW"}
void CsWrite(void)
{
    if (arg_count != 2)
    {
        printf("{\"err\":\"%sArgCount\"}\r\n", command);
        return;
    }

    uint8_t a = is_arg_in_uint8_range(0, CS_ARG_MIN, CS_ARG_MAX);
    if (!a)
    {
        return;
    }

    if (!((strcmp(arg[1], "HIGH") == 0) || (strcmp(arg[1], "LOW") == 0)))
    {
        printf("{\"err\":\"%sArg1_NaState\"}\r\n", command);
        return;
    }

    const cs_io_t *io = &cs_io[a - 1];
    HAL_GPIO_WritePin(io->port, io->pin, (strcmp(arg[1], "HIGH") == 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    PrintCsState(io);
}

// /iotog 1..5 -> JSON {"CSn":"HIGH|LOW"}
void CsToggle(void)
{
    if (arg_count != 1)
    {
        printf("{\"err\":\"%sArgCount\"}\r\n", command);
        return;
    }

    uint8_t a = is_arg_in_uint8_range(0, CS_ARG_MIN, CS_ARG_MAX);
    if (!a)
    {
        return;
    }

    const cs_io_t *io = &cs_io[a - 1];
    HAL_GPIO_TogglePin(io->port, io->pin);
    PrintCsState(io);
}
