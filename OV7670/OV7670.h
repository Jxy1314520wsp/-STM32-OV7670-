#ifndef __OV7670_H
#define __OV7670_H

#include "stm32f4xx_hal.h"

/* Measured: the module outputs 121 lines x 320 pixel-clocks = 160 px wide
 * RGB565 (38720 bytes/frame). QQVGA native. We capture 160x120 (38400 B)
 * and scale 2x to 320x240. */
#define OV7670_FRAME_WIDTH       160U
#define OV7670_FRAME_HEIGHT      120U
#define OV7670_FRAME_BYTES       (OV7670_FRAME_WIDTH * OV7670_FRAME_HEIGHT * 2U)
#define OV7670_FRAME_WORDS       (OV7670_FRAME_BYTES / 4U)

#define OV7670_SCCB_WRITE_ADDR   0x42U
#define OV7670_SCCB_READ_ADDR    0x43U
#define OV7670_PID_REG           0x0AU
#define OV7670_VER_REG           0x0BU

typedef enum
{
    OV7670_STATUS_OK = 0,
    OV7670_STATUS_ERROR,
    OV7670_STATUS_ID_ERROR,
    OV7670_STATUS_CONFIG_ERROR
} OV7670_StatusTypeDef;

extern DCMI_HandleTypeDef hdcmi;
extern DMA_HandleTypeDef hdma_dcmi;

OV7670_StatusTypeDef OV7670_Init(void);
HAL_StatusTypeDef OV7670_Start(void);
HAL_StatusTypeDef OV7670_Stop(void);

HAL_StatusTypeDef OV7670_WriteRegister(uint8_t reg, uint8_t value);
HAL_StatusTypeDef OV7670_ReadRegister(uint8_t reg, uint8_t *value);

uint8_t OV7670_IsFrameReady(void);
void OV7670_ClearFrameReady(void);
uint32_t OV7670_GetLastError(void);
const uint8_t *OV7670_GetFrameBuffer(void);
uint32_t OV7670_GetFrameSize(void);

/* Convert the current RGB565 frame to 160x120 grayscale. dst must hold
 * OV7670_FRAME_WIDTH * OV7670_FRAME_HEIGHT bytes. */
void OV7670_ToGray(uint8_t *dst);
uint16_t OV7670_GetPixel(uint16_t x, uint16_t y);

#endif
