#include "soft_spi.h"

/* 速度调节：循环次数越大，SCK 越慢（F407@168MHz，6 次约 1~2MHz） */
static inline void spi_dly(void)
{
	for (volatile uint32_t i = 0; i < 6; i++)
	{
		__NOP();
	}
}

void soft_spi_init(void)
{
	GPIO_InitTypeDef g = {0};

	__HAL_RCC_GPIOB_CLK_ENABLE();

	/* 输出脚：CS / SCK / 数据输出 */
	g.Pin   = SOFT_SPI_CS_PIN | SOFT_SPI_SCK_PIN | SOFT_SPI_OUT_PIN;
	g.Mode  = GPIO_MODE_OUTPUT_PP;
	g.Pull  = GPIO_PULLUP;
	g.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(SOFT_SPI_PORT, &g);

	/* 输入脚：数据输入 */
	g.Pin  = SOFT_SPI_IN_PIN;
	g.Mode = GPIO_MODE_INPUT;
	g.Pull = GPIO_PULLUP;      /* 板子上没外部上拉时，内部上拉避免悬空 */
	HAL_GPIO_Init(SOFT_SPI_PORT, &g);

	SOFT_SPI_CS_HIGH();        /* 不选中 */
	SOFT_SPI_SCK_LOW();        /* SPI Mode0：时钟空闲为低 */
}

/* 收发一个字节（SPI Mode 0：上升沿采样，下降沿移位） */
uint8_t soft_spi_xfer(uint8_t byte)
{
	uint8_t in = 0;

	for (uint8_t i = 0; i < 8; i++)
	{
		/* 1) 准备本位数据（高位先出） */
		if (byte & 0x80)
			SOFT_SPI_OUT_HIGH();
		else
			SOFT_SPI_OUT_LOW();
		byte <<= 1;
		spi_dly();

		/* 2) 上升沿：Flash 采样 DI，同时主机采样 DO */
		SOFT_SPI_SCK_HIGH();
		spi_dly();

		in <<= 1;
		if (SOFT_SPI_IN_READ())
			in |= 0x01;

		/* 3) 下降沿 */
		SOFT_SPI_SCK_LOW();
		spi_dly();
	}
	return in;
}

void soft_spi_write(const uint8_t *buf, uint32_t len)
{
	while (len--)
		(void)soft_spi_xfer(*buf++);
}

void soft_spi_read(uint8_t *buf, uint32_t len)
{
	while (len--)
		*buf++ = soft_spi_xfer(0xFF);   /* 读的时候 MOSI 发 0xFF（dummy） */
}