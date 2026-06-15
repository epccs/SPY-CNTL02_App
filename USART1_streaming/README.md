# USART1_streaming

USART1 (APP_TX1/APP_RX1, PA9/PA10) connects to a THVD1406 RS485 transceiver on the HOST485 pair. This project implements a addressed command-line interface over that half-duplex RS485 bus.

## printf retargeting

`syscalls.c` contains a weak `_write` stub that calls `__io_putchar`. A strong override in `main.c` routes every byte to USART1:

```C
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
```

This makes `printf` the output path for all command responses. `_read`/`scanf` are not used — receive is handled by DMA.

## Idle-line DMA receive

`HAL_UARTEx_ReceiveToIdle_DMA` listens into a `rx_buf[COMMAND_BUFFER_SIZE]` buffer (32 bytes) using DMA1 Channel3. It enables three interrupt sources, all of which call `HAL_UARTEx_RxEventCallback`:

| Event | When | `Size` passed in |
| ----- | ---- | ---------------- |
| **IDLE** | Bus quiet for one frame after the last byte | bytes actually received |
| **HT** (half-transfer) | DMA has received exactly 16 bytes with no idle yet | 16 |
| **TC** (transfer-complete) | DMA has received all 32 bytes | 32 |

**Normal operation** — a short command like `/0/pwm 127\r\n` ends with the sender going quiet. IDLE fires, `LoadCommandFromDMA` stops at `\r`, and the command is ready to dispatch.

**Buffer full (TC)** — if 32 bytes arrive with no idle, the DMA stops (it is in `DMA_NORMAL` mode). The callback fires with `Size=32`. `LoadCommandFromDMA` scans for `\r`/`\n`; if none is found the resulting command will fail `findCommand` validation. The re-arm at the bottom of the callback puts the DMA back into service immediately.

**Half-transfer (HT)** — if 16 bytes arrive mid-command with no idle yet, the callback would fire on partial data. The guard on `RxEventType` below skips `LoadCommandFromDMA` in that case; the DMA continues filling from where it left off and IDLE or TC delivers the complete line.

```C
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        if (huart->RxEventType != HAL_UART_RXEVENT_HT)
        {
            LoadCommandFromDMA(rx_buf, Size);
        }
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf, COMMAND_BUFFER_SIZE);
    }
}
```

DMA1 Channel3 is configured for USART1_RX in `HAL_UART_MspInit` inside `stm32c0xx_hal_msp.c`. Channel3 shares the `DMA1_Channel2_3_IRQHandler` with SPI1_TX (Channel2); both are serviced in the handler.

## Normal vs circular DMA mode

`DMA_NORMAL` means the DMA counts down from 32 to 0 and stops. The callback is the natural re-arm point, and there is no ambiguity about which part of the buffer contains fresh data.

`DMA_CIRCULAR` wraps back to byte 0 and keeps going, firing HT and TC repeatedly. That is useful for audio-style continuous streaming where you process one half while the other fills. For line-oriented command parsing it adds complexity with no benefit.

## Command format

Commands follow an addressed path format inspired by MQTT topics:

```
/address/command arg1,arg2,...
```

- `address` is a single ASCII character (`'1'` for the App MCU, set by `MY_ADDRESS` in `main.c`)
- Only the device whose address matches responds — all others ignore the line
- Arguments are comma-delimited; no spaces within arguments
- Example: `/1/id?` or `/1/adc?`

## Parser library — EPCCS_Lib/parse_huart1

`STM32C092KCT6/Drivers/EPCCS_Lib/Src/parse_huart1.c` provides:

| Function | Description |
| -------- | ----------- |
| `initCommandBuffer()` | Reset all parser state; call after dispatching or on error |
| `LoadCommandFromDMA(buf, size)` | Copy DMA buffer into `command_buf`, set `command_done` |
| `CheckAddress(address)` | Set `echo_on` if address field matches; gates all response output |
| `findCommand()` | Validate and null-terminate `command`; parse arguments if present |
| `findArgument(offset)` | Internal — called by `findCommand` |
| `is_arg_in_ul_range(n, min, max)` | Validate and return `arg[n]` as `unsigned long` |
| `is_arg_in_uint8_range(n, min, max)` | Validate and return `arg[n]` as `uint8_t` |

Key globals exposed by the library:

```C
extern uint8_t  command_done;   // set by LoadCommandFromDMA, cleared by initCommandBuffer
extern uint8_t  echo_on;        // set by CheckAddress; gates printf output
extern char    *command;        // points into command_buf at the /command part
extern char    *arg[];          // null-terminated argument strings
extern uint8_t  arg_count;      // number of parsed arguments
```

## Main loop dispatch pattern

```C
#define MY_ADDRESS '1'
#define MY_NAME    "App"

while (1)
{
    if (command_done)
    {
        AnalogRepeatCancel();
        CheckAddress(MY_ADDRESS);
        if (echo_on)
        {
            if (findCommand())
            {
                if      (strcmp(command, "/id?")    == 0) Id(MY_NAME);
                else if (strcmp(command, "/ee?")    == 0) EEread_cmd();
                else if (strcmp(command, "/ee")     == 0) EEwrite_cmd();
                else if (strcmp(command, "/analog?")== 0) Analogf();
                else if (strcmp(command, "/adc?")   == 0) Analogd();
                else printf("{\"err\":\"UnknownCmd\"}\r\n");
            }
        }
        initCommandBuffer();
    }
    else
    {
        AnalogRepeatCheck();
    }
}
```

Error responses from `findCommand` and the range-check helpers are JSON strings, e.g. `{"err": "BadCharInCmd 'x'"}`.

## /id? command — EPCCS_Lib/id

`EPCCS_Lib/Src/id.c` provides `Id(const char name[])`:

| Command | Response |
| ------- | -------- |
| `/1/id?` | `{"id":{"name":"App","desc":"SPY-CNTL02 App (STM32C092KCT6)","gcc":"<__VERSION__>"}}` |
| `/1/id? name` | `{"id":{"name":"App"}}` |
| `/1/id? desc` | `{"id":{"desc":"SPY-CNTL02 App (STM32C092KCT6)"}}` |
| `/1/id? gcc` | `{"id":{"gcc":"<__VERSION__>"}}` |

## /ee and /ee? commands — EPCCS_Lib/ee (emulated EEPROM)

The STM32C092 has no EEPROM, so `EPCCS_Lib/Src/ee.c` emulates 2 KB using the **last 2 KB flash page**. `STM32C092XX_FLASH.ld` shrinks `FLASH` to 254 K and adds an `EEPROM` region at `0x803F800`, anchored by linker symbol `_eeprom_start`.

| Command | Response |
| ------- | -------- |
| `/1/ee? 5` | `{"EE[5]":{"r":"<byte>"}}` (UINT8 default) |
| `/1/ee? 5,UINT16` | `{"EE[5]":{"r":"<word>"}}` |
| `/1/ee 5,42` | `{"EE[5]":{"w":"42","r":"42"}}` |
| `/1/ee 5,1000,UINT16` | `{"EE[5]":{"w":"1000","r":"1000"}}` |

Address must satisfy `addr + sizeof(type) <= 2048`. Each write erases and reprograms the full 2 KB page (~tens of ms, blocking); no wear-leveling (~10 k erase cycles).

## /analog? and /adc? commands — EPCCS_Lib/analog

`EPCCS_Lib/Src/analog.c` reads all six ADC channels (ADC1_IN2..ADC1_IN7, PA2..PA7) in one `HAL_ADC_Start` sequence. `HAL_ADCEx_Calibration_Start` runs once at startup.

| Command | Response |
| ------- | -------- |
| `/1/analog?` | `{"ADC1":"<mV>","ADC2":"<mV>","ADC3":"<mV>","ADC4":"<mV>","ADC5":"<mV>","ADC6":"<mV>"}` |
| `/1/adc?` | `{"ADC1":"<raw>","ADC2":"<raw>","ADC3":"<raw>","ADC4":"<raw>","ADC5":"<raw>","ADC6":"<raw>"}` |

`/analog?` reports millivolts (`mV = raw × 3300 / 4096`); `/adc?` reports raw 12-bit counts. Both commands repeat every 2 s until any new command line arrives on the bus (`AnalogRepeatCancel` is called at the top of every dispatch pass). Output is emitted one channel per `printf` call to keep individual call length short.
