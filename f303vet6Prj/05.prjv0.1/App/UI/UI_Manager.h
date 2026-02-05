#ifndef _UI_MANAGER_H_
#define _UI_MANAGER_H_

#include "gd32f30x.h"

#include "DataType.h"

typedef enum
{
    UI_STATE_MAIN = 0,
    UI_STATE_SETTINGS
} UI_State_t;

void UI_Init(void);
void UI_Update(void);
void UI_UpdateData(SPO2Data_t *data); // 新增数据更新接口
void UI_OnKeyLL(void);
void UI_OnKeyRL(void);
void UI_OnKeyLH(void);
void UI_OnKeyRH(void);
void UI_OnKeyMenu(void);

#endif
