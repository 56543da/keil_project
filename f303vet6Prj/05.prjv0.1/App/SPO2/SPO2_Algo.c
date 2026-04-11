#include "SPO2_Algo.h"
#include <string.h>

/* --- 宏定义与历史记录大小 --- */
#define SPO2_HIST_SIZE 5

/* --- 脉率算法优化补充宏定义 --- */
#define HR_SQI_THRESHOLD 3                /* 信号质量AC阈值 */
#define HR_HIST_SIZE 5                      /* 结果平滑历史长度 */
#define SPO2_DROPOUT_AC_THRESHOLD 200
#define DEMO_BASELINE_SHIFT 5
#define DEMO_IIR_BASELINE_SHIFT 3
#define DEMO_IIR_SHIFT 3

/* --- 环形缓冲区与处理状态 --- */
static int32_t red_wave_buf[WAVE_BUF_SIZE];
static int32_t ir_wave_buf[WAVE_BUF_SIZE];
static uint16_t wave_head = 0;        /* 环形缓冲区当前写入指针 */
static uint16_t wave_len = 0;         /* 缓冲区内有效数据总量 */
static uint16_t wave_process_cnt = 0; /* 自上次计算后新增的数据点数 */
static volatile uint8_t g_calc_ready = 0; /* 算法触发标志 */
static volatile uint8_t g_abnormal_segment_flag = 0;

/* --- 结果与平滑历史 --- */
static SPO2_Result_t g_result = {0, 0, 0, 0, 0.0f, 0};
static int32_t g_last_ac = 0;         /* 记录最近一次计算的 AC 幅值 */
static uint8_t hr_hist_index = 0;
static uint8_t hr_hist_count = 0;
static uint16_t hr_result_hist[HR_HIST_SIZE];

static uint8_t s_hist_count = 0;
static uint8_t s_hist_index = 0;
static uint16_t s_spo2_hist[SPO2_HIST_SIZE];
static uint16_t s_pi_ir_hist[SPO2_HIST_SIZE];
static uint16_t s_r_hist[SPO2_HIST_SIZE];

/* --- 滤波器状态与缓存 --- */
static int32_t s_red_filter_out = 0;
static int32_t s_ir_filter_out = 0;

/* --- 内部函数声明 (遵循 C89 变量声明置于块首原则) --- */
static void spo2_simple_iir_filter(uint16_t red_in, uint16_t ir_in, int32_t *red_out, int32_t *ir_out);
static uint8_t spo2_calc_hr_period(const int32_t *ir_buf, uint16_t head, uint16_t len, uint16_t *period);
static void spo2_find_minmax_in_period(const int32_t *red_buf, const int32_t *ir_buf, uint16_t head, uint16_t period, int32_t *red_max, int32_t *red_min, int32_t *ir_max, int32_t *ir_min);
static void spo2_update_history(uint16_t spo2, uint16_t hr, uint16_t pi_ir, uint16_t pi_red, uint16_t r_scaled);
static void hr_update_smooth(uint16_t hr_new);
/**
 * @brief 判断最近 5 秒窗口是否发生手指脱落 (Dropout)
 *        通过检测红外光 (IR) 的交流摆幅 (AC)，如果摆幅过大（如拔出手指时的剧烈变化），则认为是异常段
 * @param ir_buf 红外信号环形缓冲区
 * @param head 缓冲区当前写入头指针
 * @param len 计算窗口长度
 * @return 1=异常段(Dropout)，0=正常段
 */
static uint8_t spo2_is_dropout_by_ac(const int32_t *ir_buf, uint16_t head, uint16_t len);

/* --- Demo 滤波演示的内部状态 --- */
static int32_t s_demo_iir_y = 0;        /* IIR 低通内部状态（Q8放大） */
static int32_t s_demo_drift_baseline = 0; /* 基线估计（Q8放大） */
static uint16_t s_demo_med_buf[3] = {0,0,0}; /* 中值滤波的3点窗 */
static uint8_t  s_demo_med_idx = 0;
static uint8_t  s_demo_inited = 0;

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
    g_abnormal_segment_flag = 0;
    
    hr_hist_index = 0;
    hr_hist_count = 0;
    memset(hr_result_hist, 0, sizeof(hr_result_hist));
    
    s_hist_count = 0;
    s_hist_index = 0;
    memset(s_spo2_hist, 0, sizeof(s_spo2_hist));
    memset(s_pi_ir_hist, 0, sizeof(s_pi_ir_hist));
    memset(s_r_hist, 0, sizeof(s_r_hist));
    g_last_ac = 0;
    
    s_red_filter_out = 0;
    s_ir_filter_out = 0;
    
    s_demo_iir_y = 0;
    s_demo_drift_baseline = 0;
    s_demo_med_buf[0] = s_demo_med_buf[1] = s_demo_med_buf[2] = 0;
    s_demo_med_idx = 0;
    s_demo_inited = 0;
}

/**
 * @brief 存入最新的红光与红外光数据
 *        经过简单一阶 IIR 低通滤波后存入环形缓冲区。
 *        当新数据积累满 5 秒时，触发算法计算。
 * @param red 原始红光 ADC 值
 * @param ir  原始红外光 ADC 值
 * @param red_out 传出参数：滤波后的红光值，用于 UI 或串口转发
 * @param ir_out  传出参数：滤波后的红外光值，用于 UI 或串口转发
 */
void SPO2_Algo_PushData(uint16_t red, uint16_t ir, uint16_t *red_out, uint16_t *ir_out)
{
    int32_t f_red, f_ir;
    
    /* 1. 简单低通滤波滤除高频噪声 */
    spo2_simple_iir_filter(red, ir, &f_red, &f_ir);
    
    if (red_out) *red_out = (uint16_t)f_red;
    if (ir_out)  *ir_out  = (uint16_t)f_ir;

    /* 2. 存入环形缓冲区参与后续计算 */
    red_wave_buf[wave_head] = f_red;
    ir_wave_buf[wave_head]  = f_ir;
    
    /* 3. 更新写入指针与有效长度 */
    wave_head = (wave_head + 1) % WAVE_BUF_SIZE;
    if(wave_len < WAVE_BUF_SIZE) {
        wave_len++;
    }
    
    /* 3. 每累积 500 个新点（对应 100Hz 下的 5 秒），触发一次滑动窗口计算 */
    wave_process_cnt++;
    if(wave_len >= SPO2_ALGO_WINDOW_SIZE && wave_process_cnt >= SPO2_ALGO_WINDOW_SIZE)
    {
        g_calc_ready = 1;
        wave_process_cnt = 0;
    }
}

/**
 * @brief 核心算法处理：在最近 5 秒窗口中提取心跳特征、交直流分量并计算 R 值和血氧
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

    if(spo2_is_dropout_by_ac(ir_wave_buf, wave_head, SPO2_ALGO_WINDOW_SIZE))
    {
        g_abnormal_segment_flag = 1;
        return;
    }

    /* --- 步骤 1：寻找心率周期 (使用红外光 IR) --- */
    hr_val = spo2_calc_hr_period(ir_wave_buf, wave_head, SPO2_ALGO_WINDOW_SIZE, &period);

    if(hr_val > 0 && period > 0)
    {
        /* --- 步骤 2：提取局部特征 --- */
        spo2_find_minmax_in_period(red_wave_buf, ir_wave_buf, wave_head, period, &r_max, &r_min, &i_max, &i_min);
        
        /* --- 步骤 3：交直流分离 (AC/DC Extraction) --- */
        ac_red = r_max - r_min;
        dc_red = (r_max + r_min) / 2;
        ac_ir  = i_max - i_min;
        dc_ir  = (i_max + i_min) / 2;

        /* 确保分母不为 0 */
        if(ac_ir > 0 && dc_red > 0 && dc_ir > 0)
        {
            /* --- 步骤 4：计算特征比值 R --- */
            r_scaled = (ac_red * dc_ir * 1000) / (ac_ir * dc_red);
            
            /* --- 步骤 5：计算血氧 SpO2 --- */
            if (r_scaled >= 400 && r_scaled <= 1500) {
                spo2_calc = 110 - (25 * r_scaled) / 1000;
            } else {
                spo2_calc = 0;
            }
            
            if(spo2_calc > 100) spo2_calc = 100;
            if(spo2_calc < 70 && spo2_calc != 0) spo2_calc = 70;
            spo2_val = (uint8_t)spo2_calc;

            /* --- 步骤 6：计算灌注指数 PI --- */
            pi_ir = (uint8_t)((ac_ir * 100) / dc_ir);
            pi_red = (uint8_t)((ac_red * 100) / dc_red);
            if(pi_ir > 99) pi_ir = 99;
            if(pi_red > 99) pi_red = 99;
            
            /* --- 步骤 7：历史平滑更新 --- */
            spo2_update_history(spo2_val, hr_val, pi_ir, pi_red, (uint16_t)r_scaled);
        }
    }
    else
    {
        /* 未检测到心跳，将数值清零 */
        g_result.spo2 = 0;
        g_result.heart_rate = 0;
        
        /* 信号断开，清空滑动平均历史队列 */
        s_hist_count = 0; 
        hr_hist_count = 0;
    }
    
    /* 强制数据链路通畅：无论结果如何，每 5 秒都触发一次更新 */
    g_result.updated = 1;
}

/**
 * @brief 获取算法结果
 * @return 1=有新结果, 0=暂无更新
 */
uint8_t SPO2_Algo_GetResult(uint8_t *spo2, uint8_t *hr, uint8_t *pi_ir, uint8_t *pi_red, float *r_val, int32_t *ac_val)
{
    if(g_result.updated)
    {
        if(spo2)   *spo2 = g_result.spo2;
        if(hr)     *hr = g_result.heart_rate;
        if(pi_ir)  *pi_ir = g_result.pi_ir;
        if(pi_red) *pi_red = g_result.pi_red;
        if(r_val)  *r_val = g_result.r_val;
        if(ac_val) *ac_val = g_last_ac;
        
        g_result.updated = 0;
        return 1;
    }
    return 0;
}

/* Demo 滤波复位 */
void SPO2_Algo_DemoFilterReset(void)
{
    s_demo_iir_y = 0;
    s_demo_drift_baseline = 0;
    s_demo_med_buf[0] = s_demo_med_buf[1] = s_demo_med_buf[2] = 0;
    s_demo_med_idx = 0;
    s_demo_inited = 0;
}

/* Demo 滤波应用：根据不同模式对原始 IR 做实时滤波（只用于 UI 演示，不影响算法主链路） */
uint16_t SPO2_Algo_ApplyDemoFilter(uint16_t raw_ir, uint8_t mode)
{
    int32_t y;
    int32_t x = (int32_t)raw_ir << 8; /* Q8 放大 */
    
    if(!s_demo_inited)
    {
        s_demo_iir_y = (2048 << 8);
        s_demo_drift_baseline = x;
        s_demo_med_buf[0] = raw_ir;
        s_demo_med_buf[1] = raw_ir;
        s_demo_med_buf[2] = raw_ir;
        s_demo_inited = 1;
    }
    
    switch(mode)
    {
        case 0:
            s_demo_drift_baseline = s_demo_drift_baseline + ((x - s_demo_drift_baseline) >> DEMO_IIR_BASELINE_SHIFT);
            {
                int32_t hp = x - s_demo_drift_baseline + (2048 << 8);
                s_demo_iir_y = s_demo_iir_y + ((hp - s_demo_iir_y) >> DEMO_IIR_SHIFT);
                y = s_demo_iir_y >> 8;
            }
            break;
        case 1:
            s_demo_drift_baseline = s_demo_drift_baseline + ((x - s_demo_drift_baseline) >> DEMO_BASELINE_SHIFT);
            y = (x - s_demo_drift_baseline + (2048 << 8)) >> 8;
            break;
        default:
        {
            uint16_t a,b,c,med;
            s_demo_med_buf[s_demo_med_idx] = (uint16_t)(x >> 8);
            s_demo_med_idx = (uint8_t)((s_demo_med_idx + 1) % 3);
            a = s_demo_med_buf[0]; b = s_demo_med_buf[1]; c = s_demo_med_buf[2];
            if ((a >= b && a <= c) || (a <= b && a >= c)) med = a;
            else if ((b >= a && b <= c) || (b <= a && b >= c)) med = b;
            else med = c;
            y = med;
            break;
        }
    }
    if(y < 0) y = 0;
    if(y > 4095) y = 4095;
    return (uint16_t)y;
}
uint8_t SPO2_Algo_PopAnomalyFlag(void)
{
    uint8_t ret = g_abnormal_segment_flag;
    g_abnormal_segment_flag = 0;
    return ret;
}

/* =====================================================================
 * ======================= 内部功能子函数实现 ==========================
 * ===================================================================== */

/**
 * @brief 周期法：计算脉率，取最近 3 个周期的平均值 (使用红外光 IR)
 * @param ir_buf 红外信号环形缓冲区
 * @param head 缓冲区当前写入头指针
 * @param len 计算窗口长度 (500点=5秒)
 * @param period 传出参数：平均心跳周期（采样点数）
 * @return 有效心率值(bpm)，0=信号无效/未检测到
 */
static uint8_t spo2_calc_hr_period(const int32_t *ir_buf, uint16_t head, uint16_t len, uint16_t *period)
{
    uint16_t i;
    uint16_t write_idx;
    int32_t max_v = -1000000, min_v = 1000000;
    int32_t ac_amplitude, thr_up, thr_down, prom_min;
    uint16_t peak_indices[20];
    uint16_t peak_cnt = 0;
    uint16_t last_peak_pos = 0xFFFF;
    const uint16_t refractory_points = (SPO2_SAMPLE_RATE / 4); /* 约250ms，避免二次峰/重搏波 */
    uint8_t hr_result = 0;

    if(len < 3 || len > WAVE_BUF_SIZE) {
        *period = 0;
        return 0;
    }

    write_idx = (uint16_t)((head + WAVE_BUF_SIZE - len) % WAVE_BUF_SIZE);
    for(i = 0; i < len; i++) {
        int32_t curr = ir_buf[(write_idx + i) % WAVE_BUF_SIZE];
        if(curr > max_v) max_v = curr;
        if(curr < min_v) min_v = curr;
    }

    ac_amplitude = max_v - min_v;
    g_last_ac = ac_amplitude; /* 记录 AC 摆幅供监控 */
    
    if(ac_amplitude < HR_SQI_THRESHOLD) {
        *period = 0;
        return 0;
    }

    /* 自适应阈值与滞回，降低误检双峰（如重搏波、平台峰） */
    thr_up   = min_v + (ac_amplitude * 6) / 10;  /* 上阈值约60% */
    thr_down = min_v + (ac_amplitude * 5) / 10;  /* 下阈值约50% */
    prom_min = (ac_amplitude * 15) / 100;        /* 最小显著性约15%幅度 */

    /* 状态机峰值检测：带滞回与显著性、以及不应期约束 */
    {
        uint8_t above = 0;
        uint16_t candidate_pos = 0;
        int32_t  candidate_val = -1000000;
        int32_t  valley_val = 1000000; /* 上一个峰后的最近谷值，用于显著性判定 */

        for(i = 1; i < len - 1; i++) {
            int32_t prev = ir_buf[(write_idx + i - 1) % WAVE_BUF_SIZE];
            int32_t curr = ir_buf[(write_idx + i) % WAVE_BUF_SIZE];
            int32_t next = ir_buf[(write_idx + i + 1) % WAVE_BUF_SIZE];

            if(!above) {
                if(curr < valley_val) valley_val = curr;
                if(curr >= thr_up && curr >= prev && curr >= next) {
                    above = 1;
                    candidate_pos = i;
                    candidate_val = curr;
                }
            } else {
                if(curr > candidate_val) {
                    candidate_val = curr;
                    candidate_pos = i;
                }
                /* 离开峰值区域：跌破下阈值时确认一次峰 */
                if(curr < thr_down) {
                    if((candidate_val - valley_val) >= prom_min) {
                        if(last_peak_pos == 0xFFFF || (uint16_t)(candidate_pos - last_peak_pos) > refractory_points) {
                            if(peak_cnt < 20) {
                                peak_indices[peak_cnt++] = candidate_pos;
                                last_peak_pos = candidate_pos;
                            }
                        }
                    }
                    above = 0;
                    valley_val = curr;      /* 重置谷值跟踪 */
                    candidate_val = -1000000;
                }
            }
        }
    }

    if (peak_cnt >= 2) {
        /* 采用最近3个周期的稳健估计：若>=3个间隔取后三个的中位数，否则取平均 */
        uint16_t intervals[19];
        uint16_t interval_cnt = 0;
        for(i = 1; i < peak_cnt; i++) {
            intervals[interval_cnt++] = (uint16_t)(peak_indices[i] - peak_indices[i-1]);
        }
        if(interval_cnt >= 3) {
            uint16_t a = intervals[interval_cnt - 3];
            uint16_t b = intervals[interval_cnt - 2];
            uint16_t c = intervals[interval_cnt - 1];
            uint16_t med;
            if ((a >= b && a <= c) || (a <= b && a >= c)) med = a;
            else if ((b >= a && b <= c) || (b <= a && b >= c)) med = b;
            else med = c;
            *period = med;
        } else {
            uint32_t sum_intervals = 0;
            for(i = 0; i < interval_cnt; i++) sum_intervals += intervals[i];
            *period = (interval_cnt > 0) ? (uint16_t)(sum_intervals / interval_cnt) : 0;
        }
        if(*period > 0) {
            hr_result = (uint8_t)(6000 / *period);
            return hr_result;
        }
    }

    *period = 0;
    return 0;
}

static uint8_t spo2_is_dropout_by_ac(const int32_t *ir_buf, uint16_t head, uint16_t len)
{
    uint16_t i;
    uint16_t write_idx;
    int32_t curr;
    int32_t max_v = -1000000;
    int32_t min_v = 1000000;
    int32_t ac;

    if(len < 3 || len > WAVE_BUF_SIZE) return 0;

    write_idx = (uint16_t)((head + WAVE_BUF_SIZE - len) % WAVE_BUF_SIZE);
    for(i = 0; i < len; i++)
    {
        curr = ir_buf[(write_idx + i) % WAVE_BUF_SIZE];
        if(curr > max_v) max_v = curr;
        if(curr < min_v) min_v = curr;
    }
    ac = max_v - min_v;
    g_last_ac = ac;
    return (ac > SPO2_DROPOUT_AC_THRESHOLD) ? 1 : 0;
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

/**
 * @brief 简单一阶 IIR 低通滤波器
 *        用于滤除原始 ADC 信号中的高频毛刺噪声，提升波形显示和平滑计算质量
 *        Y[n] = 0.125 * X[n] + 0.875 * Y[n-1]
 *        截止频率约 fc ≈ 100Hz * (1-a) / (2π) ≈ 12Hz，可通过 50Hz/60Hz 工频干扰
 */
static void spo2_simple_iir_filter(uint16_t red_in, uint16_t ir_in, int32_t *red_out, int32_t *ir_out)
{
    if (s_red_filter_out == 0 && s_ir_filter_out == 0) {
        // 初始状态，直接赋值
        s_red_filter_out = (int32_t)red_in << 8;
        s_ir_filter_out = (int32_t)ir_in << 8;
    } else {
        // y(n) = a * x(n) + (1-a) * y(n-1)
        // 取 a = 0.125，系数更轻，减少信号延迟，避免心率计算偏大
        // 为保证整数精度：y(n) = y(n-1) + ( (x<<8) - y(n-1) ) >> 3
        int32_t red_scaled = (int32_t)red_in << 8;
        int32_t ir_scaled = (int32_t)ir_in << 8;
        
        s_red_filter_out = s_red_filter_out + ((red_scaled - s_red_filter_out) >> 3);
        s_ir_filter_out = s_ir_filter_out + ((ir_scaled - s_ir_filter_out) >> 3);
    }
    
    *red_out = s_red_filter_out >> 8;
    *ir_out = s_ir_filter_out >> 8;
}
