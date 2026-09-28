#ifndef __IMG_PROC_H
#define __IMG_PROC_H

#include <stdint.h>

/* ============================================================================
 * img_proc.h - Lightweight image processing helpers for face recognition
 * ==========================================================================*/

/* RGB565 (high byte first) -> grayscale. src length = w*h*2 bytes. */
void ImgProc_RGB565ToGray(const uint8_t *rgb565, uint8_t *gray,
                          uint16_t w, uint16_t h);

/* Nearest-neighbor resize. */
void ImgProc_ResizeNearest(const uint8_t *src, uint16_t sw, uint16_t sh,
                           uint8_t *dst, uint16_t dw, uint16_t dh);

/* In-place 256-level histogram equalization. */
void ImgProc_HistEq(uint8_t *img, uint32_t n);

/* Sharpness: mean of squared Laplacian (|4c - up - down - left - right|). */
uint32_t ImgProc_LaplaceVar(const uint8_t *img, uint16_t w, uint16_t h);

/* 3x3 binary erode / dilate. out must NOT alias in. */
void ImgProc_Erode3(const uint8_t *in, uint8_t *out, uint16_t w, uint16_t h);
void ImgProc_Dilate3(const uint8_t *in, uint8_t *out, uint16_t w, uint16_t h);

/* Connected-component labeling (two-pass union-find, max 253 labels).
 * Returns the number of components written to comps (<= max_comps). */
typedef struct
{
    uint16_t x0, y0;      /* inclusive */
    uint16_t x1, y1;      /* inclusive */
    uint32_t area;
} ImgProc_Component;

uint8_t ImgProc_Label(const uint8_t *mask, uint8_t *labels,
                      uint16_t w, uint16_t h,
                      ImgProc_Component *comps, uint8_t max_comps);

/* Skin thresholds in YCbCr space. */
typedef struct
{
    uint8_t y_min, y_max;
    uint8_t cb_min, cb_max;
    uint8_t cr_min, cr_max;
} ImgProc_SkinThr;

/* Build a skin mask at half resolution (dw = sw/2, dh = sh/2).
 * Each output cell samples its 2x2 source block; any skin pixel -> 1. */
void ImgProc_SkinMask2x(const uint8_t *rgb565, uint8_t *mask,
                        uint16_t sw, uint16_t sh,
                        uint16_t dw, uint16_t dh,
                        const ImgProc_SkinThr *t);

/* Standard CRC-32 (IEEE). */
uint32_t ImgProc_CRC32(const uint8_t *data, uint32_t len, uint32_t seed);

#endif /* __IMG_PROC_H */
