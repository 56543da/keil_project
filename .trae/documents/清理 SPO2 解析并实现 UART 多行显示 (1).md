# 清理 SPO2 解析并实现 UART 多行显示

## 1. 移除 SPO2 解析逻辑
我们将彻底删除 F303 工程中所有与血氧协议解析相关的代码，使 UART1 变为纯粹的透传接收模式。

### 修改 `HW/UART1/UART1.c`
- 删除 `ParseSPO2Packet` 函数及其相关状态变量 (`s_parseState`, `s_packetBuffer` 等)。
- 删除 `UART1_ProcessSPO2Data` 函数。
- 删除 `CalculateChecksum` 函数。
- 保留基础的 `InitUART1`, `ReadUART1`, `UART1_IRQHandler` 等驱动函数。

### 修改 `HW/UART1/UART1.h`
- 删除 `UART1_ProcessSPO2Data` 的声明。

## 2. 实现 LCD 多行滚动显示
为了完整显示接收到的 UART 数据（包括换行），我们需要在 `Main.c` 中实现一个简单的文本终端显示逻辑。

### 修改 `App/Main/Main.c`
- **缓冲区管理**: 定义一个较大的行缓冲区或字符数组，用于存储最近接收到的 N 行数据。
- **显示逻辑**:
    - 在 `Proc1SecTask` 中读取 UART1 数据。
    - 将数据追加到显示缓冲区。
    - 处理换行符 `\n`：当遇到换行或行满时，将屏幕内容向上滚动，并在最下方显示新内容。
    - 使用 `LCD_ShowString` 逐行显示缓冲区内容。

## 3. 清理残留代码 (可选)
- 检查并移除 `UART0.c/h` 中定义的全局变量 `g_spo2Data`，因为它不再被使用。

这个计划将使 F303 变成一个简单的 "串口显示屏"，能够如实反映 F310 发送的所有数据，非常适合调试通信链路。