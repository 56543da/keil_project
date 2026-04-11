#ifndef __POWER_H
#define __POWER_H

#include "gd32f30x.h"

// 宏定义：引脚映射
#define PWR_EN_PORT    GPIOC
#define PWR_EN_PIN     GPIO_PIN_9

#define SW_KEY_PORT    GPIOC
#define SW_KEY_PIN     GPIO_PIN_8
#define SW_KEY_EXTI    EXTI_8
#define SW_KEY_IRQn    EXTI5_9_IRQn

#define CHRG_PORT      GPIOA
#define CHRG_PIN       GPIO_PIN_0

#define STDBY_PORT     GPIOA
#define STDBY_PIN      GPIO_PIN_1

#define BAT_DET_PORT   GPIOC
#define BAT_DET_PIN    GPIO_PIN_1
#define BAT_DET_CH     ADC_CHANNEL_11

#define USB_DET_PORT   GPIOC
#define USB_DET_PIN    GPIO_PIN_2
#define USB_DET_CH     ADC_CHANNEL_12

// 函数声明
void Power_Init(void);
void Power_Process(void); // 放在主循环中或定时器任务中，用于处理 ADC 和电源逻辑
void Power_CheckKey(void); // 用于在 SysTick 等定时器中检查按键防抖
void Power_BootstrapHold(void);

// 获取状态
uint8_t Power_GetBatteryPercent(void);
uint8_t Power_IsUSBInserted(void);
uint8_t Power_IsCharging(void);
uint8_t Power_IsChargeComplete(void);

#endif /* __POWER_H */
