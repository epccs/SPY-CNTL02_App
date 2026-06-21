# USART234_debug

USART1 (APP_TX1/APP_RX1, PA9/PA10) connects to a THVD1406 RS485 transceiver on
the HOST485 pair, exactly as in `USART1_streaming` — see that project's
README for the idle-line DMA receive design, the `/address/command
arg1,arg2` parser (`EPCCS_Lib/parse_huart1`), and the `/id?` command. This
project reuses that infrastructure unchanged and adds a command to exercise
**USART2, USART3, and USART4** (PA8/PA13, PB2/PB0, PA0/PA1) via the same SPI1
slave link from `SPI1_debug`, which here is repurposed as the test
heartbeat/transport rather than a link-integrity test by itself.

## Test header wiring

This project assumes an external test header jumpers APP_TX2 to both
APP_RX3 and APP_RX4: USART2 only transmits, USART3 and USART4 only receive,
both observing the same byte stream from USART2.

## Link design

The App MCU is the SPI1 **slave**, same as `SPI1_debug`: the R-Pi (SPI0
master) toggles CS (PA15, hardware NSS) around each 2 KB block — CS asserts,
exactly `USART234_BLOCK_SIZE` (2048) bytes are clocked in both directions, CS
deasserts. Each 2 KB block is split into four 512-byte slots:

| Slot | Bytes | Direction (R-Pi's view) | Content |
| ---- | ----- | ------------------------ | ------- |
| 0 | 0-511 | R-Pi → App | forwarded out USART2 TX as soon as the block completes |
| 1 | 512-1023 | App → R-Pi | bytes captured on USART3 during the *previous* block's window |
| 2 | 1024-1535 | App → R-Pi | bytes captured on USART4 during the *previous* block's window |
| 3 | 1536-2047 | App → R-Pi | unused, always zero |

**Capture window semantics:** USART3/USART4 each run a fixed 512-byte DMA
capture window per block. At each block boundary the firmware stops that
window's DMA, records however many bytes actually arrived (zero-padding the
rest), and immediately starts a fresh window for the next block — bytes
beyond the first 512 in a window, or arriving after the window's DMA has
already filled, are not captured.

**Pipeline latency:** because the SPI1 ping-pong buffer for the *next* block
must already be armed before the *current* one finishes, USART3/4 bytes
captured during the window ending at boundary N are returned to the R-Pi in
slots 1/2 of the block transmitted starting at boundary N+1 — one block of
latency, the same ping-pong constraint `SPI1_debug` has for its own buffers.

`EPCCS_Lib/usart234_debug.c` implements this:

1. Two 2 KB `txBuf`/`rxBuf` ping-pong pairs for SPI1 (same mechanism as
   `SPI1_debug`, but slot contents are real data, not PRNG — no CRC needed).
2. `HAL_SPI_TxRxCpltCallback()` fires once a block completes. It re-arms the
   *other* pair immediately, forwards the finished pair's slot0 out USART2
   (`HAL_UART_Transmit_IT`), snapshots+restarts the USART3/USART4 capture
   windows, and fills the finished pair's slot1/slot2 from those snapshots
   for its next turn.
3. `HAL_SPI_ErrorCallback()` re-arms the in-flight pair on any SPI/DMA error.
4. `HAL_UART_ErrorCallback()` for USART3/USART4 clears the error and keeps
   listening into the same window buffer rather than ending the window early.

USART3_RX uses DMA1 Channel4, USART4_RX uses DMA1 Channel5 (hand-added in
`stm32c0xx_hal_msp.c`'s `HAL_UART_MspInit`, `DMA_REQUEST_USART3_RX`/
`DMA_REQUEST_USART4_RX`) — these are the two DMA1 channels left free after
SPI1_RX/TX (Channel1/2) and USART1_RX (Channel3). USART2_TX uses interrupt
mode instead of DMA, since no DMA channel was left for it; `USART2_IRQn` is
hand-enabled in `MX_USART2_UART_Init()` and `USART2_IRQHandler` (hand-added
in `stm32c0xx_it.c`) calls `HAL_UART_IRQHandler(&huart2)`.

## /1/usart234? command — EPCCS_Lib/usart234_debug

```
/1/usart234?
{"blocks":"<n>","u3_bytes":"<n>","u4_bytes":"<n>","u3_err":"<n>","u4_err":"<n>","u2_busy":"<n>"}
```

- `blocks`: completed 2 KB SPI transfers since boot.
- `u3_bytes`/`u4_bytes`: bytes captured during the most recently completed
  USART3/USART4 window (0-512; less than 512 means the source didn't keep up
  with the window).
- `u3_err`/`u4_err`: USART3/USART4 error-callback count (framing/overrun).
- `u2_busy`: count of blocks where USART2 was still transmitting the
  previous slot0 when the next one was ready, so it was dropped.

This command only reads state set by the SPI1/USART ISRs — it does not block
waiting for a transfer, so it can be called at any time, including between
blocks.

## R-Pi side

Not yet implemented — tracked as future work, same constraint as
`SPI1_debug`: the gap between CS deassert and the next CS assert needs to be
long enough for `HAL_SPI_TxRxCpltCallback` to run before the next block
starts, or that block will glitch.

## Verification

```bash
cd USART234_debug
make
```

Hardware verification (driving SPI1 from an actual R-Pi, feeding known bytes
into the USART2→USART3/USART4 test header loop, and confirming
`/1/usart234?`'s byte counts) is the next step and is not covered by this
build-only check.
