/*********************************************************************************************************
* 模块名称：UART1.c
* 摘    要：UART1 驱动程序，用于接收 SPO2 血氧数据
* 当前版本：1.0.0
* 作    者：Leyutek
* 完成日期：2026年01月08日
*********************************************************************************************************/
#include "UART1.h"
#include "gd32f30x_conf.h"
#include "Queue.h"
#include "DataType.h"
#include "SPO2_Display.h"
#include <stdio.h>

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量声明
*********************************************************************************************************/    
static  StructCirQue s_structUART1RecCirQue;              // 接收循环队列
static  unsigned char  s_arrUART1RecBuf[UART1_BUF_SIZE];  // 接收缓冲区

static SPO2Data_t s_spo2Data;                             // 血氧数据

#define UART1_MON_SIZE 32
static unsigned char s_arrUART1MonBuf[UART1_MON_SIZE];
static unsigned char s_u8UART1MonPos = 0;
static unsigned char s_u8UART1MonCount = 0;

/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/
static  void  InitUART1Buf(void);                          // 初始化缓冲区
static  unsigned char WriteReceiveBuf1(unsigned char d);   // 写接收缓冲区
static  void  ConfigUART1(unsigned int bound);             // 配置 UART1
static  void  ParseSPO2Packet(unsigned char data);         // 解析 SPO2 数据包
static  void  UART1_MonPush(unsigned char d);

/*********************************************************************************************************
*                                              内部函数实现
*********************************************************************************************************/

static void InitUART1Buf(void)
{
    unsigned short i;
    for(i = 0; i < UART1_BUF_SIZE; i++)
    {
        s_arrUART1RecBuf[i]  = 0;  
    }
    InitQueue(&s_structUART1RecCirQue, s_arrUART1RecBuf, UART1_BUF_SIZE);
}

static unsigned char WriteReceiveBuf1(unsigned char d)
{
    return EnQueue(&s_structUART1RecCirQue, &d, 1);
}

static void UART1_MonPush(unsigned char d)
{
    s_arrUART1MonBuf[s_u8UART1MonPos] = d;
    s_u8UART1MonPos++;
    if (s_u8UART1MonPos >= UART1_MON_SIZE)
    {
        s_u8UART1MonPos = 0;
    }
    if (s_u8UART1MonCount < UART1_MON_SIZE)
    {
        s_u8UART1MonCount++;
    }
}

static void ConfigUART1(unsigned int bound)
{
    // 1. 使能时钟 GPIOA (PA2/PA3) 和 USART1
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART1);

    // 2. 配置 GPIO
    // TX (PA2): 复用推挽输出
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    // RX (PA3): 浮空输入
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_3);

    // 3. 配置 USART1 参数
    usart_deinit(USART1);
    usart_baudrate_set(USART1, bound);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_parity_config(USART1, USART_PM_NONE);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);

    // 4. 配置中断
    usart_interrupt_enable(USART1, USART_INT_RBNE); // 使能接收非空中断
    nvic_irq_enable(USART1_IRQn, 1, 0);             // 抢占优先级 1 (低于 UART0 的优先级)

    // 5. 使能 USART1
    usart_enable(USART1);
}

static void ParseSPO2Packet(unsigned char data)
{
    static u8 s_parseState = 0;
    static u8 s_packetIndex = 0;
    static u8 s_packetLen = 0;
    static u8 s_packetBuffer[10];

    switch(s_parseState)
    {
        case 0: // Wait for Header
            if(data == SPO2_PACKET_HEAD) // 0xAA
            {
                s_parseState = 1;
            }
            break;
        case 1: // Wait for Length
             if(data == SPO2_PACKET_LENGTH) // 0x03
             {
                 s_packetLen = data;
                 s_packetIndex = 0;
                 s_parseState = 2;
             }
             else
             {
                 s_parseState = 0; 
             }
             break;
        case 2: // Data
            s_packetBuffer[s_packetIndex++] = data;
            if(s_packetIndex >= s_packetLen)
            {
                s_parseState = 3;
            }
            break;
        case 3: // Tail
            if(data == SPO2_PACKET_TAIL) // 0x55
            {
                s_spo2Data.spo2 = s_packetBuffer[0];
                s_spo2Data.heart_rate = s_packetBuffer[1];
                s_spo2Data.pi = s_packetBuffer[2];
                s_spo2Data.status = 0; // Valid
                s_spo2Data.update_flag = 1;
                
                SPO2_DisplayData(&s_spo2Data);
                SPO2_DisplayStatus(1);
            }
            s_parseState = 0;
            break;
        default:
            s_parseState = 0;
            break;
    }
}

/*********************************************************************************************************
*                                              API函数实现
*********************************************************************************************************/

void InitUART1(unsigned int bound)
{
    InitUART1Buf();
    ConfigUART1(bound);
}

unsigned char ReadUART1(unsigned char *pBuf, unsigned char len)
{
    return DeQueue(&s_structUART1RecCirQue, pBuf, len);
}

unsigned char UART1_GetLastRx(unsigned char *pBuf, unsigned char max_len)
{
    unsigned char cnt = s_u8UART1MonCount;
    unsigned char i;
    unsigned char start;

    if (cnt > max_len)
    {
        cnt = max_len;
    }

    start = (unsigned char)(s_u8UART1MonPos + UART1_MON_SIZE - cnt);
    if (start >= UART1_MON_SIZE)
    {
        start -= UART1_MON_SIZE;
    }

    for (i = 0; i < cnt; i++)
    {
        unsigned char idx = (unsigned char)(start + i);
        if (idx >= UART1_MON_SIZE)
        {
            idx -= UART1_MON_SIZE;
        }
        pBuf[i] = s_arrUART1MonBuf[idx];
    }

    return cnt;
}

unsigned char UART1_GetLastParsed(SPO2Data_t *pData)
{
    if (s_spo2Data.update_flag)
    {
        *pData = s_spo2Data;
        s_spo2Data.update_flag = 0;
        return 1;
    }
    return 0;
}

void UART1_ProcessSPO2Data(void)
{
    unsigned char data;
    while(ReadUART1(&data, 1))
    {
        ParseSPO2Packet(data);
    }
}

// 接收自检函数 (保留用于调试)
void UART1_ReceiveSelfCheck(void)
{
    // 该函数在 SPO2 解析模式下可能不再直接使用，但保留作为参考
    // 如果需要查看原始数据，可以在 ParseSPO2Packet 中添加 printf
}

// USART1 中断服务程序
void USART1_IRQHandler(void)
{
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE) != RESET)
    {
        unsigned char uData = usart_data_receive(USART1);
        WriteReceiveBuf1(uData);
        UART1_MonPush(uData);
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RBNE);
    }
    
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_ERR_ORERR) != RESET)
    {
        usart_data_receive(USART1); // 清除错误标志
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_ERR_ORERR);
    }
}
