function dataOut = IIRFilterPulseWave(dataIn)
% 使用IIR滤波器进行滤波
%   输入参数dataIn为滤波前的数据
%   输出参数dataOut为滤波后的数据
%   COPYRIGHT 2018-2020 LEYUTEK. All rights reserved.

degree = 4; % 滤波器阶数
Fs = 500;   % 采样频率
T = 1 / Fs; % 采样间隔
Fc = 50;    % 截止频率
Wn = Fc / (Fs / 2); % 归一化截止频率，Wn = 0.2
[b, a] = butter(degree, Wn, 'low'); % 设计巴特沃兹低通滤波器
dataOut = filtfilt(b, a, dataIn); % 滤波

L = length(dataIn); % 数据长度 
t = (1 : L) * T; % 时间横坐标

figure; % 创建窗口
set(gcf, 'name', 'IIR滤波器对血氧红外光信号进行低通滤波'); % 设置窗口的标题名
subplot(2,1,1);  % 将figure划分为2*1，在第1块创建坐标系
plot(t, dataIn); % 绘制原始波形
title('血氧红外光信号原始波形'); % 标注标题
xlabel('时间(s)'); % 标注X坐标
ylabel('幅值'); % 标注Y坐标

subplot(2,1,2); % 将figure划分为2*1，在第2块创建坐标系
plot(t, dataOut); % 绘制滤波后波形
title('IIR滤波器滤波后波形'); % 标注标题
xlabel('时间(s)'); % 标注X坐标
ylabel('幅值'); % 标注Y坐标
