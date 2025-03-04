#ifndef __UART_H
#define __UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "hard_enable_ctrl.h"

#if DEBUG_UART_ENABLE

/* 调试串口相关函数声明 */
void UART_Init(void);
void UART_SendChar(uint8_t ch);
void UART_SendString(const char* str);
int fputc(int ch, FILE *f);
void UART_Mutex_Init(void);
#endif /* DEBUG_UART_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __UART_H */ 