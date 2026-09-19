#include "stm32f10x.h"                  // Device header
#include "PWM.h"
/**
  * 函    数：LED初始化
  * 参    数：无
  * 返 回 值：无
  */
void LED_Init(void)
{PWM_Init();
}

void LED_SetSpeed(int8_t Speed){
	
	if (Speed >= 0)
PWM_SetCompareLED(Speed);

}