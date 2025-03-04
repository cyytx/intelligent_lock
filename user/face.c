/**
  ******************************************************************************
  * @file    face.c
  * @author  cyytx
  * @brief   人脸识别模块的源文件,实现人脸检测、匹配等功能
  ******************************************************************************
  */

#include "face.h"

#if FACE_ENABLE

static UART_HandleTypeDef huart5;  // 人脸识别模块串口句柄

void FACE_Init(void)
{
    huart5.Instance = UART5;
    huart5.Init.BaudRate = 115200;
    huart5.Init.WordLength = UART_WORDLENGTH_8B;
    huart5.Init.StopBits = UART_STOPBITS_1;
    huart5.Init.Parity = UART_PARITY_NONE;
    huart5.Init.Mode = UART_MODE_TX_RX;
    huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart5.Init.OverSampling = UART_OVERSAMPLING_16;
    huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    
    if (HAL_UART_Init(&huart5) != HAL_OK)
    {
        Error_Handler();
    }
    
    // TODO: 添加人脸识别模块的其他初始化代码
}

void FACE_Register(void)
{
    // TODO: 实现人脸注册功能
}

void FACE_Recognize(void)
{
    // TODO: 实现人脸识别功能
}

#endif /* FACE_ENABLE */ 