#include "Timer.h"
#include "gd32f3x0_conf.h"
#include "SPO2_Driver.h"

static  unsigned char  s_i1secFlag = FALSE;    //标记1s标志位，初始化为FALSE

static  void  ConfigTimer2(unsigned short arr, unsigned short psc);  //配置TIMER2

static  void ConfigTimer2(unsigned short arr, unsigned short psc)
{
  timer_parameter_struct timer_initpara;               //timer_initpara结构体变量定义

  //开启RCU时钟 
  rcu_periph_clock_enable(RCU_TIMER2);                 //开启TIMER2时钟

  timer_deinit(TIMER2);                                //复位TIMER2所有寄存器
  timer_struct_para_init(&timer_initpara);             //初始化timer_initpara

  //配置TIMER2
  timer_initpara.prescaler         = psc;              //配置预分频值
  timer_initpara.counterdirection  = TIMER_COUNTER_UP; //配置计数方向
  timer_initpara.period            = arr;              //配置周期
  timer_initpara.clockdivision     = TIMER_CKDIV_DIV1; //配置时钟分频
  timer_init(TIMER2, &timer_initpara);                 //初始化定时器

  timer_interrupt_enable(TIMER2, TIMER_INT_UP);        //使能更新中断
  nvic_irq_enable(TIMER2_IRQn, 1, 0);                  //配置NVIC中断优先级
  timer_enable(TIMER2);                                //使能定时器
}

void TIMER2_IRQHandler(void)
{  
  static  uint16_t s_iCnt1000 = 0;  // 1s计数器

  if(timer_interrupt_flag_get(TIMER2, TIMER_INT_FLAG_UP) == SET)   //判断是否产生更新中断
  {
    timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_UP);         //清除更新中断标志位
    
    // 执行血氧算法状态机 (每0.5ms调用一次)
    SPO2_Timer_Handler();
    
    // 1秒计时逻辑 (0.5ms * 2000 = 1000ms)
    s_iCnt1000++;
    if(s_iCnt1000 >= 2000)
    {
        s_iCnt1000 = 0;
        s_i1secFlag = TRUE;
    }
  }
}

void InitTimer(void)
{
    uint32_t timer_clock;
    uint16_t prescaler;
    
    // 动态获取 TIMER2 时钟频率，确保 Timer 始终工作在 1MHz (1us tick)
    // TIMER2 挂载在 APB1
    // 如果 APB1 预分频为 1，TimerClock = APB1
    // 如果 APB1 预分频 != 1，TimerClock = APB1 * 2
    // rcu_clock_freq_get 应该能返回正确的 Timer 时钟吗？
    // CK_TIMER2 可能不直接支持，我们手动计算
    
    /* 
       Note: standard library rcu_clock_freq_get(CK_APB1) returns APB1 freq.
       We need to check APB1 prescaler.
       However, for simplicity and robustness against clock changes:
       Let's assume system might fall back to 8MHz or run at 72MHz.
    */
    
    // 获取 PCLK1 (APB1) 频率
    uint32_t apb1_freq = rcu_clock_freq_get(CK_APB1);
    uint32_t ahb_freq = rcu_clock_freq_get(CK_AHB);
    
    // 如果 AHB/APB1 > 1 (即 div > 1)，则 TimerClk = APB1 * 2
    if (ahb_freq > apb1_freq) {
        timer_clock = apb1_freq * 2;
    } else {
        timer_clock = apb1_freq;
    }
    
    // 目标频率 1MHz (1us)
    // Prescaler = (TimerClock / 1MHz) - 1
    prescaler = (uint16_t)(timer_clock / 1000000) - 1;
    
    // ConfigTimer2(499, prescaler); // 500 * 1us = 500us = 0.5ms
    ConfigTimer2(499, prescaler);
}

unsigned char  Get1SecFlag(void)
{
  return(s_i1secFlag);
}

void  Clr1SecFlag(void)
{
  s_i1secFlag = FALSE;
}

// 移除未使用的 2ms Flag 接口，或保留为空以兼容
unsigned char  Get2msFlag(void) { return 0; }
void  Clr2msFlag(void) {}
