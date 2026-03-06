function spo2 = CalcSPO2(rawIRData, rawRedData)
% 计算血氧饱和度
%   输入参数rawIRData为红外光波形数据
%   输入参数rawRedData为红光波形数据
%   输出参数spo2为血氧饱和度
%   COPYRIGHT 2018-2020 LEYUTEK. All rights reserved.

RR_TABLE = [580, 620, 650, 670, 700, 730, 760, 780, 810, 840, 880];
minIRData = min(rawIRData); % 计算红外光最小值
maxIRData = max(rawIRData); % 计算红外光最大值
irADRng = maxIRData - minIRData; % 计算峰峰值之差

minRedData = min(rawRedData); % 计算红光最小值
maxRedData = max(rawRedData); % 计算红光最大值
redADRng = maxRedData - minRedData; % 计算峰峰值之差

if irADRng > 0   
    rVal = redADRng * 1000 / irADRng; % 计算R值
end

index = 1; % R值对应的索引值初始值为1

while (rVal >= RR_TABLE(index)) && (index < 11)
    index = index + 1; % 计算R值对应的索引值
end

spo2 = 101 - index; % 计算血氧饱和度

if spo2 == 100
    spo2 = 99;
end

disp(['血氧饱和度：' num2str(spo2) '%']); % 显示血氧饱和度
