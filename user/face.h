/**
  ******************************************************************************
  * @file    face.h
  * @author  cyytx
  * @brief   人脸识别模块的头文件,包含人脸检测、匹配等功能函数声明
  ******************************************************************************
  */

#ifndef __FACE_H
#define __FACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "hard_enable_ctrl.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if FACE_ENABLE



// 结果状态枚举定义
typedef enum {
    MR_SUCCESS = 0,                // 成功
    MR_REJECTED = 1,               // 模块拒绝该命令
    MR_ABORTED = 2,                // 录入/验证算法已终止
    MR_FAILED4_CAMERA = 4,         // 相机打开失败
    MR_FAILED4_UNKNOWNREASON = 5,  // 未知错误
    MR_FAILED4_INVALIDPARAM = 6,   // 无效的参数
    MR_FAILED4_NOMEMORY = 7,       // 内存不足
    MR_FAILED4_UNKNOWNUSER = 8,    // 没有已录入的用户
    MR_FAILED4_MAXUSER = 9,        // 录入超过最大用户数量
    MR_FAILED4_FACEENROLLED = 10,  // 人脸已录入
    MR_FAILED4_LIVENESSCHECK = 12, // 活体检测失败
    MR_FAILED4_TIMEOUT = 13,       // 录入或解锁超时
    MR_FAILED4_AUTHORIZATION = 14, // 加密芯片授权失败
    MR_FAILED4_READ_FILE = 19,     // 读取文件失败
    MR_FAILED4_WRITE_FILE = 20,    // 写文件失败
    MR_FAILED4_NO_ENCRYPT = 21,    // 通信协议未加密
    MR_FAILED4_NO_RGBIMAGE = 23,   // RGB 图像没有 ready
    MR_FAILED4_JPGPHOTO_LARGE = 24,// JPG照片过大（照片未注册）
    MR_FAILED4_JPGPHOTO_SMALL = 25  // JPG照片过小（照片未注册）
} ResultCode;

typedef enum {
    FACE_CMD_NONE = 0x00,
    FACE_CMD_VERIFY = 0x12,
    FACE_CMD_ENROLL = 0x13,
    FACE_CMD_ENROLL_SINGLE = 0x1D,
    FACE_CMD_GET_ALL_USERID = 0x24,
} FACE_CmdTypeDef;

/* 定义人脸识别返回状态码 */
typedef enum {
    FACE_OK = 0,              /* 操作成功 */
    FACE_ERROR = 1,           /* 通用错误 */
    FACE_NO_FACE = 2,         /* 未检测到人脸 */
    FACE_TIMEOUT_ERROR = 3,   /* 操作超时 */
    FACE_VERIFY_FAILED = 4,   /* 验证失败 */
    FACE_DUPLICATE_ID = 5,    /* 重复ID */
    FACE_USER_FULL = 6,       /* 用户数量已满 */
    FACE_USER_NOT_EXIST = 7,  /* 用户不存在 */
    FACE_INVALID_PARAM = 8    /* 无效参数 */
} FACE_StatusTypeDef;

/* 定义人脸安全级别 */
typedef enum {
    FACE_SECURITY_LOW = 1,    /* 低安全级别 */
    FACE_SECURITY_MEDIUM = 2, /* 中安全级别 */
    FACE_SECURITY_HIGH = 3    /* 高安全级别 */
} FACE_SecurityLevel;

/* 定义用户信息结构体 */
typedef struct {
    uint16_t id;              /* 用户ID */
    char name[32];            /* 用户名 */
} FACE_UserInfo;

/* 定义任务消息类型 */
typedef enum {
    FACE_MSG_NONE = 0,
    FACE_MSG_DETECT,
    FACE_MSG_ENROLL,
    FACE_MSG_IDENTIFY,
    FACE_MSG_DELETE,
    FACE_MSG_DELETE_ALL,
    FACE_MSG_GET_USERS,
    FACE_MSG_SET_SECURITY,
    FACE_MSG_GET_VERSION,
    FACE_MSG_TIMEOUT,
    FACE_MSG_DATA_READY
} FACE_MsgType;

/* 定义任务消息结构体 */
typedef struct {
    FACE_MsgType msgType;
    uint16_t data;
} FACE_Msg;


// 定义各种命令的参数结构体
typedef struct {
    uint8_t admin;           // 是否设置为管理员(1:是管理员 0:普通用户)
    uint8_t user_name[32];   // 录入用户姓名(32字节)
    uint8_t s_face_dir;      // 用户需要录入的人脸方向，具体参数参见"人脸方向定义"
    uint8_t timeout;         // 录入超时时间(单位:秒)
} FACE_EnrollParams;





/* 函数声明 */
void FACE_Init(void);                                       /* 初始化人脸识别模块 */
FACE_StatusTypeDef FACE_Register(void);          /* 注册人脸 */
void FACE_IRQ_Callback(void);                               /* UART中断回调函数 */
void FACE_CreateTask(void);                                 /* 创建人脸识别任务 */
void FACE_RxCpltCallback(void);                             /* 接收完成回调函数 */
void FACE_Register_Cmd(void);                               /* 注册人脸命令 */
void FACE_Identify_Cmd(void);                               /* 人脸识别命令 */
#endif /* FACE_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __FACE_H */