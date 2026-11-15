#include "uart.h"

uart_status uart_transmit_str(uint8_t *data)
{
    uart_status status = UART_ERROR;
    uint16_t length = 0;

    while('\0' != data[length]) {
        length++;
    }

    if (HAL_OK == HAL_USART_Transmit(&husart1, data, length, UART_TIMEOUT)) {
        status = UART_OK;
    }

    return status;
}