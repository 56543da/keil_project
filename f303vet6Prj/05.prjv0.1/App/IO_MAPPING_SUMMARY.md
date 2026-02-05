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
| MENU | KEY_NAME_MENU | GPIOC | PC9 | 下降沿触发 | EXTI9 | 应用逻辑 |

**按键中断说明：**
- 中断优先级：抢占优先级2，子优先级2
- 所有按键配置为 **输入上拉 (IPU)** 模式
- 所有按键使用 **下降沿触发** 检测按下事件
- 中断服务函数：
  - `EXTI3_IRQHandler` (KEY_LL)
  - `EXTI4_IRQHandler` (KEY_RL)
  - `EXTI5_9_IRQHandler` (LEFT_HIGH & MENU)
  - `EXTI10_15_IRQHandler` (RIGHT_HIGH)
- 软件消抖：在2ms定时任务中进行状态机消抖 (5个tick)

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

## 重要说明

1. **按键系统更新：**
   - 新增了 `RIGHT_HIGH` (PC13) 和 `MENU` (PC9) 按键
   - 所有按键统一使用 **中断下降沿触发** + **定时器消抖** 机制
   - 解决了中断资源共享问题 (EXTI5_9, EXTI10_15)

2. **LCD显示：**
   - 接口保持不变，使用高性能8080并口驱动

3. **调试信息：**
   - 可通过UART0(115200)查看LCD ID读取和按键状态
