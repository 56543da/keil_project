/*********************************************************************************************************
* 模块名称：DataType.h
* 摘    要：数据类型定义
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2021年07月01日
* 内    容：
* 注    意：                                                                  
**********************************************************************************************************
* 取代版本：
* 作    者：
* 完成日期： 
* 修改内容： 
* 修改文件：
*********************************************************************************************************/
#ifndef _DATA_TYPE_H_
#define _DATA_TYPE_H_

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "gd32f3x0_conf.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/
typedef signed char         i8;
typedef signed short        i16;
typedef signed int          i32;
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;

typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      HWORD;  //双字节的高一字节数据
typedef unsigned int        WORD;   //四字节的高一字数据
typedef long                LONG;

#define LOHWORD(w)          ((HWORD)(w))                            //字的低半字 
#define HIHWORD(w)          ((HWORD)(((WORD)(w) >> 16) & 0xFFFF))   //字的高半字

#define LOBYTE(hw)          ((BYTE)(hw) )                           //半字的低字节
#define HIBYTE(hw)          ((BYTE)(((WORD)(hw) >> 8) & 0xFF))      //半字的高字节

//双字节的高一字节数据
#define MAKEHWORD(bH, bL)   ((HWORD)(((BYTE)(bL)) | ((HWORD)((BYTE)(bH))) << 8))

//四字节的高一字数据
#define MAKEWORD(hwH, hwL)  ((WORD)(((HWORD)(hwL)) | ((WORD)((HWORD)(hwH))) << 16))

#define TRUE          1
#define FALSE         0
#define NULL          0
#define INVALID_DATA  -100

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/
#define SPO2_PACKET_HEAD      0xAA    // 包头
#define SPO2_PACKET_TAIL      0x55    // 包尾
#define SPO2_PACKET_LENGTH    0x06    // 数据长度 (SPO2, HR, PI, Status, Filter, Gain)

typedef struct {
    u8 spo2;       // 血氧饱和度
    u8 heart_rate; // 心率
    u8 pi;         // 灌注指数
    u8 status;     // 状态 (0:正常, 1:未佩戴/手指脱落)
    u8 filter_status; // 滤波器状态 (0:OFF, 1:ON)
    u8 gain_level;    // 增益等级
    u8 update_flag;// 更新标志
} SPO2Data_t;

/*********************************************************************************************************
*                                              API函数声明
*********************************************************************************************************/

#endif
