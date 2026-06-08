# SPY-DRVR02 — Claude Code Context

## Project overview

Firmware for the STM32C092KCT6 application microcontroller on the SPY-CNTL02 hardware. Each subfolder is a standalone project with its own `Makefile` and `Core/` tree; all projects share the HAL/CMSIS drivers in `STM32C092KCT6/`.

## Repository layout

```
SPY-DRVR02/
├── STM32C092KCT6/          # Shared drivers (HAL/CMSIS), linker script, startup file
│   ├── Drivers/
│   │   ├── CMSIS/
│   │   ├── EPCCS_Lib/      # Shared libs like CLI parser
│   │   └── STM32C0xx_HAL_Driver/
│   ├── STM32C092FCP6_FLASH.ld
│   ├── STM32C092FCP6_App.ioc
│   └── startup_stm32c092xx.s
├── empty/                  # Minimal template — copy this to start a new project
│   ├── Core/Src/main.c
│   ├── Core/Inc/main.h
│   └── Makefile
└── CS1_push-pull/          # GPIO push-pull test for pin 1 (PB9)
    ├── Core/Src/main.c
    ├── Core/Inc/main.h
    └── Makefile
```

## Building

```bash
cd <project-folder>   # e.g., empty or PA5_push-pull
make
```

Output is in the project's `build/` directory.

## Toolchain (Ubuntu 24.04)

```bash
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi gdb-multiarch make
```

STM32CubeMX generated HAL library (cloned into `STM32C092KCT6/`):

## Adding a new project

Copy `empty/` to a new folder and update the `Makefile` paths if needed. The `Makefile` references `../STM32C092FCP6/` for drivers and the linker script.

## Pinout summary (STM32C092KCT6 LQFP-32)

## Applicaiton MCU pinout summary (STM32C092KCT6 LQFP-32)

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

- CS1 .. CS5-6 pull down to enable a 22 mA current source (e.g., can be use to power loop sensors, but does not take damage if wiring is shorted.)
- APP_~{RST} may be pulled down by the Manager MCU and has a 10k Ohm pull up. Set in "Reset_State" durring CubeMX generator step.
- ADC1 .. ADC6 analog inputs 
- MGR~{RST} allows the applicaion MCU to reset the manager MCU.
- MGR_BOOT0 allows the application MCU to set manager MCU bootloader mode. 
- APP_BOOT0 allows the manager MCU to set the aplication MCU bootloader mode, also has 10k Ohm pull down. This also goes to the R-Pi through a 1k Ohm resistor.
- MGR_TX1 and MGR_RX1 go to a THVD1406 connected to the HOST485 pair.
- MGR_TX2 and MGR_RX2 go to a THVD1406 connected to the Lighting hader DMX1.
- MGR_TX3 and MGR_RX3 go to a THVD1406 connected to the Lighting hader DMX2.
- MGR_TX4 and MGR_RX4 go to a THVD1406 connected to the Lighting hader DMX3.
- SPI0.0 SPI0_SCLK SPI0_MOSI SPI0_MISO goes to R-Pi SPI0 pins.
- MGR2HOST485 connects to the THVD1406 on the HOST485 pair and can disconnect the manager from the host RS485 pair. This is used when multiple SPSY-CNTL02 boards are on the HOST485 pair and the none bootloaded unitis need to be blocked.
- SDA1 and SCL1 is an I2C bus between the Manager MCU and Applicaion MCU.

## PB9 was labled CS1 

Befor CubeMX generated code labled pins were set in the UI.

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

## Help with Grammer and Spelling

