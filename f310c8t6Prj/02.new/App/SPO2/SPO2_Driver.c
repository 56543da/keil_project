#include "SPO2_Driver.h"
#include "gd32f3x0_adc.h"
#include "gd32f3x0_rcu.h"
#include "gd32f3x0_gpio.h"
#include <stdio.h>

/* 
 * 移植自参考代码的血氧算法 - Timer中断版本
 */

// FIR滤波器系数 (Order 16)
static float fir_coeffs[FIR_ORDER + 1] = {
    0.0065f, 0.0103f, 0.0210f, 0.0382f, 0.0599f, 0.0829f, 0.1032f, 0.1171f,
    0.1221f,
    0.1171f, 0.1032f, 0.0829f, 0.0599f, 0.0382f, 0.0210f, 0.0103f, 0.0065f
};

// 滤波相关变量
static float red_buffer[FIR_ORDER + 1] = {0};
static float ir_buffer[FIR_ORDER + 1] = {0};

// 波形缓存
static float red_wave_buf[WAVE_BUF_SIZE] = {0};
static float ir_wave_buf[WAVE_BUF_SIZE] = {0};
static uint16_t wave_index = 0;
static uint16_t wave_count = 0;

// 结果存储
static volatile SPO2_Result_t g_spo2_result = {0, 0, 0, 0};

void SPO2_Driver_Init(void)
{
    /* Enable Clocks */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_ADC);

    /* Configure PC13 (RED CS) and PC14 (IR CS) as Output Push-Pull */
    gpio_mode_set(SPO2_RED_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPO2_RED_CS_PIN);
    gpio_output_options_set(SPO2_RED_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPO2_RED_CS_PIN);
    
    gpio_mode_set(SPO2_IR_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPO2_IR_CS_PIN);
    gpio_output_options_set(SPO2_IR_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPO2_IR_CS_PIN);

    /* Turn off LEDs initially */
    gpio_bit_reset(SPO2_RED_CS_PORT, SPO2_RED_CS_PIN);
    gpio_bit_reset(SPO2_IR_CS_PORT, SPO2_IR_CS_PIN);

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
    
    adc_enable();
    adc_calibration_enable();
}

static uint16_t ADC_Read_Fast(void)
{
    uint32_t timeout = 0x1000;
    adc_regular_channel_config(0, SPO2_ADC_CHANNEL, ADC_SAMPLETIME_55POINT5);
    adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
    
    while(!adc_flag_get(ADC_FLAG_EOC))
    {
        if(timeout-- == 0) return 0;
    }
    adc_flag_clear(ADC_FLAG_EOC);
    
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
        case 2: // 1ms时刻，开始RED LED发光
            gpio_bit_set(SPO2_RED_CS_PORT, SPO2_RED_CS_PIN);
            break;
            
        case 3: // 1.5ms时刻，RED LED ADC采样
            adc_val = ADC_Read_Fast();
            filtered_val = fir_filter((float)adc_val, red_buffer, fir_coeffs, FIR_ORDER);
            
            if (wave_index < WAVE_BUF_SIZE) {
                red_wave_buf[wave_index] = filtered_val;
            }
            break;
            
        case 6: // 3ms时刻，关闭RED LED
            gpio_bit_reset(SPO2_RED_CS_PORT, SPO2_RED_CS_PIN);
            break;
            
        case 10: // 5ms时刻，开始IR LED发光
            gpio_bit_set(SPO2_IR_CS_PORT, SPO2_IR_CS_PIN);
            break;
            
        case 11: // 5.5ms时刻，IR LED ADC采样
            adc_val = ADC_Read_Fast();
            filtered_val = fir_filter((float)adc_val, ir_buffer, fir_coeffs, FIR_ORDER);
            
            if (wave_index < WAVE_BUF_SIZE) {
                ir_wave_buf[wave_index] = filtered_val;
                wave_index++;
                if (wave_count < WAVE_BUF_SIZE) wave_count++;
            }
            break;
            
        case 14: // 7ms时刻，关闭IR LED
            gpio_bit_reset(SPO2_IR_CS_PORT, SPO2_IR_CS_PIN);
            break;
            
        case 19: // 9.5ms时刻，计算逻辑
            // 每3秒(300点)计算一次
            if (wave_index >= WAVE_BUF_SIZE) {
                wave_index = 0; // 重置索引
                
                if(wave_count > 0) {
                    float red_max = red_wave_buf[0], red_min = red_wave_buf[0];
                    float ir_max = ir_wave_buf[0], ir_min = ir_wave_buf[0];
                    float ir_dc, ir_ac, red_dc, red_ac, R, spo2_calc, threshold_ir;
                    int pulse_count, last_peak_index, heart_rate;
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
                    ir_ac = ir_max - ir_min;
                    
                    if (ir_dc < 100.0f || ir_ac < 10.0f) {
                        g_spo2_result.spo2 = 0;
                        g_spo2_result.heart_rate = 0;
                        g_spo2_result.pi = 0;
                        g_spo2_result.updated = 1;
                    } 
                    else {
                        red_dc = (red_max + red_min) / 2.0f;
                        red_ac = red_max - red_min;
                        
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
                        
                        // 3秒窗口: BPM = count * 20
                        heart_rate = pulse_count * 20;
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
