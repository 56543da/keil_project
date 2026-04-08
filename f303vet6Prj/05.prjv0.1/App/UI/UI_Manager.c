#include "UI_Manager.h"
#include "LCD.h"
#include "UART1.h"
#include <stdio.h>

// UI Colors (RGB565)
#define UI_COLOR_BG         0x0841
#define UI_COLOR_PANEL_BG   0x10A2
#define UI_COLOR_HEADER_BG  0x18E3
#define UI_COLOR_TEXT_W     WHITE
#define UI_COLOR_SPO2       0x07FF
#define UI_COLOR_PR         0x07E0
#define UI_COLOR_PI         0xFFE0
#define UI_COLOR_SELECT     0x22B8
#define UI_COLOR_BORDER     0x6B6D
#define UI_COLOR_ACCENT     0x055D
#define UI_COLOR_HINT       0xBDF7
#define UI_WAVE_X           20
#define UI_WAVE_Y           370
#define UI_WAVE_W           280
#define UI_WAVE_H           90
#define UI_WAVE_SECONDS     5
#define UI_WAVE_EXPECTED_HZ 100
#define UI_WAVE_X_STEP      2
#define UI_WAVE_POINTS      (UI_WAVE_W / UI_WAVE_X_STEP)
#define UI_WAVE_DECIM       (((UI_WAVE_EXPECTED_HZ * UI_WAVE_SECONDS) + UI_WAVE_POINTS - 1) / UI_WAVE_POINTS)

static UI_State_t s_uiState = UI_STATE_MAIN;
static uint8_t s_settingsIndex = 0;
static uint8_t s_needRefresh = 1;
static uint8_t s_autoLightEnable = 0;
static uint8_t s_workMode = 0;
static uint8_t s_alarmProfile = 1;
static uint8_t s_etco2Enable = 0;
static uint8_t s_systemBrightness = 3;
static uint8_t s_systemBeepEnable = 1;
static uint8_t s_dataReviewPage = 0;
static uint8_t s_wavePlotInited = 0;
static uint16_t s_waveXPos = 0;
static uint16_t s_wavePrevY = UI_WAVE_Y + (UI_WAVE_H / 2);
static int32_t s_waveDcQ8 = 0;
static int32_t s_waveAcQ8 = 0;
static uint8_t s_waveAcInit = 0;
static uint8_t s_waveDecimCnt = 0;
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
void UI_DrawWorkModeScreen(void);
void UI_DrawAlarmSetScreen(void);
void UI_DrawSpO2SetScreen(void);
void UI_DrawFilterSetScreen(void);
void UI_DrawRCalibScreen(void);
void UI_DrawAutoLightScreen(void);
void UI_DrawEtCO2SetScreen(void);
void UI_DrawSystemSetScreen(void);
void UI_DrawDataReviewScreen(void);
static void UI_DrawScreenBackground(void);
static void UI_DrawCard(u16 x, u16 y, u16 w, u16 h, u16 borderColor, u16 titleColor, char* title);
static void UI_DrawSettingsItem(uint8_t index, uint8_t selected);
static void UI_WaveReset(void);
static void UI_WaveTrackDcAc(uint16_t sample);
static void UI_WavePushSample(uint16_t sample);
static void UI_UpdateWorkModeStatus(void);
static void UI_UpdateAlarmSetStatus(void);
static void UI_UpdateGainValue(void);
static void UI_UpdateFilterStatus(void);
static void UI_UpdateRCalibValue(void);
static void UI_UpdateAutoLightStatus(void);
static void UI_UpdateEtCO2Status(void);
static void UI_UpdateSystemSetStatus(void);
static void UI_UpdateDataReviewValue(void);

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

        {
            POINT_COLOR = UI_COLOR_SPO2;
            BACK_COLOR = UI_COLOR_PANEL_BG;
            if(data->spo2 == 0) sprintf(buf, "SPO2: --  ");
            else sprintf(buf, "SPO2: %3d ", data->spo2);
            LCD_ShowString(24, 78, 200, 16, 16, (u8*)buf);
        }
        
        {
            POINT_COLOR = UI_COLOR_PR;
            BACK_COLOR = UI_COLOR_PANEL_BG;
            if(data->heart_rate == 0) sprintf(buf, "PR:   --  ");
            else sprintf(buf, "PR:  %3d  ", data->heart_rate);
            LCD_ShowString(24, 158, 200, 16, 16, (u8*)buf);
        }
        
        {
            POINT_COLOR = UI_COLOR_PI;
            BACK_COLOR = UI_COLOR_PANEL_BG;
            if(data->pi == 0 && data->status == 0) sprintf(buf, "PI R:-- I:-- ");
            else sprintf(buf, "PI R:%2d I:%2d ", data->status, data->pi);
            LCD_ShowString(24, 238, 220, 16, 16, (u8*)buf);
        }
        
        {
            POINT_COLOR = YELLOW;
            BACK_COLOR = UI_COLOR_PANEL_BG;
            sprintf(buf, "R: %d    ", data->filter_status);
            LCD_ShowString(24, 310, 84, 16, 16, (u8*)buf);
        }
        
        {
            POINT_COLOR = GRAY;
            BACK_COLOR = UI_COLOR_PANEL_BG;
            sprintf(buf, "PWM R:%d I:%d   ", data->pwm_red, data->pwm_ir);
            LCD_ShowString(120, 310, 96, 16, 16, (u8*)buf);
        }
        
        {
            POINT_COLOR = GRAY;
            BACK_COLOR = UI_COLOR_PANEL_BG;
            if(data->gain_level == 0xFF) sprintf(buf, "Gain: --  ");
            else sprintf(buf, "Gain: %d   ", data->gain_level);
            LCD_ShowString(240, 310, 64, 16, 16, (u8*)buf);
        }

        s_curData = *data;
    }
    else
    {
        s_curData = *data;
        if(s_uiState == UI_STATE_R_CALIB)
        {
            UI_UpdateRCalibValue();
        }
        else if(s_uiState == UI_STATE_DATA_REVIEW)
        {
            UI_UpdateDataReviewValue();
        }
    }
}

void UI_UpdateWave(uint16_t red_filtered)
{
    UI_WaveTrackDcAc(red_filtered);
    if(s_uiState == UI_STATE_MAIN)
    {
        s_waveDecimCnt++;
        if(s_waveDecimCnt < UI_WAVE_DECIM) return;
        s_waveDecimCnt = 0;
        UI_WavePushSample(red_filtered);
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
        BACK_COLOR = UI_COLOR_PANEL_BG;
        sprintf(buf, "PWM R:%d I:%d   ", s_curData.pwm_red, s_curData.pwm_ir);
        LCD_ShowString(120, 310, 96, 16, 16, (u8*)buf);
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
        BACK_COLOR = UI_COLOR_PANEL_BG;
        sprintf(buf, "Gain: %d   ", s_curData.gain_level);
        LCD_ShowString(240, 310, 64, 16, 16, (u8*)buf);
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
    else if(s_uiState == UI_STATE_WORK_MODE)
    {
        UI_DrawWorkModeScreen();
    }
    else if(s_uiState == UI_STATE_ALARM_SET)
    {
        UI_DrawAlarmSetScreen();
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
    else if(s_uiState == UI_STATE_ETCO2_SET)
    {
        UI_DrawEtCO2SetScreen();
    }
    else if(s_uiState == UI_STATE_SYSTEM_SET)
    {
        UI_DrawSystemSetScreen();
    }
    else if(s_uiState == UI_STATE_DATA_REVIEW)
    {
        UI_DrawDataReviewScreen();
    }
    
    s_needRefresh = 0;
}

void UI_Process(void)
{
    UI_Update();
}

static void UI_WaveReset(void)
{
    s_wavePlotInited = 0;
    s_waveXPos = 0;
    s_wavePrevY = UI_WAVE_Y + (UI_WAVE_H / 2);
    s_waveDecimCnt = 0;
    LCD_Fill(UI_WAVE_X, UI_WAVE_Y, UI_WAVE_X + UI_WAVE_W - 1, UI_WAVE_Y + UI_WAVE_H - 1, UI_COLOR_PANEL_BG);
    POINT_COLOR = UI_COLOR_BORDER;
    LCD_DrawLine(UI_WAVE_X, UI_WAVE_Y + UI_WAVE_H / 2, UI_WAVE_X + UI_WAVE_W - 1, UI_WAVE_Y + UI_WAVE_H / 2);
}

static void UI_WaveTrackDcAc(uint16_t sample)
{
    int32_t sample_q8 = ((int32_t)sample) << 8;
    int32_t diff;
    if(!s_waveAcInit)
    {
        s_waveDcQ8 = sample_q8;
        s_waveAcQ8 = 8 << 8;
        s_waveAcInit = 1;
    }
    s_waveDcQ8 += (sample_q8 - s_waveDcQ8) >> 5;
    diff = sample_q8 - s_waveDcQ8;
    if(diff < 0) diff = -diff;
    s_waveAcQ8 += (diff - s_waveAcQ8) >> 4;
    if(s_waveAcQ8 < (8 << 8)) s_waveAcQ8 = 8 << 8;
}

static void UI_WavePushSample(uint16_t sample)
{
    int32_t sample_q8;
    int32_t ac;
    int32_t half;
    int32_t norm;
    uint16_t x;
    uint16_t y;
    uint16_t center;
    if(!s_waveAcInit) return;
    if(!s_wavePlotInited)
    {
        UI_WaveReset();
        s_wavePlotInited = 1;
    }
    sample_q8 = ((int32_t)sample) << 8;
    ac = s_waveAcQ8 >> 8;
    if(ac < 8) ac = 8;
    half = (UI_WAVE_H - 4) / 2;
    norm = ((sample_q8 - s_waveDcQ8) * half) / (ac << 8);
    if(norm > half) norm = half;
    if(norm < -half) norm = -half;
    center = UI_WAVE_Y + (UI_WAVE_H / 2);
    y = (uint16_t)(center - norm);
    x = UI_WAVE_X + s_waveXPos;
    if(s_waveXPos == 0)
    {
        LCD_Fill(UI_WAVE_X, UI_WAVE_Y, UI_WAVE_X + UI_WAVE_W - 1, UI_WAVE_Y + UI_WAVE_H - 1, UI_COLOR_PANEL_BG);
        POINT_COLOR = UI_COLOR_BORDER;
        LCD_DrawLine(UI_WAVE_X, center, UI_WAVE_X + UI_WAVE_W - 1, center);
        s_wavePrevY = y;
    }
    else
    {
        uint16_t x2 = x + UI_WAVE_X_STEP - 1;
        if(x2 > (UI_WAVE_X + UI_WAVE_W - 1)) x2 = UI_WAVE_X + UI_WAVE_W - 1;
        LCD_Fill(x, UI_WAVE_Y, x2, UI_WAVE_Y + UI_WAVE_H - 1, UI_COLOR_PANEL_BG);
        POINT_COLOR = UI_COLOR_BORDER;
        LCD_DrawPoint(x, center);
        POINT_COLOR = UI_COLOR_SPO2;
        LCD_DrawLine((u16)(x - UI_WAVE_X_STEP), s_wavePrevY, x, y);
        s_wavePrevY = y;
    }
    s_waveXPos = (uint16_t)(s_waveXPos + UI_WAVE_X_STEP);
    if(s_waveXPos >= UI_WAVE_W)
    {
        s_waveXPos = 0;
    }
}

// 辅助函数：绘制圆角矩形风格的面板
void UI_DrawPanel(u16 x, u16 y, u16 w, u16 h, u16 color, char* title, u16 titleColor)
{
    LCD_Fill(x, y, x+w, y+h, UI_COLOR_PANEL_BG);
    POINT_COLOR = color;
    LCD_DrawRectangle(x, y, x+w, y+h);
    LCD_DrawRectangle(x+1, y+1, x+w-1, y+h-1);
    LCD_Fill(x+2, y+2, x+w-2, y+24, color);
    POINT_COLOR = titleColor;
    BACK_COLOR = color;
    LCD_ShowString(x+10, y+5, w-20, 16, 16, (u8*)title);
    BACK_COLOR = UI_COLOR_BG;
}

void UI_DrawHeader(char* title)
{
    LCD_Fill(0, 0, 319, 39, UI_COLOR_HEADER_BG);
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_HEADER_BG;
    LCD_ShowString(12, 12, 120, 16, 16, (u8*)title);
    LCD_ShowString(200, 12, 108, 16, 16, (u8*)"ID:01 ONLINE");
    BACK_COLOR = UI_COLOR_BG;
    POINT_COLOR = UI_COLOR_ACCENT;
    LCD_DrawLine(0, 40, 319, 40);
    LCD_DrawLine(0, 41, 319, 41);
}

static void UI_DrawScreenBackground(void)
{
    u16 y;
    u16 c;
    u16 g;
    u16 b;
    for(y = 0; y < 480; y++)
    {
        g = (u16)(8 + y / 16);
        if(g > 63) g = 63;
        b = (u16)(10 + y / 15);
        if(b > 31) b = 31;
        c = (u16)((1 << 11) | (g << 5) | b);
        LCD_Fill(0, y, 319, y, c);
    }
}

static void UI_DrawCard(u16 x, u16 y, u16 w, u16 h, u16 borderColor, u16 titleColor, char* title)
{
    LCD_Fill(x, y, x + w, y + h, UI_COLOR_PANEL_BG);
    POINT_COLOR = UI_COLOR_BORDER;
    LCD_DrawRectangle(x, y, x + w, y + h);
    POINT_COLOR = borderColor;
    LCD_DrawRectangle(x + 1, y + 1, x + w - 1, y + h - 1);
    LCD_Fill(x + 2, y + 2, x + w - 2, y + 18, borderColor);
    POINT_COLOR = titleColor;
    BACK_COLOR = borderColor;
    LCD_ShowString(x + 8, y + 4, w - 12, 16, 16, (u8*)title);
    BACK_COLOR = UI_COLOR_BG;
}

void UI_DrawMainScreen(void)
{
    char buf[20];
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("Monitor");

    UI_DrawCard(10, 50, 300, 70, UI_COLOR_SPO2, BLACK, "SPO2");
    UI_DrawCard(10, 130, 300, 70, UI_COLOR_PR, BLACK, "Pulse Rate");
    UI_DrawCard(10, 210, 300, 70, UI_COLOR_PI, BLACK, "Perfusion");
    UI_DrawCard(10, 290, 300, 40, UI_COLOR_ACCENT, WHITE, "Signal");
    UI_DrawCard(10, 340, 300, 130, UI_COLOR_ACCENT, WHITE, "Pulse Wave(5s)");

    POINT_COLOR = UI_COLOR_SPO2;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    if(s_curData.spo2 == 0) sprintf(buf, "SPO2: --  ");
    else sprintf(buf, "SPO2: %3d ", s_curData.spo2);
    LCD_ShowString(24, 78, 200, 16, 16, (u8*)buf);
    
    POINT_COLOR = UI_COLOR_PR;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    if(s_curData.heart_rate == 0) sprintf(buf, "PR:   --  ");
    else sprintf(buf, "PR:  %3d  ", s_curData.heart_rate);
    LCD_ShowString(24, 158, 200, 16, 16, (u8*)buf);
    
    POINT_COLOR = UI_COLOR_PI;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    if(s_curData.pi == 0 && s_curData.status == 0) sprintf(buf, "PI R:-- I:-- ");
    else sprintf(buf, "PI R:%2d I:%2d ", s_curData.status, s_curData.pi);
    LCD_ShowString(24, 238, 220, 16, 16, (u8*)buf);
    
    POINT_COLOR = YELLOW;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    sprintf(buf, "R: %d    ", s_curData.filter_status);
    LCD_ShowString(24, 310, 84, 16, 16, (u8*)buf);
    
    POINT_COLOR = GRAY;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    sprintf(buf, "PWM R:%d I:%d   ", s_curData.pwm_red, s_curData.pwm_ir);
    LCD_ShowString(120, 310, 96, 16, 16, (u8*)buf);

    POINT_COLOR = GRAY;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    if(s_curData.gain_level == 0xFF) sprintf(buf, "Gain: --  ");
    else sprintf(buf, "Gain: %d   ", s_curData.gain_level);
    LCD_ShowString(240, 310, 64, 16, 16, (u8*)buf);

    UI_WaveReset();

    BACK_COLOR = UI_COLOR_BG;
}

void UI_DrawSettingsScreen(void)
{
    uint8_t i;
    u16 original_back_color = BACK_COLOR;

    UI_DrawScreenBackground();
    UI_DrawHeader("Settings");
    
    for(i = 0; i < SETTINGS_ITEM_COUNT; i++)
    {
        UI_DrawSettingsItem(i, (i == s_settingsIndex) ? 1 : 0);
    }
    
    POINT_COLOR = UI_COLOR_HINT;
    BACK_COLOR = UI_COLOR_BG;
    LCD_ShowString(20, 440, 280, 16, 16, (u8*)"LL/RL:Sel LH:Enter RH:Back");
    
    BACK_COLOR = original_back_color;
}

static void UI_DrawSettingsItem(uint8_t index, uint8_t selected)
{
    u16 y_pos = 60 + index * 40;
    if(selected)
    {
        LCD_Fill(15, y_pos - 6, 305, y_pos + 26, UI_COLOR_SELECT);
        POINT_COLOR = WHITE;
        BACK_COLOR = UI_COLOR_SELECT;
        LCD_DrawRectangle(15, y_pos - 6, 305, y_pos + 26);
    }
    else
    {
        LCD_Fill(15, y_pos - 6, 305, y_pos + 26, UI_COLOR_PANEL_BG);
        POINT_COLOR = UI_COLOR_BORDER;
        BACK_COLOR = UI_COLOR_PANEL_BG;
        LCD_DrawRectangle(15, y_pos - 6, 305, y_pos + 26);
        POINT_COLOR = UI_COLOR_TEXT_W;
    }
    LCD_ShowString(30, y_pos, 200, 16, 16, (u8*)s_settingsItems[index]);
    if(selected)
    {
        LCD_ShowString(280, y_pos, 20, 16, 16, (u8*)">");
    }
    else
    {
        POINT_COLOR = UI_COLOR_BORDER;
        BACK_COLOR = UI_COLOR_PANEL_BG;
        LCD_ShowString(280, y_pos, 20, 16, 16, (u8*)" ");
    }
}

void UI_DrawWorkModeScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("Work Mode");
    UI_DrawPanel(60, 100, 200, 150, UI_COLOR_ACCENT, "Mode Select", WHITE);
    UI_UpdateWorkModeStatus();
    POINT_COLOR = GREEN;
    LCD_ShowString(85, 195, 180, 16, 16, (u8*)"LL: Monitor");
    POINT_COLOR = RED;
    LCD_ShowString(85, 215, 180, 16, 16, (u8*)"RL: Service");
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(70, 440, 180, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateWorkModeStatus(void)
{
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    if(s_workMode == 0)
        LCD_ShowString(80, 150, 170, 16, 16, (u8*)"Current: Monitor ");
    else
        LCD_ShowString(80, 150, 170, 16, 16, (u8*)"Current: Service ");
}

void UI_DrawAlarmSetScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("Alarm Set");
    UI_DrawPanel(40, 100, 240, 180, UI_COLOR_ACCENT, "Alarm Profile", WHITE);
    UI_UpdateAlarmSetStatus();
    POINT_COLOR = GREEN;
    LCD_ShowString(65, 230, 180, 16, 16, (u8*)"LL: Profile +");
    POINT_COLOR = RED;
    LCD_ShowString(65, 250, 180, 16, 16, (u8*)"RL: Profile -");
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(70, 440, 180, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateAlarmSetStatus(void)
{
    char buf[32];
    uint8_t spo2_low;
    uint8_t hr_low;
    uint8_t hr_high;
    if(s_alarmProfile == 0)
    {
        spo2_low = 90;
        hr_low = 45;
        hr_high = 130;
    }
    else if(s_alarmProfile == 1)
    {
        spo2_low = 92;
        hr_low = 50;
        hr_high = 140;
    }
    else
    {
        spo2_low = 94;
        hr_low = 55;
        hr_high = 150;
    }
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    if(s_alarmProfile == 0) LCD_ShowString(65, 140, 180, 16, 16, (u8*)"Profile: Low    ");
    else if(s_alarmProfile == 1) LCD_ShowString(65, 140, 180, 16, 16, (u8*)"Profile: Medium ");
    else LCD_ShowString(65, 140, 180, 16, 16, (u8*)"Profile: High   ");
    sprintf(buf, "SpO2 Low: %d   ", spo2_low);
    LCD_ShowString(65, 168, 180, 16, 16, (u8*)buf);
    sprintf(buf, "HR Min/Max:%d/%d", hr_low, hr_high);
    LCD_ShowString(65, 190, 180, 16, 16, (u8*)buf);
}

void UI_DrawSpO2SetScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("SpO2 Gain Set");
    UI_DrawPanel(60, 120, 200, 150, UI_COLOR_SPO2, "Gain Level", BLACK);
    UI_UpdateGainValue();
    POINT_COLOR = GREEN;
    LCD_ShowString(85, 210, 160, 16, 16, (u8*)"UP: Increase");
    POINT_COLOR = RED;
    LCD_ShowString(85, 230, 160, 16, 16, (u8*)"DN: Decrease");
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(70, 440, 200, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateGainValue(void)
{
    char buf[32];
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    sprintf(buf, "Current Level: %d   ", s_curData.gain_level);
    LCD_ShowString(80, 170, 160, 16, 16, (u8*)buf);
}

void UI_DrawFilterSetScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("Filter Set");
    UI_DrawPanel(60, 120, 200, 150, UI_COLOR_SPO2, "Filter Switch", BLACK);
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    UI_UpdateFilterStatus();
    POINT_COLOR = GREEN;
    LCD_ShowString(85, 210, 200, 16, 16, (u8*)"UP: Enable (ON)");
    POINT_COLOR = RED;
    LCD_ShowString(85, 230, 200, 16, 16, (u8*)"DN: Disable(OFF)");
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(70, 440, 200, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateFilterStatus(void)
{
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    if(UART1_GetWaveFilterEnable())
        LCD_ShowString(80, 170, 160, 16, 16, (u8*)"Current: ON  ");
    else
        LCD_ShowString(80, 170, 160, 16, 16, (u8*)"Current: OFF ");
}

void UI_DrawRCalibScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("R Calib");
    
    UI_DrawPanel(50, 120, 220, 150, UI_COLOR_SPO2, "R Value", BLACK);
    UI_UpdateRCalibValue();
    
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(70, 440, 200, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateRCalibValue(void)
{
    char buf[32];
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    sprintf(buf, "R: %d    ", s_curData.filter_status);
    LCD_ShowString(100, 170, 120, 16, 16, (u8*)buf);
    sprintf(buf, "PWM R:%d I:%d ", s_curData.pwm_red, s_curData.pwm_ir);
    LCD_ShowString(70, 195, 160, 16, 16, (u8*)buf);
}

void UI_DrawAutoLightScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("Auto Light");
    
    UI_DrawPanel(60, 120, 200, 150, UI_COLOR_SPO2, "Light Switch", BLACK);
    UI_UpdateAutoLightStatus();
    
    POINT_COLOR = GREEN;
    LCD_ShowString(85, 210, 200, 16, 16, (u8*)"UP: Enable (ON)");
    POINT_COLOR = RED;
    LCD_ShowString(85, 230, 200, 16, 16, (u8*)"DN: Disable(OFF)");
    
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(70, 440, 200, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateAutoLightStatus(void)
{
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    if(s_autoLightEnable)
        LCD_ShowString(80, 170, 160, 16, 16, (u8*)"Current: ON  ");
    else
        LCD_ShowString(80, 170, 160, 16, 16, (u8*)"Current: OFF ");
}

void UI_DrawEtCO2SetScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("EtCO2 Set");
    UI_DrawPanel(60, 120, 200, 150, UI_COLOR_ACCENT, "EtCO2 Switch", WHITE);
    UI_UpdateEtCO2Status();
    POINT_COLOR = GREEN;
    LCD_ShowString(85, 210, 180, 16, 16, (u8*)"LL: Enable");
    POINT_COLOR = RED;
    LCD_ShowString(85, 230, 180, 16, 16, (u8*)"RL: Disable");
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(70, 440, 180, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateEtCO2Status(void)
{
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    if(s_etco2Enable)
        LCD_ShowString(80, 170, 170, 16, 16, (u8*)"Current: ON   ");
    else
        LCD_ShowString(80, 170, 170, 16, 16, (u8*)"Current: OFF  ");
}

void UI_DrawSystemSetScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("System Set");
    UI_DrawPanel(40, 100, 240, 180, UI_COLOR_ACCENT, "System Config", WHITE);
    UI_UpdateSystemSetStatus();
    POINT_COLOR = GREEN;
    LCD_ShowString(65, 230, 180, 16, 16, (u8*)"LL: Brightness+");
    POINT_COLOR = RED;
    LCD_ShowString(65, 250, 180, 16, 16, (u8*)"RL: Brightness-");
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(70, 440, 180, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateSystemSetStatus(void)
{
    char buf[32];
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    sprintf(buf, "Brightness: %d   ", s_systemBrightness);
    LCD_ShowString(65, 160, 180, 16, 16, (u8*)buf);
    if(s_systemBeepEnable) LCD_ShowString(65, 188, 180, 16, 16, (u8*)"Key Beep: ON ");
    else LCD_ShowString(65, 188, 180, 16, 16, (u8*)"Key Beep: OFF");
}

void UI_DrawDataReviewScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("Data Review");
    UI_DrawPanel(30, 80, 260, 200, UI_COLOR_ACCENT, "Review Page", WHITE);
    UI_UpdateDataReviewValue();
    POINT_COLOR = GREEN;
    LCD_ShowString(55, 240, 180, 16, 16, (u8*)"LL: Page Next");
    POINT_COLOR = RED;
    LCD_ShowString(55, 260, 180, 16, 16, (u8*)"RL: Page Prev");
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(70, 440, 180, 16, 16, (u8*)"Press BACK to Exit");
}

static void UI_UpdateDataReviewValue(void)
{
    char buf[32];
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    if(s_dataReviewPage == 0)
    {
        LCD_ShowString(50, 120, 180, 16, 16, (u8*)"Page 1: SPO2/PR");
        sprintf(buf, "SPO2: %d    ", s_curData.spo2);
        LCD_ShowString(50, 148, 180, 16, 16, (u8*)buf);
        sprintf(buf, "PR: %d      ", s_curData.heart_rate);
        LCD_ShowString(50, 170, 180, 16, 16, (u8*)buf);
    }
    else if(s_dataReviewPage == 1)
    {
        LCD_ShowString(50, 120, 180, 16, 16, (u8*)"Page 2: PI/R");
        sprintf(buf, "PI R:%d I:%d   ", s_curData.status, s_curData.pi);
        LCD_ShowString(50, 148, 180, 16, 16, (u8*)buf);
        sprintf(buf, "R: %d         ", s_curData.filter_status);
        LCD_ShowString(50, 170, 180, 16, 16, (u8*)buf);
    }
    else
    {
        LCD_ShowString(50, 120, 180, 16, 16, (u8*)"Page 3: Drive");
        sprintf(buf, "PWM R:%d I:%d  ", s_curData.pwm_red, s_curData.pwm_ir);
        LCD_ShowString(50, 148, 180, 16, 16, (u8*)buf);
        sprintf(buf, "Gain: %d       ", s_curData.gain_level);
        LCD_ShowString(50, 170, 180, 16, 16, (u8*)buf);
    }
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
    else if(s_uiState == UI_STATE_WORK_MODE)
    {
        s_workMode = 0;
        UI_UpdateWorkModeStatus();
    }
    else if(s_uiState == UI_STATE_ALARM_SET)
    {
        if(s_alarmProfile < 2) s_alarmProfile++;
        UI_UpdateAlarmSetStatus();
    }
    else if(s_uiState == UI_STATE_ETCO2_SET)
    {
        s_etco2Enable = 1;
        UI_UpdateEtCO2Status();
    }
    else if(s_uiState == UI_STATE_SYSTEM_SET)
    {
        if(s_systemBrightness < 5) s_systemBrightness++;
        if(s_systemBrightness >= 3) s_systemBeepEnable = 1;
        UI_UpdateSystemSetStatus();
    }
    else if(s_uiState == UI_STATE_DATA_REVIEW)
    {
        if(s_dataReviewPage < 2) s_dataReviewPage++;
        else s_dataReviewPage = 0;
        UI_UpdateDataReviewValue();
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
    else if(s_uiState == UI_STATE_WORK_MODE)
    {
        s_workMode = 1;
        UI_UpdateWorkModeStatus();
    }
    else if(s_uiState == UI_STATE_ALARM_SET)
    {
        if(s_alarmProfile > 0) s_alarmProfile--;
        UI_UpdateAlarmSetStatus();
    }
    else if(s_uiState == UI_STATE_ETCO2_SET)
    {
        s_etco2Enable = 0;
        UI_UpdateEtCO2Status();
    }
    else if(s_uiState == UI_STATE_SYSTEM_SET)
    {
        if(s_systemBrightness > 1) s_systemBrightness--;
        if(s_systemBrightness < 3) s_systemBeepEnable = 0;
        UI_UpdateSystemSetStatus();
    }
    else if(s_uiState == UI_STATE_DATA_REVIEW)
    {
        if(s_dataReviewPage > 0) s_dataReviewPage--;
        else s_dataReviewPage = 2;
        UI_UpdateDataReviewValue();
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
        if(s_settingsIndex == 0)
        {
            s_uiState = UI_STATE_WORK_MODE;
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 1)
        {
            s_uiState = UI_STATE_ALARM_SET;
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 2)
        {
            s_uiState = UI_STATE_SPO2_SET;
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 3)
        {
            s_uiState = UI_STATE_FILTER_SET;
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 4)
        {
            s_uiState = UI_STATE_R_CALIB;
            UART1_SetRCalibMode(1);
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 5)
        {
            s_uiState = UI_STATE_AUTO_LIGHT;
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 6)
        {
            s_uiState = UI_STATE_ETCO2_SET;
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 7)
        {
            s_uiState = UI_STATE_SYSTEM_SET;
            s_needRefresh = 1;
        }
        else if(s_settingsIndex == 8)
        {
            s_uiState = UI_STATE_DATA_REVIEW;
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
    else if(s_uiState == UI_STATE_WORK_MODE)
    {
        s_uiState = UI_STATE_SETTINGS;
        s_needRefresh = 1;
    }
    else if(s_uiState == UI_STATE_ALARM_SET)
    {
        s_uiState = UI_STATE_SETTINGS;
        s_needRefresh = 1;
    }
    else if(s_uiState == UI_STATE_ETCO2_SET)
    {
        s_uiState = UI_STATE_SETTINGS;
        s_needRefresh = 1;
    }
    else if(s_uiState == UI_STATE_SYSTEM_SET)
    {
        s_uiState = UI_STATE_SETTINGS;
        s_needRefresh = 1;
    }
    else if(s_uiState == UI_STATE_DATA_REVIEW)
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
