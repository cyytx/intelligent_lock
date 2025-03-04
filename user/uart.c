#include "FreeRTOS.h"
#include "semphr.h"
#include "ble.h"
#include "uart.h"
#include "fingerprint.h"

#if (__ARMCC_VERSION >= 6010050)            /* 使用AC6编译器时 */
 __asm(".global __use_no_semihosting\n\t");  /* 声明不使用半主机模式 */
__asm(".global __ARM_use_no_argv \n\t");    /* AC6下需要声明main函数为无参数格式，否则部分例程可能出现半主机模式 */

#else
/* 使用AC5编译器时, 不使用半主机模式 */
#pragma import(__use_no_semihosting)
#endif

#if DEBUG_UART_ENABLE

// struct __FILE 
// { 
// 	int handle; 
// }; 

// 定义互斥量句柄
static SemaphoreHandle_t uart_mutex = NULL;

FILE __stdout;       
//定义_sys_exit()以避免使用半主机模式    
void _sys_exit(int x) 
{ 
	x = x; 
} 
//重定义fputc函数 
int fputc(int ch, FILE *f)
{ 	
    BaseType_t higher_task_woken = pdFALSE;
    
    // 检查是否在中断上下文中
    if (xPortIsInsideInterrupt() == pdTRUE) {
        // 在中断中，直接发送，不使用互斥量
        while((USART1->ISR & 0X40) == 0);
        USART1->TDR = (uint8_t)ch;
    } else {
        // 在任务中，使用互斥量保护
        if (uart_mutex != NULL) {
            xSemaphoreTake(uart_mutex, portMAX_DELAY);
            while((USART1->ISR & 0X40) == 0);
            USART1->TDR = (uint8_t)ch;
            xSemaphoreGive(uart_mutex);
        } else {
            // 互斥量未创建时直接发送
            while((USART1->ISR & 0X40) == 0);
            USART1->TDR = (uint8_t)ch;
        }
    }
    return ch;
}
// int fputc(int ch, FILE *f)
// { 	
// 	while((USART1->ISR&0X40)==0);//循环发送,直到发送完毕   
// 	USART1->TDR=(uint8_t)ch;      
// 	return ch;
// }

// UART 句柄
 UART_HandleTypeDef huart1;

/* 私有函数声明 */
static void UART1_GPIO_Init(void);
static void UART1_Error_Handler(void);



/**
  * @brief  UART1 外设初始化
  */
void UART_Init(void)
{
    /* 配置 UART 参数 */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    
    /* 高级特性保持默认 */
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        // UART1_Error_Handler();
        Error_Handler();
    }
    printf("UART1 init success\r\n");
}

// 添加新函数用于初始化互斥量
void UART_Mutex_Init(void)
{
    uart_mutex = xSemaphoreCreateMutex();
    if (uart_mutex == NULL) {
        Error_Handler();
    }
    printf("UART mutex init success\r\n");
}


/**
  * @brief  UART接收完成回调函数
  * @param  huart: UART句柄指针
  * @retval 无
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    /* 判断是哪个UART接收到数据 */
    if (huart->Instance == USART1)
    {
        /* UART1接收完成处理 - 调试串口 */
        // Debug_UART_RxCpltCallback();
    }
    else if (huart->Instance == UART4)
    {
        /* UART4接收完成处理 - 指纹模块 */
        Fingerprint_RxCpltCallback();
    }
    else if (huart->Instance == UART5)
    {
        /* UART5接收完成处理 - 人脸识别模块 */
        // FaceRecog_RxCpltCallback();
    }
    else if (huart->Instance == USART6)
    {
        /* UART6接收完成处理 - BLE模块 */
        BLE_RxCpltCallback();
    }
}

#endif /* DEBUG_UART_ENABLE */ 