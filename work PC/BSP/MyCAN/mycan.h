#ifndef __MYCAN_H__
#define __MYCAN_H__

#include "main.h"
#include "can.h"
#include "cmsis_os.h" // 引入 FreeRTOS 头文件

// CAN 句柄
extern CAN_HandleTypeDef hcan1;

// 消息结构体 用于把 CAN 数据通过队列传给任务
typedef struct {
    uint32_t id;
    uint8_t data[8];
    uint8_t len;
} CAN_Msg_t;



// 函数声明
void CAN_Init_User(void);                           // 用户层初始化（启动CAN、激活中断）
uint8_t CAN_Send_Msg(uint32_t id, uint8_t *data, uint8_t len); // 发送函数
#endif