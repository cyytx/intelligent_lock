
#include "sdcard.h"

#if SDCARD_ENABLE

static SD_HandleTypeDef hsd1;  // SD卡存储

void SDCARD_Init(void)
{
    hsd1.Instance = SDMMC1;
    hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
    hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide = SDMMC_BUS_WIDE_4B;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv = 0;
    
    if (HAL_SD_Init(&hsd1) != HAL_OK)
    {
        Error_Handler();
    }
}

HAL_StatusTypeDef SDCARD_WriteVideo(uint8_t* data, uint32_t size)
{
    // TODO: 实现视频写入功能
    return HAL_OK;
}

HAL_StatusTypeDef SDCARD_ReadVideo(uint8_t* data, uint32_t size)
{
    // TODO: 实现视频读取功能
    return HAL_OK;
}

HAL_StatusTypeDef SDCARD_WriteLog(const char* log)
{
    // TODO: 实现日志写入功能
    return HAL_OK;
}

HAL_StatusTypeDef SDCARD_ReadLog(char* buffer, uint32_t size)
{
    // TODO: 实现日志读取功能
    return HAL_OK;
}

#endif /* SDCARD_ENABLE */ 