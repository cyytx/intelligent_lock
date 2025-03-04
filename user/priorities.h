/**
  ******************************************************************************
  * @file    priorities.h
  * @brief   集中定义系统中所有的中断优先级、任务优先级和堆栈大小
  ******************************************************************************
  */
#ifndef __PRIORITIES_H
#define __PRIORITIES_H

/*
## 中断优先级规则
注意：数值越小，优先级越高。
stm32f7 中断优先级只有0-15总共十六个等级，
目前的配置为HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4); 所以有16个抢占优先级。
*/
/*
需要注意configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY变量，它目前为5，
所以要使用FreeRTOS API中断优先级不能低于5，否则会导致FreeRTOS API无法调用。
*/
#define BLE_IRQ_PRIORITY_USART6             7    /* 蓝牙串口中断优先级 */
#define KEY_IRQ_PRIORITY_EXTI               6    /* 外部中断优先级（键盘） */
#define SG90_IRQ_PRIORITY_TIM2              6    /* 定时器2中断优先级（舵机） */
#define LCD_IRQ_PRIORITY_DMA_SPI2           7    /* LCD DMA中断优先级 */
#define OV2640_IRQ_PRIORITY_DCMI            7    /* DCMI中断优先级（摄像头） */
#define OV2640_IRQ_PRIORITY_DMA_DCMI        7    /* DCMI中断优先级（摄像头） */
#define FINGERPRINT_IRQ_PRIORITY_USART4     7    /* 指纹串口中断优先级 */
#define FINGERPRINT_IRQ_PRIORITY_EXTI       6    /* 指纹外部中断优先级 */


/**
 * @注意：FreeRTOS任务优先级规则
 * 1. 数值越大，优先级越高。
 * 2. 建议任务优先级分配：
 *    - 0：空闲任务（系统定义）
 *    - 1-2：后台任务（LED、状态监控等）
 *    - 3-5：用户界面任务
 *    - 6-8：通信和IO处理任务
 *    - 9-10：关键任务
 */

/* 用户任务优先级 最大优先级数量由 configMAX_PRIORITIES定义，目前是56，IDLE任务优先级为0 tskIDLE_PRIORITY*/

#define TASK_PRIORITY_LED               1    /* LED任务优先级,仅大于IDLE任务优先级，永远显示是否卡死 */
#define TASK_PRIORITY_SG90              20    /* 舵机控制任务优先级，控制门锁的，优先级要高 */
#define TASK_PRIORITY_KEYBOARD          18    /* 按键任务优先级 */
#define TASK_PRIORITY_DISPLAY           17    /* 显示任务优先级 */
#define TASK_PRIORITY_NFC               14    /* NFC任务优先级 */
#define TASK_PRIORITY_FINGERPRINT       15    /* 指纹识别任务优先级 */
#define TASK_PRIORITY_BLE               15    /* 蓝牙任务优先级 */


/* 任务堆栈大小定义，不能小于configMINIMAL_STACK_SIZE定义的值 目前为128*/
#define STACK_SIZE_LED                  128  /* LED任务堆栈（512字节） */
#define STACK_SIZE_SG90                 512  /* 舵机控制任务堆栈（512字节） */
#define STACK_SIZE_KEYBOARD             512  /* 键盘任务堆栈（512字节） */
#define STACK_SIZE_DISPLAY              512  /* 显示任务堆栈（512字节） */
#define STACK_SIZE_NFC                  512  /* NFC任务堆栈（512字节） */
#define STACK_SIZE_FINGERPRINT          512  /* 指纹识别任务堆栈（512字节） */
#define STACK_SIZE_BLE                  512  /* 蓝牙任务堆栈（512字节） */

#endif /* __PRIORITIES_H */
