/*********************************************************************************************************
* 模块名称：UART0.c
* 摘    要：串口模块，包括串口模块初始化，以及中断服务函数处理，以及读写串口函数实现
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

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "UART0.h"
#include "gd32f30x_conf.h"
#include "Queue.h"
#include "Timer.h"

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
  UART_STATE_ON,  //串口正在发送数据
  UART_STATE_MAX
}EnumUARTState;             

/*********************************************************************************************************
*                                              内部变量定义
*********************************************************************************************************/    
static  StructCirQue s_structUARTSendCirQue;             //发送串口循环队列
static  StructCirQue s_structUARTRecCirQue;              //接收串口循环队列
static  unsigned char  s_arrSendBuf[UART0_BUF_SIZE];     //发送串口循环队列的缓冲区
static  unsigned char  s_arrRecBuf[UART0_BUF_SIZE];      //接收串口循环队列的缓冲区

static  unsigned char  s_iUARTTxSts;                     //串口发送数据状态

/* SPO2相关变量 */
static SPO2_Data s_spo2Data;                    // 当前血氧数据
static SPO2_Packet s_spo2Packet;                // 当前血氧数据包
static EnumSPO2Mode s_spo2Mode;                // SPO2工作模式
static u32 s_lastSendTime;                      // 上次发送时间
static u16 s_dataCounter;                       // 数据计数器

/* 模拟数据基准值 */
static u8 s_spo2Base = 98;                      // 血氧基准值
static u8 s_hrBase = 75;                        // 心率基准值
static u8 s_piBase = 25;                        // 灌注指数基准值(2.5%)
          
/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/
static  void  InitUARTBuf(void);                           //初始化串口缓冲区，包括发送缓冲区和接收缓冲区 
static  unsigned char   WriteReceiveBuf(unsigned char d);  //将接收到的数据写入接收缓冲区
static  unsigned char   ReadSendBuf(unsigned char *p);     //读取发送缓冲区中的数据
                                            
static  void  ConfigUART(unsigned int bound);              //配置串口相关的参数，包括GPIO、RCU、USART和NVIC 
static  void  EnableUARTTx(void);                          //使能串口发送，WriteUARTx中调用，每次发送数据之后需要调用
static  void  ConfigUARTSPO2(void);                         //配置SPO2 UART相关参数
static u8 CreateSPO2Packet(SPO2_Packet* pPacket, SPO2_Data* pData); // 创建SPO2数据包                                      
                                              
/*********************************************************************************************************
*                                              内部函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：InitUARTBuf
* 函数功能：初始化串口缓冲区，包括发送缓冲区和接收缓冲区  
* 输入参数：void
* 输出参数：void
* 返 回 值：void 
* 创建日期：2021年07月01日
* 注    意：
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

/*********************************************************************************************************
* 函数名称：WriteReceiveBuf
* 函数功能：写数据到串口接收缓冲区 
* 输入参数：d，待写入串口接收缓冲区的数据
* 输出参数：void
* 返 回 值：写入数据成功标志，0-不成功，1-成功 
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static  unsigned char  WriteReceiveBuf(unsigned char d)
{
  unsigned char ok = 0;  //写入数据成功标志，0-不成功，1-成功
                                                                    
  ok = EnQueue(&s_structUARTRecCirQue, &d, 1);   
                                                                    
  return ok;             //返回写入数据成功标志，0-不成功，1-成功 
}

/*********************************************************************************************************
* 函数名称：ReadSendBuf
* 函数功能：读取串口发送缓冲区中的数据 
* 输入参数：p，读出来的数据存放的首地址
* 输出参数：p，读出来的数据存放的首地址
* 返 回 值：读取数据成功标志，0-不成功，1-成功 
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static  unsigned char  ReadSendBuf(unsigned char *p)
{
  unsigned char ok = 0;  //读取数据成功标志，0-不成功，1-成功
                                                                   
  ok = DeQueue(&s_structUARTSendCirQue, p, 1);  
                                                                   
  return ok;             //返回读取数据成功标志，0-不成功，1-成功 
}

/*********************************************************************************************************
* 函数名称：ConfigUART
* 函数功能：配置串口相关的参数，包括GPIO、RCU、USART和NVIC  
* 输入参数：bound，波特率
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static  void  ConfigUART(unsigned int bound)
{
  rcu_periph_clock_enable(RCU_GPIOA);  //使能GPIOA时钟
  rcu_periph_clock_enable(RCU_USART1); //使能串口时钟

  //配置TX的GPIO (PA10)
  gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);
  
  //配置RX的GPIO (PA9)
  gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_9);

  //配置USART的参数
  usart_deinit(USART1);                                 //RCU配置恢复默认值
  usart_baudrate_set(USART1, bound);                    //设置波特率
  usart_stop_bit_set(USART1, USART_STB_1BIT);           //设置停止位
  usart_word_length_set(USART1, USART_WL_8BIT);         //设置数据字长度
  usart_parity_config(USART1, USART_PM_NONE);           //设置奇偶校验位
  usart_receive_config(USART1, USART_RECEIVE_ENABLE);   //使能接收
  usart_transmit_config(USART1, USART_TRANSMIT_ENABLE); //使能发送

  usart_interrupt_enable(USART1, USART_INT_RBNE);       //使能接收缓冲区非空中断
  usart_interrupt_enable(USART1, USART_INT_TBE);        //使能发送缓冲区空中断
  usart_enable(USART1);                                 //使能串口
  
  nvic_irq_enable(USART1_IRQn, 0, 0);                   //使能串口中断，设置优先级
                                                                     
  s_iUARTTxSts = UART_STATE_OFF;                        //串口发送数据状态设置为未发送数据
}

/*********************************************************************************************************
* 函数名称：EnableUARTTx
* 函数功能：使能串口发送，在WriteUARTx中调用，即每次发送数据之后需要调用这个函数来使能发送中断 
* 输入参数：void
* 输出参数：void
* 返 回 值：void 
* 创建日期：2021年07月01日
* 注    意：s_iUARTTxSts = UART_STATE_ON;这行代码必须放在usart_interrupt_enable之前，否则会导致中断打开无法执行
*********************************************************************************************************/
static  void  EnableUARTTx(void)
{
  s_iUARTTxSts = UART_STATE_ON;                     //串口发送数据状态设置为正在发送数据

  usart_interrupt_enable(USART1, USART_INT_TBE);
}

/*********************************************************************************************************
* 函数名称：USART0_IRQHandler
* 函数功能：USART0中断服务函数 
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void USART1_IRQHandler(void)            
{
  unsigned char  uData = 0;

  if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE) != RESET)  //接收缓冲区非空中断
  {                                                         
    usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RBNE);          //清除USART1中断挂起
    __NVIC_ClearPendingIRQ(USART1_IRQn);
    uData = usart_data_receive(USART1);                               //将USART1接收到的数据保存到uData
                                                          
    WriteReceiveBuf(uData);                                           //将接收到的数据写入接收缓冲区                                 
  }                                                         
                                                            
  if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_ERR_ORERR) == SET)       //溢出错误标志为1
  {                                                         
    usart_interrupt_flag_clear(USART1, USART_INT_FLAG_ERR_ORERR);             //清除溢出错误标志
    
    usart_data_receive(USART1);                                       //读取USART_DATA 
  }                                                         
                                                           
  if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_TBE)!= RESET)    //发送缓冲区空中断
  {                        
    __NVIC_ClearPendingIRQ(USART1_IRQn); 
                                   
    ReadSendBuf(&uData);                                 //读取发送缓冲区的数据到uData
                                                                    
    usart_data_transmit(USART1, uData);                  //将uData写入USART_DATA
                                                                                           
    if(QueueEmpty(&s_structUARTSendCirQue))              //当发送缓冲区为空时
    {                                                               
      s_iUARTTxSts = UART_STATE_OFF;                     //串口发送数据状态设置为未发送数据       
      usart_interrupt_disable(USART1, USART_INT_TBE);    //关闭串口发送缓冲区空中断
    }
  }
} 

/*********************************************************************************************************
*                                              API函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：InitUART0
* 函数功能：初始化UART模块 
* 输入参数：bound,波特率
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void InitUART0(unsigned int bound)
{
  InitUARTBuf();        //初始化串口缓冲区，包括发送缓冲区和接收缓冲区  
                  
  ConfigUART(bound);    //配置串口相关的参数，包括GPIO、RCU、USART和NVIC  
}

/*********************************************************************************************************
* 函数名称：WriteUART0
* 函数功能：写串口，即写数据到的串口发送缓冲区  
* 输入参数：pBuf，要写入数据的首地址，len，期望写入数据的个数
* 输出参数：void
* 返 回 值：成功写入数据的个数，不一定与形参len相等
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
unsigned char  WriteUART0(unsigned char *pBuf, unsigned char len)
{
  unsigned char wLen = 0;  //实际写入数据的个数
                                                                  
  wLen = EnQueue(&s_structUARTSendCirQue, pBuf, len);

  if(wLen < UART0_BUF_SIZE)
  {
    if(s_iUARTTxSts == UART_STATE_OFF)
    {
      EnableUARTTx();
    }    
  }
                                                                  
  return wLen;             //返回实际写入数据的个数
}

/*********************************************************************************************************
* 函数名称：ReadUART0
* 函数功能：读串口，即读取串口接收缓冲区中的数据  
* 输入参数：pBuf，读取的数据存放的首地址，len，期望读取数据的个数
* 输出参数：pBuf，读取的数据存放的首地址
* 返 回 值：成功读取数据的个数，不一定与形参len相等
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
unsigned char  ReadUART0(unsigned char *pBuf, unsigned char len)
{
  unsigned char rLen = 0;  //实际读取数据长度
                                                    
  rLen = DeQueue(&s_structUARTRecCirQue, pBuf, len);

  return rLen;             //返回实际读取数据的长度
}
    
/*********************************************************************************************************
* 函数名称：fputc
* 函数功能：重定向函数  
* 输入参数：ch，f
* 输出参数：void
* 返 回 值：int 
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
int fputc(int ch, FILE *f)
{
  usart_data_transmit(USART1, (uint8_t) ch);  //将数据写入USART数据寄存器
  
  while(RESET == usart_flag_get(USART1, USART_FLAG_TBE));
  
  return ch;  
}

/*********************************************************************************************************
*                                              SPO2相关函数实现
*********************************************************************************************************/

/*********************************************************************************************************
* 函数名称：ConfigUARTSPO2
* 函数功能：配置SPO2 UART相关参数（使用USART1，PA9-RX，PA10-TX）
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2025年12月31日
* 注    意：
*********************************************************************************************************/
static void ConfigUARTSPO2(void)
{
    /* 使能时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);   // 使能GPIOA时钟
    rcu_periph_clock_enable(RCU_USART1);  // 使能USART1时钟

    /* 配置GPIO引脚 */
    /* PA9 - RX - 浮空输入 */
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
    
    /* PA10 - TX - 推挽复用输出 */
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

    /* 配置USART1参数 */
    usart_deinit(USART1);                                  // 恢复默认配置
    usart_baudrate_set(USART1, SPO2_UART_BAUDRATE);       // 设置波特率
    usart_stop_bit_set(USART1, USART_STB_1BIT);            // 设置停止位
    usart_word_length_set(USART1, USART_WL_8BIT);          // 设置数据字长度
    usart_parity_config(USART1, USART_PM_NONE);            // 设置奇偶校验位
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);    // 使能接收
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);  // 使能发送

    usart_enable(USART1);                                  // 使能串口
}

/*********************************************************************************************************
* 函数名称：CreateSPO2Packet
* 函数功能：创建SPO2数据包
* 输入参数：pPacket-数据包指针, pData-数据指针
* 输出参数：pPacket-填充好的数据包
* 返 回 值：创建成功标志
* 创建日期：2025年12月31日
* 注    意：
*********************************************************************************************************/
static u8 CreateSPO2Packet(SPO2_Packet* pPacket, SPO2_Data* pData)
{
    if(pPacket == NULL || pData == NULL)
    {
        return FALSE;
    }
    
    /* 填充数据包 */
    pPacket->header = SPO2_PACKET_HEADER;
    pPacket->length = SPO2_PACKET_LENGTH;
    pPacket->spo2 = pData->spo2;
    pPacket->heart_rate = pData->heart_rate;
    pPacket->pi = pData->pi;
    pPacket->checksum = CalculateSPO2Checksum(pPacket);
    pPacket->end = SPO2_PACKET_END;
    
    return TRUE;
}

/*********************************************************************************************************
* 函数名称：InitUARTSPO2
* 函数功能：初始化SPO2 UART模块
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2025年12月31日
* 注    意：
*********************************************************************************************************/
void InitUARTSPO2(void)
{
    /* 初始化变量 */
    s_spo2Mode = SPO2_MODE_IDLE;
    s_lastSendTime = 0;
    s_dataCounter = 0;
    
    /* 初始化SPO2数据 */
    s_spo2Data.spo2 = s_spo2Base;
    s_spo2Data.heart_rate = s_hrBase;
    s_spo2Data.pi = s_piBase;
    s_spo2Data.valid = TRUE;
    
    /* 配置UART */
    ConfigUARTSPO2();
}

/*********************************************************************************************************
* 函数名称：GenerateSPO2Data
* 函数功能：生成模拟血氧数据
* 输入参数：pSpo2Data-血氧数据指针
* 输出参数：pSpo2Data-生成的血氧数据
* 返 回 值：void
* 创建日期：2025年12月31日
* 注    意：基于真实生理变化规律模拟数据
*********************************************************************************************************/
void GenerateSPO2Data(SPO2_Data* pSpo2Data)
{
    u8 variation;
    
    if(pSpo2Data == NULL)
    {
        return;
    }
    
    s_dataCounter++;
    variation = s_dataCounter % 10;
    
    /* 每50次发送调整基准值，模拟真实生理变化 */
    if(s_dataCounter % 50 == 0)
    {
        /* 血氧基准值调整(97-99) */
        if(s_dataCounter % 100 == 0)
        {
            s_spo2Base = 97 + (s_dataCounter / 100) % 3;
        }
        
        /* 心率基准值调整(72-79) */
        if(s_dataCounter % 75 == 0)
        {
            s_hrBase = 72 + (s_dataCounter / 75) % 8;
        }
        
        /* 灌注指数基准值调整(22-27) */
        if(s_dataCounter % 60 == 0)
        {
            s_piBase = 22 + (s_dataCounter / 60) % 6;
        }
    }
    
    /* 生成血氧饱和度数据(95-100%) */
    pSpo2Data->spo2 = s_spo2Base + (variation % 3) - 1;
    if(pSpo2Data->spo2 < SPO2_NORMAL_MIN) pSpo2Data->spo2 = SPO2_NORMAL_MIN;
    if(pSpo2Data->spo2 > SPO2_NORMAL_MAX) pSpo2Data->spo2 = SPO2_NORMAL_MAX;
    
    /* 生成心率数据(65-85 bpm) */
    pSpo2Data->heart_rate = s_hrBase + (variation % 8) - 3;
    if(pSpo2Data->heart_rate < HR_NORMAL_MIN) pSpo2Data->heart_rate = HR_NORMAL_MIN;
    if(pSpo2Data->heart_rate > HR_NORMAL_MAX) pSpo2Data->heart_rate = HR_NORMAL_MAX;
    
    /* 生成灌注指数数据(1.5-3.5%) */
    pSpo2Data->pi = s_piBase + (variation % 10) - 4;
    if(pSpo2Data->pi < 15) pSpo2Data->pi = 15;    // 1.5%
    if(pSpo2Data->pi > 35) pSpo2Data->pi = 35;    // 3.5%
    
    /* 数据有效标志 */
    pSpo2Data->valid = TRUE;
}

/*********************************************************************************************************
* 函数名称：CalculateSPO2Checksum
* 函数功能：计算SPO2数据包校验和
* 输入参数：pPacket-数据包指针
* 输出参数：void
* 返 回 值：校验和
* 创建日期：2025年12月31日
* 注    意：校验和 = 血氧值 + 心率值 + 灌注指数 + 数据长度
*********************************************************************************************************/
u8 CalculateSPO2Checksum(SPO2_Packet* pPacket)
{
    u8 checksum = 0;
    
    if(pPacket == NULL)
    {
        return 0;
    }
    
    checksum = pPacket->spo2 + pPacket->heart_rate + pPacket->pi + pPacket->length;
    
    return checksum;
}

/*********************************************************************************************************
* 函数名称：SendSPO2Packet
* 函数功能：发送SPO2数据包
* 输入参数：pPacket-数据包指针
* 输出参数：void
* 返 回 值：发送成功标志
* 创建日期：2025年12月31日
* 注    意：
*********************************************************************************************************/
u8 SendSPO2Packet(SPO2_Packet* pPacket)
{
    u8* pData = (u8*)pPacket;
    u8 i;
    
    if(pPacket == NULL)
    {
        return FALSE;
    }
    
    /* 发送数据包 */
    for(i = 0; i < sizeof(SPO2_Packet); i++)
    {
        usart_data_transmit(USART1, pData[i]);
        while(RESET == usart_flag_get(USART1, USART_FLAG_TBE));
    }
    
    return TRUE;
}

/*********************************************************************************************************
* 函数名称：SetSPO2Mode
* 函数功能：设置SPO2工作模式
* 输入参数：mode-工作模式
* 输出参数：void
* 返 回 值：void
* 创建日期：2025年12月31日
* 注    意：
*********************************************************************************************************/
void SetSPO2Mode(EnumSPO2Mode mode)
{
    if(mode < SPO2_MODE_MAX)
    {
        s_spo2Mode = mode;
        s_lastSendTime = GetSystem1Ms();  // 重置发送时间
    }
}

/*********************************************************************************************************
* 函数名称：GetSPO2Mode
* 函数功能：获取SPO2当前工作模式
* 输入参数：void
* 输出参数：void
* 返 回 值：当前工作模式
* 创建日期：2025年12月31日
* 注    意：
*********************************************************************************************************/
EnumSPO2Mode GetSPO2Mode(void)
{
    return s_spo2Mode;
}

/*********************************************************************************************************
* 函数名称：GetSPO2Data
* 函数功能：获取当前SPO2数据
* 输入参数：void
* 输出参数：void
* 返 回 值：当前SPO2数据指针
* 创建日期：2025年12月31日
* 注    意：
*********************************************************************************************************/
SPO2_Data* GetSPO2Data(void)
{
    return &s_spo2Data;
}

/*********************************************************************************************************
* 函数名称：ProcessUARTSPO2
* 函数功能：处理SPO2相关任务，在主循环中调用
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2025年12月31日
* 注    意：
*********************************************************************************************************/
void ProcessUARTSPO2(void)
{
    u32 currentTime;
    
    /* 根据工作模式处理 */
    switch(s_spo2Mode)
    {
        case SPO2_MODE_SINGLE:
            /* 单次发送模式 - 发送一次后回到空闲模式 */
            GenerateSPO2Data(&s_spo2Data);
            CreateSPO2Packet(&s_spo2Packet, &s_spo2Data);
            SendSPO2Packet(&s_spo2Packet);
            s_spo2Mode = SPO2_MODE_IDLE;
            break;
            
        case SPO2_MODE_CONTINUOUS:
            /* 连续发送模式 - 定时发送数据 */
            currentTime = GetSystem1Ms();
            if((currentTime - s_lastSendTime) >= SPO2_SEND_INTERVAL)
            {
                GenerateSPO2Data(&s_spo2Data);
                CreateSPO2Packet(&s_spo2Packet, &s_spo2Data);
                SendSPO2Packet(&s_spo2Packet);
                s_lastSendTime = currentTime;
            }
            break;
            
        case SPO2_MODE_TEST:
            /* 测试模式 - 立即发送一次测试数据 */
            s_spo2Data.spo2 = 98;
            s_spo2Data.heart_rate = 75;
            s_spo2Data.pi = 25;
            s_spo2Data.valid = TRUE;
            CreateSPO2Packet(&s_spo2Packet, &s_spo2Data);
            SendSPO2Packet(&s_spo2Packet);
            s_spo2Mode = SPO2_MODE_IDLE;
            break;
            
        case SPO2_MODE_IDLE:
        default:
            /* 空闲模式 - 不发送数据 */
            break;
    }
}
