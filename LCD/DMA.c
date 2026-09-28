#include "DMA.h"
#include "spi_init.h"

#define LCD_DMA_CHUNK_PIXELS 256U
#define LCD_DMA_TIMEOUT_MS   200U

DMA_HandleTypeDef hdma_spi2_tx;

static uint8_t s_lcd_dma_ready = 0U;
static uint8_t s_lcd_dma_buffer[LCD_DMA_CHUNK_PIXELS * 2U];

static HAL_StatusTypeDef LCD_DMA_Transmit(const uint8_t *data, uint16_t length)
{
    uint32_t start;

    if ((data == NULL) || (length == 0U))
    {
        return HAL_ERROR;
    }

    start = HAL_GetTick();
    if (HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)data, length) != HAL_OK)
    {
        return HAL_ERROR;
    }

    while (hspi2.State == HAL_SPI_STATE_BUSY_TX)
    {
        if ((HAL_GetTick() - start) > LCD_DMA_TIMEOUT_MS)
        {
            (void)HAL_SPI_Abort(&hspi2);
            LCD_SPI_Recover();
            return HAL_TIMEOUT;
        }
    }

    return (hspi2.State == HAL_SPI_STATE_READY) ? HAL_OK : HAL_ERROR;
}

void LCD_DMA_Init(void)
{
    if (s_lcd_dma_ready != 0U)
    {
        return;
    }

    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_spi2_tx.Instance                 = DMA1_Stream4;
    hdma_spi2_tx.Init.Channel              = DMA_CHANNEL_0;
    hdma_spi2_tx.Init.Direction            = DMA_MEMORY_TO_PERIPH;
    hdma_spi2_tx.Init.PeriphInc            = DMA_PINC_DISABLE;
    hdma_spi2_tx.Init.MemInc               = DMA_MINC_ENABLE;
    hdma_spi2_tx.Init.PeriphDataAlignment  = DMA_PDATAALIGN_BYTE;
    hdma_spi2_tx.Init.MemDataAlignment     = DMA_MDATAALIGN_BYTE;
    hdma_spi2_tx.Init.Mode                 = DMA_NORMAL;
    hdma_spi2_tx.Init.Priority             = DMA_PRIORITY_HIGH;
    hdma_spi2_tx.Init.FIFOMode             = DMA_FIFOMODE_DISABLE;
    hdma_spi2_tx.Init.FIFOThreshold        = DMA_FIFO_THRESHOLD_FULL;
    hdma_spi2_tx.Init.MemBurst             = DMA_MBURST_SINGLE;
    hdma_spi2_tx.Init.PeriphBurst          = DMA_PBURST_SINGLE;

    if (HAL_DMA_Init(&hdma_spi2_tx) != HAL_OK)
    {
        return;
    }

    __HAL_LINKDMA(&hspi2, hdmatx, hdma_spi2_tx);

    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    s_lcd_dma_ready = 1U;
}

void DMA_Send_LCD(const uint16_t *data, uint32_t pixel_count)
{
    uint32_t sent = 0U;

    if ((data == NULL) || (pixel_count == 0U) || (s_lcd_dma_ready == 0U))
    {
        return;
    }

    while (sent < pixel_count)
    {
        uint32_t i;
        uint32_t chunk = pixel_count - sent;

        if (chunk > LCD_DMA_CHUNK_PIXELS)
        {
            chunk = LCD_DMA_CHUNK_PIXELS;
        }

        for (i = 0U; i < chunk; i++)
        {
            uint16_t pixel = data[sent + i];
            s_lcd_dma_buffer[2U * i] = (uint8_t)(pixel >> 8);
            s_lcd_dma_buffer[2U * i + 1U] = (uint8_t)pixel;
        }

        if (LCD_DMA_Transmit(s_lcd_dma_buffer, (uint16_t)(chunk * 2U)) != HAL_OK)
        {
            break;
        }
        sent += chunk;
    }
}

void DMA_Fill_LCD(uint16_t color, uint32_t pixel_count)
{
    uint32_t i;
    uint32_t sent = 0U;

    if ((pixel_count == 0U) || (s_lcd_dma_ready == 0U))
    {
        return;
    }

    for (i = 0U; i < LCD_DMA_CHUNK_PIXELS; i++)
    {
        s_lcd_dma_buffer[2U * i] = (uint8_t)(color >> 8);
        s_lcd_dma_buffer[2U * i + 1U] = (uint8_t)color;
    }

    while (sent < pixel_count)
    {
        uint32_t chunk = pixel_count - sent;
        if (chunk > LCD_DMA_CHUNK_PIXELS)
        {
            chunk = LCD_DMA_CHUNK_PIXELS;
        }

        if (LCD_DMA_Transmit(s_lcd_dma_buffer, (uint16_t)(chunk * 2U)) != HAL_OK)
        {
            break;
        }
        sent += chunk;
    }
}
