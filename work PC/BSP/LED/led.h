#ifndef __LED_H__
#define __LED_H__

#include "main.h"

/******************************************************************************************/
/* 引脚 定义 */
#define LED1_GPIO_PORT GPIOA
#define LED1_GPIO_PIN GPIO_PIN_15

#define LED1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)             /* PB口时钟使能 */

/******************************************************************************************/

/* 函数声明 */
void LED1_Init(void);
void LED1_On(void);
void LED1_Off(void);
void LED1_Toggle(void);

#endif /* __LED_H */
