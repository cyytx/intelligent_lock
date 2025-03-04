/**
  ******************************************************************************
  * @file    ir.h
  * @author  cyytx
  * @brief   红外检测模块的头文件,包含红外的初始化、检测等功能函数声明
  ******************************************************************************
  */

#ifndef __IR_H
#define __IR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hard_enable_ctrl.h"

#if INFRARED_ENABLE

#define IR_IRQ_WKUP6_Pin GPIO_PIN_11
#define IR_IRQ_WKUP6_GPIO_Port GPIOI

/* 红外检测相关函数声明 */
void INFRARED_Init(void);
void INFRARED_Detect(void);

#endif /* INFRARED_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __INFRARED_H */ 