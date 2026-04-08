/*********************************************************************************************************
* 模块名称：UART0.c
* 摘    要：UART0驱动程序，实现串口发送功能
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2021年07月01日 
* 内    容：
*********************************************************************************************************/

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "UART0.h"
#include "gd32f3x0_conf.h"
#include <stdio.h>

#pragma import(__use_no_semihosting)             
struct __FILE { int handle; }; 

FILE __stdout;       
void _sys_exit(int x) { 
	x = x; 
} 

/*********************************************************************************************************
*                                              内部变量声明
*********************************************************************************************************/
#define UART0_RX_BUF_SIZE 64
static uint8_t s_rxBuf[UART0_RX_BUF_SIZE];
static uint8_t s_rxHead = 0;
static uint8_t s_rxTail = 0;

static uint8_t s_cmdReceived = 0;
static uint8_t s_lastCmd = 0;
static uint8_t s_lastVal = 0;

/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/
static  void  ConfigUART(unsigned int bound);              //配置UART GPIO、RCU、USART
static  void  ParseCmdPacket(uint8_t data);                //解析命令包

/*********************************************************************************************************
* 函数名称：ConfigUART
* 函数功能：配置UART参数  
* 输入参数：bound-波特率
* 输出参数：void
* 返 回 值：void
* 完成日期：2021年07月01日
* 注    意：使用TX (PA9)作为发送引脚
*********************************************************************************************************/
static  void  ConfigUART(unsigned int bound)
{
  rcu_periph_clock_enable(RCU_GPIOA);  //使能GPIOA时钟
  rcu_periph_clock_enable(RCU_USART0); //使能USART0时钟

  //配置TX引脚 GPIO (PA9)
  gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9);
  gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9);
  gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
  
  //配置RX引脚 GPIO (PA10)
  gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_10);
  gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);

  //配置USART参数
  usart_deinit(USART0);                                 //复位USART0
  usart_baudrate_set(USART0, bound);                    //设置波特率
  usart_stop_bit_set(USART0, USART_STB_1BIT);           //设置停止位
  usart_word_length_set(USART0, USART_WL_8BIT);         //设置字长
  usart_parity_config(USART0, USART_PM_NONE);           //无奇偶校验
  usart_hardware_flow_rts_config(USART0, USART_RTS_DISABLE); //禁用硬件流控RTS
  usart_hardware_flow_cts_config(USART0, USART_CTS_DISABLE); //禁用硬件流控CTS
  usart_receive_config(USART0, USART_RECEIVE_ENABLE);   //使能接收 (开启接收)
  usart_transmit_config(USART0, USART_TRANSMIT_ENABLE); //使能发送
  
  // 配置中断
  usart_interrupt_enable(USART0, USART_INT_RBNE);       //使能接收中断
  nvic_irq_enable(USART0_IRQn, 2, 0);                   //配置中断优先级 (降低至 1，让位给采样定时器)
  
  usart_enable(USART0);                                 //使能USART
}

/*********************************************************************************************************
* 函数名称：InitUART0
* 函数功能：初始化UART模块 
* 输入参数：bound-波特率
* 输出参数：void
* 返 回 值：void
* 完成日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void InitUART0(unsigned int bound)
{
  ConfigUART(bound);    //配置UART
}

/*********************************************************************************************************
* 函数名称：WriteUART0
* 函数功能：向串口发送数据  
* 输入参数：pBuf-发送缓冲区首地址，len-发送长度
* 输出参数：void
* 返 回 值：发送字节数
* 完成日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
unsigned char  WriteUART0(unsigned char *pBuf, unsigned char len)
{
  unsigned char i;
  uint32_t timeout = 0;

  for(i = 0; i < len; i++)
  {
    usart_data_transmit(USART0, pBuf[i]);
    
    // 等待发送缓冲区空，增加超时防止死循环
    timeout = 0;
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE))
    {
        timeout++;
        if(timeout > 0xFFFF) break; // 简单超时机制
    }
  }
                                                                  
  return len;
}

/*********************************************************************************************************
* 函数名称：UART0_SendSPO2Data
* 函数功能：发送血氧数据包
* 输入参数：pData-血氧数据结构体指针
* 输出参数：void
* 返 回 值：void
* 完成日期：2026年01月31日
* 注    意：数据包格式: HEAD(0xAA) + LENGTH(0x03) + SPO2 + HR + PI + TAIL(0x55)
*********************************************************************************************************/
void UART0_SendSPO2Data(SPO2Data_t *pData)
{
    u8 sendBuf[9];
    
    sendBuf[0] = SPO2_PACKET_HEAD;
    sendBuf[1] = SPO2_PACKET_LENGTH;
    sendBuf[2] = pData->spo2;
    sendBuf[3] = pData->heart_rate;
    sendBuf[4] = pData->pi;
    sendBuf[5] = pData->status;
    sendBuf[6] = pData->filter_status;
    sendBuf[7] = pData->gain_level;
    sendBuf[8] = SPO2_PACKET_TAIL;
    
    WriteUART0(sendBuf, 9);
}

/*********************************************************************************************************
* 函数名称：USART0_IRQHandler
* 函数功能：串口0中断服务函数
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2026年03月05日
*********************************************************************************************************/
void USART0_IRQHandler(void)
{
    if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE))
    {
        uint8_t data = (uint8_t)usart_data_receive(USART0);
        ParseCmdPacket(data);
    }
}

/*********************************************************************************************************
* 函数名称：ParseCmdPacket
* 函数功能：解析命令包
* 输入参数：data-接收到的字节
* 输出参数：void
* 返 回 值：void
* 完成日期：2026年03月05日
* 注    意：协议: 0xAB 0x02 CMD VAL 0x55
*********************************************************************************************************/
static void ParseCmdPacket(uint8_t data)
{
    static uint8_t s_state = 0;
    static uint8_t s_len = 0;
    static uint8_t s_buf[4];
    static uint8_t s_idx = 0;
    
    switch(s_state)
    {
        case 0: // Header
            if(data == 0xAB) s_state = 1;
            break;
        case 1: // Length
            if(data == 0x02) { s_len = data; s_idx = 0; s_state = 2; }
            else if(data == 0xAB) s_state = 1;
            else s_state = 0;
            break;
        case 2: // Data
            s_buf[s_idx++] = data;
            if(s_idx >= s_len) s_state = 3;
            break;
        case 3: // Tail
            if(data == 0x55)
            {
                s_lastCmd = s_buf[0];
                s_lastVal = s_buf[1];
                s_cmdReceived = 1;
                s_state = 0;
            }
            else if(data == 0xAB) s_state = 1;
            else s_state = 0;
            break;
        default:
            s_state = 0;
            break;
    }
}

/*********************************************************************************************************
* 函数名称：UART0_GetCmd
* 函数功能：获取最新的控制命令
* 输入参数：cmd-命令指针, val-参数指针
* 输出参数：1-有新命令, 0-无
* 返 回 值：状态
* 完成日期：2026年03月05日
*********************************************************************************************************/
unsigned char UART0_GetCmd(unsigned char *cmd, unsigned char *val)
{
    if(s_cmdReceived)
    {
        *cmd = s_lastCmd;
        *val = s_lastVal;
        s_cmdReceived = 0;
        return 1;
    }
    return 0;
}

/*********************************************************************************************************
* 函数名称：fputc
* 函数功能：重定向c库printf函数  
* 输入参数：ch-字符，f-文件指针
* 输出参数：void
* 返 回 值：int 
* 完成日期：2021年07月01日
* 注    意：注意勾选Use MicroLIB
*********************************************************************************************************/
int fputc(int ch, FILE *f)
{
  uint32_t timeout = 0;
  usart_data_transmit(USART0, (uint8_t)ch);
  
  // 等待发送缓冲区空，增加超时防止死循环
  while(RESET == usart_flag_get(USART0, USART_FLAG_TBE))
  {
      timeout++;
      if(timeout > 0xFFFF) break;
  }
  return ch;  
}
