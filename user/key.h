
/**
  ******************************************************************************
  * @file    key.h
  * @author  cyytx
  * @brief   键盘模块的头文件,包含键盘的初始化、扫描等功能函数声明
  ******************************************************************************
  */

#ifndef __KEY_H
#define __KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include "hard_enable_ctrl.h"

#if KEY_ENABLE
//列
#define KEY_C1_GPIO_Port GPIOB
#define KEY_C1_Pin GPIO_PIN_12
#define KEY_C2_GPIO_Port GPIOB
#define KEY_C2_Pin GPIO_PIN_13
#define KEY_C3_GPIO_Port GPIOB
#define KEY_C3_Pin GPIO_PIN_14
#define KEY_C4_GPIO_Port GPIOB
#define KEY_C4_Pin GPIO_PIN_15

//行
#define KEY_R1_GPIO_Port GPIOC
#define KEY_R1_Pin GPIO_PIN_3
#define KEY_R2_GPIO_Port GPIOC
#define KEY_R2_Pin GPIO_PIN_2
#define KEY_R3_GPIO_Port GPIOC
#define KEY_R3_Pin GPIO_PIN_1



/* 按键值定义 */
typedef enum {
    KEY_NONE = 0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_ENTER,
    KEY_CANCEL,
} KeyValue_t;


// 定义密码结构体
typedef struct {
    uint8_t password_len;        // 密码长度
    uint8_t password[16];        // 密码内容，最多16位
} LockPassword_t;


/* 键盘相关函数声明 */
void KEY_Init(void);
KeyValue_t KEY_Scan(void);
void KEY_IRQHandler(uint16_t GPIO_Pin);

/* 任务相关声明 */
void KEY_CreateTask(void);
void ReadPassWardFromFlash(void);

#endif /* KEY_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __KEY_H */ 