/**
  ******************************************************************************
  * @file    beep.h
  * @author  cyytx
  * @brief   蜂鸣器模块的头文件,包含蜂鸣器的初始化、开关等功能函数声明
  ******************************************************************************
  */

#ifndef __BEEP_H
#define __BEEP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include "hard_enable_ctrl.h"

#if BEEP_ENABLE

#define BEEP_Pin GPIO_PIN_8
#define BEEP_GPIO_Port GPIOI

/* 蜂鸣器相关函数声明 */
void BEEP_Init(void);
void BEEP_On(void);
void BEEP_Off(void);

#endif /* BEEP_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __BEEP_H */ 