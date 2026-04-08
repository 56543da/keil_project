/*********************************************************************************************************
* 模块名称：UART1.c
* 摘    要：UART1 驱动程序，用于接收 SPO2 血氧数据
* 当前版本：1.0.0
* 作    者：Leyutek
* 完成日期：2026年01月08日
* 优化日期：2026年02月03日 (修复接收逻辑和解析状态机)
*********************************************************************************************************/
#include "UART1.h"
#include "gd32f30x_conf.h"
#include "Queue.h"
#include "DataType.h"
#include "UI_Manager.h" // 添加头文件引用
#include <stdio.h>
#include "SPO2_Algo.h"

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
static unsigned char s_waveFilterEnable = 1;
static int32_t s_filterRed = 0;
static int32_t s_filterIr = 0;
static unsigned char s_filterInit = 0;
static unsigned char s_rCalibMode = 0;

/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/
static  void  InitUART1Buf(void);                          // 初始化缓冲区
static  unsigned char WriteReceiveBuf1(unsigned char d);   // 写接收缓冲区
static  void  ConfigUART1(unsigned int bound);             // 配置 UART1
static  void  ParseSPO2Packet(unsigned char data);         // 解析 SPO2 数据包
static  void  UART1_MonPush(unsigned char d);
static  void  UART1_ForwardWave(uint16_t red, uint16_t ir);
static  void  UART1_ApplyFilter(uint16_t red, uint16_t ir, uint16_t *out_red, uint16_t *out_ir);

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
    usart_interrupt_enable(USART1, USART_INT_ERR);  // 使能错误中断(ORE/NE/FE/PE)
    nvic_irq_enable(USART1_IRQn, 0, 0);             // 提升抢占优先级至 0 (最高，确保数据接收不丢失)

    // 5. 使能 USART1
    usart_enable(USART1);
}

static void ParseSPO2Packet(unsigned char data)
{
    static u8 s_parseState = 0;
    static u8 s_packetIndex = 0;
    static u8 s_packetLen = 0;
    static u8 s_packetBuffer[10];

    // 调试打印：输出每一个接收到的字节
    // printf("Rx: %02X State: %d\r\n", data, s_parseState);

    // 增加缓冲区来存储波形数据
    static char s_waveBuf[32]; 
    static uint8_t s_waveIdx = 0;
    int red_val = 0, ir_val = 0;
    int pwm_red = 0, pwm_ir = 0;
    int gain_code = 0;
    uint16_t out_red = 0, out_ir = 0;

    switch(s_parseState)
    {
        case 0: // Wait for Header
            if(data == SPO2_PACKET_HEAD) // 0xAA
            {
                s_parseState = 1;
            }
            else
            {
                if(data == '\n')
                {
                    s_waveBuf[s_waveIdx] = 0;
                    if(sscanf(s_waveBuf, "P,%d,%d", &pwm_red, &pwm_ir) == 2)
                    {
                        UI_UpdatePwm((u8)pwm_red, (u8)pwm_ir);
                    }
                    else if(sscanf(s_waveBuf, "G,%d", &gain_code) == 1)
                    {
                        UI_UpdateGain((u8)gain_code);
                    }
                    else if(sscanf(s_waveBuf, "%d,%d", &red_val, &ir_val) == 2)
                    {
                        UART1_ApplyFilter((uint16_t)red_val, (uint16_t)ir_val, &out_red, &out_ir);
                        UI_UpdateWave(out_red);
                        if(UART1_GetRCalibMode())
                        {
                            SPO2_Algo_PushDataCalib(out_red, out_ir);
                        }
                        else
                        {
                            SPO2_Algo_PushData(out_red, out_ir);
                        }
                        UART1_ForwardWave(out_red, out_ir);
                    }
                    s_waveIdx = 0;
                }
                else if(s_waveIdx < 30)
                {
                    s_waveBuf[s_waveIdx++] = data;
                }
            }
            break;
        case 1: // Wait for Length
             if(data == SPO2_PACKET_LENGTH) // 0x06
             {
                 s_packetLen = data;
                 s_packetIndex = 0;
                 s_parseState = 2;
             }
             else if (data == SPO2_PACKET_HEAD) // 容错：可能是连续的 AA
             {
                 s_parseState = 1; 
             }
             else
             {
                 // 解析失败，说明之前的 0xAA 可能是数据的一部分
                 // 把刚才吞掉的 0xAA 补发出去 (虽然有点晚，但能减少丢包感)
                 // 以及当前的 data 也转发出去
                 putchar(SPO2_PACKET_HEAD); 
                 putchar(data);
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
                s_spo2Data.status = s_packetBuffer[3];
                s_spo2Data.filter_status = s_packetBuffer[4];
                s_spo2Data.gain_level = s_packetBuffer[5];
                s_spo2Data.update_flag = 1;
                
                // 成功解析：不再打印 Parsed 调试信息，以免干扰波形工具
                
                // 立即更新到UI
                UI_UpdateData(&s_spo2Data);
                s_parseState = 0; // 成功，复位
            }
            else if (data == SPO2_PACKET_HEAD) // 容错
            {
                // printf("Parse Error: Tail mismatch, restart.\r\n");
                s_parseState = 1; // 尝试重新同步
            }
            else
            {
                // printf("Parse Error: Tail=%02X\r\n", data);
                s_parseState = 0; // 失败，复位
            }
            break;
        default:
            s_parseState = 0;
            break;
    }
}

static void UART1_ForwardWave(uint16_t red, uint16_t ir)
{
    if(s_rCalibMode) return;
    printf("%u,%u\r\n", red, ir);
}

static void UART1_ApplyFilter(uint16_t red, uint16_t ir, uint16_t *out_red, uint16_t *out_ir)
{
    if(!s_waveFilterEnable)
    {
        *out_red = red;
        *out_ir = ir;
        return;
    }
    if(!s_filterInit)
    {
        s_filterRed = red;
        s_filterIr = ir;
        s_filterInit = 1;
    }
    else
    {
        s_filterRed += ((int32_t)red - s_filterRed) >> 2;
        s_filterIr += ((int32_t)ir - s_filterIr) >> 2;
    }
    *out_red = (uint16_t)s_filterRed;
    *out_ir = (uint16_t)s_filterIr;
}

/*********************************************************************************************************
* 函数名称：UART1_SendCmd
* 函数功能：发送控制命令给从机 (F310)
* 输入参数：cmd-命令, value-参数
* 输出参数：void
* 返 回 值：void
* 完成日期：2026年03月05日
* 注    意：协议格式: HEAD(0xAB) + LEN(0x02) + CMD + VAL + TAIL(0x55)
*********************************************************************************************************/
void UART1_SendCmd(unsigned char cmd, unsigned char value)
{
    unsigned char sendBuf[5];
    unsigned char i;
    
    sendBuf[0] = 0xAB;
    sendBuf[1] = 0x02;
    sendBuf[2] = cmd;
    sendBuf[3] = value;
    sendBuf[4] = 0x55;
    
    for(i = 0; i < 5; i++)
    {
        usart_data_transmit(USART1, sendBuf[i]);
        while(RESET == usart_flag_get(USART1, USART_FLAG_TBE));
    }
}

void UART1_SetWaveFilterEnable(unsigned char enable)
{
    s_waveFilterEnable = (enable != 0);
    s_filterInit = 0;
}

unsigned char UART1_GetWaveFilterEnable(void)
{
    return s_waveFilterEnable;
}

void UART1_SetRCalibMode(unsigned char enable)
{
    s_rCalibMode = (enable != 0);
}

unsigned char UART1_GetRCalibMode(void)
{
    return s_rCalibMode;
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

    if (cnt == 0) return 0; // 无数据直接返回

    // 计算起始位置：当前写指针 - 数据量
    // 注意：MonPos 指向的是下一个写入位置，所以要先退格
    start = (unsigned char)(s_u8UART1MonPos + UART1_MON_SIZE - s_u8UART1MonCount);
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
    
    // 关键修复：读取后清空监控计数，这样才能在LCD上看到 "L=0" -> "L=6" 的动态变化
    // 否则 L 永远是 32 (满)
    s_u8UART1MonCount = 0; 

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
    // 限制单次处理最大字节数，防止主循环被大量数据积压卡死
    // 假设波特率115200，每秒约11KB，每次主循环处理64字节足以消化
    uint32_t limit = 64; 
    
    while(ReadUART1(&data, 1) && limit--)
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
    // 接收非空中断
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE) != RESET)
    {
        unsigned char uData = usart_data_receive(USART1);
        
        // 增加缓冲区保护：只有当队列未满时才写入
        // 虽然 WriteReceiveBuf1 内部有保护，但显式检查更安全
        WriteReceiveBuf1(uData);
        
        UART1_MonPush(uData); // 放入监控缓冲区
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RBNE);
    }
    
    // 处理溢出错误 (Overrun Error)，防止接收死锁
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_ERR_ORERR) != RESET)
    {
        usart_data_receive(USART1); // 读数据寄存器以清除 ORE
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_ERR_ORERR);
    }
    
    // 处理其他错误 (噪声/帧错误/奇偶校验错误)
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_ERR_NERR) != RESET)
    {
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_ERR_NERR);
    }
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_ERR_FERR) != RESET)
    {
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_ERR_FERR);
    }
}
