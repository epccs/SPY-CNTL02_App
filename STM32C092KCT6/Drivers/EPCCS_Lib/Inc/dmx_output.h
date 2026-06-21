#ifndef DMX_OUTPUT_H
#define DMX_OUTPUT_H

// Block size matches the R-Pi's CS-bounded SPI1 transaction: one CS
// assert/deassert clocks exactly this many bytes in both directions before
// the link goes idle again. Slots 0/1/2 (512 bytes each) become DMX1/DMX2/
// DMX3 channel data (USART2/USART3/USART4); slot3 is unused.
#define DMX_BLOCK_SIZE 2048
#define DMX_UNIVERSE_SIZE 512

// Seeds the active frame from the saved EEPROM page (the power-up default),
// arms the first SPI1 slave DMA transfer, and starts the DMX refresh clock.
// Call once at boot, after MX_SPI1_Init(), MX_USART2/3/4_UART_Init() and
// MX_TIM16_Init().
void DmxOutputInit(void);

// Call from the main loop whenever no command is being processed. Fires a
// new DMX512 frame on DMX1/2/3 at roughly 40 Hz from whatever is currently
// the active frame (live SPI data, or the last /save'd default).
void DmxRepeatCheck(void);

// /1/save -> JSON {"save":"ok"} (or {"err":"..."} on a flash failure).
// Persists the currently active 2 KB frame to the emulated EEPROM page as
// the new power-up default.
void DmxSave(void);

#endif // DMX_OUTPUT_H
