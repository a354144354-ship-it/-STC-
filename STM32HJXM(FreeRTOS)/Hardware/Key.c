#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "FreeRTOS.h"
#include "task.h"
/**
  * 函    数：按键初始化
  * 参    数：无
  * 返 回 值：无
  */
void Key_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);		//开启GPIOB的时钟
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin =GPIO_Pin_11| GPIO_Pin_12 | GPIO_Pin_13|GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);						//将PB1和PB11引脚初始化为上拉输入
}

/**
  * 函    数：按键获取键码
  * 参    数：无
  * 返 回 值：按下按键是阻塞式操作，当按键按住不放时，函数会卡住，直到按键松手
  */
uint8_t Key_GetNum(void)
{
	uint8_t KeyNum = 0;		//定义变量，默认键码值为0
	
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == 0)			//读PB1输入寄存器的状态，如果为0，则代表按键1按下
	{
		vTaskDelay(pdMS_TO_TICKS(20));										//延时消抖
		while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == 0);	//等待按键松手
		vTaskDelay(pdMS_TO_TICKS(20));											//延时消抖
		KeyNum = 1;												//置键码为motor
	}
	
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0)			//读PB11输入寄存器的状态，如果为0，则代表按键2按下
	{
		vTaskDelay(pdMS_TO_TICKS(20));											//延时消抖
		while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0);	//等待按键松手
		vTaskDelay(pdMS_TO_TICKS(20));											//延时消抖
		KeyNum = 2;												//置键码为led
	}
		
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0)			//读PB1输入寄存器的状态，如果为0，则代表按键1按下
	{
		vTaskDelay(pdMS_TO_TICKS(20));												//延时消抖
		while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0);	//等待按键松手
		vTaskDelay(pdMS_TO_TICKS(20));											//延时消抖
		KeyNum = 3;												//置键码为bun
	}
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) == 0)			//读PB1输入寄存器的状态，如果为0，则代表按键1按下
	{
		vTaskDelay(pdMS_TO_TICKS(20));											//延时消抖
		while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) == 0);	//等待按键松手
		vTaskDelay(pdMS_TO_TICKS(20));											//延时消抖
		KeyNum = 4;												//置键码为mode
	}
	return KeyNum;			//返回键码值，如果没有按键按下，所有if都不成立，则键码为默认值0
}
