#ifndef UART_H_
#define UART_H_

#include "stm32f1xx_hal.h"

extern USART_HandleTypeDef husart1;

typedef enum {
    UART_OK    =  0x00U,
    UART_ERROR =  0xFFU,
} uart_status;

uart_status uart_transmit_str(uint8_t *data);

#define UART_TIMEOUT ((uint16_t)1000u)

#endif /* UART_H_ */