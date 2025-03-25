#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "fingerprint.h"
#include "priorities.h"
#include "sg90.h"
#include "semphr.h"
#include "timers.h"

#if FINGERPRINT_ENABLE

#define FP_QUEUE_SIZE 20 //队列长度
#define FP_MAX_BUFFER_SIZE        128     // 最大缓冲区大小
#define FP_RECEIVE_TIMEOUT pdMS_TO_TICKS(10)  // 定义接收超时时间为10ms
/* 定义全局变量 */
static UART_HandleTypeDef huart4;             // 指纹模块串口句柄
static TaskHandle_t FP_TaskHandle = NULL;          // 指纹任务句柄
static QueueHandle_t FP_MsgQueue = NULL;           // 指纹消息队列
static SemaphoreHandle_t FP_Semaphore = NULL; // 指纹按下信号量
static SemaphoreHandle_t FP_RxSemaphore = NULL; /* 数据接收信号量 */

// 接收缓冲区
static uint8_t FP_RxBuffer[FP_MAX_BUFFER_SIZE];    // 接收缓冲区
static uint8_t FP_RxTempBuffer[1];                 // 临时接收缓冲区
static volatile uint16_t FP_RxIndex = 0;            // 接收索引

static TimerHandle_t FP_Timer = NULL; // 指纹模块定时器

uint8_t FP_CMD_SEND_RECORD = 0;//发送指令记录,返回数据就是该指令的返回数据
uint16_t FP_TemplateNum = 0; //有效模板数量
uint8_t FP_Mode = FP_MODE_IDENTIFY; //指纹模式，注册还是识别,0:识别，1:注册


// 指纹上电控制
static void FP_Power_On(void)
{
    HAL_GPIO_WritePin(FP_CTRL_GPIO_Port, FP_CTRL_Pin, GPIO_PIN_SET);
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
    HAL_GPIO_WritePin(FP_CTRL_GPIO_Port, FP_CTRL_Pin, GPIO_PIN_RESET);// 不上电
    
    
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

    // 初始化接收缓冲区
    FP_ResetRxBuffer();
    
    // 开始接收数据
    FP_Power_On();
    // 延时等待模块启动
    //osDelay(500);
    //HAL_UART_Receive_IT(&huart4, FP_RxTempBuffer, 1);
}

void FP_SendCommand(uint8_t *buffer, uint16_t totalLen)
{
    HAL_UART_Transmit(&huart4, buffer, totalLen, 500);
}


/*
包长度 = 包长度至校验和（指令、参数或数据）的总字节数，包含校验和，但不包含包长
度本身的字节数。
校验和是从包标识至校验和之间所有字节之和，包含包标识不包含校验和，超出 2 字节的进
位忽略。
*/

// 指令长度合法判断和校验和计算
// 包头 (2 bytes) + 设备地址 (4 bytes) + 包标识 (1 byte) + 包长度 (2 bytes) + 指令 (1 byte)\
// + 参数 1 (1 byte) + 参数 2 (1 byte) + ... + 参数 N (1 byte) + 校验和 (2 bytes)
uint8_t FP_AtCmdCheck(uint8_t *data, uint16_t length)
{
    uint16_t i = 0;
    uint16_t checksum = 0;// 计算校验和
    uint16_t original_checksum = (data[length-2] << 8) | data[length-1]; //指令中原本的校验和
    uint16_t original_packet_length = (data[7] << 8) | data[8];// 指令中原本的包长度

    for(i=0;i<length;i++)
    {
        printf("%02X ",data[i]);
    }
    printf("\r\n");
    
    //包长度为指令码+参数+校验和长度，前面的包头、设备地址、包标识、包长度为固定长度9byte
    if(original_packet_length != length-9)
    {
        printf("packet length error,original_packet_length:%d,length:%d\r\n",original_packet_length,length);
        return -1;
    }
    //校验和从包标识到检验和之前，包标识是第七个字节也就是data[6]
    for (i = 6; i < length-2; i++)
    {
        checksum += data[i];
    }
    //将checksum和校验和比较,如果不相等则赋值给对应data字节
    if(checksum != original_checksum)
    {
        data[length-2] = checksum>>8;
        data[length-1] = checksum&0xFF;
        
    }
    return 0;
}





//处理指纹模块返回数据
// 包头 (2 bytes) + 设备地址 (4 bytes) + 包标识 (1 byte) + 包长度 (2 bytes) + 确认码 (1 byte)\
// + 返回参数 (N bytes) + 校验和 (2 bytes)
uint8_t FP_AtReturnDataCheck(uint8_t *data,uint16_t length)
{
    uint8_t i=0;
    //固定的码数组
    uint8_t fixed_code[7]={0XEF,0X01,0XFF,0XFF,0XFF,0XFF,0X07};

    for(uint16_t i=0;i<length;i++)
    {
        printf("%02X ",data[i]);
    }
    printf("\r\n");
    //检验固定码
    for(uint16_t i=0;i<7;i++)
    {
        if(data[i] != fixed_code[i])
        {
            printf("fixed code error\r\n");
            return -1;
        }
    }
    //校验长度
    uint16_t data_length = (data[7] << 8) | data[8];
    uint16_t one_packet_length= 9 + data_length; //固定码+包长度（2byte)=9byte
    if(length < one_packet_length)
    {
        printf("data length error\r\n");
        return -1;
    }
    //校验和
    uint16_t checksum = 0;
    for(uint16_t i=6;i<one_packet_length-2;i++)
    {
        checksum += data[i];
    }
    if(checksum != ((data[one_packet_length-2] << 8) | data[one_packet_length-1]))
    {
        printf("checksum error,checksum:%d,data[length-2]:%d,data[length-1]:%d\r\n",checksum,data[length-2],data[length-1]);
        return -1;
    }
    return 0;
}

//处理有效指纹模板数量返回数据
// 包头 (2 bytes) + 设备地址 (4 bytes) + 包标识 (1 byte) + 包长度 (2 bytes) + 确认码 (1 byte)\
// + 有效模板数量 (2 bytes) + 校验和 (2 bytes)
void FP_HandleValidTemplateNum(uint8_t *data,uint16_t length)
{
    if(FP_AtReturnDataCheck(data,length) != 0)
    {
        printf("get valid template num error\r\n");
        return;
    }
    //确认码 00成功 01接收包有错
    if(data[9] != 0X00)
    {
        printf("confirm code error\r\n");
        return;
    }
    //有效模板数量
    FP_TemplateNum = (data[10] << 8) | data[11];
    printf("valid template num:%d\r\n",FP_TemplateNum);
}   

/**
 * @brief 获取有效指纹模板数量
 * @return 模板数量
 */
uint16_t FP_GetValidTemplateNum(void)
{
    // 发送获取模板数量命令
    // 这里需要实现具体的命令发送
    uint8_t cmd[]={0XEF,0X01,0XFF,0XFF,0XFF,0XFF,0X01,0X00,0X03,0X1d,0X00,0X21};

    
    if (FP_AtCmdCheck(cmd, sizeof(cmd)) != 0)
    {
        printf("packck length error\r\n");
        return -1;
    }

    // 发送命令
    FP_SendCommand(cmd, sizeof(cmd));

    return 0;
}

/**
 * @brief 处理注册开始返回数据
 * @param data 返回数据
 * @param length 数据长度
 */
// 包头 (2 bytes) + 设备地址 (4 bytes) + 包标识 (1 byte) + 包长度 (2 bytes) + 确认码 (1 byte)\
// + 参数 1 (1 byte) + 参数 2 (1 byte) + 校验和 (2 bytes) + 备注 (2 bytes)
void FP_HandleEnrollACK(uint8_t *data,uint16_t length)
{
    // 校验返回数据
    if(FP_AtReturnDataCheck(data,length) != 0)
    {
        printf("enroll start error\r\n");
        return;
    }
    //确认码 00成功 其余错误参考FP_ConfirmCode_t
    if(data[9] != FP_ENROLL_CONFIRM_SUCCESS)
    {
        printf("enroll error code:%d\r\n",data[9]);
        return;
    }
    // 参数1，显示注册过程，参考FP_Param1_t，只需要根据进程打印
    switch(data[10])
    {
        case FP_PARAM1_FINGERPRINT_CHECK:
            printf("fingerprint check\r\n");
            break;
        case FP_PARAM1_GET_IMAGE:
            printf("get image\r\n");
            break;
        case FP_PARAM1_GENERATE_FEATURE:
            printf("generate feature\r\n");
            break;
        case FP_PARAM1_JUDGE_FINGER:
            printf("judge finger\r\n");
            break;
        case FP_PARAM1_MERGE_TEMPLATE:
            printf("merge template\r\n");
            break;
        case FP_PARAM1_REGISTER_CHECK:
            printf("register check\r\n");
            break;
        case FP_PARAM1_STORAGE_TEMPLATE:
            printf("storage template, enroll success\r\n");
            FP_Mode = FP_MODE_IDENTIFY;//注册结束
            break;
        default:
            printf("unknown param1\r\n");
            break;
    }
}
/**
 * @brief 开始注册指纹
 * @param template_id 模板ID
 * @param recode_num 录入次数
 * @param param 参数
 * @return 0: 成功; 其他: 错误码
 */
int FP_EnrollStart(uint16_t template_id,uint8_t recode_num,uint16_t param)
{
    //注册指纹 指令码0X31
    // 包头 (2 bytes) + 设备地址 (4 bytes) + 包标识 (1 byte) + 包长度 (2 bytes) + 指令码 (1 byte)\
    // + ID号 (2 bytes) + 录入次数 (1 byte) + 参数 (2 bytes) + 校验和 (2 bytes)   
    uint8_t cmd[]={0XEF,0X01,0XFF,0XFF,0XFF,0XFF,0X01,0X00,0X08,0X31,\
    0X00,0X01,0X02,0X00,0X00,0X00,0X3D};

    // 设置模板ID
    cmd[10] = (uint8_t)(template_id >> 8);
    cmd[11] = (uint8_t)(template_id & 0xFF);
    cmd[12] = recode_num;
    cmd[13] = (uint8_t)(param >> 8);
    cmd[14] = (uint8_t)(param & 0xFF);
    if(FP_AtCmdCheck(cmd, sizeof(cmd)) != 0)
    {
        printf("packck length error\r\n");
        return -1;
    }
    // 发送命令
    FP_SendCommand(cmd, sizeof(cmd));
    return 0;
}


//处理识别开始返回数据
// 包头 (2 bytes) + 设备地址 (4 bytes) + 包标识 (1 byte) + 包长度 (2 bytes) + 确认码 (1 byte)\
// + 参数 (1 byte) + ID号 (2 bytes) + 得分 (2 bytes) + 校验和 (2 bytes)
void FP_HandleIdentify(uint8_t *data,uint16_t length)
{
    // 校验返回数据
    if(FP_AtReturnDataCheck(data,length) != 0)
    {
        printf("identify error\r\n");
        return;
    }
    //确认码 00成功 其余错误参考 FP_IdentifyConfirmCode_t
    if(data[9] != FP_IDENTIFY_CONFIRM_SUCCESS)
    {
        printf("identify error code:%d\r\n",data[9]);
        return;
    }
    // 参数1，显示识别过程，参考FP_IdentifyParam_t，只需要根据进程打印
    switch(data[10])
    {
        case FP_PARAM_FINGERPRINT_CHECK:
            printf("fingerprint check\r\n");
            break;
        case FP_PARAM_GET_IMAGE:
            printf("get image\r\n");
            break;
        case FP_PARAM_REGISTERED_FINGER_COMPARE:
            printf("registered finger compare success\r\n");
            //对比成功，开锁
            SendLockCommand(LOCK_CMD_OPEN);
            break;
        default:
            printf("unknown param1\r\n");
            break;
    }

}
/**
 * @brief 开始识别指纹
 * @param score_level 分数等级
 * @param template_id 模板ID
 * @param param 参数
 * @return 0: 成功; 其他: 错误码
 */
int FP_IdentifyStart(uint8_t score_level,uint16_t template_id,uint16_t param)
{
    //识别指纹 指令码0X32
    //包头 (2 bytes) + 设备地址 (4 bytes) + 包标识 (1 byte) + 包长度 (2 bytes) + 指令码 (1 byte)\
    // + 分数等级 (1 byte) + ID号 (2 bytes) + 参数 (2 bytes) + 校验和 (2 bytes)
    uint8_t cmd[]={0XEF,0X01,0XFF,0XFF,0XFF,0XFF,0X01,0X00,0X08,0X32,\
    0X02,0X00,0X01,0X00,0X00,0X00,0X3E};

    // 设置分数等级
    cmd[10] = score_level;
    // 设置模板ID
    cmd[11] = (uint8_t)(template_id >> 8);
    cmd[12] = (uint8_t)(template_id & 0xFF);
    // 设置参数
    cmd[13] = (uint8_t)(param >> 8);
    cmd[14] = (uint8_t)(param & 0xFF);

    if(FP_AtCmdCheck(cmd, sizeof(cmd)) != 0)    
    {
        printf("packck length error\r\n");
        return -1;
    }
    // 发送命令 
    FP_SendCommand(cmd, sizeof(cmd));
    
    return 0;
}

void FP_TimerCallback(TimerHandle_t xTimer)
{
    FP_Msg_t msg;
    // 判断是否接收完成
    if (FP_RxIndex != 0)
    {
        //发送队列消息
        msg.type = FP_CMD_SEND_RECORD;
        msg.param = FP_RxIndex;
        xQueueSend(FP_MsgQueue, &msg, 0);
    }
}


/*
设计思路
1、创建一个定时器，并启动
2、在UART中断中，每收到一个数据就更新一次定时器，如果定时器超时，说明数据接收完成，可以进行协议解析了。
3、在定时器回调函数中，判断是否接收完成，如果完成则发送信号量，并解析数据。
*/
static void FP_Task(void *argument)
{
    FP_Msg_t msg;
    BaseType_t xResult;
        // 上电
    
    //创建一个定时器，并启动
    FP_Timer = xTimerCreate("FP_Timer", pdMS_TO_TICKS(FP_RECEIVE_TIMEOUT), pdFALSE, (void *)0, FP_TimerCallback);
    xTimerStart(FP_Timer, 0);
    xTimerStop(FP_Timer, 0);       // 立即停止定时器，目前还不需要用

    //获取模板数量
    FP_CMD_SEND_RECORD = FP_MSG_GET_TEMPLATE_NUM;
    FP_GetValidTemplateNum();
    
    // 任务开始
    for (;;)
    {
        if (xQueueReceive(FP_MsgQueue, &msg, portMAX_DELAY) == pdPASS)
        {
            switch (msg.type)
            {
                case FP_MSG_FINGER_PRESSED:
                    if (FP_Mode == FP_MODE_ENROLL)
                    {
                        FP_CMD_SEND_RECORD = FP_MSG_ENROLL;
                        FP_EnrollStart(FP_TemplateNum+1,2,0); // ID 1,录入次数2，参数0
                    }
                    else
                    {
                        FP_CMD_SEND_RECORD = FP_MSG_IDENTIFY;
                        FP_IdentifyStart(2,0xffff,0);//分数等级2，搜索所有模板，参数0
                    }
                    break;
                case FP_MSG_GET_TEMPLATE_NUM:
                    FP_HandleValidTemplateNum(FP_RxBuffer,msg.param);
                    break;
                    
                case FP_MSG_ENROLL:
                    FP_HandleEnrollACK(FP_RxBuffer,msg.param);
                    break;
                    
                case FP_MSG_IDENTIFY:
                    FP_HandleIdentify(FP_RxBuffer,msg.param);
                default:
                    break;
            }
            if(msg.type != FP_MSG_FINGER_PRESSED)
            {
                if(FP_RxIndex > msg.param) //说明处理期间有新的数据
                {   uint8_t i;
                    taskENTER_CRITICAL(); // 关闭中断，并记录当前中断状态
                    for(i=0;i<FP_RxIndex-msg.param;i++)
                    {
                        FP_RxBuffer[i] = FP_RxBuffer[msg.param+i];
                    }
                    FP_RxIndex = FP_RxIndex-msg.param;
                    taskEXIT_CRITICAL();  // 恢复之前的中断状态
                }
                else 
                {
                    FP_RxIndex = 0;
                }
            }
        }
    }
}


/**
 * @brief 创建指纹模块任务
 */
void FP_CreateTask(void)
{
        // 创建二值信号量 - 用于手指按下检测
    //FP_RxSemaphore = xSemaphoreCreateBinary();
    
    // 创建消息队列
    FP_MsgQueue = xQueueCreate(FP_QUEUE_SIZE, sizeof(FP_Msg_t));
    // 创建指纹模块任务
    xTaskCreate(
        FP_Task,                /* 任务函数 */
        "FP_Task",              /* 任务名称 */
        STACK_SIZE_FINGERPRINT,                    /* 任务栈大小 */
        NULL,                   /* 任务参数 */
        TASK_PRIORITY_FINGERPRINT, /* 任务优先级 */
        &FP_TaskHandle          /* 任务句柄 */
    );
}

//注册指纹按键测试
void FP_EnrollTest(void)
{
    FP_Mode = FP_MODE_ENROLL;
    printf("enter enroll mode\r\n");
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
        //发送队列
        FP_Msg_t msg;
        msg.type = FP_MSG_FINGER_PRESSED;
        msg.param = 0;
        //判断系统是否已经启动
        if(FP_MsgQueue != NULL)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(FP_MsgQueue, &msg, &xHigherPriorityTaskWoken);
            // 释放信号量，通知任务手指按下
            // 如果有更高优先级的任务被唤醒，则进行任务切换
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        else
        {
            printf("system not start\r\n");
        }
    }
}

/**
 * @brief 串口接收回调，用于接收指纹模块数据，在uart.c中调用
 */
void Fingerprint_RxCpltCallback(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // 初始化为 pdFALSE

    // 储存接收到的数据
    if (FP_RxIndex < FP_MAX_BUFFER_SIZE)
    {
        FP_RxBuffer[FP_RxIndex++] = FP_RxTempBuffer[0];
    }

    //系统是否已经，使用freertos接口
    if(FP_Timer != NULL)
    {
        xTimerResetFromISR(FP_Timer, &xHigherPriorityTaskWoken);
        HAL_UART_Receive_IT(&huart4, FP_RxTempBuffer, 1);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        FP_RxIndex = 0;
        printf("rtos not start:%x\r\n",FP_RxTempBuffer[0]);
         HAL_UART_Receive_IT(&huart4, FP_RxTempBuffer, 1);
    }
   
}

/**
 * @brief 检查接收帧是否完整
 * @return 1: 完整; 0: 不完整
 */
static uint8_t FP_CheckFrameComplete(void)
{
    // 这里需要根据指纹模块的协议实现帧完整性检查
    // 示例实现，实际应该根据协议来判断
    
    // 假设协议格式：
    // 帧头(2字节) + 命令(1字节) + 数据长度(2字节) + 数据 + 校验和(2字节)
    // 帧头固定为 0xEF01
    
    // 检查帧长度是否足够
    if (FP_RxIndex < 7) // 最小帧长度：帧头(2) + 命令(1) + 长度(2) + 校验和(2)
        return 0;
    
    // 检查帧头
    if (FP_RxBuffer[0] != 0xEF || FP_RxBuffer[1] != 0x01)
        return 0;
    
    // 获取数据长度
    uint16_t dataLen = (FP_RxBuffer[3] << 8) | FP_RxBuffer[4];
    
    // 检查帧总长度是否符合
    if (FP_RxIndex < (5 + dataLen + 2)) // 帧头+命令+长度+数据+校验和
        return 0;
    
    // 检查校验和
    // 这里应该实现校验和计算和比较
    
    return 1; // 暂时简化为返回完整
}

/**
  * @brief This function handles UART4 global interrupt.
  */
void UART4_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart4);
}


#endif /* FINGERPRINT_ENABLE */

