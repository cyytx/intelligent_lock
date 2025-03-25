/**
  ******************************************************************************
  * @file    keyboard.c
  * @author  cyytx
  * @brief   键盘模块的源文件,实现键盘的初始化、扫描等功能
  ******************************************************************************
  */
#include "stdio.h"
#include "cmsis_os.h"
#include <string.h>
#include "stm32f7xx_hal.h"
#include "key.h"
#include "lcd.h"
#include "beep.h"
#include "sg90.h"
#include "ble.h"
#include "fingerprint.h"
#include "priorities.h"
#include "face.h"

#if KEY_ENABLE

// 使用最后一个扇区(扇区7)存储密码
#define FLASH_SECTOR_PASSWORD    FLASH_SECTOR_7      // 使用扇区7存储密码
#define FLASH_PASSWORD_ADDR      0x080C0000          // 扇区7的起始地址

/* 定义行引脚和列引脚 */
static const uint16_t KEY_ROW_PINS[] = {KEY_R1_Pin, KEY_R2_Pin, KEY_R3_Pin};
static const GPIO_TypeDef* KEY_ROW_PORTS[] = {KEY_R1_GPIO_Port, KEY_R2_GPIO_Port, KEY_R3_GPIO_Port};
static const uint16_t KEY_COL_PINS[] = {KEY_C1_Pin, KEY_C2_Pin, KEY_C3_Pin, KEY_C4_Pin};
static const GPIO_TypeDef* KEY_COL_PORTS[] = {KEY_C1_GPIO_Port, KEY_C2_GPIO_Port, KEY_C3_GPIO_Port, KEY_C4_GPIO_Port};

/* 按键映射表 */
static const KeyValue_t KEY_MAP[3][4] = {
    {KEY_1, KEY_2, KEY_3, KEY_4},
    {KEY_5, KEY_6, KEY_7, KEY_8},
    {KEY_9, KEY_0, KEY_ENTER,KEY_CANCEL}
};

LockPassword_t lock_passWard; // 门锁密码
static uint8_t input_password[16];    // 输入密码缓存
static uint8_t input_password_len = 0; // 输入密码长度
static uint32_t last_key_time = 0;     // 上次按键时间
static uint8_t enter_press_count = 0;  // ENTER键连续按下次数，连续按下3次KEY_ENTHER进入密码设置模式
static uint8_t setting_password_mode = 0; // 是否处于密码设置模式

static uint8_t scanning_flag = 0; // 扫描标志,为1时正在扫描
static uint8_t row_pressed = 0; // 行按键，标志哪个行按键被按下，减少扫描次数

// 键盘任务句柄
static osThreadId_t keyboardTaskHandle;
static const osThreadAttr_t keyboard_attributes = {
    .name = "keyboardTask",
    .stack_size = STACK_SIZE_KEYBOARD,
    .priority = (osPriority_t) TASK_PRIORITY_KEYBOARD,
};

void ReadPassWardFromFlash(void);
/**
  * @brief  键盘模块初始化
  * @param  None
  * @retval None
  */
void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能GPIO时钟
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // 配置列引脚为推挽输出+低电平
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    for(uint8_t i = 0; i < 4; i++) {
        GPIO_InitStruct.Pin = KEY_COL_PINS[i];
        HAL_GPIO_Init((GPIO_TypeDef*)KEY_COL_PORTS[i], &GPIO_InitStruct);
        HAL_GPIO_WritePin((GPIO_TypeDef*)KEY_COL_PORTS[i], KEY_COL_PINS[i], GPIO_PIN_RESET);//初始化时，列引脚为低电平
    }
    
    // 配置行引脚为上拉输入+中断
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;//下降沿触发
    GPIO_InitStruct.Pull = GPIO_PULLUP;//上拉
    

    for(uint8_t i = 0; i < 3; i++) {
        GPIO_InitStruct.Pin = KEY_ROW_PINS[i];
        HAL_GPIO_Init((GPIO_TypeDef*)KEY_ROW_PORTS[i], &GPIO_InitStruct);
    }
    
    /* EXTI中断初始化 */
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_SetPriority(EXTI1_IRQn, KEY_IRQ_PRIORITY_EXTI, 0); //设置中断优先级要低于或等于 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    HAL_NVIC_SetPriority(EXTI2_IRQn, KEY_IRQ_PRIORITY_EXTI, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
    HAL_NVIC_SetPriority(EXTI3_IRQn, KEY_IRQ_PRIORITY_EXTI, 0);

    //KEY_CreateTask();
    //读取密码
    ReadPassWardFromFlash();

}

/**
  * @brief  键盘扫描函数
  * @param  None
  * @retval KeyValue_t: 按键值
  */
KeyValue_t KEY_Scan(void)
{
    KeyValue_t key = KEY_NONE;
    uint8_t row, col;

    scanning_flag = 1;

    // 扫描列
    for(col = 0; col < 4; col++) {
        // 将当前列置低，其他列置高
        for(uint8_t i = 0; i < 4; i++) {
            HAL_GPIO_WritePin((GPIO_TypeDef*)KEY_COL_PORTS[i], KEY_COL_PINS[i], 
                            i == col ? GPIO_PIN_RESET : GPIO_PIN_SET);
        }
        //printf("col: %d\r\n", col);
        // 延时等待电平稳定
        osDelay(pdMS_TO_TICKS(1));
        
        // // 检查行状态
        // for(row = 0; row < 3; row++) {
        //     if(HAL_GPIO_ReadPin((GPIO_TypeDef*)KEY_ROW_PORTS[row], KEY_ROW_PINS[row]) == GPIO_PIN_RESET) {
        //         key = KEY_MAP[row][col];
        //         break;
        //     }
        // }
        if(HAL_GPIO_ReadPin((GPIO_TypeDef*)KEY_ROW_PORTS[row_pressed], KEY_ROW_PINS[row_pressed]) == GPIO_PIN_RESET) {
                key = KEY_MAP[row_pressed][col];
                break;
        }

    }
    
    // 恢复所有列为低电平   
    for(uint8_t i = 0; i < 4; i++) {
        HAL_GPIO_WritePin((GPIO_TypeDef*)KEY_COL_PORTS[i], KEY_COL_PINS[i], GPIO_PIN_RESET);
    }

    scanning_flag = 0;
    return key;

}


/**
  * @brief  键盘中断处理函数
  * @param  GPIO_Pin: 触发中断的引脚
  * @retval None
  */
void KEY_IRQHandler(uint16_t GPIO_Pin)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //printf("irq handler\r\n");
    if(scanning_flag == 0) //只有在非扫描期间，才能再次触发扫描
    {
        // 通知键盘扫描任务
        vTaskNotifyGiveFromISR(keyboardTaskHandle, &xHigherPriorityTaskWoken);
    }

    // 如果需要，进行上下文切换
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}


/**
  * @brief  EXTI1 中断处理函数 (ROW3 -> PC1)
  */
void EXTI1_IRQHandler(void)
{
    if(__HAL_GPIO_EXTI_GET_IT(KEY_R3_Pin) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(KEY_R3_Pin);
        row_pressed = 2;
        KEY_IRQHandler(KEY_R3_Pin);
    }
}

/**
  * @brief  EXTI2 中断处理函数 (ROW2 -> PC2)
  */
void EXTI2_IRQHandler(void)
{
    if(__HAL_GPIO_EXTI_GET_IT(KEY_R2_Pin) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(KEY_R2_Pin);
        row_pressed = 1;
        KEY_IRQHandler(KEY_R2_Pin);
    }

}

/**
  * @brief  EXTI3 中断处理函数 (ROW1 -> PC3)
  */
void EXTI3_IRQHandler(void)
{
    if(__HAL_GPIO_EXTI_GET_IT(KEY_R1_Pin) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(KEY_R1_Pin);
        row_pressed = 0;
        KEY_IRQHandler(KEY_R1_Pin);
    }

}


// 从Flash读取门锁密码
void ReadPassWardFromFlash(void)
{
    // 直接从Flash地址读取数据到密码结构体
    memcpy(&lock_passWard, (void*)FLASH_PASSWORD_ADDR, sizeof(LockPassword_t));
    
    // 检查密码是否有效（如果是空Flash，数据会是0xFF）
    if(lock_passWard.password_len == 0xFF) {
        // Flash为空，设置默认密码
        lock_passWard.password_len = 8;
        lock_passWard.password[0] = 1;
        lock_passWard.password[1] = 2;
        lock_passWard.password[2] = 3;
        lock_passWard.password[3] = 4;
        lock_passWard.password[4] = 5;
        lock_passWard.password[5] = 6;
        lock_passWard.password[6] = 7;
        lock_passWard.password[7] = 8;
        printf("Flash is empty, set default password\r\n");
    } else {
        printf("Flash is not empty, read password from flash\r\n");
    }   
}

// 保存门锁密码到Flash
HAL_StatusTypeDef SavePassWardToFlash(void)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t FirstSector = 0, NbOfSectors = 0;
    uint32_t SectorError = 0;
    FLASH_EraseInitTypeDef EraseInitStruct;
    
    // 解锁Flash
    status = HAL_FLASH_Unlock();
    if(status != HAL_OK) {
        return status;
    }
    
    // 清除所有Flash标志
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_ERSERR);
    
    // 设置要擦除的扇区
    FirstSector = FLASH_SECTOR_PASSWORD;
    NbOfSectors = 1;
    
    // 填充擦除结构体
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 2.7V-3.6V
    EraseInitStruct.Sector = FirstSector;
    EraseInitStruct.NbSectors = NbOfSectors;
    
    // 执行擦除操作
    status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
    if(status != HAL_OK) {
        HAL_FLASH_Lock();
        return status;
    }
    
    // 按字节写入数据
    uint8_t *pData = (uint8_t*)&lock_passWard;
    uint32_t address = FLASH_PASSWORD_ADDR;
    
    for(uint32_t i = 0; i < sizeof(LockPassword_t); i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address, pData[i]);
        if(status != HAL_OK) {
            HAL_FLASH_Lock();
            return status;
        }
        address++;
    }
    
    // 锁定Flash
    HAL_FLASH_Lock();
    
    return status;
}

/// 修改密码
HAL_StatusTypeDef ChangePassword(uint8_t* new_password, uint8_t length)
{
    // 检查密码长度是否有效
    if(length == 0 || length > 16) {
        return HAL_ERROR;
    }
    
    // 更新密码
    lock_passWard.password_len = length;
    memcpy(lock_passWard.password, new_password, length);
    
    // 保存到Flash
    return SavePassWardToFlash();
}


// 清除输入密码
void ClearInputPassword(void)
{
    memset(input_password, 0, sizeof(input_password));
    input_password_len = 0;
}

// 验证密码是否匹配
uint8_t ValidatePassword(void)
{
    if (input_password_len != lock_passWard.password_len)
    {
        return 0; // 密码长度不匹配
    }
    
    for (uint8_t i = 0; i < input_password_len; i++)
    {
        if (input_password[i] != lock_passWard.password[i])
        {
            return 0; // 密码内容不匹配
        }
    }
    
    return 1; // 密码匹配
}


/**
  * @brief  键盘任务函数
  * @param  argument: 任务参数
  * @retval None
  */
void KeyboardScanTask(void *pvParameters)
{
    KeyValue_t key = KEY_NONE;
    uint32_t current_time;

    while (1)
    {
        // 等待中断通知，会一直阻塞直到收到通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // 消除抖动
        osDelay(pdMS_TO_TICKS(20));// 等待20ms
        
        // 执行键盘扫描
        key = KEY_Scan();
        
        // 处理按键事件
        if (key != KEY_NONE) {
            printf("Key pressed: %d\r\n", key);
            
            current_time = osKernelGetTickCount();
            
            // 检查按键间隔
            if (last_key_time > 0)
            {
                if (!setting_password_mode && (current_time - last_key_time) > pdMS_TO_TICKS(6000))
                {
                    // 密码输入模式下，两次按键间隔超过6秒，清除已输入密码
                    printf("Input timeout, clearing password\r\n");
                    ClearInputPassword();
                    enter_press_count = 0;
                }
                else if (setting_password_mode && (current_time - last_key_time) > pdMS_TO_TICKS(10000))
                {
                    // 密码设置模式下，10秒内没有按键操作，退出设置模式
                    printf("Setting timeout, exiting setting mode\r\n");
                    setting_password_mode = 0;
                    ClearInputPassword();
                    enter_press_count = 0;
                }
            }
            
            last_key_time = current_time;
            
            // 处理按键
            if (key == KEY_ENTER)
            {
                if (setting_password_mode)
                {
                    // 密码设置模式下处理ENTER
                    if (input_password_len >= 4 && input_password_len <= 16)
                    {
                        // 保存新密码
                        printf("Saving new password...\r\n");
                        ChangePassword(input_password, input_password_len);
                        printf("Password changed successfully!\r\n");
                        setting_password_mode = 0;
                        ClearInputPassword();
                    }
                    else
                    {
                        printf("Invalid password length! Must be 4-16 digits.\r\n");
                    }
                    enter_press_count = 0;
                }
                else if (IsDoorUnlocked() && input_password_len == 0)
                {
                    // 在解锁状态下连续按三次ENTER进入密码设置模式
                    enter_press_count++;
                    printf("Enter pressed %d times\r\n", enter_press_count);
                    
                    if (enter_press_count >= 3)
                    {
                        printf("Entering password setting mode...\r\n");
                        setting_password_mode = 1;
                        enter_press_count = 0;
                    }
                }
                else
                {
                    // 验证输入的密码
                    if (input_password_len > 0)
                    {
                        if (ValidatePassword())
                        {
                            printf("Password correct! Unlocking door.\r\n");
                            SendLockCommand(1); // 发送开锁命令
                            ClearInputPassword();
                        }
                        else
                        {
                            printf("Password incorrect!\r\n");
                            ClearInputPassword();
                        }
                    }
                    enter_press_count = 0;
                }
            }
            else if (key == KEY_CANCEL)//先将取消按键作为红外触发，红外触发在开发期间不好控制。
            {
                // // 取消键清除当前输入并退出密码设置模式
                // printf("Input cancelled\r\n");
                // ClearInputPassword();
                // setting_password_mode = 0;
                // enter_press_count = 0;
                //FP_EnrollTest();
                //FACE_Register_Cmd();
                FACE_Identify_Cmd();
            }
            else if (key >= KEY_1 && key <= KEY_0)
            {
                // 处理数字键输入
                if (input_password_len < 16)
                {
                    uint8_t digit;
                    if (key == KEY_0)
                    {
                        digit = 0;
                    }
                    else
                    {
                        digit = key; // KEY_1 到 KEY_9 分别对应 1 到 9
                    }
                    
                    input_password[input_password_len++] = digit;
                    printf("Input: %d, Length: %d\r\n", digit, input_password_len);
                    enter_press_count = 0;
                }
                else
                {
                    printf("Password too long! Max 16 digits.\r\n");
                }
            }
        }
    }
}

/**
  * @brief  创建键盘任务
  * @param  None
  * @retval None
  */
void KEY_CreateTask(void)
{
    keyboardTaskHandle = osThreadNew(KeyboardScanTask, NULL, &keyboard_attributes);
}

#endif /* KEY_ENABLE */

