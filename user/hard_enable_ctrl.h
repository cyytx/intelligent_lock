
/**
  ******************************************************************************
  * @file    hard_enable_ctrl.h
  * @author  cyytx
  * @brief   硬件模块使能控制的头文件,统一管理所有外设模块的使能宏
  ******************************************************************************
  */

#ifndef __HARD_ENABLE_CTRL_H
#define __HARD_ENABLE_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

/* LCD模块使能控制 */
#define LCD_ENABLE 1

/* SDRAM模块使能控制 */
#define SDRAM_ENABLE 0

/* 摄像头模块使能控制 */
#define CAMERA_ENABLE 1

/* WiFi模块使能控制 */
#define WIFI_ENABLE 0

/* NFC模块使能控制 */
#define NFC_ENABLE 1

/* 指纹模块使能控制 */
#define FINGERPRINT_ENABLE 1

/* 人脸识别模块使能控制 */
#define FACE_ENABLE 0

/* 蓝牙模块使能控制 */
#define BLE_ENABLE 1

/* 红外检测模块使能控制 */
#define INFRARED_ENABLE 0

/* 舵机控制模块使能控制 */
#define SG90_ENABLE 1

/* 按键模块使能控制 */
#define KEY_ENABLE 1

/* LED模块使能控制 */
#define LED_ENABLE 1

/* 蜂鸣器模块使能控制 */
#define BEEP_ENABLE 1

/* 调试串口使能控制 */
#define DEBUG_UART_ENABLE 1

/* SD卡使能控制 */
#define SDCARD_ENABLE 0


#ifdef __cplusplus
}
#endif

#endif /* __HARD_ENABLE_CTRL_H */ 