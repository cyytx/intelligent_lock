#ifndef __CAMERA_H
#define __CAMERA_H

#include "stm32f7xx_hal.h"
#include "lcd_init.h"

#define DCMI_UINT8 0
#define DCMI_UINT16 1

extern void (*dcmi_rx_callback)(void);//DCMI DMA接收回调函数

extern DCMI_HandleTypeDef DCMI_Handler;        //DCMI句柄
extern DMA_HandleTypeDef  DMADMCI_Handler;     //DMA句柄

//定义DMA BUFFER,每个buffer为1/4的LCD大小
#if DCMI_UINT8
#define DCMI_BUF_SIZE (LCD_W*LCD_H*2)  // 以8位数据为单位的大小
#else
#define DCMI_BUF_SIZE (LCD_W*LCD_H)  // 以16位数据为单位的大小
#endif

void DCMI_Init(void);
void DCMI_DMA_Init(uint32_t mem0addr,uint32_t mem1addr,uint16_t memsize,uint32_t meminc);
void CAMERA_Start(void);
void CAMERA_Stop(void);
void DCMI_Set_Window(uint16_t sx,uint16_t sy,uint16_t width,uint16_t height);
void DCMI_CR_Set(uint8_t pclk,uint8_t hsync,uint8_t vsync);
#endif
