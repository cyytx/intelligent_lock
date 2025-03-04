/**
  ******************************************************************************
  * @file    servo.h
  * @author  cyytx
  * @brief   舵机控制模块的头文件,包含舵机的初始化、控制等功能函数声明
  ******************************************************************************
  */

#ifndef __SG90_H
#define __SG90_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include "hard_enable_ctrl.h"

#if SG90_ENABLE

#define SG90_CTL_Pin GPIO_PIN_6
#define SG90_CTL_GPIO_Port GPIOG

enum LOCK_CMD
{
    LOCK_CMD_CLOSE = 0,
    LOCK_CMD_OPEN = 1,
};

/* 舵机相关函数声明 */
void SG90_Init(void);
void SG90_Control(void);
void Set_Servo_Angle(uint16_t angle);
void PG6_SET_HIGH(void);
void PG6_SET_LOW(void);

// 添加舵机任务相关函数声明
void SG90_CreateTask(void);
void SendLockCommand(uint8_t command);

// 添加门锁状态获取函数
uint8_t IsDoorUnlocked(void);
uint32_t GetUnlockTime(void);
void ResetUnlockTimer(void);

#endif /* SG90_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __SG90_H */ 