/**
  ******************************************************************************
  * @file    lcd_init.h
  * @author  cyytx
  * @brief   LCD显示模块的头文件,包含LCD的初始化、显示、清屏等功能函数声明
  ******************************************************************************
  */

#ifndef __LCD_INIT_H
#define __LCD_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "hard_enable_ctrl.h"

#if LCD_ENABLE

#define USE_HORIZONTAL 0  //设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏

#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 240
#define LCD_H 320
#else
#define LCD_W 320
#define LCD_H 240
#endif


    // LCD_RES------PD4
    // LCD_DC-------PD3
    // LCD_BLK------PI2
    // SPI2_NSS-----PI0
    
/* LCD控制引脚操作宏定义 */
#define LCD_RES_Clr()  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET)    //RES
#define LCD_RES_Set()  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET)

#define LCD_DC_Clr()   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET)    //DC
#define LCD_DC_Set()   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET)
 		     
//#define LCD_CS_Clr()   HAL_GPIO_WritePin(GPIOI, GPIO_PIN_0, GPIO_PIN_RESET)    //CS(NSS)
//#define LCD_CS_Set()   HAL_GPIO_WritePin(GPIOI, GPIO_PIN_0, GPIO_PIN_SET)

#define LCD_BLK_Clr()  HAL_GPIO_WritePin(GPIOI, GPIO_PIN_2, GPIO_PIN_RESET)    //BLK
#define LCD_BLK_Set()  HAL_GPIO_WritePin(GPIOI, GPIO_PIN_2, GPIO_PIN_SET)



/* LCD相关函数声明 */
void LCD_Init(void);
void LCD_Display(void);
void LCD_Clear(void);
SPI_HandleTypeDef* LCD_GetSPIHandle(void);

/* LCD数据操作函数声明 */
void LCD_WR_DATA8(uint8_t dat);
void LCD_WR_DATA(uint16_t dat);
void LCD_WR_REG(uint8_t dat);
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
#endif /* LCD_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __LCD_H */ 