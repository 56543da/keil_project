#include "UI_Manager.h"
#include "SPO2_Algo.h"
#include "LCD.h"
#include "UART1.h"
#include "Speaker.h"
#include "Power.h"
#include <stdio.h>

// UI Colors (RGB565)
#define UI_COLOR_BG         0x0841
#define UI_COLOR_PANEL_BG   0x10A2
#define UI_COLOR_WAVE_BG    0x0000 // 纯黑波形底色，提升波形对比度
#define UI_COLOR_HEADER_BG  0x18E3
#define UI_COLOR_TEXT_W     WHITE
#define UI_COLOR_SPO2       0xF800 // 饱和红 (与主界面波形颜色一致)
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
#define UI_WAVE_RAW_MIN     0
#define UI_WAVE_RAW_MAX     4095
#define UI_COLOR_WAVE_BG    0x0000

static uint16_t s_waveMin = 4095;
static uint16_t s_waveMax = 0;
static uint16_t s_waveDispMin = 0;
static uint16_t s_waveDispMax = 4095;

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
static uint16_t s_wavePrevYRed = UI_WAVE_Y + (UI_WAVE_H / 2);
static uint16_t s_wavePrevYIr = UI_WAVE_Y + (UI_WAVE_H / 2);
static uint8_t s_waveDecimCnt = 0;
static SPO2Data_t s_curData = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // 缓存当前数据
/* 滤波演示模式状态 */
static uint8_t s_demoNoiseEnable = 0;
static uint8_t s_demoDriftEnable = 0;
static int16_t s_demoDriftVal = 0;
static int8_t  s_demoDriftStep = 1;
static uint16_t s_lfsr = 0xACE1;
static uint8_t s_demoFilterMode = 0; /* 0=IIR,1=Detrend,2=Median */
/* Demo 横屏扫描方向校准：MENU 一键循环 4 种候选方向，当前选项即为固化配置 */
static const uint8_t s_demoScanDirList[4] = {L2R_U2D, L2R_D2U, R2L_U2D, R2L_D2U};
static uint8_t s_demoScanDirIndex = 2; /* 固化为 Scan 3/4 (R2L_U2D) */
static uint8_t s_fingerOff = 0;

static uint8_t s_alarmSwitch = 1;
static uint8_t s_alarmActive = 0;
static uint8_t s_alarmVolume = 10;
static uint8_t s_alarmMode = 1;

/* 动态波形显示区域与抽取控制（默认使用底部小窗配置） */
static uint16_t s_waveX = UI_WAVE_X;
static uint16_t s_waveY = UI_WAVE_Y;
static uint16_t s_waveW = UI_WAVE_W;
static uint16_t s_waveH = UI_WAVE_H;
static uint16_t s_waveXStep = UI_WAVE_X_STEP;
static uint16_t s_waveDecimCntMax = UI_WAVE_DECIM;

#define SETTINGS_ITEM_COUNT 10
static const char* s_settingsItems[SETTINGS_ITEM_COUNT] = {
    "Work Mode",
    "Alarm Set",
    "SpO2 Set",
    "Filter Set",
    "Filter Demo",
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
void UI_DrawFilterDemoScreen(void);
static void UI_DrawFilterDemoFrame(void);
static void UI_UpdateFilterDemoStatus(void);
void UI_DrawAlarmTestScreen(void);
static void UI_UpdateAlarmTestStatus(void);
void UI_DrawRCalibScreen(void);
void UI_DrawAutoLightScreen(void);
void UI_DrawEtCO2SetScreen(void);
void UI_DrawSystemSetScreen(void);
void UI_DrawDataReviewScreen(void);
static void UI_DrawScreenBackground(void);
static void UI_DrawCard(u16 x, u16 y, u16 w, u16 h, u16 borderColor, u16 titleColor, char* title);
static void UI_DrawSettingsItem(uint8_t index, uint8_t selected);
static void UI_WaveReset(void);
static void UI_SetWaveRect(u16 x, u16 y, u16 w, u16 h);
static uint16_t UI_WaveMapY(uint16_t sample);
static void UI_WavePushSample(uint16_t red_raw, uint16_t ir_raw);
static uint16_t UI_AddDemoNoise(uint16_t v);
static uint16_t UI_AddDemoDrift(uint16_t v);
static void UI_UpdateWorkModeStatus(void);
static void UI_UpdateAlarmSetStatus(void);
static void UI_UpdateGainValue(void);
static void UI_UpdateFilterStatus(void);
static void UI_UpdateRCalibValue(void);
static void UI_UpdateAutoLightStatus(void);
static void UI_UpdateEtCO2Status(void);
static void UI_UpdateSystemSetStatus(void);
static void UI_UpdateDataReviewValue(void);
static void UI_DrawBatteryIcon(uint8_t percent, uint8_t charging);
static void UI_UpdateBattery(void);

void UI_Init(void)
{
    LCD_Clear(UI_COLOR_BG);
    s_uiState = UI_STATE_MAIN;
    s_needRefresh = 1;
    /* 默认主界面的波形区域 */
    UI_SetWaveRect(UI_WAVE_X, UI_WAVE_Y, UI_WAVE_W, UI_WAVE_H);
    UI_Update();
}

void UI_UpdateData(SPO2Data_t *data)
{
    char buf[20];
    if(data == NULL) return;

    if(data->pwm_red == 0 && s_curData.pwm_red != 0) data->pwm_red = s_curData.pwm_red;
    if(data->pwm_ir == 0 && s_curData.pwm_ir != 0) data->pwm_ir = s_curData.pwm_ir;
    if(data->gain_level == 0xFF && s_curData.gain_level != 0xFF) data->gain_level = s_curData.gain_level;
    
    if(s_alarmMode == 0){
        s_alarmActive = 0;
    }else if(s_alarmMode == 1){
        if(data->spo2 > 0 && data->spo2 < 90) s_alarmActive = 1;
        else s_alarmActive = 0;
    }else{
        if(s_uiState == UI_STATE_ALARM_TEST) s_alarmActive = 1;
        else s_alarmActive = 0;
    }
    Speaker_SetAlarmActive(s_alarmActive);

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
        UI_UpdateBattery();
    }
    else
    {
        s_curData = *data;
        switch(s_uiState)
        {
            case UI_STATE_R_CALIB:      UI_UpdateRCalibValue(); break;
            case UI_STATE_ALARM_TEST:   UI_UpdateAlarmTestStatus(); break;
            case UI_STATE_DATA_REVIEW:   UI_UpdateDataReviewValue(); break;
            default: break;
        }
        UI_UpdateBattery();
    }
}

void UI_UpdateWave(uint16_t red_raw, uint16_t ir_raw)
{
    uint8_t currentFingerOff = (red_raw > 4080 && ir_raw > 4080) ? 1 : 0;
    
    if (s_uiState == UI_STATE_MAIN)
    {
        if (currentFingerOff != s_fingerOff) {
            s_fingerOff = currentFingerOff;
            if (!s_fingerOff) {
                // 手指恢复，重置波形区域以清除弹窗
                UI_WaveReset();
            } else {
                // 手指刚脱落，绘制弹窗
                LCD_Fill(60, 395, 260, 435, RED);
                POINT_COLOR = WHITE;
                BACK_COLOR = RED;
                LCD_ShowString(64, 403, 192, 24, 24, (u8*)"ERROR: NO FINGER");
            }
        }
    }

    if(s_uiState == UI_STATE_MAIN || s_uiState == UI_STATE_FILTER_DEMO)
    {
        s_waveDecimCnt++;
        if(s_waveDecimCnt < s_waveDecimCntMax) return;
        s_waveDecimCnt = 0;
        
        // 如果手指脱落且在主界面，冻结波形刷新，保持弹窗显示
        if (s_fingerOff && s_uiState == UI_STATE_MAIN) {
            return;
        }
        
        /* 约定：red=原始IR，ir=滤波IR */
        if(s_uiState == UI_STATE_FILTER_DEMO)
        {
            uint16_t raw = red_raw;
            if(s_demoNoiseEnable) raw = UI_AddDemoNoise(raw);
            if(s_demoDriftEnable) raw = UI_AddDemoDrift(raw);
            /* 根据演示滤波模式生成滤波后的 IR */
            {
                uint16_t demo_filt = SPO2_Algo_ApplyDemoFilter(raw, s_demoFilterMode);
                UI_WavePushSample(raw, demo_filt);
            }
        }
        else
        {
            UI_WavePushSample(red_raw, ir_raw);
        }
    }
}

void UI_UpdatePwm(uint8_t pwm_red, uint8_t pwm_ir)
{
    char buf[20];
    if(pwm_red == 0 && pwm_ir == 0) return;
    s_curData.pwm_red = pwm_red;
    s_curData.pwm_ir = pwm_ir;

    switch(s_uiState)
    {
        case UI_STATE_MAIN:
            POINT_COLOR = GRAY;
            BACK_COLOR = UI_COLOR_PANEL_BG;
            sprintf(buf, "PWM R:%d I:%d   ", s_curData.pwm_red, s_curData.pwm_ir);
            LCD_ShowString(120, 310, 96, 16, 16, (u8*)buf);
            break;
            
        case UI_STATE_R_CALIB:
            UI_UpdateRCalibValue();
            break;
            
        default: break;
    }
}

void UI_UpdateGain(uint8_t gain_level)
{
    char buf[20];
    s_curData.gain_level = gain_level;

    switch(s_uiState)
    {
        case UI_STATE_MAIN:
            POINT_COLOR = GRAY;
            BACK_COLOR = UI_COLOR_PANEL_BG;
            sprintf(buf, "Gain: %d   ", s_curData.gain_level);
            LCD_ShowString(240, 310, 64, 16, 16, (u8*)buf);
            break;
            
        case UI_STATE_SPO2_SET:
            UI_UpdateGainValue();
            break;
            
        default: break;
    }
}

void UI_Update(void)
{
    if(!s_needRefresh) return;

    switch(s_uiState)
    {
        case UI_STATE_MAIN:         UI_DrawMainScreen(); break;
        case UI_STATE_SETTINGS:     UI_DrawSettingsScreen(); break;
        case UI_STATE_WORK_MODE:    UI_DrawWorkModeScreen(); break;
        case UI_STATE_ALARM_SET:    UI_DrawAlarmSetScreen(); break;
        case UI_STATE_SPO2_SET:     UI_DrawSpO2SetScreen(); break;
        case UI_STATE_FILTER_SET:   UI_DrawFilterSetScreen(); break;
        case UI_STATE_FILTER_DEMO:  UI_DrawFilterDemoScreen(); break;
        case UI_STATE_ALARM_TEST:   UI_DrawAlarmTestScreen(); break;
        case UI_STATE_R_CALIB:      UI_DrawRCalibScreen(); break;
        case UI_STATE_AUTO_LIGHT:   UI_DrawAutoLightScreen(); break;
        case UI_STATE_ETCO2_SET:    UI_DrawEtCO2SetScreen(); break;
        case UI_STATE_SYSTEM_SET:   UI_DrawSystemSetScreen(); break;
        case UI_STATE_DATA_REVIEW:  UI_DrawDataReviewScreen(); break;
        default: break;
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
    s_wavePrevYRed = (u16)(s_waveY + (s_waveH / 2));
    s_wavePrevYIr = (u16)(s_waveY + (s_waveH / 2));
    s_waveDecimCnt = 0;
    s_waveMin = 4095;
    s_waveMax = 0;
    s_waveDispMin = UI_WAVE_RAW_MIN;
    s_waveDispMax = UI_WAVE_RAW_MAX;
    
    if (s_uiState == UI_STATE_FILTER_DEMO) {
        /* 在 Demo 模式下，直接清理整个内部区域即可，因为没有边框需求 */
        LCD_Fill(s_waveX, s_waveY, (u16)(s_waveX + s_waveW - 1), (u16)(s_waveY + s_waveH - 1), UI_COLOR_WAVE_BG);
    } else {
        /* 主界面带有边框的清屏逻辑 */
        LCD_Fill(s_waveX, s_waveY, (u16)(s_waveX + s_waveW - 1), (u16)(s_waveY + s_waveH - 1), UI_COLOR_WAVE_BG); // 外框清黑
        POINT_COLOR = UI_COLOR_BORDER;
        LCD_DrawRectangle(s_waveX, s_waveY, (u16)(s_waveX + s_waveW - 1), (u16)(s_waveY + s_waveH - 1));
        /* 内部区域再清一次，避免边框覆盖导致的残留 */
        if(s_waveW > 2 && s_waveH > 2){
            LCD_Fill((u16)(s_waveX + 1), (u16)(s_waveY + 1), (u16)(s_waveX + s_waveW - 2), (u16)(s_waveY + s_waveH - 2), UI_COLOR_WAVE_BG);
        }
    }
}

static uint16_t UI_WaveMapY(uint16_t sample)
{
    uint32_t h = (uint32_t)(s_waveH - 1);
    uint32_t y_off;
    
    // 动态更新极值，留一点裕量
    if(sample < s_waveMin) s_waveMin = sample;
    if(sample > s_waveMax) s_waveMax = sample;
    
    if(sample > s_waveDispMax) sample = s_waveDispMax;
    if(sample < s_waveDispMin) sample = s_waveDispMin;
    
    if (s_waveDispMax == s_waveDispMin) {
        y_off = h / 2;
    } else {
        y_off = (uint32_t)(sample - s_waveDispMin) * h / (s_waveDispMax - s_waveDispMin);
    }
    return (uint16_t)(s_waveY + h - y_off);
}

static void UI_WavePushSample(uint16_t red_raw, uint16_t ir_raw)
{
    uint16_t x;
    uint16_t y_red;
    uint16_t y_ir;
    if(!s_wavePlotInited)
    {
        UI_WaveReset();
        s_wavePlotInited = 1;
    }
    
    // 一轮画完后，更新下一轮的显示极值，并重置统计极值
    if(s_waveXPos == 0)
    {
        uint16_t margin = (s_waveMax - s_waveMin) / 10;
        if(margin < 10) margin = 10;
        
        if (s_waveMin > margin) s_waveDispMin = s_waveMin - margin;
        else s_waveDispMin = 0;
        
        s_waveDispMax = s_waveMax + margin;
        if (s_waveDispMax > 4095) s_waveDispMax = 4095;
        
        // 限制最小显示窗口，避免噪声放大过大
        if (s_waveDispMax - s_waveDispMin < 50) {
            uint16_t mid = (s_waveDispMax + s_waveDispMin) / 2;
            s_waveDispMax = mid + 25;
            s_waveDispMin = (mid > 25) ? (mid - 25) : 0;
        }
        
        s_waveMin = 4095;
        s_waveMax = 0;
    }
    
    y_red = UI_WaveMapY(red_raw);
    y_ir = UI_WaveMapY(ir_raw);
    x = (u16)(s_waveX + s_waveXPos);
    if(s_waveXPos == 0)
    {
        if(s_uiState == UI_STATE_FILTER_DEMO)
        {
            UI_DrawFilterDemoFrame();
        }
        else
        {
            if(s_waveW > 2 && s_waveH > 2){
                LCD_Fill((u16)(s_waveX + 1), (u16)(s_waveY + 1), (u16)(s_waveX + s_waveW - 2), (u16)(s_waveY + s_waveH - 2), UI_COLOR_WAVE_BG);
            } else {
                LCD_Fill(s_waveX, s_waveY, (u16)(s_waveX + s_waveW - 1), (u16)(s_waveY + s_waveH - 1), UI_COLOR_WAVE_BG);
            }
            POINT_COLOR = UI_COLOR_BORDER;
            LCD_DrawRectangle(s_waveX, s_waveY, (u16)(s_waveX + s_waveW - 1), (u16)(s_waveY + s_waveH - 1));
        }
        s_wavePrevYRed = y_red;
        s_wavePrevYIr = y_ir;
    }
    else
    {
        uint16_t x2 = (u16)(x + s_waveXStep - 1);
        if(x2 > (s_waveX + s_waveW - 1)) x2 = (u16)(s_waveX + s_waveW - 1);
        
        if (s_uiState == UI_STATE_FILTER_DEMO)
        {
            /* Demo 模式没有上下边框，直接清理纵向整条 */
            LCD_Fill(x, s_waveY, x2, (u16)(s_waveY + s_waveH - 1), UI_COLOR_WAVE_BG);
        }
        else
        {
            /* 主界面：清理当前竖条（内部区域），保留上下边框像素 */
            if(s_waveH > 2){
                LCD_Fill(x, (u16)(s_waveY + 1), x2, (u16)(s_waveY + s_waveH - 2), UI_COLOR_WAVE_BG);
            } else {
                LCD_Fill(x, s_waveY, x2, (u16)(s_waveY + s_waveH - 1), UI_COLOR_WAVE_BG);
            }
            POINT_COLOR = UI_COLOR_BORDER;
            LCD_DrawPoint(x, s_waveY);
            LCD_DrawPoint(x, (u16)(s_waveY + s_waveH - 1));
        }
        if(s_uiState == UI_STATE_FILTER_DEMO)
    {
        /* 在演示模式：红色=滤波后IR，蓝色=原始IR */
        POINT_COLOR = BLUE;
        LCD_DrawLine((u16)(x - s_waveXStep), s_wavePrevYRed, x, y_red);
        POINT_COLOR = UI_COLOR_SPO2;
        LCD_DrawLine((u16)(x - s_waveXStep), s_wavePrevYIr, x, y_ir);
    }
    else
    {
        /* 主界面：仅显示滤波后IR波形 (红色) */
        POINT_COLOR = UI_COLOR_SPO2;
        LCD_DrawLine((u16)(x - s_waveXStep), s_wavePrevYIr, x, y_ir);
    }
        s_wavePrevYRed = y_red;
        s_wavePrevYIr = y_ir;
    }
    s_waveXPos = (uint16_t)(s_waveXPos + s_waveXStep);
    if(s_waveXPos >= s_waveW)
    {
        s_waveXPos = 0;
    }
}

static void UI_SetWaveRect(u16 x, u16 y, u16 w, u16 h)
{
    uint16_t points;
    s_waveX = x; s_waveY = y; s_waveW = w; s_waveH = h;
    /* 重新计算抽取因子，使 5s 数据刚好填满当前宽度 */
    points = (uint16_t)(w / s_waveXStep);
    if(points == 0) points = 1;
    {
        uint32_t num = (uint32_t)UI_WAVE_EXPECTED_HZ * UI_WAVE_SECONDS + points - 1;
        s_waveDecimCntMax = (uint16_t)(num / points);
        if(s_waveDecimCntMax == 0) s_waveDecimCntMax = 1;
    }
}

static uint16_t UI_AddDemoNoise(uint16_t v)
{
    /* 简易LFSR噪声，幅度±8 */
    s_lfsr ^= s_lfsr << 7;
    s_lfsr ^= s_lfsr >> 9;
    s_lfsr ^= s_lfsr << 8;
    int16_t n = (int16_t)((s_lfsr & 0x0F) - 8);
    int32_t val = (int32_t)v + n;
    if(val < 0) val = 0;
    if(val > 4095) val = 4095;
    return (uint16_t)val;
}

static uint16_t UI_AddDemoDrift(uint16_t v)
{
    /* 生成缓慢的三角波漂移，幅度±20，周期约一屏 */
    s_demoDriftVal += s_demoDriftStep;
    if(s_demoDriftVal > 20) { s_demoDriftVal = 20; s_demoDriftStep = -1; }
    if(s_demoDriftVal < -20){ s_demoDriftVal = -20; s_demoDriftStep = 1; }
    int32_t val = (int32_t)v + s_demoDriftVal;
    if(val < 0) val = 0;
    if(val > 4095) val = 4095;
    return (uint16_t)val;
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
    LCD_Fill(0, 0, (u16)(lcddev.width - 1), 39, UI_COLOR_HEADER_BG);
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_HEADER_BG;
    LCD_ShowString(12, 12, 120, 16, 16, (u8*)title);
    LCD_ShowString((u16)(lcddev.width - 200), 12, 160, 16, 16, (u8*)"ID:01 ONLINE");
    BACK_COLOR = UI_COLOR_BG;
    POINT_COLOR = UI_COLOR_ACCENT;
    LCD_DrawLine(0, 40, (u16)(lcddev.width - 1), 40);
    LCD_DrawLine(0, 41, (u16)(lcddev.width - 1), 41);
    UI_UpdateBattery();
}

static void UI_DrawScreenBackground(void)
{
    u16 y;
    u16 c;
    u16 g;
    u16 b;
    for(y = 0; y < lcddev.height; y++)
    {
        g = (u16)(12 + y / 18);
        if(g > 63) g = 63;
        b = (u16)(20 + y / 20);
        if(b > 31) b = 31;
        c = (u16)((g << 5) | b);
        LCD_Fill(0, y, (u16)(lcddev.width - 1), y, c);
    }
}

static void UI_DrawBatteryIcon(uint8_t percent, uint8_t charging)
{
    u16 x0 = (u16)(lcddev.width - 44);
    u16 y0 = 8;
    u16 bw = 34;
    u16 bh = 16;
    u16 tipw = 3;
    u16 tiph = 8;
    u16 fillw;
    u16 color = GREEN;
    if(percent < 20) color = RED;
    else if(percent < 50) color = YELLOW;
    LCD_Fill(x0-2, y0-2, (u16)(x0 + bw + tipw + 2), (u16)(y0 + bh + 2), UI_COLOR_HEADER_BG);
    POINT_COLOR = WHITE;
    LCD_DrawRectangle(x0, y0, (u16)(x0 + bw), (u16)(y0 + bh));
    LCD_Fill((u16)(x0 + bw + 1), (u16)(y0 + (bh - tiph)/2), (u16)(x0 + bw + tipw), (u16)(y0 + (bh - tiph)/2 + tiph), WHITE);
    LCD_Fill((u16)(x0 + 1), (u16)(y0 + 1), (u16)(x0 + bw - 1), (u16)(y0 + bh - 1), UI_COLOR_HEADER_BG);
    if(percent > 100) percent = 100;
    fillw = (u16)((bw - 6) * percent / 100);
    if(fillw > 0){
        LCD_Fill((u16)(x0 + 3), (u16)(y0 + 3), (u16)(x0 + 3 + fillw), (u16)(y0 + bh - 3), color);
    }
    {
        char buf[6];
        u16 tx = (u16)(x0 + 5);
        u16 ty = (u16)(y0 + ((bh > 16) ? ((bh - 16)/2) : 0));
        if(percent > 99) sprintf(buf, "100");
        else sprintf(buf, "%u", percent);
        BACK_COLOR = UI_COLOR_HEADER_BG;
        POINT_COLOR = WHITE;
        LCD_ShowString(tx, ty, 24, 16, 16, (u8*)buf);
    }
    if(charging){
        u16 lx = (u16)(x0 - 10);
        u16 ly = (u16)(y0 + 3);
        POINT_COLOR = YELLOW;
        LCD_DrawLine(lx, ly, (u16)(lx + 4), (u16)(ly + 4));
        LCD_DrawLine((u16)(lx + 4), (u16)(ly + 4), (u16)(lx + 2), (u16)(ly + 4));
        LCD_DrawLine((u16)(lx + 2), (u16)(ly + 4), (u16)(lx + 6), (u16)(ly + 10));
    }
}

static void UI_UpdateBattery(void)
{
    uint8_t p = Power_GetBatteryPercent();
    uint8_t chg = Power_IsCharging();
    UI_DrawBatteryIcon(p, chg);
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
    UI_DrawCard(10, 210, 300, 70, UI_COLOR_HEADER_BG, BLACK, "Perfusion");
    UI_DrawCard(10, 290, 300, 40, UI_COLOR_ACCENT, WHITE, "Signal");
    UI_DrawCard(10, 340, 300, 130, UI_COLOR_ACCENT, WHITE, "Pulse Wave(0-4095)");

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

    /* 底部波形演示区：仅保留 IR 提示 */
    POINT_COLOR = UI_COLOR_SPO2;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    LCD_ShowString(24, 350, 140, 16, 16, (u8*)"IR (filtered)");

    /* 主界面波形区域使用底部预设矩形 */
    UI_SetWaveRect(UI_WAVE_X, UI_WAVE_Y, UI_WAVE_W, UI_WAVE_H);
    UI_WaveReset();

    BACK_COLOR = UI_COLOR_BG;
    
    // 如果手指处于脱落状态，进入主界面时恢复弹窗
    if (s_fingerOff) {
        LCD_Fill(60, 395, 260, 435, RED);
        POINT_COLOR = WHITE;
        BACK_COLOR = RED;
        LCD_ShowString(64, 403, 192, 24, 24, (u8*)"ERROR: NO FINGER");
    }
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
    LCD_ShowString(60, 440, 200, 16, 16, (u8*)"LH: Alarm Test  RH:Back");
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

void UI_DrawFilterDemoScreen(void)
{
    UI_DrawFilterDemoFrame();
    UI_WaveReset();
}

static void UI_DrawFilterDemoFrame(void)
{
    u16 frame_x1, frame_y1, frame_x2, frame_y2;
    u16 inner_w, inner_h;
    LCD_Display_Dir(1);
    LCD_Scan_Dir(s_demoScanDirList[s_demoScanDirIndex]);
    LCD_Display_Dir(1);
    
    LCD_Clear(UI_COLOR_BG);
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("Filter Demo");
    
    frame_x1 = 10;
    frame_x2 = (u16)(lcddev.width - 10);
    frame_y1 = 70;
    frame_y2 = (u16)(lcddev.height - 10);
    
    LCD_Fill(frame_x1, frame_y1, frame_x2, frame_y2, UI_COLOR_WAVE_BG);
    POINT_COLOR = UI_COLOR_BORDER;
    LCD_DrawRectangle(frame_x1, frame_y1, frame_x2, frame_y2);
    
    inner_w = (u16)(frame_x2 - frame_x1 + 1);
    inner_h = (u16)(frame_y2 - frame_y1 + 1);
    UI_SetWaveRect(frame_x1, frame_y1, inner_w, inner_h);
    
    POINT_COLOR = UI_COLOR_HINT;
    BACK_COLOR = UI_COLOR_HEADER_BG;
    LCD_ShowString(160, 12, 300, 16, 16, (u8*)"LH:Mode  LL:Noise  RL:Drift");
    
    UI_UpdateFilterDemoStatus();
}
static const char* UI_GetDemoModeText(uint8_t mode)
{
    switch(mode){
        case 0: return "IIR LPF";
        case 1: return "Detrend";
        default: return "Median";
    }
}
static void UI_UpdateFilterDemoStatus(void)
{
    char buf[32];
    /* 状态行放在 Header 下方，不参与波形区域清屏 */
    BACK_COLOR = UI_COLOR_BG;
    
    POINT_COLOR = BLUE;
    LCD_ShowString(20, 48, 110, 16, 16, (u8*)"Blue: Raw ");
    
    POINT_COLOR = UI_COLOR_SPO2;
    LCD_ShowString(130, 48, 130, 16, 16, (u8*)"Red: Filtered");
    
    POINT_COLOR = WHITE;
    sprintf(buf, "Mode:%-8s", UI_GetDemoModeText(s_demoFilterMode));
    LCD_ShowString(270, 48, 120, 16, 16, (u8*)buf);
    
    POINT_COLOR = UI_COLOR_HINT;
    sprintf(buf, "N:%s D:%s ",
            s_demoNoiseEnable ? "ON " : "OFF",
            s_demoDriftEnable ? "ON " : "OFF");
    LCD_ShowString(390, 48, 90, 16, 16, (u8*)buf);
    POINT_COLOR = WHITE;
    sprintf(buf, "Scan:%d/4 ", (int)(s_demoScanDirIndex + 1));
    LCD_ShowString(390, 30, 90, 16, 16, (u8*)buf);
    
    BACK_COLOR = UI_COLOR_BG;
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
    
    switch(s_dataReviewPage)
    {
        case 0:
            LCD_ShowString(50, 120, 180, 16, 16, (u8*)"Page 1: SPO2/PR");
            sprintf(buf, "SPO2: %d    ", s_curData.spo2);
            LCD_ShowString(50, 148, 180, 16, 16, (u8*)buf);
            sprintf(buf, "PR: %d      ", s_curData.heart_rate);
            LCD_ShowString(50, 170, 180, 16, 16, (u8*)buf);
            break;
            
        case 1:
            LCD_ShowString(50, 120, 180, 16, 16, (u8*)"Page 2: PI/R");
            sprintf(buf, "PI R:%d I:%d   ", s_curData.status, s_curData.pi);
            LCD_ShowString(50, 148, 180, 16, 16, (u8*)buf);
            sprintf(buf, "R: %d         ", s_curData.filter_status);
            LCD_ShowString(50, 170, 180, 16, 16, (u8*)buf);
            break;
            
        default:
            LCD_ShowString(50, 120, 180, 16, 16, (u8*)"Page 3: Drive");
            sprintf(buf, "PWM R:%d I:%d  ", s_curData.pwm_red, s_curData.pwm_ir);
            LCD_ShowString(50, 148, 180, 16, 16, (u8*)buf);
            sprintf(buf, "Gain: %d       ", s_curData.gain_level);
            LCD_ShowString(50, 170, 180, 16, 16, (u8*)buf);
            break;
    }
}

void UI_OnKeyLL(void) // Up
{
    switch(s_uiState)
    {
        case UI_STATE_SETTINGS:
            if(s_settingsIndex > 0)
            {
                uint8_t prev = s_settingsIndex;
                s_settingsIndex--;
                UI_DrawSettingsItem(prev, 0);
                UI_DrawSettingsItem(s_settingsIndex, 1);
            }
            break;
            
        case UI_STATE_SPO2_SET:
            UART1_SendCmd(0x02, 0); // Gain ++
            s_curData.gain_level++;
            UI_UpdateGainValue();
            break;
            
        case UI_STATE_FILTER_SET:
            UART1_SetWaveFilterEnable(1);
            UI_UpdateFilterStatus();
            break;
        case UI_STATE_FILTER_DEMO:
            s_demoNoiseEnable = !s_demoNoiseEnable;
            /* 临时增加：使用 LL 键也作为校准切换键，防止 MENU 硬件按键失效 */
            s_demoScanDirIndex = (uint8_t)((s_demoScanDirIndex + 1) % 4);
            s_needRefresh = 1;
            UI_UpdateFilterDemoStatus();
            UI_Update();
            break;
            
        case UI_STATE_AUTO_LIGHT:
            s_autoLightEnable = 1;
            UART1_SendCmd(0x05, 1);
            UI_UpdateAutoLightStatus();
            break;
            
        case UI_STATE_WORK_MODE:
            s_workMode = 0;
            UI_UpdateWorkModeStatus();
            break;
            
        case UI_STATE_ALARM_SET:
            if(s_alarmProfile < 2) s_alarmProfile++;
            UI_UpdateAlarmSetStatus();
            break;
            
        case UI_STATE_ETCO2_SET:
            s_etco2Enable = 1;
            UI_UpdateEtCO2Status();
            break;
            
        case UI_STATE_SYSTEM_SET:
            if(s_systemBrightness < 5) s_systemBrightness++;
            if(s_systemBrightness >= 3) s_systemBeepEnable = 1;
            UI_UpdateSystemSetStatus();
            break;
            
        case UI_STATE_DATA_REVIEW:
            if(s_dataReviewPage < 2) s_dataReviewPage++;
            else s_dataReviewPage = 0;
            UI_UpdateDataReviewValue();
            break;
        case UI_STATE_ALARM_TEST:
            if(s_alarmVolume < 31) s_alarmVolume++;
            UI_UpdateAlarmTestStatus();
            Speaker_SetVolume(s_alarmVolume);
            break;
            
        default: break;
    }
}

void UI_OnKeyRL(void) // Down
{
    switch(s_uiState)
    {
        case UI_STATE_SETTINGS:
            if(s_settingsIndex < SETTINGS_ITEM_COUNT - 1)
            {
                uint8_t prev = s_settingsIndex;
                s_settingsIndex++;
                UI_DrawSettingsItem(prev, 0);
                UI_DrawSettingsItem(s_settingsIndex, 1);
            }
            break;
            
        case UI_STATE_SPO2_SET:
            UART1_SendCmd(0x03, 0); // Gain --
            if(s_curData.gain_level > 0) s_curData.gain_level--;
            UI_UpdateGainValue();
            break;
            
        case UI_STATE_FILTER_SET:
            UART1_SetWaveFilterEnable(0);
            UI_UpdateFilterStatus();
            break;
        case UI_STATE_FILTER_DEMO:
            s_demoDriftEnable = !s_demoDriftEnable;
            UI_UpdateFilterDemoStatus();
            break;
            
        case UI_STATE_AUTO_LIGHT:
            s_autoLightEnable = 0;
            UART1_SendCmd(0x05, 0);
            UI_UpdateAutoLightStatus();
            break;
            
        case UI_STATE_WORK_MODE:
            s_workMode = 1;
            UI_UpdateWorkModeStatus();
            break;
            
        case UI_STATE_ALARM_SET:
            if(s_alarmProfile > 0) s_alarmProfile--;
            UI_UpdateAlarmSetStatus();
            break;
            
        case UI_STATE_ETCO2_SET:
            s_etco2Enable = 0;
            UI_UpdateEtCO2Status();
            break;
            
        case UI_STATE_SYSTEM_SET:
            if(s_systemBrightness > 1) s_systemBrightness--;
            if(s_systemBrightness < 3) s_systemBeepEnable = 0;
            UI_UpdateSystemSetStatus();
            break;
            
        case UI_STATE_DATA_REVIEW:
            if(s_dataReviewPage > 0) s_dataReviewPage--;
            else s_dataReviewPage = 2;
            UI_UpdateDataReviewValue();
            break;
        case UI_STATE_ALARM_TEST:
            if(s_alarmVolume > 0) s_alarmVolume--;
            UI_UpdateAlarmTestStatus();
            Speaker_SetVolume(s_alarmVolume);
            break;
            
        default: break;
    }
}

void UI_OnKeyLH(void) // Enter / Settings
{
    switch(s_uiState)
    {
        case UI_STATE_MAIN:
            s_uiState = UI_STATE_SETTINGS;
            s_settingsIndex = 0;
            s_needRefresh = 1;
            break;
            
        case UI_STATE_SETTINGS:
            switch(s_settingsIndex)
            {
                case 0: s_uiState = UI_STATE_WORK_MODE; break;
                case 1: s_uiState = UI_STATE_ALARM_SET; break;
                case 2: s_uiState = UI_STATE_SPO2_SET;  break;
                case 3: s_uiState = UI_STATE_FILTER_SET; break;
                case 4: s_uiState = UI_STATE_FILTER_DEMO; break;
                case 5: s_uiState = UI_STATE_R_CALIB;   break;
                case 6: s_uiState = UI_STATE_AUTO_LIGHT; break;
                case 7: s_uiState = UI_STATE_ETCO2_SET; break;
                case 8: s_uiState = UI_STATE_SYSTEM_SET; break;
                case 9: s_uiState = UI_STATE_DATA_REVIEW; break;
                default: break;
            }
            s_needRefresh = 1;
            break;
        case UI_STATE_ALARM_SET:
            s_uiState = UI_STATE_ALARM_TEST;
            s_needRefresh = 1;
            break;
        case UI_STATE_FILTER_DEMO:
            s_demoFilterMode = (uint8_t)((s_demoFilterMode + 1) % 3);
            SPO2_Algo_DemoFilterReset();
            UI_UpdateFilterDemoStatus();
            break;
        case UI_STATE_ALARM_TEST:
            s_alarmMode = (uint8_t)((s_alarmMode + 1) % 3);
            UI_UpdateAlarmTestStatus();
            Speaker_SetAlarmActive((s_alarmMode == 2) ? 1 : s_alarmActive);
            break;
            
        default: break;
    }
    UI_Update();
}

void UI_OnKeyRH(void) // Back
{
    switch(s_uiState)
    {
        case UI_STATE_SETTINGS:
            s_uiState = UI_STATE_MAIN;
            break;
            
        case UI_STATE_WORK_MODE:
        case UI_STATE_ALARM_SET:
        case UI_STATE_SPO2_SET:
        case UI_STATE_FILTER_SET:
        case UI_STATE_FILTER_DEMO:
        case UI_STATE_ALARM_TEST:
            if(s_uiState == UI_STATE_FILTER_DEMO){
                LCD_Display_Dir(0);
                LCD_Scan_Dir(R2L_U2D);
                LCD_Clear(UI_COLOR_BG);
            }
            if(s_uiState == UI_STATE_ALARM_TEST){
                if(s_alarmMode == 2){
                    s_alarmActive = 0;
                    Speaker_SetAlarmActive(0);
                }
                s_uiState = UI_STATE_ALARM_SET;
                break;
            }
        case UI_STATE_R_CALIB:
        case UI_STATE_AUTO_LIGHT:
        case UI_STATE_ETCO2_SET:
        case UI_STATE_SYSTEM_SET:
        case UI_STATE_DATA_REVIEW:
            s_uiState = UI_STATE_SETTINGS;
            break;
            
        default: break;
    }
    s_needRefresh = 1;
    UI_Update();
}

void UI_OnKeyMenu(void)
{
    if(s_uiState == UI_STATE_FILTER_DEMO)
    {
        s_demoScanDirIndex = (uint8_t)((s_demoScanDirIndex + 1) % 4);
        s_needRefresh = 1;
        UI_Update();
    }
}

void UI_DrawAlarmTestScreen(void)
{
    UI_DrawScreenBackground();
    BACK_COLOR = UI_COLOR_BG;
    UI_DrawHeader("Alarm Test");
    UI_DrawPanel(60, 120, 200, 170, UI_COLOR_ACCENT, "Alarm Config", WHITE);
    UI_UpdateAlarmTestStatus();
    POINT_COLOR = GREEN;
    LCD_ShowString(75, 220, 220, 16, 16, (u8*)"LL: Volume +");
    POINT_COLOR = RED;
    LCD_ShowString(75, 240, 220, 16, 16, (u8*)"RL: Volume -");
    POINT_COLOR = UI_COLOR_HINT;
    LCD_ShowString(60, 440, 220, 16, 16, (u8*)"LH:Mode   RH:Back");
}

static void UI_UpdateAlarmTestStatus(void)
{
    char buf[32];
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    if(s_alarmMode == 0) sprintf(buf, "Mode: OFF   ");
    else if(s_alarmMode == 1) sprintf(buf, "Mode: AUTO  ");
    else sprintf(buf, "Mode: FORCE ");
    LCD_ShowString(80, 160, 200, 16, 16, (u8*)buf);
    sprintf(buf, "Volume: %2d   ", s_alarmVolume);
    LCD_ShowString(80, 180, 200, 16, 16, (u8*)buf);
}
