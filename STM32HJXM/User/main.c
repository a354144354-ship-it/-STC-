#include "stm32f10x.h"                  // STM32F10x底层寄存器/标准库头文件
#include "Delay.h"                     // 延时驱动（ms/us延时）
#include "OLED.h"                      // OLED屏幕显示驱动
#include "Motor.h"                     // 直流电机驱动（PWM调速）
#include "Key.h"                       // 按键扫描驱动
#include "AD.h"                        // ADC模数转换驱动
#include "LED.h"                       // LED灯PWM调光驱动
#include "DD.h"                        // 蜂鸣器/继电器模块驱动（DD_turn翻转）
#include "Serial.h"                    // 串口驱动，和上位机/蓝牙模块通信
#include "string.h"                    // C字符串库，strcmp字符串比较

uint8_t Key;                // 存储按键扫描返回的键码
uint8_t MotorSpeed=0;		// 电机PWM速度 范围0~100
uint8_t LEDSpeed=0;         // LED灯PWM亮度 范围0~100
uint8_t Modeflag=0;         // 模式标志：0=手动模式，1=自动模式
uint16_t ADD_Value[2];      // ADC采样缓存数组，通道2、通道3采样值

int main(void)
{
	/* 外设模块初始化 */
	OLED_Init();		        // OLED显示屏初始化
	Motor_Init();		        // 直流电机初始化（PWM输出）
	Key_Init();			        // 按键GPIO初始化
	AD_Init();                  // ADC模数转换初始化
	DD_Init();                  // DD外设（蜂鸣器/继电器）初始化
	Serial_Init();              // 串口初始化，用于向上位机发数据、接收上位机指令
	
	/* OLED静态文字显示，只执行一次 */
	OLED_ShowString(1, 1, "Mot:");	    // 第1行第1列：电机标识
	OLED_ShowString(1, 8, "LED:");		// 第1行第8列：LED标识
	OLED_ShowString(3, 1, "Tem");		// 第3行第1列：温度标识
	OLED_ShowString(3, 7, "C");         // 温度单位℃
	OLED_ShowString(4, 1, "Lig");       // 第4行第1列：光照标识
	OLED_ShowString(2, 1, "Buz");	    // 第2行第1列：Buz蜂鸣器标识
	OLED_ShowString(3, 9, "Mode");      // 第3行第9列：模式标识
	
	while (1)  // 主循环
	{
		// 读取ADC通道2（温度）、通道3（光照）采样原始值
		ADD_Value[0] = AD_GetLatestValue(2);
        ADD_Value[1] = AD_GetLatestValue(3);
		
		// 计算并在OLED显示温度值
		OLED_ShowNum(3, 5, 4096*12/ADD_Value[0], 2);
		// 计算并在OLED显示光照等级
		OLED_ShowNum(4, 5, 4096*3/ADD_Value[1], 2);
		// OLED显示电机速度、LED亮度
		OLED_ShowNum(1, 5, MotorSpeed, 3);
		OLED_ShowNum(1, 12, LEDSpeed, 3);
		
		// 串口向上位机发送传感器数据，自定义帧协议 @T温度 @L光照 @B亮度 @F电机速度
		Serial_Printf("@T:%d\n",4096*12/ADD_Value[0]);
		Delay_ms(10);      // 延时，防止串口发送过快，APP接收数据覆盖丢失
		Serial_Printf("@L:%d\n",4096*3/ADD_Value[1]);
		Delay_ms(10);
		Serial_Printf("@B:%d\n",LEDSpeed);
		Delay_ms(10);
		Serial_Printf("@F:%d\n",MotorSpeed);
		Delay_ms(10);
		
		// 读取PB10 IO电平，检测外部传感器开关状态
		if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_10)==Bit_RESET){
			OLED_ShowString(2, 5, "run   ");
			Serial_Printf("@A:1\n");	// 串口上报状态：run运行
		}	
		else{
			OLED_ShowString(2, 5, "notrun");
			Serial_Printf("@A:0\n"); // 串口上报状态：停止notrun
		}
		
		Key = Key_GetNum(); // 扫描按键，获取按键编号，无按键返回0
		
		// 判断串口接收标志：收到上位机下发数据包
		if (Serial_RxFlag == 1)		
		{
			// 手动模式下，收到指令F，设置电机速度
			if (Serial_RxPacket[0]=='F'&&Modeflag==0)	
			{
				// 字符转数字：百位、十位、个位，解析3位速度值
				MotorSpeed=(Serial_RxPacket[2]-'0')*100+(Serial_RxPacket[3]-'0')*10+(Serial_RxPacket[4]-'0');
     			Motor_SetSpeed(MotorSpeed);	// 更新电机PWM速度
			}
			// 手动模式，收到B指令，设置LED亮度
			if (Serial_RxPacket[0]=='B'&&Modeflag==0)	
			{
				LEDSpeed=(Serial_RxPacket[2]-'0')*100+(Serial_RxPacket[3]-'0')*10+(Serial_RxPacket[4]-'0');
				LED_SetSpeed(LEDSpeed);				
			}
			// 上位机下发A:0关闭DD模块；A:1打开DD模块（仅手动模式生效）
			if (strcmp(Serial_RxPacket, "A:0")==0&&Modeflag==0)	
			{
				DD_Set(0);
			}
			else if(strcmp(Serial_RxPacket, "A:1") == 0&&Modeflag==0)
			{
				DD_Set(1);
			}
			// 上位机下发M:0，模拟按键4按下，切换自动/手动模式
			if (strcmp(Serial_RxPacket, "M:0") == 0)	
			{
				Key=4;
			}
			
			Serial_RxFlag = 0; // 清空接收标志，准备接收下一帧
		}
		
		// 按键4按下，翻转模式标志位：手动<->自动切换
		if(Key==4)
		{
			Modeflag=!Modeflag;
		}
		
		// ========== 自动模式 Modeflag !=0 ==========
		if(Modeflag!=0)
		{
			// 根据光照传感器自动控制LED亮度
			if(4096*3/ADD_Value[1]>8)
			{
				LEDSpeed=0;		// 光照强，LED关闭
			}
		    if(4096*3/ADD_Value[1]<=8&&4096*3/ADD_Value[1]>4)
			{
				LEDSpeed=50;		// 中等光照，亮度50
			}
			if(4096*3/ADD_Value[1]<=4)
			{
				 LEDSpeed=100;	// 光照弱，LED满亮度
			}
			
			// 根据温度传感器自动控制电机转速
			if(4096*12/ADD_Value[0]<26)
			{
				MotorSpeed=0;	
                DD_Set(0);				 // 温度低，电机停止
			}
			if(4096*12/ADD_Value[0]>=26&&4096*12/ADD_Value[0]<30)
			{
				MotorSpeed=30;	
                DD_Set(0);				 // 温度中等，电机低速30
			}
			if(4096*12/ADD_Value[0]>=30)
			{
				MotorSpeed=60;		// 温度高，电机60，触发DD翻转动作
				DD_Set(1);
			}
			
			// 检测PB10外部开关状态并显示
			if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_10)==Bit_RESET)
			{
				OLED_ShowString(2, 5, "run   ");	
			}
			else
			{
				OLED_ShowString(2, 5, "notrun");
			}
			
			// 刷新电机、LED的PWM输出
			Motor_SetSpeed(MotorSpeed);
			LED_SetSpeed(LEDSpeed);	
			Serial_Printf("@M:1\n");	//串口上报当前是自动模式
			OLED_ShowString(4, 9, "auto"); //OLED显示auto自动模式
		}
		
        // ========== 手动模式 Modeflag ==0 ==========
        if(Modeflag==0)	
		{	
			OLED_ShowString(4, 9, "hand");  //OLED显示hand手动模式
			if (Key == 1)					//按键1按下：电机速度+20
			{
				MotorSpeed += 20;					
				if (MotorSpeed > 100)			//超过上限100，重置为0
				{
					MotorSpeed = 0;				
				}
			}
			Motor_SetSpeed(MotorSpeed);		//更新电机PWM
			OLED_ShowNum(1, 5, MotorSpeed, 3);	
			
			if (Key == 2)					//按键2按下：LED亮度+20
			{
				LEDSpeed += 20;					
				if (LEDSpeed > 100)			
				{
					LEDSpeed = 0;				
				}
			}
			LED_SetSpeed(LEDSpeed);			
			OLED_ShowNum(1, 12, LEDSpeed, 3);
			
			Serial_Printf("@M:2\n");		//串口上报手动模式
			if (Key == 3)					//按键3按下：触发DD翻转
			{
				DD_turn();
			}
		}
	}
}
