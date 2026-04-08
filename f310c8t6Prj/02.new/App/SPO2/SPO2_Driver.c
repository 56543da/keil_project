#include "SPO2_Driver.h"
#include "gd32f3x0_adc.h"
#include "gd32f3x0_rcu.h"
#include "gd32f3x0_gpio.h"
#include "gd32f3x0_pmu.h"
#include "gd32f3x0_timer.h"
#include <stdio.h>

/* 
 * 移植自参考代码的血氧算法 - Timer中断版本
 */

 
static volatile uint16_t g_raw_red_adc = 0;
static volatile uint16_t g_raw_ir_adc = 0;
static volatile uint16_t g_raw_ambient_adc = 0; // 新增环境光变量
static volatile uint8_t g_gain_code = 0;
static volatile uint16_t g_wave_red_seq = 0;
static volatile uint16_t g_wave_ir_seq = 0;
static volatile uint8_t g_agc_enable = 0;///自动调光

// PWM Period (ARR)
// Target Frequency: 500kHz
// Timer Clock: 72MHz (APB1 * 2 or similar, check RCU config)
// Assuming Timer Clock is 72MHz (no prescaler or div 1)
// Period = 72,000,000 / 500,000 = 144
#define SPO2_PWM_PERIOD 144

// PWM Pulse Values (Duty Cycle)
// Note: 3.3V supply, max Vref=300mV => max duty ~9% (144*0.09 = 13)
// With Complementary Output (Push-Pull), drive efficiency is higher.
// Reduce Pulse width to prevent saturation.
// RANGE: 0 ~ 13 (DO NOT EXCEED 13)
// Red: Set to 6 (Start low)
// IR:  Set to 2
#define SPO2_RED_PWM_PULSE  4
#define SPO2_IR_PWM_PULSE   5
#define SPO2_PWM_PULSE_MIN  1
#define SPO2_PWM_PULSE_MAX  13
#define SPO2_DC_TARGET_LOW  1500
#define SPO2_DC_TARGET_HIGH 3000
static volatile uint8_t g_red_pwm_pulse = SPO2_RED_PWM_PULSE;
static volatile uint8_t g_ir_pwm_pulse = SPO2_IR_PWM_PULSE;

void SPO2_SetAGCEnable(uint8_t enable)
{
    g_agc_enable = (enable != 0) ? 1U : 0U;
    if(!g_agc_enable)
    {
        g_red_pwm_pulse = SPO2_RED_PWM_PULSE;
        g_ir_pwm_pulse = SPO2_IR_PWM_PULSE;
    }
}

uint8_t SPO2_GetAGCEnable(void)
{
    return g_agc_enable;
}

void SPO2_GetWaveSeq(uint16_t *red_seq, uint16_t *ir_seq)
{
    if(red_seq) *red_seq = g_wave_red_seq;
    if(ir_seq) *ir_seq = g_wave_ir_seq;
}

void SPO2_GetPwmPulse(uint8_t *red_pulse, uint8_t *ir_pulse)
{
    if(red_pulse) *red_pulse = g_red_pwm_pulse;
    if(ir_pulse) *ir_pulse = g_ir_pwm_pulse;
}

// 函数入参gain_code：增益控制码，3位有效（0~7），对应CD4051的8个通道
void SPO2_SetGain(uint8_t gain_code)
{
    // 定义两个变量，分别存储需要置低、置高的GPIO引脚值
    uint32_t pins_reset = 0;
    uint32_t pins_set = 0;

    // 【全引脚置低准备】把GAIN_A/B/C三个引脚都赋值给pins_reset，后续统一置低
    pins_reset = (uint32_t)(SPO2_GAIN_A_PIN | SPO2_GAIN_B_PIN | SPO2_GAIN_C_PIN);

    // 【按增益码逐位解析】把gain_code的低3位，分别映射到3个GPIO引脚
    // 第0位（0x01）对应GAIN_A_PIN，为1则该引脚置高
    if(gain_code & 0x01U) pins_set |= (uint32_t)SPO2_GAIN_A_PIN;
    // 第1位（0x02）对应GAIN_B_PIN，为1则该引脚置高
    if(gain_code & 0x02U) pins_set |= (uint32_t)SPO2_GAIN_B_PIN;
    // 第2位（0x04）对应GAIN_C_PIN，为1则该引脚置高
    if(gain_code & 0x04U) pins_set |= (uint32_t)SPO2_GAIN_C_PIN;

    // 【GPIO操作1】先把GAIN_A/B/C三个引脚全部置低（避免电平跳变的中间态）
    gpio_bit_reset(SPO2_GAIN_PORT, (uint32_t)pins_reset);
    // 【GPIO操作2】再把需要置高的引脚置高，设置目标电平
    gpio_bit_set(SPO2_GAIN_PORT, (uint32_t)pins_set);
    // 【保存当前增益码】只保留低3位（0~7），存入全局变量，方便后续读取当前增益
    g_gain_code = gain_code & 0x07U;

}

uint8_t SPO2_GetGain(void)
{
    return g_gain_code;
}

static void SPO2_PWM_Init(void)
{
    timer_oc_parameter_struct timer_ocintpara;
    timer_parameter_struct timer_initpara;

    /* Enable Clocks */
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_TIMER14);

    /* Configure PB14 (IR PWM, CH0) and PB15 (Red PWM, CH1) as AF1 (TIMER14) */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_14 | GPIO_PIN_15);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_14 | GPIO_PIN_15);
    gpio_af_set(GPIOB, GPIO_AF_1, GPIO_PIN_14 | GPIO_PIN_15);

    /* Timer Configuration */
    timer_deinit(TIMER14);
    timer_struct_para_init(&timer_initpara);

    /* TIMER14 configuration */
    /* Prescaler = 0, Clock = 72MHz */
    timer_initpara.prescaler         = 0;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = SPO2_PWM_PERIOD;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER14, &timer_initpara);

    /* CH0 and CH1 configuration */
    timer_channel_output_struct_para_init(&timer_ocintpara);
    timer_ocintpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocintpara.outputnstate = TIMER_CCXN_ENABLE;
    timer_ocintpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    timer_ocintpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_ocintpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;

    timer_channel_output_config(TIMER14, TIMER_CH_0, &timer_ocintpara);

    /* CH0 (Common Driver) configuration: PWM1 mode, Pulse = 0 (OFF initially) */
    timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_0, 0);
    timer_channel_output_mode_config(TIMER14, TIMER_CH_0, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER14, TIMER_CH_0, TIMER_OC_SHADOW_DISABLE);



    /* Enable TIMER14 Main Output (Required for timers with Break/Dead-time features) */
    timer_primary_output_config(TIMER14, ENABLE);



    /* Enable TIMER14 */
    timer_enable(TIMER14);
}

void SPO2_Driver_Init(void)
{
    /* Enable Clocks */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_ADC);
    rcu_periph_clock_enable(RCU_PMU);

    /* PC13, PC14, PC15 are in Backup Domain. 
       Need to enable backup write to use them as GPIO. */
    pmu_backup_write_enable();

    /* Configure PC13 (RED CS) and PC14 (IR CS) as Output Push-Pull */
    gpio_mode_set(SPO2_RED_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPO2_RED_CS_PIN);
    gpio_output_options_set(SPO2_RED_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPO2_RED_CS_PIN);
    
    gpio_mode_set(SPO2_IR_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPO2_IR_CS_PIN);
    gpio_output_options_set(SPO2_IR_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPO2_IR_CS_PIN);

    /* Turn off LEDs initially (CS High/Low depending on circuit, assuming High = Active for now based on prev code) */
    /* Previous code used gpio_bit_reset to turn OFF, gpio_bit_set to turn ON. */
    gpio_bit_reset(SPO2_RED_CS_PORT, SPO2_RED_CS_PIN);
    gpio_bit_reset(SPO2_IR_CS_PORT, SPO2_IR_CS_PIN);

    gpio_mode_set(SPO2_GAIN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPO2_GAIN_A_PIN | SPO2_GAIN_B_PIN | SPO2_GAIN_C_PIN);
    gpio_output_options_set(SPO2_GAIN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPO2_GAIN_A_PIN | SPO2_GAIN_B_PIN | SPO2_GAIN_C_PIN);
    SPO2_SetGain(SPO2_GAIN_LEVEL_5);

    /* Initialize PWM for LED Intensity Control */
    SPO2_PWM_Init();

    /* Configure PA1 (Sign) as Analog Input */
    gpio_mode_set(SPO2_SIGN_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, SPO2_SIGN_PIN);

    /* ADC Configuration */
    adc_deinit();
    /* ADCCLK = PCLK2 / 6 = 72MHz / 6 = 12MHz */
    rcu_adc_clock_config(RCU_ADCCK_APB2_DIV6); 
    
    adc_special_function_config(ADC_CONTINUOUS_MODE, DISABLE); 
    adc_special_function_config(ADC_SCAN_MODE, DISABLE);
    adc_data_alignment_config(ADC_DATAALIGN_RIGHT);
    adc_resolution_config(ADC_RESOLUTION_12B);

    adc_channel_length_config(ADC_REGULAR_CHANNEL, 1);
    adc_external_trigger_source_config(ADC_REGULAR_CHANNEL, ADC_EXTTRIG_REGULAR_NONE);
    adc_external_trigger_config(ADC_REGULAR_CHANNEL, ENABLE);
    
    adc_enable();
    adc_calibration_enable();
}

/**
 * @brief  快速读取ADC采样值（静态函数，仅当前文件可见）
 * @note   该函数用于快速获取单次ADC常规通道的采样结果，带有超时保护，
 *         采用较长采样时间保证数据稳定性，适用于血氧仪（SPO2）等需要快速且稳定采样的场景
 * @retval uint16_t  返回ADC采样结果（16位数据），超时返回0
 */
static uint16_t ADC_Read_Fast(void)
{
    uint32_t timeout = 0x0400;
    adc_regular_channel_config(0, SPO2_ADC_CHANNEL, ADC_SAMPLETIME_239POINT5);
    adc_flag_clear(ADC_FLAG_EOC | ADC_FLAG_STRC);
    ADC_CTL1 &= ~((uint32_t)ADC_CTL1_SWRCST);
    adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
    
    while(!adc_flag_get(ADC_FLAG_EOC))
    {
        // 超时计数器递减，若减至0仍未完成转换，返回0表示采样失败
        if(timeout-- == 0) return 0xFFFF;
    }
    
    // 清除ADC转换完成标志位，避免影响下一次采样判断
    adc_flag_clear(ADC_FLAG_EOC);
    
    // 读取ADC常规通道转换结果并返回（16位无符号整数）
    return adc_regular_data_read();
}

// 供Timer中断调用，每0.5ms调用一次
void SPO2_Timer_Handler(void)
{
    static uint8_t time_slot = 0;
    uint16_t adc_val;
    
    time_slot++;
    if(time_slot >= 20) // 10ms周期
    {
        time_slot = 0;
    }
    
    switch(time_slot)
    {
        case 1: // 0.5ms时刻，采样环境光 (此时两灯皆灭)
            g_raw_ambient_adc = ADC_Read_Fast();
            break;

        case 2: // 1ms时刻，开始RED LED发光
            timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_0, g_red_pwm_pulse);
            gpio_bit_set(SPO2_RED_CS_PORT, SPO2_RED_CS_PIN);
            break;
            
        case 5: // 2.5ms时刻，RED LED ADC采样 (给1ms稳定时间)
            adc_val = ADC_Read_Fast();
            // 减去环境光，防止负值溢出
            if(adc_val >= g_raw_ambient_adc) g_raw_red_adc = adc_val - g_raw_ambient_adc;
            else g_raw_red_adc = 0;
            
            g_wave_red_seq++;
            break;
            
        case 6: // 3ms时刻，关闭RED LED
            timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_0, 0);
            gpio_bit_reset(SPO2_RED_CS_PORT, SPO2_RED_CS_PIN);
            break;
            
        case 10: // 5ms时刻，开始IR LED发光
            timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_0, g_ir_pwm_pulse);
            gpio_bit_set(SPO2_IR_CS_PORT, SPO2_IR_CS_PIN);
            break;
            
        case 13: // 6.5ms时刻，IR LED ADC采样 (给1ms稳定时间)
            adc_val = ADC_Read_Fast();
            // 减去环境光
            if(adc_val >= g_raw_ambient_adc) g_raw_ir_adc = adc_val - g_raw_ambient_adc;
            else g_raw_ir_adc = 0;
            
            g_wave_ir_seq++;
            if(g_agc_enable)
            {
                static uint8_t adjust_cnt = 0;
                adjust_cnt++;
                if(adjust_cnt >= 20)
                {//自动增益调整
                    // adjust_cnt = 0;
                    // if(g_raw_red_adc < SPO2_DC_TARGET_LOW && g_red_pwm_pulse < SPO2_PWM_PULSE_MAX) g_red_pwm_pulse++;
                    // else if(g_raw_red_adc > SPO2_DC_TARGET_HIGH && g_red_pwm_pulse > SPO2_PWM_PULSE_MIN) g_red_pwm_pulse--;

                    // if(g_raw_ir_adc < SPO2_DC_TARGET_LOW && g_ir_pwm_pulse < SPO2_PWM_PULSE_MAX) g_ir_pwm_pulse++;
                    // else if(g_raw_ir_adc > SPO2_DC_TARGET_HIGH && g_ir_pwm_pulse > SPO2_PWM_PULSE_MIN) g_ir_pwm_pulse--;

                    // if((g_raw_red_adc < SPO2_DC_TARGET_LOW && g_red_pwm_pulse == SPO2_PWM_PULSE_MAX) ||
                    //    (g_raw_ir_adc < SPO2_DC_TARGET_LOW && g_ir_pwm_pulse == SPO2_PWM_PULSE_MAX))
                    // {
                    //     if(g_gain_code < 7) SPO2_SetGain(g_gain_code + 1);
                    // }
                    // else if((g_raw_red_adc > SPO2_DC_TARGET_HIGH && g_red_pwm_pulse == SPO2_PWM_PULSE_MIN) ||
                    //         (g_raw_ir_adc > SPO2_DC_TARGET_HIGH && g_ir_pwm_pulse == SPO2_PWM_PULSE_MIN))
                    // {
                    //     if(g_gain_code > 0) SPO2_SetGain(g_gain_code - 1);
                    // }
                }
            }
            break;
            
        case 14: // 7ms时刻，关闭IR LED
            timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_0, 0);
            gpio_bit_reset(SPO2_IR_CS_PORT, SPO2_IR_CS_PIN);
            break;
            
        default:
            break;
    }
}

void SPO2_GetRawADC(uint16_t *red, uint16_t *ir)
{
    *red = g_raw_red_adc;
    *ir = g_raw_ir_adc;
}
