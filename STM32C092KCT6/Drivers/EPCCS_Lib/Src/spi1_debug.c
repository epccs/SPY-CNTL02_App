#include <stdint.h>
#include <stdio.h>
#include "stm32c0xx_hal.h"
#include "parse_huart1.h"
#include "spi1_debug.h"

extern SPI_HandleTypeDef hspi1;
extern CRC_HandleTypeDef hcrc;

// Ping-pong pairs: one pair is armed with the DMA while the other is being
// refilled/CRC'd, so the slave is always ready before the next CS assert.
static uint8_t txBuf[2][SPI1_DEBUG_BLOCK_SIZE];
static uint8_t rxBuf[2][SPI1_DEBUG_BLOCK_SIZE];
static uint32_t txCrc[2];
static uint8_t active;

static volatile uint32_t block_count;
static volatile uint32_t last_tx_crc;
static volatile uint32_t last_rx_crc;

static uint32_t prng_state;

// xorshift32 - good enough to exercise the link, not cryptographic
static uint32_t NextRandom(void)
{
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 17;
    prng_state ^= prng_state << 5;
    return prng_state;
}

static void FillRandom(uint8_t *buf)
{
    for (uint32_t i = 0; i < SPI1_DEBUG_BLOCK_SIZE; i += 4)
    {
        uint32_t r = NextRandom();
        buf[i] = (uint8_t)(r);
        buf[i + 1] = (uint8_t)(r >> 8);
        buf[i + 2] = (uint8_t)(r >> 16);
        buf[i + 3] = (uint8_t)(r >> 24);
    }
}

static uint32_t Crc32(uint8_t *buf)
{
    return HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, SPI1_DEBUG_BLOCK_SIZE);
}

// Generate fresh outgoing data for pair `idx` and cache its CRC for the next
// /spi? report once that pair's transfer completes.
static void RefillAndCrc(uint8_t idx)
{
    FillRandom(txBuf[idx]);
    txCrc[idx] = Crc32(txBuf[idx]);
}

static void Arm(uint8_t idx)
{
    active = idx;
    HAL_SPI_TransmitReceive_DMA(&hspi1, txBuf[idx], rxBuf[idx], SPI1_DEBUG_BLOCK_SIZE);
}

void Spi1DebugInit(void)
{
    prng_state = 0xA5A5A5A5u ^ HAL_GetTick();
    if (prng_state == 0)
    {
        prng_state = 1; // xorshift32 must never be seeded with 0
    }

    RefillAndCrc(0);
    RefillAndCrc(1);
    Arm(0);
}

void Spi1Status(void)
{
    printf("{\"blocks\":\"%lu\",\"tx_crc\":\"0x%08lX\",\"rx_crc\":\"0x%08lX\"}\r\n",
           (unsigned long)block_count, (unsigned long)last_tx_crc, (unsigned long)last_rx_crc);
}

// Fires once CS deasserts after exactly SPI1_DEBUG_BLOCK_SIZE bytes have been
// clocked in both directions. Re-arm first so the slave is ready before the
// R-Pi asserts CS for the next block, then process the pair that just finished.
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1)
    {
        return;
    }

    uint8_t finished = active;
    uint8_t next = finished ^ 1;

    Arm(next);

    last_tx_crc = txCrc[finished];
    last_rx_crc = Crc32(rxBuf[finished]);
    block_count++;

    RefillAndCrc(finished);
}

// A glitch (e.g. CS toggled with the wrong byte count) shouldn't permanently
// stop the debug counter -- re-arm the pair that was in flight and keep going.
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1)
    {
        return;
    }

    Arm(active);
}
