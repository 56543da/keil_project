#include "Power.h"
#include "gd32f30x_adc.h"
#include "gd32f30x_exti.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_misc.h"
#include <stdio.h>

static uint8_t s_powerState = 1; // 1: ON, 0: OFF
static uint8_t s_keyDebounceCnt = 0;
static uint8_t s_keyIsChecking = 0;
static uint16_t s_keyHoldTicks = 0;
static uint8_t s_ignoreUntilRelease = 0;

#define POWER_KEY_DEBOUNCE_TICKS 10
#define POWER_OFF_HOLD_TICKS     750  /* 2ms*750 = 1500ms 长按关机 */

static uint16_t s_batAdcVal = 0;
static uint16_t s_usbAdcVal = 0;

void Power_BootstrapHold(void)
{
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(SW_KEY_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, SW_KEY_PIN);
    gpio_init(PWR_EN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, PWR_EN_PIN);
    if (gpio_input_bit_get(SW_KEY_PORT, SW_KEY_PIN) == RESET) gpio_bit_set(PWR_EN_PORT, PWR_EN_PIN);
    else gpio_bit_reset(PWR_EN_PORT, PWR_EN_PIN);
}

void Power_Init(void)
{
    // 1. 初始化时钟
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_periph_clock_enable(RCU_AF);

    // 2. 初始化 PWR_EN (PC9)
    gpio_init(PWR_EN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, PWR_EN_PIN);
    if (gpio_input_bit_get(SW_KEY_PORT, SW_KEY_PIN) == RESET) gpio_bit_set(PWR_EN_PORT, PWR_EN_PIN);
    else gpio_bit_reset(PWR_EN_PORT, PWR_EN_PIN);

    // 3. 初始化 SW_KEY (PC8) - 上拉输入
    gpio_init(SW_KEY_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, SW_KEY_PIN);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOC, GPIO_PIN_SOURCE_8);
    exti_init(EXTI_8, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(EXTI_8);
    nvic_irq_enable(SW_KEY_IRQn, 2, 1);
    if (gpio_input_bit_get(SW_KEY_PORT, SW_KEY_PIN) == RESET) {
        s_ignoreUntilRelease = 1;
        exti_interrupt_disable(EXTI_8);
    } else {
        s_ignoreUntilRelease = 0;
        exti_interrupt_enable(EXTI_8);
    }

    // 4. 初始化充电状态引脚 CHRG (PA0), STDBY (PA1) - 上拉输入
    gpio_init(CHRG_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, CHRG_PIN);
    gpio_init(STDBY_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, STDBY_PIN);

    // 5. 初始化 ADC 引脚 BAT_DET (PC1), USB_DET (PC2)
    gpio_init(BAT_DET_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, BAT_DET_PIN);
    gpio_init(USB_DET_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, USB_DET_PIN);

    // 6. 配置 ADC0 (连续转换模式)
    adc_deinit(ADC0);
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV8);
    
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    
    // 规则组通道长度 2
    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 2);
    // 配置规则组通道 11 和 12
    adc_regular_channel_config(ADC0, 0, BAT_DET_CH, ADC_SAMPLETIME_239POINT5);
    adc_regular_channel_config(ADC0, 1, USB_DET_CH, ADC_SAMPLETIME_239POINT5);

    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_REGULAR_NONE);
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);

    adc_enable(ADC0);
    // adc_calibration_enable 仅需在开启后等待即可，GD32 的函数为 adc_calibration_enable
    adc_calibration_enable(ADC0);
    
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
}

void Power_Process(void)
{
    // ADC 读取逻辑 (轮询读取最后一次转换的数据，不使用 DMA 以简化)
    // 对于连续扫描模式，直接读可能只能读到最后一个通道，因此我们简单做两次软件触发或分时触发
    // 此处简化为分别切换通道读取，或者用中断/DMA。由于对实时性要求不高，我们改为单通道分时读取：
    
    static uint8_t s_adcCh = 0;
    
    // 重新配置只读一个通道，触发一次
    adc_special_function_config(ADC0, ADC_SCAN_MODE, DISABLE);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, DISABLE);
    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 1);
    
    if (s_adcCh == 0) {
        adc_regular_channel_config(ADC0, 0, BAT_DET_CH, ADC_SAMPLETIME_239POINT5);
        adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
        while(!adc_flag_get(ADC0, ADC_FLAG_EOC));
        adc_flag_clear(ADC0, ADC_FLAG_EOC);
        s_batAdcVal = (s_batAdcVal * 7 + adc_regular_data_read(ADC0)) / 8; // 简单低通滤波
        s_adcCh = 1;
    } else {
        adc_regular_channel_config(ADC0, 0, USB_DET_CH, ADC_SAMPLETIME_239POINT5);
        adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
        while(!adc_flag_get(ADC0, ADC_FLAG_EOC));
        adc_flag_clear(ADC0, ADC_FLAG_EOC);
        s_usbAdcVal = (s_usbAdcVal * 7 + adc_regular_data_read(ADC0)) / 8;
        s_adcCh = 0;
    }
}

// 需要在 1ms 或 2ms 定时器中调用
void Power_CheckKey(void)
{
    if (s_ignoreUntilRelease) {
        if (gpio_input_bit_get(SW_KEY_PORT, SW_KEY_PIN) != RESET) {
            s_ignoreUntilRelease = 0;
            exti_interrupt_flag_clear(EXTI_8);
            exti_interrupt_enable(EXTI_8);
        }
        return;
    }
    if (s_keyIsChecking) {
        if (gpio_input_bit_get(SW_KEY_PORT, SW_KEY_PIN) == RESET) {
            if (s_keyDebounceCnt < POWER_KEY_DEBOUNCE_TICKS) {
                s_keyDebounceCnt++;
                return;
            }
            if (s_keyHoldTicks < POWER_OFF_HOLD_TICKS) {
                s_keyHoldTicks++;
            } else {
                s_powerState = 0;
                (void)s_powerState;
                gpio_bit_reset(PWR_EN_PORT, PWR_EN_PIN);
                printf("Power OFF triggered.\r\n");
                while(1);
            }
        } else {
            s_keyIsChecking = 0;
            s_keyDebounceCnt = 0;
            s_keyHoldTicks = 0;
            exti_interrupt_flag_clear(EXTI_8);
            exti_interrupt_enable(EXTI_8);
        }
    }
}

// 在 gd32f30x_it.c 或 EXTI5_9 中断中调用
void EXTI8_IRQHandler_Impl(void)
{
    if (exti_flag_get(EXTI_8) != RESET) {
        exti_interrupt_flag_clear(EXTI_8);
        exti_interrupt_disable(EXTI_8); // 触发后禁用中断，开始防抖计时
        s_keyIsChecking = 1;
        s_keyDebounceCnt = 0;
        s_keyHoldTicks = 0;
    }
}

uint8_t Power_IsUSBInserted(void)
{
    // USB 5V 分压 2.5V 对应 ADC 约 2048 (如果参考电压 3.3V，则为 2.5/3.3 * 4095 = 3102)
    // 设定阈值 2000
    return (s_usbAdcVal > 2000) ? 1 : 0;
}

uint8_t Power_GetBatteryPercent(void)
{
    // 假设 4.2V 满电分压到 2.1V (2606)，3.0V 没电分压到 1.5V (1861)
    // 此处仅为估算映射
    if (s_batAdcVal < 1861) return 0;
    if (s_batAdcVal > 2606) return 100;
    return (uint8_t)((s_batAdcVal - 1861) * 100 / (2606 - 1861));
}

uint8_t Power_IsCharging(void)
{
    // CHRG 拉低表示正在充电
    return (gpio_input_bit_get(CHRG_PORT, CHRG_PIN) == RESET) ? 1 : 0;
}

uint8_t Power_IsChargeComplete(void)
{
    // STDBY 拉低表示充电完成
    return (gpio_input_bit_get(STDBY_PORT, STDBY_PIN) == RESET) ? 1 : 0;
}
