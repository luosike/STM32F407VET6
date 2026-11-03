/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FLASH_H__
#define __FLASH_H__


/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include "main.h"
// #include "spi.h"
#include "soft_spi.h"



/* ============ W25Q64FW 指令表（64Mbit / 8MB / 24 位地址） ============
 * 通用时序：CS 拉低 -> 发指令(+地址/数据) -> CS 拉高（数据在 CS 上升沿才真正执行）
 * 标注 [需WE] 的指令，执行前必须先发 0x06 写使能，并等 WEL 置位
*/

/* ---------- 1. 状态寄存器 ---------- */
#define FLASH_ReadStatusReg1     0x05   // 读状态寄存器1：bit0=BUSY, bit1=WEL, bit2~4=BP(块保护), bit5=TB, bit6=SEC, bit7=SRP0
#define FLASH_ReadStatusReg2     0x35   // 读状态寄存器2：bit0=SRP1, bit1=QE(四线/Quad使能), bit7=SUS(擦写挂起标志)
#define FLASH_ReadStatusReg3     0x15   // 读状态寄存器3：bit0=WPS(写保护方式选择)等；★注意：W25Q16 没有 4 字节地址的 ADS 位
#define FLASH_WriteStatusReg1    0x01   // [需WE] 写状态寄存器1（改 BP/TB/SEC/SRP 保护位）
#define FLASH_WriteStatusReg2    0x31   // [需WE] 写状态寄存器2（如打开 QE 位做四线）
#define FLASH_WriteStatusReg3    0x11   // [需WE] 写状态寄存器3（改 WPS 等）

/* ---------- 2. 读数据 ---------- */
#define FLASH_ReadData           0x03   // 读数据：指令 + 24位地址，之后连续输出；无 dummy；时钟上限较低(~50MHz)
#define FLASH_FastReadData       0x0B   // 快读：指令 + 24位地址 + 1字节dummy，速度更高(~104MHz)
#define FLASH_FastReadDual       0x3B   // 双线输出快读：指令 + 24位地址 + 1字节dummy，之后数据走 2 根 IO(MOSI/MISO)
#define FLASH_FastReadQuad       0xEB   // 四线快读：指令 + 24位地址 + 1字节模式(M7~M0) + dummy，之后数据走 4 根 IO（需先置 QE=1）
#define FLASH_FastReadQuadOut    0x6B   // 四线输出快读：地址仍用单线发，仅数据阶段用 4 线
#define FLASH_ReadUniqueID       0x4B   // 读 96 位唯一 ID：指令 + 4字节dummy + 8字节 UID（做产品序列号/防抄板常用）
#define FLASH_ReadSFDP           0x5A   // 读 SFDP 表：可读出容量、支持的指令集等参数（通用驱动用来自动识别）

/* ---------- 3. 编程与擦除 ---------- */
#define FLASH_WriteEnable        0x06   // [无需WE] 写使能：把 SR1 的 WEL 置 1，之后的编程/擦除才有效
#define FLASH_WriteDisable       0x04   // 写失能：清 WEL
#define FLASH_PageProgram        0x02   // [需WE] 页编程：指令 + 24位地址 + 数据，★一页最大 256 字节且不能跨页
#define FLASH_PageProgramQuad    0x32   // [需WE] 四线页编程：数据阶段走 4 根 IO（需 QE=1）
#define FLASH_SectorErase        0x20   // [需WE] 4KB 扇区擦除（最常用的最小擦除单位）
#define FLASH_BlockErase32K      0x52   // [需WE] 32KB 块擦除
#define FLASH_BlockErase         0xD8   // [需WE] 64KB 块擦除
#define FLASH_ChipErase          0xC7   // [需WE] 整片擦除（0x60 是老版兼容指令，效果相同）
#define FLASH_Suspend            0x75   // 擦写挂起（擦除中想插读操作时用）
#define FLASH_Resume             0x7A   // 恢复挂起的擦写

/* ---------- 4. ID 与识别 ---------- */
#define FLASH_JedecDeviceID      0x9F   // 读 JEDEC ID：返回 3 字节 = 厂商(0xEF) + 类型 + 容量
                                        //   类型 0x40 = W25Q16JV；0x30 = 老版 W25Q16DV；0x70 = W25Q16FW
                                        //   容量 0x15 = 16Mbit(2MB)；0x14 = 8Mbit
#define FLASH_ManufactDeviceID   0x90   // 读厂商/设备 ID：指令 + 3字节dummy(地址全0)，返回 0xEF 0x14
#define FLASH_ReleasePowerDown   0xAB   // 释放掉电 + 读设备 ID：指令 + 3字节dummy + 1字节DeviceID(0x14)
                                        //   ★指令 0xAB 同时承担"唤醒"和"读ID"两个作用，不是两条指令

/* ---------- 5. 低功耗与复位 ---------- */
#define FLASH_PowerDown          0xB9   // 进入掉电模式：电流降到 ~1µA，之后只认 0xAB 唤醒
#define FLASH_EnableReset        0x66   // 复位使能：必须先发它，再发 0x99 才有效（防误复位）
#define FLASH_Reset              0x99   // 软件复位：把芯片状态机恢复到上电默认

/* Flash页大小 单位字节 ------------------------------------------------------*/
#define FLASH_PageSize			256

/* 超时时间 ------------------------------------------------------------------*/
#define FLASH_TIME_OUT			0xFFFF0000

/* Flash W25Q64 ID ------------------------------------------------------------------*/
#define FLASH_W25Q64_ID			0xEF6017     

/* Flash W25Q64 最大容量 8MB */
#define FLASH_SIZE   0x800000

/* 片选信号 ------------------------------------------------------------------*/
// #define FLASH_CS_LOW		HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET)
// #define FLASH_CS_HIGH		HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET)
#define FLASH_CS_LOW		SOFT_SPI_CS_LOW()
#define FLASH_CS_HIGH		SOFT_SPI_CS_HIGH()

/* ---- 兼容层：本文件内所有 HAL_SPI_xxx 自动走软件 SPI ---- */
#define HAL_SPI_Transmit(h, p, n, t)   soft_spi_write((const uint8_t *)(p), (n))
#define HAL_SPI_Receive(h, p, n, t)    soft_spi_read((uint8_t *)(p), (n))




uint8_t Flash_Init(void);
uint8_t FlashReadID(void);
uint8_t FlashWakeUP(void);

void FlashRead(uint32_t addr, uint8_t *readBuffer, uint32_t numByteToRead);
void FlashSectorErase(uint32_t addr);
void FlashWrite(uint32_t addr, uint8_t *writeBuffer, uint32_t numByteToWrite);
void FlashPageWrite(uint32_t addr, uint8_t *writeBuffer, uint32_t numByteToWrite);

#endif


