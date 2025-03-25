#ifndef __SDCARD_H
#define __SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "hard_enable_ctrl.h"

#if SDCARD_ENABLE

#define SD_TIMEOUT      ((uint32_t)1000)       //超时时间 1000ms

/* SD卡初始化函数 */
void SDCARD1_Init(void);
uint8_t SD_Card_Test(void);
uint8_t SD_Card_Test_DMA(void);
uint8_t SD_ReadDisk(uint8_t* buf,uint32_t sector,uint8_t cnt);
uint8_t SD_WriteDisk(uint8_t* buf,uint32_t sector,uint8_t cnt);
uint8_t SD_WriteBlocks_DMA(uint8_t *buf, uint64_t sector, uint32_t cnt);
uint8_t SD_ReadBlocks_DMA(uint8_t *buf, uint64_t sector, uint32_t cnt);
#endif /* SDCARD_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __SDCARD_H */ 