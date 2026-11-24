#include "spi_flash.h"
#include "spi.h"          /* HAL生成的SPI句柄 hspi1 */

#define FLASH_CS_LOW()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define FLASH_CS_HIGH()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)

/* 指令集 */
#define CMD_WRITE_ENABLE   0x06
#define CMD_WRITE_DISABLE  0x04
#define CMD_READ_STATUS1   0x05
#define CMD_READ_DATA      0x03
#define CMD_PAGE_PROGRAM   0x02
#define CMD_SECTOR_ERASE   0x20   /* 4KB */
#define CMD_CHIP_ERASE     0xC7
#define CMD_JEDEC_ID       0x9F
#define CMD_POWER_UP       0xAB

#define STATUS_WIP_MASK    0x01   /* 写进行中 */

extern SPI_HandleTypeDef hspi1;

/* SPI 读写一个字节 */
static uint8_t SPI_SwapByte(uint8_t tx)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, 100);
    return rx;
}

static void SPI_WriteByte(uint8_t tx)
{
    HAL_SPI_Transmit(&hspi1, &tx, 1, 100);
}

/* 读状态寄存器1 */
static uint8_t SPI_FLASH_ReadStatus(void)
{
    uint8_t status;
    FLASH_CS_LOW();
    SPI_WriteByte(CMD_READ_STATUS1);
    status = SPI_SwapByte(0xFF);
    FLASH_CS_HIGH();
    return status;
}

/* 等待写完成 (WEL自动清零, WIP=0表示空闲) */
static uint8_t SPI_FLASH_WaitBusy(uint32_t timeout_ms)
{
    uint32_t tick = HAL_GetTick();
    while (SPI_FLASH_ReadStatus() & STATUS_WIP_MASK) {
        if (HAL_GetTick() - tick > timeout_ms) return 1;   /* 超时 */
    }
    return 0;
}

uint8_t SPI_FLASH_Init(void)
{
    FLASH_CS_HIGH();
    HAL_Delay(1);
    /* 唤醒芯片 (从power-down模式) */
    FLASH_CS_LOW();
    SPI_WriteByte(CMD_POWER_UP);
    FLASH_CS_HIGH();
    HAL_Delay(1);
    return 0;
}

/* 读 JEDEC ID: 正确应返回 0xEF4017 (W25Q64) */
uint32_t SPI_FLASH_ReadJEDEC_ID(void)
{
    uint32_t id = 0;
    FLASH_CS_LOW();
    SPI_WriteByte(CMD_JEDEC_ID);
    id |= (uint32_t)SPI_SwapByte(0xFF) << 16;   /* Manufacturer: 0xEF */
    id |= (uint32_t)SPI_SwapByte(0xFF) << 8;    /* Memory Type:  0x40 */
    id |= (uint32_t)SPI_SwapByte(0xFF);         /* Capacity:     0x17 */
    FLASH_CS_HIGH();
    return id;
}

uint8_t SPI_FLASH_WriteEnable(void)
{
    FLASH_CS_LOW();
    SPI_WriteByte(CMD_WRITE_ENABLE);
    FLASH_CS_HIGH();
    return 0;
}

/* 读数据, 任意地址任意长度 */
uint8_t SPI_FLASH_Read(uint32_t addr, uint8_t *buf, uint16_t len)
{
    FLASH_CS_LOW();
    SPI_WriteByte(CMD_READ_DATA);
    SPI_WriteByte((addr >> 16) & 0xFF);
    SPI_WriteByte((addr >> 8)  & 0xFF);
    SPI_WriteByte(addr & 0xFF);
    HAL_SPI_Receive(&hspi1, buf, len, 1000);
    FLASH_CS_HIGH();
    return 0;
}

/* 页编程 (内部用, 单次≤256字节且不跨页) */
static uint8_t SPI_FLASH_PageWrite(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    SPI_FLASH_WriteEnable();
    FLASH_CS_LOW();
    SPI_WriteByte(CMD_PAGE_PROGRAM);
    SPI_WriteByte((addr >> 16) & 0xFF);
    SPI_WriteByte((addr >> 8)  & 0xFF);
    SPI_WriteByte(addr & 0xFF);
    HAL_SPI_Transmit(&hspi1, (uint8_t*)buf, len, 1000);
    FLASH_CS_HIGH();
    return SPI_FLASH_WaitBusy(100);   /* 页写约0.4ms */
}

/**
 * @brief  带跨页处理的通用写 (YMODEM调用入口)
 * @note   W25Qxx限制: 写之前必须先擦除(0xFF), 由上层保证扇区已擦
 */
uint8_t SPI_FLASH_Write(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    uint16_t page_remain = SPI_FLASH_PAGE_SIZE - (addr % SPI_FLASH_PAGE_SIZE);

    while (len > 0) {
        uint16_t chunk = (len > page_remain) ? page_remain : len;
        if (SPI_FLASH_PageWrite(addr, buf, chunk)) return 1;

        addr        += chunk;
        buf         += chunk;
        len         -= chunk;
        page_remain  = SPI_FLASH_PAGE_SIZE;   /* 后续整页 */
    }
    return 0;
}

/* 擦除4KB扇区, addr为扇区内任意地址 */
uint8_t SPI_FLASH_EraseSector(uint32_t addr)
{
    addr &= ~(SPI_FLASH_SECTOR_SIZE - 1);     /* 对齐到4KB */

    SPI_FLASH_WriteEnable();
    FLASH_CS_LOW();
    SPI_WriteByte(CMD_SECTOR_ERASE);
    SPI_WriteByte((addr >> 16) & 0xFF);
    SPI_WriteByte((addr >> 8)  & 0xFF);
    SPI_WriteByte(addr & 0xFF);
    FLASH_CS_HIGH();
    return SPI_FLASH_WaitBusy(500);           /* 扇区擦除约45-400ms */
}

uint8_t SPI_FLASH_EraseChip(void)
{
    SPI_FLASH_WriteEnable();
    FLASH_CS_LOW();
    SPI_WriteByte(CMD_CHIP_ERASE);
    FLASH_CS_HIGH();
    return SPI_FLASH_WaitBusy(20000);         /* 整片擦除约几秒 */
}