#ifndef _DELAY_H
#define _DELAY_H
#include "stm32f7xx_hal.h"
 
#define  SYSTEM_SUPPORT_OS 1  //定义系统是否支持OS
void delay_init(uint8_t SYSCLK);
void delay_us(uint32_t nus);
void delay_ms(uint32_t nms);
void delay_xms(uint32_t nms);
#endif

