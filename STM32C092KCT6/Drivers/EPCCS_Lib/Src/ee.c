#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm32c0xx_hal.h"
#include "parse_huart1.h"
#include "ee.h"

// Linker symbol marking the start of the reserved EEPROM page, see STM32C092XX_FLASH.ld
extern uint8_t _eeprom_start[];
#define EEPROM_BASE ((uint32_t)_eeprom_start)

// width in bytes for a type name, or 0 if not recognized
static uint8_t ee_type_width(const char *type)
{
    if (strcmp(type, "UINT8") == 0) return 1;
    if (strcmp(type, "UINT16") == 0) return 2;
    if (strcmp(type, "UINT32") == 0) return 4;
    return 0;
}

// /ee? addr[,UINT8|UINT16|UINT32]  (UINT8 if type is omitted)
void EEread_cmd(void)
{
    if ((arg_count < 1) || (arg_count > 2))
    {
        printf("{\"err\":\"EeRdArgCount\"}\r\n");
        return;
    }

    if (!isdigit((unsigned char)arg[0][0]))
    {
        printf("{\"err\":\"EeRdAddrNaN\"}\r\n");
        return;
    }
    unsigned long addr = strtoul(arg[0], NULL, 10);

    const char *type = (arg_count == 2) ? arg[1] : "UINT8";
    uint8_t width = ee_type_width(type);
    if (width == 0)
    {
        printf("{\"err\":\"EeRdTypUINT8|16|32\"}\r\n");
        return;
    }

    if ((addr + width) > EEPROM_SIZE)
    {
        printf("{\"err\":\"EeRdMaxAddr_%u\"}\r\n", EEPROM_SIZE);
        return;
    }

    uint32_t value = 0;
    memcpy(&value, (const void *)(EEPROM_BASE + addr), width);
    printf("{\"EE[%lu]\":{\"r\":\"%lu\"}}\r\n", addr, (unsigned long)value);
}

// /ee addr,value[,UINT8|UINT16|UINT32]  (UINT8 if type is omitted)
// Erases and reprograms the whole EEPROM page, so this blocks for tens of
// milliseconds (no UART DMA or interrupts are serviced while flash is busy).
void EEwrite_cmd(void)
{
    if ((arg_count < 2) || (arg_count > 3))
    {
        printf("{\"err\":\"EeWrArgCount\"}\r\n");
        return;
    }

    if (!isdigit((unsigned char)arg[0][0]))
    {
        printf("{\"err\":\"EeWrAddrNaN\"}\r\n");
        return;
    }
    unsigned long addr = strtoul(arg[0], NULL, 10);

    if (!isdigit((unsigned char)arg[1][0]))
    {
        printf("{\"err\":\"EeWrValNaN\"}\r\n");
        return;
    }
    unsigned long value = strtoul(arg[1], NULL, 10);

    const char *type = (arg_count == 3) ? arg[2] : "UINT8";
    uint8_t width = ee_type_width(type);
    if (width == 0)
    {
        printf("{\"err\":\"EeWrTypUINT8|16|32\"}\r\n");
        return;
    }

    if ((addr + width) > EEPROM_SIZE)
    {
        printf("{\"err\":\"EeWrMaxAddr_%u\"}\r\n", EEPROM_SIZE);
        return;
    }

    // mask to the requested width, same as the value that will be stored
    switch (width)
    {
        case 1: value = (uint8_t)value; break;
        case 2: value = (uint16_t)value; break;
        default: value = (uint32_t)value; break;
    }

    // flash erase clears the whole page, so read-modify-write it via a RAM shadow
    static uint8_t page[EEPROM_SIZE];
    memcpy(page, (const void *)EEPROM_BASE, EEPROM_SIZE);
    memcpy(page + addr, &value, width);

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Page = (EEPROM_BASE - FLASH_BASE) / FLASH_PAGE_SIZE,
        .NbPages = 1,
    };
    uint32_t page_error;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &page_error);

    if (status == HAL_OK)
    {
        for (uint32_t i = 0; i < EEPROM_SIZE; i += sizeof(uint64_t))
        {
            uint64_t dword;
            memcpy(&dword, page + i, sizeof(dword));
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, EEPROM_BASE + i, dword);
            if (status != HAL_OK) break;
        }
    }

    HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        printf("{\"err\":\"EeWrFlashErr_%lu\"}\r\n", (unsigned long)HAL_FLASH_GetError());
        return;
    }

    uint32_t readback = 0;
    memcpy(&readback, (const void *)(EEPROM_BASE + addr), width);
    printf("{\"EE[%lu]\":{\"w\":\"%lu\",\"r\":\"%lu\"}}\r\n", addr, value, (unsigned long)readback);
}
