#include "uart.h"
#include "stm32f1xx_hal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;

/* ---------- 环形缓冲 ---------- */
static volatile uint8_t  rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;    /* 写指针 (ISR写) */
static volatile uint16_t rx_tail = 0;    /* 读指针 (主循环读) */

static uint8_t  tx_buf[UART_TX_BUF_SIZE];
static volatile uint16_t tx_head = 0, tx_tail = 0;
static volatile uint8_t  tx_busy = 0;

/* ---------- 初始化 ---------- */
void UART_Init(void)
{
    /* GPIO/波特率在 CubeMX 配置好, 这里只做缓冲清 */
    rx_head = rx_tail = 0;
    tx_head = tx_tail = 0;
    tx_busy = 0;

    /* 开启接收中断 + 空闲中断 (F103的CR1有IDLEIE位, 1.8.7 HAL的UART模块支持UART_IT_IDLE) */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ---------- 发送 (查询式, bootloader简单可靠) ---------- */
void UART_PutByte(uint8_t ch)
{
    HAL_UART_Transmit(&huart1, &ch, 1, 100);
}

void UART_Write(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)data, len, 1000);
}

/* ---------- 接收: 带超时取一个字节 ---------- */
int16_t UART_GetByte(uint32_t timeout_ms)
{
    uint32_t tick = HAL_GetTick();

    while (rx_head == rx_tail) {               /* 空 */
        if (HAL_GetTick() - tick > timeout_ms) return -1;
    }

    uint8_t ch = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % UART_RX_BUF_SIZE;
    return ch;
}

void UART_FlushRx(void)
{
    __disable_irq();
    rx_head = rx_tail = 0;
    __enable_irq();
}

/* ---------- 日志 ---------- */
void Log(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    UART_Write((uint8_t*)buf, strlen(buf));
}

/* ---------- 中断服务: F103 必须直接读SR/DR ---------- */
void USART1_IRQHandler(void)
{
    /* 接收非空 (RXNE) */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        uint8_t ch = (uint8_t)(huart1.Instance->DR & 0xFF);
        uint16_t next = (rx_head + 1) % UART_RX_BUF_SIZE;
        if (next != rx_tail) {        /* 满则丢弃, 保护覆盖 */
            rx_buf[rx_head] = ch;
            rx_head = next;
        }
    }

    /* 空闲中断 (IDLE): 一包数据收完的标志, YMODEM靠它判断帧边界 */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
        /* F103清IDLE标志必须: 先读SR, 再读DR */
        volatile uint32_t tmp;
        tmp = huart1.Instance->SR;
        tmp = huart1.Instance->DR;
        (void)tmp;
        /* 这里可置一个"一包到齐"信号量, 简化协议处理 */
    }
}