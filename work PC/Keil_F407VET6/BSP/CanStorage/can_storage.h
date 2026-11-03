#ifndef __CAN_STORAGE_H__
#define __CAN_STORAGE_H__

#include <stdint.h>

/* ============================================================
 *  CAN 报文 Flash 缓存模块
 *
 *  用途：PC 通过 USB 下发一批 CAN 报文 → 存到外部 SPI Flash →
 *        上电时自动从 Flash 读出，按顺序发到 CAN 总线。
 *
 *  存储布局（W25Q，24 位地址）：
 *      0x000000 第一个扇区 存放描述 magic/frameCnt/baudrate/...
 *      0x001000 第二个扇区 帧区起始，每帧固定 16 字节
 *      合计 0x000100 + 1000*16 = 0x003F40（约 16.2KB，占 4 个 4KB 扇区）
 *
 *  每帧 16 字节的规范化布局：
 *      +0      包头（高4位 8=扩展帧/0=标准帧，低4位 DLC 0~8）
 *      +1~4    ID（4 字节大端：扩展帧 29 位；标准帧高 2 字节为 0，低 2 字节低 11 位有效）
 *      +5~6    发送时间（ms，大端）= 当前帧与下一帧的延时
 *      +7~14   数据（最多 8 字节，不足部分保持 0xFF）
 *      +15     保留（0xFF）
 *
 *  USB 侧帧格式（变长）：
 *      包头1B + ID(标准2B/扩展4B) + 发送时间2B + 数据(DLC字节)
 *      即 标准帧 5+DLC 字节、扩展帧 7+DLC 字节
 * ============================================================ */

#define CAN_SLOT_SIZE        16u          /* 每帧固定占 16 字节 */
#define CAN_FRAME_MAX        1000u        /* 最多缓存 1000 帧 */
#define CAN_CFG_ADDR         0x000000UL   /* 头部地址 第一个扇区 */
#define CAN_FRAME_BASE       0x001000UL   /* CAN帧区起始地址 第二个扇区 一个扇区 256帧CAN*/
#define CAN_MAGIC            0x314E4143UL /* 'C','A','N','1' 魔数 用着写入数据包有效标志 流程:废CAN_MAGIC --> 完整写入数据包 --> 写CAN_MAGIC*/ 
#define CAN_CFG_VERSION      1u           /* 数据包版本号 */

/* 头部结构 存放数据包信息 用作判断flash上CAN数据包是否有效*/
typedef struct
{
    uint32_t magic;       /* CAN_MAGIC 表示数据有效 */
    uint16_t version;     /* CAN_CFG_VERSION */
    uint16_t frameCnt;    /* 实际缓存的帧数 */
    uint32_t baudrate;    /* CAN 波特率，如 500000 */
    uint16_t slotSize;    /* CAN_SLOT_SIZE */
    uint16_t reserved;
    uint32_t check;       /* magic ^ frameCnt ^ baudrate，用于简单校验 */
    uint32_t reserved2[26];
} CanCfg_t;

/* ---------------- 接口 ---------------- */

/* 开始接收：擦除头部所在扇区、帧数清零。应先解析出波特率再调用 */
void     CanStorage_Begin(uint32_t baudrate);

/* 喂数据：USB 收到多少喂多少，内部自动切帧 / 攒页 / 按扇区擦写 */
void     CanStorage_PushBytes(const uint8_t *data, uint16_t len);

/* 接收结束：写完剩余半页，最后写头部。返回实际缓存的帧数 */
uint16_t CanStorage_End(void);

/* 读头部，返回有效帧数（0 = 没有有效数据） */
uint16_t CanStorage_Load(CanCfg_t *cfg);

/* 上电回放：按头部里的波特率配置 CAN，然后逐帧发送 */
void     CanStorage_Replay(void);

/* 发一帧 CAN（内部处理邮箱满的情况） */
void     Can_SendOne(uint8_t isExt, uint32_t id, uint8_t dlc, const uint8_t *data);

/* 按波特率重配 CAN（PCLK1=42MHz，BS1=11TQ / BS2=2TQ） */
uint8_t  Can_SetBaudrate(uint32_t baud);

#endif /* __CAN_STORAGE_H__ */
