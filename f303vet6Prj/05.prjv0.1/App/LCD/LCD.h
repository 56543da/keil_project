/*********************************************************************************************************
* 模块名称：LCD.h
* 摘    要：LCD 驱动头文件，支持 MCU 8080 并行接口 TFT 液晶屏
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2025年12月30日
* 内    容：
* 注    意：支持 ST7796/NT35310/NT35510/ST7789 等驱动芯片
**********************************************************************************************************
* 取代版本：
* 作    者：
* 完成日期：2025年12月30日
* 修改内容：
* 修改文件：
**********************************************************************************************************/

#ifndef __LCD_H__
#define __LCD_H__

#include "gd32f30x_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <Datatype.h>

/*********************************************************************************************************
*                                              LCD 参数结构体
**********************************************************************************************************/
typedef struct
{
    u16 width;      /* LCD 宽度 */
    u16 height;     /* LCD 高度 */
    u16 id;         /* LCD ID */
    u8  dir;        /* 显示方向：0 竖屏，1 横屏 */
    u16 wramcmd;    /* 开始写 GRAM 指令 */
    u16 setxcmd;    /* 设置 X 坐标指令 */
    u16 setycmd;    /* 设置 Y 坐标指令 */
} _lcd_dev;

/*********************************************************************************************************
*                                              全局变量声明
**********************************************************************************************************/
extern _lcd_dev lcddev;      /* LCD 设备管理结构体 */
extern u16 POINT_COLOR;      /* 笔色（前景色），默认黑色 */
extern u16 BACK_COLOR;       /* 背景色，默认白色 */

/*********************************************************************************************************
*                                              颜色定义
**********************************************************************************************************/
#define WHITE           0xFFFF
#define BLACK           0x0000
#define BLUE            0x001F
#define RED             0xF800
#define MAGENTA         0xF81F
#define GREEN           0x07E0
#define CYAN            0x7FFF
#define YELLOW          0xFFE0
#define BROWN           0XBC40
#define GRAY            0X8430
#define LIGHTBLUE       0X7D7C
#define GRAYBLUE        0X5458
#define LIGHTGREEN      0X841F
#define LGRAY           0XC618
#define LGRAYBLUE       0XA651

/*********************************************************************************************************
*                                              GPIO 引脚定义（针对 GD32F303）
*                                          根据原理图实际 IO 映射配置
**********************************************************************************************************/
/* 
LCD 接口说明：8080 并行接口，16 位数据线
数据线分布：
- DB0 (PD14)   DB1 (PD15)  DB4 (PE7)   DB5 (PE8)   DB6 (PE9)  DB7 (PE10)
- DB8 (PE11)   DB9 (PE12) DB10 (PE13) DB11 (PE14) DB12 (PE15) DB13 (PD8)
- DB14 (PD9)  DB15 (PD10)

控制线：
- CS   (PC5)    RS (PD11)
- WR/RD (需确认，当前使用虚拟定义)
*/

/* 背光控制：PB0 */
#define LCD_LED_PORT        GPIOB
#define LCD_LED_PIN         GPIO_PIN_0

/* 复位：PA12 */
#define LCD_RST_PORT        GPIOA
#define LCD_RST_PIN         GPIO_PIN_12

/* 片选：PD7 */
#define LCD_CS_SET()        gpio_bit_set(GPIOD, GPIO_PIN_7)
#define LCD_CS_CLR()        gpio_bit_reset(GPIOD, GPIO_PIN_7)

/* 数据/命令线：PD11 */
#define LCD_RS_SET()        gpio_bit_set(GPIOD, GPIO_PIN_11)
#define LCD_RS_CLR()        gpio_bit_reset(GPIOD, GPIO_PIN_11)

/* 写使能和读使能（当前使用 PD5/PD4 作为占位符，实际需根据原理图确认） */
#define LCD_WR_SET()        gpio_bit_set(GPIOD, GPIO_PIN_5)
#define LCD_WR_CLR()        gpio_bit_reset(GPIOD, GPIO_PIN_5)

#define LCD_RD_SET()        gpio_bit_set(GPIOD, GPIO_PIN_4)
#define LCD_RD_CLR()        gpio_bit_reset(GPIOD, GPIO_PIN_4)

/*
数据线读写宏：
由于数据线分散在 PE 和 PD，无法通过单个端口寄存器操作
需要通过函数接口处理数据的分散映射
*/
#define LCD_DATAIN()        LCD_ReadDataPort()      /* 调用函数读取分散的数据线 */
#define LCD_DATAOUT(x)      LCD_WriteDataPort(x)    /* 调用函数写入分散的数据线 */

/* 数据线定义（用于初始化和读写函数） */
#define LCD_DB0_PIN    GPIO_PIN_14   /* PD14 */
#define LCD_DB1_PIN    GPIO_PIN_15   /* PD15 */
#define LCD_DB2_PIN    GPIO_PIN_0    /* PD0 */
#define LCD_DB3_PIN    GPIO_PIN_1    /* PD1 */
#define LCD_DB4_PIN    GPIO_PIN_7    /* PE7 */
#define LCD_DB5_PIN    GPIO_PIN_8    /* PE8 */
#define LCD_DB6_PIN    GPIO_PIN_9    /* PE9 */
#define LCD_DB7_PIN    GPIO_PIN_10   /* PE10 */
#define LCD_DB8_PIN    GPIO_PIN_11   /* PE11 */
#define LCD_DB9_PIN    GPIO_PIN_12   /* PE12 */
#define LCD_DB10_PIN   GPIO_PIN_13   /* PE13 */
#define LCD_DB11_PIN   GPIO_PIN_14   /* PE14 */
#define LCD_DB12_PIN   GPIO_PIN_15   /* PE15 */
#define LCD_DB13_PIN   GPIO_PIN_8    /* PD8 */
#define LCD_DB14_PIN   GPIO_PIN_9    /* PD9 */
#define LCD_DB15_PIN   GPIO_PIN_10   /* PD10 */

/*********************************************************************************************************
*                                              扫描方向定义
**********************************************************************************************************/
#define L2R_U2D  0  /* 从左到右，从上到下 */
#define L2R_D2U  1  /* 从左到右，从下到上 */
#define R2L_U2D  2  /* 从右到左，从上到下 */
#define R2L_D2U  3  /* 从右到左，从下到上 */
#define U2D_L2R  4  /* 从上到下，从左到右 */
#define U2D_R2L  5  /* 从上到下，从右到左 */
#define D2U_L2R  6  /* 从下到上，从左到右 */
#define D2U_R2L  7  /* 从下到上，从右到左 */

#define DFT_SCAN_DIR  L2R_U2D  /* 默认扫描方向 */

/*********************************************************************************************************
*                                              API 函数声明
**********************************************************************************************************/
void LCD_Init(void);                                           /* 初始化 LCD */
void LCD_DisplayOn(void);                                      /* 显示打开 */
void LCD_DisplayOff(void);                                     /* 显示关闭 */
void LCD_Clear(u16 Color);                                     /* 清屏 */
void LCD_SetCursor(u16 Xpos, u16 Ypos);                        /* 设置光标位置 */
void LCD_DrawPoint(u16 x, u16 y);                              /* 画点 */
void LCD_Fast_DrawPoint(u16 x, u16 y, u16 color);              /* 快速画点 */
u16  LCD_ReadPoint(u16 x, u16 y);                              /* 读点 */
void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color);      /* 单色填充 */
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);             /* 画线 */
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);        /* 画矩形 */
void LCD_Draw_Circle(u16 x0, u16 y0, u8 r);                    /* 画圆 */

/* 文字显示函数 */
void LCD_ShowChar(u16 x, u16 y, u8 num, u8 size, u8 mode);                /* 显示字符 */
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size);                  /* 显示数字 */
void LCD_ShowxNum(u16 x, u16 y, u32 num, u8 len, u8 size, u8 mode);        /* 显示带填充的数字 */
void LCD_ShowString(u16 x, u16 y, u16 width, u16 height, u8 size, u8 *p); /* 显示字符串 */

/* 寄存器读写函数 */
void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue);  /* 写寄存器 */
u16 LCD_ReadReg(u16 LCD_Reg);                      /* 读寄存器 */
void LCD_WriteRAM_Prepare(void);                   /* 准备写 GRAM */
void LCD_WriteRAM(u16 RGB_Code);                   /* 写 GRAM */

/* 设置相关函数 */
void LCD_Scan_Dir(u8 dir);                         /* 设置扫描方向 */
void LCD_Display_Dir(u8 dir);                      /* 设置显示方向 */
void LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height);  /* 设置窗口 */

#endif /* __LCD_H__ */
