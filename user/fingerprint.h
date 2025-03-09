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

#ifdef FINGERPRINT_ENABLE

#define FP_IRQ_Pin GPIO_PIN_5
#define FP_IRQ_GPIO_Port GPIOE
#define FP_CTRL_Pin GPIO_PIN_6
#define FP_CTRL_GPIO_Port GPIOE

/* ZW101通信协议相关定义 */
/* 包头为0xEF01，低字节在前 */
#define FP_PACK_HEAD_0            0xEF    // 包头低字节
#define FP_PACK_HEAD_1            0x01    // 包头高字节


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

typedef enum {
    FP_MODE_IDENTIFY = 0, //识别
    FP_MODE_ENROLL = 1 //注册
} FP_Mode_t;

//指纹AT指令 枚举
typedef enum {
    FP_AT_CMD_GET_VALID_TEMPLATE_NUM = 0x1D,   // 获取有效模板个数
    FP_AT_CMD_AUTO_ENROLL_TEMPLATE = 0x31,     // 自动注册指纹模板
    FP_AT_CMD_AUTO_IDENTIFY = 0x32,             // 自动验证指纹
} FP_AtCmd_t;


//注册回应参数
/**
 * @brief 确认码枚举类型
 */
typedef enum {
    FP_ENROLL_CONFIRM_SUCCESS = 0x00,          // 成功
    FP_ENROLL_CONFIRM_FAILURE = 0x01,          // 失败
    FP_CONFIRM_GENERATE_FEATURE_FAIL = 0x07, // 生成特征失败
    FP_CONFIRM_TEMPLATE_MERGE_FAIL = 0x0A,   // 合并模板失败
    FP_CONFIRM_ID_OUT_OF_RANGE = 0x0B,   // ID号超出范围
    FP_CONFIRM_STORAGE_FULL = 0x1F,      // 指纹库已满
    FP_CONFIRM_TEMPLATE_NOT_EMPTY = 0x22, // 指纹模板非空
    FP_CONFIRM_ENROLL_COUNT_ERROR = 0x25, // 录入次数设置错误
    FP_CONFIRM_TIMEOUT = 0x26,           // 超时
    FP_CONFIRM_FINGER_EXISTS = 0x27      // 指纹已存在
} FP_ConfirmCode_t;

/**
 * @brief 参数1枚举类型
 */
typedef enum {
    FP_PARAM1_FINGERPRINT_CHECK = 0x00,  // 指纹合法性检测
    FP_PARAM1_GET_IMAGE = 0x01,          // 获取图像
    FP_PARAM1_GENERATE_FEATURE = 0x02,   // 生产特征
    FP_PARAM1_JUDGE_FINGER = 0x03,       // 判断手指离开
    FP_PARAM1_MERGE_TEMPLATE = 0x04,     // 合并模板
    FP_PARAM1_REGISTER_CHECK = 0x05,     // 注册检验
    FP_PARAM1_STORAGE_TEMPLATE = 0x06    // 存储模板
} FP_Param1_t;

/**
 * @brief 参数2枚举类型
 */
typedef enum {
    FP_PARAM2_FINGERPRINT_CHECK = 0x00,  // 指纹合法性检测
    FP_PARAM2_MERGE_TEMPLATE = 0xF0,     // 合并模板
    FP_PARAM2_CHECK_REGISTERED = 0xF1,   // 检验该手指是否已注册
    FP_PARAM2_STORAGE_TEMPLATE = 0xF2,   // 存储模板
    // n 表示当前求入第n次数，这个需要动态设置，不适合放在枚举中
} FP_Param2_t;

//指纹验证回应
/**
 * @brief 确认码枚举类型
 */
typedef enum {
    FP_IDENTIFY_CONFIRM_SUCCESS = 0x00,          // 成功
    FP_IDENTIFY_CONFIRM_FAILURE = 0x01,          // 失败
    FP_IDENTIFY_CONFIRM_GENERATE_FEATURE_FAIL = 0x07, // 生成特征失败
    FP_IDENTIFY_CONFIRM_NOT_FOUND = 0x09,         // 没找到指纹
    FP_IDENTIFY_CONFIRM_ID_OUT_OF_RANGE = 0x0B,   // ID号超出范围
    FP_IDENTIFY_CONFIRM_REMAINING_FINGERPRINT = 0x17, // 残留指纹
    FP_IDENTIFY_CONFIRM_TEMPLATE_EMPTY = 0x23,    // 指纹模板为空
    FP_IDENTIFY_CONFIRM_FINGERPRINT_NOT_FOUND = 0x24, // 指纹库为空
    FP_IDENTIFY_CONFIRM_TIMEOUT = 0x26,           // 超时
    FP_IDENTIFY_CONFIRM_FINGER_EXISTS = 0x27      // 表示指纹已存在
} FP_IdentifyConfirmCode_t;

/**
 * @brief 参数枚举类型
 */
typedef enum {
    FP_PARAM_FINGERPRINT_CHECK = 0x00,  // 指纹合法性检测
    FP_PARAM_GET_IMAGE = 0x01,          // 获取图像
    FP_PARAM_REGISTERED_FINGER_COMPARE = 0x05 // 已注册指纹比较
} FP_IdentifyParam_t;


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

int FP_EnrollStart(uint16_t template_id,uint8_t recode_num,uint16_t param);

int FP_IdentifyStart(uint8_t score_level,uint16_t template_id,uint16_t param);

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
void FP_EnrollTest(void);

#endif /* FINGERPRINT_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __FINGERPRINT_H */