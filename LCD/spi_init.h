#ifndef __SPI_INIT_H
#define __SPI_INIT_H

#include "stm32f4xx_hal.h"

/* SPI2 instance used by the ST7789V LCD (defined in spi_init.c) */
extern SPI_HandleTypeDef hspi2;

/* Initialize SPI2 + LCD GPIO, according to the ST7789V requirements:
 * - CPOL = 0 / CPHA = 0 (SPI mode 0)
 * - 8-bit data, MSB first, software chip-select
 * - 21 MHz SCK (APB1 = 42 MHz, prescaler /2)
 *
 * Control pins are taken from LCD.h. The current pinout uses PC0..PC3 so the
 * F407 DCMI pins PB6..PB9 remain available to the camera.
 */
void LCD_SPI_Init(void);

/* Re-initialize the SPI peripheral after a transmit timeout/error so a
 * wedged LCD bus self-recovers instead of hanging the system. */
void LCD_SPI_Recover(void);

/* Stream raw data bytes to the LCD. DC is driven high and CS is driven
 * low for the whole transfer; large buffers are split into small chunks. */
void LCD_WriteRawData(const uint8_t *data, uint32_t len);

#endif
