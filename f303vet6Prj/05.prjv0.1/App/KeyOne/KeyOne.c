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

#include <stdio.h>
#include "SysTick.h"
#include "LED.h"

/* 按键处理函数声明 - 避免隐式声明警告 */
extern void ProcKeyDownKey1(void);
extern void ProcKeyUpKey1(void);
extern void ProcKeyDownKey2(void);
extern void ProcKeyUpKey2(void);
extern void ProcKeyDownKey3(void);
extern void ProcKeyUpKey3(void);

/*********************************************************************************************************
*                                              宏定义
**********************************************************************************************************/

/*********************************************************************************************************
*                                              枚举结构体
**********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量声明
**********************************************************************************************************/
/* 按键按下计数变量 */
volatile unsigned int g_u32Key1Count = 0;
volatile unsigned int g_u32Key2Count = 0;
volatile unsigned int g_u32Key3Count = 0;

#define KEY_DEBOUNCE_TICKS 5

static volatile unsigned char s_keyState[KEY_NAME_MAX];
static volatile unsigned char s_keyStableCnt[KEY_NAME_MAX];

/*********************************************************************************************************
*                                              内部函数声明
**********************************************************************************************************/
static  void  ConfigKeyOneGPIO(void);  /* 配置按键GPIO */

/*********************************************************************************************************
*                                              内部函数实现
**********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：ConfigKeyOneGPIO
* 函数功能：配置按键的GPIO 
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
  rcu_periph_clock_enable(RCU_AF);      /* 使能AFIO时钟，用于外部中断 */
  
  /* 配置按键GPIO为输入模式 */
  gpio_init(GPIOE, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_3);  /* KEY1配置为输入上拉（低电平有效） */
  gpio_init(GPIOE, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_4);  /* KEY2配置为输入上拉（低电平有效） */
  gpio_init(GPIOE, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_5);  /* KEY3配置为输入上拉（低电平有效） */
  
  /* 配置外部中断源选择 - GD32F303使用GPIO端口和引脚源配置 */
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOE, GPIO_PIN_SOURCE_3);  /* KEY1(PE3) -> EXTI3 */
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOE, GPIO_PIN_SOURCE_4);  /* KEY2(PE4) -> EXTI4 */
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOE, GPIO_PIN_SOURCE_5);  /* KEY3(PE5) -> EXTI5 */
  
  exti_init(EXTI_3, EXTI_INTERRUPT, EXTI_TRIG_FALLING);   /* KEY1下降沿触发（低电平有效） */
  exti_init(EXTI_4, EXTI_INTERRUPT, EXTI_TRIG_FALLING); /* KEY2下降沿触发（低电平有效） */
  exti_init(EXTI_5, EXTI_INTERRUPT, EXTI_TRIG_FALLING); /* KEY3下降沿触发（低电平有效） */
  
  /* 清除中断标志位，防止使能中断后立即触发 */
  exti_interrupt_flag_clear(EXTI_3);
  exti_interrupt_flag_clear(EXTI_4);
  exti_interrupt_flag_clear(EXTI_5);

  /* 配置NVIC中断优先级 */
  nvic_irq_enable(EXTI3_IRQn, 2, 2);   /* KEY1中断优先级 */
    nvic_irq_enable(EXTI4_IRQn, 2, 2);   /* KEY2中断优先级 */
    nvic_irq_enable(EXTI5_9_IRQn, 2, 2);  /* KEY3中断优先级 (EXTI5-9共享中断) */
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
static unsigned char KeyIsPressed(unsigned char keyName)
{
    switch(keyName)
    {
        case KEY_NAME_KEY1: return (gpio_input_bit_get(GPIOE, GPIO_PIN_3) == RESET);
        case KEY_NAME_KEY2: return (gpio_input_bit_get(GPIOE, GPIO_PIN_4) == RESET);
        case KEY_NAME_KEY3: return (gpio_input_bit_get(GPIOE, GPIO_PIN_5) == RESET);
        default: return 0;
    }
}

void InitKeyOne(void)
{
  unsigned char i;

  ConfigKeyOneGPIO();  /* 配置按键GPIO */

  for(i = 0; i < KEY_NAME_MAX; i++)
  {
      s_keyState[i] = 0;
      s_keyStableCnt[i] = 0;
  }
}





void EXTI3_IRQHandler(void)
{
    if(exti_flag_get(EXTI_3) != RESET)
    {
        exti_interrupt_flag_clear(EXTI_3);
        exti_interrupt_disable(EXTI_3);
        s_keyState[KEY_NAME_KEY1] = 1;
        s_keyStableCnt[KEY_NAME_KEY1] = 0;
    }
}


void EXTI4_IRQHandler(void)
{
    if(exti_flag_get(EXTI_4) != RESET)
    {
        exti_interrupt_flag_clear(EXTI_4);
        exti_interrupt_disable(EXTI_4);
        s_keyState[KEY_NAME_KEY2] = 1;
        s_keyStableCnt[KEY_NAME_KEY2] = 0;
    }
}

void EXTI9_5_IRQHandler(void)
{
    if(exti_flag_get(EXTI_5) != RESET)
    {
        exti_interrupt_flag_clear(EXTI_5);
        exti_interrupt_disable(EXTI_5);
        s_keyState[KEY_NAME_KEY3] = 1;
        s_keyStableCnt[KEY_NAME_KEY3] = 0;
    }

    exti_interrupt_flag_clear(EXTI_6);
    exti_interrupt_flag_clear(EXTI_7);
    exti_interrupt_flag_clear(EXTI_8);
    exti_interrupt_flag_clear(EXTI_9);
}


void KeyOne_2msTask(void)
{
    unsigned char i;

    for(i = 0; i < KEY_NAME_MAX; i++)
    {
        if(s_keyState[i] == 1)
        {
            if(KeyIsPressed(i))
            {
                s_keyStableCnt[i]++;
                if(s_keyStableCnt[i] >= KEY_DEBOUNCE_TICKS)
                {
                    if(i == KEY_NAME_KEY1) { LED1_Toggle(); g_u32Key1Count++; }
                    else if(i == KEY_NAME_KEY2) { LED2_Toggle(); g_u32Key2Count++; }
                    else if(i == KEY_NAME_KEY3) { LED3_Toggle(); g_u32Key3Count++; }

                    s_keyState[i] = 2;
                    s_keyStableCnt[i] = 0;
                }
            }
            else
            {
                s_keyStableCnt[i] = 0;
            }
        }
        else if(s_keyState[i] == 2)
        {
            if(!KeyIsPressed(i))
            {
                s_keyStableCnt[i]++;
                if(s_keyStableCnt[i] >= KEY_DEBOUNCE_TICKS)
                {
                    if(i == KEY_NAME_KEY1) exti_interrupt_enable(EXTI_3);
                    else if(i == KEY_NAME_KEY2) exti_interrupt_enable(EXTI_4);
                    else if(i == KEY_NAME_KEY3) exti_interrupt_enable(EXTI_5);

                    s_keyState[i] = 0;
                    s_keyStableCnt[i] = 0;
                }
            }
            else
            {
                s_keyStableCnt[i] = 0;
            }
        }
    }
}
