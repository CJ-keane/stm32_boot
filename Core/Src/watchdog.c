#include "watchdog.h"
#include "stm32f1xx_hal.h"

static IWDG_HandleTypeDef hiwdg;

/**
 * @brief  初始化独立看门狗
 * @param  timeout_ms: 期望超时时间 (ms)
 * @retval 0成功 1失败
 * @note   IWDG 一旦启动就无法停止, 只能复位芯片后才能重新配置
 */
uint8_t WatchDog_Init(uint32_t timeout_ms)
{
    /* 1. 开启 LSI (内部40kHz RC), IWDG的唯一时钟源 */
    /*    注意: 如果开启了IWDG硬件看门狗选项字节, LSI会自动开, 
              且上电后自动跑, 软件都拦不住 */
    __HAL_RCC_LSI_ENABLE();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == RESET);  /* 等稳定 */

    /* 2. 选分频 + 算重载值 */
    /*    IWDG 时钟 = LSI(约40kHz) / 分频
          公式: 超时 = (Reload + 1) * 分频 / 40000
          F103 分频可选: 4/8/16/32/64/128/256
          各分频最大超时: 4→409ms  8→819ms  16→1638ms  32→3276ms
                          64→6553ms 128→13107ms 256→26214ms
          注意: 必须按"最大超时"选分频, 否则 Reload 超 4095 被截断后
                实际超时会比要求的值短, 容易误复位 */
    static const uint32_t div_tab[7]   = {4, 8, 16, 32, 64, 128, 256};
    static const uint32_t presc_tab[7] = {IWDG_PRESCALER_4,   IWDG_PRESCALER_8,
                                          IWDG_PRESCALER_16,  IWDG_PRESCALER_32,
                                          IWDG_PRESCALER_64,  IWDG_PRESCALER_128,
                                          IWDG_PRESCALER_256};
    uint32_t lsi_khz = 40;                /* LSI 约 40kHz (实际 37~41kHz) */
    uint8_t  idx     = 6;                 /* 默认用最大分频 */
    uint32_t reload  = 4095;

    for (uint8_t i = 0; i < 7; i++) {
        /* Reload = 超时(ms) * 40 / 分频 - 1, 12 位计数器最大 4095 */
        reload = timeout_ms * lsi_khz / div_tab[i];
        if (reload > 0) reload -= 1;
        if (reload <= 4095) { idx = i; break; }
    }
    if (reload > 4095) reload = 4095;     /* 超过 26s 只能截断到上限 */

    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = presc_tab[idx];
    hiwdg.Init.Reload    = reload;

    /* 调试时: CPU 被断点停住时暂停计数器, 否则一停就被狗复位 */
#ifdef DEBUG
    __HAL_DBGMCU_FREEZE_IWDG();
#endif

    /* 3. 启动 (写KR=0xCCCC启动, 0x5555允许访问PR/RLR) */
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        return 1;
    }

    return 0;
}

/**
 * @brief  喂狗 (在 main 循环和耗时操作中调用)
 */
void WatchDog_Refresh(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}