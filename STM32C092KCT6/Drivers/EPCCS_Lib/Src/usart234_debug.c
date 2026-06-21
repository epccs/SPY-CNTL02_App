#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32c0xx_hal.h"
#include "parse_huart1.h"
#include "usart234_debug.h"

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;

// Slot layout within each 2 KB SPI block:
//   slot0 (rx, bytes received from R-Pi this transaction) -> forwarded out USART2
//   slot1 (tx, bytes returned to R-Pi)  <- USART3 capture from the previous window
//   slot2 (tx, bytes returned to R-Pi)  <- USART4 capture from the previous window
//   slot3 (tx, bytes returned to R-Pi)  <- unused, always zero
#define SLOT0_OFFSET 0
#define SLOT1_OFFSET USART234_SLOT_SIZE
#define SLOT2_OFFSET (2 * USART234_SLOT_SIZE)
#define SLOT3_OFFSET (3 * USART234_SLOT_SIZE)

// Ping-pong pairs: one pair is armed with the SPI1 DMA while the other is
// being refilled from the latest USART3/USART4 captures.
static uint8_t txBuf[2][USART234_BLOCK_SIZE];
static uint8_t rxBuf[2][USART234_BLOCK_SIZE];
static uint8_t active;

// Live DMA capture targets for the window currently in progress.
static uint8_t rx3Window[USART234_SLOT_SIZE];
static uint8_t rx4Window[USART234_SLOT_SIZE];

// Frozen snapshot of the most recently completed capture window, copied into
// the next txBuf pair's slot1/slot2.
static uint8_t rx3Snapshot[USART234_SLOT_SIZE];
static uint8_t rx4Snapshot[USART234_SLOT_SIZE];

static volatile uint32_t block_count;
static volatile uint32_t last_u3_bytes;
static volatile uint32_t last_u4_bytes;
static volatile uint32_t u3_err_count;
static volatile uint32_t u4_err_count;
static volatile uint32_t u2_busy_count;

static void Arm(uint8_t idx)
{
    active = idx;
    HAL_SPI_TransmitReceive_DMA(&hspi1, txBuf[idx], rxBuf[idx], USART234_BLOCK_SIZE);
}

static void StartCaptureWindows(void)
{
    HAL_UART_Receive_DMA(&huart3, rx3Window, USART234_SLOT_SIZE);
    HAL_UART_Receive_DMA(&huart4, rx4Window, USART234_SLOT_SIZE);
}

// Stops the DMA capture in progress on `huart`, copies whatever arrived into
// `snapshot` (zero-padded if the window wasn't filled), and reports the byte
// count actually captured.
static uint32_t SnapshotWindow(UART_HandleTypeDef *huart, uint8_t *window, uint8_t *snapshot)
{
    HAL_UART_DMAStop(huart);
    uint32_t remaining = __HAL_DMA_GET_COUNTER(huart->hdmarx);
    uint32_t received = USART234_SLOT_SIZE - remaining;

    memcpy(snapshot, window, received);
    if (received < USART234_SLOT_SIZE)
    {
        memset(snapshot + received, 0, USART234_SLOT_SIZE - received);
    }
    return received;
}

void Usart234DebugInit(void)
{
    memset(txBuf, 0, sizeof(txBuf));
    memset(rxBuf, 0, sizeof(rxBuf));
    memset(rx3Snapshot, 0, sizeof(rx3Snapshot));
    memset(rx4Snapshot, 0, sizeof(rx4Snapshot));

    Arm(0);
    StartCaptureWindows();
}

void Usart234Status(void)
{
    printf("{\"blocks\":\"%lu\",\"u3_bytes\":\"%lu\",\"u4_bytes\":\"%lu\","
           "\"u3_err\":\"%lu\",\"u4_err\":\"%lu\",\"u2_busy\":\"%lu\"}\r\n",
           (unsigned long)block_count, (unsigned long)last_u3_bytes, (unsigned long)last_u4_bytes,
           (unsigned long)u3_err_count, (unsigned long)u4_err_count, (unsigned long)u2_busy_count);
}

// Fires once CS deasserts after exactly USART234_BLOCK_SIZE bytes have been
// clocked in both directions. Re-arm first so the slave is ready before the
// R-Pi asserts CS for the next block, then process the pair that just
// finished: forward its slot0 out USART2, snapshot the USART3/USART4
// capture windows, and build the now-idle pair's next slot1/slot2 from
// those snapshots.
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1)
    {
        return;
    }

    uint8_t finished = active;
    uint8_t next = finished ^ 1;

    Arm(next);

    if (HAL_UART_Transmit_IT(&huart2, &rxBuf[finished][SLOT0_OFFSET], USART234_SLOT_SIZE) != HAL_OK)
    {
        u2_busy_count++;
    }

    last_u3_bytes = SnapshotWindow(&huart3, rx3Window, rx3Snapshot);
    last_u4_bytes = SnapshotWindow(&huart4, rx4Window, rx4Snapshot);
    StartCaptureWindows();

    memset(&txBuf[finished][SLOT0_OFFSET], 0, USART234_SLOT_SIZE);
    memcpy(&txBuf[finished][SLOT1_OFFSET], rx3Snapshot, USART234_SLOT_SIZE);
    memcpy(&txBuf[finished][SLOT2_OFFSET], rx4Snapshot, USART234_SLOT_SIZE);
    memset(&txBuf[finished][SLOT3_OFFSET], 0, USART234_SLOT_SIZE);

    block_count++;
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

// USART3/USART4 framing/overrun errors shouldn't end the current capture
// window early -- just keep listening into the same window buffer.
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        u3_err_count++;
        HAL_UART_Receive_DMA(&huart3, rx3Window, USART234_SLOT_SIZE);
    }
    else if (huart->Instance == USART4)
    {
        u4_err_count++;
        HAL_UART_Receive_DMA(&huart4, rx4Window, USART234_SLOT_SIZE);
    }
}
