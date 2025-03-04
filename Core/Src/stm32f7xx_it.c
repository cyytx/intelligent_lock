/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f7xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32f7xx_hal.h"
#include "stm32f7xx_it.h"
#include "main.h"
#include "led.h"
#include "fingerprint.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern HCD_HandleTypeDef hhcd_USB_OTG_FS;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart4;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M7 Processor Interruption and Exception Handlers          */
/******************************************************************************/

// 添加异常栈帧结构体定义
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
} StackFrame_t;

void print_fault_info(void)
{
  // 获取当前堆栈指针(MSP或PSP)
  uint32_t current_sp;

  if(__get_CONTROL() & 0x02) {
    current_sp = __get_PSP();  // 如果使用PSP
    printf("\r\n=== Using PSP: 0x%08X ===\r\n", current_sp);
  } else {
    current_sp = __get_MSP();  // 如果使用MSP
    printf("\r\n=== Using MSP: 0x%08X ===\r\n", current_sp);
  }

  // 将堆栈指针转换为异常栈帧结构
  StackFrame_t* stack_frame = (StackFrame_t*)current_sp;

  // 打印异常发生时的寄存器值
  printf("\r\n=== Exception Stack Frame ===\r\n");
  printf("R0  = 0x%08X\r\n", stack_frame->r0);   // 通常包含函数参数或返回值
  printf("R1  = 0x%08X\r\n", stack_frame->r1);   // 通常包含函数参数
  printf("R2  = 0x%08X\r\n", stack_frame->r2);   // 通常包含函数参数
  printf("R3  = 0x%08X\r\n", stack_frame->r3);   // 通常包含函数参数
  printf("R12 = 0x%08X\r\n", stack_frame->r12);  // IP (中间变量)
  printf("LR  = 0x%08X\r\n", stack_frame->lr);   // 返回地址
  printf("PC  = 0x%08X\r\n", stack_frame->pc);   // 异常发生的指令地址
  printf("PSR = 0x%08X\r\n", stack_frame->psr);  // 程序状态寄存器

  // 打印其他重要系统寄存器
  printf("\r\n=== System Control Registers ===\r\n");
  printf("CONTROL = 0x%08X\r\n", __get_CONTROL());  // 控制寄存器
  printf("BASEPRI = 0x%08X\r\n", __get_BASEPRI());  // 基础优先级寄存器
  printf("PRIMASK = 0x%08X\r\n", __get_PRIMASK());  // 优先级屏蔽寄存器

  // 获取各种fault状态寄存器
  volatile uint32_t CFSR = SCB->CFSR;    // Configurable Fault Status Register
  volatile uint32_t HFSR = SCB->HFSR;    // Hard Fault Status Register
  volatile uint32_t DFSR = SCB->DFSR;    // Debug Fault Status Register
  volatile uint32_t AFSR = SCB->AFSR;    // Auxiliary Fault Status Register
  volatile uint32_t MMFAR = SCB->MMFAR;   // MemManage Fault Address Register
  volatile uint32_t BFAR = SCB->BFAR;    // Bus Fault Address Register

  printf("\r\n=== Fault Status Info ===\r\n");

  // 打印主要fault寄存器值
  printf("CFSR  = 0x%08X\r\n", CFSR);
  printf("HFSR  = 0x%08X\r\n", HFSR);
  printf("DFSR  = 0x%08X\r\n", DFSR);
  printf("AFSR  = 0x%08X\r\n", AFSR);
  printf("MMFAR = 0x%08X\r\n", MMFAR);
  printf("BFAR  = 0x%08X\r\n", BFAR);

  // 解析CFSR (包含MMFSR, BFSR 和 UFSR)
  printf("\r\n=== CFSR Detailed Analysis ===\r\n");

  // MemManage Fault Status Register (MMFSR) - CFSR[7:0]
  if((CFSR & 0xFF) != 0) {
    printf("\r\nMemManage Fault:\r\n");
    if(CFSR & (1 << 7)) printf("- MMARVALID: MMFAR holds valid address\r\n");
    if(CFSR & (1 << 4)) printf("- MSTKERR: Stacking error\r\n");
    if(CFSR & (1 << 3)) printf("- MUNSTKERR: Unstacking error\r\n");
    if(CFSR & (1 << 1)) printf("- DACCVIOL: Data access violation\r\n");
    if(CFSR & (1 << 0)) printf("- IACCVIOL: Instruction access violation\r\n");
  }

  // Bus Fault Status Register (BFSR) - CFSR[15:8]
  if((CFSR & 0xFF00) != 0) {
    printf("\r\nBus Fault:\r\n");
    if(CFSR & (1 << 15)) printf("- BFARVALID: BFAR holds valid address\r\n");
    if(CFSR & (1 << 12)) printf("- STKERR: Stacking error\r\n");
    if(CFSR & (1 << 11)) printf("- UNSTKERR: Unstacking error\r\n");
    if(CFSR & (1 << 10)) printf("- IMPRECISERR: Imprecise data access error\r\n");
    if(CFSR & (1 << 9))  printf("- PRECISERR: Precise data access error\r\n");
    if(CFSR & (1 << 8))  printf("- IBUSERR: Instruction bus error\r\n");
  }

  // Usage Fault Status Register (UFSR) - CFSR[31:16]
  if((CFSR & 0xFFFF0000) != 0) {
    printf("\r\nUsage Fault:\r\n");
    if(CFSR & (1 << 24)) printf("- DIVBYZERO: Divide by zero\r\n");
    if(CFSR & (1 << 23)) printf("- UNALIGNED: Unaligned access\r\n");
    if(CFSR & (1 << 19)) printf("- NOCP: No coprocessor\r\n");
    if(CFSR & (1 << 18)) printf("- INVPC: Invalid PC load\r\n");
    if(CFSR & (1 << 17)) printf("- INVSTATE: Invalid state\r\n");
    if(CFSR & (1 << 16)) printf("- UNDEFINSTR: Undefined instruction\r\n");
  }

  // 解析HFSR
  printf("\r\n=== HFSR Analysis ===\r\n");
  if(HFSR & (1 << 31)) printf("- DEBUGEVT: Debug event\r\n");
  if(HFSR & (1 << 30)) printf("- FORCED: Forced hard fault\r\n");
  if(HFSR & (1 << 1))  printf("- VECTTBL: Vector table read fault\r\n");
  printf("\r\n=== End of Fault Analysis ===\r\n");
}

/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
  HAL_UART_Transmit(&huart1, (uint8_t*)"NMI\r\n", 5, 10);
  //LED_Blink(1000);
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  print_fault_info();
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  HAL_UART_Transmit(&huart1, (uint8_t*)"HardFault\r\n", 11, 10);
  //LED_Blink(1000);
  /* USER CODE END HardFault_IRQn 0 */
  print_fault_info();
  while (1)
  {
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  HAL_UART_Transmit(&huart1, (uint8_t*)"MemManage\r\n", 11, 10);
  //LED_Blink(1000);
  /* USER CODE END MemoryManagement_IRQn 0 */
  print_fault_info();
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
  HAL_UART_Transmit(&huart1, (uint8_t*)"BusFault\r\n", 10, 10);
  //LED_Blink(1000);
  /* USER CODE END BusFault_IRQn 0 */
  print_fault_info();
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  HAL_UART_Transmit(&huart1, (uint8_t*)"UsageFault\r\n", 12, 10);
  //LED_Blink(1000);
  /* USER CODE END UsageFault_IRQn 0 */
  print_fault_info();
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */
  HAL_UART_Transmit(&huart1, (uint8_t*)"DebugMon\r\n", 10, 10);
  //LED_Blink(1000);
  /* USER CODE END DebugMonitor_IRQn 0 */
  print_fault_info();
  while (1)
  {
    /* USER CODE BEGIN W1_DebugMonitor_IRQn 0 */
    /* USER CODE END W1_DebugMonitor_IRQn 0 */
  }

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32F7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM6 global interrupt, DAC1 and DAC2 underrun error interrupts.
  */
void TIM6_DAC_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_DAC_IRQn 0 */
  //HAL_UART_Transmit(&huart1, (uint8_t*)"TIM6_DAC\r\n", 10, 10);
  //LED_Blink(1000);
  /* USER CODE END TIM6_DAC_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_DAC_IRQn 1 */

  /* USER CODE END TIM6_DAC_IRQn 1 */
}


/**
  * @brief This function handles USB On The Go FS global interrupt.
  */
// void OTG_FS_IRQHandler(void)
// {
//   /* USER CODE BEGIN OTG_FS_IRQn 0 */

//   /* USER CODE END OTG_FS_IRQn 0 */
//   HAL_HCD_IRQHandler(&hhcd_USB_OTG_FS);
//   /* USER CODE BEGIN OTG_FS_IRQn 1 */

//   /* USER CODE END OTG_FS_IRQn 1 */
// }

/**
  * @brief This function handles EXTI line[9:5] interrupts.
  */
void EXTI9_5_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI9_5_IRQn 0 */
  #if FINGERPRINT_ENABLE
  // 检查是否是指纹模块的中断引脚
  if(__HAL_GPIO_EXTI_GET_IT(FP_IRQ_Pin) != RESET)
  {
    // 调用指纹模块中断回调函数
    FP_IRQ_Callback();
    // 清除中断标志
    __HAL_GPIO_EXTI_CLEAR_IT(FP_IRQ_Pin);
  }
  #endif
  /* USER CODE END EXTI9_5_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
