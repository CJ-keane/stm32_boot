#ifndef __YMODEM_H
#define __YMODEM_H

#include <stdint.h>

#define YM_PACKET_SIZE    1024    /* 1K 模式 */
#define YM_DATA_TIMEOUT   1000    /* 数据包超时 ms */
#define YM_MAX_RETRY      10      /* 最大重试次数 */

typedef enum {
    YM_OK = 0,
    YM_TIMEOUT,
    YM_ABORT,
    YM_ERROR
} ym_status_t;

ym_status_t Ymodem_Receive(uint32_t *fw_size);  /* 接收固件到SPI Flash */

#endif