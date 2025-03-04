#include <stdlib.h>
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lcd.h"
#include "lcd_pic.h"
#include "lcd_font.h"  // 字体文件
#include "priorities.h"


/* 显示任务参数结构体 
 * 用于跟踪长期显示任务的执行状态 */
typedef struct {
    uint16_t x;                 // 显示起始X坐标
    uint16_t y;                 // 显示起始Y坐标
    uint16_t width;             // 图像宽度（像素）
    uint16_t height;            // 图像高度（像素）
    const uint8_t *pic;         // 图像数据指针
    uint32_t transferred;       // 已传输字节数（用于分块传输）
    uint8_t active_buffer;      // 当前使用的DMA缓冲区标识（0/1）
} DisplayTaskParams;



// 全局资源定义
static QueueHandle_t xDisplayQueue = NULL;      // 显示命令队列
static TaskHandle_t xDisplayTaskHandle = NULL;   // 显示任务句柄
static DisplayTaskParams xDisplayParams = {0};   // 当前显示任务参数

// 新增传输状态标志
volatile uint8_t g_dma_transfer_in_progress = 0;
extern SPI_HandleTypeDef hspi2;

//void vDisplayTask(void *pvParameters);




/******************************************************************************
      函数说明：在指定位置画点
      入口数据：x,y 画点坐标
                color 点的颜色
      返回值：  无
******************************************************************************/
void LCD_DrawPoint(uint16_t x,uint16_t y,uint16_t color)
{
	LCD_Address_Set(x,y,x,y);//设置光标位置 
	LCD_WR_DATA(color);
} 


/******************************************************************************
      函数说明：画线
      入口数据：x1,y1   起始坐标
                x2,y2   终止坐标
                color   线的颜色
      返回值：  无
******************************************************************************/
void LCD_DrawLine(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t color)
{
	uint16_t t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	delta_x=x2-x1; //计算坐标增量 
	delta_y=y2-y1;
	uRow=x1;//画线起点坐标
	uCol=y1;
	if(delta_x>0)incx=1; //设置单步方向 
	else if (delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if (delta_y==0)incy=0;//水平线 
	else {incy=-1;delta_y=-delta_y;}
	if(delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y;
	for(t=0;t<distance+1;t++)
	{
		LCD_DrawPoint(uRow,uCol,color);//画点
		xerr+=delta_x;
		yerr+=delta_y;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}
}


/******************************************************************************
      函数说明：画矩形
      入口数据：x1,y1   起始坐标
                x2,y2   终止坐标
                color   矩形的颜色
      返回值：  无
******************************************************************************/
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,uint16_t color)
{
	LCD_DrawLine(x1,y1,x2,y1,color);
	LCD_DrawLine(x1,y1,x1,y2,color);
	LCD_DrawLine(x1,y2,x2,y2,color);
	LCD_DrawLine(x2,y1,x2,y2,color);
}


/******************************************************************************
      函数说明：画圆
      入口数据：x0,y0   圆心坐标
                r       半径
                color   圆的颜色
      返回值：  无
******************************************************************************/
void Draw_Circle(uint16_t x0,uint16_t y0,uint8_t r,uint16_t color)
{
	int a,b;
	a=0;b=r;	  
	while(a<=b)
	{
		LCD_DrawPoint(x0-b,y0-a,color);             //3           
		LCD_DrawPoint(x0+b,y0-a,color);             //0           
		LCD_DrawPoint(x0-a,y0+b,color);             //1                
		LCD_DrawPoint(x0-a,y0-b,color);             //2             
		LCD_DrawPoint(x0+b,y0+a,color);             //4               
		LCD_DrawPoint(x0+a,y0-b,color);             //5
		LCD_DrawPoint(x0+a,y0+b,color);             //6 
		LCD_DrawPoint(x0-b,y0+a,color);             //7
		a++;
		if((a*a+b*b)>(r*r))//判断要画的点是否过远
		{
			b--;
		}
	}
}

/******************************************************************************
      函数说明：显示汉字串
      入口数据：x,y显示坐标
                *s 要显示的汉字串
                fc 字的颜色
                bc 字的背景色
                sizey 字号 可选 16 24 32
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode)
{
	while(*s!=0)
	{
		if(sizey==12) LCD_ShowChinese12x12(x,y,s,fc,bc,sizey,mode);
		else if(sizey==16) LCD_ShowChinese16x16(x,y,s,fc,bc,sizey,mode);
		else if(sizey==24) LCD_ShowChinese24x24(x,y,s,fc,bc,sizey,mode);
		else if(sizey==32) LCD_ShowChinese32x32(x,y,s,fc,bc,sizey,mode);
		else return;
		//s+=2;// GB2312编码
        s+=3;//UTF-8编码，
		x+=sizey;
	}
}

/******************************************************************************
      函数说明：显示单个12x12汉字
      入口数据：x,y显示坐标
                *s 要显示的汉字
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese12x12(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode)
{
	uint8_t i,j,m=0;
	uint16_t k;
	uint16_t HZnum;//汉字数目
	uint16_t TypefaceNum;//一个字符所占字节大小
	uint16_t x0=x;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	                         
	HZnum=sizeof(tfont12)/sizeof(typFNT_GB12);	//统计汉字数目
	for(k=0;k<HZnum;k++) 
	{
		if((tfont12[k].Index[0]==*(s))&&(tfont12[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(!mode)//非叠加方式
					{
						if(tfont12[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//叠加方式
					{
						if(tfont12[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//画一个点
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}				  	
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
} 


/******************************************************************************
      函数说明：显示单个16x16汉字
      入口数据：x,y显示坐标
                *s 要显示的汉字
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese16x16(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode)
{
	uint8_t i,j,m=0;
	uint16_t k;
	uint16_t HZnum;//汉字数目
	uint16_t TypefaceNum;//一个字符所占字节大小
	uint16_t x0=x;
  TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	HZnum=sizeof(tfont16)/sizeof(typFNT_GB16);	//统计汉字数目
	for(k=0;k<HZnum;k++) 
	{
		if ((tfont16[k].Index[0]==*(s))&&(tfont16[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(!mode)//非叠加方式
					{
						if(tfont16[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//叠加方式
					{
						if(tfont16[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//画一个点
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}				  	
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
} 


/******************************************************************************
      函数说明：显示单个24x24汉字
      入口数据：x,y显示坐标
                *s 要显示的汉字
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese24x24(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode)
{
	uint8_t i,j,m=0;
	uint16_t k;
	uint16_t HZnum;//汉字数目
	uint16_t TypefaceNum;//一个字符所占字节大小
	uint16_t x0=x;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	HZnum=sizeof(tfont24)/sizeof(typFNT_GB24);	//统计汉字数目
	for(k=0;k<HZnum;k++) 
	{
		if ((tfont24[k].Index[0]==*(s))&&(tfont24[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(!mode)//非叠加方式
					{
						if(tfont24[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//叠加方式
					{
						if(tfont24[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//画一个点
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}				  	
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
} 

/******************************************************************************
      函数说明：显示单个32x32汉字
      入口数据：x,y显示坐标
                *s 要显示的汉字
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese32x32(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode)
{
	uint8_t i,j,m=0;
	uint16_t k;
	uint16_t HZnum;//汉字数目
	uint16_t TypefaceNum;//一个字符所占字节大小
	uint16_t x0=x;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	HZnum=sizeof(tfont32)/sizeof(typFNT_GB32);	//统计汉字数目
	for(k=0;k<HZnum;k++) 
	{
		//if ((tfont32[k].Index[0]==*(s))&&(tfont32[k].Index[1]==*(s+1))) //GB2312编码
        if ((tfont32[k].Index[0]==*(s)) && (tfont32[k].Index[1]==*(s+1)\
        && (tfont32[k].Index[2]==*(s+2)))) //UTF-8编码
		{ 	
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(!mode)//非叠加方式
					{
						if(tfont32[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//叠加方式
					{
						if(tfont32[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//画一个点
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}				  	
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}


/******************************************************************************
      函数说明：显示单个字符
      入口数据：x,y显示坐标
                num 要显示的字符
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChar(uint16_t x,uint16_t y,uint8_t num,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode)
{
	uint8_t temp,sizex,t,m=0;
	uint16_t i,TypefaceNum;//一个字符所占字节大小
	uint16_t x0=x;
	sizex=sizey/2;
	TypefaceNum=(sizex/8+((sizex%8)?1:0))*sizey;
	num=num-' ';    //得到偏移后的值
	LCD_Address_Set(x,y,x+sizex-1,y+sizey-1);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		if(sizey==12)temp=ascii_1206[num][i];		       //调用6x12字体
		else if(sizey==16)temp=ascii_1608[num][i];		 //调用8x16字体
		else if(sizey==24)temp=ascii_2412[num][i];		 //调用12x24字体
		else if(sizey==32)temp=ascii_3216[num][i];		 //调用16x32字体
		else return;
		for(t=0;t<8;t++)
		{
			if(!mode)//非叠加模式
			{
				if(temp&(0x01<<t))LCD_WR_DATA(fc);
				else LCD_WR_DATA(bc);
				m++;
				if(m%sizex==0)
				{
					m=0;
					break;
				}
			}
			else//叠加模式
			{
				if(temp&(0x01<<t))LCD_DrawPoint(x,y,fc);//画一个点
				x++;
				if((x-x0)==sizex)
				{
					x=x0;
					y++;
					break;
				}
			}
		}
	}   	 	  
}


/******************************************************************************
      函数说明：显示字符串
      入口数据：x,y显示坐标
                *p 要显示的字符串
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowString(uint16_t x,uint16_t y,const uint8_t *p,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode)
{         
	while(*p!='\0')
	{       
		LCD_ShowChar(x,y,*p,fc,bc,sizey,mode);
		x+=sizey/2;
		p++;
	}  
}


/******************************************************************************
      函数说明：显示数字
      入口数据：m底数，n指数
      返回值：  无
******************************************************************************/
uint32_t mypow(uint8_t m,uint8_t n)
{
	uint32_t result=1;	 
	while(n--)result*=m;
	return result;
}


/******************************************************************************
      函数说明：显示整数变量
      入口数据：x,y显示坐标
                num 要显示整数变量
                len 要显示的位数
                fc 字的颜色
                bc 字的背景色
                sizey 字号
      返回值：  无
******************************************************************************/
void LCD_ShowIntNum(uint16_t x,uint16_t y,uint16_t num,uint8_t len,uint16_t fc,uint16_t bc,uint8_t sizey)
{         	
	uint8_t t,temp;
	uint8_t enshow=0;
	uint8_t sizex=sizey/2;
	for(t=0;t<len;t++)
	{
		temp=(num/mypow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar(x+t*sizex,y,' ',fc,bc,sizey,0);
				continue;
			}else enshow=1; 
		 	 
		}
	 	LCD_ShowChar(x+t*sizex,y,temp+48,fc,bc,sizey,0);
	}
} 


/******************************************************************************
      函数说明：显示两位小数变量
      入口数据：x,y显示坐标
                num 要显示小数变量
                len 要显示的位数
                fc 字的颜色
                bc 字的背景色
                sizey 字号
      返回值：  无
******************************************************************************/
void LCD_ShowFloatNum1(uint16_t x,uint16_t y,float num,uint8_t len,uint16_t fc,uint16_t bc,uint8_t sizey)
{         	
	uint8_t t,temp,sizex;
	uint16_t num1;
	sizex=sizey/2;
	num1=num*100;
	for(t=0;t<len;t++)
	{
		temp=(num1/mypow(10,len-t-1))%10;
		if(t==(len-2))
		{
			LCD_ShowChar(x+(len-2)*sizex,y,'.',fc,bc,sizey,0);
			t++;
			len+=1;
		}
	 	LCD_ShowChar(x+t*sizex,y,temp+48,fc,bc,sizey,0);
	}
}


/******************************************************************************
      函数说明：显示图片
      入口数据：x,y起点坐标
                length 图片长度
                width  图片宽度
                pic[]  图片数组    
      返回值：  无
******************************************************************************/



/* 异步DMA传输函数
 * 参数：data - 要传输的数据指针
 *       size - 数据大小（字节） */
void DMA_SPI_Transmit_Async(uint8_t *data, uint16_t size)
{
    // 检查前次传输是否完成
    if(g_dma_transfer_in_progress) return;
    
    // 设置传输标志
    g_dma_transfer_in_progress = 1;
    
    // 启动DMA传输（hspi2为全局SPI句柄）
    HAL_SPI_Transmit_DMA(&hspi2, data, size);
}

/* SPI传输完成回调函数，该函数实际是在HAL_DMA_IRQHandler中调用，所以要使用中断安全的API
 * 参数：hspi - SPI句柄指针 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if(hspi->Instance == SPI2) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        g_dma_transfer_in_progress = 0;  // 清除进行中标志
        
        // 通过任务通知唤醒显示任务（继续传输下一块）
        if(xDisplayTaskHandle != NULL) {
            vTaskNotifyGiveFromISR(xDisplayTaskHandle, &xHigherPriorityTaskWoken);
        }
        
        // 如果需要，触发上下文切换
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* 启动异步传输内部函数
 * 计算剩余数据量并分块传输 */
static void vStartAsyncTransfer(void)
{
    uint32_t remaining = xDisplayParams.width * xDisplayParams.height * 2;
    remaining -= xDisplayParams.transferred;
    
    // 确定本次传输块大小（最大65534字节，DMA限制）
    uint16_t chunk_size = (remaining > 65534) ? 65534 : remaining;
    
    // 启动异步传输（偏移已传输字节数）
    DMA_SPI_Transmit_Async(
        (uint8_t*)(xDisplayParams.pic + xDisplayParams.transferred),
        chunk_size
    );
    
    // 更新已传输字节数
    xDisplayParams.transferred += chunk_size;
}



/* 显示任务主函数
 * 参数：pvParameters - FreeRTOS任务参数（未使用） */
void vDisplayTask(void *pvParameters)
{
    DisplayCommand cmd;
    const TickType_t xDMATimeout = pdMS_TO_TICKS(1000);  // 1秒超时
    
    while(1) {
        // 阻塞等待显示命令（portMAX_DELAY表示无限等待）
        if(xQueueReceive(xDisplayQueue, &cmd, portMAX_DELAY) == pdPASS) {
            // 初始化传输参数
            xDisplayParams.x = cmd.x;
            xDisplayParams.y = cmd.y;
            xDisplayParams.width = cmd.width;
            xDisplayParams.height = cmd.height;
            xDisplayParams.pic = cmd.pic;
            xDisplayParams.transferred = 0;
            
            // 设置显示区域
            LCD_Address_Set(cmd.x, cmd.y, 
                          cmd.x + cmd.width - 1, 
                          cmd.y + cmd.height - 1);
            
            // 启动首次传输
            vStartAsyncTransfer();
            
            // 循环等待所有块传输完成
            while(xDisplayParams.transferred < (cmd.width * cmd.height * 2)) {
                // 添加超时检测
                if(ulTaskNotifyTake(pdTRUE, xDMATimeout) == 0) {
                    // 超时处理
                    printf("DMA transfer timeout!\r\n");
                    g_dma_transfer_in_progress = 0;  // 重置DMA状态
                    // 可以在这里添加重试逻辑或错误处理
                    break;
                }
                
                if(xDisplayParams.transferred < (cmd.width * cmd.height * 2)) {
                    vStartAsyncTransfer();  // 继续传输下一块
                }
            }
        }
    }
}



/* 显示任务初始化函数
 * 创建消息队列和显示任务 */
void DisplayTask_Create(void)
{
    // 创建显示命令队列（长度5，每个元素大小sizeof(DisplayCommand)）
    xDisplayQueue = xQueueCreate(5, sizeof(DisplayCommand));
    
    // 创建显示任务（优先级3，堆栈512字）
    xTaskCreate(vDisplayTask,       // 任务函数
               "DisplayTask",      // 任务名称
               STACK_SIZE_DISPLAY, // 堆栈大小（单位字）
               NULL,               // 参数
               TASK_PRIORITY_DISPLAY, // 优先级
               &xDisplayTaskHandle);// 任务句柄
}

/* 异步显示图片接口函数
 * 参数：x,y - 显示位置
 *       width,height - 图片尺寸
 *       pic - 图片数据指针（需保持有效直到传输完成） */
void LCD_ShowPicture_Async(uint16_t x, uint16_t y, 
                          uint16_t width, uint16_t height,
                          uint8_t *pic)
{
    // 检查系统是否已经初始化
    if (xDisplayQueue == NULL) {
        // 系统未初始化时直接返回
        return;
    }
    
    // 系统已初始化，执行正常的异步显示流程
    DisplayCommand cmd = {
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .pic = pic
    };
    
    // 判断是否在中断中调用
    if (xPortIsInsideInterrupt()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // 使用中断安全版本发送显示命令到队列
        xQueueSendFromISR(xDisplayQueue, &cmd, &xHigherPriorityTaskWoken);
        
        // 如果需要，触发上下文切换
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
        // 非中断环境下使用普通版本
        xQueueSend(xDisplayQueue, &cmd, portMAX_DELAY);
    }
}

void LCD_SHOW_TEST2(void)
{
    // uint8_t i,j;
    // for(j=0;j<5;j++)
    // {
    //     for(i=0;i<6;i++)
    //     {
    //         LCD_ShowPicture_Async(40*i,120+j*40,40,40,gImage_1);
    //     }
    // }
    //HAL_Delay(1000);
    LCD_ShowPicture_Async(0,0,240,320,(uint8_t *)gImage_2);
}

/******************************************************************************
      函数说明：在指定区域填充颜色
      入口数据：xsta,ysta   起始坐标
                xend,yend   终止坐标
                color       要填充的颜色
      返回值：  无
******************************************************************************/
void LCD_Fill_DMA(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color)
{
    // 计算需要填充的像素总数
    uint32_t pixelCount = (xend - xsta) * (yend - ysta);
    
    // 设置LCD显示区域
    LCD_Address_Set(xsta, ysta, xend - 1, yend - 1);
    
    // 定义每次DMA传输的像素块大小（256个像素，即512字节）
    #define FILL_CHUNK_SIZE 256
    uint16_t fillBuffer[FILL_CHUNK_SIZE]  __attribute__((aligned(4)));;
    
    // 用指定的颜色填充缓冲区
    for (int i = 0; i < FILL_CHUNK_SIZE; i++)
    {
        fillBuffer[i] = color;
    }
    
    // 计算传输次数：完整块和余数块
    uint32_t fullChunks = pixelCount / FILL_CHUNK_SIZE;
    uint32_t remainder = pixelCount % FILL_CHUNK_SIZE;
    
    // 传输所有完整块
    for (uint32_t i = 0; i < fullChunks; i++)
    {
        // 启动 DMA 传输前，将状态标志置1
        g_dma_transfer_in_progress = 1;
        
        // 以DMA方式传输一个块（字节数 = FILL_CHUNK_SIZE * 2）
        HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)fillBuffer, FILL_CHUNK_SIZE * sizeof(uint16_t));
        
        // 等待本次传输完成，注意此处用忙等待（在实际应用中建议用任务通知或信号量）
        while(g_dma_transfer_in_progress);
    }
    
    // 如果还有余下不足一个块的像素，则传输剩余部分
    if(remainder)
    {
        g_dma_transfer_in_progress = 1;
        HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)fillBuffer, remainder * sizeof(uint16_t));
        while(g_dma_transfer_in_progress);
    }
}


void LCD_SHOW(void)
{
    LCD_Fill_DMA(0,0,LCD_W,LCD_H,WHITE); //使用DMA填充
    LCD_ShowChinese(0,0,(uint8_t*)"欢迎回家",RED,WHITE,32,0);
    LCD_ShowString(0,40,(const uint8_t*)"LCD_W:",RED,WHITE,16,0);
    LCD_ShowIntNum(48,40,LCD_W,3,RED,WHITE,16);
    LCD_ShowString(80,40,(const uint8_t*)"LCD_H:",RED,WHITE,16,0);
    LCD_ShowIntNum(128,40,LCD_H,3,RED,WHITE,16);
    LCD_ShowString(80,40,(const uint8_t*)"LCD_H:",RED,WHITE,16,0);
    LCD_ShowString(0,70,(const uint8_t*)"Increaseing Nun:",RED,WHITE,16,0);
    HAL_Delay(1000);
}


