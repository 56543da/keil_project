#include "SPO2_Algo.h"
#include <string.h>

static int32_t red_wave_buf[WAVE_BUF_SIZE];
static int32_t ir_wave_buf[WAVE_BUF_SIZE];
static uint16_t wave_index = 0;
static uint16_t wave_count = 0;
static volatile uint8_t g_calc_ready = 0;
static int32_t red_wave_buf_calib[R_CALIB_WAVE_BUF_SIZE];
static int32_t ir_wave_buf_calib[R_CALIB_WAVE_BUF_SIZE];
static uint16_t wave_index_calib = 0;
static uint16_t wave_count_calib = 0;
static volatile uint8_t g_calc_ready_calib = 0;

static SPO2_Result_t g_result = {0, 0, 0, 0, 0.0f, 0};
#define SPO2_HIST_SIZE 5
static uint8_t s_hist_count = 0;
static uint8_t s_hist_index = 0;
static uint16_t s_spo2_hist[SPO2_HIST_SIZE];
static uint16_t s_hr_hist[SPO2_HIST_SIZE];
static uint16_t s_pi_ir_hist[SPO2_HIST_SIZE];
static uint16_t s_pi_red_hist[SPO2_HIST_SIZE];
static uint16_t s_r_hist[SPO2_HIST_SIZE];
static void spo2_find_minmax(const int32_t *red_buf, const int32_t *ir_buf, uint16_t count, int32_t *red_max, int32_t *red_min, int32_t *ir_max, int32_t *ir_min);
static void spo2_calc_dcac(int32_t red_max, int32_t red_min, int32_t ir_max, int32_t ir_min, int32_t *red_dc, int32_t *red_ac, int32_t *ir_dc, int32_t *ir_ac);
static uint8_t spo2_calc_spo2(int32_t red_delta, int32_t ir_delta, int32_t *out_r_scaled);
static uint8_t spo2_calc_pi(int32_t dc, int32_t ac);
static uint8_t spo2_calc_hr(const int32_t *ir_buf, uint16_t count, int32_t ir_dc, int32_t ir_ac);
static void spo2_update_history(uint16_t spo2, uint16_t hr, uint16_t pi_ir, uint16_t pi_red, uint16_t r_scaled);

void SPO2_Algo_Init(void)
{
    memset(red_wave_buf, 0, sizeof(red_wave_buf));
    memset(ir_wave_buf, 0, sizeof(ir_wave_buf));
    wave_index = 0;
    wave_count = 0;
    g_calc_ready = 0;
}

void SPO2_Algo_PushData(uint16_t red, uint16_t ir)
{
    if(wave_index < WAVE_BUF_SIZE)
    {
        red_wave_buf[wave_index] = (int32_t)red;
        ir_wave_buf[wave_index] = (int32_t)ir;
        wave_index++;
        if(wave_count < WAVE_BUF_SIZE) wave_count++;
    }
    
    // 缓冲区满，准备计算
    if(wave_index >= WAVE_BUF_SIZE)
    {
        g_calc_ready = 1;
        // 注意：计算由主循环 SPO2_Algo_Process 触发
    }
}

void SPO2_Algo_PushDataCalib(uint16_t red, uint16_t ir)
{
    if(wave_index_calib < R_CALIB_WAVE_BUF_SIZE)
    {
        red_wave_buf_calib[wave_index_calib] = (int32_t)red;
        ir_wave_buf_calib[wave_index_calib] = (int32_t)ir;
        wave_index_calib++;
        if(wave_count_calib < R_CALIB_WAVE_BUF_SIZE) wave_count_calib++;
    }
    
    if(wave_index_calib >= R_CALIB_WAVE_BUF_SIZE)
    {
        g_calc_ready_calib = 1;
    }
}

void SPO2_Algo_Process(void)
{
    if(!g_calc_ready) return;

    // 执行计算 (定点化移植)
    if(wave_count >= WAVE_BUF_SIZE)
    {
        int32_t red_max, red_min, ir_max, ir_min;
        int32_t red_dc, red_ac, ir_dc, ir_ac;
        int32_t red_delta, ir_delta;
        int32_t r_scaled = 0;
        uint8_t spo2_val;
        uint8_t hr_val;
        uint8_t pi_ir;
        uint8_t pi_red;

        spo2_find_minmax(red_wave_buf, ir_wave_buf, wave_count, &red_max, &red_min, &ir_max, &ir_min);
        spo2_calc_dcac(red_max, red_min, ir_max, ir_min, &red_dc, &red_ac, &ir_dc, &ir_ac);

        red_delta = red_max - red_min;
        ir_delta = ir_max - ir_min;

        if(ir_dc > 100 && ir_ac > 5)
        {
            spo2_val = spo2_calc_spo2(red_delta, ir_delta, &r_scaled);
            hr_val = spo2_calc_hr(ir_wave_buf, wave_count, ir_dc, ir_ac);
            pi_ir = spo2_calc_pi(ir_dc, ir_max - ir_min);
            pi_red = spo2_calc_pi(red_dc, red_max - red_min);
            spo2_update_history(spo2_val, hr_val, pi_ir, pi_red, (uint16_t)r_scaled);
            g_result.updated = 1;
        }
        else
        {
            g_result.spo2 = 0;
            g_result.heart_rate = 0;
            g_result.pi_ir = 0;
            g_result.pi_red = 0;
            g_result.updated = 1;
            s_hist_count = 0;
            s_hist_index = 0;
        }
    }

    wave_index = 0;
    wave_count = 0;
    g_calc_ready = 0;
}

void SPO2_Algo_ProcessCalib(void)
{
    if(!g_calc_ready_calib) return;
    
    if(wave_count_calib >= R_CALIB_WAVE_BUF_SIZE)
    {
        int32_t red_max, red_min, ir_max, ir_min;
        int32_t red_delta, ir_delta;
        int32_t r_scaled = 0;
        int32_t spo2_calc = 0;
        
        spo2_find_minmax(red_wave_buf_calib, ir_wave_buf_calib, wave_count_calib, &red_max, &red_min, &ir_max, &ir_min);
        red_delta = red_max - red_min;
        ir_delta = ir_max - ir_min;
        if(red_delta > 0 && ir_delta > 0)
        {
            r_scaled = (red_delta * 1000) / ir_delta;
        }
        else
        {
            r_scaled = 0;
        }
        if(r_scaled >= 400 && r_scaled <= 1200) {
            spo2_calc = 110 - (25 * r_scaled) / 1000;
        } else {
            spo2_calc = 0;
        }
        if(spo2_calc > 100) spo2_calc = 100;
        if(spo2_calc < 70 && spo2_calc != 0) spo2_calc = 70;
        
        g_result.spo2 = (uint8_t)spo2_calc;
        g_result.heart_rate = 0;
        g_result.pi_ir = 0;
        g_result.pi_red = 0;
        g_result.r_val = (float)r_scaled / 1000.0f;
        g_result.updated = 1;
    }
    
    wave_index_calib = 0;
    wave_count_calib = 0;
    g_calc_ready_calib = 0;
}

uint8_t SPO2_Algo_GetResult(uint8_t *spo2, uint8_t *hr, uint8_t *pi_ir, uint8_t *pi_red, float *r_val)
{
    if(g_result.updated)
    {
        *spo2 = g_result.spo2;
        *hr = g_result.heart_rate;
        *pi_ir = g_result.pi_ir;
        *pi_red = g_result.pi_red;
        if(r_val) *r_val = g_result.r_val;
        g_result.updated = 0;
        return 1;
    }
    return 0;
}

static void spo2_find_minmax(const int32_t *red_buf, const int32_t *ir_buf, uint16_t count, int32_t *red_max, int32_t *red_min, int32_t *ir_max, int32_t *ir_min)
{
    uint16_t i;
    *red_max = red_buf[0];
    *red_min = red_buf[0];
    *ir_max = ir_buf[0];
    *ir_min = ir_buf[0];
    for(i = 1; i < count; i++) {
        if(red_buf[i] > *red_max) *red_max = red_buf[i];
        if(red_buf[i] < *red_min) *red_min = red_buf[i];
        if(ir_buf[i] > *ir_max) *ir_max = ir_buf[i];
        if(ir_buf[i] < *ir_min) *ir_min = ir_buf[i];
    }
}

static void spo2_calc_dcac(int32_t red_max, int32_t red_min, int32_t ir_max, int32_t ir_min, int32_t *red_dc, int32_t *red_ac, int32_t *ir_dc, int32_t *ir_ac)
{
    *red_dc = (red_max + red_min) / 2;
    *ir_dc = (ir_max + ir_min) / 2;
    *red_ac = (red_max - red_min) / 2;
    *ir_ac = (ir_max - ir_min) / 2;
}

static uint8_t spo2_calc_spo2(int32_t red_delta, int32_t ir_delta, int32_t *out_r_scaled)
{
    int32_t r_scaled;
    int32_t spo2_calc;
    if(red_delta > 0 && ir_delta > 0) {
        r_scaled = (red_delta * 1000) / ir_delta;
    } else {
        r_scaled = 0;
    }
    if(r_scaled >= 400 && r_scaled <= 1200) {
        spo2_calc = 110 - (25 * r_scaled) / 1000;
    } else {
        spo2_calc = 0;
    }
    if(spo2_calc > 100) spo2_calc = 100;
    if(spo2_calc < 70) spo2_calc = 70;
    *out_r_scaled = r_scaled;
    return (uint8_t)spo2_calc;
}

static uint8_t spo2_calc_pi(int32_t dc, int32_t delta_i)
{
    uint32_t pi_x10;
    uint16_t pi;
    if(dc == 0) return 0;
    pi_x10 = (uint32_t)(delta_i * 1000) / (uint32_t)dc;
    pi = (uint16_t)((pi_x10 + 5) / 10);
    if(pi == 0) pi = 1;
    if(pi > 99) pi = 99;
    return (uint8_t)pi;
}

static uint8_t spo2_calc_hr(const int32_t *ir_buf, uint16_t count, int32_t ir_dc, int32_t ir_ac)
{
    int32_t threshold;
    int32_t last_peak_index;
    int32_t first_peak_index;
    int32_t last_peak_value;
    uint16_t i;
    uint16_t interval_count;
    int32_t sum_intervals;
    int32_t hr_calc;
    const int32_t min_interval = 30;
    const int32_t max_interval = 200;

    threshold = ir_dc + ((ir_ac * 2) / 3);
    last_peak_index = -1000;
    first_peak_index = -1;
    last_peak_value = 0;
    interval_count = 0;
    sum_intervals = 0;

    for(i = 1; i < count - 1; i++) {
        int32_t cur = ir_buf[i];
        int32_t prev = ir_buf[i-1];
        int32_t next = ir_buf[i+1];
        if(cur > threshold && cur > prev && cur > next) {
            if((int32_t)i - last_peak_index < min_interval) {
                if(cur > last_peak_value) {
                    last_peak_index = (int32_t)i;
                    last_peak_value = cur;
                }
            } else {
                if(first_peak_index < 0) {
                    first_peak_index = (int32_t)i;
                } else {
                    int32_t interval = (int32_t)i - last_peak_index;
                    if(interval >= min_interval && interval <= max_interval) {
                        sum_intervals += interval;
                        interval_count++;
                    }
                }
                last_peak_index = (int32_t)i;
                last_peak_value = cur;
            }
        }
    }

    if(interval_count > 0) {
        hr_calc = (6000 * interval_count) / sum_intervals;
    } else {
        hr_calc = 0;
    }

    if(hr_calc > 200) hr_calc = 200;
    if(hr_calc > 0 && hr_calc < 30) hr_calc = 30;
    if(hr_calc < 0) hr_calc = 0;
    return (uint8_t)hr_calc;
}

static void spo2_update_history(uint16_t spo2, uint16_t hr, uint16_t pi_ir, uint16_t pi_red, uint16_t r_scaled)
{
    uint32_t sum_spo2, sum_hr, sum_pi, sum_r;
    uint16_t i;

    s_spo2_hist[s_hist_index] = spo2;
    s_hr_hist[s_hist_index] = hr;
    s_pi_ir_hist[s_hist_index] = pi_ir;
    s_pi_red_hist[s_hist_index] = pi_red;
    s_r_hist[s_hist_index] = r_scaled;
    if(s_hist_count < SPO2_HIST_SIZE) s_hist_count++;
    s_hist_index++;
    if(s_hist_index >= SPO2_HIST_SIZE) s_hist_index = 0;

    sum_spo2 = 0;
    sum_hr = 0;
    sum_pi = 0;
    sum_r = 0;
    for(i = 0; i < s_hist_count; i++) {
        sum_spo2 += s_spo2_hist[i];
        sum_hr += s_hr_hist[i];
        sum_pi += s_pi_ir_hist[i];
        sum_r += s_r_hist[i];
    }
    g_result.spo2 = (uint8_t)(sum_spo2 / s_hist_count);
    g_result.heart_rate = (uint8_t)(sum_hr / s_hist_count);
    g_result.pi_ir = (uint8_t)(sum_pi / s_hist_count);
    sum_pi = 0;
    for(i = 0; i < s_hist_count; i++) {
        sum_pi += s_pi_red_hist[i];
    }
    g_result.pi_red = (uint8_t)(sum_pi / s_hist_count);
    g_result.r_val = (float)(sum_r / s_hist_count) / 1000.0f;
}
