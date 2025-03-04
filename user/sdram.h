/**
  ******************************************************************************
  * @file    sdram.h
  * @author  cyytx
  * @brief   SDRAM模块的头文件,包含SDRAM的初始化、读写等功能函数声明
  ******************************************************************************
  */

#ifndef __SDRAM_H
#define __SDRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "hard_enable_ctrl.h"

#if SDRAM_ENABLE

/* SDRAM相关函数声明 */
void SDRAM_Init(void);
void SDRAM_WriteBuffer(uint32_t* buffer, uint32_t address, uint32_t size);
void SDRAM_ReadBuffer(uint32_t* buffer, uint32_t address, uint32_t size);

#endif /* SDRAM_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __SDRAM_H */ 