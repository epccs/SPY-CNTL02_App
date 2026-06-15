#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "stm32c0xx_hal.h"
#include "parse_huart1.h"
#include "i2c1_cmd.h"

extern I2C_HandleTypeDef hi2c1;

static uint8_t master_address = 0; // 7-bit I2C address the master will access
static uint8_t txBuffer[I2C1_BUFFER_LENGTH];
static uint8_t txBuffer_index = 0;

static void PrintWriteError(void)
{
    uint32_t err = HAL_I2C_GetError(&hi2c1);
    if (err & HAL_I2C_ERROR_AF)
    {
        printf("{\"error\":\"wrt_addr_nack\"}\r\n");
    }
    else if (err & HAL_I2C_ERROR_TIMEOUT)
    {
        printf("{\"error\":\"wrt_timeout\"}\r\n");
    }
    else
    {
        printf("{\"error\":\"wrt_error\"}\r\n");
    }
}

// /iscan? -> scan 7-bit addresses 0x08..0x77 for an ACK
void I2c1Scan(void)
{
    if (arg_count != 0)
    {
        printf("{\"err\":\"%sArgCount\"}\r\n", command);
        return;
    }

    printf("{\"scan\":[");
    uint8_t found = 0;
    for (uint16_t addr = 0x08; addr <= 0x77; addr++)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 1, I2C1_TIMEOUT_MS) == HAL_OK)
        {
            if (found)
            {
                printf(",");
            }
            printf("{\"addr\":\"0x%X\"}", addr);
            found = 1;
        }
    }
    printf("]}\r\n");
}

// /iaddr 1..127 -> set the address used by /iwrite, /iread? and /iscan?'s sibling commands
void I2c1Address(void)
{
    if (arg_count != 1)
    {
        printf("{\"err\":\"%sArgCount\"}\r\n", command);
        return;
    }

    uint8_t a = is_arg_in_uint8_range(0, 1, 127);
    if (!a)
    {
        return;
    }

    master_address = a;
    txBuffer_index = 0;
    printf("{\"master_address\":\"0x%X\"}\r\n", master_address);
}

// /ibuff [0..255[,...]] appends up to MAX_ARGUMENT_COUNT bytes; /ibuff? (no args) just prints
void I2c1TxBuffer(void)
{
    for (uint8_t i = 0; i < arg_count; i++)
    {
        if (!isdigit((unsigned char)arg[i][0]))
        {
            printf("{\"err\":\"%sArg%d_NaN\"}\r\n", command, i);
            return;
        }
        int val = atoi(arg[i]);
        if ((val < 0) || (val > 255))
        {
            printf("{\"err\":\"%sArg%d_OutOfRng\"}\r\n", command, i);
            return;
        }
        if (txBuffer_index >= I2C1_BUFFER_LENGTH)
        {
            printf("{\"err\":\"%sOVF\"}\r\n", command);
            txBuffer_index = 0;
            return;
        }
        txBuffer[txBuffer_index] = (uint8_t)val;
        txBuffer_index += 1;
    }

    printf("{\"txBuffer[%d]\":[", txBuffer_index);
    for (uint8_t i = 0; i < txBuffer_index; i++)
    {
        if (i > 0)
        {
            printf(",");
        }
        printf("{\"data\":\"0x%X\"}", txBuffer[i]);
    }
    printf("]}\r\n");
}

// /iwrite -> write the tx buffer to master_address, clearing it on success
void I2c1Write(void)
{
    if (arg_count != 0)
    {
        printf("{\"err\":\"%sArgCount\"}\r\n", command);
        return;
    }

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(master_address << 1),
                                                        txBuffer, txBuffer_index, I2C1_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        printf("{\"txBuffer\":\"wrt_success\"}\r\n");
        txBuffer_index = 0;
        return;
    }

    PrintWriteError();
}

// /iread? 1..32 -> if the tx buffer holds a command byte(s), write it (STOP) then
// read; otherwise just read. See README for the STOP-then-new-START note.
void I2c1Read(void)
{
    if (arg_count != 1)
    {
        printf("{\"err\":\"%sArgCount\"}\r\n", command);
        return;
    }

    uint8_t qty = is_arg_in_uint8_range(0, 1, I2C1_BUFFER_LENGTH);
    if (!qty)
    {
        return;
    }

    if (txBuffer_index > 0)
    {
        HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(master_address << 1),
                                                            txBuffer, txBuffer_index, I2C1_TIMEOUT_MS);
        if (status != HAL_OK)
        {
            PrintWriteError();
            return;
        }
        txBuffer_index = 0;
        printf("{\"txBuffer\":\"wrt_success\",");
    }
    else
    {
        printf("{");
    }

    uint8_t rxBuffer[I2C1_BUFFER_LENGTH];
    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(master_address << 1),
                                                       rxBuffer, qty, I2C1_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        printf("\"rxBuffer\":[");
        for (uint8_t i = 0; i < qty; i++)
        {
            if (i > 0)
            {
                printf(",");
            }
            printf("{\"data\":\"0x%X\"}", rxBuffer[i]);
        }
        printf("]}\r\n");
        return;
    }

    uint32_t err = HAL_I2C_GetError(&hi2c1);
    if (err & HAL_I2C_ERROR_AF)
    {
        printf("\"error\":\"rd_addr_nack\"}\r\n");
    }
    else if (err & HAL_I2C_ERROR_TIMEOUT)
    {
        printf("\"error\":\"rd_timeout\"}\r\n");
    }
    else
    {
        printf("\"error\":\"rd_error\"}\r\n");
    }
}
