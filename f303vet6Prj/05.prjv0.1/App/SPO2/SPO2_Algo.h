#ifndef __SPO2_ALGO_H
#define __SPO2_ALGO_H

#include <stdint.h>

/* 采样与窗口参数 */
#define SPO2_SAMPLE_RATE       100  /* 100Hz 采样率 */
#define SPO2_ALGO_WINDOW_S     5    /* 算法窗口长度：5秒 */
#define SPO2_ALGO_WINDOW_SIZE  (SPO2_ALGO_WINDOW_S * SPO2_SAMPLE_RATE)
#define SPO2_RING_BUFFER_S     8    /* 环形缓冲区容量：8秒 */
#define WAVE_BUF_SIZE          (SPO2_RING_BUFFER_S * SPO2_SAMPLE_RATE)

/* 计算结果结构体 */
typedef struct {
    uint8_t spo2;
    uint8_t heart_rate;
    uint8_t pi_ir;
    uint8_t pi_red;
    float r_val;
    uint8_t updated;
} SPO2_Result_t;

/* 初始化算法模块 */
void SPO2_Algo_Init(void);

/* 存入新的波形数据，并输出一阶 IIR 滤波后的数据供 UI 和转发使用 */
void SPO2_Algo_PushData(uint16_t red, uint16_t ir, uint16_t *red_out, uint16_t *ir_out);

/* 执行血氧算法计算 (在主循环调用) */
void SPO2_Algo_Process(void);

/* 获取最新计算结果 */
uint8_t SPO2_Algo_GetResult(uint8_t *spo2, uint8_t *hr, uint8_t *pi_ir, uint8_t *pi_red, float *r_val, int32_t *ac_val);
uint8_t SPO2_Algo_PopAnomalyFlag(void);

/* Demo 滤波展示：对原始 IR 应用不同的滤波模式，返回用于 UI 演示的滤波结果
 * mode: 0 = IIR 低通（降随机噪声）
 *       1 = 去基线漂移（高通/去趋势）
 *       2 = 中值滤波（去毛刺/随机噪声）
 */
void SPO2_Algo_DemoFilterReset(void);
uint16_t SPO2_Algo_ApplyDemoFilter(uint16_t raw_ir, uint8_t mode);

#endif
