#include "UI_Manager.h"
#include "LCD.h"
#include "UART1.h"
#include <stdio.h>

// UI Colors (RGB565)
#define UI_COLOR_BG         BLACK
#define UI_COLOR_PANEL_BG   0x1082  // Dark Gray-Blue
#define UI_COLOR_HEADER_BG  0x2124  // Dark Slate
#define UI_COLOR_TEXT_W     WHITE
#define UI_COLOR_SPO2       CYAN
#define UI_COLOR_PR         GREEN
#define UI_COLOR_PI         YELLOW
#define UI_COLOR_SELECT     0x001F  // Blue
#define UI_COLOR_BORDER     0x4208  // Gray

static UI_State_t s_uiState = UI_STATE_MAIN;
static uint8_t s_settingsIndex = 0;
static uint8_t s_needRefresh = 1;
static uint8_t s_autoLightEnable = 0;
static SPO2Data_t s_curData = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // 缓存当前数据

#define SETTINGS_ITEM_COUNT 9
static const char* s_settingsItems[SETTINGS_ITEM_COUNT] = {
    "Work Mode",
    "Alarm Set",
    "SpO2 Set",
    "Filter Set",
    "R Calib",
    "Auto Light",
    "EtCO2 Set",
    "System Set",
    "Data Review"
};

void UI_DrawMainScreen(void);
void UI_DrawSettingsScreen(void);
void UI_DrawSpO2SetScreen(void);
void UI_DrawFilterSetScreen(void);
void UI_DrawRCalibScreen(void);
void UI_DrawAutoLightScreen(void);
static void UI_DrawSettingsItem(uint8_t index, uint8_t selected);
static void UI_UpdateGainValue(void);
static void UI_UpdateFilterStatus(void);
static void UI_UpdateRCalibValue(void);
static void UI_UpdateAutoLightStatus(void);

void UI_Init(void)
{
    LCD_Clear(UI_COLOR_BG);
    s_uiState = UI_STATE_MAIN;
    s_needRefresh = 1;
    UI_Update();
}

void UI_UpdateData(SPO2Data_t *data)
{
    char buf[20];
    if(data == NULL) return;

    if(data->pwm_red == 0 && s_curData.pwm_red != 0) data->pwm_red = s_curData.pwm_red;
    if(data->pwm_ir == 0 && s_curData.pwm_ir != 0) data->pwm_ir = s_curData.pwm_ir;
    if(data->gain_level == 0xFF && s_curData.gain_level != 0xFF) data->gain_level = s_curData.gain_level;
    
    // 如果在主界面，仅更新数值区域，避免全屏刷新导致闪烁
    if(s_uiState == UI_STATE_MAIN)
    {
        uint8_t need_update_main = 0;
        if(s_curData.spo2 != data->spo2) need_update_main = 1;
        if(s_curData.heart_rate != data->heart_rate) need_update_main = 1;
        if(s_curData.pi != data->pi) need_update_main = 1;
        if(s_curData.status != data->status) need_update_main = 1;
        if(s_curData.filter_status != data->filter_status) need_update_main = 1;
        if(s_curData.pwm_red != data->pwm_red) need_update_main = 1;
        if(s_curData.pwm_ir != data->pwm_ir) need_update_main = 1;
        if(s_curData.gain_level != data->gain_level) need_update_main = 1;

        if(!need_update_main)
        {
            s_curData = *data;
            return;
        }

        // 1. Update SpO2
        {
            POINT_COLOR = UI_COLOR_SPO2;
            BACK_COLOR = UI_COLOR_BG;
            if(data->spo2 == 0) sprintf(buf, "SPO2: --  ");
            else sprintf(buf, "SPO2: %3d ", data->spo2);
            LCD_ShowString(10, 60, 200, 16, 16, (u8*)buf);
        }
        
        // 2. Update PR
        {
            POINT_COLOR = UI_COLOR_PR;
            BACK_COLOR = UI_COLOR_BG;
            if(data->heart_rate == 0) sprintf(buf, "PR:   --  ");
            else sprintf(buf, "PR:  %3d  ", data->heart_rate);
            LCD_ShowString(10, 85, 200, 16, 16, (u8*)buf);
        }
        
        // 3. Update PI
        {
            POINT_COLOR = UI_COLOR_PI;
            BACK_COLOR = UI_COLOR_BG;
            if(data->pi == 0 && data->status == 0) sprintf(buf, "PI R:-- I:-- ");
            else sprintf(buf, "PI R:%2d I:%2d ", data->status, data->pi);
            LCD_ShowString(10, 110, 220, 16, 16, (u8*)buf);
        }
        
        // 4. Update R Value (Footer)
        {
            POINT_COLOR = YELLOW;
            BACK_COLOR = UI_COLOR_BG;
            sprintf(buf, "R: %d    ", data->filter_status);
            LCD_ShowString(10, 135, 200, 16, 16, (u8*)buf);
        }
        
        {
            POINT_COLOR = GRAY;
            BACK_COLOR = UI_COLOR_BG;
            sprintf(buf, "PWM R:%d I:%d   ", data->pwm_red, data->pwm_ir);
            LCD_ShowString(10, 160, 200, 16, 16, (u8*)buf);
        }
        
        {
            POINT_COLOR = GRAY;
            BACK_COLOR = UI_COLOR_BG;
            if(data->gain_level == 0xFF) sprintf(buf, "Gain: --  ");
            else sprintf(buf, "Gain: %d   ", data->gain_level);
            LCD_ShowString(10, 185, 200, 16, 16, (u8*)buf);
        }

        s_curData = *data;
    }
    else
    {
        // 其他界面全屏刷新 -> 优化：非主界面不自动刷新，防止菜单操作时被数据更新打断
        /*
        if(s_curData.spo2 != data->spo2 || 
           s_curData.heart_rate != data->heart_rate || 
           s_curData.pi != data->pi ||
           s_curData.filter_status != data->filter_status ||
           s_curData.gain_level != data->gain_level)
        {
            s_curData = *data;
            s_needRefresh = 1;
        }
        */
        // 仅静默更新数据，不触发重绘
        s_curData = *data;
        if(s_uiState == UI_STATE_R_CALIB)
        {
            UI_UpdateRCalibValue();
        }
    }
}

void UI_UpdatePwm(uint8_t pwm_red, uint8_t pwm_ir)
{
    char buf[20];
    if(pwm_red == 0 && pwm_ir == 0) return;
    s_curData.pwm_red = pwm_red;
    s_curData.pwm_ir = pwm_ir;

    if(s_uiState == UI_STATE_MAIN)
    {
        POINT_COLOR = GRAY;
        BACK_COLOR = UI_COLOR_BG;
        sprintf(buf, "PWM R:%d I:%d   ", s_curData.pwm_red, s_curData.pwm_ir);
        LCD_ShowString(10, 160, 200, 16, 16, (u8*)buf);
    }
    else if(s_uiState == UI_STATE_R_CALIB)
    {
        UI_UpdateRCalibValue();
    }
}

void UI_UpdateGain(uint8_t gain_level)
{
    char buf[20];
    s_curData.gain_level = gain_level;

    if(s_uiState == UI_STATE_MAIN)
    {
        POINT_COLOR = GRAY;
        BACK_COLOR = UI_COLOR_BG;
        sprintf(buf, "Gain: %d   ", s_curData.gain_level);
        LCD_ShowString(10, 185, 200, 16, 16, (u8*)buf);
    }
    else if(s_uiState == UI_STATE_SPO2_SET)
    {
        UI_UpdateGainValue();
    }
}

void UI_Update(void)
{
    if(!s_needRefresh) return;

    if(s_uiState == UI_STATE_MAIN)
    {
        UI_DrawMainScreen();
    }
    else if(s_uiState == UI_STATE_SETTINGS)
    {
        UI_DrawSettingsScreen();
    }
    else if(s_uiState == UI_STATE_SPO2_SET)
    {
        UI_DrawSpO2SetScreen();
    }
    else if(s_uiState == UI_STATE_FILTER_SET)
    {
        UI_DrawFilterSetScreen();
    }
    else if(s_uiState == UI_STATE_R_CALIB)
    {
        UI_DrawRCalibScreen();
    }
    else if(s_uiState == UI_STATE_AUTO_LIGHT)
    {
        UI_DrawAutoLightScreen();
    }
    
    s_needRefresh = 0;
}

void UI_Process(void)
{
    UI_Update();
}

// 辅助函数：绘制圆角矩形风格的面板
void UI_DrawPanel(u16 x, u16 y, u16 w, u16 h, u16 color, char* title, u16 titleColor)
{
    // 绘制边框和背景
    LCD_Fill(x, y, x+w, y+h, UI_COLOR_PANEL_BG);
    POINT_COLOR = color;
    LCD_DrawRectangle(x, y, x+w, y+h);
    LCD_DrawRectangle(x+1, y+1, x+w-1, y+h-1); // 加粗边框
    
    // 绘制标题背景
    LCD_Fill(x+2, y+2, x+w-2, y+24, color);
    
    // 绘制标题文字
    POINT_COLOR = titleColor; // 通常是黑色或白色，取决于背景
    BACK_COLOR = color;       // 设置背景色为标题栏颜色
    LCD_ShowString(x+10, y+5, w-20, 16, 16, (u8*)title);
    BACK_COLOR = UI_COLOR_BG; // 恢复默认背景色
}

void UI_DrawHeader(char* title)
{
    LCD_Fill(0, 0, 240, 30, UI_COLOR_HEADER_BG);
    
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_HEADER_BG;
    // 左侧标题
    LCD_ShowString(10, 7, 200, 16, 16, (u8*)title);
    
    // 右侧时间/ID (模拟)
    LCD_ShowString(130, 7, 200, 16, 16, (u8*)"ID:01 12:00");
    BACK_COLOR = UI_COLOR_BG;
    
    // 分割线
    POINT_COLOR = UI_COLOR_SPO2;
    LCD_DrawLine(0, 30, 240, 30);
    LCD_DrawLine(0, 31, 240, 31);
}

void UI_DrawMainScreen(void)
{
    char buf[20];
    
    LCD_Clear(UI_COLOR_BG);
    BACK_COLOR = UI_COLOR_BG; // 确保清除屏幕后背景色一致
    UI_DrawHeader("Monitor");

    POINT_COLOR = UI_COLOR_SPO2;
    BACK_COLOR = UI_COLOR_BG;
    if(s_curData.spo2 == 0) sprintf(buf, "SPO2: --  ");
    else sprintf(buf, "SPO2: %3d ", s_curData.spo2);
    LCD_ShowString(10, 60, 200, 16, 16, (u8*)buf);
    
    POINT_COLOR = UI_COLOR_PR;
    BACK_COLOR = UI_COLOR_BG;
    if(s_curData.heart_rate == 0) sprintf(buf, "PR:   --  ");
    else sprintf(buf, "PR:  %3d  ", s_curData.heart_rate);
    LCD_ShowString(10, 85, 200, 16, 16, (u8*)buf);
    
    POINT_COLOR = UI_COLOR_PI;
    BACK_COLOR = UI_COLOR_BG;
    if(s_curData.pi == 0 && s_curData.status == 0) sprintf(buf, "PI R:-- I:-- ");
    else sprintf(buf, "PI R:%2d I:%2d ", s_curData.status, s_curData.pi);
    LCD_ShowString(10, 110, 220, 16, 16, (u8*)buf);
    
    POINT_COLOR = YELLOW;
    BACK_COLOR = UI_COLOR_BG;
    sprintf(buf, "R: %d    ", s_curData.filter_status);
    LCD_ShowString(10, 135, 200, 16, 16, (u8*)buf);
    
    POINT_COLOR = GRAY;
    BACK_COLOR = UI_COLOR_BG;
    sprintf(buf, "PWM R:%d I:%d   ", s_curData.pwm_red, s_curData.pwm_ir);
    LCD_ShowString(10, 160, 200, 16, 16, (u8*)buf);

    POINT_COLOR = GRAY;
    BACK_COLOR = UI_COLOR_BG;
    if(s_curData.gain_level == 0xFF) sprintf(buf, "Gain: --  ");
    else sprintf(buf, "Gain: %d   ", s_curData.gain_level);
    LCD_ShowString(10, 185, 200, 16, 16, (u8*)buf);
    
    // Footer
    POINT_COLOR = GRAY;
    BACK_COLOR = UI_COLOR_BG;
    LCD_ShowString(60, 290, 200, 16, 16, (u8*)"Press SET to Menu");
}

void UI_DrawSettingsScreen(void)
{
    uint8_t i;
    u16 original_back_color = BACK_COLOR; // 保存原始背景色

    LCD_Clear(UI_COLOR_BG);
    UI_DrawHeader("Settings");
    
    for(i = 0; i < SETTINGS_ITEM_COUNT; i++)
    {
        UI_DrawSettingsItem(i, (i == s_settingsIndex) ? 1 : 0);
    }
    
    // Footer
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG; // 恢复背景色
    LCD_ShowString(10, 300, 200, 16, 16, (u8*)"UP/DN: Select  SET: Enter");
    
    BACK_COLOR = original_back_color; // 恢复原始背景色
}

static void UI_DrawSettingsItem(uint8_t index, uint8_t selected)
{
    u16 y_pos = 50 + index*35;
    if(selected)
    {
        LCD_Fill(10, y_pos - 5, 230, y_pos + 25, UI_COLOR_SELECT);
        POINT_COLOR = WHITE;
        BACK_COLOR = UI_COLOR_SELECT;
        LCD_DrawRectangle(10, y_pos - 5, 230, y_pos + 25);
    }
    else
    {
        LCD_Fill(10, y_pos - 5, 230, y_pos + 25, UI_COLOR_PANEL_BG);
        POINT_COLOR = GRAY;
        BACK_COLOR = UI_COLOR_PANEL_BG;
        LCD_DrawRectangle(10, y_pos - 5, 230, y_pos + 25);
    }
    LCD_ShowString(20, y_pos, 200, 16, 16, (u8*)s_settingsItems[index]);
    if(selected)
    {
        LCD_ShowString(210, y_pos, 20, 16, 16, (u8*)">");
    }
    else
    {
        POINT_COLOR = GRAY;
        BACK_COLOR = UI_COLOR_PANEL_BG;
        LCD_ShowString(210, y_pos, 20, 16, 16, (u8*)" ");
    }
}

void UI_DrawSpO2SetScreen(void)
{
    char buf[32];
    LCD_Clear(UI_COLOR_BG);
    BACK_COLOR = UI_COLOR_BG; // 确保清除屏幕后背景色一致
    UI_DrawHeader("SpO2 Gain Set");
    
    // Gain Control Panel
    UI_DrawPanel(40, 80, 160, 120, UI_COLOR_SPO2, "Gain Level", BLACK);
    
    // 显示增益值
    UI_UpdateGainValue();
    
    // Instructions
    POINT_COLOR = GREEN;
    LCD_ShowString(60, 160, 120, 16, 16, (u8*)"UP: Increase");
    POINT_COLOR = RED;
    LCD_ShowString(60, 180, 120, 16, 16, (u8*)"DN: Decrease");
    
    // Footer
    POINT_COLOR = GRAY;
    LCD_ShowString(60, 290, 200, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateGainValue(void)
{
    char buf[32];
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    sprintf(buf, "Current Level: %d   ", s_curData.gain_level);
    LCD_ShowString(60, 130, 160, 16, 16, (u8*)buf);
}

void UI_DrawFilterSetScreen(void)
{
    LCD_Clear(UI_COLOR_BG);
    BACK_COLOR = UI_COLOR_BG; // 确保清除屏幕后背景色一致
    UI_DrawHeader("Filter Set");
    
    // Filter Control Panel
    UI_DrawPanel(40, 80, 160, 120, UI_COLOR_SPO2, "Filter Switch", BLACK);
    
    // 显示状态
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    
    UI_UpdateFilterStatus();
    
    // Instructions
    POINT_COLOR = GREEN;
    LCD_ShowString(60, 160, 200, 16, 16, (u8*)"UP: Enable (ON)");
    POINT_COLOR = RED;
    LCD_ShowString(60, 180, 200, 16, 16, (u8*)"DN: Disable(OFF)");
    
    // Footer
    POINT_COLOR = GRAY;
    LCD_ShowString(60, 290, 200, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateFilterStatus(void)
{
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    if(UART1_GetWaveFilterEnable())
        LCD_ShowString(60, 130, 160, 16, 16, (u8*)"Current: ON  ");
    else
        LCD_ShowString(60, 130, 160, 16, 16, (u8*)"Current: OFF ");
}

void UI_DrawRCalibScreen(void)
{
    LCD_Clear(UI_COLOR_BG);
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("R Calib");
    
    UI_DrawPanel(30, 80, 180, 120, UI_COLOR_SPO2, "R Value", BLACK);
    UI_UpdateRCalibValue();
    
    POINT_COLOR = GRAY;
    LCD_ShowString(60, 290, 200, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateRCalibValue(void)
{
    char buf[32];
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    sprintf(buf, "R: %d    ", s_curData.filter_status);
    LCD_ShowString(80, 130, 120, 16, 16, (u8*)buf);
    sprintf(buf, "PWM R:%d I:%d ", s_curData.pwm_red, s_curData.pwm_ir);
    LCD_ShowString(50, 155, 160, 16, 16, (u8*)buf);
}

void UI_DrawAutoLightScreen(void)
{
    LCD_Clear(UI_COLOR_BG);
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("Auto Light");
    
    UI_DrawPanel(40, 80, 160, 120, UI_COLOR_SPO2, "Light Switch", BLACK);
    UI_UpdateAutoLightStatus();
    
    POINT_COLOR = GREEN;
    LCD_ShowString(60, 160, 200, 16, 16, (u8*)"UP: Enable (ON)");
    POINT_COLOR = RED;
    LCD_ShowString(60, 180, 200, 16, 16, (u8*)"DN: Disable(OFF)");
    
    POINT_COLOR = GRAY;
    LCD_ShowString(60, 290, 200, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateAutoLightStatus(void)
{
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    if(s_autoLightEnable)
        LCD_ShowString(60, 130, 160, 16, 16, (u8*)"Current: ON  ");
    else
        LCD_ShowString(60, 130, 160, 16, 16, (u8*)"Current: OFF ");
}

void UI_OnKeyLL(void) // Up
{
    if(s_uiState == UI_STATE_SETTINGS)
    {
        if(s_settingsIndex > 0)
        {
            uint8_t prev = s_settingsIndex;
            s_settingsIndex--;
            UI_DrawSettingsItem(prev, 0);
            UI_DrawSettingsItem(s_settingsIndex, 1);
        }
    }
    else if(s_uiState == UI_STATE_SPO2_SET)
    {
        // 增益调整逻辑...
        // 发送命令给 F310
        UART1_SendCmd(0x02, 0); // Gain ++
        s_curData.gain_level++;
        UI_UpdateGainValue();
    }
    else if(s_uiState == UI_STATE_FILTER_SET)
    {
        UART1_SetWaveFilterEnable(1);
        UI_UpdateFilterStatus();
    }
    else if(s_uiState == UI_STATE_AUTO_LIGHT)
    {
        s_autoLightEnable = 1;
        UART1_SendCmd(0x05, 1);
        UI_UpdateAutoLightStatus();
    }
}

void UI_OnKeyRL(void) // Down
{
    if(s_uiState == UI_STATE_SETTINGS)
    {
        if(s_settingsIndex < SETTINGS_ITEM_COUNT - 1)
        {
            uint8_t prev = s_settingsIndex;
            s_settingsIndex++;
            UI_DrawSettingsItem(prev, 0);
            UI_DrawSettingsItem(s_settingsIndex, 1);
        }
    }
    else if(s_uiState == UI_STATE_SPO2_SET)
    {
        UART1_SendCmd(0x03, 0); // Gain --
        if(s_curData.gain_level > 0) s_curData.gain_level--;
        UI_UpdateGainValue();
    }
    else if(s_uiState == UI_STATE_FILTER_SET)
    {
        UART1_SetWaveFilterEnable(0);
        UI_UpdateFilterStatus();
    }
    else if(s_uiState == UI_STATE_AUTO_LIGHT)
    {
        s_autoLightEnable = 0;
        UART1_SendCmd(0x05, 0);
        UI_UpdateAutoLightStatus();
    }
}

void UI_OnKeyLH(void) // Enter / Settings
{
    if(s_uiState == UI_STATE_MAIN)
    {
        s_uiState = UI_STATE_SETTINGS;
        s_settingsIndex = 0;
        s_needRefresh = 1;
    }
    else if(s_uiState == UI_STATE_SETTINGS)
    {
        // Enter sub-menu
        if(s_settingsIndex == 2) // SpO2 Set
        {
            s_uiState = UI_STATE_SPO2_SET;
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 3) // Filter Set
        {
            s_uiState = UI_STATE_FILTER_SET;
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 4) // R Calib
        {
            s_uiState = UI_STATE_R_CALIB;
            UART1_SetRCalibMode(1);
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 5) // Auto Light
        {
            s_uiState = UI_STATE_AUTO_LIGHT;
            s_needRefresh = 1;
        }
    }
    UI_Update();
}

void UI_OnKeyRH(void) // Back
{
    if(s_uiState == UI_STATE_SETTINGS)
    {
        s_uiState = UI_STATE_MAIN;
        s_needRefresh = 1;
    }
    else if(s_uiState == UI_STATE_SPO2_SET)
    {
        s_uiState = UI_STATE_SETTINGS;
        s_needRefresh = 1;
    }
    else if(s_uiState == UI_STATE_FILTER_SET)
    {
        s_uiState = UI_STATE_SETTINGS;
        s_needRefresh = 1;
    }
    else if(s_uiState == UI_STATE_R_CALIB)
    {
        UART1_SetRCalibMode(0);
        s_uiState = UI_STATE_SETTINGS;
        s_needRefresh = 1;
    }
    else if(s_uiState == UI_STATE_AUTO_LIGHT)
    {
        s_uiState = UI_STATE_SETTINGS;
        s_needRefresh = 1;
    }
    UI_Update();
}

void UI_OnKeyMenu(void)
{
    // Reserved
}
