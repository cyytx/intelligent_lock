/**
  ******************************************************************************
  * @file    led.c
  * @author  cyytx
  * @brief   LED模块的源文件,实现LED的初始化、开关等功能
  ******************************************************************************
  */
#include "FreeRTOS.h"
#include "task.h"
#include "priorities.h"   
#include "led.h"


#if LED_ENABLE
static TaskHandle_t xLedTaskHandle = NULL;   // 显示任务句柄

void LED_Init(void)
{
    // LED初始化代码
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOE, LED2_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = LED2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

void LED_On(void)
{
    HAL_GPIO_WritePin(GPIOE, LED2_Pin, GPIO_PIN_RESET);
}

void LED_Off(void)
{
    HAL_GPIO_WritePin(GPIOE, LED2_Pin, GPIO_PIN_SET);
}

void LED_Toggle(void)
{
    HAL_GPIO_TogglePin(GPIOE, LED2_Pin);
}

void LED_Blink(uint32_t delay_ms)
{
    LED_On();
    HAL_Delay(delay_ms);
    LED_Off();
    HAL_Delay(delay_ms);
}

/* 在文件末尾添加 LED 任务函数 */
void StartLedTask(void *argument)
{
    /* Infinite loop */
    while (1)
    {
        LED_Toggle();  // 翻转 LED 状态
        //printf("LED_Toggle\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));  // 延时 500ms
    }
}

void LedTask_Create(void)
{
    
    // 创建显示任务（优先级3，堆栈512字）
    xTaskCreate(StartLedTask,       // 任务函数
               "ledTask",      // 任务名称
               STACK_SIZE_LED, // 堆栈大小（单位字）
               NULL,               // 参数
               TASK_PRIORITY_DISPLAY, // 优先级
               &xLedTaskHandle);// 任务句柄
}

#endif /* LED_ENABLE */ 