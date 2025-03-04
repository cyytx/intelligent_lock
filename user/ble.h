#ifndef __BLE_H
#define __BLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "hard_enable_ctrl.h"

#if BLE_ENABLE

/* BLE引脚定义 */
#define BLE_LINK_Pin GPIO_PIN_13
#define BLE_LINK_GPIO_Port GPIOH
#define BLE_SLEEP_Pin GPIO_PIN_8
#define BLE_SLEEP_GPIO_Port GPIOA

/* BLE状态定义 */
#define BLE_CONNECTED    1
#define BLE_DISCONNECTED 0

/* AT指令响应超时时间 */
#define BLE_AT_TIMEOUT 100  // 单位：毫秒

/* 接收缓冲区大小 */
#define BLE_RX_BUFFER_SIZE 256

/* AT指令集定义 - \r\n 为 ASCII 码 0x0d 及 0x0a */
/* 上电或重启成功的串口提示：(+READY\r\n)，HOST MCU 必须在收到此消息后，才能执行指令和数据的操作 */

#define BLE_AT_GET_MAC        "AT+MAC?\r\n"      /* 查询模块 MAC 地址 */
#define BLE_AT_SET_MAC        "AT+MAC=%s\r\n"    /* 设置模组 MAC 地址 */
#define BLE_AT_SET_NAME       "AT+NAME=%s\r\n"   /* 设置设备名称 */
#define BLE_AT_GET_NAME       "AT+NAME?\r\n"     /* 查询设备名称 */
#define BLE_AT_SET_ADV        "AT+ADV=%d\r\n"    /* 设置广播状态 */
#define BLE_AT_GET_ADV        "AT+ADV?\r\n"      /* 查询广播状态 */
#define BLE_AT_SET_UART       "AT+UART=%d\r\n"   /* 设置波特率 */
#define BLE_AT_GET_UART       "AT+UART?\r\n"     /* 查询模组串口波特率 */
#define BLE_AT_DISCONNECT     "AT+DISCONN=%d\r\n" /* 断开蓝牙连接 */
#define BLE_AT_GET_DEVICE     "AT+DEV?\r\n"      /* 查询当前已连接的设备 */
#define BLE_AT_SET_ADV_INTVL  "AT+AINTVL=%d\r\n" /* 修改广播间隔 */
#define BLE_AT_GET_ADV_INTVL  "AT+AINTVL?\r\n"   /* 查询广播间隔 */
#define BLE_AT_GET_VERSION    "AT+VER?\r\n"      /* 查询软件版本 */
#define BLE_AT_RESET          "AT+RESET=1\r\n"   /* 恢复出厂设置 */
#define BLE_AT_REBOOT         "AT+REBOOT=1\r\n"  /* 设置模组重启 */
#define BLE_AT_SET_TXPOWER    "AT+TXPOWER=%d\r\n" /* 修改模组的发射功率 */
#define BLE_AT_GET_TXPOWER    "AT+TXPOWER?\r\n"  /* 查询模组当前发射功率 */
#define BLE_AT_SET_UUIDS      "AT+UUIDS=%s\r\n"  /* 设置 BLE 主服务通道 */
#define BLE_AT_GET_UUIDS      "AT+UUIDS?\r\n"    /* 查询 BLE 主服务通道 */
#define BLE_AT_SET_UUIDN      "AT+UUIDN=%s\r\n"  /* 设置 BLE 读服务通道 */
#define BLE_AT_GET_UUIDN      "AT+UUIDN?\r\n"    /* 查询 BLE 读服务通道 */
#define BLE_AT_SET_UUIDW      "AT+UUIDW=%s\r\n"  /* 设置 BLE 写服务通道 */
#define BLE_AT_GET_UUIDW      "AT+UUIDW?\r\n"    /* 查询 BLE 写服务通道 */
#define BLE_AT_SET_AMDATA     "AT+AMDATA=%s\r\n" /* 设置自定义广播数据 */
#define BLE_AT_GET_AMDATA     "AT+AMDATA?\r\n"   /* 查询自定义广播数据 */

/* 函数声明 */
void BLE_Link_Status_Init(void);
void BLE_Sleep_Pin_Init(void);
void BLE_Init(void);
void BLE_Sleep(void);
uint8_t BLE_Is_Connected(void);
HAL_StatusTypeDef BLE_Send_AT_Command(char* command, char* response_buffer, uint16_t buffer_size, uint32_t timeout);
HAL_StatusTypeDef BLE_Reset(void);
HAL_StatusTypeDef BLE_Set_Name(char* name);
HAL_StatusTypeDef BLE_Get_Name(char* name_buffer, uint16_t buffer_size);
HAL_StatusTypeDef BLE_Set_MAC(char* mac);
HAL_StatusTypeDef BLE_Get_MAC(char* mac_buffer, uint16_t buffer_size);
HAL_StatusTypeDef BLE_Send(uint8_t* data, uint16_t size);
HAL_StatusTypeDef BLE_Receive_IT(uint8_t* buffer, uint16_t size);
HAL_StatusTypeDef BLE_Set_ADV(uint8_t state);
HAL_StatusTypeDef BLE_Get_ADV(char* state_buffer, uint16_t buffer_size);
HAL_StatusTypeDef BLE_Set_UART(uint32_t baudrate);
HAL_StatusTypeDef BLE_Get_UART(char* baudrate_buffer, uint16_t buffer_size);
HAL_StatusTypeDef BLE_Disconnect(uint8_t conn_handle);
HAL_StatusTypeDef BLE_Get_Device(char* device_buffer, uint16_t buffer_size);
HAL_StatusTypeDef BLE_Set_ADV_Interval(uint16_t interval);
HAL_StatusTypeDef BLE_Get_ADV_Interval(char* interval_buffer, uint16_t buffer_size);
HAL_StatusTypeDef BLE_Get_Version(char* version_buffer, uint16_t buffer_size);
HAL_StatusTypeDef BLE_Factory_Reset(void);
HAL_StatusTypeDef BLE_Reboot(void);
HAL_StatusTypeDef BLE_Set_TxPower(uint8_t power);
HAL_StatusTypeDef BLE_Get_TxPower(char* power_buffer, uint16_t buffer_size);
void BLE_RxCpltCallback(void);
void BLE_Process(void);
void BLE_CreateTask(void);
void BLE_KEY_TEST(void);
/* 外部变量声明 */
// extern uint8_t BLE_Link_Status;
// extern uint8_t BLE_RxBuffer[BLE_RX_BUFFER_SIZE];
// extern uint16_t BLE_RxSize;
// extern uint8_t BLE_RxReady;

#endif /* BLE_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __BLE_H */ 