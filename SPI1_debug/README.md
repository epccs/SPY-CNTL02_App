# SPI1_debug

USART1 (APP_TX1/APP_RX1, PA9/PA10) connects to a THVD1406 RS485 transceiver on
the HOST485 pair, exactly as in `USART1_streaming` — see that project's
README for the idle-line DMA receive design, the `/address/command
arg1,arg2` parser (`EPCCS_Lib/parse_huart1`), and the `/id?` command. This
project reuses that infrastructure unchanged and adds a command to exercise
**SPI1** (RPSPI0.0/SCLK/MISO/MOSI — PA15/PB3/PB4/PB5), the link between the
Application MCU and an R-Pi Zero 2's SPI0.

## Link design

The App MCU is the SPI1 **slave** (`SPI1.Mode=SPI_MODE_SLAVE`,
`SPI1.VirtualNSS=VM_NSSHARD` in the shared `.ioc`); the R-Pi is the master.
The R-Pi is expected to toggle CS (PA15, hardware NSS) around each 2 KB
block — CS asserts, exactly `SPI1_DEBUG_BLOCK_SIZE` (2048) bytes are clocked
in both directions, CS deasserts. That per-block CS toggle is what makes a
plain `DMA_NORMAL` transfer (already configured for `SPI1_RX`/`SPI1_TX` in
the `.ioc`, DMA1 Channel1/Channel2) the right tool: the DMA's own
transfer-complete interrupt fires right as CS deasserts, with no need to fake
continuous/circular SPI DMA.

Because the slave can't stall the master's clock, the firmware always keeps
one buffer pair "armed" with the DMA and uses the gap between blocks (CS
deasserted) to prepare the other pair — a ping-pong scheme, not a single
continuous buffer:

1. Two 2 KB buffer pairs (`txBuf[0]`/`rxBuf[0]` and `txBuf[1]`/`rxBuf[1]`).
2. At boot, both `txBuf` halves are filled with PRNG data and CRC'd; pair 0
   is armed with `HAL_SPI_TransmitReceive_DMA()`.
3. `HAL_SPI_TxRxCpltCallback()` fires once a block completes. It immediately
   re-arms the *other* pair (minimizing the window before the next CS
   assert), then processes the pair that just finished: CRC the received
   data, record the CRC of the data that was just sent, increment the block
   counter, and refill that pair with fresh random data for its next turn.
4. `HAL_SPI_ErrorCallback()` re-arms the in-flight pair on any SPI/DMA error
   so a single glitch (e.g. a block transferred with the wrong byte count)
   doesn't permanently stall the debug counter.

No hardware RNG exists on the STM32C092, so outgoing data comes from a
software xorshift32 PRNG seeded once at boot from `HAL_GetTick()` — fine for
exercising the link, not cryptographic.

CRC32 is computed by the hardware CRC peripheral (`EPCCS_Lib/spi1_debug.c`
calls `HAL_CRC_Calculate()`), which isn't in the shared `.ioc` — `MX_CRC_Init()`
was hand-added to `main.c` (default polynomial/init value, byte input format).
`hspi1.Init.DataSize` was also hand-changed from CubeMX's `SPI_DATASIZE_4BIT`
default to `SPI_DATASIZE_8BIT` to match the byte-oriented buffers.

## /1/spi? command — EPCCS_Lib/spi1_debug

```
/1/spi?
{"blocks":"<count>","tx_crc":"0x<hex>","rx_crc":"0x<hex>"}
```

`blocks` is the number of completed 2 KB transfers since boot. `tx_crc` and
`rx_crc` are the CRC32 of the data sent and received during that same block —
they describe one block, not a running/cumulative value, so they can be
compared directly against whatever the R-Pi computed for the block it just
sent and received.

This command only reads state set by the SPI1 ISRs — it does not block
waiting for a transfer, so it can be called at any time, including between
blocks.

## R-Pi side

Not yet implemented — tracked as future work. The constraint to keep in mind
when writing it: the gap between CS deassert and the next CS assert needs to
be long enough for `HAL_SPI_TxRxCpltCallback` to run (CRC a 2 KB block via
the hardware CRC peripheral, refill the other 2 KB buffer with PRNG data, and
re-arm DMA) before the next block starts, or that block will glitch.

## Verification

```bash
cd SPI1_debug
make
```

Hardware verification (driving SPI1 from an actual R-Pi and confirming
`/1/spi?`'s block count and CRCs against what the R-Pi computed) is the next
step and is not covered by this build-only check.
