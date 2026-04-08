#include "SPO2_Algo.h"
#include <string.h>
#include <stdio.h>
// ===================== 【微型心率算法】严格30~220BPM =====================

/* --- 配置参数 --- */
#define SPO2_SAMPLE_RATE    100
#define HR_BPM_MIN          30
#define HR_BPM_MAX          220
#define HR_PERIOD_MIN       (60 * SPO2_SAMPLE_RATE / HR_BPM_MAX)   /* 27点@220BPM */
#define HR_PERIOD_MAX       (60 * SPO2_SAMPLE_RATE / HR_BPM_MIN)   // 200点
#define HR_SQI_THRESHOLD    3                                     // 信号幅度门槛

/* --- 全局状态 --- */
static int32_t ir_wave_buf[WAVE_BUF_SIZE];
static int32_t red_wave_buf[WAVE_BUF_SIZE];
static uint16_t wave_head = 0;
static uint16_t wave_len = 0;
static uint16_t wave_process_cnt = 0;
static volatile uint8_t g_calc_ready = 0;

static SPO2_Result_t g_result = {0, 0, 0, 0, 0.0f, 0};
static int32_t g_last_ac = 0;

/* --- 内部函数 --- */

/* =====================================================================
 * ======================= 核心修改：极简边沿检测算法 ===================
 * ===================================================================== */

/** 
  * @brief 极简心率算法 | 适配上升-下降波形 | 锁死30~220BPM 
  * @param ir_buf 红外滤波缓冲区 
  * @param head 环形缓冲区头指针 
  * @param len 窗口长度 
  * @param period 输出周期点数 
  * @return 心率BPM 
  */ 
 static uint8_t Simple_HR_Detect(const int32_t *ir_buf, uint16_t head, uint16_t len, uint16_t *period) 
 { 
    uint16_t i, idx; 
    int32_t window[SPO2_WINDOW_SIZE];
     int32_t max = -0x7FFFFFFF, min = 0x7FFFFFFF; 
    int32_t valley_th;
     uint16_t edge_cnt = 0; 
     uint16_t last_edge = 0; 
     uint32_t period_sum = 0; 
 
    if(len < 3 || len > SPO2_WINDOW_SIZE)
    {
        *period = 0;
        return 0;
    }

     for (i = 0; i < len; i++) { 
        idx = (head + WAVE_BUF_SIZE - len + i) % WAVE_BUF_SIZE;
        window[i] = ir_buf[idx];
        if (window[i] > max) max = window[i];
        if (window[i] < min) min = window[i];
     } 
 
    g_last_ac = max - min;
     if ((max - min) < HR_SQI_THRESHOLD) { 
         *period = 0; 
      //   printf("SPO2 Algo: AC too small (%d)\r\n", (int)g_last_ac); 
         return 0; 
     } 
    valley_th = min + ((max - min) >> 2);

    for (i = 1; i < (uint16_t)(len - 1); i++) {
        if (window[i] <= valley_th &&
            window[i] <= window[i - 1] &&
            window[i] < window[i + 1]) {
            if (edge_cnt == 0) {
                edge_cnt = 1;
                last_edge = i;
            } else {
                uint16_t p = i - last_edge;
                if (p >= HR_PERIOD_MIN && p <= HR_PERIOD_MAX) {
                    period_sum += p;
                    edge_cnt++;
                    last_edge = i;
                } else if (p > HR_PERIOD_MAX) {
                    last_edge = i;
                }
            }
        }
    }
 
     /* 3. 计算平均周期 + 心率 */ 
     if (edge_cnt < 2) { 
         *period = 0; 
     //    printf("SPO2 Algo: No HR detected\r\n"); 
         return 0; 
     } 
 
     *period = period_sum / (edge_cnt - 1); 
     {
         uint8_t hr = (uint8_t)(60 * SPO2_SAMPLE_RATE / (*period)); 
 
         /* 最终双重锁死（绝对不会超30~220） */ 
         if (hr < 30) hr = 30; 
         if (hr > 220) hr = 220; 
 
       //  printf("SPO2 Algo: Found %d periods, Period=%d, HR=%d\r\n", edge_cnt-1, *period, hr); 
         return hr; 
     }
 } 

/* =====================================================================
 * ======================= 底层驱动与接口（保持不变）=====================
 * ===================================================================== */

void SPO2_Algo_Init(void)
{
    memset(ir_wave_buf, 0, sizeof(ir_wave_buf));
    wave_head = 0; wave_len = 0; wave_process_cnt = 0; g_calc_ready = 0;
    g_result = (SPO2_Result_t){0, 0, 0, 0, 0.0f, 0};
}

void SPO2_Algo_PushData(uint16_t red, uint16_t ir)
{
    // 去除所有滤波，直接存入原始数据
    red_wave_buf[wave_head] = (int32_t)red; 
    ir_wave_buf[wave_head]  = (int32_t)ir; 
    
    wave_head = (wave_head + 1) % WAVE_BUF_SIZE;
    if(wave_len < WAVE_BUF_SIZE) wave_len++;
    
    wave_process_cnt++;
    if(wave_len >= SPO2_WINDOW_SIZE && wave_process_cnt >= 100)
    {
        g_calc_ready = 1;
        wave_process_cnt = 0;
    }
}

void SPO2_Algo_Process(void)
{
    if(!g_calc_ready) return;
    g_calc_ready = 0;

    uint16_t period = 0;
    uint8_t hr_val = Simple_HR_Detect(ir_wave_buf, wave_head, SPO2_WINDOW_SIZE, &period);
    
    if(hr_val > 0 && period > 0)
    {
        g_result.heart_rate = hr_val;
        g_result.spo2 = 95; // 临时默认值，SpO2 逻辑可后续加
        g_result.pi_ir = 5; // 赋一个模拟值，以便 UI 更新
        g_result.pi_red = 5;
        g_result.r_val = 0.6f;
        g_result.updated = 1;
     //   printf("=== Final HR: %d ===\r\n", hr_val);
    }
    else
    {
      //  printf("No HR\r\n");
        g_result.heart_rate = 0;
        g_result.spo2 = 0;
        g_result.pi_ir = 0;
        g_result.pi_red = 0;
        g_result.r_val = 0.0f;
        g_result.updated = 1; // 强制更新，确保上位机能收到包含 AC 值的日志
    }
}

uint8_t SPO2_Algo_GetResult(uint8_t *spo2, uint8_t *hr, uint8_t *pi_ir, uint8_t *pi_red, float *r_val, int32_t *ac_val)
{
    if(g_result.updated)
    {
        if(spo2) *spo2 = g_result.spo2;
        if(hr)   *hr = g_result.heart_rate;
        if(pi_ir) *pi_ir = g_result.pi_ir;
        if(pi_red) *pi_red = g_result.pi_red;
        if(r_val) *r_val = g_result.r_val;
        if(ac_val) *ac_val = g_last_ac;
        g_result.updated = 0;
        return 1;
    }
    return 0;
}
