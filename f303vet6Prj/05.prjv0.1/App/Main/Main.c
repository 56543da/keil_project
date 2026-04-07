/*********************************************************************************************************
* 模块名称：Main.c
* 摘    要：主文件，包含软硬件初始化函数和main函数
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2021年07月01日
* 内    容：
* 注    意：注意勾选Options for Target 'Target1'->Code Generation->Use MicroLIB使printf功能生效                                                                  
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
#include "gd32f30x_conf.h"
#include "SysTick.h"
#include "RCU.h"
#include "NVIC.h"
#include "Timer.h"
#include "UART0.h"

#include "LED.h"
#include "KeyOne.h"
#include "ProcKeyOne.h"
#include "DataType.h"
#include "LCD.h"
#include "UART1.h"
#include "UI_Manager.h"
#include <string.h>
#include "SPO2_Algo.h" // 引用算法头文件

/*********************************************************************************************************
*                                              宏定义
**********************************************************************************************************/

/*********************************************************************************************************
*                                              枚举结构体
**********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量声明
**********************************************************************************************************/

/*********************************************************************************************************
*                                              内部函数声明
**********************************************************************************************************/
static  void  InitSoftware(void);   /* 初始化软件模块，在初始化硬件之后调用此函数 */
static  void  InitHardware(void);   /* 初始化硬件模块，在系统启动时调用此函数 */
static  void  Proc1SecTask(void);   /* 1s周期处理任务 */

/* 调试信息: 用于监测数据 */
static uint32_t run_cnt = 0;
static u8 debug_buf[30];

/*********************************************************************************************************
*                                              内部函数实现
**********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：InitSoftware
* 函数功能：初始化软件模块，在初始化硬件之后调用此函数
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2021年07月01日
* 注    意：
**********************************************************************************************************/
static  void  InitSoftware(void)
{

}

/*********************************************************************************************************
* 函数名称：InitHardware
* 函数功能：初始化硬件模块，在系统启动时调用此函数
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2021年07月01日
* 注    意：
**********************************************************************************************************/
static  void  InitHardware(void)
{  
  SystemInit();        /* 系统初始化 */
  InitRCU();           /* 初始化RCU模块 */
  InitNVIC();          /* 初始化NVIC模块 */  
  InitUART0(921600);   /* 初始化UART模块 - 提速至 921600 */
  InitTimer();         /* 初始化Timer模块 */
  InitLED();           /* 初始化LED模块 */
  InitSysTick();       /* 初始化SysTick模块 */
  InitUART1(921600);   /* 初始化UART1模块 (SPO2接收) - 提速至 921600 */
  InitKeyOne();        /* 初始化KeyOne模块 */
  InitProcKeyOne();    /* 初始化ProcKeyOne模块 */
  LCD_Init();          /* 初始化LCD模块 */
  // SPO2_DisplayInit();  /* 初始化血氧显示 */
  UI_Init();           /* 初始化UI管理器 */
  SPO2_Algo_Init();    /* 初始化血氧算法 */
}



/*********************************************************************************************************
* 函数名称：Proc1SecTask
* 函数功能：1s周期处理任务 
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 完成日期：2021年07月01日
* 注    意：
**********************************************************************************************************/
static  void  Proc1SecTask(void)
{ 
    /* 正常工作指示灯：红色LED每秒闪烁一次 */
    LED3_Toggle();

  // 暂时屏蔽原有显示逻辑，交由UI_Manager管理
  /*
  static u8 lcd_id[12];
  static char uart1_hex_cur[64];
  static char uart1_hex_prev[64];
  u8 key_buf[50];  
  u8 uart_buf[128];
  u8 len;
  SPO2Data_t spo2Data;
  char hex_buf[64]; 
  
    sprintf((char*)lcd_id, "LCD ID:%04X", lcddev.id);

    POINT_COLOR = RED;  
    LCD_ShowString(30, 40, 200, 24, 24, (u8*)"GD32F303VE");
    LCD_ShowString(30, 70, 200, 16, 16, lcd_id);
    LCD_ShowString(30, 90, 200, 16, 16, (u8*)"2026-01-08");

    sprintf((char*)key_buf, "LL:%d RL:%d LH:%d RH:%d M:%d", g_u32KeyLLCount, g_u32KeyRLCount, g_u32KeyLeftHighCount, g_u32KeyRightHighCount, g_u32KeyMenuCount);
    LCD_ShowString(30, 110, 240, 16, 16, key_buf); 

    LCD_ShowString(30, 140, 200, 16, 16, (u8*)"UART1 Monitor:"); 

    len = UART1_GetLastRx(uart_buf, 32);

    sprintf((char*)debug_buf, "Run:%d L:%d ", run_cnt++, len);
    LCD_ShowString(140, 140, 100, 16, 16, debug_buf);
    printf("RX Run=%lu L=%d\r\n", (unsigned long)run_cnt, len);

    if (len > 0)
    {
        int debug_len = len > 16 ? 16 : len;
        int pos = 0;
        int k;
        for(k = 0; k < debug_len; k++)
        {
            pos += sprintf(hex_buf + pos, "%02X ", uart_buf[k]);
        }
        memcpy(uart1_hex_prev, uart1_hex_cur, sizeof(uart1_hex_prev));
        memset(uart1_hex_cur, 0, sizeof(uart1_hex_cur));
        memcpy(uart1_hex_cur, hex_buf, (size_t)(pos + 1));
    }

    if (UART1_GetLastParsed(&spo2Data))
    {
        printf("SPO2=%d HR=%d PI=%d\r\n", spo2Data.spo2, spo2Data.heart_rate, spo2Data.pi);
    }

    LCD_Fill(30, 280, 240, 320, BLACK);
    LCD_ShowString(30, 282, 240, 16, 16, (u8*)uart1_hex_prev);
    LCD_ShowString(30, 302, 240, 16, 16, (u8*)uart1_hex_cur);
    */
    
    // UI_Update(); // 更新UI（如果需要定时刷新）
}
//
//dda2d12a
//
/*********************************************************************************************************
* 函数名称：main
* 函数功能：主函数 
* 输入参数：void
* 输出参数：void
* 返 回 值：int
* 完成日期：2021年07月01日
* 注    意：
**********************************************************************************************************/
int main(void)
{
  InitHardware();   /* 初始化硬件外设模块 */
  InitSoftware();   /* 初始化软件模块 */

  printf("Init System has been finished.\r\n" );  /* 打印系统状态 */
  
  // 设置波形工具的参数显示名称：参数名配置命令
  printf("{{1,SPO2}}\r\n");
  printf("{{2,HR}}\r\n");
  printf("{{3,PI_IR}}\r\n");
  printf("{{4,R_Val}}\r\n");
  printf("{{5,Gain}}\r\n");
  
  printf("LCD Display Test Start.\r\n");
  
  while(1)
  {
    if(Get2msFlag())
    {
      KeyOne_2msTask();
      Clr2msFlag();
    }

    // 处理串口1接收数据 (波形转发与解析)
    UART1_ProcessSPO2Data();

    // 执行血氧计算
    if(UART1_GetRCalibMode())
    {
        SPO2_Algo_ProcessCalib();
    }
    else
    {
        SPO2_Algo_Process();
    }

    // 检查是否有计算结果
    {
        uint8_t spo2, hr, pi_ir, pi_red;
        float r_val;
        if(SPO2_Algo_GetResult(&spo2, &hr, &pi_ir, &pi_red, &r_val))
        {
            SPO2Data_t data;
            data.spo2 = spo2;
            data.heart_rate = hr;
            data.pi = pi_ir;
            data.status = pi_red;
            // 将 R 值 (0.4~1.2) 放大1000倍显示 (400~1200)
            data.filter_status = (uint16_t)(r_val * 1000); 
            data.gain_level = 0xFF;
            data.pwm_red = 0;
            data.pwm_ir = 0;
            
            // 更新 UI
            UI_UpdateData(&data);

            if(UART1_GetRCalibMode())
            {
                printf("%d,0\r\n", data.filter_status);
            }
            else
            {
                // 打印到串口调试 (适配波形工具参数显示)
                // 格式: [[index,value]]
                printf("[[1,%d]]\r\n", spo2);
                printf("[[2,%d]]\r\n", hr);
                printf("[[3,%d]]\r\n", pi_ir);
                printf("[[4,%d]]\r\n", data.filter_status); // R值 * 1000
              //  if(data.gain_level != 0xFF) printf("[[5,%d]]\r\n", data.gain_level);
            }
        }
    }

    if(Get1SecFlag())              /* 检查1s标志状态 */
    {
      Proc1SecTask();             /* 1s周期处理任务 */
      Clr1SecFlag();             /* 清除1s标志 */
    }
    
    UI_Process(); // 处理UI逻辑

  }
}
