function [x, y] = CalcAmpSpec(dataIn)
% 计算血氧红外光信号的归一化幅度谱
%   输入参数dataIn为脉搏波的采样值
%   输出参数x和y分别为幅度谱的横坐标序列，以及与横坐标序列对应的归一化幅度
%   COPYRIGHT 2018-2020 LEYUTEK. All rights reserved.

Fs = 500; % 采样频率       
T = 1 / Fs; % 采样间隔
L = length(dataIn); % 信号长度
halfL = round(L / 2); % 幅度谱是对称的，因此只需要绘制一半即可
fftRslt = fft(dataIn - nanmean(dataIn)); % 去除直流分量，然后再进行傅里叶变换
absData = abs(fftRslt); % 计算幅度谱
maxVal  = max(absData); % 计算最大值  

x = Fs * (0 : halfL) / L; % 横坐标为频率值，单位为Hz
y = absData(1 : halfL + 1) / maxVal; % 纵坐标为归一化幅度值，最大值为1 

t = (1 : L) * T; % 时间横坐标
figure; % 创建窗口
set(gcf, 'name', '血氧红外光信号的时域波形及幅度谱'); % 设置窗口的标题名
subplot(2, 1, 1); % 将figure划分为2*1，在第1块创建坐标系
plot(t, dataIn); % 绘制波形
title('血氧红外光信号原始波形'); % 标注标题
xlabel('时间(s)'); % 标注X坐标
ylabel('幅值'); % 标注Y坐标

subplot(2, 1, 2); % 将figure划分为2*1，在第2块创建坐标系
plot(x, y); % 绘制幅度谱
axis([0 60 0 1]); % 变换坐标，只显示0-10Hz即可
title('血氧红外光信号归一化幅度谱'); % 标注标题
xlabel('频率(Hz)'); % 标注X坐标
ylabel('幅度'); % 标注Y坐标
