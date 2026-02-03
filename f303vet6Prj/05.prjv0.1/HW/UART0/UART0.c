/*********************************************************************************************************
* 模块名称：UART0.c
* 摘    要：串口0模块，包括该模块初始化函数、中断处理函数、读写串口函数以及数据处理函数实现
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2021年07月01日 
*********************************************************************************************************/

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "UART0.h"
#include "gd32f30x_conf.h"
#include "Queue.h"
#include "DataType.h"
#include "SPO2_Display.h"

#pragma import(__use_no_semihosting)
struct __FILE { int handle; };
FILE __stdout;
void _sys_exit(int x) { x = x; }
//的撒大
/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/
//串口发送状态
typedef enum
{
  UART_STATE_OFF, //串口未发送数据
  UART_STATE_ON,  //串口正发送数据
  UART_STATE_MAX
}EnumUARTState;             

/*********************************************************************************************************
*                                              内部变量声明
*********************************************************************************************************/    
static  StructCirQue s_structUARTSendCirQue;             //发送处理循环队列
static  StructCirQue s_structUARTRecCirQue;              //接收处理循环队列
static  unsigned char  s_arrSendBuf[UART0_BUF_SIZE];     //发送处理循环队列中的缓冲区
static  unsigned char  s_arrRecBuf[UART0_BUF_SIZE];      //接收处理循环队列中的缓冲区

static  unsigned char  s_iUARTTxSts;                     //串口发送处理状态

static SPO2Data_t s_spo2Data;                            // 血氧数据

/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/
static  void  InitUARTBuf(void);                           //初始化串口缓冲区，包括发送缓冲区和接收缓冲区 
static  unsigned char   WriteReceiveBuf(unsigned char d);  //将接收到的数据写入接收缓冲区
static  unsigned char   ReadSendBuf(unsigned char *p);     //读取发送缓冲区中的数据
                                            
static  void  ConfigUART(unsigned int bound);              //设置串口相关的参数：GPIO、RCU、USART、NVIC 
static  void  EnableUARTTx(void);                          //使能串口发送，在WriteUARTx中调用，每次发数据之前需要调用                                      
static  void  ParseSPO2Packet(unsigned char data);         //解析SPO2数据包

/*********************************************************************************************************
*                                              内部函数实现
*********************************************************************************************************/
static  void  InitUARTBuf(void)
{
  signed short i;

  for(i = 0; i < UART0_BUF_SIZE; i++)
  {
    s_arrSendBuf[i] = 0;
    s_arrRecBuf[i]  = 0;  
  }

  InitQueue(&s_structUARTSendCirQue, s_arrSendBuf, UART0_BUF_SIZE);
  InitQueue(&s_structUARTRecCirQue,  s_arrRecBuf,  UART0_BUF_SIZE);
}

static  unsigned char  WriteReceiveBuf(unsigned char d)
{
  unsigned char ok = 0;  
  ok = EnQueue(&s_structUARTRecCirQue, &d, 1);   
  return ok;             
}

static  unsigned char  ReadSendBuf(unsigned char *p)
{
  unsigned char ok = 0;  
  ok = DeQueue(&s_structUARTSendCirQue, p, 1);  
  return ok;             
}

static  void  ConfigUART(unsigned int bound)
{
  rcu_periph_clock_enable(RCU_GPIOA);  
  rcu_periph_clock_enable(RCU_USART0); 

  gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
  gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

  usart_deinit(USART0);                                 
  usart_baudrate_set(USART0, bound);                    
  usart_stop_bit_set(USART0, USART_STB_1BIT);           
  usart_word_length_set(USART0, USART_WL_8BIT);         
  usart_parity_config(USART0, USART_PM_NONE);           
  usart_receive_config(USART0, USART_RECEIVE_ENABLE);   
  usart_transmit_config(USART0, USART_TRANSMIT_ENABLE); 

  usart_interrupt_enable(USART0, USART_INT_RBNE);       
  usart_interrupt_enable(USART0, USART_INT_TBE);        
  usart_enable(USART0);                                 
  
  nvic_irq_enable(USART0_IRQn, 0, 0);                   
                                                                     
  s_iUARTTxSts = UART_STATE_OFF;                        
}

static  void  EnableUARTTx(void)
{
  s_iUARTTxSts = UART_STATE_ON;                     
  usart_interrupt_enable(USART0, USART_INT_TBE);
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
            }
            s_parseState = 0;
            break;
        default:
            s_parseState = 0;
            break;
    }
}

void USART0_IRQHandler(void)            
{
  unsigned char  uData = 0;

  if(usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET)  
  {                                                         
    usart_interrupt_flag_clear(USART0, USART_INT_FLAG_RBNE);          
    NVIC_ClearPendingIRQ(USART0_IRQn);
    uData = usart_data_receive(USART0);                               
                                                          
    WriteReceiveBuf(uData);                                           
  }                                                         
                                                            
  if(usart_interrupt_flag_get(USART0, USART_INT_FLAG_ERR_ORERR) == SET)       
  {                                                         
    usart_interrupt_flag_clear(USART0, USART_INT_FLAG_ERR_ORERR);             
    usart_data_receive(USART0);                                       
  }                                                         
                                                           
  if(usart_interrupt_flag_get(USART0, USART_INT_FLAG_TBE)!= RESET)    
  {                        
    NVIC_ClearPendingIRQ(USART0_IRQn); 
    ReadSendBuf(&uData);                                 
    usart_data_transmit(USART0, uData);                  
                                                                                           
    if(QueueEmpty(&s_structUARTSendCirQue))              
    {                                                               
      s_iUARTTxSts = UART_STATE_OFF;                     
      usart_interrupt_disable(USART0, USART_INT_TBE);    
    }
  }
} 

/*********************************************************************************************************
*                                              API函数实现
*********************************************************************************************************/
void InitUART0(unsigned int bound)
{
  InitUARTBuf();        
  ConfigUART(bound);    
}

unsigned char  WriteUART0(unsigned char *pBuf, unsigned char len)
{
  unsigned char wLen = 0;  
  wLen = EnQueue(&s_structUARTSendCirQue, pBuf, len);

  if(wLen < UART0_BUF_SIZE)
  {
    if(s_iUARTTxSts == UART_STATE_OFF)
    {
      EnableUARTTx();
    }    
  }
  return wLen;             
}

unsigned char  ReadUART0(unsigned char *pBuf, unsigned char len)
{
  unsigned char rLen = 0;  
  rLen = DeQueue(&s_structUARTRecCirQue, pBuf, len);
  return rLen;             
}

void UART0_ProcessSPO2Data(void)
{
    unsigned char data;
    while(ReadUART0(&data, 1))
    {
        ParseSPO2Packet(data);
    }
}
    
int fputc(int ch, FILE *f)
{
  usart_data_transmit(USART0, (uint8_t) ch);  
  while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
  return ch;  
}
