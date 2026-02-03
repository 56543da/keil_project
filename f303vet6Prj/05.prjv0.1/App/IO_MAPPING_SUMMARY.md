# GD32F303 SPO2项目 IO映射总结

## RGB LED映射

| LED颜色 | GPIO端口 | 引脚 | 功能说明 | 控制逻辑 |
|---------|----------|------|----------|----------|
| 绿色LED  | GPIOA    | PA8  | LED_G    | 按KEY1控制 |
| 黄色LED  | GPIOC    | PC6  | LED_Y    | 按KEY2控制 |
| 红色LED  | GPIOC    | PC7  | LED_R    | 按KEY3控制 |

**LED控制说明：**
- 高电平点亮，低电平熄灭
- 初始化后默认熄灭状态
- 通过LEDFlicker()函数实现闪烁效果

## 按键映射（中断触发）

| 按键名称 | GPIO端口 | 引脚 | 触发方式 | 中断线 | 控制对象 |
|---------|----------|------|----------|--------|----------|
| KEY1    | GPIOE    | PE3  | 双边沿触发 | EXTI3  | 绿色LED  |
| KEY2    | GPIOE    | PE4  | 下降沿触发 | EXTI4  | 黄色LED  |
| KEY3    | GPIOE    | PE5  | 下降沿触发 | EXTI5  | 红色LED  |

**按键中断说明：**
- 中断优先级：抢占优先级2，子优先级2
- KEY1配置为输入上拉（高电平有效），双边沿触发检测按下/释放
- KEY2/KEY3配置为输入下拉（低电平有效），下降沿触发检测按下
- 中断服务函数：EXTI3_IRQHandler、EXTI4_IRQHandler、EXTI9_5_IRQHandler
- 无需周期性扫描，响应更快，CPU占用更低

## LCD接口映射（8080并行接口）

### 控制线
| 信号名 | GPIO端口 | 引脚 | 功能说明 |
|--------|----------|------|----------|
| LCD_RST| GPIOA    | PA12 | 复位信号（低电平复位）|
| LCD_LED| GPIOB    | PB0  | 背光控制（高电平开启）|
| LCD_CS | GPIOD    | PD7  | 片选信号（低电平有效）|
| LCD_RS | GPIOD    | PD11 | 数据/命令选择（高=数据，低=命令）|
| LCD_WR | GPIOD    | PD5  | 写使能（低电平有效）|
| LCD_RD | GPIOD    | PD4  | 读使能（低电平有效）|

### 数据线（16位并行）
| 数据线 | GPIO端口 | 引脚 | 位位置 |
|--------|----------|------|--------|
| DB0    | GPIOD    | PD14 | bit0   |
| DB1    | GPIOD    | PD15 | bit1   |
| DB2    | GPIOD    | PD0  | bit2   |
| DB3    | GPIOD    | PD1  | bit3   |
| DB4    | GPIOE    | PE7  | bit4   |
| DB5    | GPIOE    | PE8  | bit5   |
| DB6    | GPIOE    | PE9  | bit6   |
| DB7    | GPIOE    | PE10 | bit7   |
| DB8    | GPIOE    | PE11 | bit8   |
| DB9    | GPIOE    | PE12 | bit9   |
| DB10   | GPIOE    | PE13 | bit10  |
| DB11   | GPIOE    | PE14 | bit11  |
| DB12   | GPIOE    | PE15 | bit12  |
| DB13   | GPIOD    | PD8  | bit13  |
| DB14   | GPIOD    | PD9  | bit14  |
| DB15   | GPIOD    | PD10 | bit15  |

**LCD驱动支持：**
- ST7789 (ID: 0x7789)
- ST7796 (ID: 0x7796) 
- NT35310 (ID: 0x5310)
- NT35510 (ID: 0x5510)

## 重要说明

1. **按键中断改造：** 已将按键改为中断触发模式，响应更快，CPU占用更低
   - KEY1支持按下/释放检测，KEY2/KEY3检测按下事件
   - 移除了周期性按键扫描任务，简化主循环逻辑

2. **LCD显示修复：** 
   - 已修复LCD_WriteDataPort()和LCD_ReadDataPort()函数中的位映射错误
   - 调整了扫描方向和Memory Access Control寄存器，修复字体倒置问题
   - 优化了ST7789/ST7796驱动芯片的初始化序列

3. **调试信息：** 可通过UART0(115200)查看LCD ID读取和按键中断触发状态

## 使用注意事项

- 所有GPIO均已正确配置时钟和模式
- LCD接口使用50MHz高速输出模式
- 按键使用输入下拉模式，避免悬空状态
- 如遇显示异常，首先检查LCD ID是否正确读取