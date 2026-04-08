#include "SPO2_Algo.h"
#include <string.h>

/* --- 宏定义与历史记录大小 --- */
#define SPO2_HIST_SIZE 5

/* --- 脉率算法优化补充宏定义 --- */
#define HR_BPM_MIN 30
#define HR_BPM_MAX 210
#define HR_PERIOD_MIN (60 * SPO2_SAMPLE_RATE / HR_BPM_MAX) /* 28点@100Hz */
#define HR_PERIOD_MAX (60 * SPO2_SAMPLE_RATE / HR_BPM_MIN) /* 200点@100Hz */
#define HR_PEAK_REFRACTORY HR_PERIOD_MIN  /* 波峰不应期 */
#define HR_SQI_THRESHOLD 10                /* 信号质量AC阈值 */
#define HR_PERIOD_TOLERANCE 25             /* 周期异常容忍度±25% */
#define HR_HIST_SIZE 5                      /* 结果平滑历史长度 */

/* --- 环形缓冲区与处理状态 --- */
static int32_t red_wave_buf[WAVE_BUF_SIZE];
static int32_t ir_wave_buf[WAVE_BUF_SIZE];
static uint16_t wave_head = 0;        /* 环形缓冲区当前写入指针 */
static uint16_t wave_len = 0;         /* 缓冲区内有效数据总量 */
static uint16_t wave_process_cnt = 0; /* 自上次计算后新增的数据点数 */
static volatile uint8_t g_calc_ready = 0; /* 算法触发标志 */

/* --- 优化后滤波器历史状态 --- */
static int32_t ir_bp_prev1 = 0, ir_bp_prev2 = 0;
static int32_t ir_bp_prev_in1 = 0, ir_bp_prev_in2 = 0;
static int32_t red_bp_prev1 = 0, red_bp_prev2 = 0;
static int32_t red_bp_prev_in1 = 0, red_bp_prev_in2 = 0;

/* --- 结果与平滑历史 --- */
static SPO2_Result_t g_result = {0, 0, 0, 0, 0.0f, 0};
static uint8_t hr_hist_index = 0;
static uint8_t hr_hist_count = 0;
static uint16_t hr_result_hist[HR_HIST_SIZE];

static uint8_t s_hist_count = 0;
static uint8_t s_hist_index = 0;
static uint16_t s_spo2_hist[SPO2_HIST_SIZE];
static uint16_t s_pi_ir_hist[SPO2_HIST_SIZE];
static uint16_t s_r_hist[SPO2_HIST_SIZE];

/* --- 内部函数声明 (遵循 C89 变量声明置于块首原则) --- */
static int32_t iir_bandpass_ir(int32_t input);
static int32_t iir_bandpass_red(int32_t input);
static uint8_t spo2_calc_hr_period(const int32_t *ir_buf, uint16_t head, uint16_t len, uint16_t *period);
static void spo2_find_minmax_in_period(const int32_t *red_buf, const int32_t *ir_buf, uint16_t head, uint16_t period, int32_t *red_max, int32_t *red_min, int32_t *ir_max, int32_t *ir_min);
static void spo2_update_history(uint16_t spo2, uint16_t hr, uint16_t pi_ir, uint16_t pi_red, uint16_t r_scaled);
static void hr_update_smooth(uint16_t hr_new);

/* --- 对外接口实现 --- */

/**
 * @brief 初始化血氧算法模块
 */
void SPO2_Algo_Init(void)
{
    memset(red_wave_buf, 0, sizeof(red_wave_buf));
    memset(ir_wave_buf, 0, sizeof(ir_wave_buf));
    wave_head = 0;
    wave_len = 0;
    wave_process_cnt = 0;
    g_calc_ready = 0;
    
    ir_bp_prev1 = 0; ir_bp_prev2 = 0;
    ir_bp_prev_in1 = 0; ir_bp_prev_in2 = 0;
    red_bp_prev1 = 0; red_bp_prev2 = 0;
    red_bp_prev_in1 = 0; red_bp_prev_in2 = 0;
    
    hr_hist_index = 0;
    hr_hist_count = 0;
    memset(hr_result_hist, 0, sizeof(hr_result_hist));
    
    s_hist_count = 0;
    s_hist_index = 0;
    memset(s_spo2_hist, 0, sizeof(s_spo2_hist));
    memset(s_pi_ir_hist, 0, sizeof(s_pi_ir_hist));
    memset(s_r_hist, 0, sizeof(s_r_hist));
}

/**
 * @brief 存入最新的红光与红外光数据
 *        执行二阶 IIR 带通滤波，并填入环形缓冲区。当新数据积累满 1 秒时，触发算法计算。
 */
void SPO2_Algo_PushData(uint16_t red, uint16_t ir)
{
    int32_t filtered_red;
    int32_t filtered_ir;

    /* 1. 二阶 IIR 带通滤波 (0.5~3.5Hz)：滤除基线漂移与高频噪声 */
    filtered_red = iir_bandpass_red((int32_t)red);
    filtered_ir  = iir_bandpass_ir((int32_t)ir);

    /* 2. 存入环形缓冲区 */
    red_wave_buf[wave_head] = filtered_red;
    ir_wave_buf[wave_head]  = filtered_ir;
    
    /* 3. 更新写入指针与有效长度 */
    wave_head = (wave_head + 1) % WAVE_BUF_SIZE;
    if(wave_len < WAVE_BUF_SIZE) {
        wave_len++;
    }
    
    /* 4. 每累积 100 个新点（对应 100Hz 下的 1 秒），触发一次滑动窗口计算 */
    wave_process_cnt++;
    if(wave_len >= SPO2_WINDOW_SIZE && wave_process_cnt >= 100)
    {
        g_calc_ready = 1;
        wave_process_cnt = 0;
    }
}

/**
 * @brief 核心算法处理：在最近 4 秒窗口中提取心跳特征、交直流分量并计算 R 值和血氧
 */
void SPO2_Algo_Process(void)
{
    uint16_t period = 0;
    uint8_t hr_val;
    int32_t r_max, r_min, i_max, i_min;
    int32_t ac_red, dc_red, ac_ir, dc_ir;
    int32_t r_scaled = 0;
    int32_t spo2_calc = 0;
    uint8_t spo2_val = 0;
    uint8_t pi_ir = 0, pi_red = 0;

    /* 若未满足触发条件，直接返回 */
    if(!g_calc_ready) {
        return;
    }
    g_calc_ready = 0;

    /* --- 步骤 1：寻找心率周期 (优化版) ---
     * 使用自适应阈值和中位数过滤提取心率
     */
    hr_val = spo2_calc_hr_period(ir_wave_buf, wave_head, SPO2_WINDOW_SIZE, &period);

    if(hr_val > 0 && period > 0)
    {
        /* --- 步骤 2：提取局部特征 ---
         * 根据上一步得到的心跳周期，在最近的一个完整周期内找出信号的最大值和最小值
         */
        spo2_find_minmax_in_period(red_wave_buf, ir_wave_buf, wave_head, period, &r_max, &r_min, &i_max, &i_min);
        
        /* --- 步骤 3：交直流分离 (AC/DC Extraction) --- */
        ac_red = r_max - r_min;
        dc_red = (r_max + r_min) / 2;
        ac_ir  = i_max - i_min;
        dc_ir  = (i_max + i_min) / 2;

        /* 确保存在有效的交流脉动信号才进行后续计算 (AC 阈值已在 HR 计算中校验过一次) */
        if(ac_ir >= HR_SQI_THRESHOLD && dc_red > 0 && dc_ir > 0)
        {
            /* --- 步骤 4：计算特征比值 R ---
             * R_scaled = (AC_red * DC_ir * 1000) / (AC_ir * dc_red)
             */
            r_scaled = (ac_red * dc_ir * 1000) / (ac_ir * dc_red);
            
            /* --- 步骤 5：计算血氧 SpO2 ---
             * 根据经验公式计算: SpO2 = 110 - 25 * R
             */
            if (r_scaled >= 400 && r_scaled <= 1500) {
                spo2_calc = 110 - (25 * r_scaled) / 1000;
            } else {
                spo2_calc = 0;
            }
            
            /* 范围限幅：合法范围 70% ~ 100% */
            if(spo2_calc > 100) spo2_calc = 100;
            if(spo2_calc < 70 && spo2_calc != 0) spo2_calc = 70;
            spo2_val = (uint8_t)spo2_calc;

            /* --- 步骤 6：计算灌注指数 PI --- */
            pi_ir = (uint8_t)((ac_ir * 100) / dc_ir);
            pi_red = (uint8_t)((ac_red * 100) / dc_red);
            if(pi_ir > 99) pi_ir = 99;
            if(pi_red > 99) pi_red = 99;
            
            /* --- 步骤 7：历史平滑更新 (集成跳变限制) --- */
            if(spo2_val >= 70) {
                spo2_update_history(spo2_val, hr_val, pi_ir, pi_red, (uint16_t)r_scaled);
                g_result.updated = 1;
            }
        }
    }
    else
    {
        /* 信号质量差或未检测到心跳，将数值清零或保持，此处选择清零触发 UI 更新 */
        g_result.spo2 = 0;
        g_result.heart_rate = 0;
        g_result.updated = 1;
        
        /* 信号断开，清空滑动平均历史队列 */
        s_hist_count = 0; 
        hr_hist_count = 0;
    }
}

/**
 * @brief 获取算法结果
 * @return 1=有新结果, 0=暂无更新
 */
uint8_t SPO2_Algo_GetResult(uint8_t *spo2, uint8_t *hr, uint8_t *pi_ir, uint8_t *pi_red, float *r_val)
{
    if(g_result.updated)
    {
        if(spo2)   *spo2 = g_result.spo2;
        if(hr)     *hr = g_result.heart_rate;
        if(pi_ir)  *pi_ir = g_result.pi_ir;
        if(pi_red) *pi_red = g_result.pi_red;
        if(r_val)  *r_val = g_result.r_val;
        
        g_result.updated = 0;
        return 1;
    }
    return 0;
}

/* =====================================================================
 * ======================= 内部功能子函数实现 ==========================
 * ===================================================================== */

/**
 * @brief 二阶 IIR 带通滤波器 0.5~3.5Hz@100Hz 采样率 (红外光)
 */
static int32_t iir_bandpass_ir(int32_t input)
{
    const int32_t b0 = 1527;
    const int32_t b1 = 0;
    const int32_t b2 = -1527;
    const int32_t a1 = 32604;
    const int32_t a2 = -30541;
    const int32_t scale = 32768;

    int32_t output;
    output = (b0 * input + b1 * ir_bp_prev_in1 + b2 * ir_bp_prev_in2 
            - a1 * ir_bp_prev1 - a2 * ir_bp_prev2) / scale;

    ir_bp_prev_in2 = ir_bp_prev_in1;
    ir_bp_prev_in1 = input;
    ir_bp_prev2 = ir_bp_prev1;
    ir_bp_prev1 = output;

    return output;
}

/**
 * @brief 二阶 IIR 带通滤波器 0.5~3.5Hz@100Hz 采样率 (红光)
 */
static int32_t iir_bandpass_red(int32_t input)
{
    const int32_t b0 = 1527;
    const int32_t b1 = 0;
    const int32_t b2 = -1527;
    const int32_t a1 = 32604;
    const int32_t a2 = -30541;
    const int32_t scale = 32768;

    int32_t output;
    output = (b0 * input + b1 * red_bp_prev_in1 + b2 * red_bp_prev_in2 
            - a1 * red_bp_prev1 - a2 * red_bp_prev2) / scale;

    red_bp_prev_in2 = red_bp_prev_in1;
    red_bp_prev_in1 = input;
    red_bp_prev2 = red_bp_prev1;
    red_bp_prev1 = output;

    return output;
}

/**
 * @brief 优化版：红外信号脉率计算，自适应波峰检测+异常剔除
 * @param ir_buf 红外信号环形缓冲区
 * @param head 缓冲区当前写入头指针
 * @param len 计算窗口长度（固定400点=4秒）
 * @param period 传出参数：平均心跳周期（采样点数）
 * @return 有效心率值(bpm)，0=信号无效/未检测到
 */
static uint8_t spo2_calc_hr_period(const int32_t *ir_buf, uint16_t head, uint16_t len, uint16_t *period)
{
    uint16_t i;
    int32_t max_v = -1000000, min_v = 1000000;
    int32_t peak_threshold, ac_amplitude;
    uint16_t idx, idx_prev, idx_next;
    int32_t last_peak_pos = -1;
    uint16_t period_list[20]; /* 4秒窗口最多8个波峰，预留足够空间 */
    uint16_t period_cnt = 0;
    uint16_t p, median_p;
    uint32_t sum_valid_p = 0;
    uint16_t valid_period_cnt = 0;
    uint8_t hr_result = 0;

    /* --- 步骤1：窗口内信号极值与质量评估 --- */
    for(i = 0; i < len; i++) {
        idx = (head + WAVE_BUF_SIZE - 1 - i) % WAVE_BUF_SIZE;
        if(ir_buf[idx] > max_v) max_v = ir_buf[idx];
        if(ir_buf[idx] < min_v) min_v = ir_buf[idx];
    }
    ac_amplitude = max_v - min_v;
    /* 信号质量不达标，直接返回无效 */
    if(ac_amplitude < HR_SQI_THRESHOLD) {
        *period = 0;
        return 0;
    }

    /* --- 步骤2：自适应峰值阈值 --- */
    /* 阈值 = 最小值 + 振幅的60%，自适应信号幅度变化 */
    peak_threshold = min_v + (ac_amplitude * 3) / 5;

    /* --- 步骤3：带不应期的波峰检测 --- */
    for(i = 2; i < len - 2; i++) {
        idx = (head + WAVE_BUF_SIZE - 1 - i) % WAVE_BUF_SIZE;
        idx_prev = (idx + WAVE_BUF_SIZE - 1) % WAVE_BUF_SIZE;
        idx_next = (idx + 1) % WAVE_BUF_SIZE;

        /* 波峰判定3重条件：
         * 1. 大于自适应阈值 2. 大于左右邻点（波峰特征）3. 满足不应期限制
         */
        if(ir_buf[idx] > peak_threshold 
            && ir_buf[idx] > ir_buf[idx_prev] 
            && ir_buf[idx] > ir_buf[idx_next]
            && (last_peak_pos == -1 || (i - (uint16_t)last_peak_pos) >= HR_PEAK_REFRACTORY))
        {
            if(last_peak_pos != -1) {
                p = i - (uint16_t)last_peak_pos;
                /* 周期先做基础范围过滤 */
                if(p >= HR_PERIOD_MIN && p <= HR_PERIOD_MAX) {
                    if(period_cnt < 20) {
                        period_list[period_cnt++] = p;
                    }
                }
            }
            last_peak_pos = (int32_t)i;
        }
    }

    /* 有效波峰数量不足，返回无效 */
    if(period_cnt < 2) {
        *period = 0;
        return 0;
    }

    /* --- 步骤4：异常周期剔除（中位数法）--- */
    /* 先对周期排序，取中位数作为基准 */
    for(i = 0; i < period_cnt - 1; i++) {
        uint16_t j, temp;
        for(j = 0; j < period_cnt - i - 1; j++) {
            if(period_list[j] > period_list[j+1]) {
                temp = period_list[j];
                period_list[j] = period_list[j+1];
                period_list[j+1] = temp;
            }
        }
    }
    median_p = period_list[period_cnt / 2];

    /* 剔除与中位数偏差超过±25%的异常周期 */
    for(i = 0; i < period_cnt; i++) {
        int32_t diff = (int32_t)period_list[i] - (int32_t)median_p;
        if(diff < 0) diff = -diff;
        if(diff * 100 / (int32_t)median_p <= HR_PERIOD_TOLERANCE) {
            sum_valid_p += period_list[i];
            valid_period_cnt++;
        }
    }

    /* 有效周期不足，返回无效 */
    if(valid_period_cnt < 2) {
        *period = 0;
        return 0;
    }

    /* --- 步骤5：计算平均周期与心率 --- */
    *period = (uint16_t)(sum_valid_p / valid_period_cnt);
    hr_result = (uint8_t)(60 * SPO2_SAMPLE_RATE / (*period));

    return hr_result;
}

/**
 * @brief 在一个心跳周期内寻找红光和红外光的最大值和最小值，用于计算交直流分量
 */
static void spo2_find_minmax_in_period(const int32_t *red_buf, const int32_t *ir_buf, uint16_t head, uint16_t period, int32_t *red_max, int32_t *red_min, int32_t *ir_max, int32_t *ir_min)
{
    uint16_t i;
    uint16_t idx;
    
    *red_max = -1000000; *red_min = 1000000;
    *ir_max = -1000000;  *ir_min = 1000000;
    
    for(i = 0; i < period; i++) {
        idx = (head + WAVE_BUF_SIZE - 1 - i) % WAVE_BUF_SIZE;
        if(red_buf[idx] > *red_max) *red_max = red_buf[idx];
        if(red_buf[idx] < *red_min) *red_min = red_buf[idx];
        if(ir_buf[idx] > *ir_max) *ir_max = ir_buf[idx];
        if(ir_buf[idx] < *ir_min) *ir_min = ir_buf[idx];
    }
}

/**
 * @brief 优化版：脉率结果滑动平滑，带跳变限制
 */
static void hr_update_smooth(uint16_t hr_new)
{
    uint32_t sum_hr = 0;
    uint16_t i;
    uint8_t last_valid_hr = 0;

    /* 读取上一次有效结果 */
    if(hr_hist_count > 0) {
        last_valid_hr = g_result.heart_rate;
    }

    /* 跳变限制：单次变化超过±20bpm，不更新，避免突变 */
    if(last_valid_hr != 0) {
        int16_t hr_diff = (int16_t)hr_new - (int16_t)last_valid_hr;
        if(hr_diff < 0) hr_diff = -hr_diff;
        if(hr_diff > 20) {
            return;
        }
    }

    /* 压入历史数组 */
    hr_result_hist[hr_hist_index] = hr_new;
    if(hr_hist_count < HR_HIST_SIZE) hr_hist_count++;
    hr_hist_index = (hr_hist_index + 1) % HR_HIST_SIZE;

    /* 计算滑动平均 */
    for(i = 0; i < hr_hist_count; i++) {
        sum_hr += hr_result_hist[i];
    }
    g_result.heart_rate = (uint8_t)(sum_hr / hr_hist_count);
}

/**
 * @brief 对最近计算的历史结果进行滑动平均平滑处理 (血氧、PI、R值)
 */
static void spo2_update_history(uint16_t spo2, uint16_t hr, uint16_t pi_ir, uint16_t pi_red, uint16_t r_scaled)
{
    uint32_t sum_spo2 = 0, sum_pi = 0, sum_r = 0;
    uint16_t i;

    /* 更新心率 (使用优化后的平滑函数) */
    hr_update_smooth(hr);

    /* 更新血氧、PI 和 R 值历史 */
    s_spo2_hist[s_hist_index] = spo2;
    s_pi_ir_hist[s_hist_index] = pi_ir;
    s_r_hist[s_hist_index] = r_scaled;

    if(s_hist_count < SPO2_HIST_SIZE) s_hist_count++;
    s_hist_index = (s_hist_index + 1) % SPO2_HIST_SIZE;

    /* 计算滑动平均 */
    for(i = 0; i < s_hist_count; i++) {
        sum_spo2 += s_spo2_hist[i];
        sum_pi += s_pi_ir_hist[i];
        sum_r += s_r_hist[i];
    }

    g_result.spo2 = (uint8_t)(sum_spo2 / s_hist_count);
    g_result.pi_ir = (uint8_t)(sum_pi / s_hist_count);
    g_result.pi_red = (uint8_t)pi_red; /* PI_RED 作为参考辅助值，不平滑直接输出实时状态 */
    g_result.r_val = (float)sum_r / (s_hist_count * 1000.0f);
}
