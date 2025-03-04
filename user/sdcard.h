#ifndef __SDCARD_H
#define __SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "hard_enable_ctrl.h"

#if SDCARD_ENABLE

/* SD卡初始化函数 */
void SDCARD_Init(void);

/* SD卡存储接口 */
HAL_StatusTypeDef SDCARD_WriteVideo(uint8_t* data, uint32_t size);
HAL_StatusTypeDef SDCARD_ReadVideo(uint8_t* data, uint32_t size);
HAL_StatusTypeDef SDCARD_WriteLog(const char* log);
HAL_StatusTypeDef SDCARD_ReadLog(char* buffer, uint32_t size);

#endif /* SDCARD_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __SDCARD_H */ 