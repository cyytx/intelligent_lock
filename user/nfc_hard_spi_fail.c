
/**
  ******************************************************************************
  * @file    nfc.c
  * @author  cyytx
  * @brief   NFC模块的源文件,实现NFC的初始化、读写等功能,本来是想通过硬件SPI来实现的，但是由于CS pin 连接说的是硬件的
  * SPI_NSS,这个不知道为什么，有个很麻烦的问题，设置为硬件NSS模式，它非常不正常，在发送时突然变高20ns,又变低，设置为
  * 软件NSS模式，设置为gpio out,也是一样，根本无法正常控制它，导致无法正常使用，根本不知道是什么所以只能用软件SPI来实现，
  * 所以以后连接spi,cs pin 千万不要连接到硬件的spi_nss,否则会出问题
  ******************************************************************************
  */


#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "nfc.h"
#include "delay.h"
#include "priorities.h"

#if NFC_ENABLE

static SPI_HandleTypeDef hspi5;  // NFC模块SPI句柄
#define RC522_RST_GPIO_PORT GPIOI
#define RC522_RST_PIN       GPIO_PIN_10

/*
PI10     ------> NFC_RESET
PF6     ------> SPI5_NSS
PF7     ------> SPI5_SCK
PF8     ------> SPI5_MISO
PF9     ------> SPI5_MOSI
*/

#define          RC522_Reset_Enable()      HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, GPIO_PIN_RESET)  // 复位使能
#define          RC522_Reset_Disable()     HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, GPIO_PIN_SET)    // 复位禁用

// SPI初始化
void SPI5_Init(void) {
    __HAL_RCC_SPI5_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct;

    // SPI5 GPIO配置
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI5;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // SPI5配置
    hspi5.Instance = SPI5;
    hspi5.Init.Mode = SPI_MODE_MASTER;
    hspi5.Init.Direction = SPI_DIRECTION_2LINES;
    hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi5.Init.NSS = SPI_NSS_HARD_OUTPUT;
    hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64; // 96MHz/16=6MHz
    hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi5);
}

// RC522复位引脚初始化
void RC522_RST_Init(void) {
    __HAL_RCC_GPIOI_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Pin = RC522_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RC522_RST_GPIO_PORT, &GPIO_InitStruct);
}

// 写寄存器
void MFRC522_WriteReg(uint8_t reg, uint8_t data) {
    uint8_t txData[2] = { (reg << 1) & 0x7E, data };
    HAL_SPI_Transmit(&hspi5, txData, 2, HAL_MAX_DELAY);
}

// 读寄存器
uint8_t MFRC522_ReadReg(uint8_t reg) {
    uint8_t txData = ((reg << 1) & 0x7E) | 0x80;
    uint8_t rxData;
    HAL_SPI_TransmitReceive(&hspi5, &txData, &rxData, 1, HAL_MAX_DELAY);
    return rxData;
}

// 设置位掩码
void MFRC522_SetBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = MFRC522_ReadReg(reg);
    MFRC522_WriteReg(reg, tmp | mask);
}

// 清除位掩码
void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = MFRC522_ReadReg(reg);
    MFRC522_WriteReg(reg, tmp & (~mask));
}

// RC522复位
void RC522_Reset(void) {
    HAL_GPIO_WritePin(RC522_RST_GPIO_PORT, RC522_RST_PIN, GPIO_PIN_RESET);
    delay_ms(1);
    HAL_GPIO_WritePin(RC522_RST_GPIO_PORT, RC522_RST_PIN, GPIO_PIN_SET);
    delay_ms(10);
}

/* 函数名：PcdRese
 * 描述  ：复位RC522 
 * 输入  ：无
 * 返回  : 无
 * 调用  ：外部调用              */
void PcdReset ( void )
{
    RC522_Reset_Disable();
    delay_us ( 1 );
    RC522_Reset_Enable();
    delay_us ( 1 );
    RC522_Reset_Disable();
    delay_us ( 1 );
    MFRC522_WriteReg(CommandReg, 0x0f);

    while ( MFRC522_ReadReg ( CommandReg ) & 0x10 );
	
    delay_us ( 1 );
    MFRC522_WriteReg ( ModeReg, 0x3D );                //定义发送和接收常用模式 和Mifare卡通讯，CRC初始值0x6363
    MFRC522_WriteReg ( TReloadRegL, 30 );              //16位定时器低位    
    MFRC522_WriteReg ( TReloadRegH, 0 );			     //16位定时器高位
    MFRC522_WriteReg ( TModeReg, 0x8D );				 //定义内部定时器的设置
    MFRC522_WriteReg ( TPrescalerReg, 0x3E );			 //设置定时器分频系数
    MFRC522_WriteReg ( TxAutoReg, 0x40 );				 //调制发送信号为100%ASK		
}



/* 函数名：PcdAntennaOn
 * 描述  ：开启天线 
 * 输入  ：无
 * 返回  : 无
 * 调用  ：内部调用            */
void PcdAntennaOn ( void )
{
    uint8_t uc;
    uc = MFRC522_ReadReg ( TxControlReg );
	
    if ( ! ( uc & 0x03 ) )
			MFRC522_SetBitMask(TxControlReg, 0x03);

}

/* 函数名：PcdAntennaOff
 * 描述  ：开启天线 
 * 输入  ：无
 * 返回  : 无
 * 调用  ：内部调用             */
void PcdAntennaOff ( void )
{
    MFRC522_ClearBitMask ( TxControlReg, 0x03 );
}

/* 函数名：M500PcdConfigISOType
 * 描述  ：设置RC522的工作方式
 * 输入  ：ucType，工作方式
 * 返回  : 无
 * 调用  ：外部调用        */
void M500PcdConfigISOType ( uint8_t ucType )
{
	if ( ucType == 'A')                     //ISO14443_A
  {
		MFRC522_ClearBitMask ( Status2Reg, 0x08 );		

    MFRC522_WriteReg ( ModeReg, 0x3D );//3F	
		MFRC522_WriteReg ( RxSelReg, 0x86 );//84
		MFRC522_WriteReg ( RFCfgReg, 0x7F );   //4F
		MFRC522_WriteReg ( TReloadRegL, 30 );//tmoLength);// TReloadVal = 'h6a =tmoLength(dec) 
		MFRC522_WriteReg ( TReloadRegH, 0 );
		MFRC522_WriteReg ( TModeReg, 0x8D );
		MFRC522_WriteReg ( TPrescalerReg, 0x3E );
		delay_us   ( 2 );
		
		PcdAntennaOn ();//开天线
   }
}


void NFC_Init ( void )
{
    RC522_RST_Init();
    SPI5_Init();	      
	PcdReset ();                  //复位RC522 
    PcdAntennaOff();              //关闭天线
	delay_us(1);
    PcdAntennaOn();               //打开天线
	M500PcdConfigISOType ( 'A' ); //设置工作方式
    printf("NFC INIT SUCCESS\r\n");
}




/* 函数名：PcdComMF522
 * 描述  ：通过RC522和ISO14443卡通讯
 * 输入  ：ucCommand，RC522命令字
 *         pInData，通过RC522发送到卡片的数据
 *         ucInLenByte，发送数据的字节长度
 *         pOutData，接收到的卡片返回数据
 *         pOutLenBit，返回数据的位长度
 * 返回  : 状态值
 *         = MI_OK，成功
 * 调用  ：内部调用              */
char PcdComMF522 ( uint8_t ucCommand, uint8_t * pInData, uint8_t ucInLenByte, uint8_t * pOutData, uint32_t * pOutLenBit )		
{
    char cStatus = MI_ERR;
    uint8_t ucIrqEn   = 0x00;
    uint8_t ucWaitFor = 0x00;
    uint8_t ucLastBits;
    uint8_t ucN;
    uint32_t ul;

    switch ( ucCommand )
    {
       case PCD_AUTHENT:		//Mifare认证
          ucIrqEn   = 0x12;		//允许错误中断请求ErrIEn  允许空闲中断IdleIEn
          ucWaitFor = 0x10;		//认证寻卡等待时候 查询空闲中断标志位
          break;
			 
       case PCD_TRANSCEIVE:		//接收发送 发送接收
          ucIrqEn   = 0x77;		//允许TxIEn RxIEn IdleIEn LoAlertIEn ErrIEn TimerIEn
          ucWaitFor = 0x30;		//寻卡等待时候 查询接收中断标志位与 空闲中断标志位
          break;
			 
       default:
         break;
			 
    }
   
    MFRC522_WriteReg ( ComIEnReg, ucIrqEn | 0x80 );		//IRqInv置位管脚IRQ与Status1Reg的IRq位的值相反 
    MFRC522_ClearBitMask ( ComIrqReg, 0x80 );			//Set1该位清零时，CommIRqReg的屏蔽位清零
    MFRC522_WriteReg ( CommandReg, PCD_IDLE );		//写空闲命令
    MFRC522_SetBitMask ( FIFOLevelReg, 0x80 );			//置位FlushBuffer清除内部FIFO的读和写指针以及ErrReg的BufferOvfl标志位被清除
    
    for ( ul = 0; ul < ucInLenByte; ul ++ )
		MFRC522_WriteReg ( FIFODataReg, pInData [ ul ] );    		//写数据进FIFOdata
			
    MFRC522_WriteReg ( CommandReg, ucCommand );					//写命令
   
    
    if ( ucCommand == PCD_TRANSCEIVE )
			MFRC522_SetBitMask(BitFramingReg,0x80);  				//StartSend置位启动数据发送 该位与收发命令使用时才有效
    
    ul = 1000;//根据时钟频率调整，操作M1卡最大等待时间25ms
		
    do 														//认证 与寻卡等待时间	
    {
         ucN = MFRC522_ReadReg ( ComIrqReg );							//查询事件中断
         ul --;
    } while ( ( ul != 0 ) && ( ! ( ucN & 0x01 ) ) && ( ! ( ucN & ucWaitFor ) ) );		//退出条件i=0,定时器中断，与写空闲命令
		
    MFRC522_ClearBitMask ( BitFramingReg, 0x80 );					//清理允许StartSend位
		
    if ( ul != 0 )
    {
		if ( ! (( MFRC522_ReadReg ( ErrorReg ) & 0x1B )) )			//读错误标志寄存器BufferOfI CollErr ParityErr ProtocolErr
		{
			cStatus = MI_OK;
			
			if ( ucN & ucIrqEn & 0x01 )					//是否发生定时器中断
			  cStatus = MI_NOTAGERR;   
				
			if ( ucCommand == PCD_TRANSCEIVE )
			{
				ucN = MFRC522_ReadReg ( FIFOLevelReg );			//读FIFO中保存的字节数
				
				ucLastBits = MFRC522_ReadReg ( ControlReg ) & 0x07;	//最后接收到得字节的有效位数
				
				if ( ucLastBits )
					* pOutLenBit = ( ucN - 1 ) * 8 + ucLastBits;   	//N个字节数减去1（最后一个字节）+最后一位的位数 读取到的数据总位数
				else
					* pOutLenBit = ucN * 8;   					//最后接收到的字节整个字节有效
				
				if ( ucN == 0 )	
                    ucN = 1;    
				
				if ( ucN > MAXRLEN )
					ucN = MAXRLEN;   
				
				for ( ul = 0; ul < ucN; ul ++ )
				  pOutData [ ul ] = MFRC522_ReadReg ( FIFODataReg );   
			}		
        }
			else
				cStatus = MI_ERR;   
    }
   
   MFRC522_SetBitMask ( ControlReg, 0x80 );           // stop timer now
   MFRC522_WriteReg ( CommandReg, PCD_IDLE ); 
	
   return cStatus;
}


/* 函数名：PcdRequest
 * 描述  ：寻卡
 * 输入  ：ucReq_code，寻卡方式
 *                     = 0x52，寻感应区内所有符合14443A标准的卡
 *                     = 0x26，寻未进入休眠状态的卡
 *         pTagType，卡片类型代码
 *                   = 0x4400，Mifare_UltraLight
 *                   = 0x0400，Mifare_One(S50)
 *                   = 0x0200，Mifare_One(S70)
 *                   = 0x0800，Mifare_Pro(X))
 *                   = 0x4403，Mifare_DESFire
 * 返回  : 状态值
 *         = MI_OK，成功
 * 调用  ：外部调用            */
char PcdRequest ( uint8_t ucReq_code, uint8_t * pTagType )
{
    char cStatus;  
    uint8_t ucComMF522Buf [ MAXRLEN ]; 
    uint32_t ulLen;

    MFRC522_ClearBitMask ( Status2Reg, 0x08 );	//清理指示MIFARECyptol单元接通以及所有卡的数据通信被加密的情况
    MFRC522_WriteReg ( BitFramingReg, 0x07 );	//	发送的最后一个字节的 七位
    MFRC522_SetBitMask ( TxControlReg, 0x03 );	//TX1,TX2管脚的输出信号传递经发送调制的13.56的能量载波信号

    ucComMF522Buf [ 0 ] = ucReq_code;		//存入 卡片命令字

    cStatus = PcdComMF522 ( PCD_TRANSCEIVE,	ucComMF522Buf, 1, ucComMF522Buf, & ulLen );	//寻卡  

    if ( ( cStatus == MI_OK ) && ( ulLen == 0x10 ) )	//寻卡成功返回卡类型 
    {    
       * pTagType = ucComMF522Buf [ 0 ];
       * ( pTagType + 1 ) = ucComMF522Buf [ 1 ];
    }
     
    else
     cStatus = MI_ERR;

    return cStatus;
}

/* 函数名：PcdAnticoll
 * 描述  ：防冲撞
 * 输入  ：pSnr，卡片序列号，4字节
 * 返回  : 状态值
 *         = MI_OK，成功
 * 调用  ：外部调用           */
char PcdAnticoll ( uint8_t * pSnr )
{
    char cStatus;
    uint8_t uc, ucSnr_check = 0;
    uint8_t ucComMF522Buf [ MAXRLEN ]; 
	  uint32_t ulLen;

    MFRC522_ClearBitMask ( Status2Reg, 0x08 );		//清MFCryptol On位 只有成功执行MFAuthent命令后，该位才能置位
    MFRC522_WriteReg ( BitFramingReg, 0x00);		//清理寄存器 停止收发
    MFRC522_ClearBitMask ( CollReg, 0x80 );			//清ValuesAfterColl所有接收的位在冲突后被清除
   
    ucComMF522Buf [ 0 ] = 0x93;	//卡片防冲突命令
    ucComMF522Buf [ 1 ] = 0x20;
   
    cStatus = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 2, ucComMF522Buf, & ulLen);//与卡片通信
	
    if ( cStatus == MI_OK)		//通信成功
    {
			for ( uc = 0; uc < 4; uc ++ )
			{
					* ( pSnr + uc )  = ucComMF522Buf [ uc ];			//读出UID
					ucSnr_check ^= ucComMF522Buf [ uc ];
			}
			
        if ( ucSnr_check != ucComMF522Buf [ uc ] )
        		cStatus = MI_ERR;    		 
    }
    MFRC522_SetBitMask ( CollReg, 0x80 );

    return cStatus;
}


/* 函数名：CalulateCRC
 * 描述  ：用RC522计算CRC16
 * 输入  ：pIndata，计算CRC16的数组
 *         ucLen，计算CRC16的数组字节长度
 *         pOutData，存放计算结果存放的首地址
 * 返回  : 无
 * 调用  ：内部调用              */
void CalulateCRC ( uint8_t * pIndata, uint8_t ucLen, uint8_t * pOutData )
{
    uint8_t uc, ucN;

    MFRC522_ClearBitMask(DivIrqReg,0x04);
    MFRC522_WriteReg(CommandReg,PCD_IDLE);
    MFRC522_SetBitMask(FIFOLevelReg,0x80);
    
    for ( uc = 0; uc < ucLen; uc ++)
        MFRC522_WriteReg ( FIFODataReg, * ( pIndata + uc ) );   

    MFRC522_WriteReg ( CommandReg, PCD_CALCCRC );
    uc = 0xFF;

    do {
        ucN = MFRC522_ReadReg ( DivIrqReg );
        uc --;} 
    while ( ( uc != 0 ) && ! ( ucN & 0x04 ) );
        
    pOutData [ 0 ] = MFRC522_ReadReg ( CRCResultRegL );
    pOutData [ 1 ] = MFRC522_ReadReg ( CRCResultRegM );
    
}


/* 函数名：PcdSelect
 * 描述  ：选定卡片
 * 输入  ：pSnr，卡片序列号，4字节
 * 返回  : 状态值
 *         = MI_OK，成功
 * 调用  ：外部调用         */
char PcdSelect ( uint8_t * pSnr )
{
    char ucN;
    uint8_t uc;
    uint8_t ucComMF522Buf [ MAXRLEN ]; 
    uint32_t  ulLen;

    ucComMF522Buf [ 0 ] = PICC_ANTICOLL1;
    ucComMF522Buf [ 1 ] = 0x70;
    ucComMF522Buf [ 6 ] = 0;
    
    for ( uc = 0; uc < 4; uc ++ )
    {
        ucComMF522Buf [ uc + 2 ] = * ( pSnr + uc );
        ucComMF522Buf [ 6 ] ^= * ( pSnr + uc );
    }
        
    CalulateCRC ( ucComMF522Buf, 7, & ucComMF522Buf [ 7 ] );
    MFRC522_ClearBitMask ( Status2Reg, 0x08 );
    ucN = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 9, ucComMF522Buf, & ulLen );
    
    if ( ( ucN == MI_OK ) && ( ulLen == 0x18 ) )
      ucN = MI_OK;  
    else
      ucN = MI_ERR;    

    return ucN; 
}



/* 函数名：PcdAuthState
 * 描述  ：验证卡片密码
 * 输入  ：ucAuth_mode，密码验证模式
 *                     = 0x60，验证A密钥
 *                     = 0x61，验证B密钥
 *         uint8_t ucAddr，块地址
 *         pKey，密码
 *         pSnr，卡片序列号，4字节
 * 返回  : 状态值
 *         = MI_OK，成功
 * 调用  ：外部调用          */
char PcdAuthState ( uint8_t ucAuth_mode, uint8_t ucAddr, uint8_t * pKey, uint8_t * pSnr )
{
    char cStatus;
    uint8_t uc, ucComMF522Buf [ MAXRLEN ];
    uint32_t ulLen;

    ucComMF522Buf [ 0 ] = ucAuth_mode;
    ucComMF522Buf [ 1 ] = ucAddr;
    
    for ( uc = 0; uc < 6; uc ++ )
        ucComMF522Buf [ uc + 2 ] = * ( pKey + uc );   
    
    for ( uc = 0; uc < 6; uc ++ )
        ucComMF522Buf [ uc + 8 ] = * ( pSnr + uc );   

    cStatus = PcdComMF522 ( PCD_AUTHENT, ucComMF522Buf, 12, ucComMF522Buf, & ulLen );
    
    if ( ( cStatus != MI_OK ) || ( ! ( MFRC522_ReadReg ( Status2Reg ) & 0x08 ) ) ){
            cStatus = MI_ERR; 
    }
		
    return cStatus;    
}

/* 函数名：PcdWrite
 * 描述  ：写数据到M1卡一块
 * 输入  ：u8 ucAddr，块地址
 *         pData，写入的数据，16字节
 * 返回  : 状态值
 *         = MI_OK，成功
 * 调用  ：外部调用           */
char PcdWrite ( uint8_t ucAddr, uint8_t * pData )
{
    char cStatus;
      uint8_t uc, ucComMF522Buf [ MAXRLEN ];
    uint32_t ulLen;

    ucComMF522Buf [ 0 ] = PICC_WRITE;
    ucComMF522Buf [ 1 ] = ucAddr;
    
    CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );
 
    cStatus = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, & ulLen );

    if ( ( cStatus != MI_OK ) || ( ulLen != 4 ) || ( ( ucComMF522Buf [ 0 ] & 0x0F ) != 0x0A ) )
      cStatus = MI_ERR;   
        
    if ( cStatus == MI_OK )
    {
      memcpy(ucComMF522Buf, pData, 16);
      for ( uc = 0; uc < 16; uc ++ )
              ucComMF522Buf [ uc ] = * ( pData + uc );  
            
      CalulateCRC ( ucComMF522Buf, 16, & ucComMF522Buf [ 16 ] );

      cStatus = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 18, ucComMF522Buf, & ulLen );
            
            if ( ( cStatus != MI_OK ) || ( ulLen != 4 ) || ( ( ucComMF522Buf [ 0 ] & 0x0F ) != 0x0A ) )
        cStatus = MI_ERR;   
            
    } 

    return cStatus;
    
}

/* 函数名：PcdRead
 * 描述  ：读取M1卡一块数据
 * 输入  ：u8 ucAddr，块地址
 *         pData，读出的数据，16字节
 * 返回  : 状态值
 *         = MI_OK，成功
 * 调用  ：外部调用             */
char PcdRead ( uint8_t ucAddr, uint8_t * pData )
{
    char cStatus;
    uint8_t uc, ucComMF522Buf [ MAXRLEN ]; 
    uint32_t ulLen;

    ucComMF522Buf [ 0 ] = PICC_READ;
    ucComMF522Buf [ 1 ] = ucAddr;
    
    CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );
   
    cStatus = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, & ulLen );
    
    if ( ( cStatus == MI_OK ) && ( ulLen == 0x90 ) )
    {
            for ( uc = 0; uc < 16; uc ++ )
        * ( pData + uc ) = ucComMF522Buf [ uc ];   
    }
        
    else
      cStatus = MI_ERR;   
    
    return cStatus;

}

/* 函数名：PcdHalt
 * 描述  ：命令卡片进入休眠状态
 * 输入  ：无
 * 返回  : 状态值
 *         = MI_OK，成功
 * 调用  ：外部调用        */
char PcdHalt( void )
{
    uint8_t ucComMF522Buf [ MAXRLEN ]; 
    uint32_t  ulLen;

    ucComMF522Buf [ 0 ] = PICC_HALT;
    ucComMF522Buf [ 1 ] = 0;

    CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );
    PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, & ulLen );

    return MI_OK;   
}



void RC522_Read_ID_Once(void)
{
	//char Str1[20],Str2[20];
	uint8_t card_type[2];//卡片类型，2字节
	uint8_t card_ID[4];//卡序列号
	uint8_t statusRt;
	//PcdAntennaOn();
	statusRt = PcdRequest(PICC_REQIDL, card_type);//寻未进入休眠的卡
	if(statusRt == MI_OK){//寻卡成功
		//printf("寻卡成功~！\r\n");
		printf ( "card_type: %02X%02X",
					card_type [ 0 ],
					card_type [ 1 ]);
		//printf ( "%s\r\n",Str1); 
		if( PcdAnticoll (card_ID) == MI_OK){//防冲撞成功
			printf ("The Card ID is: %02X%02X%02X%02X",
					card_ID [ 0 ],
					card_ID [ 1 ],
					card_ID [ 2 ],
					card_ID [ 3 ] );
			//printf ( "%s\r\n",Str2); 
			if(PcdSelect(card_ID) == MI_OK){
				//printf("选卡成功！\r\n");
				if(PcdHalt() == MI_OK){
					//printf("休眠成功！\r\n");
					printf ("cardID: %02X%02X%02X%02X", card_ID [ 0 ], card_ID [ 1 ], card_ID [ 2 ], card_ID [ 3 ] );
						//printf("read suc!\r\n");
				}
			}
		}
	}
}



static TaskHandle_t nfcTaskHandle = NULL;  // 任务句柄


void NFC_Task(void *argument)
{
    char cStr[30], tStr[30];
    uint8_t ucArray_ID[4];    // 存放IC卡的类型和UID
    uint8_t ucStatusReturn;   // 返回状态

    while(1)
    {
        // 寻卡
        ucStatusReturn = PcdRequest(PICC_REQALL, ucArray_ID);
        // if(ucStatusReturn != MI_OK)
        // {
        //     // 若失败再次寻卡
        //     ucStatusReturn = PcdRequest(PICC_REQALL, ucArray_ID);
        // }
        
        if(ucStatusReturn == MI_OK)
        {
            // 成功读取到卡
            sprintf(tStr, "The Card Type is: %02X%02X",
                    ucArray_ID[0],
                    ucArray_ID[1]);
            printf("%s\r\n", tStr);

            // 防冲撞
            if(PcdAnticoll(ucArray_ID) == MI_OK)
            {
                sprintf(cStr, "The Card ID is: %02X%02X%02X%02X",
                        ucArray_ID[0],
                        ucArray_ID[1],
                        ucArray_ID[2],
                        ucArray_ID[3]);
                printf("%s\r\n", cStr);
            }
        }

        // 添加适当的延时,避免过于频繁的读卡操作
        vTaskDelay(200);
    }
}


/**
 * @brief 创建NFC任务
 * @return BaseType_t - pdPASS:成功, pdFAIL:失败
 */
void NFC_CreateTask(void)
{
    BaseType_t xReturn = pdPASS;
    
    // 创建NFC任务
    xReturn = xTaskCreate((TaskFunction_t )NFC_Task,  // 任务函数
                         (const char*    )"NfcTask",   // 任务名称
                         (uint16_t       )STACK_SIZE_NFC,  // 任务堆栈大小
                         (void*          )NULL,  // 传递给任务函数的参数
                         (UBaseType_t    )TASK_PRIORITY_NFC,  // 任务优先级
                         (TaskHandle_t*  )&nfcTaskHandle);  // 任务句柄
    
    if (xReturn != pdPASS)
    {
        // 创建失败处理
        Error_Handler();
    }
}

#endif /* NFC_ENABLE */ 