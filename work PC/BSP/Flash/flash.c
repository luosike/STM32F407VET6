#include "flash.h"

static __IO uint32_t FlashTimeout = FLASH_TIME_OUT;

// /**
//  * @函数功能: 	Flash进入4字节地址模式
//  * @输入参数: 	无
//  * @返 回 值: 	无
//  * @说    明: 	命令：
//  *				0x15：查询状态寄存器3
//  *				0xB7：进入4字节地址工作模式
//  */
// static void FlashEntry4ByteAddressMode(void)
// {
// 	uint8_t temp = 0;
// 	uint8_t command = 0x15;

// 	// 查询当前工作地址模式
// 	FLASH_CS_LOW;
// 	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 10); // 发送命令
// 	HAL_SPI_Receive(&hspi2, (void *)&temp, 1, 10);	   // 查询状态寄存器3
// 	FLASH_CS_HIGH;

// 	// 最低位为0 则需设置工作模式
// 	if ((temp & 0x01) == 0)
// 	{
// 		command = 0xB7;
// 		FLASH_CS_LOW;
// 		HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 10); // 发送命令
// 		FLASH_CS_HIGH;
// 	}
// }

/**
 * @函数功能: 	唤醒Flash
 * @输入参数: 	无
 * @返 回 值: 	唤醒成功：1
 *				唤醒失败：0
 * @说    明: 	命令：
 *				0xAB：唤醒指令
 */
uint8_t FlashWakeUP(void)
{
	uint8_t temp = 0;
	uint8_t command = 0xAB;
	uint8_t Dummy_Byte[3] = {0};

	// 发送唤醒指令
	FLASH_CS_LOW;
	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 10); // 发送命令
	HAL_SPI_Transmit(&hspi2, (void *)Dummy_Byte, 3, 10);
	HAL_SPI_Receive(&hspi2, (void *)&temp, 1, 10); // 接收ID7 ~ ID0
	FLASH_CS_HIGH;

	if (temp == 0x16) // 检查ID7 ~ ID0
		return 1;
	else
		return 0;
}

/**
 * @函数功能: 	Flash复位
 * @输入参数: 	无
 * @返 回 值: 	复位成功：1
 *				复位失败：0
 * @说    明: 	命令：
 *				 0x66 → 0x99：复位指令
 */
uint8_t FlashReset(void)
{
	uint8_t command = FLASH_EnableReset;

	// 发送复位指令
	FLASH_CS_LOW;
	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 10); // 发送命令
	FLASH_CS_HIGH;

	// 发送复位指令
	command = FLASH_Reset;
	FLASH_CS_LOW;
	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 10); // 发送命令
	FLASH_CS_HIGH;

	HAL_Delay(1);
	return 1;
}

/**
 * @函数功能: 	Flash初始化
 * @输入参数: 	无
 * @返 回 值: 	初始化成功：1
 *				初始化失败：0
 * @说    明: 	初始化步骤：
 *				1.进入4字节地址模式
 * 				2.唤醒Flash
 */
uint8_t Flash_Init(void)
{
	// 1. 唤醒Flash
	FlashWakeUP();
	HAL_Delay(1);

	// 2. 软件复位：把芯片状态机拉回默认
	FlashReset(); // 0x66 → 0x99，之后等 tRST(约 30µs)

	// 3. 读 ID：确认通信 判断初始化成功
	if (!FlashReadID())
		return 0;

	// 4. 确认状态：BUSY 必须为 0
	// if (FlashIsBusy()) // 读 SR1 bit0
	// 	return 0;

	return 1;
}

/**
 * @函数功能: 	等待Flash准备好
 * @输入参数: 	无
 * @返 回 值: 	无
 * @说    明: 	命令：
 *				0x05：查询状态寄存器1指令
 */
static void FlashWaitForReady(void)
{
	uint8_t temp = 0;
	uint8_t command = FLASH_ReadStatusReg1;
	FlashTimeout = FLASH_TIME_OUT;

	// 查询状态寄存器1
	FLASH_CS_LOW;
	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 10); // 发送命令
	do
	{
		HAL_SPI_Receive(&hspi2, (void *)&temp, 1, 10); // 接收数据
		if (FlashTimeout-- == 0)
		{
			FLASH_CS_HIGH; // 超时退出拉高CS 释放spi
			return;
		}
	} while ((temp & 0x01) == 1);
	FLASH_CS_HIGH;
}

/**
 * @函数功能: 	读取FlashID
 * @输入参数: 	无
 * @返 回 值: 	读取ID成功：1
 *				读取ID失败：0
 * @说    明: 	命令：
 *				0x9F：查询FlashID
 */
uint8_t FlashReadID(void)
{
	uint8_t command = 0x9F;
	uint8_t temp[3] = {0};
	uint32_t ID = 0;

	// 等待Flash准备好
	FlashWaitForReady();

	// 查询ID
	FLASH_CS_LOW;
	// 发送命令
	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 10);
	

	HAL_SPI_Receive(&hspi2, (void *)temp, 3, 10); // 接收数据
	FLASH_CS_HIGH;

	ID = ((uint32_t)temp[0] << 16) | (temp[1] << 8) | temp[2];

	if (ID == FLASH_W25Q64_ID)
		return 1;
	else
		return 0;
}

/**
 * @函数功能: 	读取Flash数据
 * @输入参数: 	addr：			读取地址
 *				readBuffer：	存放读出数据的指针
 *				numByteToRead：	需要读取数据的长度
 * @返 回 值: 	无
 * @说    明: 	命令：
 *				0x03：读取数据指令
 */
void FlashRead(uint32_t addr, uint8_t *readBuffer, uint32_t numByteToRead)
{
	uint8_t command = FLASH_ReadData;
	uint8_t temp[3] = {0};

	// 计算地址
	temp[0] = (addr & 0x00FF0000) >> 16;
	temp[1] = (addr & 0x0000FF00) >> 8;
	temp[2] = (addr & 0x000000FF);

	// 等待Flash准备好
	FlashWaitForReady();

	// 读取数据
	FLASH_CS_LOW;
	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 100);				  // 发送命令
	HAL_SPI_Transmit(&hspi2, (void *)temp, 3, 100);					  // 发送地址
	HAL_SPI_Receive(&hspi2, (void *)readBuffer, numByteToRead, 5000); // 接收数据
	FLASH_CS_HIGH;
}

/**
 * @函数功能: 	Flash写使能
 * @输入参数: 	无
 * @返 回 值: 	无
 * @说    明: 	命令：
 *				0x06：写使能指令
 */
static void FlashWriteEnable(void)
{
	uint8_t command = FLASH_WriteEnable;

	// 等待Flash准备好
	FlashWaitForReady();

	// 写使能
	FLASH_CS_LOW;
	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 10); // 发送命令
	FLASH_CS_HIGH;
}

/**
 * @函数功能: 	擦除Flash扇区
 * @输入参数: 	addr：			要擦除的扇区地址
 * @返 回 值: 	无
 * @说    明: 	命令：
 *				0x20：扇区擦除指令  一个扇区大小4096B = 4KB
 */
void FlashSectorErase(uint32_t addr)
{
	uint8_t command = FLASH_SectorErase;
	uint8_t temp[3] = {0};

	// 计算地址
	temp[0] = (addr & 0x00FF0000) >> 16;
	temp[1] = (addr & 0x0000FF00) >> 8;
	temp[2] = (addr & 0x000000FF);

	// 发送写使能指令
	FlashWriteEnable();

	// 等待Flash准备好
	FlashWaitForReady();

	// 擦除扇区
	FLASH_CS_LOW;
	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 100); // 发送命令						//发送命令
	HAL_SPI_Transmit(&hspi2, (void *)temp, 3, 100);		// 发送地址
	FLASH_CS_HIGH;

	// 等待Flash擦除完毕
	FlashWaitForReady();
}

/**
 * @函数功能: 	按页向Flash写入数据
 * @输入参数: 	addr：			写入地址
 *				writeBuffer：	要写入数据的指针
 *				numByteToWrite：需要写入数据的长度
 * @返 回 值: 	无
 * @说    明: 	命令：
 *				0x02：写入数据指令
 */
void FlashPageWrite(uint32_t addr, uint8_t *writeBuffer, uint32_t numByteToWrite)
{
	uint8_t command = FLASH_PageProgram;
	uint8_t temp[3] = {0};

	// 计算地址
	temp[0] = (addr & 0x00FF0000) >> 16;
	temp[1] = (addr & 0x0000FF00) >> 8;
	temp[2] = (addr & 0x000000FF);

	// 发送写使能指令
	FlashWriteEnable();

	// 等待Flash准备好
	FlashWaitForReady();

	// 写入数据
	FLASH_CS_LOW;
	HAL_SPI_Transmit(&hspi2, (void *)&command, 1, 10);					 // 发送命令						//发送命令
	HAL_SPI_Transmit(&hspi2, (void *)temp, 3, 100);						 // 发送地址
	HAL_SPI_Transmit(&hspi2, (void *)writeBuffer, numByteToWrite, 5000); // 发送数据
	FLASH_CS_HIGH;

	// 等待写入完毕
	FlashWaitForReady();
}

/**
 * @函数功能: 	向Flash写入数据 内置擦除
 * @输入参数: 	addr：			写入地址
 *				writeBuffer：	要写入数据的指针
 *				numByteToWrite：需要写入数据的长度
 * @返 回 值: 	无
 * @说    明: 	命令：
 *				0x02：写入数据指令
 */
void FlashWrite(uint32_t addr, uint8_t *writeBuffer, uint32_t numByteToWrite)
{
	uint8_t isAlignToPage = 0;
	uint8_t count = 0;
	uint16_t pageNum = 0;
	uint8_t pageLave = 0;

	isAlignToPage = addr % FLASH_PageSize;		// 地址是否按页对齐
	count = FLASH_PageSize - isAlignToPage;		// 计算差多少个数据才能对齐
	pageNum = numByteToWrite / FLASH_PageSize;	// 计算整页数量
	pageLave = numByteToWrite % FLASH_PageSize; // 计算剩余不满一页的数据

	// 擦除
	FlashSectorErase(addr);

	if (isAlignToPage == 0) // 刚好按页对齐
	{
		if (pageNum == 0) // 不足一页
		{
			FlashPageWrite(addr, writeBuffer, numByteToWrite);
		}
		else
		{
			while (pageNum--) // 写整页
			{
				FlashPageWrite(addr, writeBuffer, FLASH_PageSize);
				addr += FLASH_PageSize;		   // 地址增加一页
				writeBuffer += FLASH_PageSize; // 写入数据的指针增加一页
			}
			FlashPageWrite(addr, writeBuffer, pageLave); // 写不足整页
		}
	}
	else // 地址没有按页对齐
	{
		if (pageNum == 0) // 不足一页
		{
			FlashPageWrite(addr, writeBuffer, numByteToWrite);
		}
		else
		{
			numByteToWrite -= count;
			pageNum = numByteToWrite / FLASH_PageSize;	// 计算整页数量
			pageLave = numByteToWrite % FLASH_PageSize; // 计算剩余不满一页的数据

			FlashPageWrite(addr, writeBuffer, count);
			addr += count;
			writeBuffer += count;

			while (pageNum--) // 写整页
			{
				FlashPageWrite(addr, writeBuffer, FLASH_PageSize);
				addr += FLASH_PageSize;		   // 地址增加一页
				writeBuffer += FLASH_PageSize; // 写入数据的指针增加一页
			}

			FlashPageWrite(addr, writeBuffer, pageLave); // 写不足整页
		}
	}
}
