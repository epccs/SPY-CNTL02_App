#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32c0xx_hal.h"
#include "main.h"
#include "parse_huart1.h"
#include "dmx_output.h"

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;
extern TIM_HandleTypeDef htim16;

// Linker symbol marking the start of the reserved EEPROM page, see
// STM32C092XX_FLASH.ld and ee.c -- this project reuses the same page as the
// power-up default DMX frame instead of arbitrary addr/value storage.
extern uint8_t _eeprom_start[];
#define EEPROM_BASE ((uint32_t)_eeprom_start)

#define DMX_REFRESH_PERIOD_MS 25 // ~40 Hz
#define DMX_BREAK_US 100
#define DMX_MAB_US 16

// Ping-pong pairs for the SPI1 slave link. txBuf's content doesn't matter to
// the R-Pi -- only rxBuf (the data driving DMX1/2/3) is meaningful.
static uint8_t txBuf[2][DMX_BLOCK_SIZE];
static uint8_t rxBuf[2][DMX_BLOCK_SIZE];
static uint8_t active;

// The currently active 2 KB frame: written by the SPI1 ISR (live data) and
// by DmxOutputInit()/DmxSave() (EEPROM default); read by DmxRepeatCheck().
static uint8_t activeFrame[DMX_BLOCK_SIZE];

// Per-universe transmit buffers (start code + 512 channels), snapshotted
// from activeFrame at the start of each refresh cycle so an in-flight
// transmission is never corrupted by a concurrent SPI block update.
static uint8_t txFrame[3][1 + DMX_UNIVERSE_SIZE];

static uint32_t last_refresh_ms;

static void Arm(uint8_t idx)
{
    active = idx;
    HAL_SPI_TransmitReceive_DMA(&hspi1, txBuf[idx], rxBuf[idx], DMX_BLOCK_SIZE);
}

// Busy-waits using TIM16's free-running 1 MHz counter (16-bit, wraps every
// ~65.5 ms -- far longer than any delay used here, so plain subtraction
// handles the wraparound correctly).
static void DelayUs(uint16_t us)
{
    uint16_t start = __HAL_TIM_GET_COUNTER(&htim16);
    while ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim16) - start) < us)
    {
    }
}

static void SetPinOutput(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &init);
    HAL_GPIO_WritePin(port, pin, state);
}

static void SetPinUart(GPIO_TypeDef *port, uint16_t pin, uint32_t alternate)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = pin;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    init.Alternate = alternate;
    HAL_GPIO_Init(port, &init);
}

// Drives PA8/PB2/PA0 low (break) then high (mark-after-break) together, then
// hands them back to USART2/3/4 AF mode for the data phase.
static void SendBreakAndMab(void)
{
    SetPinOutput(APP_TX2_GPIO_Port, APP_TX2_Pin, GPIO_PIN_RESET);
    SetPinOutput(APP_TX3_GPIO_Port, APP_TX3_Pin, GPIO_PIN_RESET);
    SetPinOutput(APP_TX4_GPIO_Port, APP_TX4_Pin, GPIO_PIN_RESET);
    DelayUs(DMX_BREAK_US);

    HAL_GPIO_WritePin(APP_TX2_GPIO_Port, APP_TX2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(APP_TX3_GPIO_Port, APP_TX3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(APP_TX4_GPIO_Port, APP_TX4_Pin, GPIO_PIN_SET);
    DelayUs(DMX_MAB_US);

    SetPinUart(APP_TX2_GPIO_Port, APP_TX2_Pin, GPIO_AF1_USART2);
    SetPinUart(APP_TX3_GPIO_Port, APP_TX3_Pin, GPIO_AF5_USART3);
    SetPinUart(APP_TX4_GPIO_Port, APP_TX4_Pin, GPIO_AF9_USART4);
}

void DmxOutputInit(void)
{
    memcpy(activeFrame, (const void *)EEPROM_BASE, DMX_BLOCK_SIZE);
    memset(txBuf, 0, sizeof(txBuf));

    Arm(0);
    last_refresh_ms = HAL_GetTick();
}

void DmxRepeatCheck(void)
{
    if ((HAL_GetTick() - last_refresh_ms) < DMX_REFRESH_PERIOD_MS)
    {
        return;
    }
    if ((HAL_UART_GetState(&huart2) & HAL_UART_STATE_BUSY_TX) ||
        (HAL_UART_GetState(&huart3) & HAL_UART_STATE_BUSY_TX) ||
        (HAL_UART_GetState(&huart4) & HAL_UART_STATE_BUSY_TX))
    {
        return; // previous frame still transmitting; try again next loop
    }

    for (uint8_t u = 0; u < 3; u++)
    {
        txFrame[u][0] = 0x00; // DMX512 start code
        memcpy(&txFrame[u][1], &activeFrame[u * DMX_UNIVERSE_SIZE], DMX_UNIVERSE_SIZE);
    }

    SendBreakAndMab();

    HAL_UART_Transmit_IT(&huart2, txFrame[0], sizeof(txFrame[0]));
    HAL_UART_Transmit_IT(&huart3, txFrame[1], sizeof(txFrame[1]));
    HAL_UART_Transmit_IT(&huart4, txFrame[2], sizeof(txFrame[2]));

    last_refresh_ms = HAL_GetTick();
}

void DmxSave(void)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Page = (EEPROM_BASE - FLASH_BASE) / FLASH_PAGE_SIZE,
        .NbPages = 1,
    };
    uint32_t page_error;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &page_error);

    if (status == HAL_OK)
    {
        for (uint32_t i = 0; i < DMX_BLOCK_SIZE; i += sizeof(uint64_t))
        {
            uint64_t dword;
            memcpy(&dword, activeFrame + i, sizeof(dword));
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, EEPROM_BASE + i, dword);
            if (status != HAL_OK) break;
        }
    }

    HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        printf("{\"err\":\"SaveFlashErr_%lu\"}\r\n", (unsigned long)HAL_FLASH_GetError());
        return;
    }

    printf("{\"save\":\"ok\"}\r\n");
}

// Fires once CS deasserts after exactly DMX_BLOCK_SIZE bytes have been
// clocked in both directions. Re-arm first so the slave is ready before the
// R-Pi asserts CS for the next block, then publish the data just received as
// the new active frame.
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1)
    {
        return;
    }

    uint8_t finished = active;
    uint8_t next = finished ^ 1;

    Arm(next);

    memcpy(activeFrame, rxBuf[finished], DMX_BLOCK_SIZE);
}

// A glitch (e.g. CS toggled with the wrong byte count) shouldn't permanently
// stop the link -- re-arm the pair that was in flight and keep going.
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1)
    {
        return;
    }

    Arm(active);
}
