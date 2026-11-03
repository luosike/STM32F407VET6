#include "mycan.h"
#include "flash.h"
/**
 * @brief  编写can应用层
 */

// 定义接收队列
osMessageQueueId_t CAN_RxQueueHandle;

// 用户层初始化
void CAN_Init_User(void)
{
    // 1. 创建接收队列，大小为 10 个消息，每个消息大小为 CAN_Msg_t
    CAN_RxQueueHandle = osMessageQueueNew(10, sizeof(CAN_Msg_t), NULL);

    // 2. 启动 CAN 硬件
    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        Error_Handler();
    }
    // 3. 开启 FIFO0 接收中断
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        Error_Handler();
    }
}

// 发送函数封装
uint8_t CAN_Send_Msg(uint32_t id, uint8_t *data, uint8_t len)
{
    CAN_TxHeaderTypeDef TxHeader;
    uint32_t TxMailbox;

    // 配置发送头
    TxHeader.StdId = id;
    TxHeader.ExtId = 0;
    TxHeader.IDE = CAN_ID_STD;   // 标准帧
    TxHeader.RTR = CAN_RTR_DATA; // 数据帧
    TxHeader.DLC = len;          // 数据长度 (0-8)
    TxHeader.TransmitGlobalTime = DISABLE;

    // 检查是否有空闲邮箱
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
    {
        if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, data, &TxMailbox) == HAL_OK)
        {
            return 1; // 发送成功（加入邮箱成功）
        }
    }
    return 0; // 发送失败（邮箱满）
}


// 中断接收回调函数
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];
    CAN_Msg_t msg;

    if (hcan->Instance == CAN1)
    {
        // 从硬件 FIFO 读取数据
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
        {
            // 组装消息
            msg.id = RxHeader.StdId;
            msg.len = RxHeader.DLC;
            for (int i = 0; i < msg.len; i++)
            {
                msg.data[i] = RxData[i];
            }
            // 发送到 FreeRTOS 队列（不能在中断里用普通 send，要用 FromISR）
            osMessageQueuePut(CAN_RxQueueHandle, &msg, 0, 0);
        }
    }
}
