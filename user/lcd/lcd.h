#ifndef __LCD_H
#define __LCD_H

#include "stm32f7xx_hal.h"
#include "lcd_init.h"  // 包含基础LCD初始化文件
//画笔颜色
#define WHITE            0xFFFF
#define BLACK            0x0000   
#define BLUE             0x001F  
#define BRED             0xF81F
#define GRED             0xFFE0
#define GBLUE            0x07FF
#define RED              0xF800
#define MAGENTA          0xF81F
#define GREEN            0x07E0
#define CYAN             0x7FFF
#define YELLOW           0xFFE0
#define BROWN            0xBC40  //棕色
#define BRRED            0xFC07  //棕红色
#define GRAY             0x8430  //灰色
#define DARKBLUE         0x01CF  //深蓝色
#define LIGHTBLUE        0x7D7C  //浅蓝色  
#define GRAYBLUE         0x5458  //灰蓝色
#define LIGHTGREEN       0x841F  //浅绿色
#define LGRAY            0xC618  //浅灰色(PANNEL),窗体背景色
#define LGRAYBLUE        0xA651  //浅灰蓝色(中间层颜色)
#define LBBLUE           0x2B12  //浅棕蓝色(选择条目的反色)


/* 显示命令队列元素
 * 用于任务间传递显示请求 */
typedef struct {
    uint16_t x;                 // 显示区域左上角X坐标
    uint16_t y;                 // 显示区域左上角Y坐标
    uint16_t width;            // 显示区域宽度
    uint16_t height;           // 显示区域高度
    uint8_t *pic;         // 图像数据指针（需保证传输期间有效）
} DisplayCommand;

void LCD_Fill(uint16_t xsta,uint16_t ysta,uint16_t xend,uint16_t yend,uint16_t color);
void LCD_Fill_DMA(uint16_t xsta,uint16_t ysta,uint16_t xend,uint16_t yend,uint16_t color);
void LCD_DrawPoint(uint16_t x,uint16_t y,uint16_t color);
void LCD_DrawLine(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t color);
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,uint16_t color);
void Draw_Circle(uint16_t x0,uint16_t y0,uint8_t r,uint16_t color);

void LCD_ShowChinese(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void LCD_ShowChinese12x12(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void LCD_ShowChinese16x16(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void LCD_ShowChinese24x24(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void LCD_ShowChinese32x32(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);

void LCD_ShowChar(uint16_t x,uint16_t y,uint8_t num,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void LCD_ShowString(uint16_t x,uint16_t y,const uint8_t *p,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
uint32_t mypow(uint8_t m,uint8_t n);
void LCD_ShowIntNum(uint16_t x,uint16_t y,uint16_t num,uint8_t len,uint16_t fc,uint16_t bc,uint8_t sizey);
void LCD_ShowFloatNum1(uint16_t x,uint16_t y,float num,uint8_t len,uint16_t fc,uint16_t bc,uint8_t sizey);
void LCD_ShowPicture(uint16_t x,uint16_t y,uint16_t length,uint16_t width,const uint8_t pic[]);
void LCD_SHOW_TEST(void);
void LCD_SHOW(void);
void LCD_SHOW_TEST2(void);
void DisplayTask_Create(void);
void LCD_QueueDisplayCommand (uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t *pic);
#endif





