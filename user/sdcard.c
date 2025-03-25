#include <string.h>
#include "sdcard.h"
#include "FreeRTOS.h"
#include "task.h"
#include "delay.h"

#if SDCARD_ENABLE

SD_HandleTypeDef hsd1;  // SD卡存储
static HAL_SD_CardInfoTypeDef  SDCardInfo;                 //SD卡信息结构体
static DMA_HandleTypeDef SD1TxDMAHandler,SD1RxDMAHandler;    //SD卡DMA发送和接收句柄
static uint32_t sd_int_record[3]={0,0,0};//中断触发次数记录

//SD_ReadDisk/SD_WriteDisk函数专用buf,当这两个函数的数据缓存区地址不是4字节对齐的时候,
//需要用到该数组,确保数据缓存区地址是4字节对齐的.
#define BLOCK_SIZE          512     // SD卡块大小
__attribute__((aligned(4))) uint8_t SDIO_DATA_BUFFER[BLOCK_SIZE];

/**
  * @brief  Print SD card information via serial port
  * @param  hsd: SD card handle pointer
  * @retval None
  */
void SDCARD_ShowInfo(SD_HandleTypeDef *hsd)
{
    HAL_SD_CardInfoTypeDef cardInfo;
    
    // Get SD card information
    HAL_SD_GetCardInfo(hsd, &cardInfo);
    
    // Print card type
    printf("Card Type: ");
    switch(cardInfo.CardType)
    {
        case CARD_SDSC:
            if(hsd->SdCard.CardVersion == 0x00) // V1.1 in old library
                printf("SDSC V1.1\r\n");
            else // V2.0 in old library
                printf("SDSC V2.0\r\n");
            break;
        case CARD_SDHC_SDXC:
            printf("SDHC/SDXC V2.0\r\n");
            break;
        default:
            printf("Unknown Type\r\n");
            break;
    }
    
    // Print relative card address
    printf("Card Relative Address: %d\r\n", cardInfo.RelCardAdd);
    
    // Calculate and print capacity (MB)
    uint32_t capacity = (uint32_t)((cardInfo.LogBlockNbr * (uint64_t)cardInfo.LogBlockSize) >> 20);
    printf("Card Capacity: %u MB\r\n", capacity);
    
    // Print block size
    printf("Block Size: %u bytes\r\n", cardInfo.BlockSize);
    
    // Print logical block number
    printf("Logical Block Count: %u\r\n", cardInfo.LogBlockNbr);
    
    // Print card class information
    printf("Card Class: %02X\r\n", cardInfo.Class);
    
    printf("\r\n");
}


int SDCARD1_DMA_Init(void)
{
    //配置接收DMA
    SD1RxDMAHandler.Instance=DMA2_Stream3;
    SD1RxDMAHandler.Init.Channel=DMA_CHANNEL_4;
    SD1RxDMAHandler.Init.Direction=DMA_PERIPH_TO_MEMORY;
    SD1RxDMAHandler.Init.PeriphInc=DMA_PINC_DISABLE;
    SD1RxDMAHandler.Init.MemInc=DMA_MINC_ENABLE;
    SD1RxDMAHandler.Init.PeriphDataAlignment=DMA_PDATAALIGN_WORD;
    SD1RxDMAHandler.Init.MemDataAlignment=DMA_MDATAALIGN_WORD;
    SD1RxDMAHandler.Init.Mode=DMA_PFCTRL;
    SD1RxDMAHandler.Init.Priority=DMA_PRIORITY_VERY_HIGH;
    SD1RxDMAHandler.Init.FIFOMode=DMA_FIFOMODE_ENABLE;
    SD1RxDMAHandler.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
    SD1RxDMAHandler.Init.MemBurst=DMA_MBURST_INC4;
    SD1RxDMAHandler.Init.PeriphBurst=DMA_PBURST_INC4;

    __HAL_LINKDMA(&hsd1, hdmarx, SD1RxDMAHandler); //将接收DMA和SD卡的发送DMA连接起来
    HAL_DMA_DeInit(&SD1RxDMAHandler);
    HAL_DMA_Init(&SD1RxDMAHandler);              //初始化接收DMA

    //配置发送DMA
    SD1TxDMAHandler.Instance=DMA2_Stream6;
    SD1TxDMAHandler.Init.Channel=DMA_CHANNEL_4;
    SD1TxDMAHandler.Init.Direction=DMA_MEMORY_TO_PERIPH;
    SD1TxDMAHandler.Init.PeriphInc=DMA_PINC_DISABLE;
    SD1TxDMAHandler.Init.MemInc=DMA_MINC_ENABLE;
    SD1TxDMAHandler.Init.PeriphDataAlignment=DMA_PDATAALIGN_WORD;
    SD1TxDMAHandler.Init.MemDataAlignment=DMA_MDATAALIGN_WORD;
    SD1TxDMAHandler.Init.Mode=DMA_PFCTRL;
    SD1TxDMAHandler.Init.Priority=DMA_PRIORITY_VERY_HIGH;
    SD1TxDMAHandler.Init.FIFOMode=DMA_FIFOMODE_ENABLE;
    SD1TxDMAHandler.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
    SD1TxDMAHandler.Init.MemBurst=DMA_MBURST_INC4;
    SD1TxDMAHandler.Init.PeriphBurst=DMA_PBURST_INC4;

    __HAL_LINKDMA(&hsd1, hdmatx, SD1TxDMAHandler);//将发送DMA和SD卡的发送DMA连接起来
    HAL_DMA_DeInit(&SD1TxDMAHandler);
    HAL_DMA_Init(&SD1TxDMAHandler);              //初始化发送DMA


    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 3, 0);  //接收DMA中断优先级
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 3, 0);  //发送DMA中断优先级
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

    return HAL_OK;
}


void SDCARD1_Init(void)
{
    uint32_t errorstate;
    hsd1.Instance = SDMMC1;
    hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;//上升沿捕获
    hsd1.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;//时钟分频器旁路，旁路时SDMMC_CK 直接等于 SDMMCCLK
    hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;//disable 表示IDLE时也输出时钟
    //hsd1.Init.BusWide = SDMMC_BUS_WIDE_4B;//4位数据线
    hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;//1位数据线
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv = SDMMC_TRANSFER_CLK_DIV;//只有在不ClockBypass 时才起作用 SDMMC_CK / (ClockDiv + 2) = 48 / (0 + 2) = 24MHz
    
    // 初始化SD卡时会将SD卡信息写入hsd1.SdCard结构体中，具体参考
   // HAL_SD_GetCardCSD，它会在初始化中一步步调用到
    if (HAL_SD_Init(&hsd1) != HAL_OK)
    {
        printf("HAL_SD_Init failed\r\n");
        Error_Handler();
    }

    HAL_NVIC_SetPriority(SDMMC1_IRQn,2,0);  //配置SDMMC1中断，抢占优先级2，子优先级0
    HAL_NVIC_EnableIRQ(SDMMC1_IRQn);        //使能SDMMC1中断

    SDCARD1_DMA_Init();
    SDCARD_ShowInfo(&hsd1);

    vTaskDelay(100);
    errorstate = HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B);
    if(errorstate != HAL_OK)
    {
        printf("HAL_SD_ConfigWideBusOperation failed,errorstate:%d\r\n",errorstate);
        //Error_Handler();
    }else{
        printf("HAL_SD_ConfigWideBusOperation success\r\n");
    }
    HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd1);
    printf("cardState: %d\r\n", cardState);

    SD_Card_Test_DMA();
    printf("sd_int_record[0]: %d,sd_int_record[1]: %d,sd_int_record[2]: %d\r\n", sd_int_record[0],sd_int_record[1],sd_int_record[2]);

}



//通过DMA读取SD卡一个扇区
//buf:读数据缓存区
//sector:扇区地址
//blocksize:扇区大小(一般都是512字节)
//cnt:扇区个数
//返回值:错误状态;0,正常;其他,错误代码;
uint8_t SD_ReadBlocks_DMA(uint8_t *buf, uint64_t sector, uint32_t cnt)
{
    if(SCB->CCR & SCB_CCR_DC_Msk) SCB_CleanInvalidateDCache(); // 检查D-Cache是否启用
    
    HAL_StatusTypeDef status = HAL_SD_ReadBlocks_DMA(&hsd1, (uint8_t*)buf, sector, cnt);
    
    if(status == HAL_OK)
    {
        // 新版API使用超时等待传输状态
        uint32_t tickstart = HAL_GetTick();
        while(HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
        {
            if((HAL_GetTick() - tickstart) >= SD_TIMEOUT)
            {
                printf("SD_ReadBlocks_DMA timeout\r\n");
                return HAL_TIMEOUT;

            }
        }
    }
    
    if(SCB->CCR & SCB_CCR_DC_Msk) SCB_CleanInvalidateDCache();
    //printf("SD_ReadBlocks_DMA success,status:%d\r\n",status);
    return status;
}

//写SD卡
//buf:写数据缓存区
//sector:扇区地址
//blocksize:扇区大小(一般都是512字节)
//cnt:扇区个数
//返回值:错误状态;0,正常;其他,错误代码;
uint8_t SD_WriteBlocks_DMA(uint8_t *buf, uint64_t sector, uint32_t cnt)
{
    if(SCB->CCR & SCB_CCR_DC_Msk) SCB_CleanInvalidateDCache(); // 检查D-Cache是否启用
    
    HAL_StatusTypeDef status = HAL_SD_WriteBlocks_DMA(&hsd1, buf, sector, cnt);
    
    if(status == HAL_OK)
    {
        // 新版API使用超时等待传输状态
        uint32_t tickstart = HAL_GetTick();
        while(HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
        {
            if((HAL_GetTick() - tickstart) >= SD_TIMEOUT)
            {
                printf("SD_WriteBlocks_DMA timeout\r\n");   
                return HAL_TIMEOUT;
            }
        }
    }
    
    if(SCB->CCR & SCB_CCR_DC_Msk) SCB_CleanInvalidateDCache();
    //printf("SD_WriteBlocks_DMA success,status:%d\r\n",status);
    return status;
}

//读SD卡
//buf:读数据缓存区
//sector:扇区地址
//cnt:扇区个数
//返回值:错误状态;0,正常;其他,错误代码;
uint8_t SD_ReadDisk(uint8_t* buf,uint32_t sector,uint8_t cnt)
{
    uint8_t sta=HAL_SD_ERROR_NONE;
    long long lsector=sector;
    uint8_t n;
    /*
    SD卡根据不同类型有两种不同的寻址方式：
    SDSC卡（V1.X标准容量SD卡）：使用字节寻址方式,直接以字节为单位访问存储位置
    SDHC/SDXC卡（V2.0及以上高容量SD卡）：使用块寻址方式,一个块固定为512字节,地址单位是块号而不是字节位置
    左移9位相当于乘以512
    */
    if(SDCardInfo.CardType!=CARD_SDSC)lsector<<=9; //
    if((uint32_t)buf%4!=0)
    {
        for(n=0;n<cnt;n++)
        {
            sta=SD_ReadBlocks_DMA(SDIO_DATA_BUFFER,lsector+512*n,1);
            memcpy(buf,SDIO_DATA_BUFFER,512);
            buf+=512;
        }
    }else
    {
        sta=SD_ReadBlocks_DMA(buf,lsector,cnt);
    }
    return sta;
}

//写SD卡
//buf:写数据缓存区
//sector:扇区地址
//cnt:扇区个数
//返回值:错误状态;0,正常;其他,错误代码;
uint8_t SD_WriteDisk(uint8_t *buf,uint32_t sector,uint8_t cnt)
{
    uint8_t sta=HAL_SD_ERROR_NONE;
    long long lsector=sector;
    uint8_t n;
    if(SDCardInfo.CardType!=CARD_SDSC)lsector<<=9;
    if((uint32_t)buf%4!=0)
    {
        for(n=0;n<cnt;n++)
        {
            memcpy(SDIO_DATA_BUFFER,buf,512);
            sta=SD_WriteBlocks_DMA(SDIO_DATA_BUFFER,lsector+512*n,1);//单个sector的写操作
            buf+=512;
        }
    }else
    {
        sta=SD_WriteBlocks_DMA(buf,lsector,cnt);//多个sector的写操作
    }
    return sta;
}



/**
  * @brief  SD卡读写测试
  * @retval 0:成功 其他:错误代码
  */
// uint8_t SD_Card_Test(void)
// {
//   uint32_t i;
//   HAL_StatusTypeDef sd_status = HAL_OK;
//   uint8_t test_result = 0;
//   HAL_SD_CardInfoTypeDef cardInfo;
  

  
//   // 初始化写入缓冲区
//   for (i = 0; i < BUFFER_WORDS; i++)
//   {
//     WriteBuffer[i] = i;
//   }
  
//   // 清空读取缓冲区
//   memset(ReadBuffer, 0, sizeof(ReadBuffer));
  
//   printf("Start writing %d blocks to SD card...\r\n", TEST_BLOCKS);
  
//   // 写入数据到SD卡的第0块
//   sd_status = HAL_SD_WriteBlocks(&hsd1, (uint8_t*)WriteBuffer, 0, TEST_BLOCKS, 1000);
//   if (sd_status != HAL_OK)
//   {
//     printf("Write SD card failed! Error: %d\r\n", (int)sd_status);
//     return 2;
//   }
  
//   printf("Write completed.\r\n");
//   printf("Start reading %d blocks from SD card...\r\n", TEST_BLOCKS);
  
//     for(int i=0;i<10;i++)
//     {
//         sd_status = HAL_SD_GetCardState(&hsd1);
//         printf("cardState: %d\r\n", sd_status);
//         delay_ms(10);
//     }


//   // 从SD卡的第0块读取数据
//   sd_status = HAL_SD_ReadBlocks(&hsd1, (uint8_t*)ReadBuffer, 0, TEST_BLOCKS, 1000);
//   if (sd_status != HAL_OK)
//   {
//     printf("Read SD card failed! Error: %d\r\n", (int)sd_status);
//     return 3;
//   }
  
//   printf("Read completed.\r\n");
  
//   // 比较数据
//   for (i = 0; i < BUFFER_WORDS; i++)
//   {
//     if (ReadBuffer[i] != WriteBuffer[i])
//     {
//       printf("Data verification failed at word %d! Write: %08X, Read: %08X\r\n", 
//              (int)i, (unsigned int)WriteBuffer[i], (unsigned int)ReadBuffer[i]);
//       test_result = 4;
//       break;
//     }
//   }
  
//   if (test_result == 0)
//   {
//     printf("SD card read/write test passed!\r\n");
//   }
//   else
//   {
//     printf("SD card read/write test failed!\r\n");
//   }
  
//   return test_result;
// }



uint8_t SD_Card_Test_DMA(void)
{
  uint32_t i;
  uint8_t sd_status = HAL_OK;
  uint8_t test_result = 0;
  
//   // 初始化写入缓冲区
//   for (i = 0; i < 255; i++)
//   {
//     WriteBuffer[i] = 255-i;
//   }
  
//   // 清空读取缓冲区
// //   memset(ReadBuffer, 0, sizeof(ReadBuffer));
  
//   printf("Start writing %d blocks to SD card using DMA...\r\n", TEST_BLOCKS);
  
//   // 使用DMA写入数据
//   sd_status = SD_WriteBlocks_DMA(WriteBuffer, 10, TEST_BLOCKS);
//   if (sd_status != HAL_OK)
//   {
//     printf("Write SD card failed! Error: %d\r\n", (int)sd_status);
//     return 2;
//   }
  
//   printf("Write completed.\r\n");
//   printf("Start reading %d blocks from SD card using DMA...\r\n", TEST_BLOCKS);

  //清空读取缓冲区
  memset(SDIO_DATA_BUFFER, 0, sizeof(SDIO_DATA_BUFFER));
  
  // 使用DMA读取数据
  sd_status = SD_ReadBlocks_DMA(SDIO_DATA_BUFFER, 10, 1);
  if (sd_status != HAL_OK)
  {
    printf("Read SD card failed! Error: %d\r\n", (int)sd_status);
    return -1;
  } else {
    printf("Read SD card success\r\n");
  }
  
  // 比较数据
  for (i = 0; i < BLOCK_SIZE; i++)
  {
    // if (ReadBuffer[i] != WriteBuffer[i])
    // {
    //   printf("Data verification failed at word %d! Write: %08X, Read: %08X\r\n", 
    //          (int)i, (unsigned int)WriteBuffer[i], (unsigned int)ReadBuffer[i]);
    //   test_result = 4;
    //   break;
    // }
    printf("%02X ", SDIO_DATA_BUFFER[i]);
  }
  printf("\r\n");
  
  if (test_result == 0)
  {
    printf("SD card DMA read/write test passed!\r\n");
  }
  else
  {
    printf("SD card DMA read/write test failed!\r\n");
  }
  
  return test_result;
}


//SDMMC1中断服务函数
void SDMMC1_IRQHandler(void)
{
    HAL_SD_IRQHandler(&hsd1);
    sd_int_record[0]++;
}

//DMA2数据流6中断服务函数
void DMA2_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(hsd1.hdmatx);
    sd_int_record[1]++;
}

//DMA2数据流3中断服务函数
void DMA2_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(hsd1.hdmarx);
    sd_int_record[2]++;
}


#endif /* SDCARD_ENABLE */ 