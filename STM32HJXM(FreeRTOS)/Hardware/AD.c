#include "stm32f10x.h"                  // Device header

#define BUFFER_SIZE 64                   // DMA缓冲区总大小，双通道一共存储64个uint16采样点
#define BUFFER_MASK (BUFFER_SIZE - 1)    // 环形缓冲区掩码，用于快速取模运算( & BUFFER_MASK 等价 % BUFFER_SIZE)

uint16_t AD_Value[BUFFER_SIZE];          // DMA目标缓冲区，存放ADC采样结果
volatile uint32_t writeIndex = 0;        // 写指针标记，DMA中断更新，表示当前DMA写入到哪半区
volatile uint32_t readIndex = 0;         // 读指针标记，主程序使用，记录读取位置

/**
  * @brief  ADC初始化，使用DMA循环搬运双通道ADC采样数据
  * @note   PA2(ADC_CH2)、PA3(ADC_CH3)，扫描模式连续采集，DMA循环模式
  */
void AD_Init(void)
{
    /* 开启外设时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);    // 使能ADC1外设时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);   // 使能GPIOA端口时钟
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);      // 使能DMA1时钟
    
    /* 配置ADC时钟：PCLK2分频6 */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);                       // PCLK2=72MHz，分频后ADCCLK=12MHz，F1最大ADC时钟14M
    
    /* GPIO初始化 PA2 PA3 设置为模拟输入 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;           // 模拟输入模式，禁止数字驱动
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;  // PA2、PA3作为ADC输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // 模拟输入Speed参数无实际作用，习惯配置
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    /* ADC规则通道配置，扫描顺序：CH2 第1位，CH3第2位 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 2, ADC_SampleTime_55Cycles5);
    
    /* ADC模块初始化 */
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;      // ADC独立模式，不使用多ADC同步
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;  // 采样数据右对齐
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; // 软件触发转换
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;      // 连续转换模式，转换完自动开启下一次
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;            // 扫描模式，多通道依次转换
    ADC_InitStructure.ADC_NbrOfChannel = 2;                  // 扫描通道数量：2路
    ADC_Init(ADC1, &ADC_InitStructure);
    
    /* DMA1通道1初始化，DMA搬运ADC采样值到内存 */
    DMA_InitTypeDef DMA_InitStructure;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;  // DMA外设地址：ADC数据寄存器DR
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 外设数据宽度：半字16bit
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; // 外设地址不自增，一直读ADC->DR
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)AD_Value;       // 内存起始地址：AD_Value数组
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord; // 内存数据宽度：半字16bit
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;          // 内存地址自增，依次存入数组
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;               // 传输方向：外设 -> 内存
    DMA_InitStructure.DMA_BufferSize = BUFFER_SIZE;                  // DMA一次传输总长度64个半字
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;                  // DMA循环模式，传输完成自动回到开头
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;                     // 禁止内存到内存传输
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;            // DMA优先级：中等
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    
    /* 开启DMA传输完成中断TC */
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
    
    /* NVIC中断优先级配置 DMA1通道1中断 */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  // 抢占优先级1
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;         // 响应优先级1
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;            // 使能DMA中断
    NVIC_Init(&NVIC_InitStructure);
    
    /* 使能DMA、ADC的DMA请求、ADC模块 */
    DMA_Cmd(DMA1_Channel1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);
    
    /* ADC校准流程，必须执行，提高采样精度 */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) == SET);        // 等待复位校准完成
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) == SET);             // 等待自校准完成
    
    /* 软件启动ADC转换 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/**
  * @brief  DMA1通道1中断服务函数
  * @note   HT半传输中断：DMA写到一半(32点)；TC全传输中断：DMA写满64点
  */
void DMA1_Channel1_IRQHandler(void)
{
    // 判断是否为DMA全传输完成中断（64个数据全部搬运完成）
    if (DMA_GetITStatus(DMA1_IT_TC1))
    {
        writeIndex = 0;                // 更新写标记：当前可读区域后半段，下一轮DMA从缓冲区0开始写入
        DMA_ClearITPendingBit(DMA1_IT_TC1); // 清除中断标志位
    }
    // 判断是否为DMA半传输中断（写完前32个数据）
    if (DMA_GetITStatus(DMA1_IT_HT1))
    {
        writeIndex = BUFFER_SIZE / 2; // 更新写标记：当前可读区域前半段，DMA正在写入后半段
        DMA_ClearITPendingBit(DMA1_IT_HT1);
    }
}

/**
  * @brief  获取ADC通道采样平均值（滑动平均滤波）
  * @param  channel: ADC通道号，支持2(PA2) 和3(PA3)
  * @retval 滤波后的ADC采样平均值
  * @note   
  */
uint16_t AD_GetLatestValue(uint8_t channel)
{
    uint32_t currentWrite =  writeIndex; // 拷贝当前写标记，防止中断修改
    uint16_t value = 0;
    
    
    if (channel == 2) 
    {
        // 通道2：累加16个采样点做平均
        for(uint16_t i=0;i<=31;i=i+2)
        {
            value += AD_Value[currentWrite+i];
        }
        value /= 16;
    } 
    else 
    {
        // 通道3：累加16个采样点做平均
        for(uint16_t i=1;i<=32;i=i+2)
        {
            value += AD_Value[currentWrite + i];
        }
        value /=16;
    }
    return value;
}
