#include "ymodem.h"
#include "uart.h"
#include "spi_flash.h"

/* YMODEM 控制字符 */
#define SOH  0x01   /* 128字节数据包 */
#define STX  0x02   /* 1024字节数据包 */
#define EOT  0x04   /* 传输结束 */
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18   /* 取消 */
#define C    0x43   /* 请求 CRC 模式 */

/* SPI Flash 暂存起始地址 (固件从W25Q64的0x000000开始存) */
#define YM_SPI_FLASH_ADDR   0x000000

static uint8_t  rx_buf[YM_PACKET_SIZE + 5];  /* 包头3 + 数据1024 + CRC2 */
static uint32_t write_addr;                  /* SPI Flash 写入偏移 */

/* CRC16 (CCITT), YMODEM标准: 初值0, 多项式0x1021 */
static uint16_t Cal_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0;
    while (len--) {
        crc ^= ((uint16_t)(*data++) << 8);
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else              crc <<= 1;
        }
    }
    return crc;
}

/* 等待一个字节, 带超时 */
static int16_t YM_WaitByte(uint32_t timeout_ms)
{
    return UART_GetByte(timeout_ms);   /* 返回 -1 表示超时 */
}

/* 发送一个字节 */
static void YM_SendByte(uint8_t ch)
{
    UART_PutByte(ch);
}

/* 清空接收缓冲(丢弃历史垃圾数据) */
static void YM_FlushRx(void)
{
    UART_FlushRx();
}

/* 接收一个完整数据包 */
/* 返回: 数据长度(128/1024), 0=EOT, -1=错误/取消, -2=超时 */
static int16_t YM_RecvPacket(uint8_t *buf, uint8_t *pkt_no)
{
    int16_t ch;
    uint16_t i, data_len;
    uint16_t crc_recv, crc_calc;

    ch = YM_WaitByte(YM_DATA_TIMEOUT);
    if (ch < 0) return -2;              /* 超时 */

    if (ch == EOT) return 0;
    if (ch == CAN) {
        if (YM_WaitByte(100) == CAN) return -1;   /* 双CAN = 主动取消 */
    }
    if (ch != SOH && ch != STX) return -1;

    data_len = (ch == STX) ? 1024 : 128;

    /* 收包号 + 取反包号 */
    int16_t b1 = YM_WaitByte(1000);
    int16_t b2 = YM_WaitByte(1000);
    if (b1 < 0 || b2 < 0) return -2;
    if ((uint8_t)b1 != (uint8_t)~b2)  return -1;  /* 包号校验错 */

    *pkt_no = (uint8_t)b1;

    /* 收数据 */
    for (i = 0; i < data_len; i++) {
        ch = YM_WaitByte(1000);
        if (ch < 0) return -2;
        buf[i] = (uint8_t)ch;
    }

    /* 收CRC */
    int16_t crch = YM_WaitByte(1000);
    int16_t crcl = YM_WaitByte(1000);
    if (crch < 0 || crcl < 0) return -2;
    crc_recv  = ((uint16_t)crch << 8) | crcl;
    crc_calc  = Cal_CRC16(buf, data_len);

    return (crc_recv == crc_calc) ? (int16_t)data_len : -1;
}

/**
 * @brief  YMODEM-1K 接收主流程, 固件写入 SPI Flash 暂存
 * @param  fw_size: 返回固件实际字节数
 * @retval YM_OK 成功
 */
 ym_status_t Ymodem_Receive(uint32_t *fw_size)
 {
    uint8_t  pkt_no   = 0;
    uint8_t  expect   = 0;
    uint8_t  retry    = 0;
    uint8_t  eot_flag = 0;
    uint8_t  stage    = 0;      /* 0=等待文件名包 1=收数据包 */
    uint32_t total    = 0;
    int16_t  len;

    YM_FlushRx();

    /* ---------- 第一阶段: 发 'C' 请求发送, 收文件名包 ---------- */
    while (retry < YM_MAX_RETRY) {
        YM_SendByte(C);
        len = YM_RecvPacket(rx_buf, &pkt_no);

        if (len == -2) { retry++; continue; }        /* 超时重发C */
        if (len == -1) { retry++; YM_FlushRx(); continue; }

        /* 收到数据包(文件名包: "文件名\0文件大小\0...") */
        YM_SendByte(ACK);
        YM_SendByte(C);                              /* 请求第一个数据包 */
        stage = 1;

        /* 解析文件大小 (SEC核心需求: 知道固件多大) */
        if (len > 0) {
            uint8_t *p = rx_buf;
            while (*p && (p - rx_buf) < len) p++;    /* 跳过文件名 */
            p++;
            *fw_size = 0;
            while (*p >= '0' && *p <= '9') {
                *fw_size = *fw_size * 10 + (*p - '0');
                p++;
            }
        } else {
            *fw_size = 0;   /* 空文件 */
            YM_SendByte(ACK);
            return YM_OK;   /* 空包直接结束 */
        }
        break;
    }
    if (stage != 1) return YM_TIMEOUT;

    /* ---------- 第二阶段: 收数据包, 写 SPI Flash ---------- */
    write_addr = YM_SPI_FLASH_ADDR;
    retry = 0;
    expect = 1;              /* 第一个数据包编号为1 */

    while (retry < YM_MAX_RETRY) {
        len = YM_RecvPacket(rx_buf, &pkt_no);

        if (len == 0) {                              /* EOT */
            if (!eot_flag) {
                YM_SendByte(NAK);                    /* 第一次EOT回NAK */
                eot_flag = 1;
            } else {
                YM_SendByte(ACK);                    /* 第二次EOT回ACK */
                YM_SendByte(C);
                /* 等待传输结束空包 */
                YM_RecvPacket(rx_buf, &pkt_no);
                YM_SendByte(ACK);
                return YM_OK;
            }
            continue;
        }

        if (len == -2) {                             /* 超时 */
            if (eot_flag) { YM_SendByte(ACK); continue; }
            retry++; continue;
        }

        if (len == -1) {                             /* CRC错/取消 */
            YM_SendByte(CAN); YM_SendByte(CAN);
            return YM_ABORT;
        }

        /* 包号连续检查 */
        if (pkt_no == expect) {
            /* 写入 SPI Flash (w25q64按页256字节写, 跨页自动处理) */
            if (SPI_FLASH_Write(write_addr, rx_buf, (uint16_t)len) != 0) {
                YM_SendByte(CAN); YM_SendByte(CAN);
                return YM_ERROR;
            }
            write_addr += len;
            total      += len;
            YM_SendByte(ACK);
            expect++;
            retry = 0;                               /* 成功则清零重试 */
        }
        else if (pkt_no == (uint8_t)(expect - 1)) {
            YM_SendByte(ACK);                        /* 重复包, 只确认 */
        }
        else {
            YM_SendByte(NAK);
            retry++;
        }
    }

    YM_SendByte(CAN); YM_SendByte(CAN);
    return YM_TIMEOUT;
 }