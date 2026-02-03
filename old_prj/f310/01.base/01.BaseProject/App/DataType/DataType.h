/*********************************************************************************************************
* 模块名称：DataType.h
* 摘    要：数据类型定义
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2021年07月01日
* 内    容：添加SPO2相关数据结构
* 注    意：                                                                  
**********************************************************************************************************
* 取代版本：
* 作    者：
* 完成日期： 
* 修改内容： 添加SPO2_Data和SPO2_Packet结构体
* 修改文件：
*********************************************************************************************************/
#ifndef _DATA_TYPE_H_
#define _DATA_TYPE_H_

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "gd32f30x_conf.h"

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
typedef unsigned short      HWORD;  //两字节组成一个半字
typedef unsigned int        WORD;   //四字节组成一个字
typedef long                LONG;

#define LOHWORD(w)          ((HWORD)(w))                            //字的低半字 
#define HIHWORD(w)          ((HWORD)(((WORD)(w) >> 16) & 0xFFFF))   //字的高半字

#define LOBYTE(hw)          ((BYTE)(hw) )                           //半字的低字节
#define HIBYTE(hw)          ((BYTE)(((WORD)(hw) >> 8) & 0xFF))      //半字的高字节

//两字节组成一个半字
#define MAKEHWORD(bH, bL)   ((HWORD)(((BYTE)(bL)) | ((HWORD)((BYTE)(bH))) << 8))

//两半字组成一个字
#define MAKEWORD(hwH, hwL)  ((WORD)(((HWORD)(hwL)) | ((WORD)((HWORD)(hwH))) << 16))

#define TRUE          1
#define FALSE         0
#define NULL          0
#define INVALID_DATA  -100

/* SPO2数据包格式定义 */
#define SPO2_PACKET_HEADER    0xAA    // 数据包头
#define SPO2_PACKET_LENGTH    0x06    // 数据长度
#define SPO2_PACKET_END       0x55    // 数据包尾

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/

/* SPO2工作模式枚举 */
typedef enum
{
    SPO2_MODE_IDLE = 0,       // 空闲模式
    SPO2_MODE_SINGLE,         // 单次发送模式
    SPO2_MODE_CONTINUOUS,     // 连续发送模式
    SPO2_MODE_TEST,           // 测试模式
    SPO2_MODE_MAX
} EnumSPO2Mode;

/* 血氧数据结构体 */
typedef struct
{
    u8 spo2;          // 血氧饱和度 (0-100%)
    u8 heart_rate;    // 心率 (30-250 bpm)
    u8 pi;           // 灌注指数 (0.1-20%, 扩大10倍存储)
    u8 valid;         // 数据有效标志 (1-有效, 0-无效)
} SPO2_Data;

/* SPO2数据包结构体 */
typedef struct
{
    u8 header;        // 数据包头 0xAA
    u8 length;        // 数据长度 0x06
    u8 spo2;         // 血氧饱和度
    u8 heart_rate;   // 心率
    u8 pi;           // 灌注指数
    u8 checksum;     // 校验和
    u8 end;          // 数据包尾 0x55
} SPO2_Packet;

/*********************************************************************************************************
*                                              API函数声明
*********************************************************************************************************/

#endif