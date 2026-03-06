/*********************************************************************************************************
* 模块名称：Main.c
* 摘    要：该文件主要包含硬件初始化以及main函数
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2021年07月01日
* 内    容：
* 注    意：注意勾选Options for Target 'Target1'->Code Generation->Use MicroLIB使printf函数生效
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
#include "Main.h"
#include "gd32f3x0_conf.h"
#include "gd32f3x0_fwdgt.h"
#include "SysTick.h"
#include "RCU.h"
#include "NVIC.h"
#include "Timer.h"
#include "UART0.h"
#include "DataType.h"
#include "SPO2_Driver.h"

/*********************************************************************************************************
*                                              宏定义
**********************************************************************************************************/
#define SPO2_WAVE_TOOL_ENABLE 1     // 是否启用波形工具，1：启用，0：禁用，启用时会发送波形数据
#define SPO2_FILTER_ENABLE 0        // 是否启用数字滤波器，1：启用，0：禁用
#define SPO2_AUTO_GAIN_ENABLE 1     // 是否启用自动增益控制 (AGC)，1：启用，0：禁用
#define SPO2_SEND_BINARY_PACKET 0   // 是否发送二进制数据包，1：启用，0：禁用

/*********************************************************************************************************
*                                              枚举结构体
**********************************************************************************************************/

/*********************************************************************************************************
*                                              内部函数声明
**********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量声明
**********************************************************************************************************/
static  void  InitSoftware(void);   /* 初始化软件模块，在初始化硬件之后调用此函数 */
static  void  InitHardware(void);   /* 初始化硬件模块，如系统时钟等调用此函数 */
static  void  InitLED(void);        /* 初始化LED (PA6) */
static  void  InitWatchdog(void);   /* 初始化独立看门狗 */
static  void  SendWavePair(uint16_t a, uint16_t b);

/*********************************************************************************************************
*                                              内部函数实现
**********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：InitSoftware
* 函数功能：初始化软件模块，在初始化硬件之后调用此函数
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
**********************************************************************************************************/
static  void  InitSoftware(void)
{

}

/*********************************************************************************************************
* 函数名称：InitHardware
* 函数功能：初始化硬件模块，如系统时钟等调用此函数
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
**********************************************************************************************************/
static  void  InitHardware(void)
{  
  InitNVIC();          /* 初始化NVIC模块 */  
  InitUART0(115200);   /* 初始化UART模块 */
  InitLED();           /* 初始化LED (PA6) */
  SPO2_Driver_Init();  /* 初始化SPO2驱动 (PA1, PC13, PC14) */
  InitTimer();         /* 初始化Timer模块 - 开启定时器中断 */
  InitWatchdog();      /* 初始化看门狗，防止因意外开启导致的复位 */
}

/*********************************************************************************************************
* 函数名称：InitLED
* 函数功能：初始化LED (PA6) 
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2026年01月08日
* 注    意：
**********************************************************************************************************/
static void InitLED(void)
{
//  rcu_periph_clock_enable(RCU_GPIOA);
//  gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
//  gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
//  
//  /* 初始状态输出低电平 */
//  gpio_bit_reset(GPIOA, GPIO_PIN_6);
}

/*********************************************************************************************************
* 函数名称：InitWatchdog
* 函数功能：初始化独立看门狗
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2026年02月03日
* 注    意：LSI约为40kHz，配置超时时间约19.2s
**********************************************************************************************************/
static void InitWatchdog(void)
{
  /* 开启对 FWDGT_PSC 和 FWDGT_RLD 寄存器的写访问 */
  fwdgt_write_enable();
    
  /* 配置预分频器为 256, 40kHz / 256 = 156.25Hz */
  /* 配置重装载值为 3000, 超时时间 = 3000 / 156.25 ≈ 19.2s */
  fwdgt_config(3000, FWDGT_PSC_DIV256);
    
  /* 开启看门狗 */
  fwdgt_enable();
}

static void SendWavePair(uint16_t a, uint16_t b)
{
  unsigned char buf[16];
  unsigned char len = 0;
  unsigned char i;
  unsigned char tmp[5];

  if(a == 0) { buf[len++] = '0'; }
  else {
    i = 0;
    while(a > 0 && i < sizeof(tmp)) { tmp[i++] = (unsigned char)('0' + (a % 10)); a = (uint16_t)(a / 10); }
    while(i > 0) { buf[len++] = tmp[--i]; }
  }

  buf[len++] = ',';

  if(b == 0) { buf[len++] = '0'; }
  else {
    i = 0;
    while(b > 0 && i < sizeof(tmp)) { tmp[i++] = (unsigned char)('0' + (b % 10)); b = (uint16_t)(b / 10); }
    while(i > 0) { buf[len++] = tmp[--i]; }
  }

  buf[len++] = '\r';
  buf[len++] = '\n';
  WriteUART0(buf, len);
}

/*********************************************************************************************************
* 函数名称：main
* 函数功能：主函数 
* 输入参数：void
* 输出参数：void
* 返 回 值：int
* 创建日期：2021年07月01日
* 注    意：
**********************************************************************************************************/
int main(void)
{
  static uint32_t run_time_sec = 0;
  SPO2Data_t spo2Data;
  uint8_t spo2, hr, pi;
  uint16_t red_adc, ir_adc;
  uint16_t wave_red_seq, wave_ir_seq;
  uint16_t wave_red_seq_last, wave_ir_seq_last;

  InitHardware();   /* 初始化硬件等相关模块 */
  InitSoftware();   /* 初始化软件模块 */

  /* 配置算法参数 */
  SPO2_SetFilterEnable(SPO2_FILTER_ENABLE);
  SPO2_SetAGCEnable(SPO2_AUTO_GAIN_ENABLE);
  
  // 初始设置为 4 级增益，避免信号过弱
  SPO2_SetGain(SPO2_GAIN_LEVEL_0);
  printf("Gain Test: Initial Level 0\r\n");

  wave_red_seq_last = 0;
  wave_ir_seq_last = 0;
	
//  if (SPO2_WAVE_TOOL_ENABLE) {
//    printf("{{1,SpO2}}\r\n{{2,HR}}\r\n{{3,PI}}\r\n{{4,GAIN}}\r\n");
//  } else {
//    /* 上电测试数据 */
//    spo2Data.spo2 = 99; spo2Data.heart_rate = 80; spo2Data.pi = 10; spo2Data.status = 0;
//    UART0_SendSPO2Data(&spo2Data);
//    printf("System Start (72MHz)...\r\n");
//    printf("CK_SYS: %d, CK_AHB: %d, CK_APB1: %d\r\n", 
//            rcu_clock_freq_get(CK_SYS), rcu_clock_freq_get(CK_AHB), rcu_clock_freq_get(CK_APB1));
//  }
	
  while(1)
  {
    fwdgt_counter_reload(); /* 喂狗 */
    
    // 0. 处理 UART 命令 (新增)
    {
        uint8_t cmd, val;
        if (UART0_GetCmd(&cmd, &val)) {
            uint8_t current_gain = SPO2_GetGain();
            if (cmd == 0x02) { // Increase Gain
                if (current_gain < SPO2_GAIN_LEVEL_MAX) {
                    SPO2_SetGain(current_gain + 1);
                    printf("CMD: Gain Inc -> %d\r\n", current_gain + 1);
                } else {
                    printf("CMD: Gain Max reached (%d)\r\n", current_gain);
                }
            } else if (cmd == 0x03) { // Decrease Gain
                if (current_gain > SPO2_GAIN_LEVEL_0) {
                    SPO2_SetGain(current_gain - 1);
                    printf("CMD: Gain Dec -> %d\r\n", current_gain - 1);
                } else {
                    printf("CMD: Gain Min reached (%d)\r\n", current_gain);
                }
            }
        }
    }

    // 1. 波形数据输出 (用于上位机绘图)
    if (SPO2_WAVE_TOOL_ENABLE) {
        SPO2_GetWaveSeq(&wave_red_seq, &wave_ir_seq);
        if (wave_red_seq != wave_red_seq_last || wave_ir_seq != wave_ir_seq_last) {
            wave_red_seq_last = wave_red_seq;
            wave_ir_seq_last = wave_ir_seq;
            SPO2_GetRawADC(&red_adc, &ir_adc);
            SendWavePair(red_adc, ir_adc);
        }
    }

    // 2. 计算结果处理
    if(SPO2_GetResult(&spo2, &hr, &pi))
    {
        spo2Data.spo2 = spo2; spo2Data.heart_rate = hr; spo2Data.pi = pi; spo2Data.status = 0;
        
        if (SPO2_SEND_BINARY_PACKET) {
            UART0_SendSPO2Data(&spo2Data);
        }
        
        if (SPO2_WAVE_TOOL_ENABLE) {
            printf("[[1,%u]]\r\n[[2,%u]]\r\n[[3,%u]]\r\n[[4,%u]]\r\n", 
                   (unsigned int)spo2, (unsigned int)hr, (unsigned int)pi, (unsigned int)SPO2_GetGain());
        }
    }
    
    // 3. 系统心跳与运行指示 & 增益循环测试逻辑
    if(Get1SecFlag())
    {
        static uint8_t gain_timer = 0;
        static uint8_t current_gain = 0;

        Clr1SecFlag();
        run_time_sec++;
        
        // 增益切换逻辑：每 10 秒切换一次
//        gain_timer++;
//        if (gain_timer >= 10) {
//            gain_timer = 0;
//            current_gain = (current_gain + 1) % 8; // 0->1->...->7->0
//            SPO2_SetGain(current_gain);
//            printf("\r\n>>> Gain Switch: Level %d <<<\r\n", current_gain);
//        }

        if (run_time_sec % 2 == 0) gpio_bit_set(GPIOA, GPIO_PIN_6);
        else gpio_bit_reset(GPIOA, GPIO_PIN_6);
    }
  }
}
