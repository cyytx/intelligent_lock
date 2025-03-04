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

#if FACE_ENABLE

void FACE_Init(void);
void FACE_Register(void);
void FACE_Recognize(void);

#endif /* FACE_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* __FACE_H */ 