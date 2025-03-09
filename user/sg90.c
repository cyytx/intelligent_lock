
/**
  ******************************************************************************
  * @file    servo.c
  * @author  cyytx
  * @brief   舵机控制模块的源文件,实现舵机的初始化、控制等功能
  ******************************************************************************
  */
#include "stdio.h"
#include "stm32f7xx_hal.h"
#include "cmsis_os.h"
#include "timers.h"
#include "sg90.h"
#include "key.h" // 添加对keyboard.h的引用以访问LockPassword_t类型
#include "priorities.h"

#ifdef SG90_ENABLE

#define SG90_UNLOCK_TIMEOUT pdMS_TO_TICKS(3000)

/* 私有变量 */
static TIM_HandleTypeDef sg90_timer;  // 使用定时器2
static uint32_t sg90_channel = TIM_CHANNEL_1;   // 使用通道1
static uint8_t lock_state = 0;      // 门锁状态
// 定义队列句柄
static osMessageQueueId_t sg90QueueHandle;
static TimerHandle_t SG90_Timer;

osThreadId_t sg90TaskHandle;
static const osThreadAttr_t sg90_attributes = {
    .name = "sg90Task",
    .stack_size = STACK_SIZE_SG90,
    .priority = (osPriority_t) TASK_PRIORITY_SG90,
};


void SG90_GPIO_Init(void)
{
     // 舵机初始化代码
     //时钟
    __HAL_RCC_GPIOG_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    HAL_GPIO_WritePin(SG90_CTL_GPIO_Port, SG90_CTL_Pin, GPIO_PIN_RESET);

      /*Configure GPIO pin : SG90_CTL_Pin */
    GPIO_InitStruct.Pin = SG90_CTL_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SG90_CTL_GPIO_Port, &GPIO_InitStruct);
    // 定时器PWM配置
    // 启动PWM输出
    //HAL_TIM_PWM_Start(&sg90_timer, sg90_channel);
}

 

/*使用定时器中断来作为舵机控制，不过由于设计失误，PG6不能复用为tim channel pwm 输出，所以设计使用在中断中翻转gpio
输出pwm，sg90 舵机控制为50HZ也就是20ms 高电平占用时间和角度关系如下。
0.5MS-0度；
1.0MS-45度；
1.5MS-90度;
2.0MS-135度；
2.5MS-180度;
*/
#define TIM2_FREQ_MHZ 96
#define TIM2_PERIOD 100
static TIM_HandleTypeDef htim2;

void TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE(); // 使能 TIM2 时钟

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = TIM2_FREQ_MHZ - 1;          // 分频后为 1 MHz (1 µs 计数周期)
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = TIM2_PERIOD - 1;              // 每 100 µs 触发一次中断
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim2);

    HAL_NVIC_SetPriority(TIM2_IRQn, SG90_IRQ_PRIORITY_TIM2, 0); // 设置中断优先级
    HAL_NVIC_EnableIRQ(TIM2_IRQn);         // 启用 TIM2 中断
}

void SG90_Init(void)
{
    SG90_GPIO_Init();
    TIM2_Init();
} 

volatile uint32_t high_time = 1500; // 初始高电平时间为 1.5ms (对应 90°)
volatile uint32_t low_time = 18500; // 初始低电平时间为 18.5ms
volatile uint32_t counter = 0;      // 计数器
volatile uint8_t pwm_state = 0;     // 当前 PWM 状态 (0: 低电平, 1: 高电平)
volatile uint16_t period_count = 0; //周期计数器，一个周期20ms，最大转动为180度，20个周期即400ms够了
void TIM2_IRQHandler(void)
{
    //HAL_TIM_IRQHandler(&htim2); // 清除中断标志
    
    __HAL_TIM_CLEAR_IT(&htim2, TIM_IT_UPDATE);

    counter += 100; // 每次中断增加 100 µs

    if (pwm_state == 1) // 当前为高电平
    {
        if (counter >= high_time) // 达到高电平时间
        {
            period_count += 1;
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET); // 设置低电平
            pwm_state = 0; // 切换到低电平状态
            counter = 0;   // 重置计数器
        }
    }
    else // 当前为低电平
    {
        if (counter >= low_time) // 达到低电平时间
        {
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET); // 设置高电平
            pwm_state = 1; // 切换到高电平状态
            counter = 0;   // 重置计数器
        }
    }
    
    if (period_count >= 40) // 20个周期即400ms够了
    {
        HAL_TIM_Base_Stop_IT(&htim2); // 停止 TIM2 中断
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);//拉低电平
    }
}

// 设置舵机角度，目前设计定时器中断为100us，90度1ms,9度100us,所以角度只能是9的倍数
void Set_Servo_Angle(uint16_t angle)
{
    printf("Set_Servo_Angle: %d\r\n", angle);
    if (angle > 180) angle = 180;

    // 计算高电平时间 (单位: µs)
    high_time = 500 + (angle * 2000 / 180); // 0.5ms ~ 2.5ms
    low_time = 20000 - high_time;           // 低电平时间
    counter = 0;
    pwm_state = 0;
    period_count = 0;
    __HAL_TIM_SET_COUNTER(&htim2, 0); // 重置计数器
    HAL_TIM_Base_Start_IT(&htim2); // 启动 TIM2 并启用中断
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);//拉高电平
}


void PG6_SET_HIGH(void)
{
    printf("PG6_SET_HIGH\r\n");
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);//拉高电平
}

void PG6_SET_LOW(void)
{
    printf("PG6_SET_LOW\r\n");
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);//拉低电平
}   


void SG90_TimerCallback(TimerHandle_t xTimer)
{
    uint8_t command = LOCK_CMD_CLOSE;
    osMessageQueuePut(sg90QueueHandle, &command, 0, 0);   
    printf("SG90_TimerCallback\r\n");
}


// 舵机任务函数,用于开关锁
void Sg90ControlTask(void *pvParameters)
{
    uint8_t command;
    uint32_t current_time;
    //创建一个定时器，并启动
    SG90_Timer = xTimerCreate("SG90_Timer", SG90_UNLOCK_TIMEOUT, pdFALSE, (void *)0, SG90_TimerCallback);
    xTimerStart(SG90_Timer, 0);
    xTimerStop(SG90_Timer, 0);
    
    while (1)
    {
        // 本身这里的队列应该是做无限等待的，但是因为要做开锁后自动关锁，所以1s后让它跑到后面是否需要关锁
        if (osMessageQueueGet(sg90QueueHandle, &command, NULL, portMAX_DELAY) == osOK)
        {
            if (command == LOCK_CMD_OPEN) // 开锁
            {
                printf("Unlocking door...\r\n");
                Set_Servo_Angle(180); // 设置舵机角度为180度开锁
                xTimerReset(SG90_Timer, 0);
                 lock_state = 1;
                // unlock_time = osKernelGetTickCount(); // 记录解锁时间
            }
            else if (command == LOCK_CMD_CLOSE) // 关锁
            {
                printf("Locking door...\r\n");
                Set_Servo_Angle(0); // 设置舵机角度为0度关锁
                lock_state = 0;
            }
        }
        
    }
}

// 创建舵机任务和队列
void SG90_CreateTask(void)
{
    // 创建消息队列
    sg90QueueHandle = osMessageQueueNew(10, sizeof(uint8_t), NULL);
    //创建定时器，用于开锁后自动关锁
    
    // 创建舵机控制任务
    sg90TaskHandle = osThreadNew(Sg90ControlTask, NULL, &sg90_attributes);
}

// // 添加门锁状态获取函数实现
uint8_t IsDoorUnlocked(void)
{
    return lock_state;
}


// 发送锁定/解锁命令到舵机任务 1-开锁 0-关锁
void SendLockCommand(uint8_t command)
{
    if (sg90QueueHandle != NULL)
    {
        osMessageQueuePut(sg90QueueHandle, &command, 0, 0);
    }
}

#endif /* SG90_ENABLE */ 