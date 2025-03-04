/**
  ******************************************************************************
  * @file    fingerprint.h
  * @author  cyytx
  * @brief   指纹模块的头文件,包含指纹的初始化、扫描、匹配等功能函数声明
  ******************************************************************************
  */

#ifndef __FINGERPRINT_H
#define __FINGERPRINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "hard_enable_ctrl.h"

#if FINGERPRINT_ENABLE

#define FP_IRQ_Pin GPIO_PIN_5
#define FP_IRQ_GPIO_Port GPIOE
#define FP_CTRL_Pin GPIO_PIN_6
#define FP_CTRL_GPIO_Port GPIOE

/* ZW101通信协议相关定义 */
/* 包头为0xEF01，低字节在前 */
#define FP_PACK_HEAD_0            0xEF    // 包头低字节
#define FP_PACK_HEAD_1            0x01    // 包头高字节
#define FP_MAX_BUFFER_SIZE        128     // 最大缓冲区大小

/* 包标识定义 */
#define FP_PACK_CMD               0x01    // 命令包
#define FP_PACK_DATA              0x02    // 数据包
#define FP_PACK_END               0x08    // 结束包

//回应包标识
#define FP_RESPONSE_FLAG                0x07    // 命令回应
/* ZW101命令定义 */
#define FP_CMD_GET_VALID_TEMPLATE_NUM    0x20   // 获取有效模板个数
#define FP_CMD_AUTO_ENROLL_TEMPLATE      0x31   // 自动注册指纹模板
#define FP_CMD_AUTO_IDENTIFY             0x32   // 自动验证指纹

/* ZW101回应定义 */
#define FP_ACK_SUCCESS               0x00    // 操作成功
#define FP_ACK_FAIL                  0x01    // 操作失败
#define FP_ACK_NO_FINGER             0x21    // 没有手指
#define FP_ACK_ENROLL_CONTINUE       0x22    // 继续注册
#define FP_ACK_IDENTIFY_CONTINUE     0x23    // 继续验证
#define FP_ACK_BAD_FINGER            0x25    // 采集质量差
#define FP_ACK_GEN_FAIL              0x30    // 特征提取失败
#define FP_ACK_DB_FULL               0x41    // 数据库已满

/**
 * @brief 指纹模块状态枚举
 */
typedef enum {
    FP_STATE_IDLE,           // 空闲
    FP_STATE_ENROLLING,      // 注册中
    FP_STATE_IDENTIFYING,    // 识别中
    FP_STATE_SUCCESS,        // 操作成功
    FP_STATE_FAIL            // 操作失败
} FP_State_t;

/**
 * @brief 指纹模块任务消息类型
 */
typedef enum {
    FP_MSG_NONE,            // 无命令
    FP_MSG_GET_TEMPLATE_NUM,// 获取模板数量
    FP_MSG_ENROLL,          // 注册指纹
    FP_MSG_IDENTIFY,        // 识别指纹
    FP_MSG_FINGER_PRESSED,  // 手指按下
} FP_MsgType_t;

/**
 * @brief 指纹模块消息结构体
 */
typedef struct {
    FP_MsgType_t type;      // 消息类型
    uint16_t param;         // 参数
} FP_Msg_t;

/**
 * @brief 初始化指纹模块
 */
void FP_Init(void);

/**
 * @brief 指纹模块任务创建
 */
void FP_CreateTask(void);

/**
 * @brief 获取指纹有效模板数量
 * @return 有效模板数量
 */
uint16_t FP_GetValidTemplateNum(void);

/**
 * @brief 开始指纹注册流程
 * @param template_id 模板ID
 * @return 0-成功, 其他-失败
 */
int FP_EnrollStart(uint16_t template_id);

/**
 * @brief 开始指纹识别流程
 * @return 0-成功, 其他-失败
 */
int FP_IdentifyStart(void);

/**
 * @brief 检查手指是否按下
 * @return 1-按下, 0-未按下
 */
uint8_t FP_IsFingerPressed(void);

/**
 * @brief 获取当前指纹模块状态
 * @return 当前状态
 */
FP_State_t FP_GetState(void);

/**
 * @brief 设置指纹模块状态
 * @param state 状态
 */
void FP_SetState(FP_State_t state);

/**
 * @brief 外部中断回调函数，用于FP_IRQ_Pin中断
 */
void FP_IRQ_Callback(void);
void Fingerprint_RxCpltCallback(void);

#endif /* FINGERPRINT_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __FINGERPRINT_H */ 