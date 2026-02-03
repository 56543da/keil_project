/*********************************************************************************************************
* 模块名称：SPO2_Display.h
* 摘    要：血氧数据显示模块，在LCD上显示血氧相关数据
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2025年12月31日 
* 内    容：
* 注    意：                                                                  
**********************************************************************************************************
* 取代版本：
* 作    者：
* 完成日期：
* 修改内容：
* 修改文件：
*********************************************************************************************************/
#ifndef _SPO2_DISPLAY_H_
#define _SPO2_DISPLAY_H_

#include "gd32f30x_conf.h"
#include "LCD.h"
#include "DataType.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/

/* 显示区域定义 */
#define SPO2_DISPLAY_START_X    30
#define SPO2_DISPLAY_START_Y    160
#define SPO2_LINE_HEIGHT        20
#define SPO2_VALUE_COLOR        GREEN
#define SPO2_TITLE_COLOR        WHITE
#define SPO2_INVALID_COLOR      RED
#define SPO2_NORMAL_COLOR       GREEN
#define SPO2_WARNING_COLOR      YELLOW
#define SPO2_DANGER_COLOR       RED

/* 血氧正常范围定义 */
#define SPO2_NORMAL_MIN         95
#define SPO2_NORMAL_MAX         100
#define SPO2_WARNING_MIN        90
#define SPO2_DANGER_MIN         85

/* 心率正常范围定义 */
#define HR_NORMAL_MIN           60
#define HR_NORMAL_MAX           100
#define HR_WARNING_LOW          50
#define HR_WARNING_HIGH         120
#define HR_DANGER_LOW           40
#define HR_DANGER_HIGH          150

/*********************************************************************************************************
*                                              API函数声明
*********************************************************************************************************/
void  SPO2_DisplayInit(void);                           //初始化血氧显示区域
void  SPO2_UpdateDisplay(void);                         //更新血氧数据显示
void  SPO2_DisplayData(SPO2Data_t* pSpo2Data);           //显示血氧数据
void  SPO2_DisplayStatus(u8 valid);                    //显示数据有效状态
void  SPO2_ClearDisplayArea(void);                      //清除血氧显示区域

#endif
