LCD 最终 IO 映射（简洁版）

- 控制与电源：
  - LCD_RST : PA12 (复位)
  - LCD_LED : PB0 (背光)
  - LCD_CS  : PD7
  - LCD_RS  : PD11 (D/C)
  - LCD_RD  : PD4  (RD, /RD)
  - LCD_WR  : PD5  (WR, /WR)

- 并行数据线（DB0..DB15）：
  - DB0  : PD14
  - DB1  : PD15
  - DB2  : PD0
  - DB3  : PD1
  - DB4  : PE7
  - DB5  : PE8
  - DB6  : PE9
  - DB7  : PE10
  - DB8  : PE11
  - DB9  : PE12
  - DB10 : PE13
  - DB11 : PE14
  - DB12 : PE15
  - DB13 : PD8
  - DB14 : PD9
  - DB15 : PD10

说明：
- 接口为 MCU 的 8080 并行（片选/读写/命令/数据）。
- 片选/读/写/命令均为低电平有效（与驱动中使用的宏匹配）。
- 请在物理连线确认后在 Keil 中 Clean + Build 并在硬件上做显示测试；如有异常我将根据编译或运行输出继续修正。
