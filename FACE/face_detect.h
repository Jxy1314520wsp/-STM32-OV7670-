#ifndef __FACE_DETECT_H
#define __FACE_DETECT_H

#include <stdint.h>
#include "img_proc.h"

/* ============================================================================
 * face_detect.h - Skin-color + geometry face detection
 *   v1 detector: YCbCr skin mask at half resolution -> morphology ->
 *   connected components -> geometry/feature filtering.
 * ==========================================================================*/

typedef struct
{
    uint16_t x, y;        /* box in 160x120 coordinates */
    uint16_t w, h;
    uint8_t  found;
} FaceDetect_Result;

typedef struct
{
    ImgProc_SkinThr skin;      /* skin thresholds */
    uint16_t min_area;         /* min component area in 80x60 space */
    uint16_t min_face_w;       /* min face width in 160x120 space */
    uint8_t  min_aspect_x10;   /* width/height * 10, lower bound */
    uint8_t  max_aspect_x10;   /* width/height * 10, upper bound */
    uint8_t  min_fill;         /* fill ratio percent */
    uint16_t eye_var_min;      /* min variance of eye band, 0 = disabled */
} FaceDetect_Config;

void FaceDetect_DefaultConfig(FaceDetect_Config *cfg);

/* rgb565: raw frame bytes; gray: 160x120 grayscale (may be NULL if feature
 * check is disabled). Returns 1 and fills res when a face is found. */
uint8_t FaceDetect_Run(const uint8_t *rgb565, const uint8_t *gray,
                       const FaceDetect_Config *cfg, FaceDetect_Result *res);

#endif /* __FACE_DETECT_H */
