#ifndef EE_H
#define EE_H

// Emulated EEPROM: the last flash page, reserved in STM32C092XX_FLASH.ld
#define EEPROM_SIZE 2048U

// /ee? addr[,UINT8|UINT16|UINT32] -> JSON {"EE[addr]":{"r":value}}
void EEread_cmd(void);

// /ee addr,value[,UINT8|UINT16|UINT32] -> JSON {"EE[addr]":{"w":value,"r":value}}
void EEwrite_cmd(void);

#endif // EE_H
