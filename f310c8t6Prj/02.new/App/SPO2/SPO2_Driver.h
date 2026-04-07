#ifndef __SPO2_DRIVER_H__
#define __SPO2_DRIVER_H__

#include "gd32f3x0.h"

// Pin Definitions
#define SPO2_SIGN_PIN   GPIO_PIN_1   // PA1 (ADC_IN1)
#define SPO2_SIGN_PORT  GPIOA

#define SPO2_GAIN_A_PIN GPIO_PIN_2   // PA2
#define SPO2_GAIN_B_PIN GPIO_PIN_3   // PA3
#define SPO2_GAIN_C_PIN GPIO_PIN_4   // PA4
#define SPO2_GAIN_PORT  GPIOA

#define SPO2_RED_CS_PIN    GPIO_PIN_13  // PC13
#define SPO2_RED_CS_PORT   GPIOC

#define SPO2_IR_CS_PIN     GPIO_PIN_14  // PC14
#define SPO2_IR_CS_PORT    GPIOC

// ADC Channel
#define SPO2_ADC_CHANNEL ADC_CHANNEL_1

// Algorithm Parameters
#define FIR_ORDER 4
#define WAVE_BUF_SIZE 100 // 1s * 100Hz

// Gain Levels
#define SPO2_GAIN_LEVEL_0 0x00U // Minimum gain
#define SPO2_GAIN_LEVEL_1 0x01U
#define SPO2_GAIN_LEVEL_2 0x02U
#define SPO2_GAIN_LEVEL_3 0x03U
#define SPO2_GAIN_LEVEL_4 0x04U
#define SPO2_GAIN_LEVEL_5 0x05U
#define SPO2_GAIN_LEVEL_6 0x06U
#define SPO2_GAIN_LEVEL_7 0x07U // Maximum gain
#define SPO2_GAIN_LEVEL_MAX SPO2_GAIN_LEVEL_7

typedef struct {
    uint8_t spo2;
    uint8_t heart_rate;
    uint8_t pi;
    float r_val;   // Added R value
    uint8_t updated;
} SPO2_Result_t;

void SPO2_Driver_Init(void);

// Call this function in Timer Interrupt (every 0.5ms)
void SPO2_Timer_Handler(void);

// Get raw ADC values for debugging
void SPO2_GetRawADC(uint16_t *red, uint16_t *ir);

void SPO2_SetGain(uint8_t gain_code);
uint8_t SPO2_GetGain(void);

void SPO2_SetAGCEnable(uint8_t enable);
uint8_t SPO2_GetAGCEnable(void);

void SPO2_GetWaveSeq(uint16_t *red_seq, uint16_t *ir_seq);
void SPO2_GetPwmPulse(uint8_t *red_pulse, uint8_t *ir_pulse);


#endif
