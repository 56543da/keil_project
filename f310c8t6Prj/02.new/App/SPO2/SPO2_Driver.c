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

// FIR滤波器系数 (Order 4) - 低阶低通滤波器
static float fir_coeffs[FIR_ORDER + 1] = {
    0.10f, 0.20f, 0.40f, 0.20f, 0.10f
};

// 滤波相关变量
static float red_buffer[FIR_ORDER + 1] = {0};
static float ir_buffer[FIR_ORDER + 1] = {0};

// DC Removal (High Pass) variables
static float red_dc_val = 0.0f;
static float ir_dc_val = 0.0f;
#define DC_ALPHA 0.95f // High pass filter coefficient

// 波形缓存
static float red_wave_buf[WAVE_BUF_SIZE] = {0};
static float ir_wave_buf[WAVE_BUF_SIZE] = {0};
static uint16_t wave_index = 0;
static uint16_t wave_count = 0;

// 结果存储
static volatile SPO2_Result_t g_spo2_result = {0, 0, 0, 0};
static volatile uint16_t g_raw_red_adc = 0;
static volatile uint16_t g_raw_ir_adc = 0;
static volatile uint16_t g_raw_ambient_adc = 0; // 新增环境光变量
static volatile uint8_t g_gain_code = 0;
static volatile uint8_t g_filter_enable = 1;
static volatile uint16_t g_wave_red_seq = 0;
static volatile uint16_t g_wave_ir_seq = 0;
static volatile uint8_t g_agc_enable = 1;

// PWM Period (ARR)
// Target Frequency: 500kHz
// Timer Clock: 72MHz (APB1 * 2 or similar, check RCU config)
// Assuming Timer Clock is 72MHz (no prescaler or div 1)
// Period = 72,000,000 / 500,000 = 144
#define SPO2_PWM_PERIOD 144

// PWM Pulse Values (Duty Cycle)
// Note: 3.3V supply, max Vref=300mV => max duty ~9% (144*0.09 = 13)
// Red: Try ~4.5% duty => 144 * 0.045 = 6
// IR:  Try ~1% duty (to fix saturation) => 144 * 0.01 = 1-2
#define SPO2_RED_PWM_PULSE  1//6
#define SPO2_IR_PWM_PULSE   6//2

void SPO2_SetFilterEnable(uint8_t enable)
{
    g_filter_enable = (enable != 0) ? 1U : 0U;
}

void SPO2_SetAGCEnable(uint8_t enable)
{
    g_agc_enable = (enable != 0) ? 1U : 0U;
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

    // 当增益改变时，重置滤波器缓存，避免旧增益数据干扰新采样结果
    {
        int i;
        for(i = 0; i <= FIR_ORDER; i++) {
            red_buffer[i] = 0.0f;
            ir_buffer[i] = 0.0f;
        }
    }
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
    timer_ocintpara.outputnstate = TIMER_CCXN_DISABLE;
    timer_ocintpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    timer_ocintpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_ocintpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;

    timer_channel_output_config(TIMER14, TIMER_CH_0, &timer_ocintpara);
    timer_channel_output_config(TIMER14, TIMER_CH_1, &timer_ocintpara);

    /* CH0 (IR) configuration: PWM1 mode, Pulse = 0 (OFF initially) */
    timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_0, 0);
    timer_channel_output_mode_config(TIMER14, TIMER_CH_0, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER14, TIMER_CH_0, TIMER_OC_SHADOW_DISABLE);

    /* CH1 (Red) configuration: PWM1 mode, Pulse = 0 (OFF initially) */
    timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_1, 0);
    timer_channel_output_mode_config(TIMER14, TIMER_CH_1, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER14, TIMER_CH_1, TIMER_OC_SHADOW_DISABLE);



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
    SPO2_SetGain(0);

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
    
    // 复位滤波器 buffer
    {
        int i;
        for(i=0; i<=FIR_ORDER; i++) {
            red_buffer[i] = 0.0f;
            ir_buffer[i] = 0.0f;
        }
    }
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

// FIR滤波器实现
static float fir_filter(float input, float *buffer, float *coeffs, int order)
{
    float output = 0.0f;
    int i;
    
    // 移位
    for(i = order; i > 0; i--) {
        buffer[i] = buffer[i-1];
    }
    
    // 存入新数据
    buffer[0] = input;
    
    // 卷积
    for(i = 0; i <= order; i++) {
        output += buffer[i] * coeffs[i];
    }
    
    return output;
}

// 供Timer中断调用，每0.5ms调用一次
void SPO2_Timer_Handler(void)
{
    static uint8_t time_slot = 0;
    uint16_t adc_val;
    float filtered_val;
    
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
            timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_1, SPO2_RED_PWM_PULSE);
            gpio_bit_set(SPO2_RED_CS_PORT, SPO2_RED_CS_PIN);
            break;
            
        case 5: // 2.5ms时刻，RED LED ADC采样 (给1ms稳定时间)
            adc_val = ADC_Read_Fast();
            // 减去环境光，防止负值溢出
            if(adc_val >= g_raw_ambient_adc) g_raw_red_adc = adc_val - g_raw_ambient_adc;
            else g_raw_red_adc = 0;
            
            g_wave_red_seq++;
            
            // 简单的低通滤波 (FIR)
            if(g_filter_enable) {
                filtered_val = fir_filter((float)g_raw_red_adc, red_buffer, fir_coeffs, FIR_ORDER);
            } else {
                filtered_val = (float)g_raw_red_adc;
            }
            
            // 存入 buffer (原始数据，仅用于调试)
            if (wave_index < WAVE_BUF_SIZE) {
                red_wave_buf[wave_index] = filtered_val;
            }
            break;
            
        case 6: // 3ms时刻，关闭RED LED
            timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_1, 0);
            gpio_bit_reset(SPO2_RED_CS_PORT, SPO2_RED_CS_PIN);
            break;
            
        case 10: // 5ms时刻，开始IR LED发光
            timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_0, SPO2_IR_PWM_PULSE);
            gpio_bit_set(SPO2_IR_CS_PORT, SPO2_IR_CS_PIN);
            break;
            
        case 13: // 6.5ms时刻，IR LED ADC采样 (给1ms稳定时间)
            adc_val = ADC_Read_Fast();
            // 减去环境光
            if(adc_val >= g_raw_ambient_adc) g_raw_ir_adc = adc_val - g_raw_ambient_adc;
            else g_raw_ir_adc = 0;
            
            g_wave_ir_seq++;
            
            // 简单的低通滤波 (FIR)
            if(g_filter_enable) {
                filtered_val = fir_filter((float)g_raw_ir_adc, ir_buffer, fir_coeffs, FIR_ORDER);
            } else {
                filtered_val = (float)g_raw_ir_adc;
            }
            
            // 存入 buffer (原始数据，仅用于调试)
            if (wave_index < WAVE_BUF_SIZE) {
                ir_wave_buf[wave_index] = filtered_val;
                wave_index++;
                if (wave_count < WAVE_BUF_SIZE) wave_count++;
            }
            break;
            
        case 14: // 7ms时刻，关闭IR LED
            timer_channel_output_pulse_value_config(TIMER14, TIMER_CH_0, 0);
            gpio_bit_reset(SPO2_IR_CS_PORT, SPO2_IR_CS_PIN);
            break;
            
        case 15: // 7.5ms时刻，计算逻辑
            // 每1秒(100点)计算一次
            if (wave_index >= WAVE_BUF_SIZE) {
                wave_index = 0; // 重置索引
                
                if(wave_count > 0) {
                    float red_max = red_wave_buf[0], red_min = red_wave_buf[0];
                    float ir_max = ir_wave_buf[0], ir_min = ir_wave_buf[0];
                    float ir_dc, ir_ac, red_dc, red_ac, R, spo2_calc, threshold_ir;
                    int pulse_count, last_peak_index, heart_rate;
                    uint8_t next_gain;
                    uint16_t i;

                    // 寻找极值
                    for(i = 1; i < wave_count; i++) {
                        if(red_wave_buf[i] > red_max) red_max = red_wave_buf[i];
                        if(red_wave_buf[i] < red_min) red_min = red_wave_buf[i];
                        if(ir_wave_buf[i] > ir_max) ir_max = ir_wave_buf[i];
                        if(ir_wave_buf[i] < ir_min) ir_min = ir_wave_buf[i];
                    }

                    // 简单的脱落判断
                    ir_dc = (ir_max + ir_min) / 2.0f;
                    // 注意：因为wave_buf现在存储的是放大且去直流后的值，所以不能直接用max/min计算DC和AC
                    // 我们需要使用之前计算的真实 DC 值
                    ir_dc = ir_dc_val;
                    red_dc = red_dc_val;
                    
                    // AC分量大约是 max - min (因为波形已经去直流并放大了，这里要反算或者直接用波形幅度估算)
                    // 由于 wave_buf = (val - dc) * 4 + 2048
                    // 所以 AC_real = (max - min) / 4
                    ir_ac = (ir_max - ir_min) / 4.0f;
                    red_ac = (red_max - red_min) / 4.0f;
                    
                    if (ir_dc < 100.0f || ir_ac < 5.0f) { // 阈值相应调整
                        g_spo2_result.spo2 = 0;
                        g_spo2_result.heart_rate = 0;
                        g_spo2_result.pi = 0;
                        g_spo2_result.updated = 1;
                    } 
                    else {
                        if(g_agc_enable) {
                            next_gain = g_gain_code;
                            
                            /* 
                             * 增益表参考 (A2A1A0):
                             * 000: 498x  | 001: 747x  | 010: 996x  | 011: 1493x
                             * 100: 1942x | 101: 2739x | 110: 2739x | 111: 3984x
                             */
                            
                            // 减小增益的情况：信号过强或接近饱和 (使用 ir_dc 判断饱和更准确)
                            if(ir_dc > 3800.0f) {
                                if(next_gain > 0U) next_gain--;
                            } 
                            // 增大增益的情况：信号太弱 (AC分量过小)
                            else if(ir_ac < 40.0f && ir_dc < 3000.0f) {
                                if(next_gain < 7U) {
                                    if(next_gain == 0x05) next_gain = 0x07; // 101->111 (Skip 110 duplicate)
                                    else next_gain++;
                                }
                            }
                            
                            if(next_gain != g_gain_code) {
                                SPO2_SetGain(next_gain);
                            }
                        }

                        // red_dc 和 red_ac 已经在前面计算了
                        
                        // 修正 R 值计算: R = (AC_Red/DC_Red) / (AC_IR/DC_IR)
                        R = (red_ac / red_dc) / (ir_ac / ir_dc);
                        
                        // 使用标准线性近似公式: SpO2 = 110 - 25 * R
                        spo2_calc = 110.0f - 25.0f * R;
                        
                        if(spo2_calc > 100.0f) spo2_calc = 100.0f;
                        if(spo2_calc < 70.0f) spo2_calc = 70.0f;
                        
                        // 心率计算 (使用IR信号，增加简单的距离判断防抖)
                        pulse_count = 0;
                        threshold_ir = ir_dc;
                        last_peak_index = -100;
                        
                        for(i = 1; i < wave_count - 1; i++) {
                            if(ir_wave_buf[i] > threshold_ir && 
                               ir_wave_buf[i] > ir_wave_buf[i-1] && 
                               ir_wave_buf[i] > ir_wave_buf[i+1]) {
                                
                                // 最小峰值间隔检查 (25点 = 250ms, 对应最高心率约240bpm)
                                if ((i - last_peak_index) > 25) {
                                    pulse_count++;
                                    last_peak_index = i;
                                }
                            }
                        }
                        
                        // 1秒窗口: BPM = count * 60
                        heart_rate = pulse_count * 60;
                        if(heart_rate > 250) heart_rate = 250;

                        g_spo2_result.heart_rate = (uint8_t)heart_rate;
                        g_spo2_result.spo2 = (uint8_t)spo2_calc;
                        
                        if(ir_dc != 0) {
                            g_spo2_result.pi = (uint8_t)((ir_ac / ir_dc) * 100.0f); 
                            if(g_spo2_result.pi == 0) g_spo2_result.pi = 1;
                        } else {
                            g_spo2_result.pi = 0;
                        }
                        
                        g_spo2_result.updated = 1;
                    }
                }
                wave_count = 0;
            }
            break;
            
        default:
            break;
    }
}

uint8_t SPO2_GetResult(uint8_t *spo2, uint8_t *hr, uint8_t *pi)
{
    if(g_spo2_result.updated)
    {
        *spo2 = g_spo2_result.spo2;
        *hr = g_spo2_result.heart_rate;
        *pi = g_spo2_result.pi;
        g_spo2_result.updated = 0;
        return 1;
    }
    return 0;
}

void SPO2_GetRawADC(uint16_t *red, uint16_t *ir)
{
    *red = g_raw_red_adc;
    *ir = g_raw_ir_adc;
}
