（当前版本UI为多级菜单结构：监护主页 + 设置菜单 + 9个功能子界面，按键驱动状态机切换）

step1.完成UI界面命名与结构梳理（已完成）
step2.完成UI各界面交互说明与按键映射说明（已完成）
step3.完成UI视觉优化方案落地（已完成）
step4.补齐未实现菜单项并落地子界面（已完成）

界面架构：使用 f303vet6 负责UI渲染与按键状态管理，使用 f310c8t6 提供血氧数据与参数执行

UI状态机实现：
1. UI_STATE_MAIN：主监护界面（一级界面）
2. UI_STATE_SETTINGS：设置菜单界面（二级入口）
3. UI_STATE_WORK_MODE：工作模式界面（三级子界面）
4. UI_STATE_ALARM_SET：告警配置界面（三级子界面）
5. UI_STATE_SPO2_SET：血氧增益设置界面（三级子界面）
6. UI_STATE_FILTER_SET：波形滤波开关界面（三级子界面）
7. UI_STATE_R_CALIB：R值校准界面（三级子界面）
8. UI_STATE_AUTO_LIGHT：自动光强开关界面（三级子界面）
9. UI_STATE_ETCO2_SET：EtCO2开关界面（三级子界面）
10. UI_STATE_SYSTEM_SET：系统参数界面（三级子界面）
11. UI_STATE_DATA_REVIEW：数据回看界面（三级子界面）

界面命名与显示内容：
1. 监护主页（Monitor Dashboard）
   - 显示内容：SPO2、PR、PI(R/IR)、R值、PWM(R/IR)、Gain
   - 底部提示：LH进入菜单，RH返回
   - 刷新策略：仅更新数值区域，减少全屏重绘闪烁
2. 设置菜单（Settings Navigator）
   - 菜单项：Work Mode / Alarm Set / SpO2 Set / Filter Set / R Calib / Auto Light / EtCO2 Set / System Set / Data Review
   - 显示策略：高亮选中项 + 非选中项分层底色
3. 血氧增益设置（SpO2 Gain Set）
   - 功能：LL发送增益增加命令，RL发送增益减小命令
   - 显示：当前Gain值 + 操作提示
4. 滤波设置（Filter Set）
   - 功能：LL开启滤波，RL关闭滤波
   - 显示：当前滤波状态 ON/OFF
5. R值校准（R Calibration）
   - 功能：进入界面自动开启R校准模式，返回时关闭
   - 显示：R值、PWM(R/IR)
6. 自动光强（Auto Light）
   - 功能：LL开启自动光强，RL关闭自动光强
   - 显示：当前状态 ON/OFF
7. 工作模式（Work Mode）
   - 功能：LL切换为 Monitor，RL切换为 Service
   - 显示：当前模式状态
8. 告警设置（Alarm Set）
   - 功能：LL提高告警档位，RL降低告警档位
   - 显示：档位 + 对应 SpO2 下限、HR 上下限
9. EtCO2 设置（EtCO2 Set）
   - 功能：LL开启 EtCO2，RL关闭 EtCO2
   - 显示：当前状态 ON/OFF
10. 系统设置（System Set）
   - 功能：LL提高亮度，RL降低亮度
   - 显示：亮度等级 + 按键蜂鸣开关状态
11. 数据回看（Data Review）
   - 功能：LL下一页，RL上一页
   - 显示：
     - Page1：SPO2/PR
     - Page2：PI/R
     - Page3：PWM/Gain

按键定义与页面行为：
1. LL（上键）
   - 菜单页：上移选择
   - 参数页：执行“增加/开启”
2. RL（下键）
   - 菜单页：下移选择
   - 参数页：执行“减小/关闭”
3. LH（左高键）
   - 主页：进入设置菜单
   - 菜单页：进入当前选中子界面
4. RH（右高键）
   - 子界面：返回设置菜单
   - 设置菜单：返回主页
5. MENU
   - 预留接口，当前未绑定具体功能

状态流转：
1. MAIN --(LH)--> SETTINGS
2. SETTINGS --(LH, index=0)--> WORK_MODE
3. SETTINGS --(LH, index=1)--> ALARM_SET
4. SETTINGS --(LH, index=2)--> SPO2_SET
5. SETTINGS --(LH, index=3)--> FILTER_SET
6. SETTINGS --(LH, index=4)--> R_CALIB
7. SETTINGS --(LH, index=5)--> AUTO_LIGHT
8. SETTINGS --(LH, index=6)--> ETCO2_SET
9. SETTINGS --(LH, index=7)--> SYSTEM_SET
10. SETTINGS --(LH, index=8)--> DATA_REVIEW
11. 各子界面 --(RH)--> SETTINGS
12. SETTINGS --(RH)--> MAIN

视觉优化说明（当前版本已落地）：
1. 背景：由纯色改为纵向渐变背景，提升层次感与可读性
2. 标题栏：统一深色标题条 + 强调分隔线 + 在线状态信息
3. 主页面：指标卡片化（SPO2/PR/PI/Signal/Pulse Wave），关键值按模块分区展示
4. 菜单页：9项菜单压缩重排，保证320高度内完整可见；非选中项改为高对比文字
5. 提示区：底部统一操作提示色，降低视觉噪声
6. 脉搏波区域：主页面最下方新增 Pulse Wave 区，展示红光滤波后的实时波形

红光ADC到波形显示的逻辑链条（当前实现）：
1. F310 采集红光/红外 ADC，并发送文本帧 `"red,ir\r\n"` 到 F303
2. F303 UART1 逐字节接收并在 `ParseSPO2Packet` 中解析为 `red_val/ir_val`
3. 原始值进入 `UART1_ApplyFilter`（一阶IIR）得到 `out_red/out_ir`
4. `out_red/out_ir` 同步分发到三条链路：
   - UI 波形链路：`UI_UpdateWave(out_red)`（用于脉搏波显示）
   - 算法链路：`SPO2_Algo_PushData(...)`（或标定模式 `PushDataCalib`）
   - 串口转发链路：`UART1_ForwardWave(...)`（外部波形工具）

脉搏波显示策略（环形缓冲区与自适应展示）：
1. 数据存储：采用环形缓冲区（Ring Buffer），新数据覆盖旧数据，消除“强制清空缓冲区”导致的上位机波形瞬降现象。
2. 调度策略：滑动窗口计算。每秒（累积100点）触发一次基于最近 4s 数据的算法更新，结果输出更平滑。
3. 直流分量 DC：使用慢速指数平均跟踪（低通），得到当前基线。
4. 交流分量 AC：对 `|sample-DC|` 做包络跟踪，估计当前脉动幅值。
5. 自适应缩放：按 `norm=(sample-DC)/AC` 映射到显示高度，并限制最大摆幅。
6. 防止失真：设置最小 AC 门限，避免弱信号导致波形暴冲。
7. 滚动绘制：X方向逐点推进，满宽后清列重绘，实现稳定连续观察。
8. 时窗目标：波形区按约 5s 时窗显示，X方向采用稀疏采样点绘制以适配有限宽度。

血氧算法逻辑（心率驱动型重构）：
1. 周期锁定：首先在 4s 窗口内通过动态阈值和峰值检测算法确定心率（HR）和平均脉动周期（Period）。
2. 特征提取：根据锁定的周期，仅在最近的一个完整脉动周期内寻找红光/红外的极大值与极小值。
3. R值计算：利用该周期内的交流（AC）和直流（DC）分量计算 R 值，有效过滤呼吸漂移和运动干扰。
4. 结果平滑：对 SpO2、HR、PI 等结果进行 5 阶中值/均值混合滤波，确保数值稳定。

说明：
1. 波形显示使用“滤波后红光值（out_red）”，不是原始红光 ADC
2. 因此界面观感比原始数据更平滑，且可随灌注变化自动调整振幅显示

主界面UI区域设计（320×480，文字版）：
1. Header 区：Y=0~40
   - 标题、在线标识
2. SPO2 卡片区：Y=50~120
   - SPO2 数值
3. PR 卡片区：Y=130~200
   - PR 数值
4. PI 卡片区：Y=210~280
   - PI(R/IR) 数值
5. Signal 摘要区：Y=290~330
   - R 值、PWM、Gain
6. Pulse Wave 波形区（最下方功能区）：Y=340~470
   - 卡片标题：Pulse Wave(5s)
   - 波形绘图区：X=20~300，Y=370~460
7. 屏幕底部保留区：Y=471~479
   - 预留，不绘制文字，避免重叠与截断

布局约束（防重叠）：
1. 统计/摘要信息全部放在 Y=290~330，不进入波形区
2. 波形绘制仅在 Y=370~460 内更新，不覆盖标题与边框
3. 竖屏适配：所有坐标基于 320×480 物理分辨率，确保占满全屏。

竖屏显示约束（当前实现）：
1. 方向：竖屏（320×480）坐标系，X 向右递增，Y 向下递增
2. 布局基准：所有主界面区域按 320×480 物理分辨率重新标定，确保 UI 铺满全屏。
3. 波形区位置：固定在最下方功能区（Y=340~470），不与数值区重叠。
4. 兼容层处理：SPO2_Display 模块不再直接占用独立 LCD 区域，统一走 UI_Manager 渲染。

后续优化建议：
1. 将 Work Mode / Alarm Set / EtCO2 Set / System Set 的参数与 F310 实际控制命令打通
2. 增加参数越界保护与非法状态提示
3. 增加告警颜色策略（低血氧、心率异常时高亮）

硬件参数：WKS35178  LCD屏幕   3.5 inch
Module size (W×H×T) 55.26×84.69×2.40
Active area (W×H) 48.96×73.44
LCD Driver IC ST7796U—— ST7796U 驱动芯片
Interface Type MCU 8080 interface  ——8080 8 位并行 MCU 接口
