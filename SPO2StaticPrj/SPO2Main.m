% 血氧控制与血氧信号处理实验脚本文件
%   设计IIR滤波器和FIR滤波器并对原始的血氧红外光信号进行滤波，然后计算脉率和血氧饱和度，
%   最后，计算血氧红外光信号的幅度谱
%   COPYRIGHT 2018-2020 LEYUTEK. All rights reserved.

rawIRData = xlsread('血氧0x33演示数据-01.csv', 'A1 : A5000'); % 读取红外光波形数据
rawRedData = xlsread('血氧0x33演示数据-01.csv', 'B1 : B5000'); % 读取红光波形数据

IIRFilterPulseWave(rawIRData); % 设计IIR滤波器对原始的血氧红外光信号进行滤波
FIRFilterPulseWave(rawIRData); % 设计FIR滤波器对原始的血氧红外光信号进行滤波
CalcPulseRate(rawIRData); % 计算脉率
CalcSPO2(rawIRData, rawRedData); % 计算血氧饱和度
CalcAmpSpec(rawIRData); % 计算血氧红外光信号的幅度谱
