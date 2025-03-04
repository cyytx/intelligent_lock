/**
  ******************************************************************************
  * @file    wifi.c
  * @author  cyytx
  * @brief   WiFi模块的源文件,实现WiFi的初始化、连接、数据收发等功能
  ******************************************************************************
  */

#include "wifi.h"

#if WIFI_ENABLE

static SD_HandleTypeDef hsd2;  // W8801 SDIO接口

//WIFI相关GPIO初始化
static void WIFI_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

      /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(WIFI_GPIO_Port, WIFI_RESET_Pin|WIFI_PDN_Pin, GPIO_PIN_SET);

    /*Configure GPIO pins : WIFI_RESET_Pin WIFI_PDN_Pin */
  GPIO_InitStruct.Pin = WIFI_RESET_Pin|WIFI_PDN_Pin
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

void WIFI_Init(void)
{
    //WIFI相关GPIO初始化
    WIFI_GPIO_Init();

    // 初始化SDMMC2接口
    hsd2.Instance = SDMMC2;
    hsd2.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd2.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
    hsd2.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd2.Init.BusWide = SDMMC_BUS_WIDE_4B;
    hsd2.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd2.Init.ClockDiv = 0;
    
    if (HAL_SD_Init(&hsd2) != HAL_OK)
    {
        Error_Handler();
    }
    
    
    // TODO: 添加W8801模块的具体初始化代码
}

void WIFI_Connect(const char* ssid, const char* password)
{
    // TODO: 实现WiFi连接功能
}

HAL_StatusTypeDef WIFI_SendVideo(uint8_t* data, uint32_t size)
{
    // TODO: 实现视频上传功能
    return HAL_OK;
}

HAL_StatusTypeDef WIFI_SendImage(uint8_t* data, uint32_t size)
{
    // TODO: 实现图片上传功能
    return HAL_OK;
}

#endif /* WIFI_ENABLE */ 