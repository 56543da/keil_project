#ifndef __SPO2_ALGO_H
#define __SPO2_ALGO_H

#include <stdint.h>

// 波形缓冲区大小 (100Hz采样，8秒数据)
#define WAVE_BUF_SIZE 800
#define R_CALIB_WAVE_BUF_SIZE 100

// 计算结果结构体
typedef struct {
    uint8_t spo2;
    uint8_t heart_rate;
    uint8_t pi_ir;
    uint8_t pi_red;
    float r_val;
    uint8_t updated;
} SPO2_Result_t;

// 初始化算法模块
void SPO2_Algo_Init(void);

// 存入新的波形数据 (Red, IR)
void SPO2_Algo_PushData(uint16_t red, uint16_t ir);
void SPO2_Algo_PushDataCalib(uint16_t red, uint16_t ir);

// 执行计算 (在主循环调用)
void SPO2_Algo_Process(void);
void SPO2_Algo_ProcessCalib(void);

// 获取结果
uint8_t SPO2_Algo_GetResult(uint8_t *spo2, uint8_t *hr, uint8_t *pi_ir, uint8_t *pi_red, float *r_val);

#endif
