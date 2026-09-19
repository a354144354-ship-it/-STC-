基于 STM32 的智能家庭管理系统（ FreeRTOS 裸机双版本 ） 
项目描述：设计并实现一套温度光照采集、OLED 本地显示与远程控制的智能系统，自主搭建传感器与执行器电路并设计 PCB。DMA 循环 ADC 采
集 + 均值滤波提升采样精度，PWM 调节风扇、灯光、报警器输出，自定义串口数据包协议（帧头、数据帧、帧尾）保证可靠解析；分别开发 FreeRTOS
（多任务划分、信号量同步、优先级调度）与裸机（主循环 + 中断、状态机解析数据包）两个版本对比软件架构。支持手动 / 自动双模式，自动模
式经环境参数自动调节风扇，灯光，可经自制手机 APP（HC-04 蓝牙）远程控制。<img width="3472" height="4624" alt="IMG_20260911_155433" src="https://github.com/user-attachments/assets/c0e2ef53-3108-4409-b4d0-9d8b4454dafc" />
<img width="1220" height="2712" alt="Screenshot_2026-09-11-15-55-07-598_com example hc" src="https://github.com/user-attachments/assets/e6ff59e4-3819-41e4-87c9-0ce8c3cad6e9" />
