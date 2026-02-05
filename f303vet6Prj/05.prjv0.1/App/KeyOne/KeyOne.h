/*********************************************************************************************************
* 模块名称：KeyOne.h
* 摘    要：KeyOne模块，进行独立按键初始化，以及按键扫描函数实现
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
#ifndef _KEY_ONE_H_
#define _KEY_ONE_H_

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "DataType.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/
//各个按键按下的电平
#define  KEY_DOWN_LEVEL_KEY1    0x00     //0x00表示按下为低电平
#define  KEY_DOWN_LEVEL_KEY2    0x00     //0x00表示按下为低电平
#define  KEY_DOWN_LEVEL_KEY3    0x00     //0x00表示按下为低电平

// 2. 按键引脚（对应你提供的4路按键电路：Key2/Key3/Key4）
#define KEY_G_PIN    GPIO_PIN_3  // Key1：控绿灯
#define KEY_G_PORT   GPIOE
#define KEY_Y_PIN    GPIO_PIN_4  // Key2：控黄灯
#define KEY_Y_PORT   GPIOE
#define KEY_R_PIN    GPIO_PIN_5  // Key3：控红灯
#define KEY_R_PORT   GPIOE
/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/
typedef enum
{
  KEY_NAME_LL = 0,    // LL (KEY1 - PE3)
  KEY_NAME_RL,        // RL (KEY2 - PE4)
  KEY_NAME_LEFT_HIGH, // LEFT_HIGH (PE5)
  KEY_NAME_RIGHT_HIGH,// RIGHT_HIGH (PC13)
  KEY_NAME_MENU,      // MENU (PC9)
  KEY_NAME_MAX
}EnumKeyOneName;

/*********************************************************************************************************
*                                              API函数声明
*********************************************************************************************************/
void  InitKeyOne(void);                                                                     //初始化KeyOne模块
void  ScanKeyOne(unsigned char keyName, void(*OnKeyOneUp)(void), void(*OnKeyOneDown)(void));//每10ms调用一次
void  KeyOne_Loop(void); /* 按键主循环处理函数 */
uint8_t key_scan(uint32_t port, uint32_t pin);
void  KeyOne_2msTask(void);

/* 中断服务函数 */
void EXTI3_IRQHandler(void);    // KEY1中断服务函数
void EXTI4_IRQHandler(void);    // KEY2中断服务函数  
void EXTI5_9_IRQHandler(void);  // LEFT_HIGH (PE5) 中断服务函数
void EXTI10_15_IRQHandler(void); // RIGHT_HIGH (PC13) 中断服务函数

/*********************************************************************************************************
*                                              全局变量声明
**********************************************************************************************************/
extern volatile unsigned int g_u32KeyLLCount;
extern volatile unsigned int g_u32KeyRLCount;
extern volatile unsigned int g_u32KeyLeftHighCount;
extern volatile unsigned int g_u32KeyRightHighCount;
extern volatile unsigned int g_u32KeyMenuCount;

/*********************************************************************************************************
*                                              内部变量声明
*********************************************************************************************************/
#endif
