
/**
  ******************************************************************************
  * @file    buzzer.c
  * @author  cyytx
  * @brief   蜂鸣器模块的源文件,实现蜂鸣器的初始化、开关等功能
  ******************************************************************************
  */

#include "beep.h"

#if BEEP_ENABLE

void BEEP_Init(void)
{
    // 蜂鸣器初始化代码
    //时钟
    __HAL_RCC_GPIOI_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

      /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);//PI8

    //__HAL_RCC_GPIOI_CLK_ENABLE();
    GPIO_InitStruct.Pin = BEEP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BEEP_GPIO_Port, &GPIO_InitStruct);
}

void BEEP_On(void)
{
    // 蜂鸣器开启代码
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);
}

void BEEP_Off(void)
{
    // 蜂鸣器关闭代码
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
}

#endif /* BEEP_ENABLE */ 