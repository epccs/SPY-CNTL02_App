#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32c0xx_hal.h"
#include "parse_huart1.h"
#include "i2c1_cmd.h"
#include "i2c1_monitor.h"

extern I2C_HandleTypeDef hi2c1;

static uint8_t mon_addr = 0; // 0 = inactive, else 7-bit 0x08..0x77

static uint8_t mon_rxbuf[I2C1_BUFFER_LENGTH]; // scratch for Slave_Seq_Receive_IT
static uint8_t mon_txbuf[I2C1_BUFFER_LENGTH]; // echoed back on the next master read
static uint8_t mon_tx_len;

static uint8_t mon_print_buf[I2C1_BUFFER_LENGTH]; // snapshot ready for I2c1MonitorCheck
static uint8_t mon_print_len;

// Copy newly received bytes into the echo buffer, and into the print
// snapshot if the previous snapshot has already been printed.
static void mon_capture(uint8_t len)
{
    if (len == 0)
    {
        return;
    }
    if (mon_print_len == 0)
    {
        memcpy(mon_print_buf, mon_rxbuf, len);
        mon_print_len = len;
    }
    memcpy(mon_txbuf, mon_rxbuf, len);
    mon_tx_len = len;
}

// /imon? 8..119 -> start monitoring I2C1 as a slave at the given 7-bit address
void I2c1Monitor(void)
{
    if (arg_count != 1)
    {
        printf("{\"err\":\"%sArgCount\"}\r\n", command);
        return;
    }

    uint8_t a = is_arg_in_uint8_range(0, 0x08, 0x77);
    if (!a)
    {
        return;
    }

    if (mon_addr != 0)
    {
        I2c1MonitorCancel();
    }

    mon_tx_len = 1;
    mon_txbuf[0] = 0;
    mon_print_len = 0;

    hi2c1.Init.OwnAddress1 = (uint16_t)(a << 1);
    HAL_I2C_Init(&hi2c1);
    HAL_I2C_EnableListen_IT(&hi2c1);
    mon_addr = a;
}

// Address-match callback: arm the receive or transmit sequence for this transaction.
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    if (hi2c->Instance != I2C1)
    {
        return;
    }

    (void)AddrMatchCode;
    if (TransferDirection == I2C_DIRECTION_TRANSMIT)
    {
        HAL_I2C_Slave_Seq_Receive_IT(hi2c, mon_rxbuf, I2C1_BUFFER_LENGTH, I2C_LAST_FRAME);
    }
    else
    {
        HAL_I2C_Slave_Seq_Transmit_IT(hi2c, mon_txbuf, mon_tx_len, I2C_LAST_FRAME);
    }
}

// Full-length receive completed (master wrote exactly I2C1_BUFFER_LENGTH bytes).
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if ((hi2c->Instance != I2C1) || (mon_addr == 0))
    {
        return;
    }
    mon_capture(I2C1_BUFFER_LENGTH);
}

// Partial receive (master STOPped early) or NACK on transmit; capture what was received.
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if ((hi2c->Instance != I2C1) || (mon_addr == 0))
    {
        return;
    }

    if (hi2c->ErrorCode & HAL_I2C_ERROR_AF)
    {
        uint8_t n = (uint8_t)(hi2c->pBuffPtr - mon_rxbuf);
        if ((n > 0) && (n <= I2C1_BUFFER_LENGTH))
        {
            mon_capture(n);
        }
    }
    hi2c->ErrorCode = HAL_I2C_ERROR_NONE;
}

// Every slave transaction with I2C_LAST_FRAME ends here; re-arm for the next one.
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if ((hi2c->Instance == I2C1) && (mon_addr != 0))
    {
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

// Called from the main loop's idle branch: print any captured monitor data.
void I2c1MonitorCheck(void)
{
    if ((mon_addr == 0) || (mon_print_len == 0))
    {
        return;
    }

    printf("{\"monitor_0x%X\":[", mon_addr);
    for (uint8_t i = 0; i < mon_print_len; i++)
    {
        if (i > 0)
        {
            printf(",");
        }
        printf("{\"data\":\"0x%X\"}", mon_print_buf[i]);
    }
    printf("]}\r\n");
    mon_print_len = 0;
}

// Called before dispatching a new command line: stop monitoring I2C1.
void I2c1MonitorCancel(void)
{
    if (mon_addr == 0)
    {
        return;
    }

    mon_addr = 0;
    if (hi2c1.State == HAL_I2C_STATE_LISTEN)
    {
        HAL_I2C_DisableListen_IT(&hi2c1);
    }
    hi2c1.Instance->OAR1 &= ~I2C_OAR1_OA1EN; // stop ACKing the monitor address
}
