#ifndef __SOFT_SPI_H__
#define __SOFT_SPI_H__

#include "main.h"

/* ===== 板子 MOSI/MISO 与 Flash 接反，所以方向互换 =====
 * 如果板子实测并没有接反，把下面改成 0 即可 */
#define BOARD_MOSI_MISO_SWAPPED   1

#define SOFT_SPI_PORT        GPIOB
#define SOFT_SPI_CS_PIN      GPIO_PIN_12
#define SOFT_SPI_SCK_PIN     GPIO_PIN_13

#if BOARD_MOSI_MISO_SWAPPED
  #define SOFT_SPI_OUT_PIN   GPIO_PIN_14    /* 驱动 Flash 的 DI */
  #define SOFT_SPI_IN_PIN    GPIO_PIN_15    /* 读 Flash 的 DO  */
#else
  #define SOFT_SPI_OUT_PIN   GPIO_PIN_15    /* 正常接线：PB15 = MOSI */
  #define SOFT_SPI_IN_PIN    GPIO_PIN_14    /* 正常接线：PB14 = MISO */
#endif

/* BSRR 直写，比 HAL_GPIO_WritePin 快很多 */
#define SOFT_SPI_CS_LOW()    (SOFT_SPI_PORT->BSRR = ((uint32_t)SOFT_SPI_CS_PIN  << 16))
#define SOFT_SPI_CS_HIGH()   (SOFT_SPI_PORT->BSRR = (uint32_t)SOFT_SPI_CS_PIN)
#define SOFT_SPI_SCK_LOW()   (SOFT_SPI_PORT->BSRR = ((uint32_t)SOFT_SPI_SCK_PIN << 16))
#define SOFT_SPI_SCK_HIGH()  (SOFT_SPI_PORT->BSRR = (uint32_t)SOFT_SPI_SCK_PIN)
#define SOFT_SPI_OUT_LOW()   (SOFT_SPI_PORT->BSRR = ((uint32_t)SOFT_SPI_OUT_PIN << 16))
#define SOFT_SPI_OUT_HIGH()  (SOFT_SPI_PORT->BSRR = (uint32_t)SOFT_SPI_OUT_PIN)
#define SOFT_SPI_IN_READ()   ((SOFT_SPI_PORT->IDR & SOFT_SPI_IN_PIN) != 0)

void    soft_spi_init(void);
uint8_t soft_spi_xfer(uint8_t byte);
void    soft_spi_write(const uint8_t *buf, uint32_t len);
void    soft_spi_read(uint8_t *buf, uint32_t len);

#endif /* __SOFT_SPI_H__ */