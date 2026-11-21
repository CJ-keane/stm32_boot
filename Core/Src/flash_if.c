#include "flash_if.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/**
 * @brief Flash 接口初始化: 解锁 + 清错误标志
 * @note  每次上电只需调用一次, 之后擦写无需重复解锁
 */
uint8_t FlashIf_Init(void)
{
    HAL_FLASH_Unlock();          /* 清除 FLASH_CR 的 LOCK 位 */

    /* 清除可能残留的错误标志, 防止上次操作失败影响本次 */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP    |   /* 操作完成 */
                           FLASH_FLAG_PGERR  |   /* 编程错误 */
                           FLASH_FLAG_WRPERR);    /* 写保护错误 */

    return 0;
}

/**
 * @brief  按页擦除一片区域
 * @param  addr: 起始地址 (须2KB对齐)
 * @param  size: 要擦除的字节数
 * @retval 0成功 1失败
 * @note   F103擦除粒度=页, 擦除后整页变为0xFF
 */
uint8_t FlashIf_Erase(uint32_t addr, uint32_t size)
{
    uint32_t page_addr = addr;
    uint32_t page_num  = (size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    FLASH_EraseInitTypeDef erase_cfg;
    uint32_t page_error  = 0;

    erase_cfg.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_cfg.PageAddress = page_addr;    /* F1的字段名, F4是Sector */
    erase_cfg.NbPages     = page_num;

    HAL_FLASH_Unlock();                   /* 防万一, 确保已解锁 */

    if (HAL_FLASHEx_Erase(&erase_cfg, &page_error) != HAL_OK) {
        return 1;
    }
    return 0;
}

/**
 * @brief  半字编程
 * @param  addr: 写入地址, 必须2字节对齐
 * @param  data: 半字数据指针
 * @param  halfword_len: 半字个数 (字节数 = halfword_len * 2)
 * @retval 0成功 1失败
 * @note   F103 的最小编程粒度是半字(16bit), 且只能 1→0 修改
 */
uint8_t FlashIf_Write(uint32_t addr, uint16_t *data, uint16_t halfword_len)
{
    for (uint16_t i = 0; i < halfword_len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              addr + i * 2, data[i]) != HAL_OK) {
            return 1;
        }
        /* 可选: 写入后立刻回读校验, 失败立即返回 */
        if (*(__IO uint16_t*)(addr + i * 2) != data[i]) {
            return 1;
        }
    }
    return 0;
}

/* ---------- 标志区读写 ---------- */

/**
 * @brief 读标志值 (直接从Flash映射地址读, 无需解锁)
 */
uint32_t FlashIf_ReadFlag(uint32_t addr)
{
    return *(__IO uint32_t*)addr;
}

/**
 * @brief 写标志位
 * @note  Flash只能从1改0, 所以要先擦除标志所在页再写
 *        这也是把标志区单独放一页(2KB)的原因, 避免频繁擦整页数据
 */
uint8_t FlashIf_WriteFlag(uint32_t addr, uint32_t value)
{
    /* 先读出整页暂存 (可选, 如果整页只存这几个标志, 直接擦就行) */
    static uint8_t page_buf[FLASH_PAGE_SIZE];
    uint32_t page_base = addr & ~(FLASH_PAGE_SIZE - 1);
    uint32_t offset    = addr - page_base;

    memcpy(page_buf, (void*)page_base, FLASH_PAGE_SIZE);

    /* 修改目标位置 */
    memcpy(page_buf + offset, &value, 4);

    /* 擦除 + 回写整页 */
    if (FlashIf_Erase(page_base, FLASH_PAGE_SIZE)) return 1;

    for (uint32_t i = 0; i < FLASH_PAGE_SIZE / 2; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              page_base + i * 2,
                              ((uint16_t*)page_buf)[i]) != HAL_OK) {
            return 1;
        }
    }
    return 0;
}