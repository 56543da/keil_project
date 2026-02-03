/*********************************************************************************************************
* 模块名称：KeyOne.c
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
**********************************************************************************************************/

/*********************************************************************************************************
*                                              包含头文件
**********************************************************************************************************/
#include "KeyOne.h"
#include "gd32f30x_conf.h"
#include "SysTick.h"

/*********************************************************************************************************
*                                              宏定义
**********************************************************************************************************/
/* KEY1为读取PE3管脚的电平 */
/* #define KEY1    (gpio_input_bit_get(GPIOE, GPIO_PIN_3)) */
/* KEY2为读取PE4管脚的电平 */
/* #define KEY2    (gpio_input_bit_get(GPIOE, GPIO_PIN_4)) */
/* KEY3为读取PE5管脚的电平 */
/* #define KEY3    (gpio_input_bit_get(GPIOE, GPIO_PIN_5)) */
/* KEY4为读取PC13管脚的电平 */
/* #define KEY4    (gpio_input_bit_get(GPIOC, GPIO_PIN_13)) */

/*********************************************************************************************************
*                                              枚举结构体
**********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量声明
**********************************************************************************************************/
/* 按键按下时的电平值，0xFF表示按下为高电平，0x00表示按下为低电平 */
static  unsigned char  s_arrKeyDownLevel[KEY_NAME_MAX];  /* 使用前需要在InitKeyOne函数中进行初始化 */

/*********************************************************************************************************
*                                              内部函数声明
**********************************************************************************************************/
static  void  ConfigKeyOneGPIO(void);  /* 配置按键GPIO */

/*********************************************************************************************************
*                                              内部函数实现
**********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：ConfigKeyOneGPIO
* 函数功能：配置按键GPIO 
* 输入参数：void 
* 输出参数：void
* 返 回 值：void
* 完成日期：2021年07月01日
* 注    意：
**********************************************************************************************************/
static  void  ConfigKeyOneGPIO(void)
{
  /* 使能RCU时钟 */
  rcu_periph_clock_enable(RCU_GPIOE);  /* 使能GPIOE时钟 */
  rcu_periph_clock_enable(RCU_GPIOC);  /* 使能GPIOC时钟 */
  
  gpio_init(GPIOE, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ, GPIO_PIN_3);  /* 配为输入模式，内部下拉电阻 */
  gpio_init(GPIOE, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ, GPIO_PIN_4);  
  gpio_init(GPIOE, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ, GPIO_PIN_5);  
  gpio_init(GPIOC, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
}

/*********************************************************************************************************
*                                              API函数实现
**********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：InitKeyOne
* 函数功能：初始化KeyOne模块
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2021年07月01日
* 注    意：
**********************************************************************************************************/
void InitKeyOne(void)
{
  ConfigKeyOneGPIO();  /* 配置按键GPIO */
                                                                
  s_arrKeyDownLevel[KEY_NAME_KEY1] = KEY_DOWN_LEVEL_KEY1;  /* 配置KEY1按下时为高电平 */
  s_arrKeyDownLevel[KEY_NAME_KEY2] = KEY_DOWN_LEVEL_KEY2;  /* 配置KEY2按下时为低电平 */
  s_arrKeyDownLevel[KEY_NAME_KEY3] = KEY_DOWN_LEVEL_KEY3;  /* 配置KEY3按下时为低电平 */
}

void ScanKeyOne(unsigned char keyName, void(*OnKeyOneUp)(void), void(*OnKeyOneDown)(void))
{
  static unsigned char s_arrKeyVal[KEY_NAME_MAX];
  static unsigned char s_arrKeyFlag[KEY_NAME_MAX] = {TRUE, TRUE, TRUE};
  unsigned char keyLevel = 0;

  s_arrKeyVal[keyName] = (unsigned char)(s_arrKeyVal[keyName] << 1);

  switch (keyName)
  {
    case KEY_NAME_KEY1:
      keyLevel = (unsigned char)(gpio_input_bit_get(GPIOE, GPIO_PIN_3) ? 1U : 0U);
      break;
    case KEY_NAME_KEY2:
      keyLevel = (unsigned char)(gpio_input_bit_get(GPIOE, GPIO_PIN_4) ? 1U : 0U);
      break;
    case KEY_NAME_KEY3:
      keyLevel = (unsigned char)(gpio_input_bit_get(GPIOE, GPIO_PIN_5) ? 1U : 0U);
      break;
    default:
      return;
  }

  s_arrKeyVal[keyName] = (unsigned char)(s_arrKeyVal[keyName] | keyLevel);

  if(s_arrKeyVal[keyName] == s_arrKeyDownLevel[keyName] && s_arrKeyFlag[keyName] == TRUE)
  {
    (*OnKeyOneDown)();
    s_arrKeyFlag[keyName] = FALSE;
  }
  else if(s_arrKeyVal[keyName] == (unsigned char)(~s_arrKeyDownLevel[keyName]) && s_arrKeyFlag[keyName] == FALSE)
  {
    (*OnKeyOneUp)();
    s_arrKeyFlag[keyName] = TRUE;
  }
}

/* =============== 便于操作的按键扫描函数 =============== */
/* 按键按下是否成立，返回1=成立，0=未成立 */
uint8_t key_scan(uint32_t port, uint32_t pin)
{
    /* 按键按下电平为硬件电路设计时为低电平 */
    if(gpio_input_bit_get(port, pin) == RESET)
    {
        DelayNms(20);
        if(gpio_input_bit_get(port, pin) == RESET)  /* 再次确认按键 */
        {
            /* 等待按键抬起 */
            while(gpio_input_bit_get(port, pin) == RESET);
            return 1;
        }
    }
    return 0;
}
