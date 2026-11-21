#ifndef UART_H_
#define UART_H_

#include <stdint.h>

#define UART_RX_BUF_SIZE  2048
#define UART_TX_BUF_SIZE  256

void UART_Init();
void UART_PutByte(uint8_t ch);
void UART_Write(const uint8_t *data, uint16_t len);
int16_t UART_GetByte(uint32_t timeout_ms);
void UART_FlushRx(void);
void Log(const char *fmt, ...);    /* 日志输出 */

#endif /* UART_H_ */