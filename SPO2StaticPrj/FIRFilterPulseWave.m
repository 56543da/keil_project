function dataOut = FIRFilterPulseWave(dataIn)
% 使用FIR滤波器进行滤波
%   输入参数dataIn为滤波前的数据
%   输出参数dataOut为滤波后的数据
%   COPYRIGHT 2018-2020 LEYUTEK. All rights reserved.

degree = 4; % 滤波器阶数
Fs = 500;   % 采样频率
T = 1 / Fs; % 采样间隔
Fc = 62.5;  % 截止频率
Wc = Fc / (Fs / 2); % 归一化截止频率，Wc = 0.25
b = fir1(degree, Wc); % 设计低通滤波器
dataOut = filtfilt(b, 1, dataIn); % 滤波

L = length(dataIn); % 数据长度 
t = (1 : L) * T; % 时间横坐标

figure; % 创建窗口
set(gcf, 'name', 'FIR滤波器对血氧红外光信号进行低通滤波'); % 设置窗口的标题名
subplot(2, 1, 1); % 将figure划分为2*1，在第1块创建坐标系
plot(t, dataIn);  % 绘制原始波形
title('血氧红外光信号原始波形'); % 标注标题
xlabel('时间(s)'); % 标注X坐标
ylabel('幅值'); % 标注Y坐标

subplot(2, 1, 2); % 将figure划分为2*1，在第2块创建坐标系
plot(t, dataOut); % 绘制滤波后波形
title('FIR滤波器滤波后波形'); % 标注标题
xlabel('时间(s)'); % 标注X坐标
ylabel('幅值'); % 标注Y坐标
