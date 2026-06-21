#ifndef USART234_DEBUG_H
#define USART234_DEBUG_H

// Block size matches the R-Pi's CS-bounded SPI1 transaction: one CS
// assert/deassert clocks exactly this many bytes in both directions before
// the link goes idle again. Split into four 512-byte slots; see
// usart234_debug.c for what each slot carries.
#define USART234_BLOCK_SIZE 2048
#define USART234_SLOT_SIZE 512

// Seeds the ping-pong SPI1 buffers, arms the first SPI1 slave DMA transfer,
// and starts the first USART3/USART4 capture windows. Call once at boot,
// after MX_SPI1_Init(), MX_USART2_UART_Init(), MX_USART3_UART_Init() and
// MX_USART4_UART_Init().
void Usart234DebugInit(void);

// /1/usart234? -> JSON {"blocks":"<n>","u3_bytes":"<n>","u4_bytes":"<n>",
//                       "u3_err":"<n>","u4_err":"<n>","u2_busy":"<n>"}
void Usart234Status(void);

#endif // USART234_DEBUG_H
