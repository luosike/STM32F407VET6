#include "led.h"

/**
 * @brief  LED1 初始化函数
 * @note   使用了 HAL 库的标准 GPIO 初始化流程
 */
void LED1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1. 开启 GPIO 时钟 */
    LED1_GPIO_CLK_ENABLE();

    /* 2. 配置 GPIO 引脚 */
    GPIO_InitStruct.Pin = LED1_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;  // 推挽输出
    GPIO_InitStruct.Pull = GPIO_NOPULL;          // 不需要上下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // 低速即可
    HAL_GPIO_Init(LED1_GPIO_PORT, &GPIO_InitStruct);

    /* 3. 默认关闭 LED1 (假设低电平点亮) */
    LED1_Off();
}

/**
 * @brief  点亮 LED1
 */
void LED1_On(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  熄灭 LED1
 */
void LED1_Off(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET);
}

/**
 * @brief  翻转 LED1 状态 (最常用，用于闪烁)
 */
void LED1_Toggle(void)
{
    HAL_GPIO_TogglePin(LED1_GPIO_PORT, LED1_GPIO_PIN);
}