/**
  ******************************************************************************
  * @file    ble.c
  * @author  cyytx
  * @brief   蓝牙模块的源文件,实现蓝牙的初始化、数据收发等功能 (FreeRTOS适配版)
  ******************************************************************************
  */
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "ble.h"
#include "priorities.h"
#include "key.h"   
#include "sg90.h"


/* FreeRTOS头文件 */

#if BLE_ENABLE

/* BLE任务相关定义 */
static TaskHandle_t BLE_TaskHandle = NULL;     /* 蓝牙任务句柄 */

/* 串口与队列相关定义 */
static UART_HandleTypeDef huart6;              /* BLE模块串口句柄 */
static QueueHandle_t BLE_RxQueue = NULL;       /* 蓝牙接收数据队列 */
static SemaphoreHandle_t BLE_TxMutex = NULL;   /* 蓝牙发送互斥量 */
static SemaphoreHandle_t BLE_RxSemaphore = NULL; /* AT命令响应信号量 */

/* 事件组定义，用于处理BLE各种状态 */
static EventGroupHandle_t BLE_EventGroup = NULL;
#define BLE_EVENT_CONNECTED        (1 << 0)    /* 蓝牙已连接事件位 */
#define BLE_EVENT_DATA_RECEIVED    (1 << 1)    /* 数据接收完成事件位 */
#define BLE_EVENT_AT_MODE          (1 << 2)    /* AT命令模式事件位 */

/* 蓝牙连接状态 */
static uint8_t BLE_Link_Status = 0;

/* 接收数据结构定义 */
typedef struct {
    uint8_t data;
    uint32_t timestamp;
} BLE_RxData_t;

/* 接收缓冲区 - 仅在AT命令模式使用 */
static uint8_t BLE_RxBuffer[BLE_RX_BUFFER_SIZE];
static uint16_t BLE_RxSize = 0;
static uint8_t BLE_RxTempBuffer[1];  // 用于中断接收的临时缓冲区


/*连接pin
PH13  ------> BLE_LINK
PA8   ------> BLE_SLEEP //高电平睡眠，低电平退出睡眠
PC6   ------> USART6_TX  
PC7   ------> USART6_RX
*/

/**
  * @brief  设置蓝牙模块休眠状态
  * @param  sleep_state: 1-休眠，0-唤醒
  * @retval 无
  */
void BLE_Sleep(void)
{
    HAL_GPIO_WritePin(BLE_SLEEP_GPIO_Port, BLE_SLEEP_Pin,  GPIO_PIN_SET);
}

void BLE_WakeUp(void)
{
    HAL_GPIO_WritePin(BLE_SLEEP_GPIO_Port, BLE_SLEEP_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  检查蓝牙连接状态
  * @param  无
  * @retval 1-已连接，0-未连接
  */
uint8_t BLE_Is_Connected(void)
{
    return (xEventGroupGetBits(BLE_EventGroup) & BLE_EVENT_CONNECTED) ? 1 : 0;
}

/**
  * @brief  发送AT指令并等待响应
  * @param  command: AT指令字符串
  * @param  response_buffer: 响应缓冲区
  * @param  buffer_size: 缓冲区大小
  * @param  timeout: 超时时间(ms)
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Send_AT_Command(char* command, char* response_buffer, uint16_t buffer_size, uint32_t timeout)
{
    HAL_StatusTypeDef status;
    BaseType_t xResult;
    uint16_t copy_size;
    
    /* 获取发送互斥量 */
    if (xSemaphoreTake(BLE_TxMutex, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return HAL_TIMEOUT;
    }
    
    /* 清空接收缓冲区 */
    BLE_RxSize = 0;
    memset(BLE_RxBuffer, 0, BLE_RX_BUFFER_SIZE);
    
    /* 确保信号量为空（防止之前没处理完的信号） */
    // #NOTE这里take 一下的原因是为了保证之前give的信号量给清空，因为后面如果超时时间内没有接受到数据，但是
    //在超时后接收到数据，在BLE_TASK中依旧会give一下，这样就会让这次发送，无论有没有接收到，但take时
    //依旧能take到，所以，在发送前take 一下，把它给清空，它是二值信号，所以一次必然能take完。
    xSemaphoreTake(BLE_RxSemaphore, 0);
    
    /* 进入AT命令模式 */
    xEventGroupSetBits(BLE_EventGroup, BLE_EVENT_AT_MODE);
    
    /* 发送AT指令 */
    status = HAL_UART_Transmit(&huart6, (uint8_t*)command, strlen(command), timeout);
    if (status != HAL_OK)
    {
        xEventGroupClearBits(BLE_EventGroup, BLE_EVENT_AT_MODE);
        xSemaphoreGive(BLE_TxMutex);
        return status;
    }
    
    /* 等待响应信号量 */
    xResult = xSemaphoreTake(BLE_RxSemaphore, pdMS_TO_TICKS(timeout));
    
    /* 退出AT命令模式 */
    xEventGroupClearBits(BLE_EventGroup, BLE_EVENT_AT_MODE);
    
    /* 处理结果 */
    if (xResult != pdTRUE)
    {
        xSemaphoreGive(BLE_TxMutex);
        return HAL_TIMEOUT;
    }
    
    /* 复制响应到缓冲区 */
    if (response_buffer != NULL)
    {
        taskENTER_CRITICAL();
        copy_size = (BLE_RxSize < buffer_size) ? BLE_RxSize : buffer_size - 1;
        memcpy(response_buffer, (char*)BLE_RxBuffer, copy_size);
        response_buffer[copy_size] = '\0';
        taskEXIT_CRITICAL();
    }
    printf("AT RETURN %s",response_buffer);
    
    /* 释放发送互斥量 */
    xSemaphoreGive(BLE_TxMutex);
    
    return HAL_OK;
}

/**
  * @brief  重置蓝牙模块,就是恢复出厂设置
  * @param  无
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Reset(void)
{
    char response[32];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+RESET\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  设置蓝牙模块名称
  * @param  name: 蓝牙名称字符串
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_Name(char* name)
{
    char command[64];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+NAME=%s\r\n", name);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  获取蓝牙模块名称
  * @param  name_buffer: 名称缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_Name(char* name_buffer, uint16_t buffer_size)
{
    char response[64];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+NAME?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && name_buffer != NULL)
    {
        char* name_start = strstr(response, "+NAME:");
        if (name_start != NULL)
        {
            name_start += 6; // Skip "+NAME:"
            /* 去除可能的回车换行 */
            char* newline = strchr(name_start, '\r');
            if (newline != NULL)
            {
                *newline = '\0';
            }
            newline = strchr(name_start, '\n');
            if (newline != NULL)
            {
                *newline = '\0';
            }
            strncpy(name_buffer, name_start, buffer_size - 1);
            name_buffer[buffer_size - 1] = '\0';
        }
    }
    
    return status;
}

/**
  * @brief  设置蓝牙模块MAC地址
  * @param  mac: MAC地址字符串 (格式: XX:XX:XX:XX:XX:XX)
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_MAC(char* mac)
{
    char command[64];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+MAC=%s\r\n", mac);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  获取蓝牙模块MAC地址
  * @param  mac_buffer: MAC地址缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_MAC(char* mac_buffer, uint16_t buffer_size)
{
    char response[64];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+MAC?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && mac_buffer != NULL)
    {
        char* mac_start = strstr(response, "+MAC:");
        if (mac_start != NULL)
        {
            mac_start += 5; // Skip "+MAC:"
            /* 去除可能的回车换行 */
            char* newline = strchr(mac_start, '\r');
            if (newline != NULL)
            {
                *newline = '\0';
            }
            newline = strchr(mac_start, '\n');
            if (newline != NULL)
            {
                *newline = '\0';
            }
            strncpy(mac_buffer, mac_start, buffer_size - 1);
            mac_buffer[buffer_size - 1] = '\0';
        }
    }
    
    return status;
}

/**
  * @brief  在透传模式下发送数据
  * @param  data: 数据缓冲区
  * @param  size: 数据大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Send(uint8_t* data, uint16_t size)
{
    HAL_StatusTypeDef status = HAL_ERROR;
    
    /* 如果蓝牙未连接，返回错误 */
    if (!BLE_Is_Connected())
    {
        return HAL_ERROR;
    }
    
    /* 获取发送互斥量 */
    if (xSemaphoreTake(BLE_TxMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        /* 发送数据 */
        status = HAL_UART_Transmit(&huart6, data, size, 100);
        
        /* 释放发送互斥量 */
        xSemaphoreGive(BLE_TxMutex);
    }
    
    return status;
}

/**
  * @brief  注册数据接收回调函数
  * @param  callback: 回调函数指针
  * @retval 无
  */
// void BLE_Register_Callback(void (callback)(uint8_t*, uint16_t))
// {
//     BLE_DataReceivedCallback = callback;
// }


/**
  * @brief  设置广播状态
  * @param  state: 广播状态(0-关闭, 1-开启)
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_ADV(uint8_t state)
{
    char command[32];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+ADV=%d\r\n", state);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  查询广播状态
  * @param  state_buffer: 状态缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_ADV(char* state_buffer, uint16_t buffer_size)
{
    char response[32];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+ADV?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && state_buffer != NULL)
    {
        strncpy(state_buffer, response, buffer_size - 1);
        state_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/**
  * @brief  设置模块串口波特率
  * @param  baudrate: 波特率值
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_UART(uint32_t baudrate)
{
    char command[32];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+UART=%d\r\n", baudrate);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  查询模块串口波特率
  * @param  baudrate_buffer: 波特率缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_UART(char* baudrate_buffer, uint16_t buffer_size)
{
    char response[32];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+UART?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && baudrate_buffer != NULL)
    {
        strncpy(baudrate_buffer, response, buffer_size - 1);
        baudrate_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/**
  * @brief  断开蓝牙连接
  * @param  conn_handle: 连接句柄(通常为0)
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Disconnect(uint8_t conn_handle)
{
    char command[32];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+DISCONN=%d\r\n", conn_handle);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  查询当前已连接的设备
  * @param  device_buffer: 设备信息缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_Device(char* device_buffer, uint16_t buffer_size)
{
    char response[64];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+DEV?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && device_buffer != NULL)
    {
        strncpy(device_buffer, response, buffer_size - 1);
        device_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/**
  * @brief  修改广播间隔
  * @param  interval: 广播间隔(ms)
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_ADV_Interval(uint16_t interval)
{
    char command[32];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+AINTVL=%d\r\n", interval);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  查询广播间隔
  * @param  interval_buffer: 间隔信息缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_ADV_Interval(char* interval_buffer, uint16_t buffer_size)
{
    char response[32];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+AINTVL?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && interval_buffer != NULL)
    {
        strncpy(interval_buffer, response, buffer_size - 1);
        interval_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/**
  * @brief  查询软件版本
  * @param  version_buffer: 版本信息缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_Version(char* version_buffer, uint16_t buffer_size)
{
    char response[64];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+VER?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && version_buffer != NULL)
    {
        strncpy(version_buffer, response, buffer_size - 1);
        version_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/**
  * @brief  恢复出厂设置
  * @param  无
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Factory_Reset(void)
{
    char response[32];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+RESET=1\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  重启蓝牙模块
  * @param  无
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Reboot(void)
{
    char response[32];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+REBOOT=1\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  设置发射功率
  * @param  power: 发射功率等级
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_TxPower(uint8_t power)
{
    char command[32];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+TXPOWER=%d\r\n", power);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  查询发射功率
  * @param  power_buffer: 功率信息缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_TxPower(char* power_buffer, uint16_t buffer_size)
{
    char response[32];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+TXPOWER?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && power_buffer != NULL)
    {
        strncpy(power_buffer, response, buffer_size - 1);
        power_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/**
  * @brief  设置BLE主服务通道
  * @param  uuid: UUID字符串
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_UUIDS(char* uuid)
{
    char command[64];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+UUIDS=%s\r\n", uuid);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  查询BLE主服务通道
  * @param  uuid_buffer: UUID信息缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_UUIDS(char* uuid_buffer, uint16_t buffer_size)
{
    char response[64];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+UUIDS?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && uuid_buffer != NULL)
    {
        strncpy(uuid_buffer, response, buffer_size - 1);
        uuid_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/**
  * @brief  设置BLE读服务通道
  * @param  uuid: UUID字符串
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_UUIDN(char* uuid)
{
    char command[64];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+UUIDN=%s\r\n", uuid);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  查询BLE读服务通道
  * @param  uuid_buffer: UUID信息缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_UUIDN(char* uuid_buffer, uint16_t buffer_size)
{
    char response[64];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+UUIDN?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && uuid_buffer != NULL)
    {
        strncpy(uuid_buffer, response, buffer_size - 1);
        uuid_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/**
  * @brief  设置BLE写服务通道
  * @param  uuid: UUID字符串
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_UUIDW(char* uuid)
{
    char command[64];
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+UUIDW=%s\r\n", uuid);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  查询BLE写服务通道
  * @param  uuid_buffer: UUID信息缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_UUIDW(char* uuid_buffer, uint16_t buffer_size)
{
    char response[64];
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+UUIDW?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && uuid_buffer != NULL)
    {
        strncpy(uuid_buffer, response, buffer_size - 1);
        uuid_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/**
  * @brief  设置自定义广播数据
  * @param  data: 广播数据字符串
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Set_AMData(char* data)
{
    char command[128]; // 广播数据可能较长
    char response[32];
    HAL_StatusTypeDef status;
    
    sprintf(command, "AT+AMDATA=%s\r\n", data);
    status = BLE_Send_AT_Command(command, response, sizeof(response), BLE_AT_TIMEOUT);
    
    return status;
}

/**
  * @brief  查询自定义广播数据
  * @param  data_buffer: 广播数据缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval HAL状态
  */
HAL_StatusTypeDef BLE_Get_AMData(char* data_buffer, uint16_t buffer_size)
{
    char response[128]; // 广播数据可能较长
    HAL_StatusTypeDef status;
    
    status = BLE_Send_AT_Command("AT+AMDATA?\r\n", response, sizeof(response), BLE_AT_TIMEOUT);
    
    if (status == HAL_OK && data_buffer != NULL)
    {
        strncpy(data_buffer, response, buffer_size - 1);
        data_buffer[buffer_size - 1] = '\0';
    }
    
    return status;
}

/* BLE密码验证函数，用于验证蓝牙接收到的密码是否正确 */
static uint8_t BLE_ValidatePassword(uint8_t* password, uint8_t length)
{
    extern LockPassword_t lock_passWard; // 引用key.c中定义的lock_passWard变量
    
    if (length != lock_passWard.password_len)
    {
        return 0; // 密码长度不匹配
    }
    
    for (uint8_t i = 0; i < length; i++)
    {
        if (password[i] != lock_passWard.password[i])
        {
            return 0; // 密码内容不匹配
        }
    }
    
    return 1; // 密码匹配
}

/* BLE透传模式下的密码处理函数 */
static void BLE_ProcessPassword(uint8_t* data, uint16_t length)
{
    // 确保数据长度合理
    if (length > 0 && length <= 16) // 最大密码长度为16
    {
        // 将ASCII字符转换为数字
        uint8_t password[16];
        uint8_t password_len = 0;
        
        // 解析输入的数字字符
        for (uint16_t i = 0; i < length; i++)
        {
            if (data[i] >= '0' && data[i] <= '9')
            {
                password[password_len++] = data[i] - '0'; // 转换为实际数字值
                
                // 确保不会超过最大密码长度
                if (password_len >= 16)
                    break;
            }
        }
        
        // 验证密码
        if (password_len > 0)
        {
            if (BLE_ValidatePassword(password, password_len))
            {
                printf("BLE: Password correct! Unlocking door.\r\n");
                SendLockCommand(1); // 发送开锁命令
            }
            else
            {
                printf("BLE: Password incorrect!\r\n");
            }
        }
    }
}

/**
  * @brief  BLE任务函数
  * @param  pvParameters: 任务参数
  * @retval 无
  */
static void BLE_Task(void *pvParameters)
{
    BLE_RxData_t rxData;
    EventBits_t events;
    static uint8_t prev_link_status = 0;
    uint8_t dataBuffer[BLE_RX_BUFFER_SIZE];
    uint16_t dataIndex = 0;
    TickType_t lastActivityTime = xTaskGetTickCount();
    const TickType_t processInterval = pdMS_TO_TICKS(10); // 10ms处理一次

    /* 重置蓝牙模块 */
    HAL_UART_Receive_IT(&huart6, BLE_RxTempBuffer, 1);
    BLE_WakeUp();
    vTaskDelay(10);
    BLE_Reboot();
    vTaskDelay(10);
    BLE_Set_ADV(1);//开始先发广播，正式用红外触发

    for(;;)
    {
        /* 等待蓝牙事件或超时 */
        events = xEventGroupWaitBits(
            BLE_EventGroup,
            BLE_EVENT_DATA_RECEIVED | BLE_EVENT_CONNECTED,
            pdTRUE,  /* 清除标志位 */
            pdFALSE, /* 任意匹配 */
            processInterval);
            
        /* 检查蓝牙连接状态变化 */
        BLE_Link_Status = HAL_GPIO_ReadPin(BLE_LINK_GPIO_Port, BLE_LINK_Pin);
        if (BLE_Link_Status != prev_link_status)
        {
            if (BLE_Link_Status)
            {
                xEventGroupSetBits(BLE_EventGroup, BLE_EVENT_CONNECTED);
                printf("BLE Connected\r\n");
                /* 这里可以执行连接后的初始化操作 */
            }
            else
            {
                xEventGroupClearBits(BLE_EventGroup, BLE_EVENT_CONNECTED);
                printf("BLE Disconnected\r\n");
                /* 这里可以执行断开连接后的清理操作 */
            }
            
            prev_link_status = BLE_Link_Status;
        }

        /* 检查是否在AT命令模式 */
        if (xEventGroupGetBits(BLE_EventGroup) & BLE_EVENT_AT_MODE)
        {
            /* AT命令模式下，累积接收数据并检查AT响应结束标志 */
            while (xQueueReceive(BLE_RxQueue, &rxData, 0) == pdPASS)
            {
                if (BLE_RxSize < BLE_RX_BUFFER_SIZE)
                {
                    BLE_RxBuffer[BLE_RxSize++] = rxData.data;
                    
                    /* 检查是否收到AT命令响应结束标志 */
                    if ((BLE_RxSize >= 2 && BLE_RxBuffer[BLE_RxSize-2] == '\r' && BLE_RxBuffer[BLE_RxSize-1] == '\n') ||
                        (BLE_RxSize >= 4 && strstr((char*)&BLE_RxBuffer[BLE_RxSize-4], "OK\r\n")) ||
                        (BLE_RxSize >= 7 && strstr((char*)&BLE_RxBuffer[BLE_RxSize-7], "ERROR\r\n")))
                    {
                        /* 释放信号量，通知AT命令完成 */
                        xSemaphoreGive(BLE_RxSemaphore);
                        break;
                    }
                }
            }
        }
        else
        {
            /* 透传模式下，累积接收数据并进行处理 */
            uint8_t hasData = 0;
            
            /* 从队列中提取数据 */
            while (xQueueReceive(BLE_RxQueue, &rxData, 0) == pdPASS)
            {
                if (dataIndex < BLE_RX_BUFFER_SIZE)
                {
                    dataBuffer[dataIndex++] = rxData.data;
                    hasData = 1;
                    lastActivityTime = xTaskGetTickCount();
                }
            }
            /* 判断是否需要处理接收到的数据 */
            if (((dataIndex > 0) && ((xTaskGetTickCount() - lastActivityTime) > pdMS_TO_TICKS(10))) ||
                dataIndex >= BLE_RX_BUFFER_SIZE)
            {  
                /* 确保数据以null结尾，形成有效的C字符串 */
                if (dataIndex < BLE_RX_BUFFER_SIZE) {
                    dataBuffer[dataIndex] = '\0';
                } else {
                    dataBuffer[BLE_RX_BUFFER_SIZE - 1] = '\0';
                }
                
                
                printf("BLE_DataReceived: %s\r\n", (char*)dataBuffer);
                
                // 处理接收到的密码
                BLE_ProcessPassword(dataBuffer, dataIndex);
                
                dataIndex = 0;
            }
        }
        
        /* 其他任务处理，如低功耗管理等 */
        vTaskDelay(1);
    }
}

/**
  * @brief  初始化蓝牙连接状态引脚
  * @param  无
  * @retval 无
  */
void BLE_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* 使能GPIOH时钟 */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = BLE_LINK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BLE_LINK_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BLE_SLEEP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BLE_SLEEP_GPIO_Port, &GPIO_InitStruct);
    
    /* 默认不休眠 */
    HAL_GPIO_WritePin(BLE_SLEEP_GPIO_Port, BLE_SLEEP_Pin, GPIO_PIN_SET);
}
/**
  * @brief  初始化蓝牙模块
  * @param  无
  * @retval 无
  */
void BLE_Init(void)
{
    /* 初始化GPIO引脚 */
    BLE_GPIO_Init();
    
    /* 配置UART6 */
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    huart6.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    
    if (HAL_UART_Init(&huart6) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART6_IRQn, BLE_IRQ_PRIORITY_USART6, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    
    /* 开启接收中断 ,不能在这里开启，中断接收会给任务发消息，此时任务和队列还没初始化*/
    //HAL_UART_Receive_IT(&huart6, BLE_RxTempBuffer, 1);

}


void BLE_CreateTask(void)
{
    /* 创建蓝牙任务和相关同步原语 */
    BLE_RxQueue = xQueueCreate(BLE_RX_BUFFER_SIZE, sizeof(BLE_RxData_t));
    BLE_TxMutex = xSemaphoreCreateMutex();
    BLE_RxSemaphore = xSemaphoreCreateBinary();
    BLE_EventGroup = xEventGroupCreate();
    
    if (BLE_RxQueue == NULL || BLE_TxMutex == NULL || 
        BLE_RxSemaphore == NULL || BLE_EventGroup == NULL)
    {
        Error_Handler(); /* 资源创建失败 */
    }
    
    /* 检查连接状态 */
    BLE_Link_Status = HAL_GPIO_ReadPin(BLE_LINK_GPIO_Port, BLE_LINK_Pin);
    if (BLE_Link_Status)
    {
        xEventGroupSetBits(BLE_EventGroup, BLE_EVENT_CONNECTED);
    }
    
    /* 创建BLE处理任务 */
    BaseType_t xReturn = xTaskCreate(BLE_Task, "BLE_Task", STACK_SIZE_BLE, 
                                     NULL, TASK_PRIORITY_BLE, &BLE_TaskHandle);
    if (xReturn != pdPASS)
    {
        Error_Handler(); /* 任务创建失败 */
    }
    
}

/**
  * @brief  UART接收完成回调函数（在HAL_UART_RxCpltCallback中调用）
  * @param  无
  * @retval 无
  */
void BLE_RxCpltCallback(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BLE_RxData_t rxData;
    
    /* 将接收到的字节放入队列 */
    rxData.data = BLE_RxTempBuffer[0];
    rxData.timestamp = xTaskGetTickCountFromISR();
    
    /* 将接收到的字节发送到队列中 */
    xQueueSendFromISR(BLE_RxQueue, &rxData, &xHigherPriorityTaskWoken);
    
    /* 设置数据接收事件位 */
    xEventGroupSetBitsFromISR(BLE_EventGroup, BLE_EVENT_DATA_RECEIVED, &xHigherPriorityTaskWoken);
    
    /* 继续接收下一个字节 */
    HAL_UART_Receive_IT(&huart6, BLE_RxTempBuffer, 1);
    
    /* 如果有更高优先级的任务被唤醒，则进行任务切换 */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/**
  * @brief  USART6中断处理函数
  * @param  无
  * @retval 无
  */
void USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart6);
}


void BLE_KEY_TEST(void)
{
    char buffer[32];
    BLE_WakeUp();
    vTaskDelay(10);
    BLE_Get_MAC(buffer, sizeof(buffer));
    printf("BLE_Get_MAC: %s\r\n", buffer);
    BLE_Set_Name("BLE_TEST");// #BUG 会出问题
    BLE_Get_Name(buffer, sizeof(buffer));// #BUG 会出问题
    printf("BLE_Get_Name: %s\r\n", buffer);
    BLE_Set_ADV(1);
    vTaskDelay(10);
    BLE_Get_ADV(buffer, sizeof(buffer));
    printf("BLE_Get_ADV: %s\r\n", buffer);
}

#endif /* BLE_ENABLE */ 