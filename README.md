# -STM32-OV7670-
基于 STM32 和 OV7670 摄像头模块的嵌入式人脸检测与识别实践项目。/A hands-on embedded face detection and recognition project based on STM32 and the OV7670 camera module.
# 主要功能：
OV7670 实时图像采集

TFT LCD 实时显示摄像头画面

图像灰度化、降采样、滤波处理

人脸检测与人脸区域定位

简单人脸识别/模板匹配

人脸录入与特征保存

按键控制与串口调试

识别结果 LCD 显示与串口输出
# 硬件
STM32F407 / STM32F411 等

OV7670 摄像头模块（此为无FIFO 版本，建议使用带 FIFO 版本）

TFT LCD，如 ILI9341、ST7789 等

LED、串口

可选：SD 卡、外部 SRAM、外部 Flash
# 采集印证流程
初始化 OV7670、LCD、串口等外设

采集一帧图像

图像灰度化、降采样和滤波

人脸检测，定位候选人脸区域

提取人脸特征

与已录入的人脸特征进行比对

输出识别结果
