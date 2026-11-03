#include "can_storage.h"
#include "flash.h"
#include "can.h"
#include "cmsis_os.h"
#include <string.h>

/* ============================================================
 *  模块内部状态
 * ============================================================ */
static CanCfg_t  s_cfg;                    /* 头部内容 */
static uint16_t  s_frameCnt;               /* 已缓存帧数 */
static uint16_t  s_pageLen;                /* 页缓冲已用字节数 */
static uint8_t   s_pageBuf[256];           /* 页缓冲（一次写一页，256B） */
static uint32_t  s_writeAddr;              /* 当前写入地址 */

static uint8_t   s_stream[64];             /* 变长帧解析用的字节流缓冲 */
static uint16_t  s_streamLen;

/* ============================================================
 *  解析一帧：成功返回 1，返回 0 表示数据还不够 等下一次再解析
 *  used = 该帧实际字节数
 * ============================================================ */
static uint8_t Frame_Parse(const uint8_t *p, uint16_t avail, uint8_t slot[CAN_SLOT_SIZE], uint16_t *used)
{
    uint8_t  hdr;
    uint8_t  type;
    uint8_t  dlen;       // 数据段长度
    uint8_t  isExt;     // 1 扩展帧 0标准帧
    uint16_t idLen;     // id长度
    uint16_t total;     // 该帧实际长度

    if (avail < 1u)
        return 0;

    hdr  = p[0];
    type = (uint8_t)((hdr >> 4) & 0x0F);   /* 高 4 位：8=扩展帧，0=标准帧 */
    dlen  = (uint8_t)(hdr & 0x0F);          /* 低 4 位：数据长度 */

    if (dlen > 8u)
        return 0;                          /* 非法长度 */
    if ((type != 0u) && (type != 8u))
        return 0;                          /* 非法帧类型 */

    isExt = (type == 8u) ? 1u : 0u;
    idLen = (isExt != 0u) ? 4u : 2u;
    total = (uint16_t)(1u + idLen + 2u + dlen);

    if (avail < total)
        return 0;                          /* 还没收全 */

    // 16字节对齐 先铺满0xFF再写有效位 感觉比 先写有效位再补缺少0xFF更好
    memset(slot, 0xFF, CAN_SLOT_SIZE);     
    memcpy(slot, p, total);               
    *used = total;
    return 1;
}

/* ============================================================
 *  把页缓冲里的数据写到 Flash
 *  跨到新扇区时先擦除（每个扇区只擦一次）
 * ============================================================ */
static void FlushPage(void)
{
    if (s_pageLen == 0u)
        return;

    if ((s_writeAddr & 0xFFFUL) == 0u)     /* CAN首帧->扇区首地址*/
        FlashSectorErase(s_writeAddr);

    // 一次写一页 
    FlashPageWrite(s_writeAddr, s_pageBuf, s_pageLen);
    s_writeAddr += s_pageLen;
    s_pageLen = 0u;
}

/* can帧页缓冲区 满一页写入flash*/
static void StoreSlot(const uint8_t *slot)
{
    memcpy(&s_pageBuf[s_pageLen], slot, CAN_SLOT_SIZE);
    s_pageLen = (uint16_t)(s_pageLen + CAN_SLOT_SIZE);

    if (s_pageLen >= 256u)
        FlushPage();
}

/* 开始接收 */
void CanStorage_Begin(uint32_t baudrate)
{
    s_frameCnt  = 0u;
    s_pageLen   = 0u;
    s_streamLen = 0u;
    s_writeAddr = CAN_FRAME_BASE;           // 0x001000，CAN帧区从扇区 1 开始 扇区 0 留给头部

    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.magic    = 0u;                   // 先作废，全部写完才写有效 magic 
    s_cfg.version  = CAN_CFG_VERSION;
    s_cfg.slotSize = CAN_SLOT_SIZE;
    s_cfg.baudrate = baudrate;

    FlashSectorErase(CAN_CFG_ADDR);         // 擦头部 完成接收后再写头

}

/* 处理USB中CAN报文 */
void CanStorage_PushBytes(const uint8_t *data, uint16_t len)
{
    uint16_t used;      // 该帧在 USB 流里占用的字节数
    uint8_t  slot[CAN_SLOT_SIZE];

    while (len > 0u)
    {
        if (s_streamLen >= (uint16_t)sizeof(s_stream))
            s_streamLen = 0u;              /* 异常保护：缓冲溢出就丢掉重来 */

        s_stream[s_streamLen] = *data;
        s_streamLen++;
        data++;
        len--;

        used = 0u;
        // usb流 每次发来can不是完整帧 攒够完整帧再写入flash
        while (Frame_Parse(s_stream, s_streamLen, slot, &used) != 0u)
        {
            //  最大设计 1000 帧
            if (s_frameCnt >= CAN_FRAME_MAX)
                return;                   

            StoreSlot(slot);
            s_frameCnt++;

            memmove(s_stream, s_stream + used, (size_t)(s_streamLen - used));
            s_streamLen = (uint16_t)(s_streamLen - used);
            used = 0u;
        }
    }
}

/* ============================================================
 *  接收结束：写剩余半页 + 写头部
 * ============================================================ */
uint16_t CanStorage_End(void)
{
    FlushPage();                           /* 把不满一页的剩余数据写掉 */

    if (s_frameCnt > 0u)
    {
        s_cfg.magic    = CAN_MAGIC;
        s_cfg.frameCnt = s_frameCnt;
        s_cfg.check    = CAN_MAGIC ^ (uint32_t)s_frameCnt ^ s_cfg.baudrate;

        FlashPageWrite(CAN_CFG_ADDR, (uint8_t *)&s_cfg, sizeof(s_cfg));
    }

    return s_frameCnt;
}

/* ============================================================
 *  读头部并校验
 * ============================================================ */
uint16_t CanStorage_Load(CanCfg_t *cfg)
{
    FlashRead(CAN_CFG_ADDR, (uint8_t *)cfg, sizeof(CanCfg_t));

    if (cfg->magic != CAN_MAGIC)
        return 0u;
    if (cfg->slotSize != CAN_SLOT_SIZE)
        return 0u;
    if ((cfg->frameCnt == 0u) || (cfg->frameCnt > CAN_FRAME_MAX))
        return 0u;
    if (cfg->check != (CAN_MAGIC ^ (uint32_t)cfg->frameCnt ^ cfg->baudrate))
        return 0u;

    return cfg->frameCnt;
}

/* ============================================================
 *  上电回放：Flash → CAN
 * ============================================================ */
void CanStorage_Replay(void)
{
    /* 16KB 帧数据缓冲，放静态区，不占栈 */
    static uint8_t frameRam[CAN_FRAME_MAX * CAN_SLOT_SIZE];

    CanCfg_t cfg;
    uint16_t cnt;
    uint16_t i;
    uint8_t  hdr;
    uint8_t  dlc;
    uint8_t  isExt;
    uint32_t id;
    uint16_t delayMs;
    const uint8_t *s;
    const uint8_t *pId;
    const uint8_t *pTime;
    const uint8_t *pData;

    cnt = CanStorage_Load(&cfg);
    if (cnt == 0u)
        return;                            /* 没有有效数据，直接返回 */

    (void)Can_SetBaudrate(cfg.baudrate);   /* 按存的波特率配置 CAN */

    /* 一次性整块读，只发 1 次 Flash 读命令（比逐帧读快几十倍） */
    FlashRead(CAN_FRAME_BASE, frameRam, (uint32_t)cnt * CAN_SLOT_SIZE);

    for (i = 0u; i < cnt; i++)
    {
        s = &frameRam[(uint32_t)i * CAN_SLOT_SIZE];

        /* ---- 槽里就是原始帧字节流：按包头解析，逐段定位 ---- */
        hdr   = s[0];
        dlc   = (uint8_t)(hdr & 0x0F);
        isExt = (((hdr >> 4) & 0x0F) == 8u) ? 1u : 0u;

        pId   = s + 1;                                  /* ID 起始 */
        pTime = pId + (isExt != 0u ? 4u : 2u);          /* 时间起始 */
        pData = pTime + 2u;                             /* 数据起始 */

        if (isExt != 0u)
            id = ((uint32_t)pId[0] << 24) | ((uint32_t)pId[1] << 16) |
                 ((uint32_t)pId[2] << 8)  |  (uint32_t)pId[3];
        else
            id = ((uint32_t)pId[0] << 8) | (uint32_t)pId[1];

        id &= (isExt != 0u) ? 0x1FFFFFFFUL : 0x7FFUL;

        delayMs = (uint16_t)(((uint16_t)pTime[0] << 8) | (uint16_t)pTime[1]);

        Can_SendOne(isExt, id, dlc, pData);

        if (delayMs > 0u)
            osDelay(delayMs);              /* 当前帧与下一帧的延时 */
    }
}

/* ============================================================
 *  发送一帧 CAN
 * ============================================================ */
void Can_SendOne(uint8_t isExt, uint32_t id, uint8_t dlc, const uint8_t *data)
{
    CAN_TxHeaderTypeDef h;
    uint32_t mailbox;
    uint32_t waitMs;

    h.StdId  = (isExt != 0u) ? 0u : (id & 0x7FFUL);
    h.ExtId  = (isExt != 0u) ? (id & 0x1FFFFFFFUL) : 0u;
    h.IDE    = (isExt != 0u) ? CAN_ID_EXT : CAN_ID_STD;
    h.RTR    = CAN_RTR_DATA;
    h.DLC    = (dlc > 8u) ? 8u : dlc;
    h.TransmitGlobalTime = DISABLE;

    /* 等一个空闲邮箱（最多等 20ms，避免死等） */
    waitMs = 0u;
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0u)
    {
        if (waitMs > 20u)
            return;
        osDelay(1u);
        waitMs++;
    }

    (void)HAL_CAN_AddTxMessage(&hcan1, &h, (uint8_t *)data, &mailbox);
}

/* ============================================================
 *  按波特率重配 CAN
 *  PCLK1 = 42MHz，1 + BS1(11) + BS2(2) = 14 tq
 *  500000 -> presc 6；1000000 -> presc 3；250000 -> presc 12
 * ============================================================ */
uint8_t Can_SetBaudrate(uint32_t baud)
{
    uint32_t pclk;
    uint32_t presc;

    if (baud == 0u)
        return 0u;

    pclk  = HAL_RCC_GetPCLK1Freq();
    presc = pclk / (baud * 14UL);

    if ((presc < 1UL) || (presc > 1024UL))
        return 0u;

    if (hcan1.Init.Prescaler == presc)
        return 1u;                         /* 已经是这个波特率，不用动 */

    (void)HAL_CAN_Stop(&hcan1);

    hcan1.Init.Prescaler     = presc;
    hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1      = CAN_BS1_11TQ;
    hcan1.Init.TimeSeg2      = CAN_BS2_2TQ;

    if (HAL_CAN_Init(&hcan1) != HAL_OK)
        return 0u;
    if (HAL_CAN_Start(&hcan1) != HAL_OK)
        return 0u;

    (void)HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    return 1u;
}
