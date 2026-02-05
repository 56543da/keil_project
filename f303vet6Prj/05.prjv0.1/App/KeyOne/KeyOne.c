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
extern void ProcKeyDownLL(void);
extern void ProcKeyUpLL(void);
extern void ProcKeyDownRL(void);
extern void ProcKeyUpRL(void);
extern void ProcKeyDownLeftHigh(void);
extern void ProcKeyUpLeftHigh(void);
extern void ProcKeyDownRightHigh(void);
extern void ProcKeyUpRightHigh(void);
extern void ProcKeyDownMenu(void);
extern void ProcKeyUpMenu(void);

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
volatile unsigned int g_u32KeyLLCount = 0;
volatile unsigned int g_u32KeyRLCount = 0;
volatile unsigned int g_u32KeyLeftHighCount = 0;
volatile unsigned int g_u32KeyRightHighCount = 0;
volatile unsigned int g_u32KeyMenuCount = 0;

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
  rcu_periph_clock_enable(RCU_AF);      /* 使能AFIO时钟 */
  
  /* 配置按键GPIO为输入模式 */
  gpio_init(GPIOE, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_3);  /* KEY1 */
  gpio_init(GPIOE, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_4);  /* KEY2 */
  gpio_init(GPIOE, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_5);  /* LEFT_HIGH (PE5) */
  gpio_init(GPIOC, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_13); /* RIGHT_HIGH (PC13) */
  gpio_init(GPIOC, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_9);  /* MENU (PC9) */
  
  /* 配置外部中断源选择 */
  /* KEY1(PE3) -> EXTI3 */
  gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOE, GPIO_PIN_SOURCE_3);
  /* KEY2(PE4) -> EXTI4 */
  gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOE, GPIO_PIN_SOURCE_4);
  /* LEFT_HIGH(PE5) -> EXTI5 */
  gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOE, GPIO_PIN_SOURCE_5);
  /* RIGHT_HIGH(PC13) -> EXTI13 */
  gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOC, GPIO_PIN_SOURCE_13);
  /* MENU(PC9) -> EXTI9 */
  gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOC, GPIO_PIN_SOURCE_9);
  
  /* 配置中断触发方式 */
  exti_init(EXTI_3, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
  exti_init(EXTI_4, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
  exti_init(EXTI_5, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
  exti_init(EXTI_13, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
  exti_init(EXTI_9, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
  
  /* 清除中断标志位 */
  exti_interrupt_flag_clear(EXTI_3);
  exti_interrupt_flag_clear(EXTI_4);
  exti_interrupt_flag_clear(EXTI_5);
  exti_interrupt_flag_clear(EXTI_13);
  exti_interrupt_flag_clear(EXTI_9);

  /* 配置NVIC中断优先级 */
  nvic_irq_enable(EXTI3_IRQn, 2, 2);      /* KEY1 */
  nvic_irq_enable(EXTI4_IRQn, 2, 2);      /* KEY2 */
  nvic_irq_enable(EXTI10_15_IRQn, 2, 2);  /* RIGHT_HIGH */
  nvic_irq_enable(EXTI5_9_IRQn, 2, 2);    /* LH (EXTI5) & MENU (EXTI9) 共享中断 */
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
        case KEY_NAME_LL: return (gpio_input_bit_get(GPIOE, GPIO_PIN_3) == RESET);
        case KEY_NAME_RL: return (gpio_input_bit_get(GPIOE, GPIO_PIN_4) == RESET);
        case KEY_NAME_LEFT_HIGH: return (gpio_input_bit_get(GPIOE, GPIO_PIN_5) == RESET);
        case KEY_NAME_RIGHT_HIGH: return (gpio_input_bit_get(GPIOC, GPIO_PIN_13) == RESET);
        case KEY_NAME_MENU: return (gpio_input_bit_get(GPIOC, GPIO_PIN_9) == RESET);
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
        s_keyState[KEY_NAME_LL] = 1;
        s_keyStableCnt[KEY_NAME_LL] = 0;
    }
}


void EXTI4_IRQHandler(void)
{
    if(exti_flag_get(EXTI_4) != RESET)
    {
        exti_interrupt_flag_clear(EXTI_4);
        exti_interrupt_disable(EXTI_4);
        s_keyState[KEY_NAME_RL] = 1;
        s_keyStableCnt[KEY_NAME_RL] = 0;
    }
}

void EXTI5_9_IRQHandler(void)
{
    /* 处理 LH (EXTI5) */
    if(exti_flag_get(EXTI_5) != RESET)
    {
        exti_interrupt_flag_clear(EXTI_5);
        exti_interrupt_disable(EXTI_5);
        s_keyState[KEY_NAME_LEFT_HIGH] = 1;
        s_keyStableCnt[KEY_NAME_LEFT_HIGH] = 0;
    }

    /* 处理 MENU (EXTI9) */
    if(exti_flag_get(EXTI_9) != RESET)
    {
        exti_interrupt_flag_clear(EXTI_9);
        exti_interrupt_disable(EXTI_9);
        s_keyState[KEY_NAME_MENU] = 1;
        s_keyStableCnt[KEY_NAME_MENU] = 0;
    }

    /* 清除所有可能的中断标志位，防止死循环 */
    exti_interrupt_flag_clear(EXTI_5);
    exti_interrupt_flag_clear(EXTI_6);
    exti_interrupt_flag_clear(EXTI_7);
    exti_interrupt_flag_clear(EXTI_8);
    exti_interrupt_flag_clear(EXTI_9);
}

void EXTI10_15_IRQHandler(void)
{
    if(exti_flag_get(EXTI_13) != RESET)
    {
        exti_interrupt_flag_clear(EXTI_13);
        exti_interrupt_disable(EXTI_13);
        s_keyState[KEY_NAME_RIGHT_HIGH] = 1;
        s_keyStableCnt[KEY_NAME_RIGHT_HIGH] = 0;
    }
    
    exti_interrupt_flag_clear(EXTI_10);
    exti_interrupt_flag_clear(EXTI_11);
    exti_interrupt_flag_clear(EXTI_12);
    exti_interrupt_flag_clear(EXTI_14);
    exti_interrupt_flag_clear(EXTI_15);
}


void KeyOne_2msTask(void)
{
    unsigned char i;

    for(i = 0; i < KEY_NAME_MAX; i++)
    {
        /* 状态0：空闲检测 */
        if(s_keyState[i] == 0)
        {
            if(KeyIsPressed(i))
            {
                s_keyState[i] = 1;
                s_keyStableCnt[i] = 0;
            }
        }
        /* 状态1：按下消抖 */
        else if(s_keyState[i] == 1)
        {
            if(KeyIsPressed(i))
            {
                s_keyStableCnt[i]++;
                if(s_keyStableCnt[i] >= KEY_DEBOUNCE_TICKS)
                {
                    if(i == KEY_NAME_LL) { LED1_Toggle(); g_u32KeyLLCount++; ProcKeyDownLL(); }
                    else if(i == KEY_NAME_RL) { LED2_Toggle(); g_u32KeyRLCount++; ProcKeyDownRL(); }
                    else if(i == KEY_NAME_LEFT_HIGH) { LED3_Toggle(); g_u32KeyLeftHighCount++; ProcKeyDownLeftHigh(); }
                    else if(i == KEY_NAME_RIGHT_HIGH) { g_u32KeyRightHighCount++; ProcKeyDownRightHigh(); }
                    else if(i == KEY_NAME_MENU) { g_u32KeyMenuCount++; ProcKeyDownMenu(); }

                    s_keyState[i] = 2;
                    s_keyStableCnt[i] = 0;
                }
            }
            else
            {
                s_keyStableCnt[i] = 0;
                s_keyState[i] = 0; /* 如果抖动，回到状态0。对于中断按键，这里其实应该重新使能中断 */
                
                /* 补充：对于中断触发的按键，如果在这里复位，需要重新开启中断 */
                if(i == KEY_NAME_LL) exti_interrupt_enable(EXTI_3);
                else if(i == KEY_NAME_RL) exti_interrupt_enable(EXTI_4);
                else if(i == KEY_NAME_LEFT_HIGH) exti_interrupt_enable(EXTI_5);
                else if(i == KEY_NAME_RIGHT_HIGH) exti_interrupt_enable(EXTI_13);
                else if(i == KEY_NAME_MENU) exti_interrupt_enable(EXTI_9);
            }
        }
        /* 状态2：释放消抖 */
        else if(s_keyState[i] == 2)
        {
            if(!KeyIsPressed(i))
            {
                s_keyStableCnt[i]++;
                if(s_keyStableCnt[i] >= KEY_DEBOUNCE_TICKS)
                {
                    if(i == KEY_NAME_LL) { ProcKeyUpLL(); exti_interrupt_enable(EXTI_3); }
                    else if(i == KEY_NAME_RL) { ProcKeyUpRL(); exti_interrupt_enable(EXTI_4); }
                    else if(i == KEY_NAME_LEFT_HIGH) { ProcKeyUpLeftHigh(); exti_interrupt_enable(EXTI_5); }
                    else if(i == KEY_NAME_RIGHT_HIGH) { ProcKeyUpRightHigh(); exti_interrupt_enable(EXTI_13); }
                    else if(i == KEY_NAME_MENU) { ProcKeyUpMenu(); exti_interrupt_enable(EXTI_9); }

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
