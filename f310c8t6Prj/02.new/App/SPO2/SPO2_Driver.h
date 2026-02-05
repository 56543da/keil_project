#ifndef __SPO2_DRIVER_H__
#define __SPO2_DRIVER_H__

#include "gd32f3x0.h"

// Pin Definitions
#define SPO2_SIGN_PIN   GPIO_PIN_1   // PA1 (ADC_IN1)
#define SPO2_SIGN_PORT  GPIOA

#define SPO2_RED_CS_PIN  GPIO_PIN_13  // PC13
#define SPO2_RED_CS_PORT GPIOC

#define SPO2_IR_CS_PIN   GPIO_PIN_14  // PC14
#define SPO2_IR_CS_PORT  GPIOC

// ADC Channel
#define SPO2_ADC_CHANNEL ADC_CHANNEL_1

// Algorithm Parameters
#define FIR_ORDER 16
#define WAVE_BUF_SIZE 300 // 3s * 100Hz

typedef struct {
    uint8_t spo2;
    uint8_t heart_rate;
    uint8_t pi;
    uint8_t updated;
} SPO2_Result_t;

void SPO2_Driver_Init(void);

// Call this function in Timer Interrupt (every 0.5ms)
void SPO2_Timer_Handler(void);

// Get the latest result
uint8_t SPO2_GetResult(uint8_t *spo2, uint8_t *hr, uint8_t *pi);

#endif
