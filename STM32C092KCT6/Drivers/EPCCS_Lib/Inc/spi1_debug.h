#ifndef SPI1_DEBUG_H
#define SPI1_DEBUG_H

// Block size matches the R-Pi's CS-bounded SPI transaction: one CS assert/deassert
// clocks exactly this many bytes in both directions before the link goes idle again.
#define SPI1_DEBUG_BLOCK_SIZE 2048

// Seeds the PRNG, fills both TX buffer halves, and arms the first SPI1 slave
// DMA transfer. Call once at boot, after MX_SPI1_Init() and MX_CRC_Init().
void Spi1DebugInit(void);

// /1/spi? -> JSON {"blocks":"<count>","tx_crc":"0x<hex>","rx_crc":"0x<hex>"}
void Spi1Status(void);

#endif // SPI1_DEBUG_H
