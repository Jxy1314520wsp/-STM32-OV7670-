#ifndef __DMA_H
#define __DMA_H

#include "stm32f4xx_hal.h"

/* Called by the LCD driver. It initializes DMA1_Stream4_Channel0
 * (SPI2_TX) used for optional high-throughput pixel transfers. */
void LCD_DMA_Init(void);

/* Streaming helpers used by LCD_ShowImage()/LCD_Fill() when
 * LCD_USE_DMA is set to 1. The byte order is converted on the fly so that
 * the high color byte is always sent first (required by ST7789V). */
void DMA_Send_LCD(const uint16_t *data, uint32_t pixel_count);
void DMA_Fill_LCD(uint16_t color, uint32_t pixel_count);

/* Exported for DMA1_Stream4_IRQHandler in stm32f4xx_it.c. */
extern DMA_HandleTypeDef hdma_spi2_tx;

#endif
