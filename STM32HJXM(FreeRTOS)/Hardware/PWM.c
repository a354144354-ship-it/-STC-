#include "stm32f10x.h"                  // Device header

/**
  * 函    数：PWM初始化
  * 参    数：无
  * 返 回 值：无4
  */
void PWM_Init(void)
{
	/*时钟使能*/
RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

/*GPIOA初始化 - 通道1、6、7*/
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_6 | GPIO_Pin_7;
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOA, &GPIO_InitStructure);

/*GPIOB初始化 - 通道0、1*/
GPIO_InitTypeDef GPIO_InitStructure2;
GPIO_InitStructure2.GPIO_Mode = GPIO_Mode_AF_PP;
GPIO_InitStructure2.GPIO_Pin = GPIO_Pin_0;
GPIO_InitStructure2.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOB, &GPIO_InitStructure2);

/*配置时钟源*/
TIM_InternalClockConfig(TIM2);
TIM_InternalClockConfig(TIM3);

/*TIM2时基单元初始化 - 频率 = 72MHz/36/100 = 20kHz*/
TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;       // ARR
TIM_TimeBaseInitStructure.TIM_Prescaler = 36 - 1;     // PSC
TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

/*TIM3时基单元初始化 - 频率 = 72MHz/720/100 = 1kHz*/
TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure2;
TIM_TimeBaseInitStructure2.TIM_ClockDivision = TIM_CKD_DIV1;
TIM_TimeBaseInitStructure2.TIM_CounterMode = TIM_CounterMode_Up;
TIM_TimeBaseInitStructure2.TIM_Period = 100 - 1;      // ARR
TIM_TimeBaseInitStructure2.TIM_Prescaler = 720 - 1;   // PSC
TIM_TimeBaseInitStructure2.TIM_RepetitionCounter = 0;
TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure2);

/*TIM2通道2 PWM输出*/
TIM_OCInitTypeDef TIM_OCInitStructure;
TIM_OCStructInit(&TIM_OCInitStructure);
TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
TIM_OCInitStructure.TIM_Pulse = 0;                     // 初始占空比0%
TIM_OC2Init(TIM2, &TIM_OCInitStructure);

/*TIM3四通道PWM输出*/
TIM_OCInitTypeDef TIM_OCInitStructure2;
TIM_OCStructInit(&TIM_OCInitStructure2);
TIM_OCInitStructure2.TIM_OCMode = TIM_OCMode_PWM1;
TIM_OCInitStructure2.TIM_OCPolarity = TIM_OCPolarity_High;
TIM_OCInitStructure2.TIM_OutputState = TIM_OutputState_Enable;
TIM_OCInitStructure2.TIM_Pulse = 0;                    // 初始占空比0%
TIM_OC1Init(TIM3, &TIM_OCInitStructure2);
TIM_OC2Init(TIM3, &TIM_OCInitStructure2);
TIM_OC3Init(TIM3, &TIM_OCInitStructure2);


/*使能定时器*/
TIM_Cmd(TIM2, ENABLE);
TIM_Cmd(TIM3, ENABLE);
}

/**
  * 函    数：PWM设置CCR
  * 参    数：Compare 要写入的CCR的值，范围：0~100
  * 返 回 值：无
  * 注意事项：CCR和ARR共同决定占空比，此函数仅设置CCR的值，并不直接是占空比
  *           占空比Duty = CCR / (ARR + 1)
  */

void PWM_SetCompareLED (uint16_t Compare)
{
	TIM_SetCompare1(TIM3, Compare);
TIM_SetCompare2(TIM3, Compare);
TIM_SetCompare3(TIM3, Compare);

}
void PWM_SetCompareMotor (uint16_t Compare)
{
	TIM_SetCompare2(TIM2, Compare);		//设置CCR3的值
}
