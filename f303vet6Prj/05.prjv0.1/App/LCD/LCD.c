/*********************************************************************************************************
* 模块名称：LCD.c
* 摘    要：LCD 驱動實現文件，使用 MCU 8080 並行接口驅動 TFT 液晶屏
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2025年12月30日
* 内    容：
* 注    意：
**********************************************************************************************************
* 取代版本：
* 作    者：
* 完成日期：
* 修改内容：
* 修改文件：
**********************************************************************************************************/

/*********************************************************************************************************
*                                              包含头文件
**********************************************************************************************************/
#include "LCD.h"
#include "Font.h"
#include <stdio.h>
#include <math.h>
#include "SysTick.h"

/*********************************************************************************************************
*                                              全局变量声明
**********************************************************************************************************/
u16 POINT_COLOR = 0x0000;   /* 画笔颜色，默认黑色 */
u16 BACK_COLOR = 0xFFFF;    /* 背景色，默认白色 */
_lcd_dev lcddev;            /* LCD 设备管理结构体 */



/*********************************************************************************************************
*                                              内部函数声明
**********************************************************************************************************/
static void LCD_GPIO_Init(void);                /* GPIO 初始化 */
static void LCD_WriteReg_Internal(u16 data);    /* 写寄存器 */
static void LCD_WriteData_Internal(u16 data);   /* 写数据 */
static u16 LCD_ReadData_Internal(void);         /* 读数据 */
static u16 LCD_ReadDataPort(void);              /* 读取数据端口并返回16位数据 */
static void LCD_WriteDataPort(u16 data);        /* 向数据端口写入16位数据 */

/*********************************************************************************************************
*                                              内部函数实现
**********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：LCD_GPIO_Init
* 函数功能：配置 LCD 相关的 LCD GPIO 引脚
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
static void LCD_GPIO_Init(void)
{
    /* 使能 GPIO 时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);  /* PA12: LCD_RST */
    rcu_periph_clock_enable(RCU_GPIOB);  /* PB0: LCD_LED */
    rcu_periph_clock_enable(RCU_GPIOD);  /* PD: LCD_CS, LCD_RS, LCD_WR, LCD_RD, 部分数据线 */
    rcu_periph_clock_enable(RCU_GPIOE);  /* PE: 部分数据线 DB4-12 */
    
    /* 配置 PA12 推挽输出，控制 LCD_RST 引脚 */
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12);
    
    /* 配置 PB0 推挽输出，控制 LCD_LED 背光引脚 */
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
    
    /* 配置 PD 的控制引脚和数据引脚 */
    gpio_init(GPIOD, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              (GPIO_PIN_0 |  GPIO_PIN_1 |   /* PD0: DB2, PD1: DB3 */
               GPIO_PIN_4 |  GPIO_PIN_5 |   /* PD4: RD, PD5: WR */
               GPIO_PIN_7 |                   /* PD7: CS */
               GPIO_PIN_8 | GPIO_PIN_9 |     /* PD8: DB13, PD9: DB14 */
               GPIO_PIN_10 | GPIO_PIN_11 |   /* PD10: DB15, PD11: RS */
               GPIO_PIN_14 | GPIO_PIN_15));  /* PD14: DB0, PD15: DB1 */
    
    /* 配置 PE 的数据引脚 */
    gpio_init(GPIOE, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              (GPIO_PIN_7 |  GPIO_PIN_8 |   /* PE7: DB4, PE8: DB5 */
               GPIO_PIN_9 |  GPIO_PIN_10 |  /* PE9: DB6, PE10: DB7 */
               GPIO_PIN_11 | GPIO_PIN_12 |  /* PE11: DB8, PE12: DB9 */
               GPIO_PIN_13 | GPIO_PIN_14 |  /* PE13: DB10, PE14: DB11 */
               GPIO_PIN_15));               /* PE15: DB12 */
    
    /* 初始化 LCD 控制引脚为高电平（空闲状态） */
    LCD_CS_SET();  /* CS 无效 */
    LCD_RS_SET();  /* RS 高电平 */
    LCD_WR_SET();  /* WR 无效 */
    LCD_RD_SET();  /* RD 无效 */
}

/*********************************************************************************************************
* 函数名称：LCD_WriteReg_Internal
* 函数功能：写 LCD 寄存器地址
* 输入参数：data - 寄存器地址
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
static void LCD_WriteReg_Internal(u16 data)
{
    LCD_RS_CLR();  /* RS 低电平选择写寄存器 */
    LCD_CS_CLR();  /* CS 片选有效 */
    LCD_WriteDataPort(data);  /* 向数据端口写入寄存器地址 */
    LCD_WR_CLR();  /* WR 写使能 */
    LCD_WR_SET();
    LCD_CS_SET();  /* CS 无效 */
}

/*********************************************************************************************************
* 函数名称：LCD_WriteData_Internal
* 函数功能：写 LCD 数据
* 输入参数：data - 数据
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
static void LCD_WriteData_Internal(u16 data)
{
    LCD_RS_SET();  /* RS 高电平选择写数据 */
    LCD_CS_CLR();  /* CS 片选有效 */
    LCD_WriteDataPort(data);  /* 向数据端口写入数据 */
    LCD_WR_CLR();  /* WR 写使能 */
    LCD_WR_SET();
    LCD_CS_SET();  /* CS 无效 */
}

/*********************************************************************************************************
* 函数名称：LCD_WriteDataPort
* 函数功能：向分散的数据端口写入16位数据
* 输入参数：data - 16位数据DB15-DB0位
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
* 数据线映射关系：
*   DB0  (PD14),   DB1  (PD15)   DB2/3 -> PD0/PD1
*   DB4  (PE7),    DB5  (PE8),   DB6  (PE9),   DB7  (PE10)
*   DB8  (PE11),   DB9  (PE12),  DB10 (PE13),  DB11 (PE14)
*   DB12 (PE15),   DB13 (PD8),   DB14 (PD9),   DB15 (PD10)
**********************************************************************************************************/
static void LCD_WriteDataPort(u16 data)
{
    u16 pd_data, pe_data;
    
    /* 配置 PD 端口数据线 */
    pd_data = gpio_output_port_get(GPIOD);
    pd_data &= ~(GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15);  /* 清除相关位 */
    pd_data |= ((data & 0x0003) << 14) |  /* DB0(bit0) -> PD14, DB1(bit1) -> PD15 */
               ((data & 0x000C) >> 2) |    /* DB2(bit2) -> PD0, DB3(bit3) -> PD1 */
               ((data & 0x2000) >> 5) |   /* DB13(bit13) -> PD8 */
               ((data & 0x4000) >> 5) |   /* DB14(bit14) -> PD9 */
               ((data & 0x8000) >> 5);    /* DB15(bit15) -> PD10 */
    gpio_port_write(GPIOD, pd_data);
    
    /* 配置 PE 端口数据线 */
    pe_data = gpio_output_port_get(GPIOE);
    pe_data &= ~(GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | 
                 GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    pe_data |= ((data & 0x00F0) << 3) |   /* DB4-7(bits4-7) -> PE7-10 */
               ((data & 0x0F00) << 3) |   /* DB8-11(bits8-11) -> PE11-14 */
               ((data & 0x1000) << 3);    /* DB12(bit12) -> PE15 */
    gpio_port_write(GPIOE, pe_data);
}

/*********************************************************************************************************
* 函数名称：LCD_ReadDataPort
* 函数功能：从分散的数据端口读取16位数据
* 输入参数：void
* 输出参数：void
* 返 回 值：16位数据DB15-DB0位
* 完成日期：2025年12月30日
* 注    意：数据位映射关系与 LCD_WriteDataPort 相反
**********************************************************************************************************/
static u16 LCD_ReadDataPort(void)
{
    u16 data = 0;
    u16 pd_data, pe_data;
    
    /* 配置数据线为输入模式 */
    gpio_init(GPIOD, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, 
              (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15));
    gpio_init(GPIOE, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ,
              (GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | 
               GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15));
    
    /* 读取 PD 端口数据 */
    pd_data = gpio_input_port_get(GPIOD);
    data |= ((pd_data >> 14) & 0x0003) |  /* PD14-15 -> DB0-1 */
            ((pd_data & 0x0003) << 2) |     /* PD0-1 -> DB2-3 */
            ((pd_data & GPIO_PIN_8) << 5) |    /* PD8 -> DB13 */
            ((pd_data & GPIO_PIN_9) << 5) |     /* PD9 -> DB14 */
            ((pd_data & GPIO_PIN_10) << 5);     /* PD10 -> DB15 */
    
    /* 读取 PE 端口数据 */
    pe_data = gpio_input_port_get(GPIOE);
    data |= ((pe_data >> 3) & 0x00F0) |   /* PE7-10 -> DB4-7 */
            ((pe_data >> 3) & 0x0F00) |   /* PE11-14 -> DB8-11 */
            ((pe_data >> 3) & 0x1000);    /* PE15 -> DB12 */
    
    /* 恢复数据线为输出模式 */
    gpio_init(GPIOD, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15));
    gpio_init(GPIOE, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              (GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | 
               GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15));
    
    return data;
}

/*********************************************************************************************************
* 函数名称：LCD_ReadData_Internal
* 函数功能：读 LCD 数据
* 输入参数：void
* 输出参数：void
* 返 回 值：读取到的16位数据
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
static u16 LCD_ReadData_Internal(void)
{
    volatile u16 data = 0;
    volatile u8 i;
    
    LCD_RS_SET();  /* RS 高电平选择读数据 */
    LCD_CS_CLR();  /* CS 有效 */
    LCD_RD_CLR();  /* RD 读使能 */
    
    /* 延时稳定 */
    for (i = 0; i < 10; i++) {
        __nop();
    }

    
    data = LCD_ReadDataPort();  /* 读取数据端口数据 */
    LCD_RD_SET();
    LCD_CS_SET();
    
    return data;
}

/*********************************************************************************************************
*                                              API 函数实现
**********************************************************************************************************/

/*********************************************************************************************************
* 函数名称：LCD_WriteReg
* 函数功能：写寄存器
* 输入参数：LCD_Reg - 寄存器地址，LCD_RegValue - 要写入的数据
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue)
{
    LCD_WriteReg_Internal(LCD_Reg);
    LCD_WriteData_Internal(LCD_RegValue);
}

/*********************************************************************************************************
* 函数名称：LCD_ReadReg
* 函数功能：读寄存器数据
* 输入参数：LCD_Reg - 寄存器地址
* 输出参数：void
* 返 回 值：寄存器数据
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
u16 LCD_ReadReg(u16 LCD_Reg)
{
    LCD_WriteReg_Internal(LCD_Reg);
    return LCD_ReadData_Internal();
}

/*********************************************************************************************************
* 函数名称：LCD_WriteRAM_Prepare
* 函数功能：准备写 GRAM
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_WriteRAM_Prepare(void)
{
    LCD_WriteReg_Internal(lcddev.wramcmd);
}

/*********************************************************************************************************
* 函数名称：LCD_WriteRAM
* 函数功能：写 GRAM 数据
* 输入参数：RGB_Code - 16位RGB数据
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_WriteRAM(u16 RGB_Code)
{
    LCD_WriteData_Internal(RGB_Code);
}

/*********************************************************************************************************
* 函数名称：LCD_SetCursor
* 函数功能：设置光标位置
* 输入参数：Xpos - X 坐标，Ypos - Y 坐标
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：参考不同LCD驱动的坐标设置方式
**********************************************************************************************************/
void LCD_SetCursor(u16 Xpos, u16 Ypos)
{
    if (lcddev.id == 0X5510)
    {
        LCD_WriteReg_Internal(lcddev.setxcmd);
        LCD_WriteData_Internal(Xpos >> 8);
        LCD_WriteReg_Internal(lcddev.setxcmd + 1);
        LCD_WriteData_Internal(Xpos & 0XFF);
        LCD_WriteReg_Internal(lcddev.setycmd);
        LCD_WriteData_Internal(Ypos >> 8);
        LCD_WriteReg_Internal(lcddev.setycmd + 1);
        LCD_WriteData_Internal(Ypos & 0XFF);
    }
    else     /* 7796/5310/7789统一处理 */
    {
        LCD_WriteReg_Internal(lcddev.setxcmd);
        LCD_WriteData_Internal(Xpos >> 8);
        LCD_WriteData_Internal(Xpos & 0XFF);
        LCD_WriteReg_Internal(lcddev.setycmd);
        LCD_WriteData_Internal(Ypos >> 8);
        LCD_WriteData_Internal(Ypos & 0XFF);
    }
}

/*********************************************************************************************************
* 函数名称：LCD_Clear
* 函数功能：清屏
* 输入参数：Color - 清屏颜色
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_Clear(u16 Color)
{
    u32 index = 0;
    u32 totalpoint = lcddev.width * lcddev.height;
    
    LCD_SetCursor(0, 0);
    LCD_WriteRAM_Prepare();
    
    for (index = 0; index < totalpoint; index++)
    {
        LCD_WriteRAM(Color);
    }
}

/*********************************************************************************************************
* 函数名称：LCD_Fill
* 函数功能：填充指定区域的颜色
* 输入参数：sx/sy - 起始坐标，ex/ey - 结束坐标，color - 颜色
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color)
{
    u16 i, j;
    u16 xlen = ex - sx + 1;
    
    for (i = sy; i <= ey; i++)
    {
        LCD_SetCursor(sx, i);
        LCD_WriteRAM_Prepare();
        for (j = 0; j < xlen; j++)
        {
            LCD_WriteRAM(color);
        }
    }
}

/*********************************************************************************************************
* 函数名称：LCD_DrawPoint
* 函数功能：画点
* 输入参数：x/y - 坐标
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_DrawPoint(u16 x, u16 y)
{
    LCD_SetCursor(x, y);
    LCD_WriteRAM_Prepare();
    LCD_WriteRAM(POINT_COLOR);
}

/*********************************************************************************************************
* 函数名称：LCD_Fast_DrawPoint
* 函数功能：快速画点
* 输入参数：x/y - 坐标，color - 颜色
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_Fast_DrawPoint(u16 x, u16 y, u16 color)
{
    LCD_SetCursor(x, y);
    LCD_WriteRAM_Prepare();
    LCD_WriteRAM(color);
}

/*********************************************************************************************************
* 函数名称：LCD_ReadPoint
* 函数功能：读点
* 输入参数：x/y - 坐标
* 输出参数：void
* 返 回 值：颜色值
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
u16 LCD_ReadPoint(u16 x, u16 y)
{
    u16 color = 0;
    
    if (x >= lcddev.width || y >= lcddev.height)
        return 0;
    
    LCD_SetCursor(x, y);
    LCD_WriteReg_Internal(0x2E);  /* 读 GRAM 指令 */
    color = LCD_ReadData_Internal();
    
    return color;
}

/*********************************************************************************************************
* 函数名称：LCD_DrawLine
* 函数功能：画线
* 输入参数：x1/y1 - 起始坐标，x2/y2 - 结束坐标
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
{
    u16 t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;
    
    if (delta_x > 0) incx = 1;
    else if (delta_x == 0) incx = 0;
    else { incx = -1; delta_x = -delta_x; }
    
    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0;
    else { incy = -1; delta_y = -delta_y; }
    
    distance = (delta_x > delta_y) ? delta_x : delta_y;
    
    for (t = 0; t <= distance + 1; t++)
    {
        LCD_DrawPoint(uRow, uCol);
        xerr += delta_x;
        yerr += delta_y;
        
        if (xerr > distance) { xerr -= distance; uRow += incx; }
        if (yerr > distance) { yerr -= distance; uCol += incy; }
    }
}

/*********************************************************************************************************
* 函数名称：LCD_DrawRectangle
* 函数功能：画矩形
* 输入参数：x1/y1 - 角点1坐标，x2/y2 - 角点2坐标
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
    LCD_DrawLine(x1, y1, x2, y1);
    LCD_DrawLine(x1, y1, x1, y2);
    LCD_DrawLine(x1, y2, x2, y2);
    LCD_DrawLine(x2, y1, x2, y2);
}

/*********************************************************************************************************
* 函数名称：LCD_Draw_Circle
* 函数功能：画圆
* 输入参数：x0/y0 - 圆心坐标，r - 半径
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_Draw_Circle(u16 x0, u16 y0, u8 r)
{
    int a = 0, b = r;
    int di = 3 - (r << 1);
    
    while (a <= b)
    {
        LCD_DrawPoint(x0 + a, y0 - b);
        LCD_DrawPoint(x0 + b, y0 - a);
        LCD_DrawPoint(x0 + b, y0 + a);
        LCD_DrawPoint(x0 + a, y0 + b);
        LCD_DrawPoint(x0 - a, y0 + b);
        LCD_DrawPoint(x0 - b, y0 + a);
        LCD_DrawPoint(x0 - a, y0 - b);
        LCD_DrawPoint(x0 - b, y0 - a);
        a++;
        
        if (di < 0) di += 4 * a + 6;
        else { di += 10 + 4 * (a - b); b--; }
    }
}

/*********************************************************************************************************
* 函数名称：LCD_ShowChar
* 函数功能：显示单个字符
* 输入参数：x/y - 坐标，num - 字符，size - 字体大小，mode - 模式
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：支持 16x16 字体
*         mode: 0=反白显示，1=正常显示
**********************************************************************************************************/
void LCD_ShowChar(u16 x, u16 y, u8 num, u8 size, u8 mode)
{
    u8 temp, t1, t;
    u16 y0 = y;
    /* u8 csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);  暂时未使用，注释掉 */
    
    num = num - ' ';
    
    /* 显示 16x16 字体 */
    if (size == 16)
    {
        /* 使用 Font.h 中的 16x16 字体 */
        for (t = 0; t < 16; t++)
        {
            temp = asc2_1608[num][t];
            for (t1 = 0; t1 < 8; t1++)
            {
                if (temp & 0x80)
                    LCD_Fast_DrawPoint(x, y, POINT_COLOR);
                else if (mode == 0)
                    LCD_Fast_DrawPoint(x, y, BACK_COLOR);
                
                temp <<= 1;
                y++;
                
                if (y >= lcddev.height) return;
                if ((y - y0) == size) { y = y0; x++; break; }
            }
        }
    }
}

/*********************************************************************************************************
* 函数名称：LCD_ShowNum
* 函数功能：显示数字
* 输入参数：x/y - 坐标，num - 数字，len - 位数，size - 字体大小
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size)
{
    u8 t, temp;
    u8 enshow = 0;
    
    for (t = 0; t < len; t++)
    {
        temp = (num / (u32)pow(10, len - t - 1)) % 10;
        
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0) { LCD_ShowChar(x + (size / 2) * t, y, ' ', size, 0); continue; }
            else enshow = 1;
        }
        
        LCD_ShowChar(x + (size / 2) * t, y, temp + '0', size, 0);
    }
}

/*********************************************************************************************************
* 函数名称：LCD_ShowxNum
* 函数功能：显示数字，可设置填充
* 输入参数：x/y - 坐标，num - 数字，len - 位数，size - 字体大小，mode - 模式
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_ShowxNum(u16 x, u16 y, u32 num, u8 len, u8 size, u8 mode)
{
    u8 t, temp;
    u8 enshow = 0;
    
    for (t = 0; t < len; t++)
    {
        temp = (num / (u32)pow(10, len - t - 1)) % 10;
        
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                if (mode & 0x80)
                    LCD_ShowChar(x + (size / 2) * t, y, '0', size, mode & 0x01);
                else
                    LCD_ShowChar(x + (size / 2) * t, y, ' ', size, mode & 0x01);
                continue;
            }
            else enshow = 1;
        }
        
        LCD_ShowChar(x + (size / 2) * t, y, temp + '0', size, mode & 0x01);
    }
}

/*********************************************************************************************************
* 函数名称：LCD_ShowString
* 函数功能：显示字符串
* 输入参数：x/y - 坐标，width/height - 显示区域，size - 字体大小，*p - 字符串指针
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_ShowString(u16 x, u16 y, u16 width, u16 height, u8 size, u8 *p)
{
    u8 x0 = x;
    width += x;
    height += y;
    
    while ((*p <= '~') && (*p >= ' '))
    {
        if (x >= width) { x = x0; y += size; }
        if (y >= height) break;
        
        LCD_ShowChar(x, y, *p, size, 0);
        x += size / 2;
        p++;
    }
}

/*********************************************************************************************************
* 函数名称：LCD_Scan_Dir
* 函数功能：设置扫描方向
* 输入参数：dir - 扫描方向
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：参考不同驱动芯片的扫描方向设置
**********************************************************************************************************/
void LCD_Scan_Dir(u8 dir)
{
    u16 regval = 0;
    u16 dirreg = 0;
    u16 temp;

    /* 根据当前显示方向调整IC扫描方向 */
    if (lcddev.dir == 1)
    {
        switch (dir)
        {
            case 0: dir = 6; break;
            case 1: dir = 7; break;
            case 2: dir = 4; break;
            case 3: dir = 5; break;
            case 4: dir = 1; break;
            case 5: dir = 0; break;
            case 6: dir = 3; break;
            case 7: dir = 2; break;
        }
    }

    switch (dir)
    {
        case L2R_U2D:
            regval |= (0 << 7) | (0 << 6) | (0 << 5);
            break;
        case L2R_D2U:
            regval |= (1 << 7) | (0 << 6) | (0 << 5);
            break;
        case R2L_U2D:
            regval |= (0 << 7) | (1 << 6) | (0 << 5);
            break;
        case R2L_D2U:
            regval |= (1 << 7) | (1 << 6) | (0 << 5);
            break;
        case U2D_L2R:
            regval |= (0 << 7) | (0 << 6) | (1 << 5);
            break;
        case U2D_R2L:
            regval |= (0 << 7) | (1 << 6) | (1 << 5);
            break;
        case D2U_L2R:
            regval |= (1 << 7) | (0 << 6) | (1 << 5);
            break;
        case D2U_R2L:
            regval |= (1 << 7) | (1 << 6) | (1 << 5);
            break;
    }

    if (lcddev.id == 0X5510)
        dirreg = 0X3600;
    else
        dirreg = 0X36;

    /* 7796 & 7789 设置 BGR 位以修复红蓝反色问题 */
    if (lcddev.id == 0X7796 || lcddev.id == 0X7789)
    {
        /* 
         * ST7796 寄存器 0x36(MADCTL) 的 Bit 3 是 BGR 控制位：
         * 0 = RGB order, 1 = BGR order.
         * 根据实际屏幕表现，这里强制取反或者取消这位置位来适配。
         * 如果之前是 |= 0x08 导致偏蓝，现在尝试取消这个位 (即 0x00，使用 RGB)
         */
        regval &= ~0X08; 
    }

    LCD_WriteReg(dirreg, regval);

    /* 根据扫描方向调整显示参数 */
    if (regval & 0X20)
    {
        if (lcddev.width < lcddev.height)
        {
            temp = lcddev.width;
            lcddev.width = lcddev.height;
            lcddev.height = temp;
        }
    }
    else
    {
        if (lcddev.width > lcddev.height)
        {
            temp = lcddev.width;
            lcddev.width = lcddev.height;
            lcddev.height = temp;
        }
    }
}

/*********************************************************************************************************
* 函数名称：LCD_Display_Dir
* 函数功能：设置显示方向
* 输入参数：dir - 显示方向
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_Display_Dir(u8 dir)
{
    lcddev.dir = dir;
    
    if (lcddev.id == 0x5510)
    {
        /* NT35510: 800x480 */
        if (dir == 0)  /* 竖屏 */
        {
            lcddev.width = 480;
            lcddev.height = 800;
        }
        else  /* 横屏 */
        {
            lcddev.width = 800;
            lcddev.height = 480;
        }
    }
    else if (lcddev.id == 0x5310)
    {
        /* NT35310: 320x480 */
        if (dir == 0)  /* 竖屏 */
        {
            lcddev.width = 320;
            lcddev.height = 480;
        }
        else  /* 横屏 */
        {
            lcddev.width = 480;
            lcddev.height = 320;
        }
    }
    else
    {
        /* ST7796/ST7789: 320x480 */
        if (dir == 0)  /* 竖屏 */
        {
            lcddev.width = 320;
            lcddev.height = 480;
        }
        else  /* 横屏 */
        {
            lcddev.width = 480;
            lcddev.height = 320;
        }
    }
}

/*********************************************************************************************************
* 函数名称：LCD_Set_Window
* 函数功能：设置显示窗口
* 输入参数：sx/sy - 起始坐标，width/height - 窗口大小
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height)
{
    u16 twidth, theight;
    twidth = sx + width - 1;
    theight = sy + height - 1;
    
    if (lcddev.id == 0x5510)
    {
        LCD_WriteReg(lcddev.setxcmd, sx >> 8);
        LCD_WriteReg(lcddev.setxcmd + 1, sx & 0XFF);
        LCD_WriteReg(lcddev.setxcmd + 2, twidth >> 8);
        LCD_WriteReg(lcddev.setxcmd + 3, twidth & 0XFF);
        LCD_WriteReg(lcddev.setycmd, sy >> 8);
        LCD_WriteReg(lcddev.setycmd + 1, sy & 0XFF);
        LCD_WriteReg(lcddev.setycmd + 2, theight >> 8);
        LCD_WriteReg(lcddev.setycmd + 3, theight & 0XFF);
    }
    else
    {
        LCD_WriteReg(lcddev.setxcmd, sx);
        LCD_WriteReg(lcddev.setxcmd, twidth);
        LCD_WriteReg(lcddev.setycmd, sy);
        LCD_WriteReg(lcddev.setycmd, theight);
    }
}

/*********************************************************************************************************
* 函数名称：LCD_DisplayOn
* 函数功能：打开显示
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_DisplayOn(void)
{
    LCD_WriteReg(0x29, 0);  /* 开启显示 */
}

/*********************************************************************************************************
* 函数名称：LCD_DisplayOff
* 函数功能：关闭显示
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：
**********************************************************************************************************/
void LCD_DisplayOff(void)
{
    LCD_WriteReg(0x28, 0);  /* 关闭显示 */
}

/*********************************************************************************************************
* 函数名称：LCD_Init
* 函数功能：LCD 初始化，包含版本识别和初始化序列
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2025年12月30日
* 注    意：版本识别后根据不同驱动初始化，修复GPIO配置和数据位映射
*         初始化序列参考各驱动的数据手册配置
**********************************************************************************************************/
void LCD_Init(void)
{
    /* 初始化 GPIO */
    LCD_GPIO_Init();
    
    /* LCD 复位时序 */
    gpio_bit_set(LCD_RST_PORT, LCD_RST_PIN);
    DelayNms(10);
    gpio_bit_reset(LCD_RST_PORT, LCD_RST_PIN);
    DelayNms(50);
    gpio_bit_set(LCD_RST_PORT, LCD_RST_PIN);
    DelayNms(200);
    
    /* 读取 LCD ID，防止驱动不匹配导致初始化失败 */
    LCD_WriteReg_Internal(0XD3);
    lcddev.id = LCD_ReadData_Internal();          /* dummy read */
    lcddev.id = LCD_ReadData_Internal();          /* 读取 0X00 */
    lcddev.id = LCD_ReadData_Internal();          /* 期望 0X77 左右的ID */
    lcddev.id <<= 8;
    lcddev.id |= LCD_ReadData_Internal();         /* 读取ID低8位 */
    
    printf("[LCD] Read ID: 0x%04X\r\n", lcddev.id);
    
    /* 根据读取的 ID 进行不同配置 */
    if (lcddev.id == 0x5510)
    {
        /* NT35510 驱动配置 */
        lcddev.wramcmd = 0X2C00;
        lcddev.setxcmd = 0X2A00;
        lcddev.setycmd = 0X2B00;
        lcddev.width = 800;
        lcddev.height = 480;
    }
    else if (lcddev.id == 0x5310)
    {
        /* NT35310 驱动配置 */
        lcddev.wramcmd = 0X2C;
        lcddev.setxcmd = 0X2A;
        lcddev.setycmd = 0X2B;
        lcddev.width = 320;
        lcddev.height = 480;
    }
    else
    {
        /* 默认 ST7796 或 ST7789 */
        lcddev.wramcmd = 0X2C;
        lcddev.setxcmd = 0X2A;
        lcddev.setycmd = 0X2B;
        lcddev.width = 320;
        lcddev.height = 480;
        
        if (lcddev.id == 0x7789)
        {
            /* ST7789 驱动初始化序列 */
            LCD_WriteReg(0x11, 0x00);  /* Sleep Out */
            DelayNms(120);
            /* LCD_WriteReg(0x36, 0x70); 移除硬编码方向设置，交由 LCD_Scan_Dir 统一管理 */
            LCD_WriteReg(0x3A, 0x05);  /* Interface Pixel Format - 16bit/pixel */
            LCD_WriteReg(0xB2, 0x0C);  /* Porch Setting */
            LCD_WriteReg(0xB2, 0x0C);
            LCD_WriteReg(0xB7, 0x00);  /* Gate Control */
            LCD_WriteReg(0xBB, 0x3D);  /* VCOM Setting */
            LCD_WriteReg(0xC0, 0x2C);  /* LCM Control */
            LCD_WriteReg(0xC2, 0x01);  /* VDV and VRH Command Enable */
            LCD_WriteReg(0xC3, 0x13);  /* VRH Set */
            LCD_WriteReg(0xC4, 0x20);  /* VDV Set */
            LCD_WriteReg(0xC6, 0x0F);  /* Frame Rate Control */
            LCD_WriteReg(0xD0, 0xA4);  /* Power Control */
            LCD_WriteReg(0xD0, 0xA1);
            LCD_WriteReg(0x29, 0x00);  /* Display On */
            DelayNms(120);
        }
        else if (lcddev.id == 0x7796)
        {
            /* ST7796 驱动初始化序列 */
            LCD_WriteReg(0x11, 0x00);  /* Sleep Out */
            DelayNms(120);
            LCD_WriteReg(0x36, 0x08);  /* Memory Access Control - 修复倒置问题 */
            LCD_WriteReg(0x3A, 0x05);  /* Interface Pixel Format - 16bit/pixel */
            LCD_WriteReg(0xB1, 0x00);  /* Frame Rate Control */
            LCD_WriteReg(0xB1, 0x18);
            LCD_WriteReg(0xB3, 0x00);  /* Display Function Control */
            LCD_WriteReg(0xB3, 0x02);
            LCD_WriteReg(0xB3, 0x03);
            LCD_WriteReg(0xB3, 0x08);
            
            /* 
             * Inversion Control (0xB4)
             * 如果设置 0x00，在有些批次的屏幕上会导致色彩反相（负片，纯黑变纯白，彩色变互补色）
             * 改为 0x01 或者尝试发送 0x20/0x21(Display Inversion OFF/ON) 命令 
             * 这里先在 0xB4 设置默认反转，并在下方发送 0x21 强制开启反转（通常 IPS 屏需要 0x21）
             */
            LCD_WriteReg(0xB4, 0x01);  /* Inversion Control */
            LCD_WriteReg(0xC0, 0x02);  /* Power Control 1 */
            LCD_WriteReg(0xC1, 0x02);  /* Power Control 2 */
            LCD_WriteReg(0xC5, 0x32);  /* VCOM Control 1 */
            LCD_WriteReg(0xC7, 0x32);  /* VCOM Control 2 */
            LCD_WriteReg(0x36, 0x08);  /* Memory Access Control */
            LCD_WriteReg(0x37, 0x00);  /* Vertical Scroll Start Address */
            LCD_WriteReg(0x38, 0x00);  /* Vertical Scroll Start Address */
            LCD_WriteReg(0x39, 0x00);  /* Vertical Scroll Start Address */
            
            LCD_WriteReg(0x21, 0x00);  /* Display Inversion ON (如果屏幕还是反色，请将 0x21 改为 0x20) */
            
            LCD_WriteReg(0x29, 0x00);  /* Display On */
            DelayNms(120);
        }
        else
        {
            /* 默认驱动配置 */
            LCD_WriteReg(0x11, 0x00);  /* Sleep Out */
            DelayNms(120);
            LCD_WriteReg(0x29, 0x00);  /* Display On */
            DelayNms(120);
        }
    }
    
    /* 设置显示方向 */
    LCD_Display_Dir(0);
    
    /* 设置扫描方向 - 修复字体镜像问题 (L2R -> R2L) */
    LCD_Scan_Dir(R2L_U2D);
    
    /* 开启背光 */
    gpio_bit_set(LCD_LED_PORT, LCD_LED_PIN);
    
    LCD_Clear(BLACK);
    
    printf("[LCD] Init Done, ID: 0x%04X, Width: %d, Height: %d\r\n", 
           lcddev.id, lcddev.width, lcddev.height);
}
