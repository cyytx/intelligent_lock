/**
  ******************************************************************************
  * @file    face.c
  * @author  cyytx
  * @brief   人脸识别模块的源文件,实现人脸检测、匹配等功能
  ******************************************************************************
  */

#include "face.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "timers.h"
#include "priorities.h"
#include "sg90.h"
#if FACE_ENABLE


/* 配置宏定义 */
#define FACE_BUFFER_SIZE      512     /* 通信缓冲区大小 */
#define FACE_RX_TIMEOUT       pdMS_TO_TICKS(10)  
#define FACE_TX_TIMEOUT       100 //100ms

#define FACE_ENROLL_TIMEOUT  10 //10s
#define FACE_IDENTIFY_TIMEOUT 10 //10s



/* 私有变量定义 */
static UART_HandleTypeDef huart5;            // 人脸识别模块串口句柄
static uint8_t FACE_RxBuffer[FACE_BUFFER_SIZE];   // 接收缓冲区
static uint16_t FACE_RxIndex = 0;                 // 接收缓冲区索引
static uint8_t FACE_RxTempBuffer[1];         // 单字节接收缓冲区
static uint8_t FACE_RegisterUserNum = 0;              // 注册用户数量

/* FreeRTOS相关变量 */
static TaskHandle_t faceTaskHandle = NULL;   // 人脸识别任务句柄
static QueueHandle_t faceMsgQueue = NULL;    // 人脸识别消息队列
static TimerHandle_t faceRxTimer = NULL;       // 人脸识别定时器

FACE_EnrollParams userEnrollParams = {
    .admin = 0x01,//管理员
    .user_name = "admin1",//用户名
    .s_face_dir = 0x01,//人脸方向
    .timeout = 0x0A//超时时间
};



/**
  * @brief  初始化人脸识别模块
  * @param  无
  * @retval 无
  */
void FACE_Init(void)
{
    /* 初始化人脸识别模块的UART接口 */
    huart5.Instance = UART5;
    huart5.Init.BaudRate = 115200;
    huart5.Init.WordLength = UART_WORDLENGTH_8B;
    huart5.Init.StopBits = UART_STOPBITS_1;
    huart5.Init.Parity = UART_PARITY_NONE;
    huart5.Init.Mode = UART_MODE_TX_RX;
    huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart5.Init.OverSampling = UART_OVERSAMPLING_16;
    huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    
    if (HAL_UART_Init(&huart5) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_NVIC_SetPriority(UART5_IRQn, FACE_IRQ_PRIORITY_USART5, 0);
    HAL_NVIC_EnableIRQ(UART5_IRQn);
    /* 开启UART中断接收 */
    HAL_UART_Receive_IT(&huart5, FACE_RxTempBuffer, 1);
}


/**
  * @brief  计算校验和
  * @param  data: 数据缓冲区
  * @param  length: 数据长度
  * @retval uint8_t: 协议的奇偶校验码，计算方式为整条协议除去 SyncWord 部分后，其余字节按位做 XOR运算。
  */
static uint8_t FACE_CalculateChecksum(uint8_t *data, uint16_t length)
{
    uint8_t parity = 0;
    for (uint16_t i = 2; i < length; i++) {
        parity ^= data[i];
    }
    return parity;
}


/**
  * @brief  发送命令到人脸识别模块
  * @param  cmd: 命令码
  * @param  data: 命令数据
  * @param  length: 数据长度
  * @retval FACE_StatusTypeDef: 操作状态
  * // 消息格式：SyncWord(2byte为0xEF 0xAA) + MsgID(1byte消息ID) + 
  * Size(2byte) + Data(Nbyte) + ParityCheck(1byte校验码)
  * size=N 表示data的长度，如data 没有数据，则size=0，如0xEF 0xAA 0x10 0x00 0x00 0x10
  */
static FACE_StatusTypeDef FACE_SendCommand(uint8_t *data, uint16_t length)
{
    uint8_t checksum = 0;
    /* 计算校验和 */
    checksum = FACE_CalculateChecksum(data, length);
    data[length++] = checksum;
    for(uint16_t i=0;i<length;i++)
    {
        printf("%02X ",data[i]);
    }
    printf("\r\n");
    printf("length:%d\r\n",length);

    /* 使用UART发送数据 */
    if (HAL_UART_Transmit(&huart5, data, length, FACE_TX_TIMEOUT) != HAL_OK) {
        return FACE_ERROR;
        printf("face send cmd error\r\n");
    }
    
    return FACE_OK;
}

static FACE_StatusTypeDef FACE_SendCommand_test(void)
{
    uint8_t txBuffer[]={0xEF,0xAA,0x10,0x00,0x00,0x10};


    /* 使用UART发送数据 */
    if (HAL_UART_Transmit(&huart5, txBuffer, sizeof(txBuffer), FACE_TX_TIMEOUT) != HAL_OK) {
        return FACE_ERROR;
        printf("face send cmd error\r\n");
    }
    
    return FACE_OK;
}


/**
  * @brief  定时器回调函数
  * @param  xTimer: 定时器句柄
  * @retval 无
  */
void FACE_TimerCallback(TimerHandle_t xTimer)
{
    FACE_Msg msg;
    msg.msgType = FACE_MSG_DATA_READY;
    msg.data = FACE_RxIndex;
    
    /* 向人脸识别任务发送超时消息 */
    xQueueSend(faceMsgQueue, &msg, 0);
}


void FACE_RxCpltCallback(void)
{
   
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // 初始化为 pdFALSE

    // 储存接收到的数据
    if (FACE_RxIndex < FACE_BUFFER_SIZE)
    {
        FACE_RxBuffer[FACE_RxIndex++] = FACE_RxTempBuffer[0];
    }

    //系统是否已经，使用freertos接口
    if(faceRxTimer != NULL)
    {
        xTimerResetFromISR(faceRxTimer, &xHigherPriorityTaskWoken);
        HAL_UART_Receive_IT(&huart5, FACE_RxTempBuffer, 1);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        FACE_RxIndex = 0;
        printf("face rtos not start:%x\r\n",FACE_RxTempBuffer[0]);
        HAL_UART_Receive_IT(&huart5, FACE_RxTempBuffer, 1);
    }
}


/**
  * @brief  UART中断回调函数
  * @param  无
  * @retval 无
  */
void UART5_IRQHandler(void)
{
    /* 在UART中断处理函数中调用 */
    HAL_UART_IRQHandler(&huart5);
}

/**
  * @brief  接收完成回调函数
  * @param  无
  * @retval 无
  */



/**
  * @brief  检查接收到的帧是否完整
  * @param  无
  * @retval 0: 帧不完整, 1: 帧完整
  * //SyncWord(2byte EFAA)+MsgID(1byte)+Size(2byte)+Data(Nbyte)+ParityCheck(1byte) 
  */
static uint8_t FACE_CheckFrameComplete(uint16_t dataLength)
{
    uint16_t length = 0;
    for(uint16_t i=0;i<dataLength;i++)
    {
        printf("%02X ",FACE_RxBuffer[i]);
    }
    printf("\r\n");
    if(FACE_RxIndex < 6)
    {
        printf("face check frame complete base length failed\r\n");
        return 0;
    }
    if(FACE_RxBuffer[0] != 0xEF || FACE_RxBuffer[1] != 0xAA)
    {
        printf("face check frame complete syncword failed\r\n");
        return 0;
    }
    length = (FACE_RxBuffer[3] << 8) | FACE_RxBuffer[4];
    if(length+6 != dataLength)
    {
        printf("face check frame complete length failed \r\n");
        return 0;
    }
    //检验校验和
    uint8_t checksum = FACE_CalculateChecksum(FACE_RxBuffer, dataLength-1);
    if(checksum != FACE_RxBuffer[dataLength-1])
    {
        printf("face check frame complete checksum failed length:%d,dataLength:%d,checksum:%d\r\n",length,dataLength,checksum);
        return 0;
    }
    return 1;
}

void FACE_Register_Single_Handle(void)
{
    if(FACE_RxBuffer[6] == MR_SUCCESS)
    {
        printf("face register success %x \r\n",FACE_RxBuffer[6]);
    }
    else
    {
        printf("face register failed %x\r\n",FACE_RxBuffer[6]);
    }
}
//SyncWord(2byte EFAA)+MsgID(1byte)+Size(2byte)+Data(Nbyte)+ParityCheck(1byte) 
/**
  * @brief  注册新用户人脸
  * @param  userId: 要注册的用户ID
  * @retval FACE_StatusTypeDef: 操作状态

  * admin (1 byte) + user_name (32 bytes) + s_face_dir (1 byte) + timeout (1 byte 单位s)
  * admin: 0x01 表示管理员，0x00 表示用户
  */
FACE_StatusTypeDef FACE_Register_Single(void)
{
    uint8_t txBuffer[50];
    uint8_t index=0;
    uint16_t data_length=0;
        /* 命令包头 */
    txBuffer[index++] = 0xEF;
    txBuffer[index++] = 0xAA;
    /* 命令码 */
    txBuffer[index++] = FACE_CMD_ENROLL_SINGLE;
    data_length = sizeof(userEnrollParams);
    /* 数据长度 */
    txBuffer[index++] = (data_length >> 8) & 0xFF;
    txBuffer[index++] = data_length & 0xFF;
    /* 数据 */
    txBuffer[index++] = userEnrollParams.admin;
    memcpy(&txBuffer[index],userEnrollParams.user_name,32);
    index += 32;
    txBuffer[index++] = userEnrollParams.s_face_dir;
    txBuffer[index++] = userEnrollParams.timeout;
    
    // uint8_t checksum = FACE_CalculateChecksum(txBuffer+2, index-2);
    // txBuffer[index++] = checksum;
    /* 发送人脸注册命令 */
    FACE_StatusTypeDef status = FACE_SendCommand(txBuffer, index);
    if (status != FACE_OK) {
        return status;
    }
    return FACE_OK;
}

//人脸识别结果处理
void FACE_Identify_Result_Handle(void)
{
    if(FACE_RxBuffer[6] == MR_SUCCESS)
    {
        printf("face identify success\r\n");
        //开锁
        SendLockCommand(LOCK_CMD_OPEN);
    }
    else
    {
        printf("face identify failed,result:%x\r\n",FACE_RxBuffer[6]);
    }
}
//人脸识别指令填充和发送
//ef aa 12 00 02 00 0a 1a
uint8_t FACE_Identify_Cmd_Send(void)
{
    uint8_t txBuffer[]={0xEF,0xAA,0x12,0x00,0x02,0x00,0x0A,0x1A};
    txBuffer[6] = FACE_IDENTIFY_TIMEOUT;//设置超时时间
    uint8_t status = FACE_SendCommand(txBuffer, sizeof(txBuffer));
    if (status != FACE_OK) {
        printf("face identify cmd send failed\r\n");
        return status;
    }
    return FACE_OK;
}

//获取用户数量和ID结果处理
void FACE_Get_User_Num_And_ID_Handle(void)
{
    FACE_RegisterUserNum = FACE_RxBuffer[7];
    printf("face get user num and id success,num:%d\r\n",FACE_RegisterUserNum);
}

//获取用户数量和ID
//ef aa 24 00 01 00 25
uint8_t FACE_Get_User_Num_Cmd_Send(void)
{
    uint8_t txBuffer[]={0xEF,0xAA,0x24,0x00,0x01,0x00,0x25};
    uint8_t status = FACE_SendCommand(txBuffer, sizeof(txBuffer));
    if (status != FACE_OK) {
        printf("face get user num and id cmd send failed\r\n");
        return status;
    }
    return FACE_OK;
}

void FACE_Register_Cmd(void)
{
    FACE_Msg msg;
    msg.msgType = FACE_MSG_ENROLL;
    printf("face register cmd\r\n");
    /* 向人脸识别任务发送超时消息 */
    xQueueSend(faceMsgQueue, &msg, 0);
}


void FACE_Identify_Cmd(void)
{
    FACE_Msg msg;
    msg.msgType = FACE_MSG_IDENTIFY;
    printf("face identify cmd\r\n");
    xQueueSend(faceMsgQueue, &msg, 0);
}

/**
  * @brief  人脸识别任务函数
  * @param  argument: 任务参数
  * @retval 无
  */
static void FACE_Task(void *argument)
{
    printf("FACE_Task started\r\n");
    FACE_Msg msg;
    uint8_t complete = 0;
    faceRxTimer = xTimerCreate("FaceTimer", FACE_RX_TIMEOUT, 
                            pdFALSE, (void*)0, FACE_TimerCallback);
    xTimerStart(faceRxTimer, 0);
    xTimerStop(faceRxTimer, 0);
    FACE_Get_User_Num_Cmd_Send();//获取用户数量

    for(;;)
    {
        /* 等待消息队列中的消息 */
        if(xQueueReceive(faceMsgQueue, &msg, portMAX_DELAY) == pdPASS)
        {
            switch(msg.msgType)
            {             
                case FACE_MSG_ENROLL:
                    /* 处理人脸注册消息 */
                    FACE_Register_Single();
                    //FACE_SendCommand_test();
                    break;
                
                case FACE_MSG_IDENTIFY:
                    FACE_Identify_Cmd_Send();
                    /* 处理人脸识别消息 */
                    break;
                //SyncWord(2byte)+MsgID(1byte)+Size(2byte)+Data(Nbyte)+ParityCheck(1byte) 
                // SyncWord=EFAA   Data(Nbyte)=mid(1byte)+result(1byte)+data(n-byte)
                case FACE_MSG_DATA_READY:
                    complete = FACE_CheckFrameComplete(msg.data);
                    if(complete == 0)
                    {
                        FACE_RxIndex = 0;
                        break;
                    }
                   uint8_t mid=FACE_RxBuffer[5];
                   switch(mid)  
                   {
                    case FACE_CMD_ENROLL_SINGLE:
                        FACE_Register_Single_Handle();
                        break;
                    case FACE_CMD_VERIFY:
                        FACE_Identify_Result_Handle();
                        break;
                    case FACE_CMD_GET_ALL_USERID:
                        FACE_Get_User_Num_And_ID_Handle();
                        break;
                    default:
                    break;
                   }
                   FACE_RxIndex = 0;
                break;
                
                default:
                    break;
            }
        }
    }
}

/**
  * @brief  创建人脸识别任务
  * @param  无
  * @retval 无
  */
void FACE_CreateTask(void)
{
    printf("FACE_CreateTask called\r\n");
    /* 创建消息队列 */
    faceMsgQueue = xQueueCreate(10, sizeof(FACE_Msg));
    /* 创建人脸识别任务 */
    xTaskCreate(FACE_Task, "FaceTask", 1024, NULL, TASK_PRIORITY_FACE, &faceTaskHandle);
}


#endif /* FACE_ENABLE */