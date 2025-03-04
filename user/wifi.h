/**
  ******************************************************************************
  * @file    wifi.h
  * @author  cyytx
  * @brief   WiFi模块的头文件,包含WiFi的初始化、连接、数据收发等功能函数声明
  ******************************************************************************
  */

#ifndef __WIFI_H
#define __WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "hard_enable_ctrl.h"

#if WIFI_ENABLE

#define WIFI_GPIO_Port GPIOE
#define WIFI_RESET_Pin GPIO_PIN_2
#define WIFI_PDN_Pin GPIO_PIN_3


/* WiFi相关函数声明 */
void WIFI_Init(void);
void WIFI_Connect(const char* ssid, const char* password);
HAL_StatusTypeDef WIFI_SendVideo(uint8_t* data, uint32_t size);
HAL_StatusTypeDef WIFI_SendImage(uint8_t* data, uint32_t size);

#endif /* WIFI_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_H */ 