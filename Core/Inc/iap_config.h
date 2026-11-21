#ifndef IAP_CONFIG_H_
#define IAP_CONFIG_H_

#define FLASH_BASE_ADDR      0x08000000
#define FLASH_END_ADDR       0x0807FFFF

/* Bootloader 区: 0x08000000 - 0x0800FFFF  (64KB) */
#define BOOTLOADER_SIZE      0x10000

/* APP 运行区: 0x08010000 - 0x0805FFFF (320KB) */
#define APP_ADDR             0x08010000
#define APP_MAX_SIZE         0x50000

/* APP 备份区: 0x08060000 - 0x0807FFFF (128KB 双备份用) */
#define APP_BAK_ADDR         0x08060000

/* 升级状态标志,存放在 Flash 最后一页 (0x0807FC00), 保证断电不丢 */
#define FLAG_ADDR            0x0807FC00

/* 固件头结构: APP 区前 16 字节固定布局 */
typedef struct {
    uint32_t magic;        // 0xA5A5A5A5 表示有效固件
    uint32_t size;         // 固件实际长度
    uint32_t version;      // 版本号
    uint32_t crc32;        // 整包 CRC32
} fw_header_t;

#endif