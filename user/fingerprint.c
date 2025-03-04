/**
  ******************************************************************************
  * @file    fingerprint.c
  * @author  cyytx
  * @brief   指纹模块的源文件,实现指纹的初始化、扫描、匹配等功能
  ******************************************************************************
  */

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "fingerprint.h"
#include "priorities.h"
#include "cmsis_os.h"
#include "sg90.h"

#if FINGERPRINT_ENABLE

/* 私有定义 */
#define FP_TASK_STACK_SIZE        256     // 任务栈大小
#define FP_QUEUE_SIZE             10       // 队列大小
#define FP_RESPONSE_TIMEOUT       2000    // 指纹模块响应超时时间(ms)
#define FP_MAX_TEMPLATE_NUM       10     // 最大模板数量
#define FP_POWER_ON_DELAY         100     // 指纹模块上电稳定延时(ms)


/* 私有变量 */
static UART_HandleTypeDef huart4;             // 指纹模块串口句柄
static uint8_t FP_RxTempBuffer[1];            // 用于中断接收的临时缓冲区
static uint8_t FP_RxBuffer[FP_MAX_BUFFER_SIZE]; // 接收缓冲区
static uint16_t FP_RxIndex = 0;               // 接收索引
static FP_State_t FP_CurrentState = FP_STATE_IDLE; // 当前状态
static uint16_t FP_TemplateCount = 0;         // 有效模板数量
static uint16_t FP_CurrentTemplateID = 0;     // 当前使用的模板ID
static osMessageQueueId_t fpQueueHandle;      // 指纹消息队列句柄
static osThreadId_t fpTaskHandle;             // 指纹任务句柄


// FreeRTOS任务属性设置
static const osThreadAttr_t fp_attributes = {
    .name = "FingerprintTask",
    .stack_size = FP_TASK_STACK_SIZE * 4,
    .priority = (osPriority_t)TASK_PRIORITY_FINGERPRINT,
};

// 指纹上电控制
static void FP_Power_On(void)
{
    HAL_GPIO_WritePin(FP_CTRL_GPIO_Port, FP_CTRL_Pin, GPIO_PIN_SET);
    osDelay(FP_POWER_ON_DELAY); // 等待指纹模块上电稳定
}

static void FP_Power_Off(void)
{
    HAL_GPIO_WritePin(FP_CTRL_GPIO_Port, FP_CTRL_Pin, GPIO_PIN_RESET);
}

// 指纹相关GPIO初始化
/*
FP_CTRL_Pin --- GPIOE6 指纹模块活体供电控制，控制指纹模组 VCC，高电平给电，低电平断电
FP_IRQ_Pin --- GPIOE5 指纹模块中断引脚，连接ZW101的TOUCH_OUT 唤醒 IRQ (true:1, flase:0)
*/
static void FP_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* 配置FP_CTRL_Pin为输出模式 */
    HAL_GPIO_WritePin(FP_CTRL_GPIO_Port, FP_CTRL_Pin, GPIO_PIN_RESET);
    
    GPIO_InitStruct.Pin = FP_CTRL_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(FP_CTRL_GPIO_Port, &GPIO_InitStruct);

    /* 配置FP_IRQ_Pin为外部中断模式，这是TOUCH_OUT引脚 */
    GPIO_InitStruct.Pin = FP_IRQ_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;  // 上升沿触发中断
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(FP_IRQ_GPIO_Port, &GPIO_InitStruct);
    
    /* 启用EXTI中断 */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, FINGERPRINT_IRQ_PRIORITY_EXTI, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

// UART初始化
static void FP_UART_Init(void)
{
    huart4.Instance = UART4;
    huart4.Init.BaudRate = 57600;
    huart4.Init.WordLength = UART_WORDLENGTH_8B;
    huart4.Init.StopBits = UART_STOPBITS_1;
    huart4.Init.Parity = UART_PARITY_NONE;
    huart4.Init.Mode = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    
    if (HAL_UART_Init(&huart4) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_NVIC_SetPriority(UART4_IRQn, FINGERPRINT_IRQ_PRIORITY_USART4, 0);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
    
    // 启动UART接收中断
    HAL_UART_Receive_IT(&huart4, FP_RxTempBuffer, 1);
}

// 重置接收缓冲区
static void FP_ResetRxBuffer(void)
{
    memset(FP_RxBuffer, 0, sizeof(FP_RxBuffer));
    FP_RxIndex = 0;
}

/**
 * @brief 初始化指纹模块
 */
void FP_Init(void)
{
    // GPIO初始化
    FP_GPIO_Init();
    
    // UART初始化
    FP_UART_Init();
    
    // 初始状态为空闲
    FP_CurrentState = FP_STATE_IDLE;
    
    // 重置缓冲区
    FP_ResetRxBuffer();
}


// 发送命令给指纹模块
static int FP_SendCommand(uint8_t cmd, uint8_t *data, uint16_t dataLen)
{
    uint8_t buffer[FP_MAX_BUFFER_SIZE] = {0};
    uint16_t totalLen = 0;
    uint16_t chkSum = 0;
    
    // 准备包头 (2字节 0xEF01)
    buffer[0] = FP_PACK_HEAD_0;  // 包头低字节 0xEF
    buffer[1] = FP_PACK_HEAD_1;  // 包头高字节 0x01
    
    // 准备设备地址 (4字节 0xFFFFFFFF)
    buffer[2] = 0xFF;
    buffer[3] = 0xFF;
    buffer[4] = 0xFF;
    buffer[5] = 0xFF;
    
    // 准备包标识 (1字节 0x01 命令包)
    buffer[6] = FP_PACK_CMD;
    
    // 准备包长度 (2字节，包含指令码和数据，不包含校验和)
    //uint16_t packLen = dataLen + 1;  // 数据长度 + 指令码长度(1字节)
    buffer[7] = (uint8_t)(dataLen >> 8);     // 高字节
    buffer[8] = (uint8_t)(dataLen & 0xFF);   // 低字节 #WTD 需要验证
    
    // 准备指令码 (1字节)
    buffer[9] = cmd;
    
    // 复制数据 (可变长度)
    if (data != NULL && dataLen > 0)
    {
        memcpy(&buffer[10], data, dataLen);
    }
    
    // 计算校验和 (从包标识到包尾的和)
    for (uint16_t i = 6; i < 10 + dataLen; i++)
    {
        chkSum += buffer[i];
    }
    
    // 添加校验和 (2字节)
    buffer[10 + dataLen] = (uint8_t)(chkSum >> 8);     // 高字节
    buffer[11 + dataLen] = (uint8_t)(chkSum & 0xFF);   // 低字节
    
    // 总长度 = 包头(2字节) + 设备地址(4字节) + 包标识(1字节) + 包长度(2字节) + 指令码(1字节) + 数据(dataLen) + 校验和(2字节)
    totalLen = 12 + dataLen;
    
    // 清空接收缓冲区
    FP_ResetRxBuffer();
    
    // 发送数据
    if(HAL_UART_Transmit(&huart4, buffer, totalLen, 500) != HAL_OK)
    {
        return -1;
    }
    
    return 0;
}

// 处理指纹模块响应
//应答包格式 ：包头(2) + 设备地址(4) + 包标识(1) + 包长度(2) + 确认码(1) + 返回参数(N) + 校验和(2)
static int FP_ProcessResponse(void)
{
    uint32_t startTime = osKernelGetTickCount();
    uint16_t chkSum = 0;
    uint16_t recvChkSum = 0;
    
    // 等待接收至少12字节基本数据或超时
    // 包头(2) + 设备地址(4) + 包标识(1) + 包长度(2) + 确认码(1) + 校验和(2) = 12字节
    while (FP_RxIndex < 12)
    {
        if (osKernelGetTickCount() - startTime > FP_RESPONSE_TIMEOUT)
        {
            return -1; // 超时
        }
        osDelay(10);
    }
    
    // 验证包头 (0xEF01)
    if (FP_RxBuffer[0] != FP_PACK_HEAD_0 || FP_RxBuffer[1] != FP_PACK_HEAD_1)
    {
        return -2; // 包头错误
    }
    
    // 获取包标识 (命令包/数据包/结束包)
    uint8_t packFlag = FP_RxBuffer[6];
    if (packFlag != FP_RESPONSE_FLAG)//回应包的包表示固定为0X07
    {
        return -3; // 包标识错误
    }
    
    // 获取包长度
    uint16_t packLen = (FP_RxBuffer[7] << 8) | FP_RxBuffer[8];
    
    // 包长度 = 包长度至校验和（指令、参数或数据）的总字节数，包含校验和，但不包含包长度本身的字节数
    // 完整长度 = 包头(2) + 设备地址(4) + 包标识(1) + 包长度(2) + 数据(packLen 包括校验和) 
    uint16_t expectedLength = 9 + packLen;
    
    startTime = osKernelGetTickCount();
    while (FP_RxIndex < expectedLength)// WTD 需要考虑
    {
        if (osKernelGetTickCount() - startTime > FP_RESPONSE_TIMEOUT)
        {
            return -4; // 数据接收超时
        }
        osDelay(10);
    }
    
    // 计算校验和,校验和是从包标识至校验和之间所有字节之和，包含包标识不包含校验和
    for (uint16_t i = 6; i < 9 + packLen-2; i++)//减去2因为packlen也包含了校验和的字节数
    {
        chkSum += FP_RxBuffer[i];
    }
    
    // 获取接收到的校验和
    recvChkSum = (FP_RxBuffer[9 + packLen-2] << 8) | FP_RxBuffer[10 + packLen-2];
    
    // 验证校验和
    if (chkSum != recvChkSum)
    {
        return -5; // 校验和错误
    }
    
    // 返回确认码 (位于包标识后3字节位置，即索引9)
    return FP_RxBuffer[9];
}

// 内部获取有效模板数量
static int FP_GetValidTemplateNum_Internal(void)
{
    int ret;
    
    // 发送获取有效模板个数命令
    ret = FP_SendCommand(FP_CMD_GET_VALID_TEMPLATE_NUM, NULL, 0);
    if (ret != 0)
    {
        return -1;
    }
    
    // 处理响应
    ret = FP_ProcessResponse();
    if (ret != FP_ACK_SUCCESS)
    {
        return -2;
    }
    
    // 解析模板数量，位于确认码后的数据区
    // 包头(2) + 设备地址(4) + 包标识(1) + 包长度(2) + 确认码(1) = 10字节
    // 模板数量占2字节，高字节在前
    FP_TemplateCount = (FP_RxBuffer[10] << 8) | FP_RxBuffer[11];
    
    return FP_TemplateCount;
}

// 注册指纹模板
static int FP_EnrollTemplate(uint16_t templateID)
{
    int ret;
    uint8_t data[2];
    
    // 保存模板ID
    FP_CurrentTemplateID = templateID;
    
    // 准备数据: 模板ID (高字节在前，低字节在后)
    data[0] = (uint8_t)(templateID >> 8);
    data[1] = (uint8_t)(templateID & 0xFF);
    
    // 发送自动注册模板命令,指令 的包长度在指令本身定义
    ret = FP_SendCommand(FP_CMD_AUTO_ENROLL_TEMPLATE, data, 8);
    if (ret != 0)
    {
        return -1;
    }
    
    // 设置状态为注册中
    FP_CurrentState = FP_STATE_ENROLLING;
    
    return 0;
}

// 识别指纹
static int FP_IdentifyFingerprint(void)
{
    int ret;
    
    // 发送自动验证指纹命令
    ret = FP_SendCommand(FP_CMD_AUTO_IDENTIFY, NULL, 0);
    if (ret != 0)
    {
        return -1;
    }
    
    // 设置状态为识别中
    FP_CurrentState = FP_STATE_IDENTIFYING;
    
    return 0;
}

// 处理手指检测
static void FP_ProcessFingerDetection(void)
{
    // 检查指纹模块是否已初始化
    if (fpQueueHandle == NULL)
    {
        return;
    }
    
    // 创建消息
    FP_Msg_t msg;
    msg.type = FP_MSG_FINGER_PRESSED;
    msg.param = 0;
    
    // 发送消息到队列
    osMessageQueuePut(fpQueueHandle, &msg, 0, 0);
}


/**
 * @brief 获取指纹有效模板数量
 * @return 有效模板数量
 */
uint16_t FP_GetValidTemplateNum(void)
{
    if (fpQueueHandle != NULL)
    {
        // 创建消息
        FP_Msg_t msg;
        msg.type = FP_MSG_GET_TEMPLATE_NUM;
        msg.param = 0;
        
        // 发送消息到队列
        osMessageQueuePut(fpQueueHandle, &msg, 0, 0);
    }
    
    return FP_TemplateCount;
}

/**
 * @brief 开始指纹注册流程
 * @param template_id 模板ID
 * @return 0-成功, 其他-失败
 */
int FP_EnrollStart(uint16_t template_id)
{
    if (FP_CurrentState != FP_STATE_IDLE)
    {
        return -1; // 模块忙
    }
    
    if (fpQueueHandle != NULL)
    {
        // 创建消息
        FP_Msg_t msg;
        msg.type = FP_MSG_ENROLL;
        msg.param = template_id;
        
        // 发送消息到队列
        osMessageQueuePut(fpQueueHandle, &msg, 0, 0);
        return 0;
    }
    
    return -2;
}

/**
 * @brief 开始指纹识别流程
 * @return 0-成功, 其他-失败
 */
int FP_IdentifyStart(void)
{
    if (FP_CurrentState != FP_STATE_IDLE)
    {
        return -1; // 模块忙
    }
    
    if (fpQueueHandle != NULL)
    {
        // 创建消息
        FP_Msg_t msg;
        msg.type = FP_MSG_IDENTIFY;
        msg.param = 0;
        
        // 发送消息到队列
        osMessageQueuePut(fpQueueHandle, &msg, 0, 0);
        return 0;
    }
    
    return -2;
}

/**
 * @brief 检查手指是否按下
 * @return 1-按下, 0-未按下
 */
uint8_t FP_IsFingerPressed(void)
{
    return (HAL_GPIO_ReadPin(FP_IRQ_GPIO_Port, FP_IRQ_Pin) == GPIO_PIN_SET) ? 1 : 0;
}

/**
 * @brief 获取当前指纹模块状态
 * @return 当前状态
 */
FP_State_t FP_GetState(void)
{
    return FP_CurrentState;
}

/**
 * @brief 设置指纹模块状态
 * @param state 状态
 */
void FP_SetState(FP_State_t state)
{
    FP_CurrentState = state;
}


// 指纹模块任务
static void FP_Task(void *argument)
{
    FP_Msg_t msg;
    int ret;
    uint16_t matchID;
    
    // 任务初始化
    FP_Power_On();
    osDelay(500); // 等待指纹模块完全启动
    
    // 获取初始模板数量
    FP_GetValidTemplateNum_Internal();
    
    // 任务主循环
    while(1)
    {
        // 等待消息
        if (osMessageQueueGet(fpQueueHandle, &msg, NULL, osWaitForever) == osOK)
        {
            switch (msg.type)
            {
                case FP_MSG_GET_TEMPLATE_NUM:
                    FP_GetValidTemplateNum_Internal();
                    break;
                
                case FP_MSG_ENROLL:
                    FP_EnrollTemplate(msg.param);
                    break;
                
                case FP_MSG_IDENTIFY:
                    FP_IdentifyFingerprint();
                    break;
                
                case FP_MSG_FINGER_PRESSED:
                    // 如果模块上电并且当前状态为空闲
                    if (FP_CurrentState == FP_STATE_IDLE)
                    {
                        // 根据模板数量决定执行注册还是识别
                        if (FP_TemplateCount == 0)
                        {
                            // 没有模板，执行注册
                            printf("没有指纹模板，开始注册指纹\r\n");
                            FP_EnrollTemplate(0); // 使用ID 0注册
                        }
                        else
                        {
                            // 有模板，执行识别
                            printf("开始指纹识别...\r\n");
                            FP_IdentifyFingerprint();
                        }
                    }
                    break;
                
                default:
                    break;
            }
        }
        
        // 处理当前状态下的响应
        if (FP_CurrentState == FP_STATE_ENROLLING || FP_CurrentState == FP_STATE_IDENTIFYING)
        {
            ret = FP_ProcessResponse();
            
            if (FP_CurrentState == FP_STATE_ENROLLING)
            {
                // 处理注册状态下的响应
                switch (ret)
                {
                    case FP_ACK_SUCCESS:
                        printf("指纹注册成功\r\n");
                        FP_TemplateCount++; // 增加模板计数
                        FP_CurrentState = FP_STATE_SUCCESS;
                        osDelay(1000);
                        FP_CurrentState = FP_STATE_IDLE;
                        break;
                    
                    case FP_ACK_ENROLL_CONTINUE:
                        printf("请再次按下手指完成注册\r\n");
                        // 继续等待响应
                        break;
                    
                    case FP_ACK_NO_FINGER:
                        printf("没有检测到手指，请按下手指\r\n");
                        // 继续等待响应
                        break;
                    
                    case FP_ACK_BAD_FINGER:
                        printf("指纹质量差，请重新按下\r\n");
                        // 继续等待响应
                        break;
                    
                    case FP_ACK_DB_FULL:
                        printf("指纹数据库已满\r\n");
                        FP_CurrentState = FP_STATE_FAIL;
                        osDelay(1000);
                        FP_CurrentState = FP_STATE_IDLE;
                        break;
                    
                    case FP_ACK_GEN_FAIL:
                        printf("特征提取失败\r\n");
                        FP_CurrentState = FP_STATE_FAIL;
                        osDelay(1000);
                        FP_CurrentState = FP_STATE_IDLE;
                        break;
                    
                    case FP_ACK_FAIL:
                        printf("指纹注册失败\r\n");
                        FP_CurrentState = FP_STATE_FAIL;
                        osDelay(1000);
                        FP_CurrentState = FP_STATE_IDLE;
                        break;
                    
                    default:
                        if (ret < 0) // 通信错误
                        {
                            printf("指纹通信错误: %d\r\n", ret);
                            FP_CurrentState = FP_STATE_FAIL;
                            osDelay(1000);
                            FP_CurrentState = FP_STATE_IDLE;
                        }
                        break;
                }
            }
            else if (FP_CurrentState == FP_STATE_IDENTIFYING)
            {
                // 处理识别状态下的响应
                switch (ret)
                {
                    case FP_ACK_SUCCESS:
                        // 识别成功，解析匹配的模板ID
                        // 根据ZW101协议，模板ID位于确认码后的数据区(索引10和11)
                        matchID = (FP_RxBuffer[10] << 8) | FP_RxBuffer[11];
                        printf("指纹识别成功，匹配ID: %d\r\n", matchID);
                        FP_CurrentState = FP_STATE_SUCCESS;
                        
                        // 发送开锁命令
                        SendLockCommand(LOCK_CMD_OPEN);
                        
                        osDelay(1000);
                        FP_CurrentState = FP_STATE_IDLE;
                        break;
                    
                    case FP_ACK_IDENTIFY_CONTINUE:
                        printf("正在识别...\r\n");
                        // 继续等待响应
                        break;
                    
                    case FP_ACK_NO_FINGER:
                        printf("没有检测到手指，请按下手指\r\n");
                        // 继续等待响应
                        break;
                    
                    case FP_ACK_BAD_FINGER:
                        printf("指纹质量差，请重新按下\r\n");
                        // 继续等待响应
                        break;
                    
                    case FP_ACK_FAIL:
                        printf("指纹识别失败，没有匹配\r\n");
                        FP_CurrentState = FP_STATE_FAIL;
                        osDelay(1000);
                        FP_CurrentState = FP_STATE_IDLE;
                        break;
                    
                    default:
                        if (ret < 0) // 通信错误
                        {
                            printf("指纹通信错误: %d\r\n", ret);
                            FP_CurrentState = FP_STATE_FAIL;
                            osDelay(1000);
                            FP_CurrentState = FP_STATE_IDLE;
                        }
                        break;
                }
            }
        }
        
        // 如果当前没有特定任务，可以短暂延时
        if (FP_CurrentState == FP_STATE_IDLE)
        {
            osDelay(50);
        }
    }
}

/**
 * @brief 创建指纹模块任务和队列
 */
void FP_CreateTask(void)
{
    // 创建消息队列
    fpQueueHandle = osMessageQueueNew(FP_QUEUE_SIZE, sizeof(FP_Msg_t), NULL);
    
    // 创建任务
    fpTaskHandle = osThreadNew(FP_Task, NULL, &fp_attributes);
}

/**
 * @brief 外部中断回调函数，用于FP_IRQ_Pin中断
 * 
 * 注意：此函数需要在外部中断处理函数中调用，例如stm32f7xx_it.c中的 EXTI9_5_IRQHandler 函数中调用
 * ，所有GPIO 的5-9pin 共享一个EXIT中断，所以在EXTI9_5_IRQHandler中判断是不是FP_IRQ_Pin 的bit
 * 同时要在FP_IRQ_Callback中确认对应的PORT 和PIN是否改变。
 */
void FP_IRQ_Callback(void)
{
    // 检查是否是指纹模块的中断引脚
    if (HAL_GPIO_ReadPin(FP_IRQ_GPIO_Port, FP_IRQ_Pin) == GPIO_PIN_SET)
    {
        // 在中断中尽量减少处理时间，直接置位标志或通知任务
        if (FP_CurrentState == FP_STATE_IDLE)
        {
            // 标记手指按下，稍后在任务中处理
            // 这里仅通知任务，实际处理在任务中进行
            FP_ProcessFingerDetection();
        }
    }
}

/**
 * @brief 串口接收回调，用于接收指纹模块数据，在uart.c中调用
 */
void Fingerprint_RxCpltCallback(void)
{
  // 储存接收到的数据
    if (FP_RxIndex < FP_MAX_BUFFER_SIZE)
    {
        FP_RxBuffer[FP_RxIndex++] = FP_RxTempBuffer[0];
    }
    
    // 继续接收下一个字节
    HAL_UART_Receive_IT(&huart4, FP_RxTempBuffer, 1);

}


/**
  * @brief This function handles UART4 global interrupt.
  */
void UART4_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart4);
}
#endif /* FINGERPRINT_ENABLE */


