#ifndef _UI_MANAGER_H_
#define _UI_MANAGER_H_

#include "gd32f30x.h"

#include "DataType.h"

typedef enum
{
    UI_STATE_MAIN = 0,
    UI_STATE_SETTINGS,
    UI_STATE_WORK_MODE,
    UI_STATE_ALARM_SET,
    UI_STATE_SPO2_SET,
    UI_STATE_FILTER_SET,
    UI_STATE_R_CALIB,
    UI_STATE_AUTO_LIGHT,
    UI_STATE_ETCO2_SET,
    UI_STATE_SYSTEM_SET,
    UI_STATE_DATA_REVIEW,
    UI_STATE_FILTER_DEMO,
    UI_STATE_ALARM_TEST
} UI_State_t;

void UI_Init(void);
void UI_Process(void); // 新增 UI 处理函数
void UI_Update(void);
void UI_UpdateData(SPO2Data_t *data); // 新增数据更新接口
void UI_UpdateWave(uint16_t red_raw, uint16_t ir_raw);
void UI_UpdatePwm(uint8_t pwm_red, uint8_t pwm_ir);
void UI_UpdateGain(uint8_t gain_level);
void UI_OnKeyLL(void);
void UI_OnKeyRL(void);
void UI_OnKeyLH(void);
void UI_OnKeyRH(void);
void UI_OnKeyMenu(void);

#endif
