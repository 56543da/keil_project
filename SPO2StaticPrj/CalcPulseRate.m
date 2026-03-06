function pulseRate = CalcPulseRate(dataIn)
% 计算脉率
%   输入参数dataIn为波形数据
%   输出参数pulseRate为脉率
%   COPYRIGHT 2018-2020 LEYUTEK. All rights reserved.

MIN_PULSE_RATE = 20; % 脉率最小值
MAX_PULSE_RATE = 120; % 脉率最大值

[~, index] = findpeaks(dataIn, 'minpeakdistance', 250, 'minpeakheight', 100);

arrX = zeros(length(index) - 1, 1); % 预分配内存
for iCnt = 1 : length(index) - 1
    arrX(iCnt) = (index(iCnt + 1) - index(iCnt));
end

medianY = median(arrX); % 求数组的中值
pulseRate = int16(30000 / medianY); % 转换为bpm为单位的脉率值，并取值为整数

if pulseRate < MIN_PULSE_RATE || pulseRate > MAX_PULSE_RATE 
    pulseRate = int16(-100); % 赋值为无效值
end  

% 绘制波形，同时显示脉率值
Fs = 500; % 采样率
T = 1 / Fs; % 采样间隔
L = length(dataIn); % 信号长度
t = (1 : L) * T; % 时间横坐标

figure; % 创建窗口
plot(t, dataIn, t(index), dataIn(index), 'ro'); % 绘制波形，同时标注最大值
set(gcf, 'name', '标定血氧红外光信号峰值'); % 设置窗口的标题名
title('血氧红外光信号原始波形'); % 标注标题
xlabel('时间(s)'); % 标注X坐标
ylabel('幅值'); % 标注Y坐标

disp(['脉率：' num2str(pulseRate) 'bpm']); % 在命令窗口显示脉率值