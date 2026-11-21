#ifndef __FLASH_IF_H
#define __FLASH_IF_H

#include <stdint.h>

/* F103ZET6 Flash 参数 */
#define FLASH_PAGE_SIZE      2048     /* 大容量系列每页 2KB (小/中容量是1KB) */

/* 标志区: 最后一页 0x0807F800 - 0x0807FFFF */
#define FLAG_ADDR            0x0807F800

/* 标志值定义 */
#define FLAG_EMPTY           0xFFFFFFFF
#define FLAG_UPDATE_REQ      0xA5A5A5A5   /* APP请求升级 */
#define FLAG_NEW_FW_OK       0x5A5A5A5A   /* 新固件校验通过 */
#define FLAG_APP_CONFIRMED   0xAA55AA55   /* APP运行自确认(防回滚误判) */

uint8_t  FlashIf_Init(void);
uint8_t  FlashIf_Erase(uint32_t addr, uint32_t size);
uint8_t  FlashIf_Write(uint32_t addr, uint16_t *data, uint16_t halfword_len);
uint32_t FlashIf_ReadFlag(uint32_t addr);
uint8_t  FlashIf_WriteFlag(uint32_t addr, uint32_t value);

#endif