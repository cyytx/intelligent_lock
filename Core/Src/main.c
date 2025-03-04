/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "lcd_init.h"
#include "lcd.h"
#include "uart.h"
#include "fingerprint.h"
#include "face.h"
#include "ble.h"
#include "nfc.h"
#include "sdram.h"
#include "sdcard.h"
#include "wifi.h"
#include "key.h"
#include "led.h"
#include "ov2640.h"
#include "beep.h"
#include "sg90.h"
#include "nfc.h"
#include "delay.h"
#include "ble.h"


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* MPU Configuration--------------------------------------------------------*/
    MPU_Config();

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    UART_Init();
    delay_init(96);
    LED_Init();
    LCD_Init();
    LCD_SHOW();

    KEY_Init();//锁密码也在里面读出
    BEEP_Init();
    SG90_Init();
    FP_Init();
    
    NFC_Init();
    BLE_Init();
    OV2640_Init();
    OV2640_DISPLAY();

    osKernelInitialize();
    /* 在这里初始化UART互斥量 */
    UART_Mutex_Init();
    /* 创建 LED 任务 */
    LedTask_Create();
    
    BLE_CreateTask();
    
    /* 创建键盘任务 */
    KEY_CreateTask();

    /* 创建显示任务 */
    DisplayTask_Create();

    /* 创建NFC任务 */
    NFC_CreateTask();

    /* 创建舵机任务 */
    SG90_CreateTask();

    /* 创建指纹任务 */
    FP_CreateTask();

    /* Start scheduler */
     osKernelStart();

    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  //设置电压调节器输出电压,用以配置最低支持电压，和最高主频也有关系,如果要提高主频，需要改这个。
  /*
    SCALE1 ：最高电压（1.2V），支持系统运行在最高频率（如216 MHz），适用于需要高性能的场景，但功耗较高 
    SCALE2 ：中等电压（1.0V），支持180 MHz，平衡性能与功耗 
    SCALE3 ：最低电压（0.8V），限制频率至144 MHz，用于低功耗场景，但可能影响仿真稳定性（如调试时异常）
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
    //HSE 25Mhz 使用25Mhz的晶振
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;//使用外部高速晶振HSE
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;//使能HSE
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;//使能PLL
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;//使用HSE作为PLL的时钟源
  RCC_OscInitStruct.PLL.PLLM = 25;//除以25,可以调控它设置主频
  RCC_OscInitStruct.PLL.PLLN = 192;//乘以192，可以调控它设置主频
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;//主频SYSCLK=25/25*192/2=96Mhz,HCKL为96Mhz
  RCC_OscInitStruct.PLL.PLLQ = 4;//
  RCC_OscInitStruct.PLL.PLLR = 2;//
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;//不分频率，即HCKL=SYSCLK=96Mhz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;//96/2=48Mhz，APB1最大54Mhz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; //96Mhz，APB2最大108Mhz

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}


 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /* 配置区域0 - 设置全局保护，但允许特定区域访问 */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* 配置区域1 - 专门允许Flash区域访问 */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x08000000;  // Flash 基地址
  MPU_InitStruct.Size = MPU_REGION_SIZE_1MB;  // 调整大小以匹配您的Flash大小
  MPU_InitStruct.SubRegionDisable = 0x00;  // 不禁用任何子区域
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;  // 允许完全访问
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* 启用MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
 //在stm32 hal库中使用HAL_Delay 则是靠HAL_IncTick 来实现的，默认的设置是1ms中断一次，正常来说，可以使用
 //systick 中断来实现，在SysTick_Handler中添加HAL_IncTick();就可以，但前提是如果rtos,则需要配置systick中断
 //也要是1ms中断一次，否则会影响到HAL_Delay的准确性，也就是configTICK_RATE_HZ需要配置为1000。目前资源较为
 //丰富，所以HAL_IncTick来实现，这个就不受到configTICK_RATE_HZ的影响了，始终是1ms中断一次。
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  printf("Error occurred in %s on line %d\r\n", file, line);
  while (1)
  {
    LED_Blink(200);//快速闪烁
  }
  /* USER CODE END Error_Handler_Debug */
}


#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
     printf("Wrong parameters value: file %s on line %d\r\n", file, line);
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

