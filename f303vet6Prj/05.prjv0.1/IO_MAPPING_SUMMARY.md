# GD32F303 SPO2项目 IO映射总结

## RGB LED映射

| LED颜色 | GPIO端口 | 引脚 | 功能说明 | 控制逻辑 |
| :--- | :--- | :--- | :--- | :--- |
| 绿色LED | GPIOA | PA8 | LED_G | 按KEY_LL(KEY1)控制 |
| 黄色LED | GPIOC | PC6 | LED_Y | 按KEY_RL(KEY2)控制 |
| 红色LED | GPIOC | PC7 | LED_R | 按KEY_LH控制 |

**LED控制说明：**
- 高电平点亮，低电平熄灭
- 初始化后默认熄灭状态
- 通过按键中断回调函数实现翻转

## 按键映射（中断触发）

| 按键名称 | 代码定义 | GPIO端口 | 引脚 | 触发方式 | 中断线 | 控制对象 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| KEY1 | KEY_NAME_LL | GPIOE | PE3 | 下降沿触发 | EXTI3 | 绿色LED翻转 |
| KEY2 | KEY_NAME_RL | GPIOE | PE4 | 下降沿触发 | EXTI4 | 黄色LED翻转 |
| LEFT_HIGH | KEY_NAME_LEFT_HIGH | GPIOE | PE5 | 下降沿触发 | EXTI5 | 红色LED翻转 |
| RIGHT_HIGH | KEY_NAME_RIGHT_HIGH | GPIOC | PC13 | 下降沿触发 | EXTI13 | 应用逻辑 |

**按键中断说明：**
- 中断优先级：抢占优先级2，子优先级2
- 所有按键配置为 **输入上拉 (IPU)** 模式
- 所有按键使用 **下降沿触发** 检测按下事件
- 中断服务函数：
  - `EXTI3_IRQHandler` (KEY_LL)
  - `EXTI4_IRQHandler` (KEY_RL)
  - `EXTI5_9_IRQHandler` (LEFT_HIGH & SW_KEY 电源键)
  - `EXTI10_15_IRQHandler` (RIGHT_HIGH)
- 软件消抖：在2ms定时任务中进行状态机消抖 (5个tick)

## 电源与充电管理 (Power 模块)

| 信号名 | GPIO端口 | 引脚 | 功能说明 | 控制逻辑 |
| :--- | :--- | :--- | :--- | :--- |
| PWR_EN | GPIOC | PC9 | 主电源使能控制 | 推挽输出。开机后持续输出高电平开启 5V，关机时拉低断电 |
| SW_KEY | GPIOC | PC8 | 电源开关按键 | 上拉输入，外部中断(EXTI8)下降沿触发。软件消抖 20ms 后执行关机 |
| CHRG   | GPIOA | PA0 | 充电状态指示 | 输入。低电平表示正在充电 |
| STDBY  | GPIOA | PA1 | 充电完成指示 | 输入。低电平表示充电完成/待机 |
| BAT_DET| GPIOC | PC1 | 电池电压检测 | ADC输入 (ADC0_CH11)。检测电池电量百分比 |
| USB_DET| GPIOC | PC2 | USB插入检测 | ADC输入 (ADC0_CH12)。高电平(2.5V)表示已插入USB，低电平表示拔出 |

**电源控制机制：**
- **开机**：长按 SW_KEY 硬件触发开机，单片机启动后立刻拉高 PWR_EN (PC9) 维持电源开启。
- **关机**：开机状态下按下 SW_KEY，触发 EXTI8 中断，在 SysTick (2ms) 中进行 20ms 软件消抖后，单片机拉低 PWR_EN (PC9)，切断主升压芯片，实现彻底关机。
- **ADC采样**：在主循环中轮询切换通道 11 和 12 读取电池和 USB 电压值，并做简单的平滑滤波。

## LCD接口映射（8080并行接口）

### 控制线

| 信号名 | GPIO端口 | 引脚 | 功能说明 |
| :--- | :--- | :--- | :--- |
| LCD_RST | GPIOA | PA12 | 复位信号（低电平复位） |
| LCD_LED | GPIOB | PB0 | 背光控制（高电平开启） |
| LCD_CS | GPIOD | PD7 | 片选信号（低电平有效） |
| LCD_RS | GPIOD | PD11 | 数据/命令选择（高=数据，低=命令） |
| LCD_WR | GPIOD | PD5 | 写使能（低电平有效） |
| LCD_RD | GPIOD | PD4 | 读使能（低电平有效） |

### 数据线（16位并行）

| 数据线 | GPIO端口 | 引脚 | 位位置 |
| :--- | :--- | :--- | :--- |
| DB0 | GPIOD | PD14 | bit0 |
| DB1 | GPIOD | PD15 | bit1 |
| DB2 | GPIOD | PD0 | bit2 |
| DB3 | GPIOD | PD1 | bit3 |
| DB4 | GPIOE | PE7 | bit4 |
| DB5 | GPIOE | PE8 | bit5 |
| DB6 | GPIOE | PE9 | bit6 |
| DB7 | GPIOE | PE10 | bit7 |
| DB8 | GPIOE | PE11 | bit8 |
| DB9 | GPIOE | PE12 | bit9 |
| DB10 | GPIOE | PE13 | bit10 |
| DB11 | GPIOE | PE14 | bit11 |
| DB12 | GPIOE | PE15 | bit12 |
| DB13 | GPIOD | PD8 | bit13 |
| DB14 | GPIOD | PD9 | bit14 |
| DB15 | GPIOD | PD10 | bit15 |

**LCD驱动支持：**
- ST7789 (ID: 0x7789)
- ST7796 (ID: 0x7796)
- NT35310 (ID: 0x5310)
- NT35510 (ID: 0x5510)

## 音频与音量控制

### 模拟音频输出

| 信号名 | GPIO端口 | 引脚 | 功能说明 |
| :--- | :--- | :--- | :--- |
| DAC_OUT | GPIOA | PA4 | DAC 模拟输出，驱动外部功放 |

### 数字音量控制（AD5160）

| 信号名 | GPIO端口 | 引脚 | 连接说明 |
| :--- | :--- | :--- | :--- |
| 5160_CLK | GPIOB | PB13 | SPI 时钟 (SCK) |
| 5160_CS  | GPIOB | PB14 | SPI 片选 (NSS) |
| 5160_SDI | GPIOB | PB15 | SPI 数据输入 (MOSI) |

### 功放

- 模块：FM8002A 音频功放
- 音量：通过 AD5160 数字电位器调节（SPI 控制）

## 重要说明

1. **按键与电源系统更新：**
   - 移除了原有的 `MENU` (PC9) 按键，将 PC9 分配给 `PWR_EN`（主电源使能）
   - 新增 `SW_KEY` (PC8) 作为电源开关按键，复用 EXTI5_9 中断
   - 新增 `BAT_DET` (PC1), `USB_DET` (PC2), `CHRG` (PA0), `STDBY` (PA1) 用于充电和电量管理
   - 解决了中断资源共享问题 (EXTI5_9 负责 LEFT_HIGH 和 SW_KEY)

2. **LCD显示：**
   - 接口保持不变，使用高性能8080并口驱动

3. **调试信息：**
   - 可通过UART0(115200)查看LCD ID读取和按键状态
