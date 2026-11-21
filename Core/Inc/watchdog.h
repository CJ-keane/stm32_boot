/* watchdog.h */
#ifndef __WATCHDOG_H
#define __WATCHDOG_H

#include <stdint.h>

uint8_t WatchDog_Init(uint32_t timeout_ms);
void    WatchDog_Refresh(void);

#endif