# Application MCU (STM32C092KCT6) Firmware on SPY-CNTL02

This repository has some folders with makefiles that compile firmware that can be uploaded

```
# ways to clone this repo
gh repo clone epccs/SPY-CNTL02_App
git clone https://github.com/epccs/SPY-CNTL02_App
```

## toolchain setup

```
git clone --recursive https://github.com/STMicroelectronics/STM32CubeC0.git
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi gdb-multiarch make

# STM32CubeMX is essentially a visual code generator that creates the "glue code" (HAL/LL drivers) 
# and the correct Linker Script https://www.st.com/en/development-tools/stm32cubemx.html
# once you have the files unzip and run:
unzip stm32cubemx-lin-v6-17-0.zip
chmod +x SetupSTM32CubeMX-6.17.0
./SetupSTM32CubeMX-6.17.0
# it should show up in the applications menu
# Click "Access to MCU Selector" and type the MCU e.g., STM32C092FCP6
# Select it and click Start Project
# Configure Hardware: (Optional) Enable your High-Speed External (HSE) clock or GPIOs if you know what you need
# Project Manager Tab (Crucial Step): Project Name: e.g., C0_Project. Project Location: -. Toolchain / IDE: Select Makefile (For a pure Linux workflow).
# Code Generator Tab: Check "Copy only the necessary library files" to keep the folder small.
# Generate Code: Click the blue button in the top right.
```

CubeMX will create a folder structure like this:

```
C0_Project/
├── Core/
│   ├── Src/ (main.c lives here)
│   └── Inc/ (header files, main.h lives here)
├── Drivers/ (STM32 HAL/CMSIS headers)
├── Makefile (The build script)
├── STM32C092FCP6_FLASH.ld
├── STM32C092FCP6.ioc
└── startup_stm32c092xx.s
```

With the correct linker script (.ld) and a Makefile, the rest is normal terminal and editor activities.

This repo will have multiple folders with makefiles that can use those Drivers, linker script, and assembly file to build various firmware for testing, learning, and SPY-CNTL02 hardware-specific applications. To accomplish this, the content of C0_Project is copied into a folder named STM32C092KCT6, and then the Makefile and Core are copied into the empty folder like this:

```
empty/
├── Core/
│   ├── Src/ (main.c lives here)
│   └── Inc/ (header files, main.h lives here)
├── Makefile (The build script)
STM32C092KCT6/
├── Drivers/ (STM32 HAL/CMSIS headers)
├── STM32C092KCT6_FLASH.ld
├── STM32C092KCT6.ioc
└── startup_stm32c092xx.s
```

## How to init a repo on Linux

```bash
cd ~/git/SPY-CNTL02_App
# check that .git is nuked
git init
#git branch -M main
git add .
git commit -m "commit initial staged files"
# GitHub CLI will ask questions (e.g., description, visibility...)
gh repo create
```

