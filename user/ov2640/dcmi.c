
#include "stdio.h"
#include "dcmi.h" 
#include "ov2640.h" 
#include "lcd_init.h"
#include "lcd.h"
#include "priorities.h"

extern uint16_t g_dcmi_dma_buf[2][DCMI_BUF_SIZE];
	
DCMI_HandleTypeDef  DCMI_Handler;           //DCMI句柄
DMA_HandleTypeDef   DMADMCI_Handler;        //DMA句柄

// 定义全局计数器
volatile uint8_t dma_transfer_count = 0;

/* JPEG尺寸支持列表 */
const uint16_t jpeg_img_size_tbl[][2] =
{
    160, 120,       /* QQVGA */
    176, 144,       /* QCIF */
    320, 240,       /* QVGA */
    400,240,        /* WGVGA */
    352,288,        /* CIF */
    640, 480,       /* VGA */
    800, 600,       /* SVGA */
    1024, 768,      /* XGA */
    1280, 800,      /* WXGA */
    1280, 960,      /* XVGA */
    1440, 900,      /* WXGA+ */
    1280, 1024,     /* SXGA */
    1600, 1200,     /* UXGA */
};

//DCMI初始化
void DCMI_Init(void)
{
    DCMI_Handler.Instance=DCMI;
    DCMI_Handler.Init.SynchroMode=DCMI_SYNCHRO_HARDWARE;    //硬件同步HSYNC,VSYNC
    DCMI_Handler.Init.PCKPolarity=DCMI_PCKPOLARITY_RISING;  //PCLK 上升沿有效
    DCMI_Handler.Init.VSPolarity=DCMI_VSPOLARITY_LOW;       //VSYNC 低电平有效
    DCMI_Handler.Init.HSPolarity=DCMI_HSPOLARITY_LOW;       //HSYNC 低电平有效
    DCMI_Handler.Init.CaptureRate=DCMI_CR_ALL_FRAME;        //全帧捕获
    DCMI_Handler.Init.ExtendedDataMode=DCMI_EXTEND_DATA_8B; //8位数据格式 
    HAL_DCMI_Init(&DCMI_Handler);                           //初始化DCMI

    // 先禁用所有中断
    __HAL_DCMI_DISABLE_IT(&DCMI_Handler, DCMI_IT_FRAME|DCMI_IT_OVR|DCMI_IT_ERR|DCMI_IT_VSYNC|DCMI_IT_LINE);
    
    // 清除所有中断标志
    __HAL_DCMI_CLEAR_FLAG(&DCMI_Handler, DCMI_FLAG_FRAMERI|DCMI_FLAG_OVFRI|DCMI_FLAG_ERRRI|DCMI_FLAG_VSYNCRI|DCMI_FLAG_LINERI);
    
    // 只使能需要的中断
    __HAL_DCMI_ENABLE_IT(&DCMI_Handler,DCMI_IT_FRAME|DCMI_IT_OVR|DCMI_IT_ERR);
    
    __HAL_DCMI_ENABLE(&DCMI_Handler);                       //使能DCMI
    
    
    HAL_NVIC_SetPriority(DCMI_IRQn, OV2640_IRQ_PRIORITY_DCMI, 0);       //设置中断优先级
    HAL_NVIC_EnableIRQ(DCMI_IRQn);
    printf("DCMI IER: 0x%x\r\n", DCMI->IER); // 打印中断使能寄存器的值
}



/**
* @brief DCMI MSP Initialization
* This function configures the hardware resources used in this example
* @param hdcmi: DCMI handle pointer
* @retval None
*/
void HAL_DCMI_MspInit(DCMI_HandleTypeDef* hdcmi)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hdcmi->Instance==DCMI)
  {
  /* USER CODE BEGIN DCMI_MspInit 0 */

  /* USER CODE END DCMI_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_DCMI_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    /**DCMI GPIO Configuration
    PA6     ------> DCMI_PIXCLK
    PH8     ------> DCMI_HSYNC
    PH9     ------> DCMI_D0
    PH10     ------> DCMI_D1
    PH11     ------> DCMI_D2
    PH12     ------> DCMI_D3
    PH14     ------> DCMI_D4
    PI4     ------> DCMI_D5
    PI5     ------> DCMI_VSYNC
    PI6     ------> DCMI_D6
    PI7     ------> DCMI_D7
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

    // HAL_NVIC_SetPriority(DCMI_IRQn,6,0);       //抢占优先级2，子优先级2
    // HAL_NVIC_EnableIRQ(DCMI_IRQn);             //使能DCMI中断

  }

}

//DCMI DMA配置
//mem0addr:存储器地址0  将要存储摄像头数据的内存地址(也可以是外设地址)
//mem1addr:存储器地址1  当只使用mem0addr的时候,该值必须为0
//memsize:存储器大小,如果是一帧的话就是rgb16bit 则大小设置为IMG_WIDTH*IMG_HEIGHT在DMA_MDATAALIGN_HALFWORD时
//memblen:存储器位宽,可以为:DMA_MDATAALIGN_BYTE/DMA_MDATAALIGN_HALFWORD/DMA_MDATAALIGN_WORD
//meminc:存储器增长方式,可以为:DMA_MINC_ENABLE/DMA_MINC_DISABLE
void DCMI_DMA_Init(uint32_t mem0addr,uint32_t mem1addr,uint16_t memsize,uint32_t memblen,uint32_t meminc)
{ 
    __HAL_RCC_DMA2_CLK_ENABLE();                                    //使能DMA2时钟
    __HAL_LINKDMA(&DCMI_Handler,DMA_Handle,DMADMCI_Handler);        //将DMA与DCMI联系起来
	__HAL_DMA_DISABLE_IT(&DMADMCI_Handler,DMA_IT_TC);    			//先关闭DMA传输完成中断(否则在使用MCU屏的时候会出现花屏的情况)
	
    DMADMCI_Handler.Instance=DMA2_Stream1;                          //DMA2数据流1                     
    DMADMCI_Handler.Init.Channel=DMA_CHANNEL_1;                     //通道1
    DMADMCI_Handler.Init.Direction=DMA_PERIPH_TO_MEMORY;            //外设到存储器
    DMADMCI_Handler.Init.PeriphInc=DMA_PINC_DISABLE;                //外设非增量模式
    DMADMCI_Handler.Init.MemInc=meminc;                   			//存储器增量模式
    DMADMCI_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_WORD;   //外设数据长度:32位
    DMADMCI_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;     				//存储器数据长度:8/16/32位
    DMADMCI_Handler.Init.Mode=DMA_CIRCULAR;                         //使用循环模式 
    DMADMCI_Handler.Init.Priority=DMA_PRIORITY_HIGH;                //高优先级
    DMADMCI_Handler.Init.FIFOMode=DMA_FIFOMODE_ENABLE;              //使能FIFO
    DMADMCI_Handler.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_HALFFULL; //使用1/2的FIFO 
    DMADMCI_Handler.Init.MemBurst=DMA_MBURST_SINGLE;                //存储器突发传输
    DMADMCI_Handler.Init.PeriphBurst=DMA_PBURST_SINGLE;             //外设突发单次传输 
    HAL_DMA_DeInit(&DMADMCI_Handler);                               //先清除以前的设置
    HAL_DMA_Init(&DMADMCI_Handler);	                                //初始化DMA
    
    //在开启DMA之前先使用__HAL_UNLOCK()解锁一次DMA,因为HAL_DMA_Statrt()HAL_DMAEx_MultiBufferStart()
    //这两个函数一开始要先使用__HAL_LOCK()锁定DMA,而函数__HAL_LOCK()会判断当前的DMA状态是否为锁定状态，如果是
    //锁定状态的话就直接返回HAL_BUSY，这样会导致函数HAL_DMA_Statrt()和HAL_DMAEx_MultiBufferStart()后续的DMA配置
    //程序直接被跳过！DMA也就不能正常工作，为了避免这种现象，所以在启动DMA之前先调用__HAL_UNLOC()先解锁一次DMA。
    __HAL_UNLOCK(&DMADMCI_Handler);
    if(mem1addr==0)    //开启DMA，不使用双缓冲
    {
        HAL_DMA_Start(&DMADMCI_Handler,(uint32_t)&DCMI->DR,mem0addr,memsize);
    }
    else                //使用双缓冲
    {
        HAL_DMAEx_MultiBufferStart(&DMADMCI_Handler,(uint32_t)&DCMI->DR,mem0addr,mem1addr,memsize);//开启双缓冲
        __HAL_DMA_ENABLE_IT(&DMADMCI_Handler,DMA_IT_TC|DMA_IT_TE);    //开启传输完成中断
       //DMA中断优先级要高于DCMI中断优先级，因为传输完成时
        HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, OV2640_IRQ_PRIORITY_DMA_DCMI, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
        
    }    
}

//DCMI,启动传输 
void DCMI_Start(void)
{  
    LCD_Address_Set(0,0,LCD_W-1,LCD_H-1);  // 假设是320x240分辨率，根据实际情况调整
    __HAL_DMA_ENABLE(&DMADMCI_Handler); //使能DMA
    DCMI->CR|=DCMI_CR_CAPTURE;          //DCMI捕获使能
}

//DCMI,关闭传输
void DCMI_Stop(void)
{ 
    DCMI->CR&=~(DCMI_CR_CAPTURE);       //关闭捕获
    while(DCMI->CR&0X01);               //等待传输完成
    __HAL_DMA_DISABLE(&DMADMCI_Handler);//关闭DMA
} 

// //RGB屏数据接收回调函数
// void rgblcd_dcmi_rx_callback(void)
// {  
// 	u16 *pbuf;
// 	if(DMA2_Stream1->CR&(1<<19))//DMA使用buf1,读取buf0
// 	{ 
// 		pbuf=(u16*)dcmi_line_buf[0]; 
// 	}else 						//DMA使用buf0,读取buf1
// 	{
// 		pbuf=(u16*)dcmi_line_buf[1]; 
// 	} 	
// 	LTDC_Color_Fill(0,curline,lcddev.width-1,curline,pbuf);//DM2D填充 
// 	if(curline<lcddev.height)curline++;
// }
//DCMI中断服务函数
void DCMI_IRQHandler(void)
{
    uint32_t isr = DCMI->MISR; // 获取DCMI中断状态寄存器值
    
    // printf("DCMI ISR: 0x%x ", isr);
    // if(isr & DCMI_MIS_FRAME_MIS) printf("FRAME ");
    // if(isr & DCMI_MIS_OVR_MIS) printf("OVR ");
    // if(isr & DCMI_MIS_ERR_MIS) printf("ERR ");
    // if(isr & DCMI_MIS_VSYNC_MIS) printf("VSYNC ");
    // if(isr & DCMI_MIS_LINE_MIS) printf("LINE ");
    // printf("\r\n");
    
    HAL_DCMI_IRQHandler(&DCMI_Handler);
}

/**
 * @brief       DCMI中断回调服务函数
 * @param       hdcmi:DCMI句柄
 * @note        捕获到一帧图像处理
 * @retval      无
 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
   
    //printf("2\r\n");
    //printf("Frame Complete: 0x%x\r\n", DCMI->CR);
    //重新使能帧中断,因为HAL_DCMI_IRQHandler()函数会关闭帧中断
    dma_transfer_count = 0;
    //printf("0x%d\r\n", dma_transfer_count);
    __HAL_DCMI_ENABLE_IT(&DCMI_Handler,DCMI_IT_FRAME);
}

// void HAL_DCMI_VsyncEventCallback(DCMI_HandleTypeDef *hdcmi)
// {
//     printf("VSYNC Event\r\n");
// }

void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi)
{
    printf("DCMI Error: 0x%x\r\n", hdcmi->ErrorCode);
}

//DMA2数据流1中断服务函数
void DMA2_Stream1_IRQHandler(void)
{
    //printf("3\r\n");
    /* #NOTE DMA_FLAG_TCIF1_5的意思是 stream1 和stream5的传输完成标志位,stream1的在LIFCR
     stream5的在HIFCR 它所在的bit位置是一样的，那怎么区分是1还是5呢，用 DMADMCI_Handler里的值
     去区分。*/

    if(__HAL_DMA_GET_FLAG(&DMADMCI_Handler,DMA_FLAG_TCIF1_5)!=RESET) // DMA传输完成
    {
        __HAL_DMA_CLEAR_FLAG(&DMADMCI_Handler,DMA_FLAG_TCIF1_5); // 清除DMA传输完成中断标志位
        
        // 判断当前使用的缓冲区
        //uint8_t current_buffer = (DMA2_Stream1->CR & DMA_SxCR_CT) ? 1 : 0;
        uint8_t current_buffer = (DMA2_Stream1->CR & DMA_SxCR_CT) ? 0 : 1;
        
        if(dma_transfer_count > 3)
        {
            return;
        }
        // 根据计数器值设置显示位置
        //printf("0x%d\r\n", dma_transfer_count);
        uint16_t x = 0;
        uint16_t y = dma_transfer_count *80;
        uint16_t width = LCD_W;
        uint16_t height = 80;
        // 更新计数器
        dma_transfer_count++;
        // #BUG 目前首先存在DMA大小端和LCD 大小端不同，第二可能还有MSB或LSB不同问题，因为jpeg格式在
        //电脑看也是偏色，需要将buffer,提取出来在电脑做各种转换看是哪种问题，哪一个是正确的。
        LCD_ShowPicture_Async(x, y, width, height, (uint8_t *)g_dcmi_dma_buf[current_buffer]+1);
  
    }
}


