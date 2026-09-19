
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "Delay.h"
#include "OLED.h"
#include "Motor.h"
#include "Key.h"
#include "AD.h"
#include "LED.h"
#include "DD.h"
#include "Serial.h"
#include "string.h"

/* ===================== 任务句柄 ===================== */
TaskHandle_t SensorTask_Handle  = NULL;
TaskHandle_t OLEDTask_Handle    = NULL;
TaskHandle_t SerialTask_Handle  = NULL;
TaskHandle_t KeyTask_Handle     = NULL;
TaskHandle_t ControlTask_Handle = NULL;

/* ===================== 队列 / 互斥锁 ===================== */
QueueHandle_t Key_Queue;          /* 按键事件队列：Key任务 -> Control任务 */
SemaphoreHandle_t OLED_Mutex;     /* OLED 显示互斥锁 */
SemaphoreHandle_t Serial_Mutex;   /* 串口发送互斥锁 */

/* ===================== 任务间共享数据（volatile） ===================== */
volatile uint16_t ADD_Value[2];   /* ADC采样值 [0]=温度(通道2) [1]=光照(通道3) */
volatile uint8_t  MotorSpeed = 0; /* 电机PWM速度 0~100 */
volatile uint8_t  LEDSpeed   = 0; /* LED亮度 0~100 */
volatile uint8_t  Modeflag   = 0; /* 0=手动模式 1=自动模式 */
volatile uint8_t  RunFlag    = 0; /* PB10外部开关状态 1=run 0=notrun */

/* =====================================================================
 * 任务1：传感器采集  100ms
 * =====================================================================*/
void Sensor_Task(void *arg)
{
	for (;;)
	{
		ADD_Value[0] = AD_GetLatestValue(2);          /* 温度通道2 */
		ADD_Value[1] = AD_GetLatestValue(3);          /* 光照通道3 */
		if (ADD_Value[0] == 0) ADD_Value[0] = 1;      /* 防除0（原代码隐患） */
		if (ADD_Value[1] == 0) ADD_Value[1] = 1;
		RunFlag = (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == Bit_RESET) ? 1 : 0;
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

/* =====================================================================
 * 任务2：OLED 显示  200ms
 * =====================================================================*/
void OLED_Task(void *arg)
{
	for (;;)
	{
		xSemaphoreTake(OLED_Mutex, portMAX_DELAY);
		OLED_ShowNum(3, 5, 4096*12/ADD_Value[0], 2);   /* 温度 */
		OLED_ShowNum(4, 5, 4096*3/ADD_Value[1], 2);    /* 光照 */
		OLED_ShowNum(1, 5, MotorSpeed, 3);             /* 电机速度 */
		OLED_ShowNum(1, 12, LEDSpeed, 3);              /* LED亮度 */
		OLED_ShowString(2, 5, RunFlag ? "run   " : "notrun");
		OLED_ShowString(4, 9, Modeflag ? "auto" : "hand");
		xSemaphoreGive(OLED_Mutex);
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

/* =====================================================================
 * 任务3：串口上报  100ms
 * 说明：串口驱动与APP约定按帧间隔>=10ms，故在锁内保持原节奏；
 *       若未来多任务都要打印，应改为"每帧单独加锁"避免长时间占锁。
 * =====================================================================*/
void Serial_Task(void *arg)
{
	for (;;)
	{
		xSemaphoreTake(Serial_Mutex, portMAX_DELAY);
		Serial_Printf("@T:%d\n", 4096*12/ADD_Value[0]);
	vTaskDelay(pdMS_TO_TICKS(10));//防止发送过快数据覆盖
		Serial_Printf("@L:%d\n", 4096*3/ADD_Value[1]);
	vTaskDelay(pdMS_TO_TICKS(10));
		Serial_Printf("@B:%d\n", LEDSpeed);
		vTaskDelay(pdMS_TO_TICKS(10));
		Serial_Printf("@F:%d\n", MotorSpeed);
		vTaskDelay(pdMS_TO_TICKS(10));
		Serial_Printf("@A:%d\n", RunFlag);
		Serial_Printf("@M:%d\n", Modeflag ? 1 : 2);
		xSemaphoreGive(Serial_Mutex);
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

/* =====================================================================
 * 任务4：按键扫描  20ms
 * =====================================================================*/
void Key_Task(void *arg)
{
	uint8_t key;
	for (;;)
	{
		key = Key_GetNum();
		if (key != 0)
		{
			xQueueSend(Key_Queue, &key, 0);   /* 非阻塞发送，队列满则丢弃本次 */
		}
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}

/* =====================================================================
 * 任务5：控制核心 —— 模式切换 / 指令解析 / 自动闭环 / 输出控制
 * =====================================================================*/
void Control_Task(void *arg)
{
	uint8_t key = 0;
	char pkt[8];
	uint8_t i;

	for (;;)
	{
		/* 等待按键事件（20ms超时，期间顺带轮询串口指令） */
		if (xQueueReceive(Key_Queue, &key, pdMS_TO_TICKS(20)) == pdTRUE)
		{
			if (key == 4)                    /* 按键4：手动<->自动切换 */
			{
				Modeflag = !Modeflag;
			}
			else if (Modeflag == 0)          /* 仅手动模式下按键有效 */
			{
				if (key == 1)                /* 电机速度+20 */
				{
					MotorSpeed += 20;
					if (MotorSpeed > 100) MotorSpeed = 0;
					Motor_SetSpeed(MotorSpeed);
				}
				else if (key == 2)           /* LED亮度+20 */
				{
					LEDSpeed += 20;
					if (LEDSpeed > 100) LEDSpeed = 0;
					LED_SetSpeed(LEDSpeed);
				}
				else if (key == 3)
				{
					DD_turn();               /* 蜂鸣器/继电器翻转 */
				}
			}
		}

		/* ② 处理上位机指令（轮询串口接收标志） */
		if (Serial_RxFlag == 1)
		{
			
			for (i = 0; i < 7 && Serial_RxPacket[i]; i++)
				pkt[i] = (char)Serial_RxPacket[i];
			pkt[i] = 0;
			while (i > 0 && (pkt[i-1] == '\n' || pkt[i-1] == '\r'))
				pkt[--i] = 0;

			if (strcmp(pkt, "M:0") == 0)     /* 上位机切换模式 */
			{
				Modeflag = !Modeflag;
			}
			else if (Modeflag == 0)          /* 以下指令仅手动模式生效 */
			{
				if (pkt[0] == 'F')           /* Fxxx：电机速度 */
				{
					MotorSpeed = (pkt[2]-'0')*100 + (pkt[3]-'0')*10 + (pkt[4]-'0');
					Motor_SetSpeed(MotorSpeed);
				}
				else if (pkt[0] == 'B')      /* Bxxx：LED亮度 */
				{
					LEDSpeed = (pkt[2]-'0')*100 + (pkt[3]-'0')*10 + (pkt[4]-'0');
					LED_SetSpeed(LEDSpeed);
				}
				else if (strcmp(pkt, "A:0") == 0) { DD_Set(0); }
				else if (strcmp(pkt, "A:1") == 0) { DD_Set(1); }
			}
			Serial_RxFlag = 0;               /* 清接收标志 */
		}

		/* ③ 自动模式：传感器闭环控制（约20ms刷新一次） */
		if (Modeflag != 0)
		{
			uint16_t Light = 4096*3/ADD_Value[1];
			uint16_t Temp  = 4096*12/ADD_Value[0];

			if      (Light > 8)       LEDSpeed = 0;    /* 光照强 -> 关 */
			else if (Light > 4)       LEDSpeed = 50;   /* 中等 -> 50 */
			else                      LEDSpeed = 100;  /* 弱 -> 满亮度 */

			if      (Temp < 26)       { MotorSpeed = 0;  DD_Set(0); }
			else if (Temp < 30)       { MotorSpeed = 30; DD_Set(0); }
			else                      { MotorSpeed = 60; DD_Set(1); }

			Motor_SetSpeed(MotorSpeed);
			LED_SetSpeed(LEDSpeed);
		}
	}
}

/* =====================================================================
 * main：外设初始化 -> 创建同步原语 -> 创建任务 -> 启动调度器
 * =====================================================================*/
int main(void)
{
	/* 外设模块初始化（与裸机版一致） */
	OLED_Init();
	Motor_Init();
	Key_Init();
	AD_Init();
	DD_Init();
	Serial_Init();

	/* OLED 静态文字，只执行一次 */
	OLED_ShowString(1, 1, "Mot:");
	OLED_ShowString(1, 8, "LED:");
	OLED_ShowString(3, 1, "Tem");
	OLED_ShowString(3, 7, "C");
	OLED_ShowString(4, 1, "Lig");
	OLED_ShowString(2, 1, "Buz");
	OLED_ShowString(3, 9, "Mode");

	/* 创建互斥锁 */
	OLED_Mutex   = xSemaphoreCreateMutex();
	Serial_Mutex = xSemaphoreCreateMutex();

	/* 创建队列 */
	Key_Queue = xQueueCreate(5, sizeof(uint8_t));

	/* 创建任务 */
	xTaskCreate(Sensor_Task,  "Sensor",  128, NULL, 2, &SensorTask_Handle);
	xTaskCreate(OLED_Task,    "OLED",    256, NULL, 1, &OLEDTask_Handle);
	xTaskCreate(Serial_Task,  "Serial",  256, NULL, 4, &SerialTask_Handle);
	xTaskCreate(Key_Task,     "Key",     128, NULL, 3, &KeyTask_Handle);
	xTaskCreate(Control_Task, "Control", 256, NULL, 3, &ControlTask_Handle);

	/* 启动调度器 */
	vTaskStartScheduler();

	while (1) { }   
}
