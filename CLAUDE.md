# SPY-CNTL02_App — Claude Code Context

## Project overview

Firmware for the STM32C092KCT6 application microcontroller on the SPY-CNTL02 hardware. Each subfolder is a standalone project with its own `Makefile` and `Core/` tree; all projects share the HAL/CMSIS drivers in `STM32C092KCT6/`.

## Repository layout

```
SPY-CNTL02_App/
├── STM32C092KCT6/                  # Shared drivers (HAL/CMSIS), linker script, startup file
│   ├── Drivers/
│   │   ├── CMSIS/
│   │   ├── EPCCS_Lib/              # Shared application libraries
│   │   │   ├── Inc/parse_huart1.h  # RS485 CLI parser header
│   │   │   ├── Src/parse_huart1.c  # RS485 CLI parser — printf via huart1
│   │   │   ├── Inc/id.h, Src/id.c              # /id? command
│   │   │   ├── Inc/i2c1_cmd.h, Src/i2c1_cmd.c  # /iscan?, /iaddr, /ibuff, /iwrite, /iread? (I2C1 master)
│   │   │   ├── Inc/i2c1_monitor.h, Src/i2c1_monitor.c  # /imon? (I2C1 slave-listen monitor)
│   │   │   ├── Inc/spi1_debug.h, Src/spi1_debug.c  # /spi? (SPI1 slave block-test vs R-Pi SPI0)
│   │   │   └── Inc/usart234_debug.h, Src/usart234_debug.c  # /usart234? (SPI1-driven USART2/3/4 test bridge)
│   │   └── STM32C0xx_HAL_Driver/
│   ├── STM32C092XX_FLASH.ld
│   ├── STM32C092KCT6_App.ioc
│   └── startup_stm32c092xx.s
├── empty/                  # Minimal template — copy this to start a new project
│   ├── Core/Src/main.c
│   ├── Core/Inc/main.h
│   └── Makefile
├── USART1_streaming/       # USART1 HOST485 command interface with idle-line DMA
│   ├── Core/Src/main.c
│   ├── Core/Inc/main.h
│   └── Makefile
├── I2C1_debug/             # I2C1 (SDA1/SCL1) master/slave-monitor debug commands over USART1
│   ├── Core/Src/main.c
│   ├── Core/Inc/main.h
│   └── Makefile
├── SPI1_debug/             # SPI1 (RPSPI0.0/SCLK/MISO/MOSI) slave block-test vs R-Pi SPI0, over USART1
│   ├── Core/Src/main.c
│   ├── Core/Inc/main.h
│   └── Makefile
└── USART234_debug/         # SPI1-driven test bridge to USART2/3/4 (test header loop), over USART1
    ├── Core/Src/main.c
    ├── Core/Inc/main.h
    └── Makefile
```

## Building

```bash
cd <project-folder>   # e.g., empty or USART1_streaming
make
```

Output is in the project's `build/` directory.

## Toolchain (Ubuntu 24.04)

```bash
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi gdb-multiarch make
```

The STM32CubeMX generated HAL library is cloned into `STM32C092KCT6/`.

## Adding a new project

Copy `empty/` to a new folder and update the `Makefile` paths if needed. The `Makefile` references `../STM32C092KCT6/` for drivers and the linker script.

## Application MCU pinout summary (STM32C092KCT6 LQFP-32)

| Pin | Label | Function |
| ----- | -------- | ---------- |
| 1 | CS1 | PB9 push-pull GPIO |
| 2 | CS2 | PC14 push-pull GPIO |
| 3 | CS3 | PC15 push-pull GPIO |
| 6 | APP~{RST} | PF2 NRST |
| 7 | APP_TX4 | PA0 USART4_TX |
| 8 | APP_RX4 | PA1 USART4_RX |
| 9 | ADC1 | PA2 analog in ADC1_IN2 |
| 10 | ADC2 | PA3 analog in ADC1_IN3 |
| 11 | ADC3 | PA4 analog in ADC1_IN4 |
| 12 | ADC4 | PA5 analog in ADC1_IN5 |
| 13 | ADC5 | PA6 analog in ADC1_IN6 |
| 14 | ADC6 | PA7 analog in ADC1_IN7 |
| 15 | APP_RX3 | PB0 USART3_RX |
| 16 | CS4 | PB1 push-pull GPIO |
| 17 | APP_TX3 | PB2 USART3_TX |
| 18 | APP_TX2 | PA8 USART2_TX |
| 19 | APP_TX1| PA9 USART1_TX |
| 20 | MGR~{RST} | PC6 pull GPIO |
| 21 | APP_RX1 | PA10 USART1_RX|
| 22 | CS5-6 | PA11 push-pull GPIO |
| 23 | MGR_BOOT0 | PA12 push-pull GPIO |
| 24 | APP_RX2 | PA13 USART2_RX |
| 25 | APP_BOOT0 | PA14 BOOT0 |
| 26 | RPSPI0.0 | PA15  SPI1_NSS |
| 27 | RPSPI0_SCLK | PB3 SPI1_SCK |
| 28 | RPSPI0_MISO | PB4 SPI1_MISO |
| 29 | RPSPI0_MOSI | PB5 SPI1_MOSI |
| 30 | MGR2HOST485 | PB6 pull GPIO |
| 31 | SDA1 | PB7 I2C1_SDA |
| 32 | SCL1 | PB8 I2C1_SCL |

- CS1 .. CS5-6 pull down to enable a 22 mA current source (e.g., can be used to power loop sensors, but does not take damage if wiring is shorted.)
- APP_~{RST} may be pulled down by the Manager MCU and has a 10k Ohm pull up. Set in "Reset_State" during CubeMX generator step.
- ADC1 .. ADC6 analog inputs.
- MGR~{RST} allows the application MCU to reset the manager MCU.
- MGR_BOOT0 allows the application MCU to set manager MCU bootloader mode.
- APP_BOOT0 allows the manager MCU to set the application MCU bootloader mode, also has 10k Ohm pull down. This also goes to the R-Pi through a 1k Ohm resistor.
- MGR_TX1 and MGR_RX1 go to a THVD1406 connected to the HOST485 pair.
- MGR_TX2 and MGR_RX2 go to a THVD1406 connected to the Lighting header DMX1.
- MGR_TX3 and MGR_RX3 go to a THVD1406 connected to the Lighting header DMX2.
- MGR_TX4 and MGR_RX4 go to a THVD1406 connected to the Lighting header DMX3.
- SPI0.0 SPI0_SCLK SPI0_MOSI SPI0_MISO goes to R-Pi SPI0 pins.
- MGR2HOST485 connects to the THVD1406 on the HOST485 pair and can disconnect the manager from the host RS485 pair. This is used when multiple SPY-CNTL02 boards are on the HOST485 pair and the non-bootloaded units need to be blocked.
- SDA1 and SCL1 is an I2C bus between the Manager MCU and Application MCU.

## USART1_streaming — HOST485 command interface

USART1 (PA9/PA10) connects via a THVD1406 to the HOST485 RS485 pair. The project uses idle-line DMA for receive and retargeted `printf` for transmit.

**Receive flow:** `HAL_UARTEx_ReceiveToIdle_DMA` listens on DMA1 Channel3 (`DMA_REQUEST_USART1_RX`). When the bus goes idle after a transmission the HAL fires `HAL_UARTEx_RxEventCallback`, which calls `LoadCommandFromDMA` and immediately re-arms the DMA.

**Transmit:** `__io_putchar` in `main.c` overrides the weak stub in `syscalls.c`, routing `printf`/`putchar` to `HAL_UART_Transmit(&huart1, ...)`.

**Parser:** `EPCCS_Lib/parse_huart1` parses addressed commands of the form `/address/command arg1,arg2`. `CheckAddress(MY_ADDRESS)` sets `echo_on`; all `printf` calls in the parser are gated on `echo_on` so only the addressed device responds on the shared bus. `MY_ADDRESS` is `'1'` for the App MCU (Manager MCU uses `'0'`). `MY_NAME` is `"App"`.

**Main loop pattern** (with a repeating command active):

```C
if (command_done)
{
    RepeatCancel();           // cancel any active repeating command
    CheckAddress(MY_ADDRESS);
    if (echo_on && findCommand())
    {
        // dispatch on command string, e.g. strcmp(command, "/id?") == 0
    }
    initCommandBuffer();
}
else
{
    RepeatCheck();            // fire the next repeat if the interval has elapsed
}
```

**DMA interrupt:** DMA1 Channel3 shares `DMA1_Channel2_3_IRQHandler` with SPI1_TX (Channel2). Both `HAL_DMA_IRQHandler` calls are in the handler.

## EPCCS_Lib — shared command library

All reusable command implementations live in `STM32C092KCT6/Drivers/EPCCS_Lib/`. Each module is a `.c`/`.h` pair; add it to the project's `Makefile` `C_SOURCES` list to use it.

| Module | Commands | Notes |
| ------ | -------- | ----- |
| `parse_huart1` | *(parser, not a command)* | always linked |
| `id` | `/id?` | board name/desc/gcc version |
| `ee` | `/ee?`, `/ee` | emulated EEPROM in last 2 KB flash page; requires the `EEPROM` region in the linker script (already in `STM32C092XX_FLASH.ld`) |
| `analog` | `/analog?`, `/adc?` | ADC1_IN2..IN7 (PA2..PA7); free-running, triggered every 1 ms by TIM3's TRGO, with circular DMA (DMA1 Channel4) into a background buffer, so reports never block on a conversion; optional args select 1..6 channels (default all 6); reports mV or raw counts; repeats every 2 s |
| `i2c1_cmd` | `/iscan?`, `/iaddr`, `/ibuff`/`/ibuff?`, `/iwrite`, `/iread?` | I2C1 master; blocking HAL API |
| `i2c1_monitor` | `/imon?` | I2C1 slave-listen; requires `I2C1_IRQn` enabled and `I2C1_IRQHandler` in `stm32c0xx_it.c` |
| `cs_io` | `/iowrt`, `/iotog` | CS1..CS5 current source enable outputs (PB9, PC14, PC15, PB1, PA11); output-only (no `/iodir`/`/iord?`); index 1..5 matches the CSn silkscreen labels, index 5 (`CS5_6`) drives the shared CS5/CS6 pin; CubeMX resets all five LOW (off) at boot |
| `spi1_debug` | `/spi?` | SPI1 slave (R-Pi SPI0 is master, hardware NSS, CS toggles per 2 KB block); ping-pong `DMA_NORMAL` transfers on `SPI1_RX`/`SPI1_TX` re-armed in `HAL_SPI_TxRxCpltCallback`; reports completed-block count and the CRC32 (hardware CRC peripheral) of the data sent/received that block; outgoing data is software-PRNG (xorshift32), since this part has no hardware RNG |
| `usart234_debug` | `/usart234?` | Reuses the SPI1 slave ping-pong from `spi1_debug` as the test heartbeat, but splits each 2 KB block into four 512 B slots: slot0 (received from R-Pi) is forwarded out USART2 TX (`HAL_UART_Transmit_IT`, no DMA channel left for it); slot1/slot2 (returned to R-Pi) are filled from fixed 512 B DMA capture windows on USART3/USART4 (DMA1 Channel4/Channel5, snapshotted and restarted every block via `HAL_UART_DMAStop`/`__HAL_DMA_GET_COUNTER`); slot3 is always zero. One block of pipeline latency between a USART3/4 capture and its return to the R-Pi, same ping-pong constraint as `spi1_debug` |

**float printf:** the Makefiles use `nano.specs`, which does not support `%f`/`%g` by default. Use integer arithmetic instead (e.g. millivolts) or add `-u _printf_float` to `LDFLAGS`.

**Repeating commands** (`analog`, and any future module that streams): the module exposes `XxxRepeatCheck()` (call from the `else` branch of the main loop) and `XxxRepeatCancel()` (call at the top of `if (command_done)`, before `CheckAddress`, so any incoming line — even to a different address — cancels the repeat).

## Adding a new command

1. Add `EPCCS_Lib/Inc/mycmd.h` and `EPCCS_Lib/Src/mycmd.c` (or reuse an existing module).
2. Add `$(SHARED_DIR)/Drivers/EPCCS_Lib/Src/mycmd.c \` to `C_SOURCES` in the project's `Makefile`.
3. In `main.c`:
   - `#include "mycmd.h"` in the `USER CODE BEGIN Includes` block.
   - Add an `else if (strcmp(command, "/mycmd") == 0)` branch in the dispatch chain.
   - If the command repeats, add `MycmdRepeatCancel()` / `MycmdRepeatCheck()` calls.
4. `make` to verify.

## I2C1_debug — I2C1 master/slave-monitor commands over HOST485

Reuses USART1_streaming's idle-line-DMA receive, `parse_huart1` command parser and `__io_putchar` transmit unchanged, and adds commands that exercise I2C1 (SDA1/SCL1, PB7/PB8 — the App↔Mgr I2C bus): `/id?`, `/iscan?`, `/iaddr`, `/ibuff`/`/ibuff?`, `/iwrite`, `/iread?`, `/imon?`. See `I2C1_debug/README.md` for the full command reference and JSON response shapes.

**Master commands** (`EPCCS_Lib/i2c1_cmd`): blocking HAL master API (`HAL_I2C_IsDeviceReady`, `HAL_I2C_Master_Transmit`, `HAL_I2C_Master_Receive`) against a `master_address` set by `/iaddr`, using a shared `txBuffer[32]` filled by `/ibuff`.

**Slave monitor** (`EPCCS_Lib/i2c1_monitor`): `/imon?` puts I2C1 into slave-listen mode (`HAL_I2C_EnableListen_IT`) at a given address so writes from another master (e.g. the Manager MCU) can be observed and printed between commands. `I2c1MonitorCheck()` runs from the main loop whenever `command_done == 0`; `I2c1MonitorCancel()` runs before every command dispatch and on receiving any other command.

**I2C1 IRQ:** I2C1 has a single combined event+error interrupt. `MX_I2C1_Init()` enables `I2C1_IRQn`, and `I2C1_IRQHandler` in `stm32c0xx_it.c` calls both `HAL_I2C_EV_IRQHandler(&hi2c1)` and `HAL_I2C_ER_IRQHandler(&hi2c1)`.

## PB9 was labeled CS1

Before CubeMX generated code, labeled pins were set in the UI.

```C
#define CS1_Pin GPIO_PIN_9
#define CS1_GPIO_Port GPIOB
```

And may be used as follows.

```C
// set pin HIGH
HAL_GPIO_WritePin(CS1_GPIO_Port, CS1_Pin, GPIO_PIN_SET);

// set pin LOW
HAL_GPIO_WritePin(CS1_GPIO_Port, CS1_Pin, GPIO_PIN_RESET);

// toggle pin
HAL_GPIO_TogglePin(CS1_GPIO_Port, CS1_Pin);
```

## Help with Grammar and Spelling

