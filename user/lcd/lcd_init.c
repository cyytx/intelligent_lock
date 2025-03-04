/**
  ******************************************************************************
  * @file    lcd.c
  * @author  cyytx
  * @brief   LCD显示模块的源文件,实现LCD的初始化、显示、清屏等功能
  ******************************************************************************
  */
#include "lcd_init.h"
#include "priorities.h"

#if LCD_ENABLE

SPI_HandleTypeDef hspi2;  // LCD使用的SPI句柄
DMA_HandleTypeDef hdma_spi_tx;  // DMA句柄

volatile uint8_t g_dma_transfer_complete = 0;  // DMA传输完成标志



static void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
    //  引脚对应
    // LCD_RES------PD4
    // LCD_DC-------PD3
    GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4;//DC和RES
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_3|GPIO_PIN_4,GPIO_PIN_SET);

    // SPI2_NSS-----PI0
    // LCD_BLK------PI2
    //GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2;
    GPIO_InitStruct.Pin = GPIO_PIN_2;;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOI,GPIO_PIN_2,GPIO_PIN_SET);
    printf("spi2 gpio init success\r\n");
}


/**
  * @brief SPI2 初始化函数 - 用于LCD通信
  * @param None
  * @retval None
  */
static void LCD_SPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_SPI2_CLK_ENABLE();

    //SPI2初始化
    hspi2.Instance = SPI2;                                         // 选择SPI2外设
    hspi2.Init.Mode = SPI_MODE_MASTER;                            // 配置为主机模式
    hspi2.Init.Direction = SPI_DIRECTION_1LINE;                    // 单线模式(仅发送)
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;                      // 8位数据帧
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;                   // 时钟极性:空闲时为低电平
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;                        // 时钟相位:第1个边沿采样
    hspi2.Init.NSS = SPI_NSS_HARD_OUTPUT;                               // 软件控制片选
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;      // 波特率预分频值:2分频
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;                      // 高位在前
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;                      // 禁用TI模式
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;      // 禁用CRC校验
    // hspi2.Init.CRCPolynomial = 7;                                // CRC多项式(未使用)
    // hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;              // CRC长度(未使用)
     hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;                 // 禁用NSS脉冲模式
    
    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }
    printf("spi2 init success\r\n");
}




// DMA初始化函数
void LCD_DMA_Init(void)
{
    // 使能DMA1时钟s
    __HAL_RCC_DMA1_CLK_ENABLE();

    // 配置DMA
    hdma_spi_tx.Instance = DMA1_Stream4;                    // 使用DMA1 Stream4 (SPI2_TX)
    hdma_spi_tx.Init.Channel = DMA_CHANNEL_0;              // SPI2_TX使用通道0
    hdma_spi_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;     // 存储器到外设
    hdma_spi_tx.Init.PeriphInc = DMA_PINC_DISABLE;        // 外设地址不增加
    hdma_spi_tx.Init.MemInc = DMA_MINC_ENABLE;            // 存储器地址增加
    hdma_spi_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE; // 外设数据宽度为字节
    hdma_spi_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;    // 存储器数据宽度为字节
    hdma_spi_tx.Init.Mode = DMA_NORMAL;                   // 普通模式，单次传输
    hdma_spi_tx.Init.Priority = DMA_PRIORITY_HIGH;        // 高优先级
    hdma_spi_tx.Init.FIFOMode = DMA_FIFOMODE_ENABLE;    // 使用FIFO
    /*#NOTE如果禁用FIFO,需要注意，在HAL_DMA_Start_IT中关闭FIFO错误中断，也就是要注释掉
    hdma->Instance->FCR |= DMA_IT_FE; 这句，否则每次传输都会触发FIFO错误中断*/
    //hdma_spi_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE; 
    hdma_spi_tx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL; // 触发传输的FIFO阈值半满，也就是8个字节
     hdma_spi_tx.Init.MemBurst = DMA_MBURST_INC4;       // 内存突发传输 单次传输
     hdma_spi_tx.Init.PeriphBurst = DMA_PBURST_SINGLE;    // 外设突发传输 单次传输


    HAL_DMA_Init(&hdma_spi_tx);

    __HAL_LINKDMA(&hspi2, hdmatx, hdma_spi_tx);
    
    // 配置DMA中断
     HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, LCD_IRQ_PRIORITY_DMA_SPI2, 0);  // 将优先级改为定义的值
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
   
}

/**
 * @brief 打印DMA错误信息
 * @param hspi: SPI句柄
 */
void Print_DMA_Error(SPI_HandleTypeDef *hspi)
{
    // 检查SPI错误标志
    if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_MODF)) {
        printf("SPI Mode Fault Error\r\n");
    }
    if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_OVR)) {
        printf("SPI Overrun Error\r\n");
    }
    if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_FRE)) {
        printf("SPI Frame Format Error\r\n");
    }
    if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_CRCERR)) {
        printf("SPI CRC Error\r\n");
    }

    // 检查DMA错误标志
    DMA_HandleTypeDef *hdma = hspi->hdmatx; // 获取DMA句柄
    if (hdma->ErrorCode != HAL_DMA_ERROR_NONE) {
        if (hdma->ErrorCode & HAL_DMA_ERROR_TE) {
            printf("DMA Transfer Error\r\n");
        }
        if (hdma->ErrorCode & HAL_DMA_ERROR_FE) {
            printf("DMA FIFO Error\r\n");
           //__HAL_DMA_DISABLE_IT(&hdma_spi_tx, DMA_IT_FE);  // 关闭FIFO错误中断[[1]][[7]]

        }
        if (hdma->ErrorCode & HAL_DMA_ERROR_DME) {
            printf("DMA Direct Mode Error\r\n");
        }
        if (hdma->ErrorCode & HAL_DMA_ERROR_TIMEOUT) {
            printf("DMA Timeout Error\r\n");
        }
        if (hdma->ErrorCode & HAL_DMA_ERROR_NO_XFER) {
            printf("DMA No Transfer Error\r\n");
        }
    }

    // 清除错误标志
    __HAL_SPI_CLEAR_OVRFLAG(hspi); // 清除过载标志
    __HAL_SPI_CLEAR_FREFLAG(hspi); // 清除帧格式错误标志
    __HAL_DMA_CLEAR_FLAG(hdma, __HAL_DMA_GET_TE_FLAG_INDEX(hdma)); // 清除传输错误标志
}

/**
 * @brief DMA错误回调函数
 * 思路：在DMA传输发生错误时，此函数会被调用，可以在此处理错误。
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2) // 确保是SPI2触发的回调
    {
        printf("SPI2 DMA Error Detected!\r\n");
        Print_DMA_Error(hspi); // 打印详细的错误信息
    }
}
// DMA中断服务函数
void DMA1_Stream4_IRQHandler(void)
{
    //printf("DMA1_Stream4_IRQHandler\r\n");
    HAL_DMA_IRQHandler(&hdma_spi_tx); 
} 

/**
 * @brief  SPI写入一个字节到LCD
 * @param  TxData: 要发送的字节
 * @retval HAL_OK: 发送成功
 *         HAL_ERROR: 发送失败
 */
uint8_t LCD_SPI_Write(uint8_t TxData)
{    
    if(HAL_SPI_GetState(&hspi2) == HAL_SPI_STATE_READY)
    {
        if(HAL_SPI_Transmit(&hspi2, &TxData, 1, 100) == HAL_OK)
        {
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}

/******************************************************************************
      函数说明：LCD串行数据写入函数
      入口数据：dat  要写入的串行数据
      返回值：  无
******************************************************************************/
void LCD_Writ_Bus(uint8_t dat) 
{	
    //LCD_CS_Clr();
    LCD_SPI_Write(dat);
    // LCD_CS_Set();
}

/******************************************************************************
      函数说明：LCD写入数据
      入口数据：dat 写入的数据
      返回值：  无
******************************************************************************/
void LCD_WR_DATA8(uint8_t dat)
{
    LCD_Writ_Bus(dat);
}

/******************************************************************************
      函数说明：LCD写入数据
      入口数据：dat 写入的数据
      返回值：  无
******************************************************************************/
void LCD_WR_DATA(uint16_t dat)
{
    LCD_Writ_Bus(dat>>8);
    LCD_Writ_Bus(dat);
}

/******************************************************************************
      函数说明：LCD写入命令
      入口数据：dat 写入的命令
      返回值：  无
******************************************************************************/
void LCD_WR_REG(uint8_t dat)
{
    LCD_DC_Clr();//写命令
    LCD_Writ_Bus(dat);
    LCD_DC_Set();//写数据
}

/******************************************************************************
      函数说明：设置起始和结束地址
      入口数据：x1,x2 设置列的起始和结束地址
                y1,y2 设置行的起始和结束地址
      返回值：  无
******************************************************************************/
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)
{
    LCD_WR_REG(0x2a);//列地址设置
    LCD_WR_DATA(x1);
    LCD_WR_DATA(x2);
    LCD_WR_REG(0x2b);//行地址设置
    LCD_WR_DATA(y1);
    LCD_WR_DATA(y2);
    LCD_WR_REG(0x2c);//储存器写
}

void LCD_Init(void)
{
    LCD_GPIO_Init();//初始化GPIO
    LCD_SPI_Init();
    LCD_DMA_Init();
    
    LCD_RES_Clr();//复位
    HAL_Delay(100);
    LCD_RES_Set();
    HAL_Delay(100);
    
    LCD_BLK_Set();//打开背光
    HAL_Delay(100);
    
    //************* Start Initial Sequence **********//
    LCD_WR_REG(0x11); //Sleep out 
    HAL_Delay(120);              //Delay 120ms 
    //************* Start Initial Sequence **********// 
    LCD_WR_REG(0xCF);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0xC1);
    LCD_WR_DATA8(0X30);
    LCD_WR_REG(0xED);
    LCD_WR_DATA8(0x64);
    LCD_WR_DATA8(0x03);
    LCD_WR_DATA8(0X12);
    LCD_WR_DATA8(0X81);
    LCD_WR_REG(0xE8);
    LCD_WR_DATA8(0x85);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x79);
    LCD_WR_REG(0xCB);
    LCD_WR_DATA8(0x39);
    LCD_WR_DATA8(0x2C);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x34);
    LCD_WR_DATA8(0x02);
    LCD_WR_REG(0xF7);
    LCD_WR_DATA8(0x20);
    LCD_WR_REG(0xEA);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xC0); //Power control
    LCD_WR_DATA8(0x1D); //VRH[5:0]
    LCD_WR_REG(0xC1); //Power control
    LCD_WR_DATA8(0x12); //SAP[2:0];BT[3:0]
    LCD_WR_REG(0xC5); //VCM control
    LCD_WR_DATA8(0x33);
    LCD_WR_DATA8(0x3F);
    LCD_WR_REG(0xC7); //VCM control
    LCD_WR_DATA8(0x92);
    LCD_WR_REG(0x3A); // Memory Access Control
    LCD_WR_DATA8(0x55);
    LCD_WR_REG(0x36); // Memory Access Control
    if(USE_HORIZONTAL==0)LCD_WR_DATA8(0x08);
    else if(USE_HORIZONTAL==1)LCD_WR_DATA8(0xC8);
    else if(USE_HORIZONTAL==2)LCD_WR_DATA8(0x78);
    else LCD_WR_DATA8(0xA8);
    LCD_WR_REG(0xB1);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x12);
    LCD_WR_REG(0xB6); // Display Function Control
    LCD_WR_DATA8(0x0A);
    LCD_WR_DATA8(0xA2);

    LCD_WR_REG(0x44);
    LCD_WR_DATA8(0x02);

    LCD_WR_REG(0xF2); // 3Gamma Function Disable
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0x26); //Gamma curve selected
    LCD_WR_DATA8(0x01);
    LCD_WR_REG(0xE0); //Set Gamma
    LCD_WR_DATA8(0x0F);
    LCD_WR_DATA8(0x22);
    LCD_WR_DATA8(0x1C);
    LCD_WR_DATA8(0x1B);
    LCD_WR_DATA8(0x08);
    LCD_WR_DATA8(0x0F);
    LCD_WR_DATA8(0x48);
    LCD_WR_DATA8(0xB8);
    LCD_WR_DATA8(0x34);
    LCD_WR_DATA8(0x05);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x09);
    LCD_WR_DATA8(0x0F);
    LCD_WR_DATA8(0x07);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0XE1); //Set Gamma
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x23);
    LCD_WR_DATA8(0x24);
    LCD_WR_DATA8(0x07);
    LCD_WR_DATA8(0x10);
    LCD_WR_DATA8(0x07);
    LCD_WR_DATA8(0x38);
    LCD_WR_DATA8(0x47);
    LCD_WR_DATA8(0x4B);
    LCD_WR_DATA8(0x0A);
    LCD_WR_DATA8(0x13);
    LCD_WR_DATA8(0x06);
    LCD_WR_DATA8(0x30);
    LCD_WR_DATA8(0x38);
    LCD_WR_DATA8(0x0F);
    LCD_WR_REG(0x29); //Display on
    printf("lcd init success\r\n");
}

#endif
