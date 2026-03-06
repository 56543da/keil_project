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
static SPO2Data_t s_curData = {0, 0, 0, 0}; // 缓存当前数据

#define SETTINGS_ITEM_COUNT 6
static const char* s_settingsItems[SETTINGS_ITEM_COUNT] = {
    "Work Mode",
    "Alarm Set",
    "SpO2 Set",
    "EtCO2 Set",
    "System Set",
    "Data Review"
};

void UI_DrawMainScreen(void);
void UI_DrawSettingsScreen(void);
void UI_DrawSpO2SetScreen(void);

void UI_Init(void)
{
    LCD_Clear(UI_COLOR_BG);
    s_uiState = UI_STATE_MAIN;
    s_needRefresh = 1;
    UI_Update();
}

void UI_UpdateData(SPO2Data_t *data)
{
    if(data == NULL) return;
    
    // 如果数据有变化，则更新显示
    if(s_curData.spo2 != data->spo2 || 
       s_curData.heart_rate != data->heart_rate || 
       s_curData.pi != data->pi)
    {
        s_curData = *data;
        if(s_uiState == UI_STATE_MAIN)
        {
            s_needRefresh = 1;
        }
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
    
    s_needRefresh = 0;
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
    
    // 1. SpO2 大面板 (顶部)
    // 位置: x=10, y=40, w=220, h=100
    UI_DrawPanel(10, 40, 220, 100, UI_COLOR_SPO2, "SpO2 %", BLACK);
    
    // 显示 SpO2 数值
    POINT_COLOR = UI_COLOR_SPO2;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    if(s_curData.spo2 == 0) sprintf(buf, "--");
    else sprintf(buf, "%d", s_curData.spo2);
    
    // 模拟大字体：这里用偏移多次绘制来加粗，或者只是居中显示
    // 假设最大字体是 24x24
    LCD_ShowString(80, 80, 100, 24, 24, (u8*)buf);
    
    
    // 2. PR bpm 面板 (左下)
    // 位置: x=10, y=150, w=105, h=100
    UI_DrawPanel(10, 150, 105, 100, UI_COLOR_PR, "PR bpm", BLACK);
    
    POINT_COLOR = UI_COLOR_PR;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    if(s_curData.heart_rate == 0) sprintf(buf, "--");
    else sprintf(buf, "%d", s_curData.heart_rate);
    LCD_ShowString(35, 190, 80, 24, 24, (u8*)buf);

    
    // 3. PI % 面板 (右下)
    // 位置: x=125, y=150, w=105, h=100
    UI_DrawPanel(125, 150, 105, 100, UI_COLOR_PI, "PI %", BLACK);
    
    POINT_COLOR = UI_COLOR_PI;
    BACK_COLOR = UI_COLOR_PANEL_BG;
    if(s_curData.pi == 0) sprintf(buf, "--");
    else sprintf(buf, "%d", s_curData.pi);
    LCD_ShowString(150, 190, 80, 24, 24, (u8*)buf);

    
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
        u16 y_pos = 50 + i*35;
        
        if(i == s_settingsIndex)
        {
            // 选中项背景高亮
            LCD_Fill(10, y_pos - 5, 230, y_pos + 25, UI_COLOR_SELECT);
            POINT_COLOR = WHITE;
            BACK_COLOR = UI_COLOR_SELECT; // 设置背景色为选中背景色
            LCD_DrawRectangle(10, y_pos - 5, 230, y_pos + 25);
        }
        else
        {
            // 未选中项背景
            LCD_Fill(10, y_pos - 5, 230, y_pos + 25, UI_COLOR_PANEL_BG);
            POINT_COLOR = GRAY;
            BACK_COLOR = UI_COLOR_PANEL_BG; // 设置背景色为面板背景色
        }
        
        LCD_ShowString(20, y_pos, 200, 16, 16, (u8*)s_settingsItems[i]);
        
        // Draw arrow
        if(i == s_settingsIndex)
        {
             LCD_ShowString(210, y_pos, 20, 16, 16, (u8*)">");
        }
    }
    
    // Footer
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG; // 恢复背景色
    LCD_ShowString(10, 300, 200, 16, 16, (u8*)"UP/DN: Select  SET: Enter");
    
    BACK_COLOR = original_back_color; // 恢复原始背景色
}

void UI_DrawSpO2SetScreen(void)
{
    char buf[32];
    LCD_Clear(UI_COLOR_BG);
    BACK_COLOR = UI_COLOR_BG; // 确保清除屏幕后背景色一致
    UI_DrawHeader("SpO2 Gain Set");
    
    // Gain Control Panel
    UI_DrawPanel(40, 80, 160, 120, UI_COLOR_SPO2, "Gain Level", BLACK);
    
    // 显示增益值 (当前无法获取真实值，显示Adjusting...)
    // 或者我们可以在本地维护一个影子变量，但最安全的是只显示操作提示
    POINT_COLOR = WHITE;
    BACK_COLOR = UI_COLOR_BG;
    LCD_ShowString(60, 130, 120, 16, 16, (u8*)"Adjust Gain");
    
    // Instructions
    POINT_COLOR = GREEN;
    LCD_ShowString(60, 160, 120, 16, 16, (u8*)"UP: Increase");
    POINT_COLOR = RED;
    LCD_ShowString(60, 180, 120, 16, 16, (u8*)"DN: Decrease");
    
    // Footer
    POINT_COLOR = GRAY;
    LCD_ShowString(60, 290, 200, 16, 16, (u8*)"Press BACK to Exit");
}



void UI_OnKeyLL(void) // Up
{
    if(s_uiState == UI_STATE_SETTINGS)
    {
        if(s_settingsIndex > 0)
        {
            s_settingsIndex--;
            s_needRefresh = 1;
        }
    }
    else if(s_uiState == UI_STATE_SPO2_SET)
    {
        // Increase Gain
        UART1_SendCmd(0x02, 0); 
    }
    UI_Update();
}

void UI_OnKeyRL(void) // Down
{
    if(s_uiState == UI_STATE_SETTINGS)
    {
        if(s_settingsIndex < SETTINGS_ITEM_COUNT - 1)
        {
            s_settingsIndex++;
            s_needRefresh = 1;
        }
    }
    else if(s_uiState == UI_STATE_SPO2_SET)
    {
        // Decrease Gain
        UART1_SendCmd(0x03, 0); 
    }
    UI_Update();
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
    UI_Update();
}

void UI_OnKeyMenu(void)
{
    // Reserved
}
