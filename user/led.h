
/**
  ******************************************************************************
  * @file    led.h
  * @author  cyytx
  * @brief   LED模块的头文件,包含LED的初始化、开关等功能函数声明
  ******************************************************************************
  */

#ifndef __LED_H
#define __LED_H

#ifdef __cplusplus
extern "C" {
#endif
#include "stm32f7xx_hal.h"
#include "hard_enable_ctrl.h"

#if LED_ENABLE

#define LED2_Pin GPIO_PIN_4
#define LED2_GPIO_Port GPIOE

/* LED相关函数声明 */
void LED_Init(void);
void LED_On(void);
void LED_Off(void);
void LED_Toggle(void);
void LED_Blink(uint32_t delay_ms);
void LedTask_Create(void);

#endif /* LED_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __LED_H */ 