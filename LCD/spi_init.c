#include "spi_init.h"
#include "LCD.h"

SPI_HandleTypeDef hspi2;
static uint8_t s_lcd_spi_ready = 0U;

/* -------------------------------------------------------------------------
 * LCD_GPIO_Init
 * SCK  = PB13 (AF5)
 * MISO = PB14 (AF5, not used, reserved)
 * MOSI = PB15 (AF5)
 * CS   = PC0, DC = PC1, RST = PC2, BL = PC3
 * ------------------------------------------------------------------------- */
static void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* SPI2 signals */
    gpio.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* LCD control signals */
    gpio.Pin       = LCD_CS_PIN | LCD_DC_PIN | LCD_RST_PIN | LCD_BL_PIN;
    gpio.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = 0U;
    HAL_GPIO_Init(LCD_CS_PORT, &gpio);

    /* Idle state: CS high, DC high, RST high, back-light off */
    LCD_CS_SET();
    LCD_DC_SET();
    LCD_RST_SET();
    LCD_BL_CLR();
}

/* -------------------------------------------------------------------------
 * LCD_SPI_Init
 * Idempotent init of the SPI master used by the ST7789V controller.
 * ------------------------------------------------------------------------- */
void LCD_SPI_Init(void)
{
    if (s_lcd_spi_ready != 0U)
    {
        return;
    }

    LCD_GPIO_Init();

    __HAL_RCC_SPI2_CLK_ENABLE();

    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;   /* CK idle low */
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;    /* sample on 1st edge */
    hspi2.Init.NSS               = SPI_NSS_SOFT;       /* CS is a GPIO */
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2; /* 42/2=21 MHz */
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial     = 7U;

    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        /* Initialization error: keep CS deselected. */
        LCD_CS_SET();
        return;
    }

    s_lcd_spi_ready = 1U;

    /* Make sure the SPI is enabled after init. */
    __HAL_SPI_ENABLE(&hspi2);
}

/* -------------------------------------------------------------------------
 * LCD_WriteRawData
 * Const-DC/data transaction intended for image data streams.
 * ------------------------------------------------------------------------- */
void LCD_SPI_Recover(void)
{
    HAL_SPI_DeInit(&hspi2);
    HAL_SPI_Init(&hspi2);
    __HAL_SPI_ENABLE(&hspi2);
}

void LCD_WriteRawData(const uint8_t *data, uint32_t len)
{
    uint32_t left;

    if ((data == NULL) || (len == 0U))
    {
        return;
    }

    LCD_DC_SET();
    LCD_CS_CLR();

    left = len;
    while (left > 0U)
    {
        uint16_t chunk = (left > 1024U) ? (uint16_t)1024U : (uint16_t)left;

        if (HAL_SPI_Transmit(&hspi2, (uint8_t *)data, chunk, LCD_SPI_TIMEOUT) != HAL_OK)
        {
            LCD_SPI_Recover();
        }
        data += chunk;
        left -= chunk;
    }

    LCD_CS_SET();
}
