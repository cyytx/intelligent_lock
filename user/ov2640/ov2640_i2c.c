
#include "ov2640_i2c.h"

  
I2C_HandleTypeDef I2C_Handle;					
/*******************************  Function ************************************/

/**
  * @brief  初始化I2C总线，使用I2C前需要调用
  * @param  无
  * @retval 无
  */
void I2CMaster_Init(void) 
{
	
	if(HAL_I2C_GetState(&I2C_Handle) == HAL_I2C_STATE_RESET)
	{	
		/* 强制复位I2C外设时钟 */  
		SENSORS_I2C_FORCE_RESET(); 

		/* 释放I2C外设时钟复位 */  
		SENSORS_I2C_RELEASE_RESET(); 
		
		/* I2C 配置  APB1时钟为48MHz*/
		I2C_Handle.Instance = I2C1;
		I2C_Handle.Init.Timing           = 0x60201E2B;//100KHz
        //I2C_Handle.Init.Timing           = 0x60255556;//40KHz
		I2C_Handle.Init.OwnAddress1      = 0;
		I2C_Handle.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
		I2C_Handle.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
		I2C_Handle.Init.OwnAddress2      = 0;
		I2C_Handle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
		I2C_Handle.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
		I2C_Handle.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

		/* 初始化I2C */
		HAL_I2C_Init(&I2C_Handle);	
		/* 使能模拟滤波器 */
		HAL_I2CEx_AnalogFilter_Config(&I2C_Handle, I2C_ANALOGFILTER_ENABLE); 
	}
}
/**
  * @brief  Manages error callback by re-initializing I2C.
  * @param  Addr: I2C Address
  * @retval None
  */
static void I2Cx_Error(void)
{
	/* 恢复I2C寄存器为默认值 */
	HAL_I2C_DeInit(&I2C_Handle); 
	/* 重新初始化I2C外设 */
	I2CMaster_Init();
}
/**
  * @brief  写一字节数据到OV2640寄存器
  * @param  Addr: OV2640 的寄存器地址
  * @param  Data: 要写入的数据
  * @retval 返回0表示写入正常，0xFF表示错误
  */
uint8_t ov2640_write_reg(uint16_t Addr, uint8_t Data)
{
//    I2Cx_WriteMultiple(&I2C_Handle, OV2640_DEVICE_WRITE_ADDRESS, (uint16_t)Addr, I2C_MEMADD_SIZE_8BIT,(uint8_t*)&Data, 1);
  HAL_StatusTypeDef status = HAL_OK;
  
  status = HAL_I2C_Mem_Write(&I2C_Handle, OV2640_DEVICE_ADDRESS, (uint16_t)Addr, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&Data, 1, 1000);
  
  /* Check the communication status */
  if(status != HAL_OK)
  {
    /* Re-Initiaize the I2C Bus */
    I2Cx_Error();
  }
  return status;
}

/**
  * @brief  从OV2640寄存器中读取一个字节的数据
  * @param  Addr: 寄存器地址
  * @retval 返回读取得的数据
  */
uint8_t ov2640_read_reg(uint16_t Addr)
{
    uint8_t Data = 0;
//    I2Cx_ReadMultiple(&I2C_Handle, OV2640_DEVICE_READ_ADDRESS, Addr, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&Data, 1);
    
  HAL_StatusTypeDef status = HAL_OK;

  status = HAL_I2C_Mem_Read(&I2C_Handle, OV2640_DEVICE_ADDRESS, (uint16_t)Addr, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&Data, 1, 1000);

  /* Check the communication status */
  if(status != HAL_OK)
  {
    /* I2C error occurred */
    I2Cx_Error();
  }
  /* return the read data */
    return Data;
}
