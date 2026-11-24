#ifndef __SPI_FLASH_H
#define __SPI_FLASH_H

#include <stdint.h>

#define SPI_FLASH_PAGE_SIZE    256     /* 页 */
#define SPI_FLASH_SECTOR_SIZE  4096    /* 扇区 */
#define SPI_FLASH_TOTAL_SIZE   (2*1024*1024)  /* W25Q16 = 8MB */

uint8_t  SPI_FLASH_Init(void);
uint32_t SPI_FLASH_ReadJEDEC_ID(void);

uint8_t  SPI_FLASH_Read(uint32_t addr, uint8_t *buf, uint16_t len);
uint8_t  SPI_FLASH_Write(uint32_t addr, const uint8_t *buf, uint16_t len);
uint8_t  SPI_FLASH_EraseSector(uint32_t addr);       /* 按4KB扇区擦 */
uint8_t  SPI_FLASH_EraseChip(void);
uint8_t  SPI_FLASH_WriteEnable(void);

#endif