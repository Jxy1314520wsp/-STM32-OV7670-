#include "LCD.h"
#include "DMA.h"
#include "spi_init.h"
#include "delay.h"
#include "stdio.h"

extern SPI_HandleTypeDef hspi2;

/* Write a command to LCD via SPI */
void LCD_WriteCmd(uint8_t cmd)
{
    LCD_DC_CLR();
    LCD_CS_CLR();
    if (HAL_SPI_Transmit(&hspi2, &cmd, sizeof(cmd), LCD_SPI_TIMEOUT) != HAL_OK)
    {
        LCD_SPI_Recover();
    }
    LCD_CS_SET();
}

/* Write a single data byte to LCD via SPI */
void LCD_WriteData(uint8_t data)
{
    LCD_DC_SET();
    LCD_CS_CLR();
    if (HAL_SPI_Transmit(&hspi2, &data, sizeof(data), LCD_SPI_TIMEOUT) != HAL_OK)
    {
        LCD_SPI_Recover();
    }
    LCD_CS_SET();
}

/* Hardware reset */
static void LCD_Reset(void)
{
    LCD_RST_CLR();
    delay_ms(100);
    LCD_RST_SET();
    delay_ms(120);
}

/* Initialize the ST7789V LCD controller */
void LCD_Init(void)
{
    LCD_SPI_Init();
    LCD_DMA_Init();

    LCD_Reset();

    /* ST7789V initialization sequence - reference: st7789.c */
    LCD_WriteCmd(0x3A); LCD_WriteData(0x55);  /* COLMOD: 16bit RGB565 */
    LCD_WriteCmd(0xB2); LCD_WriteData(0x0C);  /* Porch control */
    LCD_WriteData(0x0C);
    LCD_WriteData(0x00);
    LCD_WriteData(0x33);
    LCD_WriteData(0x33);
    LCD_WriteCmd(0x36); LCD_WriteData(0x60);  /* MADCTL: landscape mode */
    LCD_WriteCmd(0xB7); LCD_WriteData(0x35);  /* Gate control */
    LCD_WriteCmd(0xBB); LCD_WriteData(0x19);  /* VCOM setting */
    LCD_WriteCmd(0xC0); LCD_WriteData(0x2C);  /* LCMCTRL */
    LCD_WriteCmd(0xC2); LCD_WriteData(0x01);  /* VDV and VRH command enable */
    LCD_WriteCmd(0xC3); LCD_WriteData(0x12);  /* VRH set */
    LCD_WriteCmd(0xC4); LCD_WriteData(0x20);  /* VDV set */
    LCD_WriteCmd(0xC6); LCD_WriteData(0x0F);  /* Frame rate control (60Hz) */
    LCD_WriteCmd(0xD0); LCD_WriteData(0xA4);  /* Power control */
    LCD_WriteData(0xA1);
    LCD_WriteCmd(0xE0); LCD_WriteData(0xD0);  /* Positive gamma */
    LCD_WriteData(0x04);
    LCD_WriteData(0x0D);
    LCD_WriteData(0x11);
    LCD_WriteData(0x13);
    LCD_WriteData(0x2B);
    LCD_WriteData(0x3F);
    LCD_WriteData(0x54);
    LCD_WriteData(0x4C);
    LCD_WriteData(0x18);
    LCD_WriteData(0x0D);
    LCD_WriteData(0x0B);
    LCD_WriteData(0x1F);
    LCD_WriteData(0x23);
    LCD_WriteCmd(0xE1); LCD_WriteData(0xD0);  /* Negative gamma */
    LCD_WriteData(0x04);
    LCD_WriteData(0x0C);
    LCD_WriteData(0x11);
    LCD_WriteData(0x13);
    LCD_WriteData(0x2C);
    LCD_WriteData(0x3F);
    LCD_WriteData(0x44);
    LCD_WriteData(0x51);
    LCD_WriteData(0x2F);
    LCD_WriteData(0x1F);
    LCD_WriteData(0x1F);
    LCD_WriteData(0x20);
    LCD_WriteData(0x23);

    /* INVON removed: this panel shows natural colors without it */
    LCD_WriteCmd(0x11);  /* SLPOUT: sleep out */
    delay_ms(120);
    LCD_WriteCmd(0x13);  /* NORON: normal display on */
    LCD_WriteCmd(0x29);  /* DISPON: display on */

    LCD_Clear(BLACK);
    LCD_BL_SET();
}

/* Set the drawing area */
void LCD_SetArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_WriteCmd(0x2A);
    LCD_WriteData((uint8_t)(x1 >> 8));
    LCD_WriteData((uint8_t)(x1 & 0xFF));
    LCD_WriteData((uint8_t)(x2 >> 8));
    LCD_WriteData((uint8_t)(x2 & 0xFF));

    LCD_WriteCmd(0x2B);
    LCD_WriteData((uint8_t)(y1 >> 8));
    LCD_WriteData((uint8_t)(y1 & 0xFF));
    LCD_WriteData((uint8_t)(y2 >> 8));
    LCD_WriteData((uint8_t)(y2 & 0xFF));

    LCD_WriteCmd(0x2C);
}

/* Fill a rectangular area with a solid color */
void LCD_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    if ((x1 >= LCD_WIDTH) || (y1 >= LCD_HEIGHT) || (x2 < x1) || (y2 < y1)) {
        return;
    }
    if (x2 >= LCD_WIDTH) x2 = LCD_WIDTH - 1;
    if (y2 >= LCD_HEIGHT) y2 = LCD_HEIGHT - 1;

    uint32_t count = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);

    LCD_SetArea(x1, y1, x2, y2);

#if LCD_USE_DMA
    LCD_DC_SET();
    LCD_CS_CLR();
    DMA_Fill_LCD(color, count);
    LCD_CS_SET();
#else
    static uint8_t tx[512];
    uint32_t sent = 0U;

    for (uint32_t i = 0U; i < (sizeof(tx) / 2U); i++) {
        tx[2U * i] = (uint8_t)(color >> 8);
        tx[2U * i + 1U] = (uint8_t)color;
    }

    while (sent < count) {
        uint32_t chunk = count - sent;
        if (chunk > (sizeof(tx) / 2U)) chunk = sizeof(tx) / 2U;
        LCD_WriteRawData(tx, chunk * 2U);
        sent += chunk;
    }
#endif
}

/* Clear the entire screen */
void LCD_Clear(uint16_t color)
{
    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, color);
}

/* Draw a single pixel */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    LCD_SetArea(x, y, x, y);
    LCD_WriteData((uint8_t)(color >> 8));
    LCD_WriteData((uint8_t)(color & 0xFF));
}

/* Draw a line using Bresenham's algorithm */
void LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    int dx = (x1 > x0) ? (int)(x1 - x0) : (int)(x0 - x1);
    int dy = (y1 > y0) ? (int)(y1 - y0) : (int)(y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int e2;
    int steep = dy > dx;

    if (steep) {
        int t = x0; x0 = y0; y0 = t;
        int t2 = x1; x1 = y1; y1 = t2;
        int ts = sx; sx = sy; sy = ts;
        int td = dx; dx = dy; dy = td;
        err = dx - dy;
    }

    while (1) {
        LCD_DrawPoint(steep ? y0 : x0, steep ? x0 : y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

/* Draw a rectangle (filled or outline) */
void LCD_DrawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t filled, uint16_t color)
{
    if (filled) {
        LCD_Fill(x, y, x + width - 1, y + height - 1, color);
    } else {
        LCD_DrawLine(x, y, x + width - 1, y, color);
        LCD_DrawLine(x, y + height - 1, x + width - 1, y + height - 1, color);
        LCD_DrawLine(x, y, x, y + height - 1, color);
        LCD_DrawLine(x + width - 1, y, x + width - 1, y + height - 1, color);
    }
}

/* Draw a circle using midpoint algorithm */
void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t radius, uint8_t filled, uint16_t color)
{
    int16_t dx = 0, dy = (int16_t)radius;
    int16_t d = 3 - 2 * radius;

    if (filled) {
        while (dx <= dy) {
            LCD_DrawLine(x - dy, y - dx, x + dy, y - dx, color);
            LCD_DrawLine(x - dx, y - dy, x + dx, y - dy, color);
            LCD_DrawLine(x - dy, y + dx, x + dy, y + dx, color);
            LCD_DrawLine(x - dx, y + dy, x + dx, y + dy, color);
            if (d < 0) d += 4 * dx + 6;
            else { d += 4 * (dx - dy) + 10; dy--; }
            dx++;
        }
    } else {
        while (dx <= dy) {
            LCD_DrawPoint(x + dx, y + dy, color);
            LCD_DrawPoint(x - dx, y + dy, color);
            LCD_DrawPoint(x + dx, y - dy, color);
            LCD_DrawPoint(x - dx, y - dy, color);
            LCD_DrawPoint(x + dy, y + dx, color);
            LCD_DrawPoint(x - dy, y + dx, color);
            LCD_DrawPoint(x + dy, y - dx, color);
            LCD_DrawPoint(x - dy, y - dx, color);
            if (d < 0) d += 4 * dx + 6;
            else { d += 4 * (dx - dy) + 10; dy--; }
            dx++;
        }
    }
}

/* Draw a character using font data
 * Font data format: one uint16_t per row, MSB first, bit1=foreground */
void LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bgcolor, FontDef font)
{
    uint32_t i, j, b;

    LCD_SetArea(x, y, x + font.width - 1, y + font.height - 1);

    for (i = 0; i < font.height; i++) {
        b = font.data[(uint8_t)(ch - 32) * font.height + i];
        for (j = 0; j < font.width; j++) {
            if ((b << j) & 0x8000) {
                LCD_WriteData((uint8_t)(color >> 8));
                LCD_WriteData((uint8_t)(color & 0xFF));
            } else {
                LCD_WriteData((uint8_t)(bgcolor >> 8));
                LCD_WriteData((uint8_t)(bgcolor & 0xFF));
            }
        }
    }
}

/* Draw a string of characters */
void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgcolor, FontDef font)
{
    while (*str) {
        if (x + font.width > LCD_WIDTH) {
            x = 0;
            y += font.height;
            if (y + font.height > LCD_HEIGHT) break;
            if (*str == ' ') { str++; continue; }
        }
        LCD_ShowChar(x, y, *str, color, bgcolor, font);
        x += font.width;
        str++;
    }
}

/* Display an unsigned integer */
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t color, uint16_t bgcolor, FontDef font)
{
    char buf[12];
    sprintf(buf, "%0*lu", len, (unsigned long)num);
    LCD_ShowString(x, y, buf, color, bgcolor, font);
}

/* Display a signed integer */
void LCD_ShowSignedNum(uint16_t x, uint16_t y, int32_t num, uint8_t len, uint16_t color, uint16_t bgcolor, FontDef font)
{
    char buf[13];
    sprintf(buf, "%+0*ld", len, (long)num);
    LCD_ShowString(x, y, buf, color, bgcolor, font);
}

/* Display a floating-point number */
void LCD_ShowFloat(uint16_t x, uint16_t y, double num, uint8_t int_len, uint8_t frac_len, uint16_t color, uint16_t bgcolor, FontDef font)
{
    char fmt[32], buf[32];
    sprintf(fmt, "%%%d.%df", int_len, frac_len);
    sprintf(buf, fmt, num);
    LCD_ShowString(x, y, buf, color, bgcolor, font);
}

/* Display an RGB565 image from a 2D array */
void LCD_ShowImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *img)
{
    if ((img == NULL) || (width == 0U) || (height == 0U) ||
        (x >= LCD_WIDTH) || (y >= LCD_HEIGHT)) {
        return;
    }
    if (width > (LCD_WIDTH - x)) width = LCD_WIDTH - x;
    if (height > (LCD_HEIGHT - y)) height = LCD_HEIGHT - y;

    uint32_t count = (uint32_t)width * height;

    LCD_SetArea(x, y, x + width - 1, y + height - 1);

#if LCD_USE_DMA
    LCD_DC_SET();
    LCD_CS_CLR();
    DMA_Send_LCD((uint16_t *)img, count);
    LCD_CS_SET();
#else
    /* RGB565 values are stored little-endian by the Cortex-M4, while the
     * ST7789V expects the high byte before the low byte on the wire. */
    uint8_t tx[512];
    uint32_t sent = 0U;

    while (sent < count) {
        uint32_t i;
        uint32_t chunk = count - sent;
        if (chunk > (sizeof(tx) / 2U)) chunk = sizeof(tx) / 2U;

        for (i = 0U; i < chunk; i++) {
            uint16_t pixel = img[sent + i];
            tx[2U * i] = (uint8_t)(pixel >> 8);
            tx[2U * i + 1U] = (uint8_t)pixel;
        }
        LCD_WriteRawData(tx, chunk * 2U);
        sent += chunk;
    }
#endif
}
