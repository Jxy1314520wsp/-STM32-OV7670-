#ifndef __LCD_H
#define __LCD_H

#include "stdint.h"
#include "fonts.h"

/* LCD control pins.
 *
 * PB6..PB9 are reserved by the F407 DCMI interface (D5/VSYNC/D6/D7),
 * therefore the ST7789V control signals are placed on PC0..PC3.  Change
 * these definitions together with the PCB wiring if another pinout is used.
 */
#define LCD_CS_PORT     GPIOC
#define LCD_CS_PIN      GPIO_PIN_0
#define LCD_DC_PORT     GPIOC
#define LCD_DC_PIN      GPIO_PIN_1
#define LCD_RST_PORT    GPIOC
#define LCD_RST_PIN     GPIO_PIN_2
#define LCD_BL_PORT     GPIOC
#define LCD_BL_PIN      GPIO_PIN_3

/* Control macros */
#define LCD_CS_SET()    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define LCD_CS_CLR()    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define LCD_DC_SET()    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)
#define LCD_DC_CLR()    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define LCD_RST_SET()   HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)
#define LCD_RST_CLR()   HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_BL_SET()    HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET)
#define LCD_BL_CLR()    HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_RESET)

/* Display resolution after setting ST7789V MADCTL to landscape mode. */
#define LCD_WIDTH       320
#define LCD_HEIGHT      240

/* RGB565 color definitions */
#define WHITE           0xFFFF
#define BLACK           0x0000
#define BLUE            0x001F
#define RED             0xF800
#define GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define LIGHTGRAY       0xC618
#define DARKGRAY        0x7BEF
#define BROWN           0xBC40

/* DMA support toggle: 0=disabled (default), 1=enabled */
#define LCD_USE_DMA     1

/* Bounded SPI wait so a wedged LCD bus cannot hang the system. */
#define LCD_SPI_TIMEOUT 100U

/* Function declarations */
void LCD_WriteCmd(uint8_t cmd);
void LCD_WriteData(uint8_t data);
void LCD_Init(void);

void LCD_SetArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_Clear(uint16_t color);

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LCD_DrawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t filled, uint16_t color);
void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t radius, uint8_t filled, uint16_t color);

void LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bgcolor, FontDef font);
void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgcolor, FontDef font);
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t color, uint16_t bgcolor, FontDef font);
void LCD_ShowSignedNum(uint16_t x, uint16_t y, int32_t num, uint8_t len, uint16_t color, uint16_t bgcolor, FontDef font);
void LCD_ShowFloat(uint16_t x, uint16_t y, double num, uint8_t int_len, uint8_t frac_len, uint16_t color, uint16_t bgcolor, FontDef font);
void LCD_ShowImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *img);

#endif
